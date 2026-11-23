#include "pch.h"
#include "fleet_movement.h"

namespace HAROLD {
    uint8_t __cdecl Player_Has_Visited(int16_t player_idx, int16_t star_idx) {
        uint8_t mask = (uint8_t)(1u << ((uint8_t)player_idx & 0x1F));

        return game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).visited & mask;
    }

    const char* __cdecl H_Message(int16_t id, int unused_save_slot, const s_save_game_description* unused_save_description) {
        size_t buffer_offset = 0;

        (void)unused_save_slot;
        (void)unused_save_description;

        if (id >= 0 && id < 397) {
            buffer_offset = MOX::h_string_ix[id];
        }

        return &MOX::h_strings[buffer_offset];
    }

    bool __cdecl Player_Is_Omniscient(int16_t player_idx) {
        if (OFFICER::One_Leader_With_Galactic_Lore(player_idx)) {
            return true;
        }

        if (MOX::player[player_idx].traits[TRAIT_OMNISCIENCE] != 0) {
            return true;
        }

        return false;
    }

    int16_t __cdecl XY_To_Star_Id(int16_t x, int16_t y) {
        const auto& state = game::current_state;
        if (const auto star = game::Find_Star_At_Coordinates(state, x, y)) return game::Star_Row(state, *star);
        if (state.antares_destination.x == x && state.antares_destination.y == y) return game::ANTARES_POSITION;
        return -1;
    }

    int16_t N_Turns_To_Star(game::id_ship ship, game::s_star_destination destination) {
        const game::id_ship keys[] = {ship};
        s_ship_move_info move_info{};
        SHIPMOVE::Evaluate_Fleet_Move(keys, destination, &move_info);
        return move_info.moving ? move_info.turns_left : -1;
    }

} // namespace HAROLD
