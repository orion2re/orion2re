#include "pch.h"
#include "game_events.h"

namespace invasion {

    void __cdecl Compute_Player_Ground_Combat_Bonuses(int player_idx, s_invasion_player_bonuses* bonuses, int8_t is_defender) {
        memset(bonuses, 0, sizeof(s_invasion_player_bonuses));

        if (player_idx >= MAX_PLAYERS) {
            bonuses->antigrav_bonus = troop.bonus.anti_grav_harness;
            bonuses->battleoids_bonus = troop.bonus.battleoids;
            bonuses->has_battleoids = 1;
            bonuses->powered_armor_bonus = troop.bonus.powered_armor;
            bonuses->has_powered_armor = 1;
            bonuses->best_armor_tech_app_id = (uint8_t)TECHDATA::armor[ARMOR_XENTRONIUM_ARMOR].tech_app_id;
            bonuses->troop_bonus = TECHDATA::armor[ARMOR_XENTRONIUM_ARMOR].troops_bonus;
            bonuses->rifle_tech_app_id = (uint8_t)TECHDATA::rifles[RIFLE_PLASMA_RIFLE].tech_app_id;
            bonuses->rifle_bonus = TECHDATA::rifles[RIFLE_PLASMA_RIFLE].bonus;
            bonuses->personal_shield_tech_app_id = (uint8_t)TECHDATA::personal_shields[PERSONAL_SHIELD_PERSONAL_SHIELD].tech_app_id;
            bonuses->personal_shield_bonus = troop.bonus.personal_shield;
            bonuses->difficulty_bonus = (MOX::settings.game_difficulty * 2) - 4;
            return;
        }

        s_player* player = &MOX::player[player_idx];

        if (player->tech_applications[TECH_APP_ANTI_GRAV_HARNESS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            bonuses->antigrav_bonus = troop.bonus.anti_grav_harness;
        }
        if (player->tech_applications[TECH_APP_BATTLEOIDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            bonuses->battleoids_bonus = troop.bonus.battleoids;
            bonuses->has_battleoids = 1;
        }
        if (player->tech_applications[TECH_APP_POWERED_ARMOR] == TECH_RESEARCH_STATUS_RESEARCHED) {
            bonuses->powered_armor_bonus = troop.bonus.powered_armor;
            bonuses->has_powered_armor = 1;
        }

        int armor_idx = aitech::Player_Best_Armor(player);
        bonuses->best_armor_tech_app_id = (uint8_t)TECHDATA::armor[armor_idx].tech_app_id;
        bonuses->troop_bonus = TECHDATA::armor[armor_idx].troops_bonus;

        int rifle_idx = aitech::Player_Best_Rifle(player);
        bonuses->rifle_tech_app_id = (uint8_t)TECHDATA::rifles[rifle_idx].tech_app_id;
        bonuses->rifle_bonus = TECHDATA::rifles[rifle_idx].bonus;

        int shield_idx = aitech::Player_Best_Personal_Shield(player);
        bonuses->personal_shield_tech_app_id = (uint8_t)TECHDATA::personal_shields[shield_idx].tech_app_id;
        bonuses->personal_shield_bonus = shield_idx == PERSONAL_SHIELD_PERSONAL_SHIELD
            ? troop.bonus.personal_shield
            : 0;

        bonuses->ground_combat_bonus = player->traits[TRAIT_GROUND_COMBAT_BONUS];

        if (player->traits[TRAIT_HEAVY_G_WORLD] != 0) {
            bonuses->heavy_g_bonus = (int8_t)troop.bonus.high_g_hits;
        } else if (player->traits[TRAIT_LOW_G_WORLD] != 0) {
            bonuses->low_g_penalty = troop.bonus.low_g;
        }

        if (player->traits[TRAIT_SUBTERRANEAN] != 0 && is_defender != 0) {
            bonuses->subterranean_bonus = (int8_t)troop.bonus.subterranean;
        }

        if (player->objectives == PLAYER_OBJECTIVE_HUMAN) {
            bonuses->difficulty_bonus = 0;
        } else {
            bonuses->difficulty_bonus = MOX::settings.game_difficulty - 2;
        }
    }

    void __cdecl Compute_Colony_Officer_Bonus(s_colony* colony, s_invasion_player_bonuses* bonuses) {
        int leader_idx = COLCALC::Colony_Officer(colony);

        if (leader_idx != -1) {
            s_leader_data* leader = &MOX::leaders[leader_idx];

            if (leader->general_skills & LEADER_GENERAL_SKILL_COMMANDO2) {
                int16_t level = OFFICER::Owned_Officer_Level(leader_idx);
                bonuses->officer_bonus = ground_commando_defender_x5
                    ? ((level + 1) * 15) / 2
                    : (level + 1) * 3;
            } else if (leader->general_skills & LEADER_GENERAL_SKILL_COMMANDO) {
                int16_t level = OFFICER::Owned_Officer_Level(leader_idx);
                bonuses->officer_bonus =
                    (level + 1) * (ground_commando_defender_x5 ? 5 : 2);
            }

            if (bonuses->officer_bonus > 0) {
                bonuses->officer_idx = (int8_t)leader_idx;
            }
        }
    }

    void __cdecl Compute_Attacker_Officer_Bonus(int16_t* ship_indexes, int16_t count, s_invasion_player_bonuses* bonuses) {
        if (count <= 0) {
            return;
        }

        for (int16_t i = count - 1; i >= 0; --i) {
            int16_t ship_idx = ship_indexes[i];
            int16_t officer_idx = MOX::ship[ship_idx].officer_index;

            if (officer_idx >= 0) {
                if (MOX::leaders[officer_idx].eta == 0) {
                    int16_t bonus = 0;

                    if ((MOX::leaders[officer_idx].general_skills & LEADER_GENERAL_SKILL_COMMANDO2) != 0) {
                        int16_t level = OFFICER::Owned_Officer_Level(officer_idx);
                        bonus = ground_commando_attacker_x2
                            ? (level + 1) * 3
                            : (level * 15 + 16) / 2;
                    } else if ((MOX::leaders[officer_idx].general_skills & LEADER_GENERAL_SKILL_COMMANDO) != 0) {
                        int16_t level = OFFICER::Owned_Officer_Level(officer_idx);
                        bonus = (level + 1) *
                            (ground_commando_attacker_x2 ? 2 : 5);
                    }

                    if (bonuses->officer_bonus < bonus) {
                        bonuses->officer_bonus = bonus;
                        bonuses->officer_idx = (int8_t)officer_idx;
                    }
                }
            }
        }
    }

    void __cdecl Compute_Ground_Combat_Info(s_invasion_side_info* side, s_invasion_player_bonuses* own_mods, s_invasion_player_bonuses* opponent_mods) {
        int16_t base_strength = (uint8_t)own_mods->troop_bonus +
            (uint8_t)own_mods->personal_shield_bonus +
            (uint8_t)own_mods->rifle_bonus +
            (uint8_t)own_mods->antigrav_bonus;

        int16_t global_mod = own_mods->ground_combat_bonus +
            own_mods->low_g_penalty +
            (uint8_t)own_mods->subterranean_bonus +
            own_mods->difficulty_bonus +
            own_mods->officer_bonus;

        for (int16_t i = 0; i < 4; ++i) {
            if (i == 3) {
                if (opponent_mods != nullptr) {
                    side->unit_strengths[i] = (uint8_t)opponent_mods->troop_bonus +
                        (uint8_t)opponent_mods->personal_shield_bonus +
                        (uint8_t)opponent_mods->rifle_bonus +
                        (uint8_t)opponent_mods->antigrav_bonus;
                } else {
                    side->unit_strengths[i] = 0;
                }
            } else {
                side->unit_strengths[i] = base_strength;
            }

            side->unit_strengths[i] += global_mod;
            side->unit_hps[i] = own_mods->heavy_g_bonus + 1;

            switch (i) {
                case 0:
                    side->unit_strengths[i] += troop.type.tanks;
                    side->unit_hps[i]++;
                    side->unit_strengths[i] += (uint8_t)own_mods->battleoids_bonus;
                    side->unit_hps[i] += own_mods->has_battleoids *
                        (int8_t)troop.bonus.battleoids_hits;
                    break;
                case 1:
                    side->unit_strengths[i] += (uint8_t)own_mods->powered_armor_bonus;
                    side->unit_hps[i] += own_mods->has_powered_armor *
                        (int8_t)troop.bonus.powered_armor_hits;
                    break;
                case 2:
                    side->unit_strengths[i] += troop.type.militia;
                    break;
                case 3:
                    side->unit_strengths[i] += troop.type.rebels;
                    break;
            }
        }

        side->round_hit_unit_type = -1;
        side->round_killed_unit_type = -1;
        side->current_troop_type_idx = 0;

        Find_Troops(side);
    }

    void __cdecl Get_Boarding_Info(int16_t attacker_player_id, int16_t attacker_unit_count, int16_t attacker_additional_modifier, int16_t defender_player_id, int16_t defender_unit_count, int16_t defender_additional_modifier, s_invasion_side_info* side_a, s_invasion_side_info* side_b) {
        memset(side_a, 0, sizeof(s_invasion_side_info));
        side_a->colony_id = -1;
        side_a->ship_list = nullptr;
        side_a->player_id = attacker_player_id;
        side_a->opponent_player_id = defender_player_id;
        side_a->n_ships = 0;
        side_a->unit_counts[1] = attacker_unit_count;

        memset(side_b, 0, sizeof(s_invasion_side_info));
        side_b->colony_id = -1;
        side_b->ship_list = nullptr;
        side_b->player_id = defender_player_id;
        side_b->opponent_player_id = attacker_player_id;
        side_b->n_ships = 0;
        side_b->unit_counts[1] = defender_unit_count;

        invasion::Compute_Player_Ground_Combat_Bonuses(side_a->player_id, &side_a->combat_modifiers, 0);
        side_a->combat_modifiers.officer_bonus = attacker_additional_modifier;

        invasion::Compute_Player_Ground_Combat_Bonuses(side_b->player_id, &side_b->combat_modifiers, 0);
        side_b->combat_modifiers.officer_bonus = defender_additional_modifier;

        invasion::Compute_Ground_Combat_Info(side_a, &side_a->combat_modifiers, &side_b->combat_modifiers);
        invasion::Compute_Ground_Combat_Info(side_b, &side_b->combat_modifiers, &side_a->combat_modifiers);
    }

    void __cdecl Check_All_Rebellions() {
        s_colony* colony = MOX::colony;

        while (colony < MOX::colony + MOX::NUM_COLONIES) {
            if (colony->owner != -1 && colony->outpost_flag == 0) {
                Check_Rebellion(colony);
            }
            colony++;
        }
    }

    void __cdecl Ground_Combat_Round(s_invasion_side_info* side1, s_invasion_side_info* side2) {
        int16_t roll1 = side1->unit_strengths[(uint8_t)side1->current_troop_type_idx] + (int16_t)game_random::Random(100);
        int16_t roll2 = side2->unit_strengths[(uint8_t)side2->current_troop_type_idx] + (int16_t)game_random::Random(100);

        side1->round_hit_unit_type = -1;
        side1->round_killed_unit_type = -1;

        if (roll2 >= roll1) {
            uint8_t idx = (uint8_t)side1->current_troop_type_idx;
            side1->current_unit_damage++;

            if (side1->unit_hps[idx] == side1->current_unit_damage) {
                side1->round_killed_unit_type = idx;
                side1->unit_counts[idx]--;
                side1->current_unit_damage = 0;

                if (side1->unit_counts[idx] == 0) {
                    Find_Troops(side1);
                }
            } else {
                side1->round_hit_unit_type = idx;
            }
        }

        side2->round_hit_unit_type = -1;
        side2->round_killed_unit_type = -1;

        if (roll2 <= roll1) {
            uint8_t idx = (uint8_t)side2->current_troop_type_idx;
            side2->current_unit_damage++;

            if (side2->unit_hps[idx] == side2->current_unit_damage) {
                side2->round_killed_unit_type = idx;
                side2->unit_counts[idx]--;
                side2->current_unit_damage = 0;

                if (side2->unit_counts[idx] == 0) {
                    Find_Troops(side2);
                }
            } else {
                side2->round_hit_unit_type = idx;
            }
        }
    }

    void __cdecl Resolve_Ground_Combat(s_invasion_side_info* side_a, s_invasion_side_info* side_b) {
        while ((uint8_t)side_a->current_troop_type_idx < 4 &&
            (uint8_t)side_b->current_troop_type_idx < 4) {
            Ground_Combat_Round(side_a, side_b);
        }
    }

    void __cdecl Find_Troops(s_invasion_side_info* side_info) {
        uint8_t idx = (uint8_t)side_info->current_troop_type_idx;
        while (idx < UNIT_COUNT && side_info->unit_counts[idx] == 0) {
            idx++;
        }

        side_info->current_troop_type_idx = (int8_t)idx;
        side_info->current_unit_damage = 0;
    }

    /**
 * @brief Assigns a new home planet to a player by selecting their colony with the largest population.
 * 
 * @param player_idx The index of the player.
 * @param excluded_colony_idx An index of a colony to exclude from the search (usually the one being lost).
 */
    void __cdecl Assign_New_Home_Planet(int8_t player_idx, int16_t excluded_colony_idx) {
        int max_pops = 0;
        int16_t best_colony_idx = -1;

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            if (i != excluded_colony_idx &&
                MOX::colony[i].owner == (int8_t)player_idx &&
                MOX::colony[i].outpost_flag == 0) {
                int current_pops = (uint8_t)MOX::colony[i].n_pops;

                if (current_pops >= max_pops) {
                    max_pops = current_pops;
                    best_colony_idx = i;
                }
            }
        }

        if (best_colony_idx == -1) {
            MOX::player[player_idx].home_planet_id = -1;
        } else {
            MOX::player[player_idx].home_planet_id = MOX::colony[best_colony_idx].planet;
        }
    }

    int32_t __cdecl Colony_Infantry_Limit(s_colony* colony) {
        int32_t limit =
            COLCALC::population_capacity.size[MOX::planet[colony->planet].size];

        if (MOX::player[colony->owner].traits[TRAIT_WARLORD] == 0) {
            limit /= 2;
        }

        if (limit == 0) {
            limit = 1;
        }

        return limit;
    }

    int __cdecl Unload_Transports(s_colony* colony, int16_t* transport_ship_ids, int num_transports) {
        int count = 0;
        int limit = Colony_Infantry_Limit(colony);

        if (colony->military[0] < limit) {
            while (count < num_transports) {
                int16_t ship_idx = *transport_ship_ids;
                transport_ship_ids++;
                count++;

                SHIPS::Kill_Ship(ship_idx);

                colony->military[0] += 4;
                if (colony->military[0] >= limit) {
                    break;
                }
            }
        }

        COLCALC::Update_Player_Stats(&MOX::player[colony->owner]);

        return count;
    }

    void __cdecl Check_Rebellion(s_colony* colony) {
        if (colony->occupation_policy == 4) {
            return;
        }

        uint8_t rebel_weights[MAX_PLAYERS] = {0};
        int total_rebel_pop = 0;

        if (colony->n_pops != 0) {
            for (int i = 0; i < colony->n_pops; i++) {
                uint32_t pop_flags = colony->pop[i];
                if ((pop_flags & 0x400) != 0) {
                    int plr_idx = pop_flags & 0xF;
                    if (!MOX::player[plr_idx].eliminated) {
                        total_rebel_pop++;
                        rebel_weights[plr_idx]++;
                    }
                }
            }
        }

        if (total_rebel_pop <= 0) {
            return;
        }

        int16_t rebel_player = RUSS::Get_Weighted_Choice_Char(rebel_weights, MOX::NUM_PLAYERS);

        int chance = total_rebel_pop * 10;

        if (MOX::player[colony->owner].objectives == PLAYER_OBJECTIVE_HUMAN &&
            MOX::player[rebel_player].objectives != PLAYER_OBJECTIVE_HUMAN) {
            chance += MOX::settings.game_difficulty * 4 - 8;
        }

        if (colony->buildings[BUILDING_ALIEN_CONTROL_CENTER] != 0) {
            chance /= 2;
        }

        if (colony->occupation_policy == 0) {
            chance *= 2;
        }

        if ((int32_t)game_random::Random(1000) <= chance) {
            int rebel_count = game_random::Random(total_rebel_pop);
            s_invasion_side_info rebel_side;
            s_invasion_side_info defender_side;

            Get_Rebellion_Info(rebel_player, rebel_count, colony, &rebel_side, &defender_side);
            Resolve_Ground_Combat(&rebel_side, &defender_side);

            s_MSG msg = {};
            msg.msg_type = MSG_TYPE_COLONY;
            msg.u.colony.planet_idx = colony->planet;
            msg.u.colony.subtype = MSG_COLONY_SUBTYPE_REBELLION;
            msg.u.colony.c.rebels.owner_player = (int8_t)rebel_player;
            msg.u.colony.c.rebels.rebels = (int8_t)rebel_count;
            msg.u.colony.c.rebels.lost = (int8_t)(rebel_count - rebel_side.unit_counts[3]);

            MSG_::Add_Msg(msg, colony->owner);
            MSG_::Add_Msg(msg, rebel_player);

            if (rebel_side.current_troop_type_idx < 4) {
                Change_Colony_Ownership(colony, rebel_side.player_id, 0);
                Resolve_Rebellion_Troops(rebel_side);
                colony->just_produced = COLONY_PRODUCTION_JUST_LIBERATED;
            }
        }
    }

    void __cdecl Get_Rebellion_Info(int16_t attacker_player_id, int16_t marines_count, s_colony* colony, s_invasion_side_info* side_a, s_invasion_side_info* side_b) {
        memset(side_a, 0, sizeof(s_invasion_side_info));

        side_a->player_id = attacker_player_id;
        side_a->opponent_player_id = (int16_t)colony->owner;
        side_a->ship_list = nullptr;
        side_a->n_ships = 0;
        side_a->colony_id = (int16_t)(colony - MOX::colony);
        side_a->unit_counts[3] = marines_count;

        memset(side_b, 0, sizeof(s_invasion_side_info));

        side_b->player_id = (int16_t)colony->owner;
        side_b->opponent_player_id = side_a->player_id;
        side_b->ship_list = nullptr;
        side_b->n_ships = 0;
        side_b->colony_id = (int16_t)(colony - MOX::colony);
        side_b->unit_counts[0] = colony->military[1]; // Tanks
        side_b->unit_counts[1] = colony->military[0]; // Infantry
        side_b->unit_counts[2] = Colony_N_Militia(colony);

        Compute_Player_Ground_Combat_Bonuses(side_a->player_id, &side_a->combat_modifiers, 0);
        side_a->combat_modifiers.officer_bonus = 0;

        Compute_Player_Ground_Combat_Bonuses(side_b->player_id, &side_b->combat_modifiers, 1);
        Compute_Colony_Officer_Bonus(colony, &side_b->combat_modifiers);

        Compute_Ground_Combat_Info(side_a, &side_a->combat_modifiers, &side_b->combat_modifiers);
        Compute_Ground_Combat_Info(side_b, &side_b->combat_modifiers, &side_a->combat_modifiers);
    }

    void __cdecl Resolve_Rebellion_Troops(s_invasion_side_info info) {
        s_colony* colony = &MOX::colony[info.colony_id];
        int16_t limit = (int16_t)Colony_Infantry_Limit(colony);

        if (info.unit_counts[3] < 4) {
            colony->military[0] = info.unit_counts[3];
        } else {
            colony->military[0] = 4;
        }

        if (colony->military[0] >= limit) {
            colony->military[0] = limit;
        }

        COLCALC::Compute_Star_Colony_Stuff(&MOX::star[MOX::planet[colony->planet].star_index]);
    }

    void __cdecl Resolve_Invasion_Troops(s_invasion_side_info info) {
        s_colony* colony = &MOX::colony[info.colony_id];
        int16_t limit = (int16_t)Colony_Infantry_Limit(colony);

        colony->military[0] = 1;
        int16_t remaining_infantry = info.unit_counts[1] - 1;

        if (info.n_ships > 0) {
            for (int16_t i = 0; i < info.n_ships; ++i) {
                int16_t ship_idx = info.ship_list[i];
                if (MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_TRANSPORT) {
                    if (remaining_infantry < 4) {
                        SHIPS::Kill_Ship(ship_idx);
                    } else {
                        remaining_infantry -= 4;
                    }
                }
            }
        }

        int16_t final_infantry = colony->military[0] + remaining_infantry;
        colony->military[0] = final_infantry;

        if (final_infantry >= limit) {
            colony->military[0] = limit;
        }

        int16_t planet_idx = colony->planet;
        int16_t star_idx = MOX::planet[planet_idx].star_index;

        COLCALC::Compute_Star_Colony_Stuff(&MOX::star[star_idx]);
    }

    void __cdecl Change_Colony_Ownership(s_colony* colony, uint32_t new_owner_idx, s_battle_results_info* results) {
        int8_t old_owner_idx = colony->owner;
        int16_t colony_idx = (int16_t)(colony - MOX::colony);

        // Increase war losses for the old owner against the new owner
        MOX::player[old_owner_idx].colony_war_losses[new_owner_idx] += 10;

        invasion::Change_Pop_Ownership(colony_idx, new_owner_idx);

        const int16_t captured_satellite = COMBINIT::Take_Captured_Satellite(colony_idx);
        if (captured_satellite != -1) {
            ERICNET::Add_Building(colony_idx, captured_satellite);
        }

        // Remove defensive/security buildings upon capture
        if (colony->buildings[BUILDING_ARMOR_BARRACKS] != 0) {
            ERICNET::Remove_Building(colony_idx, BUILDING_ARMOR_BARRACKS);
        }
        if (colony->buildings[BUILDING_MARINE_BARRACKS] != 0) {
            ERICNET::Remove_Building(colony_idx, BUILDING_MARINE_BARRACKS);
        }
        if (colony->buildings[BUILDING_ALIEN_CONTROL_CENTER] != 0) {
            ERICNET::Remove_Building(colony_idx, BUILDING_ALIEN_CONTROL_CENTER);
        }

        // Reset military units and roundoff buffers
        colony->military[0] = 0;
        colony->infantry_roundoff = 0;
        colony->military[1] = 0;
        colony->tank_roundoff = 0;

        // Record captured population for the new owner
        MOX::player[new_owner_idx].captured_population += (uint8_t)colony->n_pops;

        if (results != nullptr) {
            int16_t captured_techs[4] = {0};
            uint16_t weights[TECH_APP_COUNT];
            int32_t captured_count = 0;
            int32_t building_chance;

            if (MOX::player[new_owner_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                building_chance = settings.technology_chance.building_player;
            } else {
                building_chance = MOX::settings.game_difficulty * 5 +
                    settings.technology_chance.building_ai;
            }

            for (int32_t i = 0; i < BUILDING_COUNT; ++i) {
                if (colony->buildings[i] != 0) {
                    int16_t tech_app = TECHDATA::buildings[i].tech_app;
                    if (COLCALC::Player_May_Get_Tech_App(&MOX::player[new_owner_idx], tech_app)) {
                        if ((int32_t)game_random::Random(1000) <=
                            (building_chance / (captured_count + 1))) {
                            captured_techs[captured_count] = tech_app;
                            captured_count++;
                            COLCALC::Player_Gets_Tech_App(&MOX::player[new_owner_idx], tech_app);

                            if (captured_count >= 4) {
                                break;
                            }
                        }
                    }
                }
            }

            if (captured_count < 4) {
                int32_t population_chance;
                if (MOX::player[new_owner_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    population_chance = settings.technology_chance.population_player;
                } else {
                    population_chance = MOX::settings.game_difficulty * 5 +
                        settings.technology_chance.population_ai;
                }

                memset(weights, 0, sizeof(weights));
                for (int32_t i = 0; i < 204; ++i) {
                    if (MOX::player[old_owner_idx].tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                        if (COLCALC::Player_May_Get_Tech_App(&MOX::player[new_owner_idx], i)) {
                            weights[i] = 1;
                        }
                    }
                }

                int32_t n_tries = (uint8_t)colony->n_pops;
                while (n_tries > 0 && captured_count < 4) {
                    if ((int32_t)game_random::Random(1000) <=
                        (population_chance / (captured_count + 1))) {
                        int16_t tech_id = RUSS::Get_Weighted_Choice_Short(weights, 204);
                        if (tech_id == -1) {
                            n_tries = 0; // No more techs to capture
                        } else {
                            weights[tech_id] = 0;
                            captured_techs[captured_count] = tech_id;
                            captured_count++;
                            COLCALC::Player_Gets_Tech_App(&MOX::player[new_owner_idx], tech_id);
                        }
                    }
                    n_tries--;
                }
            }

            memcpy(results->captured_techs_data, captured_techs, captured_count * sizeof(int16_t));
            results->captured_tech_id = (int16_t)captured_count;
            results->captured_tech_planet_id = colony->planet;
        }
    }

    int __cdecl Colony_N_Militia(s_colony* colony) {
        int count = 0;
        uint8_t n_pops = (uint8_t)colony->n_pops;

        for (uint8_t i = 0; i < n_pops; ++i) {
            uint32_t pop_flags = colony->pop[i];

            if (((pop_flags & 0x0F) < MAX_PLAYERS) && ((pop_flags & 0x0400) == 0)) {
                count++;
            }
        }

        return count / 5;
    }

    void __cdecl Change_Pop_Ownership(int16_t colony_idx, uint32_t new_owner_idx) {
        s_colony* colony = &MOX::colony[colony_idx];
        game_events::Publish_Colony_Replacing(colony->owner);
        int8_t old_owner = colony->owner;

        BOMB::Handle_Leader_At_Lost_Colony(colony);

        MOX::player[old_owner].colony_war_losses[new_owner_idx] += (uint8_t)colony->n_pops;

        if (colony->buildings[BUILDING_CAPITOL] != 0) {
            ERICNET::Remove_Building(colony_idx, BUILDING_CAPITOL);
        }

        if (MOX::player[old_owner].home_planet_id == colony->planet) {
            invasion::Assign_New_Home_Planet(old_owner, colony_idx);
        }

        colony->owner = (int8_t)new_owner_idx;
        COLCALC::Do_Colony_Owner_Change_Star_Colony_Stuff(colony);

        if (MOX::player[new_owner_idx].home_planet_id == -1) {
            MOX::player[new_owner_idx].home_planet_id = colony->planet;
        }

        colony->auto_building = 0;

        if (MOX::player[new_owner_idx].traits[0] / 2 == GOVERNMENT_UNIFICATION / 2) {
            if (colony->buildings[BUILDING_HOLO_SIMULATOR] != 0) {
                ERICNET::Remove_Building(colony_idx, BUILDING_HOLO_SIMULATOR);
                MOX::player[new_owner_idx].bc += TECHDATA::buildings[BUILDING_HOLO_SIMULATOR].cost / 2;
            }
            if (colony->buildings[BUILDING_PLEASURE_DOME] != 0) {
                ERICNET::Remove_Building(colony_idx, BUILDING_PLEASURE_DOME);
                MOX::player[new_owner_idx].bc += TECHDATA::buildings[BUILDING_PLEASURE_DOME].cost / 2;
            }
        }

        int n_pops = (uint8_t)colony->n_pops;
        for (int i = 0; i < n_pops; ++i) {
            uint32_t pop_val = colony->pop[i];
            uint32_t race = pop_val & 0xF;
            uint32_t original_owner_idx = (pop_val & 0x70) >> 4;

            if (race == new_owner_idx) {
                pop_val &= ~0x400;
                pop_val = (pop_val & ~0x70) | ((new_owner_idx << 4) & 0x70);
            } else if (race == 8 || race == 9) {
                pop_val = (pop_val & ~0x70) | ((new_owner_idx << 4) & 0x70);
            } else {
                if (original_owner_idx == new_owner_idx) {
                    pop_val &= ~0x400;
                } else if (MOX::player[new_owner_idx].traits[25] != 0) {
                    pop_val &= ~0x400;
                    pop_val = (pop_val & ~0x70) | ((new_owner_idx << 4) & 0x70);
                } else {
                    pop_val |= 0x400;
                }
            }
            colony->pop[i] = pop_val;
        }

        COLCALC::Colony_Environmental_Stuff(colony);
        invasion::Enforce_Population_Limits_At_Colony(colony);
        COLBLDG::Clear_Colony_Queue(colony_idx);

        colony->production_spent = 0;
        colony->bought_outright = 0;
        colony->just_produced = COLONY_PRODUCTION_JUST_CONQUERED;
        colony->occupation_points = 0;
        colony->occupation_policy = 4;
        colony->buildings[BUILDING_COLONY_BASE] = 0;

        int16_t star_idx = MOX::planet[colony->planet].star_index;
        COLCALC::Compute_Star_Colony_Stuff(&MOX::star[star_idx]);

        colony->n_turns_existed = 0;
    }

    void __cdecl Enforce_Population_Limits_At_Colony(s_colony* colony) {
        int16_t race_pop_limit[10];
        int16_t race_pop_count[10];
        uint8_t limit_has_pops[MAX_COLONY_POPULATION];

        memset(limit_has_pops, 0, sizeof(limit_has_pops));
        memset(race_pop_count, 0, sizeof(race_pop_count));
        for (int i = 0; i < 10; ++i) {
            race_pop_limit[i] = -1;
        }

        for (int i = 0; i < (int)colony->n_pops; ++i) {
            uint32_t pop = colony->pop[i];
            int race = (int)(pop & 0xf);
            race_pop_count[race]++;
            if (race_pop_limit[race] == -1) {
                race_pop_limit[race] = COLCALC::Colony_Race_Pop_Limit(colony, (int16_t)race);
                if (race_pop_limit[race] >= 0 &&
                    race_pop_limit[race] < MAX_COLONY_POPULATION) {
                    limit_has_pops[race_pop_limit[race]] = 1;
                }
            }
        }

        RUSS::Shuffle(colony->pop, sizeof(colony->pop[0]), (int16_t)colony->n_pops);

        for (int current_limit = 1;
             current_limit < MAX_COLONY_POPULATION;
             ++current_limit) {
            if (limit_has_pops[current_limit] == 0) {
                continue;
            }

            bool limit_satisfied = false;
            while (!limit_satisfied) {
                uint32_t* ptr_race_9 = nullptr;
                uint32_t* ptr_diff_orig_owner = nullptr;
                uint32_t* ptr_same_orig_owner = nullptr;
                uint32_t* ptr_race_8 = nullptr;
                uint32_t* ptr_owner_race = nullptr;
                int pops_with_limit = 0;

                for (int i = 0; i < (int)colony->n_pops; ++i) {
                    uint32_t pop = colony->pop[i];
                    int race = (int)(pop & 0xf);
                    if (race_pop_limit[race] <= current_limit) {
                        pops_with_limit++;
                        if (race == 9) {
                            ptr_race_9 = &colony->pop[i];
                        } else if (race == 8) {
                            ptr_race_8 = &colony->pop[i];
                        } else if ((int)colony->owner == race) {
                            ptr_owner_race = &colony->pop[i];
                        } else {
                            int original_owner = (int)((pop & 0x70) >> 4);
                            if (original_owner == (int)colony->owner) {
                                ptr_same_orig_owner = &colony->pop[i];
                            } else {
                                ptr_diff_orig_owner = &colony->pop[i];
                            }
                        }
                    }
                }

                if (pops_with_limit <= current_limit) {
                    limit_satisfied = true;
                } else {
                    colony->n_pops--;
                    uint32_t* target_ptr = nullptr;
                    if (ptr_race_9 != nullptr) {
                        target_ptr = ptr_race_9;
                    } else if (ptr_diff_orig_owner != nullptr) {
                        target_ptr = ptr_diff_orig_owner;
                    } else if (ptr_same_orig_owner != nullptr) {
                        target_ptr = ptr_same_orig_owner;
                    } else if (ptr_race_8 != nullptr) {
                        target_ptr = ptr_race_8;
                    } else {
                        target_ptr = ptr_owner_race;
                    }
                    POP::Store(target_ptr, colony->pop[(int)colony->n_pops]);
                }
            }
        }

        if (COLCALC::Colony_Food2_Per_Farmer(colony, (int16_t)colony->owner, nullptr) == 0) {
            for (int i = 0; i < (int)colony->n_pops; ++i) {
                uint32_t pop = colony->pop[i];
                if ((pop & 0x180) == 0 && (pop & 0xf) != 8) {
                    colony->pop[i] = (pop & 0xfffffeff) | 0x80;
                }
            }
        }
    }

    int16_t __cdecl Colony_Infantry_Barracks_Limit(s_colony* colony) {
        int32_t limit = (uint8_t)colony->n_pops;

        if (MOX::player[colony->owner].traits[TRAIT_WARLORD] == 0) {
            limit /= 2;
        }

        if (limit == 0) {
            limit = 1;
        }

        return (int16_t)limit;
    }

    int __cdecl Colony_Tank_Barracks_Limit(s_colony* colony) {
        int limit;
        int pops = (uint8_t)colony->n_pops;

        if (MOX::player[colony->owner].traits[30 /* TRAIT_WARLORD */] == 0) {
            limit = pops / 4;
        } else {
            limit = pops / 2;
        }

        if (limit == 0) {
            limit = 1;
        }

        return limit;
    }

    void __cdecl Get_Invasion_Info(int16_t* ship_list, int16_t n_ships, s_colony* colony, s_invasion_side_info* attacker_side, s_invasion_side_info* defender_side) {
        memset(attacker_side, 0, sizeof(s_invasion_side_info));

        attacker_side->player_id = MOX::ship[ship_list[0]].owner;
        attacker_side->opponent_player_id = colony->owner;
        attacker_side->colony_id = (int16_t)(colony - MOX::colony);
        attacker_side->ship_list = ship_list;
        attacker_side->n_ships = n_ships;

        for (int16_t i = 0; i < n_ships; ++i) {
            if (MOX::ship[ship_list[i]].d.ship_type == SHIP_TYPE_TRANSPORT) {
                attacker_side->unit_counts[1] += 4;
            }
        }

        memset(defender_side, 0, sizeof(s_invasion_side_info));

        defender_side->player_id = colony->owner;
        defender_side->opponent_player_id = attacker_side->player_id;
        defender_side->colony_id = (int16_t)(colony - MOX::colony);
        defender_side->ship_list = ship_list;
        defender_side->n_ships = n_ships;

        defender_side->unit_counts[0] = colony->military[1];
        defender_side->unit_counts[1] = colony->military[0];
        defender_side->unit_counts[2] = (int16_t)Colony_N_Militia(colony);

        Compute_Player_Ground_Combat_Bonuses(attacker_side->player_id, &attacker_side->combat_modifiers, 0);
        Compute_Attacker_Officer_Bonus(ship_list, n_ships, &attacker_side->combat_modifiers);

        Compute_Player_Ground_Combat_Bonuses(attacker_side->opponent_player_id, &defender_side->combat_modifiers, 1);
        Compute_Colony_Officer_Bonus(colony, &defender_side->combat_modifiers);

        Compute_Ground_Combat_Info(attacker_side, &attacker_side->combat_modifiers, &defender_side->combat_modifiers);
        Compute_Ground_Combat_Info(defender_side, &defender_side->combat_modifiers, &attacker_side->combat_modifiers);
    }

    void __cdecl Invade(int16_t* ship_list, int16_t n_ships, int16_t colony_id, s_battle_results_info* battle_results) {
        s_invasion_side_info local_attacker;
        s_invasion_side_info local_defender;

        s_colony* colony = &MOX::colony[colony_id];

        invasion::Get_Invasion_Info(ship_list, n_ships, colony, &local_attacker, &local_defender);

        battle_results->attacker_invasion = local_attacker;
        battle_results->defender_invasion = local_defender;

        uint32_t random_seed = game_random::Get_Random_Seed();
        battle_results->captured_tech_id = 0;
        battle_results->ground_defense_strength = (int32_t)random_seed;

        invasion::Resolve_Ground_Combat(&local_attacker, &local_defender);

        if ((uint8_t)local_attacker.current_troop_type_idx < 4) {
            invasion::Change_Colony_Ownership(colony, (uint32_t)local_attacker.player_id, battle_results);
            invasion::Resolve_Invasion_Troops(local_attacker);
        } else {
            colony->military[1] = local_defender.unit_counts[0];
            colony->military[0] = local_defender.unit_counts[1];

            for (int16_t i = 0; i < n_ships; ++i) {
                int16_t ship_idx = ship_list[i];
                if (MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_TRANSPORT) {
                    SHIPS::Kill_Ship(ship_idx);
                }
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();
    }

    bool __cdecl Has_Defending_Troops(int8_t unused_attacker_id, int16_t* ship_list, int n_ships, int16_t colony_idx) {
        s_invasion_side_info side1;
        s_invasion_side_info side2;

        (void)unused_attacker_id;
        Get_Invasion_Info(ship_list, (int16_t)n_ships, &MOX::colony[colony_idx], &side1, &side2);

        return side1.unit_counts[1] != 0;
    }

    void __cdecl Compute_Colony_Ground_Combat_Info(s_colony* colony, s_invasion_side_info* side_info, bool include_rebels) {
        memset(side_info, 0, sizeof(s_invasion_side_info));

        side_info->opponent_player_id = -1;
        side_info->player_id = colony->owner;
        side_info->ship_list = nullptr;
        side_info->n_ships = 0;

        side_info->colony_id = (int16_t)(colony - MOX::colony);

        side_info->unit_counts[0] = colony->military[1];
        side_info->unit_counts[1] = colony->military[0];
        side_info->unit_counts[2] = (int16_t)Colony_N_Militia(colony);

        Compute_Player_Ground_Combat_Bonuses(colony->owner, &side_info->combat_modifiers, 1);
        Compute_Colony_Officer_Bonus(colony, &side_info->combat_modifiers);
        Compute_Ground_Combat_Info(
            side_info,
            &side_info->combat_modifiers,
            include_rebels ? &side_info->combat_modifiers : nullptr);
    }

    int32_t __cdecl Colony_Tank_Limit(s_colony* colony) {
        int32_t tank_limit =
            COLCALC::population_capacity.size[MOX::planet[colony->planet].size];

        if (MOX::player[colony->owner].traits[TRAIT_WARLORD] == 0) {
            tank_limit /= 4;
        } else {
            tank_limit /= 2;
        }

        if (tank_limit == 0) {
            tank_limit = 1;
        }

        return tank_limit;
    }

} // namespace invasion