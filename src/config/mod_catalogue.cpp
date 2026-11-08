#include "pch.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "config/mod_catalogue.h"
#include "game/user_paths.h"
#include "game/vfs.h"
#include "miniz.h"

namespace config {

    namespace {

        constexpr size_t MOD_MANIFEST_MAX_SIZE = 1024 * 1024;

        bool Read_File(const std::filesystem::path& path, std::string* source) {
            std::error_code ec;
            const auto size = std::filesystem::file_size(path, ec);
            if (ec || size > MOD_MANIFEST_MAX_SIZE) {
                return false;
            }
            std::ifstream input(path, std::ios::binary);
            if (!input) {
                return false;
            }
            source->assign(std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
            return input.good() || input.eof();
        }

        s_mod_catalogue_entry Broken_Entry(const std::string& id,
            e_mod_source source, const std::string& reason) {
            s_mod_catalogue_entry entry;
            entry.id = id;
            entry.name = id;
            entry.source = source;
            entry.broken = true;
            entry.reason = reason;
            return entry;
        }

        // Builds a catalogue entry from a validated manifest. The
        // loader only canonicalizes the config path; the catalogue
        // additionally requires the referenced file to exist in the
        // package. A mod whose config is missing is broken, not merely
        // unconfigured. `config_exists` answers that question for the
        // package form (directory or ZIP).
        template <typename t_config_exists>
        s_mod_catalogue_entry Entry_From_Manifest(const s_mod_manifest& manifest,
            e_mod_source source, bool enabled, t_config_exists config_exists) {
            if (!config_exists(manifest.config)) {
                s_mod_catalogue_entry entry = Broken_Entry(manifest.id, source,
                    "referenced config file '" + manifest.config +
                        "' is missing");
                entry.enabled = false;
                return entry;
            }
            s_mod_catalogue_entry entry;
            entry.id = manifest.id;
            entry.name = manifest.name;
            entry.description = manifest.description;
            entry.version_text = manifest.version_text;
            entry.engine_range = manifest.engine_range;
            entry.dependencies = manifest.dependencies;
            entry.conflicts = manifest.conflicts;
            entry.enabled = enabled;
            entry.source = source;
            return entry;
        }

        // Validates one manifest source with the loader's checks and
        // builds its entry. `source_name` is the display name used in
        // diagnostics.
        template <typename t_config_exists>
        s_mod_catalogue_entry Catalogue_Manifest(const std::string& id,
            std::string manifest_source, const std::string& source_name,
            e_mod_source source, bool enabled, t_config_exists config_exists) {
            s_mod_manifest manifest;
            s_cfg_diagnostic diagnostic;
            if (!Load_Mod_Manifest_From_Source(id, std::move(manifest_source),
                    source_name, &manifest, &diagnostic)) {
                s_mod_catalogue_entry entry = Broken_Entry(id, source,
                    diagnostic.message.empty()
                        ? "invalid mod manifest"
                        : diagnostic.message);
                entry.enabled = false;
                return entry;
            }
            return Entry_From_Manifest(manifest, source, enabled, config_exists);
        }

        // Validates one installed package directory and builds its
        // catalogue entry. The directory name is the package id: a
        // manifest whose id differs is broken, exactly as the loader
        // rejects it.
        s_mod_catalogue_entry Catalogue_Directory(
            const std::filesystem::path& directory, e_mod_source source,
            bool enabled) {
            const std::string id = directory.filename().string();
            const std::filesystem::path manifest_path = directory / "mod.json5";
            std::string manifest_source;
            if (!Read_File(manifest_path, &manifest_source)) {
                s_mod_catalogue_entry entry =
                    Broken_Entry(id, source, "mod manifest not found");
                entry.enabled = false;
                return entry;
            }
            return Catalogue_Manifest(id, std::move(manifest_source),
                manifest_path.string(), source, enabled,
                [&directory](const std::string& config) {
                    return std::filesystem::is_regular_file(directory / config);
                });
        }

        // Validates one installed package ZIP and builds its catalogue
        // entry. The file stem is the package id. The shipped game
        // carries its mods as ZIPs; the loader supports both forms,
        // so the catalogue must too.
        s_mod_catalogue_entry Catalogue_Zip(
            const std::filesystem::path& zip_path, e_mod_source source,
            bool enabled) {
            const std::string id = zip_path.stem().string();
            mz_zip_archive archive{};
            mz_zip_zero_struct(&archive);
            if (mz_zip_reader_init_file(&archive, zip_path.string().c_str(), 0) !=
                MZ_TRUE) {
                return Broken_Entry(id, source, "unable to open package ZIP: '" + id + "'");
            }
            size_t manifest_size = 0;
            void* manifest_data = mz_zip_reader_extract_file_to_heap(
                &archive, "mod.json5", &manifest_size, 0);
            if (manifest_data == nullptr || manifest_size > MOD_MANIFEST_MAX_SIZE) {
                if (manifest_data != nullptr) {
                    mz_free(manifest_data);
                }
                mz_zip_reader_end(&archive);
                return Broken_Entry(id, source,
                    manifest_data == nullptr ? "mod manifest not found"
                        : "mod manifest has an invalid size");
            }
            std::string manifest_source(
                static_cast<const char*>(manifest_data), manifest_size);
            mz_free(manifest_data);
            // The config existence check runs while the archive is
            // still open; the manifest validation copies what it needs.
            s_mod_catalogue_entry entry = Catalogue_Manifest(id,
                std::move(manifest_source), zip_path.string() + "!/mod.json5",
                source, enabled,
                [&archive](const std::string& config) {
                    return mz_zip_reader_locate_file(&archive, config.c_str(),
                        nullptr, 0) >= 0;
                });
            mz_zip_reader_end(&archive);
            return entry;
        }

        bool Is_Zip_Package(const std::filesystem::path& path) {
            // The loader only recognizes the lowercase ".zip" form.
            const std::string name = path.filename().string();
            return name.size() > 4 &&
                name.compare(name.size() - 4, 4, ".zip") == 0;
        }

        void Scan_Root(const std::string& root, e_mod_source source,
            const std::vector<std::string>& enabled_ids,
            std::map<std::string, s_mod_catalogue_entry>* entries) {
            if (root.empty()) {
                return;
            }
            std::error_code ec;
            std::filesystem::directory_iterator iterator(root, ec);
            if (ec) {
                return;
            }
            std::vector<std::filesystem::path> directories;
            std::vector<std::filesystem::path> zips;
            for (const std::filesystem::directory_entry& child : iterator) {
                if (child.is_directory(ec) && !ec) {
                    directories.push_back(child.path());
                } else if (child.is_regular_file(ec) && !ec &&
                    Is_Zip_Package(child.path())) {
                    zips.push_back(child.path());
                }
            }
            const auto enabled = [&enabled_ids](const std::string& id) {
                return std::find(enabled_ids.begin(), enabled_ids.end(), id) !=
                    enabled_ids.end();
            };
            std::sort(directories.begin(), directories.end());
            for (const std::filesystem::path& directory : directories) {
                const std::string id = directory.filename().string();
                // A later root wins: the user root is scanned after
                // the bundled roots, so a user mod with the same id
                // replaces the bundled one.
                (*entries)[id] =
                    Catalogue_Directory(directory, source, enabled(id));
            }
            std::sort(zips.begin(), zips.end());
            for (const std::filesystem::path& zip_path : zips) {
                const std::string id = zip_path.stem().string();
                const auto existing = entries->find(id);
                if (existing != entries->end() &&
                    existing->second.source == source) {
                    // Both forms in one root: the loader refuses the
                    // package, so the catalogue lists it broken with
                    // the loader's reason instead of guessing a winner.
                    existing->second = Broken_Entry(id, source,
                        "package '" + id + "' has both directory and ZIP forms");
                    existing->second.enabled = false;
                    continue;
                }
                (*entries)[id] = Catalogue_Zip(zip_path, source, enabled(id));
            }
        }

    } // namespace

    std::vector<s_mod_catalogue_entry> Build_Mod_Catalogue(
        const std::vector<std::string>& bundled_mods_roots,
        const std::string& user_mods_root,
        const std::vector<std::string>& enabled_ids) {
        std::map<std::string, s_mod_catalogue_entry> entries;
        for (const std::string& root : bundled_mods_roots) {
            Scan_Root(root, MOD_SOURCE_BUNDLED, enabled_ids, &entries);
        }
        Scan_Root(user_mods_root, MOD_SOURCE_USER, enabled_ids, &entries);
        std::vector<s_mod_catalogue_entry> catalogue;
        catalogue.reserve(entries.size());
        for (auto& [id, entry] : entries) {
            catalogue.push_back(std::move(entry));
        }
        return catalogue;
    }

    std::vector<s_mod_catalogue_entry> Build_Mod_Catalogue(
        const std::vector<std::string>& enabled_ids) {
        // Only the first data search path (the VFS base path) is a
        // bundled root: the loader resolves mods from the user mods
        // path, else base_path + "data/mods/", and never consults the
        // raw or fallback search paths for mods. Scanning those would
        // list mods the loader cannot resolve.
        std::vector<std::string> bundled_roots;
        const std::vector<std::string> search_paths = vfs::Get_Data_Search_Paths();
        if (!search_paths.empty()) {
            bundled_roots.push_back(search_paths.front() + "data/mods/");
        }
        std::string user_mods_root;
        const char* data_directory =
            user_paths::Get_Directory(user_paths::USER_DIRECTORY_DATA);
        if (data_directory != nullptr && data_directory[0] != '\0') {
            user_mods_root = std::string(data_directory) + "mods/";
        }
        return Build_Mod_Catalogue(bundled_roots, user_mods_root, enabled_ids);
    }

    std::vector<std::string> Prune_Disabled_Mod_Set(
        const std::vector<s_mod_catalogue_entry>& catalogue,
        const std::vector<std::string>& enabled_ids,
        const std::string& disabled_id) {
        if (std::find(enabled_ids.begin(), enabled_ids.end(), disabled_id) ==
            enabled_ids.end()) {
            return enabled_ids;
        }
        // Dependency edges, by id, from the catalogue.
        std::map<std::string, std::vector<std::string>> dependencies;
        for (const s_mod_catalogue_entry& entry : catalogue) {
            std::vector<std::string>& edges = dependencies[entry.id];
            for (const s_mod_dependency& dependency : entry.dependencies) {
                if (!dependency.id.empty()) {
                    edges.push_back(dependency.id);
                }
            }
        }
        // All ids reachable from id through dependency edges.
        const auto transitive_dependencies = [&](const std::string& id) {
            std::set<std::string> reached;
            std::vector<std::string> stack;
            const auto found = dependencies.find(id);
            if (found != dependencies.end()) {
                stack = found->second;
            }
            while (!stack.empty()) {
                const std::string current = stack.back();
                stack.pop_back();
                if (!reached.insert(current).second) {
                    continue;
                }
                const auto next = dependencies.find(current);
                if (next != dependencies.end()) {
                    stack.insert(stack.end(), next->second.begin(), next->second.end());
                }
            }
            return reached;
        };
        // The disabled mod and every mod that (transitively) needs it.
        std::set<std::string> removed;
        removed.insert(disabled_id);
        for (const s_mod_catalogue_entry& entry : catalogue) {
            if (transitive_dependencies(entry.id).count(disabled_id) != 0) {
                removed.insert(entry.id);
            }
        }
        std::vector<std::string> remaining;
        for (const std::string& id : enabled_ids) {
            if (removed.count(id) == 0) {
                remaining.push_back(id);
            }
        }
        // A dependency of a removed mod stays only while a remaining mod
        // still needs it. Repeat to a fixpoint: removing one orphan can
        // orphan its own dependencies.
        bool changed = true;
        while (changed) {
            changed = false;
            std::set<std::string> needed;
            for (const std::string& id : remaining) {
                const std::set<std::string> deps = transitive_dependencies(id);
                needed.insert(deps.begin(), deps.end());
            }
            std::set<std::string> orphaned;
            for (const std::string& id : removed) {
                for (const std::string& dep : transitive_dependencies(id)) {
                    if (needed.count(dep) == 0) {
                        orphaned.insert(dep);
                    }
                }
            }
            std::vector<std::string> kept;
            for (const std::string& id : remaining) {
                if (orphaned.count(id) == 0) {
                    kept.push_back(id);
                } else {
                    removed.insert(id);
                    changed = true;
                }
            }
            remaining = std::move(kept);
        }
        return remaining;
    }

    bool Read_Enabled_Mods(const std::string& path,
        std::vector<std::string>* enabled_ids, s_cfg_diagnostic* diagnostic) {
        if (enabled_ids == nullptr || diagnostic == nullptr) {
            return false;
        }
        *enabled_ids = {};
        *diagnostic = {};
        std::string source;
        if (!Read_File(std::filesystem::path(path), &source)) {
            diagnostic->message = "could not read the configuration file";
            return false;
        }
        s_cfg_document document;
        if (!Parse_Cfg(path.c_str(), source.data(), source.size(), &document,
                diagnostic)) {
            return false;
        }
        for (const s_cfg_statement& statement : document.statements) {
            if (statement.kind == CFG_STATEMENT_STOP) {
                break;
            }
            if (statement.kind == CFG_STATEMENT_ENABLE) {
                enabled_ids->push_back(statement.argument.raw);
            }
        }
        return true;
    }

} // namespace config
