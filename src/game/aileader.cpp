#include "pch.h"

namespace aileader {
    void __cdecl Do_AI_Leaders(AIDATA::s_state* ai, const game_world::s_view* world) {
        for (int16_t i = 0; i < world->Player_Count(); ++i) {
            s_player* player = &world->players[i];

            if (player->objectives != PLAYER_OBJECTIVE_HUMAN) {
                int8_t leader_idx = player->officer_for_hire;

                if (leader_idx != -1) {
                    bool hire_criteria_met = false;
                    s_leader_data* leader = &world->leaders[leader_idx];
                    uint32_t general_skills = leader->general_skills;

                    if ((general_skills & (LEADER_GENERAL_SKILL_MEGAWEALTH | LEADER_GENERAL_SKILL_MEGAWEALTH2)) != 0) {
                        hire_criteria_met = true;
                    } else if ((general_skills & LEADER_GENERAL_SKILL_RESEARCHER2) != 0 && player->do_not_research == 0) {
                        hire_criteria_met = true;
                    } else if ((general_skills & LEADER_GENERAL_SKILL_RESEARCHER) != 0 && player->do_not_research == 0) {
                        hire_criteria_met = true;
                    } else if ((general_skills & (LEADER_GENERAL_SKILL_ASSASSIN | LEADER_GENERAL_SKILL_ASSASSIN2 | LEADER_GENERAL_SKILL_FAMOUS | LEADER_GENERAL_SKILL_FAMOUS2 | LEADER_GENERAL_SKILL_OPERATIONS | LEADER_GENERAL_SKILL_OPERATIONS2 | LEADER_GENERAL_SKILL_SPY_MASTER | LEADER_GENERAL_SKILL_SPY_MASTER2 | LEADER_GENERAL_SKILL_TELEPATH | LEADER_GENERAL_SKILL_TELEPATH2)) != 0) {
                        hire_criteria_met = true;
                    } else if ((general_skills & LEADER_GENERAL_SKILL_TRADER2) != 0 && player->traits[TRAIT_REPULSIVE] == 0) {
                        hire_criteria_met = true;
                    } else if ((general_skills & LEADER_GENERAL_SKILL_TRADER) != 0 && player->traits[TRAIT_REPULSIVE] == 0) {
                        hire_criteria_met = true;
                    } else if (leader->type == LEADER_TYPE_SHIP &&
                        (leader->special_skills & (LEADER_SHIP_SKILL_HELMSMAN | LEADER_SHIP_SKILL_HELMSMAN2 | LEADER_SHIP_SKILL_WEAPONRY | LEADER_SHIP_SKILL_WEAPONRY2)) != 0) {
                        hire_criteria_met = true;
                    }

                    int16_t accept_offer = 0;
                    if (hire_criteria_met) {
                        int32_t cost = OFFICER::Calculate_Officer_Cost(i, (int16_t)leader_idx);
                        if (cost + 50 < player->bc) {
                            accept_offer = 1;
                            player->bc -= cost;
                        }
                    }

                    OFFICER::Set_Officer_To_Player((int16_t)leader_idx, i, (uint8_t)accept_offer);
                }

                player->officer_for_hire = -1;
            }
        }

        for (int16_t star_idx = 0; star_idx < world->Star_Count(); ++star_idx) {
            s_star_data* star = &world->stars[star_idx];
            int8_t system_special = star->system_special;

            if (system_special >= 80 && system_special < 90) {
                int16_t player_idx = (int16_t)(system_special - 80);
                if (world->players[player_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                    int16_t joined_result;
                    OFFICER::Marooned_Hero_At_Star_Can_Join(star_idx, &joined_result, player_idx);
                }
            }
        }

        for (int32_t i = 0; i < world->Player_Count(); ++i) {
            if (world->players[i].objectives != PLAYER_OBJECTIVE_HUMAN) {
                Assign_Unassigned_Leaders(ai, world, i);
            }
        }
    }

    void Assign_Star_Officer(AIDATA::s_state* ai, const game_world::s_view* world, uint32_t player_index, uint32_t leader_index) {
        std::vector<int32_t> star_scores(world->Star_Count(), 0);

        uint32_t skills = world->leaders[leader_index].special_skills;

        for (int16_t i = 0; i < world->Colony_Count(); ++i) {
            s_colony* colony = &world->colonies[i];

            if (colony->owner == player_index && colony->outpost_flag == 0) {
                int16_t star_index = world->planets[colony->planet].star_index;

                if (world->stars[star_index].officer_index[player_index] == -1) {
                    if ((skills & (LEADER_COLONY_SKILL_ENVIRONMENTALIST | LEADER_COLONY_SKILL_ENVIRONMENTALIST2)) &&
                        ai->colonies[i].pollution_controls_relevant) {
                        star_scores[star_index] += colony->pollution;
                    }

                    if ((skills & (LEADER_COLONY_SKILL_FARMING_LEADER | LEADER_COLONY_SKILL_FARMING_LEADER2)) &&
                        colony->food2_per_farmer != 0) {
                        star_scores[star_index] += colony->production[ECON_FOOD];
                    }

                    if (skills & (LEADER_COLONY_SKILL_FINANCIAL_LEADER | LEADER_COLONY_SKILL_FINANCIAL_LEADER2)) {
                        star_scores[star_index] += (uint8_t)colony->n_pops;
                    }

                    if (skills & (LEADER_COLONY_SKILL_LABOR_LEADER | LEADER_COLONY_SKILL_LABOR_LEADER2)) {
                        star_scores[star_index] += colony->production[ECON_INDUSTRY];
                    }

                    if (skills & (LEADER_COLONY_SKILL_SCIENCE_LEADER | LEADER_COLONY_SKILL_SCIENCE_LEADER2)) {
                        star_scores[star_index] += colony->production[ECON_RESEARCH];
                    }

                    if ((skills & (LEADER_COLONY_SKILL_SPIRITUAL_LEADER | LEADER_COLONY_SKILL_SPIRITUAL_LEADER2)) &&
                        (world->players[player_index].traits[TRAIT_CURRENT_GOVERNMENT] / 2 != GOVERNMENT_UNIFICATION / 2)) {
                        star_scores[star_index] += (uint8_t)colony->n_pops;
                    }

                    if (skills & (LEADER_COLONY_SKILL_MEDICINE | LEADER_COLONY_SKILL_MEDICINE2)) {
                        int32_t bonus = (uint8_t)ai->colonies[i].population_limit - (uint8_t)colony->n_pops;
                        if (bonus > 0) {
                            star_scores[star_index] += bonus;
                        }
                    }

                    if (skills & (LEADER_COLONY_SKILL_TACTICS | LEADER_COLONY_SKILL_TACTICS2)) {
                        star_scores[star_index] += (uint8_t)ai->stars[star_index].neighborhood.per_player[player_index].neutral_players_in_range +
                            (uint8_t)ai->stars[star_index].neighborhood.per_player[player_index].hostile_players_in_range;
                    }
                }
            }
        }

        int16_t best_star_idx = world->Star_Count() - 1;
        for (int16_t i = world->Star_Count() - 2; i >= 0; --i) {
            if (star_scores[i] > star_scores[best_star_idx]) {
                best_star_idx = i;
            }
        }

        if (star_scores[best_star_idx] != 0) {
            OFFICER::Ai_Assign_Leader_To_Star(&world->players[player_index], &world->leaders[leader_index], &world->stars[best_star_idx]);
        }
    }

    void Assign_Fleet_Officer(const game_world::s_view* world, uint32_t player_index, uint32_t leader_index) {
        s_ship_data* best_ship = nullptr;
        s_ship_data* ship_list = world->ships;
        int32_t num_ships = world->Ship_Count();

        for (int16_t i = 0; i < num_ships; ++i) {
            s_ship_data* current_ship = &ship_list[i];

            if (current_ship->owner == (char)player_index &&
                current_ship->status <= 2 &&
                current_ship->d.ship_type == SHIP_TYPE_COMBAT &&
                current_ship->officer_index == -1) {
                if (best_ship == nullptr || (best_ship->d.size <= current_ship->d.size && best_ship->d.shield_type <= current_ship->d.shield_type && best_ship->d.armor_type <= current_ship->d.armor_type && best_ship->d.computer_type <= current_ship->d.computer_type && best_ship->crew_experience < current_ship->crew_experience)) {
                    best_ship = current_ship;
                }
            }
        }

        if (best_ship != nullptr) {
            OFFICER::Ai_Assign_Leader_To_Ship(
                &world->players[player_index],
                &world->leaders[leader_index],
                best_ship);
        }
    }

    void Reassign_Fleet_Officer(const game_world::s_view* world, uint32_t player_index, uint32_t leader_index) {
        int16_t original_location = world->leaders[leader_index].location;
        s_ship_data* original_ship = &world->ships[original_location];
        s_ship_data* best_ship = original_ship;

        for (int32_t ship_index = world->Ship_Count() - 1; ship_index >= 0; --ship_index) {
            s_ship_data* ship = &world->ships[ship_index];
            if (ship->owner == player_index && ship->status <= 2 && ship->d.ship_type == SHIP_TYPE_COMBAT && ship->officer_index == -1) {
                if (OFFICER::Ships_In_Same_Place((int16_t)ship_index, original_location)) {
                    if ((best_ship == original_ship && original_ship->status == 7 /* Scrapped */) ||
                        (ship->d.size >= best_ship->d.size &&
                            ship->d.shield_type >= best_ship->d.shield_type &&
                            ship->d.armor_type >= best_ship->d.armor_type &&
                            ship->d.computer_type >= best_ship->d.computer_type &&
                            ship->crew_experience > best_ship->crew_experience)) {
                        best_ship = ship;
                    }
                }
            }
        }

        if (best_ship != original_ship) {
            OFFICER::Ai_Assign_Leader_To_Ship(&world->players[player_index], &world->leaders[leader_index], best_ship);
        }
    }

    void Assign_Unassigned_Leaders(AIDATA::s_state* ai, const game_world::s_view* world, uint32_t player_idx) {
        for (int32_t leader_index = 0; leader_index < MAX_LEADERS; ++leader_index) {
            s_leader_data* leader = &world->leaders[leader_index];

            if (leader->player_index == (char)player_idx) {
                if (leader->status == 0 /* Available/Unassigned */) {
                    if (leader->type == LEADER_TYPE_SHIP) {
                        Assign_Fleet_Officer(world, player_idx, leader_index);
                    } else /* Star Officer */ {
                        Assign_Star_Officer(ai, world, player_idx, leader_index);
                    }
                } else if (leader->status == 1 /* Assigned */) {
                    // Only fleet officers seem to be reassigned in this manner
                    if (leader->type == LEADER_TYPE_SHIP) {
                        Reassign_Fleet_Officer(world, player_idx, leader_index);
                    }
                }
            }
        }
    }
} // namespace aileader