#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "config/cfg_parser.h"
#include "config/semantic_version.h"

namespace config {

    struct s_mod_dependency {
        std::string id;
        std::string version;
    };

    struct s_mod_requirement {
        std::string id;
        std::string version;

        bool operator==(const s_mod_requirement&) const = default;
    };

    struct s_lua_migration_manifest {
        uint32_t from = 0;
        uint32_t to = 0;
        std::string script;
    };

    struct s_lua_runtime_manifest {
        uint32_t api = 0;
        uint32_t storage_version = 0;
        std::vector<s_lua_migration_manifest> migrations;
    };

    struct s_mod_manifest {
        std::string id;
        std::string name;
        std::string description;
        s_semantic_version version;
        std::string version_text;
        std::string compatible_versions;
        std::string engine_range;
        std::vector<s_mod_dependency> dependencies;
        std::vector<s_mod_dependency> optional_dependencies;
        std::vector<std::string> conflicts;
        std::vector<std::string> load_after;
        std::string config;
        bool has_lua_runtime = false;
        s_lua_runtime_manifest lua_runtime;
    };

    bool Validate_Semantic_Version_Range(const std::string& range);
    bool Semantic_Version_Matches(const s_semantic_version& version,
        const std::string& range);
    bool Load_Mod_Manifest(const std::string& package_id, s_mod_manifest* manifest, s_cfg_diagnostic* diagnostic);

    // Same validation as Load_Mod_Manifest, from manifest source text
    // instead of the VFS (the mod catalogue reads manifests of mods
    // that are not mounted). `source_name` is the display name used in
    // diagnostics. On failure the diagnostic carries the loader's
    // message; when the package id itself is invalid it is left empty.
    bool Load_Mod_Manifest_From_Source(const std::string& package_id,
        std::string source, const std::string& source_name,
        s_mod_manifest* manifest, s_cfg_diagnostic* diagnostic);

} // namespace config