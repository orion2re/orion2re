#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "config/config_lifecycle.h"
#include "config/mod_manifest.h"

namespace orion2re::lua {
    struct s_lua_registry;
    struct s_lua_save_snapshot;
} // namespace orion2re::lua

namespace config {

    struct s_config_with_mods_transaction;

    struct s_loaded_mod {
        // Zero is the highest-priority package in the resolved load order.
        size_t priority_index = 0;
        s_mod_manifest manifest;
    };

    using t_loaded_mod_list = std::vector<s_loaded_mod>;

    using t_config_document_loader = bool (*)(const std::string& entry_path,
        s_cfg_document* document,
        s_cfg_diagnostic* diagnostic);
    using t_mount_transaction_commit = int8_t (*)(s_vfs_mount_transaction* transaction);

    bool Apply_Config_With_Mods(const s_cfg_document& base_document,
        const char* package_id,
        s_effective_config* effective,
        s_cfg_diagnostic* diagnostic,
        t_config_document_loader load_document,
        std::vector<std::string>* package_order = nullptr,
        t_mount_transaction_commit commit_mounts = nullptr,
        const std::vector<s_mod_requirement>* required_packages = nullptr,
        const s_config_snapshot_descriptor* snapshot_descriptor = nullptr,
        const s_config_state* snapshot_state = nullptr,
        std::vector<s_mod_requirement>* resolved_packages = nullptr,
        const orion2re::lua::s_lua_save_snapshot* lua_snapshot = nullptr,
        const s_config_state* prior_state = nullptr,
        const s_effective_config* prepared_base = nullptr);
    bool Stage_Config_With_Mods(const s_cfg_document& base_document,
        const char* package_id,
        s_effective_config* candidate,
        std::vector<std::string>* package_order,
        s_cfg_diagnostic* diagnostic,
        t_config_document_loader load_document,
        std::vector<s_mod_requirement>* resolved_packages = nullptr);
    bool Stage_Config_With_Mods_Transaction(const s_cfg_document& base_document,
        const char* package_id,
        s_cfg_diagnostic* diagnostic,
        t_config_document_loader load_document,
        const std::vector<s_mod_requirement>* required_packages,
        const s_config_snapshot_descriptor* snapshot_descriptor,
        const s_config_state* snapshot_state,
        s_config_with_mods_transaction** transaction,
        const s_effective_config* prepared_base = nullptr);
    const orion2re::lua::s_lua_registry* Get_Staged_Config_Registry(
        const s_config_with_mods_transaction* transaction);
    bool Restore_Lua_Save_To_Config_Transaction(
        s_config_with_mods_transaction* transaction,
        const orion2re::lua::s_lua_save_snapshot* snapshot,
        const s_config_state* prior_state,
        std::string* diagnostic);
    bool Commit_Config_With_Mods_Transaction(
        s_config_with_mods_transaction** transaction,
        s_effective_config* effective,
        std::vector<std::string>* package_order,
        std::vector<s_mod_requirement>* resolved_packages,
        s_cfg_diagnostic* diagnostic);
    bool Commit_Reconstructed_Lua_Runtimes_Transaction(
        s_config_with_mods_transaction** transaction,
        s_cfg_diagnostic* diagnostic);
    void Discard_Config_With_Mods_Transaction(
        s_config_with_mods_transaction** transaction);

    /**
     * Returns all committed mods in high-to-low priority order.
     * The reference remains valid until the next successful mod commit or reset.
     */
    const t_loaded_mod_list& Get_Loaded_Mods();

    /**
     * Finds a committed mod by stable package ID.
     * The pointer remains valid until the next successful mod commit or reset.
     */
    const s_loaded_mod* Find_Loaded_Mod(std::string_view package_id);

    namespace internal {
        void Reset_Loaded_Mods();
    } // namespace internal

} // namespace config