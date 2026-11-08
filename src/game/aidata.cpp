#include "pch.h"
#include "fleet_movement.h"

#include "legacy_world.h"

namespace AIDATA {
    using game_world::e_ship_location;

    int16_t __cdecl Colony_Dominant_Player(int32_t player_count, const s_colony* colony) {
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
            return (int16_t)max_player;
        }

        int16_t current_owner = (int16_t)colony->owner;
        uint8_t owner_count = player_counts[current_owner];

        if (owner_count * 3 <= total_pops && max_count != 0 && owner_count != max_count) {
            return (int16_t)max_player;
        }

        return current_owner;
    }

    bool __cdecl Star_In_Range_Of_Given_Star(const game_world::s_view* world, int16_t target_star_idx, int16_t source_star_idx, int32_t max_range_parsecs) {
        bool in_range = false;

        int32_t distance = GEO::Parsecs_Between_Stars(source_star_idx, target_star_idx);
        if (distance <= max_range_parsecs) {
            return true;
        }

        int16_t wormhole_id = world->stars[source_star_idx].wormhole_star_id;
        if (wormhole_id != -1) {
            distance = GEO::Parsecs_Between_Stars(wormhole_id, target_star_idx);
            if (distance <= max_range_parsecs) {
                in_range = true;
            }
        }

        return in_range;
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

    const char* Validate_Inputs(const game_world::s_view* world) {
        if (!world) return "missing world view";
        if (!world->players) return "missing player records";
        if (!world->ships) return "missing ship records";
        if (!world->stars) return "missing star records";
        if (!world->colonies) return "missing colony records";
        if (!world->planets) return "missing planet records";
        if (!world->leaders) return "missing leader records";
        if (!world->settings) return "missing game settings";
        if (!world->stardate) return "missing stardate";
        if (!world->random_seed) return "missing random seed";
        if (!world->player_count || *world->player_count <= 0 || *world->player_count > MAX_PLAYERS) return "invalid player count";
        if (!world->ship_count || *world->ship_count < 0 || *world->ship_count > MAX_SHIPS) return "invalid ship count";
        if (!world->star_count || *world->star_count < 0 || *world->star_count > MAX_STARS) return "invalid star count";
        if (!world->colony_count || *world->colony_count < 0 || *world->colony_count > MAX_COLONIES) return "invalid colony count";
        if (!world->planet_count || *world->planet_count < 0 || *world->planet_count > MAX_PLANETS) return "invalid planet count";
        return nullptr;
    }

    bool Compute_AI_Data(s_state* ai, const game_world::s_view* world) {
        if (Validate_Inputs(world) != nullptr) {
            Reset(ai);
            return false;
        }
        RUSS::Assert_Settings();
        Begin_Turn(ai, world->Stardate());

        RUSS::Mox_Update();

        for (int16_t i = 0; i < world->Ship_Count(); ++i) {
            s_ship_data& ship = world->ships[i];
            if (ship.status < 3) {
                int owner = ship.owner;
                bool player_owned = owner >= 0 && owner < MAX_PLAYERS;
                const auto location = legacy_world::Read_Ship_Location(&ship);
                if (!location) continue;
                std::optional<uint32_t> star_id;
                int turn_idx = -1;
                if (ship.status == 0) {
                    star_id = location->star_id;
                    turn_idx = 0;
                } else if (ship.status == 1) {
                    star_id = location->star_id;
                    turn_idx = (int)ship.turns_left;
                } else if (ship.status == 2) {
                    star_id = legacy_world::Object_Id(HAROLD::XY_To_Star_Id(ship.x, ship.y));
                    turn_idx = 0;
                }

                if (star_id && *star_id < (uint32_t)world->Star_Count()) {
                    const uint32_t star_idx = *star_id;
                    auto& star_assessment = ai->stars[star_idx];
                    bool in_nebula = world->stars[star_idx].in_nebula != 0;
                    if (player_owned && turn_idx < FORECAST_TURN_COUNT && world->players[owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        star_assessment.enroute_human_ships[turn_idx] = 1;
                    }
                    for (int p = 0; p < world->Player_Count(); ++p) {
                        if (owner != p && world->players[p].eliminated == 0) {
                            if (ship.d.ship_type == SHIP_TYPE_COMBAT) {
                                uint16_t str = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships(world, i, p);
                                ai->ships[i].strengths[p] = str;
                                if (player_owned) {
                                    ai->players[owner].empire.strength_vs_player[p] += str;
                                }
                                ai->ships[i].strengths_in_nebula[p] = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships_In_Nebula(world, i, p);
                                AIPOWER::Ship_Strength_Vs_Planetary_Shield(world, i, ai->ships[i].bomb_strengths);
                                RUSS::Mox_Update();
                                if (player_owned && turn_idx < FORECAST_TURN_COUNT) {
                                    uint16_t cur_str = in_nebula ? ai->ships[i].strengths_in_nebula[p] : ai->ships[i].strengths[p];
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
                    world->players[owner].total_ship_strength[p] += ai->ships[i].strengths[p];
                }
            }
        }

        RUSS::Mox_Update();
        for (int16_t s = 0; s < world->Star_Count(); ++s) {
            auto& star_assessment = ai->stars[s];
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

        RUSS::Mox_Update();
        int32_t range_sq[MAX_PLAYERS], ext_range_sq[MAX_PLAYERS];
        for (int p = 0; p < world->Player_Count(); ++p) {
            int32_t r = world->players[p].ship_range;
            range_sq[p] = (r * 30) * (r * 30);
            int32_t ext_r = r * 45;
            ext_range_sq[p] = ext_r * ext_r;
        }

        for (int16_t c = 0; c < world->Colony_Count(); ++c) {
            s_colony& col = world->colonies[c];
            if (col.planet != -1) {
                int16_t star_idx = world->planets[col.planet].star_index;
                int16_t sx = world->stars[star_idx].x;
                int16_t sy = world->stars[star_idx].y;
                for (int16_t s = 0; s < world->Star_Count(); ++s) {
                    int32_t dx = sx - world->stars[s].x;
                    int32_t dy = sy - world->stars[s].y;
                    int32_t d2 = dx * dx + dy * dy;
                    if (d2 <= ext_range_sq[col.owner]) {
                        if (!struct_::Test_Bit_Field(world->stars[star_idx].black_hole_blocks, s)) {
                            auto& star_assessment = ai->stars[s];
                            star_assessment.in_extended_range |= (1 << col.owner);
                            if (d2 <= range_sq[(int)col.owner] && !struct_::Test_Bit_Field(world->stars[star_idx].black_hole_blocks, s)) {
                                star_assessment.in_range |= (1 << col.owner);
                            }
                        }
                    }
                }
            }
        }

        RUSS::Mox_Update();
        for (int16_t s1 = 0; s1 < world->Star_Count(); ++s1) {
            const auto& source_assessment = ai->stars[s1];
            for (int p = 0; p < world->Player_Count(); ++p) {
                if ((source_assessment.in_extended_range >> p) & 1) {
                    for (int16_t s2 = 0; s2 < world->Star_Count(); ++s2) {
                        if ((ai->stars[s2].in_extended_range >> p) & 1) {
                            auto turns = legacy_world::Travel_Turns(
                                SHIPMOVE::Player_N_Turns_From_Star_To_Star((int16_t)p, s1, s2));
                            if (turns) ai->routes.direct.Store(s1, s2, p, (uint8_t)*turns);
                            RUSS::Mox_Update();
                        }
                    }
                }
            }
        }

        RUSS::Mox_Update();
        for (int16_t i = 0; i < world->Ship_Count(); ++i) {
            s_ship_data& ship = world->ships[i];
            if (ship.status < 5 && ship.status == 1) {
                int16_t dest_star = SHIPMOVE::Ship_Destination_Star(&ship);
                auto turns = legacy_world::Travel_Turns(ship.turns_left);
                if (turns) ai->routes.ship_direct.Store(i, dest_star, 0, (uint8_t)*turns);
                if (SHIPMOVE::Can_Order_Ship(&ship)) {
                    uint8_t mask = (1 << ship.owner);
                    const std::array<uint32_t, 1> move_info{(uint32_t)i};
                    s_ship_move_info out_info;
                    for (int16_t s = 0; s < world->Star_Count(); ++s) {
                        if ((ai->stars[s].in_extended_range & mask) && s != dest_star) {
                            SHIPMOVE::Evaluate_Fleet_Move(move_info, s, &out_info);
                            auto turns = legacy_world::Travel_Turns(out_info.turns_left);
                            if (turns) ai->routes.ship_direct.Store(i, s, 0, (uint8_t)*turns);
                            RUSS::Mox_Update();
                        }
                    }
                }
            }
        }

        RUSS::Mox_Update();
        for (int16_t c = 0; c < world->Colony_Count(); ++c) {
            s_colony& col = world->colonies[c];
            if (col.owner != -1) {
                s_colony_assessment& ai_col = ai->colonies[c];
                RUSS::Mox_Update();
                for (int p_idx = 0; p_idx < col.n_pops; ++p_idx) {
                    if (POP::Is_Conquered(col.pop[p_idx])) {
                        ai_col.conquered_population++;
                    }
                }
                ai_col.dominant_race = (int8_t)Colony_Dominant_Player(world->Player_Count(), &col);
                ai_col.population_limit = (int8_t)COLCALC::Colony_Race_Pop_Limit(&col, (int16_t)ai_col.dominant_race);
                if (world->players[ai_col.dominant_race].traits[TRAIT_EATS_MINERALS] == 0 || world->players[col.owner].traits[TRAIT_EATS_MINERALS] == 0) {
                    ai_col.food_buildings_relevant = true;
                }
                if (world->players[(int)ai_col.dominant_race].traits[TRAIT_ENVIRONMENT_IMMUNE] == 0) {
                    ai_col.pollution_controls_relevant = true;
                }
            }
        }

        for (int16_t c = 0; c < world->Colony_Count(); ++c) {
            s_colony& col = world->colonies[c];
            if (col.owner != -1) {
                int owner = col.owner;
                int16_t star_idx = world->planets[col.planet].star_index;
                RUSS::Mox_Update();
                for (int p = world->Player_Count() - 1; p >= 0; --p) {
                    if (p != owner && ai->stars[star_idx].enroute_ships[p][LAST_FORECAST_TURN]) {
                        ai->colonies[c].other_player_arrivals = true;
                        break;
                    }
                }
            }
        }

        RUSS::Mox_Update();
        for (int16_t s = 0; s < world->Star_Count(); ++s) {
            s_star_neighborhood& ai_star = ai->stars[s].neighborhood;
            for (int pl_i = 0; pl_i < 5; ++pl_i) {
                if (world->stars[s].planet_index[pl_i] != -1) {
                    ai_star.planet_count++;
                }
            }
            for (int p = 0; p < world->Player_Count(); ++p) {
                if (world->players[p].objectives != PLAYER_OBJECTIVE_HUMAN && world->players[p].eliminated == 0 && ((ai->stars[s].in_extended_range >> p) & 1)) {
                    for (int other_p = world->Player_Count() - 1; other_p >= 0; --other_p) {
                        if (other_p != p && ai->stars[s].enroute_ships[other_p][LAST_FORECAST_TURN]) {
                            ai_star.per_player[p].other_player_arrivals = true;
                            break;
                        }
                    }
                }
            }
        }

        Compute_Star_Player_Range_Info(ai, world);

        RUSS::Mox_Update();
        for (int p1 = 0; p1 < world->Player_Count(); ++p1) {
            RUSS::Mox_Update();
            for (int16_t s = 0; s < world->Star_Count(); ++s) {
                if ((world->stars[s].has_colony >> p1) & 1) {
                    ai->players[p1].empire.colonized_star_count++;
                    if ((world->stars[s].blockaded >> p1) & 1) {
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
                    for (int16_t s = 0; s < world->Star_Count(); ++s) {
                        if (ai->stars[s].neighborhood.planet_count != 0 && ((ai->stars[s].in_extended_range & mask) == mask)) {
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

        RUSS::Mox_Update();
        for (int16_t c = 0; c < world->Colony_Count(); ++c) {
            s_colony& col = world->colonies[c];
            if (col.owner != -1) {
                ai->players[(int)col.owner].empire.colony_count++;
                if ((world->stars[world->planets[col.planet].star_index].blockaded >> col.owner) & 1) {
                    ai->players[(int)col.owner].empire.blockaded_colony_count++;
                }
            }
        }

        RUSS::Mox_Update();
        int16_t max_colonies = 0;
        for (int p = 0; p < world->Player_Count(); ++p) {
            if (ai->players[p].empire.colony_count > max_colonies) {
                max_colonies = ai->players[p].empire.colony_count;
            }
        }
        for (int p = 0; p < world->Player_Count(); ++p) {
            ai->players[p].empire.colony_deficit = max_colonies - ai->players[p].empire.colony_count;
        }

        RUSS::Mox_Update();
        Compute_Base_Planet_Values(ai, world);
        RUSS::Mox_Update();
        Compute_Contextual_Planet_Values(ai, world);
        RUSS::Mox_Update();
        ::aitech::Compute_AI_Tech_Info(ai, world);
        RUSS::Mox_Update();

        for (int16_t i = 0; i < world->Ship_Count(); ++i) {
            s_ship_data* ship = &world->ships[i];
            if (ship->d.ship_type == SHIP_TYPE_COMBAT && ship->owner >= 0 && ship->owner < world->Player_Count() && ship->status < 3 && ship->mission != 4 /* SHIP_MISSION_SCRAP */) {
                ai->combat_ships.push_back(i);
                if (ship->status == 0) {
                    RUSS::Mox_Update();
                    int32_t d = Refit_Desirability(&world->players[ship->owner], &ai->players[ship->owner].technology, ship);
                    ai->ships[i].refit_desirability = d;
                    if (d != 0) {
                        ai->players[ship->owner].refits.push_back({(uint32_t)i, d});
                    }
                }
            }
        }

        for (int p = 0; p < world->Player_Count(); ++p) {
            RUSS::Mox_Update();
            auto& refits = ai->players[p].refits;
            qsort(refits.data(), refits.size(), sizeof(s_refit_candidate), airefit_cmp);
        }

        RUSS::Mox_Update();
        qsort(ai->combat_ships.data(), ai->combat_ships.size(), sizeof(uint32_t), sort_ships, world->ships);

        for (uint32_t i = 0; i < ai->combat_ships.size(); ++i) {
            uint32_t ship_idx = ai->combat_ships[i];
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

        RUSS::Mox_Update();

        ai->routes.direct.Prepare_Live_Indices(world->Star_Count(), world->Player_Count());

        for (uint32_t i_as = 0; i_as < ai->combat_ships.size(); ++i_as) {
            uint32_t i = ai->combat_ships[i_as];
            int8_t stealth = (int8_t)struct_::Test_Bit_Field(world->ships[i].d.special_device_flags, 11);
            const auto location = legacy_world::Read_Ship_Location(&world->ships[i]);
            if (!location) continue;
            uint32_t start_star = location->star_id;
            int owner = world->ships[i].owner;
            int initial_delay = 0;

            RUSS::Mox_Update();

            if (world->ships[i].status == 10) { // SHIP_STATUS_ANTARAN
                initial_delay = 0;
            } else if (location->kind == e_ship_location::at_star) {
                initial_delay = 0;
            } else if (location->kind == e_ship_location::in_flight) {
                initial_delay = (int)world->ships[i].turns_left;
            } else {
                auto source = legacy_world::Object_Id(HAROLD::XY_To_Star_Id(world->ships[i].x, world->ships[i].y));
                if (!source) continue;
                start_star = *source;
            }

            if (start_star >= (uint32_t)world->Star_Count()) continue;

            bool in_nebula = world->stars[start_star].in_nebula != 0;

            for (int p = 0; p < world->Player_Count(); ++p) {
                if (p != owner) {
                    for (int16_t s = 0; s < world->Star_Count(); ++s) {
                        auto& star_assessment = ai->stars[s];
                        if (((star_assessment.in_extended_range >> p) & 1) || s == start_star) {
                            auto route = Player_N_Turns_From_Star_To_Star_With_Intermediate(ai, world, owner, start_star, s, stealth);
                            if (route) {
                                int eta = initial_delay + route->turns;
                                if (eta < FORECAST_TURN_COUNT) {
                                    uint16_t str = in_nebula ? ai->ships[i].strengths_in_nebula[p] : ai->ships[i].strengths[p];
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

        RUSS::Mox_Update();
        for (int16_t pl = 0; pl < world->Planet_Count(); ++pl) {
            if (world->planets[pl].colony_index != -1) {
                int owner = world->colonies[world->planets[pl].colony_index].owner;
                ai->planets[pl].stage_weights[owner] = ai->planets[pl].base_values[owner];
            }
        }

        RUSS::Mox_Update();

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

        for (int16_t s = 0; s < world->Star_Count(); ++s) {
            if (world->stars[s].has_colony) {
                std::array<std::optional<uint32_t>, MAX_PLAYERS> best_planet_at_star{};
                std::array<int16_t, MAX_PLAYERS> best_planet_val_at_star{};
                RUSS::Mox_Update();
                for (int pl_i = 0; pl_i < 5; ++pl_i) {
                    int16_t pl_idx = world->stars[s].planet_index[pl_i];
                    if (pl_idx != -1 && world->planets[pl_idx].colony_index != -1) {
                        int owner = world->colonies[world->planets[pl_idx].colony_index].owner;
                        int16_t val = ai->planets[pl_idx].stage_weights[owner];
                        if (val > best_planet_val_at_star[owner]) {
                            best_planet_at_star[owner] = pl_idx;
                            best_planet_val_at_star[owner] = val;
                        }
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
                                RUSS::Mox_Update();
                                for (int16_t other_s = 0; other_s < world->Star_Count(); ++other_s) {
                                    int32_t str = ai->stars[other_s].possible_strengths[p_v][p_o][0];
                                    if (str != 0) {
                                        auto route = Player_N_Turns_From_Star_To_Star_With_Intermediate(ai, world, p_v, other_s, s, true);
                                        if (route && route->turns < max_eta) {
                                            threat_sum += str / (route->turns + 1);
                                        }
                                    }
                                }
                            }
                        }

                        for (int16_t pl = 0; pl < world->Planet_Count(); ++pl) {
                            if ((uint32_t)pl != *best_planet_at_star[p_o]) {
                                auto route = Player_N_Turns_From_Star_To_Star_With_Intermediate(ai, world, p_o, s, world->planets[pl].star_index, false);
                                if (route && route->turns < 3) {
                                    RUSS::Mox_Update();
                                    value_sum += ai->planets[pl].stage_weights[p_o];
                                }
                            }
                        }
                        int32_t base_weight = best_planet_val_at_star[p_o] + (value_sum >> 4);
                        ai->stars[s].base_stage_weights[p_o] = base_weight;
                        ai->stars[s].stage_weights[p_o] = ((base_weight + 7) >> 3) * ((threat_sum + 7) >> 3);
                    }
                }
            }
        }

        RUSS::Mox_Update();
        for (int p = 0; p < world->Player_Count(); ++p) {
            AISTAGE::Compute_Defensive_Stage_Points(ai, world, (int16_t)p);
        }

        for (int p = 0; p < world->Player_Count(); ++p) {
            for (int16_t s = 0; s < world->Star_Count(); ++s) {
                int8_t target = world->stars[s].surrender_to[p];
                if (target >= MAX_PLAYERS) {
                    int target_p = target - MAX_PLAYERS;
                    world->stars[s].surrender_to[p] = -1;
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
        for (int16_t s = 0; s < world->Star_Count(); ++s) {
            world->stars[s].just_visited = 0;
        }
        return true;
    }

    void __cdecl Compute_Star_Player_Range_Info(s_state* ai, const game_world::s_view* world) {
        if (world->Star_Count() <= 0) {
            return;
        }

        for (int16_t star_idx = 0; star_idx < world->Star_Count(); ++star_idx) {
            s_star_neighborhood& ai_info = ai->stars[star_idx].neighborhood;

            for (int16_t colony_idx = 0; colony_idx < world->Colony_Count(); ++colony_idx) {
                s_colony& col = world->colonies[colony_idx];
                int owner = (int)col.owner;

                if (owner != -1) {
                    int16_t col_star_idx = world->planets[col.planet].star_index;
                    int ship_range = (int)world->players[owner].ship_range;

                    if (Star_In_Range_Of_Given_Star(world, star_idx, col_star_idx, ship_range)) {
                        ai_info.per_player[owner].covering_colony_count++;
                    }
                }
            }

            int8_t in_normal_range[MAX_PLAYERS];
            int8_t in_extended_range[MAX_PLAYERS];
            Find_Players_In_Range(world, star_idx, in_normal_range, in_extended_range);

            for (int player_idx = 0; player_idx < (int)world->Player_Count(); ++player_idx) {
                if ((world->stars[star_idx].has_colony >> player_idx) & 1) {
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

    void __cdecl Find_Players_In_Range(const game_world::s_view* world, int16_t star_idx, int8_t* in_normal_range_out, int8_t* in_extended_range_out) {
        auto& target_star = world->stars[star_idx];
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

        for (int16_t colony_idx = 0; colony_idx < world->Colony_Count(); ++colony_idx) {
            auto& colony = world->colonies[colony_idx];
            int8_t owner = colony.owner;

            if (owner != -1 && in_normal_range_out[owner] == 0) {
                auto& planet = world->planets[colony.planet];
                auto& colony_star = world->stars[planet.star_index];

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

    int __cdecl sort_ships(void* context, const void* lhs, const void* rhs) {
        uint32_t idx1 = *(const uint32_t*)lhs;
        uint32_t idx2 = *(const uint32_t*)rhs;
        const auto* ships = static_cast<const s_ship_data*>(context);

        const s_ship_data& ship1 = ships[idx1];
        const s_ship_data& ship2 = ships[idx2];

        if (ship1.owner < ship2.owner) {
            return -1;
        }
        if (ship1.owner > ship2.owner) {
            return 1;
        }

        return legacy_world::Compare_Ship_Locations(&ship1, &ship2);
    }

    void __cdecl Compute_Base_Planet_Values(s_state* ai, const game_world::s_view* world) {
        for (auto& [id, planet] : ai->planets) {
            std::fill(std::begin(planet.base_values), std::end(planet.base_values), 0);
        }

        if (world->Player_Count() <= 0) {
            return;
        }

        for (int16_t player_idx = 0; player_idx < world->Player_Count(); ++player_idx) {
            for (int16_t planet_idx = 0; planet_idx < world->Planet_Count(); ++planet_idx) {
                s_planet_data* planet = &world->planets[planet_idx];

                uint8_t system_visibility_bits = ai->stars[planet->star_index].in_extended_range;

                if ((system_visibility_bits >> (player_idx & 0x1F)) & 1) {
                    int16_t col_idx = planet->colony_index;

                    RUSS::Mox_Update();

                    uint16_t value;
                    if (col_idx == -1) {
                        value = AIDATA::Uncolonized_Planet_Worth_To_Player(world, planet, &world->players[player_idx]);
                    } else {
                        value = Colony_Worth_To_Player(ai, world, (uint32_t)col_idx, &world->colonies[col_idx], &world->players[player_idx]);
                    }

                    ai->planets[planet_idx].base_values[player_idx] = value;
                }
            }
        }
    }

    /**
     * Computes the "contextual" value of planets for each player, adjusting the base
     * values based on ownership of other planets in the same star system.
     */
    void __cdecl Compute_Contextual_Planet_Values(s_state* ai, const game_world::s_view* world) {
        for (auto& [id, planet] : ai->planets) {
            std::fill(std::begin(planet.contextual_values), std::end(planet.contextual_values), 0);
        }

        if (world->Player_Count() <= 0) {
            return;
        }

        for (int p = 0; p < (int)world->Player_Count(); ++p) {
            for (int16_t i = 0; i < world->Planet_Count(); ++i) {
                uint16_t base_val = ai->planets[i].base_values[p];
                if (base_val <= 0) {
                    continue;
                }

                s_planet_data* planet = &world->planets[i];
                s_star_data* star = &world->stars[planet->star_index];

                // Refresh UI/input during processing
                RUSS::Mox_Update();

                uint16_t& contextual_val = ai->planets[i].contextual_values[p];
                contextual_val = base_val;

                int uncolonized_system_base_sum = 0;
                int owned_system_base_sum = 0;
                int owned_colonies_in_system = 0;
                int enemy_colonies_in_system = 0;

                // Examine all potential planets in the system (up to 5)
                for (int k = 0; k < 5; ++k) {
                    int16_t sys_planet_idx = star->planet_index[k];

                    if (sys_planet_idx == -1 || sys_planet_idx == i) {
                        continue;
                    }

                    int16_t colony_idx = world->planets[sys_planet_idx].colony_index;

                    if (colony_idx == -1) {
                        // Uncolonized planet in system
                        uncolonized_system_base_sum += ai->planets[sys_planet_idx].base_values[p];
                    } else {
                        if (world->colonies[colony_idx].owner == p) {
                            // Colony owned by the current player
                            owned_colonies_in_system++;
                            owned_system_base_sum += ai->planets[sys_planet_idx].base_values[p];
                        } else {
                            // Colony owned by an enemy
                            enemy_colonies_in_system++;
                        }
                    }
                }

                if (planet->colony_index == -1) {
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

    /**
     * Calculates the "proximity worth" value of a colony for a specific player.
     * This logic accounts for difficulty settings, alien management centers,
     * and strategic range to existing colonies.
     */
    int16_t __cdecl Proximity_Worth_To_Player(s_state* ai, const game_world::s_view* world, s_colony* colony, s_player* evaluating_player) {
        if (world->settings->game_difficulty == GAME_DIFFICULTY_TUTOR) {
            return 0;
        }

        int8_t owner_idx = colony->owner;
        s_player* owner_ptr = &world->players[owner_idx];

        // Check for Alien Management Center tech
        if (owner_ptr->tech_applications[TECH_APP_ALIEN_MANAGEMENT_CENTER] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return 0;
        }

        int16_t ship_range = owner_ptr->ship_range;
        int16_t colony_star_idx = world->planets[colony->planet].star_index;
        int16_t evaluating_player_idx = (int16_t)(evaluating_player - world->players);

        int16_t total_worth = 0;

        if (evaluating_player_idx == owner_idx) {
            for (int16_t i = 0; i < world->Star_Count(); ++i) {
                if (world->stars[i].spectral_class != STAR_CLASS_BLACK_HOLE) {
                    if (Star_In_Range_Of_Given_Star(world, i, colony_star_idx, ship_range)) {
                        uint8_t has_colony_mask = world->stars[i].has_colony;
                        bool other_players_present = (has_colony_mask & ~(1 << owner_idx)) != 0;

                        // Score is 120 if others are present, 80 otherwise
                        int score = other_players_present ? 120 : 80;
                        auto assessment = ai->stars.find(i);
                        if (assessment == ai->stars.end()) continue;
                        uint32_t covering_colonies = assessment->second.neighborhood.per_player[owner_idx].covering_colony_count;
                        if (covering_colonies == 0) continue;
                        total_worth += (short)(score / covering_colonies);
                    }
                }
            }
        } else {
            uint32_t min_eta = 10000;
            for (int16_t i = 0; i < world->Star_Count(); ++i) {
                if (Star_In_Range_Of_Given_Star(world, i, colony_star_idx, ship_range)) {
                    // Check if target player has a colony at star i
                    if (world->stars[i].has_colony & (1 << evaluating_player_idx)) {
                        auto assessment = ai->stars.find(i);
                        if (assessment == ai->stars.end()) continue;
                        uint32_t covering_colonies = assessment->second.neighborhood.per_player[owner_idx].covering_colony_count;
                        if (covering_colonies == 0) continue;
                        total_worth += (short)(100 / covering_colonies);
                        if ((uint32_t)covering_colonies < min_eta) {
                            min_eta = covering_colonies;
                        }
                    }
                }
            }

            if (total_worth != 0) {
                total_worth += (short)(400 / min_eta);
            }
        }

        return total_worth;
    }

    // Preserve the existing travel-search budget independently of result availability.
    constexpr uint16_t MAX_CACHED_TRAVEL_TURNS = 254;

    std::optional<s_route> Player_N_Turns_From_Star_To_Star_With_Intermediate(
        s_state* ai, const game_world::s_view* world, uint32_t player_id, uint32_t source_star, uint32_t destination_star, bool use_extended_range) {
        const s_route_key key{player_id, source_star, destination_star};
        auto cached = ai->routes.intermediate.find(key);
        if (cached != ai->routes.intermediate.end()) {
            return use_extended_range ? cached->second.extended : cached->second.normal;
        }

        s_route_choices choices;
        const s_player* player = &world->players[player_id];
        const auto& direct_routes = ai->routes.direct;
        const auto source_row = direct_routes.Find_Row_Index(source_star, player_id);
        const auto destination_offset = direct_routes.Find_Destination(destination_star);
        const auto direct = direct_routes.Find_Cell(source_row, destination_offset);
        bool direct_is_best = false;
        if (player->ship_speed != 0 &&
            player->tech_applications[TECH_APP_JUMP_GATE] != TECH_RESEARCH_STATUS_RESEARCHED &&
            player->tech_applications[TECH_APP_STAR_GATE] != TECH_RESEARCH_STATUS_RESEARCHED &&
            world->stars[source_star].wormhole_star_id == -1 &&
            world->stars[destination_star].wormhole_star_id == -1 && direct) {
            auto source_index = legacy_world::Index(source_star);
            auto destination_index = legacy_world::Index(destination_star);
            if (source_index && destination_index) {
                const int32_t distance = GEO::Parsecs_Between_Stars(*source_index, *destination_index);
                const uint16_t turns = (uint8_t)((distance + player->ship_speed - 1) / player->ship_speed);
                if (*direct <= (distance + player->ship_speed - 1) / player->ship_speed) {
                    const auto destination_assessment = ai->stars.find(destination_star);
                    if (destination_assessment != ai->stars.end() && ((destination_assessment->second.in_range >> player_id) & 1)) {
                        choices.normal = s_route{destination_star, turns};
                    }
                    choices.extended = s_route{destination_star, turns};
                    direct_is_best = true;
                }
            }
        }

        if (!direct_is_best) {
            for (int32_t index = world->Star_Count() - 1; index >= 0; --index) {
                const uint32_t star_id = (uint32_t)index;
                const auto assessment = ai->stars.find(star_id);
                if (assessment == ai->stars.end()) continue;
                const auto& star_assessment = assessment->second;
                if ((star_assessment.in_extended_range >> player_id) & 1) {
                    const s_star_data* star = &world->stars[star_id];
                    if (star_id == destination_star ||
                        (star->spectral_class != STAR_CLASS_BLACK_HOLE &&
                            star->system_special != SYSTEM_SPECIAL_SPACE_MONSTER &&
                            (((star->has_colony >> player_id) & 1) || star->has_colony == 0))) {
                        const auto intermediate_offset = direct_routes.Find_Destination(star_id);
                        const auto intermediate_row = direct_routes.Find_Row_Index(star_id, player_id);
                        const auto first_leg = direct_routes.Find_Cell(source_row, intermediate_offset);
                        const auto second_leg = direct_routes.Find_Cell(intermediate_row, destination_offset);
                        if (first_leg && second_leg) {
                            const uint16_t turns = *first_leg + *second_leg;
                            const uint32_t next_star = star_id == source_star ? destination_star : star_id;
                            if (turns <= MAX_CACHED_TRAVEL_TURNS) {
                                if (((star_assessment.in_range >> player_id) & 1) &&
                                    (!choices.normal || turns < choices.normal->turns)) {
                                    choices.normal = s_route{next_star, turns};
                                }
                                if (!choices.extended || turns < choices.extended->turns) {
                                    choices.extended = s_route{next_star, turns};
                                }
                            }
                        }
                    }
                }
                if (choices.normal && choices.normal->turns <= 1) break;
            }
        }
        ai->routes.intermediate.emplace(key, choices);
        return use_extended_range ? choices.extended : choices.normal;
    }

    std::optional<s_route> Ship_N_Turns_To_Star_With_Intermediate(s_state* ai, const game_world::s_view* world, uint32_t ship_id, uint32_t destination_star) {
        const uint64_t key = Ship_Route_Key(ship_id, destination_star);
        auto cached = ai->routes.ship_intermediate.find(key);
        if (cached != ai->routes.ship_intermediate.end()) return cached->second;

        std::optional<s_route> result;
        const s_ship_data* ship = &world->ships[ship_id];
        const bool has_stargate = struct_::Test_Bit_Field(world->ships[ship_id].d.special_device_flags, 11);
        const auto location = legacy_world::Read_Ship_Location(ship);
        if (location && location->kind == e_ship_location::in_flight) {
            const uint32_t destination = location->star_id;
            const uint32_t owner = (uint32_t)ship->owner;
            for (int32_t index = world->Star_Count() - 1; index >= 0; --index) {
                const uint32_t star_id = (uint32_t)index;
                const auto& star_assessment = ai->stars[star_id];
                const bool in_range = (star_assessment.in_range >> owner) & 1;
                const bool in_extended_range = (star_assessment.in_extended_range >> owner) & 1;
                const auto* star = &world->stars[star_id];
                if ((in_range || (in_extended_range && has_stargate)) &&
                    (star_id == destination_star || star_id == destination ||
                        (star->spectral_class != STAR_CLASS_BLACK_HOLE &&
                            star->system_special != SYSTEM_SPECIAL_SPACE_MONSTER &&
                            (((star->has_colony >> owner) & 1) || star->has_colony == 0)))) {
                    if (!GEO::Black_Hole_Blocks_Points(ship->x, ship->y, star->x, star->y)) {
                        const auto first_leg = Ship_N_Turns_To(&ai->routes, ship_id, star_id);
                        const auto second_leg = N_Turns_From_To(&ai->routes, star_id, destination_star, owner);
                        if (first_leg && second_leg) {
                            const uint16_t turns = *first_leg + *second_leg;
                            if (turns <= MAX_CACHED_TRAVEL_TURNS && (!result || turns < result->turns)) {
                                result = s_route{star_id, turns};
                            }
                        }
                    }
                }
                if (result && result->turns <= 1) break;
            }
        } else {
            auto source = location && location->kind == e_ship_location::in_wormhole
                ? legacy_world::Object_Id(HAROLD::XY_To_Star_Id(ship->x, ship->y))
                : location ? std::optional<uint32_t>{location->star_id} : std::nullopt;
            if (source) {
                result = Player_N_Turns_From_Star_To_Star_With_Intermediate(
                    ai, world, ship->owner, *source, destination_star, has_stargate);
            }
        }
        ai->routes.ship_intermediate.emplace(key, result);
        return result;
    }

    int16_t __cdecl Colony_Worth_To_Player(s_state* ai, const game_world::s_view* world, uint32_t colony_idx, s_colony* colony, s_player* evaluating_player) {
        int16_t evaluating_player_idx = (int16_t)(evaluating_player - world->players);
        s_planet_data* planet = &world->planets[colony->planet];
        int16_t colony_race = (int16_t)ai->colonies[colony_idx].dominant_race;
        s_player* race_player = &world->players[colony_race];

        int16_t max_pop_race = COLCALC::Planet_Max_Population_For_Player(planet, colony_race);
        int16_t max_pop_target = COLCALC::Planet_Max_Population_For_Player(planet, evaluating_player_idx);
        int avg_sum = max_pop_target + max_pop_race + 1;
        int avg_max_pop = (avg_sum - (avg_sum >> 31)) >> 1;

        if (colony->outpost_flag != 0) {
            return AIDATA::Proximity_Worth_To_Player(ai, world, colony, evaluating_player);
        }

        int food_base = COLCALC::Colony_Empire_Base_Food2_Produced(colony, evaluating_player_idx, colony_race, nullptr);
        int food = (food_base - (food_base >> 31)) >> 1;
        int industry = COLCALC::Colony_Empire_Base_Industry_Produced(colony, (uint16_t)evaluating_player_idx, colony_race, nullptr);
        int research = COLCALC::Colony_Empire_Base_Research_Produced(colony, (int16_t)evaluating_player_idx, colony_race, nullptr);

        int prod_mult;
        if (race_player->traits[TRAIT_EATS_MINERALS] != 0) {
            prod_mult = (industry + research) * 6;
        } else if (race_player->traits[TRAIT_CYBERNETIC] != 0) {
            prod_mult = (industry + research + food_base / 2) * 4;
        } else {
            prod_mult = (research + food * 2 + industry) * 3;
        }

        int growth_trait = evaluating_player->traits[TRAIT_POP_GROWTH_BONUS];
        const auto& growth_options = raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH];
        int growth_mult;
        if (growth_trait == growth_options[raceopt::PRIMARY_CHOICE_PENALTY].value) {
            growth_mult = (colony->n_pops * 19 + avg_max_pop) * 5;
        } else if (growth_trait == 0) {
            growth_mult = (colony->n_pops * 9 + avg_max_pop) * 10;
        } else if (growth_trait == growth_options[raceopt::PRIMARY_CHOICE_BONUS].value) {
            growth_mult = colony->n_pops * 85 + avg_max_pop * 15;
        } else if (growth_trait == growth_options[raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
            growth_mult = (avg_max_pop + colony->n_pops * 4) * 20;
        } else {
            growth_mult = food;
        }

        int worth = (avg_max_pop - colony->n_pops + 100) * growth_mult * prod_mult;
        worth /= 100;

        bool is_swamp_or_ocean = (planet->climate == PLANET_CLIMATE_OCEAN || planet->climate == PLANET_CLIMATE_SWAMP);
        if (!is_swamp_or_ocean || race_player->traits[TRAIT_AQUATIC] == 0) {
            int climate_modifier = COLCALC::climate_maintenance_modifiers[(int)planet->climate] >> 2;
            worth = worth * (100 - climate_modifier) / 100;

            if (is_swamp_or_ocean) {
                int adjusted_worth = worth * 3;
                worth = (adjusted_worth + ((uint32_t)adjusted_worth >> 31 & 3)) >> 2;
            }
        }

        if (evaluating_player_idx != colony->owner || colony->buildings[BUILDING_GRAVITY_GENERATOR] == 0) {
            int grav_gen_bonus = (evaluating_player->tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] == TECH_RESEARCH_STATUS_RESEARCHED) ? 1 : 0;
            int gravity = planet->gravity_class;

            if (gravity == PLANET_GRAVITY_LOW_G) {
                if (race_player->traits[TRAIT_LOW_G_WORLD] == 0) {
                    worth = (grav_gen_bonus + 12) * worth / 16;
                }
            } else if (gravity == PLANET_GRAVITY_NORMAL_G) {
                if (race_player->traits[TRAIT_LOW_G_WORLD] != 0) {
                    worth = (grav_gen_bonus + 12) * worth / 16;
                }
            } else if (gravity == PLANET_GRAVITY_HEAVY_G) {
                if (race_player->traits[TRAIT_HEAVY_G_WORLD] == 0) {
                    worth = (grav_gen_bonus + 6) * worth / 12;
                }
            }
        }

        if (colony->buildings[BUILDING_CAPITOL] != 0) {
            int government_tier = world->players[(int)colony->owner].traits[TRAIT_CURRENT_GOVERNMENT] / 2;
            if (government_tier == 0) {
                worth = worth * 3 / 2;
            } else if (government_tier == 1) {
                worth = worth * 4 / 3;
            } else if (government_tier == 2) {
                worth = worth * 6 / 5;
            }
        }

        if (planet->planet_special == SYSTEM_SPECIAL_GOLD_DEPOSITS) {
            worth += 1000;
        } else if (planet->planet_special == SYSTEM_SPECIAL_GEM_DEPOSITS) {
            worth += 2000;
        }

        int final_worth = (worth + ((uint32_t)worth >> 31 & 0x3F)) >> 6;

        if (world->settings->game_difficulty >= GAME_DIFFICULTY_AVERAGE) {
            int16_t proximity_worth = AIDATA::Proximity_Worth_To_Player(ai, world, colony, evaluating_player);
            final_worth += (proximity_worth + ((uint32_t)proximity_worth >> 31 & 7)) >> 3;
            if (final_worth < proximity_worth) {
                final_worth = proximity_worth;
            }
        }

        if (final_worth >= 65535) {
            final_worth = 65535;
        }

        return (int16_t)final_worth;
    }

    int32_t __cdecl Uncolonized_Planet_Worth_To_Player(const game_world::s_view* world, s_planet_data* planet, s_player* evaluating_player) {
        int64_t worth = 0;

        if (planet->planet_type == PLANET_TYPE_PLANET) {
            int16_t size_bonus = (int16_t)((planet->size + 1) * 2);

            int16_t player_idx = (int16_t)(evaluating_player - world->players);
            int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(planet, player_idx);

            int64_t total_pop_sum = max_pop + size_bonus;
            if (evaluating_player->tech_applications[TECH_APP_ADVANCED_CITY_PLANNING] == TECH_RESEARCH_STATUS_RESEARCHED) {
                total_pop_sum += COLCALC::population_bonus.advanced_city_planning;
            }

            uint8_t mineral_bonus = COLCALC::minerals_per_mine[planet->mineral_class];
            int64_t scaled_pop = (mineral_bonus + 1) * total_pop_sum / 3;

            int8_t pop_growth_bonus = evaluating_player->traits[TRAIT_POP_GROWTH_BONUS];
            const auto& growth_options = raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH];
            if (pop_growth_bonus == growth_options[raceopt::PRIMARY_CHOICE_PENALTY].value) {
                worth = 80 * mineral_bonus + scaled_pop;
            } else if (pop_growth_bonus == 0) {
                worth = 75 * mineral_bonus + 2 * scaled_pop;
            } else if (pop_growth_bonus == growth_options[raceopt::PRIMARY_CHOICE_BONUS].value) {
                worth = 70 * mineral_bonus + 3 * scaled_pop;
            } else if (pop_growth_bonus == growth_options[raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                worth = 65 * mineral_bonus + 4 * scaled_pop;
            }

            int64_t food_per_farmer = planet->food_per_farmer;
            bool has_grav_gen_tech = (evaluating_player->tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] == TECH_RESEARCH_STATUS_RESEARCHED);
            if (evaluating_player->tech_applications[TECH_APP_BIOMORPHIC_FUNGI] == TECH_RESEARCH_STATUS_RESEARCHED &&
                COLCALC::Biomorphic_Fungi_Applies((int16_t)food_per_farmer)) {
                food_per_farmer += COLCALC::food_farmer_bonus.biomorphic_fungi;
            }

            if (evaluating_player->traits[TRAIT_EATS_MINERALS] == 0) {
                if (evaluating_player->traits[TRAIT_CYBERNETIC] == 0) {
                    worth += food_per_farmer * planet_valuation_rules.food.biological;
                } else {
                    worth += food_per_farmer * planet_valuation_rules.food.cybernetic;
                }
            }

            switch (planet->planet_special) {
                case SYSTEM_SPECIAL_GOLD_DEPOSITS: worth += planet_valuation_rules.special.gold_deposits; break;
                case SYSTEM_SPECIAL_GEM_DEPOSITS: worth += planet_valuation_rules.special.gem_deposits; break;
                case SYSTEM_SPECIAL_ANCIENT_ARTIFACTS: worth += planet_valuation_rules.special.ancient_artifacts; break;
                case SYSTEM_SPECIAL_ORION_STAR: worth += planet_valuation_rules.special.orion; break;
                default: break;
            }

            worth = (mineral_bonus + 7) * total_pop_sum * worth / 10;

            bool is_swamp_ocean = (planet->climate == PLANET_CLIMATE_OCEAN || planet->climate == PLANET_CLIMATE_SWAMP);
            bool is_aquatic = (evaluating_player->traits[TRAIT_AQUATIC] != 0);

            if (!is_swamp_ocean || !is_aquatic) {
                int64_t maintenance_reduction = COLCALC::climate_maintenance_modifiers[planet->climate] >> 2;
                worth = worth * (100 - maintenance_reduction) / 100;
                if (is_swamp_ocean) {
                    worth = (worth * 3) / 4;
                }
            }

            int64_t divisor = 1;
            int64_t penalty_factor = has_grav_gen_tech ? 1 : 0;
            bool apply_penalty = false;

            if (planet->gravity_class == PLANET_GRAVITY_LOW_G) {
                if (evaluating_player->traits[TRAIT_LOW_G_WORLD] == 0) {
                    divisor = 20;
                    penalty_factor += 18;
                    apply_penalty = true;
                }
            } else if (planet->gravity_class == PLANET_GRAVITY_NORMAL_G) {
                if (evaluating_player->traits[TRAIT_LOW_G_WORLD] != 0) {
                    divisor = 20;
                    penalty_factor += 18;
                    apply_penalty = true;
                }
            } else if (planet->gravity_class == PLANET_GRAVITY_HEAVY_G) {
                if (evaluating_player->traits[TRAIT_HEAVY_G_WORLD] == 0) {
                    divisor = 6;
                    penalty_factor += 4;
                    apply_penalty = true;
                }
            }

            if (apply_penalty) {
                worth = (worth * penalty_factor) / divisor;
            }
        }

        int64_t final_worth = (worth + ((worth >> 63) & 0x3F)) >> 6;
        if (final_worth <= 0) {
            return 0;
        }
        if (final_worth > 0xFFFE) {
            return 0xFFFF;
        }
        return (int32_t)final_worth;
    }

} // namespace AIDATA