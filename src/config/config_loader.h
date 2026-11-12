#pragma once

#include <string>
#include <vector>

#include "config/config_lifecycle.h"
#include "config/mod_manifest.h"

namespace orion2re::lua {
    struct s_lua_registry;
    struct s_lua_save_snapshot;
} // namespace orion2re::lua

namespace config {

    struct s_network_config_candidate;
    struct s_prepared_config_snapshot;

    struct s_active_config {
        s_config_baseline baseline;
        std::string entry_path;
        // Filesystem path of the file the entry document was loaded from,
        // resolved through the VFS at load time. This is the one config:
        // the file the game loaded is the file the settings UI saves to,
        // and the path the user is shown. Empty when the entry was not
        // loaded from a real file (defaults, or a zip-only entry): the
        // settings UI then falls back to a user-writable file. The entry
        // path itself stays a VFS key: the loader reads through the VFS
        // layers, and only this field names a real file.
        std::string entry_filesystem_path;
        std::string base_package;
        std::vector<std::string> package_order;
        std::vector<s_mod_requirement> required_mods;
    };

    bool Load_Config_File(const char* entry_path, const char* package_id, s_effective_config* effective, s_cfg_diagnostic* diagnostic);
    bool Load_Startup_Config(const char* entry_path, const char* package_id, s_effective_config* effective, bool* loaded, s_cfg_diagnostic* diagnostic, bool require_file = false);
    bool Commit_Active_Config_Writes(const s_config_write_set* writes,
        s_cfg_diagnostic* diagnostic);
    bool Record_Active_Native_Config_Write(t_variable_id variable_id);
    bool Restore_Active_Config(const s_active_config* snapshot,
        s_cfg_diagnostic* diagnostic);
    const s_active_config* Get_Active_Config();
    bool Restore_Launch_Config(s_cfg_diagnostic* diagnostic);
    void Shutdown_Config_Loader();
    bool Get_Prepared_Active_Config_Snapshot(
        const s_config_snapshot_descriptor* descriptor,
        const s_prepared_config_snapshot** snapshot,
        s_cfg_diagnostic* diagnostic);
    bool Get_Canonical_Active_Config_Identity(
        const std::vector<uint8_t>** identity,
        s_cfg_diagnostic* diagnostic);
    bool Commit_Config_Snapshot(const s_config_snapshot_descriptor* descriptor,
        const s_config_state* state,
        s_cfg_diagnostic* diagnostic);
    bool Reapply_Config_Snapshot(const std::vector<s_mod_requirement>& required_mods,
        const s_config_snapshot_descriptor* descriptor,
        const s_config_state* state,
        s_cfg_diagnostic* diagnostic,
        const orion2re::lua::s_lua_save_snapshot* lua_snapshot = nullptr);
    bool Stage_Network_Config_Snapshot(const void* bytes,
        uint32_t size,
        s_network_config_candidate** candidate,
        s_cfg_diagnostic* diagnostic);
    const orion2re::lua::s_lua_registry* Get_Network_Config_Candidate_Registry(
        const s_network_config_candidate* candidate);
    bool Finalize_Network_Config_Snapshot(
        s_network_config_candidate** candidate,
        const void* lua_snapshot,
        uint32_t lua_snapshot_size,
        std::string* diagnostic);
    bool Commit_Network_Config_Snapshot(
        s_network_config_candidate** candidate,
        s_cfg_diagnostic* diagnostic);
    void Discard_Network_Config_Snapshot(
        s_network_config_candidate** candidate);
    bool Reconstruct_Active_Lua_Runtimes(std::string* diagnostic);
    bool Can_Reload_Config_Group(const s_config_group_descriptor* descriptor,
        bool network_game);
    using t_config_group_merger = bool (*)(t_config_group group,
        const s_effective_config* candidate,
        s_effective_config* effective);
    using t_config_group_reload_policy = bool (*)(
        const s_config_group_descriptor* descriptor, bool network_game);
    bool Reload_Active_Config_Groups(bool network_game,
        s_cfg_diagnostic* diagnostic,
        t_config_group_merger merge_group = nullptr,
        t_config_group_reload_policy reload_policy = nullptr);
    bool Reload_All_Active_Config_Groups(s_cfg_diagnostic* diagnostic);
    // Dry-run outcome of enabling a candidate mod set, resolved by the
    // existing staging path. Nothing is changed: the active mod set, the
    // loaded mods and the configuration file are untouched. On failure the
    // diagnostic carries the resolver's message (missing package,
    // conflict, ...).
    struct s_mod_set_preview {
        // Resolved mods in priority order (highest priority first),
        // including the dependencies the resolver pulled in.
        std::vector<s_mod_requirement> resolved_mods;
        // The same ids as resolved_mods, in the same order.
        std::vector<std::string> package_order;
    };
    bool Preview_Enabled_Mods(const std::vector<std::string>& enabled_mods,
        s_mod_set_preview* preview,
        s_cfg_diagnostic* diagnostic);
    // Applies a candidate mod set as the active set: a full load (VFS
    // mounts, Lua registries and runtimes, and all config groups), the
    // same path a save load takes. Only safe with no game loaded — the
    // picker calls it from the main menu — so it refuses while a game
    // config is in effect. The launch config follows the picked set, so
    // returning to the main menu does not revert it. The configuration
    // file is NOT written; persisting the choice is the config writer's job.
    // Until the file is updated, a file-based reload (Alt+R) refuses with
    // a package-order change, as it should.
    // The rebuild is a live-session build, so the entry document
    // overrides the package documents: a setting the user changed
    // earlier in the session survives the apply, because the change
    // lives in the entry document (the user file once one exists) and
    // wins over a mod's assignment of the same path, in the live state
    // as well as in the file. Saves and network snapshots are built
    // the other way (packages override the entry), so loading a save
    // still restores its own mod's configuration.
    // Limitation: in-memory overrides not present in any document
    // (recorded via Record_Active_Native_Config_Write) are still
    // dropped by the rebuild. That is latent today (no production
    // caller records overrides before an Apply).
    bool Apply_Enabled_Mods(const std::vector<std::string>& enabled_mods,
        s_cfg_diagnostic* diagnostic);

} // namespace config