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
    inline bool Player_Reference(const char* label, game::id_player* player, bool allow_none = true) {
        auto slot = static_cast<int8_t>(legacy_world::Encode_Player_Slot(*player));
        if (!Player_Reference(label, &slot, allow_none)) return false;
        *player = legacy_world::Decode_Player_Slot(slot);
        return true;
    }
    inline bool Player_Reference(const char* label, unaligned::field<game::id_player>* field, bool allow_none = true) {
        auto player = field->get();
        if (!Player_Reference(label, &player, allow_none)) return false;
        *field = player;
        return true;
    }
    inline bool Player_Reference(const char* label, game::owner_slot* faction,
        bool allow_none = true, bool allow_contested = false) {
        auto slot = static_cast<int8_t>(faction->code);
        if (!Player_Reference(label, &slot, allow_none, allow_contested)) return false;
        *faction = game::owner_slot{slot};
        return true;
    }
    bool Star_Reference(const game::s_game_state& state, const char* label, game::id_star* star_id, bool allow_none = true);
    bool Planet_Reference(const char* label, game::id_planet* planet_id, bool allow_none = true);
    bool Colony_Reference(const char* label, game::id_colony* colony_index, bool allow_none = true);
    bool Officer_Reference(const char* label, int8_t* officer_index, bool allow_none = true);
    bool Player_Bit(const char* label, game::player_bitmap* bits, int16_t player_index);
    bool Player_Bitmap(const char* id, game::player_bitmap* bits, uint16_t column_count = 2);
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
