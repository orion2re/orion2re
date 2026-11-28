#include "pch.h"
#include "legacy_ids.h"
#include "ship_ids.h"

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


    int16_t Ship_Strength_Vs_Shield(game::s_game_state* world, int16_t strategic_combat, game::id_ship ship_id, game::owner_slot target, int16_t shield_strength) {
        const auto ship_entry = world->ships.find(ship_id);
        auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
        if (!ship) return 0;
        int32_t total_strength = 0;
        int16_t attack_bonus = 0;
        int16_t defense_bonus;
        int16_t temp_attack;
        int16_t temp_defense;

        if (strategic_combat != 0) {
            attack_bonus = AIPOWER::Base_Generic_Ocv(world, factions::Player(ship->owner));
            INITSHIP::Update_Strategic_Ship(*world, strategic_combat, *ship);
        } else {
            INITSHIP::Get_Ship_Combat_Bonuses(*world, strategic_combat, *ship, &temp_attack, &temp_defense, &attack_bonus);
        }

        defense_bonus = Base_Generic_Dcv(world, factions::Player(target));

        for (int32_t i = 0; i < 8; ++i) {
            int16_t weapon_type = ship->d.ship_weapon[i].type;
            if (weapon_type <= 0) continue;

            int16_t count = ship->d.ship_weapon[i].current_count;
            uint16_t slot_specials = ship->d.ship_weapon[i].specials;
            uint32_t slot_strength;

            if (!(slot_specials & WEAPON_MOD_FLAG_SHIELD_PIERCING) && weapon_type != 0x22 && weapon_type != 0x0D) {
                if (weapon_type == 0x1F /* Point Defense */) {
                    weapon_type = INITSHIP::Best_Point_Defense_Weapon(*world, ship->owner);
                    count *= 3;
                } else if (weapon_type == 0x1E || weapon_type == 0x1D /* Bombs */) {
                    weapon_type = INITSHIP::Best_Bomb_Weapon(*world, strategic_combat, ship->owner);
                    count *= 3;
                } else if (weapon_type == 0x1C /* Stellar Converter */) {
                    weapon_type = 0x1B;
                    count *= 3;
                } else if (weapon_type == 0x22 || weapon_type == 0x0D || weapon_type == 0x24 /* Fighters/Bombers */) {
                    count *= 3;
                }

                int16_t damage = TECHDATA::weapons[weapon_type].max_damage;
                if (slot_specials & WEAPON_MOD_FLAG_HEAVY_MOUNT) {
                    damage += (int16_t)(((damage * 5 + 5) * 10) / 100);
                } else if (slot_specials & WEAPON_MOD_FLAG_POINT_DEFENSE) {
                    damage = (int16_t)(((damage * 5 + 5) * 10) / 100);
                }

                int32_t adjusted_damage = (int32_t)damage - (int32_t)shield_strength;
                if (adjusted_damage < 1) adjusted_damage = 0;
                slot_strength = adjusted_damage * count;
            } else {
                slot_strength = (int32_t)TECHDATA::weapons[weapon_type].max_damage * (int32_t)count;
            }

            if (slot_strength != 0) {
                int8_t weapon_type_class = TECHDATA::weapons[weapon_type].type;
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

            uint32_t mod_strength = Modified_Weapon_Strength(TECHDATA::weapon_modifications, slot_strength, slot_specials);
            total_strength += (uint16_t)mod_strength;
        }

        int16_t best_shield = INITSHIP::Best_Ship_Shield(*world, ship->owner);
        int16_t current_shield = ship->d.shield_type;
        int16_t shield_diff = best_shield - current_shield;
        int16_t shield_scale = 100;

        if (shield_diff < 0) {
            int16_t abs_diff = (shield_diff < 0) ? -shield_diff : shield_diff;
            shield_scale = (10 - abs_diff) * 10;
        }

        if (shield_scale < 10) shield_scale = 10;

        uint32_t scaled_strength = ((uint32_t)shield_scale * (uint32_t)total_strength) / 100;

        if (strategic_combat == 0) {
            int16_t current_hits = Current_Ship_Hits(world, strategic_combat, ship_id);
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

    int16_t __cdecl Max_Ship_Hits(game::s_game_state* world, int16_t strategic_combat, game::id_ship ship_id) {
        const auto ship_entry = world->ships.find(ship_id);
        const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
        return ship ? Max_Ship_Hits_For_Record(*world, strategic_combat, *ship) : 0;
    }

    int16_t Max_Ship_Hits_For_Record(game::s_game_state& world, int16_t strategic_combat, const s_ship_data& ship) {
        if (strategic_combat == 0) {
            return INITSHIP::Get_Ship_Structure(ship, false) + INITSHIP::Get_Ship_Armor_Hits(ship);
        }

        const int16_t base_hits = TECHDATA::hull_data[ship.d.size].strat_hits;
        const auto armor = INITSHIP::Best_Armor(world, ship.owner);
        if (armor <= 0) return base_hits;
        const int32_t absorption = TECHDATA::armor[armor].ships_bonus;
        return base_hits + static_cast<int16_t>(absorption * base_hits / 100);
    }

    uint16_t Ship_Strength_Vs_Ships(game::s_game_state* world, int16_t strategic_combat, game::id_ship attacker_idx, game::owner_slot target) {
        int16_t shield_value = Get_Best_Ship_Shield_Value(*world, TECHDATA::shields, target);
        return (uint16_t)Ship_Strength_Vs_Shield(world, strategic_combat, attacker_idx, target, shield_value);
    }

    uint16_t Ship_Strength_Vs_Ships_In_Nebula(game::s_game_state* world, int16_t strategic_combat, game::id_ship attacker_idx, game::owner_slot target) {
        return (uint16_t)AIPOWER::Ship_Strength_Vs_Shield(world, strategic_combat, attacker_idx, target, 0);
    }

    int16_t __cdecl Current_Ship_Hits(game::s_game_state* world, int16_t strategic_combat, game::id_ship ship_id) {
        const auto ship_entry = world->ships.find(ship_id);
        const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
        if (!ship) return 0;
        auto hits = Max_Ship_Hits_For_Record(*world, strategic_combat, *ship);

        // Subtract structural damage.
        hits -= ship->structural_damage;

        // In strategic combat armor damage is not counted
        if (strategic_combat == 0) {
            hits -= ship->armor_damage;
        }

        return hits;
    }

    void __cdecl Ship_Strength_Vs_Planetary_Shield(game::s_game_state* world, int16_t strategic_combat, game::id_ship ship_id, uint16_t* shield_strengths) {
        const auto ship_entry = world->ships.find(ship_id);
        const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
        if (!ship) {
            std::fill_n(shield_strengths, PLANETARY_SHIELD_COUNT, 0);
            return;
        }
        const auto& design = ship->d;
        uint32_t strengths[PLANETARY_SHIELD_COUNT];
        uint16_t shield_reductions[PLANETARY_SHIELD_COUNT] = {2, 7, 12, 22};

        for (int32_t i = 0; i < 4; ++i) {
            strengths[i] = 0;
        }

        if (strategic_combat == 1) {
            int32_t size = design.size;
            uint32_t base_strength = (uint32_t)(size * size * 10);
            strengths[0] = base_strength;

            for (int32_t i = 1; i < 4; ++i) {
                strengths[i] = (base_strength + (uint32_t)i - 1) / (uint32_t)i;
            }
        } else {
            for (int32_t i = 0; i < 8; ++i) {
                int16_t weapon_type = design.ship_weapon[i].type;
                if (weapon_type > 0) {
                    int16_t weapon_damage;
                    uint8_t weapon_type_class = TECHDATA::weapons[weapon_type].type;

                    if (weapon_type_class == WEAPON_TYPE_BEAM || weapon_type_class == WEAPON_TYPE_TORPEDO) {
                        weapon_damage = TECHDATA::weapons[weapon_type].max_damage / 2;
                    } else {
                        weapon_damage = TECHDATA::weapons[weapon_type].max_damage;
                    }

                    int32_t count = design.ship_weapon[i].current_count;

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
            int16_t current_hits = Current_Ship_Hits(world, strategic_combat, ship_id);
            int16_t max_hits = Max_Ship_Hits(world, strategic_combat, ship_id);

            // Scale strength by current ship health
            uint32_t final_power = (uint32_t)current_hits * strengths[i] / (uint32_t)max_hits;

            shield_strengths[i] = 0xfa00; /* 64000 */
            if (final_power <= 0xfa00) {
                shield_strengths[i] = (uint16_t)final_power;
            }
        }
    }

    int16_t Base_Generic_Ocv(game::s_game_state* world, game::id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        const auto legacy_player = player_id.id;
        if (legacy_player >= MAX_PLAYERS) return 0;
        int16_t computer_idx;
        int16_t bonus;

        computer_idx = INITSHIP::Best_Computer(*world, factions::From_Legacy(legacy_player));
        bonus = TECHDATA::computers[computer_idx].bonus;

        if (player_idx < MAX_PLAYERS) {
            bonus += world->players[player_idx].traits[TRAIT_SHIP_ATTACK];
        }

        return bonus;
    }

    uint16_t Colony_Space_Strength_Vs_Player(game::s_game_state* world, int16_t strategic_combat, game::id_colony colony_id, game::id_player player_id) {
        const s_colony* colony = &world->colonies.at(colony_id);
        int16_t building_type = -1;

        if (colony->buildings[BUILDING_STAR_BASE] != 0) {
            building_type = 1;
        } else if (colony->buildings[BUILDING_BATTLESTATION] != 0) {
            building_type = 2;
        } else if (colony->buildings[BUILDING_STAR_FORTRESS] != 0) {
            building_type = 3;
        }

        return AIPOWER::Satelite_Strength_Vs_Ships(world, strategic_combat, building_type, colony->owner, player_id);
    }

    int16_t Base_Generic_Dcv(game::s_game_state* world, game::id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        const auto legacy_player = player_id.id;
        if (legacy_player >= MAX_PLAYERS) return 0;
        const int16_t best_drive_idx = INITSHIP::Best_Warp_Drive(*world, factions::From_Legacy(legacy_player));

        int16_t dcv =
            (int16_t)TECHDATA::drives[best_drive_idx].combat_speed_min[2];
        dcv = (int16_t)(dcv * 5);

        if (player_idx < MAX_PLAYERS) {
            dcv = (int16_t)(dcv + world->players[player_idx].traits[TRAIT_SHIP_DEFENSE]);
            if (world->players[player_idx].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
                dcv = (int16_t)(dcv + 0x14);
            }
        }

        return dcv;
    }

    uint16_t Satelite_Strength_Vs_Ships(game::s_game_state* world, int16_t strategic_combat, int16_t building_type, game::id_player player_id, game::id_player target_player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        const auto legacy_player = player_id.id;
        if (legacy_player >= MAX_PLAYERS) return 0;
        uint32_t total_strength = 0;

        if (building_type != -1) {
            int16_t ocv = AIPOWER::Base_Generic_Ocv(world, player_id);
            int16_t dcv = (int16_t)Base_Generic_Dcv(world, target_player_id);
            int32_t accuracy_mod = (ocv - dcv) + 100;

            if (accuracy_mod < 10) {
                accuracy_mod = 10;
            }
            if (accuracy_mod > 100) {
                accuracy_mod = 100;
            }

            int16_t best_beam = INITSHIP::Best_Beam_Weapon(factions::From_Legacy(legacy_player));
            int16_t best_missile = INITSHIP::Best_Missile_Weapon_NT(factions::From_Legacy(legacy_player));

            int16_t beam_count_normal;
            int16_t beam_count_heavy;
            int16_t missile_count;
            const s_tech_hull_data* hull = &TECHDATA::hull_data[5 + building_type];

            if (strategic_combat == 0) {
                int16_t hull_size = hull->size;
                int16_t size_div_2 = hull_size / 2;
                int16_t rem_size = hull_size - size_div_2;

                // Tactical: Calculate weapon counts based on available hull space
                int32_t space_normal = DESIGN::Weapon_Space(factions::From_Legacy(legacy_player), best_beam, 1, WEAPON_FIRING_ARC_360, 0, 0);
                beam_count_normal = (int16_t)(COMBINIT::Satellite_Allocated_Space(
                                                  size_div_2,
                                                  COMBINIT::satellite_space_allocation.normal) /
                    space_normal);

                int32_t space_heavy = DESIGN::Weapon_Space(factions::From_Legacy(legacy_player), best_beam, 1, WEAPON_FIRING_ARC_360, 0, 2 /* MOD_HEAVY_MOUNT */);
                beam_count_heavy = (int16_t)(COMBINIT::Satellite_Allocated_Space(
                                                 size_div_2,
                                                 COMBINIT::satellite_space_allocation.heavy) /
                    space_heavy);

                int32_t space_missile = DESIGN::Weapon_Space(factions::From_Legacy(legacy_player), best_missile, 1, WEAPON_FIRING_ARC_360, COMBINIT::satellite_ammo_basis, 0);
                missile_count = (int16_t)(COMBINIT::Satellite_Allocated_Space(
                                              rem_size,
                                              COMBINIT::satellite_space_allocation.missile_torp) /
                    space_missile);
            } else {
                // Strategic: Use predefined counts
                beam_count_heavy = 0;
                beam_count_normal = hull->strat_beam;
                missile_count = hull->strat_missile;
            }

            int16_t shield_val = AIPOWER::Get_Best_Ship_Shield_Value(TECHDATA::shields, factions::From_Player(target_player_id));

            int32_t adj_beam_dmg_heavy = ((TECHDATA::weapons[best_beam].max_damage * 3) / 2) - shield_val;
            if (adj_beam_dmg_heavy < 0) adj_beam_dmg_heavy = 0;

            int32_t adj_missile_dmg = TECHDATA::weapons[best_missile].max_damage - shield_val;
            if (adj_missile_dmg < 0) adj_missile_dmg = 0;

            int32_t adj_beam_dmg_normal = TECHDATA::weapons[best_beam].max_damage - shield_val;
            if (adj_beam_dmg_normal < 0) adj_beam_dmg_normal = 0;

            total_strength = (adj_beam_dmg_heavy * beam_count_heavy * accuracy_mod) / 100;
            total_strength += (adj_missile_dmg * missile_count);
            total_strength += (adj_beam_dmg_normal * beam_count_normal * accuracy_mod) / 100;

            if (strategic_combat == 0) {
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


    int16_t __cdecl Strength_Hits_Modifier(int16_t strength) {
        if (strength <= 100) {
            return (int16_t)(strength + (100 - strength) / 10);
        }

        return (int16_t)(100 + (strength - 100) / 4);
    }

} // namespace AIPOWER
