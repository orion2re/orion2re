#include "pch.h"

namespace lostship {
    void __cdecl Compute_Stars_In_Range_Of_Player(int8_t* basic_range_array, int8_t* extended_range_array, int16_t player_idx) {
        int16_t range = MOX::player[player_idx].ship_range;
        // Extended range is 1.5x basic range, rounded up.
        int16_t extended_range = (int16_t)((range * 3 + 1) / 2);

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            s_star_data* star = &MOX::star[star_idx];

            basic_range_array[star_idx] = SHIPMOVE::Star_In_Range_Of_Player(star, player_idx, range);
            extended_range_array[star_idx] = SHIPMOVE::Star_In_Range_Of_Player(star, player_idx, extended_range);
        }
    }

    void __cdecl Lose_Out_Of_Range_Ships() {
        int8_t basic_range_array[MAX_STARS];
        int8_t extended_range_array[MAX_STARS];

        if (0 < MOX::NUM_PLAYERS) {
            for (int16_t player_idx = 0; player_idx < (int16_t)MOX::NUM_PLAYERS; ++player_idx) {
                lostship::Compute_Stars_In_Range_Of_Player(basic_range_array, extended_range_array, player_idx);
                lostship::Player_Lose_Out_Of_Range_Ships(basic_range_array, extended_range_array, player_idx);
                COMBFIND::Process_Retreating_Ships(&game::world);
            }
        }
    }

    void __cdecl Player_Lose_Out_Of_Range_Ships(int8_t* basic_range_array, int8_t* extended_range_array, int16_t player_idx) {
        int32_t ship_count = (int32_t)MOX::NUM_SHIPS;
        s_ship_data* ship_array = MOX::ship;

        for (int16_t i = 0; i < ship_count; ++i) {
            s_ship_data* ship = &ship_array[i];

            if (ship->status == 0 && (int16_t)ship->owner == player_idx) {
                bool is_out_of_range = false;

                if (extended_range_array[ship->location] == 0) {
                    is_out_of_range = true;
                } else if (basic_range_array[ship->location] == 0) {
                    if (!struct_::Test_Bit_Field(ship->d.special_device_flags, (uint16_t)SPECIAL_EXTENDED_FUEL_TANKS)) {
                        is_out_of_range = true;
                    }
                }

                if (is_out_of_range) {
                    ship->status = 9;
                }
            }
        }
    }
} // namespace lostship