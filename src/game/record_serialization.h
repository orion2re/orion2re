#pragma once

#include "world.h"
#include "serialization.h"

namespace SAVEGAME::records {
    void Write_Ship(SERIAL::Writer_* writer, const s_ship_data* ship);
    void Read_Ship(SERIAL::Reader_* reader, s_ship_data* ship, uint16_t version, int16_t legacy_star_count = -1);
    void Write_Planet(SERIAL::Writer_* writer, const s_planet_data* planet);
    void Read_Planet(SERIAL::Reader_* reader, s_planet_data* planet, uint16_t version);
    inline constexpr uint16_t LEGACY_VERSION = 0;
    inline constexpr uint16_t PLANET_ID_VERSION = 8;
    inline constexpr uint16_t ANTARAN_RUNTIME_STATE_VERSION = 3;
    inline constexpr uint16_t FREIGHT_CAPACITY_VERSION = 3;
    inline constexpr uint16_t CONFIG_SNAPSHOT_VERSION = 3;
    inline constexpr uint16_t JUMP_GATE_REDIRECT_STATE_VERSION = 4;
    inline constexpr uint16_t LUA_SAVE_SNAPSHOT_VERSION = 5;
    inline constexpr uint16_t EXPANDED_SHIP_CAPACITY_VERSION = 6;
    inline constexpr uint16_t BUILDING_SCRAP_COUNT_VERSION = 7;
    game::id_planet Read_Planet_Reference(SERIAL::Reader_* reader, uint16_t version, bool original_byte = false);
    void Write_Zeroes(SERIAL::Writer_* writer, size_t size);
    void Write_Int16_Array(SERIAL::Writer_* writer, const int16_t* values, size_t count);
    void Read_Int16_Array(SERIAL::Reader_* reader, int16_t* values, size_t count);
    void Write_Uint16_Array(SERIAL::Writer_* writer, const void* values, size_t count);
    void Read_Uint16_Array(SERIAL::Reader_* reader, void* values, size_t count);
    void Write_Int32_Array(SERIAL::Writer_* writer, const void* values, size_t count);
    void Read_Int32_Array(SERIAL::Reader_* reader, void* values, size_t count);
    void Write_Ship_Weapons(SERIAL::Writer_* writer, const s_ship_weapons* weapons);
    void Read_Ship_Weapons(SERIAL::Reader_* reader, s_ship_weapons* weapons);
    void Write_Ship_Design(SERIAL::Writer_* writer, const s_ship_design* design);
    void Read_Ship_Design(SERIAL::Reader_* reader, s_ship_design* design);
    void Write_Colony(SERIAL::Writer_* writer, const s_colony* colony);
    void Read_Colony(SERIAL::Reader_* reader, s_colony* colony, uint16_t version);
    void Write_Star(SERIAL::Writer_* writer, const s_star_data* star);
    void Read_Star(SERIAL::Reader_* reader, s_star_data* star, uint16_t version);
    void Write_Player(game::s_world_state* world_state, SERIAL::Writer_* writer, const s_player* player, game::id_player player_id);
    bool Read_Player(game::s_world_state* world_state, SERIAL::Reader_* reader, s_player* player, uint16_t version, game::id_player player_id);
    void Write_Report(SERIAL::Writer_* writer, const s_report* report);
    void Read_Report(SERIAL::Reader_* reader, s_report* report, uint16_t version);
    void Write_Colony_Message(SERIAL::Writer_* writer, const s_MSG_colony* colony);
    void Read_Colony_Message(SERIAL::Reader_* reader, s_MSG_colony* colony, uint16_t version);
    void Write_Message(SERIAL::Writer_* writer, const s_MSG* message);
    void Read_Message(SERIAL::Reader_* reader, s_MSG* message, uint16_t version);
    int16_t Convert_Legacy_Ship_Location(int16_t location);
} // namespace SAVEGAME::records
