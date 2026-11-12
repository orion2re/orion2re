#include "pch.h"

#include "lua/lua_game.h"

namespace COMBAT {
    int16_t __cdecl QGet_Target(int16_t ship_idx) {
        int8_t player = MOX::qcombat_data[ship_idx].player;
        int16_t target_count = 0;

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            // Target must belong to another player and be active
            if (MOX::qcombat_data[i].player != player && MOX::qcombat_data[i].status == 0 /* Active */) {
                target_count++;
            }
        }

        if (target_count != 0) {
            // Preserve target order and one RNG draw without a fixed-size target buffer.
            int random_val = game_random::Random(target_count);
            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                if (MOX::qcombat_data[i].player != player && MOX::qcombat_data[i].status == 0) {
                    if (--random_val == 0) {
                        return i;
                    }
                }
            }
        }

        // If no ship targets were found, check if we can target the fixed defense (index 0)
        // Only the attacker can target the colony/starbase.
        if (MOX::combat_attacker_player == player && MOX::cmbt_colony_id != -1) {
            if (MOX::qcombat_data[0].status == 0 /* Active */) {
                return 0;
            }
        }

        return -1;
    }

    void __cdecl Stellar_Converter_Attack(int16_t target_idx) {
        for (int i = 0; i < 4; ++i) {
            Apply_Damage_To_Qship(
                target_idx,
                TECHDATA::weapons[WEAPON_STELLAR_CONVERTER].strat_max_damage);
        }
    }

    int16_t __cdecl QGet_Target_SC(int16_t attacker_idx) {
        int8_t attacker_player = MOX::qcombat_data[attacker_idx].player;
        int16_t target_idx = -1;
        int16_t max_hits_remaining = 0;

        // The loop starts at ship index 1, effectively ignoring index 0
        if (MOX::combat_total_ship_count > 1) {
            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                s_qship* ship = &MOX::qcombat_data[i];

                // Target must belong to a different player and be active
                if (ship->player != attacker_player && ship->status == 0 /* Active */) {
                    int16_t hits_remaining = ship->max_hits - ship->structural_damage;

                    // Prefer targets with the highest remaining hit points
                    if (hits_remaining > max_hits_remaining) {
                        max_hits_remaining = hits_remaining;
                        target_idx = i;
                    }
                }
            }
        }

        return target_idx;
    }

    void __cdecl Beam_Attack(int16_t attacker_idx, int16_t target_idx) {
        s_qship* qships = MOX::qcombat_data;
        int16_t beam_weapon_id = qships[attacker_idx].beam_weapon_id;
        int16_t beam_weapon_count = TECHDATA::hull_data[qships[attacker_idx].size].strat_beam;

        // Base miss threshold (hit chance = 100 - miss_threshold)
        int16_t miss_threshold = (qships[target_idx].beam_defense_bonus - qships[attacker_idx].base_ocv) + 40;

        if (miss_threshold > 95) {
            miss_threshold = 95;
        }

        if (beam_weapon_id > 0 && beam_weapon_count > 0) {
            int min_damage = TECHDATA::weapons[beam_weapon_id].strat_min_damage;
            int max_damage = TECHDATA::weapons[beam_weapon_id].strat_max_damage;

            if (qships[attacker_idx].beam_damage_modifier > 0) {
                int modifier = qships[attacker_idx].beam_damage_modifier + 100;
                min_damage = (min_damage * modifier) / 100;
                max_damage = (max_damage * modifier) / 100;
            }

            for (int i = 0; i < (int)beam_weapon_count; ++i) {
                int roll = game_random::Random(100);

                // If miss_threshold <= roll, the attack hits
                if (miss_threshold <= (int16_t)roll) {
                    int damage_roll = game_random::Random((max_damage - min_damage) + 1);
                    int final_damage = min_damage + damage_roll - 1;

                    orion2re::lua::game::s_combat_beam_damage_context context;
                    context.attacker_player_id =
                        qships[attacker_idx].player;
                    context.target_player_id = qships[target_idx].player;
                    context.weapon_id = beam_weapon_id;
                    context.beam_ordinal = static_cast<std::uint16_t>(i + 1);
                    context.beam_count =
                        static_cast<std::uint16_t>(beam_weapon_count);
                    final_damage =
                        orion2re::lua::game::Modify_Combat_Beam_Damage(
                            &context, final_damage);

                    COMBAT::Apply_Damage_To_Qship(target_idx, (int16_t)final_damage);

                    if (qships[target_idx].status == 1 /* STATUS_DESTROYED */) {
                        target_idx = COMBAT::QGet_Target(attacker_idx);
                    }
                }

                if (target_idx == -1) {
                    break;
                }
            }
        }
    }

    void __cdecl Missile_Attack(int16_t attacker_idx, int16_t target_idx) {
        s_qship* attacker = &MOX::qcombat_data[attacker_idx];
        int16_t defense_bonus = MOX::qcombat_data[target_idx].missile_defense_bonus;
        int16_t missile_weapon_id = attacker->missile_weapon_id;
        int16_t missile_count = INITSHIP::Strat_Missile_Weapon_Count(attacker->size);

        if (missile_weapon_id > 0 && missile_count > 0) {
            int16_t weapon_damage = TECHDATA::weapons[missile_weapon_id].strat_max_damage;
            int16_t damage_modifier = attacker->missile_damage_modifier;

            if (damage_modifier > 0) {
                weapon_damage = (int16_t)(((damage_modifier + 100) * (int32_t)weapon_damage) / 100);
            }

            int8_t attacker_player_idx = attacker->player;

            // Apply sensor bonus to hit chance by reducing target defense
            if (attacker_player_idx < MAX_PLAYERS) {
                s_player* player = &MOX::player[attacker_player_idx];
                if (player->tech_applications[TECH_APP_SENSORS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    defense_bonus -= CMBTMIS::missile_defense.sensors;
                } else if (player->tech_applications[TECH_APP_NEUTRON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    defense_bonus -= CMBTMIS::missile_defense.neutron_scanner;
                } else if (player->tech_applications[TECH_APP_TACHYON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    defense_bonus -= CMBTMIS::missile_defense.tachyon_scanner;
                }
            } else {
                defense_bonus -= CMBTMIS::missile_defense.sensors;
            }

            do {
                int16_t roll = (int16_t)game_random::Random(100);
                if (roll > 95) {
                    roll = 1000;
                }

                if (defense_bonus <= roll) {
                    COMBAT::Apply_Damage_To_Qship(target_idx, weapon_damage);
                    s_qship* target = &MOX::qcombat_data[target_idx];

                    if (target->max_hits <= target->structural_damage) {
                        target->status = 1 /* Destroyed */;
                        target_idx = COMBAT::QGet_Target(attacker_idx);
                    }
                }
                missile_count--;
            } while (missile_count > 0 && target_idx != -1);
        }
    }

    void __cdecl Strat_Special_Attack(int16_t attacker_idx, int16_t target_idx) {
        s_qship* attacker = &MOX::qcombat_data[attacker_idx];
        int16_t weapon_id = attacker->special_weapon_id;
        int16_t count = INITSHIP::Strat_Special_Weapon_Count((uint32_t)attacker->size);

        if (weapon_id == 0x25 /* PULSAR */) {
            count = (count + 1) / 2;
        } else if (weapon_id == 0x26 /* GRAVITY_BEAM */) {
            count = (count + 3) / 4;
        }

        if (weapon_id > 0 && count > 0) {
            while (count > 0 && target_idx != -1) {
                int16_t min_dmg = TECHDATA::weapons[weapon_id].strat_min_damage;
                int16_t max_dmg = TECHDATA::weapons[weapon_id].strat_max_damage;

                int damage_roll = min_dmg + game_random::Random(max_dmg - min_dmg + 1) - 1;

                COMBAT::Apply_Damage_To_Qship(target_idx, (int16_t)damage_roll);

                if (MOX::qcombat_data[target_idx].status == 1 /* Destroyed */) {
                    target_idx = COMBAT::QGet_Target(attacker_idx);
                }

                count--;
            }
        }
    }

    void __cdecl Strat_Bomb_Attack(int16_t attacker_idx) {
        s_qship* attacker = &MOX::qcombat_data[attacker_idx];
        int16_t weapon_id = attacker->bomb_weapon_id;
        int16_t weapon_count = TECHDATA::hull_data[attacker->size].strat_bomb;

        if (weapon_id > 0 && weapon_count > 0) {
            int16_t min_dmg = TECHDATA::weapons[weapon_id].strat_min_damage;
            int16_t max_dmg = TECHDATA::weapons[weapon_id].strat_max_damage;

            // The loop processes damage for each bomb weapon on the ship
            for (int16_t i = weapon_count; i > 0; --i) {
                int range = (int)max_dmg - (int)min_dmg + 1;
                int random_val = game_random::Random(range);

                // Damage calculation includes an off-by-one adjustment as per original logic
                int applied_damage = (int)min_dmg + random_val - 1;

                // Index 0 in qcombat_data is always the target of the attack
                Apply_Damage_To_Qship(0, (int16_t)applied_damage);

                // If target takes structural damage exceeding its max hits, mark it destroyed
                if (MOX::qcombat_data[0].max_hits <= MOX::qcombat_data[0].structural_damage) {
                    MOX::qcombat_data[0].status = 1 /* Destroyed */;
                }
            }
        }
    }

    void __cdecl Apply_Damage_To_Qship(int16_t qship_idx, int16_t damage) {
        s_qship* ship = &MOX::qcombat_data[qship_idx];
        int16_t applied_damage;

        if (ship->shield_id == 7) {
            // Special case for Antaran Damper (shield_id 7) which reduces incoming damage to 1/4.
            applied_damage = damage / 4;
        } else {
            // Normal shields block a flat amount of damage.
            applied_damage = damage - TECHDATA::shields[ship->shield_id].shield_class;
        }

        if (applied_damage < 0) {
            applied_damage = 0;
        }

        ship->structural_damage += applied_damage;

        // Cap structural damage at 30000.
        if (ship->structural_damage > 30000) {
            ship->structural_damage = 30000;
        }

        // If structural damage meets or exceeds max hits, the ship is destroyed.
        if (ship->structural_damage >= ship->max_hits) {
            ship->status = 1 /* Destroyed */;
        }
    }

    int __cdecl qsort_ship_array_by_size(const void* a, const void* b) {
        const int16_t* p1 = (const int16_t*)a;
        const int16_t* p2 = (const int16_t*)b;
        int8_t size2 = MOX::ship[*p2].d.size;
        int8_t size1 = MOX::ship[*p1].d.size;

        if (size2 == size1) {
            return 0;
        }

        if (size2 > size1) {
            return 1;
        }

        return -1;
    }

    void __cdecl Tactical_Bombardment(int16_t* ship_array, int16_t ship_count, s_combat_bomb_result* bomb_result) {
        MOX::cmbt_colony_id = bomb_result->colony_idx;
        MOX::apply_damage_to_planet = 0;
        MOX::combat_total_ship_count = 1;

        allocate::Push_Block(MOX::screen_seg);

        MOX::combat_data = (s_combat_data*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(s_combat_data) * MAX_COMBAT_SHIPS);
        struct_::Clear_Structure(MOX::combat_data, sizeof(s_combat_data) * MAX_COMBAT_SHIPS);

        MOX::missile = (s_missile*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(s_missile) * MAX_COMBAT_FLYERS);
        struct_::Clear_Structure(MOX::missile, sizeof(s_missile) * MAX_COMBAT_FLYERS);

        MOX::combat_attacker_player = bomb_result->attacker_idx;
        MOX::combat_defender_player = MOX::colony[MOX::cmbt_colony_id].owner;

        if ((uint16_t)MOX::combat_attacker_player <= MAX_PLAYERS) {
            const int16_t attacker = MOX::combat_attacker_player;
            MOX::fighter_beam_type[attacker] = INITSHIP::Best_Point_Defense_Weapon(attacker);
            MOX::fighter_bomb_type[attacker] = INITSHIP::Best_Bomb_Weapon(attacker);
        }

        qsort(ship_array, (size_t)ship_count, sizeof(*ship_array), qsort_ship_array_by_size);

        int16_t loaded_ships = 0;
        if (ship_count > 0) {
            for (int16_t i = 0; i < ship_count &&
                 loaded_ships < COMBINIT::max_ships_per_side;
                 ++i) {
                if (MOX::ship[ship_array[i]].d.ship_type == SHIP_TYPE_COMBAT) {
                    COMBINIT::Load_Combat_Ship(ship_array[i], MOX::combat_total_ship_count);
                    MOX::combat_data[MOX::combat_total_ship_count].x = 0;
                    MOX::combat_data[MOX::combat_total_ship_count].y = 0;
                    MOX::combat_total_ship_count++;
                    ++loaded_ships;
                }
            }
        }

        COMBINIT::Load_Tactical_Colony(MOX::cmbt_colony_id, 0);
        CMBTFIR2::Set_Combat_Ordnance_Values();

        MOX::combat_data[0].x = 0;
        MOX::combat_data[0].y = 0;

        if (MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                MOX::combat_data[i].tac_val_attack = CMBTFIR2::Offensive_Combat_Bonus(i);
                MOX::combat_data[i].tac_val_defense = CMBTFIR2::Defensive_Combat_Bonus(i, 0);
                MOX::combat_data[i].tac_val_evade = CMBTFIR2::Defensive_Combat_Bonus(i, 1);
                COMBINIT::Init_Special_Devices(i);
            }
        }

        int16_t turns = 10;
        do {
            if (MOX::combat_total_ship_count > 0) {
                for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                    for (int16_t j = 0; j < 8; ++j) {
                        MOX::combat_data[i].weapons[j].shots_left = MOX::combat_data[i].weapons[j].count;
                        MOX::combat_data[i].weapons[j].active = 1;

                        const int16_t weapon_id = MOX::combat_data[i].weapons[j].weapon_id;
                        if (weapon_id > 0 &&
                            TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_TORPEDO &&
                            MOX::combat_data[i].weapons[j].ammo > 0) {
                            MOX::combat_data[i].weapons[j].ammo--;
                        }
                    }
                }
            }

            if (MOX::combat_total_ship_count > 1) {
                for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                    CMBTFIRE::Fire_Ship(i, 0, 0, 0, 0, 0, 0);
                }
            }

            turns--;
        } while (turns != 0);

        int16_t best_armor_idx = INITSHIP::Best_Armor(MOX::combat_data[0].owner);
        uint16_t civilian_armor_hits = CMBTFIRE::civilian_armor[best_armor_idx];
        bomb_result->bomb_hits_remaining =
            MOX::combat_data[0].structure_max / civilian_armor_hits;
        bomb_result->is_planetary_hit = MOX::combat_bomb_result.is_planetary_hit;
        bomb_result->is_bio_weapon = MOX::combat_bomb_result.is_bio_weapon;
        bomb_result->bio_hits_remaining = MOX::combat_bomb_result.bio_hits_remaining;

        allocate::Pop_Block(MOX::screen_seg);
    }
} // namespace COMBAT
