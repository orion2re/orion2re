#include "pch.h"

#include "legacy_sort.h"
#include "legacy_ids.h"
#include "ship_location_queries.h"
#include "ship_ids.h"

namespace aidudes {

    void Assign_Empire_Building(AIDATA::s_state* ai, game::s_game_state* world, int16_t difficulty, int16_t strategic_combat,
        int32_t stardate, int16_t combat_x, game::id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        const auto legacy_player = player_id.id;
        if (legacy_player >= MAX_PLAYERS) return;
        s_player* p = &world->players[player_idx];
        auto& refits = ai->players[player_idx].refits;
        auto& needs = ai->players[player_idx].build;
        const auto& player_info = ai->players[player_idx].empire;

        int war_level = AIBUILD::Player_War_Level(world, player_id);
        int32_t colony_queue_count = 0;

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

        if (p->war_buildup_duration != 0 && war_level == 0) {
            war_level = is_militarist + 1;
        }

        bool recalculate_colony = false;
        std::vector<game::id_colony> colony_queue_storage(std::ssize(world->colonies));
        game::id_colony* colony_queue = colony_queue_storage.data();

        if (std::ssize(world->colonies) > 0) {
            for (auto& [roster_id, roster_colony] : world->colonies) {
                s_colony* temp_colony = &roster_colony;

                if (legacy_world::Encode_Player_Slot(temp_colony->owner) == (char)player_idx &&
                    temp_colony->outpost_flag == 0 &&
                    temp_colony->n_pops != 0) {
                    if (world != &game::current_state || EVENTS::Event_Check_Space_Anomaly(roster_id) == 0) {
                        if (COLCALC::Colony_Is_Blockaded(*world, temp_colony) ||
                            ai->colonies[roster_id].other_player_arrivals ||
                            low_money) {
                            bool delete_from_queue = false;
                            game::colony_product prod = temp_colony->producing[0];
                            if (COLBLDG::Colony_Production_Is_Queued_Ship(prod)) {
                                const auto* ship = game::Find_Queued_Ship(std::as_const(*world), prod);
                                if (ship && ship->d.ship_type != SHIP_TYPE_COMBAT) {
                                    delete_from_queue = true;
                                }
                            } else if (const auto code = production::Code_Or(prod); code > COLONY_PRODUCTION_REFIT) {
                                if (code > COLONY_PRODUCTION_FARMER) {
                                    if (prod == COLONY_PRODUCTION_TRADE_GOODS) {
                                        recalculate_colony = true;
                                        delete_from_queue = true;
                                    }
                                } else {
                                    delete_from_queue = true;
                                }
                            }

                            if (delete_from_queue) {
                                COLBLDG::Delete_From_Colony_Queue(*world, roster_id, 0);
                            }
                        }

                        game::colony_product prod = temp_colony->producing[0];
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
                        colony_queue[colony_queue_count] = roster_id;
                        colony_queue_count++;
                    }
                }
            }
        }

        if (recalculate_colony) {
            COLCALC::Do_Player_Colony_Calculations(*world, player_id, difficulty);
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
        auto best_planet = AIMOVE::Best_Uncolonized_Undestined_Planet(ai, world, difficulty, strategic_combat, player_id);
        bool is_expansionist = (p->objectives == PLAYER_OBJECTIVE_EXPANSIONIST);

        s_ship_design ship_design;
        ship_design.cost = 0;

        for (int i = 0; i < colony_queue_count;) {
            s_colony* temp_colony = &world->colonies.at(colony_queue[i]);
            bool changed = false;

            const auto incoming_human_ships = [&] {
                if (war_level == 0) return false;
                const auto& planet = world->planets.at(temp_colony->planet_id);
                const auto& system = ai->stars[planet.star_id];
                return system.enroute_human_ships[AIDATA::LAST_FORECAST_TURN] != 0;
            };
            if (AIBUILD::rules.strategic.emergency_defense.enabled &&
                (incoming_human_ships() ||
                    AIBUILD::Colony_Is_Conquered(world, temp_colony))) {
                for (int32_t order_idx = 0; order_idx < AIBUILD::EMERGENCY_DEFENSE_PRODUCT_COUNT; ++order_idx) {
                    int16_t product = AIBUILD::rules.strategic.emergency_defense.order[order_idx];
                    if (product > BUILDING_NO_BUILDING && product < BUILDING_COUNT &&
                        COLCALC::Colony_Can_Build_Product(*world, temp_colony, product)) {
                        AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, product);
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
        if (best_planet) {
            if (player_info.contact_count == 0 ||
                (war_level == 0 && needs.combat_strength.total > 0) ||
                needs.combat_strength.additional_wanted < 10 ||
                isqrt_val > 1) {
                if (AIBUILD::rules.colony_bases_prewarp ||
                    (p->tech_applications[TECH_APP_COLONY_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED &&
                        !no_range_or_speed)) {
                    int val = ai->planets[*best_planet].contextual_values[player_idx];
                    if (war_level > 0) {
                        val /= 4;
                    }

                    int deficit = player_info.colony_deficit;
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

        const auto& colonies = world->colonies;
        Legacy_Sort(std::span{colony_queue, static_cast<size_t>(colony_queue_count)},
            [&colonies](game::id_colony lhs, game::id_colony rhs) {
                return AIBUILD::Industry_Worth(colonies.at(lhs)) - AIBUILD::Industry_Worth(colonies.at(rhs));
            });

        if (AIBUILD::rules.strategic.expansion.enabled &&
            p->tech_applications[TECH_APP_COLONY_BASE] == TECH_RESEARCH_STATUS_RESEARCHED) {
            int i = colony_queue_count;
            while (want_colony_base && i > 0) {
                i--;
                s_colony* temp_colony = &world->colonies.at(colony_queue[i]);
                if (!COLCALC::Colony_Is_Blockaded(*world, temp_colony) &&
                    AIBUILD::Industry_Worth(*temp_colony) >=
                        AIBUILD::rules.strategic.expansion.colony_base_minimum_industry) {
                    const auto& planets = world->planets;
                    const auto& colony_planet = planets.at(temp_colony->planet_id);
                    const auto& destination = planets.at(*best_planet);
                    if (colony_planet.star_id == destination.star_id) {
                        if (temp_colony->buildings[BUILDING_COLONY_BASE] != 1) {
                            AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, BUILDING_COLONY_BASE);
                        }
                        want_colony_base = false;
                    }
                }
            }
        }

        if (AIBUILD::rules.strategic.expansion.enabled && want_colony_base &&
            p->tech_applications[TECH_APP_COLONY_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED) {
            int r = 0;
            if (is_expansionist) {
                if (game_random::Random(5) == 1) {
                    r = 1;
                }
            }

            if (needs.colony_ships.current <= r) {
                int i = colony_queue_count;
                while (want_colony_base && i > 0) {
                    i--;
                    s_colony* temp_colony = &world->colonies.at(colony_queue[i]);
                    if (!COLCALC::Colony_Is_Blockaded(*world, temp_colony) &&
                        AIBUILD::Industry_Worth(*temp_colony) >=
                            AIBUILD::rules.strategic.expansion.colony_ship_minimum_industry) {
                        if (player_info.colony_deficit + 50 < (int32_t)game_random::Random(100)) {
                            s_ship_design colony_ship_design = {};
                            INITSHIP::Load_Colony_Ship_Design(*world, player_id, &colony_ship_design);
                            const auto legacy_colony = colony_queue[i];
                            const auto key = COLBLDG::Create_Ship_From_Design(
                                *world, player_id, &colony_ship_design, legacy_colony, strategic_combat != 0);
                            if (const auto ship_entry = world->ships.find(key); ship_entry != world->ships.end()) {
                                auto* ship = &ship_entry->second;
                                ship->status = 6;
                                AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, game::Queued_Ship_Product(key));
                            } else {
                                no_range_or_speed = true;
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
            s_colony* temp_colony = &world->colonies.at(colony_queue[i]);

            if (AIBUILD::rules.strategic.economic_emergency.enabled && colony_queue_count > 0 &&
                p->bc + p->surplus_bc < p->total_maintenance &&
                p->surplus_bc < 0 &&
                temp_colony->maintenance[1] < temp_colony->production[1]) {
                const game::id_colony current_colony_idx = colony_queue[i];
                AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, AIBUILD::rules.strategic.economic_emergency.product);
                COLCALC::Recalculate_Colony_And_Player(*world, current_colony_idx, difficulty);
                continue;
            }

            int vs_ships_2 = needs.combat_strength.additional_wanted;
            const game::id_colony current_colony_idx = colony_queue[i];
            const auto legacy_colony = current_colony_idx;

            int ship_build_weight = 0;
            if (vs_ships_2 > 0 && p->ship_range != 0 && p->ship_speed != 0 && !no_range_or_speed) {
                int strength_0 = needs.combat_strength.total;
                ship_build_weight = (vs_ships_2 * 1000) / (strength_0 + vs_ships_2);
                if (war_level > 0) {
                    ship_build_weight *= 2;
                }
            }

            int freighter_weight = 0;
            int spy_weight = 0;
            int transport_weight = 0;
            int defer_weight = 0;

            bool threatened = ai->colonies[current_colony_idx].other_player_arrivals;
            if (threatened == 0) {
                if (needs.freighter_groups_wanted != 0 &&
                    freighter_groups_assigned <= 1 &&
                    p->tech_applications[TECH_APP_FREIGHTERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    freighter_weight = (needs.freighter_groups_wanted - freighter_groups_assigned * 2) * 100 + 500;
                    if (war_level > 0) {
                        freighter_weight /= (war_level * war_level);
                    }
                }

                if (needs.spies.additional_wanted != 0) {
                    spy_weight = (needs.spies.additional_wanted * 4 + 40) * 25;
                    if (war_level > 0) {
                        spy_weight /= (war_level * war_level);
                    }
                }

                if (needs.transports.additional_wanted != 0 &&
                    p->tech_applications[TECH_APP_TRANSPORT] == TECH_RESEARCH_STATUS_RESEARCHED &&
                    !no_range_or_speed) {
                    transport_weight = (needs.transports.additional_wanted * 4 + 20) * 25;
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

            uint32_t choice_weights[AIBUILD::STRATEGIC_ALLOCATION_CANDIDATE_COUNT] = {};
            int32_t allocation_weights[AIBUILD::STRATEGIC_ALLOCATION_CANDIDATE_COUNT] = {
                ship_build_weight,
                freighter_weight,
                0,
                spy_weight,
                transport_weight,
                defer_weight,
            };
            production_rules::s_context production_context;
            AIBUILD::Build_Production_Context(ai, world, current_colony_idx, temp_colony, war_level, &production_context);
            production_context.metrics[production_rules::METRIC_FREIGHTER_NEED] =
                needs.freighter_groups_wanted - freighter_groups_assigned * 2;
            production_context.metrics[production_rules::METRIC_REFIT_NEED] = (int32_t)refits.size();
            const production_rules::s_candidate* allocation_candidates =
                AIBUILD::rules.strategic.allocation.candidates;
            for (int32_t weight_idx = 0; weight_idx < AIBUILD::STRATEGIC_ALLOCATION_CANDIDATE_COUNT; ++weight_idx) {
                if (!allocation_candidates[weight_idx].enabled ||
                    !production_rules::Evaluate_Eligibility(
                        &production_context, &allocation_candidates[weight_idx].eligibility)) {
                    allocation_weights[weight_idx] = 0;
                } else {
                    allocation_weights[weight_idx] = production_rules::Evaluate_Weight(
                        &production_context, &allocation_candidates[weight_idx].weight, allocation_weights[weight_idx]);
                }
            }
            production_rules::Apply_Cutoff(&AIBUILD::rules.strategic.allocation.cutoff, 0, allocation_candidates, allocation_weights, AIBUILD::STRATEGIC_ALLOCATION_CANDIDATE_COUNT);
            for (int32_t weight_idx = 0; weight_idx < AIBUILD::STRATEGIC_ALLOCATION_CANDIDATE_COUNT; ++weight_idx) {
                choice_weights[weight_idx] = (uint32_t)allocation_weights[weight_idx];
            }

            while (true) {
                auto random_choice = RUSS::Choose_Weighted_Index_U32(choice_weights, AIBUILD::STRATEGIC_ALLOCATION_CANDIDATE_COUNT);

                if (!random_choice || *random_choice == AIBUILD::ALLOCATION_DEFER) {
                    colony_queue_count--;
                    break;
                }

                switch (*random_choice) {
                    case AIBUILD::ALLOCATION_COMBAT_SHIPS: {
                        bool battlestation_candidate = false;
                        bool star_fortress_candidate = false;
                        bool star_base_candidate = false;

                        int ind_worth = AIBUILD::Industry_Worth(*temp_colony);
                        const auto& colony_planet = world->planets.at(temp_colony->planet_id);
                        const auto& system = ai->stars[colony_planet.star_id];
                        const auto& pressure = system.neighborhood.per_player[player_idx];
                        if (p->surplus_bc > 5 && p->bc > 50) {
                            ind_worth += special::isqrt(special::isqrt(p->bc)) + special::isqrt(p->surplus_bc);
                        }

                        uint16_t ship_weights[HULL_CLASS_COUNT] = {};

                        if (p->ship_designs[SHIP_SIZE_SMALL].cost <= (war_level + 15) * ind_worth * 2) {
                            ship_weights[SHIP_SIZE_SMALL] = 2;
                            int cost_threshold_1 = (war_level * 2 + 33) * ind_worth;
                            if (p->ship_designs[SHIP_SIZE_MEDIUM].cost <= cost_threshold_1) {
                                ship_weights[SHIP_SIZE_MEDIUM] = 16;
                                ship_weights[SHIP_SIZE_SMALL] = 1;

                                if (temp_colony->buildings[BUILDING_STAR_FORTRESS] == 0) {
                                    if (p->tech_applications[TECH_APP_STAR_FORTRESS] == TECH_RESEARCH_STATUS_RESEARCHED &&
                                        COLCALC::Colony_Product_Cost(*world, temp_colony, BUILDING_STAR_FORTRESS) <= (war_level * 2 + 31) * ind_worth) {
                                        star_fortress_candidate = true;
                                        ship_weights[8] = (uint16_t)((pressure.neutral_players_in_range + 2 +
                                                                         (ai->colonies[current_colony_idx].other_player_arrivals + pressure.hostile_players_in_range) * 2) *
                                                                  2 +
                                                              pressure.treaty_partners_in_range) *
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
                                        COLCALC::Colony_Product_Cost(*world, temp_colony, BUILDING_BATTLESTATION) <= cost_threshold_1) {
                                        battlestation_candidate = true;
                                        ship_weights[7] = (uint16_t)((ai->colonies[current_colony_idx].other_player_arrivals + pressure.hostile_players_in_range) * 2 +
                                                              pressure.neutral_players_in_range + 2) *
                                                5 +
                                            pressure.treaty_partners_in_range * 3;
                                        if (p->command_points < p->command_points_used) {
                                            ship_weights[7] += (p->command_points_used - p->command_points) * 2;
                                        }
                                    }

                                    if (temp_colony->buildings[BUILDING_STAR_BASE] == 0) {
                                        if (p->tech_applications[TECH_APP_STAR_BASE] == TECH_RESEARCH_STATUS_RESEARCHED &&
                                            COLCALC::Colony_Product_Cost(*world, temp_colony, BUILDING_STAR_BASE) <= (war_level * 2 + 35) * ind_worth) {
                                            star_base_candidate = true;
                                            ship_weights[6] = (uint16_t)(ai->colonies[current_colony_idx].other_player_arrivals + pressure.hostile_players_in_range) * 10 +
                                                pressure.treaty_partners_in_range * 4 + 12 +
                                                pressure.neutral_players_in_range * 6;
                                            if (p->command_points < p->command_points_used) {
                                                ship_weights[6] += (p->command_points_used - p->command_points) * 2;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        if (COLCALC::Colony_Can_Build_Large_Ships(temp_colony)) {
                            int size_9 = needs.ship_sizes.target_ship_size;
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
                                                *world, player_id, &ship_design, strategic_combat != 0, difficulty, stardate, combat_x);
                                        }
                                        if (ship_design.cost <= (war_level * 2 + 48) * ind_worth) {
                                            ship_weights[SHIP_SIZE_DOOM_STAR] = 150;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (star_base_candidate) {
                                ship_weights[6] += ai->colonies[current_colony_idx].other_player_arrivals * 7 + 2;
                            }
                            if (battlestation_candidate) {
                                ship_weights[7] += ai->colonies[current_colony_idx].other_player_arrivals * 6 + 2;
                            }
                            if (star_fortress_candidate) {
                                ship_weights[8] += ai->colonies[current_colony_idx].other_player_arrivals * 5 + 2;
                            }
                        }

                        if (needs.combat_strength.total > 0 && game_random::Random(5) == 1) {
                            ship_weights[SHIP_SIZE_SMALL] = 0;
                        }

                        auto ship_choice = RUSS::Choose_Weighted_Index_U16(ship_weights, HULL_CLASS_COUNT);
                        if (!ship_choice) {
                            choice_weights[AIBUILD::ALLOCATION_COMBAT_SHIPS] = 0;
                            break;
                        }
                        switch (*ship_choice) {
                            case SHIP_SIZE_SMALL:
                            case SHIP_SIZE_MEDIUM:
                            case SHIP_SIZE_LARGE:
                            case SHIP_SIZE_HUGE:
                            case SHIP_SIZE_TITAN: {
                                const auto key = COLBLDG::Create_Ship_From_Design(*world, player_id, &p->ship_designs[*ship_choice], legacy_colony, strategic_combat != 0);
                                if (const auto ship_entry = world->ships.find(key); ship_entry != world->ships.end()) {
                                    auto* ship = &ship_entry->second;
                                    ship->status = 6;
                                    AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, game::Queued_Ship_Product(key));
                                    const auto opponents = AIBUILD::Opponents(world, difficulty);
                                    needs.combat_strength.additional_wanted -= AIBUILD::Mean_Ship_Strength(&opponents, &ai->ships[key], &player_info, ship);
                                } else {
                                    no_range_or_speed = true;
                                }
                                break;
                            }
                            case SHIP_SIZE_DOOM_STAR: {
                                const auto key = COLBLDG::Create_Ship_From_Design(
                                    *world, player_id, &ship_design, legacy_colony, strategic_combat != 0);
                                if (const auto ship_entry = world->ships.find(key); ship_entry != world->ships.end()) {
                                    auto* ship = &ship_entry->second;
                                    ship->status = 6;
                                    AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, game::Queued_Ship_Product(key));
                                    const auto opponents = AIBUILD::Opponents(world, difficulty);
                                    needs.combat_strength.additional_wanted -= AIBUILD::Mean_Ship_Strength(&opponents, &ai->ships[key], &player_info, ship);
                                } else {
                                    no_range_or_speed = true;
                                }
                                break;
                            }
                            case HULL_STAR_BASE:
                                AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, BUILDING_STAR_BASE);
                                needs.combat_strength.additional_wanted -= (pressure.hostile_players_in_range * 2 + pressure.neutral_players_in_range + pressure.treaty_partners_in_range);
                                break;
                            case HULL_BATTLESTATION:
                                AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, BUILDING_BATTLESTATION);
                                needs.combat_strength.additional_wanted -= (pressure.hostile_players_in_range * 3 + pressure.neutral_players_in_range * 2 + pressure.treaty_partners_in_range);
                                break;
                            case HULL_STAR_FORTRESS:
                                AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, BUILDING_STAR_FORTRESS);
                                needs.combat_strength.additional_wanted -= (pressure.hostile_players_in_range * 6 + pressure.neutral_players_in_range * 4 + pressure.treaty_partners_in_range);
                                break;
                        }
                        choice_weights[AIBUILD::ALLOCATION_COMBAT_SHIPS] = 0;
                        break;
                    }
                    case AIBUILD::ALLOCATION_FREIGHTERS:
                        if (!COLCALC::Colony_Is_Blockaded(*world, temp_colony) && AIBUILD::Industry_Worth(*temp_colony) >= 12) {
                            AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, COLONY_PRODUCTION_FREIGHTERS);
                            freighter_groups_assigned++;
                            needs.freighter_groups_wanted--;
                        }
                        choice_weights[AIBUILD::ALLOCATION_FREIGHTERS] = 0;
                        break;
                    case AIBUILD::ALLOCATION_REFITS: {
                        const auto refit_star = world->planets.at(temp_colony->planet_id).star_id;
                        int n_entries = (int)refits.size() - 1;
                        bool refitted = false;
                        for (int j = n_entries; j >= 0; j--) {
                            const auto key = refits[j].ship_id;
                            const auto s_entry = world->ships.find(key);
                            auto* s = s_entry == world->ships.end() ? nullptr : &s_entry->second;
                            if (!s || s->status == 5) continue;
                            if (ships::Orbit_Star(s->position) == refit_star &&
                                (s->d.size < SHIP_SIZE_LARGE || COLCALC::Colony_Can_Build_Large_Ships(temp_colony))) {
                                s_ship_design* refit_design;
                                if (s->d.size == SHIP_SIZE_DOOM_STAR) {
                                    if (ship_design.cost == 0) {
                                        INITSHIP::Load_Doom_Star_Ship_Design(
                                            *world, player_id, &ship_design, strategic_combat != 0, difficulty, stardate, combat_x);
                                    }
                                    refit_design = &ship_design;
                                } else {
                                    refit_design = &p->ship_designs[s->d.size];
                                }

                                int16_t cost = AIBUILD::Refit_Cost(p, &s->d, refit_design);
                                if (cost <= refit_design->cost &&
                                    cost <= AIBUILD::Industry_Worth(*temp_colony) * (war_level + 10)) {
                                    if (s->officer_id != game::NO_LEADER) {
                                        OFFICER::Move_Officer_To_Pool(*world, s->officer_id);
                                        s->officer_id = game::NO_LEADER;
                                    }

                                    s->d = *refit_design;
                                    s->d.cost = cost;
                                    s->status = 4;
                                    AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, game::Queued_Ship_Product(key));

                                    refits.erase(refits.begin() + j);
                                    refitted = true;
                                    break;
                                }
                            }
                        }
                        if (!refitted) {
                        choice_weights[AIBUILD::ALLOCATION_REFITS] = 0;
                        }
                        break;
                    }
                    case AIBUILD::ALLOCATION_SPIES:
                        if (!COLCALC::Colony_Is_Blockaded(*world, temp_colony) && AIBUILD::Industry_Worth(*temp_colony) >= 15) {
                            AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, COLONY_PRODUCTION_SPY);
                            needs.spies.additional_wanted--;
                        }
                        choice_weights[AIBUILD::ALLOCATION_SPIES] = 0;
                        break;
                    case AIBUILD::ALLOCATION_TRANSPORTS:
                        if (!COLCALC::Colony_Is_Blockaded(*world, temp_colony) && AIBUILD::Industry_Worth(*temp_colony) >= 14) {
                            if (temp_colony->buildings[BUILDING_MARINE_BARRACKS] == 0) {
                                AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, BUILDING_MARINE_BARRACKS);
                                needs.transports.additional_wanted--;
                            } else {
                                s_ship_design transport_design = {};
                                INITSHIP::Load_Transport_Ship_Design(*world, player_id, &transport_design);
                                const auto key = COLBLDG::Create_Ship_From_Design(
                                    *world, player_id, &transport_design, legacy_colony, strategic_combat != 0);
                                if (const auto ship_entry = world->ships.find(key); ship_entry != world->ships.end()) {
                                    auto* ship = &ship_entry->second;
                                    ship->status = 6;
                                    AIBUILD::Force_Produce(world, colony_queue, i, &colony_queue_count, game::Queued_Ship_Product(key));
                                    needs.transports.additional_wanted--;
                                } else {
                                    no_range_or_speed = true;
                                }
                            }
                        }
                        choice_weights[AIBUILD::ALLOCATION_TRANSPORTS] = 0;
                        break;
                }

                if (colony_queue_count != prev_count) {
                    break;
                }
            }
        }
    }
} // namespace aidudes
