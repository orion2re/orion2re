#include "pch.h"

#include "vfs_detail.h"

namespace vfs_detail {
    s_vfs_mount_transaction* g_vfs_staged_transaction = nullptr;

    const s_vfs_backend_ops g_vfs_zip_file_ops = {
        Zip_File_Read_,
        Zip_File_Size_,
        Zip_File_Seek_,
        Zip_File_Tell_,
        Zip_File_Close_,
    };

    bool Compare_Zip_Cache_Entries_(const s_vfs_zip_cache_entry& lhs, const s_vfs_zip_cache_entry& rhs) {
        const int path_compare = strcmp(lhs.asset_key, rhs.asset_key);
        if (path_compare != 0) {
            return path_compare < 0;
        }

        if (lhs.entry.archive_index != rhs.entry.archive_index) {
            return lhs.entry.archive_index < rhs.entry.archive_index;
        }

        return lhs.entry.file_index < rhs.entry.file_index;
    }

    void Destroy_Zip_Iter_State_(s_vfs_zip_file_stream* zip_stream) {
        if (zip_stream == nullptr || zip_stream->iter_state == nullptr) {
            return;
        }

        mz_zip_reader_extract_iter_free(zip_stream->iter_state);
        zip_stream->iter_state = nullptr;
    }

    bool Create_Zip_Iter_State_(s_vfs_zip_file_stream* zip_stream) {
        if (zip_stream == nullptr) {
            return false;
        }

        mz_zip_archive* archive = Get_Zip_Archive_(zip_stream->entry);
        if (archive == nullptr) {
            return false;
        }
        zip_stream->iter_state = mz_zip_reader_extract_iter_new(
            archive, zip_stream->entry.file_index, 0);
        return zip_stream->iter_state != nullptr;
    }

    bool Rewind_Zip_Stream_(s_vfs_zip_file_stream* zip_stream) {
        Destroy_Zip_Iter_State_(zip_stream);
        return Create_Zip_Iter_State_(zip_stream);
    }

    bool Skip_Zip_Stream_(s_vfs_zip_file_stream* zip_stream, int32_t byte_count) {
        uint8_t discard_buffer[4096];
        int32_t remaining = byte_count;

        while (remaining > 0) {
            const size_t chunk_size = std::min((size_t)remaining, sizeof(discard_buffer));
            const size_t bytes_read = mz_zip_reader_extract_iter_read(zip_stream->iter_state, discard_buffer, chunk_size);
            if (bytes_read != chunk_size) {
                return false;
            }

            remaining -= (int32_t)bytes_read;
        }

        return true;
    }

    bool Find_Package_Zip_Entry_(const char* asset_path, s_vfs_zip_entry* zip_entry) {
        s_vfs_zip_cache_entry lookup_entry{};
        if (!Build_Asset_Key_(asset_path, lookup_entry.asset_key, sizeof(lookup_entry.asset_key))) {
            return false;
        }

        const auto find_in_layers = [&](const std::vector<s_vfs_package_layer>& layers,
                                        bool staged) {
            for (size_t layer_index = 0; layer_index < layers.size(); ++layer_index) {
                const std::vector<s_vfs_zip_cache_entry>& entries = layers[layer_index].zip_entries;
                auto it = std::lower_bound(entries.begin(), entries.end(), lookup_entry, Compare_Zip_Cache_Entries_);
                if (it != entries.end() && strcmp(it->asset_key, lookup_entry.asset_key) == 0) {
                    if (zip_entry != nullptr) {
                        *zip_entry = it->entry;
                        zip_entry->package_entry = true;
                        zip_entry->staged = staged;
                        zip_entry->package_layer = static_cast<uint32_t>(layer_index);
                    }
                    return true;
                }
            }
            return false;
        };

        if (g_vfs_staged_transaction != nullptr &&
            !g_vfs_staged_transaction->commit_in_progress &&
            find_in_layers(g_vfs_staged_transaction->layers, true)) {
            return true;
        }
        if (g_vfs_staged_transaction != nullptr &&
            !g_vfs_staged_transaction->commit_in_progress) {
            return false;
        }
        return find_in_layers(g_vfs_state.package_layers, false);
    }

    mz_zip_archive* Get_Zip_Archive_(const s_vfs_zip_entry& zip_entry) {
        if (!zip_entry.package_entry) {
            if (zip_entry.archive_index >= g_vfs_state.zip_archives.size()) {
                return nullptr;
            }
            return &g_vfs_state.zip_archives[zip_entry.archive_index].archive;
        }

        std::vector<s_vfs_package_layer>* layers = nullptr;
        if (zip_entry.staged) {
            if (g_vfs_staged_transaction == nullptr) {
                return nullptr;
            }
            layers = &g_vfs_staged_transaction->layers;
        } else {
            layers = &g_vfs_state.package_layers;
        }
        if (zip_entry.package_layer >= layers->size()) {
            return nullptr;
        }
        s_vfs_package_layer& layer = (*layers)[zip_entry.package_layer];
        return layer.zip_archive == nullptr ? nullptr : &layer.zip_archive->archive;
    }

    void Close_Package_Layers_(std::vector<s_vfs_package_layer>* layers) {
        if (layers == nullptr) {
            return;
        }
        for (s_vfs_package_layer& layer : *layers) {
            if (layer.archive_open && layer.zip_archive != nullptr) {
                mz_zip_reader_end(&layer.zip_archive->archive);
                layer.archive_open = false;
            }
            delete layer.zip_archive;
            layer.zip_archive = nullptr;
        }
        layers->clear();
    }

    bool Find_Zip_Entry_(const char* asset_path, s_vfs_zip_entry* zip_entry) {
        s_vfs_zip_cache_entry lookup_entry{};
        std::vector<s_vfs_zip_cache_entry>::const_iterator it;

        if (!Build_Asset_Key_(asset_path, lookup_entry.asset_key, sizeof(lookup_entry.asset_key))) {
            return false;
        }

        it = std::lower_bound(g_vfs_state.zip_entries.begin(), g_vfs_state.zip_entries.end(), lookup_entry, Compare_Zip_Cache_Entries_);
        if (it == g_vfs_state.zip_entries.end() || strcmp(it->asset_key, lookup_entry.asset_key) != 0) {
            return false;
        }

        if (zip_entry != nullptr) {
            *zip_entry = it->entry;
        }

        return true;
    }

    void Close_Zip_Archives_() {
        for (s_vfs_zip_archive& zip_archive : g_vfs_state.zip_archives) {
            mz_zip_reader_end(&zip_archive.archive);
            zip_archive.archive_path[0] = '\0';
        }

        g_vfs_state.zip_archives.clear();
        g_vfs_state.zip_entries.clear();
    }

    bool Read_Zip_File_Entry_(mz_zip_archive* archive, uint32_t archive_index, mz_uint file_index, s_vfs_zip_cache_entry* cache_entry) {
        mz_zip_archive_file_stat file_stat;
        char normalized_path[VFS_MAX_PATH];

        if (archive == nullptr || cache_entry == nullptr) {
            return false;
        }

        if (!mz_zip_reader_file_stat(archive, file_index, &file_stat)) {
            return false;
        }

        if (file_stat.m_is_directory || file_stat.m_is_encrypted || !file_stat.m_is_supported || file_stat.m_uncomp_size > INT32_MAX) {
            return false;
        }

        if (!Normalize_Asset_Path_(file_stat.m_filename, normalized_path, sizeof(normalized_path))) {
            return false;
        }

        if (!Build_Asset_Key_(normalized_path, cache_entry->asset_key, sizeof(cache_entry->asset_key))) {
            return false;
        }

        cache_entry->entry.archive_index = archive_index;
        cache_entry->entry.file_index = file_index;
        cache_entry->entry.uncompressed_size = (int32_t)file_stat.m_uncomp_size;
        cache_entry->entry.package_entry = false;
        cache_entry->entry.staged = false;
        cache_entry->entry.package_layer = 0;
        return true;
    }

    bool Load_Base_Zip_() {
        char candidate_path[VFS_MAX_PATH];
        char archive_path[VFS_MAX_PATH];
        if (!Build_Candidate_Path_(g_vfs_state.base_path, "", VFS_BASE_ZIP_PATH, candidate_path, sizeof(candidate_path))) {
            return false;
        }
        if (!Resolve_Candidate_Path_(candidate_path, archive_path, sizeof(archive_path))) {
            return true;
        }

        g_vfs_state.zip_archives.emplace_back();
        s_vfs_zip_archive& zip_archive = g_vfs_state.zip_archives.back();
        memset(&zip_archive.archive, 0, sizeof(zip_archive.archive));
        if (strlcpy(zip_archive.archive_path, archive_path, sizeof(zip_archive.archive_path)) >= sizeof(zip_archive.archive_path) ||
            !mz_zip_reader_init_file(&zip_archive.archive, zip_archive.archive_path, 0)) {
            Close_Zip_Archives_();
            return false;
        }

        const mz_uint total_files = mz_zip_reader_get_num_files(&zip_archive.archive);
        for (mz_uint file_index = 0; file_index < total_files; ++file_index) {
            s_vfs_zip_cache_entry cache_entry{};
            if (Read_Zip_File_Entry_(&zip_archive.archive, 0, file_index, &cache_entry)) {
                g_vfs_state.zip_entries.push_back(cache_entry);
            }
        }
        std::sort(g_vfs_state.zip_entries.begin(), g_vfs_state.zip_entries.end(), Compare_Zip_Cache_Entries_);
        return true;
    }

    s_vfs_file* Open_Zip_File_(const s_vfs_zip_entry& zip_entry) {
        s_vfs_zip_file_stream* zip_stream = new s_vfs_zip_file_stream{};
        s_vfs_file* vfs_file;

        zip_stream->entry = zip_entry;
        if (!Create_Zip_Iter_State_(zip_stream)) {
            delete zip_stream;
            return nullptr;
        }

        vfs_file = new s_vfs_file{};
        vfs_file->backend_kind = VFS_BACKEND_ZIP_ENTRY;
        vfs_file->ops = &g_vfs_zip_file_ops;
        vfs_file->backend_state = zip_stream;
        vfs_file->data_size = zip_entry.uncompressed_size;
        vfs_file->position = 0;
        return vfs_file;
    }

    s_vfs_zip_file_stream* Get_Zip_File_Stream_(s_vfs_file* file) {
        if (file == nullptr || file->backend_kind != VFS_BACKEND_ZIP_ENTRY) {
            return nullptr;
        }

        return (s_vfs_zip_file_stream*)file->backend_state;
    }

    bool Seek_Zip_File_(s_vfs_file* file, int32_t new_position) {
        s_vfs_zip_file_stream* zip_stream = Get_Zip_File_Stream_(file);

        if (zip_stream == nullptr) {
            return false;
        }

        if (new_position < file->position) {
            if (!Rewind_Zip_Stream_(zip_stream)) {
                return false;
            }
            file->position = 0;
        }

        if (new_position > file->position && !Skip_Zip_Stream_(zip_stream, new_position - file->position)) {
            return false;
        }

        file->position = new_position;
        return true;
    }

    size_t Zip_File_Read_(void* ptr, size_t size, size_t count, s_vfs_file* file) {
        s_vfs_zip_file_stream* zip_stream;
        size_t remaining_bytes;
        size_t max_count;
        size_t requested_bytes;
        size_t bytes_read;

        if (ptr == nullptr || file == nullptr || size == 0 || count == 0) {
            return 0;
        }

        if (file->position >= file->data_size) {
            return 0;
        }

        zip_stream = Get_Zip_File_Stream_(file);
        if (zip_stream == nullptr) {
            return 0;
        }

        remaining_bytes = (size_t)(file->data_size - file->position);
        max_count = remaining_bytes / size;
        if (max_count == 0) {
            return 0;
        }

        if (max_count > count) {
            max_count = count;
        }

        requested_bytes = max_count * size;
        bytes_read = mz_zip_reader_extract_iter_read(zip_stream->iter_state, ptr, requested_bytes);
        if (bytes_read > 0) {
            file->position += (int32_t)bytes_read;
        }

        return bytes_read / size;
    }

    int32_t Zip_File_Size_(s_vfs_file* file) {
        if (file == nullptr) {
            return -1;
        }

        return file->data_size;
    }

    int32_t Zip_File_Seek_(s_vfs_file* file, int32_t offset, int32_t origin) {
        int64_t new_position;

        if (file == nullptr) {
            return -1;
        }

        switch (origin) {
            case SEEK_SET:
                new_position = offset;
                break;
            case SEEK_CUR:
                new_position = (int64_t)file->position + offset;
                break;
            case SEEK_END:
                new_position = (int64_t)file->data_size + offset;
                break;
            default:
                return -1;
        }

        if (new_position < 0 || new_position > file->data_size) {
            return -1;
        }

        return Seek_Zip_File_(file, (int32_t)new_position) ? 0 : -1;
    }

    int32_t Zip_File_Tell_(s_vfs_file* file) {
        if (file == nullptr) {
            return -1;
        }

        return file->position;
    }

    int32_t Zip_File_Close_(s_vfs_file* file) {
        s_vfs_zip_file_stream* zip_stream;

        if (file == nullptr) {
            return -1;
        }

        zip_stream = Get_Zip_File_Stream_(file);
        Destroy_Zip_Iter_State_(zip_stream);
        delete zip_stream;
        delete file;
        return 0;
    }
} // namespace vfs_detail