#include "pch.h"
#include "legacy_ids.h"

namespace aileader {
    void __cdecl Do_AI_Leaders(AIDATA::s_state* ai, game::s_game_state* world) {
        for (int16_t i = 0; i < world->player_count; ++i) {
            s_player* player = &world->players[i];

            if (player->objectives != PLAYER_OBJECTIVE_HUMAN) {
                const auto offered_leader = player->officer_for_hire;

                if (offered_leader.id < MAX_LEADERS) {
                    bool hire_criteria_met = false;
                    auto& state = *world;
                    auto* leader = &state.leaders.at(offered_leader.id);
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
                        int32_t cost = OFFICER::Calculate_Officer_Cost(player->id, offered_leader);
                        if (cost + 50 < player->bc) {
                            accept_offer = 1;
                            player->bc -= cost;
                        }
                    }

                    OFFICER::Set_Officer_To_Player(offered_leader, player->id, (uint8_t)accept_offer);
                }

                player->officer_for_hire = game::NO_LEADER;
            }
        }

        for (int16_t star_idx = 0; star_idx < std::ssize(world->star_order); ++star_idx) {
            const auto star_id = world->star_order[star_idx];
            s_star_data* star = &world->stars.at(star_id);
            int8_t system_special = star->system_special;

            if (system_special >= 80 && system_special < 90) {
                int16_t player_idx = (int16_t)(system_special - 80);
                if (world->players[player_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                    game::id_leader joined_result;
                    OFFICER::Marooned_Hero_At_Star_Can_Join(game::Star_Key_At_Slot(*world, star_idx), &joined_result, world->players[player_idx].id);
                }
            }
        }

        for (int32_t i = 0; i < world->player_count; ++i) {
            if (world->players[i].objectives != PLAYER_OBJECTIVE_HUMAN) {
                Assign_Unassigned_Leaders(ai, world, world->players[i].id);
            }
        }
    }

    void Assign_Star_Officer(game::s_game_state* world, AIDATA::s_state* ai, game::id_player player_id, game::id_leader leader_index) {
        const uint32_t player_index = legacy_world::Slot(player_id);
        auto& state = *world;
        if (std::ssize(world->stars) == 0) return;
        std::vector<int32_t> star_scores(std::ssize(world->stars), 0);

        uint32_t skills = state.leaders.at(leader_index.id).special_skills;

        for (auto& [roster_id, roster_colony] : world->colonies) {
            s_colony* colony = &roster_colony;

            if (legacy_world::Encode_Player_Slot(colony->owner) == player_index && colony->outpost_flag == 0) {
                const auto star_id = state.planets.at(colony->planet_id).star_id;
                const auto star_index = game::Star_Row(state, star_id);
                if (star_index < 0) continue;

                if (state.stars.at(star_id).officer_id[player_index] == game::NO_LEADER) {
                    if ((skills & (LEADER_COLONY_SKILL_ENVIRONMENTALIST | LEADER_COLONY_SKILL_ENVIRONMENTALIST2)) &&
                        ai->colonies[roster_id].pollution_controls_relevant) {
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
                        int32_t bonus = (uint8_t)ai->colonies[roster_id].population_limit - (uint8_t)colony->n_pops;
                        if (bonus > 0) {
                            star_scores[star_index] += bonus;
                        }
                    }

                    if (skills & (LEADER_COLONY_SKILL_TACTICS | LEADER_COLONY_SKILL_TACTICS2)) {
                        star_scores[star_index] += (uint8_t)ai->stars[star_id].neighborhood.per_player[player_index].neutral_players_in_range +
                            (uint8_t)ai->stars[star_id].neighborhood.per_player[player_index].hostile_players_in_range;
                    }
                }
            }
        }

        int16_t best_star_idx = std::ssize(world->stars) - 1;
        for (int16_t i = std::ssize(world->stars) - 2; i >= 0; --i) {
            if (star_scores[i] > star_scores[best_star_idx]) {
                best_star_idx = i;
            }
        }

        if (star_scores[best_star_idx] != 0) {
            OFFICER::Ai_Assign_Leader_To_Star(*world, player_id, leader_index, state.stars.at(game::Star_Key_At_Row(state, best_star_idx)));
        }
    }

    void Assign_Fleet_Officer(game::s_game_state* world, game::id_player player_id, game::id_leader leader_index) {
        const uint32_t player_index = legacy_world::Slot(player_id);
        s_ship_data* best_ship = nullptr;
        int32_t num_ships = static_cast<int16_t>(world->ship_slots.size());

        for (int16_t i = 0; i < num_ships; ++i) {
            if (world->ship_slots[i] == game::NO_SHIP) continue;
            s_ship_data* current_ship = &ship_slots::At(*world, i);

            if (factions::Legacy_Code(current_ship->owner) == (char)player_index &&
                current_ship->status <= 2 &&
                current_ship->d.ship_type == SHIP_TYPE_COMBAT &&
                current_ship->officer_id == game::NO_LEADER) {
                if (best_ship == nullptr || (best_ship->d.size <= current_ship->d.size && best_ship->d.shield_type <= current_ship->d.shield_type && best_ship->d.armor_type <= current_ship->d.armor_type && best_ship->d.computer_type <= current_ship->d.computer_type && best_ship->crew_experience < current_ship->crew_experience)) {
                    best_ship = current_ship;
                }
            }
        }

        if (best_ship != nullptr) {
            OFFICER::Ai_Assign_Leader_To_Ship(
                *world,
                &world->players[player_index],
                leader_index,
                best_ship);
        }
    }


    void Assign_Unassigned_Leaders(AIDATA::s_state* ai, game::s_game_state* world, game::id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        auto& state = *world;
        for (int16_t leader_index = 0; leader_index < MAX_LEADERS; ++leader_index) {
            auto* leader = &state.leaders.at(leader_index);

            if (player_id != game::NO_PLAYER && leader->employer == player_id) {
                if (leader->status == 0 /* Available/Unassigned */) {
                    if (leader->type == LEADER_TYPE_SHIP) {
                        Assign_Fleet_Officer(world, player_id, leader->id);
                    } else /* Star Officer */ {
                        Assign_Star_Officer(world, ai, player_id, leader->id);
                    }
                } else if (leader->status == 1 /* Assigned */) {
                    // Only fleet officers seem to be reassigned in this manner
                    if (leader->type == LEADER_TYPE_SHIP) {
                        Reassign_Fleet_Officer(world, player_id, leader->id);
                    }
                }
            }
        }
    }
} // namespace aileader
