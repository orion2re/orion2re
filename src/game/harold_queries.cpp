#include "pch.h"

namespace HAROLD {
    uint8_t __cdecl Player_Has_Visited(int16_t player_idx, int16_t star_idx) {
        uint8_t mask = (uint8_t)(1u << ((uint8_t)player_idx & 0x1F));

        return MOX::star[star_idx].visited & mask;
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
        for (int16_t i = 0; i < MOX::NUM_STARS + 1; ++i) {
            if (MOX::star[i].x == x && MOX::star[i].y == y) {
                return i;
            }
        }

        return -1;
    }

    int16_t __cdecl N_Turns_To_Star(int16_t ship_id, int16_t star_id) {
        int16_t ship_ids[2];
        s_ship_move_info move_info;
        int16_t result = -1;

        // Initialize ship IDs list with a terminator
        ship_ids[0] = ship_id;
        ship_ids[1] = -1;

        SHIPMOVE::Ships_Try_To_Move_To(ship_ids, star_id, &move_info);

        // If the ships are able to move, return the number of turns required
        if (move_info.moving != 0) {
            result = (int16_t)move_info.turns_left;
        }

        return result;
    }

    int16_t __cdecl Absolute_Location(int16_t location) {
        if (location >= SHIP_LOCATION_MOVING_OFFSET && location < SHIP_LOCATION_WORMHOLE_OFFSET) {
            return location - SHIP_LOCATION_MOVING_OFFSET;
        }

        if (location >= SHIP_LOCATION_WORMHOLE_OFFSET && location < SHIP_LOCATION_LIMIT) {
            return location - SHIP_LOCATION_WORMHOLE_OFFSET;
        }

        return location;
    }
} // namespace HAROLD
