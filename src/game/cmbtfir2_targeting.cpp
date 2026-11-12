#include "pch.h"

namespace CMBTFIR2 {
    int32_t __cdecl Total_Internal_Space(int16_t ship_idx) {
        s_combat_data* ship = &MOX::combat_data[ship_idx];

        int32_t engine_space = ship->drive_current_hits == 1 ? 1 : ship->drive_current_hits / 2;
        int32_t total = engine_space + ship->computer_current_hits;

        for (int i = 0; i < 8; ++i) {
            total += ship->weapon_hits[i];
        }

        for (int i = 0; i < 8; ++i) {
            total += ship->special_hits[i];
        }

        total += ship->shield_current_hits;

        return total;
    }

    void __cdecl Select_Internal(int16_t ship_idx, int16_t* hit_type, int16_t* target_idx, int16_t heavy_hit, int16_t direct_hit, int16_t armor_piercing, int16_t internal_only) {
        int16_t random_roll;
        int16_t internal_chance;
        int32_t total_internal_space;
        uint16_t weights[19];
        int16_t choice;
        bool found;

        random_roll = (int16_t)game_random::Random(100);
        *hit_type = -1;

        if (internal_only != 0) {
            *hit_type = 5;
            return;
        }

        if (armor_piercing == 0 && MOX::combat_data[ship_idx].armor_remaining > 0 && direct_hit == 0) {
            *hit_type = 7;
            return;
        }

        total_internal_space = CMBTFIR2::Total_Internal_Space(ship_idx);
        internal_chance = 100;

        if (direct_hit == 0) {
            internal_chance = (total_internal_space * 100) /
                ((MOX::combat_data[ship_idx].structure_max - MOX::combat_data[ship_idx].structure_damage) + total_internal_space);
        }

        if (MOX::combat_data[ship_idx].owner > 9 && internal_chance > 25) {
            internal_chance = 25;
        }

        if (internal_chance < random_roll) {
            *hit_type = 0;
        } else {
            memset(weights, 0, sizeof(weights));

            weights[0] = MOX::combat_data[ship_idx].drive_current_hits;
            weights[1] = MOX::combat_data[ship_idx].computer_current_hits;

            for (int16_t i = 0; i < 8; ++i) {
                if (heavy_hit != 0) {
                    weights[i + 2] = MOX::combat_data[ship_idx].weapon_hits[i] * 3;
                } else {
                    weights[i + 2] = MOX::combat_data[ship_idx].weapon_hits[i];
                }
            }

            for (int16_t i = 0; i < 8; ++i) {
                weights[i + 10] = MOX::combat_data[ship_idx].special_hits[i];
            }

            weights[18] = MOX::combat_data[ship_idx].shield_current_hits;
            if (heavy_hit != 0) {
                weights[18] *= 3;
            }

            choice = RUSS::Get_Weighted_Choice_Short(weights, 19);

            if (choice == 0) {
                *hit_type = 5;
                return;
            }

            if (choice == 1) {
                *hit_type = 8;
                return;
            }

            found = false;
            for (int16_t i = 0; i < 8; ++i) {
                if (i - choice == -2) {
                    found = true;
                    *hit_type = 3;
                    *target_idx = i;
                    break;
                }
            }

            if (!found) {
                for (int16_t i = 0; i < 8; ++i) {
                    if (i - choice == -10) {
                        found = true;
                        *hit_type = 9;
                        *target_idx = i;
                        break;
                    }
                }
            }

            if (choice == 18) {
                *hit_type = 2;
                return;
            }
        }
    }
} // namespace CMBTFIR2
