#include "pch.h"

namespace AISTAGE {

    void Compute_Defensive_Stage_Points(AIDATA::s_state* ai, const game_world::s_view* world, game_world::t_player_id player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        std::vector<uint8_t> processed_stars(world->Star_Count(), 0);
        int16_t selected_stagepoint_count = 0;
        bool finished;
        uint32_t best_weight;
        int16_t best_star;

        if (world->players[player_idx].eliminated != 0) {
            return;
        }

        world->players[player_idx].offensive_stagepoint = -1;

        for (int16_t i = 0; i < world->Star_Count(); ++i) {
            world->stars[i].is_stagepoint &= ~(1 << (player_idx & 0x1F));
        }

        do {
            finished = false;
            best_star = -1;
            best_weight = 0;

            for (int16_t i = 0; i < world->Star_Count(); ++i) {
                if (processed_stars[i] == 0) {
                    uint32_t weight = ai->stars[legacy_world::Star_Id(i)].stage_weights[player_idx];
                    if (best_weight < weight) {
                        best_weight = weight;
                        best_star = i;
                    }
                }
            }

            if (best_star == -1) {
                finished = true;
            } else {
                world->stars[best_star].is_stagepoint |= (1 << (player_idx & 0x1F));

                if (selected_stagepoint_count == 0) {
                    world->players[player_idx].offensive_stagepoint = best_star;
                }

                ++selected_stagepoint_count;
                processed_stars[best_star] = 1;

                for (int16_t i = 0; i < world->Star_Count(); ++i) {
                    if (i != best_star) {
                        if (ai->stars[legacy_world::Star_Id(i)].stage_weights[player_idx] != 0 &&
                            (world->stars[i].is_stagepoint & (1 << (player_idx & 0x1F))) == 0) {
                            auto turns = AIDATA::N_Turns_From_To(&ai->routes, legacy_world::Star_Id(best_star), legacy_world::Star_Id(i), player_id);
                            if (turns && *turns <= 1) {
                                ai->stars[legacy_world::Star_Id(i)].stage_weights[player_idx] = 0;
                                processed_stars[i] = 1;
                            }
                        }
                    }
                }
            }
        } while (!finished);

        if (selected_stagepoint_count == 0) {
            uint32_t max_weight = 0;
            int16_t candidate_star = -1;
            int32_t search_speed = (uint8_t)world->players[player_idx].ship_speed;

            do {
                for (int16_t i = 0; i < world->Star_Count(); ++i) {
                    if (((world->stars[i].has_colony >> (player_idx & 0x1F)) & 1) != 0) {
                        uint8_t in_range_mask = ai->stars[legacy_world::Star_Id(i)].in_range;
                        if ((in_range_mask & ~(1 << (player_idx & 0x1F))) != 0) {
                            for (int16_t other_player_idx = 0; other_player_idx < world->Player_Count(); ++other_player_idx) {
                                if (other_player_idx != player_idx && ((in_range_mask >> (other_player_idx & 0x1F)) & 1) != 0) {
                                    if (SHIPMOVE::Star_In_Range_Of_Player(&world->stars[i], other_player_idx, (int16_t)search_speed)) {
                                        uint32_t weight = ai->stars[legacy_world::Star_Id(i)].base_stage_weights[player_idx];
                                        if (max_weight < weight) {
                                            max_weight = weight;
                                            candidate_star = i;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (candidate_star != -1) break;
                search_speed += 3;
            } while (search_speed < 50);

            if (candidate_star == -1) {
                int16_t ship_range = world->players[player_idx].ship_range;
                for (int16_t owned_star_idx = 0; owned_star_idx < world->Star_Count(); ++owned_star_idx) {
                    if (((world->stars[owned_star_idx].has_colony >> (player_idx & 0x1F)) & 1) != 0) {
                        for (int16_t enemy_colony_star_idx = 0; enemy_colony_star_idx < world->Star_Count(); ++enemy_colony_star_idx) {
                            if (((world->stars[enemy_colony_star_idx].has_colony & ~(1 << (player_idx & 0x1F))) != 0)) {
                                if (GEO::Parsecs_Between_Stars(owned_star_idx, enemy_colony_star_idx) <= ship_range) {
                                    uint32_t weight = ai->stars[legacy_world::Star_Id(owned_star_idx)].base_stage_weights[player_idx];
                                    if (max_weight < weight) {
                                        max_weight = weight;
                                        candidate_star = owned_star_idx;
                                    }
                                }
                            }
                        }
                    }
                }

                if (candidate_star == -1) {
                    for (int16_t i = 0; i < world->Star_Count(); ++i) {
                        if (((world->stars[i].has_colony >> (player_idx & 0x1F)) & 1) != 0) {
                            uint32_t weight = ai->stars[legacy_world::Star_Id(i)].base_stage_weights[player_idx];
                            if (max_weight <= weight) {
                                max_weight = weight;
                                candidate_star = i;
                            }
                        }
                    }
                }
            }

            world->players[player_idx].offensive_stagepoint = candidate_star;
            world->stars[candidate_star].is_stagepoint |= (1 << (player_idx & 0x1F));
        }
    }

} // namespace AISTAGE