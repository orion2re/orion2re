#include "pch.h"

#include <filesystem>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#endif

#include "startup_directory.h"

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
        const auto has_packaged_data = [](const std::string& root) {
            if (root.empty()) return false;
            std::error_code error;
            return std::filesystem::is_regular_file(
                std::filesystem::path(std::u8string(root.begin(), root.end())) / "data" / "base.zip", error);
        };

        // Never discover a runtime tree through HOME or XDG: user config and
        // mods have their own layers, and portable mode must remain isolated.
        *directory = game_directory;
        if (!has_packaged_data(game_directory) && installed_datadir != nullptr && has_packaged_data(installed_datadir)) {
            *directory = installed_datadir;
        }
        return true;
    }
} // namespace startup
