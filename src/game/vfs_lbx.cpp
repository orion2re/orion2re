#include "pch.h"

#include "vfs.h"

#include "vfs_detail.h"

namespace vfs_detail {
    constexpr uint16_t VFS_LBX_MAX_ENTRIES = 509;
    constexpr uint16_t VFS_LBX_OFFSET_COUNT = 510;

    const s_vfs_backend_ops g_vfs_slice_file_ops = {
        Slice_File_Read_,
        Slice_File_Size_,
        Slice_File_Seek_,
        Slice_File_Tell_,
        Slice_File_Close_,
    };

    bool Read_Lbx_Header_(s_vfs_file* file, s_lbx_disk_header* header, uint32_t* offsets) {
        if (file == nullptr || header == nullptr || offsets == nullptr) {
            return false;
        }

        if (vfs::fseek(file, 0, SEEK_SET) != 0) {
            return false;
        }

        if (vfs::fread(header, sizeof(*header), 1, file) != 1) {
            return false;
        }

        if (header->magic != 0xFEAD || header->entry_count > VFS_LBX_MAX_ENTRIES) {
            return false;
        }

        if (vfs::fread(offsets, sizeof(offsets[0]), VFS_LBX_OFFSET_COUNT, file) != VFS_LBX_OFFSET_COUNT) {
            return false;
        }

        return true;
    }

    bool Build_Lbx_Entry_Path_(const char* lbx_name, int32_t entry_idx, char* output_path, size_t output_path_size) {
        if (lbx_name == nullptr || output_path == nullptr || output_path_size == 0 || entry_idx < 0) {
            return false;
        }

        return snprintf(output_path, output_path_size, "%s/%03d", lbx_name, entry_idx) < (int)output_path_size;
    }

    bool Get_Lbx_Entry_Range_(const uint32_t* offsets, uint16_t entry_count, int32_t entry_idx, int32_t* data_offset, int32_t* data_size) {
        uint32_t start_offset;
        uint32_t end_offset;

        if (offsets == nullptr || data_offset == nullptr || data_size == nullptr) {
            return false;
        }

        if (entry_idx < 0 || entry_idx >= (int32_t)entry_count) {
            return false;
        }

        start_offset = entry_idx == 0 ? offsets[0] : offsets[entry_idx];
        end_offset = offsets[entry_idx + 1];
        if (end_offset < start_offset || end_offset > INT32_MAX) {
            return false;
        }

        *data_offset = (int32_t)start_offset;
        *data_size = (int32_t)(end_offset - start_offset);
        return true;
    }

    s_vfs_file* Open_Slice_File_(s_vfs_file* parent_file, int32_t data_offset, int32_t data_size) {
        s_vfs_slice_file_stream* slice_stream;
        s_vfs_file* slice_file;

        if (parent_file == nullptr || data_offset < 0 || data_size < 0) {
            return nullptr;
        }

        if (vfs::fseek(parent_file, data_offset, SEEK_SET) != 0) {
            vfs::fclose(parent_file);
            return nullptr;
        }

        slice_stream = new s_vfs_slice_file_stream{};
        slice_stream->parent_file = parent_file;
        slice_stream->data_offset = data_offset;

        slice_file = new s_vfs_file{};
        slice_file->backend_kind = VFS_BACKEND_SLICE;
        slice_file->ops = &g_vfs_slice_file_ops;
        slice_file->backend_state = slice_stream;
        slice_file->data_size = data_size;
        slice_file->position = 0;
        slice_file->source_kind = parent_file->source_kind;
        slice_file->source_layer = parent_file->source_layer;
        return slice_file;
    }

    s_vfs_slice_file_stream* Get_Slice_File_Stream_(s_vfs_file* file) {
        if (file == nullptr || file->backend_kind != VFS_BACKEND_SLICE) {
            return nullptr;
        }

        return (s_vfs_slice_file_stream*)file->backend_state;
    }

    size_t Slice_File_Read_(void* ptr, size_t size, size_t count, s_vfs_file* file) {
        s_vfs_slice_file_stream* slice_stream;
        size_t remaining_bytes;
        size_t max_count;
        size_t read_count;

        if (ptr == nullptr || file == nullptr || size == 0 || count == 0) {
            return 0;
        }

        if (file->position >= file->data_size) {
            return 0;
        }

        slice_stream = Get_Slice_File_Stream_(file);
        if (slice_stream == nullptr || slice_stream->parent_file == nullptr) {
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

        if (vfs::fseek(slice_stream->parent_file, slice_stream->data_offset + file->position, SEEK_SET) != 0) {
            return 0;
        }

        read_count = vfs::fread(ptr, size, max_count, slice_stream->parent_file);
        if (read_count > 0) {
            file->position += (int32_t)(read_count * size);
        }

        return read_count;
    }

    int32_t Slice_File_Size_(s_vfs_file* file) {
        if (file == nullptr) {
            return -1;
        }

        return file->data_size;
    }

    int32_t Slice_File_Seek_(s_vfs_file* file, int32_t offset, int32_t origin) {
        s_vfs_slice_file_stream* slice_stream;
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

        slice_stream = Get_Slice_File_Stream_(file);
        if (slice_stream == nullptr || slice_stream->parent_file == nullptr) {
            return -1;
        }

        if (vfs::fseek(slice_stream->parent_file, slice_stream->data_offset + (int32_t)new_position, SEEK_SET) != 0) {
            return -1;
        }

        file->position = (int32_t)new_position;
        return 0;
    }

    int32_t Slice_File_Tell_(s_vfs_file* file) {
        if (file == nullptr) {
            return -1;
        }

        return file->position;
    }

    int32_t Slice_File_Close_(s_vfs_file* file) {
        s_vfs_slice_file_stream* slice_stream;
        int result;

        if (file == nullptr) {
            return -1;
        }

        slice_stream = Get_Slice_File_Stream_(file);
        result = (slice_stream != nullptr && slice_stream->parent_file != nullptr)
            ? vfs::fclose(slice_stream->parent_file)
            : -1;
        delete slice_stream;
        delete file;
        return result;
    }
} // namespace vfs_detail

namespace vfs {
    s_vfs_file* fopen_lbx(const char* lbx_name, int32_t entry_idx) {
        char entry_path[vfs_detail::VFS_MAX_PATH];
        s_vfs_file* entry_override_file;
        s_vfs_file* whole_lbx_file;
        s_lbx_disk_header header{};
        uint32_t offsets[vfs_detail::VFS_LBX_OFFSET_COUNT] = {};
        int32_t data_offset;
        int32_t data_size;

        if (!vfs_detail::g_vfs_state.initialized || lbx_name == nullptr || entry_idx < 0) {
            return nullptr;
        }

        if (vfs_detail::Build_Lbx_Entry_Path_(lbx_name, entry_idx, entry_path, sizeof(entry_path))) {
            entry_override_file = vfs::fopen(entry_path, "rb");
            if (entry_override_file != nullptr) {
                return entry_override_file;
            }
        }

        whole_lbx_file = vfs::fopen(lbx_name, "rb");
        if (whole_lbx_file == nullptr) {
            return nullptr;
        }

        if (!vfs_detail::Read_Lbx_Header_(whole_lbx_file, &header, offsets) ||
            !vfs_detail::Get_Lbx_Entry_Range_(offsets, header.entry_count, entry_idx, &data_offset, &data_size)) {
            vfs::fclose(whole_lbx_file);
            return nullptr;
        }

        return vfs_detail::Open_Slice_File_(whole_lbx_file, data_offset, data_size);
    }
} // namespace vfs