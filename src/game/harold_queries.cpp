#include "pch.h"
#include "fleet_movement.h"

namespace HAROLD {
    bool __cdecl Player_Has_Visited(game::id_player player, int16_t star_idx) {
        const auto& state = game::current_state;
        if (star_idx == game::ANTARES_POSITION) return state.antares_destination.visited.test(player);
        const auto star = state.stars.find(game::Star_Key_At_Slot(state, star_idx));
        return star != state.stars.end() && star->second.visited.test(player);
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

    bool __cdecl Player_Is_Omniscient(game::id_player player) {
        if (player.id >= MAX_PLAYERS) return false;
        if (OFFICER::One_Leader_With_Galactic_Lore(player)) {
            return true;
        }

        if (game::current_state.players[player.id].traits[TRAIT_OMNISCIENCE] != 0) {
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
        SHIPMOVE::Evaluate_Fleet_Move(game::current_state, keys, destination, &move_info);
        return move_info.moving ? move_info.turns_left : -1;
    }

} // namespace HAROLD
