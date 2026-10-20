#include "pch.h"

#include "user_paths.h"

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

    void Legacy_Qsort_Swap(uint8_t* first, uint8_t* second, size_t width) {
        if (first == second) {
            return;
        }

        while (width-- > 0) {
            uint8_t value = *first;
            *first++ = *second;
            *second++ = value;
        }
    }

    void Legacy_Qsort_Short(uint8_t* low, uint8_t* high, size_t width, legacy_qsort_compare compare) {
        while (high > low) {
            uint8_t* maximum = low;
            for (uint8_t* current = low + width; current <= high; current += width) {
                if (compare(current, maximum) > 0) {
                    maximum = current;
                }
            }

            Legacy_Qsort_Swap(maximum, high, width);
            high -= width;
        }
    }

    struct s_case_insensitive_file_match {
        const char* requested_file_name;
        char* resolved_path;
        size_t resolved_path_size;
        bool found;
    };

    SDL_EnumerationResult SDLCALL Find_Case_Insensitive_File_Match(void* userdata, const char* dirname, const char* fname) {
        s_case_insensitive_file_match* match = (s_case_insensitive_file_match*)userdata;

        if (strcasecmp(fname, match->requested_file_name) != 0) {
            return SDL_ENUM_CONTINUE;
        }

        if (snprintf(match->resolved_path, match->resolved_path_size, "%s%s", dirname, fname) >= (int)match->resolved_path_size) {
            return SDL_ENUM_FAILURE;
        }

        match->found = true;
        return SDL_ENUM_SUCCESS;
    }
} // namespace

void Legacy_Qsort(void* base, size_t count, size_t width, legacy_qsort_compare compare) {
    if (count < 2 || width == 0) {
        return;
    }

    uint8_t* low = (uint8_t*)base;
    uint8_t* high = low + (count - 1) * width;
    uint8_t* low_stack[30];
    uint8_t* high_stack[30];
    int stack_count = 0;

    while (true) {
        const size_t partition_count = (size_t)(high - low) / width + 1;
        if (partition_count <= 8) {
            Legacy_Qsort_Short(low, high, width, compare);
        } else {
            Legacy_Qsort_Swap(low + (partition_count / 2) * width, low, width);

            uint8_t* low_scan = low;
            uint8_t* high_scan = high + width;
            while (true) {
                do {
                    low_scan += width;
                } while (low_scan <= high && compare(low_scan, low) <= 0);

                do {
                    high_scan -= width;
                } while (high_scan > low && compare(high_scan, low) >= 0);

                if (high_scan < low_scan) {
                    break;
                }

                Legacy_Qsort_Swap(low_scan, high_scan, width);
            }

            Legacy_Qsort_Swap(low, high_scan, width);

            const ptrdiff_t left_span = high_scan - low - 1;
            const ptrdiff_t right_span = high - low_scan;
            if (left_span >= right_span) {
                if (low_scan < high) {
                    low_stack[stack_count] = low_scan;
                    high_stack[stack_count] = high;
                    ++stack_count;
                }

                if (low + width < high_scan) {
                    high = high_scan - width;
                    continue;
                }
            } else {
                if (low + width < high_scan) {
                    low_stack[stack_count] = low;
                    high_stack[stack_count] = high_scan - width;
                    ++stack_count;
                }

                if (low_scan < high) {
                    low = low_scan;
                    continue;
                }
            }
        }

        if (stack_count == 0) {
            return;
        }

        --stack_count;
        low = low_stack[stack_count];
        high = high_stack[stack_count];
    }
}

bool File_Exists(const char* path) {
    SDL_PathInfo path_info;
    char user_path[512];

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
    s_case_insensitive_file_match match;
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

    match.requested_file_name = file_name;
    match.resolved_path = resolved_path;
    match.resolved_path_size = resolved_path_size;
    match.found = false;

    if (!SDL_EnumerateDirectory(resolved_directory_path,
            Find_Case_Insensitive_File_Match,
            &match)) {
        return false;
    }

    return match.found;
}

FILE* fopen_case(const char* path, const char* mode) {
    char user_path[512];
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