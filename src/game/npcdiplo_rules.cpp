#include "pch.h"

namespace npcdiplo {
    s_npcdiplo_tech human_tech[TECH_APP_COUNT];
    uint16_t ai_surrender_delay = 150;

    int32_t Tech_Trade_Value(
        int16_t player_idx,
        int16_t other_player_idx,
        int16_t tech_app_idx) {
        s_player* player = &MOX::player[player_idx];
        if (player->tech_applications[tech_app_idx] == TECH_RESEARCH_STATUS_RESEARCHED ||
            !COLCALC::Player_May_Get_Tech_App(player, tech_app_idx)) {
            return 0;
        }

        int32_t tech_value = newtech::Calc_Tech_Value(player_idx, tech_app_idx);
        if (tech_value == 0 &&
            player->objectives == PLAYER_OBJECTIVE_HUMAN &&
            MOX::player[other_player_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            return 1;
        }

        return tech_value;
    }

    int __cdecl reverse_compare_tech(const void* a, const void* b) {
        return ((const s_npcdiplo_tech*)b)->value - ((const s_npcdiplo_tech*)a)->value;
    }

    void __cdecl Get_Differential_Tech_List(
        int16_t player_idx,
        int16_t other_player_idx,
        uint32_t min_tech_value,
        uint32_t max_tech_value,
        uint8_t* tech_list_indices,
        int32_t* tech_list_values,
        int16_t* tech_count,
        int16_t sort_flag) {
        *tech_count = 0;

        newtech::Set_Competition_Tech_Values(player_idx);

        for (int16_t tech_idx = 0; tech_idx < 204; ++tech_idx) {
            if (MOX::player[other_player_idx].tech_applications[tech_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                uint32_t tech_value = Tech_Trade_Value(player_idx, other_player_idx, tech_idx);

                if (tech_idx == TECH_APP_CONFEDERATION && MOX::player[player_idx].traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_FEUDAL) {
                    tech_value = 0;
                }
                if (tech_idx == TECH_APP_IMPERIUM && MOX::player[player_idx].traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_DICTATORSHIP) {
                    tech_value = 0;
                }
                if (tech_idx == TECH_APP_FEDERATION && MOX::player[player_idx].traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_DEMOCRACY) {
                    tech_value = 0;
                }
                if (tech_idx == TECH_APP_GALACTIC_UNIFICATION && MOX::player[player_idx].traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_UNIFICATION) {
                    tech_value = 0;
                }

                if (!COLCALC::Player_May_Get_Tech_App(&MOX::player[player_idx], tech_idx)) {
                    tech_value = 0;
                }

                if (tech_value != 0 && tech_value >= min_tech_value && tech_value <= max_tech_value) {
                    human_tech[*tech_count].index = (uint8_t)tech_idx;
                    human_tech[*tech_count].value = (int32_t)tech_value;
                    *tech_count += 1;
                }
            }
        }

        if (sort_flag == 1 && *tech_count != 0) {
            qsort(human_tech, *tech_count, sizeof(s_npcdiplo_tech), reverse_compare_tech);
        }

        if (*tech_count > 0) {
            for (int16_t i = 0; i < *tech_count; ++i) {
                tech_list_indices[i] = human_tech[i].index;
                tech_list_values[i] = human_tech[i].value;
            }
        }
    }

    int16_t __cdecl NPC_Surrender_Check(int16_t player_idx) {
        s_player* plr = &MOX::player[player_idx];

        // Already surrendered
        if (plr->objectives == PLAYER_OBJECTIVE_HUMAN) {
            return -1;
        }

        // Too early for surrender
        if ((int)(MOX::stardate - 35000) < ai_surrender_delay) {
            return -1;
        }

        int32_t max_pop = 1;
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (i != player_idx && !MOX::player[i].eliminated) {
                if (max_pop < MOX::player[i].total_pop) {
                    max_pop = MOX::player[i].total_pop;
                }
            }
        }

        int32_t max_strength = 1;
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                if (i != player_idx && j != player_idx && i != j) {
                    if (!MOX::player[i].eliminated && !MOX::player[j].eliminated) {
                        if (max_strength < MOX::player[i].total_ship_strength[j]) {
                            max_strength = MOX::player[i].total_ship_strength[j];
                        }
                    }
                }
            }
        }

        int32_t contact_count = 0;
        int32_t total_strength_vs_npc = 0;
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (i != player_idx && plr->contact[i] == 1 && !MOX::player[i].eliminated) {
                contact_count++;
                total_strength_vs_npc += plr->total_ship_strength[i];
            }
        }

        int32_t avg_strength = 0;
        if (contact_count > 0) {
            avg_strength = total_strength_vs_npc / contact_count;
        }

        int32_t pop_ratio = (plr->total_pop * 100) / max_pop;
        int32_t surrender_prob = 0;
        int32_t strength_ratio = (avg_strength * 100) / max_pop;

        if (pop_ratio < 26 || strength_ratio < 26) {
            surrender_prob = 5;
            if (pop_ratio < 26) {
                surrender_prob = ((26 - pop_ratio) * 5) / 12;
            }
            if (strength_ratio < 26) {
                surrender_prob = ((26 - strength_ratio) * surrender_prob) / 12;
            }
        }

        int32_t total_pop = plr->total_pop;
        if (total_pop < 1) total_pop = 1;

        int16_t losses = 0;
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (i != player_idx) {
                losses += plr->colony_war_losses[i];
            }
        }

        int32_t loss_factor = (int32_t)losses;
        loss_factor = ((loss_factor * loss_factor * 100) / (total_pop + loss_factor)) / (total_pop + loss_factor);

        if (surrender_prob < loss_factor) {
            surrender_prob = loss_factor;
        }

        if ((int32_t)game_random::Random(100) > surrender_prob) {
            return -1;
        }

        int16_t best_target = -1;
        int16_t best_pop = 0;

        // Search 1: Pick best contact (max pop) with no war.
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (plr->contact[i] == 1 && i != player_idx && !MOX::player[i].eliminated) {
                if (!COLCALC::May_Surrender_Empire(player_idx, i)) continue;
                if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) continue;
                if (plr->treaty[i] >= 4 /* NO_WAR */) continue;
                if (best_pop <= MOX::player[i].total_pop) {
                    best_target = i;
                    best_pop = MOX::player[i].total_pop;
                }
            }
        }

        if (best_target == -1) {
            // Search 2: Pick best contact with minimal pop.
            best_pop = 0;
            for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
                if (plr->contact[i] == 1 && i != player_idx && !MOX::player[i].eliminated) {
                    if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN &&
                        COLCALC::May_Surrender_Empire(player_idx, i)) {
                        if (best_pop > MOX::player[i].total_pop) {
                            best_target = i;
                            best_pop = MOX::player[i].total_pop;
                        }
                    }
                }
            }
        }

        if (best_target == -1 && MOX::settings.game_difficulty < GAME_DIFFICULTY_HARD) {
            // Search 3: Low difficulty fallback.
            best_pop = 0;
            for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
                if (plr->contact[i] == 1 && i != player_idx && !MOX::player[i].eliminated) {
                    if (COLCALC::May_Surrender_Empire(player_idx, i) &&
                        plr->treaty[i] < 4 /* NO_WAR */) {
                        if (best_pop > MOX::player[i].total_pop) {
                            best_target = i;
                            best_pop = MOX::player[i].total_pop;
                        }
                    }
                }
            }
        }

        if (best_target == -1 && MOX::settings.game_difficulty < GAME_DIFFICULTY_HARD) {
            // Search 4: Final low difficulty fallback (Max Pop)
            best_pop = 0;
            for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
                if (plr->contact[i] == 1 && i != player_idx && !MOX::player[i].eliminated) {
                    if (COLCALC::May_Surrender_Empire(player_idx, i)) {
                        if (best_pop < MOX::player[i].total_pop) {
                            best_target = i;
                            best_pop = MOX::player[i].total_pop;
                        }
                    }
                }
            }
        }

        return best_target;
    }
} // namespace npcdiplo
