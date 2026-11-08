#pragma once

#include <stddef.h>
#include <string>
#include <vector>

namespace user_paths {
    enum e_user_directory {
        USER_DIRECTORY_CONFIG,
        USER_DIRECTORY_DATA,
        USER_DIRECTORY_STATE,
        USER_DIRECTORY_CACHE,
        USER_DIRECTORY_COUNT,
    };

#if defined(__linux__)
    inline constexpr bool PLATFORM_USES_XDG = true;
#else
    inline constexpr bool PLATFORM_USES_XDG = false;
#endif

    inline constexpr const char XDG_MARKER_NAME[] = "xdg";

    // Directory arguments must end with a separator. The explicit platform flag
    // lets tests exercise XDG resolution without changing non-Linux startup.
    bool Init(const char* executable_directory, bool platform_uses_xdg, const char* installed_data_directory = nullptr);
    void Shutdown();
    bool Is_Enabled();

    // Directory with a trailing '/', or "" when disabled.
    const char* Get_Directory(e_user_directory directory);
    const std::vector<std::string>& Get_Game_Asset_Directories();

    // Only recognized bare legacy names and DOS save patterns are redirected.
    // Explicit paths and all paths in portable mode are left alone.
    // An insufficient output buffer yields an empty path and ENAMETOOLONG, but
    // still reports redirected so callers cannot fall back to the installation.
    bool Redirect_Legacy_Path(const char* path, bool writing, char* output_path, size_t output_path_size);
} // namespace user_paths
