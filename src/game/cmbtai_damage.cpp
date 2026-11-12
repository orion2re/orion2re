#include "pch.h"

namespace CMBTAI {
    namespace {
        int32_t Expected_Damage_After_Absorber(
            int16_t target_idx,
            int32_t damage) {
            if (COMBINIT::Does_Combat_Ship_Have_Special(
                    target_idx, SPECIAL_ENERGY_ABSORBER) != 0) {
                damage -= CMBTFIRE::Energy_Absorber_Blocked_Damage(damage);
            }
            return damage;
        }

        int16_t Expected_Shield_Block(int16_t target_idx) {
            if (target_idx == 0) {
                return MOX::combat_data[0].shield_arc_max;
            }

            const bool has_hard_shields =
                COMBINIT::Does_Combat_Ship_Have_Special(
                    target_idx, SPECIAL_HARD_SHIELDS) != 0;
            const s_combat_data* target = &MOX::combat_data[target_idx];
            if (target->shield_current_hits <= 0 ||
                (MOX::in_nebula_flag != 0 && !has_hard_shields)) {
                return 0;
            }

            int16_t blocked = TECHDATA::shields[target->shield_type].shield_class;
            if (has_hard_shields) {
                blocked += TECHDATA::hard_shields_damage_reduction;
            }
            return blocked;
        }

        bool Expected_Shield_Piercing(
            int16_t target_idx,
            int16_t weapon_id,
            uint16_t specials) {
            if (COMBINIT::Does_Combat_Ship_Have_Special(
                    target_idx, SPECIAL_HARD_SHIELDS) != 0) {
                return false;
            }

            return (TECHDATA::weapons[weapon_id].natural_mods &
                       WEAPON_NATURAL_FLAG_IGNORES_SHIELDS) != 0 ||
                (specials & WEAPON_MOD_FLAG_SHIELD_PIERCING) != 0;
        }

        int32_t Expected_Special_Hit_Damage(
            int16_t target_idx,
            int16_t weapon_id,
            uint16_t specials,
            int32_t damage) {
            damage = Expected_Damage_After_Absorber(target_idx, damage);
            if (!Expected_Shield_Piercing(target_idx, weapon_id, specials)) {
                damage -= Expected_Shield_Block(target_idx);
            }
            // 1.50 retains negative special-weapon estimates for the caller's AI scoring.
            return damage;
        }

        int32_t Expected_Damage_After_Cloak(
            int32_t damage,
            bool actively_cloaked) {
            if (!actively_cloaked ||
                CMBTMIS::missile_defense.cloaking_device_missile_defense != 0) {
                return damage;
            }

            const int32_t retained_percent =
                100 - CMBTMIS::missile_defense.cloaking_device_missile_miss_percent;
            return (int32_t)((int64_t)damage * retained_percent / 100);
        }

        bool Missile_Can_Damage_Target(
            int16_t target_idx,
            const s_missile* missile) {
            const int16_t missile_weapon_id = missile->type;
            switch (missile_weapon_id) {
                case WEAPON_ANTI_MATTER_TORP:
                case WEAPON_PROTON_TORPEDO:
                case WEAPON_PLASMA_TORPEDO:
                    return false;
                default:
                    break;
            }
            if (missile_weapon_id == WEAPON_ASSAULT_SHUTTLE) {
                return true;
            }

            const bool is_planet = target_idx == 0;
            const bool planet_has_defenses =
                CMBTFIRE::Does_Combat_Planet_Have_Defenses() != 0;
            if (is_planet && !planet_has_defenses) {
                return false;
            }

            const bool has_hard_shields =
                COMBINIT::Does_Combat_Ship_Have_Special(
                    target_idx, SPECIAL_HARD_SHIELDS) != 0;
            const bool has_damper_field =
                COMBINIT::Does_Combat_Ship_Have_Special(
                    target_idx, SPECIAL_DAMPER_FIELD) != 0;

            int32_t shield = 0;
            if (is_planet) {
                shield = planet_has_defenses && !has_damper_field
                    ? MOX::combat_data[0].shield_arc_current[0]
                    : MOX::combat_data[0].shield_arc_current[1];
            } else {
                const s_combat_data* target = &MOX::combat_data[target_idx];
                const bool shields_online =
                    MOX::in_nebula_flag == 0 || has_hard_shields;
                if (target->shield_current_hits > 0 && shields_online) {
                    shield = TECHDATA::shields[target->shield_type].shield_class;
                    if (has_hard_shields) {
                        shield += TECHDATA::hard_shields_damage_reduction;
                    }
                }
            }

            const int16_t owner = missile->owner;
            const bool has_fighter_loadout =
                owner >= 0 && owner <= MAX_PLAYERS;
            int16_t weapon_ids[3] = {
                missile_weapon_id,
                (int16_t)(has_fighter_loadout
                        ? MOX::fighter_beam_type[owner]
                        : -1),
                (int16_t)(has_fighter_loadout
                        ? MOX::fighter_bomb_type[owner]
                        : -1),
            };
            enum e_missile_payload {
                MISSILE_PAYLOAD_MISSILE,
                MISSILE_PAYLOAD_BEAM,
                MISSILE_PAYLOAD_BOMB,
                MISSILE_PAYLOAD_COUNT,
            };

            switch (missile_weapon_id) {
                case WEAPON_INTERCEPTOR:
                    weapon_ids[MISSILE_PAYLOAD_MISSILE] = -1;
                    weapon_ids[MISSILE_PAYLOAD_BOMB] = -1;
                    break;
                case WEAPON_BOMBER:
                    weapon_ids[MISSILE_PAYLOAD_MISSILE] = -1;
                    weapon_ids[MISSILE_PAYLOAD_BEAM] = -1;
                    break;
                case WEAPON_HEAVY_FIGHTER:
                    weapon_ids[MISSILE_PAYLOAD_MISSILE] = -1;
                    break;
                default:
                    weapon_ids[MISSILE_PAYLOAD_BEAM] = -1;
                    weapon_ids[MISSILE_PAYLOAD_BOMB] = -1;
                    break;
            }

            for (int payload = 0; payload < MISSILE_PAYLOAD_COUNT; ++payload) {
                const int16_t weapon_id = weapon_ids[payload];
                if (weapon_id < 0) {
                    continue;
                }

                const s_tech_weapon_data* weapon = &TECHDATA::weapons[weapon_id];
                int32_t damage = weapon->max_damage;
                if (payload == MISSILE_PAYLOAD_BEAM) {
                    if (CMBTFIRE::fighters_use_point_defense != 0) {
                        const uint32_t modifier =
                            CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].pd;
                        damage = (damage * modifier + 50) / 100;
                        if (damage < 1) {
                            damage = 1;
                        }
                    }
                    if (CMBTFIRE::fighter_beam_no_plus_one == 0) {
                        damage++;
                    }
                    damage = damage * MOX::ordnance_bonus[owner] / 100;
                    if (is_planet) {
                        damage /= 2;
                    }
                } else {
                    int32_t ordnance_bonus = MOX::ordnance_bonus[owner];
                    if (payload == MISSILE_PAYLOAD_MISSILE &&
                        (missile->specials & WEAPON_MOD_FLAG_OVERLOADED) != 0) {
                        ordnance_bonus += 50;
                    }
                    damage = damage * ordnance_bonus / 100;
                }

                damage = Expected_Damage_After_Absorber(target_idx, damage);
                if (has_damper_field) {
                    damage = CMBTFIRE::Damper_Field_Remaining_Damage(damage);
                }

                const bool shield_piercing =
                    (weapon->natural_mods &
                        WEAPON_NATURAL_FLAG_IGNORES_SHIELDS) != 0 &&
                    !has_hard_shields;
                if (damage > (shield_piercing ? 0 : shield)) {
                    return true;
                }
            }

            return false;
        }
    } // namespace

    int16_t __cdecl Will_Missile_Do_Damage(int16_t target_idx, int16_t missile_idx) {
        return Missile_Can_Damage_Target(target_idx, &MOX::missile[missile_idx]);
    }
    int32_t __cdecl Expected_Beam_Bomb_Damage(int16_t ship_idx, int16_t weapon_slot_idx, int16_t target_idx, int16_t count, int16_t distance, int16_t target_type_flag) {
        int32_t range_penalty = 0;
        int32_t damage_bonus_pct = 0;
        int64_t attack_defense_delta;
        int32_t points_blocked;
        int16_t use_planetary_shield_adjustment = 0;

        uint16_t specials = MOX::combat_data[ship_idx].weapons[weapon_slot_idx].specials;
        int16_t weapon_id = MOX::combat_data[ship_idx].weapons[weapon_slot_idx].weapon_id;
        const s_tech_weapon_data& weapon = TECHDATA::weapons[weapon_id];

        if (target_type_flag == TARGET_TYPE_SHIP &&
            target_idx == 0 &&
            weapon.type == WEAPON_TYPE_BEAM &&
            (weapon.natural_mods &
                WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE) != 0) {
            return 0;
        }

        points_blocked = target_type_flag == TARGET_TYPE_SHIP
            ? Expected_Shield_Block(target_idx)
            : 0;
        if (target_type_flag == TARGET_TYPE_SHIP) {
            use_planetary_shield_adjustment = target_idx == 0;
            if (Expected_Shield_Piercing(target_idx, weapon_id, specials)) {
                points_blocked = 0;
            }
        }

        int32_t dist_modifier;
        if (weapon.type == WEAPON_TYPE_BEAM) {
            CMBTFIRE::Get_Beam_Weapon_Modifiers(ship_idx, distance, specials, weapon_id, &range_penalty, &damage_bonus_pct);
            dist_modifier = 40 - range_penalty;
            if (dist_modifier > 95) {
                dist_modifier = 95;
            }
            attack_defense_delta = MOX::combat_data[ship_idx].tac_val_attack;
            if (target_type_flag == TARGET_TYPE_SHIP) {
                attack_defense_delta -= MOX::combat_data[target_idx].tac_val_defense;
            } else {
                int16_t source_unit_idx = MOX::missile[target_idx].source_unit_idx;
                int16_t owner = MOX::combat_data[source_unit_idx].owner;
                attack_defense_delta -= CMBTMIS::Missile_Dcv(MOX::missile[target_idx].type, owner);
            }
        } else {
            damage_bonus_pct = 100;
            dist_modifier = 0;
            range_penalty = 0;
            attack_defense_delta = 0;
        }

        int32_t ordnance_bonus = MOX::ordnance_bonus[MOX::combat_data[ship_idx].owner] + damage_bonus_pct - 100;

        int64_t min_damage_calc =
            ((int64_t)weapon.min_damage *
                    ordnance_bonus +
                50) /
            100;
        if (min_damage_calc < 1) {
            min_damage_calc = 1;
        }
        int32_t min_damage = (int32_t)min_damage_calc;

        int64_t max_damage_calc =
            ((int64_t)weapon.max_damage *
                    ordnance_bonus +
                50) /
            100;
        if (max_damage_calc <= min_damage) {
            max_damage_calc = min_damage;
        }
        int32_t max_damage = (int32_t)max_damage_calc;

        if (target_idx != 0) {
            min_damage = Expected_Damage_After_Absorber(target_idx, min_damage);
            max_damage = Expected_Damage_After_Absorber(target_idx, max_damage);
        }

        if (use_planetary_shield_adjustment != 0) {
            int16_t shield_id = INITSHIP::Best_Ship_Shield(MOX::combat_defender_player);
            if (weapon.type == WEAPON_TYPE_BEAM) {
                min_damage /= 2;
                max_damage /= 2;
            }
            if (shield_id != SPECIAL_DAMPER_FIELD) {
                min_damage -= TECHDATA::shields[shield_id].shield_class;
                max_damage -= TECHDATA::shields[shield_id].shield_class;
            }
            if (min_damage < 0) {
                min_damage = 0;
            }
            if (max_damage < 0) {
                max_damage = 0;
            }
        }

        int32_t actual_dmg = GEO::Actual_Expected_Damage(min_damage, max_damage, dist_modifier, attack_defense_delta, points_blocked);
        return count * actual_dmg;
    }

    int16_t __cdecl Expected_Weapon_Damage(int16_t source_idx, int16_t weapon_slot, int16_t target_idx, int16_t target_type, int16_t range) {
        int32_t expected_damage = 0;

        int16_t weapon_id = MOX::combat_data[source_idx].weapons[weapon_slot].weapon_id;
        uint16_t specials = MOX::combat_data[source_idx].weapons[weapon_slot].specials;
        int16_t count = MOX::combat_data[source_idx].weapons[weapon_slot].count;

        if (weapon_id > 0 && CMBTFIRE::Target_Is_Legal_For_Weapon(weapon_id, specials, target_idx, target_type)) {
            if (range == 0) {
                range = 1;
            } else {
                short bearing;
                if (target_type == TARGET_TYPE_SHIP) {
                    bearing = CMBTDRW1::Relative_Bearing(source_idx, target_idx);
                } else {
                    bearing = CMBTDRW1::Relative_Bearing_XY(source_idx, MOX::missile[target_idx].x, MOX::missile[target_idx].y);
                }
                int16_t arc = (int16_t)CMBTDRW1::Get_Weapon_Arc(bearing);
                count = 0;

                if (COMBAT1::Weapon_Is_On(source_idx, weapon_slot)) {
                    if ((arc & (int16_t)MOX::combat_data[source_idx].weapons[weapon_slot].arc) != 0) {
                        uint8_t weapon_type_class = TECHDATA::weapons[weapon_id].type;
                        bool can_fire = false;

                        if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
                            if (MOX::combat_data[source_idx].weapons[weapon_slot].ammo == 0) {
                                can_fire = true;
                            }
                        } else {
                            if (MOX::combat_data[source_idx].weapons[weapon_slot].ammo != 0) {
                                can_fire = true;
                            }
                        }

                        if (can_fire && CMBTFIRE::Weapon_In_Range(weapon_slot, source_idx, target_idx, target_type, range)) {
                            count = MOX::combat_data[source_idx].weapons[weapon_slot].shots_left;
                        }
                    }
                }
            }

            if (count > 0) {
                uint8_t weapon_type_class = TECHDATA::weapons[weapon_id].type;

                if (weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_FIGHTER || weapon_type_class == WEAPON_TYPE_TORPEDO) {
                    expected_damage = Expected_Missile_Damage(target_idx, weapon_id, count, specials, MOX::combat_data[source_idx].owner);
                } else if (weapon_type_class == WEAPON_TYPE_BEAM || weapon_type_class == WEAPON_TYPE_BOMB) {
                    expected_damage = Expected_Beam_Bomb_Damage(source_idx, weapon_slot, target_idx, count, range, target_type);
                } else if (weapon_type_class == WEAPON_TYPE_SPECIAL) {
                    int16_t min_dam = TECHDATA::weapons[weapon_id].min_damage;
                    int16_t max_dam = TECHDATA::weapons[weapon_id].max_damage;
                    int32_t avg_damage_sum = ((int32_t)min_dam) + ((int32_t)max_dam);
                    int32_t avg_damage = (avg_damage_sum - (avg_damage_sum >> 31)) >> 1;

                    switch (weapon_id) {
                        case WEAPON_SPATIAL_COMPRESSOR:
                        case WEAPON_GYRO_DESTABILIZER: {
                            int size_mult = MOX::combat_data[target_idx].size_class + 1;
                            expected_damage = avg_damage * size_mult * count;
                            break;
                        }
                        case WEAPON_STASIS_FIELD:
                        case WEAPON_BLACK_HOLE_GENERATOR:
                            expected_damage = 1000;
                            break;
                        case WEAPON_ANTI_MISSILE_ROCKET:
                            if (target_type != TARGET_TYPE_SHIP) {
                                s_missile* m = &MOX::missile[target_idx];
                                int hits = (int16_t)CMBTMIS::Get_Missile_Hits(
                                    m->type, m->owner, m->specials);
                                expected_damage = hits * (((8 - range) * count * 5 + 25) * 2 / 100);
                            }
                            break;
                        case WEAPON_PLASMA_WEB:
                            expected_damage = Expected_Special_Hit_Damage(
                                                  target_idx, weapon_id, specials, avg_damage) *
                                count * 4;
                            break;
                        case WEAPON_STELLAR_CONVERTER:
                        case WEAPON_CAUSTIC_SLIME: {
                            int blocked = TECHDATA::shields[MOX::combat_data[target_idx].shield_type].shield_class;
                            expected_damage = (avg_damage - blocked) * count * 4;
                            break;
                        }
                        case WEAPON_PULSAR: {
                            int size_mult = MOX::combat_data[target_idx].size_class + 1;
                            expected_damage = Expected_Special_Hit_Damage(
                                                  target_idx, weapon_id, specials, avg_damage) *
                                size_mult * count;
                            break;
                        }
                        case WEAPON_TRACTOR_BEAM:
                            if (MOX::combat_data[target_idx].current_speed > 0) {
                                expected_damage = 50;
                            }
                            break;
                        case WEAPON_PLASMA_FLUX: {
                            int size_mult = MOX::combat_data[target_idx].size_class + 1;
                            expected_damage = Expected_Special_Hit_Damage(
                                                  target_idx, weapon_id, specials, avg_damage) *
                                size_mult * count * 4;
                            break;
                        }
                    }
                }
            }
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(target_idx, SPECIAL_DAMPER_FIELD)) {
            expected_damage =
                CMBTFIRE::Damper_Field_Remaining_Damage(expected_damage);
        }

        uint8_t weapon_type_class = TECHDATA::weapons[weapon_id].type;
        if (weapon_type_class == WEAPON_TYPE_BEAM || weapon_type_class == WEAPON_TYPE_BOMB) {
            expected_damage = (int32_t)(((uint32_t)(expected_damage + 99)) / 100);
        }

        return (int16_t)expected_damage;
    }

    int16_t __cdecl Expected_Missile_Damage(int16_t target_ship_idx, int16_t weapon_id, int16_t weapon_count, uint16_t weapon_specials, int16_t attacker_player_idx) {
        int16_t shield_reduction = 0;
        int32_t damage;
        int32_t result_damage = 0;

        if (target_ship_idx == 0) {
            if (CMBTFIRE::Does_Combat_Planet_Have_Defenses() != 0) {
                shield_reduction = MOX::combat_data[0].shield_arc_current[0];
            } else {
                shield_reduction = MOX::combat_data[0].shield_arc_current[1];
            }
        } else {
            s_combat_data* target_ship = &MOX::combat_data[target_ship_idx];
            const bool has_hard_shields =
                COMBINIT::Does_Combat_Ship_Have_Special(
                    target_ship_idx, SPECIAL_HARD_SHIELDS) != 0;
            if (target_ship->shield_current_hits > 0 &&
                (MOX::in_nebula_flag == 0 || has_hard_shields)) {
                shield_reduction = TECHDATA::shields[target_ship->shield_type].shield_class;
                if (has_hard_shields) {
                    shield_reduction += TECHDATA::hard_shields_damage_reduction;
                }
            }
        }

        s_tech_weapon_data* weapon = &TECHDATA::weapons[weapon_id];
        if (weapon->type == WEAPON_TYPE_FIGHTER) {
            int16_t base_damage = 0;
            switch (weapon_id) {
                case WEAPON_ASSAULT_SHUTTLE:
                    base_damage = 100;
                    break;
                case WEAPON_HEAVY_FIGHTER:
                case WEAPON_BOMBER:
                    base_damage = TECHDATA::weapons[MOX::fighter_bomb_type[attacker_player_idx]].max_damage;
                    break;
                case WEAPON_INTERCEPTOR:
                    base_damage = TECHDATA::weapons[MOX::fighter_beam_type[attacker_player_idx]].max_damage;
                    break;
                default:
                    break;
            }

            damage = (MOX::ordnance_bonus[attacker_player_idx] * base_damage) / 100;
            // 1.50 applies absorber and Shield Piercing corrections only to missiles.
            damage -= shield_reduction;
            int32_t volley_damage = damage * weapon_count;
            result_damage = volley_damage + volley_damage;
        } else {
            int16_t base_damage = weapon->max_damage;

            if ((weapon_specials & WEAPON_MOD_FLAG_OVERLOADED) != 0) {
                base_damage = (int16_t)((base_damage * 3) / 2);
            }

            damage = (MOX::ordnance_bonus[attacker_player_idx] * base_damage) / 100;

            int16_t evade = MOX::combat_data[target_ship_idx].tac_val_evade;
            if (attacker_player_idx < MAX_PLAYERS) {
                s_player* attacker = &MOX::player[attacker_player_idx];
                if (attacker->tech_applications[TECH_APP_SENSORS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    evade -= CMBTMIS::missile_defense.sensors;
                } else if (attacker->tech_applications[TECH_APP_NEUTRON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    evade -= CMBTMIS::missile_defense.neutron_scanner;
                } else if (attacker->tech_applications[TECH_APP_TACHYON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    evade -= CMBTMIS::missile_defense.tachyon_scanner;
                }
            } else {
                evade -= CMBTMIS::missile_defense.neutron_scanner;
            }

            if (weapon_id == WEAPON_DRAGON_BREATH) {
                evade = 0;
            }

            if (evade < 0) {
                evade = 0;
            } else if ((weapon_specials & WEAPON_MOD_FLAG_ECCM) != 0) {
                evade /= 2;
            }

            if ((weapon_id == WEAPON_PLASMA_TORPEDO || weapon_id == WEAPON_DRAGON_BREATH) && ((weapon_specials & WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION) == 0)) {
                damage /= 2;
            }

            if (target_ship_idx == 0 && weapon->type == WEAPON_TYPE_TORPEDO) {
                damage /= 2;
            }

            damage = Expected_Damage_After_Absorber(
                target_ship_idx, damage);
            if (!Expected_Shield_Piercing(
                    target_ship_idx, weapon_id, weapon_specials)) {
                damage -= shield_reduction;
            }
            if ((weapon_specials & WEAPON_MOD_FLAG_MIRV) != 0) {
                damage *= 4;
            }
            // 1.50 prediction always applies Enveloping, including against planets.
            if ((weapon_specials & WEAPON_MOD_FLAG_ENVELOPING) != 0) {
                damage *= 4;
            }

            int32_t volley_damage = damage * weapon_count;
            result_damage = ((100 - evade) * volley_damage) / 100;

            result_damage = Expected_Damage_After_Cloak(
                result_damage,
                MOX::combat_data[target_ship_idx].special_status_flag == 1);
        }

        if (result_damage < 1) {
            result_damage = 0;
        }

        return result_damage;
    }
} // namespace CMBTAI
