#include "pch.h"

namespace npcdiplo {
    s_npcdiplo_tech target_tech[TECH_APP_COUNT];

    void __cdecl Clear_Last_Proposal_Turn(int16_t player_id_1, int16_t player_id_2) {
        MOX::player[player_id_1].last_diplomacy_proposal_turn[player_id_2] = 0;
        MOX::player[player_id_2].last_diplomacy_proposal_turn[player_id_1] = 0;
    }

    void __cdecl NPC_Diplomacy_Buildup_Penalties() {
        // Only run every 2 stardates after date 35000
        int32_t date_diff = (int32_t)MOX::stardate - 35000;
        if (date_diff % 2 != 0) {
            return;
        }

        if (MOX::NUM_PLAYERS <= 0) {
            return;
        }

        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < MOX::NUM_PLAYERS; ++j) {
                if (i == j) {
                    continue;
                }

                // Skip if either player is pursuing "objective" status (e.g., Guardian/Antaran)
                if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    continue;
                }

                if (MOX::player[j].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    continue;
                }

                // Only consider active players with colonies
                if (MOX::player[j].total_colonies <= 0) {
                    continue;
                }

                // Check for non-aggression or alliance treaties
                int8_t treaty_status = MOX::player[j].treaty[i];
                if (treaty_status == 2 || treaty_status == 3) {
                    continue;
                }

                int32_t galaxy_size = Galaxy_Size_For_Classic_Rules_(
                    MOX::settings.galaxy_size);
                int32_t threshold = galaxy_size * 3 + 3;
                if (threshold >= MOX::player[i].total_colonies) {
                    continue;
                }

                // 25% chance per eligible pair per turn to apply penalty
                if (game_random::Random(4) == 1) {
                    int32_t difficulty_mod =
                        (galaxy_size - MOX::settings.game_difficulty) + 6;
                    int32_t expansion_excess = threshold - MOX::player[i].total_colonies;

                    // Note: expansion_excess is negative here
                    int32_t base_penalty = (expansion_excess / difficulty_mod) * game_random::Random(4);
                    int16_t relations_change = (int16_t)((base_penalty / 3) * 4);
                    if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN) {
                        relations_change /= 2;
                    }

                    diplomac::Change_Relations(relations_change, i, j, 8 /* Expansion buildup penalty */, 0, 0);
                }
            }
        }
    }

    void __cdecl NPC_Diplomacy() {
        diplomac::NPC_Sneak_Attacks();
        NPC_Diplomacy_Buildup_Penalties();
        NPC_Threats();
        NPC_To_NPC_Treaty_Negotiations();
        NoOp3();
        NPC_Declarations_Of_War();
        NPC_Peace_Negotiations();
        NPC_Tech_Exchange();
    }

    void __cdecl NPC_Threats() {
        for (int p1_idx = 0; p1_idx < MOX::NUM_PLAYERS; ++p1_idx) {
            s_player& p1 = MOX::player[p1_idx];
            if (p1.objectives == PLAYER_OBJECTIVE_HUMAN) {
                continue;
            }

            for (int p2_idx = 0; p2_idx < MOX::NUM_PLAYERS; ++p2_idx) {
                s_player& p2 = MOX::player[p2_idx];
                if (p2.objectives == PLAYER_OBJECTIVE_HUMAN) {
                    continue;
                }

                int16_t severity = p1.diplomacy_severity[p2_idx];
                if (severity == 0) {
                    continue;
                }

                if (p1.diplomacy_incident[p2_idx] != 0 && severity != 10000) {
                    int8_t increment = 1;

                    // Treaty modifier check: Alliance, Peace, or Trade/Research/Tribute
                    if ((p1.treaty[p2_idx] >= 1 && p1.treaty[p2_idx] <= 3) ||
                        p1.trade_treaty[p2_idx] != 0 ||
                        p1.research_treaty[p2_idx] != 0 ||
                        p1.tribute_treaty[p2_idx] < 0) {
                        increment = 2;
                    }

                    if (severity > 0) {
                        // Positive severity reduces threats over time
                        if ((int16_t)game_random::Random(100) < (severity * 2)) {
                            p1.threats[p2_idx]--;
                            if (p1.threats[p2_idx] < 0) {
                                p1.threats[p2_idx] = 0;
                            }
                            p2.threats[p1_idx] = p1.threats[p2_idx];
                        } else {
                            // Resolve incident if no threat reduction occurred
                            p1.diplomacy_incident[p2_idx] = 0;
                        }
                    } else {
                        // Negative severity (diplomatic insult/attack) increases threats
                        int16_t threat_threshold;
                        if (p1.personality == PLAYER_PERSONALITY_HONORABLE && p1.dishonored_flag[p2_idx] == 1) {
                            threat_threshold = diplodef::personality_threat_modifiers[PLAYER_PERSONALITY_DISHONORED];
                        } else {
                            threat_threshold = diplodef::personality_threat_modifiers[p1.personality];
                        }

                        // Severity is negative, so subtracting it increases the random threshold
                        if ((int16_t)game_random::Random(100) < (threat_threshold - severity * 2) || increment == 2) {
                            p1.threats[p2_idx] += increment;
                            p2.threats[p1_idx] = p1.threats[p2_idx];
                        } else {
                            // Resolve incident if no threat increase occurred
                            p1.diplomacy_incident[p2_idx] = 0;
                        }
                    }
                }
            }
        }
    }

    void __cdecl NPC_Declarations_Of_War() {
        int16_t decision[MAX_PLAYERS];
        int16_t reason[MAX_PLAYERS];

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            auto& p_source = MOX::player[i];

            if (p_source.objectives == PLAYER_OBJECTIVE_HUMAN || p_source.eliminated) {
                continue;
            }

            int16_t flux = EVENTS::Event_Check_Hyperspace_Flux();
            if (flux != 0) {
                flux = EVENTS::Event_Check_Hyperspace_Flux();
                if (flux != 1 || p_source.traits[TRAIT_TRANS_DIMENSIONAL] != 1) {
                    continue;
                }
            }

            for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                if (j != i && p_source.treaty[j] >= 4 && p_source.contact[j] == 1) {
                    decision[j] = 3;
                } else {
                    decision[j] = 0;
                }
                reason[j] = 0;
            }

            // Case 1: Ruthless Aggression
            if (p_source.personality == PLAYER_PERSONALITY_ERRATIC) {
                for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                    auto& p_target = MOX::player[j];
                    if (j != i && decision[j] == 0 && p_source.within_range[j] == 1 &&
                        p_source.peace_duration[j] < 1 && !p_target.eliminated) {
                        if (diplomac::Fleet_Strength_Comparison(i, j) >= 100 && p_target.objectives != PLAYER_OBJECTIVE_HUMAN) {
                            int diff = MOX::settings.game_difficulty;
                            int roll_limit = (diff * diff * 2 + 2) * 15;
                            if (game_random::Random(roll_limit) == 1) {
                                decision[j] = 1;
                                reason[j] = 20 /* PERSONALITY_AGGRESSION */;
                            }
                        }
                    }
                }
            }

            // Case 2: Opportunistic Attack based on Relations
            for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                auto& p_target = MOX::player[j];
                if (j != i && p_target.objectives != PLAYER_OBJECTIVE_HUMAN && !p_target.eliminated) {
                    if (p_source.contact[j] == 1 && p_source.within_range[j] == 1 &&
                        p_source.peace_duration[j] < 1 &&
                        decision[j] == 0) {
                        if (diplomac::Fleet_Strength_Comparison(i, j) >= 100 && (int)MOX::stardate % MOX::NUM_PLAYERS == j) {
                            int diff = MOX::settings.game_difficulty;
                            int threshold = (-5 - p_source.relations[j]) / (diff * 2 + 1);
                            if ((int16_t)game_random::Random(100) <= threshold) {
                                decision[j] = 1;
                                reason[j] = 68 /* OPPORTUNISTIC_ATTACK */;
                            }
                        }
                    }
                }
            }

            // Case 3: Failed to Expand
            if (p_source.failed_to_expand == 1) {
                for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                    auto& p_target = MOX::player[j];
                    if (j != i && p_target.objectives != PLAYER_OBJECTIVE_HUMAN && !p_target.eliminated) {
                        if (p_source.contact[j] == 1 && p_source.within_range[j] == 1 &&
                            p_source.peace_duration[j] < 1 && decision[j] == 0) {
                            if (diplomac::Fleet_Strength_Comparison(i, j) >= 100) {
                                decision[j] = 1;
                                reason[j] = 113 /* EXPANSION_FAILURE */;
                            }
                        }
                    }
                }
            }

            // Case 4: Food Shortage desperation
            if ((int16_t)game_random::Random(100) < p_source.food_shortage_duration) {
                for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                    auto& p_target = MOX::player[j];
                    if (j != i && p_target.objectives != PLAYER_OBJECTIVE_HUMAN && !p_target.eliminated) {
                        if (p_source.contact[j] == 1 && p_source.within_range[j] == 1 &&
                            p_source.peace_duration[j] < 1 && decision[j] == 0) {
                            if (diplomac::Fleet_Strength_Comparison(i, j) >= 100) {
                                decision[j] = 1;
                                reason[j] = 113 /* HUNGER_ATTACK */;
                            }
                        }
                    }
                }
            }

            // Case 5: Revenge for colony losses
            for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                auto& p_target = MOX::player[j];
                if (j != i && p_target.objectives != PLAYER_OBJECTIVE_HUMAN && !p_target.eliminated) {
                    if (p_source.contact[j] == 1 && p_source.within_range[j] == 1 &&
                        p_source.peace_duration[j] < 1 && decision[j] == 0) {
                        if (p_source.colony_war_losses[j] > 0) {
                            decision[j] = 2;
                            reason[j] = 22 /* REVENGE */;
                        }
                    }
                }
            }

            // Case 6: Suppress declarations against targets already at war with an objective player
            for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                auto& p_target = MOX::player[j];
                if (j != i && p_target.objectives != PLAYER_OBJECTIVE_HUMAN && !p_target.eliminated) {
                    for (int k = 0; k < MOX::NUM_PLAYERS; ++k) {
                        auto& p_other = MOX::player[k];
                        if (k != i && k != j && p_other.objectives == PLAYER_OBJECTIVE_HUMAN && !p_other.eliminated) {
                            if (p_other.treaty[j] >= 4) {
                                decision[j] = 0;
                            }
                        }
                    }
                }
            }

            // Case 7: Detailed personality threshold comparison
            for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                auto& p_target = MOX::player[j];
                if (j != i && p_target.objectives != PLAYER_OBJECTIVE_HUMAN && !p_target.eliminated) {
                    if (p_source.contact[j] == 1 && p_source.within_range[j] == 1 &&
                        decision[j] == 0 && p_source.peace_duration[j] < 1 &&
                        (int)MOX::stardate % MOX::NUM_PLAYERS == j) {
                        int personality_idx = diplomac::Get_Player_Diplomacy_Personality(i, j);
                        int threshold = diplodef::personality_relation_modifiers[personality_idx] * 3 +
                            game_random::Random(200) + (MOX::settings.game_difficulty + 5) * 25;

                        int8_t t = p_source.treaty[j];
                        if (t == 1)
                            threshold += 50;
                        else if (t == 2)
                            threshold += 100;
                        else if (t == 3)
                            threshold += 200;

                        if (p_source.trade_treaty[j]) threshold += 50;
                        if (p_source.research_treaty[j]) threshold += 50;
                        if (p_source.tribute_treaty[j] == 1)
                            threshold += 50;
                        else if (p_source.tribute_treaty[j] == 2)
                            threshold += 100;

                        for (int k = 0; k < MOX::NUM_PLAYERS; ++k) {
                            if (k != i && k != j && !MOX::player[k].eliminated &&
                                MOX::player[k].objectives == PLAYER_OBJECTIVE_HUMAN && MOX::player[k].within_range[j] == 1) {
                                int strength = diplomac::Fleet_Strength_Comparison(k, i);
                                if (strength > 100) {
                                    threshold -= diplomac::Fleet_Strength_Comparison(k, i);
                                }
                            }
                        }

                        if (diplomac::Fleet_Strength_Comparison(i, j) >= threshold) {
                            decision[j] = 1;
                            reason[j] = 23 /* CALCULATED_ATTACK */;
                        }
                    }
                }
            }

            // Case 8: Alliance protection logic (Difficulty >= Average)
            if (MOX::settings.game_difficulty >= GAME_DIFFICULTY_HARD) {
                for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                    if (j != i && !MOX::player[j].eliminated && MOX::player[j].objectives != PLAYER_OBJECTIVE_HUMAN) {
                        for (int k = 0; k < MOX::NUM_PLAYERS; ++k) {
                            if (k != i && k != j && !MOX::player[k].eliminated && MOX::player[k].objectives != PLAYER_OBJECTIVE_HUMAN) {
                                if (MOX::player[k].treaty[j] >= 4) {
                                    decision[j] = 0;
                                }
                            }
                        }
                    }
                }
            }

            // Execution Loop 1: Immediate/Forced Declarations
            for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                if (j != i && decision[j] == 2 && p_source.peace_duration[j] < 1) {
                    if (MOX::player[j].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        diplomac::Change_Relations(-10000, j, i, reason[j], 0, 0);
                    }
                    diplomac::Declare_War(i, j);
                }
            }

            // Execution Loop 2: Randomly pick ONE from potential declarations
            int protective_treaties = 0;
            int potential_targets_count = 0;
            for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                if (j != i && p_source.peace_duration[j] < 1) {
                    if (p_source.treaty[j] >= 4 || p_source.sneak_attack_planet != -1) {
                        protective_treaties++;
                    }
                    if (decision[j] == 1) {
                        potential_targets_count++;
                    }
                }
            }

            if (protective_treaties == 0 && potential_targets_count > 0) {
                int target_idx = game_random::Random(potential_targets_count);
                for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                    if (j != i && decision[j] == 1) {
                        if (--target_idx == 0) {
                            if (MOX::player[j].objectives == PLAYER_OBJECTIVE_HUMAN) {
                                diplomac::Change_Relations(-10000, j, i, reason[j], 0, 0);
                            }
                            diplomac::Declare_War(i, j);
                            break;
                        }
                    }
                }
            }
        }
    }

    void __cdecl NPC_Tech_Exchange() {
        if (MOX::NUM_PLAYERS <= 0) {
            return;
        }

        for (int16_t p1_idx = 0; p1_idx < (int16_t)MOX::NUM_PLAYERS; ++p1_idx) {
            s_player* p1 = &MOX::player[p1_idx];

            if (p1->objectives != PLAYER_OBJECTIVE_HUMAN && p1->traits[TRAIT_REPULSIVE] == 0) {
                if (MOX::stardate % (int32_t)MOX::NUM_PLAYERS == (int32_t)p1_idx) {
                    for (int16_t p2_idx = 0; p2_idx < (int16_t)MOX::NUM_PLAYERS; ++p2_idx) {
                        if (p1_idx == p2_idx) continue;

                        s_player* p2 = &MOX::player[p2_idx];
                        if (p2->objectives != PLAYER_OBJECTIVE_HUMAN && p2->traits[TRAIT_REPULSIVE] == 0 && p1->contact[p2_idx] != 0) {
                            if ((int16_t)game_random::Random(100) < (int)p1->relations[p2_idx] + 50) {
                                if ((int16_t)game_random::Random(4) <= (int)MOX::settings.game_difficulty) {
                                    int16_t best_tech_for_p1 = -1;
                                    uint32_t max_val_for_p1 = 0;
                                    int16_t best_tech_for_p2 = -1;
                                    uint32_t max_val_for_p2 = 0;

                                    for (int16_t tech_id = 1; tech_id < 204; ++tech_id) {
                                        // P1 offers to P2
                                        if (p1->tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED && p2->tech_applications[tech_id] != TECH_RESEARCH_STATUS_RESEARCHED) {
                                            if (COLCALC::Player_May_Get_Tech_App(p2, tech_id)) {
                                                int16_t field_id = TECHDATA::technology_applications[tech_id].tech_field_id;
                                                uint32_t cost = TECHDATA::technology_fields[field_id].cost;
                                                if (TECHDATA::technology_applications[tech_id].ai_group == 39) {
                                                    cost *= 1000;
                                                }
                                                if (cost > max_val_for_p2) {
                                                    max_val_for_p2 = cost;
                                                    best_tech_for_p2 = tech_id;
                                                }
                                            }
                                        }

                                        // P2 offers to P1
                                        if (p2->tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED && p1->tech_applications[tech_id] != TECH_RESEARCH_STATUS_RESEARCHED) {
                                            if (COLCALC::Player_May_Get_Tech_App(p1, tech_id)) {
                                                int16_t field_id = TECHDATA::technology_applications[tech_id].tech_field_id;
                                                uint32_t cost = TECHDATA::technology_fields[field_id].cost;
                                                if (TECHDATA::technology_applications[tech_id].ai_group == 39) {
                                                    cost *= 1000;
                                                }
                                                if (cost > max_val_for_p1) {
                                                    max_val_for_p1 = cost;
                                                    best_tech_for_p1 = tech_id;
                                                }
                                            }
                                        }
                                    }

                                    if (best_tech_for_p1 != -1 && best_tech_for_p2 != -1) {
                                        COLCALC::Player_Gets_Tech_App(p1, best_tech_for_p1);
                                        COLCALC::Player_Gets_Tech_App(p2, best_tech_for_p2);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void __cdecl NPC_Peace_Negotiations() {
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            auto& p_i = MOX::player[i];

            if (p_i.objectives == PLAYER_OBJECTIVE_HUMAN || p_i.eliminated != 0) {
                continue;
            }

            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                if (i == j) {
                    continue;
                }

                auto& p_j = MOX::player[j];

                if (p_j.objectives == PLAYER_OBJECTIVE_HUMAN || p_j.eliminated != 0) {
                    continue;
                }

                // Check if at war (treaty 4 or 5) and in contact
                if ((p_i.treaty[j] == 4 || p_i.treaty[j] == 5) && p_i.contact[j] == 1) {
                    int16_t war_count = 0;
                    for (int16_t k = 0; k < (int16_t)MOX::NUM_PLAYERS; ++k) {
                        auto& p_k = MOX::player[k];
                        if (p_k.objectives == PLAYER_OBJECTIVE_HUMAN && p_k.eliminated == 0) {
                            if (i != k && j != k && p_i.treaty[k] >= 4) {
                                war_count++;
                            }
                        }
                    }

                    int threshold = (6 - MOX::settings.game_difficulty) * 15 - (int)war_count * 20;

                    if (threshold < (int)(uint8_t)p_i.time_since_last_attack[j]) {
                        int16_t surrender_to = NPC_Surrender_Check(i);
                        if (surrender_to == -1) {
                            diplomac::Declare_Peace(j, i);
                        } else {
                            COLCALC::Surrender_Empire(i, surrender_to);
                            EVENTS::Event_Surrender((int8_t)i, surrender_to);
                        }
                    }
                }
            }
        }
    }

    void __cdecl NPC_Treaty_Hatred() {
        if (MOX::settings.game_difficulty < GAME_DIFFICULTY_HARD) {
            return;
        }

        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            s_player& p_i = MOX::player[i];

            if (p_i.objectives == PLAYER_OBJECTIVE_HUMAN) {
                int16_t hatred_score = 0;

                for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                    if (j != i && p_i.contact[j]) {
                        if (p_i.treaty[j] == 1 /* Non-aggression treaty */) {
                            hatred_score += 100;
                        } else if (p_i.treaty[j] == 2 /* Alliance treaty */) {
                            hatred_score += 200;
                        }

                        if (p_i.trade_treaty[j] != 0) {
                            hatred_score += 50;
                        }

                        if (p_i.research_treaty[j] != 0) {
                            hatred_score += 50;
                        }
                    }
                }

                // Resistance to jealousy if Xeno Psychology learned
                if (p_i.tech_applications[TECH_APP_XENO_PSYCHOLOGY] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    hatred_score = (int16_t)((hatred_score * 2) / 3);
                }

                // Reduction on Hard difficulty compared to Impossible
                if (MOX::settings.game_difficulty == GAME_DIFFICULTY_HARD) {
                    hatred_score /= 2;
                }

                if (hatred_score > 0) {
                    for (int16_t k = 0; k < (int16_t)MOX::NUM_PLAYERS; ++k) {
                        s_player& p_k = MOX::player[k];

                        // Check for contact and ensure p_k is not also looking for a diplomatic win 'd'
                        if (p_k.objectives != PLAYER_OBJECTIVE_HUMAN &&
                            p_i.contact[k] &&
                            p_i.treaty[k] == 0 &&
                            p_i.research_treaty[k] == 0 &&
                            p_i.trade_treaty[k] == 0 &&
                            p_k.personality != PLAYER_PERSONALITY_PACIFIST) {
                            // Relationship penalty based on total treaties held by the winner candidate
                            p_i.relations[k] -= (int8_t)(hatred_score / 100);

                            // Probabilistic reduction for the remainder
                            if ((int32_t)game_random::Random(100) <= (hatred_score % 100)) {
                                p_i.relations[k] -= 1;
                            }

                            // Clamp relationship minimum
                            if (p_i.relations[k] < -100) {
                                p_i.relations[k] = -100;
                            }

                            // Mirror relationship changes
                            p_k.relations[i] = p_i.relations[k];
                        }
                    }
                }
            }
        }
    }

    void __cdecl NoOp3() {
        return;
    }

    /**
     * Checks if an NPC player should surrender and identifies a target empire to surrender to.
     */
    void __cdecl Eliminate_NPC_Treaty_Conflicts() {
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            int16_t k = i + 1;

            if (k >= (int16_t)MOX::NUM_PLAYERS) {
                break;
            }

            if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN) {
                for (int16_t j = k; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                    if (MOX::player[j].objectives != PLAYER_OBJECTIVE_HUMAN) {
                        /*
                         * Break player i's treaties with both empires when their
                         * existing treaty state conflicts with player i's pair of treaties.
                         */
                        if (MOX::player[j].treaty[k] >= 4 &&
                            MOX::player[i].treaty[j] == 2 &&
                            MOX::player[i].treaty[k] == 2) {
                            diplomac::Break_Treaties(i, j);
                            diplomac::Break_Treaties(i, k);
                        }
                    }
                }
            }
        }
    }

    void __cdecl Honor_Alliances(int16_t player_idx, int16_t ally_idx) {
        if (MOX::player[player_idx].treaty[ally_idx] == 2 /* TREATY_ALLIANCE */) {
            for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
                if (i == player_idx) {
                    continue;
                }

                if (MOX::player[ally_idx].treaty[i] >= 4 /* TREATY_WAR */ &&
                    MOX::player[player_idx].treaty[i] < 4 /* TREATY_WAR */ &&
                    MOX::player[player_idx].contact[i] == 1 &&
                    MOX::player[player_idx].within_range[i] == 1) {
                    if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        diplomac::Change_Relations(-10000, i, player_idx, 19 /* 0x13 */, 0, 0);
                    }

                    MOX::player[player_idx].diplomacy_proposal_war_player[ally_idx] = (int8_t)i;
                    diplomac::Declare_War(player_idx, i);
                }
            }
        }
    }

    void __cdecl NPC_To_NPC_Treaty_Negotiations() {
        if (MOX::NUM_PLAYERS <= 0) {
            Eliminate_NPC_Treaty_Conflicts();
            return;
        }

        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            s_player& plr_i = MOX::player[i];
            if (plr_i.objectives == PLAYER_OBJECTIVE_HUMAN) continue;

            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                s_player& plr_j = MOX::player[j];

                if (plr_j.objectives == PLAYER_OBJECTIVE_HUMAN) continue;
                if (plr_i.traits[TRAIT_REPULSIVE] != 0) continue;
                if (plr_j.traits[TRAIT_REPULSIVE] != 0) continue;
                if (i == j) continue;

                if (game_random::Random(250 - (40 * (int32_t)MOX::settings.game_difficulty)) != 1) continue;
                if (plr_i.contact[j] != 1) continue;

                if (plr_i.treaty[j] < 4 && plr_i.treaty_modifier[j] > -30) {
                    int32_t score_base = (int32_t)plr_i.relations[j] + (int32_t)plr_i.broken_treaties_modifier[j];

                    if (plr_i.treaty[j] == 1 /* TREATY_PEACE */) score_base += 20;
                    if (plr_i.treaty[j] == 2 /* TREATY_NON_AGGRESSION */) score_base += 40;
                    if (plr_i.trade_treaty[j] != 0) score_base += 10;
                    if (plr_i.trade_treaty[j] != 0) score_base += 10;

                    int32_t common_score = score_base;
                    for (int16_t k = 0; k < (int16_t)MOX::NUM_PLAYERS; ++k) {
                        if (k != i && k != j) {
                            if (plr_i.treaty[k] >= 4) common_score += 10;
                            if (plr_j.treaty[k] >= 4) common_score += 20;
                            if (plr_j.threats[k] > 0) common_score += 5;
                        }
                    }

                    int32_t num_player_allies = 0;
                    int32_t num_nonplayer_allies = 0;
                    for (int16_t k = 0; k < (int16_t)MOX::NUM_PLAYERS; ++k) {
                        if (k != i && k != j) {
                            if (plr_j.treaty[k] >= 4) {
                                if (MOX::player[k].objectives == PLAYER_OBJECTIVE_HUMAN) {
                                    num_player_allies++;
                                } else {
                                    num_nonplayer_allies++;
                                }
                            }
                            if (plr_i.treaty[k] >= 4) {
                                if (MOX::player[k].objectives == PLAYER_OBJECTIVE_HUMAN) {
                                    num_player_allies++;
                                } else {
                                    num_nonplayer_allies++;
                                }
                            }
                        }
                    }

                    int32_t alliance_score = common_score +
                        diplodef::personality_relation_modifiers[(uint8_t)plr_j.personality] +
                        diplodef::personality_relation_modifiers[(uint8_t)plr_i.personality] +
                        plr_i.treaty_modifier[j] + game_random::Random(100) + (num_player_allies * 50);

                    if (alliance_score >= 200 && plr_i.treaty[j] == 1 /* TREATY_PEACE */ && num_nonplayer_allies == 0) {
                        diplomac::Start_Treaty(i, j, 2 /* ALLIANCE_TYPE */);
                    } else if (alliance_score >= 100 && plr_i.treaty[j] != 1 && plr_i.treaty[j] != 2) {
                        diplomac::Start_Treaty(i, j, 1 /* PEACE_TYPE */);
                    }

                    int32_t trade_score = common_score +
                        diplodef::personality_relation_modifiers[(uint8_t)plr_j.personality] +
                        plr_i.trade_modifier[j] + game_random::Random(100);

                    if (trade_score >= 111) {
                        if (plr_i.research_treaty[j] == 0) {
                            plr_i.research_treaty[j] = 1;
                            plr_j.research_treaty[i] = 1;
                        }
                    } else if (trade_score > 80) {
                        if (plr_i.trade_treaty[j] == 0) {
                            plr_i.trade_treaty[j] = 1;
                            plr_j.trade_treaty[i] = 1;
                        }
                    }

                    int32_t tribute_score_val = common_score +
                        diplodef::personality_relation_modifiers[(uint8_t)plr_j.personality] +
                        diplodef::personality_relation_modifiers[(uint8_t)plr_i.personality] +
                        plr_i.treaty_modifier[j] + game_random::Random(100);

                    bool is_paying_tribute = false;
                    if (MOX::NUM_PLAYERS > 0) {
                        int16_t n = (int16_t)MOX::NUM_PLAYERS;
                        do {
                            if (plr_i.tribute_treaty[j] < 0) {
                                is_paying_tribute = true;
                            }
                        } while (--n != 0);
                    }

                    if (tribute_score_val > 150 && !is_paying_tribute) {
                        if (diplomac::Fleet_Strength_Comparison(i, j) < 100) {
                            if (plr_i.bc_produced < plr_j.bc_produced &&
                                (int32_t)game_random::Random(20) <= (int32_t)MOX::settings.game_difficulty + 1) {
                                game_random::Random(4);
                                diplomac::Start_Tribute_Treaty(i, j, 2);

                                int32_t rel_change = game_random::Random(3) + 3;
                                diplomac::Change_Relations((int16_t)rel_change, i, j, 14 /* TRIBUTE_START */, 0, 0);
                            }
                        }
                    }
                }

                Honor_Alliances(i, j);
                diplomac::Adjust_Diplomat_Modifiers(i, j);
                diplomac::Adjust_Diplomat_Modifiers(i, j);
                diplomac::Adjust_Diplomat_Modifiers(i, j);
            }
        }

        Eliminate_NPC_Treaty_Conflicts();
    }

    /**
     * @brief Adjusts the diplomacy proposal rejection value between two players based on personality and acceptance.
     *
     * This function is called when a diplomatic proposal is either accepted or rejected.
     * It modifies the rejection counter which tracks the player's history of rejecting proposals from another player.
     *
     * @param player_idx The index of the player whose personality influences the change.
     * @param other_player_idx The index of the other involved player.
     * @param accepted Whether the proposal was accepted (1) or rejected (0).
     */
    void __cdecl NPC_Proposal_Rejection_Accept(int16_t player_idx, int16_t other_player_idx, int8_t accepted) {
        s_player* player = &MOX::player[player_idx];
        int16_t rejection_val = player->diplomacy_proposal_rejection[other_player_idx];

        if (accepted == 1) {
            if (rejection_val < 0) {
                switch (player->personality) {
                    case PLAYER_PERSONALITY_XENOPHOBIC:
                        rejection_val++;
                        break;
                    case PLAYER_PERSONALITY_RUTHLESS:
                    case PLAYER_PERSONALITY_AGGRESSIVE:
                    case PLAYER_PERSONALITY_ERRATIC:
                    case PLAYER_PERSONALITY_HONORABLE:
                    case PLAYER_PERSONALITY_PACIFIST:
                        rejection_val = 1;
                        break;
                }
            } else {
                rejection_val++;
            }
        } else {
            // Case for rejection (accepted != 1)
            if (rejection_val <= 0) {
                rejection_val--;
            } else {
                switch (player->personality) {
                    case PLAYER_PERSONALITY_XENOPHOBIC:
                        rejection_val = -1;
                        break;
                    case PLAYER_PERSONALITY_RUTHLESS:
                    case PLAYER_PERSONALITY_AGGRESSIVE:
                    case PLAYER_PERSONALITY_ERRATIC:
                        rejection_val /= 2;
                        break;
                    case PLAYER_PERSONALITY_HONORABLE:
                        if (player->dishonored_flag[other_player_idx] == 1) {
                            rejection_val = -1;
                        }
                        break;
                    case PLAYER_PERSONALITY_PACIFIST:
                        rejection_val--;
                        break;
                }
            }
        }

        player->diplomacy_proposal_rejection[other_player_idx] = rejection_val;
        MOX::player[other_player_idx].diplomacy_proposal_rejection[player_idx] = rejection_val;
    }

    bool __cdecl NPC_Good_Proposal_Check(int16_t npc_idx, int16_t player_idx) {
        s_player* npc = &MOX::player[npc_idx];
        s_player* other = &MOX::player[player_idx];

        // Skip proposal generation while already in a high-conflict treaty state.
        if (npc->treaty[player_idx] >= 4) {
            return false;
        }

        int bonus = COLCALC::Best_Leader_Diplomat_Bonus(player_idx);
        int score = bonus / -15;

        score -= diplodef::personality_relation_modifiers[(uint8_t)npc->personality] / 5;
        score -= npc->broken_treaties_modifier[player_idx] / 20;
        score += npc->relations[player_idx] / 20;
        score += -npc->treaty_modifier[player_idx] / 20;
        score += npc->diplomacy_proposal_rejection[player_idx] * 3;
        score += 10 + MOX::settings.game_difficulty * 4;

        if (other->treaty[npc_idx] == 1 /* TREATY_NON_AGGRESSION */) {
            score -= 4;
        } else if (other->treaty[npc_idx] == 2 /* TREATY_TRADE_AGREEMENT */) {
            score -= 8;
        }

        // Xeno Psychology makes diplomatic proposals slightly more likely.
        if (other->tech_applications[TECH_APP_XENO_PSYCHOLOGY] == TECH_RESEARCH_STATUS_RESEARCHED) {
            score -= 5;
        }

        if (score < 0) {
            score = 0;
        }
        if (score > 40) {
            score = 40;
        }

        int divisor = (score * score) / 50 + 1;
        int threshold = (int)npc->last_diplomacy_proposal_turn[player_idx] / divisor;

        int random_val = game_random::Random(100);

        return random_val <= threshold;
    }

    int __cdecl compare_tech(const void* a, const void* b) {
        return ((const s_npcdiplo_tech*)a)->value - ((const s_npcdiplo_tech*)b)->value;
    }

    int16_t __cdecl Get_Tech_Exchange_Reaction(int16_t player_idx, int16_t target_idx, int16_t reaction_bonus_flag) {
        int32_t rand_val = game_random::Random(150);

        int32_t reaction = MOX::player[player_idx].diplomacy_proposal_rejection[target_idx] * 20 + COLCALC::Best_Leader_Diplomat_Bonus(player_idx) + (GAME_DIFFICULTY_HARD - MOX::settings.game_difficulty) * 50 + MOX::player[player_idx].broken_treaties_modifier[target_idx] + MOX::player[player_idx].relations[target_idx];

        if (MOX::player[player_idx].traits[TRAIT_CHARISMATIC] != 0) {
            reaction += 50;
        }

        if (MOX::player[player_idx].tech_applications[TECH_APP_XENO_PSYCHOLOGY] == TECH_RESEARCH_STATUS_RESEARCHED) {
            reaction += 50;
        }

        int32_t personality_modifier;
        if (MOX::player[target_idx].personality == PLAYER_PERSONALITY_HONORABLE && MOX::player[target_idx].dishonored_flag[player_idx] == 1) {
            personality_modifier = diplodef::personality_relation_modifiers[PLAYER_PERSONALITY_DISHONORED];
        } else {
            personality_modifier = diplodef::personality_relation_modifiers[MOX::player[target_idx].personality];
        }

        reaction += personality_modifier;

        if (reaction_bonus_flag == 1) {
            reaction += 150;
        } else {
            reaction += rand_val + MOX::player[player_idx].tech_exchange_modifier[target_idx] - 1;
        }

        if (reaction < 0) {
            return (int16_t)(10000 / (100 - reaction));
        }

        return (int16_t)(reaction + 100);
    }

    void __cdecl NPC_Message_Rejection_Check(int16_t player_idx, int16_t target_idx, int16_t* msg_type) {
        int16_t rejection_val = MOX::player[player_idx].diplomacy_proposal_rejection[target_idx];
        if (rejection_val > -4) {
            return;
        }

        if (*msg_type != 0x8a && *msg_type != 0x8b && *msg_type != 0x8c &&
            *msg_type != 0x8d && *msg_type != 0x8e && *msg_type != 0x8f &&
            *msg_type != 0x90 && *msg_type != 0x91 && *msg_type != 0x92 &&
            *msg_type != 0x93 && *msg_type != 0x94 && *msg_type != 0x95 &&
            *msg_type != 0x96 && *msg_type != 0x97 && *msg_type != 0x98 &&
            *msg_type != 0xae) {
            return;
        }

        *msg_type = 0xaf;

        switch (MOX::player[player_idx].personality) {
            case PLAYER_PERSONALITY_XENOPHOBIC:
                rejection_val = (int16_t)(game_random::Random(4) - 6);
                break;
            case PLAYER_PERSONALITY_RUTHLESS:
            case PLAYER_PERSONALITY_AGGRESSIVE:
                rejection_val = (int16_t)(-1 - (int32_t)game_random::Random(3));
                break;
            case PLAYER_PERSONALITY_ERRATIC:
                rejection_val = (int16_t)(-(int32_t)game_random::Random(4));
                break;
            case PLAYER_PERSONALITY_HONORABLE:
                if (MOX::player[player_idx].dishonored_flag[target_idx] == 1) {
                    rejection_val = (int16_t)(game_random::Random(4) - 6);
                } else {
                    rejection_val = (int16_t)(-(int32_t)game_random::Random(3));
                }
                break;
            case PLAYER_PERSONALITY_PACIFIST:
                rejection_val = (int16_t)(-(int32_t)game_random::Random(3));
                break;
            default:
                break;
        }

        MOX::player[player_idx].diplomacy_proposal_rejection[target_idx] = rejection_val;
        MOX::player[target_idx].diplomacy_proposal_rejection[player_idx] = rejection_val;
    }

    int32_t __cdecl Get_Tech_Exchange_Expectation(int16_t player_idx, int16_t target_idx) {
        int32_t expectation = game_random::Random(150 /* 0x96 */);

        expectation -= MOX::settings.game_difficulty * 25;
        expectation += MOX::player[player_idx].broken_treaties_modifier[target_idx];

        if (MOX::player[player_idx].traits[TRAIT_CHARISMATIC] != 0) {
            expectation += 50;
        }

        if (MOX::player[player_idx].tech_applications[TECH_APP_XENO_PSYCHOLOGY] == TECH_RESEARCH_STATUS_RESEARCHED) {
            expectation += 50;
        }

        if (MOX::player[target_idx].personality == PLAYER_PERSONALITY_HONORABLE && MOX::player[target_idx].dishonored_flag[player_idx] == 1) {
            expectation += diplodef::personality_relation_modifiers[PLAYER_PERSONALITY_DISHONORED];
        } else {
            expectation += diplodef::personality_relation_modifiers[(uint8_t)MOX::player[target_idx].personality];
        }

        if (expectation < 0) {
            return 10000 / (100 - expectation);
        }

        return expectation + 100;
    }

    void __cdecl NPC_Tech_Exchange_Check(int16_t player_idx, int16_t target_player_idx) {
        int16_t available_tech_count = 0;
        uint8_t available_tech_ids[TECH_APP_COUNT];
        int32_t available_tech_values[TECH_APP_COUNT];

        if (MOX::player[player_idx].diplomacy_message[target_player_idx] == '\0' &&
            MOX::player[player_idx].treaty[target_player_idx] < 4) {
            int32_t expectation = Get_Tech_Exchange_Expectation(target_player_idx, player_idx);
            if (expectation < 10) {
                expectation = 10;
            }

            int32_t num_elements = 0;
            newtech::Set_Competition_Tech_Values(player_idx);

            for (int32_t i = 0; i < 204; ++i) {
                if (MOX::player[target_player_idx].tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int32_t tech_value = newtech::Calc_Tech_Value(player_idx, i);
                    if (tech_value * 2 != 0) {
                        human_tech[num_elements].index = i;
                        human_tech[num_elements].value = tech_value * 2;
                        num_elements++;
                    }
                }
            }

            if (num_elements > 0) {
                qsort(human_tech, num_elements, sizeof(s_npcdiplo_tech), reverse_compare_tech);
                int32_t random_pick = game_random::Random((num_elements + 1) / 2);
                uint8_t selected_tech_index = human_tech[random_pick - 1].index;
                int32_t exchange_val = (human_tech[random_pick - 1].value * expectation) / 100;

                Get_Differential_Tech_List(target_player_idx, player_idx, 0, (uint32_t)exchange_val, available_tech_ids, available_tech_values, &available_tech_count, 0);

                if (available_tech_count > 0) {
                    MOX::player[player_idx].diplomacy_message[target_player_idx] = 101; /* 'e' */
                    MOX::player[player_idx].diplomacy_proposal_exchange_max_value[target_player_idx] = exchange_val;
                    MOX::player[player_idx].diplomacy_proposal_exchange_tech[target_player_idx] = selected_tech_index;
                }
            }
        }
    }

    void __cdecl Get_Exchange_Tech_List(int16_t player_a, int16_t player_b, int16_t* out_count, uint8_t* out_tech_index, uint8_t* out_human_tech_index) {
        int16_t reaction_modifier = Get_Tech_Exchange_Reaction(player_a, player_b, 0);

        if (reaction_modifier <= 0) {
            *out_count = 0;
            return;
        }

        newtech::Set_Competition_Tech_Values(player_b);
        int human_count = 0;

        for (int i = 0; i < 204; i++) {
            if (MOX::player[player_a].tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                if (COLCALC::Player_May_Get_Tech_App(&MOX::player[player_b], i)) {
                    int32_t val = Tech_Trade_Value(player_b, player_a, i);
                    if (val != 0) {
                        human_tech[human_count].index = (uint8_t)i;
                        human_tech[human_count].value = val;
                        human_count++;
                    }
                }
            }
        }

        int max_human_tech = 0;
        if (human_count > 0) {
            for (int i = 0; i < human_count; i++) {
                int scaled_val = (human_tech[i].value * reaction_modifier) / 100;
                if (max_human_tech < scaled_val) {
                    max_human_tech = scaled_val;
                }
                human_tech[i].value = scaled_val;
            }
        }

        int target_count = 0;
        newtech::Set_Competition_Tech_Values(player_a);

        int max_target_val = (reaction_modifier * max_human_tech) / reaction_modifier;

        for (int i = 0; i < TECH_APP_COUNT; i++) {
            if (MOX::player[player_b].tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                if (COLCALC::Player_May_Get_Tech_App(&MOX::player[player_a], i)) {
                    int32_t val = Tech_Trade_Value(player_a, player_b, i);
                    if (val != 0 && val <= max_target_val) {
                        target_tech[target_count].index = (uint8_t)i;
                        target_tech[target_count].value = val;
                        target_count++;
                    }
                }
            }
        }

        if (human_count == 0 || target_count == 0) {
            *out_count = 0;
            return;
        }

        qsort(human_tech, human_count, sizeof(human_tech[0]), compare_tech);
        qsort(target_tech, target_count, sizeof(target_tech[0]), reverse_compare_tech);

        int median_human_tech = human_tech[human_count / 2].value;
        *out_count = 0;

        for (int i = 0; i < target_count; i++) {
            int target_val = target_tech[i].value;

            if (target_val > max_human_tech) {
                out_tech_index[*out_count] = target_tech[i].index;
                out_human_tech_index[*out_count] = 0;
            } else {
                int j = (target_val < median_human_tech) ? 0 : (human_count / 2);
                while (j < human_count) {
                    if (human_tech[j].value >= target_val) {
                        break;
                    }
                    j++;
                }

                if (j < human_count) {
                    out_tech_index[*out_count] = target_tech[i].index;
                    out_human_tech_index[*out_count] = human_tech[j].index;
                } else {
                    out_tech_index[*out_count] = target_tech[i].index;
                    out_human_tech_index[*out_count] = 1;
                }
            }
            (*out_count)++;
        }
    }
} // namespace npcdiplo