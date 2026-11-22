#include "pch.h"

namespace AISTAGE {

    void Compute_Defensive_Stage_Points(AIDATA::s_state* ai, const game::s_view* world, game::id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        auto& state = *world->state;
        int16_t selected_stagepoint_count = 0;
        bool finished;
        uint32_t best_weight;
        game::id_star best_star;

        if (world->players[player_idx].eliminated != 0) {
            return;
        }

        world->players[player_idx].offensive_stagepoint = -1;

        const auto player_bit = 1u << player_idx;
        for (auto& [key, star] : state.stars) star.is_stagepoint &= ~player_bit;

        do {
            finished = false;
            best_star = game::NO_STAR;
            best_weight = 0;

            for (const auto key : state.star_order) {
                if (!(state.stars.at(key).is_stagepoint & player_bit)) {
                    uint32_t weight = ai->stars[key].stage_weights[player_idx];
                    if (best_weight < weight) {
                        best_weight = weight;
                        best_star = key;
                    }
                }
            }

            if (best_star == game::NO_STAR) {
                finished = true;
            } else {
                state.stars.at(best_star).is_stagepoint |= player_bit;

                if (selected_stagepoint_count == 0) {
                    world->players[player_idx].offensive_stagepoint = game::Star_Row(state, best_star);
                }

                ++selected_stagepoint_count;
                for (const auto key : state.star_order) {
                    auto& weight = ai->stars[key].stage_weights[player_idx];
                    if (weight != 0 && !(state.stars.at(key).is_stagepoint & player_bit)) {
                        const auto turns = AIDATA::N_Turns_From_To(&ai->routes, best_star, key, player_id);
                        if (turns && *turns <= 1) weight = 0;
                    }
                }
            }
        } while (!finished);

        if (selected_stagepoint_count == 0) {
            uint32_t max_weight = 0;
            auto candidate_star = game::NO_STAR;
            int32_t search_speed = (uint8_t)world->players[player_idx].ship_speed;

            do {
                for (const auto key : state.star_order) {
                    const auto& star = state.stars.at(key);
                    if (game::Has_Player_Colony(star, player_idx)) {
                        const auto& assessment = ai->stars[key];
                        uint8_t in_range_mask = assessment.in_range;
                        if ((in_range_mask & ~player_bit) != 0) {
                            for (int16_t other_player_idx = 0; other_player_idx < world->Player_Count(); ++other_player_idx) {
                                if (other_player_idx != player_idx && ((in_range_mask >> (other_player_idx & 0x1F)) & 1) != 0) {
                                    if (SHIPMOVE::Star_In_Range_Of_Player(*world, star, other_player_idx, (int16_t)search_speed)) {
                                        uint32_t weight = assessment.base_stage_weights[player_idx];
                                        if (max_weight < weight) {
                                            max_weight = weight;
                                            candidate_star = key;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (candidate_star != game::NO_STAR) break;
                search_speed += 3;
            } while (search_speed < 50);

            if (candidate_star == game::NO_STAR) {
                int16_t ship_range = world->players[player_idx].ship_range;
                for (const auto owned_key : state.star_order) {
                    const auto& from = state.stars.at(owned_key);
                    if (game::Has_Player_Colony(from, player_idx)) {
                        for (const auto target_key : state.star_order) {
                            const auto& to = state.stars.at(target_key);
                            if (game::Has_Other_Colonies(to, player_idx)) {
                                if (GEO::Parsecs_Between_Points(from.x, from.y, to.x, to.y) <= ship_range) {
                                    uint32_t weight = ai->stars[from.id].base_stage_weights[player_idx];
                                    if (max_weight < weight) {
                                        max_weight = weight;
                                        candidate_star = owned_key;
                                    }
                                }
                            }
                        }
                    }
                }

                if (candidate_star == game::NO_STAR) {
                    for (const auto key : state.star_order) {
                        if (game::Has_Player_Colony(state.stars.at(key), player_idx)) {
                            uint32_t weight = ai->stars[key].base_stage_weights[player_idx];
                            if (max_weight <= weight) {
                                max_weight = weight;
                                candidate_star = key;
                            }
                        }
                    }
                }
            }

            if (candidate_star != game::NO_STAR) {
                world->players[player_idx].offensive_stagepoint = game::Star_Row(state, candidate_star);
                state.stars.at(candidate_star).is_stagepoint |= player_bit;
            }
        }
    }

} // namespace AISTAGE
