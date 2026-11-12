#include "pch.h"

namespace COMBAT1 {
    int16_t __cdecl Combat_Ship_Is_Functional(int16_t ship_idx) {
        int16_t is_functional = 0;

        if (MOX::combat_data[ship_idx].unit_status == 0 &&
            MOX::combat_data[ship_idx].is_captured == 0) {
            if (!CMBTMOV1::Ship_Is_In_Stasis(ship_idx)) {
                is_functional = 1;
            }
        }

        return is_functional;
    }

    bool Combat_Ship_Participates_In_Pass(int16_t ship_idx, bool time_warp_only) {
        return Combat_Ship_Is_Functional(ship_idx) != 0 &&
            (!time_warp_only ||
                COMBINIT::Does_Combat_Ship_Have_Special(
                    ship_idx, SPECIAL_TIME_WARP_FACILITATOR) != 0);
    }

    bool Combat_Ship_Participates_In_Turn_Order(
        int16_t ship_idx, bool time_warp_only) {
        const s_combat_data* ship = &MOX::combat_data[ship_idx];
        return ship->unit_status == 0 && ship->is_captured == 0 &&
            (MOX::combat_init != 0 ||
                CMBTMOV1::Ship_Is_In_Stasis(ship_idx) == 0) &&
            (!time_warp_only ||
                COMBINIT::Does_Combat_Ship_Have_Special(
                    ship_idx, SPECIAL_TIME_WARP_FACILITATOR) != 0);
    }

    int16_t __cdecl Calc_Current_Speed(int16_t ship_idx) {
        int16_t speed = 0;

        if ((MOX::combat_data[ship_idx].ship_idx < 0 &&
                MOX::combat_data[ship_idx].ftl_type == DRIVE_NO_DRIVE) ||
            CMBTMOV1::Ship_In_Black_Hole(ship_idx)) {
            speed = 0;
        } else {
            int combat_speed = (uint8_t)MOX::combat_data[ship_idx].combat_speed_base;

            if (struct_::Test_Bit_Field(MOX::combat_data[ship_idx].special_device_flags, SPECIAL_AUGMENTED_ENGINES) == 1 &&
                struct_::Test_Bit_Field(MOX::combat_data[ship_idx].special_device_damage_flags, SPECIAL_AUGMENTED_ENGINES) == 0) {
                combat_speed += DESIGN::augmented_engines_speed_bonus;
            }

            int owner = MOX::combat_data[ship_idx].owner;

            if (MOX::combat_data[ship_idx].ftl_type != DRIVE_NO_DRIVE) {
                int current_hits = MOX::combat_data[ship_idx].drive_current_hits;
                int max_hits = MOX::combat_data[ship_idx].drive_max_hits;

                if (owner <= 9) {
                    int half_max = max_hits / 2;
                    int range = max_hits - half_max;
                    speed = (int16_t)(((current_hits - half_max) * combat_speed) / range);
                } else {
                    speed = (int16_t)((current_hits * combat_speed) / max_hits);
                }
            }

            if (speed < 0) {
                speed = 0;
            }

            if (owner < MAX_PLAYERS && MOX::player[owner].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
                speed += td_combat_speed_bonus;
            }

            if (speed > 0) {
                uint32_t tractor_beams = 0;
                for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                    const int16_t tractor_target_idx = MOX::combat_data[i].special_cooldown == COMBAT_SHIP_NONE
                        ? -1
                        : MOX::combat_data[i].special_cooldown;
                    if (tractor_target_idx == ship_idx &&
                        Combat_Ship_Is_Functional(i)) {
                        for (int j = 0; j < 8; ++j) {
                            if (MOX::combat_data[i].weapons[j].weapon_id == WEAPON_TRACTOR_BEAM) {
                                tractor_beams += (uint8_t)MOX::combat_data[i].weapons[j].count;
                            }
                        }
                    }
                }

                const int8_t size_class = MOX::combat_data[ship_idx].size_class;
                int16_t tractors_to_stop = size_class + 1;
                if (size_class >= 0 && size_class < SHIP_SIZE_COUNT) {
                    tractors_to_stop = COMBAT1::tractors_to_stop[(uint8_t)size_class];
                }

                if (tractors_to_stop <= 0 ||
                    tractor_beams >= (uint32_t)tractors_to_stop) {
                    speed = 0;
                } else {
                    speed -= (int16_t)(((uint32_t)speed * tractor_beams) /
                        (uint32_t)tractors_to_stop);
                    if (speed < 1) {
                        speed = 1;
                    }
                }
            }
        }

        return speed;
    }

    int16_t __cdecl Get_Number_Of_Ships_In_Stasis(int16_t source_ship_idx) {
        int16_t count = 0;

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            const int16_t stasis_source_idx = MOX::combat_data[i].stasis_source_idx == COMBAT_SHIP_NONE
                ? -1
                : MOX::combat_data[i].stasis_source_idx;
            if (source_ship_idx >= 0 && stasis_source_idx == source_ship_idx) {
                count++;
            }
        }

        return count;
    }

    int16_t __cdecl Enemy_Dissipator_Exists(int16_t player_idx) {
        int16_t exists = 0;
        int16_t total_ships = MOX::combat_total_ship_count;

        for (int16_t i = 0; i < total_ships; ++i) {
            if (!CMBTMOV1::Ship_Is_In_Stasis(i) &&
                COMBINIT::Does_Combat_Ship_Have_Special(i, SPECIAL_WARP_DISSIPATOR) != 0) {
                s_combat_data* ship = &MOX::combat_data[i];

                if (ship->is_captured == 0 && ship->unit_status == 0 && ship->owner != player_idx) {
                    exists = 1;
                }
            }
        }

        return exists;
    }

    bool __cdecl Weapon_Is_On(int16_t ship_idx, int16_t weapon_idx) {
        return MOX::combat_data[ship_idx].weapons[weapon_idx].active == 1;
    }
} // namespace COMBAT1
