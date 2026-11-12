#include "pch.h"

namespace COMBAT1 {
    static int16_t movement_reserve[MAX_COMBAT_SHIPS] = {};

    static int16_t Maximum_Combat_Speed(int16_t ship_idx) {
        s_combat_data& ship = MOX::combat_data[ship_idx];
        int16_t speed = (uint8_t)ship.combat_speed_base;

        if (struct_::Test_Bit_Field(
                ship.special_device_flags, SPECIAL_AUGMENTED_ENGINES) != 0) {
            speed += DESIGN::augmented_engines_speed_bonus;
        }

        if (ship.owner >= 0 && ship.owner < MAX_PLAYERS &&
            MOX::player[ship.owner].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
            speed += td_combat_speed_bonus;
        }

        return speed;
    }

    void Recalculate_Combat_Ship(int16_t ship_idx, bool tractor_applied) {
        if (ship_idx < 0 || ship_idx >= MOX::combat_total_ship_count) {
            return;
        }

        s_combat_data& ship = MOX::combat_data[ship_idx];
        const int16_t old_speed = ship.current_speed;
        const int16_t new_speed = Calc_Current_Speed(ship_idx);
        ship.current_speed = new_speed;

        if (interactive_combat_stats != 0) {
            ship.movement_left = Adjust_Movement_After_Speed_Change(
                old_speed,
                new_speed,
                Maximum_Combat_Speed(ship_idx),
                ship.movement_left,
                &movement_reserve[ship_idx]);
            ship.tac_val_attack = CMBTFIR2::Offensive_Combat_Bonus(ship_idx);
            ship.tac_val_defense = CMBTFIR2::Defensive_Combat_Bonus(ship_idx, 0);
            ship.tac_val_evade = CMBTFIR2::Defensive_Combat_Bonus(ship_idx, 1);
        } else if (non_ics_tractor_and_bhg_mode != 0 &&
            ship.movement_left > new_speed &&
            (tractor_applied || ship.black_hole_source_idx != COMBAT_SHIP_NONE)) {
            ship.movement_left = new_speed;
        }
    }

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

    void Update_Ordinary_Cloak(int16_t ship_idx) {
        const int8_t recloak_state = recloak_next_turn != 0 ? 2 : 3;
        if (MOX::combat_data[ship_idx].special_status_flag != recloak_state) {
            return;
        }

        const bool has_ordinary_cloak =
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_CLOAKING_DEVICE) != 0 ||
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_PHASING_CLOAK) != 0 ||
            (CMBTFIR2::combat_bonus.stealth_field_beam_defense != 0 &&
                COMBINIT::Does_Combat_Ship_Have_Special(
                    ship_idx, SPECIAL_STEALTH_FIELD) != 0);
        if (!has_ordinary_cloak) {
            return;
        }

        CMBTSPEC::Draw_Cloak(ship_idx, 1);
        MOX::combat_data[ship_idx].special_status_flag = 1;
    }

    void __cdecl Update_Phasing_Cloak_Timers(bool time_warp_only) {
        if (MOX::combat_total_ship_count <= 0) {
            return;
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (Combat_Ship_Is_Functional(i) == 0) {
                continue;
            }
            if (time_warp_only &&
                COMBINIT::Does_Combat_Ship_Have_Special(
                    i, SPECIAL_TIME_WARP_FACILITATOR) == 0) {
                continue;
            }

            s_combat_data* ship = &MOX::combat_data[i];
            if (ship->special_status_flag != 4 ||
                COMBINIT::Does_Combat_Ship_Have_Special(
                    i, SPECIAL_PHASING_CLOAK) == 0) {
                continue;
            }

            if (ship->special_status_timer > 0) {
                ship->special_status_timer--;
            }

            if (ship->special_status_timer == 0) {
                CMBTSPEC::Draw_Cloak(i, 5);
                CMBTSPEC::Draw_Cloak(i, 1);
                ship->special_status_flag = 1;
            }
        }
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

    void __cdecl Init_Ship_For_Start_Of_Turn(int16_t ship_idx) {
        int16_t is_captured_now = CMBTFIR2::Being_Captured(ship_idx);
        s_combat_data* ship = &MOX::combat_data[ship_idx];

        ship->combat_status_flags = 0;
        if (is_captured_now != 0) {
            CMBTFIR2::Set_Capturing_Flag(ship_idx);
        }
        ship->field42_0x49 = 0;

        if (ship->is_captured == 1) {
            ship->movement_left = 0;
            ship->special_status_flag = 2;
            for (int i = 0; i < 8; ++i) {
                ship->weapons[i].shots_left = 0;
                ship->weapons[i].status = 0;
            }
            ship->ai_range_tactic =
                (int8_t)CMBTFIR2::Max_Defensive_Fire_Range(ship_idx);
            return;
        }

        int16_t stasis_count = Get_Number_Of_Ships_In_Stasis(ship_idx);
        int16_t speed = Calc_Current_Speed(ship_idx);
        ship->current_speed = speed;
        ship->movement_left = speed;
        movement_reserve[ship_idx] = Maximum_Combat_Speed(ship_idx) - speed;

        if (ship->special_status_flag < 4 &&
            ship->special_status_timer != 0 &&
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_PHASING_CLOAK) != 0) {
            ship->special_status_flag = 6;
        }
        if (ship->special_status_flag == 2) {
            ship->special_status_flag = 3;
        }
        if (ship->special_status_flag == 6) {
            ship->special_status_flag = 7;
        }

        for (int i = 0; i < 8; ++i) {
            s_combat_weapon_slot* slot = &ship->weapons[i];
            uint8_t count = slot->count;

            if (slot->weapon_id == WEAPON_STASIS_FIELD) {
                if ((int16_t)count < stasis_count) {
                    slot->shots_left = 0;
                    stasis_count -= (int16_t)count;
                } else {
                    slot->shots_left = (uint8_t)((int16_t)count - stasis_count);
                    stasis_count = 0;
                }
            } else {
                slot->shots_left = count;
            }

            if (slot->active != -1) {
                slot->active = 1;
            }

            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_HYPER_X_CAPACITORS) != 0) {
                if (TECHDATA::weapons[slot->weapon_id].type == WEAPON_TYPE_BEAM && slot->status != 1) {
                    slot->status = 2;
                }
            }

            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_FAST_MISSILE_RACKS) != 0) {
                if (TECHDATA::weapons[slot->weapon_id].type == WEAPON_TYPE_MISSILE && slot->status != 1) {
                    slot->status = 2;
                }
            }

            if (slot->weapon_id > 0 && TECHDATA::weapons[slot->weapon_id].type == WEAPON_TYPE_TORPEDO) {
                if (slot->ammo > 0) {
                    slot->ammo--;
                }
            }
        }

        if (continuous_bhg != 0) {
            int16_t active_target_count = 0;
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (MOX::combat_data[i].black_hole_source_idx == ship_idx) {
                    active_target_count++;
                }
            }

            for (int i = 0; i < 8 && active_target_count > 0; ++i) {
                s_combat_weapon_slot* slot = &ship->weapons[i];
                if (slot->weapon_id != WEAPON_BLACK_HOLE_GENERATOR) {
                    continue;
                }

                int16_t committed_shots = slot->shots_left;
                if (committed_shots > active_target_count) {
                    committed_shots = active_target_count;
                }
                slot->shots_left -= (uint8_t)committed_shots;
                active_target_count -= committed_shots;
            }
        }
        ship->ai_range_tactic =
            (int8_t)CMBTFIR2::Max_Defensive_Fire_Range(ship_idx);
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

    void __cdecl Remove_Continuing_Effect_Specials(int16_t source_idx) {
        if (source_idx >= 0 && source_idx < MOX::combat_total_ship_count) {
            MOX::combat_data[source_idx].special_cooldown = COMBAT_SHIP_NONE;
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            // Check if this ship is being held in stasis by the source ship
            if (MOX::combat_data[i].stasis_source_idx == source_idx) {
                CMBTFIRE::Remove_Stasis_Beam(i);
            }

            // Check if this ship is affected by a black hole created by the source ship
            if (MOX::combat_data[i].black_hole_source_idx == source_idx) {
                MOX::combat_data[i].black_hole_flag = 0;
                MOX::combat_data[i].black_hole_source_idx = COMBAT_SHIP_NONE;
            }

            // Check if special cooldown (or effect tracking) matches the source ship
            if (MOX::combat_data[i].special_cooldown == source_idx) {
                MOX::combat_data[i].special_cooldown = COMBAT_SHIP_NONE;
            }
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            Recalculate_Combat_Ship(i);
        }
        if (interactive_combat_stats != 0) {
            CMBTFIR2::Set_Combat_Ordnance_Values();
        }
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
