#include "pch.h"

#include <algorithm>

#include "vfs.h"

#include "vfs_detail.h"

namespace vfs_detail {
    s_vfs_state g_vfs_state{};
}

namespace vfs {
    bool Get_Package_Source_Stamp(const char* package_id, std::string* stamp) {
        if (package_id == nullptr || stamp == nullptr || !vfs_detail::g_vfs_state.initialized) return false;
        stamp->clear();
        for (const auto& layer : vfs_detail::g_vfs_state.package_layers) {
            if (layer.package_id != package_id) continue;
            SDL_PathInfo info{};
            if (!SDL_GetPathInfo(layer.source_path.c_str(), &info)) return false;
            const std::string alternative = layer.is_archive
                ? layer.source_path.substr(0, layer.source_path.size() - 4)
                : layer.source_path.substr(0, layer.source_path.size() - 1) + ".zip";
            SDL_PathInfo other{};
            if (SDL_GetPathInfo(alternative.c_str(), &other)) return false;
            *stamp = layer.source_path + ":" + std::to_string(info.size) + ":" +
                std::to_string(info.modify_time) + ":" + std::to_string(info.create_time);
            return true;
        }
        return false;
    }

    int8_t Init(const char* base_path) {
        char clean_base_path[vfs_detail::VFS_MAX_PATH];

        Shutdown();

        if (!vfs_detail::Build_Base_Path_(base_path, clean_base_path, sizeof(clean_base_path))) {
            return 0;
        }

        strlcpy(vfs_detail::g_vfs_state.base_path, clean_base_path, sizeof(vfs_detail::g_vfs_state.base_path));
        if (!vfs_detail::Load_Base_Zip_()) {
            Shutdown();
            return 0;
        }

        vfs_detail::g_vfs_state.initialized = true;
        return 1;
    }

    void Shutdown() {
        if (vfs_detail::g_vfs_staged_transaction != nullptr) {
            Discard_Mount_Transaction(vfs_detail::g_vfs_staged_transaction);
        }
        vfs_detail::Close_Package_Layers_(&vfs_detail::g_vfs_state.package_layers);
        vfs_detail::Close_Zip_Archives_();
        vfs_detail::g_vfs_state.initialized = false;
        vfs_detail::g_vfs_state.base_path[0] = '\0';
        vfs_detail::g_vfs_state.fallback_path[0] = '\0';
        vfs_detail::g_vfs_state.user_config_path[0] = '\0';
        vfs_detail::g_vfs_state.user_mods_path[0] = '\0';
        vfs_detail::g_vfs_state.raw_search_paths.clear();
    }

    int8_t Set_Raw_Fallback_Path(const char* path) {
        char clean_fallback_path[vfs_detail::VFS_MAX_PATH];

        if (!vfs_detail::Build_Base_Path_(path, clean_fallback_path, sizeof(clean_fallback_path))) {
            return 0;
        }

        strlcpy(vfs_detail::g_vfs_state.fallback_path, clean_fallback_path, sizeof(vfs_detail::g_vfs_state.fallback_path));
        return 1;
    }

    int8_t Set_User_Config_Path(const char* path) {
        char clean_path[vfs_detail::VFS_MAX_PATH];

        if (!vfs_detail::Build_Base_Path_(path, clean_path, sizeof(clean_path))) {
            return 0;
        }

        strlcpy(vfs_detail::g_vfs_state.user_config_path, clean_path, sizeof(vfs_detail::g_vfs_state.user_config_path));
        return 1;
    }

    int8_t Set_User_Mods_Path(const char* path) {
        char clean_path[vfs_detail::VFS_MAX_PATH];

        if (!vfs_detail::Build_Base_Path_(path, clean_path, sizeof(clean_path))) {
            return 0;
        }

        strlcpy(vfs_detail::g_vfs_state.user_mods_path, clean_path, sizeof(vfs_detail::g_vfs_state.user_mods_path));
        return 1;
    }

    int8_t Add_Raw_Search_Path(const char* path) {
        char clean_path[vfs_detail::VFS_MAX_PATH];

        if (!vfs_detail::Build_Base_Path_(path, clean_path, sizeof(clean_path))) {
            return 0;
        }

        if (clean_path[0] == '\0') {
            return 0;
        }

        vfs_detail::g_vfs_state.raw_search_paths.push_back(clean_path);
        return 1;
    }

    s_vfs_mount_transaction* Begin_Mount_Transaction() {
        if (!vfs_detail::g_vfs_state.initialized ||
            vfs_detail::g_vfs_staged_transaction != nullptr) {
            return nullptr;
        }
        s_vfs_mount_transaction* transaction = new s_vfs_mount_transaction{};
        vfs_detail::g_vfs_staged_transaction = transaction;
        return transaction;
    }

    int8_t Stage_Package_Mount(s_vfs_mount_transaction* transaction,
        const char* package_id,
        std::string* diagnostic) {
        if (transaction == nullptr || transaction != vfs_detail::g_vfs_staged_transaction ||
            package_id == nullptr || package_id[0] == '\0' || diagnostic == nullptr) {
            return 0;
        }
        if (package_id[0] < 'a' || package_id[0] > 'z') {
            *diagnostic = "invalid package ID '" + std::string(package_id) + "'";
            return 0;
        }
        for (const char* cursor = package_id; *cursor != '\0'; ++cursor) {
            if (!((*cursor >= 'a' && *cursor <= 'z') ||
                    (*cursor >= '0' && *cursor <= '9') || *cursor == '_' ||
                    *cursor == '-')) {
                *diagnostic = "invalid package ID '" + std::string(package_id) + "'";
                return 0;
            }
        }
        for (const vfs_detail::s_vfs_package_layer& layer : transaction->layers) {
            if (strcasecmp(layer.package_id.c_str(), package_id) == 0) {
                return 1;
            }
        }

        char directory_candidate[vfs_detail::VFS_MAX_PATH];
        char archive_candidate[vfs_detail::VFS_MAX_PATH];
        char directory_path[vfs_detail::VFS_MAX_PATH];
        char archive_path[vfs_detail::VFS_MAX_PATH];
        bool have_directory = false;
        bool have_archive = false;
        const auto find_package = [&](const char* root, const char* prefix) {
            const std::string directory_relative = std::string(prefix) + package_id;
            const std::string archive_relative = directory_relative + ".zip";
            have_directory =
                vfs_detail::Build_Candidate_Path_(root, "", directory_relative.c_str(), directory_candidate, sizeof(directory_candidate)) &&
                vfs_detail::Resolve_Candidate_Path_(directory_candidate, directory_path, sizeof(directory_path), vfs_detail::VFS_CANDIDATE_DIRECTORY);
            have_archive =
                vfs_detail::Build_Candidate_Path_(root, "", archive_relative.c_str(), archive_candidate, sizeof(archive_candidate)) &&
                vfs_detail::Resolve_Candidate_Path_(archive_candidate, archive_path, sizeof(archive_path));
            return have_directory || have_archive;
        };
        // A package in the user's mods directory replaces an installed one with the same ID.
        if (vfs_detail::g_vfs_state.user_mods_path[0] == '\0' ||
            !find_package(vfs_detail::g_vfs_state.user_mods_path, "")) {
            find_package(vfs_detail::g_vfs_state.base_path, vfs_detail::VFS_MODS_PATH);
        }
        if (have_directory == have_archive) {
            *diagnostic = have_directory ? "package '" + std::string(package_id) + "' has both directory and ZIP forms" : "package not found: '" + std::string(package_id) + "'";
            return 0;
        }

        vfs_detail::s_vfs_package_layer layer;
        layer.package_id = package_id;
        layer.source_path = have_directory ? directory_path : archive_path;
        layer.is_archive = have_archive;
        if (!have_archive) {
            SDL_PathInfo path_info;
            if (!SDL_GetPathInfo(directory_path, &path_info) ||
                path_info.type != SDL_PATHTYPE_DIRECTORY) {
                *diagnostic = "package path is not a directory: '" + std::string(package_id) + "'";
                return 0;
            }
            layer.source_path += "/";
            transaction->layers.push_back(std::move(layer));
            return 1;
        }

        layer.zip_archive = new vfs_detail::s_vfs_zip_archive{};
        memset(&layer.zip_archive->archive, 0, sizeof(layer.zip_archive->archive));
        if (!mz_zip_reader_init_file(&layer.zip_archive->archive, archive_path, 0)) {
            delete layer.zip_archive;
            *diagnostic = "unable to open package ZIP: '" + std::string(package_id) + "'";
            return 0;
        }
        layer.archive_open = true;
        strlcpy(layer.zip_archive->archive_path, archive_path, sizeof(layer.zip_archive->archive_path));

        const mz_uint file_count = mz_zip_reader_get_num_files(&layer.zip_archive->archive);
        for (mz_uint file_index = 0; file_index < file_count; ++file_index) {
            mz_zip_archive_file_stat file_stat;
            if (!mz_zip_reader_file_stat(&layer.zip_archive->archive, file_index, &file_stat) ||
                file_stat.m_is_directory) {
                continue;
            }
            if (file_stat.m_is_encrypted || !file_stat.m_is_supported ||
                file_stat.m_uncomp_size > INT32_MAX) {
                vfs_detail::Close_Package_Layers_(&transaction->layers);
                mz_zip_reader_end(&layer.zip_archive->archive);
                delete layer.zip_archive;
                *diagnostic = "unsupported entry in package ZIP: '" + std::string(package_id) + "'";
                return 0;
            }
            std::string raw_entry = file_stat.m_filename;
            for (char& character : raw_entry) {
                if (character == '\\') {
                    character = '/';
                }
            }
            size_t part_start = 0;
            bool unsafe_path = raw_entry.empty() || raw_entry.front() == '/';
            while (!unsafe_path && part_start <= raw_entry.size()) {
                const size_t part_end = raw_entry.find('/', part_start);
                const size_t length = (part_end == std::string::npos ? raw_entry.size() : part_end) - part_start;
                unsafe_path = length == 0 ||
                    (length == 1 && raw_entry[part_start] == '.') ||
                    (length == 2 && raw_entry[part_start] == '.' &&
                        raw_entry[part_start + 1] == '.');
                if (part_end == std::string::npos) {
                    break;
                }
                part_start = part_end + 1;
            }
            if (unsafe_path) {
                mz_zip_reader_end(&layer.zip_archive->archive);
                delete layer.zip_archive;
                *diagnostic = "unsafe path in package ZIP: '" + raw_entry + "'";
                return 0;
            }
            char canonical_entry[vfs_detail::VFS_MAX_PATH];
            if (Canonicalize_Asset_Path(file_stat.m_filename, canonical_entry, sizeof(canonical_entry)) == 0) {
                mz_zip_reader_end(&layer.zip_archive->archive);
                delete layer.zip_archive;
                *diagnostic = "unsafe path in package ZIP: '" + std::string(file_stat.m_filename) + "'";
                return 0;
            }

            const auto add_entry = [&](const std::string& virtual_path) {
                vfs_detail::s_vfs_zip_cache_entry cache_entry{};
                if (!vfs_detail::Build_Asset_Key_(virtual_path.c_str(), cache_entry.asset_key, sizeof(cache_entry.asset_key))) {
                    return false;
                }
                cache_entry.entry.file_index = file_index;
                cache_entry.entry.uncompressed_size = static_cast<int32_t>(file_stat.m_uncomp_size);
                cache_entry.entry.package_entry = true;
                layer.zip_entries.push_back(cache_entry);
                return true;
            };
            if (!add_entry("mods/" + layer.package_id + "/" + canonical_entry)) {
                mz_zip_reader_end(&layer.zip_archive->archive);
                delete layer.zip_archive;
                *diagnostic = "package ZIP path is too long: '" + std::string(package_id) + "'";
                return 0;
            }
            constexpr const char* data_prefix = "data/";
            if (strncmp(canonical_entry, data_prefix, strlen(data_prefix)) == 0 &&
                canonical_entry[strlen(data_prefix)] != '\0' &&
                !add_entry(canonical_entry + strlen(data_prefix))) {
                mz_zip_reader_end(&layer.zip_archive->archive);
                delete layer.zip_archive;
                *diagnostic = "package ZIP data path is too long: '" + std::string(package_id) + "'";
                return 0;
            }
        }
        std::sort(layer.zip_entries.begin(), layer.zip_entries.end(), vfs_detail::Compare_Zip_Cache_Entries_);
        for (size_t index = 1; index < layer.zip_entries.size(); ++index) {
            if (strcmp(layer.zip_entries[index - 1].asset_key,
                    layer.zip_entries[index].asset_key) == 0) {
                mz_zip_reader_end(&layer.zip_archive->archive);
                delete layer.zip_archive;
                *diagnostic = "duplicate virtual path in package ZIP: '" +
                    std::string(layer.zip_entries[index].asset_key) + "'";
                return 0;
            }
        }
        transaction->layers.push_back(std::move(layer));
        return 1;
    }

    int8_t Commit_Mount_Transaction(s_vfs_mount_transaction* transaction) {
        if (!Begin_Mount_Transaction_Commit(transaction)) {
            return 0;
        }
        Finalize_Mount_Transaction_Commit(transaction);
        return 1;
    }

    bool Can_Commit_Mount_Transaction(
        const s_vfs_mount_transaction* transaction) {
        return transaction != nullptr &&
            transaction == vfs_detail::g_vfs_staged_transaction;
    }

    bool Begin_Mount_Transaction_Commit(
        s_vfs_mount_transaction* transaction) {
        if (!Can_Commit_Mount_Transaction(transaction)) {
            return false;
        }
        transaction->layers.swap(vfs_detail::g_vfs_state.package_layers);
        transaction->commit_in_progress = true;
        return true;
    }

    void Rollback_Mount_Transaction_Commit(
        s_vfs_mount_transaction* transaction) {
        if (Can_Commit_Mount_Transaction(transaction)) {
            transaction->layers.swap(vfs_detail::g_vfs_state.package_layers);
            transaction->commit_in_progress = false;
        }
    }

    void Finalize_Mount_Transaction_Commit(
        s_vfs_mount_transaction* transaction) {
        if (!Can_Commit_Mount_Transaction(transaction)) {
            return;
        }
        vfs_detail::Close_Package_Layers_(&transaction->layers);
        vfs_detail::g_vfs_staged_transaction = nullptr;
        delete transaction;
    }

    int8_t Order_Package_Mounts(s_vfs_mount_transaction* transaction,
        const std::vector<std::string>& package_ids,
        std::string* diagnostic) {
        if (transaction == nullptr || transaction != vfs_detail::g_vfs_staged_transaction ||
            diagnostic == nullptr || package_ids.size() != transaction->layers.size()) {
            return 0;
        }
        std::vector<vfs_detail::s_vfs_package_layer> ordered;
        ordered.reserve(transaction->layers.size());
        std::vector<bool> selected(transaction->layers.size(), false);
        for (const std::string& package_id : package_ids) {
            bool found = false;
            for (size_t index = 0; index < transaction->layers.size(); ++index) {
                if (!selected[index] &&
                    transaction->layers[index].package_id == package_id) {
                    selected[index] = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                *diagnostic = "unable to order package mount '" + package_id + "'";
                return 0;
            }
        }
        std::fill(selected.begin(), selected.end(), false);
        for (const std::string& package_id : package_ids) {
            for (size_t index = 0; index < transaction->layers.size(); ++index) {
                if (!selected[index] &&
                    transaction->layers[index].package_id == package_id) {
                    selected[index] = true;
                    ordered.push_back(std::move(transaction->layers[index]));
                    break;
                }
            }
        }
        transaction->layers = std::move(ordered);
        return 1;
    }

    void Discard_Mount_Transaction(s_vfs_mount_transaction* transaction) {
        if (transaction == nullptr) {
            return;
        }
        if (transaction == vfs_detail::g_vfs_staged_transaction) {
            vfs_detail::g_vfs_staged_transaction = nullptr;
        }
        vfs_detail::Close_Package_Layers_(&transaction->layers);
        delete transaction;
    }

    s_vfs_file* fopen(const char* path, const char* mode) {
        char asset_path[vfs_detail::VFS_MAX_PATH];
        char resolved_path[vfs_detail::VFS_MAX_PATH];
        vfs_detail::s_vfs_zip_entry zip_entry{};

        if (!vfs_detail::Is_Read_Only_Mode_(mode) || !vfs_detail::g_vfs_state.initialized) {
            return nullptr;
        }

        if (!vfs_detail::Normalize_Asset_Path_(path, asset_path, sizeof(asset_path))) {
            return nullptr;
        }

        const bool private_package_path = strncasecmp(asset_path, "mods/", 5) == 0;

        if (private_package_path) {
            if (vfs_detail::Resolve_Package_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path))) {
                return vfs_detail::Open_Raw_File_(resolved_path, mode);
            }
            if (vfs_detail::Find_Package_Zip_Entry_(asset_path, &zip_entry)) {
                return vfs_detail::Open_Zip_File_(zip_entry);
            }
            return nullptr;
        }

        if (vfs_detail::Resolve_User_Config_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path))) {
            return vfs_detail::Open_Raw_File_(resolved_path, mode);
        }

        if (vfs_detail::Resolve_Override_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path))) {
            return vfs_detail::Open_Raw_File_(resolved_path, mode);
        }

        if (vfs_detail::Resolve_Package_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path))) {
            return vfs_detail::Open_Raw_File_(resolved_path, mode);
        }

        if (vfs_detail::Find_Package_Zip_Entry_(asset_path, &zip_entry)) {
            return vfs_detail::Open_Zip_File_(zip_entry);
        }

        if (vfs_detail::Find_Zip_Entry_(asset_path, &zip_entry)) {
            return vfs_detail::Open_Zip_File_(zip_entry);
        }

        if (vfs_detail::Resolve_Data_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path))) {
            return vfs_detail::Open_Raw_File_(resolved_path, mode);
        }

        if (vfs_detail::Resolve_Raw_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path))) {
            return vfs_detail::Open_Raw_File_(resolved_path, mode);
        }

        return nullptr;
    }

    size_t fread(void* ptr, size_t size, size_t count, s_vfs_file* file) {
        if (file == nullptr || file->ops == nullptr || file->ops->read == nullptr) {
            return 0;
        }

        return file->ops->read(ptr, size, count, file);
    }

    int32_t fsize(s_vfs_file* file) {
        if (file == nullptr || file->ops == nullptr || file->ops->size == nullptr) {
            return -1;
        }

        return file->ops->size(file);
    }

    int32_t fseek(s_vfs_file* file, int32_t offset, int32_t origin) {
        if (file == nullptr || file->ops == nullptr || file->ops->seek == nullptr) {
            return -1;
        }

        return file->ops->seek(file, offset, origin);
    }

    int32_t ftell(s_vfs_file* file) {
        if (file == nullptr || file->ops == nullptr || file->ops->tell == nullptr) {
            return -1;
        }

        return file->ops->tell(file);
    }

    int32_t fclose(s_vfs_file* file) {
        if (file == nullptr || file->ops == nullptr || file->ops->close == nullptr) {
            return -1;
        }

        return file->ops->close(file);
    }

    int8_t Canonicalize_Asset_Path(const char* path, char* canonical_path, size_t canonical_path_size) {
        char normalized_path[vfs_detail::VFS_MAX_PATH];
        if (!vfs_detail::Normalize_Asset_Path_(path, normalized_path, sizeof(normalized_path))) {
            return 0;
        }
        char collapsed_path[vfs_detail::VFS_MAX_PATH];
        size_t collapsed_length = 0;
        size_t start = 0;
        while (normalized_path[start] != '\0') {
            size_t end = start;
            while (normalized_path[end] != '\0' && normalized_path[end] != '/') {
                ++end;
            }
            const size_t part_length = end - start;
            if (part_length == 1 && normalized_path[start] == '.') {
            } else if (part_length == 2 && normalized_path[start] == '.' &&
                normalized_path[start + 1] == '.') {
                if (collapsed_length == 0) {
                    return 0;
                }
                while (collapsed_length > 0 && collapsed_path[collapsed_length - 1] != '/') {
                    --collapsed_length;
                }
                if (collapsed_length > 0) {
                    --collapsed_length;
                }
            } else if (part_length != 0) {
                const size_t separator_length = collapsed_length == 0 ? 0 : 1;
                if (collapsed_length + separator_length + part_length >=
                    sizeof(collapsed_path)) {
                    return 0;
                }
                if (separator_length != 0) {
                    collapsed_path[collapsed_length++] = '/';
                }
                memcpy(collapsed_path + collapsed_length, normalized_path + start, part_length);
                collapsed_length += part_length;
            }
            start = normalized_path[end] == '\0' ? end : end + 1;
        }
        if (collapsed_length == 0) {
            return 0;
        }
        collapsed_path[collapsed_length] = '\0';
        return vfs_detail::Build_Asset_Key_(collapsed_path, canonical_path, canonical_path_size) ? 1 : 0;
    }

    int8_t Resolve_Read_Path_No_Zip(const char* path, char* resolved_path, size_t resolved_path_size) {
        return vfs_detail::Resolve_Read_Path_No_Zip_(path, resolved_path, resolved_path_size) ? 1 : 0;
    }

    int8_t File_Exists(const char* path) {
        char asset_path[vfs_detail::VFS_MAX_PATH];
        char resolved_path[vfs_detail::VFS_MAX_PATH];

        if (!vfs_detail::g_vfs_state.initialized || !vfs_detail::Normalize_Asset_Path_(path, asset_path, sizeof(asset_path))) {
            return 0;
        }

        if (strncasecmp(asset_path, "mods/", 5) == 0) {
            return vfs_detail::Resolve_Package_Asset_Path_(
                       asset_path, resolved_path, sizeof(resolved_path)) ||
                    vfs_detail::Find_Package_Zip_Entry_(asset_path, nullptr)
                ? 1
                : 0;
        }

        if (vfs_detail::Resolve_User_Config_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path))) {
            return 1;
        }

        if (vfs_detail::Resolve_Override_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path))) {
            return 1;
        }

        if (vfs_detail::Resolve_Package_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path)) ||
            vfs_detail::Find_Package_Zip_Entry_(asset_path, nullptr)) {
            return 1;
        }

        if (vfs_detail::Find_Zip_Entry_(asset_path, nullptr)) {
            return 1;
        }

        if (vfs_detail::Resolve_Data_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path))) {
            return 1;
        }

        return vfs_detail::Resolve_Raw_Asset_Path_(asset_path, resolved_path, sizeof(resolved_path)) ? 1 : 0;
    }
} // namespace vfs