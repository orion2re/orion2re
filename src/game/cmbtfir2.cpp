#include "pch.h"

namespace CMBTFIR2 {
    namespace {
        void Refresh_Shield_Capacity(int16_t ship_idx) {
            s_combat_data* ship = &MOX::combat_data[ship_idx];
            int16_t max_shield = COMBINIT::Calc_Combat_Shield_Max(ship_idx);
            ship->shield_arc_max = max_shield;

            for (int i = 0; i < 4; ++i) {
                if (ship->shield_arc_current[i] > max_shield) {
                    ship->shield_arc_current[i] = max_shield;
                }
            }
        }

    } // namespace

    s_artemis_system_net artemis_system_net = {
        .random_total_damage = 401,
        .minimum_total_damage = 149,
        .counter = 20,
        .single_hit_damage = 20,
        .inherent_modifications = 0,
        .trigger_chance = {20, 30, 40, 50, 80, 100},
    };
    s_combat_bonus combat_bonus = {
        .battle_scanner_beam_offense = 50,
        .scout_lab_fleet_beam_offense = 20,
        .starbase_fleet_beam_offense = 0,
        .battlestation_fleet_beam_offense = 10,
        .starfortress_fleet_beam_offense = 20,
        .inertial_stabilizer_beam_defense = 50,
        .inertial_nullifier_beam_defense = 100,
        .stealth_field_beam_defense = 0,
        .cloaking_device_beam_defense = 80,
        .phasing_cloak_beam_defense = 80,
        .phasing_cloak_effective_turns = 10,
        .displacement_device_beam_miss_percent = 30,
        .displacement_device_affects_stellar_converter = 0,
        .reflection_field_strength = 15,
    };
    s_quantum_detonator quantum_detonator = {6};
    s_defensive_fire_config defensive_fire = {
        .initiative_enabled = 0,
        .maximum_range = 8,
        .alternate_enabled = 0,
        .alternate_chance = {100, 95, 90, 80, 70, 50, 30, 10},
    };

    int16_t __cdecl Helmsman_Bonus(int16_t leader_idx) {
        int16_t bonus = 0;
        uint32_t special_skills = MOX::leaders[leader_idx].special_skills;

        if (special_skills & LEADER_SHIP_SKILL_HELMSMAN) {
            int16_t level = OFFICER::Owned_Officer_Level(leader_idx);
            return (int16_t)((level + 1) * 5);
        }

        if (special_skills & LEADER_SHIP_SKILL_HELMSMAN2) {
            int16_t level = OFFICER::Owned_Officer_Level(leader_idx);
            bonus = (int16_t)(((level * 3 + 3) * 5) / 2);
        }

        return bonus;
    }

    int16_t __cdecl Weaponry_Bonus(int16_t leader_idx) {
        int16_t bonus = 0;

        if ((MOX::leaders[leader_idx].special_skills & LEADER_SHIP_SKILL_WEAPONRY) != 0) {
            bonus = OFFICER::Owned_Officer_Level(leader_idx);
            return (int16_t)((bonus + 1) * 5);
        }

        if ((MOX::leaders[leader_idx].special_skills & LEADER_SHIP_SKILL_WEAPONRY2) != 0) {
            bonus = OFFICER::Owned_Officer_Level(leader_idx);
            bonus = (int16_t)(((bonus * 3 + 3) * 5) / 2);
        }

        return bonus;
    }

    int16_t __cdecl Engineer_Bonus(int16_t leader_idx) {
        int16_t bonus = 0;

        if ((MOX::leaders[leader_idx].special_skills & LEADER_SHIP_SKILL_ENGINEER) != 0) {
            bonus = OFFICER::Owned_Officer_Level(leader_idx);
            return bonus * 2 + 2;
        }

        if ((MOX::leaders[leader_idx].special_skills & LEADER_SHIP_SKILL_ENGINEER2) != 0) {
            bonus = OFFICER::Owned_Officer_Level(leader_idx);
            bonus = (bonus + 1) * 3;
        }

        return bonus;
    }

    int16_t __cdecl Fighter_Pilot_Bonus(int16_t leader_idx) {
        int16_t bonus = 0;
        uint32_t special_skills = MOX::leaders[leader_idx].special_skills;

        if (special_skills & LEADER_SHIP_SKILL_FIGHTER_PILOT) {
            int16_t level = OFFICER::Owned_Officer_Level(leader_idx);
            return (level + 1) * 5;
        }

        if (special_skills & LEADER_SHIP_SKILL_FIGHTER_PILOT2) {
            int16_t level = OFFICER::Owned_Officer_Level(leader_idx);
            bonus = (int16_t)(((level * 3 + 3) * 5) / 2);
        }

        return bonus;
    }

    /**
     * @brief Calculates the Galactic Lore bonus for a leader.
     * @param leader_idx Index of the leader in the global leaders array.
     * @return The calculated bonus value.
     */
    int16_t __cdecl Galactic_Lore_Bonus(int16_t leader_idx) {
        int16_t bonus = 0;
        uint32_t special_skills = MOX::leaders[leader_idx].special_skills;

        if (special_skills & LEADER_SHIP_SKILL_GALACTIC_LORE) {
            int16_t level = OFFICER::Owned_Officer_Level(leader_idx);
            return (int16_t)((level + 1) * 5);
        }

        if (special_skills & LEADER_SHIP_SKILL_GALACTIC_LORE2) {
            int32_t level = OFFICER::Owned_Officer_Level(leader_idx);
            bonus = (int16_t)(((level * 3 + 3) * 5) / 2);
        }

        return bonus;
    }

    void __cdecl Set_Combat_Ordnance_Values() {
        int16_t attacker_idx = MOX::combat_attacker_player;
        int16_t attacker_bonus = CMBTFIR2::Get_Fleet_Ordnance_Bonus(attacker_idx) + 100;

        int16_t defender_idx = MOX::combat_defender_player;
        MOX::ordnance_bonus[attacker_idx] = attacker_bonus;

        int16_t defender_bonus = CMBTFIR2::Get_Fleet_Ordnance_Bonus(defender_idx) + 100;
        MOX::ordnance_bonus[defender_idx] = defender_bonus;
    }

    int16_t __cdecl Crew_Defense_Bonus(int16_t combat_idx) {
        int8_t quality = MOX::combat_data[combat_idx].crew_quality;
        return MOX::crew_data[quality].defense;
    }

    int16_t __cdecl Fleet_Defensive_Bonus(int16_t player_idx, int16_t mode, int16_t* bonus_val_ptr) {
        int16_t max_helmsman_bonus = 0;

        if (mode == 1) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                s_combat_data* ship = &MOX::combat_data[i];
                if ((int16_t)ship->owner == player_idx && ship->is_captured == 0 && ship->unit_status == 0) {
                    if (!CMBTMOV1::Ship_Is_In_Stasis(i) &&
                        COMBINIT::Does_Combat_Ship_Have_Special(i, SPECIAL_WIDE_AREA_JAMMER)) {
                        *bonus_val_ptr = CMBTMIS::missile_defense.wide_area_jammer_fleet_missile_defense;
                    }
                    if (ship->officer_idx > -1) {
                        int16_t bonus = Helmsman_Bonus(ship->officer_idx);
                        if (bonus > max_helmsman_bonus) {
                            max_helmsman_bonus = bonus;
                        }
                    }
                }
            }
            return (max_helmsman_bonus / 2) + *bonus_val_ptr;
        } else {
            int16_t extra_bonus = 0;
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                s_combat_data* ship = &MOX::combat_data[i];
                if ((int16_t)ship->owner == player_idx && ship->is_captured == 0 && ship->unit_status == 0) {
                    if (ship->officer_idx > -1) {
                        int16_t bonus = Helmsman_Bonus(ship->officer_idx);
                        if (bonus > max_helmsman_bonus) {
                            max_helmsman_bonus = bonus;
                        }
                    }
                    if (ship->ftl_type == 0) {
                        if (ship->size_class == 2 || ship->size_class == 3 || ship->size_class == 4) {
                            extra_bonus = 0;
                        }
                    }
                }
            }
            return max_helmsman_bonus + extra_bonus;
        }
    }

    int16_t __cdecl Defensive_Combat_Bonus(int16_t combat_unit_idx, int16_t target_type) {
        int16_t specials_base = 0;
        int16_t fleet_base = 0;
        int16_t trait_bonus = 0;
        int16_t speed_bonus;
        int16_t crew_bonus = CMBTFIR2::Crew_Defense_Bonus(combat_unit_idx);

        if (target_type == 0) {
            speed_bonus = MOX::combat_data[combat_unit_idx].current_speed * 5;
            if (MOX::combat_data[combat_unit_idx].current_speed == 0) {
                speed_bonus = -20;
            }
        } else {
            speed_bonus = 0;
            crew_bonus /= 2;
        }

        int16_t specials_bonus = CMBTFIR2::Ship_Specials_Defensive_Bonus(combat_unit_idx, target_type, &specials_base);
        int8_t owner = MOX::combat_data[combat_unit_idx].owner;
        int16_t fleet_bonus = CMBTFIR2::Fleet_Defensive_Bonus(owner, target_type, &fleet_base);

        if (fleet_base > specials_base) {
            specials_bonus -= specials_base;
        } else {
            fleet_bonus -= fleet_base;
        }

        if ((MOX::combat_data[combat_unit_idx].ship_idx != -1 ||
                satellite_racial_defense_bonus) &&
            owner < MAX_PLAYERS) {
            trait_bonus = (int16_t)MOX::player[owner].traits[TRAIT_SHIP_DEFENSE];
        }

        return fleet_bonus + trait_bonus + speed_bonus + specials_bonus + crew_bonus;
    }

    int16_t __cdecl Computer_Bonus(int16_t ship_idx) {
        if (MOX::combat_data[ship_idx].computer_current_hits < 1) {
            return 0;
        }

        return TECHDATA::computers[MOX::combat_data[ship_idx].computer_type].bonus;
    }

    int16_t __cdecl Crew_Attack_Bonus(int16_t ship_idx) {
        int8_t quality = MOX::combat_data[ship_idx].crew_quality;
        return MOX::crew_data[quality].attack;
    }

    int16_t __cdecl Ship_Specials_Offensive_Bonus(int16_t ship_idx) {
        int16_t bonus = 0;

        if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_BATTLE_SCANNER) != 0) {
            bonus = combat_bonus.battle_scanner_beam_offense;
        }

        return bonus;
    }

    int16_t __cdecl Fleet_Offensive_Bonus(int16_t player_idx) {
        int16_t size_bonus = 0;
        int16_t special_bonus = 0;
        int16_t max_weaponry_bonus = 0;
        int16_t max_lore_bonus = 0;
        int16_t is_ancient = 0;

        if (MOX::combat_attacker_player > MAX_PLAYERS ||
            MOX::combat_defender_player > MAX_PLAYERS ||
            (lab_and_lore_against_antarans &&
                (MOX::combat_attacker_player == MAX_PLAYERS ||
                    MOX::combat_defender_player == MAX_PLAYERS))) {
            is_ancient = 1;
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];

            if (ship->owner == (int8_t)player_idx && ship->is_captured == 0 && ship->unit_status == 0) {
                if (COMBINIT::Is_Combat_Satellite(ship) &&
                    !CMBTMOV1::Ship_Is_In_Stasis(i)) {
                    const int16_t station_tier =
                        COMBINIT::Combat_Satellite_Hull_Class(ship) - HULL_STAR_BASE;
                    size_bonus = station_tier == 0
                        ? combat_bonus.starbase_fleet_beam_offense
                        : station_tier == 1
                        ? combat_bonus.battlestation_fleet_beam_offense
                        : combat_bonus.starfortress_fleet_beam_offense;
                }

                int16_t officer_idx = ship->officer_idx;
                if (officer_idx > -1) {
                    int16_t w_bonus = CMBTFIR2::Weaponry_Bonus(officer_idx);
                    if (w_bonus > max_weaponry_bonus) {
                        max_weaponry_bonus = w_bonus;
                    }

                    if (is_ancient != 0) {
                        int16_t l_bonus = CMBTFIR2::Galactic_Lore_Bonus(officer_idx);
                        if (l_bonus > max_lore_bonus) {
                            max_lore_bonus = l_bonus;
                        }
                    }
                }

                if (is_ancient == 1) {
                    if (!CMBTMOV1::Ship_Is_In_Stasis(i) &&
                        COMBINIT::Does_Combat_Ship_Have_Special(i, SPECIAL_SCOUT_LAB)) {
                        special_bonus = combat_bonus.scout_lab_fleet_beam_offense;
                    }
                }
            }
        }

        return max_lore_bonus + size_bonus + max_weaponry_bonus + special_bonus;
    }

    int16_t __cdecl Offensive_Combat_Bonus(int16_t ship_idx) {
        int16_t computer_bonus;
        int16_t crew_bonus;
        int16_t specials_bonus;
        int16_t fleet_bonus;
        int16_t trait_bonus;
        int8_t owner;

        computer_bonus = Computer_Bonus(ship_idx);
        crew_bonus = Crew_Attack_Bonus(ship_idx);
        specials_bonus = Ship_Specials_Offensive_Bonus(ship_idx);

        owner = MOX::combat_data[ship_idx].owner;
        fleet_bonus = Fleet_Offensive_Bonus(owner);

        trait_bonus = 0;
        if (owner < MAX_PLAYERS) {
            trait_bonus = (int16_t)MOX::player[owner].traits[TRAIT_SHIP_ATTACK];
        }

        return computer_bonus + crew_bonus + trait_bonus + fleet_bonus + specials_bonus;
    }

    int16_t __cdecl Being_Captured(int16_t ship_idx) {
        return (int16_t)struct_::Test_Bit_Field(&MOX::combat_data[ship_idx].combat_status_flags, 4);
    }

    void __cdecl Set_Capturing_Flag(int16_t unit_idx) {
        struct_::Set_Bit_Field(&MOX::combat_data[unit_idx].combat_status_flags, 4);
    }

    void __cdecl Set_Combat_Ship_Done(int16_t ship_idx) {
        struct_::Set_Bit_Field(&MOX::combat_data[ship_idx].combat_status_flags, 2);
    }

    int16_t __cdecl Used_Teleporter(int16_t ship_idx) {
        return (int16_t)struct_::Test_Bit_Field(&MOX::combat_data[ship_idx].combat_status_flags, 0);
    }

    void __cdecl Set_Teleporter_Used(int16_t ship_idx) {
        struct_::Set_Bit_Field(&MOX::combat_data[ship_idx].combat_status_flags, 0);
    }

    int16_t __cdecl Get_Fleet_Ordnance_Bonus(int16_t owner) {
        int16_t max_bonus = 0;

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (MOX::combat_data[i].owner == owner &&
                MOX::combat_data[i].is_captured == 0 &&
                MOX::combat_data[i].unit_status == 0 &&
                MOX::combat_data[i].officer_idx > -1) {
                int16_t bonus = Leader_Ordnance_Bonus(MOX::combat_data[i].officer_idx);

                if (bonus > max_bonus) {
                    max_bonus = bonus;
                }
            }
        }

        return max_bonus;
    }

    int16_t __cdecl Get_Fleet_Engineer_Bonus(int16_t player_idx) {
        int16_t max_bonus = 0;

        if (MOX::combat_total_ship_count <= 0) {
            return max_bonus;
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];

            if ((int16_t)ship->owner == player_idx && ship->is_captured == 0 && ship->unit_status == 0) {
                if (ship->officer_idx > -1) {
                    int16_t current_bonus = CMBTFIR2::Engineer_Bonus(ship->officer_idx);
                    if (current_bonus > max_bonus) {
                        max_bonus = current_bonus;
                    }
                }
            }
        }

        return max_bonus;
    }

    int16_t __cdecl Get_Fleet_Pilot_Bonus(int16_t owner_idx) {
        int16_t max_bonus = 0;

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data& ship = MOX::combat_data[i];

            if ((int16_t)ship.owner == owner_idx) {
                if (ship.is_captured == 0) {
                    if (ship.unit_status == 0 /* ACTIVE */) {
                        if (ship.officer_idx > -1) {
                            int16_t bonus = Fighter_Pilot_Bonus(ship.officer_idx);
                            if (bonus > max_bonus) {
                                max_bonus = bonus;
                            }
                        }
                    }
                }
            }
        }

        return max_bonus;
    }

    int16_t Leader_Ordnance_Bonus(int16_t leader_idx) {
        int16_t bonus = 0;

        if ((MOX::leaders[leader_idx].special_skills & LEADER_SHIP_SKILL_ORDNANCE) != 0) {
            bonus = (OFFICER::Owned_Officer_Level(leader_idx) + 1) * 5;
        } else if ((MOX::leaders[leader_idx].special_skills & LEADER_SHIP_SKILL_ORDNANCE2) != 0) {
            int16_t level = OFFICER::Owned_Officer_Level(leader_idx);
            bonus = (int16_t)(((level * 3 + 3) * 5) / 2);
        }

        return bonus;
    }

    int16_t __cdecl Max_Defensive_Fire_Range(int16_t ship_idx) {
        int16_t min_range = -1;

        for (int i = 0; i < 8; ++i) {
            uint16_t weapon_id = MOX::combat_data[ship_idx].weapons[i].weapon_id;
            if ((int16_t)weapon_id > 0 &&
                TECHDATA::weapons[(int16_t)weapon_id].type == WEAPON_TYPE_BEAM &&
                (MOX::combat_data[ship_idx].weapons[i].specials & 6) == 0 &&
                MOX::combat_data[ship_idx].weapons[i].shots_left > 0) {
                min_range = 24;
            }
        }

        if (min_range > 0 && MOX::combat_total_ship_count > 1) {
            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                if (MOX::combat_data[ship_idx].owner != MOX::combat_data[i].owner &&
                    MOX::combat_data[i].is_captured == 0 &&
                    MOX::combat_data[i].unit_status == 0) {
                    int16_t move_cost = CMBTMOV1::Move_Cost(i, (int16_t)MOX::combat_data[ship_idx].x, (int16_t)MOX::combat_data[ship_idx].y);
                    int16_t current_speed = COMBAT1::Calc_Current_Speed(i);
                    int16_t range = 1;
                    if (move_cost - current_speed > 1) {
                        range = move_cost - current_speed;
                    }
                    if (range < min_range) {
                        min_range = range;
                    }
                }
            }
        }
        return min_range;
    }

    int32_t __cdecl Max_Total_Internal_Space(int16_t ship_idx) {
        s_combat_data* ship = &MOX::combat_data[ship_idx];

        int32_t total_space = ship->drive_max_hits / 2;
        total_space += ship->computer_max_hits;

        for (int i = 0; i < 8; ++i) {
            total_space += ship->weapon_total_hits[i];
        }

        for (int i = 0; i < 8; ++i) {
            total_space += ship->special_device_hits[i];
        }

        total_space += ship->shield_max_hits;

        return total_space;
    }

    int16_t __cdecl Ship_Specials_Defensive_Bonus(int16_t ship_idx, int16_t target_type, int16_t* field_bonus_out) {
        int16_t total_bonus = 0;
        s_combat_data* ship = &MOX::combat_data[ship_idx];

        // target_type: 0 = Ship, 1 = Missile/Other
        if (ship->special_status_flag == 1 /* STATUS_PHANTOM_SHADOW */ ||
            (ship->special_status_flag == 4 &&
                COMBAT1::phasing_cloak_decloak_defense != 0 &&
                COMBAT1::interactive_combat_stats == 0)) {
            if (target_type == 0) {
                if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_STEALTH_FIELD)) {
                    total_bonus = combat_bonus.stealth_field_beam_defense;
                }
                if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_CLOAKING_DEVICE) &&
                    combat_bonus.cloaking_device_beam_defense > total_bonus) {
                    total_bonus = combat_bonus.cloaking_device_beam_defense;
                }
                if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_PHASING_CLOAK) &&
                    combat_bonus.phasing_cloak_beam_defense > total_bonus) {
                    total_bonus = combat_bonus.phasing_cloak_beam_defense;
                }
            } else {
                total_bonus = CMBTMIS::missile_defense.cloaking_device_missile_defense;
            }
        }

        if (target_type == 1) {
            *field_bonus_out = 0;
            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_WIDE_AREA_JAMMER)) {
                *field_bonus_out = CMBTMIS::missile_defense.wide_area_jammer_missile_defense;
            } else if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_MULTI_WAVE_ECM_JAMMER)) {
                *field_bonus_out = CMBTMIS::missile_defense.multi_wave_jammer_missile_defense;
            } else if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_ECM_JAMMER)) {
                *field_bonus_out = CMBTMIS::missile_defense.ecm_jammer_missile_defense;
            }
            total_bonus += *field_bonus_out;
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_INERTIAL_NULLIFIER)) {
            if (target_type == 0) {
                total_bonus += combat_bonus.inertial_nullifier_beam_defense;
            }
            if (target_type == 1) {
                total_bonus += CMBTMIS::missile_defense.inertial_nullifier_missile_defense;
            }
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_INERTIAL_STABILIZER)) {
            if (target_type == 0) {
                total_bonus += combat_bonus.inertial_stabilizer_beam_defense;
            }
            if (target_type == 1) {
                total_bonus += CMBTMIS::missile_defense.inertial_stabilizer_missile_defense;
            }
        }

        return total_bonus;
    }

    void __cdecl Kill_Crew(int16_t combat_data_idx, int16_t kill_count) {
        s_combat_data& ship = MOX::combat_data[combat_data_idx];

        // The assembly treats current marine_count as unsigned (0-255) for comparison
        if ((int16_t)(uint8_t)ship.marine_count < kill_count) {
            ship.marine_count = 0;
        } else {
            ship.marine_count -= (int8_t)kill_count;
        }
    }

    int16_t __cdecl Combat_Ship_Done(int16_t ship_idx) {
        return struct_::Test_Bit_Field(&MOX::combat_data[ship_idx].combat_status_flags, 2);
    }

    void __cdecl Apply_Internal_Damage(int16_t target_ship_idx, int16_t* remaining_damage, int16_t sys_index, int16_t count, int16_t flag) {
        s_combat_data* ship = &MOX::combat_data[target_ship_idx];

        if (sys_index == 7) {
            if (ship->armor_remaining < *remaining_damage) {
                sys_index = -1;
                *remaining_damage -= ship->armor_remaining;
                ship->armor_remaining = 0;
            } else {
                ship->armor_remaining -= *remaining_damage;
                *remaining_damage = 0;
            }
        }

        int16_t initial_damage = *remaining_damage;

        if (initial_damage > 0) {
            if (sys_index == 2) {
                if (ship->shield_current_hits <= initial_damage) {
                    *remaining_damage = initial_damage - ship->shield_current_hits;
                    ship->shield_current_hits = 0;
                    for (int i = 0; i < 4; ++i) {
                        ship->shield_arc_current[i] = 0;
                    }
                    ship->shield_arc_max = 0;
                } else {
                    ship->shield_current_hits -= initial_damage;
                    Refresh_Shield_Capacity(target_ship_idx);
                    *remaining_damage = 0;
                }
            } else if (sys_index == 3) {
                int16_t rem = ship->weapon_hits[count] % ship->weapon_hits_per_unit[count];
                if (rem == 0) {
                    rem = ship->weapon_hits_per_unit[count];
                }

                if (rem < initial_damage) {
                    ship->weapon_hits[count] -= rem;
                    *remaining_damage -= rem;
                } else {
                    ship->weapon_hits[count] -= initial_damage;
                    *remaining_damage = 0;
                }

                int16_t old_count = ship->weapons[count].count;
                ship->weapons[count].count = (ship->weapon_hits[count] + ship->weapon_hits_per_unit[count] - 1) / ship->weapon_hits_per_unit[count];

                if (ship->weapons[count].weapon_id == 0x20) {
                    if (old_count != ship->weapons[count].count) {
                        if (ship->weapons[count].shots_left != 0) {
                            ship->weapons[count].shots_left--;
                        } else {
                            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                                const int16_t stasis_source_idx = MOX::combat_data[i].stasis_source_idx == COMBAT_SHIP_NONE
                                    ? -1
                                    : MOX::combat_data[i].stasis_source_idx;
                                if (stasis_source_idx == target_ship_idx) {
                                    CMBTFIRE::Remove_Stasis_Beam(i);
                                    break;
                                }
                            }
                        }
                    }
                } else if (ship->weapons[count].weapon_id == 0x25) {
                    if (old_count != ship->weapons[count].count) {
                        if (ship->weapons[count].shots_left != 0) {
                            ship->weapons[count].shots_left--;
                        } else {
                            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                                if (MOX::combat_data[i].black_hole_source_idx == target_ship_idx) {
                                    MOX::combat_data[i].black_hole_flag = 0;
                                    MOX::combat_data[i].black_hole_source_idx = COMBAT_SHIP_NONE;
                                    COMBAT1::Recalculate_Combat_Ship(i);
                                    break;
                                }
                            }
                        }
                    }
                } else if (ship->weapons[count].weapon_id == WEAPON_TRACTOR_BEAM) {
                    const uint16_t target_idx = ship->special_cooldown;
                    if (ship->weapons[count].count == 0) {
                        ship->special_cooldown = COMBAT_SHIP_NONE;
                    }
                    if (target_idx < MOX::combat_total_ship_count) {
                        COMBAT1::Recalculate_Combat_Ship(target_idx);
                    }
                }

                if (ship->weapons[count].shots_left > ship->weapons[count].count) {
                    ship->weapons[count].shots_left = ship->weapons[count].count;
                }
            } else if (sys_index == 5) {
                if (ship->drive_current_hits < initial_damage) {
                    *remaining_damage = initial_damage - ship->drive_current_hits;
                    ship->drive_current_hits = 0;
                } else {
                    ship->drive_current_hits -= initial_damage;
                    *remaining_damage = 0;
                }

                if (*remaining_damage > 0) {
                    ship->structure_damage += *remaining_damage;
                    if (ship->structure_damage > ship->structure_max) {
                        ship->structure_damage = ship->structure_max;
                    }
                    *remaining_damage = 0;
                }
            } else if (sys_index == 8) {
                if (ship->computer_current_hits < initial_damage) {
                    *remaining_damage = initial_damage - ship->computer_current_hits;
                    ship->computer_current_hits = 0;
                } else {
                    ship->computer_current_hits -= initial_damage;
                    *remaining_damage = 0;
                }
            } else if (sys_index == 9) {
                ship->special_hits[count] -= initial_damage;
                if (ship->special_hits[count] <= 0) {
                    *remaining_damage = -ship->special_hits[count];
                    ship->special_hits[count] = 0;

                    int16_t flag_idx = 0;
                    for (int16_t i = 0; i < 39; ++i) {
                        if (struct_::Test_Bit_Field(ship->special_device_flags, i)) {
                            if (flag_idx == count) {
                                struct_::Set_Bit_Field(ship->special_device_damage_flags, i);
                                if (i == SPECIAL_CLOAKING_DEVICE ||
                                    i == SPECIAL_PHASING_CLOAK ||
                                    (i == SPECIAL_STEALTH_FIELD &&
                                        combat_bonus.stealth_field_beam_defense != 0)) {
                                    const bool has_ordinary_cloak =
                                        COMBINIT::Does_Combat_Ship_Have_Special(
                                            target_ship_idx,
                                            SPECIAL_CLOAKING_DEVICE) != 0 ||
                                        (combat_bonus.stealth_field_beam_defense != 0 &&
                                            COMBINIT::Does_Combat_Ship_Have_Special(
                                                target_ship_idx,
                                                SPECIAL_STEALTH_FIELD) != 0);
                                    COMBAT1::Handle_Destroyed_Cloak(
                                        &ship->special_status_flag,
                                        ship->special_status_timer,
                                        i == SPECIAL_PHASING_CLOAK,
                                        has_ordinary_cloak,
                                        COMBINIT::Does_Combat_Ship_Have_Special(
                                            target_ship_idx,
                                            SPECIAL_PHASING_CLOAK) != 0);
                                }
                                if (i == SPECIAL_MULTI_PHASED_SHIELDS) {
                                    Refresh_Shield_Capacity(target_ship_idx);
                                }
                                if (i == 13 && MOX::in_nebula_flag == 1) {
                                    for (int j = 0; j < 4; ++j) {
                                        ship->shield_arc_current[j] = 0;
                                    }
                                    ship->shield_arc_max = 0;
                                }
                                break;
                            }
                            flag_idx++;
                        }
                    }
                } else {
                    *remaining_damage = 0;
                }
            } else if (sys_index == 7 || sys_index == 0) {
                ship->structure_damage += initial_damage;
                if (ship->structure_damage > ship->structure_max) {
                    ship->structure_damage = ship->structure_max;
                }
                *remaining_damage = 0;
            }
        }

        int16_t damage_absorbed = initial_damage - *remaining_damage;

        if (flag == 1 && ship->marine_count != 0xFF && damage_absorbed > 0) {
            for (int16_t i = 0; i < damage_absorbed; ++i) {
                if (game_random::Random(100) <= 1) {
                    Kill_Crew(target_ship_idx, 1);
                }
                MOX::ken_trace = 0;
            }
        }

        COMBAT1::Recalculate_Combat_Ship(target_ship_idx);
    }

    int16_t __cdecl Get_Fleet_Commando_Bonus(int16_t player_idx) {
        int16_t max_bonus = 0;

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data& ship = MOX::combat_data[i];

            if (ship.owner == player_idx && ship.is_captured == 0 && ship.unit_status == 0) {
                if (ship.officer_idx > -1) {
                    int16_t bonus = CMBTFIR2::Commando_Bonus(ship.officer_idx);
                    if (bonus > max_bonus) {
                        max_bonus = bonus;
                    }
                }
            }
        }

        return max_bonus;
    }

    int16_t __cdecl Commando_Bonus(int16_t leader_idx) {
        const s_leader_data& leader = MOX::leaders[leader_idx];

        if (leader.general_skills & LEADER_GENERAL_SKILL_COMMANDO) {
            return OFFICER::Owned_Officer_Level(leader_idx) * 2 + 2;
        }

        if (leader.general_skills & LEADER_GENERAL_SKILL_COMMANDO2) {
            return (OFFICER::Owned_Officer_Level(leader_idx) + 1) * 3;
        }

        return 0;
    }

    int16_t __cdecl Crew_Boarding_Combat_Bonus(int16_t ship_idx) {
        return MOX::crew_data[MOX::combat_data[ship_idx].crew_quality].ground;
    }

    int16_t __cdecl Get_Fleet_Security_Bonus(int16_t player_idx) {
        int16_t max_bonus = 0;

        if (MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                s_combat_data& ship = MOX::combat_data[i];

                if (ship.owner == player_idx && ship.is_captured == 0 && ship.unit_status == 0) {
                    if (ship.officer_idx > -1) {
                        int16_t current_bonus = CMBTFIR2::Security_Bonus(ship.officer_idx);
                        if (current_bonus > max_bonus) {
                            max_bonus = current_bonus;
                        }
                    }
                }
            }
        }

        return max_bonus;
    }

    int16_t __cdecl Security_Bonus(int16_t leader_idx) {
        if ((MOX::leaders[leader_idx].special_skills & LEADER_SHIP_SKILL_SECURITY) != 0) {
            return OFFICER::Owned_Officer_Level(leader_idx) * 2 + 2;
        }

        if ((MOX::leaders[leader_idx].special_skills & LEADER_SHIP_SKILL_SECURITY2) != 0) {
            return (OFFICER::Owned_Officer_Level(leader_idx) + 1) * 3;
        }

        return 0;
    }

} // namespace CMBTFIR2