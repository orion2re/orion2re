#include "pch.h"

#include <SDL3/SDL_main.h>
#if defined(ORION2RE_ASAN_BUILD)
#include <sanitizer/common_interface_defs.h>
#endif

#include "config/config_loader.h"
#include "config/text_encoding.h"
#include "lua/lua_mod_runtime.h"
#include "rpc/rpc_settings.h"
#if defined(ORION2RE_ENABLE_RPC)
#include "rpc/rpc_service.h"
#include "rpc/rpc_tactical.h"
#endif
#include "startup_directory.h"
#include "startup_options.h"

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

static void Configure_Game_Log_() {
#if defined(ORION2RE_DEBUG_BUILD)
    const char* base_path = SDL_GetBasePath();
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
    const char* base_path = SDL_GetBasePath();
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
    Configure_Game_Log_();

#if defined(ORION2RE_ASAN_BUILD)
    Configure_Sanitizer_Runtime_();
#endif

    startup::s_startup_options startup_options;
    std::string startup_error;
    if (!startup::Parse_Startup_Options(argc, argv, &startup_options, &startup_error)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "main: %s", startup_error.c_str());
        Show_Message_Box(nullptr, startup_error.c_str(), "Orion2 Redone", true);
        Shutdown_Game_Log_();
        return EXIT_FAILURE;
    }
    MOX2::skip_intro = startup_options.skip_intro ? 1 : 0;
    MAINMENU::autoload_save_slot = static_cast<int16_t>(startup_options.load_save - 1);

    std::string game_directory;
    if (!startup::Set_Game_Directory(&game_directory, &startup_error)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "main: %s", startup_error.c_str());
        Shutdown_Game_Log_();
        return -1;
    }

    if (vfs::Init(game_directory.c_str()) == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "main: vfs::Init failed for base path '%s'", game_directory.c_str());
        Shutdown_Game_Log_();
        return -1;
    }

    config::s_effective_config effective_config;
    config::s_cfg_diagnostic config_diagnostic;
    bool config_loaded = false;
    const char* startup_config_path = startup_options.config_path.c_str();
    if (!config::Load_Startup_Config(startup_config_path, "base_game", &effective_config, &config_loaded, &config_diagnostic, startup_options.config_selected)) {
        char config_error[2048];
        snprintf(config_error, sizeof(config_error), "Failed to load configuration:\n%s:%u:%u: %s", config_diagnostic.location.source.empty() ? startup_config_path : config_diagnostic.location.source.c_str(), config_diagnostic.location.line, config_diagnostic.location.column, config_diagnostic.message.c_str());
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", config_error);
        Show_Message_Box(nullptr, config_error, "Orion2 Redone", true);
        orion2re::lua::Shutdown_Lua();
        config::Shutdown_Config_Loader();
        config::Shutdown_Runtime();
        vfs::Shutdown();
        Shutdown_Game_Log_();
        return -1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
        config_loaded ? "configuration: loaded %s" : "configuration: %s not found; using defaults", startup_config_path);

#if defined(ORION2RE_ENABLE_RPC)
    rpc::s_service rpc_service;
    rpc::s_tactical_forwarder tactical_forwarder;
    tactical_forwarder.reader = {tactical_view::Local_Observer, rpc::Read_Tactical_View, rpc::Read_Tactical_Unit};
    tactical_forwarder.options = tactical_control::Read_Options;
    tactical_forwarder.authorize = tactical_control::Authorize;
    tactical_forwarder.missile = rpc::Read_Tactical_Missile;
    tactical_forwarder.missiles = rpc::Read_Tactical_Missiles;
    tactical_forwarder.authorize_missile = tactical_view::Missile_Disclosed;
    if (rpc::enabled) {
        rpc::s_service_config rpc_config;
        rpc_config.port = rpc::port;
        config::Decode_Cp850_To_Utf8(rpc::shared_key, strlen(rpc::shared_key), &rpc_config.key);
        std::string diagnostic;
        if (!rpc::Start_Service(&rpc_service, &rpc_config, &diagnostic)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "rpc: %s", diagnostic.c_str());
        } else if (!rpc::Subscribe_Tactical_Events(&tactical_forwarder, &rpc_service)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "rpc: unable to register tactical event observer");
            rpc::Stop_Service(&rpc_service);
        } else {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "rpc: listening on loopback port %u", (unsigned)rpc::Service_Port(&rpc_service));
        }
    }
#else
    if (rpc::enabled) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "rpc: requested but not compiled into this build");
#endif
    const int result = platform::Run_Main_Loop(nullptr);
#if defined(ORION2RE_ENABLE_RPC)
    rpc::Unsubscribe_Tactical_Events(&tactical_forwarder);
    rpc::Stop_Service(&rpc_service);
#endif

    orion2re::lua::Shutdown_Lua();
    ai_state::Shutdown();
    config::Shutdown_Config_Loader();
    config::Shutdown_Runtime();
    vfs::Shutdown();
    Shutdown_Game_Log_();

    return result;
}