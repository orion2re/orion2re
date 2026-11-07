#include "pch.h"

#include "user_paths.h"

namespace dos {
    namespace {
        enum {
            NANOSECONDS_PER_SECOND = 1000000000,
        };

        const char* Get_File_Name(const char* path) {
            const char* slash;
            const char* backslash;

            slash = strrchr(path, '/');
            backslash = strrchr(path, '\\');

            if (slash == nullptr) {
                slash = path;
            } else {
                ++slash;
            }

            if (backslash == nullptr) {
                return slash;
            }

            ++backslash;
            if (backslash > slash) {
                return backslash;
            }

            return slash;
        }

        void Split_Search_Pattern(const char* pattern, char* directory, size_t directory_size, char* filename_pattern, size_t filename_pattern_size) {
            const char* file_name;
            size_t directory_length;

            file_name = Get_File_Name(pattern);
            if (file_name == pattern) {
                strlcpy(directory, ".", directory_size);
                strlcpy(filename_pattern, pattern, filename_pattern_size);
                return;
            }

            directory_length = (size_t)(file_name - pattern);
            if (directory_length >= directory_size) {
                directory_length = directory_size - 1;
            }

            memcpy(directory, pattern, directory_length);
            directory[directory_length] = '\0';

            if (directory_length > 0 &&
                (directory[directory_length - 1] == '/' || directory[directory_length - 1] == '\\')) {
                directory[directory_length - 1] = '\0';
            }

            if (directory[0] == '\0') {
                strlcpy(directory, ".", directory_size);
            }

            strlcpy(filename_pattern, file_name, filename_pattern_size);
        }
    } // namespace

    bool __cdecl DIR(const char* pattern, char* output_name, size_t output_name_size) {
        char directory[DOS_MAX_PATH];
        char filename_pattern[DOS_MAX_PATH];
        char** matches;
        int count;

        if (output_name == nullptr || output_name_size == 0) {
            return false;
        }

        if (pattern == nullptr || pattern[0] == '\0') {
            output_name[0] = '\0';
            return false;
        }

        char user_pattern[DOS_MAX_PATH];
        if (user_paths::Redirect_Legacy_Path(pattern, false, user_pattern, sizeof(user_pattern))) {
            pattern = user_pattern;
        }

        Split_Search_Pattern(pattern, directory, sizeof(directory), filename_pattern, sizeof(filename_pattern));

        matches = SDL_GlobDirectory(directory, filename_pattern, SDL_GLOB_CASEINSENSITIVE, &count);
        if (matches == nullptr || count <= 0) {
            if (matches != nullptr) {
                SDL_free(matches);
            }
            output_name[0] = '\0';
            return false;
        }

        strlcpy(output_name, Get_File_Name(matches[0]), output_name_size);
        SDL_free(matches);
        return true;
    }

    void __cdecl Delete_File(const char* path) {
        char user_path[DOS_MAX_PATH];
        if (user_paths::Redirect_Legacy_Path(path, true, user_path, sizeof(user_path))) {
            path = user_path;
        }
        remove(path);
    }

    int32_t __cdecl Disk_Free(void) {
        uint64_t available_bytes;
        SDL_Storage* storage;

        storage = SDL_OpenFileStorage(nullptr);
        if (storage == nullptr) {
            return 0;
        }

        if (!SDL_StorageReady(storage)) {
            SDL_CloseStorage(storage);
            return 0;
        }

        available_bytes = (uint64_t)SDL_GetStorageSpaceRemaining(storage);
        SDL_CloseStorage(storage);

        if (available_bytes > INT32_MAX) {
            return INT32_MAX;
        }

        return (int32_t)available_bytes;
    }

    struct tm* __cdecl Time_Stamp(const char* pathname) {
        SDL_PathInfo path_info;
        time_t modified_time;

        if (pathname == nullptr) {
            return nullptr;
        }

        char user_path[DOS_MAX_PATH];
        if (user_paths::Redirect_Legacy_Path(pathname, false, user_path, sizeof(user_path))) {
            pathname = user_path;
        }

        if (!SDL_GetPathInfo(pathname, &path_info) || path_info.type != SDL_PATHTYPE_FILE) {
            return nullptr;
        }

        modified_time = (time_t)(path_info.modify_time / NANOSECONDS_PER_SECOND);
        return localtime(&modified_time);
    }
} // namespace dos