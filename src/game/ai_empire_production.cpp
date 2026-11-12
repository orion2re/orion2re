#include "pch.h"

namespace aidudes {
    bool builds_colony_bases_prewarp = false;

    int __cdecl colony_less_industry_cmp(const void* p1, const void* p2) {
        int16_t worth1 = AIBUILD::Industry_Worth(&MOX::colony[*((const int16_t*)p1)]);
        int16_t worth2 = AIBUILD::Industry_Worth(&MOX::colony[*((const int16_t*)p2)]);

        return (int)worth1 - (int)worth2;
    }

    void __cdecl Assign_Empire_Building(int16_t player_idx) {
        s_player* p = &MOX::player[player_idx];
        s_ai_refit_player* p_refit = &(*AIDATA::airefit)[player_idx];

        int war_level = AIBUILD::Player_War_Level(player_idx);
        int colony_queue_count = 0;

        bool is_militarist = (p->objectives == PLAYER_OBJECTIVE_MILITARIST);
        bool low_money = (p->bc + p->surplus_bc < p->total_maintenance);

        int isqrt_val = 0;
        if (low_money || p->bc < 1500) {
            isqrt_val = 0;
        } else {
            isqrt_val = special::isqrt((p->surplus_bc + ((p->surplus_bc >> 31) & 0x3f)) >> 6);
        }

        if (isqrt_val > 10) {
            isqrt_val = 10;
        }

        RUSS::Mox_Update();

        if (p->war_buildup_duration != 0 && war_level == 0) {
            war_level = is_militarist + 1;
        }

        bool recalculate_colony = false;
        int16_t colony_queue[MAX_COLONIES];

        if (MOX::NUM_COLONIES > 0) {
            for (int16_t current_colony_idx = 0; current_colony_idx < MOX::NUM_COLONIES; ++current_colony_idx) {
                s_colony* temp_colony = &MOX::colony[current_colony_idx];

                if (temp_colony->owner == (char)player_idx &&
                    temp_colony->outpost_flag == 0 &&
                    temp_colony->n_pops != 0) {
                    if (EVENTS::Event_Check_Space_Anomaly(current_colony_idx) == 0) {
                        RUSS::Mox_Update();

                        if (COLCALC::Colony_Is_Blockaded(temp_colony) ||
                            (*AIDATA::ai_colony_info)[current_colony_idx].threatened != 0 ||
                            low_money) {
                            bool delete_from_queue = false;
                            int16_t prod = temp_colony->producing[0];
                            if (COLBLDG::Colony_Production_Is_Queued_Ship(prod)) {
                                if (MOX::ship[COLBLDG::Colony_Production_Ship_Index(prod)].d.ship_type != SHIP_TYPE_COMBAT) {
                                    delete_from_queue = true;
                                }
                            } else if (prod > COLONY_PRODUCTION_REFIT) {
                                if (prod > COLONY_PRODUCTION_FARMER) {
                                    if (prod == COLONY_PRODUCTION_TRADE_GOODS) {
                                        recalculate_colony = true;
                                        delete_from_queue = true;
                                    }
                                } else {
                                    delete_from_queue = true;
                                }
                            }

                            if (delete_from_queue) {
                                COLBLDG::Delete_From_Colony_Queue(current_colony_idx, 0);
                            }
                        }

                        int16_t prod = temp_colony->producing[0];
                        if (prod == COLONY_PRODUCTION_HOUSING) {
                            // pass
                        } else if (prod == COLONY_PRODUCTION_TRADE_GOODS) {
                            recalculate_colony = true;
                        } else if (prod == COLONY_PRODUCTION_NONE) {
                            // pass
                        } else {
                            continue;
                        }

                        temp_colony->producing[0] = COLONY_PRODUCTION_AI_UNASSIGNED;
                        colony_queue[colony_queue_count] = current_colony_idx;
                        colony_queue_count++;
                    }
                }
            }
        }

        if (recalculate_colony) {
            COLCALC::Do_Player_Colony_Calculations(player_idx);
        }

        if (colony_queue_count <= 0) {
            return;
        }

        bool no_range_or_speed;
        if (p->ship_range == 0 || p->ship_speed == 0) {
            no_range_or_speed = true;
        } else {
            no_range_or_speed = false;
        }

        int freighter_groups_assigned = 0;
        int16_t best_planet = AIMOVE::Best_Uncolonized_Undestined_Planet(player_idx);
        bool is_expansionist = (p->objectives == PLAYER_OBJECTIVE_EXPANSIONIST);

        s_ship_design ship_design;
        ship_design.cost = 0;

        for (int i = 0; i < colony_queue_count;) {
            s_colony* temp_colony = &MOX::colony[colony_queue[i]];
            RUSS::Mox_Update();
            bool changed = false;

            if (AIBUILD::strategic_production.emergency_defense.enabled &&
                ((war_level != 0 && (*AIDATA::enroute_human_ships)[MOX::planet[temp_colony->planet].star_index][9] != 0) ||
                    AIBUILD::Colony_Is_Conquered(temp_colony))) {
                for (int32_t order_idx = 0; order_idx < AIBUILD::EMERGENCY_DEFENSE_PRODUCT_COUNT; ++order_idx) {
                    int16_t product = AIBUILD::strategic_production.emergency_defense.order[order_idx];
                    if (product > BUILDING_NO_BUILDING && product < BUILDING_COUNT &&
                        COLCALC::Colony_Can_Build_Product(temp_colony, product)) {
                        AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, product);
                        changed = true;
                        break;
                    }
                }
            }

            if (!changed) {
                ++i;
            }
        }

        bool want_colony_base = false;
        if (best_planet != -1) {
            if ((*AIDATA::ai_player_info)[player_idx].n_contacts == 0 ||
                (war_level == 0 && AIBUILD::player_total_strength_vs_ships[player_idx][0] > 0) ||
                AIBUILD::player_total_strength_vs_ships[player_idx][2] < 10 ||
                isqrt_val > 1) {
                if (builds_colony_bases_prewarp ||
                    (p->tech_applications[TECH_APP_COLONY_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED &&
                        !no_range_or_speed)) {
                    int val = (*AIDATA::contextual_planet_values)[player_idx * MAX_PLANETS + best_planet];
                    if (war_level > 0) {
                        val /= 4;
                    }

                    int deficit = (*AIDATA::ai_player_info)[player_idx].colony_deficit;
                    int bonus = deficit * 10;
                    if (val / 2 < bonus) {
                        bonus = val / 2;
                    }
                    val += bonus;

                    int rand_val = game_random::Random(700);
                    int adjusted_rand = rand_val + 100 + (is_expansionist ? 0 : 100);

                    if (adjusted_rand < (isqrt_val + 64) * val) {
                        want_colony_base = true;
                    }
                }
            }
        }

        RUSS::Mox_Update();
        qsort(colony_queue, (uint32_t)colony_queue_count, sizeof(int16_t), aidudes::colony_less_industry_cmp);

        if (AIBUILD::strategic_production.expansion.enabled &&
            p->tech_applications[TECH_APP_COLONY_BASE] == TECH_RESEARCH_STATUS_RESEARCHED) {
            int i = colony_queue_count;
            while (want_colony_base && i > 0) {
                i--;
                s_colony* temp_colony = &MOX::colony[colony_queue[i]];
                if (!COLCALC::Colony_Is_Blockaded(temp_colony) &&
                    AIBUILD::Industry_Worth(temp_colony) >=
                        AIBUILD::strategic_production.expansion.colony_base_minimum_industry) {
                    if (MOX::planet[temp_colony->planet].star_index == MOX::planet[best_planet].star_index) {
                        if (temp_colony->buildings[BUILDING_COLONY_BASE] != 1) {
                            AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, BUILDING_COLONY_BASE);
                        }
                        want_colony_base = false;
                    }
                }
            }
        }

        RUSS::Mox_Update();
        if (AIBUILD::strategic_production.expansion.enabled && want_colony_base &&
            p->tech_applications[TECH_APP_COLONY_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED) {
            int r = 0;
            if (is_expansionist) {
                if (game_random::Random(5) == 1) {
                    r = 1;
                }
            }

            if (AIBUILD::player_n_colony_ships[player_idx][0] <= r) {
                int i = colony_queue_count;
                while (want_colony_base && i > 0) {
                    i--;
                    s_colony* temp_colony = &MOX::colony[colony_queue[i]];
                    if (!COLCALC::Colony_Is_Blockaded(temp_colony) &&
                        AIBUILD::Industry_Worth(temp_colony) >=
                            AIBUILD::strategic_production.expansion.colony_ship_minimum_industry) {
                        if ((*AIDATA::ai_player_info)[player_idx].colony_deficit + 50 < (int32_t)game_random::Random(100)) {
                            s_ship_design colony_ship_design = {};
                            INITSHIP::Load_Colony_Ship_Design(player_idx, &colony_ship_design);
                            int16_t ship_idx = COLBLDG::Create_Ship_From_Design(
                                player_idx, &colony_ship_design, colony_queue[i]);
                            if (ship_idx == -1) {
                                no_range_or_speed = true;
                            } else {
                                MOX::ship[ship_idx].status = 6;
                                AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, COLBLDG::Colony_Production_From_Ship_Index(ship_idx));
                            }
                            want_colony_base = false;
                        }
                    }
                }
            }
        }

        while (colony_queue_count > 0) {
            int prev_count = colony_queue_count;
            int i = colony_queue_count - 1;
            s_colony* temp_colony = &MOX::colony[colony_queue[i]];

            RUSS::Mox_Update();

            if (AIBUILD::strategic_production.economic_emergency.enabled && colony_queue_count > 0 &&
                p->bc + p->surplus_bc < p->total_maintenance &&
                p->surplus_bc < 0 &&
                temp_colony->maintenance[1] < temp_colony->production[1]) {
                int16_t current_colony_idx = colony_queue[i];
                AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, AIBUILD::strategic_production.economic_emergency.product);
                COLCALC::Colony_Calculation(current_colony_idx);
                continue;
            }

            int vs_ships_2 = AIBUILD::player_total_strength_vs_ships[player_idx][2];
            int16_t current_colony_idx = colony_queue[i];

            int ship_build_weight = 0;
            if (vs_ships_2 > 0 && p->ship_range != 0 && p->ship_speed != 0 && !no_range_or_speed) {
                int strength_0 = AIBUILD::player_total_strength_vs_ships[player_idx][0];
                ship_build_weight = (vs_ships_2 * 1000) / (strength_0 + vs_ships_2);
                if (war_level > 0) {
                    ship_build_weight *= 2;
                }
            }

            int freighter_weight = 0;
            int spy_weight = 0;
            int transport_weight = 0;
            int defer_weight = 0;

            uint8_t threatened = (*AIDATA::ai_colony_info)[current_colony_idx].threatened;
            if (threatened == 0) {
                if (AIBUILD::player_n_freighter_groups_wanted[player_idx] != 0 &&
                    freighter_groups_assigned <= 1 &&
                    p->tech_applications[TECH_APP_FREIGHTERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    freighter_weight = (AIBUILD::player_n_freighter_groups_wanted[player_idx] - freighter_groups_assigned * 2) * 100 + 500;
                    if (war_level > 0) {
                        freighter_weight /= (war_level * war_level);
                    }
                }

                if (AIBUILD::player_n_spies[player_idx][1] != 0) {
                    spy_weight = (AIBUILD::player_n_spies[player_idx][1] * 4 + 40) * 25;
                    if (war_level > 0) {
                        spy_weight /= (war_level * war_level);
                    }
                }

                if (AIBUILD::player_n_transports[player_idx][1] != 0 &&
                    p->tech_applications[TECH_APP_TRANSPORT] == TECH_RESEARCH_STATUS_RESEARCHED &&
                    !no_range_or_speed) {
                    transport_weight = (AIBUILD::player_n_transports[player_idx][1] * 4 + 20) * 25;
                    if (war_level > 0) {
                        transport_weight = (transport_weight * 3) / 2;
                    }
                }
            }

            if (war_level == 0) {
                defer_weight = 333;
            } else {
                defer_weight = 350 - war_level * 100;
            }

            uint32_t choice_weights[6] = {0, 0, 0, 0, 0, 0};
            int32_t allocation_weights[6] = {
                ship_build_weight,
                freighter_weight,
                0,
                spy_weight,
                transport_weight,
                defer_weight,
            };
            production_rules::s_context production_context;
            AIBUILD::Build_Production_Context(temp_colony, war_level, &production_context);
            production_context.metrics[production_rules::METRIC_FREIGHTER_NEED] =
                AIBUILD::player_n_freighter_groups_wanted[player_idx] - freighter_groups_assigned * 2;
            production_context.metrics[production_rules::METRIC_REFIT_NEED] = p_refit->n_entries;
            production_rules::s_candidate* allocation_candidates =
                AIBUILD::strategic_production.allocation.candidates;
            for (int32_t weight_idx = 0; weight_idx < 6; ++weight_idx) {
                if (!allocation_candidates[weight_idx].enabled ||
                    !production_rules::Evaluate_Eligibility(
                        &production_context, &allocation_candidates[weight_idx].eligibility)) {
                    allocation_weights[weight_idx] = 0;
                } else {
                    allocation_weights[weight_idx] = production_rules::Evaluate_Weight(
                        &production_context, &allocation_candidates[weight_idx].weight, allocation_weights[weight_idx]);
                }
            }
            production_rules::Apply_Cutoff(&AIBUILD::strategic_production.allocation.cutoff, 0, allocation_candidates, allocation_weights, AIBUILD::STRATEGIC_ALLOCATION_CANDIDATE_COUNT);
            for (int32_t weight_idx = 0; weight_idx < AIBUILD::STRATEGIC_ALLOCATION_CANDIDATE_COUNT; ++weight_idx) {
                choice_weights[weight_idx] = (uint32_t)allocation_weights[weight_idx];
            }

            while (true) {
                RUSS::Mox_Update();
                int random_choice = RUSS::Get_Weighted_Choice_Int(choice_weights, 6);

                if (random_choice == -1 || random_choice == 5) {
                    colony_queue_count--;
                    break;
                }

                switch (random_choice) {
                    case 0: {
                        bool battlestation_candidate = false;
                        bool star_fortress_candidate = false;
                        bool star_base_candidate = false;

                        int ind_worth = AIBUILD::Industry_Worth(temp_colony);
                        if (p->surplus_bc > 5 && p->bc > 50) {
                            ind_worth += special::isqrt(special::isqrt(p->bc)) + special::isqrt(p->surplus_bc);
                        }

                        uint16_t ship_weights[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

                        if (p->ship_designs[SHIP_SIZE_SMALL].cost <= (war_level + 15) * ind_worth * 2) {
                            ship_weights[SHIP_SIZE_SMALL] = 2;
                            int cost_threshold_1 = (war_level * 2 + 33) * ind_worth;
                            if (p->ship_designs[SHIP_SIZE_MEDIUM].cost <= cost_threshold_1) {
                                ship_weights[SHIP_SIZE_MEDIUM] = 16;
                                ship_weights[SHIP_SIZE_SMALL] = 1;

                                if (temp_colony->buildings[BUILDING_STAR_FORTRESS] == 0) {
                                    if (p->tech_applications[TECH_APP_STAR_FORTRESS] == TECH_RESEARCH_STATUS_RESEARCHED &&
                                        COLCALC::Colony_Product_Cost(MOX::colony, BUILDING_STAR_FORTRESS) <= (war_level * 2 + 31) * ind_worth) {
                                        star_fortress_candidate = true;
                                        ship_weights[8] = (uint16_t)(((*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].neutral_count + 2 +
                                                                         ((*AIDATA::ai_colony_info)[i].threatened + (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].alliance_count) * 2) *
                                                                  2 +
                                                              (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].enemy_count) *
                                            2;
                                        if (p->command_points < p->command_points_used) {
                                            ship_weights[8] += (p->command_points_used - p->command_points) * 2;
                                        }
                                    }
                                }

                                // ORION95 0047ca47 skips all station candidates when a Fortress exists.
                                if (temp_colony->buildings[BUILDING_STAR_FORTRESS] == 0 &&
                                    temp_colony->buildings[BUILDING_BATTLESTATION] == 0) {
                                    if (p->tech_applications[TECH_APP_BATTLESTATION] == TECH_RESEARCH_STATUS_RESEARCHED &&
                                        COLCALC::Colony_Product_Cost(MOX::colony, BUILDING_BATTLESTATION) <= cost_threshold_1) {
                                        battlestation_candidate = true;
                                        ship_weights[7] = (uint16_t)(((*AIDATA::ai_colony_info)[i].threatened + (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].alliance_count) * 2 +
                                                              (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].neutral_count + 2) *
                                                5 +
                                            (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].enemy_count * 3;
                                        if (p->command_points < p->command_points_used) {
                                            ship_weights[7] += (p->command_points_used - p->command_points) * 2;
                                        }
                                    }

                                    if (temp_colony->buildings[BUILDING_STAR_BASE] == 0) {
                                        if (p->tech_applications[TECH_APP_STAR_BASE] == TECH_RESEARCH_STATUS_RESEARCHED &&
                                            COLCALC::Colony_Product_Cost(MOX::colony, BUILDING_STAR_BASE) <= (war_level * 2 + 35) * ind_worth) {
                                            star_base_candidate = true;
                                            ship_weights[6] = (uint16_t)((*AIDATA::ai_colony_info)[i].threatened + (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].alliance_count) * 10 +
                                                (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].enemy_count * 4 + 12 +
                                                (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].neutral_count * 6;
                                            if (p->command_points < p->command_points_used) {
                                                ship_weights[6] += (p->command_points_used - p->command_points) * 2;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        if (COLCALC::Colony_Can_Build_Large_Ships(temp_colony)) {
                            int size_9 = AIBUILD::player_ship_sizes[player_idx].target_ship_size;
                            if (size_9 >= SHIP_SIZE_LARGE && p->ship_designs[SHIP_SIZE_LARGE].cost <= (war_level * 2 + 45) * ind_worth) {
                                ship_weights[SHIP_SIZE_LARGE] = 32;
                                ship_weights[SHIP_SIZE_MEDIUM] /= 5;
                                if (size_9 >= SHIP_SIZE_HUGE && p->ship_designs[SHIP_SIZE_HUGE].cost <= (war_level * 2 + 55) * ind_worth) {
                                    ship_weights[SHIP_SIZE_HUGE] = 64;
                                    ship_weights[SHIP_SIZE_LARGE] = 5;
                                    if (p->tech_applications[TECH_APP_TITAN_CONSTRUCTION] == TECH_RESEARCH_STATUS_RESEARCHED && size_9 >= SHIP_SIZE_TITAN && p->ship_designs[SHIP_SIZE_TITAN].cost <= (war_level * 2 + 48) * ind_worth) {
                                        ship_weights[SHIP_SIZE_TITAN] = 100;
                                    }
                                    if (p->tech_applications[TECH_APP_DOOM_STAR_CONSTRUCTION] == TECH_RESEARCH_STATUS_RESEARCHED && size_9 >= SHIP_SIZE_DOOM_STAR) {
                                        if (ship_design.cost == 0) {
                                            INITSHIP::Load_Doom_Star_Ship_Design(
                                                player_idx, &ship_design, MOX::settings.strategic_combat_flag != 0);
                                        }
                                        if (ship_design.cost <= (war_level * 2 + 48) * ind_worth) {
                                            ship_weights[SHIP_SIZE_DOOM_STAR] = 150;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (star_base_candidate) {
                                ship_weights[6] += (*AIDATA::ai_colony_info)[current_colony_idx].threatened * 7 + 2;
                            }
                            if (battlestation_candidate) {
                                ship_weights[7] += (*AIDATA::ai_colony_info)[current_colony_idx].threatened * 6 + 2;
                            }
                            if (star_fortress_candidate) {
                                ship_weights[8] += (*AIDATA::ai_colony_info)[current_colony_idx].threatened * 5 + 2;
                            }
                        }

                        if (AIBUILD::player_total_strength_vs_ships[player_idx][0] > 0 && game_random::Random(5) == 1) {
                            ship_weights[SHIP_SIZE_SMALL] = 0;
                        }

                        int ship_choice = RUSS::Get_Weighted_Choice_Short(ship_weights, 9);
                        switch (ship_choice + 1) {
                            case 0:
                                break;
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                            case 5: {
                                int16_t created_idx = COLBLDG::Create_Ship(player_idx, (int16_t)ship_choice, current_colony_idx);
                                if (created_idx == -1) {
                                    no_range_or_speed = true;
                                } else {
                                    MOX::ship[created_idx].status = 6;
                                    AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, COLBLDG::Colony_Production_From_Ship_Index(created_idx));
                                    AIBUILD::player_total_strength_vs_ships[player_idx][2] -= AIBUILD::Mean_Ship_Strength(&MOX::ship[created_idx]);
                                }
                                break;
                            }
                            case 6: {
                                int16_t ds_idx = COLBLDG::Create_Ship_From_Design(
                                    player_idx, &ship_design, current_colony_idx);
                                if (ds_idx == -1) {
                                    no_range_or_speed = true;
                                } else {
                                    MOX::ship[ds_idx].status = 6;
                                    AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, COLBLDG::Colony_Production_From_Ship_Index(ds_idx));
                                    AIBUILD::player_total_strength_vs_ships[player_idx][2] -= AIBUILD::Mean_Ship_Strength(&MOX::ship[ds_idx]);
                                }
                                break;
                            }
                            case 7:
                                AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, BUILDING_STAR_BASE);
                                AIBUILD::player_total_strength_vs_ships[player_idx][2] -= ((*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].alliance_count * 2 + (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].neutral_count + (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].enemy_count);
                                break;
                            case 8:
                                AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, BUILDING_BATTLESTATION);
                                AIBUILD::player_total_strength_vs_ships[player_idx][2] -= ((*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].alliance_count * 3 + (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].neutral_count * 2 + (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].enemy_count);
                                break;
                            case 9:
                                AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, BUILDING_STAR_FORTRESS);
                                AIBUILD::player_total_strength_vs_ships[player_idx][2] -= ((*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].alliance_count * 6 + (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].neutral_count * 4 + (*AIDATA::ai_star_info)[MOX::planet[temp_colony->planet].star_index].per_player[player_idx].enemy_count);
                                break;
                        }
                        choice_weights[0] = 0;
                        break;
                    }
                    case 1:
                        if (!COLCALC::Colony_Is_Blockaded(temp_colony) && AIBUILD::Industry_Worth(temp_colony) >= 12) {
                            AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, COLONY_PRODUCTION_FREIGHTERS);
                            freighter_groups_assigned++;
                            AIBUILD::player_n_freighter_groups_wanted[player_idx]--;
                        }
                        choice_weights[1] = 0;
                        break;
                    case 2: {
                        int16_t refit_planet_idx = MOX::planet[temp_colony->planet].star_index;
                        int n_entries = p_refit->n_entries - 1;
                        bool refitted = false;
                        for (int j = n_entries; j >= 0; j--) {
                            int16_t ship_idx = p_refit->entry[j].ship_idx;
                            s_ship_data* s = &MOX::ship[ship_idx];
                            if (s->location == refit_planet_idx &&
                                (s->d.size < SHIP_SIZE_LARGE || COLCALC::Colony_Can_Build_Large_Ships(temp_colony))) {
                                s_ship_design* refit_design;
                                if (s->d.size == SHIP_SIZE_DOOM_STAR) {
                                    if (ship_design.cost == 0) {
                                        INITSHIP::Load_Doom_Star_Ship_Design(
                                            player_idx, &ship_design, MOX::settings.strategic_combat_flag != 0);
                                    }
                                    refit_design = &ship_design;
                                } else {
                                    refit_design = &p->ship_designs[s->d.size];
                                }

                                int16_t cost = AIBUILD::Refit_Cost(player_idx, &s->d, refit_design);
                                if (cost <= refit_design->cost &&
                                    cost <= AIBUILD::Industry_Worth(temp_colony) * (war_level + 10)) {
                                    if (s->officer_index != -1) {
                                        OFFICER::Move_Officer_To_Pool(s->officer_index);
                                        s->officer_index = -1;
                                    }

                                    memcpy(&s->d, refit_design, sizeof(s_ship_design));
                                    s->d.cost = cost;
                                    s->status = 4;
                                    AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, COLBLDG::Colony_Production_From_Ship_Index(ship_idx));

                                    p_refit->n_entries--;
                                    memmove(&p_refit->entry[j], &p_refit->entry[j + 1], (p_refit->n_entries - j) * sizeof(s_ai_refit_entry));
                                    refitted = true;
                                    break;
                                }
                            }
                        }
                        if (!refitted) {
                            choice_weights[2] = 0;
                        }
                        break;
                    }
                    case 3:
                        if (!COLCALC::Colony_Is_Blockaded(temp_colony) && AIBUILD::Industry_Worth(temp_colony) >= 15) {
                            AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, COLONY_PRODUCTION_SPY);
                            AIBUILD::player_n_spies[player_idx][1]--;
                        }
                        choice_weights[3] = 0;
                        break;
                    case 4:
                        if (!COLCALC::Colony_Is_Blockaded(temp_colony) && AIBUILD::Industry_Worth(temp_colony) >= 14) {
                            if (temp_colony->buildings[BUILDING_MARINE_BARRACKS] == 0) {
                                AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, BUILDING_MARINE_BARRACKS);
                                AIBUILD::player_n_transports[player_idx][1]--;
                            } else {
                                s_ship_design transport_design = {};
                                INITSHIP::Load_Transport_Ship_Design(player_idx, &transport_design);
                                int16_t tr_idx = COLBLDG::Create_Ship_From_Design(
                                    player_idx, &transport_design, current_colony_idx);
                                if (tr_idx == -1) {
                                    no_range_or_speed = true;
                                } else {
                                    MOX::ship[tr_idx].status = 6;
                                    AIBUILD::Force_Produce(colony_queue, (int16_t)i, (int32_t*)&colony_queue_count, COLBLDG::Colony_Production_From_Ship_Index(tr_idx));
                                    AIBUILD::player_n_transports[player_idx][1]--;
                                }
                            }
                        }
                        choice_weights[4] = 0;
                        break;
                }

                if (colony_queue_count != prev_count) {
                    break;
                }
            }
        }
    }
} // namespace aidudes
