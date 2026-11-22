#include "pch.h"
#include "ai_compat.h"

#include "legacy_sort.h"
#include "legacy_world.h"

namespace AIDATA {
    using game::e_ship_location;

    id_player Colony_Dominant_Player(int32_t player_count, const s_colony* colony) {
        uint8_t player_counts[MAX_PLAYERS] = {0};
        int total_pops = 0;

        if (colony->n_pops != 0) {
            for (int i = 0; i < (int)colony->n_pops; ++i) {
                uint32_t player_id = POP::Get_Race(colony->pop[i]);
                if (player_id < MAX_PLAYERS) {
                    total_pops++;
                    player_counts[player_id]++;
                }
            }
        }

        int max_player = player_count - 1;
        int i = 0;
        if (max_player > 0) {
            do {
                if (player_counts[i] > player_counts[max_player]) {
                    max_player = i;
                }
                i++;
            } while (i < max_player);
        }

        uint8_t max_count = player_counts[max_player];

        if (max_count * 2 > total_pops) {
            return legacy_world::Player_Id(max_player);
        }

        int16_t current_owner = (int16_t)colony->owner;
        uint8_t owner_count = player_counts[current_owner];

        if (owner_count * 3 <= total_pops && max_count != 0 && owner_count != max_count) {
            return legacy_world::Player_Id(max_player);
        }

        return legacy_world::Player_Id(current_owner);
    }


    int32_t __cdecl Refit_Desirability(const s_player* owner, const s_technology_assessment* ai_tech, const s_ship_data* ship) {
        int32_t desirability = 0;

        if ((int8_t)ship->d.previous_owner != (int8_t)ship->owner) {
            return 0;
        }

        if (ai_tech->best_shield > ship->d.shield_type) {
            desirability += (ai_tech->best_shield - ship->d.shield_type) * 10;
        }

        if (ai_tech->best_computer > ship->d.computer_type) {
            desirability += (ai_tech->best_computer - ship->d.computer_type) * 7;
        }

        if (ai_tech->best_armor > ship->d.armor_type) {
            desirability += (ai_tech->best_armor - ship->d.armor_type) * 5;
        }

        uint8_t best_beam_cost = 0;
        uint8_t best_missile_cost = 0;

        for (int i = 0; i < 8; ++i) {
            int weapon_type = (int)ship->d.ship_weapon[i].type;
            if (weapon_type != WEAPON_NO_WEAPONS) {
                uint16_t app_id = TECHDATA::weapons[weapon_type].tech_app_id;
                uint16_t field_id = TECHDATA::technology_applications[app_id].tech_field_id;
                uint8_t normalized_cost = (uint8_t)(TECHDATA::technology_fields[field_id].cost / 100);

                int8_t weapon_type_class = TECHDATA::weapons[weapon_type].type;
                if (weapon_type_class == WEAPON_TYPE_BEAM) {
                    if (normalized_cost > best_beam_cost) {
                        best_beam_cost = normalized_cost;
                    }
                } else if (weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_TORPEDO) {
                    if (normalized_cost > best_missile_cost) {
                        best_missile_cost = normalized_cost;
                    }
                }
            }
        }

        if (best_beam_cost != 0 && ai_tech->best_beam_score > best_beam_cost) {
            desirability += (ai_tech->best_beam_score - best_beam_cost) * 8;
        }

        if (best_missile_cost != 0 && ai_tech->best_missile_score > best_missile_cost) {
            desirability += (ai_tech->best_missile_score - best_missile_cost) * 6;
        }

        int size = (int)(int8_t)ship->d.size;
        if (desirability != 0 && size < SHIP_SIZE_DOOM_STAR) {
            const s_ship_design* standard_design = &owner->ship_designs[size];
            // NOTE: This raw compare assumes s_ship_design has a stable packed byte layout.
            if (memcmp(&ship->d, standard_design, sizeof(s_ship_design)) == 0) {
                return 0;
            }
        }

        return desirability;
    }

    const char* Validate_Inputs(const game::s_view* world) {
        if (!world) return "missing world view";
        if (!world->players) return "missing player records";
        if (!world->ships) return "missing ship records";
        if (!world->state) return "missing leader records";
        if (!world->settings) return "missing game settings";
        if (!world->stardate) return "missing stardate";
        if (!world->player_count || *world->player_count <= 0 || *world->player_count > MAX_PLAYERS) return "invalid player count";
        if (!world->ship_count || *world->ship_count < 0 || *world->ship_count > MAX_SHIPS) return "invalid ship count";
        if (world->state->star_order.size() > MAX_STARS) return "invalid star count";
        if (world->state->colonies.size() > MAX_COLONIES) return "invalid colony count";
        if (world->state->planets.size() > MAX_PLANETS) return "invalid planet count";
        return nullptr;
    }

    bool Compute_AI_Data(s_state* ai, const game::s_view* world) {
        if (Validate_Inputs(world) != nullptr) {
            Reset(ai);
            return false;
        }
        RUSS::Assert_Settings();
        Begin_Turn(ai, world->Stardate());

        for (int16_t i = 0; i < world->Ship_Count(); ++i) {
            s_ship_data& ship = world->ships[i];
            if (ship.status < 3) {
                int owner = ship.owner;
                bool player_owned = owner >= 0 && owner < MAX_PLAYERS;
                const auto location = legacy_world::Read_Ship_Location(&ship);
                if (!location) continue;
                std::optional<id_star> star_id;
                int turn_idx = -1;
                if (ship.status == 0) {
                    star_id = location->star_id;
                    turn_idx = 0;
                } else if (ship.status == 1) {
                    star_id = location->star_id;
                    turn_idx = (int)ship.turns_left;
                } else if (ship.status == 2) {
                    star_id = game::Find_Star_At_Coordinates(*world->state, ship.x, ship.y);
                    turn_idx = 0;
                }

                if (star_id && world->state->stars.contains(*star_id)) {
                    auto& star_assessment = ai->stars[*star_id];
                    bool in_nebula = world->state->stars.at(*star_id).in_nebula != 0;
                    if (player_owned && turn_idx < FORECAST_TURN_COUNT && world->players[owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        star_assessment.enroute_human_ships[turn_idx] = 1;
                    }
                    for (int p = 0; p < world->Player_Count(); ++p) {
                        if (owner != p && world->players[p].eliminated == 0) {
                            if (ship.d.ship_type == SHIP_TYPE_COMBAT) {
                                uint16_t str = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships(world, legacy_world::Ship_Id(i), legacy_world::Player_Id(p));
                                ai->ships[legacy_world::Ship_Id(i)].strengths[p] = str;
                                if (player_owned) {
                                    ai->players[owner].empire.strength_vs_player[p] += str;
                                }
                                ai->ships[legacy_world::Ship_Id(i)].strengths_in_nebula[p] = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships_In_Nebula(world, legacy_world::Ship_Id(i), legacy_world::Player_Id(p));
                                AIPOWER::Ship_Strength_Vs_Planetary_Shield(world, legacy_world::Ship_Id(i), ai->ships[legacy_world::Ship_Id(i)].bomb_strengths);
                                if (player_owned && turn_idx < FORECAST_TURN_COUNT) {
                                    uint16_t cur_str = in_nebula ? ai->ships[legacy_world::Ship_Id(i)].strengths_in_nebula[p] : ai->ships[legacy_world::Ship_Id(i)].strengths[p];
                                    star_assessment.enroute_strengths[owner][p][turn_idx] += cur_str;
                                }
                            }
                            if (player_owned) {
                                if (turn_idx < FORECAST_TURN_COUNT) {
                                    star_assessment.enroute_ships[owner][turn_idx] = 1;
                                }
                                if (world->players[owner].traits[TRAIT_STEALTHY_SHIPS] == 0 && star_assessment.enroute_nonstealthy_ships[owner] == 0) {
                                    if (!SHIPSTAK::Ship_Has_Stealth_Device(i)) {
                                        star_assessment.enroute_nonstealthy_ships[owner] = 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        for (int p = 0; p < world->Player_Count(); ++p) {
            memset(world->players[p].total_ship_strength, 0, sizeof(world->players[p].total_ship_strength));
        }

        for (int16_t i = 0; i < world->Ship_Count(); ++i) {
            int owner = world->ships[i].owner;
            if (owner >= 0 && owner < MAX_PLAYERS) {
                for (int p = 0; p < world->Player_Count(); ++p) {
                    world->players[owner].total_ship_strength[p] += ai->ships[legacy_world::Ship_Id(i)].strengths[p];
                }
            }
        }

        for (const auto star_id : world->state->star_order) {
            auto& star_assessment = ai->stars[star_id];
            for (int p_o = 0; p_o < world->Player_Count(); ++p_o) {
                for (int t = 0; t < LAST_FORECAST_TURN; ++t) {
                    for (int p_v = 0; p_v < world->Player_Count(); ++p_v) {
                        star_assessment.enroute_strengths[p_o][p_v][t + 1] += star_assessment.enroute_strengths[p_o][p_v][t];
                        if (star_assessment.enroute_ships[p_o][t]) {
                            star_assessment.enroute_ships[p_o][t + 1] = 1;
                        }
                    }
                    if (star_assessment.enroute_human_ships[t]) {
                        star_assessment.enroute_human_ships[t + 1] = 1;
                    }
                }
            }
        }

        int32_t range_sq[MAX_PLAYERS], ext_range_sq[MAX_PLAYERS];
        for (int p = 0; p < world->Player_Count(); ++p) {
            int32_t r = world->players[p].ship_range;
            range_sq[p] = (r * 30) * (r * 30);
            int32_t ext_r = r * 45;
            ext_range_sq[p] = ext_r * ext_r;
        }

        for (auto& [roster_id, roster_colony] : world->state->colonies) {
            s_colony& col = roster_colony;
            if (col.planet_id != game::NO_PLANET) {
                const auto& home = world->state->stars.at(world->state->planets.at(col.planet_id).star_id);
                const auto sx = home.x;
                const auto sy = home.y;
                for (int16_t s = 0; s < std::ssize(world->state->star_order); ++s) {
                    const auto star_id = world->state->star_order[s];
                    auto& star_record = world->state->stars.at(star_id);
                    int32_t dx = sx - star_record.x;
                    int32_t dy = sy - star_record.y;
                    int32_t d2 = dx * dx + dy * dy;
                    if (d2 <= ext_range_sq[col.owner]) {
                        if (!struct_::Read_Bit_Field(home.black_hole_blocks, s)) {
                            auto& star_assessment = ai->stars[star_id];
                            star_assessment.in_extended_range |= (1 << col.owner);
                            if (d2 <= range_sq[(int)col.owner]) {
                                star_assessment.in_range |= (1 << col.owner);
                            }
                        }
                    }
                }
            }
        }

        for (const auto source_id : world->state->star_order) {
            const auto& source_assessment = ai->stars[source_id];
            for (int p = 0; p < world->Player_Count(); ++p) {
                if ((source_assessment.in_extended_range >> p) & 1) {
                    for (const auto destination_id : world->state->star_order) {
                        if ((ai->stars[destination_id].in_extended_range >> p) & 1) {
                            auto turns = legacy_world::Travel_Turns(
                                SHIPMOVE::Player_N_Turns_From_Star_To_Star(*world, (int16_t)p,
                                    source_id, destination_id));
                            if (turns) ai->routes.direct.Store(source_id, destination_id, legacy_world::Player_Id(p), (uint8_t)*turns);
                        }
                    }
                }
            }
        }

        for (int16_t i = 0; i < world->Ship_Count(); ++i) {
            s_ship_data& ship = world->ships[i];
            if (ship.status < 5 && ship.status == 1) {
                const auto destination = legacy_world::Read_Ship_Location(&ship);
                if (!destination || !world->state->stars.contains(destination->star_id)) continue;
                const auto dest_star = destination->star_id;
                auto turns = legacy_world::Travel_Turns(ship.turns_left);
                if (turns) ai->routes.ship_direct.Store(legacy_world::Ship_Id(i), dest_star, legacy_world::Player_Id(0), (uint8_t)*turns);
                // Reordering a fleet in transit uses the live movement engine.
                if (world->state == &game::current_state && world->ships == MOX::ship && SHIPMOVE::Can_Order_Ship(&ship)) {
                    uint8_t mask = (1 << ship.owner);
                    const std::array<id_ship, 1> move_info{legacy_world::Ship_Id(i)};
                    s_ship_move_info out_info;
                    for (const auto star_id : world->state->star_order) {
                        if ((ai->stars[star_id].in_extended_range & mask) && star_id != dest_star) {
                            ai_compat::Evaluate_Fleet_Move(move_info, star_id, &out_info);
                            auto turns = legacy_world::Travel_Turns(out_info.turns_left);
                            if (turns) ai->routes.ship_direct.Store(legacy_world::Ship_Id(i), star_id, legacy_world::Player_Id(0), (uint8_t)*turns);
                        }
                    }
                }
            }
        }

        for (auto& [roster_id, roster_colony] : world->state->colonies) {
            s_colony& col = roster_colony;
            if (col.owner != -1) {
                s_colony_assessment& ai_col = ai->colonies[roster_id];
                for (int p_idx = 0; p_idx < col.n_pops; ++p_idx) {
                    if (POP::Is_Conquered(col.pop[p_idx])) {
                        ai_col.conquered_population++;
                    }
                }
                ai_col.dominant_race = Colony_Dominant_Player(world->Player_Count(), &col);
                const auto dominant_index = ai_col.dominant_race.id;
                if (dominant_index >= MAX_PLAYERS) continue;
                ai_col.population_limit = (int8_t)COLCALC::Colony_Race_Pop_Limit(&col, dominant_index);
                if (world->players[dominant_index].traits[TRAIT_EATS_MINERALS] == 0 || world->players[col.owner].traits[TRAIT_EATS_MINERALS] == 0) {
                    ai_col.food_buildings_relevant = true;
                }
                if (world->players[dominant_index].traits[TRAIT_ENVIRONMENT_IMMUNE] == 0) {
                    ai_col.pollution_controls_relevant = true;
                }
            }
        }

        for (auto& [roster_id, roster_colony] : world->state->colonies) {
            s_colony& col = roster_colony;
            if (col.owner != -1) {
                int owner = col.owner;
                const auto star_id = world->state->planets.at(col.planet_id).star_id;
                for (int p = world->Player_Count() - 1; p >= 0; --p) {
                    if (p != owner && ai->stars[star_id].enroute_ships[p][LAST_FORECAST_TURN]) {
                        ai->colonies[roster_id].other_player_arrivals = true;
                        break;
                    }
                }
            }
        }

        for (const auto star_id : world->state->star_order) {
            s_star_neighborhood& ai_star = ai->stars[star_id].neighborhood;
            for (int pl_i = 0; pl_i < 5; ++pl_i) {
                if (world->state->stars.at(star_id).orbit_planet_ids[pl_i] != game::NO_PLANET) {
                    ai_star.planet_count++;
                }
            }
            for (int p = 0; p < world->Player_Count(); ++p) {
                if (world->players[p].objectives != PLAYER_OBJECTIVE_HUMAN && world->players[p].eliminated == 0 && ((ai->stars[star_id].in_extended_range >> p) & 1)) {
                    for (int other_p = world->Player_Count() - 1; other_p >= 0; --other_p) {
                        if (other_p != p && ai->stars[star_id].enroute_ships[other_p][LAST_FORECAST_TURN]) {
                            ai_star.per_player[p].other_player_arrivals = true;
                            break;
                        }
                    }
                }
            }
        }

        Compute_Star_Player_Range_Info(ai, world);

        for (int p1 = 0; p1 < world->Player_Count(); ++p1) {
            for (const auto& [star_id, star_record] : world->state->stars) {
                if (game::Has_Player_Colony(star_record, p1)) {
                    ai->players[p1].empire.colonized_star_count++;
                    if ((star_record.blockaded >> p1) & 1) {
                        ai->players[p1].empire.blockaded_star_count++;
                    }
                }
            }
            for (int p2 = 0; p2 < world->Player_Count(); ++p2) {
                if (p1 == p2) continue;
                if (world->players[p1].contact[p2]) {
                    ai->players[p1].empire.contact_count++;
                    ai->players[p1].empire.reachable_player_count++;
                    ai->players[p1].empire.reachable_players[p2] = true;
                } else {
                    bool reachable = false;
                    uint8_t mask = (1 << p1) | (1 << p2);
                    for (const auto star_id : world->state->star_order) {
                        if (ai->stars[star_id].neighborhood.planet_count != 0 && ((ai->stars[star_id].in_extended_range & mask) == mask)) {
                            reachable = true;
                            break;
                        }
                    }
                    if (reachable) {
                        ai->players[p1].empire.reachable_player_count++;
                        ai->players[p1].empire.reachable_players[p2] = true;
                    }
                }
            }
        }

        for (auto& [roster_id, roster_colony] : world->state->colonies) {
            s_colony& col = roster_colony;
            if (col.owner != -1) {
                ai->players[(int)col.owner].empire.colony_count++;
                if ((world->state->stars.at(world->state->planets.at(col.planet_id).star_id).blockaded >> col.owner) & 1) {
                    ai->players[(int)col.owner].empire.blockaded_colony_count++;
                }
            }
        }

        int16_t max_colonies = 0;
        for (int p = 0; p < world->Player_Count(); ++p) {
            if (ai->players[p].empire.colony_count > max_colonies) {
                max_colonies = ai->players[p].empire.colony_count;
            }
        }
        for (int p = 0; p < world->Player_Count(); ++p) {
            ai->players[p].empire.colony_deficit = max_colonies - ai->players[p].empire.colony_count;
        }

        Compute_Base_Planet_Values(ai, world);
        Compute_Contextual_Planet_Values(ai, world);
        ::aitech::Compute_AI_Tech_Info(ai, world);

        for (int16_t i = 0; i < world->Ship_Count(); ++i) {
            s_ship_data* ship = &world->ships[i];
            if (ship->d.ship_type == SHIP_TYPE_COMBAT && ship->owner >= 0 && ship->owner < world->Player_Count() && ship->status < 3 && ship->mission != 4 /* SHIP_MISSION_SCRAP */) {
                ai->combat_ships.push_back(legacy_world::Ship_Id(i));
                if (ship->status == 0) {
                    int32_t d = Refit_Desirability(&world->players[ship->owner], &ai->players[ship->owner].technology, ship);
                    ai->ships[legacy_world::Ship_Id(i)].refit_desirability = d;
                    if (d != 0) {
                        ai->players[ship->owner].refits.push_back({legacy_world::Ship_Id(i), d});
                    }
                }
            }
        }

        for (int p = 0; p < world->Player_Count(); ++p) {
            auto& refits = ai->players[p].refits;
            qsort(refits.data(), refits.size(), sizeof(s_refit_candidate), airefit_cmp);
        }

        Legacy_Sort(std::span{ai->combat_ships}, [&](id_ship lhs, id_ship rhs) {
            return Compare_Combat_Ships(*world, lhs, rhs);
        });

        for (uint32_t i = 0; i < ai->combat_ships.size(); ++i) {
            const uint32_t ship_idx = legacy_world::Slot(ai->combat_ships[i]);
            auto orbit = legacy_world::Orbit_Star(&world->ships[ship_idx]);
            int owner = world->ships[ship_idx].owner;
            if (orbit) {
                auto& groups = ai->players[owner].combat_ship_groups;
                auto group = groups.try_emplace(*orbit, s_combat_ship_group{i, i + 1}).first;
                group->second.end = i + 1;
            } else {
                auto& group = ai->players[owner].in_transit_combat_ships;
                if (!group) group = s_combat_ship_group{i, i + 1};
                group->end = i + 1;
            }
        }

        ai->routes.direct.Prepare_Live_Indices(world->Star_Count(), world->Player_Count());

        for (uint32_t i_as = 0; i_as < ai->combat_ships.size(); ++i_as) {
            const uint32_t i = legacy_world::Slot(ai->combat_ships[i_as]);
            int8_t stealth = (int8_t)struct_::Test_Bit_Field(world->ships[i].d.special_device_flags, 11);
            const auto location = legacy_world::Read_Ship_Location(&world->ships[i]);
            if (!location) continue;
            auto source_star = location->star_id;
            int owner = world->ships[i].owner;
            int initial_delay = 0;

            if (world->ships[i].status == 10) { // SHIP_STATUS_ANTARAN
                initial_delay = 0;
            } else if (location->kind == e_ship_location::at_star) {
                initial_delay = 0;
            } else if (location->kind == e_ship_location::in_flight) {
                initial_delay = (int)world->ships[i].turns_left;
            } else {
                const auto source = game::Find_Star_At_Coordinates(*world->state, world->ships[i].x, world->ships[i].y);
                if (!source) continue;
                source_star = *source;
            }

            const auto source = world->state->stars.find(source_star);
            if (source == world->state->stars.end()) continue;
            bool in_nebula = source->second.in_nebula != 0;

            for (int p = 0; p < world->Player_Count(); ++p) {
                if (p != owner) {
                    for (const auto star_id : world->state->star_order) {
                        auto& star_assessment = ai->stars[star_id];
                        if (((star_assessment.in_extended_range >> p) & 1) || star_id == source_star) {
                            auto route = Player_N_Turns_From_Star_To_Star_With_Intermediate(ai, world, legacy_world::Player_Id(owner), source_star, star_id, stealth);
                            if (route) {
                                int eta = initial_delay + route->turns;
                                if (eta < FORECAST_TURN_COUNT) {
                                    uint16_t str = in_nebula ? ai->ships[legacy_world::Ship_Id(i)].strengths_in_nebula[p] : ai->ships[legacy_world::Ship_Id(i)].strengths[p];
                                    for (int t = eta; t < FORECAST_TURN_COUNT; ++t) {
                                        star_assessment.possible_strengths[owner][p][t] += str;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        for (const auto& [id, body] : world->state->planets) {
            if (body.colony_id != game::NO_COLONY) {
                int owner = world->state->colonies.at(body.colony_id).owner;
                auto& planet = ai->planets[id];
                planet.stage_weights[owner] = planet.base_values[owner];
            }
        }

        uint8_t player_treaty_matrix[MAX_PLAYERS][MAX_PLAYERS];
        memset(player_treaty_matrix, 0, sizeof(player_treaty_matrix));

        for (int i = 0; i < world->Player_Count(); i++) {
            int flag = 0;
            for (int j = 0; j < world->Player_Count(); j++) {
                if (world->players[j].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    player_treaty_matrix[i][j] = 1;
                }
                if (world->players[i].treaty[j] > 3 && world->players[i].treaty[j] < 7) {
                    flag = 1;
                }
            }
            if (!flag) {
                memset(player_treaty_matrix[i], 1, sizeof(player_treaty_matrix[i]));
            }

            player_treaty_matrix[i][i] = 0;
        }

        for (const auto star_id_ : world->state->star_order) {
            auto& star_record = world->state->stars.at(star_id_);
            if (star_record.has_colony) {
                std::array<std::optional<id_planet>, MAX_PLAYERS> best_planet_at_star{};
                std::array<int16_t, MAX_PLAYERS> best_planet_val_at_star{};
                for (const auto& link : star_record.orbit_planet_ids) {
                    const auto id = link;
                    if (id == game::NO_PLANET) continue;
                    const auto& planet = world->state->planets.at(id);
                    if (planet.colony_id == game::NO_COLONY) continue;
                    const auto owner = world->state->colonies.at(planet.colony_id).owner;
                    const auto value = ai->planets[id].stage_weights[owner];
                    if (value > best_planet_val_at_star[owner]) {
                        best_planet_at_star[owner] = id;
                        best_planet_val_at_star[owner] = value;
                    }
                }
                for (int p_o = 0; p_o < world->Player_Count(); ++p_o) {
                    if (best_planet_at_star[p_o]) {
                        int32_t threat_sum = 0;
                        int32_t value_sum = 0;

                        for (int p_v = 0; p_v < world->Player_Count(); ++p_v) {
                            if (player_treaty_matrix[p_o][p_v]) {
                                int16_t max_eta = 8;
                                if (ai->players[p_v].empire.colonized_star_count * 5 < ai->players[p_o].empire.colonized_star_count) {
                                    max_eta = (ai->players[p_v].empire.colonized_star_count <= 1) ? 1 : 4;
                                }
                                for (const auto other_star_id : world->state->star_order) {
                                    int32_t str = ai->stars[other_star_id].possible_strengths[p_v][p_o][0];
                                    if (str != 0) {
                                        auto route = Player_N_Turns_From_Star_To_Star_With_Intermediate(ai, world, legacy_world::Player_Id(p_v), other_star_id, star_id_, true);
                                        if (route && route->turns < max_eta) {
                                            threat_sum += str / (route->turns + 1);
                                        }
                                    }
                                }
                            }
                        }

                        for (const auto& [id, planet] : world->state->planets) {
                            if (id != *best_planet_at_star[p_o]) {
                                auto route = Player_N_Turns_From_Star_To_Star_With_Intermediate(ai, world, legacy_world::Player_Id(p_o), star_id_, planet.star_id, false);
                                if (route && route->turns < 3) {
                                    value_sum += ai->planets[id].stage_weights[p_o];
                                }
                            }
                        }
                        int32_t base_weight = best_planet_val_at_star[p_o] + (value_sum >> 4);
                        ai->stars[star_id_].base_stage_weights[p_o] = base_weight;
                        ai->stars[star_id_].stage_weights[p_o] = ((base_weight + 7) >> 3) * ((threat_sum + 7) >> 3);
                    }
                }
            }
        }

        for (int p = 0; p < world->Player_Count(); ++p) {
            AISTAGE::Compute_Defensive_Stage_Points(ai, world, legacy_world::Player_Id(p));
        }

        for (int p = 0; p < world->Player_Count(); ++p) {
            for (int16_t s = 0; s < std::ssize(world->state->star_order); ++s) {
                const auto star_id = world->state->star_order[s];
                auto& star_record = world->state->stars.at(star_id);
                int8_t target = star_record.surrender_to[p];
                if (target >= MAX_PLAYERS) {
                    int target_p = target - MAX_PLAYERS;
                    star_record.surrender_to[p] = -1;
                    if (target_p >= 0 && target_p < world->Player_Count() &&
                        world->players[p].eliminated == 0 && world->players[target_p].eliminated == 0) {
                        if (COLCALC::May_Surrender_Star(s, (int16_t)p, (int16_t)target_p)) {
                            COLCALC::Surrender_Star(s, (int16_t)p, (int16_t)target_p);
                        }
                    }
                }
            }
        }

        RUSS::Assert_Settings();
        for (auto& [star_id, star] : world->state->stars) {
            star.just_visited = 0;
        }
        return true;
    }

    void __cdecl Compute_Star_Player_Range_Info(s_state* ai, const game::s_view* world) {
        if (world->Star_Count() <= 0) {
            return;
        }

        for (const auto star_id_ : world->state->star_order) {
            s_star_neighborhood& ai_info = ai->stars[star_id_].neighborhood;

            for (auto& [roster_id, roster_colony] : world->state->colonies) {
                s_colony& col = roster_colony;
                int owner = (int)col.owner;

                if (owner != -1) {
                    const auto colony_star = world->state->planets.at(col.planet_id).star_id;
                    int ship_range = (int)world->players[owner].ship_range;

                    if (Star_In_Range_Of_Given_Star(world, star_id_, colony_star, ship_range)) {
                        ai_info.per_player[owner].covering_colony_count++;
                    }
                }
            }

            int8_t in_normal_range[MAX_PLAYERS];
            int8_t in_extended_range[MAX_PLAYERS];
            Find_Players_In_Range(world, star_id_, in_normal_range, in_extended_range);

            for (int player_idx = 0; player_idx < (int)world->Player_Count(); ++player_idx) {
                if ((world->state->stars.at(star_id_).has_colony >> player_idx) & 1) {
                    s_player& plr = world->players[player_idx];
                    s_star_player_assessment& block = ai_info.per_player[player_idx];

                    for (int other_idx = 0; other_idx < (int)world->Player_Count(); ++other_idx) {
                        if (player_idx == other_idx) {
                            continue;
                        }
                        if (world->players[other_idx].eliminated) {
                            continue;
                        }

                        if (in_normal_range[other_idx]) {
                            int treaty = (int)plr.treaty[other_idx];
                            switch (treaty) {
                                case 0:
                                    block.neutral_players_in_range++;
                                    break;
                                case 1:
                                case 2:
                                case 3:
                                    block.treaty_partners_in_range++;
                                    break;
                                case 4:
                                case 5:
                                case 6:
                                    block.hostile_players_in_range++;
                                    break;
                                default:
                                    break;
                            }
                        } else if (in_extended_range[other_idx]) {
                            block.players_only_in_extended_range++;
                        }
                    }
                }
            }
        }
    }

    void Find_Players_In_Range(const game::s_view* world, id_star star_idx, int8_t* in_normal_range_out, int8_t* in_extended_range_out) {
        auto& target_star = world->state->stars.at(star_idx);
        int32_t target_x = target_star.x;
        int32_t target_y = target_star.y;

        memset(in_normal_range_out, 0, MAX_PLAYERS);
        memset(in_extended_range_out, 0, MAX_PLAYERS);

        int32_t normal_range_sq[MAX_PLAYERS];
        int32_t extended_range_sq[MAX_PLAYERS];

        for (int player_idx = 0; player_idx < world->Player_Count(); ++player_idx) {
            int32_t range = world->players[player_idx].ship_range;
            int32_t range_sq = range * range;

            normal_range_sq[player_idx] = range_sq * 900;
            extended_range_sq[player_idx] = range_sq * 2025;
        }

        for (auto& [roster_id, roster_colony] : world->state->colonies) {
            auto& colony = roster_colony;
            int8_t owner = colony.owner;

            if (owner != -1 && in_normal_range_out[owner] == 0) {
                auto& planet = world->state->planets.at(colony.planet_id);
                auto& colony_star = world->state->stars.at(planet.star_id);

                int32_t dx = (int32_t)colony_star.x - target_x;
                int32_t dy = (int32_t)colony_star.y - target_y;
                int32_t dist_sq = dx * dx + dy * dy;

                if (dist_sq <= extended_range_sq[owner]) {
                    in_extended_range_out[owner] = 1;
                    if (dist_sq <= normal_range_sq[owner]) {
                        in_normal_range_out[owner] = 1;
                    }
                }
            }
        }
    }

    int __cdecl airefit_cmp(const void* lhs, const void* rhs) {
        const s_refit_candidate* lhs_entry = (const s_refit_candidate*)lhs;
        const s_refit_candidate* rhs_entry = (const s_refit_candidate*)rhs;

        int32_t lhs_desirability = lhs_entry->desirability;
        int32_t rhs_desirability = rhs_entry->desirability;

        return lhs_desirability - rhs_desirability;
    }

    int Compare_Combat_Ships(const game::s_view& world, id_ship lhs, id_ship rhs) {
        const s_ship_data& ship1 = world.ships[legacy_world::Slot(lhs)];
        const s_ship_data& ship2 = world.ships[legacy_world::Slot(rhs)];

        if (ship1.owner < ship2.owner) {
            return -1;
        }
        if (ship1.owner > ship2.owner) {
            return 1;
        }

        return legacy_world::Compare_Ship_Locations(*world.state, &ship1, &ship2);
    }

    void __cdecl Compute_Base_Planet_Values(s_state* ai, const game::s_view* world) {
        for (auto& [id, planet] : ai->planets) {
            std::fill(std::begin(planet.base_values), std::end(planet.base_values), 0);
        }

        if (world->Player_Count() <= 0) {
            return;
        }

        for (int16_t player_idx = 0; player_idx < world->Player_Count(); ++player_idx) {
            for (auto& [id, record] : world->state->planets) {
                auto* planet = &record;

                uint8_t system_visibility_bits = ai->stars[planet->star_id].in_extended_range;

                if ((system_visibility_bits >> (player_idx & 0x1F)) & 1) {
                    game::id_colony col_idx = planet->colony_id;

                    uint16_t value;
                    if (col_idx == game::NO_COLONY) {
                        value = AIDATA::Uncolonized_Planet_Worth_To_Player(world, planet, &world->players[player_idx]);
                    } else {
                        value = Colony_Worth_To_Player(ai, world, col_idx, &world->state->colonies.at(col_idx), &world->players[player_idx]);
                    }

                    ai->planets[id].base_values[player_idx] = value;
                }
            }
        }
    }

    /**
     * Computes the "contextual" value of planets for each player, adjusting the base
     * values based on ownership of other planets in the same star system.
     */
    void __cdecl Compute_Contextual_Planet_Values(s_state* ai, const game::s_view* world) {
        for (auto& [id, planet] : ai->planets) {
            std::fill(std::begin(planet.contextual_values), std::end(planet.contextual_values), 0);
        }

        if (world->Player_Count() <= 0) {
            return;
        }

        for (int p = 0; p < (int)world->Player_Count(); ++p) {
            for (auto& [id, record] : world->state->planets) {
                uint16_t base_val = ai->planets[id].base_values[p];
                if (base_val <= 0) {
                    continue;
                }

                const auto* planet = &record;
                s_star_data* star = &world->state->stars.at(planet->star_id);

                uint16_t& contextual_val = ai->planets[id].contextual_values[p];
                contextual_val = base_val;

                int uncolonized_system_base_sum = 0;
                int owned_system_base_sum = 0;
                int owned_colonies_in_system = 0;
                int enemy_colonies_in_system = 0;

                // Examine all potential planets in the system (up to 5)
                for (const auto& link : star->orbit_planet_ids) {
                    const auto neighbor_id = link;
                    if (neighbor_id == game::NO_PLANET || neighbor_id == id) continue;
                    const auto colony_idx = world->state->planets.at(neighbor_id).colony_id;

                    if (colony_idx == game::NO_COLONY) {
                        // Uncolonized planet in system
                        uncolonized_system_base_sum += ai->planets[neighbor_id].base_values[p];
                    } else {
                        if (world->state->colonies.at(colony_idx).owner == p) {
                            // Colony owned by the current player
                            owned_colonies_in_system++;
                            owned_system_base_sum += ai->planets[neighbor_id].base_values[p];
                        } else {
                            // Colony owned by an enemy
                            enemy_colonies_in_system++;
                        }
                    }
                }

                if (planet->colony_id == game::NO_COLONY) {
                    // Current planet is not yet colonized
                    if (owned_colonies_in_system > 0) {
                        // Bonus if we already have a presence in the system
                        contextual_val = (int16_t)((contextual_val * (planet->size + 1)) / 10);
                    } else {
                        // Small bonus for general system potential
                        contextual_val += (int16_t)(uncolonized_system_base_sum / 8);
                    }

                    // Penalize value if enemies are present
                    if (enemy_colonies_in_system > 0) {
                        contextual_val = (int16_t)(contextual_val / (enemy_colonies_in_system + 2));
                    }
                } else {
                    // Current planet is already colonized; add value of neighboring potential
                    contextual_val += (int16_t)(uncolonized_system_base_sum / 8);
                    contextual_val += (int16_t)(owned_system_base_sum / 4);
                }
            }
        }
    }




} // namespace AIDATA
