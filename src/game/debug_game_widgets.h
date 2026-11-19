#pragma once

namespace debug_game_widgets {
    bool Enum(
        const char* label,
        int8_t* value,
        const char* const* names,
        int16_t count,
        int16_t first_value = 0);

    bool Player_Reference(
        const char* label,
        int8_t* player_index,
        bool allow_none = true,
        bool allow_contested = false);
    bool Star_Reference(const char* label, int16_t* star_index, bool allow_none = true);
    bool Star_Reference(const char* label, game::id_star* star_id, bool allow_none = true);
    bool Planet_Reference(game::s_world_state* world_state, const char* label, game::id_planet* planet_index, bool allow_none = true);
    bool Colony_Reference(const char* label, int16_t* colony_index, bool allow_none = true);
    bool Officer_Reference(game::s_world_state* world_state, const char* label, int8_t* officer_index, bool allow_none = true);
    bool Player_Bit(const char* label, uint8_t* bits, int16_t player_index);
    bool Player_Bitmap(const char* id, uint8_t* bits, uint16_t column_count = 2);
    bool Star_Bitmap(
        const char* id,
        void* bits,
        uint16_t byte_count,
        uint16_t column_count = 2);

    bool Ship_Specials(
        const char* id,
        void* special_bits,
        uint16_t byte_count,
        uint16_t column_count = 2);
} // namespace debug_game_widgets