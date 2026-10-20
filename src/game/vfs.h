#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

struct s_vfs_file;
struct s_vfs_mount_transaction;

namespace vfs {
    int8_t Init(const char* base_path);
    void Shutdown();
    int8_t Set_Raw_Fallback_Path(const char* path);
    // Configuration files (*.cfg) resolve from this directory before the
    // installation's data directory.
    int8_t Set_User_Config_Path(const char* path);
    // Packages here take precedence over data/mods/ in the installation.
    int8_t Set_User_Mods_Path(const char* path);
    // Searched for original game files after the base path and before the
    // raw fallback path.
    int8_t Add_Raw_Search_Path(const char* path);

    s_vfs_mount_transaction* Begin_Mount_Transaction();
    int8_t Stage_Package_Mount(s_vfs_mount_transaction* transaction,
        const char* package_id,
        std::string* diagnostic);
    int8_t Order_Package_Mounts(s_vfs_mount_transaction* transaction,
        const std::vector<std::string>& package_ids,
        std::string* diagnostic);
    bool Can_Commit_Mount_Transaction(
        const s_vfs_mount_transaction* transaction);
    bool Begin_Mount_Transaction_Commit(
        s_vfs_mount_transaction* transaction);
    void Rollback_Mount_Transaction_Commit(
        s_vfs_mount_transaction* transaction);
    void Finalize_Mount_Transaction_Commit(
        s_vfs_mount_transaction* transaction);
    int8_t Commit_Mount_Transaction(s_vfs_mount_transaction* transaction);
    void Discard_Mount_Transaction(s_vfs_mount_transaction* transaction);

    s_vfs_file* fopen(const char* path, const char* mode);
    s_vfs_file* fopen_lbx(const char* lbx_name, int32_t entry_idx);

    size_t fread(void* ptr, size_t size, size_t count, s_vfs_file* file);
    int32_t fsize(s_vfs_file* file);
    int32_t fseek(s_vfs_file* file, int32_t offset, int32_t origin);
    int32_t ftell(s_vfs_file* file);
    int32_t fclose(s_vfs_file* file);

    int8_t Canonicalize_Asset_Path(const char* path, char* canonical_path, size_t canonical_path_size);
    int8_t Resolve_Read_Path_No_Zip(const char* path, char* resolved_path, size_t resolved_path_size);
    int8_t File_Exists(const char* path);
    bool Get_Package_Source_Stamp(const char* package_id, std::string* stamp);
} // namespace vfs