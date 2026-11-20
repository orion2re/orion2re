#pragma once

#include <string>

#include "game/serialization.h"

namespace SAVEGAME {
    inline constexpr uint32_t LEGACY_HEADER = 0xE0;
    inline constexpr uint32_t VERSIONED_HEADER = 0xE1;
    inline constexpr uint16_t CURRENT_VERSION = 8;
    inline constexpr uint32_t BUFFER_CAPACITY = 4 * 1024 * 1024;
    extern bool loaded_config_prompt;

    struct s_metadata {
        uint32_t header;
        uint16_t version;
        char description[37];
        int32_t stardate;
        int8_t game_type;
    };

    bool Write_Game(SERIAL::Writer_* writer, const char* description, uint32_t random_seed, std::string* error_text = nullptr);
    bool Read_Game(SERIAL::Reader_* reader, s_metadata* metadata, s_settings* saved_settings, uint32_t* random_seed, std::string* error_text = nullptr);
    std::string Consume_Loaded_Config_Message();
    int16_t Convert_Legacy_Ship_Location(int16_t location);
} // namespace SAVEGAME