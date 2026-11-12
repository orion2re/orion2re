#include "pch.h"

#include "game/boarding.h"

namespace CMBTAI {

    uint8_t ships_without_ammo_stay;
    uint8_t human_fleet_auto_retreat;
    int16_t fleet_target;


    /**
 * Checks if all active enemy ships are currently in stasis.
 * If so, releases the smallest enemy ship from stasis to prevent a combat deadlock.
 * Returns the number of enemy ships that are NOT in stasis.
 */
    int16_t __cdecl Ai_Stasis_Check(int16_t player_idx) {
        int16_t enemy_player;
        int16_t total_enemy_ships = 0;
        int16_t ships_in_stasis = 0;

        if (MOX::combat_attacker_player == player_idx) {
            enemy_player = MOX::combat_defender_player;
        } else {
            enemy_player = MOX::combat_attacker_player;
        }

        // Determine starting index: if planet exists, it is at index 0.
        // If not, we skip index 0.
        int16_t start_idx = (CMBTFIRE::Does_Combat_Planet_Have_Defenses() <= 0);

        for (int16_t i = start_idx; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];

            // Only consider ships that are active, not captured, and belong to the enemy
            if (ship->unit_status == 0 && ship->is_captured == 0 && ship->owner != player_idx) {
                total_enemy_ships++;

                if (CMBTMOV1::Ship_Is_In_Stasis(i)) {
                    ships_in_stasis++;
                }
            }
        }

        // If all active enemy ships are in stasis, force-release one
        if ((total_enemy_ships - ships_in_stasis) < 1) {
            CMBTAI::Release_Smallest_Ship_From_Stasis(enemy_player);
        }

        return total_enemy_ships - ships_in_stasis;
    }

    void __cdecl Toggle_Auto_Combat_Flags() {
        int16_t off_val = 0;
        int16_t on_val = 1;
        int16_t ship_owner = MOX::combat_data[MOX::cur_ship].owner;

        if (MOX::combat_attacker_player == ship_owner) {
            if (ship_owner == MOX::PLAYER_NUM || MOX::game_type == 1) {
                MOX::auto_combat_flag[0] = 0;
                MOX::auto_combat = 0;
                MOX::super_fast_flag = 0;

                if (MOX::net_flag != 0) {
                    NETMOX::Mox_Send_Message(MOX::player[MOX::combat_defender_player].network_player_id, TACTICAL_NET_PACKET_AUTO_COMBAT_DISABLED, &off_val, sizeof(off_val));
                }
            } else {
                MOX::auto_combat_flag[1] = 0;
                MOX::super_fast_flag = 0;

                if (MOX::net_flag != 0) {
                    NETMOX::Mox_Send_Message(MOX::player[ship_owner].network_player_id, TACTICAL_NET_PACKET_AUTO_COMBAT_DISABLED, &on_val, sizeof(on_val));
                }
            }
        } else {
            if (ship_owner == MOX::PLAYER_NUM || MOX::game_type == 1) {
                MOX::auto_combat_flag[1] = 0;
                MOX::auto_combat = 0;
                MOX::super_fast_flag = 0;

                if (MOX::net_flag != 0) {
                    NETMOX::Mox_Send_Message(MOX::player[MOX::combat_attacker_player].network_player_id, TACTICAL_NET_PACKET_AUTO_COMBAT_DISABLED, &on_val, sizeof(on_val));
                }
            } else {
                MOX::auto_combat_flag[0] = 0;
                MOX::super_fast_flag = 0;

                if (MOX::net_flag != 0) {
                    NETMOX::Mox_Send_Message(MOX::player[ship_owner].network_player_id, TACTICAL_NET_PACKET_AUTO_COMBAT_DISABLED, &off_val, sizeof(off_val));
                }
            }
        }
    }

    int8_t __cdecl Get_Player_Mode(int16_t player_idx) {
        int16_t opponent;
        int16_t player_missile_limit;
        int16_t opponent_missile_limit;
        int16_t player_eff_missile;
        int16_t player_lr_beam;
        int16_t opponent_eff_missile;
        int16_t player_eff_beam;
        int16_t opponent_eff_beam;
        int16_t opponent_lr_beam;

        if (player_idx == MOX::combat_attacker_player) {
            opponent = MOX::combat_defender_player;
        } else {
            opponent = MOX::combat_attacker_player;
        }

        if (Retreat_Check(player_idx) != 0) {
            return (int8_t)0xFF;
        }

        player_eff_missile = Get_Effective_Missile_Strength(player_idx, &player_missile_limit);
        player_lr_beam = Get_Long_Range_Beam_Strength(player_idx);
        opponent_eff_missile = Get_Effective_Missile_Strength(opponent, &opponent_missile_limit);
        player_eff_beam = Get_Effective_Beam_Strength(player_idx);
        opponent_eff_beam = Get_Effective_Beam_Strength(opponent);
        opponent_lr_beam = Get_Long_Range_Beam_Strength(opponent);

        // Check if the player is in a strong tactical position
        if ((opponent_eff_missile + opponent_lr_beam < player_eff_missile + player_lr_beam) &&
            MOX::player_mode[player_idx] != 1 && MOX::player_mode[player_idx] != 0) {
            if ((opponent_eff_beam * 3 / 4) >= opponent_missile_limit) {
                return 3;
            }

            if (player_idx == MOX::combat_attacker_player && opponent_missile_limit > 50) {
                return 0;
            }

            return 2;
        } else {
            // Defensive or standard engagement check
            return (opponent_eff_beam * 3 / 2 >= player_eff_beam);
        }
    }

    void __cdecl Do_Auto_Ship_Turn(void) {
        s_combat_data* ship = &MOX::combat_data[MOX::cur_ship];
        int8_t did_move = 0;

        if (CMBTMOV1::Ship_Is_In_Stasis(MOX::cur_ship)) {
            return;
        }

        if (!CMBTDRW1::Ship_Completely_On_Screen(MOX::cur_ship)) {
            COMBINIT::Snap_Center_Combat_Screen(ship->x, ship->y);
        }

        if (((Is_Ship_Out_Of_Ammo(MOX::cur_ship) && ships_without_ammo_stay == 0) ||
                MOX::player_mode[ship->owner] == -1) &&
            ship->ftl_type != 0) {
            if (COMBAT1::Enemy_Dissipator_Exists(ship->owner) == 0 && ship->owner < MAX_PLAYERS && MOX::ai_retreat_flag == 1) {
                ship->is_retreating = 1;
                ship->field40_0x47 = -1;
            }
        }

        Ai_Stasis_Check(ship->owner);
        COMBAT1::Assign_Combat_Grids(MOX::cur_ship);

        int16_t action_result = 0;
        do {
            int16_t target_idx = Choose_Target(MOX::cur_ship);
            if (fields::Get_Input() == MOX::auto_button) {
                Toggle_Auto_Combat_Flags();
            }

            if (target_idx == -1) {
                did_move = 0;
                action_result = 0;
            } else {
                action_result = Auto_Move_Ship(MOX::cur_ship, target_idx);
                if (action_result == 0) {
                    MOX::missile_warning = 2;
                    did_move = 1 - did_move;
                } else {
                    did_move = 0;
                }
            }

            if (fields::Get_Input() == MOX::auto_button) {
                Toggle_Auto_Combat_Flags();
            }

            if (ship->unit_status == 0 && ship->is_captured == 0) {
                for (int i = 0; i < 8; ++i) {
                    ship->weapons[i].active = -1;
                }

                int16_t last_target = -1;
                int16_t weapon_idx = 0;
                do {
                    int16_t weapon_id = ship->weapons[weapon_idx].weapon_id;
                    bool skip_fire = false;
                    bool advance_weapon = false;

                    if (weapon_id == WEAPON_SPATIAL_COMPRESSOR) {
                        if (!Does_Ship_Have_Missiles_Incoming(MOX::cur_ship)) {
                            // Proceed to other checks
                        } else {
                            skip_fire = true;
                        }
                    }

                    if (!skip_fire) {
                        if (weapon_id == WEAPON_PULSAR || weapon_id == WEAPON_SPATIAL_COMPRESSOR) {
                            if (!Safe_To_Fire_Sphere_Weapon(MOX::cur_ship)) {
                                skip_fire = true;
                            }
                        }
                    }

                    if (!skip_fire && weapon_id == WEAPON_STASIS_FIELD) {
                        if (Ai_Stasis_Check(ship->owner) < 2) {
                            skip_fire = true;
                        }
                    }

                    if (!skip_fire) {
                        ship->weapons[weapon_idx].active = 1;
                        int16_t target_idx_inner = Get_Current_Target(MOX::cur_ship, last_target);

                        if (ship->target_type_flag == TARGET_TYPE_SHIP) {
                            if (target_idx_inner != -1) {
                                last_target = target_idx_inner;
                            }
                            int16_t boarding_type = Boarding_Action_Type(MOX::cur_ship, target_idx_inner);
                            if (boarding_type != 0) {
                                Resolve_Ai_Boarding(MOX::cur_ship, target_idx_inner, boarding_type);
                                target_idx_inner = Get_Current_Target(MOX::cur_ship, target_idx_inner);
                            }
                        }

                        if (target_idx_inner != -1) {
                            if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_BEAM) {
                                uint16_t specials = ship->weapons[weapon_idx].specials;
                                if (!(specials & 2)) {
                                    if (!(specials & 4) || Is_Ship_Targeted_By_Missile(MOX::cur_ship) == 0) {
                                        int16_t range;
                                        if (ship->target_type_flag == TARGET_TYPE_SHIP) {
                                            range = Range_To_Ship(MOX::cur_ship, target_idx_inner);
                                        } else {
                                            range = Range_To_Missile(MOX::cur_ship, target_idx_inner);
                                        }
                                        int32_t mod1 = 0;
                                        int32_t mod2 = 0;
                                        CMBTFIRE::Get_Beam_Weapon_Modifiers(MOX::cur_ship, range, specials, weapon_id, &mod1, &mod2);
                                    }
                                }
                            }

                            if (!skip_fire) {
                                int16_t t_x, t_y;
                                if (ship->target_type_flag == TARGET_TYPE_MISSILE) {
                                    t_x = MOX::missile[target_idx_inner].x;
                                    t_y = MOX::missile[target_idx_inner].y;
                                    action_result = CMBTFIRE::Fire_Ship(MOX::cur_ship, ship->x * 20, ship->y * 20, target_idx_inner, t_x, t_y, 1);
                                } else {
                                    t_x = MOX::combat_data[target_idx_inner].x * 20;
                                    t_y = MOX::combat_data[target_idx_inner].y * 20;
                                    action_result = CMBTFIRE::Fire_Ship(MOX::cur_ship, ship->x * 20, ship->y * 20, target_idx_inner, t_x, t_y, 0);
                                    if (ship->owner == 11 /* Crystal */) {
                                        Crystal_Control(target_idx_inner);
                                    }
                                }
                            }
                        } else {
                            skip_fire = true;
                            advance_weapon = true;
                        }
                    }

                    if (skip_fire) {
                        advance_weapon = true;
                        ship->weapons[weapon_idx].active = -1;
                    }

                    video::Set_Page_Off();
                    CMBTDRW1::Draw_Main_Combat_Screen();
                    RUSS::Mox_Sync_Update();
                    Ai_Stasis_Check(ship->owner);
                    if (advance_weapon) {
                        weapon_idx++;
                    }
                    if (weapon_idx >= 8) {
                        break;
                    }
                } while (CMBTFIRE::Is_Combat_Ship_Dead(MOX::cur_ship) == 0);

                for (int i = 0; i < 8; ++i) {
                    ship->weapons[i].active = 1;
                }
            }
        } while (ship->movement_left > 0 && (action_result != 0 || did_move != 0));

        if (COMBAT1::Combat_Ship_Is_Functional(MOX::cur_ship)) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                int16_t boarding_type = Boarding_Action_Type(MOX::cur_ship, i);
                if (boarding_type != 0) {
                    Resolve_Ai_Boarding(MOX::cur_ship, i, boarding_type);
                }
            }
        }

        if (fields::Get_Input() == MOX::auto_button) {
            Toggle_Auto_Combat_Flags();
        }

        if (Ai_Self_Destruct_Check(MOX::cur_ship)) {
            ship->drive_current_hits = 0;
            int8_t old_ftl = ship->ftl_type;
            ship->ftl_type = 1;
            int16_t dead_flag = CMBTFIRE::Is_Combat_Ship_Dead(MOX::cur_ship);
            CMBTFIRE::Destroy_Ship(MOX::cur_ship, ship->x * 20, ship->y * 20, dead_flag);
            ship->ftl_type = old_ftl;
            MOX::ship_turn_order[MOX::cur_ptr] = -1;
        }
    }

    int16_t __cdecl Retreat_Check(int16_t player_idx) {
        static const uint32_t kThreatSentinel = 3000000000u;
        static const int32_t kThreatPercentScale = 100;
        int16_t retreat_result = 0;
        int32_t enemy_expected_damage = 0;
        int32_t enemy_total_hits = 0;
        int32_t player_expected_damage = 0;
        int32_t player_total_hits = 0;
        int32_t ship_remaining_hits[MAX_COMBAT_SHIPS] = {};

        if (player_idx < MAX_PLAYERS &&
            (MOX::player[player_idx].objectives != PLAYER_OBJECTIVE_HUMAN ||
                human_fleet_auto_retreat != 0) &&
            MOX::ai_retreat_flag == 1) {
            if (COMBAT1::Enemy_Dissipator_Exists(player_idx) == 0) {
                for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                    if (i == 0) {
                        if (CMBTFIRE::Does_Combat_Planet_Have_Defenses() == 0) {
                            continue;
                        }
                    }

                    if (MOX::combat_data[i].unit_status == 0 && MOX::combat_data[i].is_captured == 0) {
                        int32_t hits = CMBTAI::Get_Total_Hits(i);
                        ship_remaining_hits[i] = hits;

                        int32_t hit_value = hits;
                        if (i == 0) {
                            hit_value *= 3;
                        }

                        if (MOX::combat_data[i].owner == player_idx) {
                            player_total_hits += hit_value;
                        } else {
                            enemy_total_hits += hit_value;
                        }
                    }
                }

                for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                    if (MOX::combat_data[i].unit_status == 0 && MOX::combat_data[i].is_captured == 0) {
                        int16_t max_dmg = 0;
                        int16_t best_target_idx = -1;

                        for (int16_t j = 0; j < MOX::combat_total_ship_count; ++j) {
                            if (j == 0) {
                                if (CMBTFIRE::Does_Combat_Planet_Have_Defenses() == 0) {
                                    continue;
                                }
                            }

                            if (MOX::combat_data[j].unit_status == 0 && MOX::combat_data[j].is_captured == 0) {
                                if (MOX::combat_data[i].owner != MOX::combat_data[j].owner) {
                                    int16_t total_expected_dmg = 0;
                                    for (int16_t slot = 0; slot < 8; ++slot) {
                                        total_expected_dmg += CMBTAI::Expected_Weapon_Damage(i, slot, j, 0, 0);
                                    }

                                    if (total_expected_dmg > ship_remaining_hits[j]) {
                                        total_expected_dmg = ship_remaining_hits[j];
                                    }

                                    if (total_expected_dmg > max_dmg) {
                                        max_dmg = total_expected_dmg;
                                        best_target_idx = j;
                                    }
                                }
                            }
                        }

                        if (max_dmg > 0) {
                            ship_remaining_hits[best_target_idx] -= max_dmg;
                            if (MOX::combat_data[i].owner == player_idx) {
                                player_expected_damage += max_dmg;
                            } else {
                                enemy_expected_damage += max_dmg;
                            }
                        }
                    }
                }

                uint32_t player_threat = kThreatSentinel;
                if (enemy_total_hits != 0) {
                    player_threat = (uint32_t)((player_expected_damage * kThreatPercentScale) / enemy_total_hits);
                }

                uint32_t enemy_threat = kThreatSentinel;
                if (player_total_hits != 0) {
                    enemy_threat = (uint32_t)((enemy_expected_damage * kThreatPercentScale) / player_total_hits);
                }

                if (player_threat * 5 < enemy_threat) {
                    retreat_result = 1;
                }
            }
        }

        return retreat_result;
    }

    int16_t __cdecl Get_Long_Range_Beam_Strength(int16_t player_idx) {
        int32_t total_strength = 0;

        for (int16_t ship_idx = 0; ship_idx < MOX::combat_total_ship_count; ++ship_idx) {
            s_combat_data* ship = &MOX::combat_data[ship_idx];

            if (ship->unit_status == 0 && ship->is_captured == 0 && ship->owner == player_idx) {
                for (int16_t slot_idx = 0; slot_idx < 8; ++slot_idx) {
                    int16_t weapon_id = ship->weapons[slot_idx].weapon_id;

                    if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_BEAM || weapon_id == WEAPON_PROTON_TORPEDO) {
                        bool is_long_range = false;

                        if ((TECHDATA::weapons[weapon_id].natural_mods & WEAPON_NATURAL_FLAG_NO_RANGE_DISSIPATION) != 0) {
                            is_long_range = true;
                        } else if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, 15 /* SPEC_RANGE_FINDER */) != 0) {
                            is_long_range = true;
                        } else if ((ship->weapons[slot_idx].specials & 0x20 /* WSPEC_MOD_CONTINUOUS */) != 0) {
                            is_long_range = true;
                        }

                        bool add_damage = false;
                        if (is_long_range) {
                            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, 25 /* SPEC_STRUCT_SCANNER */) != 0) {
                                add_damage = true;
                            }
                        }

                        if (!add_damage) {
                            // Heavy mount check or Mauler Device fallback
                            if ((ship->weapons[slot_idx].specials & 2 /* WSPEC_MOD_HEAVY */) != 0 || weapon_id == 19) {
                                add_damage = true;
                            }
                        }

                        if (add_damage) {
                            total_strength += CMBTAI::Get_Effective_Weapon_Damage(ship_idx, slot_idx);
                        }
                    }
                }
            }
            RUSS::Mox_Update();
        }

        return total_strength > INT16_MAX ? INT16_MAX : (int16_t)total_strength;
    }

    int16_t __cdecl Get_Effective_Beam_Strength(int16_t player_idx) {
        int32_t total_strength = 0;
        int16_t ship_idx = 0;

        if (MOX::combat_total_ship_count > 0) {
            for (ship_idx = 0; ship_idx < MOX::combat_total_ship_count; ++ship_idx) {
                s_combat_data* ship = &MOX::combat_data[ship_idx];

                if (ship->unit_status == 0 && ship->is_captured == 0 && ship->owner == player_idx) {
                    for (int16_t weapon_idx = 0; weapon_idx < 8; ++weapon_idx) {
                        int16_t weapon_id = ship->weapons[weapon_idx].weapon_id;

                        if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_BEAM || weapon_id == WEAPON_PROTON_TORPEDO) {
                            total_strength += CMBTAI::Get_Effective_Weapon_Damage(ship_idx, weapon_idx);
                        }
                    }
                }
                RUSS::Mox_Update();
            }
        }

        return total_strength > INT16_MAX ? INT16_MAX : (int16_t)total_strength;
    }

    /**
 * @brief Selects the best ship to target for a given ship.
 * @param ship_idx The index of the ship selecting a target.
 * @return The index of the best target ship, or -1 if no suitable target is found.
 */
    int16_t __cdecl Select_Ship_To_Target(int16_t ship_idx) {
        int16_t best_target_idx = -1;
        int32_t best_value = -5000;

        if (MOX::combat_total_ship_count > 1) {
            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                // Only target active ships that are enemies, not in stasis, and not cloaked/disabled (flag 4)
                if (MOX::combat_data[i].unit_status == 0 &&
                    MOX::combat_data[ship_idx].owner != MOX::combat_data[i].owner &&
                    MOX::combat_data[i].special_status_flag != 4 &&
                    !CMBTMOV1::Ship_Is_In_Stasis(i)) {
                    int32_t value = Target_Ship_Value(ship_idx, i, 0);
                    if (value > best_value) {
                        best_value = value;
                        best_target_idx = i;
                    }
                }
            }
        }

        return best_target_idx;
    }

    /**
 * @brief Chooses a target for the given ship in combat.
 * @param ship_idx The index of the ship in the combat data array.
 * @return The index of the selected target ship, or 0 in specific edge cases where no ship is found.
 */
    int16_t __cdecl Choose_Target(int16_t ship_idx) {
        MOX::combat_data[ship_idx].target_type_flag = TARGET_TYPE_SHIP;

        int16_t target_idx = CMBTAI::Select_Ship_To_Target(ship_idx);

        if (target_idx == -1) {
            // Special case: if no target is found, and the first ship in the combat list
            // is in a specific unit status (0), and the current ship belongs to the attacker,
            // default the target to ship 0.
            if (MOX::combat_data[0].unit_status == 0 &&
                MOX::combat_data[ship_idx].owner == MOX::combat_attacker_player) {
                target_idx = 0;
            }
        }

        return target_idx;
    }

    int32_t __cdecl Target_Ship_Value(int16_t attacker_idx, int16_t target_idx, int16_t distance_flag) {
        int16_t damage_dealt = 0;
        int16_t damage_received = 0;

        int32_t total_hits = CMBTAI::Get_Total_Hits(target_idx);

        if (CMBTMOV1::Ship_Is_In_Stasis(target_idx)) {
            return -20000;
        }

        int16_t range;
        if (distance_flag <= 0) {
            range = 0;
        } else {
            range = CMBTAI::Range_To_Ship(attacker_idx, target_idx);
            if (range < 1) {
                range = 1;
            }
        }

        for (int16_t w = 0; w < 8; ++w) {
            int16_t expected_dmg = CMBTAI::Expected_Weapon_Damage(attacker_idx, w, target_idx, 0, range);
            if (target_idx != 0 && TECHDATA::weapons[MOX::combat_data[attacker_idx].weapons[w].weapon_id].type == WEAPON_TYPE_FIGHTER) {
                expected_dmg *= 2;
            }
            damage_dealt += expected_dmg;

            if (MOX::combat_data[target_idx].is_captured == 0) {
                damage_received += CMBTAI::Expected_Weapon_Damage(target_idx, w, attacker_idx, 0, 0);
            }
        }

        if (damage_dealt == 0) {
            return -10000;
        }

        uint32_t value_num = (uint32_t)((int64_t)damage_dealt * damage_received * 3);
        int32_t value = (int32_t)(value_num / (uint32_t)total_hits);

        if (distance_flag != 0) {
            if (target_idx != 0) {
                if (MOX::combat_data[target_idx].shield_current_hits > 0) {
                    int16_t bearing = CMBTDRW1::Relative_Bearing(target_idx, attacker_idx);
                    int16_t shield_idx = CMBTFIRE::Facing_Shield(bearing);
                    int16_t shield_hp = MOX::combat_data[target_idx].shield_arc_current[shield_idx];

                    if (damage_dealt < shield_hp / 4) {
                        value = 1;
                    } else if (damage_dealt < shield_hp) {
                        value /= 2;
                        if (value < 2) {
                            value = 2;
                        }
                    }
                }
            }
        }

        if (target_idx == 0) {
            if (damage_dealt > total_hits) {
                value += damage_received * 3;
            }
        } else {
            if (damage_dealt > total_hits / 3) {
                value += damage_received * 5;
            }
        }

        if (CMBTMOV1::Ship_In_Black_Hole(target_idx)) {
            value /= 2;
        }

        if (CMBTAI::Missile_Overkill_Check(target_idx) == 1) {
            value -= 1000;
        }

        if (damage_received == 0) {
            value -= 2000;
        }

        if (CMBTFIR2::Being_Captured(target_idx) != 0) {
            value -= 5000;
        }

        return value;
    }

    int8_t __cdecl Combat_Ship_Class(int16_t ship_idx, int16_t target_idx) {
        int16_t dmg_short_range = 0;
        int16_t dmg_med_range = 0;
        int16_t dmg_long_range = 0;

        for (int16_t slot = 0; slot < 8; ++slot) {
            int16_t weapon_id = MOX::combat_data[ship_idx].weapons[slot].weapon_id;
            if (weapon_id == 0) {
                continue;
            }

            int8_t weapon_type_class = TECHDATA::weapons[weapon_id].type;
            switch (weapon_type_class) {
                case 0: {
                    int16_t d1 = Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 1);
                    int16_t d5 = Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 5);
                    int16_t d12 = Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 12);

                    if (d5 / 2 < d12) {
                        dmg_long_range += d12;
                    } else if (d1 / 2 < d5) {
                        dmg_med_range += d5;
                    } else {
                        dmg_short_range += d1;
                    }
                    break;
                }
                case 1:
                    if (MOX::combat_turn == 1) {
                        dmg_med_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 5);
                    } else {
                        dmg_long_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 12);
                    }
                    break;
                case 2:
                    if (weapon_id == WEAPON_PLASMA_TORPEDO || weapon_id == WEAPON_DRAGON_BREATH) {
                        dmg_short_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 1);
                    } else if (weapon_id == WEAPON_PROTON_TORPEDO) {
                        dmg_med_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 5);
                    } else {
                        dmg_long_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 12);
                    }
                    break;
                case 3:
                    if (target_idx == 0) {
                        dmg_short_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 0);
                    }
                    break;
                case 4:
                    dmg_short_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 1);
                    break;
                case 5:
                    if (weapon_id == WEAPON_PULSAR || weapon_id == WEAPON_PLASMA_FLUX || weapon_id == WEAPON_STASIS_FIELD ||
                        weapon_id == WEAPON_SPATIAL_COMPRESSOR || weapon_id == WEAPON_TRACTOR_BEAM) {
                        dmg_short_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 1);
                    } else if (weapon_id == WEAPON_GYRO_DESTABILIZER || weapon_id == WEAPON_CAUSTIC_SLIME ||
                        weapon_id == WEAPON_BLACK_HOLE_GENERATOR || weapon_id == WEAPON_PLASMA_WEB) {
                        dmg_med_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 1);
                    } else if (weapon_id == WEAPON_STELLAR_CONVERTER) {
                        dmg_long_range += Expected_Weapon_Damage(ship_idx, slot, target_idx, 0, 12);
                    }
                    break;
                default:
                    break;
            }
        }

        int8_t owner = MOX::combat_data[MOX::cur_ship].owner;
        if (MOX::player_mode[owner] == 0 || MOX::player_mode[owner] == 1) {
            dmg_short_range *= 2;
        }

        int32_t short_3_2 = (((int32_t)dmg_short_range) * 3) / 2;
        if (short_3_2 <= (int32_t)dmg_long_range) {
            return ((((int32_t)dmg_med_range) * 3) / 2 <= (int32_t)dmg_long_range) + 1;
        } else {
            return (int32_t)dmg_med_range >= short_3_2;
        }
    }

    bool __cdecl Does_Ship_Have_Missiles_Incoming(int16_t ship_idx) {
        bool has_incoming = false;

        for (int32_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile* missile = &MOX::missile[i];

            if (missile->target_unit_idx == ship_idx && missile->type > 0 && missile->target_is_missile == 0) {
                if (CMBTAI::Will_Missile_Do_Damage(ship_idx, (int16_t)i)) {
                    // Check if the missile is hostile to the ship owner
                    if (MOX::combat_data[ship_idx].owner != CMBTMIS::Missile_Owner((int16_t)i)) {
                        has_incoming = true;
                    }
                }
            }
        }

        return has_incoming;
    }

    int16_t __cdecl Auto_Move_Ship(int16_t ship_idx, int16_t target_idx) {
        int16_t moved = 0;
        int16_t num_possible_moves = 0;
        int16_t cur_ship_idx = MOX::cur_ship;
        s_combat_data* cur_ship = &MOX::combat_data[cur_ship_idx];
        int8_t movement_tactic = Combat_Ship_Class(cur_ship_idx, target_idx);

        if (MOX::player_mode[cur_ship->owner] == -1) {
            movement_tactic = 2;
        }

        bool skip_ai_movement = false;
        if (cur_ship->target_type_flag == TARGET_TYPE_SHIP) {
            int8_t mode = MOX::player_mode[cur_ship->owner];
            if (mode == 3 || mode == 2) {
                if (MOX::combat_turn != 1 || TECHDATA::weapons[cur_ship->weapons[0].weapon_id].type != WEAPON_TYPE_MISSILE) {
                    skip_ai_movement = true;
                }
            }
        }

        MOX::auto_msl_warning_off = 0;

        int16_t move_y_coords[20];
        int16_t move_x_coords[20];

        if (!skip_ai_movement) {
            int16_t target_x;
            int16_t target_y;

            if (cur_ship->target_type_flag == TARGET_TYPE_MISSILE) {
                target_y = MOX::missile[target_idx].y / 20;
                target_x = MOX::missile[target_idx].x / 20;
            } else {
                target_y = (int16_t)MOX::combat_data[target_idx].y;
                target_x = (int16_t)MOX::combat_data[target_idx].x;
            }

            int16_t dist = special::Range((int16_t)cur_ship->x, (int16_t)cur_ship->y, target_x, target_y);
            int16_t score_limit = 0;

            if (movement_tactic == 0) {
                score_limit = dist - 3;
            } else if (movement_tactic == 1) {
                score_limit = dist - 15;
            } else if (movement_tactic == 2) {
                score_limit = dist - 50;
            }

            if (score_limit > 0) {
                for (int16_t gx = 0; gx < MOX::combat_grid_width; ++gx) {
                    for (int16_t gy = 0; gy < MOX::combat_grid_height; ++gy) {
                        if (MOX::combat_grid[gx][gy] == 2000) {
                            int16_t dist_from_hex;
                            if (cur_ship->target_type_flag == TARGET_TYPE_MISSILE) {
                                dist_from_hex = special::Range(gx, gy, MOX::missile[target_idx].x / 20, MOX::missile[target_idx].y / 20);
                            } else {
                                dist_from_hex = special::Range(gx, gy, (int16_t)MOX::combat_data[target_idx].x, (int16_t)MOX::combat_data[target_idx].y);
                            }

                            score_limit = CMBTAI::Evaluate_Possible_Move(
                                dist_from_hex,
                                gx,
                                gy,
                                movement_tactic,
                                &num_possible_moves,
                                move_x_coords,
                                move_y_coords,
                                score_limit);
                        }
                    }
                }
            }

            if (num_possible_moves > 0) {
                int rnd_idx = game_random::Random((int)num_possible_moves) - 1;
                int16_t mx = move_x_coords[rnd_idx];
                int16_t my = move_y_coords[rnd_idx];
                int16_t move_cost = CMBTMOV1::Move_Cost(cur_ship_idx, mx, my);
                int16_t movement_used = CMBTMOV1::Move_Ship(cur_ship_idx, mx, my, move_cost);

                if (movement_used > 0) {
                    moved = 1;
                    cur_ship->movement_left -= movement_used;
                    if (cur_ship->movement_left < 0) {
                        cur_ship->movement_left = 0;
                    }
                }
            }
        }

        int16_t final_target_x;
        int16_t final_target_y;

        if (cur_ship->target_type_flag == TARGET_TYPE_SHIP) {
            final_target_y = (int16_t)MOX::combat_data[target_idx].y;
            final_target_x = (int16_t)MOX::combat_data[target_idx].x;
        } else {
            final_target_y = MOX::missile[target_idx].y / 20;
            final_target_x = MOX::missile[target_idx].x / 20;
        }

        CMBTMOV1::Turn_To_Face(cur_ship_idx, final_target_x, final_target_y);
        MOX::auto_msl_warning_off = 0;

        return moved;
    }

    /**
 * Checks if a specific ship is the target of any active offensive missile.
 * 
 * @param ship_idx The index of the ship to check.
 * @return 1 if the ship is targeted by a missile that is not an anti-missile rocket and not fired by itself; 0 otherwise.
 */
    int16_t __cdecl Is_Ship_Targeted_By_Missile(int16_t ship_idx) {
        int16_t result = 0;

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            // Early exit if a match was already found in a previous iteration
            if (result != 0) {
                break;
            }

            // A missile is targeting the ship if:
            // 1. Its target matches the ship index
            // 2. It is not an anti-missile rocket (which targets other missiles/fighters)
            // 3. The ship did not fire the missile at itself (source index check)
            if (MOX::missile[i].target_unit_idx == ship_idx &&
                MOX::missile[i].target_is_missile == 0 &&
                MOX::missile[i].source_unit_idx != ship_idx) {
                result = 1;
            }
        }

        return result;
    }

    void __cdecl Release_Smallest_Ship_From_Stasis(int16_t owner_idx) {
        int16_t smallest_ship_idx = -1;
        int16_t min_size_class = 100;

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];

            if (ship->unit_status == 0 && ship->is_captured == 0 && ship->owner == owner_idx) {
                if (CMBTMOV1::Ship_Is_In_Stasis(i)) {
                    if (ship->size_class < min_size_class) {
                        min_size_class = ship->size_class;
                        smallest_ship_idx = i;
                    }
                }
            }
        }

        if (smallest_ship_idx != -1) {
            CMBTFIRE::Remove_Stasis_Beam(smallest_ship_idx);
        }
    }

    /**
 * @brief Evaluates a possible move score against the current best score and updates the best moves list.
 * 
 * @param score The raw score of the move being evaluated.
 * @param move_x First value associated with the move (for example, an X coordinate).
 * @param move_y Second value associated with the move (for example, a Y coordinate).
 * @param move_type Determines the base offset to subtract from the score.
 * @param best_move_count Pointer to the current count of tied best moves.
 * @param best_move_xs Array storing the first values of the tied best moves.
 * @param best_move_ys Array storing the second values of the tied best moves.
 * @param best_score The current best score found so far.
 * @return The updated best score.
 */
    int16_t __cdecl Evaluate_Possible_Move(int16_t score, int16_t move_x, int16_t move_y, int16_t move_type, int16_t* best_move_count, int16_t* best_move_xs, int16_t* best_move_ys, int16_t best_score) {
        int16_t base_val;

        if (move_type == 0) {
            base_val = 3;
        } else if (move_type == 1) {
            base_val = 15; /* 0xF */
        } else if (move_type == 2) {
            base_val = 50; /* 0x32 */
        } else {
            // In the original binary, if move_type is not 0, 1, or 2,
            // base_val is loaded from an uninitialized stack location.
        }

        int16_t eval_score = score - base_val;

        // Only consider moves with non-negative adjusted scores.
        if (eval_score >= 0) {
            if (eval_score < best_score) {
                // Found a new strictly better score: reset the ties list.
                *best_move_count = 0;
                best_move_xs[0] = move_x;
                best_move_ys[0] = move_y;
                (*best_move_count)++;
                return eval_score;
            } else if (eval_score == best_score) {
                // Found a move with an equivalent score: add it to the ties list if space remains.
                if (*best_move_count < 20 /* 0x14 */) {
                    best_move_xs[*best_move_count] = move_x;
                    best_move_ys[*best_move_count] = move_y;
                    (*best_move_count)++;
                }
                return best_score;
            }
        }

        return best_score;
    }

    void __cdecl Crystal_Control(int16_t ship_idx) {
        char msg_buffer[80];

        if (ship_idx != 0) {
            s_combat_data* ship = &MOX::combat_data[ship_idx];

            if (ship->unit_status == 0 && ship->marine_count == 0) {
                CMBTFIR2::Capture_Ship(ship_idx, (int16_t)ship->x * 20, (int16_t)ship->y * 20);

                ship->is_captured = 0;

                KEN::Ken_Get_Text_Message(0x4c, msg_buffer, 0x50);
                strlcat(msg_buffer, ship->name, sizeof(msg_buffer));

                palette::Refresh_Palette();
                GENDRAW::Message_Box(msg_buffer);
                CMBTDRW1::Refresh_Combat_Screen_Full();
            }
        }
    }

    /**
 * @brief Evaluates and selects the best target (ship or missile) for the current unit.
 *
 * @param ship_idx Index of the ship performing targeting.
 * @param reference_target_idx Used as a target ship index filter/limit in human modes.
 * @return int16_t The index of the selected target.
 */
    int16_t __cdecl Get_Current_Target(int16_t ship_idx, int16_t reference_target_idx) {
        int32_t missile_score;
        int32_t ship_score;
        int16_t missile_target;
        int16_t ship_target;
        int8_t owner_idx;

        owner_idx = MOX::combat_data[ship_idx].owner;

        if (MOX::player_mode[owner_idx] != 1 && MOX::player_mode[owner_idx] != 3) {
            // AI Logic: Prioritize targets based on weighted scores
            missile_target = Get_Current_Missile_Target(ship_idx, &missile_score);
            ship_target = Get_Current_Ship_Target(ship_idx, -1, &ship_score);

            if (ship_score / 2 > missile_score) {
                MOX::combat_data[ship_idx].target_type_flag = TARGET_TYPE_SHIP;
                return ship_target;
            } else {
                MOX::combat_data[ship_idx].target_type_flag = TARGET_TYPE_MISSILE;
                return missile_target;
            }
        } else {
            // Human/Manual Logic
            ship_target = Get_Current_Ship_Target(ship_idx, reference_target_idx, &ship_score);
            missile_target = Get_Current_Missile_Target(ship_idx, &missile_score);

            if (missile_score / 2 > ship_score) {
                MOX::combat_data[ship_idx].target_type_flag = TARGET_TYPE_MISSILE;
                return missile_target;
            } else {
                MOX::combat_data[ship_idx].target_type_flag = TARGET_TYPE_SHIP;
                return ship_target;
            }
        }
    }

    /**
 * Finds the best target for the current ship based on distance, ship values, and weapon availability.
 * Returns the target ship index and updates out_value with the target's relative priority score.
 */
    int16_t __cdecl Get_Current_Ship_Target(int16_t current_ship_idx, int16_t reference_target_idx, int32_t* out_value) {
        int16_t best_target_idx = -1;
        int32_t best_value = -5000;

        int16_t start_idx = (CMBTFIRE::Does_Combat_Planet_Have_Defenses() == 0) ? 1 : 0;

        for (int16_t target_idx = start_idx; target_idx < MOX::combat_total_ship_count; ++target_idx) {
            if (MOX::combat_data[target_idx].unit_status == 0 &&
                MOX::combat_data[current_ship_idx].owner != MOX::combat_data[target_idx].owner &&
                MOX::combat_data[target_idx].special_status_flag != 4 &&
                CMBTMOV1::Ship_Is_In_Stasis(target_idx) == 0) {
                bool weapon_found = false;

                for (int16_t weapon_idx = 0; weapon_idx < 8; ++weapon_idx) {
                    s_combat_weapon_slot* weapon = &MOX::combat_data[current_ship_idx].weapons[weapon_idx];

                    if (weapon->weapon_id != 0) {
                        int16_t bearing = CMBTDRW1::Relative_Bearing(current_ship_idx, target_idx);
                        int8_t arc_mask = CMBTDRW1::Get_Weapon_Arc(bearing);

                        int8_t weapon_type_class = TECHDATA::weapons[weapon->weapon_id].type;

                        if (COMBAT1::Weapon_Is_On(current_ship_idx, weapon_idx) != 0) {
                            if (weapon->shots_left > 0) {
                                if ((arc_mask & weapon->arc) != 0) {
                                    if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
                                        if (weapon->ammo != 0) {
                                            continue;
                                        }
                                    } else {
                                        if (weapon->ammo == 0) {
                                            continue;
                                        }
                                    }

                                    if (CMBTFIRE::Weapon_In_Range(weapon_idx, current_ship_idx, target_idx, 0, 0) != 0) {
                                        weapon_found = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if (weapon_found) {
                    int32_t value = CMBTAI::Target_Ship_Value(current_ship_idx, target_idx, 1);

                    if (target_idx == reference_target_idx && value > 0) {
                        if (value <= 6000) {
                            value *= 10;
                        } else {
                            value = 60000;
                        }
                    }

                    if (CMBTAI::fleet_target == target_idx && value > 0) {
                        int32_t old_value = value;
                        value *= 5;
                        if (old_value > 12000) {
                            value = 60000;
                        }
                    }

                    if (value > best_value) {
                        best_target_idx = target_idx;
                        best_value = value;
                    }
                }
            }
        }

        if (best_target_idx == -1) {
            *out_value = -20000;
        } else {
            *out_value = best_value;
        }

        return best_target_idx;
    }

    int16_t __cdecl Missile_Overkill_Check(int16_t target_idx) {
        int64_t total_damage = 0;
        int16_t overkill = 0;

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile* missile = &MOX::missile[i];

            if (missile->target_unit_idx == target_idx &&
                missile->source_unit_idx != target_idx &&
                missile->type > 0) {
                int16_t source_owner = MOX::combat_data[missile->source_unit_idx].owner;
                total_damage += CMBTAI::Expected_Missile_Damage(
                    target_idx,
                    missile->type,
                    missile->quantity,
                    missile->specials,
                    source_owner);
            }
        }

        if (target_idx == 0) {
            int32_t total_hits = CMBTAI::Get_Total_Hits(0);
            if (total_damage > (total_hits * 3) / 2) {
                overkill = 1;
            }
        } else {
            int32_t total_hits = CMBTAI::Get_Total_Hits(target_idx);
            if (total_damage > total_hits) {
                overkill = 1;
            }
        }

        return overkill;
    }

    int16_t __cdecl Convert_Ordinal_To_Msl_Type(int16_t ordinal) {
        switch (ordinal) {
            case 0:
                return WEAPON_NUCLEAR_MISSILE;
            case 1:
                return WEAPON_MERCULITE_MISSILE;
            case 2:
                return WEAPON_PULSON_MISSILE;
            case 3:
                return WEAPON_ZEON_MISSILE;
            case 4:
                return WEAPON_HEAVY_FIGHTER;
            case 5:
                return WEAPON_BOMBER;
            case 6:
                return WEAPON_INTERCEPTOR;
            case 7:
                return WEAPON_ANTI_MATTER_TORP;
            case 8:
                return WEAPON_PLASMA_TORPEDO;
            case 9:
                return WEAPON_STELLAR_CONVERTER;
            default:
                return ordinal;
        }
    }

    int16_t __cdecl Get_Effective_Missile_Strength(int16_t player_idx, int16_t* beam_strength) {
        int16_t target_player;
        int16_t shield_points;
        int32_t missile_counts[11];
        int32_t total_quantity = 0;

        if (player_idx == MOX::combat_attacker_player) {
            target_player = MOX::combat_defender_player;
        } else {
            target_player = MOX::combat_attacker_player;
        }

        int16_t best_shield = INITSHIP::Best_Ship_Shield(target_player);
        if (best_shield == SPECIAL_DAMPER_FIELD) {
            shield_points = 100;
        } else {
            shield_points = TECHDATA::shields[best_shield].shield_class;
        }

        *beam_strength = 0;
        struct_::Clear_Structure(missile_counts, sizeof(missile_counts));

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (MOX::combat_data[i].unit_status == 0 && MOX::combat_data[i].is_captured == 0 && MOX::combat_data[i].owner == player_idx) {
                for (int16_t j = 0; j < 8; ++j) {
                    int16_t weapon_id = MOX::combat_data[i].weapons[j].weapon_id;
                    if (weapon_id <= 0) continue;

                    uint8_t count = MOX::combat_data[i].weapons[j].count;
                    if (count <= 0) continue;

                    int8_t weapon_type_class = TECHDATA::weapons[weapon_id].type;
                    if (weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_FIGHTER) {
                        if (MOX::combat_data[i].weapons[j].ammo <= 0) continue;
                    } else if (weapon_type_class != WEAPON_TYPE_TORPEDO) {
                        continue;
                    }

                    int16_t ordinal = -1;
                    if (weapon_id >= 14 && weapon_id <= 31) {
                        static const uint8_t ordinal_map[] = {0, 1, 2, 3, 9, 9, 7, 9, 8, 9, 9, 9, 9, 9, 9, 4, 5, 6};
                        ordinal = ordinal_map[weapon_id - 14];
                    }

                    if (ordinal >= 0 && ordinal <= 6) {
                        if (ordinal >= 4) {
                            missile_counts[ordinal] += (count * 4);
                        } else {
                            missile_counts[ordinal] += count;
                        }
                        total_quantity += count;
                    } else if (ordinal == 7 || ordinal == 8) {
                        missile_counts[ordinal] += count;
                    }
                }
            }
            RUSS::Mox_Update();
        }

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile* msl = &MOX::missile[i];
            if (msl->type > 0 && TECHDATA::weapons[msl->type].type == WEAPON_TYPE_FIGHTER && MOX::combat_data[msl->source_unit_idx].owner == player_idx) {
                if (msl->type == WEAPON_HEAVY_FIGHTER) {
                    missile_counts[4] += msl->quantity;
                    total_quantity += msl->quantity;
                } else if (msl->type == WEAPON_BOMBER) {
                    missile_counts[5] += msl->quantity;
                    total_quantity += msl->quantity;
                } else if (msl->type == WEAPON_INTERCEPTOR) {
                    missile_counts[6] += msl->quantity;
                    total_quantity += msl->quantity;
                }
            }
            RUSS::Mox_Update();
        }

        if (total_quantity <= 0) {
            *beam_strength = CMBTAI::Get_Effective_Beam_Strength(target_player);
        } else {
            int16_t current_ordinal = 0;
            while (missile_counts[current_ordinal] == 0 && current_ordinal < 7) {
                current_ordinal++;
            }
            int16_t msl_type = Convert_Ordinal_To_Msl_Type(current_ordinal);

            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (current_ordinal > 6) {
                    break;
                }

                if (MOX::combat_data[i].unit_status == 0 && MOX::combat_data[i].is_captured == 0 && MOX::combat_data[i].owner != player_idx) {
                    for (int16_t j = 0; j < 8; ++j) {
                        if (current_ordinal > 6) {
                            break;
                        }

                        int16_t weapon_id = MOX::combat_data[i].weapons[j].weapon_id;
                        int16_t remaining_pd_count = 0;

                        if (CMBTFIRE::Weapon_Can_Fire_At_Missiles(weapon_id, MOX::combat_data[i].weapons[j].specials)) {
                            if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_BEAM) {
                                remaining_pd_count = MOX::combat_data[i].weapons[j].count;
                                while (current_ordinal <= 6 && remaining_pd_count > 0) {
                                    const int16_t evaluated_count = missile_counts[current_ordinal] > INT16_MAX
                                        ? INT16_MAX
                                        : (int16_t)missile_counts[current_ordinal];
                                    int16_t kills = Average_Missile_Kills(i, j, evaluated_count, msl_type, &remaining_pd_count, -1);
                                    missile_counts[current_ordinal] -= (int16_t)kills;

                                    while (missile_counts[current_ordinal] == 0 && current_ordinal < 7) {
                                        current_ordinal++;
                                    }
                                    msl_type = Convert_Ordinal_To_Msl_Type(current_ordinal);

                                    if (missile_counts[current_ordinal] == 0 || current_ordinal > 6 || remaining_pd_count <= 0) {
                                        break;
                                    }
                                }
                            }
                        }

                        if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_BEAM) {
                            const int32_t strength = *beam_strength +
                                (int32_t)TECHDATA::weapons[weapon_id].max_damage * remaining_pd_count;
                            *beam_strength = strength > INT16_MAX ? INT16_MAX : (int16_t)strength;
                        }
                    }
                }
                RUSS::Mox_Update();
            }
        }

        int64_t total_effective_strength = 0;
        for (int16_t i = 0; i <= 9; ++i) {
            int16_t msl_id = Convert_Ordinal_To_Msl_Type(i);
            int16_t dmg_per;
            int16_t max_damage = TECHDATA::weapons[msl_id].max_damage;
            if (shield_points == 100) {
                dmg_per = (max_damage + (max_damage < 0 ? 3 : 0)) / 4;
            } else {
                dmg_per = 0;
                if (max_damage > shield_points) {
                    dmg_per = max_damage - shield_points;
                }
            }
            total_effective_strength += (int64_t)dmg_per * missile_counts[i];
        }

        return total_effective_strength > INT16_MAX ? INT16_MAX : (int16_t)total_effective_strength;
    }

    int16_t __cdecl Average_Missile_Kills(int16_t ship_idx, int16_t weapon_slot_idx, int16_t msl_count, int16_t msl_type, int16_t* remaining_pd_count, int16_t missile_idx) {
        int16_t kills = 0;
        int16_t slot_weapon_id = MOX::combat_data[ship_idx].weapons[weapon_slot_idx].weapon_id;
        uint16_t specials = MOX::combat_data[ship_idx].weapons[weapon_slot_idx].specials;
        int16_t range;

        if (missile_idx > -1) {
            range = Range_To_Missile(ship_idx, missile_idx);
        } else {
            range = 2;
        }

        int16_t target_player;
        if (MOX::combat_data[ship_idx].owner == MOX::combat_attacker_player) {
            target_player = MOX::combat_defender_player;
        } else {
            target_player = MOX::combat_attacker_player;
        }

        int16_t current_health = *remaining_pd_count;

        if (TECHDATA::weapons[msl_type].type != WEAPON_TYPE_TORPEDO) {
            int64_t hit_chance = (CMBTMIS::Missile_Dcv(msl_type, target_player) -
                                     MOX::combat_data[ship_idx].tac_val_attack -
                                     MOX::ranged_to_hit_penalty[range]) +
                40;

            if (hit_chance < 95) {
                hit_chance = (CMBTMIS::Missile_Dcv(msl_type, target_player) -
                                 MOX::combat_data[ship_idx].tac_val_attack -
                                 MOX::ranged_to_hit_penalty[range]) +
                    40;
            } else {
                hit_chance = 95;
            }

            int16_t base_dmg = (int16_t)((TECHDATA::weapons[slot_weapon_id].min_damage + TECHDATA::weapons[slot_weapon_id].max_damage) * 5);

            if (!((specials & 0x20) || (TECHDATA::weapons[slot_weapon_id].natural_mods & WEAPON_NATURAL_FLAG_NO_RANGE_DISSIPATION) || COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, 0xF))) {
                int16_t penalty = MOX::ranged_damage_penalty[range];
                base_dmg += (abs(penalty) * base_dmg) / -100;
            }

            int64_t avg_dmg_per_missile = ((100 - hit_chance) * base_dmg) / 100;
            int64_t total_hits = CMBTMIS::Get_Missile_Hits(msl_type, 0, 0) * 10;
            int64_t divisor;

            if (total_hits < avg_dmg_per_missile) {
                divisor = 1;
            } else if (avg_dmg_per_missile == 0) {
                divisor = 1000;
            } else {
                divisor = (total_hits + avg_dmg_per_missile - 1) / avg_dmg_per_missile;
            }

            kills = (int16_t)(current_health / divisor);

            if (msl_count < kills) {
                *remaining_pd_count = current_health - (int16_t)(msl_count * divisor);
                return msl_count;
            } else {
                *remaining_pd_count = 0;
            }
        }

        return kills;
    }

    int8_t __cdecl Ai_Self_Destruct_Check(int16_t ship_idx) {
        int8_t ret_val = 0;
        int16_t dist_friendly = 0; // BUG: Original code failed to initialize this variable due to a compiler offset bug
        int16_t dist_enemy = 0;
        int32_t friendly_rack4 = 0;
        int32_t enemy_rack4 = 0;
        int32_t friendly_other = 0;
        int32_t score = 0;
        int16_t strength_attacker = 0;
        int16_t strength_defender = 0;
        int32_t marine_count_sum = 0;

        if (ship_idx == 0) return ret_val;

        s_combat_data* ship = &MOX::combat_data[ship_idx];

        if (ship->owner > 8 || ship->ftl_type <= 0 ||
            (BOARDING::board_in_black_hole == 0 &&
                CMBTMOV1::Ship_In_Black_Hole(ship_idx))) {
            return ret_val;
        }

        int16_t player_id = MOX::combat_attacker_player;
        if (ship->owner == MOX::combat_attacker_player) {
            player_id = MOX::combat_defender_player;
        }

        if (player_id < MOX::NUM_PLAYERS) {
            int16_t attacker_bonus = CMBTFIR2::Get_Fleet_Commando_Bonus(player_id);
            int16_t defender_bonus = CMBTFIR2::Crew_Boarding_Combat_Bonus(ship_idx);
            defender_bonus += COMBINIT::Does_Combat_Ship_Have_Special(
                                  ship_idx, SPECIAL_SECURITY_STATIONS) *
                invasion::troop.bonus.security_stations;
            defender_bonus += CMBTFIR2::Get_Fleet_Commando_Bonus(ship->owner);
            defender_bonus += CMBTFIR2::Get_Fleet_Security_Bonus(ship->owner);

            s_invasion_side_info attacker_info;
            s_invasion_side_info defender_info;

            invasion::Get_Boarding_Info(
                player_id, 5, attacker_bonus, ship->owner, 5, defender_bonus, &attacker_info, &defender_info);

            strength_attacker = attacker_info.unit_strengths[1];
            strength_defender = defender_info.unit_strengths[1];
        }

        if (MOX::combat_total_ship_count > 1) {
            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                if (MOX::combat_data[i].unit_status == 0 && i != ship_idx) {
                    if (CMBTMOV1::Ship_Is_In_Stasis(i) == 0) {
                        int16_t range = Range_To_Ship_Squares(ship_idx, i);
                        strength_attacker += CMBTFIR2::Crew_Boarding_Combat_Bonus(i);

                        if (MOX::combat_data[i].owner == ship->owner) {
                            if (range <= BOARDING::board_range.normal) {
                                dist_friendly += 2;
                            } else if (range == BOARDING::board_range.normal * 2) {
                                dist_friendly += 1;
                            }
                        } else {
                            int8_t has_transporters = (int8_t)COMBINIT::Does_Combat_Ship_Have_Special(i, SPECIAL_TRANSPORTERS);
                            bool add_marines = false;

                            if (range <= BOARDING::board_range.normal) {
                                dist_enemy += 2;
                                if (has_transporters != 0 || ship->current_speed == 0) {
                                    add_marines = true;
                                }
                            } else if (range == BOARDING::board_range.normal * 2) {
                                dist_enemy += 1;
                                if (has_transporters != 0 || ship->current_speed == 0) {
                                    add_marines = true;
                                }
                            } else if (range < BOARDING::board_range.normal * 7) {
                                if (has_transporters != 0) {
                                    add_marines = true;
                                }
                            }

                            if (add_marines) {
                                marine_count_sum += (MOX::combat_data[i].marine_count >> 1);
                            }
                        }
                    }
                }
            }
        }

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            int16_t type = MOX::missile[i].type;
            if (type > 0) {
                if (TECHDATA::weapons[type].type != WEAPON_TYPE_TORPEDO) {
                    int16_t range = Range_To_Missile(ship_idx, i);
                    if (range < 3) {
                        int16_t owner = CMBTMIS::Missile_Owner(i);
                        if (owner == ship->owner) {
                            if (TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER) {
                                friendly_rack4 += MOX::missile[i].quantity;
                            } else {
                                friendly_other += MOX::missile[i].quantity;
                            }
                        } else {
                            if (TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER) {
                                enemy_rack4 += MOX::missile[i].quantity;
                            } else {
                                score += MOX::missile[i].quantity;
                            }
                        }
                    }
                }
            }
        }

        score += ((((dist_enemy * 2) - dist_friendly) * 2 - friendly_rack4 + enemy_rack4) * 5) - friendly_other;

        if (marine_count_sum > 0 && player_id < MOX::NUM_PLAYERS) {
            int16_t strength_diff = strength_attacker - strength_defender;
            uint16_t ship_marines = ship->marine_count;

            if (strength_diff >= 51) {
                marine_count_sum *= 10;
            } else if (strength_diff >= 40) {
                marine_count_sum *= 7;
            } else if (strength_diff >= 30) {
                marine_count_sum *= 4;
            } else if (strength_diff >= 20) {
                marine_count_sum *= 3;
            } else if (strength_diff >= 10) {
                marine_count_sum *= 2;
            } else if (strength_diff <= -50) {
                ship_marines *= 10;
            } else if (strength_diff <= -40) {
                ship_marines *= 7;
            } else if (strength_diff <= -30) {
                ship_marines *= 4;
            } else if (strength_diff <= -20) {
                ship_marines *= 3;
            } else if (strength_diff <= -10) {
                ship_marines *= 2;
            }

            if (ship_marines < marine_count_sum) {
                int32_t diff = marine_count_sum - ship_marines;
                if (MOX::player[player_id].traits[TRAIT_TELEPATHIC] != 0) {
                    score += diff * 10;
                } else {
                    score += diff * 5;
                }
            }
        }

        if (score > 0) {
            if ((int)game_random::Random(100) < score) {
                if (ship->current_speed == 0) {
                    ret_val = 1;
                }
            }
        }

        return ret_val;
    }

} // namespace CMBTAI