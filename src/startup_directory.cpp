#include "pch.h"

#include <filesystem>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#endif

#include "startup_directory.h"

namespace {
    bool Has_Packaged_Base_Data_(const std::string& directory) {
        if (directory.empty()) {
            return false;
        }
        std::error_code error;
        return std::filesystem::is_regular_file(
            std::filesystem::path(std::u8string(directory.begin(), directory.end())) / "data" / "base.zip", error);
    }
} // namespace

namespace startup {
    bool Set_Game_Directory(std::string* directory, std::string* error) {
        if (directory == nullptr || error == nullptr) {
            return false;
        }
        directory->clear();
        error->clear();

#if defined(__APPLE__)
        // Packaged apps symlink to the executable beside the original game data.
        char executable_path[PATH_MAX];
        uint32_t executable_path_size = sizeof(executable_path);
        char resolved_path[PATH_MAX];
        if (_NSGetExecutablePath(executable_path, &executable_path_size) != 0) {
            *error = "unable to resolve executable path: path is too long";
            return false;
        }
        if (realpath(executable_path, resolved_path) == nullptr) {
            *error = std::string("unable to resolve executable path: ") + strerror(errno);
            return false;
        }
        char* filename = strrchr(resolved_path, '/');
        if (filename == nullptr) {
            *error = std::string("executable path has no directory: '") + resolved_path + "'";
            return false;
        }
        filename[1] = '\0';
        const char* base_path = resolved_path;
#else
        const char* base_path = SDL_GetBasePath();
        if (base_path == nullptr || base_path[0] == '\0') {
            *error = std::string("unable to resolve game directory: ") + SDL_GetError();
            return false;
        }
#endif

        std::error_code change_error;
        const std::u8string utf8_path(base_path, base_path + strlen(base_path));
        std::filesystem::current_path(std::filesystem::path(utf8_path), change_error);
        if (change_error) {
            *error = std::string("unable to set working directory to '") + base_path + "': " + change_error.message();
            return false;
        }
        *directory = base_path;
        return true;
    }

    bool Resolve_Data_Directory(const std::string& game_directory, const char* installed_datadir, std::string* directory) {
        if (directory == nullptr) {
            return false;
        }
        directory->clear();

        // A game directory that ships its own data wins: development builds
        // and distributable archives keep the executable and data together.
        if (Has_Packaged_Base_Data_(game_directory)) {
            *directory = game_directory;
            return true;
        }

        // Otherwise fall back to the installed data directory, when one was
        // baked in at build time and actually holds the packaged base data.
        if (installed_datadir != nullptr && installed_datadir[0] != '\0' &&
            Has_Packaged_Base_Data_(installed_datadir)) {
            *directory = installed_datadir;
            return true;
        }

        // No packaged data anywhere: keep the historical behaviour and let
        // the VFS report the missing data for the game directory.
        *directory = game_directory;
        return true;
    }
} // namespace startup
