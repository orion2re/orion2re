#include "pch.h"

namespace aileader {
    void __cdecl Do_AI_Leaders() {
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            s_player* player = &MOX::player[i];

            if (player->objectives != PLAYER_OBJECTIVE_HUMAN) {
                int8_t leader_idx = player->officer_for_hire;

                if (leader_idx != -1) {
                    bool hire_criteria_met = false;
                    s_leader_data* leader = &MOX::leaders[leader_idx];
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

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            s_star_data* star = &MOX::star[star_idx];
            int8_t system_special = star->system_special;

            if (system_special >= 80 && system_special < 90) {
                int16_t player_idx = (int16_t)(system_special - 80);
                if (MOX::player[player_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                    int16_t joined_result;
                    OFFICER::Marooned_Hero_At_Star_Can_Join(star_idx, &joined_result, player_idx);
                }
            }
        }

        for (int32_t i = 0; i < (int32_t)MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN) {
                Assign_Unassigned_Leaders(i);
            }
        }
    }

    void __cdecl Assign_Star_Officer(int32_t player_index, int32_t leader_index) {
        int32_t star_scores[MAX_STARS];
        memset(star_scores, 0, sizeof(star_scores));

        uint32_t skills = MOX::leaders[leader_index].special_skills;

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];

            if (colony->owner == player_index && colony->outpost_flag == 0) {
                int16_t star_index = MOX::planet[colony->planet].star_index;

                if (MOX::star[star_index].officer_index[player_index] == -1) {
                    if ((skills & (LEADER_COLONY_SKILL_ENVIRONMENTALIST | LEADER_COLONY_SKILL_ENVIRONMENTALIST2)) &&
                        (*AIDATA::ai_colony_info)[i].pollution_controls_applicable != 0) {
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
                        (MOX::player[player_index].traits[TRAIT_CURRENT_GOVERNMENT] / 2 != GOVERNMENT_UNIFICATION / 2)) {
                        star_scores[star_index] += (uint8_t)colony->n_pops;
                    }

                    if (skills & (LEADER_COLONY_SKILL_MEDICINE | LEADER_COLONY_SKILL_MEDICINE2)) {
                        int32_t bonus = (uint8_t)(*AIDATA::ai_colony_info)[i].pop_limit - (uint8_t)colony->n_pops;
                        if (bonus > 0) {
                            star_scores[star_index] += bonus;
                        }
                    }

                    if (skills & (LEADER_COLONY_SKILL_TACTICS | LEADER_COLONY_SKILL_TACTICS2)) {
                        star_scores[star_index] += (uint8_t)(*AIDATA::ai_star_info)[star_index].per_player[player_index].neutral_count +
                            (uint8_t)(*AIDATA::ai_star_info)[star_index].per_player[player_index].alliance_count;
                    }
                }
            }
        }

        int16_t best_star_idx = MOX::NUM_STARS - 1;
        for (int16_t i = MOX::NUM_STARS - 2; i >= 0; --i) {
            if (star_scores[i] > star_scores[best_star_idx]) {
                best_star_idx = i;
            }
        }

        if (star_scores[best_star_idx] != 0) {
            OFFICER::Ai_Assign_Leader_To_Star(&MOX::player[player_index], &MOX::leaders[leader_index], &MOX::star[best_star_idx]);
        }
    }

    void __cdecl Assign_Fleet_Officer(int32_t player_index, int32_t leader_index) {
        s_ship_data* best_ship = nullptr;
        s_ship_data* ship_list = MOX::ship;
        int32_t num_ships = (int32_t)MOX::NUM_SHIPS;

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
                &MOX::player[player_index],
                &MOX::leaders[leader_index],
                best_ship);
        }
    }

    void __cdecl Assign_Unassigned_Leaders(int32_t player_idx) {
        for (int32_t leader_index = 0; leader_index < MAX_LEADERS; ++leader_index) {
            s_leader_data* leader = &MOX::leaders[leader_index];

            if (leader->player_index == (char)player_idx) {
                if (leader->status == 0 /* Available/Unassigned */) {
                    if (leader->type == LEADER_TYPE_SHIP) {
                        Assign_Fleet_Officer(player_idx, leader_index);
                    } else /* Star Officer */ {
                        Assign_Star_Officer(player_idx, leader_index);
                    }
                } else if (leader->status == 1 /* Assigned */) {
                    // Only fleet officers seem to be reassigned in this manner
                    if (leader->type == LEADER_TYPE_SHIP) {
                        Reassign_Fleet_Officer(player_idx, leader_index);
                    }
                }
            }
        }
    }
} // namespace aileader