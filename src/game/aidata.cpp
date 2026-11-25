#include "pch.h"
#include "fleet_movement.h"

#include "legacy_sort.h"
#include "legacy_ids.h"
#include "ship_location_queries.h"
#include "ship_ids.h"

namespace AIDATA {
    using game::e_ship_location;

    id_player Colony_Dominant_Player(const game::s_game_state& world, const s_colony* colony) {
        if (world.player_count <= 0) return game::NO_PLAYER;
        uint8_t player_counts[MAX_PLAYERS] = {0};
        int total_pops = 0;

        if (colony->n_pops != 0) {
            for (int i = 0; i < (int)colony->n_pops; ++i) {
                const auto player = POP::Group_Player(colony->pop[i]);
                if (player != game::NO_PLAYER) {
                    total_pops++;
                    player_counts[player.id]++;
                }
            }
        }

        int max_player = std::min<int>(world.player_count, MAX_PLAYERS) - 1;
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
            return world.players[max_player].id;
        }

        const auto owner = colony->owner;
        const uint8_t owner_count = owner.id < MAX_PLAYERS ? player_counts[owner.id] : 0;

        if (owner_count * 3 <= total_pops && max_count != 0 && owner_count != max_count) {
            return world.players[max_player].id;
        }

        return owner;
    }


    int32_t __cdecl Refit_Desirability(const s_player* owner, const s_technology_assessment* ai_tech, const s_ship_data* ship) {
        int32_t desirability = 0;

        if ((int8_t)factions::Legacy_Code(ship->d.previous_owner) != (int8_t)factions::Legacy_Code(ship->owner)) {
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
            if (ship->d == *standard_design) {
                return 0;
            }
        }

        return desirability;
    }

    const char* Validate_Inputs(game::s_game_state* world) {
        if (!world) return "missing game state";
        if (world->player_count <= 0 || world->player_count > MAX_PLAYERS) return "invalid player count";
        if (world->ship_slots.size() > MAX_SHIPS) return "invalid ship count";
        if (world->star_order.size() > MAX_STARS) return "invalid star count";
        if (world->colonies.size() > MAX_COLONIES) return "invalid colony count";
        if (world->planets.size() > MAX_PLANETS) return "invalid planet count";
        return nullptr;
    }

    bool Compute_AI_Data(s_state* ai, game::s_game_state* world, int16_t difficulty, int16_t strategic_combat, int32_t stardate) {
        if (Validate_Inputs(world) != nullptr) {
            Reset(ai);
            return false;
        }
        RUSS::Assert_Settings();
        Begin_Turn(ai, stardate);

        for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
            if (world->ship_slots[i] == ::game::NO_SHIP) continue;
            s_ship_data& ship = ship_slots::At(*world, i);
            if (ship.status < 3) {
                int owner = factions::Legacy_Code(ship.owner);
                bool player_owned = owner >= 0 && owner < MAX_PLAYERS;
                const auto location = ships::Location(ship.position);
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
                    star_id = game::Find_Star_At_Coordinates(*world, ship.x, ship.y);
                    turn_idx = 0;
                }

                if (star_id && world->stars.contains(*star_id)) {
                    auto& star_assessment = ai->stars[*star_id];
                    bool in_nebula = world->stars.at(*star_id).in_nebula != 0;
                    if (player_owned && turn_idx < FORECAST_TURN_COUNT && world->players[owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        star_assessment.enroute_human_ships[turn_idx] = 1;
                    }
                    for (int p = 0; p < world->player_count; ++p) {
                        if (owner != p && world->players[p].eliminated == 0) {
                            if (ship.d.ship_type == SHIP_TYPE_COMBAT) {
                                uint16_t str = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships(world, strategic_combat, game::Ship_Key_At_Row(*world, i), factions::From_Player(world->players[p].id));
                                ai->ships[game::Ship_Key_At_Row(*world, i)].strengths[p] = str;
                                if (player_owned) {
                                    ai->players[owner].empire.strength_vs_player[p] += str;
                                }
                                ai->ships[game::Ship_Key_At_Row(*world, i)].strengths_in_nebula[p] = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships_In_Nebula(world, strategic_combat, game::Ship_Key_At_Row(*world, i), factions::From_Player(world->players[p].id));
                                AIPOWER::Ship_Strength_Vs_Planetary_Shield(world, strategic_combat, game::Ship_Key_At_Row(*world, i), ai->ships[game::Ship_Key_At_Row(*world, i)].bomb_strengths);
                                if (player_owned && turn_idx < FORECAST_TURN_COUNT) {
                                    uint16_t cur_str = in_nebula ? ai->ships[game::Ship_Key_At_Row(*world, i)].strengths_in_nebula[p] : ai->ships[game::Ship_Key_At_Row(*world, i)].strengths[p];
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

        for (int p = 0; p < world->player_count; ++p) {
            memset(world->players[p].total_ship_strength, 0, sizeof(world->players[p].total_ship_strength));
        }

        for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
            if (world->ship_slots[i] == ::game::NO_SHIP) continue;
            int owner = factions::Legacy_Code(ship_slots::At(*world, i).owner);
            if (owner >= 0 && owner < MAX_PLAYERS) {
                for (int p = 0; p < world->player_count; ++p) {
                    world->players[owner].total_ship_strength[p] += ai->ships[game::Ship_Key_At_Row(*world, i)].strengths[p];
                }
            }
        }

        for (const auto& [star_id, star] : world->stars) {
            auto& star_assessment = ai->stars[star_id];
            for (int p_o = 0; p_o < world->player_count; ++p_o) {
                for (int t = 0; t < LAST_FORECAST_TURN; ++t) {
                    for (int p_v = 0; p_v < world->player_count; ++p_v) {
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
        for (int p = 0; p < world->player_count; ++p) {
            int32_t r = world->players[p].ship_range;
            range_sq[p] = (r * 30) * (r * 30);
            int32_t ext_r = r * 45;
            ext_range_sq[p] = ext_r * ext_r;
        }

        for (auto& [roster_id, roster_colony] : world->colonies) {
            s_colony& col = roster_colony;
            if (col.planet_id != game::NO_PLANET) {
                const auto& home = world->stars.at(world->planets.at(col.planet_id).star_id);
                const auto sx = home.x;
                const auto sy = home.y;
                for (int16_t s = 0; s < std::ssize(world->star_order); ++s) {
                    const auto star_id = world->star_order[s];
                    auto& star_record = world->stars.at(star_id);
                    int32_t dx = sx - star_record.x;
                    int32_t dy = sy - star_record.y;
                    int32_t d2 = dx * dx + dy * dy;
                    if (d2 <= ext_range_sq[col.owner.id]) {
                        if (!struct_::Read_Bit_Field(home.black_hole_blocks, s)) {
                            auto& star_assessment = ai->stars[star_id];
                            star_assessment.in_extended_range.set(col.owner);
                            if (d2 <= range_sq[(int)legacy_world::Encode_Player_Slot(col.owner)]) {
                                star_assessment.in_range.set(col.owner);
                            }
                        }
                    }
                }
            }
        }

        for (const auto& [source_id, source] : world->stars) {
            const auto& source_assessment = ai->stars[source_id];
            for (int p = 0; p < world->player_count; ++p) {
                if (source_assessment.in_extended_range.test_slot(p)) {
                    for (const auto& [destination_id, destination] : world->stars) {
                        if (ai->stars[destination_id].in_extended_range.test_slot(p)) {
                            auto turns = AIDATA::Cache_Travel_Turns(
                                SHIPMOVE::Player_N_Turns_From_Star_To_Star(*world, world->players[p].id,
                                    source_id, destination_id));
                            if (turns) ai->routes.direct.Store(source_id, destination_id, world->players[p].id, (uint8_t)*turns);
                        }
                    }
                }
            }
        }

        for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
            if (world->ship_slots[i] == ::game::NO_SHIP) continue;
            s_ship_data& ship = ship_slots::At(*world, i);
            if (ship.status < 5 && ship.status == 1) {
                const auto destination = ships::Location(ship.position);
                if (!destination || !world->stars.contains(destination->star_id)) continue;
                const auto dest_star = destination->star_id;
                auto turns = AIDATA::Cache_Travel_Turns(ship.turns_left);
                if (turns) ai->routes.ship_direct.Store(game::Ship_Key_At_Row(*world, i), dest_star, (uint8_t)*turns);
                // Reordering a fleet in transit uses the live movement engine.
                if (world == &game::current_state && SHIPMOVE::Can_Order_Ship(*world, ship)) {
                    const auto mask = game::player_bitmap::Only(factions::Player(ship.owner));
                    const std::array<id_ship, 1> move_info{game::Ship_Key_At_Row(*world, i)};
                    s_ship_move_info out_info;
                    for (const auto star_id : world->star_order) {
                        if ((ai->stars[star_id].in_extended_range & mask) && star_id != dest_star) {
                            SHIPMOVE::Evaluate_Fleet_Move(*world, move_info, {game::e_star_destination::star, star_id}, &out_info);
                            auto turns = AIDATA::Cache_Travel_Turns(out_info.turns_left);
                            if (turns) ai->routes.ship_direct.Store(game::Ship_Key_At_Row(*world, i), star_id, (uint8_t)*turns);
                        }
                    }
                }
            }
        }

        for (auto& [roster_id, roster_colony] : world->colonies) {
            s_colony& col = roster_colony;
            if (legacy_world::Encode_Player_Slot(col.owner) != -1) {
                s_colony_assessment& ai_col = ai->colonies[roster_id];
                for (int p_idx = 0; p_idx < col.n_pops; ++p_idx) {
                    if (col.pop[p_idx].conquered) {
                        ai_col.conquered_population++;
                    }
                }
                ai_col.dominant_race = Colony_Dominant_Player(*world, &col);
                const auto dominant_index = ai_col.dominant_race.id;
                if (dominant_index >= MAX_PLAYERS) continue;
                ai_col.population_limit = (int8_t)COLCALC::Colony_Race_Pop_Limit(&col, dominant_index);
                if (world->players[dominant_index].traits[TRAIT_EATS_MINERALS] == 0 || world->players[col.owner.id].traits[TRAIT_EATS_MINERALS] == 0) {
                    ai_col.food_buildings_relevant = true;
                }
                if (world->players[dominant_index].traits[TRAIT_ENVIRONMENT_IMMUNE] == 0) {
                    ai_col.pollution_controls_relevant = true;
                }
            }
        }

        for (auto& [roster_id, roster_colony] : world->colonies) {
            s_colony& col = roster_colony;
            if (legacy_world::Encode_Player_Slot(col.owner) != -1) {
                int owner = legacy_world::Encode_Player_Slot(col.owner);
                const auto star_id = world->planets.at(col.planet_id).star_id;
                for (int p = world->player_count - 1; p >= 0; --p) {
                    if (p != owner && ai->stars[star_id].enroute_ships[p][LAST_FORECAST_TURN]) {
                        ai->colonies[roster_id].other_player_arrivals = true;
                        break;
                    }
                }
            }
        }

        for (const auto& [star_id, star] : world->stars) {
            s_star_neighborhood& ai_star = ai->stars[star_id].neighborhood;
            for (int pl_i = 0; pl_i < 5; ++pl_i) {
                if (star.orbit_planet_ids[pl_i] != game::NO_PLANET) {
                    ai_star.planet_count++;
                }
            }
            for (int p = 0; p < world->player_count; ++p) {
                if (world->players[p].objectives != PLAYER_OBJECTIVE_HUMAN && world->players[p].eliminated == 0 && (ai->stars[star_id].in_extended_range.test_slot(p))) {
                    for (int other_p = world->player_count - 1; other_p >= 0; --other_p) {
                        if (other_p != p && ai->stars[star_id].enroute_ships[other_p][LAST_FORECAST_TURN]) {
                            ai_star.per_player[p].other_player_arrivals = true;
                            break;
                        }
                    }
                }
            }
        }

        Compute_Star_Player_Range_Info(ai, world);

        for (int p1 = 0; p1 < world->player_count; ++p1) {
            for (const auto& [star_id, star_record] : world->stars) {
                if (game::Has_Player_Colony(star_record, p1)) {
                    ai->players[p1].empire.colonized_star_count++;
                    if (star_record.blockaded.test_slot(p1)) {
                        ai->players[p1].empire.blockaded_star_count++;
                    }
                }
            }
            for (int p2 = 0; p2 < world->player_count; ++p2) {
                if (p1 == p2) continue;
                if (world->players[p1].contact[p2]) {
                    ai->players[p1].empire.contact_count++;
                    ai->players[p1].empire.reachable_player_count++;
                    ai->players[p1].empire.reachable_players[p2] = true;
                } else {
                    bool reachable = false;
                    const auto mask = game::player_bitmap::Only_Slot(p1) | game::player_bitmap::Only_Slot(p2);
                    for (const auto& [star_id, star] : world->stars) {
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

        for (auto& [roster_id, roster_colony] : world->colonies) {
            s_colony& col = roster_colony;
            if (legacy_world::Encode_Player_Slot(col.owner) != -1) {
                ai->players.at(col.owner.id).empire.colony_count++;
                if (world->stars.at(world->planets.at(col.planet_id).star_id).blockaded.test(col.owner)) {
                    ai->players.at(col.owner.id).empire.blockaded_colony_count++;
                }
            }
        }

        int16_t max_colonies = 0;
        for (int p = 0; p < world->player_count; ++p) {
            if (ai->players[p].empire.colony_count > max_colonies) {
                max_colonies = ai->players[p].empire.colony_count;
            }
        }
        for (int p = 0; p < world->player_count; ++p) {
            ai->players[p].empire.colony_deficit = max_colonies - ai->players[p].empire.colony_count;
        }

        Compute_Base_Planet_Values(ai, world, difficulty);
        Compute_Contextual_Planet_Values(ai, world);
        ::aitech::Compute_AI_Tech_Info(ai, world, difficulty, strategic_combat);

        for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
            if (world->ship_slots[i] == ::game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(*world, i);
            if (ship->d.ship_type == SHIP_TYPE_COMBAT && factions::Legacy_Code(ship->owner) >= 0 && factions::Legacy_Code(ship->owner) < world->player_count && ship->status < 3 && ship->mission != 4 /* SHIP_MISSION_SCRAP */) {
                ai->combat_ships.push_back(game::Ship_Key_At_Row(*world, i));
                if (ship->status == 0) {
                    int32_t d = Refit_Desirability(&world->players.at(factions::Player(ship->owner).id), &ai->players.at(factions::Player(ship->owner).id).technology, ship);
                    ai->ships[game::Ship_Key_At_Row(*world, i)].refit_desirability = d;
                    if (d != 0) {
                        ai->players.at(factions::Player(ship->owner).id).refits.push_back({game::Ship_Key_At_Row(*world, i), d});
                    }
                }
            }
        }

        for (int p = 0; p < world->player_count; ++p) {
            auto& refits = ai->players[p].refits;
            qsort(refits.data(), refits.size(), sizeof(s_refit_candidate), airefit_cmp);
        }

        Legacy_Sort(std::span{ai->combat_ships}, [&](id_ship lhs, id_ship rhs) {
            return Compare_Combat_Ships(*world, lhs, rhs);
        });

        for (uint32_t i = 0; i < ai->combat_ships.size(); ++i) {
            const auto ship_entry = world->ships.find(ai->combat_ships[i]);
            const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
            if (!ship) continue;
            const auto orbit = ships::Orbit_Star(ship->position);
            const int owner = factions::Legacy_Code(ship->owner);
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

        ai->routes.direct.Prepare_Live_Indices(std::ssize(world->stars), world->player_count);

        for (const auto key : ai->combat_ships) {
            const auto ship_entry = world->ships.find(key);
            const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
            if (!ship) continue;
            const bool stealth = struct_::Read_Bit_Field(ship->d.special_device_flags, 11);
            const auto location = ships::Location(ship->position);
            if (!location) continue;
            auto source_star = location->star_id;
            const int owner = factions::Legacy_Code(ship->owner);
            int initial_delay = 0;

            if (ship->status == 10) { // SHIP_STATUS_ANTARAN
                initial_delay = 0;
            } else if (location->kind == e_ship_location::at_star) {
                initial_delay = 0;
            } else if (location->kind == e_ship_location::in_flight) {
                initial_delay = ship->turns_left;
            } else {
                const auto source = game::Find_Star_At_Coordinates(*world, ship->x, ship->y);
                if (!source) continue;
                source_star = *source;
            }

            const auto source = world->stars.find(source_star);
            if (source == world->stars.end()) continue;
            bool in_nebula = source->second.in_nebula != 0;

            for (int p = 0; p < world->player_count; ++p) {
                if (p != owner) {
                    for (const auto star_id : world->star_order) {
                        auto& star_assessment = ai->stars[star_id];
                        if ((star_assessment.in_extended_range.test_slot(p)) || star_id == source_star) {
                            auto route = Player_N_Turns_From_Star_To_Star_With_Intermediate(ai, world, world->players[owner].id, source_star, star_id, stealth);
                            if (route) {
                                int eta = initial_delay + route->turns;
                                if (eta < FORECAST_TURN_COUNT) {
                                    const auto& stats = ai->ships[key];
                                    uint16_t str = in_nebula ? stats.strengths_in_nebula[p] : stats.strengths[p];
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

        for (const auto& [id, body] : world->planets) {
            if (body.colony_id != game::NO_COLONY) {
                int owner = legacy_world::Encode_Player_Slot(world->colonies.at(body.colony_id).owner);
                auto& planet = ai->planets[id];
                planet.stage_weights[owner] = planet.base_values[owner];
            }
        }

        uint8_t player_treaty_matrix[MAX_PLAYERS][MAX_PLAYERS];
        memset(player_treaty_matrix, 0, sizeof(player_treaty_matrix));

        for (int i = 0; i < world->player_count; i++) {
            int flag = 0;
            for (int j = 0; j < world->player_count; j++) {
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

        for (const auto star_id_ : world->star_order) {
            auto& star_record = world->stars.at(star_id_);
            if (star_record.has_colony) {
                std::array<std::optional<id_planet>, MAX_PLAYERS> best_planet_at_star{};
                std::array<int16_t, MAX_PLAYERS> best_planet_val_at_star{};
                for (const auto& link : star_record.orbit_planet_ids) {
                    const auto id = link;
                    if (id == game::NO_PLANET) continue;
                    const auto& planet = world->planets.at(id);
                    if (planet.colony_id == game::NO_COLONY) continue;
                    const auto owner = legacy_world::Encode_Player_Slot(world->colonies.at(planet.colony_id).owner);
                    const auto value = ai->planets[id].stage_weights[owner];
                    if (value > best_planet_val_at_star[owner]) {
                        best_planet_at_star[owner] = id;
                        best_planet_val_at_star[owner] = value;
                    }
                }
                for (int p_o = 0; p_o < world->player_count; ++p_o) {
                    if (best_planet_at_star[p_o]) {
                        int32_t threat_sum = 0;
                        int32_t value_sum = 0;

                        for (int p_v = 0; p_v < world->player_count; ++p_v) {
                            if (player_treaty_matrix[p_o][p_v]) {
                                int16_t max_eta = 8;
                                if (ai->players[p_v].empire.colonized_star_count * 5 < ai->players[p_o].empire.colonized_star_count) {
                                    max_eta = (ai->players[p_v].empire.colonized_star_count <= 1) ? 1 : 4;
                                }
                                for (const auto other_star_id : world->star_order) {
                                    int32_t str = ai->stars[other_star_id].possible_strengths[p_v][p_o][0];
                                    if (str != 0) {
                                        auto route = Player_N_Turns_From_Star_To_Star_With_Intermediate(ai, world, world->players[p_v].id, other_star_id, star_id_, true);
                                        if (route && route->turns < max_eta) {
                                            threat_sum += str / (route->turns + 1);
                                        }
                                    }
                                }
                            }
                        }

                        for (const auto& [id, planet] : world->planets) {
                            if (id != *best_planet_at_star[p_o]) {
                                auto route = Player_N_Turns_From_Star_To_Star_With_Intermediate(ai, world, world->players[p_o].id, star_id_, planet.star_id, false);
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

        for (int p = 0; p < world->player_count; ++p) {
            AISTAGE::Compute_Defensive_Stage_Points(ai, world, world->players[p].id);
        }

        for (int p = 0; p < world->player_count; ++p) {
            for (int16_t s = 0; s < std::ssize(world->star_order); ++s) {
                const auto star_id = world->star_order[s];
                auto& star_record = world->stars.at(star_id);
                auto& order = star_record.surrender_to[p];
                if (order.Needs_Review()) {
                    const auto target = order.Proposed_Recipient();
                    order = game::star_surrender_order::None();
                    if (target.id < static_cast<uint32_t>(world->player_count) &&
                        world->players[p].eliminated == 0 && world->players[target.id].eliminated == 0) {
                        const auto player = world->players[p].id;
                        const auto recipient = world->players[target.id].id;
                        if (COLCALC::May_Surrender_Star(*world, star_id, player, recipient)) {
                            COLCALC::Surrender_Star(*world, star_id, player, recipient);
                        }
                    }
                }
            }
        }

        RUSS::Assert_Settings();
        for (auto& [star_id, star] : world->stars) {
            star.just_visited.reset();
        }
        return true;
    }

    void __cdecl Compute_Star_Player_Range_Info(s_state* ai, game::s_game_state* world) {
        for (const auto& [star_id, star] : world->stars) {
            s_star_neighborhood& ai_info = ai->stars[star_id].neighborhood;

            for (auto& [roster_id, roster_colony] : world->colonies) {
                s_colony& col = roster_colony;
                int owner = (int)legacy_world::Encode_Player_Slot(col.owner);

                if (owner != -1) {
                    const auto colony_star = world->planets.at(col.planet_id).star_id;
                    int ship_range = (int)world->players[owner].ship_range;

                    if (Star_In_Range_Of_Given_Star(world, star_id, colony_star, ship_range)) {
                        ai_info.per_player[owner].covering_colony_count++;
                    }
                }
            }

            int8_t in_normal_range[MAX_PLAYERS];
            int8_t in_extended_range[MAX_PLAYERS];
            Find_Players_In_Range(world, star_id, in_normal_range, in_extended_range);

            for (int player_idx = 0; player_idx < (int)world->player_count; ++player_idx) {
                if (game::Has_Player_Colony(star, player_idx)) {
                    s_player& plr = world->players[player_idx];
                    s_star_player_assessment& block = ai_info.per_player[player_idx];

                    for (int other_idx = 0; other_idx < (int)world->player_count; ++other_idx) {
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

    void Find_Players_In_Range(game::s_game_state* world, id_star star_idx, int8_t* in_normal_range_out, int8_t* in_extended_range_out) {
        auto& target_star = world->stars.at(star_idx);
        int32_t target_x = target_star.x;
        int32_t target_y = target_star.y;

        memset(in_normal_range_out, 0, MAX_PLAYERS);
        memset(in_extended_range_out, 0, MAX_PLAYERS);

        int32_t normal_range_sq[MAX_PLAYERS];
        int32_t extended_range_sq[MAX_PLAYERS];

        for (int player_idx = 0; player_idx < world->player_count; ++player_idx) {
            int32_t range = world->players[player_idx].ship_range;
            int32_t range_sq = range * range;

            normal_range_sq[player_idx] = range_sq * 900;
            extended_range_sq[player_idx] = range_sq * 2025;
        }

        for (auto& [roster_id, roster_colony] : world->colonies) {
            auto& colony = roster_colony;
            int8_t owner = legacy_world::Encode_Player_Slot(colony.owner);

            if (owner != -1 && in_normal_range_out[owner] == 0) {
                auto& planet = world->planets.at(colony.planet_id);
                auto& colony_star = world->stars.at(planet.star_id);

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

    int Compare_Combat_Ships(game::s_game_state& world, id_ship lhs, id_ship rhs) {
        const auto ship1_entry = world.ships.find(lhs);
        const auto* ship1 = ship1_entry == world.ships.end() ? nullptr : &ship1_entry->second;
        const auto ship2_entry = world.ships.find(rhs);
        const auto* ship2 = ship2_entry == world.ships.end() ? nullptr : &ship2_entry->second;
        if (!ship1 || !ship2) return (!ship1) - (!ship2);

        if (factions::Legacy_Code(ship1->owner) < factions::Legacy_Code(ship2->owner)) {
            return -1;
        }
        if (factions::Legacy_Code(ship1->owner) > factions::Legacy_Code(ship2->owner)) {
            return 1;
        }

        // Keep fleet grouping in galaxy row order, then by coordinates in transit.
        const auto antares_row = static_cast<int16_t>(world.star_order.size());
        const auto sort_row = [&](const s_ship_data& ship) {
            return ship.position.kind == game::e_ship_location::at_star
                ? game::Ship_Orbit_Row(world, ship.position, antares_row) : MAX_STARS;
        };
        const auto a = sort_row(*ship1);
        const auto b = sort_row(*ship2);
        if (a < MAX_STARS || b < MAX_STARS) return (a > b) - (a < b);
        if (ship1->x != ship2->x) return ship1->x < ship2->x ? -1 : 1;
        return (ship1->y > ship2->y) - (ship1->y < ship2->y);
    }

    void __cdecl Compute_Base_Planet_Values(s_state* ai, game::s_game_state* world, int16_t difficulty) {
        for (auto& [id, planet] : ai->planets) {
            std::fill(std::begin(planet.base_values), std::end(planet.base_values), 0);
        }

        if (world->player_count <= 0) {
            return;
        }

        for (int16_t player_idx = 0; player_idx < world->player_count; ++player_idx) {
            for (auto& [id, record] : world->planets) {
                auto* planet = &record;

                const auto system_visibility_bits = ai->stars[planet->star_id].in_extended_range;

                if (system_visibility_bits.test_slot(player_idx)) {
                    game::id_colony col_idx = planet->colony_id;

                    uint16_t value;
                    if (col_idx == game::NO_COLONY) {
                        value = AIDATA::Uncolonized_Planet_Worth_To_Player(world, planet, &world->players[player_idx]);
                    } else {
                        value = Colony_Worth_To_Player(ai, world, difficulty, col_idx, &world->colonies.at(col_idx), &world->players[player_idx]);
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
    void __cdecl Compute_Contextual_Planet_Values(s_state* ai, game::s_game_state* world) {
        for (auto& [id, planet] : ai->planets) {
            std::fill(std::begin(planet.contextual_values), std::end(planet.contextual_values), 0);
        }

        if (world->player_count <= 0) {
            return;
        }

        for (int p = 0; p < (int)world->player_count; ++p) {
            for (auto& [id, record] : world->planets) {
                uint16_t base_val = ai->planets[id].base_values[p];
                if (base_val <= 0) {
                    continue;
                }

                const auto* planet = &record;
                s_star_data* star = &world->stars.at(planet->star_id);

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
                    const auto colony_idx = world->planets.at(neighbor_id).colony_id;

                    if (colony_idx == game::NO_COLONY) {
                        // Uncolonized planet in system
                        uncolonized_system_base_sum += ai->planets[neighbor_id].base_values[p];
                    } else {
                        if (legacy_world::Encode_Player_Slot(world->colonies.at(colony_idx).owner) == p) {
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
