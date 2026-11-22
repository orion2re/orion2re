#pragma once

#include <cstdint>
#include <string>

#include "command_line.h"

namespace startup {
    struct s_startup_options {
        std::string config_path = "orion2re.cfg";
        bool config_selected = false;
        bool skip_intro = false;
        bool log_requested = false;
        int32_t load_save = 0; // SAVE<n>.GAM, 1-10; zero means no request.
        std::string game_directory;
        std::string oracle_record;
        std::string oracle_replay;
        int32_t oracle_turns = 100;
    };

    bool Parse_Startup_Options(int argc, const char* const* argv, s_startup_options* options, std::string* error, char prefix = command_line::OPTION_PREFIX);
} // namespace startup
