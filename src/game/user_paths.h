#pragma once

#include <stddef.h>

#include <string>
#include <vector>

// Per-user file locations. By default the game is portable: every query
// reports "disabled" and callers keep using the working directory exactly as
// the original game did. On Linux the XDG Base Directory Specification is
// followed instead when requested, or when the executable's directory is not
// writable (a system-wide installation).
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

    // Name of the marker file next to the executable that selects XDG mode.
    inline constexpr const char XDG_MARKER_NAME[] = "xdg";

    // executable_directory must end with a path separator. Returns true when
    // XDG directories are in use.
    bool Init(const char* executable_directory, bool platform_uses_xdg);
    void Shutdown();

    bool Is_Enabled();

    // Directory with a trailing '/', or "" when disabled.
    const char* Get_Directory(e_user_directory directory);

    // Directories searched for the original game files after the executable
    // directory, each with a trailing '/'. Empty when disabled.
    const std::vector<std::string>& Get_Game_Asset_Directories();

    // Maps a bare legacy file name (or DOS search pattern) that the original
    // game kept in its working directory to its per-user location. Paths with
    // a directory component are never redirected. Returns false and leaves
    // output untouched when the path is not redirected.
    bool Redirect_Legacy_Path(const char* path, bool writing, char* output_path, size_t output_path_size);
} // namespace user_paths
