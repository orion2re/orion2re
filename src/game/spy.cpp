#include "pch.h"

namespace spy {
    int16_t spy_bonuses[MAX_PLAYERS * 2];


    void __cdecl Resolve_Spies(const AIDATA::s_player_assessment* players) {
        uint8_t num_active_players = 0;
        int8_t active_player_indices[MAX_PLAYERS];

        Resolve_Assassins();

        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            // player structure size: 0xEA9 (3753 bytes)
            if (MOX::player[i].eliminated == 0) {
                active_player_indices[num_active_players] = (int8_t)i;
                num_active_players++;

                Compute_Spy_Bonuses(
                    &MOX::player[i],
                    &spy_bonuses[i * 2],
                    &spy_bonuses[i * 2 + 1]);
            }
        }

        RUSS::Shuffle_Char(active_player_indices, (int16_t)num_active_players);

        if (num_active_players != 0) {
            for (int16_t j = 0; j < (int16_t)num_active_players; ++j) {
                int8_t player_idx = active_player_indices[j];
                if (MOX::player[player_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                    Allocate_AI_Spies(&MOX::player[player_idx], players[player_idx].empire.contact_count);
                }
            }
        }

        RUSS::Shuffle_Char(active_player_indices, (int16_t)num_active_players);

        if (num_active_players != 0) {
            for (int16_t j = 0; j < (int16_t)num_active_players; ++j) {
                int8_t player_idx = active_player_indices[j];
                Resolve_Player_Spies(&MOX::player[player_idx]);
            }
        }
    }


    void __cdecl Allocate_AI_Spies(s_player* player, int16_t contact_count) {
        if (player->eliminated != 0) {
            return;
        }

        if (MOX::settings.game_difficulty < GAME_DIFFICULTY_EASY) {
            return;
        }

        int16_t player_idx = (int16_t)(player - MOX::player);

        int32_t sum_spies_100 = 0;
        int32_t sum_spies_100_bonused = 0;
        int32_t max_spy_val_100 = 0;

        int32_t sum_spies_other = 0;
        int32_t sum_spies_other_bonused = 0;
        int32_t max_spy_val_other = 0;

        int32_t total_spies = 0;

        if (MOX::NUM_PLAYERS > 0) {
            for (int16_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                int16_t their_spies_here = BILL::Get_Their_Spy_Number(i, player_idx);
                int16_t our_spies_there = BILL::Get_Their_Spy_Number(player_idx, i);

                total_spies += our_spies_there;
                BILL::Set_Their_Spy_Number(player_idx, i, 0);

                if (their_spies_here > 0 && i != player_idx) {
                    int32_t val = their_spies_here;
                    if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        sum_spies_100 += val;
                        sum_spies_100_bonused += val * spy_bonuses[i * 2];
                        if (max_spy_val_100 < val) {
                            max_spy_val_100 = val;
                        }
                    } else {
                        sum_spies_other += val;
                        sum_spies_other_bonused += val * spy_bonuses[i * 2];
                        if (max_spy_val_other < val) {
                            max_spy_val_other = val;
                        }
                    }
                }
            }
        }

        if (total_spies == 0) {
            if (contact_count > 0 && MOX::stardate > 35030) {
                if (MOX::settings.game_difficulty == GAME_DIFFICULTY_IMPOSSIBLE) {
                    if (player->objectives != PLAYER_OBJECTIVE_DIPLOMATIC) {
                        game_random::Random(8);
                    }
                    if (player->traits[TRAIT_SPYING_BONUS] > 0) {
                        game_random::Random(5);
                    }
                    total_spies = 1;
                } else if (MOX::settings.game_difficulty == GAME_DIFFICULTY_HARD) {
                    if (player->objectives == PLAYER_OBJECTIVE_DIPLOMATIC && game_random::Random(7) == 1) {
                        total_spies = 1;
                    } else if (game_random::Random(25) == 1) {
                        total_spies = 1;
                    } else if (player->traits[TRAIT_SPYING_BONUS] > 0 && game_random::Random(6) == 1) {
                        total_spies = 1;
                    }
                } else if (MOX::settings.game_difficulty == GAME_DIFFICULTY_AVERAGE) {
                    if (player->objectives == PLAYER_OBJECTIVE_DIPLOMATIC && game_random::Random(13) == 1) {
                        total_spies = 1;
                    } else if (player->traits[TRAIT_SPYING_BONUS] > 0 && game_random::Random(13) == 1) {
                        total_spies = 1;
                    }
                }
            }
        }

        if (total_spies <= 0) {
            return;
        }

        int32_t allocated_home = 0;
        if (spy_bonuses[player_idx * 2] >= 0) {
            if (MOX::settings.game_difficulty > GAME_DIFFICULTY_AVERAGE) {
                allocated_home = max_spy_val_100;
            } else {
                allocated_home = max_spy_val_100;
                if (max_spy_val_100 <= max_spy_val_other) {
                    allocated_home = max_spy_val_other;
                }
            }
            if (allocated_home > total_spies) {
                allocated_home = total_spies;
            }
        } else {
            allocated_home = total_spies;
        }

        if (allocated_home > 0x3f) {
            allocated_home = 0x3f;
        }

        BILL::Set_Their_Spy_Number(player_idx, player_idx, (uint8_t)allocated_home);

        if (allocated_home >= total_spies) {
            return;
        }

        int32_t remaining_spies = total_spies - allocated_home;
        uint32_t target_weights[MAX_PLAYERS] = {0};
        uint32_t max_weight = 0;

        if (MOX::NUM_PLAYERS > 0) {
            for (int16_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                if (i == player_idx) {
                    continue;
                }

                if (MOX::player[player_idx].contact[i] != 0 && MOX::player[player_idx].stop_spying_duration[i] == 0) {
                    int16_t target_home_spies = BILL::Get_Their_Spy_Number(i, i);
                    uint32_t total_cost = 0;

                    for (int16_t j = 0; j < 204; j++) {
                        if (MOX::player[i].tech_applications[j] == TECH_RESEARCH_STATUS_RESEARCHED) {
                            if (COLCALC::Player_May_Get_Tech_App(player, j)) {
                                int16_t field_id = TECHDATA::technology_applications[j].tech_field_id;
                                if (field_id != 0) {
                                    total_cost += TECHDATA::technology_fields[field_id].cost;
                                }
                            }
                        }
                    }

                    if (total_cost != 0) {
                        if (target_home_spies > 0) {
                            int16_t foreign_spies = 0;
                            for (int16_t k = 0; k < MOX::NUM_PLAYERS; k++) {
                                if (k != i) {
                                    foreign_spies += BILL::Get_Their_Spy_Number(k, i);
                                }
                            }

                            if (foreign_spies > 0) {
                                uint32_t u_sum = (uint32_t)(foreign_spies + remaining_spies);
                                total_cost = (u_sum * total_cost) / (uint32_t)remaining_spies;
                            }

                            int32_t their_spy_bonus = spy_bonuses[i * 2 + 1];
                            uint32_t mult1 = 100 - their_spy_bonus;
                            total_cost = (total_cost * mult1) / 100;

                            if (their_spy_bonus >= 25) {
                                uint32_t mult2 = 125 - their_spy_bonus;
                                total_cost = (total_cost * mult2) / 100;
                            }

                            total_cost /= (uint32_t)target_home_spies;
                        }

                        if (remaining_spies > target_home_spies) {
                            uint32_t diff = (uint32_t)(remaining_spies - target_home_spies);
                            total_cost *= diff;
                        }

                        total_cost++;

                        target_weights[i] = total_cost;
                        if (total_cost > max_weight) {
                            max_weight = total_cost;
                        }
                    }
                }
            }
        }

        while (remaining_spies > 0) {
            int16_t target = RUSS::Get_Weighted_Choice_Int(target_weights, MOX::NUM_PLAYERS);

            if (target == -1) {
                int32_t sum = remaining_spies + allocated_home;
                if (sum >= 0x3f) {
                    BILL::Set_Their_Spy_Number(player_idx, player_idx, 0x3f);
                } else {
                    BILL::Set_Their_Spy_Number(player_idx, player_idx, (uint8_t)sum);
                }
                remaining_spies = 0;
            } else {
                if (remaining_spies > 0x3f) {
                    BILL::Set_Their_Spy_Number(player_idx, target, 0x3f);
                    remaining_spies -= 0x3f;
                } else {
                    BILL::Set_Their_Spy_Number(player_idx, target, (uint8_t)remaining_spies);
                    remaining_spies = 0;
                }

                int16_t mission = 0;
                if (MOX::player[player_idx].personality == PLAYER_PERSONALITY_ERRATIC) {
                    if (game_random::Random(8) == 1) {
                        mission = 1;
                    }
                }
                BILL::Set_Their_Spy_Mission(player_idx, target, (int8_t)mission);
            }
        }
    }

    void __cdecl Resolve_Player_Spies(s_player* player) {
        int16_t player_idx = (int16_t)(player - MOX::player);
        int16_t def_spies = BILL::Get_Their_Spy_Number(player_idx, player_idx);
        int32_t def_spies_bonus = spy::N_Spies_Bonus(def_spies);
        int32_t caught_spies = 0;

        for (int16_t other_plr_idx = 0; other_plr_idx < MOX::NUM_PLAYERS; other_plr_idx++) {
            if (other_plr_idx == player_idx) {
                continue;
            }

            s_player* other_player = &MOX::player[other_plr_idx];
            int16_t other_spies = BILL::Get_Their_Spy_Number(other_plr_idx, player_idx);
            int32_t other_spies_bonus = spy::N_Spies_Bonus(other_spies);

            if (other_spies == 0) {
                continue;
            }

            int32_t rand1 = game_random::Random(random_bonus.attacker);
            int32_t rand2 = game_random::Random(random_bonus.defender);

            int32_t other_off = spy_bonuses[other_plr_idx * 2];
            int32_t my_def = spy_bonuses[player_idx * 2 + 1];

            int32_t roll = other_spies_bonus + other_off - my_def - def_spies_bonus - rand2 + rand1;

            if (other_player->objectives != PLAYER_OBJECTIVE_HUMAN && player->objectives == PLAYER_OBJECTIVE_HUMAN) {
                roll += MOX::settings.game_difficulty - GAME_DIFFICULTY_AVERAGE;
            }

            int16_t mission = BILL::Get_Their_Spy_Mission(other_plr_idx, player_idx);

            if (mission == 0) {
                if (roll >= success.steal || rand1 == guaranteed_roll.steal) {
                    uint8_t flag = 0;
                    if (roll >= success.steal_and_frame ||
                        rand1 == guaranteed_roll.steal) {
                        flag = 1;
                    }
                    spy::Steal_App(other_player, player, flag);
                }
            } else if (mission == 1) {
                if (roll >= success.sabotage || rand1 == guaranteed_roll.sabotage) {
                    int16_t blamed = other_plr_idx;
                    if (roll >= success.sabotage_and_frame ||
                        rand1 == guaranteed_roll.sabotage) {
                        spy::Frame_Another(other_plr_idx, player_idx, &blamed, -1);
                    }
                    spy::Destroy_Random_Building(other_plr_idx, player_idx, blamed);
                }
            }

            int32_t rand3 = game_random::Random(100);
            int32_t rand4 = game_random::Random(100);

            int32_t catch_roll = other_off - my_def - def_spies_bonus - rand4 + rand3 + other_spies_bonus - 20;

            if (BILL::Get_Their_Spy_Mission(other_plr_idx, player_idx) == 2) {
                catch_roll += 20;
            }

            if (catch_roll >= 80 || rand3 == 100) {
                caught_spies++;
            }

            if (catch_roll <= -80 || rand4 == 100) {
                other_spies--;
                BILL::Set_Their_Spy_Number(other_plr_idx, player_idx, (uint8_t)other_spies);

                s_MSG msg;
                memset(&msg, 0, sizeof(s_MSG));
                msg.msg_type = MSG_TYPE_LOST_ATTACKING_SPY;
                msg.u.lost_attacking_spy.player_idx = player_idx;
                MSG_::Add_Msg(msg, other_plr_idx);
            }
        }

        if (caught_spies > 0) {
            int16_t def_spies_rem = def_spies - caught_spies;
            if (def_spies_rem < 0) {
                def_spies_rem = 0;
            }
            BILL::Set_Their_Spy_Number(player_idx, player_idx, (uint8_t)def_spies_rem);

            int16_t actually_lost = def_spies - def_spies_rem;
            if (actually_lost > 0) {
                s_MSG msg;
                memset(&msg, 0, sizeof(s_MSG));
                msg.msg_type = MSG_TYPE_LOST_DEFENDING_SPY;
                msg.u.lost_defending_spy.count = actually_lost;
                MSG_::Add_Msg(msg, player_idx);
            }
        }
    }

    void __cdecl Steal_App(s_player* attacker, s_player* victim, int8_t frame_flag) {
        int32_t attacker_idx = (int32_t)(attacker - MOX::player);
        int32_t victim_idx = (int32_t)(victim - MOX::player);
        int32_t chosen_app_id = -1;
        uint32_t total_cost = 0;

        for (int i = 0; i < 204 /* 0xCC */; ++i) {
            if (victim->tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                if (COLCALC::Player_May_Get_Tech_App(attacker, i)) {
                    int16_t field_id = TECHDATA::technology_applications[i].tech_field_id;
                    if (field_id != 0 && Current_Research_Allows_Theft(field_id, i, attacker->current_research_field, attacker->current_research_application)) {
                        uint32_t field_cost = TECHDATA::technology_fields[field_id].cost;
                        if ((int32_t)field_cost > 0) {
                            total_cost += field_cost;
                            if ((uint32_t)game_random::Random(total_cost) <= field_cost) {
                                chosen_app_id = i;
                            }
                        }
                    }
                }
            }
        }

        if (chosen_app_id >= 0) {
            COLCALC::Player_Gets_Tech_App(attacker, chosen_app_id);

            int16_t framed_player_idx = (int16_t)attacker_idx;
            if (frame_flag != 0) {
                spy::Frame_Another((int16_t)attacker_idx, (int16_t)victim_idx, &framed_player_idx, chosen_app_id);
            }

            int32_t penalty = -((int32_t)game_random::Random(5) + (int32_t)game_random::Random(15));
            int32_t mode = (framed_player_idx == (int16_t)attacker_idx) ? 1 : 2;
            diplomac::Change_Relations(penalty, framed_player_idx, victim_idx, mode, 0, chosen_app_id);

            s_report report;
            memset(&report, 0, sizeof(report));

            report.type = 1; /* MSG_TYPE_STEAL */
            report.player_idx = (int16_t)attacker_idx;
            report.action = 0;
            report.d.steal.victim_player_idx = (uint8_t)victim_idx;
            if (framed_player_idx == (int16_t)attacker_idx) {
                report.d.steal.framed_player_idx = -1;
            } else {
                report.d.steal.framed_player_idx = (int8_t)framed_player_idx;
            }
            report.d.steal.tech_app_id = (int16_t)chosen_app_id;
            REPORT::Add_Report(&report);

            report.type = 2; /* MSG_TYPE_ENEMY_STEAL */
            report.player_idx = (int16_t)victim_idx;
            report.action = 0;
            report.d.enemy_steal.enemy_player_idx = framed_player_idx;
            report.d.enemy_steal.tech_app_id = (int16_t)chosen_app_id;
            REPORT::Add_Report(&report);
        }
    }

    void __cdecl Destroy_Random_Building(int16_t framed_player_idx, int16_t victim_player_idx, int32_t acting_player_idx) {
        int16_t target_colony = -1;
        int32_t target_building = -1;
        int32_t total_weight = 0;

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if (colony->owner == victim_player_idx) {
                for (int32_t b_idx = 0; b_idx < BUILDING_COUNT; ++b_idx) {
                    s_tech_building_data* building_data = &TECHDATA::buildings[b_idx];

                    if (b_idx == BUILDING_CAPITOL) {
                        continue;
                    }

                    if (colony->buildings[b_idx] != 0) {
                        int32_t cost = building_data->cost;
                        total_weight += cost;

                        if ((int32_t)game_random::Random(total_weight) <= cost) {
                            target_colony = i;
                            target_building = b_idx;
                        }
                    }
                }
            }
        }

        if (target_colony != -1) {
            ERICNET::Remove_Building(target_colony, (int16_t)target_building);

            int16_t planet_idx = MOX::colony[target_colony].planet;
            int32_t diplomacy_mode = (framed_player_idx == (int16_t)acting_player_idx) ? 3 : 4;

            int32_t penalty = -((int32_t)game_random::Random(5) + (int32_t)game_random::Random(15));
            diplomac::Change_Relations((int16_t)penalty, (int16_t)acting_player_idx, victim_player_idx, diplomacy_mode, planet_idx, (int16_t)target_building);

            s_report report;
            memset(&report, 0, sizeof(report));

            // Report 1: framed player side (Sabotage)
            report.type = 1;
            report.player_idx = framed_player_idx;
            report.action = 1;
            report.d.sabotage.framed_player_idx = (framed_player_idx == (int16_t)acting_player_idx) ? -1 : (int8_t)acting_player_idx;
            report.d.sabotage.victim_player_idx = (uint8_t)victim_player_idx;
            report.d.sabotage.planet_idx = planet_idx;
            report.d.sabotage.building_id = (uint8_t)target_building;
            REPORT::Add_Report(&report);

            // Report 2: victim player side (Enemy Sabotage)
            report.type = 2;
            report.player_idx = victim_player_idx;
            report.action = 1;
            report.d.enemy_sabotage.enemy_player_idx = (int16_t)acting_player_idx;
            report.d.enemy_sabotage.planet_idx = planet_idx;
            report.d.enemy_sabotage.building_id = (uint8_t)target_building;
            REPORT::Add_Report(&report);
        }
    }

} // namespace spy