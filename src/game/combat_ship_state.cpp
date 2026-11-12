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

} // namespace COMBAT1
