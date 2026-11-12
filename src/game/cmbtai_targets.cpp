#include "pch.h"

namespace CMBTAI {
    bool __cdecl Safe_To_Fire_Sphere_Weapon(int16_t source_ship_idx) {
        int32_t friendly_ship_score = 0;
        int32_t enemy_ship_score = 0;
        int32_t friendly_fighter_qty = 0;
        int32_t friendly_missile_qty = 0;
        int32_t enemy_fighter_qty = 0;
        int32_t enemy_missile_qty = 0;
        bool can_fire = true;

        if (MOX::combat_total_ship_count > 1) {
            for (int16_t target_ship_idx = 1; target_ship_idx < MOX::combat_total_ship_count; ++target_ship_idx) {
                if (MOX::combat_data[target_ship_idx].unit_status == 0 && target_ship_idx != source_ship_idx) {
                    if (!CMBTMOV1::Ship_Is_In_Stasis(target_ship_idx)) {
                        int16_t distance = Range_To_Ship(source_ship_idx, target_ship_idx);
                        if (MOX::combat_data[source_ship_idx].owner == MOX::combat_data[target_ship_idx].owner) {
                            if (distance < 2) {
                                friendly_ship_score += 2;
                            } else if (distance == 2) {
                                friendly_ship_score += 1;
                            }
                        } else {
                            if (distance < 2) {
                                enemy_ship_score += 2;
                            } else if (distance == 2) {
                                enemy_ship_score += 1;
                            }
                        }
                    }
                }
            }
        }

        for (int16_t missile_idx = 0; missile_idx < MAX_COMBAT_FLYERS; ++missile_idx) {
            int16_t weapon_type = MOX::missile[missile_idx].type;
            if (weapon_type > 0) {
                if (TECHDATA::weapons[weapon_type].type != WEAPON_TYPE_TORPEDO) {
                    int16_t distance = Range_To_Missile(source_ship_idx, missile_idx);
                    if (distance < 3) {
                        int16_t missile_owner = CMBTMIS::Missile_Owner(missile_idx);
                        int16_t quantity = MOX::missile[missile_idx].quantity;
                        if (MOX::combat_data[source_ship_idx].owner == missile_owner) {
                            if (TECHDATA::weapons[weapon_type].type == WEAPON_TYPE_FIGHTER) {
                                friendly_fighter_qty += quantity;
                            } else {
                                friendly_missile_qty += quantity;
                            }
                        } else {
                            if (TECHDATA::weapons[weapon_type].type == WEAPON_TYPE_FIGHTER) {
                                enemy_fighter_qty += quantity;
                            } else {
                                enemy_missile_qty += quantity;
                            }
                        }
                    }
                }
            }
        }

        int32_t total_score = (enemy_ship_score * 3 - friendly_ship_score * 2 + enemy_fighter_qty) * 5;
        total_score -= (friendly_fighter_qty * 8);
        total_score -= friendly_missile_qty;
        total_score += enemy_missile_qty;

        if (total_score < 0) {
            can_fire = false;
        }

        return can_fire;
    }

    int16_t __cdecl Is_Ship_Out_Of_Ammo(int16_t ship_idx) {
        int16_t out_of_ammo = 1;

        if (MOX::combat_data[ship_idx].officer_idx >= 0) {
            return 0;
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, 37 /* 0x25 */) != 0) {
            out_of_ammo = 0;
        } else if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, 38 /* 0x26 */) != 0) {
            out_of_ammo = 0;
        } else {
            for (int16_t slot_idx = 0; slot_idx < 8 && out_of_ammo == 1; ++slot_idx) {
                s_combat_weapon_slot& slot = MOX::combat_data[ship_idx].weapons[slot_idx];

                if (slot.weapon_id != 0 && slot.count > 0) {
                    int8_t weapon_type_class = TECHDATA::weapons[slot.weapon_id].type;

                    switch (weapon_type_class) {
                        case WEAPON_TYPE_BEAM:
                        case WEAPON_TYPE_TORPEDO:
                            out_of_ammo = 0;
                            break;

                        case WEAPON_TYPE_MISSILE:
                            if (slot.ammo > 0) {
                                out_of_ammo = 0;
                            }
                            break;

                        case WEAPON_TYPE_BOMB:
                            if (slot.ammo > 0 && MOX::combat_data[ship_idx].owner == MOX::combat_attacker_player) {
                                out_of_ammo = 0;
                            }
                            break;

                        case WEAPON_TYPE_FIGHTER:
                            if (slot.ammo > 0) {
                                out_of_ammo = 0;
                            } else {
                                if (CMBTAI::Check_For_Launched_Fighters(ship_idx) == 1) {
                                    out_of_ammo = 0;
                                }
                            }
                            break;

                        case WEAPON_TYPE_SPECIAL:
                            if (slot.weapon_id == WEAPON_ANTI_MISSILE_ROCKET) {
                                if (slot.ammo > 0) {
                                    out_of_ammo = 0;
                                }
                            } else {
                                out_of_ammo = 0;
                            }
                            break;

                        default:
                            break;
                    }
                }
            }
        }

        return out_of_ammo;
    }

    /**
     * @brief Checks if there are any active fighter-type missiles/units launched by the specified unit.
     *
     * @param source_unit_idx The index of the ship or unit that launched the fighters.
     * @return 1 if fighters from this source are found, 0 otherwise.
     */
    int16_t __cdecl Check_For_Launched_Fighters(int16_t source_unit_idx) {
        int16_t found = 0;

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            // Check if we found it in a previous iteration
            if (found != 0) {
                break;
            }

            // Check if the missile/fighter belongs to the specified source unit
            if (MOX::missile[i].source_unit_idx == source_unit_idx) {
                if (TECHDATA::weapons[MOX::missile[i].type].type == WEAPON_TYPE_FIGHTER) {
                    found = 1;
                }
            }
        }

        return found;
    }

    int16_t __cdecl Get_Current_Missile_Target(int16_t unit_idx, int32_t* value_out) {
        int32_t best_value = -5000;
        int16_t best_missile_idx = -1;

        MOX::combat_data[unit_idx].target_type_flag = TARGET_TYPE_SHIP;

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile* missile = &MOX::missile[i];

            if (missile->type > 0 && missile->quantity > 0) {
                if (CMBTAI::Range_To_Missile(unit_idx, i) >= 30) {
                    continue;
                }
                if (CMBTMIS::Missile_Owner(i) != MOX::combat_data[unit_idx].owner) {
                    if (CMBTAI::Will_Missile_Do_Damage(unit_idx, i)) {
                        int8_t weapon_type_class = TECHDATA::weapons[missile->type].type;
                        if (weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_FIGHTER) {
                            int32_t current_value = CMBTAI::Target_Missile_Value(unit_idx, i, 1);
                            if (best_value < current_value) {
                                best_missile_idx = i;
                                best_value = current_value;
                            }
                        }
                    }
                }
            }
        }

        if (best_missile_idx == -1) {
            *value_out = -20000;
        } else {
            *value_out = best_value;
        }

        return best_missile_idx;
    }

    int32_t __cdecl Get_Total_Hits(int16_t ship_idx) {
        int32_t total_hits = 0;

        if (ship_idx != 0) {
            int32_t internal_space = CMBTFIR2::Total_Internal_Space(ship_idx);
            s_combat_data* ship = &MOX::combat_data[ship_idx];

            total_hits = ship->armor_remaining;
            total_hits += internal_space;
            total_hits += ship->structure_max;
            total_hits -= ship->structure_damage;
            total_hits += COMBINIT::Calc_Combat_Shield_Max(ship_idx);
        } else {
            int16_t owner = MOX::combat_data[0].owner;
            int16_t best_armor_idx = INITSHIP::Best_Armor(owner);
            int32_t defense_value = CMBTFIRE::Ground_Defense_Structure(
                TECHDATA::armor[best_armor_idx].ships_bonus);
            int16_t planet_defenses = CMBTFIRE::Does_Combat_Planet_Have_Defenses();

            for (int16_t i = 0; i < 4; ++i) {
                if (planet_defenses & (1 << i)) {
                    total_hits += defense_value;
                }
            }
        }

        return total_hits;
    }

    int16_t __cdecl Get_Effective_Weapon_Damage(int16_t unit_idx, int16_t slot_idx) {
        s_combat_data* unit = &MOX::combat_data[unit_idx];
        s_combat_weapon_slot& slot = unit->weapons[slot_idx];

        int16_t weapon_id = slot.weapon_id;
        uint16_t specials = slot.specials;

        // Note: The following damage calculation is performed in the original binary,
        // but the result in EAX is immediately overwritten by the return value.
        // We preserve it here to maintain semantic equivalence with the assembly.
        int16_t damage = TECHDATA::weapons[weapon_id].max_damage;

        if (specials & 0x4) { /* Point Defense */
            damage = (damage + 1) / 2;
        }

        if (specials & 0x2) { /* Heavy Mount */
            damage = (damage * 3) / 2;
        }

        // MOVZX AX, byte ptr [ECX + 0x54]
        return slot.count;
    }

    int32_t __cdecl Target_Missile_Value(int16_t unit_idx, int32_t missile_idx, int16_t range_sensitive) {
        int16_t range;
        int16_t m_idx = (int16_t)missile_idx;

        if (range_sensitive > 0) {
            range = Range_To_Missile(unit_idx, m_idx);
            if (range < 1) {
                range = 1;
            }
        } else {
            range = 0;
        }

        int16_t total_expected_pd_damage = 0;
        for (int16_t weapon_idx = 0; weapon_idx < 8; ++weapon_idx) {
            total_expected_pd_damage += Expected_Weapon_Damage(unit_idx, weapon_idx, m_idx, 1, range);
        }

        s_missile* missile = &MOX::missile[m_idx];
        int16_t hits_to_kill = (int16_t)CMBTMIS::Get_Missile_Hits(
            missile->type,
            missile->owner,
            missile->specials);

        int16_t effective_missile_hits = total_expected_pd_damage / hits_to_kill;
        if (missile->quantity < effective_missile_hits) {
            effective_missile_hits = missile->quantity;
        }

        int16_t weapon_damage;
        int16_t missile_type = missile->type;
        if (TECHDATA::weapons[missile_type].type == WEAPON_TYPE_FIGHTER) {
            // ORACLE: The original indexes fighter loadouts by the owner evaluating the threat,
            // but owners above 8 read beyond these nine-element tables into unrelated globals.
            int16_t unit_owner = MOX::combat_data[unit_idx].owner;
            const bool has_fighter_loadout = unit_owner >= 0 && unit_owner <= MAX_PLAYERS;
            switch (missile_type) {
                case WEAPON_ASSAULT_SHUTTLE:
                    weapon_damage = 100;
                    break;
                case WEAPON_HEAVY_FIGHTER:
                case WEAPON_BOMBER:
                    weapon_damage = has_fighter_loadout
                        ? TECHDATA::weapons[MOX::fighter_bomb_type[unit_owner]].max_damage
                        : 0;
                    break;
                case WEAPON_INTERCEPTOR:
                    weapon_damage = has_fighter_loadout
                        ? TECHDATA::weapons[MOX::fighter_beam_type[unit_owner]].max_damage
                        : 0;
                    break;
                default:
                    weapon_damage = 0;
                    break;
            }
        } else {
            weapon_damage = TECHDATA::weapons[missile_type].max_damage;
        }

        int16_t shield_blocked;
        if (unit_idx == 0) {
            shield_blocked = MOX::combat_data[0].shield_arc_current[0];
        } else {
            shield_blocked = TECHDATA::shields[MOX::combat_data[unit_idx].shield_type].shield_class;
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(unit_idx, SPECIAL_DAMPER_FIELD)) {
            effective_missile_hits =
                (int16_t)CMBTFIRE::Damper_Field_Remaining_Damage(
                    effective_missile_hits);
        }

        int32_t final_value = (int16_t)(uint16_t)((weapon_damage - shield_blocked) * effective_missile_hits);

        if (TECHDATA::weapons[missile->type].type != WEAPON_TYPE_FIGHTER) {
            final_value /= 2;
        }

        if (final_value < 1) {
            final_value = -10000;
        }

        if (!Will_Missile_Do_Damage(missile->target_unit_idx, m_idx)) {
            final_value = -10000;
        }

        return final_value;
    }
} // namespace CMBTAI
