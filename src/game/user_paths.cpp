#include "pch.h"

#include "user_paths.h"

#if !defined(_WIN32)
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace user_paths {
    namespace {
        constexpr const char* APPLICATION_DIRECTORY_NAME = "orion2re/";
        constexpr const char* GAME_ASSET_DIRECTORY_NAME = "orion2re/game/";
        constexpr const char* DEFAULT_DATA_DIRS = "/usr/local/share/:/usr/share/";

        enum e_legacy_file_kind {
            LEGACY_FILE_NONE,
            LEGACY_FILE_USER,
            // Lives in a user directory, but an installation may ship one.
            LEGACY_FILE_USER_WITH_INSTALL_FALLBACK,
            // Read-only installation file found next to the original game.
            LEGACY_FILE_INSTALL,
        };

        struct s_legacy_file {
            const char* name;
            e_user_directory directory;
            e_legacy_file_kind kind;
        };

        const s_legacy_file legacy_files[] = {
            {"HOF.M2", USER_DIRECTORY_DATA, LEGACY_FILE_USER},
            {"ORION2RE.RAC", USER_DIRECTORY_DATA, LEGACY_FILE_USER},
            {GAME_SETTINGS_FILENAME, USER_DIRECTORY_CONFIG, LEGACY_FILE_USER},
            {"LANGUAGE.INI", USER_DIRECTORY_CONFIG, LEGACY_FILE_USER_WITH_INSTALL_FALLBACK},
            {"TEMP.TMP", USER_DIRECTORY_CACHE, LEGACY_FILE_USER},
            {"ORIONCD.INI", USER_DIRECTORY_COUNT, LEGACY_FILE_INSTALL},
        };

        struct s_user_paths_state {
            bool enabled;
            std::string executable_directory;
            std::string directories[USER_DIRECTORY_COUNT];
            std::vector<std::string> game_asset_directories;
        };

        s_user_paths_state state;

        bool Is_Absolute_(const char* path) {
            return path != nullptr && path[0] == '/';
        }

        std::string With_Trailing_Separator_(std::string path) {
            if (path.empty() || path.back() != '/') {
                path += '/';
            }
            return path;
        }

        // The specification requires relative values to be treated as invalid.
        const char* Absolute_Environment_Value_(const char* name) {
            const char* value = SDL_getenv(name);
            return Is_Absolute_(value) ? value : nullptr;
        }

        bool Find_Home_Directory_(std::string* home) {
            const char* value = Absolute_Environment_Value_("HOME");
            if (value != nullptr) {
                *home = With_Trailing_Separator_(value);
                return true;
            }
#if !defined(_WIN32)
            const struct passwd* entry = getpwuid(getuid());
            if (entry != nullptr && Is_Absolute_(entry->pw_dir)) {
                *home = With_Trailing_Separator_(entry->pw_dir);
                return true;
            }
#endif
            return false;
        }

        std::string Base_Directory_(const char* variable, const std::string& home, const char* home_relative_default) {
            const char* value = Absolute_Environment_Value_(variable);
            return value != nullptr ? With_Trailing_Separator_(value) : home + home_relative_default;
        }

        bool Create_Directories_(const std::string& path) {
#if defined(_WIN32)
            return SDL_CreateDirectory(path.c_str());
#else
            // The specification asks for 0700 on directories the application creates.
            for (size_t index = 1; index <= path.size(); ++index) {
                if (index != path.size() && path[index] != '/') {
                    continue;
                }
                const std::string component = path.substr(0, index);
                if (mkdir(component.c_str(), 0700) != 0 && errno != EEXIST) {
                    return false;
                }
            }
            struct stat info;
            return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
        }

        // ORION2RE_XDG=0 forces portable mode; any other non-empty value, or the
        // marker file, requests XDG mode. Otherwise XDG is used only when the
        // installation cannot hold the files the game writes.
        bool Xdg_Mode_Selected_(const char* executable_directory) {
            const char* value = SDL_getenv("ORION2RE_XDG");
            if (value != nullptr && value[0] != '\0') {
                return strcmp(value, "0") != 0;
            }
            const std::string marker = std::string(executable_directory) + XDG_MARKER_NAME;
            SDL_PathInfo info;
            if (SDL_GetPathInfo(marker.c_str(), &info)) {
                return true;
            }
#if !defined(_WIN32)
            return access(executable_directory, W_OK) != 0;
#else
            return false;
#endif
        }

        bool Has_Directory_Component_(const char* path) {
            return strchr(path, '/') != nullptr || strchr(path, '\\') != nullptr || strchr(path, ':') != nullptr;
        }

        bool Ends_With_Case_(const char* text, const char* suffix) {
            const size_t text_length = strlen(text);
            const size_t suffix_length = strlen(suffix);
            return text_length >= suffix_length && strcasecmp(text + text_length - suffix_length, suffix) == 0;
        }

        // Matches SAVE1.GAM .. SAVE10.GAM and the DOS search patterns built for them.
        bool Is_Save_Game_Name_(const char* name) {
            return strncasecmp(name, "SAVE", 4) == 0 && Ends_With_Case_(name, ".GAM");
        }

        const s_legacy_file* Find_Legacy_File_(const char* name) {
            static const s_legacy_file save_game = {"SAVE?.GAM", USER_DIRECTORY_DATA, LEGACY_FILE_USER};
            if (Is_Save_Game_Name_(name)) {
                return &save_game;
            }
            for (const s_legacy_file& file : legacy_files) {
                if (strcasecmp(name, file.name) == 0) {
                    return &file;
                }
            }
            return nullptr;
        }

        bool Find_Existing_File_(const std::string& directory, const char* name, std::string* path) {
            char resolved_path[1024];
            const std::string candidate = directory + name;
            SDL_PathInfo info;
            if (!Resolve_File_Path_Case(candidate.c_str(), resolved_path, sizeof(resolved_path)) ||
                !SDL_GetPathInfo(resolved_path, &info) || info.type != SDL_PATHTYPE_FILE) {
                return false;
            }
            *path = resolved_path;
            return true;
        }

        bool Find_Install_File_(const char* name, std::string* path) {
            if (Find_Existing_File_(state.executable_directory, name, path)) {
                return true;
            }
            for (const std::string& directory : state.game_asset_directories) {
                if (Find_Existing_File_(directory, name, path)) {
                    return true;
                }
            }
            return false;
        }

        void Reset_State_() {
            state.enabled = false;
            state.executable_directory.clear();
            for (std::string& directory : state.directories) {
                directory.clear();
            }
            state.game_asset_directories.clear();
        }
    } // namespace

    bool Init(const char* executable_directory, bool platform_uses_xdg) {
        Reset_State_();

        if (!platform_uses_xdg || executable_directory == nullptr) {
            return false;
        }
        if (!Xdg_Mode_Selected_(executable_directory)) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "user paths: portable mode; using the working directory");
            return false;
        }

        std::string home;
        if (!Find_Home_Directory_(&home)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "user paths: no home directory; using the working directory");
            return false;
        }

        const std::string data_home = Base_Directory_("XDG_DATA_HOME", home, ".local/share/");
        state.directories[USER_DIRECTORY_CONFIG] = Base_Directory_("XDG_CONFIG_HOME", home, ".config/") + APPLICATION_DIRECTORY_NAME;
        state.directories[USER_DIRECTORY_DATA] = data_home + APPLICATION_DIRECTORY_NAME;
        state.directories[USER_DIRECTORY_STATE] = Base_Directory_("XDG_STATE_HOME", home, ".local/state/") + APPLICATION_DIRECTORY_NAME;
        state.directories[USER_DIRECTORY_CACHE] = Base_Directory_("XDG_CACHE_HOME", home, ".cache/") + APPLICATION_DIRECTORY_NAME;

        state.game_asset_directories.push_back(data_home + GAME_ASSET_DIRECTORY_NAME);
        const char* data_dirs = SDL_getenv("XDG_DATA_DIRS");
        if (data_dirs == nullptr || data_dirs[0] == '\0') {
            data_dirs = DEFAULT_DATA_DIRS;
        }
        const char* cursor = data_dirs;
        while (*cursor != '\0') {
            const char* end = strchr(cursor, ':');
            const std::string entry = end != nullptr ? std::string(cursor, end) : std::string(cursor);
            if (Is_Absolute_(entry.c_str())) {
                state.game_asset_directories.push_back(With_Trailing_Separator_(entry) + GAME_ASSET_DIRECTORY_NAME);
            }
            if (end == nullptr) {
                break;
            }
            cursor = end + 1;
        }

        for (const std::string& directory : state.directories) {
            if (!Create_Directories_(directory)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "user paths: unable to create '%s': %s", directory.c_str(), strerror(errno));
            }
        }

        state.executable_directory = executable_directory;
        state.enabled = true;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "user paths: config '%s', data '%s', state '%s', cache '%s'",
            state.directories[USER_DIRECTORY_CONFIG].c_str(),
            state.directories[USER_DIRECTORY_DATA].c_str(),
            state.directories[USER_DIRECTORY_STATE].c_str(),
            state.directories[USER_DIRECTORY_CACHE].c_str());
        return true;
    }

    void Shutdown() {
        Reset_State_();
    }

    bool Is_Enabled() {
        return state.enabled;
    }

    const char* Get_Directory(e_user_directory directory) {
        if (!state.enabled || directory < 0 || directory >= USER_DIRECTORY_COUNT) {
            return "";
        }
        return state.directories[directory].c_str();
    }

    const std::vector<std::string>& Get_Game_Asset_Directories() {
        return state.game_asset_directories;
    }

    bool Redirect_Legacy_Path(const char* path, bool writing, char* output_path, size_t output_path_size) {
        if (!state.enabled || path == nullptr || path[0] == '\0' || output_path == nullptr ||
            output_path_size == 0 || Has_Directory_Component_(path)) {
            return false;
        }

        const s_legacy_file* file = Find_Legacy_File_(path);
        if (file == nullptr) {
            return false;
        }

        std::string redirected;
        if (file->kind == LEGACY_FILE_INSTALL) {
            if (writing || !Find_Install_File_(path, &redirected)) {
                return false;
            }
        } else {
            redirected = state.directories[file->directory] + path;
            std::string existing;
            if (file->kind == LEGACY_FILE_USER_WITH_INSTALL_FALLBACK && !writing &&
                !Find_Existing_File_(state.directories[file->directory], path, &existing) &&
                Find_Install_File_(path, &existing)) {
                redirected = existing;
            }
        }

        return strlcpy(output_path, redirected.c_str(), output_path_size) < output_path_size;
    }
} // namespace user_paths
