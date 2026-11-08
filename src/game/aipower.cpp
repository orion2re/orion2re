#include "pch.h"

namespace AIPOWER {
    uint32_t __cdecl Modified_Weapon_Strength(const s_tech_weapon_mod_data* modifications, uint32_t weapon_strength, uint16_t modification_flags) {
        int16_t total_space = 100;

        if ((modification_flags & WEAPON_MOD_FLAG_HEAVY_MOUNT) != 0) {
            total_space = (int16_t)(modifications[WEAPON_MOD_HEAVY_MOUNT].space + 100);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_POINT_DEFENSE) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_POINT_DEFENSE].space);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_NO_RANGE_DISSIPATION].space);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_AUTO_FIRE) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_AUTO_FIRE].space);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_ENVELOPING) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_ENVELOPING].space);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_MIRV) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_MIRV].space);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_ECCM) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_ECCM].space);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_HEAVILY_ARMORED) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_HEAVILY_ARMORED].space);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_FAST) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_FAST].space);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_EMISSION_GUIDANCE) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_EMISSIONS_GUIDANCE].space);
        }
        if ((modification_flags & WEAPON_MOD_FLAG_OVERLOADED) != 0) {
            total_space = (int16_t)(total_space + modifications[WEAPON_MOD_OVERLOADED].space);
        }

        int32_t result =
            ((int32_t)(weapon_strength & 0xFFFFu) * (int32_t)total_space) / 100;

        if ((uint32_t)result > 64000u) {
            result = 64000;
        }

        return (uint32_t)result;
    }

    int16_t __cdecl Get_Best_Ship_Shield_Value(const s_tech_shield_data* shields, int16_t player_idx) {
        if (player_idx >= MAX_PLAYERS) {
            switch (player_idx) {
                case MAX_PLAYERS:
                    return 6;
                case 9:
                    return 8;
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                    return 0;
                default: {
                    // The binary returns an uninitialized 16-bit stack value here for out-of-range player_idx.
                    // We intentionally normalize that undefined behavior to 0.
                    return 0;
                }
            }
        }

        int16_t shield_idx = INITSHIP::Best_Ship_Shield(player_idx);
        return shields[shield_idx].shield_class;
    }

    int16_t __cdecl Ship_Strength_Vs_Shield(const AIDATA::s_world_view* world, int16_t ship_idx, int16_t target_idx, int16_t shield_strength) {
        s_ship_data* ship = &world->ships[ship_idx];
        int32_t total_strength = 0;
        int16_t attack_bonus = 0;
        int16_t defense_bonus;
        int16_t temp_attack;
        int16_t temp_defense;

        if (world->settings->strategic_combat_flag != 0) {
            attack_bonus = AIPOWER::Base_Generic_Ocv(world, ship->owner);
            INITSHIP::Update_Strategic_Ship(ship_idx);
        } else {
            INITSHIP::Get_Ship_Combat_Bonuses(ship_idx, &temp_attack, &temp_defense, &attack_bonus);
        }

        defense_bonus = Base_Generic_Dcv(world, target_idx);

        for (int32_t i = 0; i < 8; ++i) {
            int16_t weapon_type = ship->d.ship_weapon[i].type;
            if (weapon_type <= 0) continue;

            int16_t count = ship->d.ship_weapon[i].current_count;
            uint16_t slot_specials = ship->d.ship_weapon[i].specials;
            uint32_t slot_strength;

            if (!(slot_specials & WEAPON_MOD_FLAG_SHIELD_PIERCING) && weapon_type != 0x22 && weapon_type != 0x0D) {
                if (weapon_type == 0x1F /* Point Defense */) {
                    weapon_type = INITSHIP::Best_Point_Defense_Weapon(ship->owner);
                    count *= 3;
                } else if (weapon_type == 0x1E || weapon_type == 0x1D /* Bombs */) {
                    weapon_type = INITSHIP::Best_Bomb_Weapon(ship->owner);
                    count *= 3;
                } else if (weapon_type == 0x1C /* Stellar Converter */) {
                    weapon_type = 0x1B;
                    count *= 3;
                } else if (weapon_type == 0x22 || weapon_type == 0x0D || weapon_type == 0x24 /* Fighters/Bombers */) {
                    count *= 3;
                }

                int16_t damage = world->rules.technology.weapons[weapon_type].max_damage;
                if (slot_specials & WEAPON_MOD_FLAG_HEAVY_MOUNT) {
                    damage += (int16_t)(((damage * 5 + 5) * 10) / 100);
                } else if (slot_specials & WEAPON_MOD_FLAG_POINT_DEFENSE) {
                    damage = (int16_t)(((damage * 5 + 5) * 10) / 100);
                }

                int32_t adjusted_damage = (int32_t)damage - (int32_t)shield_strength;
                if (adjusted_damage < 1) adjusted_damage = 0;
                slot_strength = adjusted_damage * count;
            } else {
                slot_strength = (int32_t)world->rules.technology.weapons[weapon_type].max_damage * (int32_t)count;
            }

            if (slot_strength != 0) {
                int8_t weapon_type_class = world->rules.technology.weapons[weapon_type].type;
                switch (weapon_type_class) {
                    case WEAPON_TYPE_BEAM: {
                        int16_t hit_chance = (attack_bonus - defense_bonus) + 50;
                        if (slot_specials != 0) {
                            hit_chance += 25;
                        }
                        if (hit_chance < 10) hit_chance = 10;
                        if (hit_chance > 100) hit_chance = 100;
                        slot_strength = (slot_strength * (uint32_t)hit_chance) / 100;
                        break;
                    }
                    case WEAPON_TYPE_MISSILE: {
                        uint8_t ammo = ship->d.ship_weapon[i].ammo;
                        if (ammo == 2) {
                            slot_strength = (slot_strength * 3) >> 2;
                        }
                        break;
                    }
                    case WEAPON_TYPE_TORPEDO:
                        slot_strength = (slot_strength * 3) >> 2;
                        break;
                    case WEAPON_TYPE_BOMB:
                        slot_strength = 0;
                        break;
                    default:
                        break;
                }
            }

            uint32_t mod_strength = Modified_Weapon_Strength(world->rules.technology.weapon_modifications, slot_strength, slot_specials);
            total_strength += (uint16_t)mod_strength;
        }

        int16_t best_shield = INITSHIP::Best_Ship_Shield(ship->owner);
        int16_t current_shield = ship->d.shield_type;
        int16_t shield_diff = best_shield - current_shield;
        int16_t shield_scale = 100;

        if (shield_diff < 0) {
            int16_t abs_diff = (shield_diff < 0) ? -shield_diff : shield_diff;
            shield_scale = (10 - abs_diff) * 10;
        }

        if (shield_scale < 10) shield_scale = 10;

        uint32_t scaled_strength = ((uint32_t)shield_scale * (uint32_t)total_strength) / 100;

        if (world->settings->strategic_combat_flag == 0) {
            int16_t current_hits = Current_Ship_Hits(world, ship_idx);
            uint32_t exists_flag = (scaled_strength != 0) ? 1 : 0;
            int16_t hits_mod = Strength_Hits_Modifier(current_hits);
            uint32_t hit_modified_strength = ((uint32_t)hits_mod * scaled_strength) / 100;

            if (scaled_strength > 0) {
                if (hit_modified_strength > exists_flag) {
                    scaled_strength = hit_modified_strength;
                } else {
                    scaled_strength = exists_flag;
                }
            }
        }

        if (scaled_strength > 0xFA00 /* 64000 */) {
            scaled_strength = 0xFA00;
        }

        return (int16_t)scaled_strength;
    }

    int16_t __cdecl Max_Ship_Hits(const AIDATA::s_world_view* world, int16_t ship_idx) {
        if (world->settings->strategic_combat_flag == 0) {
            int16_t structure = INITSHIP::Get_Ship_Structure(ship_idx);
            int16_t armor_hits = INITSHIP::Get_Ship_Armor_Hits(ship_idx);
            return structure + armor_hits;
        } else {
            s_ship_data* ship = &world->ships[ship_idx];

            int32_t hull_type = (int32_t)ship->d.size;
            int16_t base_hits = world->rules.technology.hulls[hull_type].strat_hits;

            int16_t best_armor_idx = INITSHIP::Best_Armor((int16_t)ship->owner);
            if (best_armor_idx > 0) {
                // The original code performs a second call to Best_Armor if result is positive
                best_armor_idx = INITSHIP::Best_Armor((int16_t)ship->owner);

                int32_t absorption_pct = (int32_t)world->rules.technology.armor[best_armor_idx].ships_bonus;

                // bonus = (absorption_percent * base_hits) / 100
                int16_t bonus_hits = (int16_t)((absorption_pct * (int32_t)base_hits) / 100);
                return base_hits + bonus_hits;
            }

            return base_hits;
        }
    }

    uint16_t __cdecl Ship_Strength_Vs_Ships(const AIDATA::s_world_view* world, int16_t attacker_idx, int16_t target_idx) {
        int16_t shield_value = Get_Best_Ship_Shield_Value(world->rules.technology.shields, target_idx);
        return (uint16_t)Ship_Strength_Vs_Shield(world, attacker_idx, target_idx, shield_value);
    }

    uint16_t __cdecl Ship_Strength_Vs_Ships_In_Nebula(const AIDATA::s_world_view* world, int16_t attacker_idx, int16_t target_idx) {
        return (uint16_t)AIPOWER::Ship_Strength_Vs_Shield(world, attacker_idx, target_idx, 0);
    }

    int16_t __cdecl Current_Ship_Hits(const AIDATA::s_world_view* world, int16_t ship_idx) {
        int16_t hits;

        hits = AIPOWER::Max_Ship_Hits(world, ship_idx);

        // Subtract structural damage.
        hits -= world->ships[ship_idx].structural_damage;

        // In strategic combat armor damage is not counted
        if (world->settings->strategic_combat_flag == 0) {
            hits -= world->ships[ship_idx].armor_damage;
        }

        return hits;
    }

    void __cdecl Ship_Strength_Vs_Planetary_Shield(const AIDATA::s_world_view* world, int16_t ship_idx, uint16_t* shield_strengths) {
        uint32_t strengths[PLANETARY_SHIELD_COUNT];
        uint16_t shield_reductions[PLANETARY_SHIELD_COUNT] = {2, 7, 12, 22};

        for (int32_t i = 0; i < 4; ++i) {
            strengths[i] = 0;
        }

        if (world->settings->strategic_combat_flag == 1) {
            int32_t size = world->ships[ship_idx].d.size;
            uint32_t base_strength = (uint32_t)(size * size * 10);
            strengths[0] = base_strength;

            for (int32_t i = 1; i < 4; ++i) {
                strengths[i] = (base_strength + (uint32_t)i - 1) / (uint32_t)i;
            }
        } else {
            for (int32_t i = 0; i < 8; ++i) {
                int16_t weapon_type = world->ships[ship_idx].d.ship_weapon[i].type;
                if (weapon_type > 0) {
                    int16_t weapon_damage;
                    uint8_t weapon_type_class = world->rules.technology.weapons[weapon_type].type;

                    if (weapon_type_class == WEAPON_TYPE_BEAM || weapon_type_class == WEAPON_TYPE_TORPEDO) {
                        weapon_damage = world->rules.technology.weapons[weapon_type].max_damage / 2;
                    } else {
                        weapon_damage = world->rules.technology.weapons[weapon_type].max_damage;
                    }

                    int32_t count = world->ships[ship_idx].d.ship_weapon[i].current_count;

                    for (int32_t j = 0; j < 4; ++j) {
                        int32_t damage_vs_shield = (int32_t)weapon_damage - (int32_t)shield_reductions[j];
                        int32_t weapon_power = damage_vs_shield * count;

                        if (weapon_power > 0) {
                            strengths[j] += weapon_power;
                        }
                    }
                }
            }
        }

        for (int32_t i = 0; i < 4; ++i) {
            int16_t current_hits = Current_Ship_Hits(world, ship_idx);
            int16_t max_hits = Max_Ship_Hits(world, ship_idx);

            // Scale strength by current ship health
            uint32_t final_power = (uint32_t)current_hits * strengths[i] / (uint32_t)max_hits;

            shield_strengths[i] = 0xfa00; /* 64000 */
            if (final_power <= 0xfa00) {
                shield_strengths[i] = (uint16_t)final_power;
            }
        }
    }

    int16_t __cdecl Base_Generic_Ocv(const AIDATA::s_world_view* world, int16_t player_idx) {
        int16_t computer_idx;
        int16_t bonus;

        computer_idx = INITSHIP::Best_Computer(player_idx);
        bonus = world->rules.technology.computers[computer_idx].bonus;

        if (player_idx < MAX_PLAYERS) {
            bonus += world->players[player_idx].traits[TRAIT_SHIP_ATTACK];
        }

        return bonus;
    }

    uint16_t __cdecl Colony_Space_Strength_Vs_Player(const AIDATA::s_world_view* world, int16_t colony_idx, int16_t player_idx) {
        s_colony* colony = &world->colonies[colony_idx];
        int16_t building_type = -1;

        if (colony->buildings[BUILDING_STAR_BASE] != 0) {
            building_type = 1;
        } else if (colony->buildings[BUILDING_BATTLESTATION] != 0) {
            building_type = 2;
        } else if (colony->buildings[BUILDING_STAR_FORTRESS] != 0) {
            building_type = 3;
        }

        return AIPOWER::Satelite_Strength_Vs_Ships(world, building_type, (int16_t)colony->owner, player_idx);
    }

    int16_t __cdecl Base_Generic_Dcv(const AIDATA::s_world_view* world, int16_t player_idx) {
        const int16_t best_drive_idx = INITSHIP::Best_Warp_Drive(player_idx);

        int16_t dcv =
            (int16_t)world->rules.technology.drives[best_drive_idx].combat_speed_min[2];
        dcv = (int16_t)(dcv * 5);

        if (player_idx < MAX_PLAYERS) {
            dcv = (int16_t)(dcv + world->players[player_idx].traits[TRAIT_SHIP_DEFENSE]);
            if (world->players[player_idx].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
                dcv = (int16_t)(dcv + 0x14);
            }
        }

        return dcv;
    }

    uint16_t __cdecl Satelite_Strength_Vs_Ships(const AIDATA::s_world_view* world, int16_t building_type, int16_t player_idx, int16_t target_player_idx) {
        uint32_t total_strength = 0;

        if (building_type != -1) {
            int16_t ocv = AIPOWER::Base_Generic_Ocv(world, player_idx);
            int16_t dcv = (int16_t)Base_Generic_Dcv(world, target_player_idx);
            int32_t accuracy_mod = (ocv - dcv) + 100;

            if (accuracy_mod < 10) {
                accuracy_mod = 10;
            }
            if (accuracy_mod > 100) {
                accuracy_mod = 100;
            }

            int16_t best_beam = INITSHIP::Best_Beam_Weapon(player_idx);
            int16_t best_missile = INITSHIP::Best_Missile_Weapon_NT(player_idx);

            int16_t beam_count_normal;
            int16_t beam_count_heavy;
            int16_t missile_count;
            const s_tech_hull_data* hull = &world->rules.technology.hulls[5 + building_type];

            if (world->settings->strategic_combat_flag == 0) {
                int16_t hull_size = hull->size;
                int16_t size_div_2 = hull_size / 2;
                int16_t rem_size = hull_size - size_div_2;

                // Tactical: Calculate weapon counts based on available hull space
                int32_t space_normal = DESIGN::Weapon_Space(player_idx, best_beam, 1, WEAPON_FIRING_ARC_360, 0, 0);
                beam_count_normal = (int16_t)(COMBINIT::Satellite_Allocated_Space(
                                                  size_div_2,
                                                  world->rules.combat.satellite_space_allocation->normal) /
                    space_normal);

                int32_t space_heavy = DESIGN::Weapon_Space(player_idx, best_beam, 1, WEAPON_FIRING_ARC_360, 0, 2 /* MOD_HEAVY_MOUNT */);
                beam_count_heavy = (int16_t)(COMBINIT::Satellite_Allocated_Space(
                                                 size_div_2,
                                                 world->rules.combat.satellite_space_allocation->heavy) /
                    space_heavy);

                int32_t space_missile = DESIGN::Weapon_Space(player_idx, best_missile, 1, WEAPON_FIRING_ARC_360, *world->rules.combat.satellite_ammo_basis, 0);
                missile_count = (int16_t)(COMBINIT::Satellite_Allocated_Space(
                                              rem_size,
                                              world->rules.combat.satellite_space_allocation->missile_torp) /
                    space_missile);
            } else {
                // Strategic: Use predefined counts
                beam_count_heavy = 0;
                beam_count_normal = hull->strat_beam;
                missile_count = hull->strat_missile;
            }

            int16_t shield_val = AIPOWER::Get_Best_Ship_Shield_Value(world->rules.technology.shields, target_player_idx);

            int32_t adj_beam_dmg_heavy = ((world->rules.technology.weapons[best_beam].max_damage * 3) / 2) - shield_val;
            if (adj_beam_dmg_heavy < 0) adj_beam_dmg_heavy = 0;

            int32_t adj_missile_dmg = world->rules.technology.weapons[best_missile].max_damage - shield_val;
            if (adj_missile_dmg < 0) adj_missile_dmg = 0;

            int32_t adj_beam_dmg_normal = world->rules.technology.weapons[best_beam].max_damage - shield_val;
            if (adj_beam_dmg_normal < 0) adj_beam_dmg_normal = 0;

            total_strength = (adj_beam_dmg_heavy * beam_count_heavy * accuracy_mod) / 100;
            total_strength += (adj_missile_dmg * missile_count);
            total_strength += (adj_beam_dmg_normal * beam_count_normal * accuracy_mod) / 100;

            if (world->settings->strategic_combat_flag == 0) {
                uint32_t structural_mod = Strength_Hits_Modifier(hull->structure_hp);
                uint32_t val = (structural_mod * total_strength) / 100;

                if (val > 1) {
                    val = (Strength_Hits_Modifier(hull->structure_hp) * total_strength) / 100;
                } else {
                    val = 1;
                }

                total_strength = (val * 75) / 100;
            }

            if (total_strength > 0xFA00 /* 64000 */) {
                total_strength = 0xFA00;
            }
        }

        return (uint16_t)total_strength;
    }

    uint32_t __cdecl Missile_Base_Strength(const AIDATA::s_world_view* world, int16_t player_idx, int16_t target_idx) {
        if (world->settings->strategic_combat_flag == 1) {
            return 90 /* 0x5a */;
        }

        int16_t weapon_idx = INITSHIP::Best_Missile_Weapon_NT(player_idx);

        // weapon_idx is used here to get space cost
        int32_t weapon_space = DESIGN::Weapon_Space(player_idx, weapon_idx, 1, WEAPON_FIRING_ARC_360, *world->rules.combat.missile_base_ammo_basis, 0);

        int16_t capacity = (int16_t)(world->rules.combat.ground_defense->missile_base_space / weapon_space);

        int16_t shield_val = AIPOWER::Get_Best_Ship_Shield_Value(world->rules.technology.shields, target_idx);

        // Calculate base damage per weapon mount minus shield reduction
        int32_t net_damage = (int32_t)world->rules.technology.weapons[weapon_idx].max_damage - (int32_t)shield_val;

        if (net_damage <= 0) {
            net_damage = 0;
        }

        uint32_t strength = (uint32_t)net_damage * (uint32_t)capacity * 2;

        if (strength > 64000 /* 0xfa00 */) {
            strength = 64000;
        }

        return strength;
    }

    uint16_t __cdecl Colony_Ground_Strength_Vs_Player(const AIDATA::s_world_view* world, int16_t colony_idx, int16_t player_idx) {
        uint16_t strength = 0;
        s_colony* colony = &world->colonies[colony_idx];

        // BUILDING_MISSILE_BASE: 26
        if (colony->buildings[BUILDING_MISSILE_BASE] != 0) {
            strength = AIPOWER::Missile_Base_Strength(world, (int16_t)colony->owner, player_idx);
        }

        // BUILDING_GROUND_BATTERIES: 27
        if (colony->buildings[BUILDING_GROUND_BATTERIES] != 0) {
            strength += AIPOWER::Phasor_Base_Strength(world, (int16_t)colony->owner, player_idx);
        }

        // BUILDING_STELLAR_CONVERTER: 42
        if (colony->buildings[BUILDING_STELLAR_CONVERTER] != 0) {
            if (world->settings->strategic_combat_flag == 1) {
                strength += 200;
            } else {
                strength += 500;
            }
        }

        // BUILDING_FIGHTER_GARRISON: 47
        if (colony->buildings[BUILDING_FIGHTER_GARRISON] != 0) {
            strength += AIPOWER::Fighter_Garrison_Strength(world, (int16_t)colony->owner, player_idx);
        }

        // Cap maximum defensive strength
        if (strength > 64000) {
            strength = 64000;
        }

        return strength;
    }

    uint32_t __cdecl Phasor_Base_Strength(const AIDATA::s_world_view* world, int16_t player_idx, int16_t target_idx) {
        if (world->settings->strategic_combat_flag == 1) {
            return 120 /* 0x78 */;
        }

        int16_t weapon_idx = INITSHIP::Best_Beam_Weapon(
            player_idx, WEAPON_MOD_FLAG_HEAVY_MOUNT);
        int32_t space = DESIGN::Weapon_Space(player_idx, weapon_idx, 1, WEAPON_FIRING_ARC_360, 1, 2);

        int16_t weapon_count = (int16_t)(world->rules.combat.ground_defense->battery_hv_space / space);

        int16_t shield_value = AIPOWER::Get_Best_Ship_Shield_Value(world->rules.technology.shields, target_idx);

        int32_t max_damage = world->rules.technology.weapons[weapon_idx].max_damage;
        int32_t base_damage = ((max_damage * 3) / 2) - shield_value;

        if (base_damage < 0) {
            base_damage = 0;
        }

        uint32_t total_power = (uint32_t)base_damage * (uint32_t)weapon_count;

        if ((world->rules.technology.weapons[weapon_idx].natural_mods & WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE) != 0) {
            total_power *= 3;
        }

        if (total_power > 64000 /* 0xFA00 */) {
            total_power = 64000 /* 0xFA00 */;
        }

        return total_power;
    }

    uint16_t __cdecl Fighter_Garrison_Strength(const AIDATA::s_world_view* world, int16_t player_idx, int16_t target_player_idx) {
        int16_t pd_weapon_idx = INITSHIP::Best_Point_Defense_Weapon(player_idx);
        int16_t bomb_weapon_idx = INITSHIP::Best_Bomb_Weapon(player_idx);
        int16_t bomber_count = 0;
        int16_t shield_value = Get_Best_Ship_Shield_Value(world->rules.technology.shields, target_player_idx);
        int16_t heavy_fighter_count = 0;

        if (world->settings->strategic_combat_flag == 1) {
            return 120;
        }

        int16_t pd_count = world->rules.combat.ground_defense->garrison_interceptors;
        s_player* player = &world->players[player_idx];

        if (player->tech_applications[TECH_APP_HEAVY_FIGHTER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            pd_count = 0;
            bomber_count = 0;
            heavy_fighter_count =
                world->rules.combat.ground_defense->garrison_heavy_fighters;
        } else if (player->tech_applications[TECH_APP_BOMBER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            pd_count = 0;
            bomber_count = world->rules.combat.ground_defense->garrison_bombers;
        }

        int32_t bomb_dmg = (int32_t)world->rules.technology.weapons[bomb_weapon_idx].max_damage - shield_value;
        if (bomb_dmg < 0) {
            bomb_dmg = 0;
        }

        int32_t pd_dmg = (int32_t)world->rules.technology.weapons[pd_weapon_idx].max_damage - shield_value;
        if (pd_dmg < 0) {
            pd_dmg = 0;
        }

        uint16_t base_strength = (int16_t)bomb_dmg * ((heavy_fighter_count * 3 / 2) + bomber_count) +
            (int16_t)pd_dmg * (pd_count + heavy_fighter_count + bomber_count);

        base_strength &= 0x3fff;

        uint16_t final_strength = base_strength * 2;
        if (final_strength > 64000 /* 0xFA00 */) {
            final_strength = 64000;
        }

        return final_strength;
    }

    int16_t __cdecl Strength_Hits_Modifier(int16_t strength) {
        if (strength <= 100) {
            return (int16_t)(strength + (100 - strength) / 10);
        }

        return (int16_t)(100 + (strength - 100) / 4);
    }

} // namespace AIPOWER