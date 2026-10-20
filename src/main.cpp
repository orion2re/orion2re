#include "pch.h"

#include <SDL3/SDL_main.h>
#if defined(ORION2RE_ASAN_BUILD)
#include <sanitizer/common_interface_defs.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "config/config_loader.h"
#include "game/user_paths.h"
#include "lua/lua_mod_runtime.h"

#if defined(_WIN32)
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#if defined(ORION2RE_DEBUG_BUILD)
static void Build_Path_(char* output, size_t output_size, const char* base_path, const char* relative_path) {
    if (base_path != nullptr && base_path[0] != '\0') {
        snprintf(output, output_size, "%s%s", base_path, relative_path);
    } else {
        snprintf(output, output_size, "%s", relative_path);
    }
}

static FILE* _game_log_file;
static SDL_LogOutputFunction _console_log_output;
static void* _console_log_userdata;

static const char* Log_Priority_Name_(SDL_LogPriority priority) {
    switch (priority) {
        case SDL_LOG_PRIORITY_TRACE: return "TRACE";
        case SDL_LOG_PRIORITY_VERBOSE: return "VERBOSE";
        case SDL_LOG_PRIORITY_DEBUG: return "DEBUG";
        case SDL_LOG_PRIORITY_INFO: return "INFO";
        case SDL_LOG_PRIORITY_WARN: return "WARN";
        case SDL_LOG_PRIORITY_ERROR: return "ERROR";
        case SDL_LOG_PRIORITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

static void SDLCALL Game_Log_Output_(void* userdata, int category, SDL_LogPriority priority, const char* message) {
    if (_console_log_output != nullptr) {
        _console_log_output(_console_log_userdata, category, priority, message);
    }

    FILE* log_file = static_cast<FILE*>(userdata);
    if (log_file != nullptr) {
        fprintf(log_file, "%s: %s\n", Log_Priority_Name_(priority), message);
        fflush(log_file);
    }
}
#endif

#if defined(ORION2RE_DEBUG_BUILD) || defined(ORION2RE_ASAN_BUILD)
static const char* Logs_Parent_Path_() {
    return user_paths::Is_Enabled() ? user_paths::Get_Directory(user_paths::USER_DIRECTORY_STATE) : SDL_GetBasePath();
}
#endif

static void Configure_Game_Log_() {
#if defined(ORION2RE_DEBUG_BUILD)
    const char* base_path = Logs_Parent_Path_();
    char logs_path[1024];
    char log_file_path[1024];

    Build_Path_(logs_path, sizeof(logs_path), base_path, "logs");
    SDL_CreateDirectory(logs_path);

#if defined(_WIN32)
    const int process_id = _getpid();
    const char path_separator = '\\';
#else
    const int process_id = getpid();
    const char path_separator = '/';
#endif

    snprintf(log_file_path, sizeof(log_file_path), "%s%cgame.%d.log", logs_path, path_separator, process_id);
    _game_log_file = fopen(log_file_path, "w");
    if (_game_log_file != nullptr) {
        SDL_GetLogOutputFunction(&_console_log_output, &_console_log_userdata);
        SDL_SetLogOutputFunction(Game_Log_Output_, _game_log_file);
    }
#endif
}

static void Shutdown_Game_Log_() {
#if defined(ORION2RE_DEBUG_BUILD)
    if (_game_log_file == nullptr) {
        return;
    }

    SDL_SetLogOutputFunction(_console_log_output, _console_log_userdata);
    fclose(_game_log_file);
    _game_log_file = nullptr;
#endif
}

#if defined(ORION2RE_ASAN_BUILD)
extern "C" const char* __asan_default_options() {
    return "disable_coredump=1";
}

#if defined(__linux__)
extern "C" const char* __lsan_default_suppressions() {
    // Work around SDL error-buffer TLS leaks on PulseAudio-owned discovery threads.
    return "leak:AddPulseAudioDevice\n";
}
#endif

static void Configure_Sanitizer_Runtime_() {
    const char* base_path = Logs_Parent_Path_();
    if (base_path == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ASAN: unable to resolve game directory: %s", SDL_GetError());
        return;
    }

    char* logs_path = nullptr;
    if (SDL_asprintf(&logs_path, "%slogs", base_path) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ASAN: unable to allocate logs path");
        return;
    }
    if (!SDL_CreateDirectory(logs_path)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ASAN: unable to create '%s': %s", logs_path, SDL_GetError());
        SDL_free(logs_path);
        return;
    }

    char* report_path = nullptr;
    if (SDL_asprintf(&report_path, "%s/asan", logs_path) >= 0) {
        // Set the runtime path directly; environment options were read before main.
        __sanitizer_set_report_path(report_path);
        SDL_free(report_path);
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ASAN: unable to allocate report path");
    }

#if defined(_WIN32)
    const int process_id = _getpid();
#else
    const int process_id = getpid();
#endif
    char* stderr_path = nullptr;
    if (SDL_asprintf(&stderr_path, "%s/sanitizer-stderr.%d.log", logs_path, process_id) >= 0) {
        // GCC's separate UBSan runtime does not use ASan's report path. Redirect
        // the descriptor too, since sanitizer diagnostics bypass C stdio.
        FILE* stderr_log = fopen(stderr_path, "a");
        if (stderr_log != nullptr) {
            fflush(stderr);
#if defined(_WIN32)
            const int result = _dup2(_fileno(stderr_log), _fileno(stderr));
#else
            const int result = dup2(fileno(stderr_log), STDERR_FILENO);
#endif
            if (result < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Sanitizers: unable to redirect stderr to '%s': %s", stderr_path, strerror(errno));
            }
            fclose(stderr_log);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Sanitizers: unable to open '%s': %s", stderr_path, strerror(errno));
        }
        SDL_free(stderr_path);
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Sanitizers: unable to allocate stderr path");
    }
    SDL_free(logs_path);
}
#endif

int main(int argc, char** argv) {
    user_paths::Init(SDL_GetBasePath(), user_paths::PLATFORM_USES_XDG);
    Configure_Game_Log_();

#if defined(ORION2RE_ASAN_BUILD)
    Configure_Sanitizer_Runtime_();
#endif

    const char* base_path = SDL_GetBasePath();
#if defined(__APPLE__)
    char executable_path[PATH_MAX];
    uint32_t executable_path_size = sizeof(executable_path);
    char resolved_path[PATH_MAX];
    if (_NSGetExecutablePath(executable_path, &executable_path_size) != 0 || realpath(executable_path, resolved_path) == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "main: unable to resolve executable path: %s", strerror(errno));
        Shutdown_Game_Log_();
        return -1;
    }
    char* filename = strrchr(resolved_path, '/');
    if (filename == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "main: executable path has no directory: '%s'", resolved_path);
        Shutdown_Game_Log_();
        return -1;
    }
    filename[1] = '\0';
    if (chdir(resolved_path) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "main: unable to set working directory to '%s': %s", resolved_path, strerror(errno));
        Shutdown_Game_Log_();
        return -1;
    }
#endif
    // In XDG mode the installation is found next to the executable,
    // not wherever a desktop launcher happened to start the process.
#if !defined(_WIN32) && !defined(__APPLE__)
    if (user_paths::Is_Enabled() && base_path != nullptr && chdir(base_path) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "main: unable to set working directory to '%s': %s", base_path, strerror(errno));
        Shutdown_Game_Log_();
        return -1;
    }
#endif

    char* current_directory = SDL_GetCurrentDirectory();
    const char* vfs_base_path = current_directory != nullptr ? current_directory : base_path;

    if (vfs::Init(vfs_base_path) == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "main: vfs::Init failed for base path '%s'", vfs_base_path != nullptr ? vfs_base_path : "");
        if (current_directory != nullptr) {
            SDL_free(current_directory);
        }
        Shutdown_Game_Log_();
        return -1;
    }

    if (user_paths::Is_Enabled()) {
        const std::string user_mods_path = std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_DATA)) + "mods/";
        vfs::Set_User_Config_Path(user_paths::Get_Directory(user_paths::USER_DIRECTORY_CONFIG));
        vfs::Set_User_Mods_Path(user_mods_path.c_str());
        for (const std::string& directory : user_paths::Get_Game_Asset_Directories()) {
            vfs::Add_Raw_Search_Path(directory.c_str());
        }
    }

    config::s_effective_config effective_config;
    config::s_cfg_diagnostic config_diagnostic;
    bool config_loaded = false;
    if (!config::Load_Startup_Config("orion2re.cfg", "base_game", &effective_config, &config_loaded, &config_diagnostic)) {
        char config_error[2048];
        snprintf(config_error, sizeof(config_error), "Failed to load configuration:\n%s:%u:%u: %s", config_diagnostic.location.source.empty() ? "orion2re.cfg" : config_diagnostic.location.source.c_str(), config_diagnostic.location.line, config_diagnostic.location.column, config_diagnostic.message.c_str());
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", config_error);
        Show_Message_Box(nullptr, config_error, "Orion2 Redone", true);
        orion2re::lua::Shutdown_Lua();
        config::Shutdown_Config_Loader();
        config::Shutdown_Runtime();
        vfs::Shutdown();
        if (current_directory != nullptr) {
            SDL_free(current_directory);
        }
        Shutdown_Game_Log_();
        return -1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
        config_loaded ? "configuration: loaded orion2re.cfg" : "configuration: orion2re.cfg not found; using defaults");

    size_t command_line_length = 0;
    for (int i = 1; i < argc; ++i) {
        command_line_length += strlen(argv[i]);
        if (i + 1 < argc) {
            ++command_line_length;
        }
    }

    char* command_line = nullptr;
    if (command_line_length != 0) {
        command_line = (char*)malloc(command_line_length + 1);
        if (command_line == nullptr) {
            orion2re::lua::Shutdown_Lua();
            config::Shutdown_Config_Loader();
            config::Shutdown_Runtime();
            vfs::Shutdown();
            if (current_directory != nullptr) {
                SDL_free(current_directory);
            }
            Shutdown_Game_Log_();
            return -1;
        }

        size_t write_offset = 0;
        for (int i = 1; i < argc; ++i) {
            const size_t arg_length = strlen(argv[i]);
            memcpy(command_line + write_offset, argv[i], arg_length);
            write_offset += arg_length;

            if (i + 1 < argc) {
                command_line[write_offset++] = ' ';
            }
        }
        command_line[write_offset] = '\0';
    }

    const int result = platform::Run_Main_Loop(command_line);
    if (command_line != nullptr) {
        free(command_line);
    }

    orion2re::lua::Shutdown_Lua();
    ai_state::Shutdown();
    config::Shutdown_Config_Loader();
    config::Shutdown_Runtime();
    vfs::Shutdown();
    if (current_directory != nullptr) {
        SDL_free(current_directory);
    }
    Shutdown_Game_Log_();

    return result;
}