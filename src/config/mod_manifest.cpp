#include "mod_manifest.h"

#include <cctype>
#include <cstring>
#include <string_view>
#include <tuple>

#include "game/vfs.h"
#include "version.h"
#include "yyjson.h"

namespace config {
    namespace {
        constexpr size_t MOD_MANIFEST_MAX_SIZE = 1024 * 1024;

        bool Fail(const std::string& path, const std::string& message, s_cfg_diagnostic* diagnostic);

        std::tuple<int64_t, int64_t, int64_t> Semantic_Version_Tuple(
            const s_semantic_version& version) {
            return std::tuple(
                Semantic_Version_Component_Value(version.major),
                Semantic_Version_Component_Value(version.minor),
                Semantic_Version_Component_Value(version.patch));
        }

        bool Is_Identifier(std::string_view value) {
            if (value.empty() || value.front() < 'a' || value.front() > 'z') {
                return false;
            }
            for (const char character : value) {
                if (!((character >= 'a' && character <= 'z') ||
                        (character >= '0' && character <= '9') ||
                        character == '_' || character == '-')) {
                    return false;
                }
            }
            return true;
        }

        bool Read_Vfs_File(const std::string& path, std::string* source, s_cfg_diagnostic* diagnostic) {
            s_vfs_file* file = vfs::fopen(path.c_str(), "rb");
            if (file == nullptr) {
                diagnostic->location.source = path;
                diagnostic->message = "mod manifest not found";
                return false;
            }
            const int32_t size = vfs::fsize(file);
            if (size < 0 || static_cast<size_t>(size) > MOD_MANIFEST_MAX_SIZE) {
                vfs::fclose(file);
                diagnostic->location.source = path;
                diagnostic->message = "mod manifest has an invalid size";
                return false;
            }
            source->resize(static_cast<size_t>(size));
            const size_t read = source->empty() ? 0 : vfs::fread(source->data(), 1, source->size(), file);
            const int32_t close_result = vfs::fclose(file);
            if (read != source->size() || close_result != 0) {
                diagnostic->location.source = path;
                diagnostic->message = "unable to read mod manifest";
                return false;
            }
            return true;
        }

        bool Get_Required_Path_String(yyjson_val* object, const char* name, const std::string& property_path, const std::string& path, std::string* output, s_cfg_diagnostic* diagnostic) {
            yyjson_val* value = yyjson_obj_get(object, name);
            if (!yyjson_is_str(value) || yyjson_get_len(value) == 0) {
                return Fail(path, "manifest property '" + property_path + "' must be a non-empty string", diagnostic);
            }
            output->assign(yyjson_get_str(value), yyjson_get_len(value));
            return true;
        }

        bool Validate_Object_Properties(yyjson_val* object,
            const std::vector<std::string_view>& known,
            const std::string& property_path,
            const std::string& path,
            s_cfg_diagnostic* diagnostic) {
            size_t index = 0;
            size_t count = 0;
            yyjson_val* key = nullptr;
            yyjson_val* value = nullptr;
            yyjson_obj_foreach(object, index, count, key, value) {
                const std::string name(yyjson_get_str(key), yyjson_get_len(key));
                bool found = false;
                for (const std::string_view candidate : known) {
                    found = found || name == candidate;
                }
                if (!found) {
                    return Fail(path, "unknown manifest property '" + property_path + name + "'", diagnostic);
                }
            }
            return true;
        }

        bool Fail(const std::string& path, const std::string& message, s_cfg_diagnostic* diagnostic) {
            diagnostic->location.source = path;
            diagnostic->location.line = 1;
            diagnostic->location.column = 1;
            diagnostic->message = message;
            return false;
        }

        bool Validate_Unique_Keys(yyjson_val* value, const std::string& path, const std::string& value_path, s_cfg_diagnostic* diagnostic, size_t depth) {
            if (depth > 64) {
                return Fail(path, "mod manifest nesting exceeds 64 levels", diagnostic);
            }
            if (yyjson_is_obj(value)) {
                std::vector<std::string> keys;
                size_t index = 0;
                size_t count = 0;
                yyjson_val* key = nullptr;
                yyjson_val* child = nullptr;
                yyjson_obj_foreach(value, index, count, key, child) {
                    const std::string name(yyjson_get_str(key), yyjson_get_len(key));
                    for (const std::string& prior : keys) {
                        if (prior == name) {
                            return Fail(path, "duplicate manifest property '" + value_path + name + "'", diagnostic);
                        }
                    }
                    keys.push_back(name);
                    if (!Validate_Unique_Keys(child, path, value_path + name + ".", diagnostic, depth + 1)) {
                        return false;
                    }
                }
            } else if (yyjson_is_arr(value)) {
                size_t index = 0;
                size_t count = 0;
                yyjson_val* child = nullptr;
                yyjson_arr_foreach(value, index, count, child) {
                    std::string indexed_path = value_path;
                    if (!indexed_path.empty() && indexed_path.back() == '.') {
                        indexed_path.pop_back();
                    }
                    indexed_path += "[" + std::to_string(index) + "].";
                    if (!Validate_Unique_Keys(child, path, indexed_path, diagnostic, depth + 1)) {
                        return false;
                    }
                }
            }
            return true;
        }

        bool Get_Required_String(yyjson_val* object, const char* name, const std::string& path, std::string* output, s_cfg_diagnostic* diagnostic) {
            yyjson_val* value = yyjson_obj_get(object, name);
            if (!yyjson_is_str(value) || yyjson_get_len(value) == 0) {
                return Fail(path, std::string("manifest property '") + name + "' must be a non-empty string", diagnostic);
            }
            output->assign(yyjson_get_str(value), yyjson_get_len(value));
            return true;
        }

        bool Read_String_Array(yyjson_val* object, const char* name, const std::string& path, std::vector<std::string>* output, s_cfg_diagnostic* diagnostic) {
            yyjson_val* array = yyjson_obj_get(object, name);
            if (array == nullptr) {
                return true;
            }
            if (!yyjson_is_arr(array)) {
                return Fail(path, std::string("manifest property '") + name + "' must be an array", diagnostic);
            }
            size_t index = 0;
            size_t count = 0;
            yyjson_val* value = nullptr;
            yyjson_arr_foreach(array, index, count, value) {
                if (!yyjson_is_str(value) || yyjson_get_len(value) == 0) {
                    return Fail(path, std::string("manifest property '") + name + "' contains a non-string value", diagnostic);
                }
                std::string item(yyjson_get_str(value), yyjson_get_len(value));
                if (!Is_Identifier(item)) {
                    return Fail(path, "invalid package ID '" + item + "'", diagnostic);
                }
                for (const std::string& prior : *output) {
                    if (prior == item) {
                        return Fail(path, std::string("duplicate '") + name + "' entry '" + item + "'", diagnostic);
                    }
                }
                output->push_back(std::move(item));
            }
            return true;
        }

        bool Read_Dependencies(yyjson_val* object, const char* name, const std::string& path, std::vector<s_mod_dependency>* output, s_cfg_diagnostic* diagnostic) {
            yyjson_val* array = yyjson_obj_get(object, name);
            if (array == nullptr) {
                return true;
            }
            if (!yyjson_is_arr(array)) {
                return Fail(path, std::string("manifest property '") + name + "' must be an array", diagnostic);
            }
            size_t index = 0;
            size_t count = 0;
            yyjson_val* value = nullptr;
            yyjson_arr_foreach(array, index, count, value) {
                if (!yyjson_is_obj(value)) {
                    return Fail(path, std::string("manifest property '") + name + "' contains a non-object value", diagnostic);
                }
                if (yyjson_obj_size(value) != 2) {
                    return Fail(path, std::string("manifest property '") + name + "' entries must contain only 'id' and 'version'", diagnostic);
                }
                s_mod_dependency dependency;
                if (!Get_Required_String(value, "id", path, &dependency.id, diagnostic) ||
                    !Get_Required_String(value, "version", path, &dependency.version, diagnostic)) {
                    return false;
                }
                if (!Is_Identifier(dependency.id)) {
                    return Fail(path, "invalid package ID '" + dependency.id + "'", diagnostic);
                }
                if (!Validate_Semantic_Version_Range(dependency.version)) {
                    return Fail(path, "invalid package version range '" + dependency.version + "'", diagnostic);
                }
                for (const s_mod_dependency& prior : *output) {
                    if (prior.id == dependency.id) {
                        return Fail(path, std::string("duplicate '") + name + "' entry '" + dependency.id + "'", diagnostic);
                    }
                }
                output->push_back(std::move(dependency));
            }
            return true;
        }

        bool Read_Required_U32(yyjson_val* object, const char* name,
            const std::string& property_path, const std::string& path,
            uint32_t* output, s_cfg_diagnostic* diagnostic) {
            yyjson_val* value = yyjson_obj_get(object, name);
            if (!yyjson_is_uint(value) || yyjson_get_uint(value) > UINT32_MAX) {
                return Fail(path, "manifest property '" + property_path +
                        "' must be an unsigned 32-bit integer",
                    diagnostic);
            }
            *output = static_cast<uint32_t>(yyjson_get_uint(value));
            return true;
        }

        bool Read_Lua_Migrations(yyjson_val* lua, const std::string& path,
            s_lua_runtime_manifest* output, s_cfg_diagnostic* diagnostic) {
            yyjson_val* migrations = yyjson_obj_get(lua, "migrations");
            if (migrations == nullptr) {
                return true;
            }
            if (!yyjson_is_arr(migrations)) {
                return Fail(path,
                    "manifest property 'lua.migrations' must be an array",
                    diagnostic);
            }
            size_t index = 0;
            size_t count = 0;
            yyjson_val* value = nullptr;
            yyjson_arr_foreach(migrations, index, count, value) {
                const std::string property_path =
                    "lua.migrations[" + std::to_string(index) + "]";
                if (!yyjson_is_obj(value)) {
                    return Fail(path, "manifest property '" + property_path+
                            "' must be an object",
                        diagnostic);
                }
                if (!Validate_Object_Properties(value,
                        {"from", "to", "script"}, property_path + ".", path,
                        diagnostic)) {
                    return false;
                }
                s_lua_migration_manifest migration;
                if (!Read_Required_U32(value, "from", property_path + ".from",
                        path, &migration.from, diagnostic) ||
                    !Read_Required_U32(value, "to", property_path + ".to",
                        path, &migration.to, diagnostic) ||
                    !Get_Required_Path_String(value, "script",
                        property_path + ".script", path, &migration.script,
                        diagnostic)) {
                    return false;
                }
                if (migration.from >= migration.to) {
                    return Fail(path, "manifest property '" + property_path+
                            "' must increase the storage version",
                        diagnostic);
                }
                if (migration.to > output->storage_version) {
                    return Fail(path, "manifest property '" + property_path+
                            ".to' exceeds lua.storage_version",
                        diagnostic);
                }
                char canonical[1024];
                if (vfs::Canonicalize_Asset_Path(migration.script.c_str(),
                        canonical, sizeof(canonical)) == 0 ||
                    migration.script != canonical ||
                    migration.script.size() < 4 ||
                    migration.script.substr(migration.script.size() - 4) !=
                        ".lua") {
                    return Fail(path, "invalid Lua migration script path '"+
                            migration.script + "'",
                        diagnostic);
                }
                for (const s_lua_migration_manifest& prior :
                    output->migrations) {
                    if (prior.from == migration.from) {
                        return Fail(path,
                            "duplicate Lua migration from storage version " +
                                std::to_string(migration.from),
                            diagnostic);
                    }
                }
                output->migrations.push_back(std::move(migration));
            }
            for (size_t migration_index = 1;
                 migration_index < output->migrations.size();
                 ++migration_index) {
                if (output->migrations[migration_index - 1].to !=
                    output->migrations[migration_index].from) {
                    return Fail(path,
                        "manifest property 'lua.migrations' must declare one ordered version chain",
                        diagnostic);
                }
            }
            if (!output->migrations.empty() &&
                output->migrations.back().to != output->storage_version) {
                return Fail(path,
                    "manifest property 'lua.migrations' must end at lua.storage_version",
                    diagnostic);
            }
            return true;
        }

        bool Read_Lua_Runtime(yyjson_val* root, const std::string& path,
            s_mod_manifest* manifest, s_cfg_diagnostic* diagnostic) {
            yyjson_val* lua = yyjson_obj_get(root, "lua");
            if (lua == nullptr) {
                return true;
            }
            if (!yyjson_is_obj(lua)) {
                return Fail(path,
                    "manifest property 'lua' must be an object", diagnostic);
            }
            if (!Validate_Object_Properties(lua,
                    {"api", "storage_version", "migrations"},
                    "lua.",
                    path,
                    diagnostic)) {
                return false;
            }
            uint32_t api = 0;
            if (!Read_Required_U32(lua, "api", "lua.api", path, &api,
                    diagnostic)) {
                return false;
            }
            if (api != 1) {
                return Fail(path,
                    "manifest property 'lua.api' must be the supported integer API version 1",
                    diagnostic);
            }
            if (!Read_Required_U32(lua, "storage_version",
                    "lua.storage_version", path,
                    &manifest->lua_runtime.storage_version, diagnostic)) {
                return false;
            }
            if (manifest->lua_runtime.storage_version == 0) {
                return Fail(path,
                    "manifest property 'lua.storage_version' must be nonzero",
                    diagnostic);
            }
            manifest->has_lua_runtime = true;
            manifest->lua_runtime.api = api;
            return Read_Lua_Migrations(lua, path, &manifest->lua_runtime, diagnostic);
        }

        bool Validate_Properties(yyjson_val* object, const std::string& path, s_cfg_diagnostic* diagnostic) {
            static constexpr const char* known[] = {
                "id",
                "name",
                "description",
                "version",
                "compatible_versions",
                "requires",
                "dependencies",
                "optional_dependencies",
                "conflicts",
                "load_after",
                "config",
                "lua",
            };
            size_t index = 0;
            size_t count = 0;
            yyjson_val* key = nullptr;
            yyjson_val* value = nullptr;
            yyjson_obj_foreach(object, index, count, key, value) {
                const std::string name(yyjson_get_str(key), yyjson_get_len(key));
                bool found = false;
                for (const char* candidate : known) {
                    found = found || name == candidate;
                }
                if (!found) {
                    return Fail(path, "unknown manifest property '" + name + "'", diagnostic);
                }
            }
            return true;
        }
    } // namespace

    bool Semantic_Version_Matches(const s_semantic_version& version,
        const std::string& range) {
        if (range.empty()) {
            return false;
        }
        size_t cursor = 0;
        bool matched_token = false;
        while (cursor < range.size()) {
            while (cursor < range.size() &&
                std::isspace(static_cast<unsigned char>(range[cursor]))) {
                ++cursor;
            }
            if (cursor == range.size()) {
                break;
            }
            size_t end = cursor;
            while (end < range.size() &&
                !std::isspace(static_cast<unsigned char>(range[end]))) {
                ++end;
            }
            std::string_view token(range.data() + cursor, end - cursor);
            std::string_view operation = "=";
            if (token.starts_with(">=") || token.starts_with("<=")) {
                operation = token.substr(0, 2);
                token.remove_prefix(2);
            } else if (token.starts_with('>') || token.starts_with('<') ||
                token.starts_with('=')) {
                operation = token.substr(0, 1);
                token.remove_prefix(1);
            }
            s_semantic_version required;
            if (!Parse_Semantic_Version(std::string(token), &required)) {
                return false;
            }
            const auto actual = Semantic_Version_Tuple(version);
            const auto expected = Semantic_Version_Tuple(required);
            const bool matches = operation == ">=" ? actual >= expected : operation == "<=" ? actual <= expected
                : operation == ">"                                                          ? actual > expected
                : operation == "<"                                                          ? actual < expected
                                                                                            : actual == expected;
            if (!matches) {
                return false;
            }
            matched_token = true;
            cursor = end;
        }
        return matched_token;
    }

    bool Validate_Semantic_Version_Range(const std::string& range) {
        if (range.empty()) {
            return false;
        }
        size_t cursor = 0;
        bool found = false;
        while (cursor < range.size()) {
            while (cursor < range.size() &&
                std::isspace(static_cast<unsigned char>(range[cursor]))) {
                ++cursor;
            }
            if (cursor == range.size()) {
                break;
            }
            size_t end = cursor;
            while (end < range.size() &&
                !std::isspace(static_cast<unsigned char>(range[end]))) {
                ++end;
            }
            std::string_view token(range.data() + cursor, end - cursor);
            if (token.starts_with(">=") || token.starts_with("<=")) {
                token.remove_prefix(2);
            } else if (token.starts_with('>') || token.starts_with('<') ||
                token.starts_with('=')) {
                token.remove_prefix(1);
            }
            s_semantic_version ignored;
            if (!Parse_Semantic_Version(std::string(token), &ignored)) {
                return false;
            }
            found = true;
            cursor = end;
        }
        return found;
    }

    // Parses and validates a mod manifest from its source text, with
    // exactly the checks Load_Mod_Manifest performs. The catalogue uses
    // this directly: disabled mods are not mounted in the VFS, so their
    // manifests cannot be read through it, but the catalogue and the
    // loader must never disagree about whether a manifest is valid.
    // `source_name` is the display name used in diagnostics (the VFS
    // path for the loader, the filesystem path for the catalogue).
    // The source is taken by value: the JSON reader parses it in place.
    bool Load_Mod_Manifest_From_Source(const std::string& package_id,
        std::string source, const std::string& source_name,
        s_mod_manifest* manifest, s_cfg_diagnostic* diagnostic) {
        if (manifest == nullptr || diagnostic == nullptr || !Is_Identifier(package_id)) {
            return false;
        }
        *diagnostic = {};
        const std::string& path = source_name;

        const yyjson_read_flag flags = YYJSON_READ_ALLOW_TRAILING_COMMAS |
            YYJSON_READ_ALLOW_COMMENTS |
            YYJSON_READ_ALLOW_EXT_NUMBER |
            YYJSON_READ_ALLOW_SINGLE_QUOTED_STR |
            YYJSON_READ_ALLOW_UNQUOTED_KEY;
        yyjson_read_err error{};
        yyjson_doc* document = yyjson_read_opts(source.data(), source.size(), flags, nullptr, &error);
        if (document == nullptr) {
            size_t line = 1;
            size_t column = 1;
            size_t character = 0;
            yyjson_locate_pos(source.data(), source.size(), error.pos, &line, &column, &character);
            diagnostic->location.source = path;
            diagnostic->location.offset = error.pos;
            diagnostic->location.line = static_cast<uint32_t>(line);
            diagnostic->location.column = static_cast<uint32_t>(column);
            diagnostic->message = std::string("invalid mod manifest: ") + error.msg;
            return false;
        }

        yyjson_val* root = yyjson_doc_get_root(document);
        bool valid = yyjson_is_obj(root) &&
            Validate_Unique_Keys(root, path, "", diagnostic, 0) &&
            Validate_Properties(root, path, diagnostic);
        if (!yyjson_is_obj(root)) {
            valid = Fail(path, "mod manifest root must be an object", diagnostic);
        }

        s_mod_manifest candidate;
        if (valid) {
            valid = Get_Required_String(root, "id", path, &candidate.id, diagnostic) &&
                Get_Required_String(root, "name", path, &candidate.name, diagnostic) &&
                Get_Required_String(root, "version", path, &candidate.version_text, diagnostic);
        }
        yyjson_val* description = valid ? yyjson_obj_get(root, "description") : nullptr;
        if (valid && description != nullptr) {
            if (!yyjson_is_str(description) || yyjson_get_len(description) == 0) {
                valid = Fail(path, "manifest property 'description' must be a non-empty "
                                    "string",
                    diagnostic);
            } else {
                candidate.description.assign(yyjson_get_str(description),
                    yyjson_get_len(description));
            }
        }
        if (valid && (!Is_Identifier(candidate.id) || candidate.id != package_id)) {
            valid = Fail(path, "manifest ID '" + candidate.id + "' does not match package '" + package_id + "'", diagnostic);
        }
        if (valid && !Parse_Semantic_Version(candidate.version_text, &candidate.version)) {
            valid = Fail(path, "invalid semantic version '" + candidate.version_text + "'", diagnostic);
        }
        yyjson_val* compatible_versions =
            valid ? yyjson_obj_get(root, "compatible_versions") : nullptr;
        if (valid && compatible_versions != nullptr) {
            if (!yyjson_is_str(compatible_versions) ||
                yyjson_get_len(compatible_versions) == 0) {
                valid = Fail(path, "manifest property 'compatible_versions' must be a "
                                    "non-empty string",
                    diagnostic);
            } else {
                candidate.compatible_versions.assign(
                    yyjson_get_str(compatible_versions),
                    yyjson_get_len(compatible_versions));
                if (!Validate_Semantic_Version_Range(candidate.compatible_versions)) {
                    valid = Fail(path, "invalid compatible version range '" + candidate.compatible_versions + "'", diagnostic);
                }
            }
        }

        yyjson_val* requirements = valid ? yyjson_obj_get(root, "requires") : nullptr;
        if (valid && !yyjson_is_obj(requirements)) {
            valid = Fail(path, "manifest property 'requires' must be an object", diagnostic);
        }
        if (valid) {
            size_t requires_size = yyjson_obj_size(requirements);
            yyjson_val* engine = yyjson_obj_get(requirements, "engine");
            if (requires_size != 1 || !yyjson_is_str(engine) || yyjson_get_len(engine) == 0) {
                valid = Fail(path, "manifest property 'requires.engine' must be the only "
                                    "non-empty requirement",
                    diagnostic);
            } else {
                candidate.engine_range.assign(yyjson_get_str(engine), yyjson_get_len(engine));
                const s_semantic_version engine_version = {
                    orion2re::ENGINE_VERSION_MAJOR,
                    orion2re::ENGINE_VERSION_MINOR,
                    orion2re::ENGINE_VERSION_PATCH,
                };
                if (!Validate_Semantic_Version_Range(candidate.engine_range)) {
                    valid = Fail(path, "invalid engine version range '" + candidate.engine_range + "'", diagnostic);
                } else if (!Semantic_Version_Matches(engine_version, candidate.engine_range)) {
                    valid = Fail(path, "package requires engine '" + candidate.engine_range + "' but this engine is " + orion2re::ENGINE_VERSION, diagnostic);
                }
            }
        }

        if (valid) {
            valid = Read_Dependencies(root, "dependencies", path, &candidate.dependencies, diagnostic) &&
                Read_Dependencies(root, "optional_dependencies", path, &candidate.optional_dependencies, diagnostic) &&
                Read_String_Array(root, "conflicts", path, &candidate.conflicts, diagnostic) &&
                Read_String_Array(root, "load_after", path, &candidate.load_after, diagnostic);
        }
        yyjson_val* config = valid ? yyjson_obj_get(root, "config") : nullptr;
        if (valid) {
            if (!yyjson_is_str(config) || yyjson_get_len(config) == 0) {
                valid = Fail(path, "manifest property 'config' must be a non-empty string", diagnostic);
            } else {
                candidate.config.assign(yyjson_get_str(config), yyjson_get_len(config));
                char canonical[1024];
                if (vfs::Canonicalize_Asset_Path(candidate.config.c_str(), canonical, sizeof(canonical)) == 0) {
                    valid = Fail(path, "invalid package configuration path '" + candidate.config + "'", diagnostic);
                } else {
                    candidate.config = canonical;
                }
            }
        }
        if (valid) {
            valid = Read_Lua_Runtime(root, path, &candidate, diagnostic);
        }
        yyjson_doc_free(document);
        if (valid) {
            *manifest = std::move(candidate);
        }
        return valid;
    }

    bool Load_Mod_Manifest(const std::string& package_id, s_mod_manifest* manifest, s_cfg_diagnostic* diagnostic) {
        if (manifest == nullptr || diagnostic == nullptr || !Is_Identifier(package_id)) {
            return false;
        }
        const std::string path = "mods/" + package_id + "/mod.json5";
        std::string source;
        if (!Read_Vfs_File(path, &source, diagnostic)) {
            return false;
        }
        return Load_Mod_Manifest_From_Source(package_id, source, path, manifest, diagnostic);
    }

} // namespace config