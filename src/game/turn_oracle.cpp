#include "pch.h"

#include "game/turn_oracle.h"

#if defined(ORION2RE_DEBUG_BUILD)
#include <chrono>
#include <filesystem>
#include <fstream>

#include "game/config_refresh.h"
#include "game/savegame.h"
#include "game/turn_oracle_history.h"
#include "game/user_paths.h"

namespace turn_oracle {
    namespace {
        std::filesystem::path history_directory;
        std::filesystem::path temporary_directory;
        bool recording;
        int32_t turns;
        int exit_status = EXIT_FAILURE;

        bool Capture_(const std::filesystem::path& path, std::string* error) {
            std::vector<uint8_t> bytes(SAVEGAME::BUFFER_CAPACITY);
            const char description[sizeof(SAVEGAME::s_metadata::description)] = "Turn oracle";
            SERIAL::Writer_ writer(bytes.data(), static_cast<uint32_t>(bytes.size()), "oracle save overflow");
            if (!SAVEGAME::Write_Game(&writer, description, game_random::Get_Random_Seed(), error)) return false;
            bytes.resize(writer.Length());
            return Write_File(path, bytes, error);
        }

        bool Load_Initial_(std::string* error) {
            std::vector<uint8_t> bytes;
            if (!Read_File(Turn_Path(history_directory, 0), &bytes, error)) return false;
            MOX2::Init_Map_Defaults();
            KEN::Ken_Init();
            HAROLD::Harold_Init();
            ERIC::Eric_Init();
            BILL::Bill_Init();
            SERIAL::Reader_ reader(bytes.data(), static_cast<uint32_t>(bytes.size()), "invalid oracle save", true);
            SAVEGAME::s_metadata metadata = {};
            s_settings settings = {};
            uint32_t seed = 0;
            if (!SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed, error) || reader.Remaining() != 0) {
                if (error->empty()) *error = "invalid initial oracle save";
                return false;
            }
            if (metadata.game_type != 0 || (metadata.version != 7 && metadata.version != SAVEGAME::CURRENT_VERSION)) {
                *error = "oracle requires a v7 or current-version single-player save";
                return false;
            }
            MOX::settings = settings;
            MOX::stardate = metadata.stardate;
            MOX::game_type = metadata.game_type;
            RUSS::all_ai_game = 1;
            BILL::Cache_Player_Scientist_Anim();
            if (MOX::NUM_NEBULAS != 0) MAPGEN::Load_Nebula_Pictures();
            HAROLD::Load_Leader_Titles();
            CONFIG_REFRESH::Reload_Resources_Between_Screens();
            game_random::Set_Random_Seed(seed);
            return true;
        }

        bool Run_(std::string* error) {
            MOX::game_type = 0;
            RUSS::all_ai_game = 1;
            if (recording) {
                // Every player is generated as AI, without race-selection UI.
                memset(MOX::player, 0, MAX_PLAYERS * sizeof(*MOX::player));
                MOX::settings.strategic_combat_flag = 1;
                MOX::settings.auto_saves = 0;
                MOX::cmd_line_seed = 1;
                if (!INITGAME::Init_New_Game(MOX::settings.game_difficulty, MOX::settings.galaxy_size, MOX::settings.galaxy_age, MOX::settings.number_of_players, 1, MOX::settings.starting_civilization_level)) {
                    *error = "oracle new-game initialization failed";
                    return false;
                }
                if (!Capture_(Turn_Path(history_directory, 0), error)) return false;
            }
            // Recording and playback must start at the same load boundary.
            if (!Load_Initial_(error)) return false;
            for (int32_t turn = 1; turn <= turns; ++turn) {
                MOX::return_screen = SCREEN_MAIN;
                NEXTTURN::Next_Turn();
                const auto canonical = Turn_Path(history_directory, turn);
                const auto actual = Turn_Path(temporary_directory, turn);
                if (!Capture_(recording ? canonical : actual, error)) return false;
                if (!recording && !Compare_Files(canonical, actual, error)) {
                    *error = "oracle mismatch at turn " + std::to_string(turn) + ", stardate " + std::to_string(MOX::stardate) + "\n" + *error;
                    return false;
                }
                if (!recording) {
                    std::error_code ignored;
                    std::filesystem::remove(actual, ignored);
                }
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "oracle: %s turn %d/%d", recording ? "recorded" : "matched", turn, turns);
            }
            if (recording) {
                std::ofstream manifest(history_directory / "history.txt");
                manifest << "orion2re-turn-oracle 1\n"
                         << turns << '\n';
                manifest.close();
                if (!manifest) {
                    *error = "cannot write oracle history manifest";
                    return false;
                }
            }
            return true;
        }
    } // namespace

    bool Prepare(const startup::s_startup_options* options, std::string* error) {
        if (options->oracle_record.empty() && options->oracle_replay.empty()) return true;
        recording = !options->oracle_record.empty();
        std::error_code code;
        const std::string& directory = recording ? options->oracle_record : options->oracle_replay;
        history_directory = std::filesystem::absolute(std::filesystem::path(std::u8string(directory.begin(), directory.end())), code).lexically_normal();
        if (history_directory.filename().empty()) history_directory = history_directory.parent_path();
        if (!code && recording) {
            std::filesystem::create_directories(history_directory.parent_path(), code);
            if (!code && !std::filesystem::create_directory(history_directory, code)) {
                *error = "oracle record directory must not already exist: " + history_directory.string();
                return false;
            }
        }
        if (!code && !recording) {
            std::ifstream manifest(history_directory / "history.txt");
            std::string header;
            std::getline(manifest, header);
            if (header != "orion2re-turn-oracle 1" || !(manifest >> turns) || turns < 1 || turns > 100000) {
                *error = "invalid or incomplete oracle history: " + history_directory.string();
                return false;
            }
            manifest >> std::ws;
            if (!manifest.eof()) {
                *error = "unexpected trailing data in oracle history manifest";
                return false;
            }
        } else {
            turns = options->oracle_turns;
        }
        if (code) {
            *error = "cannot prepare oracle history: " + code.message();
            return false;
        }
        const auto temp_root = std::filesystem::temp_directory_path(code);
        if (!code) {
            for (int attempt = 0; attempt < 100; ++attempt) {
                const auto candidate = temp_root / ("orion2re-oracle-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(attempt));
                if (std::filesystem::create_directory(candidate, code)) {
                    temporary_directory = candidate;
                    break;
                }
                if (code) break;
            }
        }
        if (code || temporary_directory.empty()) {
            *error = "cannot prepare oracle directories: " + code.message();
            return false;
        }
        user_paths::Set_Session_Directory(temporary_directory.string());
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "oracle history: %s; temporary files: %s", history_directory.string().c_str(), temporary_directory.string().c_str());
        return true;
    }

    bool Is_Enabled() {
        return !history_directory.empty();
    }

    void Run() {
        std::string error;
        printf("oracle: %s %s\n", recording ? "recording" : "replaying", history_directory.string().c_str());
        fflush(stdout);
        if (Run_(&error)) {
            exit_status = EXIT_SUCCESS;
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "oracle: %s %d turns successfully", recording ? "recorded" : "verified", turns);
            printf("oracle: %s %d turns successfully\n", recording ? "recorded" : "verified", turns);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.c_str());
            // Sanitizer builds redirect stderr to a log; keep CLI diagnostics visible.
            printf("%s\ntemporary files retained: %s\n", error.c_str(), temporary_directory.string().c_str());
        }
    }

    int Exit_Status() {
        return exit_status;
    }

    void Shutdown() {
        user_paths::Set_Session_Directory("");
        // Keep failed output available for inspection, but never modify history.
        if (exit_status == EXIT_SUCCESS && !temporary_directory.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(temporary_directory, ignored);
        }
    }
} // namespace turn_oracle
#endif
