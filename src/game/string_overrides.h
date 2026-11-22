#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace string_overrides {
    // Sidecars use the LBX path with .json5 in place of .lbx. All IDs are 1-based.
    // Cached parses are checked against current VFS contents on every access.
    // Copies never retain cache pointers; resetting only releases cached storage.
    void Reset_Cache();
    bool Try_Apply_String(const char* filename, size_t id, char* dest, size_t capacity, std::string* diagnostic);
    // Rebuilds owned storage before callers publish pointers. Input size bounds
    // disk parsing, never the size of the overridden output.
    bool Try_Apply_Table(const char* filename, std::vector<char>* data, size_t count, size_t first_id, bool skip_padding, std::string* diagnostic);
    void Apply_String(const char* filename, size_t id, char* dest, size_t capacity);
    void Apply_Table(const char* filename, std::vector<char>* data, size_t count, size_t first_id = 1, bool skip_padding = false);

    // Reuse resolved KENTEXT messages during battle. Changes to cached messages
    // become visible again when the combat scope ends.
    struct s_combat_message_cache_scope {
        s_combat_message_cache_scope();
        ~s_combat_message_cache_scope();
        s_combat_message_cache_scope(const s_combat_message_cache_scope&) = delete;
        s_combat_message_cache_scope& operator=(const s_combat_message_cache_scope&) = delete;
    };

    // Disk entry/record selection and logical string numbering belong to the caller.
    void Load_Message(const char* filename, int32_t entry, uint32_t record, uint16_t record_size, size_t id, char* dest, size_t capacity);
    char* Load_Raw_Table(const char* filename, int32_t entry, void* pool, int32_t allocation_mode, size_t count, size_t first_id = 1, bool skip_padding = false);
    // Only for standalone, exclusively owned pools. May replace and free *pool;
    // all pointers into the old table must be rebuilt by the caller.
    char* Reload_Raw_Table(const char* filename, int32_t entry, void** pool, size_t count, size_t first_id = 1, bool skip_padding = false);
} // namespace string_overrides