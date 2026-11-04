#include "pch.h"

namespace diplomac {
    namespace {
        constexpr std::intptr_t kInvalidTributePlayerMarker = -1599;
        constexpr std::intptr_t kImpossiblePeaceEndedPlayerMarker = -1839;
    } // namespace

    uint8_t alternate_diplomacy = 0;

    int16_t diplomacy_proposal_system_bribe;

    int8_t diplomacy_proposal_tribute_bribe;

    int16_t diplomacy_proposal_gold_bribe;

    uint8_t diplomacy_proposal_tech_bribe1;

    int16_t starting_relations[STARTING_RELATIONS_RACE_COUNT][STARTING_RELATIONS_RACE_COUNT] = {
        0,
        0,
        -24,
        24,
        0,
        0,
        -12,
        0,
        -24,
        0,
        12,
        0,
        24,
        0,
        0,
        0,
        -12,
        24,
        0,
        0,
        0,
        -12,
        0,
        0,
        0,
        0,
        0,
        0,
        -24,
        -12,
        0,
        0,
        12,
        0,
        0,
        12,
        0,
        0,
        0,
        12,
        0,
        0,
        24,
        24,
        0,
        0,
        -24,
        0,
        0,
        0,
        24,
        -12,
        0,
        -12,
        0,
        0,
        0,
        0,
        12,
        -24,
        0,
        0,
        0,
        12,
        -12,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        -12,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        -12,
        0,
        -12,
        12,
        24,
        0,
        0,
        -12,
        12,
        0,
        12,
        0,
        0,
        0,
        12,
        0,
        0,
        -12,
        -12,
        0,
        -24,
        0,
        0,
        24,
        -12,
        0,
        -12,
        12,
        0,
        0,
        0,
        0,
        12,
        0,
        0,
        0,
        0,
        -12,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        12,
        0,
        12,
        0,
        0,
        0,
        0,
        0,
        -12,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        12,
        -12,
        0,
        0,
        12,
        -12,
        0,
        0,
        0,
        0,
        0,
        0,
        24,
        0,
        0,
        0,
        0,
        0,
        24,
        -12,
        12,
        12,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    };

    int16_t personality_peace_duration[12] = {
        5,
        10,
        20,
        5,
        50,
        40,
        5,
        0,
        0,
        0,
        0,
        0,
    };

    int16_t personality_break_peace_treaty_chance[8] = {
        7,
        10,
        1,
        1,
        0,
        0,
        5,
        0,
    };

    int16_t personality_sneak_attack_modifiers[8] = {
        -10,
        -5,
        -3,
        0,
        20,
        20,
        -10,
        0,
    };

    int16_t __cdecl Sneak_Attack_Evaluations(int16_t player_idx, int16_t target_idx, int16_t* out_eval_threshold, int16_t* out_message_id, int16_t* out_unused) {
        (void)out_unused;

        int8_t rel = MOX::player[player_idx].relations[target_idx];
        int8_t rel_divisor = (rel < 50) ? 10 : 5;

        int16_t msg_id = 67 /* 0x43 */;
        int16_t severity = 200;
        int16_t score = rel / rel_divisor;
        int16_t min_mod = score;

        int8_t threats = MOX::player[player_idx].threats[target_idx];
        if (threats > 0 && MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] != 0) {
            int16_t personality = Get_Player_Diplomacy_Personality(player_idx, target_idx);
            int16_t threat_mod = -(int16_t)((threats * 10) / diplodef::personality_max_threats[personality]);
            if (threat_mod < min_mod) {
                msg_id = (uint8_t)MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] + 70 /* 0x46 */;
                min_mod = threat_mod;
            }
            score += threat_mod;
        }

        if (MOX::player[player_idx].failed_to_expand == 1) {
            if (MOX::player[player_idx].objectives == PLAYER_OBJECTIVE_EXPANSIONIST ||
                MOX::player[player_idx].total_pop > COLCALC::Get_Player_Max_Population(target_idx) / 2) {
                msg_id = 114 /* 0x72 */;
                min_mod = -150;
                score = -150;
            }
        }

        if (MOX::player[player_idx].personality == PLAYER_PERSONALITY_ERRATIC) {
            if ((int16_t)game_random::Random(200) <= MOX::settings.game_difficulty + 1) {
                msg_id = 109 /* 0x6D */;
                severity = 100;
                min_mod = -150;
                score = -150;
            }
        }

        if ((int16_t)game_random::Random(100) < MOX::player[player_idx].food_shortage_duration) {
            msg_id = 119 /* 0x77 */;
            min_mod = -150;
            score = -150;
        }

        if (MOX::player[player_idx].personality == PLAYER_PERSONALITY_RUTHLESS) {
            if (Fleet_Strength_Comparison(player_idx, target_idx) >= 100) {
                if (MOX::player[player_idx].opportunity_actual_attacker_ratio[target_idx] >= 200 &&
                    MOX::player[player_idx].opportunity_planet_num[target_idx] != -1) {
                    int16_t opp_mod = (int16_t)(MOX::player[player_idx].opportunity_actual_attacker_ratio[target_idx] / -20);
                    if (opp_mod < min_mod) {
                        msg_id = 115 /* 0x73 */;
                        min_mod = opp_mod;
                    }
                    score += opp_mod;
                }
            }
        }

        if (MOX::player[player_idx].personality == PLAYER_PERSONALITY_XENOPHOBIC) {
            if (aitech::System_Proximity_Count(player_idx, (uint8_t)target_idx) >= (int16_t)game_random::Random(400)) {
                msg_id = 121 /* 0x79 */;
                min_mod = -150;
                score = -150;
            }
        }

        int16_t trust_mod = (MOX::player[player_idx].trust_worthiness[target_idx] * 3) / 5;
        if (trust_mod < min_mod && MOX::player[player_idx].trust_breaker_player[target_idx] != -1) {
            min_mod = trust_mod;
            msg_id = 176 /* 0xB0 */ + (MOX::player[player_idx].trust_breaker_player[target_idx] == player_idx);
        }
        score += trust_mod;

        if ((Fleet_Strength_Comparison(player_idx, target_idx) >= 300 || MOX::player[player_idx].total_pop * 3 <= MOX::player[player_idx].total_pop * 2) &&
            MOX::player[player_idx].personality != PLAYER_PERSONALITY_PACIFIST) {
            int16_t str_mod = (int16_t)(Fleet_Strength_Comparison(player_idx, target_idx) / -40);
            if (str_mod < min_mod) {
                msg_id = 112 /* 0x70 */;
                min_mod = str_mod;
            }
            severity = 150;
            score += str_mod;
        }

        int16_t active_players = 0;
        int16_t max_other_pop = 0;
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN && !MOX::player[i].eliminated) {
                active_players++;
                if (i != player_idx && MOX::player[i].total_pop > max_other_pop) {
                    max_other_pop = MOX::player[i].total_pop;
                }
            }
        }

        if (((active_players < 3 && MOX::player[player_idx].total_pop > max_other_pop) || active_players == 1)) {
            if (min_mod > -10) {
                msg_id = 178 /* 0xB2 */;
                min_mod = -10;
            }
            severity = 150;
            score -= 10;
        }

        int16_t turn = (int16_t)MOX::stardate - 35001;
        if (turn > 99) {
            int16_t comparison_turn = (turn < 350) ? turn - 40 : 309;
            int16_t source_growth = (uint8_t)MOX::player[player_idx].population_history[turn] - (uint8_t)MOX::player[player_idx].population_history[comparison_turn];
            int16_t target_growth = (uint8_t)MOX::player[target_idx].population_history[turn] - (uint8_t)MOX::player[target_idx].population_history[comparison_turn];
            if (source_growth < target_growth) {
                int16_t growth_mod = (source_growth - target_growth) / 2;
                if (growth_mod < min_mod) {
                    msg_id = 117 /* 0x75 */;
                    min_mod = growth_mod;
                }
                score += growth_mod;
            }
        }

        if (MOX::player[player_idx].treaty[target_idx] == 1) score += 10;
        if (MOX::player[player_idx].treaty[target_idx] == 2) score += 20;

        int16_t personality_idx = Get_Player_Diplomacy_Personality(player_idx, target_idx);
        score += diplomac::personality_sneak_attack_modifiers[personality_idx];

        if (MOX::player[target_idx].tech_applications[TECH_APP_XENO_PSYCHOLOGY] == TECH_RESEARCH_STATUS_RESEARCHED) score += 5;
        if (MOX::player[target_idx].traits[TRAIT_CHARISMATIC] == 1) score += 10;

        score = (score + COLCALC::Best_Leader_Diplomat_Bonus(target_idx) - MOX::settings.game_difficulty) + 15;

        int16_t final_prob;
        uint8_t last_threat_turn = MOX::player[player_idx].last_diplomacy_threat_turn[target_idx];

        if (score < 0) {
            final_prob = -(int16_t)(last_threat_turn * score);
        } else {
            uint32_t divisor = special::isqrt(score * score * score + 5);
            final_prob = (divisor != 0) ? (int16_t)(last_threat_turn / divisor) : 0;
        }

        if ((MOX::player[player_idx].treaty[target_idx] == 1 || MOX::player[player_idx].treaty[target_idx] == 2) &&
            Get_Player_Diplomacy_Personality(player_idx, target_idx) == PLAYER_PERSONALITY_HONORABLE) {
            final_prob = 0;
        }

        int16_t demands_count = (int16_t)(Fleet_Strength_Comparison(player_idx, target_idx) / 40) - 1 + game_random::Random(3);
        if (demands_count < 1) demands_count = 1;

        if (Get_Demands(player_idx, target_idx, demands_count, 1, 1, 1, 1) == 0) {
            demands_count = 0;
        }

        if ((int16_t)game_random::Random(100) <= final_prob && last_threat_turn > 9) {
            if ((int16_t)game_random::Random(16) + MOX::settings.game_difficulty < 16) {
                if (demands_count > 0 && min_mod != -150) {
                    bool is_most_populous = true;
                    for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
                        if (i != player_idx && MOX::player[player_idx].total_pop < MOX::player[i].total_pop) {
                            is_most_populous = false;
                        }
                    }

                    if (game_random::Random(4) < 4 && is_most_populous && diplodef::COUNCIL_FLAG == 1 && demands_count > 3) {
                        *out_eval_threshold = severity;
                        *out_message_id = msg_id;
                        return 4; /* ULTIMATUM */
                    }

                    if (MOX::player[player_idx].traits[TRAIT_REPULSIVE] == 0 && MOX::player[target_idx].traits[TRAIT_REPULSIVE] == 0) {
                        if (special::isqrt(demands_count + 4) < (uint32_t)game_random::Random(4)) {
                            *out_eval_threshold = severity;
                            *out_message_id = msg_id;
                            return 3; /* DEMAND */
                        }
                        *out_eval_threshold = severity;
                        *out_message_id = msg_id;
                        return 1; /* ATTACK */
                    }

                    return 0;
                }
            }

            *out_eval_threshold = severity;
            *out_message_id = msg_id;
            return 2; /* THREAT */
        }

        return 0;
    }

    void __cdecl NPC_Sneak_Attacks() {
        int16_t eval_results[MAX_PLAYERS];
        int16_t eval_messages[MAX_PLAYERS];
        int16_t eval_ratios[MAX_PLAYERS];
        int16_t eval_unused[MAX_PLAYERS];

        for (int16_t p1 = 0; p1 < (int16_t)MOX::NUM_PLAYERS; ++p1) {
            s_player* plr1 = &MOX::player[p1];

            if (plr1->objectives == PLAYER_OBJECTIVE_HUMAN || plr1->eliminated) {
                continue;
            }

            int16_t human_war_count = 0;
            int16_t npc_war_count = 0;

            for (int16_t p2 = 0; p2 < (int16_t)MOX::NUM_PLAYERS; ++p2) {
                if (plr1->treaty[p2] >= 4) {
                    if (MOX::player[p2].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        human_war_count++;
                    } else {
                        npc_war_count++;
                    }
                }
            }

            if (plr1->sneak_attack_planet == -1 && human_war_count == 0) {
                for (int16_t p2 = 0; p2 < (int16_t)MOX::NUM_PLAYERS; ++p2) {
                    if (plr1->opportunity_planet_num[p2] == -1) {
                        plr1->opportunity_planet_value[p2] = 0;
                        plr1->opportunity_actual_attacker_ratio[p2] = 0;
                    }
                }

                for (int16_t p2 = 0; p2 < (int16_t)MOX::NUM_PLAYERS; ++p2) {
                    s_player* plr2 = &MOX::player[p2];
                    if (plr2->objectives == PLAYER_OBJECTIVE_HUMAN &&
                        plr1->treaty[p2] < 4 &&
                        plr1->within_range[p2] == 1 &&
                        plr1->contact[p2] == 1 &&
                        !plr2->eliminated &&
                        plr1->last_turn_treaty[p2] != 1 &&
                        plr1->last_turn_treaty[p2] != 2 &&
                        plr1->last_turn_treaty[p2] != 3) {
                        eval_results[p2] = Sneak_Attack_Evaluations(p1, p2, &eval_ratios[p2], &eval_messages[p2], &eval_unused[p2]);
                    } else {
                        eval_results[p2] = 0;
                    }
                }

                for (int16_t p2 = 0; p2 < (int16_t)MOX::NUM_PLAYERS; ++p2) {
                    if (plr1->peace_duration[p2] != 0) {
                        int16_t personality = Get_Player_Diplomacy_Personality(p1, p2);
                        int16_t break_chance = personality_break_peace_treaty_chance[personality];
                        if ((int32_t)game_random::Random(100) > (int32_t)break_chance) {
                            if (eval_results[p2] != 0) {
                                plr1->war_buildup_duration = (int8_t)game_random::Random(20) + 20;
                            }
                            continue;
                        }
                    }

                    if (eval_results[p2] == 2) {
                        if (Get_Player_Diplomacy_Personality(p1, p2) != PLAYER_PERSONALITY_HONORABLE) {
                            Clear_Last_Threat_Turn(p1, p2);
                            if (plr1->opportunity_planet_num[p2] == -1 ||
                                plr1->opportunity_actual_attacker_ratio[p2] <= (int32_t)eval_ratios[p2]) {
                                plr1->war_buildup_duration = (int8_t)game_random::Random(20) + 20;
                                if (npc_war_count > 0 && (int32_t)game_random::Random(4) < (int32_t)MOX::settings.game_difficulty) {
                                    for (int16_t p3 = 0; p3 < (int16_t)MOX::NUM_PLAYERS; ++p3) {
                                        if (MOX::player[p3].objectives != PLAYER_OBJECTIVE_HUMAN && plr1->treaty[p3] >= 4) {
                                            Declare_Peace(p1, p3);
                                        }
                                    }
                                }
                            } else if (plr1->opportunity_planet_value[p2] > 0) {
                                plr1->sneak_attack_planet = plr1->opportunity_planet_num[p2];
                                plr1->sneak_attack_player = (int8_t)p2;
                                plr1->sneak_attack_message = (int8_t)eval_messages[p2];
                                plr1->war_buildup_duration = (int8_t)game_random::Random(20) + 20;
                                if (npc_war_count > 0) {
                                    for (int16_t p3 = 0; p3 < (int16_t)MOX::NUM_PLAYERS; ++p3) {
                                        if (MOX::player[p3].objectives != PLAYER_OBJECTIVE_HUMAN && plr1->treaty[p3] >= 4) {
                                            Declare_Peace(p1, p3);
                                        }
                                    }
                                }
                            }
                        }
                    } else if (eval_results[p2] == 1) {
                        Clear_Last_Threat_Turn(p1, p2);
                        plr1->diplomacy_demand_rejection_message[p2] = (int8_t)eval_messages[p2];
                        plr1->war_buildup_duration = (int8_t)game_random::Random(20) + 20;
                        plr1->diplomacy_severity[p2] = 10000;
                        plr1->diplomacy_incident[p2] = 0;
                        plr1->diplomacy_message[p2] = 0x6a;
                        Set_Demands(p1, p2);
                    } else if (eval_results[p2] == 3) {
                        Clear_Last_Threat_Turn(p1, p2);
                        plr1->diplomacy_severity[p2] = 10000;
                        plr1->diplomacy_incident[p2] = 0;
                        plr1->diplomacy_message[p2] = 0x69;
                        plr1->war_buildup_duration = (int8_t)game_random::Random(20) + 10;
                        Set_Demands(p1, p2);
                    } else if (eval_results[p2] == 4) {
                        plr1->council_threat = (int8_t)p2;
                        Clear_Last_Threat_Turn(p1, p2);
                        plr1->diplomacy_severity[p2] = 10000;
                        plr1->diplomacy_incident[p2] = 0;
                        plr1->diplomacy_message[p2] = 0x7c;
                        plr1->war_buildup_duration = (int8_t)game_random::Random(20) + 20;
                        Set_Demands(p1, p2);
                    }
                }
            } else if (plr1->war_buildup_duration == 0) {
                plr1->sneak_attack_planet = -1;
            }
        }
    }

    void __cdecl Diplomacy_Broken_Contacts() {
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                // If there is no contact between player i and player j
                if (MOX::player[i].contact[j] == 0) {
                    // Reset economic and research treaties/modifiers
                    MOX::player[i].trade_modifier[j] = 0;
                    MOX::player[i].trade_treaty[j] = 0;
                    MOX::player[i].research_treaty[j] = 0;
                }
            }
        }
    }

    void Init_Diplomatic_Relations() {
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                int32_t race_i = (uint8_t)MOX::player[i].race;
                int32_t race_j = (uint8_t)MOX::player[j].race;

                int16_t relation_val = starting_relations[race_i][race_j];
                MOX::player[i].relations[j] = (int8_t)relation_val;
                MOX::player[i].base_relations[j] = MOX::player[i].relations[j];

                MOX::player[i].treaty[j] = 0;
                MOX::player[i].trade_treaty[j] = 0;
                MOX::player[i].research_treaty[j] = 0;
                MOX::player[i].first_contact[j] = 0;
                MOX::player[i].diplomacy_severity[j] = 0;
                MOX::player[i].diplomacy_incident[j] = 0;
                MOX::player[i].diplomacy_value[j] = 0;
                MOX::player[i].diplomacy_system[j] = 0;
                MOX::player[i].treaty_modifier[j] = 0;
                MOX::player[i].trade_modifier[j] = 0;
                MOX::player[i].tech_exchange_modifier[j] = 0;
                MOX::player[i].peace_modifier[j] = 0;
                MOX::player[i].last_bad_diplomatic_incident[j] = 0;
                MOX::player[i].broken_treaties_modifier[j] = 0;
                MOX::player[i].threats[j] = 0;
                MOX::player[i].trust_worthiness[j] = 0;
                MOX::player[i].trust_breaker_player[j] = -1;
                MOX::player[i].last_diplomacy_threat_turn[j] = 0;
                MOX::player[i].reward_amount[j] = 0;
                MOX::player[i].reward_tech[j] = 0;
                MOX::player[i].reward_attack_player[j] = -1;
                MOX::player[i].peace_duration[j] = 0;
                MOX::player[i].tribute_treaty[j] = 0;
                MOX::player[i].ship_war_losses[j] = 0;
                MOX::player[i].colony_war_losses[j] = 0;
                MOX::player[i].biological_weapons_flag[j] = 0;
                MOX::player[i].time_since_last_attack[j] = 0;
                MOX::player[i].dishonored_flag[j] = 0;
                MOX::player[i].last_turn_treaty[j] = 0;
                MOX::player[i].diplomacy_message[j] = 0;
                MOX::player[i].diplomacy_proposal_request[j] = 0;
                MOX::player[i].diplomacy_proposal_tribute_bribe[j] = 0;
                MOX::player[i].diplomacy_proposal_tech_bribe1[j] = 0;
                MOX::player[i].diplomacy_proposal_tech_bribe2[j] = 0;
                MOX::player[i].diplomacy_proposal_gold_bribe[j] = 0;
                MOX::player[i].diplomacy_proposal_war_player[j] = 0;
                MOX::player[i].opportunity_planet_num[j] = -1;
                MOX::player[i].delayed_diplomacy_orders[j] = -1;

                npcdiplo::Clear_Last_Proposal_Turn(i, j);
            }
            MOX::player[i].war_buildup_duration = 0;
            MOX::player[i].council_voted_for = -1;
            MOX::player[i].council_threat = -1;
            MOX::player[i].sneak_attack_planet = -1;
            MOX::player[i].sneak_attack_message = 0;
            MOX::player[i].time_since_last_lucky_event = 0;
        }

        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN) {
                for (int16_t j = i + 1; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                    if (MOX::player[j].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        int16_t rejection_mod = 0;
                        switch (MOX::player[i].personality) {
                            case PLAYER_PERSONALITY_XENOPHOBIC: rejection_mod = -3; break;
                            case PLAYER_PERSONALITY_RUTHLESS: rejection_mod = -2; break;
                            case PLAYER_PERSONALITY_AGGRESSIVE:
                            case PLAYER_PERSONALITY_ERRATIC: rejection_mod = 0; break;
                            case PLAYER_PERSONALITY_HONORABLE: rejection_mod = 2; break;
                            case PLAYER_PERSONALITY_PACIFIST: rejection_mod = 5; break;
                        }
                        MOX::player[i].diplomacy_proposal_rejection[j] = rejection_mod;
                        MOX::player[j].diplomacy_proposal_rejection[i] = rejection_mod;
                    }
                }
            }
        }

        // Initialize Galactic Council global flags/state.
        diplodef::COUNCIL_FLAG = 0;
        diplodef::council_winner = -1;
        diplodef::next_council_meeting_turn = 25;
    }

    void NoOp2() {
        return;
    }

    void Clear_Diplomacy_Messages() {
        NoOp2();
        Modifier_Diplomacy_Adjustments();

        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                s_player* p = &MOX::player[i];
                p->last_turn_diplomacy_severity[j] = p->diplomacy_severity[j];
                p->diplomacy_severity[j] = 0;
                p->diplomacy_incident[j] = 0;
                p->diplomacy_proposal_request[j] = 0;
                p->diplomacy_proposal_tribute_bribe[j] = 0;
                p->diplomacy_proposal_tech_bribe1[j] = 0;
                p->diplomacy_proposal_tech_bribe2[j] = 0;
                p->diplomacy_proposal_gold_bribe[j] = 0;
                p->diplomacy_message[j] = 0;
            }
        }

        // Handle peace duration expiration and treaty revocation
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = i + 1; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                if (MOX::player[i].peace_duration[j] > 0) {
                    MOX::player[i].peace_duration[j]--;
                }
                if (MOX::player[j].peace_duration[i] > 0) {
                    MOX::player[j].peace_duration[i]--;
                }

                // If peace duration hits 0 and the treaty was a Peace Treaty (0x03), break it
                if (MOX::player[i].peace_duration[j] == 0 && MOX::player[i].treaty[j] == 3 /* TREATY_PEACE */) {
                    MOX::player[i].treaty[j] = 0;
                    MOX::player[j].treaty[i] = 0;
                }
            }
        }

        // Increment time since last attack for players at war
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = i + 1; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                // Treaty >= 4 usually represents a state of war
                if (MOX::player[i].treaty[j] >= 4 /* TREATY_WAR */) {
                    if ((uint8_t)MOX::player[i].time_since_last_attack[j] < 250) {
                        MOX::player[i].time_since_last_attack[j]++;
                    }
                    if ((uint8_t)MOX::player[j].time_since_last_attack[i] < 250) {
                        MOX::player[j].time_since_last_attack[i]++;
                    }
                }
            }
        }
    }

    void __cdecl End_Of_Turn_Diplomacy_Adjustments() {
        Diplomacy_Broken_Contacts();
        npcdiplo::NPC_Treaty_Hatred();
        DIP_SCRN::Add_Diplomacy_Turn_Summary_Messages();

        // Store current treaties as last turn's state
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                MOX::player[i].last_turn_treaty[j] = MOX::player[i].treaty[j];
            }
        }

        // Adjust food shortage duration
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].surplus_food < 0) {
                MOX::player[i].food_shortage_duration++;
            } else {
                MOX::player[i].food_shortage_duration = 0;
            }
        }

        // Decay diplomatic modifiers towards zero
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                MOX::player[i].treaty_modifier[j] += 10;
                if (MOX::player[i].treaty_modifier[j] > 0) {
                    MOX::player[i].treaty_modifier[j] = 0;
                }

                MOX::player[i].trade_modifier[j] += 10;
                if (MOX::player[i].trade_modifier[j] > 0) {
                    MOX::player[i].trade_modifier[j] = 0;
                }

                MOX::player[i].tech_exchange_modifier[j] += 10;
                if (MOX::player[i].tech_exchange_modifier[j] > 0) {
                    MOX::player[i].tech_exchange_modifier[j] = 0;
                }

                MOX::player[i].peace_modifier[j] += 10;
                if (MOX::player[i].peace_modifier[j] > 0) {
                    MOX::player[i].peace_modifier[j] = 0;
                }
            }
        }

        // Decay war buildup duration
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].war_buildup_duration != 0) {
                MOX::player[i].war_buildup_duration--;
            }
        }

        // Update diplomacy counters for living players regarding dead players they have contact with
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN && MOX::player[j].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    if (MOX::player[i].contact[j] != 0) {
                        if (MOX::player[i].last_diplomacy_proposal_turn[j] < 250) {
                            MOX::player[i].last_diplomacy_proposal_turn[j]++;
                        }
                        if (MOX::player[i].last_diplomacy_threat_turn[j] < 250) {
                            MOX::player[i].last_diplomacy_threat_turn[j]++;
                        }
                    }
                }
            }
        }

        // Terminate specific peace treaties (Type 3) under special conditions
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                if (MOX::player[i].treaty[j] == 3) {
                    // Preserve the original impossible x86 address comparison literally,
                    // but express it as an integer compare to avoid x64 pointer-cast warnings.
                    if ((std::intptr_t)(MOX::player + i) == kImpossiblePeaceEndedPlayerMarker) {
                        MOX::player[i].treaty[j] = 0;
                        MOX::player[j].treaty[i] = 0;
                        MSG_::Add_Peace_Ended_Msg(i, j);
                    }
                }
            }
        }
    }

    int16_t __cdecl Get_Player_Diplomacy_Personality(int16_t player_idx, int16_t target_idx) {
        // If personality is 4 (Honorable) and player has dishonored a treaty with the target
        if (MOX::player[player_idx].personality == PLAYER_PERSONALITY_HONORABLE &&
            MOX::player[player_idx].dishonored_flag[target_idx] == 1) {
            return PLAYER_PERSONALITY_DISHONORED;
        }

        // Return the actual personality byte, zero-extended to short
        return (int16_t)(uint8_t)MOX::player[player_idx].personality;
    }

    /**
     * @brief Calculates a weighted count of players at war with the specified player.
     * 
     * Iterates through all players and counts those who are at war with player_idx,
     * excluding both player_idx and exclude_idx. Players with objective 100 (often 
     * associated with extermination goals) are weighted double.
     * 
     * @param player_idx Index of the player whose war status is being checked.
     * @param exclude_idx Index of a second player to exclude from the count.
     * @return The weighted war count.
     */
    int16_t __cdecl Get_War_Count(int16_t player_idx, int16_t exclude_idx) {
        int16_t war_count = 0;

        if ((int16_t)MOX::NUM_PLAYERS > 0) {
            for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
                // Skip the target player and the specified excluded player
                if (i != player_idx && i != exclude_idx) {
                    // Check diplomatic treaty status. A value of 4 or higher indicates a state of war.
                    if (MOX::player[player_idx].treaty[i] >= 4) {
                        // Players with specific objectives (100 / 'd') contribute double to the war count
                        if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                            war_count += 2;
                        } else {
                            war_count += 1;
                        }
                    }
                }
            }
        }

        return war_count;
    }

    void __cdecl Sneak_Attack_Or_Declare_War(int16_t attacker_idx, int16_t target_idx, int8_t message_idx) {
        s_player* player_ptr = MOX::player;
        s_player& attacker = player_ptr[attacker_idx];

        int16_t planet_num = attacker.opportunity_planet_num[target_idx];
        int32_t actual_ratio = attacker.opportunity_actual_attacker_ratio[target_idx];

        if (planet_num != -1 && actual_ratio >= 200) {
            int16_t personality = Get_Player_Diplomacy_Personality(attacker_idx, target_idx);
            if (personality != PLAYER_PERSONALITY_HONORABLE) {
                attacker.sneak_attack_planet = planet_num;
                attacker.sneak_attack_player = (int8_t)target_idx;
                attacker.sneak_attack_message = message_idx + 70;
                return;
            }
        }

        int32_t attacker_strength = attacker.total_ship_strength[target_idx];
        int32_t target_strength = player_ptr[target_idx].total_ship_strength[attacker_idx];

        if (((attacker_strength * 4 + 4) * 25) / (target_strength + 1) > 66) {
            Declare_War(target_idx, attacker_idx);
            attacker.diplomacy_message[target_idx] = message_idx + 56;
        }
    }

    void Determine_First_Contacts() {
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            // Check if player is human or standard AI ('d' = 100)
            if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                    if (MOX::player[i].contact[j] == 1) {
                        if (MOX::player[i].first_contact[j] == 0) {
                            MOX::player[i].first_contact[j] = 1;
                            MOX::player[i].treaty[j] = 0;
                            MOX::player[j].treaty[i] = 0;

                            int16_t personality_idx_i = Get_Player_Diplomacy_Personality(i, j);
                            MOX::player[i].peace_duration[j] += (int8_t)(personality_peace_duration[personality_idx_i] * 2);

                            int16_t personality_idx_j = Get_Player_Diplomacy_Personality(j, i);
                            MOX::player[j].peace_duration[i] += (int8_t)(personality_peace_duration[personality_idx_j] * 2);

                            if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN && MOX::player[j].objectives == PLAYER_OBJECTIVE_HUMAN) {
                                if (diplodef::COUNCIL_FLAG != 3) {
                                    MOX::player[j].diplomacy_message[i] = MOX::player[j].personality + 1;
                                }
                            } else {
                                Change_Relations(-10000, i, j, 17 /* INCIDENT_FIRST_CONTACT */, 0, 0);
                            }
                        } else if ((MOX::player[i].just_established_contact >> j) & 1) {
                            if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                                if (MOX::player[j].objectives == PLAYER_OBJECTIVE_HUMAN) {
                                    if (diplodef::COUNCIL_FLAG != 3) {
                                        if (MOX::player[j].relations[i] < 0 && MOX::player[j].treaty[i] < 4) {
                                            MOX::player[j].diplomacy_message[i] = MOX::player[j].personality + 7;
                                        } else {
                                            MOX::player[j].diplomacy_message[i] = MOX::player[j].personality + 13;
                                        }
                                    }
                                } else {
                                    Change_Relations(-10000, i, j, 18 /* INCIDENT_ESTABLISHED_CONTACT */, 0, 0);
                                }
                            } else {
                                Change_Relations(-10000, i, j, 18 /* INCIDENT_ESTABLISHED_CONTACT */, 0, 0);
                            }
                        }
                    }
                }
            }
        }
    }

    void Determine_Diplomacy_Messages() {
        Limit_Treaty_Modifiers();

        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                auto& player_i = MOX::player[i];
                auto& player_j = MOX::player[j];

                if (player_i.contact[j] == 1 &&
                    player_i.diplomacy_message[j] == 0 &&
                    player_i.objectives != PLAYER_OBJECTIVE_HUMAN &&
                    player_j.objectives == PLAYER_OBJECTIVE_HUMAN &&
                    player_j.eliminated == 0 &&
                    player_i.eliminated == 0 &&
                    (player_i.treaty[j] != 6 /* TREATY_SURRENDER */ || player_i.last_turn_treaty[j] != 6)) {
                    uint8_t incident = player_i.diplomacy_incident[j];

                    if (diplodef::COUNCIL_FLAG == 3 /* COUNCIL_FINAL_VOTE */) {
                        if (incident >= 1 && incident <= 11) {
                            int severity_abs = abs(player_i.diplomacy_severity[j] / 10);
                            if (severity_abs >= (int16_t)game_random::Random(100)) {
                                player_i.diplomacy_message[j] = '{';
                            }
                        }
                    } else {
                        switch (incident) {
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                            case 6:
                            case 7:
                            case 8:
                            case 9:
                                Determine_Bad_Message(i, j);
                                break;

                            case 10:
                            case 11:
                                if (player_i.treaty[j] < 4 /* TREATY_WAR */) {
                                    int16_t personality = Get_Player_Diplomacy_Personality(i, j);
                                    if (personality == PLAYER_PERSONALITY_HONORABLE &&
                                        (player_i.treaty[j] == 1 || player_i.treaty[j] == 2 ||
                                            player_i.trade_treaty[j] != 0 || player_i.research_treaty[j] != 0)) {
                                        player_i.dishonored_flag[j] = 1;
                                        player_j.dishonored_flag[i] = 1;
                                        player_i.diplomacy_message[j] = 'B';
                                    } else {
                                        player_i.diplomacy_message[j] = (incident == 10) ? 'P' : 'Q';
                                    }
                                    Declare_War(j, i);
                                }
                                break;

                            case 12:
                            case 13:
                            case 14:
                            case 15:
                            case 16:
                                Determine_Good_Message(i, j);
                                break;

                            case 17:
                                player_i.diplomacy_message[j] = player_i.personality + 1;
                                break;

                            case 18:
                                if (player_i.relations[j] < 0 && player_i.treaty[j] < 4) {
                                    player_i.diplomacy_message[j] = player_i.personality + 7;
                                } else {
                                    player_i.diplomacy_message[j] = player_i.personality + 13;
                                }
                                break;

                            case 19: player_i.diplomacy_message[j] = 'F'; break;
                            case 20: player_i.diplomacy_message[j] = 'l'; break;
                            case 21: player_i.diplomacy_message[j] = 'R'; break;
                            case 22: player_i.diplomacy_message[j] = 'P'; break;
                            case 23: player_i.diplomacy_message[j] = 'o'; break;
                            default: break;
                        }

                        if (player_i.diplomacy_message[j] == 0 && player_i.treaty[j] >= 4) {
                            Peace_Proposal(i, j, 0);
                        }
                        if (player_i.diplomacy_message[j] == 0) {
                            Required_Alliances(i, j);
                        }
                        if (player_i.diplomacy_message[j] == 0) {
                            NPC_Required_Alliances(i, j);
                        }
                        if (player_i.diplomacy_message[j] == 0) {
                            Unprovoked_Good_Messages(i, j);
                        }
                    }

                    uint8_t msg = player_i.diplomacy_message[j];
                    // Ranges check for Repulsive trait canceling certain positive messages
                    if (((msg >= 0x13 && msg <= 0x38) || (msg >= 0x56 && msg <= 0x6B) || (msg == 0x7C || msg == 0x7D)) &&
                        (player_i.traits[TRAIT_REPULSIVE] == 1 || player_j.traits[TRAIT_REPULSIVE] == 1)) {
                        player_i.diplomacy_message[j] = 0;
                        player_j.diplomacy_message[i] = 0;
                    }
                }
            }
        }
    }

    void __cdecl Start_Tribute_Treaty(int16_t player_idx, int16_t target_idx, int16_t amount) {
        MOX::player[player_idx].tribute_treaty[target_idx] = amount;

        // Clear existing threats between both players
        MOX::player[player_idx].threats[target_idx] = 0;
        MOX::player[target_idx].threats[player_idx] = 0;

        // Update peace duration for player_idx based on personality
        int16_t personality1 = Get_Player_Diplomacy_Personality(player_idx, target_idx);
        MOX::player[player_idx].peace_duration[target_idx] += (int8_t)(personality_peace_duration[personality1] * 2);

        // Update peace duration for target_idx based on personality
        int16_t personality2 = Get_Player_Diplomacy_Personality(target_idx, player_idx);
        MOX::player[target_idx].peace_duration[player_idx] += (int8_t)(personality_peace_duration[personality2] * 2);
    }

    void __cdecl Start_Treaty(int16_t p1, int16_t p2, int16_t treaty_type) {
        MOX::player[p1].treaty[p2] = (int8_t)treaty_type;
        MOX::player[p2].treaty[p1] = (int8_t)treaty_type;

        MOX::player[p1].threats[p2] = 0;
        MOX::player[p2].threats[p1] = 0;

        int16_t pers1 = Get_Player_Diplomacy_Personality(p1, p2);
        int16_t pers2 = Get_Player_Diplomacy_Personality(p2, p1);

        if (treaty_type == 1 /* TREATY_PEACE */) {
            int16_t dur1 = personality_peace_duration[pers1];
            MOX::player[p1].peace_duration[p2] += (int8_t)(dur1 + dur1 / 2);

            int16_t dur2 = personality_peace_duration[pers2];
            MOX::player[p2].peace_duration[p1] += (int8_t)(dur2 + dur2 / 2);
            return;
        }

        if (treaty_type == 2 /* TREATY_ALLIANCE */) {
            int16_t dur1 = personality_peace_duration[pers1];
            MOX::player[p1].peace_duration[p2] += (int8_t)(dur1 * 2);

            int16_t dur2 = personality_peace_duration[pers2];
            MOX::player[p2].peace_duration[p1] += (int8_t)(dur2 * 2);
        }
    }

    void __cdecl Declare_War(int16_t player_idx, int16_t target_idx) {
        s_player* p1 = &MOX::player[player_idx];
        s_player* p2 = &MOX::player[target_idx];

        if (p1->objectives == PLAYER_OBJECTIVE_HUMAN && p1->first_contact[target_idx] == 0) {
            p1->first_contact[target_idx] = 1;
        }

        if (p2->objectives == PLAYER_OBJECTIVE_HUMAN && p2->first_contact[player_idx] == 0) {
            p2->first_contact[player_idx] = 1;
        }

        if (player_idx < MOX::NUM_PLAYERS && target_idx < MOX::NUM_PLAYERS && p1->treaty[target_idx] < 4) {
            p1->threats[target_idx] = 0;
            p2->threats[player_idx] = 0;

            p1->ship_war_losses[target_idx] = 0;
            p2->ship_war_losses[player_idx] = 0;

            p1->colony_war_losses[target_idx] = 0;
            p2->colony_war_losses[player_idx] = 0;

            p1->biological_weapons_flag[target_idx] = 0;
            p2->biological_weapons_flag[player_idx] = 0;

            Break_Trade(player_idx, target_idx);
            Break_Research(player_idx, target_idx);
            Break_Tribute(player_idx, target_idx);
            Break_Treaties(player_idx, target_idx);

            p1->base_relations[target_idx] -= 10;
            p2->base_relations[player_idx] -= 10;

            int8_t rand_offset = (int8_t)game_random::Random(25);
            int8_t initial_relations = (int8_t)181 - rand_offset;

            p1->relations[target_idx] = initial_relations;
            p2->relations[player_idx] = initial_relations;

            int8_t war_treaty_type;
            if (p1->objectives == PLAYER_OBJECTIVE_HUMAN || p2->objectives == PLAYER_OBJECTIVE_HUMAN) {
                war_treaty_type = (MOX::settings.game_difficulty < GAME_DIFFICULTY_HARD) ? 5 : 6;
            } else {
                war_treaty_type = 4;
            }

            p1->treaty[target_idx] = war_treaty_type;
            p2->treaty[player_idx] = war_treaty_type;

            p1->treaty_modifier[target_idx] = -200;
            p2->treaty_modifier[player_idx] = -200;
            p1->trade_modifier[target_idx] = -200;
            p2->trade_modifier[player_idx] = -200;
            p1->tech_exchange_modifier[target_idx] = -200;
            p2->tech_exchange_modifier[player_idx] = -200;
            p1->peace_modifier[target_idx] = -130;
            p2->peace_modifier[player_idx] = -130;

            p1->reward_attack_player[target_idx] = -1;
            p2->reward_attack_player[player_idx] = -1;

            p1->time_since_last_attack[target_idx] = 0;
            p2->time_since_last_attack[player_idx] = 0;

            p1->peace_duration[target_idx] = 0;
            p2->peace_duration[player_idx] = 0;
        }
    }

    int __cdecl Fleet_Strength_Comparison(int16_t player_idx_1, int16_t player_idx_2) {
        int32_t strength1_v_2 = MOX::player[player_idx_1].total_ship_strength[player_idx_2];
        int32_t strength2_v_1 = MOX::player[player_idx_2].total_ship_strength[player_idx_1];

        // Result calculation: ((Strength(p1->p2) * 4 + 4) * 25) / (Strength(p2->p1) + 1)
        int32_t result = ((strength1_v_2 * 4 + 4) * 25) / (strength2_v_1 + 1);

        if (result > 800) {
            result = 800;
        }

        // Apply the original treaty-state reduction for each third-party player in state >= 4.
        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            if (i != player_idx_1 && i != player_idx_2) {
                if (MOX::player[player_idx_1].treaty[i] >= 4) {
                    result >>= 1;
                }
            }
        }

        return (int)result;
    }

    /**
     * Compares the total research strength between two players and returns a relative score.
     * Calculated as: (100 * player1_research) / (player2_research + 1) + 1
     */
    int __cdecl Tech_Strength_Comparison(int16_t player_idx1, int16_t player_idx2) {
        // Load research values and perform scaling.
        // The assembly uses MOVSX to sign-extend the 16-bit research value to 32-bit.
        uint32_t scaled_research1 = (uint32_t)((int32_t)MOX::player[player_idx1].total_research * 100);
        uint32_t divisor = (uint32_t)((int32_t)MOX::player[player_idx2].total_research + 1);

        // DIV instruction in assembly performs unsigned division.
        return (int)(scaled_research1 / divisor) + 1;
    }

    /**
     * @brief Compares the total maintenance of two players.
     *
     * Calculates a ratio based on the total maintenance of player_idx_1 and player_idx_2.
     * The formula used is: (maint1 * 100) / (maint2 + 1) + 1.
     * All calculations are performed using unsigned 32-bit arithmetic as per the
     * x86 DIV instruction behavior.
     */
    int __cdecl Maintenance_Strength_Comparison(int16_t player_idx_1, int16_t player_idx_2) {
        // total_maintenance is a signed int at offset 0xB4 of s_player
        uint32_t maint_1 = (uint32_t)MOX::player[player_idx_1].total_maintenance;
        uint32_t maint_2 = (uint32_t)MOX::player[player_idx_2].total_maintenance;

        // Division by zero is avoided by the +1 in the denominator.
        // The return value is the quotient + 1.
        return (int)((maint_1 * 100) / (maint_2 + 1) + 1);
    }

    void __cdecl Reward_Check(int16_t amount, int16_t p_a, int16_t p_b, int16_t system, int16_t value) {
        (void)amount;
        (void)value;

        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (p_a == i || i == p_b) {
                continue;
            }

            s_player* plr = &MOX::player[i];

            if (plr->reward_attack_player[p_a] == p_b) {
                Change_Relations(200, p_a, i, 15, system, p_b);
            } else {
                if (plr->relations[p_b] <= -50 &&
                    plr->contact[p_a] == 1 &&
                    plr->contact[p_b] == 1 &&
                    plr->reward_attack_player[p_a] == -1) {
                    if (plr->ship_war_losses[i] > plr->ship_war_losses[i]) { /* Dead code branch */
                        int16_t chance = diplodef::personality_reward_proposal_chance[(uint8_t)plr->personality];
                        if (chance > (int16_t)game_random::Random(100)) {
                            int16_t random_amount = (int16_t)game_random::Random(15) + 15;
                            Change_Relations(random_amount, p_a, i, 15, system, p_b);
                        }
                    }
                }
            }
        }
    }

    void __cdecl Change_Relations(int16_t amount, int16_t p_a, int16_t p_b, int16_t incident, int16_t system, int16_t value) {
        if (p_a >= MAX_PLAYERS || p_b >= MAX_PLAYERS || p_a > (int16_t)MOX::NUM_PLAYERS || p_b > (int16_t)MOX::NUM_PLAYERS || p_a == p_b || amount == 0) {
            return;
        }

        if (MOX::player[p_b].objectives == PLAYER_OBJECTIVE_HUMAN) {
            return;
        }

        if (MOX::player[p_a].eliminated == 1 || MOX::player[p_b].eliminated == 1) {
            return;
        }

        if (MOX::player[p_a].contact[p_b] == 0 || MOX::player[p_b].contact[p_a] == 0) {
            return;
        }

        if (MOX::player[p_a].treaty[p_b] == 6 /* War */) {
            return;
        }

        if (incident == 10) {
            Reward_Check(amount, p_a, p_b, system, value);
        }

        // Special mode: Force a severe diplomatic incident
        if (amount == -10000) {
            if (MOX::player[p_a].objectives == PLAYER_OBJECTIVE_HUMAN && incident > 0) {
                MOX::player[p_b].diplomacy_severity[p_a] = 10000;
                MOX::player[p_b].diplomacy_incident[p_a] = (uint8_t)incident;
                MOX::player[p_b].diplomacy_value[p_a] = value;
                MOX::player[p_b].diplomacy_system[p_a] = system;
            }
            return;
        }

        int8_t current_rel = MOX::player[p_b].relations[p_a];
        int16_t scaled_amount;

        // Relation change scaling based on current relations
        if (amount > 0) {
            if (current_rel < 0) {
                scaled_amount = amount * 2;
                if (scaled_amount + current_rel > 10) {
                    scaled_amount = 10 - current_rel;
                }
            } else {
                scaled_amount = amount / (current_rel / 25 + 1);
            }
        } else {
            if (current_rel > 0) {
                scaled_amount = amount * 2;
            } else {
                scaled_amount = amount / (1 - current_rel / 25);
            }
        }

        // Personality modifiers
        if (MOX::player[p_b].personality == PLAYER_PERSONALITY_HONORABLE && scaled_amount < 0) {
            scaled_amount *= 2;
        }

        if (MOX::player[p_b].personality == PLAYER_PERSONALITY_XENOPHOBIC) {
            if (scaled_amount < 0) {
                scaled_amount = (scaled_amount * 3) / 2;
            } else if (scaled_amount > 0) {
                scaled_amount = (scaled_amount * 3) / 4;
            }
        }

        int16_t base_change = scaled_amount;

        // Trait modifier: Charismatic
        if (MOX::player[p_a].traits[TRAIT_CHARISMATIC] == 1) {
            if (scaled_amount < 1) {
                scaled_amount /= 2;
            } else {
                scaled_amount *= 2;
            }
        }

        int8_t treaty = MOX::player[p_b].treaty[p_a];
        int16_t final_change = scaled_amount;

        // Alliance (4, 5) logic
        if ((treaty == 4 || treaty == 5) && incident >= 1 && incident <= 9 && final_change < 0) {
            if (MOX::player[p_a].objectives == PLAYER_OBJECTIVE_HUMAN) {
                int16_t peace_adj = (int16_t)(((int32_t)final_change + ((final_change >> 15) & 3)) >> 2);
                MOX::player[p_b].peace_modifier[p_a] += peace_adj;
                MOX::player[p_a].peace_modifier[p_b] -= peace_adj;
                return;
            }
            final_change = 0;
        }

        // Main relation update (only for non-alliance/war states)
        if (treaty < 4) {
            if (MOX::player[p_a].objectives != PLAYER_OBJECTIVE_HUMAN) {
                // If late game (stardate > 35100) and both players are AI
                if (MOX::player[p_b].objectives != PLAYER_OBJECTIVE_HUMAN && (MOX::stardate > 35100)) {
                    if (final_change > 0) {
                        final_change *= 2;
                    } else if (final_change < 0) {
                        final_change /= (MOX::settings.game_difficulty / 2 + 1);
                    }
                }
            }

            if (final_change != 0) {
                int new_rel = MOX::player[p_b].relations[p_a] + final_change;
                if (new_rel < -100) new_rel = -100;
                if (new_rel > 100) new_rel = 100;
                MOX::player[p_b].relations[p_a] = (int8_t)new_rel;

                // Sync modifiers symmetrically
                MOX::player[p_b].treaty_modifier[p_a] += final_change;
                MOX::player[p_b].trade_modifier[p_a] += final_change;
                MOX::player[p_b].tech_exchange_modifier[p_a] += final_change;

                MOX::player[p_a].treaty_modifier[p_b] = MOX::player[p_b].treaty_modifier[p_a];
                MOX::player[p_a].trade_modifier[p_b] = MOX::player[p_b].trade_modifier[p_a];
                MOX::player[p_a].tech_exchange_modifier[p_b] = MOX::player[p_b].tech_exchange_modifier[p_a];

                // Update peace modifier
                if (base_change < 0) {
                    int16_t peace_adj = (int16_t)(((int32_t)base_change + ((base_change >> 15) & 3)) >> 2);
                    MOX::player[p_a].peace_modifier[p_b] -= peace_adj;
                    MOX::player[p_b].peace_modifier[p_a] += peace_adj;
                } else {
                    MOX::player[p_b].peace_modifier[p_a] += base_change;
                }

                // Check if this incident is more significant than the current stored incident
                int current_sev_abs = abs((int)MOX::player[p_b].diplomacy_severity[p_a]);
                int final_chg_abs = abs((int)final_change);
                if (current_sev_abs < final_chg_abs) {
                    MOX::player[p_b].diplomacy_severity[p_a] = final_change;
                    MOX::player[p_b].diplomacy_incident[p_a] = (uint8_t)incident;
                    MOX::player[p_b].diplomacy_value[p_a] = value;
                    MOX::player[p_b].diplomacy_system[p_a] = system;
                }
            }

            // Cap relations based on treaty status
            if (MOX::player[p_b].treaty[p_a] >= 4 && MOX::player[p_b].relations[p_a] > -25) {
                MOX::player[p_b].relations[p_a] = -25;
            }
            if (MOX::player[p_b].treaty[p_a] != 2 /* treaty 2 exempt state */ && MOX::player[p_b].relations[p_a] > 65) {
                MOX::player[p_b].relations[p_a] = 65;
            }

            // Sync relations symmetrically
            MOX::player[p_a].relations[p_b] = MOX::player[p_b].relations[p_a];
        }
    }

    void __cdecl Break_Treaties(int16_t p1_idx, int16_t p2_idx) {
        if (p2_idx >= (int16_t)MOX::NUM_PLAYERS || p1_idx >= (int16_t)MOX::NUM_PLAYERS) {
            return;
        }

        s_player* p1 = &MOX::player[p1_idx];
        s_player* p2 = &MOX::player[p2_idx];

        if (p1->treaty[p2_idx] == 0) {
            return;
        }

        if (p2->treaty[p1_idx] >= 4) {
            return;
        }

        uint8_t current_treaty = p1->treaty[p2_idx];

        p2->last_broken_treaty[p1_idx] = p2->treaty[p1_idx];
        p2->threats[p1_idx]++;
        p2->dishonored_flag[p1_idx] = 1;
        p1->peace_duration[p2_idx] = 0;
        p2->peace_duration[p1_idx] = 0;

        int16_t penalty = 0;
        if (current_treaty == 1) {
            penalty = -20;
        } else if (current_treaty == 2 || current_treaty == 3) {
            penalty = -40;
        }

        if (p1->personality == PLAYER_PERSONALITY_HONORABLE) {
            penalty *= 3;
        }

        p1->broken_treaties_modifier[p2_idx] -= (char)penalty;

        for (int16_t other_idx = 0; other_idx < (int16_t)MOX::NUM_PLAYERS; ++other_idx) {
            if (other_idx == p1_idx || other_idx == p2_idx) {
                continue;
            }

            s_player* other = &MOX::player[other_idx];
            if (other->eliminated == 0 && (p1->contact[other_idx] == 1 || p2->contact[other_idx] == 1)) {
                if (other->personality == PLAYER_PERSONALITY_HONORABLE) {
                    other->trust_worthiness[p1_idx] -= 20;
                } else {
                    other->trust_worthiness[p1_idx] -= 10;
                }
                other->trust_breaker_player[p1_idx] = (char)p2_idx;
                other->trust_breaker_treaty[p1_idx] = current_treaty;
            }
        }

        if (p2->personality == PLAYER_PERSONALITY_HONORABLE) {
            p2->trust_worthiness[p1_idx] -= 20;
        } else {
            p2->trust_worthiness[p1_idx] -= 10;
        }
        p2->trust_breaker_player[p1_idx] = (char)p2_idx;
        p2->trust_breaker_treaty[p1_idx] = current_treaty;

        if (current_treaty == 2) {
            int new_base = p1->base_relations[p2_idx] - penalty;
            if (new_base < -100) {
                p1->base_relations[p2_idx] = (char)-100;
            } else {
                p1->base_relations[p2_idx] -= (char)penalty;
            }

            if (p1->base_relations[p2_idx] < -100) {
                p1->base_relations[p2_idx] = (char)-100;
            }
            p2->base_relations[p1_idx] = p1->base_relations[p2_idx];
        }

        if (penalty != 0) {
            p1->last_broken_treaty_type[p2_idx] = current_treaty;
            p2->last_broken_treaty_type[p1_idx] = current_treaty;
            p2->relations[p1_idx] = p1->relations[p2_idx];
        }

        p1->treaty[p2_idx] = 0;
        p2->treaty[p1_idx] = 0;
        p1->reward_attack_player[p2_idx] = 0;
        p2->reward_attack_player[p1_idx] = 0;

        if (p1->relations[p2_idx] < -100) {
            p1->relations[p2_idx] = (char)-100;
        }
        p2->relations[p1_idx] = p1->relations[p2_idx];

        p1->treaty_modifier[p2_idx] = -200;
        p1->trade_modifier[p2_idx] = -200;
        p1->tech_exchange_modifier[p2_idx] = -200;
        p1->peace_modifier[p2_idx] = -200;

        p2->treaty_modifier[p1_idx] = -200;
        p2->trade_modifier[p1_idx] = -200;
        p2->tech_exchange_modifier[p1_idx] = -200;
        p2->peace_modifier[p1_idx] = -200;
    }

    void __cdecl Break_Trade(int16_t p1_idx, int16_t p2_idx) {
        // Preserve original signed upper-bound-only check semantics; negatives are not rejected here.
        if (p1_idx >= (int16_t)MOX::NUM_PLAYERS || p2_idx >= (int16_t)MOX::NUM_PLAYERS) {
            return;
        }

        s_player* plr1 = &MOX::player[p1_idx];
        s_player* plr2 = &MOX::player[p2_idx];

        uint8_t original_treaty = plr1->trade_treaty[p2_idx];
        if (original_treaty == 0) {
            return;
        }

        plr2->last_broken_treaty[p1_idx] = 7;

        if (plr1->relations[p2_idx] < -100) {
            plr1->relations[p2_idx] = -100;
        }

        plr2->relations[p1_idx] = plr1->relations[p2_idx];
        plr1->trade_treaty[p2_idx] = 0;
        plr2->trade_treaty[p1_idx] = 0;
        plr1->reward_attack_player[p2_idx] = 0;
        plr2->reward_attack_player[p1_idx] = 0;

        // Additional penalties if no non-aggression pact exists and it was a trade treaty (type 1)
        if (plr2->treaty[p1_idx] < 4 && original_treaty == 1) {
            if (plr1->personality == PLAYER_PERSONALITY_HONORABLE) {
                plr1->broken_treaties_modifier[p2_idx] -= 15;
            } else {
                plr1->broken_treaties_modifier[p2_idx] -= 5;
            }

            int32_t drop = game_random::Random(20) + game_random::Random(20);
            int32_t new_rel = (int32_t)plr1->relations[p2_idx] - drop;
            if (new_rel < -100) {
                plr1->relations[p2_idx] = -100;
            } else {
                plr1->relations[p2_idx] = (int8_t)new_rel;
            }

            if (plr1->relations[p2_idx] < -100) {
                plr1->relations[p2_idx] = -100;
            }
            plr2->relations[p1_idx] = plr1->relations[p2_idx];

            int32_t base_drop = game_random::Random(20);
            int32_t new_base = (int32_t)plr1->base_relations[p2_idx] - base_drop;
            if (new_base < -100) {
                plr1->base_relations[p2_idx] = -100;
            } else {
                plr1->base_relations[p2_idx] = (int8_t)new_base;
            }
            plr2->base_relations[p1_idx] = plr1->base_relations[p2_idx];

            plr1->last_broken_treaty_type[p2_idx] = 7;

            const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            for (int16_t i = 0; i < player_count; ++i) {
                if (i != p1_idx && i != p2_idx) {
                    plr1->broken_treaties_modifier[i] -= 5;
                }
            }
        }
    }

    void __cdecl Limit_Treaty_Modifiers() {
        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            for (int j = 0; j < (int)MOX::NUM_PLAYERS; ++j) {
                if (i == j) {
                    continue;
                }

                if (MOX::player[i].treaty_modifier[j] > 120) {
                    MOX::player[i].treaty_modifier[j] = 120;
                }

                if (MOX::player[i].treaty_modifier[j] < -200) {
                    MOX::player[i].treaty_modifier[j] = -200;
                }
            }
        }
    }

    /**
     * Chooses a positive diplomacy message and any follow-up demand for the current incident.
     */
    void __cdecl Determine_Good_Message(int16_t player_idx, int16_t other_player_idx) {
        uint8_t incident = (uint8_t)MOX::player[player_idx].diplomacy_incident[other_player_idx];
        int16_t chance = 3;

        if (incident == 15 /* 0x0F */) {
            chance = 10;
        }
        if (incident == 16 /* 0x10 */) {
            chance = 20;
        }

        if ((int16_t)game_random::Random(100) > chance) {
            return;
        }

        MOX::player[player_idx].threats[other_player_idx] = 0;

        switch ((uint8_t)MOX::player[player_idx].diplomacy_incident[other_player_idx]) {
            case 12: /* 0x0C */
                MOX::player[player_idx].diplomacy_message[other_player_idx] = 'Y';
                break;

            case 13: /* 0x0D */
                MOX::player[player_idx].diplomacy_message[other_player_idx] = 'Z';
                break;

            case 14: /* 0x0E */
                MOX::player[player_idx].diplomacy_message[other_player_idx] = '[';
                break;

            case 15: /* 0x0F */
                if (MOX::player[player_idx].reward_attack_player[other_player_idx] != -1) {
                    MOX::player[player_idx].reward_attack_player[other_player_idx] = -1;
                    MOX::player[player_idx].diplomacy_message[other_player_idx] = ']';
                } else if (game_random::Random(100) + MOX::player[player_idx].relations[other_player_idx] >= 75 /* 0x4B */) {
                    MOX::player[player_idx].diplomacy_message[other_player_idx] = '^';
                } else {
                    int16_t demands = 0;
                    if (game_random::Random(3) == 1 &&
                        (demands = Get_Demands(other_player_idx, player_idx, 4, 0, 1, 1, 0)) == 1) {
                        MOX::player[player_idx].reward_amount[other_player_idx] = diplomacy_proposal_gold_bribe;
                        MOX::player[player_idx].reward_tech[other_player_idx] = diplomacy_proposal_tech_bribe1;
                        MOX::player[player_idx].diplomacy_message[other_player_idx] = ']';
                        Set_Demands(player_idx, other_player_idx);
                    } else {
                        MOX::player[player_idx].diplomacy_message[other_player_idx] = '\\';
                    }
                }
                break;

            case 16: /* 0x10 */
            {
                int16_t demands = 0;
                if (game_random::Random(4) == 1 &&
                    (demands = Get_Demands(other_player_idx, player_idx, 4, 0, 1, 1, 0)) == 1) {
                    MOX::player[player_idx].diplomacy_message[other_player_idx] = '`';
                    Set_Demands(player_idx, other_player_idx);
                } else {
                    MOX::player[player_idx].diplomacy_message[other_player_idx] = '_';
                }
            } break;

            default:
                break;
        }
    }

    void __cdecl Determine_Bad_Message(int16_t current_player, int16_t target_player) {
        s_player* plr = &MOX::player[current_player];
        uint8_t incident = plr->diplomacy_incident[target_player];
        int16_t severity = plr->diplomacy_severity[target_player];

        if (severity == 0) return;

        if (plr->treaty[target_player] >= 4) {
            Peace_Proposal(current_player, target_player, 1);
            return;
        }

        bool p1_repulsive = (MOX::player[current_player].traits[TRAIT_REPULSIVE] == 1);
        bool p2_repulsive = (MOX::player[target_player].traits[TRAIT_REPULSIVE] == 1);

        if (!(p1_repulsive || p2_repulsive) || severity == -10000) {
            int16_t personality = Get_Player_Diplomacy_Personality(current_player, target_player);
            int32_t adj_severity = (int32_t)severity * 2;
            if (adj_severity < 0) {
                adj_severity = -adj_severity;
            }

            adj_severity -= diplodef::personality_relation_modifiers[personality];
            adj_severity -= plr->broken_treaties_modifier[target_player];
            adj_severity -= plr->trust_worthiness[target_player];

            if (incident == 9 && plr->council_threat == target_player) {
                adj_severity = 100;
                plr->council_threat = -1;
            }

            if (plr->relations[target_player] < 0) {
                adj_severity -= (int16_t)plr->relations[target_player];
            }

            if ((int16_t)game_random::Random(100) <= adj_severity) {
                plr->threats[target_player]++;
            }
            if ((int16_t)game_random::Random(100) <= adj_severity / 2) {
                plr->threats[target_player]++;
            }
            if ((int16_t)game_random::Random(100) <= adj_severity / 4) {
                plr->threats[target_player]++;
            }

            bool has_active_bond = (plr->treaty[target_player] == 1 || plr->treaty[target_player] == 2 ||
                plr->trade_treaty[target_player] != 0 || plr->research_treaty[target_player] != 0);

            if (incident >= 1 && incident <= 9) {
                plr->last_bad_diplomatic_incident[target_player] = incident;
            }

            if (plr->threats[target_player] > 0) {
                int16_t p_check = Get_Player_Diplomacy_Personality(current_player, target_player);
                if (p_check == PLAYER_PERSONALITY_HONORABLE) {
                    if (has_active_bond) {
                        plr->dishonored_flag[target_player] = 1;
                        MOX::player[target_player].dishonored_flag[current_player] = 1;
                        Declare_War(target_player, current_player);
                        plr->diplomacy_message[target_player] = 0x42 /* 'B' */;
                        return;
                    }
                }
            }

            int8_t threats = plr->threats[target_player];
            if (threats < 3 && (threats < 2 || has_active_bond)) {
                if (threats == 1) {
                    if (has_active_bond) {
                        plr->diplomacy_message[target_player] = incident + 0x26 /* '&' */;
                    } else {
                        plr->diplomacy_message[target_player] = incident + 0x12;
                    }
                    return;
                }

                if (threats == 2 && has_active_bond) {
                    int16_t p_idx = Get_Player_Diplomacy_Personality(current_player, target_player);
                    int16_t demand_chance = diplodef::threat_demand_chance[p_idx];
                    if ((int16_t)game_random::Random(100) <= demand_chance) {
                        if (Get_Demands(current_player, target_player, 6, 1, 1, 1, 1) == 1) {
                            plr->diplomacy_message[target_player] = incident + 0x1B;
                            Set_Demands(current_player, target_player);
                            return;
                        }
                    }
                    Break_Treaties(target_player, current_player);
                    Break_Research(target_player, current_player);
                    Break_Trade(target_player, current_player);
                    plr->diplomacy_message[target_player] = incident + 0x2F /* '/' */;
                    return;
                }
            } else {
                Sneak_Attack_Or_Declare_War(current_player, target_player, incident);
            }
        }
    }

    void __cdecl Peace_Proposal(int16_t player_idx, int16_t target_idx, int16_t) {
        s_player* plr = &MOX::player[player_idx];
        uint8_t time_since_attack = plr->time_since_last_attack[target_idx];

        if (time_since_attack <= 9) {
            return;
        }

        int16_t base_chance = (int16_t)time_since_attack - 9;

        int32_t p1_attacks = AIMOVE::Number_Of_Attacks_Underway(player_idx, target_idx);
        int32_t p2_attacks = AIMOVE::Number_Of_Attacks_Underway(target_idx, player_idx);

        int16_t war_count_p2_p1 = Get_War_Count(target_idx, player_idx);
        int16_t war_count_p1_p2 = Get_War_Count(player_idx, target_idx);

        int16_t score = (int16_t)(((p1_attacks * 2 - p2_attacks) * 4 - war_count_p2_p1 + war_count_p1_p2 - 2) * 5);

        int16_t personality_idx = Get_Player_Diplomacy_Personality(player_idx, target_idx);
        score -= (int16_t)(diplodef::personality_relation_modifiers[personality_idx] / 5);

        score += Total_War_Loss_Comparison(player_idx, target_idx) / 10;
        score += (int16_t)(Fleet_Strength_Comparison(player_idx, target_idx) / 10);

        score += (int16_t)(plr->broken_treaties_modifier[target_idx] / 2);
        score += (int16_t)plr->trust_worthiness[target_idx];

        for (int16_t p = 0; p < (int16_t)MOX::NUM_PLAYERS; ++p) {
            if (p != player_idx && p != target_idx) {
                score += (int16_t)(AIMOVE::Number_Of_Attacks_Underway(p, target_idx) * 20);
            }
        }

        int16_t roll_threshold;
        if (score < 0) {
            roll_threshold = -(score * base_chance);
        } else {
            uint32_t root = special::isqrt(score + 3);
            roll_threshold = (int16_t)((uint32_t)base_chance / root);
        }

        if ((int16_t)game_random::Random(100) > roll_threshold) {
            return;
        }

        if (score < -10) {
            int16_t demand_val = (int16_t)((-10 - score) / 5 + 2);
            if (Get_Demands(target_idx, player_idx, demand_val, 1, 1, 1, 1) == 1) {
                plr->diplomacy_message[target_idx] = 'U'; /* 0x55 */
                Set_Demands(player_idx, target_idx);
            } else {
                plr->diplomacy_message[target_idx] = 'S'; /* 0x53 */
            }
        } else if (score > 10) {
            int16_t demand_val = (int16_t)((score - 10) / 5 + 2);
            if (Get_Demands(player_idx, target_idx, demand_val, 1, 1, 1, 1) == 1) {
                plr->diplomacy_message[target_idx] = 'T'; /* 0x54 */
                Set_Demands(player_idx, target_idx);
            } else {
                plr->diplomacy_message[target_idx] = 'S'; /* 0x53 */
            }
        } else {
            plr->diplomacy_message[target_idx] = 'S'; /* 0x53 */
        }

        plr->time_since_last_attack[target_idx] = 0;
    }

    void __cdecl Declare_Peace(int16_t p1, int16_t p2) {
        MOX::player[p1].treaty[p2] = 3 /* PEACE */;
        MOX::player[p2].treaty[p1] = 3 /* PEACE */;

        // Improve relations by 50, but cap at 0 (neutral) if previously negative (war)
        MOX::player[p1].relations[p2] += 50;
        if (MOX::player[p1].relations[p2] > 0) {
            MOX::player[p1].relations[p2] = 0;
        }
        MOX::player[p2].relations[p1] = MOX::player[p1].relations[p2];

        Get_Player_Diplomacy_Personality(p1, p2);
        MOX::player[p1].peace_duration[p2] = 30;

        Get_Player_Diplomacy_Personality(p2, p1);
        MOX::player[p2].peace_duration[p1] = 30;

        MOX::player[p1].threats[p2] = 0;
        MOX::player[p2].threats[p1] = 0;

        MOX::player[p1].ship_war_losses[p2] = 0;
        MOX::player[p2].ship_war_losses[p1] = 0;

        MOX::player[p1].colony_war_losses[p2] = 0;
        MOX::player[p2].colony_war_losses[p1] = 0;

        MOX::player[p1].biological_weapons_flag[p2] = 0;
        MOX::player[p2].biological_weapons_flag[p1] = 0;
    }

    /**
     * Tries to queue a diplomacy message asking another player to attack a third party.
     */
    void __cdecl Required_Alliances(int16_t player_idx, int16_t other_player_idx) {
        if (MOX::player[player_idx].treaty[other_player_idx] == 2) {
            if (game_random::Random(10) == 1) {
                int16_t target_ally_idx = -1;

                for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
                    // Cannot target the players involved in the primary check
                    if (i == player_idx || i == other_player_idx) {
                        continue;
                    }

                    if (MOX::player[i].treaty[player_idx] >= 4) {
                        if (MOX::player[other_player_idx].contact[i] == 1 &&
                            MOX::player[other_player_idx].within_range[i] == 1 &&
                            MOX::player[other_player_idx].treaty[i] < 4) {
                            target_ally_idx = i;
                        }
                    }
                }

                if (target_ally_idx != -1) {
                    // Set diplomacy message 'V' (likely a war demand code)
                    MOX::player[player_idx].diplomacy_message[other_player_idx] = 'V';
                    MOX::player[player_idx].diplomacy_proposal_war_player[other_player_idx] = (int8_t)target_ally_idx;
                }
            }
        }
    }

    void __cdecl Unprovoked_Good_Messages(int16_t player_idx, int16_t target_idx) {
        s_player& player = MOX::player[player_idx];

        if (player.treaty[target_idx] >= 4) return;

        if (npcdiplo::NPC_Good_Proposal_Check(player_idx, target_idx) == 0) return;

        int choice = 0;
        for (int i = 0; i < 3; ++i) {
            int roll = game_random::Random(20);
            if (roll > 0 && roll <= 20) {
                if (roll <= 5) {
                    choice = 5;
                } else if (roll <= 9) {
                    if (player.treaty[target_idx] == 0) choice = 2;
                } else if (roll <= 12) {
                    if (player.relations[target_idx] >= 0 && player.trade_treaty[target_idx] == 0) choice = 1;
                } else if (roll <= 15) {
                    if (player.relations[target_idx] >= 0 && player.research_treaty[target_idx] == 0) choice = 4;
                } else if (roll <= 18) {
                    if (player.treaty[target_idx] == 1) choice = 3;
                } else if (player.treaty[target_idx] != 2 && game_random::Random(3) == 1 && player.relations[target_idx] >= 0) {
                    choice = 6;
                }
            }
        }

        if (choice == 5) {
            npcdiplo::NPC_Tech_Exchange_Check(player_idx, target_idx);
            if (player.diplomacy_message[target_idx] != 0) {
                npcdiplo::Clear_Last_Proposal_Turn(player_idx, target_idx);
                return;
            }
        }

        if (choice == 2) {
            int threshold = (diplodef::personality_relation_modifiers[(uint8_t)player.personality] + (10 - MOX::settings.game_difficulty) * 25) * 2;
            if ((int32_t)game_random::Random((uint32_t)threshold) >= Fleet_Strength_Comparison(player_idx, target_idx)) {
                player.diplomacy_message[target_idx] = 'a' /* PROPOSAL_PEACE */;
                npcdiplo::Clear_Last_Proposal_Turn(player_idx, target_idx);
                return;
            }
        }

        if (choice == 3) {
            int threshold = (diplodef::personality_relation_modifiers[(uint8_t)player.personality] + (7 - MOX::settings.game_difficulty) * 25) * 2;
            if ((int32_t)game_random::Random((uint32_t)threshold) >= Fleet_Strength_Comparison(player_idx, target_idx)) {
                player.diplomacy_message[target_idx] = 'b' /* PROPOSAL_ALLIANCE */;
                npcdiplo::Clear_Last_Proposal_Turn(player_idx, target_idx);
                return;
            }
        }

        if (choice == 4) {
            int threshold = Tech_Strength_Comparison(player_idx, target_idx) + (MOX::settings.game_difficulty * 25 - diplodef::personality_relation_modifiers[(uint8_t)player.personality]) * 2;
            if (player.treaty[target_idx] == 1) threshold -= 25;
            if (player.treaty[target_idx] == 2) threshold -= 50;
            if ((int32_t)game_random::Random(300) >= threshold) {
                player.diplomacy_message[target_idx] = 'd' /* PROPOSAL_RESEARCH_TREATY */;
                npcdiplo::Clear_Last_Proposal_Turn(player_idx, target_idx);
                return;
            }
        }

        if (choice == 1) {
            int threshold = Maintenance_Strength_Comparison(player_idx, target_idx) + (MOX::settings.game_difficulty * 25 - diplodef::personality_relation_modifiers[(uint8_t)player.personality]) * 2;
            if (player.treaty[target_idx] == 1) threshold -= 25;
            if (player.treaty[target_idx] == 2) threshold -= 50;

            bool trade_offered = false;
            if ((int32_t)game_random::Random(300) >= threshold) {
                player.diplomacy_message[target_idx] = 'c' /* PROPOSAL_TRADE_TREATY */;
                trade_offered = true;
            }

            if (trade_offered) {
                npcdiplo::Clear_Last_Proposal_Turn(player_idx, target_idx);
                return;
            }

            // If trade treaty roll failed, check for potential common enemy proposal
            int16_t candidates[20];
            int16_t num_candidates = 0;
            for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
                if (i != player_idx && i != target_idx) {
                    if (player.contact[i] == 1 && MOX::player[target_idx].contact[i] == 1 &&
                        MOX::player[target_idx].treaty[i] < 4 && player.relations[i] <= -50 &&
                        MOX::player[i].ship_war_losses[player_idx] < player.ship_war_losses[i] &&
                        player.personality != PLAYER_PERSONALITY_XENOPHOBIC && player.time_since_last_attack[i] >= 20 &&
                        player.ship_war_losses[i] > 0) {
                        candidates[num_candidates++] = i;
                    }
                }
            }

            if (num_candidates > 0) {
                int16_t victim_idx = candidates[game_random::Random(num_candidates) - 1];
                int32_t ratio = player.ship_war_losses[victim_idx] / (MOX::player[victim_idx].ship_war_losses[player_idx] + 1);
                if (ratio > 6) ratio = 6;

                if (Get_Demands(target_idx, player_idx, (int16_t)ratio, 0, 1, 1, 0) == 1) {
                    player.diplomacy_message[target_idx] = '}' /* PROPOSAL_ATTACK_PLAYER */;
                    player.reward_amount[target_idx] = diplomacy_proposal_gold_bribe;
                    player.reward_tech[target_idx] = diplomacy_proposal_tech_bribe1;
                    player.reward_attack_player[target_idx] = (int8_t)victim_idx;
                    npcdiplo::Clear_Last_Proposal_Turn(player_idx, target_idx);
                    Set_Demands(player_idx, target_idx);
                    return;
                }
            }
        }

        npcdiplo::Clear_Last_Proposal_Turn(player_idx, target_idx);
    }

    void __cdecl NPC_Required_Alliances(int16_t p1_idx, int16_t p2_idx) {
        int active_plr = (int)p1_idx;
        int other_plr = (int)p2_idx;

        // Only consider the treaty state that enables this NPC pressure path.
        if (MOX::player[active_plr].treaty[other_plr] == 2) {
            if (game_random::Random(10) == 1) {
                int16_t target_idx = -1;

                for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
                    if (i != p1_idx && i != p2_idx) {
                        // Check whether the candidate is in the required treaty state with other_plr,
                        // and whether active_plr knows them, can reach them, and is not already in state >= 4.
                        if ((MOX::player[i].treaty[other_plr] == 4 || MOX::player[i].treaty[other_plr] == 5) &&
                            (MOX::player[active_plr].contact[i] == 1 &&
                                MOX::player[active_plr].within_range[i] == 1 &&
                                MOX::player[active_plr].treaty[i] < 4)) {
                            target_idx = i;
                        }
                    }
                }

                if (target_idx != -1) {
                    // Set diplomacy message to 'F' and the proposed war target
                    MOX::player[active_plr].diplomacy_message[other_plr] = 0x46 /* 'F' */;
                    MOX::player[active_plr].diplomacy_proposal_war_player[other_plr] = (int8_t)target_idx;

                    Declare_War(p1_idx, target_idx);
                }
            }
        }
    }

    /**
     * Updates mutual diplomacy demands between two players using globally stored proposal values.
     */
    void __cdecl Set_Demands(int16_t p1, int16_t p2) {
        s_player* players = MOX::player;

        // Set demands for player 1 against player 2
        players[p1].diplomacy_proposal_tribute_bribe[p2] = diplomacy_proposal_tribute_bribe;
        players[p1].diplomacy_proposal_tech_bribe1[p2] = diplomacy_proposal_tech_bribe1;
        players[p1].diplomacy_proposal_gold_bribe[p2] = diplomacy_proposal_gold_bribe;
        players[p1].diplomacy_proposal_system_bribe[p2] = diplomacy_proposal_system_bribe;

        // Set demands for player 2 against player 1
        players[p2].diplomacy_proposal_tribute_bribe[p1] = diplomacy_proposal_tribute_bribe;
        players[p2].diplomacy_proposal_tech_bribe1[p1] = diplomacy_proposal_tech_bribe1;
        players[p2].diplomacy_proposal_gold_bribe[p1] = diplomacy_proposal_gold_bribe;
        players[p2].diplomacy_proposal_system_bribe[p1] = diplomacy_proposal_system_bribe;
    }

    void Diplomacy_Growth() {
        // Phase 1: Basic relation changes based on treaties and random factors
        for (int p1 = 0; p1 < (int)MOX::NUM_PLAYERS; ++p1) {
            for (int p2 = p1 + 1; p2 < (int)MOX::NUM_PLAYERS; ++p2) {
                // Objectives 100 is typically used for specific internal logic (e.g., Orion)
                if (MOX::player[p1].objectives != PLAYER_OBJECTIVE_HUMAN || MOX::player[p2].objectives != PLAYER_OBJECTIVE_HUMAN) {
                    if (game_random::Random(2) == 1) {
                        if (MOX::player[p1].relations[p2] < -50 && MOX::player[p1].peace_duration[p2] > 0) {
                            Change_Relations(1, p1, p2, 0, 0, 0);
                        }
                    }

                    if (MOX::player[p1].treaty[p2] == 1 /* Peace */) {
                        if ((int16_t)game_random::Random(100) <= (100 - MOX::player[p1].relations[p2])) {
                            Change_Relations(game_random::Random(3), p1, p2, 0, 0, 0);
                        }
                    }

                    if (MOX::player[p1].trade_treaty[p2] == 1) {
                        if ((int16_t)game_random::Random(100) <= (100 - MOX::player[p1].relations[p2])) {
                            Change_Relations(game_random::Random(3), p1, p2, 12 /* TRADE_TREATY */, 0, 0);
                        }
                    }

                    if (MOX::player[p1].research_treaty[p2] == 1) {
                        if ((int16_t)game_random::Random(100) <= (100 - MOX::player[p1].relations[p2])) {
                            Change_Relations(game_random::Random(3), p1, p2, 13 /* RESEARCH_TREATY */, 0, 0);
                        }
                    }

                    if (MOX::player[p1].treaty[p2] == 2 /* Alliance */) {
                        Change_Relations(game_random::Random(5), p1, p2, 0, 0, 0);
                    }

                    if (MOX::player[p1].tribute_treaty[p2] == 1) {
                        if ((int16_t)game_random::Random(100) <= (100 - MOX::player[p1].relations[p2])) {
                            Change_Relations(game_random::Random(3), p1, p2, 14 /* TRIBUTE */, 0, 0);
                        }
                    }

                    if (MOX::player[p1].tribute_treaty[p2] == 2) {
                        if ((int16_t)game_random::Random(100) <= (100 - MOX::player[p1].relations[p2])) {
                            Change_Relations(game_random::Random(8), p1, p2, 14 /* TRIBUTE */, 0, 0);
                        }
                    }

                    if (MOX::player[p1].relations[p2] > 100) {
                        MOX::player[p1].relations[p2] = 100;
                    }
                    if (MOX::player[p2].relations[p1] > 100) {
                        MOX::player[p2].relations[p1] = 100;
                    }
                }
            }
        }

        // Phase 2: Adjust relations if a specific objective player threatens a normal player
        for (int p1 = 0; p1 < (int)MOX::NUM_PLAYERS; ++p1) {
            for (int p2 = 0; p2 < (int)MOX::NUM_PLAYERS; ++p2) {
                if (MOX::player[p1].objectives == PLAYER_OBJECTIVE_HUMAN && MOX::player[p2].objectives != PLAYER_OBJECTIVE_HUMAN && MOX::player[p1].treaty[p2] != 2) {
                    int16_t star_id = -1;
                    int threat_val;
                    int16_t planet_id = -1;
                    AIMOVE::Player_Threatens_Player(p1, p2, &star_id, &threat_val, &planet_id);

                    if (planet_id != -1) {
                        int16_t col_idx = MOX::planet[planet_id].colony_index;
                        if (col_idx != -1) {
                            if (star_id != -1 && MOX::colony[col_idx].n_pops > 4 && threat_val > 100) {
                                int amount = -((MOX::colony[col_idx].n_pops - 4) + (threat_val / 50));
                                Change_Relations(amount, p1, p2, 5, star_id, 0);
                            }
                        }
                    }
                }
            }
        }

        // Phase 3: Drift relations back toward base relations
        for (int p1 = 0; p1 < (int)MOX::NUM_PLAYERS; ++p1) {
            for (int p2 = 0; p2 < (int)MOX::NUM_PLAYERS; ++p2) {
                int8_t drift = 0;
                int8_t current_rel = MOX::player[p1].relations[p2];

                if (abs(current_rel) < (int)game_random::Random(105)) {
                    if (game_random::Random(4) == 1) {
                        drift = game_random::Random(2) - 1; // drift is 0 or -1
                    }
                }

                if (p1 != p2 && MOX::player[p1].last_turn_diplomacy_severity[p2] == 0) {
                    int8_t base_rel = MOX::player[p1].base_relations[p2];

                    if (current_rel < base_rel) {
                        current_rel += drift;
                        if (current_rel > base_rel) {
                            current_rel = base_rel;
                        }
                    } else {
                        current_rel -= drift;
                        if (current_rel < base_rel) {
                            current_rel = base_rel;
                        }
                    }

                    // Clamping
                    if (current_rel < -100) current_rel = -100;
                    if (current_rel > 100) current_rel = 100;
                    MOX::player[p1].relations[p2] = current_rel;
                }

                // Enforce minimum hostility if at war or broken treaties
                if (MOX::player[p1].treaty[p2] >= 4 && MOX::player[p1].relations[p2] > -90) {
                    MOX::player[p1].relations[p2] = -90;
                    MOX::player[p2].relations[p1] = -90;
                }
            }
        }

        // Phase 4: Synchronize relations between players
        for (int p1 = 0; p1 < (int)MOX::NUM_PLAYERS; ++p1) {
            for (int p2 = 0; p2 < (int)MOX::NUM_PLAYERS; ++p2) {
                if (p1 == p2) {
                    MOX::player[p1].relations[p2] = 0;
                } else {
                    MOX::player[p1].relations[p2] = MOX::player[p2].relations[p1];
                }
            }
        }

        // Phase 5: Decrement diplomatic block/spy durations
        for (int p1 = 0; p1 < (int)MOX::NUM_PLAYERS; ++p1) {
            for (int p2 = 0; p2 < (int)MOX::NUM_PLAYERS; ++p2) {
                if (MOX::player[p1].stop_spying_duration[p2] != 0) {
                    MOX::player[p1].stop_spying_duration[p2]--;
                }
                if (MOX::player[p1].stop_blockading_duration[p2] != 0) {
                    MOX::player[p1].stop_blockading_duration[p2]--;
                }
            }
        }

        // Phase 6: Global war if council flag is set
        if (diplodef::COUNCIL_FLAG == 3) {
            for (int p1 = 0; p1 < (int)MOX::NUM_PLAYERS; ++p1) {
                for (int p2 = 0; p2 < (int)MOX::NUM_PLAYERS; ++p2) {
                    if (p1 != p2) {
                        MOX::player[p1].treaty[p2] = 6 /* TOTAL_WAR */;
                        MOX::player[p2].treaty[p1] = 6;
                        MOX::player[p1].relations[p2] = -100;
                        MOX::player[p2].relations[p1] = -100;
                    }
                }
            }
        }
    }

    void __cdecl Adjust_Diplomat_Modifiers(int16_t player_idx, int16_t target_idx) {
        // Decrement diplomatic modifiers for a player relative to a target player.
        // Array indexing into player data for player_idx and target_idx.
        MOX::player[player_idx].trade_modifier[target_idx] -= 10;
        MOX::player[player_idx].treaty_modifier[target_idx] -= 10;
        MOX::player[player_idx].tech_exchange_modifier[target_idx] -= 10;
        MOX::player[player_idx].peace_modifier[target_idx] -= 10;
    }

    void __cdecl Clear_Last_Threat_Turn(int16_t player_idx_1, int16_t player_idx_2) {
        MOX::player[player_idx_1].last_diplomacy_threat_turn[player_idx_2] = 0;
        MOX::player[player_idx_2].last_diplomacy_threat_turn[player_idx_1] = 0;
    }

    /**
     * Compares ship war losses between two players relative to target player's losses from allies.
     * Returns a score between -300 and 300 representing relative conflict intensity.
     */
    int16_t __cdecl Total_War_Loss_Comparison(int16_t attacker_idx, int16_t target_idx) {
        int ally_losses_sum = 0;

        if ((int)MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
                // Check if player is active and has an alliance with the target player
                // 100 (0x64) is a status/objective flag, 4 is TREATY_ALLIANCE
                if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN && MOX::player[target_idx].treaty[i] >= 4) {
                    ally_losses_sum += MOX::player[target_idx].ship_war_losses[i];
                }
            }
        }

        if (ally_losses_sum < 10) {
            ally_losses_sum = 10;
        }

        int attacker_losses_vs_target = MOX::player[attacker_idx].ship_war_losses[target_idx];
        if (attacker_losses_vs_target < 10) {
            attacker_losses_vs_target = 10;
        }

        int result;
        if (attacker_losses_vs_target > ally_losses_sum) {
            // Attacker is suffering more losses than the target's allies are inflicting on the target
            result = 100 - (attacker_losses_vs_target * 100) / ally_losses_sum;
        } else {
            // Attacker is suffering fewer losses than the target's allies are inflicting on the target
            result = (ally_losses_sum * 100) / attacker_losses_vs_target - 100;
        }

        // Clamp result to [-300, 300]
        if (result > 300) {
            result = 300;
        } else if (result < -300) {
            result = -300;
        }

        return (int16_t)result;
    }

    void __cdecl Break_Research(int16_t player1_idx, int16_t player2_idx) {
        if (player1_idx >= (int16_t)MOX::NUM_PLAYERS || player2_idx >= (int16_t)MOX::NUM_PLAYERS) {
            return;
        }

        int16_t p1 = player1_idx;
        int16_t p2 = player2_idx;

        if (MOX::player[p1].research_treaty[p2] != 0) {
            int8_t research_val = MOX::player[p1].research_treaty[p2];
            MOX::player[p2].last_broken_treaty[p1] = 8;

            if (MOX::player[p1].relations[p2] < -100) {
                MOX::player[p1].relations[p2] = -100;
            }
            MOX::player[p2].relations[p1] = MOX::player[p1].relations[p2];

            MOX::player[p1].research_treaty[p2] = 0;
            MOX::player[p2].research_treaty[p1] = 0;

            MOX::player[p1].reward_attack_player[p2] = 0;
            MOX::player[p2].reward_attack_player[p1] = 0;

            // Check if player2 is not in a significant alliance and the broken research treaty was active (val 1)
            if (MOX::player[p2].treaty[p1] < 4 && research_val == 1) {
                if (MOX::player[p1].personality == PLAYER_PERSONALITY_HONORABLE) {
                    MOX::player[p1].broken_treaties_modifier[p2] -= 15;
                } else {
                    MOX::player[p1].broken_treaties_modifier[p2] -= 5;
                }

                int penalty = game_random::Random(20) + game_random::Random(20);
                int new_rel = MOX::player[p1].relations[p2] - penalty;
                if (new_rel < -100) {
                    new_rel = -100;
                }
                MOX::player[p1].relations[p2] = (int8_t)new_rel;

                // Redundant safety check from original code
                if (MOX::player[p1].relations[p2] < -100) {
                    MOX::player[p1].relations[p2] = -100;
                }
                MOX::player[p2].relations[p1] = MOX::player[p1].relations[p2];

                int base_penalty = game_random::Random(20);
                int new_base_rel = MOX::player[p1].base_relations[p2] - base_penalty;
                if (new_base_rel < -100) {
                    new_base_rel = -100;
                }
                MOX::player[p1].base_relations[p2] = (int8_t)new_base_rel;
                MOX::player[p2].base_relations[p1] = MOX::player[p1].base_relations[p2];

                MOX::player[p1].last_broken_treaty_type[p2] = 8;

                // Penalize player1's reputation with all other players
                const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
                for (int16_t i = 0; i < player_count; ++i) {
                    if (i != p1 && i != p2) {
                        MOX::player[p1].broken_treaties_modifier[i] -= 5;
                    }
                }
            }
        }
    }

    void __cdecl Break_Tribute(int16_t p1_idx, int16_t p2_idx) {
        if (p2_idx >= (int16_t)MOX::NUM_PLAYERS || p1_idx >= (int16_t)MOX::NUM_PLAYERS) {
            return;
        }

        s_player* p1 = &MOX::player[p1_idx];
        s_player* p2 = &MOX::player[p2_idx];

        // REVIEW: This sentinel pointer check likely stands in for a missing recovered field
        // and depends on implementation-defined pointer-to-integer conversion.
        if ((std::intptr_t)p1 == kInvalidTributePlayerMarker) {
            return;
        }

        p2->threats[p1_idx]++;

        int16_t old_tribute = p1->tribute_treaty[p2_idx];

        // Ensure relations are within floor and synchronized
        if (p1->relations[p2_idx] < -100) {
            p1->relations[p2_idx] = -100;
        }
        p2->relations[p1_idx] = p1->relations[p2_idx];

        // Clear existing tribute and related reward targets
        p1->tribute_treaty[p2_idx] = 0;
        p2->tribute_treaty[p1_idx] = 0;
        p1->reward_attack_player[p2_idx] = 0;
        p2->reward_attack_player[p1_idx] = 0;

        // If there was an active tribute and no existing alliance, apply penalties
        if (p2->treaty[p1_idx] < 4 /* TREATY_ALLIANCE */ && old_tribute > 0) {
            if (p1->personality == PLAYER_PERSONALITY_HONORABLE) {
                p1->broken_treaties_modifier[p2_idx] -= 20;
            } else {
                p1->broken_treaties_modifier[p2_idx] -= 10;
            }

            // Calculate a random hit to diplomatic relations
            int16_t penalty = (int16_t)game_random::Random(50) + (int16_t)game_random::Random(50);
            int32_t relation_after = (int32_t)p1->relations[p2_idx] - penalty;

            if (relation_after < -100) {
                p1->relations[p2_idx] = -100;
            } else {
                p1->relations[p2_idx] = (int8_t)(p1->relations[p2_idx] - (int8_t)penalty);
            }

            // Final clamp and sync
            if (p1->relations[p2_idx] < -100) {
                p1->relations[p2_idx] = -100;
            }
            p2->relations[p1_idx] = p1->relations[p2_idx];
        }
    }

    /**
     * @brief Checks if a proposed treaty between two players is valid given their current relations.
     * 
     * @param player_idx Index of the player initiating the proposal.
     * @param target_idx Index of the target player receiving the proposal.
     * @param treaty_type The type of treaty being proposed.
     * @return int8_t 1 if the proposal is valid, 0 otherwise.
     */
    int16_t Valid_Treaty_Proposal(int16_t player_idx, int16_t target_idx, int16_t treaty_type) {
        int8_t current_treaty = MOX::player[player_idx].treaty[target_idx];

        // If the proposed treaty is already in effect, it's not a valid proposal.
        if (current_treaty == (int8_t)treaty_type) {
            return 0;
        }

        switch (treaty_type) {
            case 1: /* Peace Treaty */
                // Cannot propose peace if already at peace or better (treaty >= 4) or if currently under a specific status (2).
                if (current_treaty >= 4 || current_treaty == 2) {
                    return 0;
                }
                break;

            case 2:
                // Specific transition check: must be in state 1 to propose state 2.
                if (current_treaty != 1) {
                    return 0;
                }
                break;

            case 3:
                // Can only propose state 3 if current status is 4 or higher.
                if (current_treaty < 4) {
                    return 0;
                }
                break;

            case 7: /* Trade Treaty */
                // Cannot propose trade if treaty status is too high (>= 4) or if a trade treaty is already active.
                if (current_treaty >= 4 || MOX::player[player_idx].trade_treaty[target_idx] == 1) {
                    return 0;
                }
                break;

            case 8: /* Research Treaty */
                // Cannot propose research if treaty status is too high (>= 4) or if a research treaty is already active.
                if (current_treaty >= 4 || MOX::player[player_idx].research_treaty[target_idx] == 1) {
                    return 0;
                }
                break;

            default:
                // For treaty types 4, 5, 6 or any unknown types, the proposal is considered valid.
                break;
        }

        return 1;
    }

    void __cdecl Modifier_Diplomacy_Adjustments() {
        if (MOX::NUM_PLAYERS == 0) {
            return;
        }

        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            for (int16_t j = 0; j < (int16_t)MOX::NUM_PLAYERS; ++j) {
                // If the treaty is not in a specific state (likely contact lost or eliminated)
                if (MOX::player[i].treaty[j] != 6) {
                    // Gradually improve treaty modifier if it's low or negative
                    if (MOX::player[i].treaty_modifier[j] < 0) {
                        MOX::player[i].treaty_modifier[j] += (int16_t)game_random::Random(5);
                    }
                    if (MOX::player[i].treaty_modifier[j] < 50) {
                        MOX::player[i].treaty_modifier[j] += (int16_t)game_random::Random(5);
                    }

                    // Gradually improve trade modifier if it's low or negative
                    if (MOX::player[i].trade_modifier[j] < 0) {
                        MOX::player[i].trade_modifier[j] += (int16_t)game_random::Random(5);
                    }
                    if (MOX::player[i].trade_modifier[j] < 50) {
                        MOX::player[i].trade_modifier[j] += (int16_t)game_random::Random(5);
                    }

                    // Gradually improve tech exchange modifier if it's low or negative
                    if (MOX::player[i].tech_exchange_modifier[j] < 0) {
                        MOX::player[i].tech_exchange_modifier[j] += (int16_t)game_random::Random(5);
                    }
                    if (MOX::player[i].tech_exchange_modifier[j] < 50) {
                        MOX::player[i].tech_exchange_modifier[j] += (int16_t)game_random::Random(5);
                    }

                    // Gradually improve peace modifier if it's low or negative
                    if (MOX::player[i].peace_modifier[j] < 0) {
                        MOX::player[i].peace_modifier[j] += (int16_t)game_random::Random(5);
                    }
                    if (MOX::player[i].peace_modifier[j] < 50) {
                        MOX::player[i].peace_modifier[j] += (int16_t)game_random::Random(5);
                    }

                    // Clamp peace modifier between -200 and 200
                    if (MOX::player[i].peace_modifier[j] < -200) {
                        MOX::player[i].peace_modifier[j] = -200;
                    }
                    if (MOX::player[i].peace_modifier[j] > 200) {
                        MOX::player[i].peace_modifier[j] = 200;
                    }
                }
            }
        }
    }

    void __cdecl Get_Break_Treaty_Message(int16_t victim_idx, int16_t actor_idx, int16_t* msg_id) {
        int16_t war_count = diplomac::Get_War_Count(actor_idx, victim_idx);
        int16_t fleet_comp = diplomac::Fleet_Comparison(actor_idx, victim_idx);

        int16_t random_score = (int16_t)game_random::Random(200);
        int16_t treaty_break_score = random_score + (war_count * -50) - fleet_comp;

        int16_t personality_idx = (uint8_t)MOX::player[actor_idx].personality;
        int16_t war_chance = diplodef::personality_break_treaty_war[personality_idx];
        int16_t war_roll = (int16_t)game_random::Random(100);

        if (war_roll <= war_chance && treaty_break_score > 100) {
            diplomac::Declare_War(victim_idx, actor_idx);
            *msg_id = 155 /* 0x9b */;
        }

        *msg_id = 154 /* 0x9a */;
        if (MOX::player[actor_idx].personality != PLAYER_PERSONALITY_HONORABLE) {
            *msg_id = 153 /* 0x99 */;
        }

        MOX::player[victim_idx].dishonored_flag[actor_idx] = 1;
        MOX::player[actor_idx].dishonored_flag[victim_idx] = 1;
    }

    void __cdecl Get_Break_Trade_Message(int16_t other_player_idx, int16_t this_player_idx, int16_t* out_message_id) {
        int16_t war_count;
        int32_t fleet_strength_diff;
        int32_t rand_val_200;
        int16_t personality_threshold;
        int32_t rand_val_100;
        int16_t break_score;

        war_count = Get_War_Count(this_player_idx, other_player_idx);
        fleet_strength_diff = Fleet_Comparison(this_player_idx, other_player_idx);
        rand_val_200 = game_random::Random(200);

        // Calculate break score using fleet strength, active wars and randomness
        break_score = (int16_t)(war_count * -50 - (int16_t)fleet_strength_diff + (int16_t)rand_val_200);

        personality_threshold = diplodef::personality_break_treaty_war[(uint8_t)MOX::player[this_player_idx].personality];
        rand_val_100 = game_random::Random(100);

        // If personality and fleet/war conditions are met, break trade/treaty
        if (rand_val_100 <= personality_threshold && break_score > 100) {
            int8_t treaty = MOX::player[this_player_idx].treaty[other_player_idx];
            if (treaty == 1 || treaty == 2) {
                Break_Treaties(other_player_idx, this_player_idx);

                // Note: The binary explicitly overwrites 0x9D with 0x9C immediately after this assignment.
                // This appears to be a logic bug in the original code where a message choice was intended.
                *out_message_id = 157 /* 0x9d */;
            }
        }

        *out_message_id = 156 /* 0x9c */;
    }

    void __cdecl Get_Break_Tribute_Treaty_Message(int16_t other_player_idx, int16_t acting_player_idx, int16_t* out_msg_id) {
        int16_t war_count;
        int16_t fleet_comp;
        int16_t random_200;
        int16_t score;
        int16_t personality_threshold;
        int16_t random_150;
        uint8_t personality;

        war_count = Get_War_Count(acting_player_idx, other_player_idx);
        fleet_comp = Fleet_Comparison(acting_player_idx, other_player_idx);
        random_200 = (int16_t)game_random::Random(200);

        // Score calculation: (war_count * -50) - fleet_comparison + random(200)
        score = (war_count * -50) - fleet_comp + random_200;

        personality = (uint8_t)MOX::player[acting_player_idx].personality;
        personality_threshold = diplodef::personality_break_treaty_war[personality];
        random_150 = (int16_t)game_random::Random(150);

        if (personality_threshold >= random_150 && score > 75) {
            Declare_War(other_player_idx, acting_player_idx);
            *out_msg_id = 173 /* 0xAD MSG_WAR_BREAK_TRIBUTE */;
        }

        // Note: The following logic checks the acting player's personality and sets the message ID,
        // effectively overwriting the value 173 (MSG_WAR_BREAK_TRIBUTE) if war was declared.
        if (MOX::player[acting_player_idx].personality == PLAYER_PERSONALITY_HONORABLE) {
            *out_msg_id = 172 /* 0xAC MSG_PERSONALITY_BREAK_TRIBUTE_AGGRESSIVE */;
        } else {
            *out_msg_id = 171 /* 0xAB MSG_PERSONALITY_BREAK_TRIBUTE_NORMAL */;
        }
    }

    /**
     * Determines whether an AI player should break research/trade treaties.
     * Note: This function always sets out_msg_id to 0x9E due to the flow
     * in the original compiled code, making the 0x9F branch effectively dead code.
     */
    void __cdecl Get_Break_Research_Message(int16_t other_player_idx, int16_t this_player_idx, int16_t* out_msg_id) {
        int16_t war_count = Get_War_Count(other_player_idx, this_player_idx);
        int16_t fleet_diff = Fleet_Comparison(other_player_idx, this_player_idx);
        int16_t rand_bonus = (int16_t)game_random::Random(200);

        // Calculate diplomatic score based on state and military comparison
        int16_t break_score = (int16_t)(war_count * -50 - fleet_diff + rand_bonus);

        uint8_t personality = (uint8_t)MOX::player[this_player_idx].personality;
        int16_t personality_limit = diplodef::personality_break_treaty_war[personality];
        int16_t personality_roll = (int16_t)game_random::Random(100);

        if (personality_limit >= personality_roll && break_score > 100) {
            int8_t treaty_type = MOX::player[this_player_idx].treaty[other_player_idx];

            // 1 = Trade, 2 = Research
            if (treaty_type == 1 || treaty_type == 2) {
                Break_Treaties(other_player_idx, this_player_idx);
                // This value is immediately overwritten by 0x9E below
                *out_msg_id = 159 /* 0x9f */;
            }
        }

        *out_msg_id = 158 /* 0x9e */;
    }

    void __cdecl Start_Trade_Treaty(int16_t player_idx, int16_t target_player_idx) {
        int16_t personality;
        int8_t bonus;

        personality = Get_Player_Diplomacy_Personality(player_idx, target_player_idx);
        bonus = (int8_t)personality_peace_duration[personality];
        MOX::player[player_idx].peace_duration[target_player_idx] += bonus;

        personality = Get_Player_Diplomacy_Personality(target_player_idx, player_idx);
        bonus = (int8_t)personality_peace_duration[personality];
        MOX::player[target_player_idx].peace_duration[player_idx] += bonus;

        TRADE::Start_Trade_Agreement(&MOX::player[player_idx], target_player_idx);
    }

    void __cdecl Start_Research_Treaty(int16_t player_idx, int16_t other_idx) {
        int16_t personality_a;
        int16_t personality_b;

        personality_a = Get_Player_Diplomacy_Personality(player_idx, other_idx);
        MOX::player[player_idx].peace_duration[other_idx] += (int8_t)personality_peace_duration[personality_a];

        personality_b = Get_Player_Diplomacy_Personality(other_idx, player_idx);
        MOX::player[other_idx].peace_duration[player_idx] += (int8_t)personality_peace_duration[personality_b];

        TRADE::Start_Research_Agreement(&MOX::player[player_idx], other_idx);
    }

    int16_t __cdecl Find_Worst_Modifier(int16_t player_idx, int16_t target_idx) {
        if (diplodef::COUNCIL_FLAG == 3) {
            return 0;
        }

        s_player* plr = &MOX::player[player_idx];
        int16_t worst = plr->treaty_modifier[target_idx];

        if (plr->trade_modifier[target_idx] < worst) {
            worst = plr->trade_modifier[target_idx];
        }

        if (plr->tech_exchange_modifier[target_idx] < worst) {
            worst = plr->tech_exchange_modifier[target_idx];
        }

        if (plr->peace_modifier[target_idx] < worst) {
            worst = plr->peace_modifier[target_idx];
        }

        return worst;
    }

    void __cdecl Get_Declare_War_Message(int16_t player_a_idx, int16_t player_b_idx, int16_t* msg_id) {
        // If the player declaring war (player_b_idx) has an honorable personality (4),
        // we check if a treaty is being broken dishonorably.
        if (MOX::player[player_b_idx].personality == PLAYER_PERSONALITY_HONORABLE) {
            if (MOX::player[player_a_idx].dishonored_flag[player_b_idx] == 0 &&
                (MOX::player[player_b_idx].treaty[player_a_idx] == 1 ||
                    MOX::player[player_b_idx].treaty[player_a_idx] == 2)) {
                *msg_id = 0x42; // Special message for breaking a treaty
                return;
            }
        }

        *msg_id = 0xa0; // Standard war declaration message
    }

    int __cdecl Fleet_Comparison(int16_t player_idx, int16_t opponent_idx) {
        int32_t strength_a = MOX::player[player_idx].total_ship_strength[opponent_idx];
        int32_t strength_b = MOX::player[opponent_idx].total_ship_strength[player_idx];

        int32_t numer = (strength_a * 4 + 4) * 25;
        int32_t ratio = numer / (strength_b + 1);

        if (ratio < 100) {
            ratio = -100 / (ratio + 1);
        } else {
            ratio++;
        }

        if (ratio > 300) {
            ratio = 300;
        } else if (ratio < -300) {
            ratio = -300;
        }

        return ratio;
    }

    void __cdecl Get_Gift_Response(int16_t plr_a, int16_t plr_b, int16_t gift_type, int16_t gift_data, int16_t* response_code) {
        int32_t gift_value = 0;

        MOX::player[plr_b].last_gift[plr_a] = (int8_t)gift_type;

        switch (gift_type) {
            case 1: /* BC gift */
            {
                *response_code = 163 /* STR_DIP_GIFT_RESPONSE_BC_163 */;
                int32_t maintenance = MOX::player[plr_a].total_maintenance;
                if (maintenance < 10) {
                    maintenance = 10;
                }
                gift_value = (gift_data / maintenance) / 10;
                if (gift_value > 8) {
                    gift_value = 8;
                }
            } break;

            case 2:
                gift_value = 2;
                *response_code = 162 /* STR_DIP_GIFT_RESPONSE_BC_162 */;
                break;

            case 3: /* Technology gift */
            {
                *response_code = 161 /* STR_DIP_GIFT_RESPONSE_TECH_161 */;
                uint32_t max_ai_group = 1;

                // Loop through known technologies to find highest AI group value
                for (int i = 0; i < 204; ++i) {
                    // If player A has learned this application
                    if (MOX::player[plr_a].tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                        int16_t field_id = TECHDATA::technology_applications[i].tech_field_id;
                        uint8_t ai_group = TECHDATA::technology_fields[field_id].ai_group;
                        if (ai_group > max_ai_group) {
                            max_ai_group = ai_group;
                        }
                    }
                }

                int16_t target_field_id = TECHDATA::technology_applications[gift_data].tech_field_id;
                uint8_t target_ai_group = TECHDATA::technology_fields[target_field_id].ai_group;
                gift_value = (target_ai_group * 5) / max_ai_group;
            } break;

            case 4: /* Star system gift */
            {
                *response_code = 164 /* STR_DIP_GIFT_RESPONSE_SYSTEM_164 */;
                int16_t population = COLCALC::Get_System_Population(plr_a, gift_data);
                gift_value = (population / 2) + 1;
            } break;

            default:
                break;
        }

        if (gift_value != 0) {
            // Soften previous rejection severity based on gift value
            MOX::player[plr_a].last_diplomacy_proposal_turn[plr_b] /= (int16_t)gift_value;
            MOX::player[plr_b].last_diplomacy_proposal_turn[plr_a] /= (int16_t)gift_value;
        }

        // Update relations
        int32_t relation_bonus = (game_random::Random(4) + 4) * gift_value;
        int32_t current_relations = MOX::player[plr_a].relations[plr_b];

        if (current_relations + relation_bonus > 100) {
            MOX::player[plr_a].relations[plr_b] = 100;
        } else {
            MOX::player[plr_a].relations[plr_b] = (int8_t)(current_relations + relation_bonus);
        }

        // Safety cap for relations
        if (MOX::player[plr_a].relations[plr_b] > 100) {
            MOX::player[plr_a].relations[plr_b] = 100;
        }

        // Update base relations
        int32_t current_base = MOX::player[plr_a].base_relations[plr_b];
        if (current_base + gift_value > 100) {
            MOX::player[plr_a].base_relations[plr_b] = 100;
        } else {
            MOX::player[plr_a].base_relations[plr_b] = (int8_t)(current_base + gift_value);
        }

        // Safety cap for base relations
        if (MOX::player[plr_a].base_relations[plr_b] > 100) {
            MOX::player[plr_a].base_relations[plr_b] = 100;
        }

        // Sync mirrored relations
        MOX::player[plr_b].base_relations[plr_a] = MOX::player[plr_a].base_relations[plr_b];
        MOX::player[plr_b].relations[plr_a] = MOX::player[plr_a].relations[plr_b];
    }

    void __cdecl Get_Demand_Response(int16_t player_a, int16_t player_b, int16_t demand_type, int16_t demand_value, int16_t* accepted, int16_t* response_code) {
        int32_t tech_comp = Tech_Comparison(player_a, player_b);
        int32_t fleet_comp = Fleet_Comparison(player_a, player_b);
        int32_t pop_comp = Population_Comparison(player_a, player_b);
        int16_t worst_mod = Find_Worst_Modifier(player_a, player_b);
        int32_t rnd = game_random::Random(200);

        int16_t pers_mod;
        if (MOX::player[player_b].personality != PLAYER_PERSONALITY_HONORABLE || MOX::player[player_a].dishonored_flag[player_b] != 1) {
            pers_mod = diplodef::personality_relation_modifiers[MOX::player[player_b].personality];
        } else {
            pers_mod = diplodef::personality_relation_modifiers[PLAYER_PERSONALITY_DISHONORED];
        }

        int16_t modifier = (int16_t)(((tech_comp + fleet_comp + pop_comp) / 3) + worst_mod + rnd + (pers_mod * 2));

        switch (demand_type) {
            case 0: {
                if (MOX::player[player_b].treaty[demand_value] == 1) {
                    modifier -= 75;
                } else if (MOX::player[player_b].treaty[demand_value] == 2) {
                    modifier -= 150;
                } else {
                    modifier -= 25;
                }

                int8_t rel = MOX::player[player_b].relations[demand_value];
                modifier -= rel / 2;

                int32_t tgt_fleet = Fleet_Comparison(player_b, demand_value);
                modifier -= tgt_fleet / 4;

                if (MOX::player[demand_value].objectives != PLAYER_OBJECTIVE_HUMAN) {
                    modifier -= 25;
                }
                break;
            }
            case 1: {
                modifier -= (diplodef::personality_relation_modifiers[MOX::player[demand_value].personality] + 80);
                if (MOX::player[demand_value].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    modifier = 0;
                }
                break;
            }
            case 2: {
                modifier -= MOX::player[player_b].relations[demand_value] * 2;
                break;
            }
            case 3:
            case 5:
            case 8: {
                modifier -= 50;
                break;
            }
            case 4: {
                modifier -= 150;
                break;
            }
            case 6: {
                uint32_t tech_val = newtech::Calc_Tech_Value(player_a, demand_value);
                int32_t max_tech_val = aitech::Get_Max_Technology_Value(player_a);
                modifier -= (int16_t)((tech_val * 100) / max_tech_val);
                break;
            }
            case 7: {
                int16_t sys_pop = COLCALC::Get_System_Population(player_b, demand_value);
                int32_t val = (sys_pop * 1000) / MOX::player[player_b].total_pop;
                if (val > 300) {
                    val = 300;
                }
                modifier -= (int16_t)val;
                break;
            }
        }

        Adjust_Diplomat_Modifiers(player_a, player_b);
        Adjust_Diplomat_Modifiers(player_a, player_b);
        Adjust_Diplomat_Modifiers(player_a, player_b);
        Adjust_Diplomat_Modifiers(player_a, player_b);
        Adjust_Diplomat_Modifiers(player_a, player_b);

        int8_t treaty = MOX::player[player_a].treaty[player_b];
        if (treaty == 1 || treaty == 2) {
            if (modifier < -100 || Get_Player_Diplomacy_Personality(player_b, player_a) == PLAYER_PERSONALITY_DISHONORED) {
                *response_code = 0xA5;
                Declare_War(player_a, player_b);
                *accepted = 0;
            } else if (modifier < -50) {
                MOX::player[player_a].threats[player_b]++;
                MOX::player[player_b].threats[player_a]++;
                *response_code = 0xA6;
                Break_Treaties(player_a, player_b);
                Break_Research(player_a, player_b);
                Break_Trade(player_a, player_b);
                *accepted = 0;
            } else if (modifier < -25) {
                MOX::player[player_a].threats[player_b]++;
                MOX::player[player_b].threats[player_a]++;

                int32_t base_rel = MOX::player[player_a].base_relations[player_b] - 10;
                if (base_rel < -100) {
                    base_rel = -100;
                }
                MOX::player[player_a].base_relations[player_b] = (int8_t)base_rel;
                MOX::player[player_b].base_relations[player_a] = (int8_t)base_rel;

                *response_code = 0xA7;
                *accepted = 0;
            } else if (modifier < 0) {
                *response_code = 0xA8;
                *accepted = 0;
            } else {
                *response_code = 0xA9;
                *accepted = 1;
            }
        } else {
            if (modifier < -150 || Get_Player_Diplomacy_Personality(player_b, player_a) == PLAYER_PERSONALITY_DISHONORED) {
                *response_code = 0xA5;
                Declare_War(player_a, player_b);
                *accepted = 0;
            } else if (modifier < -75) {
                MOX::player[player_a].threats[player_b]++;
                MOX::player[player_b].threats[player_a]++;

                int32_t base_rel = MOX::player[player_a].base_relations[player_b] - 10;
                if (base_rel < -100) {
                    base_rel = -100;
                }
                MOX::player[player_a].base_relations[player_b] = (int8_t)base_rel;
                MOX::player[player_b].base_relations[player_a] = (int8_t)base_rel;

                *response_code = 0xA7;
                *accepted = 0;
            } else if (modifier < 0) {
                *response_code = 0xA8;
                *accepted = 0;
            } else if (modifier > 0) {
                *response_code = 0xA9;
                *accepted = 1;
            }
        }

        if (*accepted == 1 && demand_type == 5) {
            int32_t rnd1 = game_random::Random(50);
            int32_t rnd2 = game_random::Random(50);
            MOX::player[player_b].stop_spying_duration[demand_value] = (int8_t)(rnd1 + rnd2 + 10);
        }
    }

    int16_t __cdecl Get_Demands(int16_t player_idx, int16_t target_idx, int16_t severity, int16_t demands_tribute, int16_t demands_tech, int16_t demands_gold, int16_t demands_system) {
        diplomacy_proposal_gold_bribe = 0;
        diplomacy_proposal_tribute_bribe = 0;
        diplomacy_proposal_tech_bribe1 = 0;
        diplomacy_proposal_system_bribe = -1;

        if (severity > 10) {
            severity = 10;
        }

        uint8_t diff_tech_list[TECH_APP_COUNT];
        int32_t diff_tech_categories[TECH_APP_COUNT];
        int16_t num_techs = 0;
        int16_t tech_severity_limit = 0;

        if (demands_tech == 1) {
            npcdiplo::Get_Differential_Tech_List(player_idx, target_idx, 0, 0xc343cb00, diff_tech_list, diff_tech_categories, &num_techs, 1);

            uint32_t target_max_ai_group = 1;
            uint32_t player_max_ai_group = 1;

            for (int i = 0; i < 204; ++i) {
                if (MOX::player[player_idx].tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int16_t tech_field_id = TECHDATA::technology_applications[i].tech_field_id;
                    uint8_t ai_group = TECHDATA::technology_fields[tech_field_id].ai_group;
                    if (target_max_ai_group < ai_group) {
                        target_max_ai_group = ai_group;
                    }
                }
                if (MOX::player[target_idx].tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int16_t tech_field_id = TECHDATA::technology_applications[i].tech_field_id;
                    uint8_t ai_group = TECHDATA::technology_fields[tech_field_id].ai_group;
                    if (player_max_ai_group < ai_group) {
                        player_max_ai_group = ai_group;
                    }
                }
            }

            if (player_max_ai_group < target_max_ai_group) {
                tech_severity_limit = (int16_t)((player_max_ai_group * 10) / target_max_ai_group);
            } else {
                tech_severity_limit = 10;
            }

            if (tech_severity_limit < 1) {
                tech_severity_limit = 1;
            }
        }

        int16_t max_gold_severity = 0;
        if (demands_gold == 1) {
            int32_t total_maint = MOX::player[player_idx].total_maintenance;
            if (total_maint < 10) {
                total_maint = 10;
            }

            int32_t target_bc = MOX::player[target_idx].bc;
            if (target_bc == 0) {
                max_gold_severity = 0;
            } else {
                max_gold_severity = (int16_t)((target_bc / total_maint) / 10);
            }

            if (max_gold_severity > 10) {
                max_gold_severity = 10;
            }
        } else {
            max_gold_severity = (int16_t)diff_tech_categories[0];
        }

        if (severity == 0) {
            return 0;
        }

        if (severity > 6) {
            demands_tribute = 0;
        }

        if (severity > tech_severity_limit) {
            demands_tech = 0;
        }

        if (num_techs == 0) {
            demands_tech = 0;
        }

        if (severity > max_gold_severity) {
            demands_gold = 0;
        }

        int16_t actual_demand_system = 0;
        if (severity > 5) {
            actual_demand_system = demands_system;
        }

        if (actual_demand_system + demands_tribute + demands_gold + demands_tech == 0) {
            return 0;
        }

        int chosen_demand_type = 0;

        if (actual_demand_system == 1 && (game_random::Random(3) == 1 || (demands_gold + demands_tribute + demands_tech == 0))) {
            chosen_demand_type = 3;
        } else if (demands_tech == 1 && (game_random::Random(3) == 1 || (demands_gold + demands_tribute == 0))) {
            chosen_demand_type = 2;
        } else if (demands_tribute == 1 && (game_random::Random(2) == 1 || demands_gold == 0)) {
            chosen_demand_type = 4;
        } else if (demands_gold != 0) {
            chosen_demand_type = 1;
        }

        if (chosen_demand_type == 4) {
            if (severity < 3) {
                diplomacy_proposal_tribute_bribe = 1;
            } else {
                diplomacy_proposal_tribute_bribe = 2;
            }
            return 1;
        }

        if (chosen_demand_type == 1) {
            int32_t total_maint = MOX::player[player_idx].total_maintenance;
            if (total_maint < 10) {
                total_maint = 10;
            }

            int32_t computed_gold = severity * total_maint * 10;
            int32_t target_bc = MOX::player[target_idx].bc;

            int32_t final_gold = computed_gold;
            if (final_gold > target_bc) {
                final_gold = target_bc;
            }

            if (final_gold >= 100) {
                final_gold = (final_gold / 100) * 100;
            } else {
                final_gold = (final_gold / 10) * 10;
            }

            if (final_gold == 0) {
                return 0;
            }

            if (final_gold > 32000) {
                final_gold = 32000;
            }

            diplomacy_proposal_gold_bribe = (int16_t)final_gold;
            return 1;
        }

        if (chosen_demand_type == 3) {
            int16_t system_idx = COLCALC::Get_Surrender_System(target_idx, player_idx, severity > 6 ? 1 : 0);
            if (system_idx != -1) {
                if (COLCALC::May_Surrender_Star(system_idx, target_idx, player_idx)) {
                    diplomacy_proposal_system_bribe = system_idx;
                    return 1;
                }
            }
        }

        if (chosen_demand_type == 2) {
            int tech_idx = num_techs - (severity * num_techs) / tech_severity_limit;
            tech_idx = tech_idx - game_random::Random(3) + 1;

            if (tech_idx < 0) {
                tech_idx = 0;
            }

            if (tech_idx >= num_techs) {
                tech_idx = num_techs - 1;
            }

            diplomacy_proposal_tech_bribe1 = diff_tech_list[tech_idx];
            return 1;
        }

        return 0;
    }

    int16_t __cdecl Check_Treaty_Proposal(int16_t player_idx, int16_t target_idx, int16_t proposal_type, int16_t* response, int16_t* demand_type, int16_t* demand_tech, int16_t* demand_gold, int16_t* demand_system, int16_t* demand_tribute) {
        int16_t result = 0;
        int16_t demand_category = 0;

        *demand_type = 0;

        switch (proposal_type) {
            case 1: {
                result = Diplomacy_Test(player_idx, target_idx, 0x4B, 0);
                demand_category = 4;
                switch (result) {
                    case 0:
                        if (game_random::Random(4) == 1 && MOX::player[player_idx].last_broken_treaty[target_idx] != '\0') {
                            *response = 0x8B;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].personality == PLAYER_PERSONALITY_HONORABLE && MOX::player[player_idx].dishonored_flag[target_idx] != '\0') {
                            *response = 0x8C;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].trust_worthiness[target_idx] != '\0') {
                            *response = 0x8D;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] != 0) {
                            *response = (uint8_t)MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] + 0x8D;
                        } else {
                            *response = 0x8A;
                        }
                        break;
                    case 2:
                        demand_category = 8;
                        // fallthrough
                    case 1:
                        if (game_random::Random(2) == 1 && Get_Demands(player_idx, target_idx, demand_category, 0, 1, 1, 0) == 1) {
                            if (diplomacy_proposal_tribute_bribe != 0) {
                                *demand_type = 3;
                                *demand_tribute = diplomacy_proposal_tribute_bribe;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                            if (diplomacy_proposal_gold_bribe != 0) {
                                *demand_type = 1;
                                *demand_gold = diplomacy_proposal_gold_bribe;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                            if (diplomacy_proposal_system_bribe != -1) {
                                *demand_type = 2;
                                *demand_system = diplomacy_proposal_system_bribe;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                            if (diplomacy_proposal_tech_bribe1 != 0) {
                                *demand_type = 4;
                                *demand_tech = diplomacy_proposal_tech_bribe1;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                        } else {
                            result = 0;
                            *response = 0x8A;
                        }
                        break;
                    case 3:
                        if (game_random::Random(3) == 1 && MOX::player[player_idx].treaty[target_idx] == 2) {
                            *response = 0x88;
                        } else if (game_random::Random(4) == 1 && MOX::player[target_idx].last_gift[player_idx] != '\0') {
                            *response = 0x89;
                        } else {
                            *response = 0x83;
                        }
                        break;
                }
                break;
            }
            case 2: {
                result = Diplomacy_Test(player_idx, target_idx, MOX::player[player_idx].broken_treaties_modifier[target_idx] + 0x7D, 0);
                if (MOX::player[player_idx].relations[target_idx] < 50) {
                    result = 0;
                }
                demand_category = 4;
                switch (result) {
                    case 0:
                        if (game_random::Random(4) == 1 && MOX::player[player_idx].last_broken_treaty[target_idx] != '\0') {
                            *response = 0x8B;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].personality == PLAYER_PERSONALITY_HONORABLE && MOX::player[player_idx].dishonored_flag[target_idx] != '\0') {
                            *response = 0x8C;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].trust_worthiness[target_idx] != '\0') {
                            *response = 0x8D;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] != 0) {
                            *response = (uint8_t)MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] + 0x8D;
                        } else {
                            *response = 0x8A;
                        }
                        break;
                    case 2:
                        demand_category = 8;
                        // fallthrough
                    case 1:
                        if (game_random::Random(4) == 1 && Get_Demands(player_idx, target_idx, demand_category, 0, 1, 1, 0) == 1) {
                            if (diplomacy_proposal_tribute_bribe != 0) {
                                *demand_type = 3;
                                *demand_tribute = diplomacy_proposal_tribute_bribe;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                            if (diplomacy_proposal_gold_bribe != 0) {
                                *demand_type = 1;
                                *demand_gold = diplomacy_proposal_gold_bribe;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                            if (diplomacy_proposal_system_bribe != -1) {
                                *demand_type = 2;
                                *demand_system = diplomacy_proposal_system_bribe;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                            if (diplomacy_proposal_tech_bribe1 != 0) {
                                *demand_type = 4;
                                *demand_tech = diplomacy_proposal_tech_bribe1;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                        } else {
                            result = 0;
                            *response = 0x8A;
                        }
                        break;
                    case 3:
                        if (game_random::Random(4) == 1 && MOX::player[target_idx].last_gift[player_idx] != '\0') {
                            *response = 0x89;
                        } else {
                            *response = 0x84;
                        }
                        break;
                }
                break;
            }
            case 3: {
                int attacks_out = AIMOVE::Number_Of_Attacks_Underway(player_idx, target_idx);
                int attacks_in = AIMOVE::Number_Of_Attacks_Underway(target_idx, player_idx);
                int16_t war_count_out = Get_War_Count(player_idx, target_idx);
                int16_t war_count_in = Get_War_Count(target_idx, player_idx);
                int fleet_cmp = Fleet_Strength_Comparison(player_idx, target_idx);
                int16_t war_loss_cmp = Total_War_Loss_Comparison(player_idx, target_idx);

                int16_t test_val = (9 - (int16_t)(uint8_t)MOX::player[player_idx].time_since_last_attack[target_idx]) +
                    ((((attacks_out * 2 + 2 - attacks_in) * 2 - war_count_out) + war_count_in) * 25 +
                        ((fleet_cmp - 100) / 2) - war_loss_cmp) -
                    MOX::player[player_idx].broken_treaties_modifier[target_idx] -
                    MOX::player[player_idx].trust_worthiness[target_idx] +
                    MOX::player[player_idx].relations[target_idx];

                result = Diplomacy_Test(
                    player_idx, target_idx, test_val, alternate_diplomacy != 0 ? 3 : 0);

                if ((uint8_t)MOX::player[player_idx].time_since_last_attack[target_idx] < 10) {
                    result = 0;
                }

                demand_category = 4;
                switch (result) {
                    case 0:
                        if (game_random::Random(4) == 1 && MOX::player[player_idx].last_broken_treaty[target_idx] != '\0') {
                            *response = 0x8B;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].personality == PLAYER_PERSONALITY_HONORABLE && MOX::player[player_idx].dishonored_flag[target_idx] != '\0') {
                            *response = 0x8C;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].trust_worthiness[target_idx] != '\0') {
                            *response = 0x8D;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] != 0) {
                            *response = (uint8_t)MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] + 0x8D;
                        } else {
                            *response = 0x8A;
                        }
                        break;
                    case 2:
                        demand_category = 8;
                        // fallthrough
                    case 1:
                        if (Get_Demands(player_idx, target_idx, demand_category, 1, 1, 1, 1) == 1) {
                            if (diplomacy_proposal_tribute_bribe != 0) {
                                *demand_type = 3;
                                *demand_tribute = diplomacy_proposal_tribute_bribe;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                            if (diplomacy_proposal_gold_bribe != 0) {
                                *demand_type = 1;
                                *demand_gold = diplomacy_proposal_gold_bribe;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                            if (diplomacy_proposal_system_bribe != -1) {
                                *demand_type = 2;
                                *demand_system = diplomacy_proposal_system_bribe;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                            if (diplomacy_proposal_tech_bribe1 != 0) {
                                *demand_type = 4;
                                *demand_tech = diplomacy_proposal_tech_bribe1;
                                *response = 0xAA;
                                Set_Demands(player_idx, target_idx);
                            }
                        } else {
                            result = 0;
                            *response = 0x8A;
                        }
                        break;
                    case 3:
                        if (game_random::Random(4) == 1 && MOX::player[target_idx].last_gift[player_idx] != '\0') {
                            *response = 0x89;
                        } else {
                            *response = 0x87;
                        }
                        break;
                }
                break;
            }
            case 7: {
                result = Diplomacy_Test(
                    player_idx, target_idx, 0x3C, alternate_diplomacy != 0 ? 1 : 0);
                switch (result) {
                    case 0:
                        if (game_random::Random(4) == 1 && MOX::player[player_idx].last_broken_treaty[target_idx] != '\0') {
                            *response = 0x8B;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].personality == PLAYER_PERSONALITY_HONORABLE && MOX::player[player_idx].dishonored_flag[target_idx] != '\0') {
                            *response = 0x8C;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].trust_worthiness[target_idx] != '\0') {
                            *response = 0x8D;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] != 0) {
                            *response = (uint8_t)MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] + 0x8D;
                        } else {
                            *response = 0x8A;
                        }
                        break;
                    case 1:
                    case 2:
                        result = 0;
                        break;
                    case 3:
                        if (game_random::Random(3) == 1 && MOX::player[player_idx].treaty[target_idx] == 2) {
                            *response = 0x88;
                        } else if (game_random::Random(4) == 1 && MOX::player[target_idx].last_gift[player_idx] != '\0') {
                            *response = 0x89;
                        } else {
                            *response = 0x85;
                        }
                        break;
                }
                break;
            }
            case 8: {
                result = Diplomacy_Test(
                    player_idx, target_idx, 0x28, alternate_diplomacy != 0 ? 1 : 0);
                switch (result) {
                    case 0:
                        if (game_random::Random(4) == 1 && MOX::player[player_idx].last_broken_treaty[target_idx] != '\0') {
                            *response = 0x8B;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].personality == PLAYER_PERSONALITY_HONORABLE && MOX::player[player_idx].dishonored_flag[target_idx] != '\0') {
                            *response = 0x8C;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].trust_worthiness[target_idx] != '\0') {
                            *response = 0x8D;
                        } else if (game_random::Random(4) == 1 && MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] != 0) {
                            *response = (uint8_t)MOX::player[player_idx].last_bad_diplomatic_incident[target_idx] + 0x8D;
                        } else {
                            *response = 0x8A;
                        }
                        break;
                    case 1:
                    case 2:
                        result = 0;
                        *response = 0x8A;
                        break;
                    case 3:
                        if (game_random::Random(3) == 1 && MOX::player[player_idx].treaty[target_idx] == 2) {
                            *response = 0x88;
                        } else if (game_random::Random(4) == 1 && MOX::player[target_idx].last_gift[player_idx] != '\0') {
                            *response = 0x89;
                        } else {
                            *response = 0x86;
                        }
                        break;
                }
                break;
            }
        }

        return result;
    }

    int32_t __cdecl Tech_Comparison(int16_t player_1, int16_t player_2) {
        int16_t initial_tech = (int16_t)aitech::Get_Max_Technology_Value(player_1);
        if (initial_tech == 0) {
            return 300;
        }

        int32_t tech_2 = aitech::Get_Max_Technology_Value(player_2);
        int32_t tech_1 = aitech::Get_Max_Technology_Value(player_1);
        int32_t diff = tech_2 - tech_1;

        int32_t tech_1_divisor = aitech::Get_Max_Technology_Value(player_1);
        int32_t result = (diff * 100) / tech_1_divisor;

        if (result > 300) {
            result = 300;
        }

        if (result < -300) {
            result = -300;
        }

        return result;
    }

    int32_t __cdecl Population_Comparison(int16_t player_idx1, int16_t player_idx2) {
        int32_t pop1 = MOX::player[player_idx1].total_pop;
        if (pop1 == 0) {
            return 300;
        }

        int32_t pop2 = MOX::player[player_idx2].total_pop;
        int32_t diff_percent = ((pop1 - pop2) * 100) / pop1;

        if (diff_percent > 300) {
            diff_percent = 300;
        }

        if (diff_percent < -300) {
            diff_percent = -300;
        }

        if (diff_percent == 0) {
            return 0;
        }

        if (diff_percent < 0) {
            return -(int32_t)game_random::Random((uint32_t)(-diff_percent));
        }

        return (int32_t)game_random::Random((uint32_t)diff_percent);
    }

    int16_t __cdecl Diplomacy_Test(int16_t evaluator_idx, int16_t proposer_idx, int16_t difficulty_mod, int16_t treaty_type) {
        int16_t score = (int16_t)MOX::player[evaluator_idx].broken_treaties_modifier[proposer_idx] +
            (int16_t)MOX::player[evaluator_idx].relations[proposer_idx] +
            (int16_t)MOX::player[evaluator_idx].trust_worthiness[proposer_idx];

        int16_t modifier;
        if (MOX::player[evaluator_idx].personality == PLAYER_PERSONALITY_HONORABLE) {
            score += (int16_t)MOX::player[evaluator_idx].trust_worthiness[proposer_idx];
            if (MOX::player[evaluator_idx].dishonored_flag[proposer_idx] == 1) {
                modifier = diplodef::personality_relation_modifiers[PLAYER_PERSONALITY_DISHONORED];
            } else {
                modifier = diplodef::personality_relation_modifiers[MOX::player[evaluator_idx].personality];
            }
        } else {
            modifier = diplodef::personality_relation_modifiers[MOX::player[evaluator_idx].personality];
        }

        score -= (difficulty_mod - modifier);
        score += (int16_t)game_random::Random(100);

        switch (treaty_type) {
            case 0:
                score += MOX::player[evaluator_idx].treaty_modifier[proposer_idx];
                break;
            case 1:
                score += MOX::player[evaluator_idx].trade_modifier[proposer_idx];
                break;
            case 2:
                score += MOX::player[evaluator_idx].tech_exchange_modifier[proposer_idx];
                break;
            case 3:
                score += MOX::player[evaluator_idx].peace_modifier[proposer_idx];
                break;
            default:
                break;
        }

        Adjust_Diplomat_Modifiers(evaluator_idx, proposer_idx);

        switch (treaty_type) {
            case 0:
                MOX::player[evaluator_idx].treaty_modifier[proposer_idx] -= (int16_t)(game_random::Random(30) + 20);
                break;
            case 1:
                MOX::player[evaluator_idx].trade_modifier[proposer_idx] -= (int16_t)(game_random::Random(30) + 20);
                break;
            case 2:
                MOX::player[evaluator_idx].tech_exchange_modifier[proposer_idx] -= (int16_t)(game_random::Random(50) + 20);
                break;
            case 3:
                MOX::player[evaluator_idx].peace_modifier[proposer_idx] -= (int16_t)(game_random::Random(50) + 50);
                break;
            default:
                break;
        }

        if (MOX::player[proposer_idx].traits[TRAIT_CHARISMATIC] != 0) {
            score += 50;
        } else if (MOX::player[proposer_idx].traits[TRAIT_REPULSIVE] != 0) {
            score -= 50;
        }

        if (MOX::player[proposer_idx].traits[TRAIT_TELEPATHIC] != 0) {
            score += 25;
        }

        if (MOX::player[proposer_idx].tech_applications[TECH_APP_XENO_PSYCHOLOGY] == TECH_RESEARCH_STATUS_RESEARCHED) {
            score += 30;
        }

        score += COLCALC::Best_Leader_Diplomat_Bonus(proposer_idx);

        if (score < -75) {
            return 0;
        }
        if (score < -50) {
            return 2;
        }
        if (score < 0) {
            return 1;
        }
        return 3;
    }
} // namespace diplomac