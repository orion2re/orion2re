#include "pch.h"

#include <charconv>

#include "startup_options.h"

namespace startup {
    bool Parse_Startup_Options(int argc, const char* const* argv, s_startup_options* options, std::string* error, char prefix) {
        if (options == nullptr || error == nullptr) {
            return false;
        }
        *options = {};
        const command_line::s_option_definition definitions[] = {
            {"config", true},
            {"skip-intro", false},
            {"load-save", true},
            {"game-dir", true},
            {"log", false},
#if defined(ORION2RE_DEBUG_BUILD)
            {"oracle-record", true},
            {"oracle-replay", true},
            {"oracle-turns", true},
#endif
        };
        std::vector<command_line::s_option> parsed;
        if (!command_line::Parse_Options(argc, argv, prefix, definitions, std::size(definitions), &parsed, error)) {
            return false;
        }
        for (const command_line::s_option& option : parsed) {
            switch (option.definition_index) {
                case 0:
                    options->config_path = option.value;
                    options->config_selected = true;
                    break;
                case 1:
                    options->skip_intro = true;
                    break;
                case 2: {
                    const char* begin = option.value.data();
                    const char* end = begin + option.value.size();
                    int32_t slot = 0;
                    const auto result = std::from_chars(begin, end, slot);
                    if (result.ec != std::errc() || result.ptr != end || slot < 1 || slot > 10) {
                        *error = std::string(1, prefix) + "load-save expects a save number from 1 to 10";
                        return false;
                    }
                    options->load_save = slot;
                    break;
                }
                case 3: options->game_directory = option.value; break;
                case 4: options->log_requested = true; break;
                case 5: options->oracle_record = option.value; break;
                case 6: options->oracle_replay = option.value; break;
                case 7: {
                    const char* begin = option.value.data();
                    const char* end = begin + option.value.size();
                    const auto result = std::from_chars(begin, end, options->oracle_turns);
                    if (result.ec != std::errc() || result.ptr != end || options->oracle_turns < 1 || options->oracle_turns > 100000) {
                        *error = "oracle-turns expects a whole number from 1 to 100000";
                        return false;
                    }
                    break;
                }
            }
        }
        if ((!options->oracle_record.empty() && !options->oracle_replay.empty()) ||
            (options->load_save != 0 && (!options->oracle_record.empty() || !options->oracle_replay.empty()))) {
            *error = "oracle-record, oracle-replay and load-save are mutually exclusive";
            return false;
        }
        for (const command_line::s_option& option : parsed) {
            if (option.definition_index == 7 && options->oracle_record.empty()) {
                *error = "oracle-turns requires oracle-record; replay uses the recorded turn count";
                return false;
            }
        }
        return true;
    }
} // namespace startup
