#include "mod_manager.h"

#include <unordered_map>
#include <utility>
#include <vector>

#include "config/mod_manifest.h"
#include "config/config_profile.h"
#include "game/config_refresh.h"
#include "game/vfs.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_registry.h"
#include "lua/lua_save.h"

namespace config {
    struct s_config_with_mods_transaction {
        s_vfs_mount_transaction* mounts = nullptr;
        s_effective_config effective;
        std::vector<std::string> package_order;
        std::vector<s_mod_requirement> resolved_packages;
        t_loaded_mod_list loaded_mods;
        orion2re::lua::s_lua_registry lua_registry;
        orion2re::lua::s_lua_mod_runtime_set* lua_runtimes = nullptr;
    };

    namespace {
        t_loaded_mod_list active_loaded_mods;

        struct s_enabled_package {
            s_mod_manifest manifest;
            s_cfg_document document;
        };

        struct s_package_baseline_cache {
            std::string key;
            s_effective_config effective;
        };
        s_package_baseline_cache package_baseline_cache;
        std::vector<s_enabled_package> active_packages;
        std::vector<std::string> active_package_stamps;
        std::vector<std::string> active_manifest_sources;
        bool active_package_cache_ready = false;

        bool Read_Manifest_Source(const std::string& id, std::string* source) {
            const auto path = "mods/" + id + "/mod.json5";
            auto* file = vfs::fopen(path.c_str(), "rb");
            if (file == nullptr) return false;
            const auto size = vfs::fsize(file);
            if (size < 0 || size > 16 * 1024 * 1024) { vfs::fclose(file); return false; }
            source->resize(static_cast<size_t>(size));
            const bool ok = vfs::fread(source->data(), 1, source->size(), file) == source->size();
            return vfs::fclose(file) == 0 && ok;
        }

        void Append_Document_Key(const s_cfg_document& document, std::string* key) {
            if (document.source_revision != 0) {
                *key += "revision:" + std::to_string(document.source_revision) + ";";
                return;
            }
            const auto append = [&](const std::string& value) {
                *key += std::to_string(value.size()) + ":" + value;
            };
            for (const auto& statement : document.statements) {
                append(std::to_string(statement.kind));
                append(statement.location.source);
                append(std::to_string(statement.location.line));
                append(std::to_string(statement.location.column));
                append(std::to_string(statement.location.offset));
                append(std::to_string(statement.statement_number));
                append(std::to_string(statement.path.size()));
                for (const auto& token : statement.path) append(token.raw);
                append(std::to_string(statement.values.size()));
                for (const auto& token : statement.values) append(token.raw);
                append(std::to_string(statement.groups.size()));
                for (const auto& group : statement.groups) {
                    append(group.selector.raw);
                    append(std::to_string(group.values.size()));
                    for (const auto& token : group.values) append(token.raw);
                    append("end-group");
                }
                append(statement.argument.raw);
                append("end-statement");
            }
        }

        struct s_package_resolver {
            s_vfs_mount_transaction* mounts = nullptr;
            std::vector<s_enabled_package> packages;
            std::unordered_map<std::string, size_t> package_indexes;
            std::vector<std::string> stack;
            std::vector<std::string> requested_packages;
            s_cfg_diagnostic* diagnostic = nullptr;
            t_config_document_loader load_document = nullptr;
        };

        bool Reuse_Active_Packages(const std::vector<s_mod_requirement>* required,
            s_package_resolver* resolver) {
            if (!active_package_cache_ready || required == nullptr || required->size() != active_packages.size() ||
                active_package_stamps.size() != active_packages.size() ||
                active_manifest_sources.size() != active_packages.size()) return false;
            for (size_t index = 0; index < required->size(); ++index) {
                const auto& package = active_packages[index];
                // A previously absent optional dependency may now be installed.
                // Resolve that topology normally rather than trusting this cache.
                if (!package.manifest.optional_dependencies.empty()) return false;
                if ((*required)[index].id != package.manifest.id || (*required)[index].version != package.manifest.version_text) return false;
                std::string stamp, manifest;
                if (!vfs::Get_Package_Source_Stamp(package.manifest.id.c_str(), &stamp) ||
                    stamp != active_package_stamps[index] ||
                    !Read_Manifest_Source(package.manifest.id, &manifest) || manifest != active_manifest_sources[index]) return false;
                s_cfg_document document;
                if (!resolver->load_document("mods/" + package.manifest.id + "/" + package.manifest.config, &document, resolver->diagnostic)) return false;
                std::string old_key, new_key;
                Append_Document_Key(package.document, &old_key);
                Append_Document_Key(document, &new_key);
                if (old_key != new_key) return false;
            }
            resolver->packages = active_packages;
            return true;
        }

        void Cache_Active_Packages(const s_package_resolver& resolver) {
            active_package_cache_ready = false;
            active_packages = resolver.packages;
            active_package_stamps.clear();
            active_manifest_sources.clear();
            for (const auto& package : active_packages) {
                std::string stamp, manifest;
                if (!vfs::Get_Package_Source_Stamp(package.manifest.id.c_str(), &stamp) ||
                    !Read_Manifest_Source(package.manifest.id, &manifest)) {
                    active_packages.clear();
                    return;
                }
                active_package_stamps.push_back(std::move(stamp));
                active_manifest_sources.push_back(std::move(manifest));
            }
            active_package_cache_ready = true;
        }

        void Record_Package_Request(s_package_resolver* resolver,
            const std::string& package_id) {
            for (const std::string& requested : resolver->requested_packages) {
                if (requested == package_id) {
                    return;
                }
            }
            resolver->requested_packages.push_back(package_id);
        }

        void Normalize_Package_Priority(s_package_resolver* resolver) {
            std::vector<s_enabled_package> ordered;
            ordered.reserve(resolver->packages.size());
            std::vector<bool> selected(resolver->packages.size(), false);
            for (const std::string& requested : resolver->requested_packages) {
                const auto found = resolver->package_indexes.find(requested);
                if (found != resolver->package_indexes.end()) {
                    selected[found->second] = true;
                    ordered.push_back(std::move(resolver->packages[found->second]));
                }
            }
            for (size_t index = 0; index < resolver->packages.size(); ++index) {
                if (!selected[index]) {
                    ordered.push_back(std::move(resolver->packages[index]));
                }
            }
            resolver->packages = std::move(ordered);
            resolver->package_indexes.clear();
            for (size_t index = 0; index < resolver->packages.size(); ++index) {
                resolver->package_indexes.emplace(resolver->packages[index].manifest.id, index);
            }
        }

        void Get_Loaded_Mods(const s_package_resolver& resolver,
            t_loaded_mod_list* loaded_mods) {
            loaded_mods->clear();
            loaded_mods->reserve(resolver.packages.size());
            for (size_t index = 0; index < resolver.packages.size(); ++index) {
                loaded_mods->push_back({
                    index,
                    resolver.packages[index].manifest,
                });
            }
        }

        void Get_Package_Manifests(const s_package_resolver& resolver,
            std::vector<s_mod_manifest>* manifests) {
            manifests->clear();
            manifests->reserve(resolver.packages.size());
            for (const s_enabled_package& package : resolver.packages) {
                manifests->push_back(package.manifest);
            }
        }

        void Discard_Config_Transaction(
            s_config_with_mods_transaction* transaction) {
            orion2re::lua::Destroy_Mod_Runtime_Set(transaction->lua_runtimes);
            vfs::Discard_Mount_Transaction(transaction->mounts);
            delete transaction;
        }

        std::string Package_Chain(const s_package_resolver* resolver,
            size_t cycle_start,
            const std::string& repeated_id) {
            std::string chain;
            for (size_t index = cycle_start; index < resolver->stack.size(); ++index) {
                if (!chain.empty()) {
                    chain += " -> ";
                }
                chain += resolver->stack[index];
            }
            chain += " -> " + repeated_id;
            return chain;
        }

        bool Resolve_Package(s_package_resolver* resolver, const std::string& package_id, bool optional, const s_source_location& location);

        bool Resolve_Dependency(s_package_resolver* resolver,
            const s_mod_dependency& dependency,
            bool optional,
            const s_source_location& location) {
            if (!Resolve_Package(resolver, dependency.id, optional, location)) {
                return false;
            }
            const auto found = resolver->package_indexes.find(dependency.id);
            if (found == resolver->package_indexes.end()) {
                return true;
            }
            const s_mod_manifest& manifest = resolver->packages[found->second].manifest;
            if (!Semantic_Version_Matches(manifest.version, dependency.version)) {
                resolver->diagnostic->location = location;
                resolver->diagnostic->message = "package '" + dependency.id + "' version " +
                    manifest.version_text + " does not satisfy '" + dependency.version + "'";
                return false;
            }
            return true;
        }

        bool Resolve_Package(s_package_resolver* resolver, const std::string& package_id, bool optional, const s_source_location& location) {
            for (size_t index = 0; index < resolver->stack.size(); ++index) {
                if (resolver->stack[index] == package_id) {
                    resolver->diagnostic->location = location;
                    resolver->diagnostic->message = "mod dependency cycle: " +
                        Package_Chain(resolver, index, package_id);
                    return false;
                }
            }
            if (resolver->package_indexes.contains(package_id)) {
                return true;
            }

            std::string mount_diagnostic;
            if (vfs::Stage_Package_Mount(resolver->mounts, package_id.c_str(), &mount_diagnostic) == 0) {
                if (optional && mount_diagnostic == "package not found: '" + package_id + "'") {
                    return true;
                }
                resolver->diagnostic->location = location;
                resolver->diagnostic->message = mount_diagnostic;
                return false;
            }

            s_mod_manifest manifest;
            if (!Load_Mod_Manifest(package_id, &manifest, resolver->diagnostic)) {
                return false;
            }

            const size_t package_index = resolver->packages.size();
            resolver->package_indexes.emplace(package_id, package_index);
            resolver->packages.emplace_back();
            resolver->packages.back().manifest = std::move(manifest);
            resolver->stack.push_back(package_id);

            const std::vector<s_mod_dependency> dependencies =
                resolver->packages[package_index].manifest.dependencies;
            const std::vector<s_mod_dependency> optional_dependencies =
                resolver->packages[package_index].manifest.optional_dependencies;
            for (const s_mod_dependency& dependency : dependencies) {
                if (!Resolve_Dependency(resolver, dependency, false, location)) {
                    resolver->stack.pop_back();
                    return false;
                }
            }
            for (const s_mod_dependency& dependency : optional_dependencies) {
                if (!Resolve_Dependency(resolver, dependency, true, location)) {
                    resolver->stack.pop_back();
                    return false;
                }
            }
            resolver->stack.pop_back();
            return true;
        }

        bool Validate_Package_Set(s_package_resolver* resolver) {
            for (size_t package_index = 0; package_index < resolver->packages.size();
                 ++package_index) {
                const s_mod_manifest& manifest = resolver->packages[package_index].manifest;
                for (const std::string& conflict : manifest.conflicts) {
                    if (resolver->package_indexes.contains(conflict)) {
                        resolver->diagnostic->location.source =
                            "mods/" + manifest.id + "/mod.json5";
                        resolver->diagnostic->message = "package '" + manifest.id +
                            "' conflicts with active package '" + conflict + "'";
                        return false;
                    }
                }
                for (const std::string& lower_priority : manifest.load_after) {
                    const auto found = resolver->package_indexes.find(lower_priority);
                    if (found != resolver->package_indexes.end() &&
                        package_index > found->second) {
                        resolver->diagnostic->location.source =
                            "mods/" + manifest.id + "/mod.json5";
                        resolver->diagnostic->message = "package '" + manifest.id +
                            "' must load after '" + lower_priority + "'";
                        return false;
                    }
                }
            }
            return true;
        }

        bool Resolve_Enabled_Packages(const s_cfg_document& base_document,
            const std::vector<s_mod_requirement>* required_packages,
            s_package_resolver* resolver) {
            if (required_packages == nullptr) {
                for (const s_cfg_statement& statement : base_document.statements) {
                    if (statement.kind == CFG_STATEMENT_ENABLE) {
                        Record_Package_Request(resolver, statement.argument.value);
                        if (!Resolve_Package(resolver, statement.argument.value, false, statement.argument.location)) {
                            return false;
                        }
                    }
                }
            } else {
                s_source_location location;
                location.source = "configuration snapshot";
                for (const s_mod_requirement& required : *required_packages) {
                    if (required.id.empty()) {
                        resolver->diagnostic->location = location;
                        resolver->diagnostic->message =
                            "configuration snapshot contains an invalid package ID";
                        return false;
                    }
                    Record_Package_Request(resolver, required.id);
                    if (!Resolve_Package(resolver, required.id, false, location)) {
                        return false;
                    }
                }
            }

            for (size_t package_index = 0; package_index < resolver->packages.size();
                 ++package_index) {
                const std::string package_id = resolver->packages[package_index].manifest.id;
                const std::string config_path = resolver->packages[package_index].manifest.config;
                const std::string entry_path = "mods/" + package_id + "/" + config_path;
                s_cfg_document document;
                if (!resolver->load_document(entry_path, &document, resolver->diagnostic)) {
                    return false;
                }
                std::vector<std::pair<std::string, s_source_location>> nested_enables;
                for (const s_cfg_statement& statement : document.statements) {
                    if (statement.kind == CFG_STATEMENT_ENABLE &&
                        !statement.argument.value.empty()) {
                        nested_enables.emplace_back(statement.argument.value,
                            statement.argument.location);
                    }
                }
                resolver->packages[package_index].document = std::move(document);
                for (const auto& enable : nested_enables) {
                    Record_Package_Request(resolver, enable.first);
                    if (!Resolve_Package(resolver, enable.first, false, enable.second)) {
                        return false;
                    }
                }
            }

            Normalize_Package_Priority(resolver);
            if (required_packages != nullptr) {
                if (resolver->packages.size() != required_packages->size()) {
                    resolver->diagnostic->location.source = "configuration snapshot";
                    resolver->diagnostic->message =
                        "configuration snapshot package order does not match installed dependencies";
                    return false;
                }
                for (size_t index = 0; index < resolver->packages.size(); ++index) {
                    const s_mod_manifest& installed =
                        resolver->packages[index].manifest;
                    const s_mod_requirement& required = (*required_packages)[index];
                    if (installed.id != required.id) {
                        resolver->diagnostic->location.source = "configuration snapshot";
                        resolver->diagnostic->message =
                            "configuration snapshot package order does not match installed dependencies";
                        return false;
                    }
                    if (required.version.empty()) continue;
                    s_semantic_version required_version;
                    if (!Parse_Semantic_Version(required.version, &required_version)) {
                        resolver->diagnostic->location.source = "configuration snapshot";
                        resolver->diagnostic->message = "configuration snapshot contains invalid "
                                                        "version '" +
                            required.version + "' for package '" + required.id + "'";
                        return false;
                    }
                    if (!Semantic_Versions_Equal(
                            installed.version_text, required.version) &&
                        (installed.compatible_versions.empty() ||
                            !Semantic_Version_Matches(
                                required_version, installed.compatible_versions))) {
                        resolver->diagnostic->location.source =
                            "mods/" + installed.id + "/mod.json5";
                        resolver->diagnostic->message = "installed package '" + installed.id +
                            "' version " + installed.version_text +
                            " is not compatible with required version " + required.version;
                        return false;
                    }
                }
            }
            std::vector<std::string> package_order;
            package_order.reserve(resolver->packages.size());
            for (const s_enabled_package& package : resolver->packages) {
                package_order.push_back(package.manifest.id);
            }
            std::string order_diagnostic;
            if (vfs::Order_Package_Mounts(resolver->mounts, package_order, &order_diagnostic) == 0) {
                resolver->diagnostic->message = order_diagnostic.empty() ? "unable to order package mounts" : order_diagnostic;
                return false;
            }
            return Validate_Package_Set(resolver);
        }

        void Get_Package_Order(const s_package_resolver& resolver,
            std::vector<std::string>* package_order) {
            package_order->clear();
            package_order->reserve(resolver.packages.size());
            for (const s_enabled_package& package : resolver.packages) {
                package_order->push_back(package.manifest.id);
            }
        }

        void Get_Resolved_Packages(const s_package_resolver& resolver,
            std::vector<s_mod_requirement>* packages) {
            packages->clear();
            packages->reserve(resolver.packages.size());
            for (const s_enabled_package& package : resolver.packages) {
                packages->push_back({
                    package.manifest.id,
                    package.manifest.version_text,
                });
            }
        }

        bool Build_Candidate(const s_cfg_document& base_document,
            const char* package_id,
            const s_package_resolver& resolver,
            s_effective_config* candidate,
            s_cfg_diagnostic* diagnostic,
            e_config_precedence precedence) {
            s_config_profile profile("build-candidate");
            std::string key = package_id;
            key += precedence == CONFIG_PRECEDENCE_ENTRY_OVERRIDES_PACKAGES
                ? "entry-wins;"
                : "packages-win;";
            Append_Document_Key(base_document, &key);
            for (const auto& package : resolver.packages) {
                key += package.manifest.id + ":" + package.manifest.version_text + ";";
                Append_Document_Key(package.document, &key);
            }
            if (key == package_baseline_cache.key &&
                package_baseline_cache.effective.state.values.Shares_Baseline(Get_Default_State()->values)) {
                *candidate = package_baseline_cache.effective;
                return true;
            }
            if (!Initialize_Effective_Config(candidate)) {
                diagnostic->message = "unable to initialize configuration defaults";
                return false;
            }
            // The entry document and the package documents apply in
            // the order the precedence asks for. When the entry wins,
            // the packages go first and the entry last: the entry is the
            // one config (the user's file once one exists, the shipped
            // file otherwise), so a value the user set there wins over a
            // mod's assignment of the same path, and enabling a mod no
            // longer silently reverts a setting the user changed earlier
            // in the session. A mod still provides every value the entry
            // leaves unset, and packages still order among themselves by
            // priority below. When the packages win, the entry goes
            // first: the config being built belongs to a world being
            // loaded (a save, a network peer), whose packages define it.
            const auto apply_packages = [&]() {
                for (size_t index = resolver.packages.size(); index > 0; --index) {
                    const s_enabled_package& package = resolver.packages[index - 1];
                    if (!Apply_Config_Document_To_Candidate(&package.document,
                            package.manifest.id.c_str(),
                            candidate,
                            diagnostic)) {
                        return false;
                    }
                }
                return true;
            };
            const auto apply_entry = [&]() {
                return Apply_Config_Document_To_Candidate(
                    &base_document, package_id, candidate, diagnostic);
            };
            if (precedence == CONFIG_PRECEDENCE_ENTRY_OVERRIDES_PACKAGES) {
                if (!apply_packages() || !apply_entry()) {
                    return false;
                }
            } else if (!apply_entry() || !apply_packages()) {
                return false;
            }
            // Saved or synchronized values may replace invalid local combinations.
            package_baseline_cache = {std::move(key), *candidate};
            return true;
        }

        bool Overlay_Snapshot(const s_config_snapshot_descriptor* descriptor,
            const s_config_state* state,
            s_effective_config* candidate,
            s_cfg_diagnostic* diagnostic) {
            if (descriptor == nullptr && state == nullptr) return true;
            if (descriptor == nullptr || state == nullptr ||
                descriptor->variables == nullptr || descriptor->variable_count == 0 ||
                state->values.size() != variable_count) {
                diagnostic->message = "invalid configuration snapshot";
                return false;
            }
            if (!Overlay_Config_State(descriptor, state, &candidate->state)) {
                diagnostic->message = "invalid configuration snapshot state";
                return false;
            }
            for (uint32_t index = 0;
                 descriptor != &saved_config_snapshot && descriptor != &synchronized_config_snapshot && index < descriptor->variable_count;
                 ++index) {
                const t_variable_id variable_id = descriptor->variables[index];
                if (variable_id >= variable_count ||
                    (descriptor->group_mask &
                        (UINT32_C(1) << Get_Variable_Group(variable_id))) == 0) {
                    diagnostic->message = "invalid configuration snapshot descriptor";
                    return false;
                }
            }
            const auto included = [&](size_t id) {
                return Snapshot_Contains_Variable(descriptor, static_cast<t_variable_id>(id));
            };
            candidate->provenance.Erase_Stored_If(included);
            candidate->prior_values.Erase_Stored_If(included);
            return true;
        }
    } // namespace

    bool Apply_Config_With_Mods(const s_cfg_document& base_document,
        const char* package_id,
        s_effective_config* effective,
        s_cfg_diagnostic* diagnostic,
        t_config_document_loader load_document,
        std::vector<std::string>* package_order,
        t_mount_transaction_commit commit_mounts,
        const std::vector<s_mod_requirement>* required_packages,
        const s_config_snapshot_descriptor* snapshot_descriptor,
        const s_config_state* snapshot_state,
        std::vector<s_mod_requirement>* resolved_packages,
        const orion2re::lua::s_lua_save_snapshot* lua_snapshot,
        const s_config_state* prior_state,
        const s_effective_config* prepared_base,
        e_config_precedence precedence) {
        if (package_id == nullptr || effective == nullptr || diagnostic == nullptr ||
            load_document == nullptr) {
            return false;
        }

        s_package_resolver resolver;
        resolver.diagnostic = diagnostic;
        resolver.load_document = load_document;
        const bool reuse = prior_state != nullptr && commit_mounts == nullptr && Reuse_Active_Packages(required_packages, &resolver);
        if (!reuse) *diagnostic = {};
        s_vfs_mount_transaction* mounts = reuse ? nullptr : vfs::Begin_Mount_Transaction();
        if (!reuse && mounts == nullptr) {
            diagnostic->message = "unable to begin VFS mount transaction";
            return false;
        }
        resolver.mounts = mounts;
        if (!reuse && !Resolve_Enabled_Packages(base_document, required_packages, &resolver)) {
            vfs::Discard_Mount_Transaction(mounts);
            return false;
        }

        s_effective_config candidate;
        if (prepared_base != nullptr) candidate = *prepared_base;
        if (prepared_base == nullptr && !Build_Candidate(base_document, package_id, resolver, &candidate, diagnostic, precedence)) {
            vfs::Discard_Mount_Transaction(mounts);
            return false;
        }
        if (!Overlay_Snapshot(
                snapshot_descriptor, snapshot_state, &candidate, diagnostic) ||
            !Validate_State(&candidate.state)) {
            if (diagnostic->message.empty()) {
                diagnostic->message = "unable to validate configuration state";
            }
            vfs::Discard_Mount_Transaction(mounts);
            return false;
        }

        std::vector<s_mod_manifest> manifests;
        Get_Package_Manifests(resolver, &manifests);
        t_loaded_mod_list loaded_mods;
        Get_Loaded_Mods(resolver, &loaded_mods);
        orion2re::lua::s_lua_registry lua_registry;
        if (!orion2re::lua::Stage_Registry(
                &manifests, &lua_registry, diagnostic)) {
            vfs::Discard_Mount_Transaction(mounts);
            return false;
        }
        orion2re::lua::s_lua_mod_runtime_set* lua_runtimes = nullptr;
        const orion2re::lua::s_lua_mod_limits lua_mod_limits;
        bool staged_lua = false;
        {
            s_config_profile profile("stage-lua");
            staged_lua = orion2re::lua::Stage_Mod_Runtimes(&manifests, &lua_mod_limits, &lua_registry, &lua_runtimes, diagnostic);
        }
        if (!staged_lua) {
            vfs::Discard_Mount_Transaction(mounts);
            return false;
        }
        std::string fingerprint_diagnostic;
        if (!orion2re::lua::Finalize_Registry_Fingerprint(
                &lua_registry, &candidate.state, &fingerprint_diagnostic)) {
            diagnostic->message = fingerprint_diagnostic.empty() ? "unable to build Lua script-set fingerprint" : fingerprint_diagnostic;
            orion2re::lua::Destroy_Mod_Runtime_Set(lua_runtimes);
            vfs::Discard_Mount_Transaction(mounts);
            return false;
        }
        if (lua_snapshot != nullptr) {
            if (!orion2re::lua::Restore_Lua_Save_Snapshot(lua_snapshot,
                    &lua_registry,
                    lua_runtimes,
                    &fingerprint_diagnostic)) {
                diagnostic->message = fingerprint_diagnostic.empty()
                    ? "unable to restore Lua save snapshot"
                    : fingerprint_diagnostic;
                orion2re::lua::Destroy_Mod_Runtime_Set(lua_runtimes);
                vfs::Discard_Mount_Transaction(mounts);
                return false;
            }
        }

        const bool default_mount_commit = !reuse && commit_mounts == nullptr;
        if (default_mount_commit &&
            !vfs::Begin_Mount_Transaction_Commit(mounts)) {
            diagnostic->message = "unable to commit VFS mount transaction";
            orion2re::lua::Destroy_Mod_Runtime_Set(lua_runtimes);
            vfs::Discard_Mount_Transaction(mounts);
            return false;
        }
        if (!reuse && !default_mount_commit && commit_mounts(mounts) == 0) {
            diagnostic->message = "unable to commit VFS mount transaction";
            orion2re::lua::Destroy_Mod_Runtime_Set(lua_runtimes);
            vfs::Discard_Mount_Transaction(mounts);
            return false;
        }
        static_assert(CONFIG_GROUP_COUNT <= 32);
        const uint32_t group_mask = CONFIG_GROUP_COUNT == 32
            ? UINT32_MAX
            : (UINT32_C(1) << CONFIG_GROUP_COUNT) - 1;
        const bool committed = prior_state == nullptr
            ? Commit_Initial_State(&candidate.state)
            : Commit_Config_Transition(
                  group_mask, prior_state, &candidate.state);
        if (!committed) {
            if (default_mount_commit) {
                vfs::Rollback_Mount_Transaction_Commit(mounts);
            }
            diagnostic->message = "unable to commit configuration state";
            orion2re::lua::Destroy_Mod_Runtime_Set(lua_runtimes);
            if (default_mount_commit) {
                vfs::Discard_Mount_Transaction(mounts);
            }
            return false;
        }
        if (default_mount_commit) {
            vfs::Finalize_Mount_Transaction_Commit(mounts);
        }
        if (!reuse) Cache_Active_Packages(resolver);
        orion2re::lua::Commit_Registry(&lua_registry);
        orion2re::lua::Commit_Mod_Runtimes(&lua_runtimes);
        active_loaded_mods = std::move(loaded_mods);
        if (package_order != nullptr) Get_Package_Order(resolver, package_order);
        if (resolved_packages != nullptr) {
            Get_Resolved_Packages(resolver, resolved_packages);
        }
        *effective = std::move(candidate);
        CONFIG_REFRESH::Refresh_After_Config_Reload();
        return true;
    }

    bool Stage_Config_With_Mods(const s_cfg_document& base_document,
        const char* package_id,
        s_effective_config* candidate,
        std::vector<std::string>* package_order,
        s_cfg_diagnostic* diagnostic,
        t_config_document_loader load_document,
        std::vector<s_mod_requirement>* resolved_packages,
        e_config_precedence precedence) {
        if (package_id == nullptr || candidate == nullptr || package_order == nullptr ||
            diagnostic == nullptr || load_document == nullptr) {
            return false;
        }
        s_vfs_mount_transaction* mounts = vfs::Begin_Mount_Transaction();
        if (mounts == nullptr) {
            diagnostic->message = "unable to begin VFS mount transaction";
            return false;
        }
        s_package_resolver resolver;
        resolver.mounts = mounts;
        resolver.diagnostic = diagnostic;
        resolver.load_document = load_document;
        if (!Resolve_Enabled_Packages(base_document, nullptr, &resolver) ||
            !Build_Candidate(base_document, package_id, resolver, candidate, diagnostic, precedence) ||
            !Validate_State(&candidate->state)) {
            if (diagnostic->message.empty()) {
                diagnostic->message = "unable to validate configuration state";
            }
            vfs::Discard_Mount_Transaction(mounts);
            return false;
        }
        Get_Package_Order(resolver, package_order);
        if (resolved_packages != nullptr) {
            Get_Resolved_Packages(resolver, resolved_packages);
        }
        vfs::Discard_Mount_Transaction(mounts);
        return true;
    }

    bool Stage_Config_With_Mods_Transaction(const s_cfg_document& base_document,
        const char* package_id,
        s_cfg_diagnostic* diagnostic,
        t_config_document_loader load_document,
        const std::vector<s_mod_requirement>* required_packages,
        const s_config_snapshot_descriptor* snapshot_descriptor,
        const s_config_state* snapshot_state,
        s_config_with_mods_transaction** transaction,
        const s_effective_config* prepared_base,
        e_config_precedence precedence) {
        if (package_id == nullptr || diagnostic == nullptr ||
            load_document == nullptr || transaction == nullptr) {
            return false;
        }
        *transaction = nullptr;
        auto* staged = new (std::nothrow) s_config_with_mods_transaction;
        if (staged == nullptr) {
            diagnostic->message = "unable to allocate configuration transaction";
            return false;
        }
        s_package_resolver resolver;
        resolver.diagnostic = diagnostic;
        resolver.load_document = load_document;
        staged->mounts = vfs::Begin_Mount_Transaction();
        if (staged->mounts == nullptr) {
            diagnostic->message = "unable to begin VFS mount transaction";
            delete staged;
            return false;
        }
        resolver.mounts = staged->mounts;
        if (prepared_base != nullptr) staged->effective = *prepared_base;
        if (!Resolve_Enabled_Packages(
                base_document, required_packages, &resolver) ||
            (prepared_base == nullptr && !Build_Candidate(base_document,
                package_id,
                resolver,
                &staged->effective,
                diagnostic,
                precedence)) ||
            !Overlay_Snapshot(snapshot_descriptor,
                snapshot_state,
                &staged->effective,
                diagnostic) ||
            !Validate_State(&staged->effective.state)) {
            if (diagnostic->message.empty()) {
                diagnostic->message = "unable to validate configuration state";
            }
            Discard_Config_Transaction(staged);
            return false;
        }
        std::vector<s_mod_manifest> manifests;
        Get_Package_Manifests(resolver, &manifests);
        if (!orion2re::lua::Stage_Registry(
                &manifests,
                &staged->lua_registry,
                diagnostic)) {
            Discard_Config_Transaction(staged);
            return false;
        }
        const orion2re::lua::s_lua_mod_limits runtime_limits;
        if (!orion2re::lua::Stage_Mod_Runtimes(&manifests,
                &runtime_limits,
                &staged->lua_registry,
                &staged->lua_runtimes,
                diagnostic)) {
            Discard_Config_Transaction(staged);
            return false;
        }
        std::string fingerprint_diagnostic;
        if (!orion2re::lua::Finalize_Registry_Fingerprint(
                &staged->lua_registry,
                &staged->effective.state,
                &fingerprint_diagnostic)) {
            diagnostic->message = fingerprint_diagnostic.empty()
                ? "unable to build Lua script-set fingerprint"
                : fingerprint_diagnostic;
            Discard_Config_Transaction(staged);
            return false;
        }
        Get_Package_Order(resolver, &staged->package_order);
        Get_Resolved_Packages(resolver, &staged->resolved_packages);
        Get_Loaded_Mods(resolver, &staged->loaded_mods);
        *transaction = staged;
        return true;
    }

    const orion2re::lua::s_lua_registry* Get_Staged_Config_Registry(
        const s_config_with_mods_transaction* transaction) {
        return transaction == nullptr ? nullptr : &transaction->lua_registry;
    }

    bool Restore_Lua_Save_To_Config_Transaction(
        s_config_with_mods_transaction* transaction,
        const orion2re::lua::s_lua_save_snapshot* snapshot,
        const s_config_state* prior_state,
        std::string* diagnostic) {
        if (transaction == nullptr || snapshot == nullptr ||
            diagnostic == nullptr || prior_state == nullptr ||
            prior_state->values.size() != variable_count ||
            !State_Tracks_Non_Default_Values(prior_state)) {
            return false;
        }
        // The loader owns the authoritative state, including recorded native
        // writes. Retain a sparse copy across candidate callbacks and rollback.
        const s_config_state prior = *prior_state;
        static_assert(CONFIG_GROUP_COUNT <= 32);
        const uint32_t group_mask = CONFIG_GROUP_COUNT == 32
            ? UINT32_MAX
            : (UINT32_C(1) << CONFIG_GROUP_COUNT) - 1;
        if (!Commit_Config_Transition(group_mask,
                &prior,
                &transaction->effective.state)) {
            *diagnostic = "unable to stage network configuration state";
            return false;
        }
        const bool restored = orion2re::lua::Restore_Lua_Save_Snapshot(snapshot,
            &transaction->lua_registry,
            transaction->lua_runtimes,
            diagnostic,
            false,
            true);
        if (!Commit_Config_Transition(group_mask,
                &transaction->effective.state,
                &prior)) {
            *diagnostic = "unable to restore active configuration state";
            return false;
        }
        return restored;
    }

    bool Commit_Config_With_Mods_Transaction(
        s_config_with_mods_transaction** transaction,
        s_effective_config* effective,
        std::vector<std::string>* package_order,
        std::vector<s_mod_requirement>* resolved_packages,
        s_cfg_diagnostic* diagnostic) {
        if (transaction == nullptr || *transaction == nullptr ||
            effective == nullptr || package_order == nullptr ||
            resolved_packages == nullptr || diagnostic == nullptr) {
            return false;
        }
        s_config_with_mods_transaction* staged = *transaction;
        if (!vfs::Can_Commit_Mount_Transaction(staged->mounts)) {
            diagnostic->message = "unable to commit VFS mount transaction";
            return false;
        }
        if (!vfs::Begin_Mount_Transaction_Commit(staged->mounts)) {
            diagnostic->message = "unable to begin VFS mount commit";
            return false;
        }
        static_assert(CONFIG_GROUP_COUNT <= 32);
        const uint32_t group_mask = CONFIG_GROUP_COUNT == 32
            ? UINT32_MAX
            : (UINT32_C(1) << CONFIG_GROUP_COUNT) - 1;
        if (!Commit_Config_Transition(group_mask,
                &effective->state,
                &staged->effective.state)) {
            vfs::Rollback_Mount_Transaction_Commit(staged->mounts);
            diagnostic->message = "unable to commit configuration state";
            return false;
        }
        vfs::Finalize_Mount_Transaction_Commit(staged->mounts);
        staged->mounts = nullptr;
        orion2re::lua::Commit_Registry(&staged->lua_registry);
        orion2re::lua::Commit_Mod_Runtimes(&staged->lua_runtimes);
        active_loaded_mods = std::move(staged->loaded_mods);
        active_package_cache_ready = false;
        active_packages.clear();
        active_package_stamps.clear();
        active_manifest_sources.clear();
        *effective = std::move(staged->effective);
        *package_order = std::move(staged->package_order);
        *resolved_packages = std::move(staged->resolved_packages);
        delete staged;
        *transaction = nullptr;
        CONFIG_REFRESH::Refresh_After_Config_Reload();
        return true;
    }

    bool Commit_Reconstructed_Lua_Runtimes_Transaction(
        s_config_with_mods_transaction** transaction,
        s_cfg_diagnostic* diagnostic) {
        if (transaction == nullptr || *transaction == nullptr ||
            diagnostic == nullptr) {
            return false;
        }
        s_config_with_mods_transaction* staged = *transaction;
        if (!vfs::Can_Commit_Mount_Transaction(staged->mounts) ||
            !vfs::Begin_Mount_Transaction_Commit(staged->mounts)) {
            diagnostic->message =
                "unable to commit reconstructed Lua package mounts";
            return false;
        }
        vfs::Finalize_Mount_Transaction_Commit(staged->mounts);
        staged->mounts = nullptr;
        orion2re::lua::Commit_Registry(&staged->lua_registry);
        orion2re::lua::Commit_Mod_Runtimes(&staged->lua_runtimes);
        active_loaded_mods = std::move(staged->loaded_mods);
        active_package_cache_ready = false;
        active_packages.clear();
        active_package_stamps.clear();
        active_manifest_sources.clear();
        delete staged;
        *transaction = nullptr;
        CONFIG_REFRESH::Request_Resource_Reload();
        return true;
    }

    void Discard_Config_With_Mods_Transaction(
        s_config_with_mods_transaction** transaction) {
        if (transaction == nullptr || *transaction == nullptr) {
            return;
        }
        s_config_with_mods_transaction* staged = *transaction;
        orion2re::lua::Destroy_Mod_Runtime_Set(staged->lua_runtimes);
        vfs::Discard_Mount_Transaction(staged->mounts);
        delete staged;
        *transaction = nullptr;
    }

    const t_loaded_mod_list& Get_Loaded_Mods() {
        return active_loaded_mods;
    }

    const s_loaded_mod* Find_Loaded_Mod(std::string_view package_id) {
        for (const s_loaded_mod& loaded_mod : active_loaded_mods) {
            if (loaded_mod.manifest.id == package_id) {
                return &loaded_mod;
            }
        }
        return nullptr;
    }

    namespace internal {
        void Reset_Loaded_Mods() {
            active_package_cache_ready = false;
            active_loaded_mods.clear();
            package_baseline_cache = {};
            active_packages.clear();
            active_package_stamps.clear();
            active_manifest_sources.clear();
        }
    } // namespace internal

} // namespace config