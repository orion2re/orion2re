#include "pch.h"

#include "user_paths.h"

#include <mutex>

namespace {
    constexpr char k_native_path_separator =
#if defined(_WIN32)
        '\\';
#else
        '/';
#endif

    const char* Get_File_Name(const char* path) {
        const char* slash = strrchr(path, '/');
        const char* backslash = strrchr(path, '\\');
        const char* file_name = path;

        if (slash != nullptr && slash + 1 > file_name) {
            file_name = slash + 1;
        }

        if (backslash != nullptr && backslash + 1 > file_name) {
            file_name = backslash + 1;
        }

        return file_name;
    }

    bool Build_Uppercase_File_Name(const char* path, char* upper_path, size_t upper_path_size) {
        if (SDL_strlcpy(upper_path, path, upper_path_size) >= upper_path_size) {
            return false;
        }

        char* file_name = const_cast<char*>(Get_File_Name(upper_path));
        while (*file_name != '\0') {
            *file_name = (char)SDL_toupper((unsigned char)*file_name);
            ++file_name;
        }

        return true;
    }

    bool Mode_Creates_File(const char* mode) {
        return mode[0] == 'w' || mode[0] == 'a';
    }


    // Names of a directory by lowercase name, kept while the directory's
    // modification time is unchanged. A lookup that misses the exact case
    // would otherwise list the directory again every time, which is slow for
    // the many search directories of an installed game (the executable's
    // directory can be /usr/bin).
    struct s_directory_index {
        SDL_Time modify_time;
        std::unordered_map<std::string, std::string> names;
    };

    // A directory changed this recently can change again within the same
    // timestamp, so its listing is not kept.
    constexpr SDL_Time k_directory_index_settle_ns = SDL_SECONDS_TO_NS(2);

    std::mutex directory_index_mutex;
    std::unordered_map<std::string, s_directory_index> directory_indexes;

    std::string Lowercase_Name(const char* name) {
        std::string lowercase(name);
        for (char& character : lowercase) {
            character = (char)SDL_tolower((unsigned char)character);
        }
        return lowercase;
    }

    SDL_EnumerationResult SDLCALL Add_Directory_Index_Name(void* userdata, const char*, const char* fname) {
        // The first name listed wins, as when the listing stopped at a match.
        ((s_directory_index*)userdata)->names.emplace(Lowercase_Name(fname), fname);
        return SDL_ENUM_CONTINUE;
    }

    bool Write_Case_Resolved_Path(const char* directory_path, const char* file_name, char* resolved_path, size_t resolved_path_size) {
        const size_t directory_length = strlen(directory_path);
        const bool needs_separator = directory_length > 0 &&
            directory_path[directory_length - 1] != '/' && directory_path[directory_length - 1] != '\\';
        const char* separator = needs_separator ? "/" : "";
        const int length = snprintf(resolved_path, resolved_path_size, "%s%s%s", directory_path, separator, file_name);
        return length >= 0 && (size_t)length < resolved_path_size;
    }

    struct s_directory_scan {
        const char* directory_path;
        const char* requested_name;
        char* resolved_path;
        size_t resolved_path_size;
        bool found = false;
    };

    SDL_EnumerationResult SDLCALL Scan_Directory_For_Name(void* userdata, const char*, const char* fname) {
        auto* scan = (s_directory_scan*)userdata;
        const char* requested = scan->requested_name;
        const char* entry = fname;
        while (*requested != '\0' && SDL_tolower((unsigned char)*requested) == SDL_tolower((unsigned char)*entry)) {
            ++requested;
            ++entry;
        }
        if (*requested != *entry) {
            return SDL_ENUM_CONTINUE;
        }
        scan->found = Write_Case_Resolved_Path(
            scan->directory_path, fname, scan->resolved_path, scan->resolved_path_size);
        return scan->found ? SDL_ENUM_SUCCESS : SDL_ENUM_FAILURE;
    }

    bool Find_Case_Insensitive_File_Match(const char* directory_path, const char* file_name, char* resolved_path, size_t resolved_path_size) {
        SDL_PathInfo directory_info;
        if (!SDL_GetPathInfo(directory_path, &directory_info) || directory_info.type != SDL_PATHTYPE_DIRECTORY) {
            return false;
        }

        std::unique_lock<std::mutex> lock(directory_index_mutex);
        auto cached = directory_indexes.find(directory_path);
        if (cached == directory_indexes.end() || cached->second.modify_time != directory_info.modify_time) {
            if (cached != directory_indexes.end()) {
                directory_indexes.erase(cached);
            }
            SDL_Time now = 0;
            if (!SDL_GetCurrentTime(&now) || directory_info.modify_time >= now - k_directory_index_settle_ns) {
                lock.unlock();
                s_directory_scan scan{directory_path, file_name, resolved_path, resolved_path_size};
                return SDL_EnumerateDirectory(directory_path, Scan_Directory_For_Name, &scan) && scan.found;
            }

            s_directory_index listed{directory_info.modify_time, {}};
            if (!SDL_EnumerateDirectory(directory_path, Add_Directory_Index_Name, &listed)) {
                return false;
            }
            cached = directory_indexes.emplace(directory_path, std::move(listed)).first;
        }

        const auto& names = cached->second.names;
        const auto name = names.find(Lowercase_Name(file_name));
        return name != names.end() &&
            Write_Case_Resolved_Path(directory_path, name->second.c_str(), resolved_path, resolved_path_size);
    }
} // namespace


bool File_Exists(const char* path) {
    SDL_PathInfo path_info;
    char user_path[1024];

    if (path == nullptr || path[0] == '\0') {
        return false;
    }

    if (user_paths::Redirect_Legacy_Path(path, false, user_path, sizeof(user_path))) {
        path = user_path;
    }
    return SDL_GetPathInfo(path, &path_info) && path_info.type == SDL_PATHTYPE_FILE;
}

bool Cleanup_Path(const char* input_path, char* output_path, size_t output_path_size, bool ensure_trailing_separator) {
    if (input_path == nullptr || output_path == nullptr || output_path_size == 0) {
        return false;
    }

    size_t write_pos = 0;
    bool last_was_separator = false;

    for (size_t read_pos = 0; input_path[read_pos] != '\0'; ++read_pos) {
        char ch = input_path[read_pos];
        if (ch == '/' || ch == '\\') {
            ch = k_native_path_separator;
            if (last_was_separator) {
                continue;
            }
            last_was_separator = true;
        } else {
            last_was_separator = false;
        }

        if (write_pos + 1 >= output_path_size) {
            return false;
        }

        output_path[write_pos++] = ch;
    }

    output_path[write_pos] = '\0';

    while (write_pos >= 2 &&
        output_path[write_pos - 2] == '.' &&
        output_path[write_pos - 1] == k_native_path_separator) {
        output_path[write_pos - 2] = '\0';
        write_pos -= 2;
    }

    if (strcmp(output_path, ".") == 0 ||
        (output_path[0] == '.' && output_path[1] == k_native_path_separator && output_path[2] == '\0')) {
        output_path[0] = '\0';
        write_pos = 0;
    }

    while (write_pos >= 3 &&
        output_path[0] == '.' &&
        output_path[1] == k_native_path_separator &&
        output_path[2] == '.') {
        memmove(output_path, output_path + 2, write_pos - 1);
        write_pos -= 2;
    }

    if (ensure_trailing_separator) {
        if (write_pos > 0 && output_path[write_pos - 1] != k_native_path_separator) {
            if (write_pos + 1 >= output_path_size) {
                return false;
            }
            output_path[write_pos++] = k_native_path_separator;
            output_path[write_pos] = '\0';
        }
    } else {
        while (write_pos > 1 && output_path[write_pos - 1] == k_native_path_separator) {
            output_path[--write_pos] = '\0';
        }
    }

    return true;
}

bool Resolve_File_Path_Case(const char* path, char* resolved_path, size_t resolved_path_size) {
    char clean_path[512];
    char directory_path[512];
    char resolved_directory_path[512];
    const char* open_path;
    const char* slash;
    const char* backslash;
    const char* separator;
    const char* file_name;
    size_t directory_length;
    SDL_PathInfo path_info;

    if (path == nullptr || resolved_path == nullptr || resolved_path_size == 0) {
        return false;
    }

    if (!Cleanup_Path(path, clean_path, sizeof(clean_path), false)) {
        return false;
    }

    open_path = clean_path[0] != '\0' ? clean_path : path;
    if (SDL_strlcpy(resolved_path, open_path, resolved_path_size) >= resolved_path_size) {
        return false;
    }

    if (SDL_GetPathInfo(resolved_path, &path_info)) {
        return true;
    }

    slash = strrchr(open_path, '/');
    backslash = strrchr(open_path, '\\');
    separator = slash;
    if (backslash != nullptr && (separator == nullptr || backslash > separator)) {
        separator = backslash;
    }

    file_name = separator != nullptr ? separator + 1 : open_path;
    if (file_name[0] == '\0') {
        return false;
    }

    if (separator != nullptr) {
        directory_length = (size_t)(file_name - open_path);
        if (directory_length >= sizeof(directory_path)) {
            return false;
        }

        memcpy(directory_path, open_path, directory_length);
        directory_path[directory_length] = '\0';
    } else {
        directory_path[0] = '.';
        directory_path[1] = '\0';
    }

    if (!Resolve_File_Path_Case(directory_path, resolved_directory_path, sizeof(resolved_directory_path))) {
        return false;
    }

    return Find_Case_Insensitive_File_Match(resolved_directory_path, file_name, resolved_path, resolved_path_size);
}

void Clear_Directory_Indexes() {
    const std::lock_guard<std::mutex> lock(directory_index_mutex);
    directory_indexes.clear();
}

FILE* fopen_case(const char* path, const char* mode) {
    char user_path[1024];
    char clean_path[512];
    char upper_path[512];
    char cwd[512];
    char* cwd_alloc;
    const char* open_path;
    bool has_upper_path;
    int original_errno;

    if (path == nullptr || mode == nullptr) {
        errno = EINVAL;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "fopen_case invalid args: path=%p mode=%p", (const void*)path, (const void*)mode);
        return nullptr;
    }

    if (user_paths::Redirect_Legacy_Path(path, Mode_Creates_File(mode) || strchr(mode, '+') != nullptr, user_path, sizeof(user_path))) {
        if (user_path[0] == '\0') return nullptr;
        path = user_path;
    }

    if (!Cleanup_Path(path, clean_path, sizeof(clean_path), false)) {
        errno = ENAMETOOLONG;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "fopen_case path cleanup failed: requested='%s' mode='%s'", path, mode);
        return nullptr;
    }

    open_path = clean_path[0] != '\0' ? clean_path : path;

    has_upper_path = Build_Uppercase_File_Name(open_path, upper_path, sizeof(upper_path)) &&
        strcmp(open_path, upper_path) != 0;

    cwd_alloc = SDL_GetCurrentDirectory();
    if (cwd_alloc != nullptr) {
        SDL_strlcpy(cwd, cwd_alloc, sizeof(cwd));
        SDL_free(cwd_alloc);
    } else {
        SDL_strlcpy(cwd, "<unavailable>", sizeof(cwd));
    }

    if (Mode_Creates_File(mode)) {
        if (File_Exists(open_path)) {
            return fopen(open_path, mode);
        }

        if (has_upper_path && File_Exists(upper_path)) {
            return fopen(upper_path, mode);
        }

        return fopen(open_path, mode);
    }

    FILE* file = fopen(open_path, mode);
    if (file != nullptr) {
        return file;
    }

    original_errno = errno;

    if (has_upper_path) {
        file = fopen(upper_path, mode);
        if (file != nullptr) {
            SDL_LogInfo(
                SDL_LOG_CATEGORY_APPLICATION,
                "fopen_case fallback success: requested='%s' opened='%s' mode='%s' cwd='%s'",
                path,
                upper_path,
                mode,
                cwd);
            return file;
        }
    }

    SDL_LogWarn(
        SDL_LOG_CATEGORY_APPLICATION,
        "fopen_case failed: requested='%s' cleaned='%s' uppercase='%s' mode='%s' cwd='%s' exists=%d upper_exists=%d errno=%d (%s)",
        path,
        open_path,
        has_upper_path ? upper_path : "<none>",
        mode,
        cwd,
        File_Exists(open_path) ? 1 : 0,
        (has_upper_path && File_Exists(upper_path)) ? 1 : 0,
        original_errno,
        strerror(original_errno));

    return nullptr;
}

void Query_Memory_Status(s_memorystatus* memory_status) {
    uint64_t total_phys;
    uint32_t clamped_total_phys;

    if (memory_status == nullptr) {
        return;
    }

    SDL_zero(*memory_status);
    memory_status->dwLength = sizeof(*memory_status);

    total_phys = (uint64_t)SDL_GetSystemRAM() * 1024ull * 1024ull;
    if (total_phys > UINT32_MAX) {
        clamped_total_phys = UINT32_MAX;
    } else {
        clamped_total_phys = (uint32_t)total_phys;
    }

    memory_status->dwTotalPhys = clamped_total_phys;
    memory_status->dwAvailPhys = clamped_total_phys;
    memory_status->dwTotalPageFile = clamped_total_phys;
    memory_status->dwAvailPageFile = clamped_total_phys;
    memory_status->dwTotalVirtual = clamped_total_phys;
    memory_status->dwAvailVirtual = clamped_total_phys;
}

int Show_Message_Box(void* window_handle, const char* text, const char* caption, bool is_error) {
    return SDL_ShowSimpleMessageBox(
        is_error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION,
        caption != nullptr ? caption : "",
        text != nullptr ? text : "",
        (SDL_Window*)window_handle);
}