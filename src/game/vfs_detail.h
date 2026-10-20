#pragma once

#include <string>
#include <vector>

#include "miniz.h"

struct s_vfs_file;
struct s_vfs_mount_transaction;

namespace vfs_detail {
    constexpr size_t VFS_MAX_PATH = 1024;
    constexpr const char* VFS_OVERRIDES_PATH = "data/overrides/";
    constexpr const char* VFS_DATA_PATH = "data/";
    constexpr const char* VFS_MODS_PATH = "data/mods/";
    constexpr const char* VFS_BASE_ZIP_PATH = "data/base.zip";

    enum e_vfs_backend_kind {
        VFS_BACKEND_RAW_FILE,
        VFS_BACKEND_ZIP_ENTRY,
        VFS_BACKEND_SLICE,
    };

    enum e_vfs_candidate_kind {
        VFS_CANDIDATE_FILE,
        VFS_CANDIDATE_DIRECTORY,
    };

    struct s_vfs_zip_archive {
        mz_zip_archive archive;
        char archive_path[VFS_MAX_PATH];
    };

    struct s_vfs_zip_entry {
        uint32_t archive_index;
        mz_uint file_index;
        int32_t uncompressed_size;
        bool package_entry;
        bool staged;
        uint32_t package_layer;
    };

    struct s_vfs_zip_cache_entry {
        char asset_key[VFS_MAX_PATH];
        s_vfs_zip_entry entry;
    };

    struct s_vfs_package_layer {
        std::string package_id;
        std::string source_path;
        bool is_archive = false;
        bool archive_open = false;
        s_vfs_zip_archive* zip_archive = nullptr;
        std::vector<s_vfs_zip_cache_entry> zip_entries;
    };

    struct s_vfs_raw_file_stream {
        FILE* file;
        int32_t data_offset;
    };

    struct s_vfs_zip_file_stream {
        s_vfs_zip_entry entry;
        mz_zip_reader_extract_iter_state* iter_state;
    };

    struct s_vfs_slice_file_stream {
        s_vfs_file* parent_file;
        int32_t data_offset;
    };

    struct s_vfs_state {
        bool initialized;
        char base_path[VFS_MAX_PATH];
        char fallback_path[VFS_MAX_PATH];
        char user_config_path[VFS_MAX_PATH];
        char user_mods_path[VFS_MAX_PATH];
        std::vector<std::string> raw_search_paths;
        std::vector<s_vfs_zip_archive> zip_archives;
        std::vector<s_vfs_zip_cache_entry> zip_entries;
        std::vector<s_vfs_package_layer> package_layers;
    };

    struct s_vfs_backend_ops {
        size_t (*read)(void* ptr, size_t size, size_t count, s_vfs_file* file);
        int32_t (*size)(s_vfs_file* file);
        int32_t (*seek)(s_vfs_file* file, int32_t offset, int32_t origin);
        int32_t (*tell)(s_vfs_file* file);
        int32_t (*close)(s_vfs_file* file);
    };

    extern s_vfs_state g_vfs_state;
    extern s_vfs_mount_transaction* g_vfs_staged_transaction;

    bool Is_Read_Only_Mode_(const char* mode);
    bool Build_Base_Path_(const char* base_path, char* output_path, size_t output_path_size);
    bool Is_Absolute_Asset_Path_(const char* path);
    bool Normalize_Asset_Path_(const char* path, char* output_path, size_t output_path_size);
    bool Build_Asset_Key_(const char* asset_path, char* output_key, size_t output_key_size);
    bool Build_Candidate_Path_(const char* base_path, const char* prefix, const char* asset_path, char* output_path, size_t output_path_size);
    bool Resolve_Candidate_Path_(const char* candidate_path, char* resolved_path, size_t resolved_path_size, e_vfs_candidate_kind candidate_kind = VFS_CANDIDATE_FILE);
    bool Resolve_User_Config_Asset_Path_(const char* asset_path, char* resolved_path, size_t resolved_path_size);
    bool Resolve_Override_Asset_Path_(const char* asset_path, char* resolved_path, size_t resolved_path_size);
    bool Resolve_Data_Asset_Path_(const char* asset_path, char* resolved_path, size_t resolved_path_size);
    bool Resolve_Package_Asset_Path_(const char* asset_path, char* resolved_path, size_t resolved_path_size);
    bool Resolve_Raw_Asset_Path_(const char* asset_path, char* resolved_path, size_t resolved_path_size);
    bool Resolve_Read_Path_No_Zip_(const char* path, char* resolved_path, size_t resolved_path_size);
    bool Determine_File_Size_(FILE* file, int32_t* size_out);

    bool Compare_Zip_Cache_Entries_(const s_vfs_zip_cache_entry& lhs, const s_vfs_zip_cache_entry& rhs);
    void Destroy_Zip_Iter_State_(s_vfs_zip_file_stream* zip_stream);
    bool Create_Zip_Iter_State_(s_vfs_zip_file_stream* zip_stream);
    bool Rewind_Zip_Stream_(s_vfs_zip_file_stream* zip_stream);
    bool Skip_Zip_Stream_(s_vfs_zip_file_stream* zip_stream, int32_t byte_count);
    bool Find_Zip_Entry_(const char* asset_path, s_vfs_zip_entry* zip_entry);
    bool Find_Package_Zip_Entry_(const char* asset_path, s_vfs_zip_entry* zip_entry);
    mz_zip_archive* Get_Zip_Archive_(const s_vfs_zip_entry& zip_entry);
    void Close_Package_Layers_(std::vector<s_vfs_package_layer>* layers);
    void Close_Zip_Archives_();
    bool Read_Zip_File_Entry_(mz_zip_archive* archive, uint32_t archive_index, mz_uint file_index, s_vfs_zip_cache_entry* cache_entry);
    bool Load_Base_Zip_();

    s_vfs_file* Open_Raw_File_(const char* resolved_path, const char* mode);
    s_vfs_file* Open_Zip_File_(const s_vfs_zip_entry& zip_entry);
    s_vfs_raw_file_stream* Get_Raw_File_Stream_(s_vfs_file* file);
    s_vfs_zip_file_stream* Get_Zip_File_Stream_(s_vfs_file* file);
    bool Seek_Zip_File_(s_vfs_file* file, int32_t new_position);
    s_vfs_file* Open_Slice_File_(s_vfs_file* parent_file, int32_t data_offset, int32_t data_size);
    s_vfs_slice_file_stream* Get_Slice_File_Stream_(s_vfs_file* file);

    size_t Raw_File_Read_(void* ptr, size_t size, size_t count, s_vfs_file* file);
    int32_t Raw_File_Size_(s_vfs_file* file);
    int32_t Raw_File_Seek_(s_vfs_file* file, int32_t offset, int32_t origin);
    int32_t Raw_File_Tell_(s_vfs_file* file);
    int32_t Raw_File_Close_(s_vfs_file* file);

    size_t Zip_File_Read_(void* ptr, size_t size, size_t count, s_vfs_file* file);
    int32_t Zip_File_Size_(s_vfs_file* file);
    int32_t Zip_File_Seek_(s_vfs_file* file, int32_t offset, int32_t origin);
    int32_t Zip_File_Tell_(s_vfs_file* file);
    int32_t Zip_File_Close_(s_vfs_file* file);

    size_t Slice_File_Read_(void* ptr, size_t size, size_t count, s_vfs_file* file);
    int32_t Slice_File_Size_(s_vfs_file* file);
    int32_t Slice_File_Seek_(s_vfs_file* file, int32_t offset, int32_t origin);
    int32_t Slice_File_Tell_(s_vfs_file* file);
    int32_t Slice_File_Close_(s_vfs_file* file);

    extern const s_vfs_backend_ops g_vfs_raw_file_ops;
    extern const s_vfs_backend_ops g_vfs_zip_file_ops;
    extern const s_vfs_backend_ops g_vfs_slice_file_ops;
} // namespace vfs_detail

struct s_vfs_mount_transaction {
    std::vector<vfs_detail::s_vfs_package_layer> layers;
    bool commit_in_progress = false;
};

struct s_vfs_file {
    vfs_detail::e_vfs_backend_kind backend_kind;
    const vfs_detail::s_vfs_backend_ops* ops;
    void* backend_state;
    int32_t data_size;
    int32_t position;
};