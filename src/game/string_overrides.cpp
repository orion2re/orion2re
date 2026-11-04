#include "pch.h"

#include "string_overrides.h"
#include "yyjson.h"

namespace string_overrides {
    namespace {
        bool Fail(const std::string& path, const std::string& reason, std::string* diagnostic) {
            *diagnostic = path + ": " + reason;
            return false;
        }

        // JSON5 identifier keys cannot start with a digit. Accept decimal IDs as a
        // small extension, without rewriting text inside strings or comments.
        std::string Quote_Numeric_Keys(std::string_view source) {
            std::string result;
            bool key = false;
            for (size_t i = 0; i < source.size();) {
                char c = source[i];
                if (c == '\'' || c == '"') {
                    const char quote = c;
                    result += source[i++];
                    while (i < source.size()) {
                        c = source[i++];
                        result += c;
                        if (c == '\\' && i < source.size()) {
                            result += source[i++];
                        } else if (c == quote) {
                            break;
                        }
                    }
                    key = false;
                } else if (c == '/' && i + 1 < source.size() && (source[i + 1] == '/' || source[i + 1] == '*')) {
                    const bool line = source[i + 1] == '/';
                    result += source[i++];
                    result += source[i++];
                    while (i < source.size()) {
                        c = source[i++];
                        result += c;
                        if (line && (c == '\n' || c == '\r')) break;
                        if (!line && c == '*' && i < source.size() && source[i] == '/') {
                            result += source[i++];
                            break;
                        }
                    }
                } else if (key && c >= '0' && c <= '9') {
                    result += '"';
                    do {
                        result += source[i++];
                    } while (i < source.size() && source[i] >= '0' && source[i] <= '9');
                    result += '"';
                    key = false;
                } else {
                    result += c;
                    ++i;
                    if (c == '{' || c == ',') {
                        key = true;
                    } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                        key = false;
                    }
                }
            }
            return result;
        }

        bool Decode_Bytes(std::string_view text, std::string* bytes) {
            bytes->clear();
            bytes->reserve(text.size());
            for (size_t i = 0; i < text.size(); ++i) {
                const auto c = (unsigned char)text[i];
                if (c == 0) return false;
                if (c < 0x80) {
                    bytes->push_back((char)c);
                } else {
                    // U+0080..U+00FF have only these two UTF-8 lead bytes.
                    // Preserve their numeric byte values, including game tokens.
                    if ((c != 0xC2 && c != 0xC3) || i + 1 == text.size()) return false;
                    const auto next = (unsigned char)text[++i];
                    if ((next & 0xC0) != 0x80) return false;
                    bytes->push_back((char)(((c & 0x03) << 6) | (next & 0x3F)));
                }
            }
            return true;
        }

        using t_overrides = std::map<size_t, std::string>;
        struct s_cached_sidecar {
            std::string source;
            t_overrides overrides;
        };
        std::map<std::string, s_cached_sidecar> cache;

        bool Read(const char* filename, const std::string& path, std::string* source, bool* exists, std::string* diagnostic) {
            source->clear();
            s_vfs_file* file = vfs::fopen(path.c_str(), "rb");
            *exists = file != nullptr;
            if (file == nullptr) return true;
            s_vfs_file* lbx = vfs::fopen(filename, "rb");
            const bool shadowed = vfs::File_Has_Higher_Priority(lbx, file);
            if (lbx != nullptr) vfs::fclose(lbx);
            if (shadowed) {
                vfs::fclose(file);
                *exists = false;
                return true;
            }
            const int32_t size = vfs::fsize(file);
            if (size < 0 || size > 4 * 1024 * 1024) {
                vfs::fclose(file);
                return Fail(path, "invalid sidecar size (maximum 4 MiB)", diagnostic);
            }
            source->resize(size_t(size));
            const bool read = vfs::fread(source->data(), 1, source->size(), file) == source->size();
            vfs::fclose(file);
            if (!read) {
                return Fail(path, "could not read sidecar", diagnostic);
            }
            return true;
        }

        bool Parse(const std::string& path, std::string_view original, t_overrides* overrides, std::string* diagnostic) {
            std::string source = Quote_Numeric_Keys(original);
            yyjson_read_err error{};
            std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> doc(
                yyjson_read_opts(source.data(), source.size(), YYJSON_READ_JSON5, nullptr, &error), yyjson_doc_free);
            if (!doc) {
                return Fail(path, "invalid JSON5 at byte " + std::to_string(error.pos) + ": " + error.msg, diagnostic);
            }
            yyjson_val* root = yyjson_doc_get_root(doc.get());
            if (!yyjson_is_obj(root)) {
                return Fail(path, "expected an object of 1-based string IDs and strings", diagnostic);
            }
            size_t i, count;
            yyjson_val *key, *value;
            yyjson_obj_foreach(root, i, count, key, value) {
                const char* begin = yyjson_get_str(key);
                const char* end = begin + yyjson_get_len(key);
                size_t id = 0;
                auto parsed = std::from_chars(begin, end, id);
                if (parsed.ec != std::errc{} || parsed.ptr != end || id == 0 || !yyjson_is_str(value)) {
                    return Fail(path, "expected positive decimal string IDs and string values", diagnostic);
                }
                std::string encoded;
                if (!Decode_Bytes(std::string_view(yyjson_get_str(value), yyjson_get_len(value)), &encoded)) {
                    return Fail(path, "string " + std::to_string(id) + " must contain only byte values U+0001..U+00FF (no NUL)", diagnostic);
                }
                if (!overrides->emplace(id, std::move(encoded)).second) {
                    return Fail(path, "duplicate string ID " + std::to_string(id), diagnostic);
                }
            }
            return true;
        }

        std::string Sidecar_Path(const char* filename) {
            std::string name(filename);
            for (char& c : name) {
                if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
                if (c == '\\') c = '/';
            }
            if (name.ends_with(".lbx")) name.resize(name.size() - 4);
            return name + ".json5";
        }

        const t_overrides* Get(const char* filename, const std::string& path, std::string* diagnostic) {
            std::string source;
            bool exists;
            if (!Read(filename, path, &source, &exists, diagnostic)) {
                cache.erase(path);
                return nullptr;
            }
            if (!exists) {
                cache.erase(path);
                static const t_overrides empty;
                return &empty;
            }

            // Compare resolved bytes, not timestamps: VFS precedence can change
            // and same-size edits may preserve a file's modification time.
            auto found = cache.find(path);
            if (found != cache.end() && found->second.source == source) {
                return &found->second.overrides;
            }
            s_cached_sidecar candidate;
            if (!Parse(path, source, &candidate.overrides, diagnostic)) {
                cache.erase(path);
                return nullptr;
            }
            candidate.source = std::move(source);
            found = cache.insert_or_assign(path, std::move(candidate)).first;
            return &found->second.overrides;
        }

        bool Copy(const t_overrides* overrides, size_t id, char* dest, size_t capacity, const std::string& path, std::string* diagnostic) {
            auto found = overrides->find(id);
            if (found == overrides->end()) return true;
            if (capacity == 0 || found->second.size() >= capacity) {
                return Fail(path, "string " + std::to_string(id) + " does not fit destination capacity " + std::to_string(capacity), diagnostic);
            }
            memset(dest, 0, capacity);
            memcpy(dest, found->second.data(), found->second.size());
            return true;
        }

        bool Packed(const t_overrides* overrides, std::vector<char>* data, size_t count, size_t first_id, bool skip_padding, const std::string& path, std::string* diagnostic) {
            std::vector<char> result;
            size_t pos = 0;
            for (size_t i = 0; i < count; ++i) {
                if (pos >= data->size()) {
                    return Fail(path, "truncated packed string table", diagnostic);
                }
                const char* end = (const char*)memchr(data->data() + pos, 0, data->size() - pos);
                if (!end) {
                    return Fail(path, "unterminated packed string table", diagnostic);
                }
                const size_t length = size_t(end - data->data()) - pos;
                auto found = overrides->find(first_id + i);
                const std::string_view text = found == overrides->end()
                    ? std::string_view(data->data() + pos, length)
                    : std::string_view(found->second);
                if (skip_padding && text.empty()) {
                    return Fail(path, "empty strings are not supported by this packed table", diagnostic);
                }
                if (text.size() >= size_t(INT32_MAX) - 16 - result.size()) {
                    return Fail(path, "overridden table exceeds allocator size limit", diagnostic);
                }
                result.insert(result.end(), text.begin(), text.end());
                result.push_back(0);
                pos += length + 1;
                if (skip_padding) {
                    while (pos < data->size() && (*data)[pos] == 0) ++pos;
                }
            }
            data->swap(result);
            return true;
        }

        bool Read_Raw_Table(const char* filename, int32_t entry, size_t count, size_t first_id, bool skip_padding, std::vector<char>* data) {
            s_vfs_file* file = vfs::fopen_lbx(filename, entry);
            if (!file) {
                farload::Error_Handler(filename, 1, entry);
                return false;
            }
            const int32_t size = vfs::fsize(file);
            if (size <= 0) {
                vfs::fclose(file);
                farload::Error_Handler(filename, 7, entry);
                return false;
            }
            data->resize(size_t(size));
            const bool read = vfs::fread(data->data(), 1, data->size(), file) == data->size();
            vfs::fclose(file);
            if (!read) {
                farload::Error_Handler(filename, 7, entry);
                return false;
            }
            std::string diagnostic;
            if (!Try_Apply_Table(filename, data, count, first_id, skip_padding, &diagnostic)) {
                exitm::Exit_With_Message(diagnostic.c_str());
                return false;
            }
            return true;
        }

    } // namespace

    void Reset_Cache() {
        cache.clear();
    }

    bool Try_Apply_String(const char* filename, size_t id, char* dest, size_t capacity, std::string* diagnostic) {
        diagnostic->clear();
        const std::string path = Sidecar_Path(filename);
        if (id == 0 || dest == nullptr || capacity == 0)
            return Fail(path, "invalid string ID or destination", diagnostic);
        const t_overrides* overrides = Get(filename, path, diagnostic);
        return overrides != nullptr && Copy(overrides, id, dest, capacity, path, diagnostic);
    }

    bool Try_Apply_Table(const char* filename, std::vector<char>* data, size_t count, size_t first_id, bool skip_padding, std::string* diagnostic) {
        diagnostic->clear();
        const std::string path = Sidecar_Path(filename);
        if (first_id == 0 || count > SIZE_MAX - first_id || data == nullptr || data->empty())
            return Fail(path, "invalid string range or destination", diagnostic);
        const t_overrides* overrides = Get(filename, path, diagnostic);
        if (overrides == nullptr) return false;
        return Packed(overrides, data, count, first_id, skip_padding, path, diagnostic);
    }

    void Apply_String(const char* filename, size_t id, char* dest, size_t capacity) {
        std::string diagnostic;
        if (!Try_Apply_String(filename, id, dest, capacity, &diagnostic))
            exitm::Exit_With_Message(diagnostic.c_str());
    }

    void Apply_Table(const char* filename, std::vector<char>* data, size_t count, size_t first_id, bool skip_padding) {
        std::string diagnostic;
        if (!Try_Apply_Table(filename, data, count, first_id, skip_padding, &diagnostic))
            exitm::Exit_With_Message(diagnostic.c_str());
    }

    void Load_Message(const char* filename, int32_t entry, uint32_t record, uint16_t record_size, size_t id, char* dest, size_t capacity) {
        std::vector<char> original(record_size);
        farload::Farload_Data_Static(filename, entry, original.data(), record, 1, record_size);
        const std::string path = Sidecar_Path(filename);
        std::string diagnostic;
        const t_overrides* overrides = Get(filename, path, &diagnostic);
        if (overrides == nullptr) {
            exitm::Exit_With_Message(diagnostic.c_str());
            return;
        }
        if (overrides->contains(id)) {
            if (!Copy(overrides, id, dest, capacity, path, &diagnostic))
                exitm::Exit_With_Message(diagnostic.c_str());
            return;
        }
        const char* end = (const char*)memchr(original.data(), 0, original.size());
        if (!end || size_t(end - original.data()) >= capacity) {
            Fail(filename, "string " + std::to_string(id) + " is unterminated or exceeds destination capacity", &diagnostic);
            exitm::Exit_With_Message(diagnostic.c_str());
            return;
        }
        memcpy(dest, original.data(), size_t(end - original.data()) + 1);
    }

    char* Load_Raw_Table(const char* filename, int32_t entry, void* pool, int32_t allocation_mode, size_t count, size_t first_id, bool skip_padding) {
        std::vector<char> table;
        if (!Read_Raw_Table(filename, entry, count, first_id, skip_padding, &table)) return nullptr;
        void* data = nullptr;
        switch (allocation_mode) {
            case 0: data = allocate::Allocate_Space_No_Header((uint32_t)table.size()); break;
            case 1: data = allocate::Allocate_First_Block_No_Header(pool, (uint32_t)table.size()); break;
            case 2: data = allocate::Allocate_Next_Block_No_Header(pool, (uint32_t)table.size()); break;
            default: farload::Error_Handler(filename, 7, entry); return nullptr;
        }
        if (!data) {
            farload::Error_Handler(filename, allocation_mode == 0 ? 3 : 5, entry);
            return nullptr;
        }
        memcpy(data, table.data(), table.size());
        return (char*)data;
    }

    char* Reload_Raw_Table(const char* filename, int32_t entry, void** pool, size_t count, size_t first_id, bool skip_padding) {
        std::vector<char> table;
        if (!Read_Raw_Table(filename, entry, count, first_id, skip_padding, &table)) return nullptr;
        const uint32_t required = allocate::Nested_No_Header_Pool_Size((uint32_t)table.size());
        if (*pool == nullptr || allocate::Get_Max_Block_Space(*pool) < required) {
            void* replacement = allocate::Allocate_Space(required);
            if (*pool != nullptr) free((allocate::s_alloc_header*)*pool - 1);
            *pool = replacement;
        }
        char* data = (char*)allocate::Allocate_First_Block_No_Header(*pool, (uint32_t)table.size());
        if (!data) {
            farload::Error_Handler(filename, 5, entry);
            return nullptr;
        }
        memcpy(data, table.data(), table.size());
        return data;
    }
} // namespace string_overrides