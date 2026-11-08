#include "pch.h"
#include "fleet_movement.h"

#include "legacy_world.h"

namespace AIMOVE {
    std::optional<uint32_t> First_Threat_Turn(const std::array<int32_t, AIDATA::FORECAST_TURN_COUNT>* first, const std::array<int32_t, AIDATA::FORECAST_TURN_COUNT>* second, uint32_t from) {
        for (uint32_t turn = from; turn < AIDATA::FORECAST_TURN_COUNT; ++turn) {
            if ((*first)[turn] != 0 || (*second)[turn] != 0) return turn;
        }
        return std::nullopt;
    }
    using game_world::e_ship_location;
    using AIDATA::FORECAST_TURN_COUNT;
    using AIDATA::LAST_FORECAST_TURN;
    struct s_candidate_sort_context {
        const std::unordered_map<uint32_t, AIDATA::s_star_assessment>* stars;
        int16_t player_idx;
        int16_t player_count;
    };

    int __cdecl Sort_colonizable(const void* a, const void* b) {
        auto p1 = (const AIDATA::s_colonization_candidate*)a;
        auto p2 = (const AIDATA::s_colonization_candidate*)b;
        return p1->contextual_value - p2->contextual_value;
    }

    void __cdecl Move_All_AI(AIDATA::s_state* ai, const game_world::s_view* world) {
        for (int16_t i = 0; i < world->Player_Count(); ++i) {
            s_player& plr = world->players[i];

            if (plr.objectives != PLAYER_OBJECTIVE_HUMAN && plr.ship_speed != 0) {
                RUSS::Mox_Update();

                if (plr.traits[TRAIT_TRANS_DIMENSIONAL] != 0 || EVENTS::Event_Check_Hyperspace_Flux() == 0) {
                    Move_AI(ai, world, i);
                }
            }
        }

        Move_NPCs(world);
    }

    void __cdecl Launch_Sneak_Attack(AIDATA::s_state* ai, s_ai_movement_context* ctx) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        s_player* p_ai = ctx->player;
        if (p_ai->sneak_attack_planet != -1) {
            if (AIBUILD::Player_War_Level(world, i_ai) != 0) {
                p_ai->sneak_attack_planet = -1;
            }
        }

        if (p_ai->sneak_attack_planet != -1) {
            int16_t target_planet_idx = p_ai->sneak_attack_planet;
            int16_t colony_idx = world->planets[target_planet_idx].colony_index;
            uint8_t target_player = (uint8_t)p_ai->sneak_attack_player;

            if (colony_idx != -1 && world->colonies[colony_idx].owner == target_player) {
                int16_t stage_point = world->players[i_ai].offensive_stagepoint;

                if (ai->players[i_ai].Find_Combat_Ships(stage_point)) {
                    std::vector<uint32_t> ship_list;
                    s_ship_move_info move_info;
                    int16_t star_idx = world->planets[target_planet_idx].star_index;

                    Gather_Player_Ships_At_Star(ai, ctx, i_ai, stage_point, &ship_list);
                    SHIPMOVE::Evaluate_Fleet_Move(ship_list, star_idx, &move_info);

                    if (move_info.moving != 0) {
                        for (uint32_t id : ship_list) {
                            world->ships[id].mission = 6;
                        }
                        SHIPMOVE::Order_Fleet_Move(ship_list, star_idx, &move_info);
                        ai->players[i_ai].combat_ship_groups.erase(stage_point);
                    }
                }
            }

            int16_t star_idx = world->planets[p_ai->sneak_attack_planet].star_index;
            if (ai->stars[star_idx].enroute_strengths[i_ai][target_player][1] != 0) {
                diplomac::Declare_War(i_ai, target_player);
            }
        }
    }

    void __cdecl Move_AI(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx) {
        auto& work = ai->players[player_idx].movement;
        work = {};
        s_ai_movement_context context{player_idx, &world->players[player_idx], &work, world};
        s_ai_movement_context* ctx = &context;
        const int16_t i_ai = player_idx;
        const s_player* p_ai = ctx->player;

        RUSS::Mox_Update();

        Get_Colonizable_Planets(ai, ctx, &work.colonizable);
        qsort(work.colonizable.data(), work.colonizable.size(), sizeof(AIDATA::s_colonization_candidate), Sort_colonizable);

        Mark_Flee_Stars(world, &work, player_idx);

        s_ship_data* ship = world->ships;
        if (ship < world->ships + world->Ship_Count()) {
            do {
                if (ship->owner >= MAX_PLAYERS && ship->status == 0) {
                    auto orbit = legacy_world::Orbit_Star(ship);
                    if (orbit) work.nonplayer_here[*orbit] = ship->owner;
                }
                ship++;
            } while (ship < world->ships + world->Ship_Count());
        }

        RUSS::Mox_Update();
        Delete_Already_Assigned_Planets(ctx, &work.colonizable);

        RUSS::Mox_Update();
        Launch_Sneak_Attack(ai, ctx);

        RUSS::Mox_Update();
        Launch_Attacks(ai, ctx, 6, 2);

        RUSS::Mox_Update();
        Intercept_Invaders(ai, ctx);

        RUSS::Mox_Update();
        Launch_Attacks(ai, ctx, 1, 2);

        RUSS::Mox_Update();
        Assign_New_Colony_Ship_Destinations(ai, ctx, &work.colonizable);

        RUSS::Mox_Update();
        Launch_Attacks(ai, ctx, 2, 2);

        RUSS::Mox_Update();
        Launch_Attacks(ai, ctx, 6, 0);

        RUSS::Mox_Update();
        Launch_Attacks(ai, ctx, 1, 0);

        RUSS::Mox_Update();
        Launch_Attacks(ai, ctx, 2, 0);

        RUSS::Mox_Update();
        Move_Transports(ai, ctx);

        RUSS::Mox_Update();
        Move_To_Stage_Points(ai, ctx);

        RUSS::Mox_Update();
        if (ai->players[i_ai].empire.contact_count == 0 ||
            world->settings->game_difficulty == GAME_DIFFICULTY_TUTOR) {
            Move_To_Unexplored_Stars(ai, ctx);
        }

        work.complete = true;
    }

    std::optional<s_threat> Player_Threatens_Player(AIDATA::s_state* ai, const game_world::s_view* world, int32_t plr1, int32_t plr2) {

        // Check if player is Trans-Dimensional or if Hyperspace Flux is active
        if (world->players[plr1].traits[TRAIT_TRANS_DIMENSIONAL] == 0) {
            if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
                return std::nullopt;
            }
        }

        if (world->Star_Count() < 1) {
            return std::nullopt;
        }

        for (int16_t i = 0; i < world->Star_Count(); ++i) {
            uint32_t current_strength = (uint32_t)ai->players[plr1].empire.strength_vs_player[plr2];
            uint32_t possible_strength = (uint32_t)ai->stars[i].possible_strengths[plr1][plr2][0];

            // AI logic: check if existing strength ratio allows for a threat/attack opportunity
            if ((current_strength >> 2) <= (possible_strength / 10)) {
                auto attack = Find_Opportunity_Attack_Aux(ai, world, plr1, plr2, 150, i);
                if (attack) return s_threat{(uint32_t)i, *attack};
            }
        }
        return std::nullopt;
    }

    std::optional<uint32_t> Best_Uncolonized_Undestined_Planet(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx) {
        AIDATA::s_movement_workspace work;
        s_ai_movement_context context{player_idx, &world->players[player_idx], &work, world};
        Get_Colonizable_Planets(ai, &context, &work.colonizable);
        qsort(work.colonizable.data(), work.colonizable.size(), sizeof(AIDATA::s_colonization_candidate), Sort_colonizable);
        Delete_Already_Assigned_Planets(&context, &work.colonizable);

        if (!work.colonizable.empty()) {
            return work.colonizable.back().planet_id;
        }

        return std::nullopt;
    }

    void __cdecl Move_To_Unexplored_Stars(AIDATA::s_state* ai, s_ai_movement_context* ctx) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        std::vector<int8_t> star_flags_0(world->Star_Count(), 0);
        int32_t enemy_strength_1[ENEMY_STRENGTH_TURN_WINDOW];
        int32_t enemy_strength_2[ENEMY_STRENGTH_TURN_WINDOW];
        std::vector<uint32_t> ship_indices(world->Ship_Count());
        s_ship_move_info move_info;
        std::array<uint32_t, 1> single_ship_list;


        int32_t num_ships_selected = 0;
        if (world->Ship_Count() > 0) {
            for (int16_t i = 0; i < world->Ship_Count(); ++i) {
                if (world->ships[i].owner == i_ai) {
                    int8_t status = world->ships[i].status;
                    const auto location = legacy_world::Read_Ship_Location(&world->ships[i]);
                    if (!location || location->star_id >= (uint32_t)world->Star_Count()) continue;
                    const uint32_t loc = location->star_id;

                    if (status == 0) {
                        if ((world->ships[i].mission == 0 || world->ships[i].mission == 3) &&
                            world->ships[i].d.ship_type == SHIP_TYPE_COMBAT) {
                            ship_indices[num_ships_selected] = i;
                            num_ships_selected++;
                        }
                        star_flags_0[loc] = 1;
                    } else if (status == 1) {
                        star_flags_0[loc] = 1;
                    } else if (status == 2) {
                        star_flags_0[loc] = 1;
                    }
                }
            }
        }

        if (world->Star_Count() > 0) {
            for (int16_t i = 0; i < world->Star_Count(); ++i) {
                if (((ai->stars[i].in_extended_range >> (uint8_t)i_ai) & 1) != 0 &&
                    ((world->stars[i].visited >> (uint8_t)i_ai) & 1) == 0 &&
                    star_flags_0[i] == 0 &&
                    (world->settings->game_difficulty != GAME_DIFFICULTY_TUTOR || world->stars[i].has_colony == 0)) {
                    int32_t best_ship_idx = -1;
                    int16_t best_star = -1;
                    int32_t min_turns = 255;

                    Get_Enemy_Strength_On_Turn(ai, ctx, i, enemy_strength_1, enemy_strength_2);

                    if (num_ships_selected == 0) {
                        return;
                    }

                    for (int32_t j = 0; j < num_ships_selected; ++j) {
                        uint32_t ship_idx = ship_indices[j];
                        bool has_device = struct_::Test_Bit_Field(world->ships[ship_idx].d.special_device_flags, 11);

                        if (has_device || ((ai->stars[i].in_range >> (uint8_t)i_ai) & 1) != 0) {
                            auto orbit = legacy_world::Orbit_Star(&world->ships[ship_idx]);
                            if (!orbit) continue;
                            auto turns = AIDATA::N_Turns_From_To(&ai->routes, *orbit, i, i_ai);

                            if (turns && *turns < min_turns && *turns < ENEMY_STRENGTH_TURN_WINDOW && enemy_strength_2[*turns] == 0) {
                                min_turns = *turns;
                                best_ship_idx = j;
                                best_star = i;
                            }
                        }
                    }

                    if (best_ship_idx != -1) {
                        single_ship_list[0] = ship_indices[best_ship_idx];
                        num_ships_selected--;
                        ship_indices[best_ship_idx] = ship_indices[num_ships_selected];

                        SHIPMOVE::Evaluate_Fleet_Move(single_ship_list, best_star, &move_info);
                        world->ships[single_ship_list[0]].mission = 7;
                        SHIPMOVE::Order_Fleet_Move(single_ship_list, best_star, &move_info);
                    }
                }
            }
        }
    }

    void __cdecl Launch_Attacks(AIDATA::s_state* ai, s_ai_movement_context* ctx, int mission_type, int min_pop) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        if (world->Star_Count() > 0) {
            for (int16_t star_idx = 0; star_idx < world->Star_Count(); ++star_idx) {
                if (ai->players[i_ai].Find_Combat_Ships(star_idx)) {
                    AIMOVE::Find_Best_Attack_From_Star(ai, ctx, star_idx, nullptr, mission_type, min_pop);
                }
            }
        }
    }

    void __cdecl Mark_Flee_Stars(const game_world::s_view* world, AIDATA::s_movement_workspace* work, int16_t player_idx) {
        // Iterate through all players to check for active blockading durations.
        for (int i = 0; i < world->Player_Count(); ++i) {
            // Check if player i is currently being asked to stop blockading by player_idx.
            if (world->players[player_idx].stop_blockading_duration[i] != 0) {
                // If so, iterate through stars to identify retreat/flee targets.
                for (int16_t star_idx = 0; star_idx < world->Star_Count(); ++star_idx) {
                    // Only mark the star if it hasn't been processed yet.
                    if (!AIDATA::Is_Fleeing(work, star_idx, 0)) {
                        // Check if player i has a colony at this star (has_colony is a bitmask).
                        if ((world->stars[star_idx].has_colony >> i) & 1) {
                            // Mark the entire forecast window for this star.
                            work->flee_stars[star_idx].fill(1);
                        }
                    }
                }
            }
        }
    }

    void Gather_Player_Ships_At_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, int16_t player_id, uint32_t star_id, std::vector<uint32_t>* ships) {
        const auto* world = ctx->world;
        ships->clear();
        s_player* p_ai = ctx->player;
        const auto* group = ai->players[player_id].Find_Combat_Ships(star_id);
        if (group == nullptr || group->next == group->end) {
            return;
        }
        uint32_t index = group->next;
        uint32_t ship_id = ai->combat_ships[index];
        ships->push_back(ship_id);

        while (true) {
            index++;
            if (index >= group->end) {
                break;
            }

            ship_id = ai->combat_ships[index];
            s_ship_data* ship = &world->ships[ship_id];

            if (ship->owner != player_id || legacy_world::Orbit_Star(ship) != star_id) {
                break;
            }

            // Skip ships on mission 5 (potentially 'In transit') unless we are at an offensive stagepoint
            if (p_ai->offensive_stagepoint == star_id || ship->mission != 5) {
                ships->push_back(ship_id);
            }
        }

    }

    void Get_Colonizable_Planets(AIDATA::s_state* ai, s_ai_movement_context* ctx, std::vector<AIDATA::s_colonization_candidate>* candidates) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        candidates->clear();
        s_planet_data* planet = world->planets;
        s_planet_data* end = planet + world->Planet_Count();
        uint32_t i = 0;

        while (planet < end) {
            // Only consider uncolonized planets.
            // PLANET_TYPE_PLANET is a standard colonizable planet (excluding gas giants, asteroids).
            if (planet->colony_index == -1 && planet->planet_type == PLANET_TYPE_PLANET) {
                // Each byte in the array represents a star, with bits corresponding to players
                // who have that star within their extended travel range.
                int16_t star_idx = planet->star_index;
                uint8_t star_range_mask = ai->stars[star_idx].in_extended_range;

                if (((1u << ((uint8_t)i_ai & 0x1F)) & (uint32_t)star_range_mask) != 0) {
                    candidates->push_back({i,
                        (int32_t)(uint16_t)ai->planets[i].contextual_values[ctx->player_idx]});
                }
            }

            ++planet;
            ++i;
        }
    }

    void Delete_Already_Assigned_Planets(s_ai_movement_context* ctx, std::vector<AIDATA::s_colonization_candidate>* candidates) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        s_player* p_ai = ctx->player;
        if (p_ai->tech_applications[TECH_APP_HYPERSPACE_COMMUNICATIONS] != TECH_RESEARCH_STATUS_RESEARCHED) {
            for (int16_t i = 0; i < world->Ship_Count(); ++i) {
                s_ship_data* ship = &world->ships[i];

                if (ship->owner == i_ai &&
                    ship->d.ship_type == SHIP_TYPE_COLONY &&
                    ship->status == 1 &&
                    !SHIPMOVE::Can_Order_Ship(ship)) {
                    auto location = legacy_world::Read_Ship_Location(ship);
                    if (!location) continue;
                    uint32_t star_idx = location->star_id;
                    for (uint32_t j = (uint32_t)candidates->size(); j-- > 0;) {
                        if (world->planets[(*candidates)[j].planet_id].star_index == star_idx) {
                            candidates->erase(candidates->begin() + j);
                            break;
                        }
                    }
                }
            }
        }

        if (world->players[i_ai].objectives != PLAYER_OBJECTIVE_HUMAN) {
            for (int16_t i = 0; i < world->Colony_Count(); ++i) {
                s_colony* colony = &world->colonies[i];

                if (colony->owner == i_ai && colony->producing[0] == BUILDING_COLONY_BASE) {
                    int16_t colony_star_idx = world->planets[colony->planet].star_index;
                    bool found = false;

                    for (uint32_t j = (uint32_t)candidates->size(); j-- > 0;) {
                        if (world->planets[(*candidates)[j].planet_id].star_index == colony_star_idx) {
                            candidates->erase(candidates->begin() + j);
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        colony->producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
                    }
                }
            }
        }
    }

    void __cdecl Get_Enemy_Strength_On_Turn(AIDATA::s_state* ai, s_ai_movement_context* ctx, uint32_t star_id, int* turn_strengths, int* cumulative_strengths) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        uint32_t star_idx = star_id;
        uint8_t in_nebula = world->stars[star_idx].in_nebula;

        memset(turn_strengths, 0, sizeof(int[ENEMY_STRENGTH_TURN_WINDOW]));

        s_ship_data* ship = world->ships;
        s_ship_data* ship_end = world->ships + world->Ship_Count();

        while (ship < ship_end) {
            uint32_t ship_idx = (uint32_t)(ship - world->ships);

            if (ship->owner != i_ai && ship->status < 3) {
                auto location = legacy_world::Read_Ship_Location(ship);
                if (!location) { ship++; continue; }
                if (location->kind == e_ship_location::at_star && location->star_id == star_idx) {
                    const auto& assessment = ai->ships[ship_idx];
                    turn_strengths[0] += in_nebula ? assessment.strengths_in_nebula[i_ai] : assessment.strengths[i_ai];
                } else {
                    if (location->kind != e_ship_location::at_star && location->star_id == star_idx && ship->turns_left < ENEMY_STRENGTH_TURN_WINDOW) {
                        const auto& assessment = ai->ships[ship_idx];
                        turn_strengths[ship->turns_left] += in_nebula ? assessment.strengths_in_nebula[i_ai] : assessment.strengths[i_ai];
                    }
                }
            }

            ship++;
        }

        cumulative_strengths[0] = turn_strengths[0];
        for (int t = 1; t < ENEMY_STRENGTH_TURN_WINDOW; ++t) {
            cumulative_strengths[t] = turn_strengths[t] + cumulative_strengths[t - 1];
        }
    }

    uint32_t __cdecl Ship_Rough_Strength_At_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, uint32_t ship_idx, uint32_t star_idx) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        uint8_t in_nebula = world->stars[star_idx].in_nebula;
        uint32_t min_val1 = 0xffffffff;
        uint32_t min_val2 = 0xffffffff;
        uint32_t sum1 = 0;
        uint32_t sum2 = 0;
        int32_t count1 = 0;

        if (world->Player_Count() > 0) {
            for (int16_t i = 0; i < world->Player_Count(); i++) {
                if (i_ai != i) {
                    if ((ai->stars[star_idx].in_extended_range >> i) & 1) {
                        if (((ai->stars[star_idx].in_range >> i) & 1) ||
                            world->players[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                            uint16_t strength;
                            if (in_nebula) {
                                strength = (uint16_t)ai->ships[ship_idx].strengths_in_nebula[i];
                            } else {
                                strength = (uint16_t)ai->ships[ship_idx].strengths[i];
                            }

                            count1++;
                            sum1 += strength;
                            if (strength < min_val1) {
                                min_val1 = strength;
                            }
                        }
                    }

                    if (ai->players[i_ai].empire.reachable_players[i] != 0) {
                        uint16_t strength;
                        if (in_nebula) {
                            strength = (uint16_t)ai->ships[ship_idx].strengths_in_nebula[i];
                        } else {
                            strength = (uint16_t)ai->ships[ship_idx].strengths[i];
                        }

                        sum2 += strength;
                        if (strength < min_val2) {
                            min_val2 = strength;
                        }
                    }
                }
            }
        }

        const auto nonplayer = AIDATA::Nonplayer_At(ctx->work, star_idx);
        if (nonplayer) {
            uint32_t strength;
            if (in_nebula) {
                strength = AIPOWER::Ship_Strength_Vs_Ships_In_Nebula(world, ship_idx, *nonplayer);
            } else {
                strength = AIPOWER::Ship_Strength_Vs_Ships(world, ship_idx, *nonplayer);
            }

            strength &= 0xffff;
            if (strength < min_val1) {
                min_val1 = strength;
            }
            return min_val1;
        }

        if (count1 > 0) {
            return (((min_val1 + 2) * (uint32_t)count1) + sum1 - 1) / ((uint32_t)count1 * 2);
        }

        int16_t n_reachable = ai->players[i_ai].empire.reachable_player_count;
        if (n_reachable > 0) {
            return (((min_val2 + 2) * (uint32_t)n_reachable) + sum2 - 1) / ((uint32_t)n_reachable * 2);
        }

        return (uint32_t)((int32_t)world->ships[ship_idx].d.size + 1);
    }

    std::optional<s_attack_choice> Find_Opportunity_Attack(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx, int16_t target_idx, int16_t threshold) {
        if (ai->initialized) {
            if (world->players[player_idx].traits[TRAIT_TRANS_DIMENSIONAL] == 0) {
                if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
                    return std::nullopt;
                }
            }
            auto source = legacy_world::Object_Id(world->players[player_idx].offensive_stagepoint);
            if (!source) return std::nullopt;
            return Find_Opportunity_Attack_Aux(
                ai, world, player_idx,
                target_idx,
                threshold,
                *source);
        }
        return std::nullopt;
    }

    std::optional<AIDATA::s_route> Move_Ships_With_Possible_Intermediate(s_ai_movement_context* ctx, std::span<const uint32_t> ships, uint32_t destination_star) {
        const auto* world = ctx->world;
        if (ships.empty()) return std::nullopt;
        const int16_t i_ai = ctx->player_idx;
        auto orbit = legacy_world::Orbit_Star(&world->ships[ships.front()]);

        s_ship_move_info move_info;
        if (SHIPMOVE::Evaluate_Fleet_Move(ships, destination_star, &move_info) == SHIPMOVE::e_fleet_evaluation::invalid_input) return std::nullopt;

        if (move_info.moving != 0) {
            return AIDATA::s_route{destination_star, (uint8_t)move_info.turns_left};
        }

        std::optional<AIDATA::s_route> best;

        s_colony* colony_ptr = world->colonies;
        if (colony_ptr < world->colonies + world->Colony_Count()) {
            do {
                if (colony_ptr->owner == i_ai) {
                    int16_t current_star_idx = world->planets[colony_ptr->planet].star_index;

                    if (orbit != (uint32_t)current_star_idx && (uint32_t)current_star_idx != destination_star) {
                        SHIPMOVE::Evaluate_Fleet_Move(ships, current_star_idx, &move_info);

                        if (move_info.moving != 0) {
                            // Bug in original game: compares 'moving' flag with 'best_eta' instead of using 'turns_left'
                            if (!best || move_info.moving < best->turns) {
                                int16_t leg1_turns_left = (uint8_t)move_info.turns_left;
                                SHIPMOVE::Evaluate_Fleet_Move(ships, destination_star, &move_info, (uint32_t)current_star_idx);

                                if (move_info.moving != 0) {
                                    leg1_turns_left += (uint8_t)move_info.turns_left;

                                    if (!best || leg1_turns_left < best->turns) {
                                        best = AIDATA::s_route{(uint32_t)current_star_idx, (uint16_t)leg1_turns_left};
                                    }
                                }
                            }
                        }
                    }
                }
                colony_ptr++;
            } while (colony_ptr < world->colonies + world->Colony_Count());
        }

        return best;
    }

    int __cdecl Number_Of_Attacks_Underway(const game_world::s_view* world, int16_t player_idx, int16_t target_player_idx) {
        // attacked_stars tracks which stars have an active threat from the player.
        // The array is sized to the game's maximum number of stars.
        std::vector<uint8_t> attacked_stars(world->Star_Count(), 0);

        s_ship_data* ship = world->ships;
        s_ship_data* ship_end = world->ships + world->Ship_Count();

        while (ship < ship_end) {
            // Only consider ships owned by the player that are combat ships (type 0)
            if (ship->owner == player_idx && ship->d.ship_type == SHIP_TYPE_COMBAT) {
                const auto location = legacy_world::Read_Ship_Location(ship);
                if (ship->status >= 0 && ship->status <= 2 && location && location->star_id < (uint32_t)world->Star_Count()) {
                    const uint32_t star_idx = location->star_id;
                    // Check if the target player has a colony at this star via bitmask.
                    uint8_t has_colony_mask = world->stars[star_idx].has_colony;
                    if ((has_colony_mask >> (target_player_idx & 0x1F)) & 1) {
                        attacked_stars[star_idx] = 1;
                    }
                }
            }

            ship++;
        }

        int total_attacks = 0;
        int32_t num_stars = world->Star_Count();
        for (int16_t i = 0; i < num_stars; ++i) {
            total_attacks += attacked_stars[i];
        }

        return total_attacks;
    }

    void __cdecl Move_To_Stage_Points(AIDATA::s_state* ai, s_ai_movement_context* ctx) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        s_player* p_ai = ctx->player;
        uint8_t warp_beast_caution;
        if (EVENTS::Event_Check_Warp_Beast() != 0 && p_ai->traits[TRAIT_LUCKY] == 0) {
            warp_beast_caution = 1;
        } else {
            warp_beast_caution = 0;
        }

        int32_t total_stage_weight = 0;
        int16_t num_stage_points = 0;
        int16_t num_ships = 0;
        std::unordered_map<uint32_t, uint32_t> stage_point_map;
        std::vector<s_staging_point> stage_points(world->Star_Count());
        std::vector<uint32_t> ship_indices(world->Ship_Count());

        for (int16_t star_idx = 0; star_idx < world->Star_Count(); ++star_idx) {
            if (!AIDATA::Is_Fleeing(ctx->work, star_idx, LAST_FORECAST_TURN)) {
                if ((world->stars[star_idx].is_stagepoint >> i_ai) & 1) {
                    stage_point_map[star_idx] = num_stage_points;
                    stage_points[num_stage_points].star_id = star_idx;
                    stage_points[num_stage_points].strength = 0;
                    total_stage_weight += ai->stars[star_idx].stage_weights[i_ai];
                    num_stage_points++;
                }
            }
        }

        int16_t offensive_stagepoint = p_ai->offensive_stagepoint;
        int16_t war_level = AIBUILD::Player_War_Level(world, i_ai);
        ai->stars[offensive_stagepoint].stage_weights[i_ai] += (war_level + 1) * total_stage_weight;

        for (int16_t ship_idx = 0; ship_idx < world->Ship_Count(); ++ship_idx) {
            s_ship_data* ship = &world->ships[ship_idx];
            if (ship->owner == i_ai &&
                ship->status <= 2 &&
                ship->d.ship_type == SHIP_TYPE_COMBAT) {
                if (ship->mission != 1 && ship->mission != 2 &&
                    ship->mission != 4 && ship->mission != 5 &&
                    ship->mission != 6 && ship->mission != 7) {
                    if (SHIPMOVE::Can_Order_Ship(ship)) {
                        ship_indices[num_ships] = ship_idx;
                        num_ships++;
                    } else {
                        auto destination = legacy_world::Object_Id(SHIPMOVE::Ship_Destination_Star(ship));
                        if (destination) {
                            auto point = stage_point_map.find(*destination);
                            if (point != stage_point_map.end()) {
                                stage_points[point->second].strength += (int16_t)Ship_Rough_Strength_At_Star(ai, ctx, ship_idx, *destination);
                            }
                        }
                    }
                }
            }
        }

        if (num_stage_points == 0) {
            return;
        }

        if (num_stage_points == 1) {
            std::array<uint32_t, 1> ship_list;
            uint32_t target_star = stage_points[0].star_id;

            for (int16_t i = 0; i < num_ships; ++i) {
                uint32_t ship_idx = ship_indices[i];
                bool has_navigator = struct_::Test_Bit_Field((uint8_t*)world->ships[ship_idx].d.special_device_flags, 11);
                ship_list[0] = ship_idx;

                auto location = legacy_world::Read_Ship_Location(&world->ships[ship_idx]);
                if (!location) continue;
                std::optional<uint32_t> intermediate_star;

                if (location->kind == e_ship_location::at_star) {
                    auto route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                        ai, world, i_ai, location->star_id, target_star, has_navigator);
                    if (route) intermediate_star = route->next_star;
                } else {
                    auto route = Move_Ships_With_Possible_Intermediate(ctx, ship_list, target_star);
                    if (route) intermediate_star = route->next_star;
                }

                if (intermediate_star) {
                    world->ships[ship_idx].mission = 3;
                    s_ship_move_info move_info;
                    SHIPMOVE::Evaluate_Fleet_Move(ship_list, *intermediate_star, &move_info);
                    uint8_t turns_left = move_info.turns_left;
                    if (turns_left <= 1 || warp_beast_caution == 0) {
                        SHIPMOVE::Order_Fleet_Move(ship_list, *intermediate_star, &move_info);
                    }
                }
            }
            return;
        }

        RUSS::Shuffle_Uint(world->random_seed, ship_indices.data(), num_ships);

        while (num_ships > 0 && num_stage_points > 0) {
            int32_t best_sp_idx = -1;
            uint32_t best_strength = 0xFFFFFFFF;
            uint32_t best_weight = 1;
            int32_t best_ship_i = -1;
            std::optional<uint32_t> best_ship_star;

            int32_t best_turns = (warp_beast_caution == 1) ? 2 : 255;

            RUSS::Mox_Update();

            for (int16_t i = 0; i < num_stage_points; ++i) {
                uint32_t strength = stage_points[i].strength;
                uint32_t star_idx = stage_points[i].star_id;
                uint32_t weight = ai->stars[star_idx].stage_weights[i_ai];

                bool better = false;
                if (best_sp_idx == -1) {
                    better = true;
                } else {
                    int32_t cmp = RUSS::Compare_Ratios(strength, weight, best_strength, best_weight);
                    if (cmp == -1) {
                        better = true;
                    } else if (cmp == 0 && best_weight < weight) {
                        better = true;
                    }
                }
                if (better) {
                    best_strength = strength;
                    best_sp_idx = i;
                    best_weight = weight;
                }
            }

            uint32_t target_star = stage_points[best_sp_idx].star_id;
            int32_t best_ship_turns = best_turns;

            for (int16_t i = 0; i < num_ships; ++i) {
                uint32_t ship_idx = ship_indices[i];
                bool has_navigator = struct_::Test_Bit_Field((uint8_t*)world->ships[ship_idx].d.special_device_flags, 11);
                auto location = legacy_world::Read_Ship_Location(&world->ships[ship_idx]);
                if (!location) continue;

                if (location->kind == e_ship_location::at_star && location->star_id == target_star) {
                    best_ship_i = i;
                    best_ship_star = target_star;
                    best_ship_turns = 0;
                } else if (location->kind == e_ship_location::at_star) {
                    auto route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                        ai, world, i_ai, location->star_id, target_star, has_navigator);
                    if (route && best_ship_turns > route->turns) {
                        best_ship_i = i;
                        best_ship_star = route->next_star;
                        best_ship_turns = route->turns;
                    }
                } else {
                    const std::array<uint32_t, 1> ship_list{(uint32_t)ship_idx};
                    auto route = Move_Ships_With_Possible_Intermediate(ctx, ship_list, target_star);
                    if (route && best_ship_turns > route->turns) {
                        best_ship_i = i;
                        best_ship_star = route->next_star;
                        best_ship_turns = route->turns;
                    }
                }
            }

            if (best_ship_i == -1 || !best_ship_star) {
                num_stage_points--;
            } else {
                uint32_t ship_idx = ship_indices[best_ship_i];
                const std::array<uint32_t, 1> ship_list{(uint32_t)ship_idx};
                world->ships[ship_idx].mission = 3;
                s_ship_move_info temp_move_info;

                SHIPMOVE::Evaluate_Fleet_Move(ship_list, *best_ship_star, &temp_move_info);
                SHIPMOVE::Order_Fleet_Move(ship_list, *best_ship_star, &temp_move_info);

                int16_t rough_strength = (int16_t)Ship_Rough_Strength_At_Star(ai, ctx, ship_idx, target_star);
                num_ships--;
                stage_points[best_sp_idx].strength += rough_strength;
                ship_indices[best_ship_i] = ship_indices[num_ships];
            }
        }
    }

    void __cdecl Move_NPCs(const game_world::s_view* world) {
        std::vector<uint8_t> star_flag1(world->Star_Count(), 0);
        std::vector<uint8_t> star_flag2(world->Star_Count(), 0);

        std::optional<uint32_t> monster_count;

        for (int16_t ship_idx = 0; ship_idx < world->Ship_Count(); ++ship_idx) {
            s_ship_data* ship = &world->ships[ship_idx];

            if (ship->owner < MAX_PLAYERS) {
                continue;
            }

            if (ship->d.speed == 0) {
                continue;
            }

            if (ship->status == 1) {
                auto location = legacy_world::Read_Ship_Location(ship);
                if (location && location->star_id < star_flag2.size()) star_flag2[location->star_id] = 1;
                continue;
            }

            if (ship->status != 0) {
                continue;
            }

            const auto orbit = legacy_world::Orbit_Star(ship);
            if (!orbit || *orbit >= (uint32_t)world->Star_Count()) continue;
            const uint32_t current_star = *orbit;

            bool try_move = false;

            if (ship->owner != 13) {
                try_move = true;
            } else {
                if (star_flag1[current_star] != 0) {
                    try_move = true;
                }

                star_flag1[current_star] = 1;
                star_flag2[current_star] = 1;

                ship->d.date_of_design++;

                if (ship->d.date_of_design == rules.eel_spawn_frequency) {
                    auto destination = AIMOVE::Npc_Dest(world, ship_idx, star_flag2.data());
                    ship->d.date_of_design = 0;

                    // Assembly uses d.date_of_design at ship offset +0x61 as the 30-turn eel timer.
                    // Preserve the classic spawn gate when the population count is unknown.
                    if (destination) {
                        if (!monster_count) {
                            monster_count = 0;
                            for (int16_t i = 0; i < world->Ship_Count(); ++i) {
                                s_ship_data* s = &world->ships[i];
                                if (s->owner == 13 && s->d.speed > 0 && s->status < 5) {
                                    ++*monster_count;
                                }
                            }
                        }
                    }

                    if (!monster_count || *monster_count < rules.max_eels) {
                        s_ship_design eel_design = {};
                        INITSHIP::Load_Eel_Ship_Design(&eel_design, 1);

                        s_star_data* star = &world->stars[current_star];
                        int16_t new_ship_idx = SHIPMOVE::Create_Nonplayer_Ship(NONPLAYER_SHIP_TYPE_EEL, &eel_design, star->x, star->y);

                        if (new_ship_idx != -1) {
                            s_MSG msg = {};
                            msg.msg_type = MSG_TYPE_EEL_SPAWNED;
                            msg.u.eel.star_idx = (int16_t)current_star;
                            MSG_::Add_Msg_All(msg);

                            // Assembly clears the same +0x61 timer field on the spawned eel.
                            world->ships[new_ship_idx].d.date_of_design = 0;
                        }
                        monster_count = monster_count ? *monster_count + 1 : 0;
                    }
                }
            }

            if (try_move) {
                auto destination = AIMOVE::Npc_Dest(world, ship_idx, star_flag2.data());

                if (!destination) {
                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_EEL_VANISHED;
                    msg.u.eel.star_idx = (int16_t)current_star;
                    MSG_::Add_Msg_All(msg);

                    SHIPS::Kill_Ship(ship_idx);
                } else {
                    const std::array<uint32_t, 1> ships{(uint32_t)ship_idx};
                    s_ship_move_info move_info = {};
                    SHIPMOVE::Evaluate_Fleet_Move(ships, *destination, &move_info);
                    SHIPMOVE::Order_Fleet_Move(ships, *destination, &move_info);
                }
            }
        }
    }

    /**
     * @brief Finds the colony ship that can reach the destination in the minimum number of turns 
     * and moves it, potentially via intermediate stars.
     * 
     * @param ships Candidate ship IDs; the selected ship is removed in-place.
     * @param destination The target destination star index.
     */
    void Dispatch_Unescorted_Colony_Ship(s_ai_movement_context* ctx, std::vector<uint32_t>* ships, uint32_t destination) {
        std::optional<uint32_t> best_ship_array_idx;
        std::optional<AIDATA::s_route> best_route;
        std::array<uint32_t, 1> ship_list;
        s_ship_move_info move_info;

        for (uint32_t i = 0; i < ships->size(); ++i) {
            ship_list[0] = (*ships)[i];
            auto route = Move_Ships_With_Possible_Intermediate(ctx, ship_list, destination);
            if (route && (!best_route || route->turns < best_route->turns)) {
                best_route = route;
                best_ship_array_idx = i;
            }
        }

        if (best_ship_array_idx) {
            ship_list[0] = (*ships)[*best_ship_array_idx];
            SHIPMOVE::Evaluate_Fleet_Move(ship_list, best_route->next_star, &move_info);
            SHIPMOVE::Order_Fleet_Move(ship_list, best_route->next_star, &move_info);

            // Remove the ship from the array by replacing it with the last element
            (*ships)[*best_ship_array_idx] = ships->back();
            ships->pop_back();
        }
    }

    bool __cdecl Star_Has_Monster(const game_world::s_view* world, int16_t star_idx) {
        for (int16_t i = 0; i < world->Ship_Count(); ++i) {
            s_ship_data* ship = &world->ships[i];

            if (ship->owner >= MAX_PLAYERS && legacy_world::Orbit_Star(ship) == (uint32_t)star_idx && ship->status == 0) {
                return true;
            }
        }

        return false;
    }

    std::optional<uint32_t> I_Soonest_Enemy_Colony_Ship(const game_world::s_view* world, std::span<const uint32_t> ships, uint32_t target_location) {
        std::optional<uint32_t> best_idx;
        uint16_t min_turns = 0x7fff;

        for (uint32_t i = 0; i < ships.size(); ++i) {
            uint32_t ship_idx = ships[i];
            s_ship_data* ship = &world->ships[ship_idx];

            auto location = legacy_world::Read_Ship_Location(ship);
            if (!location) continue;
            if (location->kind == e_ship_location::at_star && location->star_id == target_location) {
                return i;
            }

            if (location->kind != e_ship_location::at_star && location->star_id == target_location) {
                uint16_t turns = (uint8_t)ship->turns_left;
                if (turns < min_turns) {
                    min_turns = turns;
                    best_idx = i;
                }
            }
        }

        return best_idx;
    }

    int32_t __cdecl N_Enemy_Stars_Reachable(const game_world::s_view* world, int16_t player_idx, int16_t player_bit, int16_t source_star_idx) {
        int32_t reachable_count = 0;

        for (int16_t i = 0; i < world->Star_Count(); ++i) {
            if ((world->stars[i].has_colony >> (player_bit & 0x1f)) & 1) {
                int16_t ship_range = world->players[player_idx].ship_range;
                int16_t distance = GEO::Parsecs_Between_Stars(i, source_star_idx);

                if (distance <= ship_range) {
                    if (!struct_::Test_Bit_Field(world->stars[i].black_hole_blocks, source_star_idx)) {
                        reachable_count++;
                    }
                }
            }
        }

        return reachable_count;
    }

    int32_t __cdecl Enemy_Colony_Worth_To_Player(AIDATA::s_state* ai, const game_world::s_view* world, int16_t colony_idx, int16_t player_idx) {
        int8_t owner = world->colonies[colony_idx].owner;
        int32_t weight_player;
        int32_t weight_owner;

        if (world->players[player_idx].treaty[owner] == 4) {
            weight_player = 2;
            weight_owner = 4;
        } else if (world->players[player_idx].treaty[owner] == 6) {
            weight_player = 0;
            weight_owner = 6;
        } else {
            weight_player = 1;
            weight_owner = 5;
        }

        if (world->players[player_idx].traits[TRAIT_TELEPATHIC] != 0 && world->players[owner].traits[TRAIT_TELEPATHIC] == 0) {
            weight_player++;
            weight_owner--;
        }

        int16_t planet_idx = world->colonies[colony_idx].planet;

        return (
                   ai->planets[planet_idx].base_values[player_idx] * weight_player +
                   ai->planets[planet_idx].base_values[owner] * weight_owner) /
            6;
    }

    void __cdecl Move_Transports(AIDATA::s_state* ai, s_ai_movement_context* ctx) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        s_player* p_ai = ctx->player;
        int16_t ai_idx = i_ai;
        uint8_t ai_shift = (uint8_t)i_ai;
        struct s_transport_target {
            uint8_t colony_turn = 0;
            uint8_t outpost_turn = 0;
            std::optional<uint32_t> destination;
        };
        std::unordered_map<uint32_t, s_transport_target> targets;
        for (uint32_t i = 0; i < (uint32_t)world->Star_Count(); ++i) targets.emplace(i, s_transport_target{});

        std::vector<uint32_t> transport_ships(world->Ship_Count());
        int16_t num_transports = 0;

        for (int16_t i = 0; i < world->Ship_Count(); i++) {
            s_ship_data* ship = &world->ships[i];

            if (ship->owner == ai_idx && ship->status < 3) {
                auto location = legacy_world::Read_Ship_Location(ship);
                if (!location || location->star_id >= (uint32_t)world->Star_Count()) continue;
                auto& target = targets.at(location->star_id);
                uint8_t ship_type = ship->d.ship_type;

                if (ship_type == SHIP_TYPE_COMBAT) {
                    if (ship->mission == 1) {
                        if (location->kind == e_ship_location::at_star) {
                            target.colony_turn = 1;
                            target.destination = location->star_id;
                        } else {
                            uint8_t turns = ship->turns_left;
                            if (target.colony_turn == 0) {
                                target.colony_turn = turns + 1;
                            } else if (turns + 1 < target.colony_turn) {
                                target.colony_turn = turns + 1;
                            }
                        }
                    } else if (ship->mission == 6) {
                        if (location->kind == e_ship_location::at_star) {
                            target.outpost_turn = 1;
                            target.destination = location->star_id;
                        } else {
                            uint8_t turns = ship->turns_left;
                            if (target.outpost_turn == 0) {
                                target.outpost_turn = turns + 1;
                            } else if (turns + 1 < target.outpost_turn) {
                                target.outpost_turn = turns + 1;
                            }
                        }
                    }
                } else if (ship_type == SHIP_TYPE_TRANSPORT) {
                    const uint32_t loc = location->star_id;
                    if (location->kind == e_ship_location::at_star) {
                        RUSS::Mox_Update();
                        if (world->settings->game_difficulty > GAME_DIFFICULTY_TUTOR) {
                            if (((world->stars[loc].has_colony >> ai_shift) & 1) != 0) {
                                if (world->settings->game_difficulty >= GAME_DIFFICULTY_AVERAGE || p_ai->objectives == PLAYER_OBJECTIVE_MILITARIST) {
                                    int16_t ship_idx = i;
                                    for (int j = 4; j >= 0; j--) {
                                        int16_t planet_idx = world->stars[loc].planet_index[j];
                                        if (planet_idx != -1) {
                                            int16_t colony_idx = world->planets[planet_idx].colony_index;
                                            if (colony_idx != -1) {
                                                s_colony* colony = &world->colonies[colony_idx];
                                                if (colony->owner == ai_idx && colony->outpost_flag == 0) {
                                                    if (ai->colonies[colony_idx].other_player_arrivals ||
                                                        colony->military[0] < ai->colonies[colony_idx].conquered_population * 2) {
                                                        if (invasion::Unload_Transports(colony, &ship_idx, 1) == 1) {
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (ship->status != 5) {
                        transport_ships[num_transports] = i;
                        num_transports++;
                    }
                }
            }
        }

        while (num_transports > 0) {
            num_transports--;
            uint32_t ship_idx = transport_ships[num_transports];
            std::array<uint32_t, 1> ship_arr;
            ship_arr[0] = ship_idx;

            s_ship_data* ship = &world->ships[ship_idx];
            auto location = legacy_world::Read_Ship_Location(ship);
            if (!location) continue;
            uint32_t loc = location->star_id;
            RUSS::Mox_Update();

            if (location->kind == e_ship_location::at_star) {
                uint8_t need_to_move = 1;
                auto dest_star = targets.at(loc).destination;

                if (dest_star) {
                    auto route = Move_Ships_With_Possible_Intermediate(ctx, ship_arr, *dest_star);
                    if (route) {
                        auto destination_index = legacy_world::Index(route->next_star);
                        if (!destination_index) continue;
                        const int16_t out_dest = *destination_index;
                        const uint16_t out_eta = route->turns;
                        if (out_eta >= targets.at(*dest_star).outpost_turn - 1) {
                            s_ship_move_info move_info;
                            SHIPMOVE::Evaluate_Fleet_Move(ship_arr, out_dest, &move_info);
                            SHIPMOVE::Order_Fleet_Move(ship_arr, out_dest, &move_info);
                        } else if (targets.at(*dest_star).colony_turn - out_eta == 1) {
                            s_ship_move_info move_info;
                            SHIPMOVE::Evaluate_Fleet_Move(ship_arr, out_dest, &move_info);
                            SHIPMOVE::Order_Fleet_Move(ship_arr, out_dest, &move_info);
                        }
                        need_to_move = 0;
                    }
                }

                if (need_to_move) {
                    bool found_better = false;
                    std::optional<uint32_t> best_dest;
                    int16_t best_eta = 0x7FFF;

                    for (int16_t i = 0; i < world->Star_Count(); i++) {
                        uint8_t t_colony = targets.at(i).colony_turn;
                        uint8_t t_outpost = targets.at(i).outpost_turn;

                        if (t_colony == 0 && t_outpost == 0) continue;

                        if (t_colony <= FORECAST_TURN_COUNT && targets.at(loc).colony_turn >= 2) {
                            if (AIDATA::Is_Fleeing(ctx->work, i, t_colony - 1)) continue;
                        }
                        if (t_outpost <= FORECAST_TURN_COUNT && targets.at(loc).outpost_turn >= 2) {
                            if (AIDATA::Is_Fleeing(ctx->work, i, t_outpost - 1)) continue;
                        }

                        auto route = Move_Ships_With_Possible_Intermediate(ctx, ship_arr, i);
                        if (!route) continue;
                        auto destination_index = legacy_world::Index(route->next_star);
                        if (!destination_index) continue;
                        const int16_t out_dest = *destination_index;
                        int16_t out_eta = (int16_t)route->turns;

                        bool better = false;
                        bool worse = false;

                        if (out_eta < (int16_t)t_outpost - 1) {
                            better = true;
                            out_eta = (int16_t)t_colony - 1;
                        } else {
                            int16_t t_colony_minus_1 = (int16_t)t_colony - 1;
                            if (out_eta < t_colony_minus_1) {
                                better = true;
                                out_eta = (int16_t)t_colony - 1;
                            } else {
                                if ((t_colony_minus_1 < out_eta && t_outpost == 0) || out_eta >= FORECAST_TURN_COUNT || AIDATA::Is_Fleeing(ctx->work, i, out_eta)) {
                                    worse = true;
                                }
                            }
                        }

                        if (!worse && out_eta < best_eta) {
                            best_dest = out_dest;
                            best_eta = out_eta;
                            found_better = better;
                        }
                    }

                    if (best_dest) {
                        if (!found_better) {
                            s_ship_move_info move_info;
                            SHIPMOVE::Evaluate_Fleet_Move(ship_arr, *best_dest, &move_info);
                            SHIPMOVE::Order_Fleet_Move(ship_arr, *best_dest, &move_info);
                        }
                    } else {
                        int16_t stagepoint = p_ai->offensive_stagepoint;
                        need_to_move = 0;
                        if (!AIDATA::Is_Fleeing(ctx->work, stagepoint, LAST_FORECAST_TURN) &&
                            ((world->stars[stagepoint].blockaded >> ai_shift) & 1) == 0 &&
                            loc != stagepoint) {
                            s_ship_move_info move_info;
                            SHIPMOVE::Evaluate_Fleet_Move(ship_arr, stagepoint, &move_info);
                            if (move_info.moving != 0) {
                                SHIPMOVE::Order_Fleet_Move(ship_arr, stagepoint, &move_info);
                                need_to_move = 1;
                            }
                        }

                        if (!need_to_move && AIDATA::Is_Fleeing(ctx->work, loc, 1)) {
                            int32_t best_eta_32 = 0x7FFF;
                            std::optional<uint32_t> best_dest_32;
                            for (int16_t i = 0; i < world->Star_Count(); i++) {
                                if (!AIDATA::Is_Fleeing(ctx->work, i, LAST_FORECAST_TURN) &&
                                    ((world->stars[i].has_colony >> ai_shift) & 1) != 0) {
                                    s_ship_move_info move_info;
                                    SHIPMOVE::Evaluate_Fleet_Move(ship_arr, i, &move_info);
                                    if (move_info.moving != 0) {
                                        if (ai->stars[i].neighborhood.per_player[i_ai].other_player_arrivals) {
                                            move_info.turns_left -= 24;
                                        }
                                        if ((int32_t)move_info.turns_left < best_eta_32) {
                                            best_eta_32 = move_info.turns_left;
                                            best_dest_32 = i;
                                        }
                                    }
                                }
                            }

                            if (best_dest_32) {
                                s_ship_move_info move_info;
                                SHIPMOVE::Evaluate_Fleet_Move(ship_arr, *best_dest_32, &move_info);
                                SHIPMOVE::Order_Fleet_Move(ship_arr, *best_dest_32, &move_info);
                            }
                        }
                    }
                }
            }
        }
    }

    void __cdecl Gather_Intercepting_Ships(AIDATA::s_state* ai, s_ai_movement_context* ctx, uint32_t* ships_list, int32_t* ship_count, const std::unordered_map<uint32_t, uint32_t>* intercept_map) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        *ship_count = 0;

        for (int32_t i = 0; i < (int32_t)ai->combat_ships.size(); ++i) {
            uint32_t ship_idx = ai->combat_ships[i];
            s_ship_data* ship = &world->ships[ship_idx];

            if (ship->owner == i_ai) {
                bool should_gather = false;

                if (SHIPMOVE::Can_Order_Ship(ship)) {
                    // Check if mission is one of the interceptable/idle types
                    switch (ship->mission) {
                        case 0:
                        case 2:
                        case 3:
                        case 5:
                        case 7:
                            should_gather = true;
                            break;
                        default:
                            break;
                    }
                } else {
                    // Ship is already moving, check if it's arriving at a target star
                    int16_t dest_star = SHIPMOVE::Ship_Destination_Star(ship);
                    if (dest_star != -1 && intercept_map->find((uint32_t)dest_star) != intercept_map->end()) {
                        should_gather = true;
                    }
                }

                if (should_gather) {
                    ships_list[*ship_count] = ship_idx;
                    *ship_count = *ship_count + 1;
                }
            }
        }
    }

    void Assign_New_Colony_Ship_Destinations(AIDATA::s_state* ai, s_ai_movement_context* ctx, std::vector<AIDATA::s_colonization_candidate>* planets) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        s_player* p_ai = ctx->player;
        std::vector<uint32_t> my_colony_ships;
        std::vector<uint32_t> competing_colony_ships;

        for (int16_t i = 0; i < world->Ship_Count(); i++) {
            s_ship_data* ship = &world->ships[i];
            if (ship->d.ship_type == SHIP_TYPE_COLONY) {
                if (ship->owner == i_ai) {
                    if (SHIPMOVE::Can_Order_Ship(ship)) {
                        my_colony_ships.push_back(i);
                    } else if (ship->status < 3) {
                        competing_colony_ships.push_back(i);
                    }
                } else if (ship->status < 3) {
                    competing_colony_ships.push_back(i);
                }
            }
        }

        // The legacy buffer was filled backwards; keep that tie-breaking order.
        std::reverse(competing_colony_ships.begin(), competing_colony_ships.end());

        while (!planets->empty()) {
            if (my_colony_ships.empty()) return;
            int16_t star_idx = world->planets[planets->back().planet_id].star_index;

            if ((~((1 << i_ai)) & ai->stars[star_idx].in_extended_range) == 0 &&
                !ai->stars[star_idx].neighborhood.per_player[i_ai].other_player_arrivals &&
                !AIMOVE::Star_Has_Monster(world, star_idx)) {
                if (world->settings->game_difficulty >= GAME_DIFFICULTY_AVERAGE ||
                    (world->settings->game_difficulty >= GAME_DIFFICULTY_EASY && p_ai->objectives == PLAYER_OBJECTIVE_EXPANSIONIST) ||
                    ((ai->stars[star_idx].in_range >> i_ai) & 1) != 0) {
                    AIMOVE::Dispatch_Unescorted_Colony_Ship(ctx, &my_colony_ships, star_idx);
                }
            } else {
                auto competing_ship = AIMOVE::I_Soonest_Enemy_Colony_Ship(world, competing_colony_ships, star_idx);
                int16_t max_eta = 0x7FFF;

                if (competing_ship) {
                    max_eta = (uint8_t)world->ships[competing_colony_ships[*competing_ship]].turns_left;
                }

                if (!AIMOVE::Dispatch_Escorted_Colony_Ship(ai, ctx, &my_colony_ships, star_idx, max_eta)) {
                    if (competing_ship) {
                        competing_colony_ships[*competing_ship] = competing_colony_ships.front();
                        competing_colony_ships.erase(competing_colony_ships.begin());
                    }
                }
            }

            planets->pop_back();
        }

        if (!my_colony_ships.empty()) {
            std::vector<uint8_t> star_added(world->Star_Count(), 0);

            int16_t num_candidates = 0;
            std::vector<uint32_t> candidate_stars(world->Star_Count());

            for (int16_t i = 0; i < world->Star_Count(); i++) {
                s_star_data* star = &world->stars[i];
                if (((star->is_stagepoint >> i_ai) & 1) != 0 &&
                    ((star->blockaded >> i_ai) & 1) == 0) {
                    if (star_added[i] == 0) {
                        candidate_stars[num_candidates++] = i;
                        star_added[i] = 1;
                    }
                }
            }

            for (uint32_t i = 0; i < my_colony_ships.size(); i++) {
                RUSS::Shuffle_Uint(world->random_seed, candidate_stars.data(), num_candidates);

                s_candidate_sort_context sort{&ai->stars, i_ai, (int16_t)world->Player_Count()};
                qsort(candidate_stars.data(), num_candidates, sizeof(uint32_t), AIMOVE::Sort_candidate_stars, &sort);

                if (num_candidates > 0) {
                    for (int j = 0; j < num_candidates; j++) {
                        uint32_t star_idx = candidate_stars[j];
                        const std::array<uint32_t, 1> ship_idx_list{my_colony_ships[i]};
                        s_ship_move_info move_info{};

                        SHIPMOVE::Evaluate_Fleet_Move(ship_idx_list, star_idx, &move_info);

                        if (move_info.moving != 0) {
                            num_candidates--;
                            SHIPMOVE::Order_Fleet_Move(ship_idx_list, star_idx, &move_info);
                            candidate_stars[j] = candidate_stars[num_candidates];
                            break;
                        }
                    }
                }
            }
        }
    }

    void __cdecl Interceptions(AIDATA::s_state* ai, s_ai_movement_context* ctx, s_interception_target* intercepts, int* count, const std::unordered_map<uint32_t, uint32_t>* star_to_intercept_idx, int is_desperate) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        int32_t num_intercepting_ships;
        auto& work = ctx->work->interception;
        work.ships.resize(world->Ship_Count());
        work.assignments.clear();
        work.stars.clear();

        Gather_Intercepting_Ships(ai, ctx, work.ships.data(), &num_intercepting_ships, star_to_intercept_idx);
        work.ships.resize(num_intercepting_ships);
        Knock_Out_Impossible_Interceptions(ai, ctx, intercepts, count, is_desperate);

        while (true) {
            if (*count <= 0) {
                return;
            }

            RUSS::Mox_Update();

            for (auto& [id, assignment] : work.assignments) assignment.reset();
            for (auto& [id, star] : work.stars) star = {};

            if (num_intercepting_ships > 0) {
                for (int ship_list_index = 0; ship_list_index < num_intercepting_ships; ++ship_list_index) {
                    uint32_t ship_id = work.ships[ship_list_index];
                    auto& assignment = work.assignments[ship_id];
                    bool has_special_bit_11 = struct_::Test_Bit_Field(world->ships[ship_id].d.special_device_flags, 11);
                    int best_target = -1;

                    if (*count > 0) {
                        for (int target_index = 0; target_index < *count; ++target_index) {
                            s_interception_target* target = &intercepts[target_index];
                            uint32_t star_idx = target->star_id;

                            work.stars[star_idx].candidate_strength = Ship_Min_Strength_Versus_Players_At_Star(ai, world, ship_id, target->player_mask, star_idx);

                            std::optional<AIDATA::s_route> route;

                            auto orbit = legacy_world::Orbit_Star(&world->ships[ship_id]);
                            if (orbit) {
                                route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                                    ai, world,
                                    i_ai,
                                    *orbit,
                                    star_idx,
                                    has_special_bit_11);
                            } else {
                                route = AIDATA::Ship_N_Turns_To_Star_With_Intermediate(
                                    ai, world,
                                    ship_id,
                                    star_idx);
                            }

                            if (!route) continue;
                            const uint16_t eta = route->turns;
                            if (eta < FORECAST_TURN_COUNT) {
                                bool update_target = false;

                                if (!assignment || eta < assignment->arrival_turn) {
                                    update_target = true;
                                } else if (eta == assignment->arrival_turn) {
                                    if (best_target != -1) {
                                        auto turn = First_Threat_Turn(&target->strengths, &intercepts[best_target].strengths, eta);
                                        if (!turn || target->strengths[*turn] == 0) {
                                            // Condition implicitly fails, update_target remains false
                                        } else if (intercepts[best_target].strengths[*turn] == 0) {
                                            update_target = true;
                                        } else {
                                            int cmp = RUSS::Compare_Ratios(
                                                work.stars.at(target->star_id).assigned_strength[*turn],
                                                target->strengths[*turn],
                                                work.stars.at(intercepts[best_target].star_id).assigned_strength[*turn],
                                                intercepts[best_target].strengths[*turn]);
                                            if (cmp < 0) {
                                                update_target = true;
                                            }
                                        }
                                    } else {
                                        update_target = true;
                                    }
                                }

                                if (update_target) {
                                    best_target = target_index;
                                    assignment = AIDATA::s_interception_assignment{target->star_id, route->next_star, (uint8_t)eta};
                                }
                            }
                        }
                    }

                    if (best_target != -1) {
                        uint8_t min_eta = assignment->arrival_turn;
                        if (min_eta < FORECAST_TURN_COUNT) {
                            int rem = FORECAST_TURN_COUNT - min_eta;
                            auto& selected = work.stars.at(intercepts[best_target].star_id);
                            uint32_t* ptr = &selected.assigned_strength[min_eta];
                            int16_t min_str = selected.candidate_strength;

                            do {
                                *ptr += min_str;
                                ptr++;
                                rem--;
                            } while (rem != 0);
                        }
                    }
                }
            }

            bool loop_again = false;
            if (*count > 0) {
                for (int target_index = 0; target_index < *count; ++target_index) {
                    s_interception_target* target = &intercepts[target_index];
                    uint32_t star_idx = target->star_id;
                    auto& star_work = work.stars[star_idx];
                    bool target_met = false;

                    star_work.surplus_turn = FORECAST_TURN_COUNT;
                    star_work.sufficient_turn = 0;

                    for (int e = 0; e < FORECAST_TURN_COUNT; ++e) {
                        uint32_t strength_needed = (uint32_t)target->strengths[e];

                        if (strength_needed != 0 || target->strengths[LAST_FORECAST_TURN] == 0) {
                            if (star_work.assigned_strength[e] >= strength_needed) {
                                if (!target_met) {
                                    star_work.sufficient_turn = (char)e;
                                }
                                target_met = true;
                                if (star_work.assigned_strength[e] > strength_needed * 4 + 8) {
                                    star_work.surplus_turn = (char)e;
                                    break;
                                }
                            }
                        }
                    }

                    if (!target_met && is_desperate == 0) {
                        star_work.sufficient_turn = -1;
                        loop_again = true;
                    }
                }
            }

            if (loop_again) {
                (*count)--;
            } else {
                if (num_intercepting_ships > 0) {
                    for (int ship_list_index = 0; ship_list_index < num_intercepting_ships; ++ship_list_index) {
                        uint32_t ship_id = work.ships[ship_list_index];
                        const auto& assignment = work.assignments.at(ship_id);

                        if (assignment && SHIPMOVE::Can_Order_Ship(&world->ships[ship_id])) {
                            s_ship_move_info move_info;
                            const std::array<uint32_t, 1> ship_list{(uint32_t)ship_id};
                            auto destination_index = legacy_world::Index(assignment->next_star);
                            if (!destination_index) continue;
                            int16_t target_star = *destination_index;
                            SHIPMOVE::Evaluate_Fleet_Move(ship_list, target_star, &move_info);

                            const auto& target_work = work.stars.at(assignment->target_star);
                            if (move_info.turns_left <= target_work.surplus_turn) {
                                int16_t ship_loc = HAROLD::XY_To_Star_Id(world->ships[ship_id].x, world->ships[ship_id].y);
                                world->ships[ship_id].mission = 5;

                                bool issue_order = false;
                                if (move_info.turns_left < target_work.sufficient_turn) {
                                    if (world->ships[ship_id].status == 1) {
                                        issue_order = true;
                                    } else if (world->ships[ship_id].status == 2) {
                                        SHIPMOVE::Stop_Ship_Transit(&world->ships[ship_id], legacy_world::Object_Id(ship_loc));
                                    }
                                } else {
                                    issue_order = true;
                                }

                                if (issue_order) {
                                    SHIPMOVE::Order_Fleet_Move(ship_list, target_star, &move_info);
                                    // Redirected ships in transit may not be at any star.
                                    if (ship_loc != -1) {
                                        ai->players[i_ai].combat_ship_groups.erase(ship_loc);
                                    }
                                }
                            }
                        }
                    }
                }

                return;
            }
        }
    }

    void Intercept_Invaders(AIDATA::s_state* ai, s_ai_movement_context* ctx) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        int i = 0;
        std::vector<s_interception_target> intercepts(world->Star_Count());
        std::unordered_map<uint32_t, uint32_t> star_to_intercept_idx;

        if (world->Star_Count() > 0) {
            for (int16_t star_idx = 0; star_idx < world->Star_Count(); star_idx++) {
                if ((world->stars[star_idx].has_colony >> i_ai) & 1) {
                    for (int p = 0; p < world->Player_Count(); p++) {
                        if (p != i_ai) {
                            if (ai->stars[star_idx].enroute_ships[p][LAST_FORECAST_TURN] != 0 &&
                                (ai->stars[star_idx].enroute_ships[p][0] != 0 ||
                                    ai->stars[star_idx].enroute_nonstealthy_ships[p] != 0)) {
                                int eta = 0;
                                while (ai->stars[star_idx].enroute_ships[p][eta] == 0) {
                                    eta++;
                                    if (eta >= FORECAST_TURN_COUNT) break;
                                }

                                if (eta < FORECAST_TURN_COUNT) {
                                    // Keep UI responsive during heavy AI computation
                                    RUSS::Mox_Update();

                                    auto found = star_to_intercept_idx.find(star_idx);
                                    uint32_t idx;
                                    if (found == star_to_intercept_idx.end()) {
                                        intercepts[i].star_id = star_idx;
                                        intercepts[i].earliest_arrival.reset();
                                        star_to_intercept_idx.emplace(star_idx, i);
                                        idx = i;
                                        i++;
                                    } else {
                                        idx = found->second;
                                    }

                                    intercepts[idx].player_mask |= (1 << p);
                                    if (!intercepts[idx].earliest_arrival || eta < *intercepts[idx].earliest_arrival) {
                                        intercepts[idx].earliest_arrival = eta;
                                    }

                                    if (eta < FORECAST_TURN_COUNT) {
                                        for (int j = 0; j < FORECAST_TURN_COUNT - eta; j++) {
                                            intercepts[idx].strengths[eta + j] +=
                                                ai->stars[star_idx].enroute_strengths[p][i_ai][eta + j];
                                        }
                                    }

                                    intercepts[idx].present_players[p] = true;
                                }
                            }
                        }
                    }

                    auto found = star_to_intercept_idx.find(star_idx);
                    if (found != star_to_intercept_idx.end()) {
                        uint32_t idx = found->second;
                        if (intercepts[idx].earliest_arrival && *intercepts[idx].earliest_arrival < FORECAST_TURN_COUNT) {
                            for (int j = 0; j < FORECAST_TURN_COUNT - *intercepts[idx].earliest_arrival; j++) {
                                if (intercepts[idx].strengths[*intercepts[idx].earliest_arrival + j] == 0) {
                                    intercepts[idx].strengths[*intercepts[idx].earliest_arrival + j] = 1;
                                }
                            }
                        }
                    }
                }
            }
        }

        constexpr int nonplayer_owner_count = NONPLAYER_SHIP_TYPE_HYDRA - MAX_PLAYERS + 1;
        std::vector<std::array<s_nonplayer_arrival, nonplayer_owner_count>> alien_str(world->Star_Count());

        bool aliens_present = false;

        for (int16_t s = 0; s < world->Ship_Count(); s++) {
            s_ship_data* ship = &world->ships[s];
            if (ship->owner >= MAX_PLAYERS && ship->owner <= NONPLAYER_SHIP_TYPE_HYDRA && ship->d.speed != 0) {
                const auto location = legacy_world::Read_Ship_Location(ship);
                if (!location || location->star_id >= (uint32_t)world->Star_Count()) continue;
                if (ship->status == 0) // In orbit
                {
                    uint32_t star_idx = location->star_id;
                    if ((world->stars[star_idx].has_colony >> i_ai) & 1) {
                        uint32_t strength = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships(world, s, i_ai);
                        alien_str[star_idx][ship->owner - MAX_PLAYERS].strength += strength;
                        if (alien_str[star_idx][ship->owner - MAX_PLAYERS].strength == 0) {
                            alien_str[star_idx][ship->owner - MAX_PLAYERS].strength = 1;
                        }
                        alien_str[star_idx][ship->owner - MAX_PLAYERS].arrival_turn = 0;
                        aliens_present = true;
                    }
                } else if (ship->status > 0 && ship->status <= 2) // En-route
                {
                    uint32_t star_idx = location->star_id;
                    if ((world->stars[star_idx].has_colony >> i_ai) & 1) {
                        uint32_t strength = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships(world, s, i_ai);
                        alien_str[star_idx][ship->owner - MAX_PLAYERS].strength += strength;
                        if (alien_str[star_idx][ship->owner - MAX_PLAYERS].strength == 0) {
                            alien_str[star_idx][ship->owner - MAX_PLAYERS].strength = 1;
                        }

                        // Preserve the original zero-initialized arrival estimate.
                        if (ship->turns_left < alien_str[star_idx][ship->owner - MAX_PLAYERS].arrival_turn) {
                            alien_str[star_idx][ship->owner - MAX_PLAYERS].arrival_turn = ship->turns_left;
                        }
                        aliens_present = true;
                    }
                }
            }
        }

        if (aliens_present) {
            for (int o = 0; o < nonplayer_owner_count; o++) {
                for (int16_t star_idx = 0; star_idx < world->Star_Count(); star_idx++) {
                    if (alien_str[star_idx][o].strength != 0) {
                        auto found = star_to_intercept_idx.find(star_idx);
                        uint32_t idx;
                        if (found == star_to_intercept_idx.end()) {
                            intercepts[i].star_id = star_idx;
                            intercepts[i].earliest_arrival.reset();
                            star_to_intercept_idx.emplace(star_idx, i);
                            idx = i;
                            i++;
                        } else {
                            idx = found->second;
                        }

                        intercepts[idx].player_mask |= (1 << (o + MAX_PLAYERS));

                        if (!intercepts[idx].earliest_arrival || alien_str[star_idx][o].arrival_turn < *intercepts[idx].earliest_arrival) {
                            intercepts[idx].earliest_arrival = alien_str[star_idx][o].arrival_turn;
                        }

                        if (intercepts[idx].earliest_arrival && *intercepts[idx].earliest_arrival < FORECAST_TURN_COUNT) {
                            for (int j = 0; j < FORECAST_TURN_COUNT - *intercepts[idx].earliest_arrival; j++) {
                                intercepts[idx].strengths[*intercepts[idx].earliest_arrival + j] += alien_str[star_idx][o].strength;
                            }
                        }
                    }
                }
            }
        }

        if (i > 0) {
            bool is_desperate = (ai->players[i_ai].empire.colonized_star_count <= 3);
            AIMOVE::Interceptions(ai, ctx, intercepts.data(), &i, &star_to_intercept_idx, is_desperate);
        }
    }

    void __cdecl Find_Best_Attack_From_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, int16_t star_idx, s_attack_info* target_info, int mission_type, int min_pop) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        s_attack_info target_info_copy;
        s_attack_info* filtered_attack = nullptr;
        uint16_t max_score = 0;
        bool valid_target_found = false;

        s_ship_move_info move_info;
        std::vector<uint8_t> visited_stars(world->Star_Count(), 0);
        std::vector<int8_t> mission_array(world->Colony_Count());
        std::vector<uint16_t> scores(world->Colony_Count(), 0);
        std::vector<uint32_t> ship_list;

        // Initialize copy of target_info
        if (target_info != nullptr) {
            target_info_copy = *target_info;

            filtered_attack = &target_info_copy;
        }

        Gather_Player_Ships_At_Star(ai, ctx, i_ai, star_idx, &ship_list);

        if (world->Colony_Count() > 0) {
            for (int16_t i = 0; i < world->Colony_Count(); i++) {
                int8_t owner = world->colonies[i].owner;

                if (owner != -1) {
                    if (target_info == nullptr) {
                        if (!AI_May_Attack_Player(world, i_ai, owner)) {
                            continue;
                        }
                    } else {
                        if (target_info->target_player != owner) {
                            continue;
                        }
                    }

                    int16_t planet_idx = world->colonies[i].planet;
                    int16_t target_star = world->planets[planet_idx].star_index;

                    if (visited_stars[target_star] == 0) {
                        if (((ai->stars[target_star].in_range >> i_ai) & 1) != 0) {
                            SHIPMOVE::Evaluate_Fleet_Move(ship_list, target_star, &move_info);

                            if (move_info.moving != 0 && (uint8_t)move_info.turns_left < FORECAST_TURN_COUNT) {
                                uint16_t score = Compute_Attack_Worthiness(ai, ctx, i, ship_list, (uint8_t)move_info.turns_left, &mission_array[i], &visited_stars[target_star], filtered_attack, min_pop);
                                scores[i] = score;

                                if (score != 0) {
                                    int8_t mission = mission_array[i];

                                    if (mission == 1) {
                                        if (mission_type != 1) {
                                            scores[i] = 0;
                                            continue;
                                        }
                                        if (max_score < score) {
                                            max_score = score;
                                            if (filtered_attack != nullptr && filtered_attack->target_planet) {
                                                target_info->target_planet = filtered_attack->target_planet;
                                                target_info->colony_value = filtered_attack->colony_value;
                                                target_info->strength_ratio_percent = filtered_attack->strength_ratio_percent;
                                            }
                                        }
                                    } else if (mission == 2) {
                                        if (mission_type != 2) {
                                            scores[i] = 0;
                                            continue;
                                        }
                                        if (max_score < score) {
                                            max_score = score;
                                        }
                                    } else if (mission == 6) {
                                        if (mission_type != 6) {
                                            scores[i] = 0;
                                            continue;
                                        }
                                        if (max_score < score) {
                                            max_score = score;
                                        }
                                    } else {
                                        // Bypasses validation setup entirely, leaves initial calculated score intact
                                        continue;
                                    }

                                    valid_target_found = true;
                                    continue;
                                }
                            }
                        }
                    }
                }
                scores[i] = 0;
            }
        }

        if (valid_target_found && target_info == nullptr) {
            for (int16_t i = 0; i < world->Colony_Count(); i++) {
                // Drop scores below 2/3 of maximum
                if ((uint32_t)scores[i] * 3 < (uint32_t)max_score * 2) {
                    scores[i] = 0;
                }
            }

            auto best_colony = RUSS::Choose_Weighted_Index_U16(world->random_seed, scores.data(), world->Colony_Count());
            if (!best_colony) {
                return;
            }
            const uint32_t best_colony_idx = *best_colony;

            int8_t best_mission = mission_array[best_colony_idx];
            int16_t best_planet_idx = world->colonies[best_colony_idx].planet;
            int16_t best_star_idx = world->planets[best_planet_idx].star_index;

            ai->players[i_ai].combat_ship_groups.erase(star_idx);

            SHIPMOVE::Evaluate_Fleet_Move(ship_list, best_star_idx, &move_info);
            SHIPMOVE::Order_Fleet_Move(ship_list, best_star_idx, &move_info);

            for (auto ship = ship_list.rbegin(); ship != ship_list.rend(); ++ship) {
                world->ships[*ship].mission = best_mission;
            }
        }
    }

    void __cdecl Knock_Out_Impossible_Interceptions(
        AIDATA::s_state* ai, s_ai_movement_context* ctx,
        s_interception_target* intercept_entries,
        int* num_entries,
        int keep_one_entry) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;

        for (int entry_idx = *num_entries - 1; entry_idx >= 0; --entry_idx) {
            s_interception_target* current_entry = &intercept_entries[entry_idx];
            bool can_intercept = false;
            uint32_t star_idx = current_entry->star_id;

            for (int class_idx = 0; class_idx < FORECAST_TURN_COUNT; ++class_idx) {
                uint32_t min_possible = 0xFFFFFFFF;
                uint32_t total_enroute = 0;

                if (world->Player_Count() > 0) {
                    for (int p = 0; p < world->Player_Count(); ++p) {
                        if (current_entry->present_players[p]) {
                            uint32_t possible = (uint32_t)(ai->stars[star_idx].possible_strengths[i_ai][p][class_idx]);

                            if (possible < min_possible) {
                                min_possible = possible;
                            }

                            total_enroute += (uint32_t)(ai->stars[star_idx].enroute_strengths[p][i_ai][class_idx]);
                        }
                    }
                }

                if (total_enroute < min_possible) {
                    can_intercept = true;
                    break;
                }
            }

            if (!can_intercept && (keep_one_entry == 0 || *num_entries > 1)) {
                (*num_entries)--;
                int num_to_move = *num_entries - entry_idx;

                std::move(current_entry + 1, current_entry + 1 + num_to_move, current_entry);

                ctx->work->flee_stars[star_idx].fill(1);
            }
        }
    }

    int __cdecl Sort_npc_dests(const void* a_ptr, const void* b_ptr) {
        auto a = (const s_nonplayer_destination*)a_ptr;
        auto b = (const s_nonplayer_destination*)b_ptr;
        int result;

        result = ((int)a->has_lucky - (int)a->has_unlucky) - ((int)b->has_lucky - (int)b->has_unlucky);
        if (result != 0) {
            return result;
        }

        result = (int)(a->distance_parsecs >> 3) - (int)(b->distance_parsecs >> 3);
        if (result != 0) {
            return result;
        }

        result = (int)b->max_unlucky_empire_strength - (int)a->max_unlucky_empire_strength;
        if (result != 0) {
            return result;
        }

        result = (int)b->max_lucky_empire_strength - (int)a->max_lucky_empire_strength;
        if (result != 0) {
            return result;
        }

        return (int)a->distance_parsecs - (int)b->distance_parsecs;
    }

    std::optional<uint32_t> Npc_Dest(const game_world::s_view* world, uint32_t ship_idx, const uint8_t* star_flags) {
        std::optional<uint32_t> destination;
        auto orbit = legacy_world::Orbit_Star(&world->ships[ship_idx]);
        if (!orbit) return std::nullopt;
        const uint32_t ship_location = *orbit;
        uint32_t player_str[MAX_PLAYERS] = {0};
        std::vector<s_nonplayer_destination> npc_dest(world->Star_Count());

        for (int i = 0; i < world->Player_Count(); i++) {
            if (world->players[i].eliminated == 0) {
                uint32_t str_val = special::isqrt(world->players[i].bc);
                str_val = special::isqrt(str_val);
                player_str[i] = str_val +
                    world->players[i].total_colonies * 4 +
                    (int16_t)(world->players[i].total_research / 4) +
                    world->players[i].total_ships;
            }
        }

        for (int16_t i = 0; i < world->Colony_Count(); i++) {
            int8_t owner = world->colonies[i].owner;
            if (owner != -1) {
                int16_t planet_idx = world->colonies[i].planet;
                int16_t star_idx = world->planets[planet_idx].star_index;

                if (ship_location != star_idx && star_flags[star_idx] == 0) {
                    if (world->players[owner].traits[TRAIT_LUCKY] == 0) {
                        npc_dest[star_idx].has_unlucky = 1;
                        if (npc_dest[star_idx].max_unlucky_empire_strength < player_str[owner]) {
                            npc_dest[star_idx].max_unlucky_empire_strength = player_str[owner];
                        }
                    } else {
                        npc_dest[star_idx].has_lucky = 1;
                        if (npc_dest[star_idx].max_lucky_empire_strength < player_str[owner]) {
                            npc_dest[star_idx].max_lucky_empire_strength = player_str[owner];
                        }
                    }
                    npc_dest[star_idx].star_id = star_idx;
                    npc_dest[star_idx].distance_parsecs = (uint8_t)GEO::Parsecs_Between_Stars(ship_location, star_idx);
                }
            }
        }

        qsort(npc_dest.data(), world->Star_Count(), sizeof(s_nonplayer_destination), Sort_npc_dests);

        int32_t valid_count = 0;
        for (int16_t i = 0; i < world->Star_Count(); i++) {
            if (valid_count >= 7) {
                break;
            }
            if (npc_dest[i].distance_parsecs != 0 && npc_dest[i].distance_parsecs < 20) {
                valid_count++;
                if (game_random::Random_From_Seed(world->random_seed, valid_count) == 1) {
                    destination = npc_dest[i].star_id;
                }
            }
        }

        return destination;
    }

    int8_t __cdecl Player_Is_Hostile_To_Player(const game_world::s_view* world, int player_idx_a, int player_idx_b) {
        int8_t result = 2;
        s_player* p_a = &world->players[player_idx_a];

        // Check if a specific high-level treaty exists (e.g., Non-Aggression or Alliance)
        if (p_a->treaty[player_idx_b] >= 4 && p_a->treaty[player_idx_b] <= 6) {
            result = 1; /* Not Hostile */
        }

        if (result == 2) {
            switch (p_a->personality) {
                case PLAYER_PERSONALITY_XENOPHOBIC:
                case PLAYER_PERSONALITY_RUTHLESS:
                case PLAYER_PERSONALITY_AGGRESSIVE:
                case PLAYER_PERSONALITY_ERRATIC:
                    result = 1; /* Not Hostile */
                    break;
                case PLAYER_PERSONALITY_HONORABLE:
                case PLAYER_PERSONALITY_PACIFIST:
                    result = 0; /* Hostile */
                    break;
                default:
                    // Keep result as 2 to proceed to objective check
                    break;
            }

            if (result != 2) {
                return result;
            }

            // Fallback to checking objectives if personality was not recognized or is neutral
            if (p_a->objectives == PLAYER_OBJECTIVE_MILITARIST) {
                result = 1;
            }
        }

        // Final safety fallback: default to Not Hostile (1)
        if (result == 2) {
            result = 1;
        }

        return result;
    }

    int8_t __cdecl AI_May_Attack_Player(const game_world::s_view* world, int16_t player_idx, int16_t target_idx) {
        int8_t may_attack = 0;

        if (target_idx != player_idx) {
            s_player* plr = &world->players[player_idx];

            if (plr->stop_blockading_duration[target_idx] == 0 &&
                plr->trade_treaty[target_idx] == 0 &&
                plr->research_treaty[target_idx] == 0 &&
                plr->treaty[target_idx] >= 4 &&
                plr->treaty[target_idx] <= 6) {
                may_attack = 1;
            }
        }

        return may_attack;
    }

    uint16_t __cdecl Compute_Attack_Worthiness(
        AIDATA::s_state* ai, s_ai_movement_context* ctx,
        int16_t colony_idx,
        std::span<const uint32_t> ships,
        uint8_t eta,
        int8_t* out_attack_type,
        uint8_t* out_flee,
        s_attack_info* target_info,
        int32_t min_pop) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        s_player* p_ai = ctx->player;
        s_colony* colony = &world->colonies[colony_idx];
        int8_t owner = colony->owner;
        int16_t planet_idx = colony->planet;
        int16_t star_idx = world->planets[planet_idx].star_index;

        uint32_t attack_worthiness = 0;
        uint32_t total_space_strength = 0;
        uint32_t total_bomb_strength = 0;
        bool in_nebula = world->stars[star_idx].in_nebula != 0;
        uint32_t hostility_bonus = (uint8_t)Player_Is_Hostile_To_Player(world, i_ai, owner);
        uint32_t total_ship_size = 0;

        bool can_attack = true;
        uint32_t n_pops = (uint8_t)colony->n_pops;
        if ((int32_t)n_pops < min_pop && colony->outpost_flag == 0 && p_ai->personality != PLAYER_PERSONALITY_RUTHLESS && n_pops != 0) {
            for (uint32_t i = 0; i < n_pops; ++i) {
                if (POP::Get_Race(colony->pop[i]) == (uint32_t)i_ai) {
                    can_attack = false;
                }
            }
        }

        if (!can_attack) {
            return 0;
        }

        int16_t shield = COLCALC::Colony_Planetary_Shield(colony);

        for (uint32_t ship_idx : ships) {
            uint16_t space_str = in_nebula ? ai->ships[ship_idx].strengths_in_nebula[owner] : ai->ships[ship_idx].strengths[owner];
            total_space_strength += space_str;

            uint16_t bomb_str = ai->ships[ship_idx].bomb_strengths[shield];
            total_bomb_strength += bomb_str;

            total_ship_size += (1 << (world->ships[ship_idx].d.size & 0x1F));
        }

        if (target_info == nullptr && total_ship_size > 0x96) {
            uint32_t scaled = (total_space_strength / 0x96) * total_ship_size;
            if (total_space_strength < scaled) {
                total_space_strength = scaled;
            }
        }

        uint32_t possible_space_strength = ai->stars[star_idx].possible_strengths[owner][i_ai][eta];
        uint32_t added_strength = 0;
        int producing_idx = 0;
        int32_t current_bc = world->players[owner].bc;
        int32_t spent = colony->production_spent;

        for (int i = 0; i < 7; ++i) {
            if (current_bc + spent < 1 || eta <= producing_idx) break;

            int16_t prod_id = colony->producing[i];
            if (COLBLDG::Colony_Production_Is_Queued_Ship(prod_id)) {
                int16_t ship_design_idx = COLBLDG::Colony_Production_Ship_Index(prod_id);
                if (world->ships[ship_design_idx].d.ship_type == SHIP_TYPE_COMBAT) {
                    int32_t cost = world->ships[ship_design_idx].d.cost;
                    if (cost > spent) {
                        int32_t buy_cost = COLCALC::Colony_Cost_To_Buy_Given_Product(colony, prod_id, spent);
                        if (buy_cost <= current_bc) {
                            spent = 0;
                            added_strength += AIPOWER::Ship_Strength_Vs_Ships(world, ship_design_idx, i_ai);
                            producing_idx++;
                            current_bc -= buy_cost;
                        }
                    } else {
                        added_strength += AIPOWER::Ship_Strength_Vs_Ships(world, ship_design_idx, i_ai);
                        spent -= cost;
                        producing_idx++;
                    }
                }
            }
        }

        possible_space_strength += added_strength;

        if (total_bomb_strength + total_space_strength == 0 || (total_space_strength * 3 < possible_space_strength * 4)) {
            *out_flee = 1;
            if (eta < FORECAST_TURN_COUNT) {
                for (int i = eta; i < FORECAST_TURN_COUNT; ++i) {
                    ctx->work->flee_stars[star_idx][i] = 1;
                }
            }
        } else {
            uint32_t colony_worth = Enemy_Colony_Worth_To_Player(ai, world, colony_idx, i_ai);
            uint32_t defending_space = possible_space_strength + (AIPOWER::Colony_Space_Strength_Vs_Player(world, colony_idx, i_ai) & 0xFFFF);
            uint32_t defending_ground = AIPOWER::Colony_Ground_Strength_Vs_Player(world, colony_idx, i_ai) & 0xFFFF;

            uint32_t attack_space = total_space_strength + hostility_bonus;
            uint32_t attack_ground = total_bomb_strength + hostility_bonus;

            bool success = true;
            if (attack_space <= defending_space || attack_ground <= defending_ground) {
                success = false;
            }

            if (success) {
                *out_attack_type = 1;
                if (defending_space * 3 < total_space_strength) {
                    total_space_strength = 3;
                    defending_space = 1;
                } else if (total_space_strength == 0) {
                    total_space_strength = 1;
                    defending_space = 1;
                }

                if (defending_ground * 3 < total_bomb_strength) {
                    total_bomb_strength = 3;
                    defending_ground = 1;
                } else if (total_bomb_strength == 0) {
                    total_bomb_strength = 1;
                    defending_ground = 1;
                }

                if (target_info == nullptr && total_space_strength == 3 && defending_space == 1) {
                    *out_attack_type = 6;
                }

                if (defending_space >= 0x10000) {
                    defending_space >>= 8;
                    total_space_strength >>= 8;
                }
                if (defending_ground >= 0x10000) {
                    defending_ground >>= 8;
                    total_bomb_strength >>= 8;
                }

                int32_t reachable = N_Enemy_Stars_Reachable(world, i_ai, owner, star_idx);
                uint32_t reachable_sq = special::isqrt(reachable * 100);

                uint32_t attack_strength = (colony_worth * total_space_strength) / defending_space;
                attack_strength = (attack_strength * total_bomb_strength) / defending_ground;

                attack_worthiness = (reachable_sq * attack_strength) / 10;

                if (target_info != nullptr) {
                    uint32_t target_val = (total_space_strength * 100 / defending_space * total_bomb_strength) / defending_ground;

                    target_info->strength_ratio_percent = (int16_t)target_val;

                    if (target_info->minimum_strength_ratio_percent < (int16_t)target_val) {
                        target_info->target_planet = planet_idx;
                        target_info->colony_value = (int16_t)colony_worth;
                    } else {
                        attack_worthiness = 0;
                    }
                }
            } else {
                *out_attack_type = 2;
                if (possible_space_strength * 3 < total_space_strength) {
                    total_space_strength = 3;
                    possible_space_strength = 1;
                } else if (total_space_strength == 0) {
                    total_space_strength = 1;
                    possible_space_strength = 1;
                }

                attack_worthiness = (colony_worth * total_space_strength) / (possible_space_strength * 2);

                if ((world->stars[star_idx].blockaded >> owner) & 1) {
                    uint8_t blocked_by = world->stars[star_idx].blockaded_by[owner];
                    if ((blocked_by & ~(1 << i_ai)) != 0 || ((blocked_by >> i_ai) & 1) == 0) {
                        attack_worthiness = 0;
                    }
                }
            }
        }

        if (eta < 2) {
            attack_worthiness += attack_worthiness / 6;
        }

        if (attack_worthiness > 0xFFFF) {
            attack_worthiness = 0xFFFF;
        }

        return (uint16_t)attack_worthiness;
    }

    uint16_t __cdecl Ship_Min_Strength_Versus_Players_At_Star(AIDATA::s_state* ai, const game_world::s_view* world, uint32_t ship_idx, uint16_t player_mask, uint32_t star_idx) {
        uint16_t min_strength = 0x7fff;
        bool in_nebula = world->stars[star_idx].in_nebula != 0;

        // Check empire players (indices 0 to 7)
        if ((uint8_t)player_mask != 0) {
            for (int32_t i = 0; i < world->Player_Count(); ++i) {
                if ((player_mask >> i) & 1) {
                    const auto& assessment = ai->ships[ship_idx];
                    uint16_t strength = in_nebula ? assessment.strengths_in_nebula[i] : assessment.strengths[i];
                    if (strength < min_strength) {
                        min_strength = strength;
                    }
                }
            }
        }

        // Check special entities or high-index players (indices 8 to 14)
        if ((uint8_t)(player_mask >> MAX_PLAYERS) != 0) {
            for (int32_t i = MAX_PLAYERS; i < 15; ++i) {
                if ((player_mask >> i) & 1) {
                    uint16_t strength;
                    if (in_nebula) {
                        strength = AIPOWER::Ship_Strength_Vs_Ships_In_Nebula(world, ship_idx, (int16_t)i);
                    } else {
                        strength = AIPOWER::Ship_Strength_Vs_Ships(world, ship_idx, (int16_t)i);
                    }

                    if (strength < min_strength) {
                        min_strength = strength;
                    }
                }
            }
        }

        return min_strength;
    }

    bool Dispatch_Escorted_Colony_Ship(AIDATA::s_state* ai, s_ai_movement_context* ctx, std::vector<uint32_t>* colony_ships, uint32_t star_idx, int16_t max_turns) {
        const auto* world = ctx->world;
        const int16_t i_ai = ctx->player_idx;
        s_player* p_ai = ctx->player;
        int32_t enemy_strength[ENEMY_STRENGTH_TURN_WINDOW] = {0};
        int32_t enemy_strength2[ENEMY_STRENGTH_TURN_WINDOW];
        int32_t total_enemy_strength[ENEMY_STRENGTH_TURN_WINDOW];

        for (int i = 0; i < ENEMY_STRENGTH_TURN_WINDOW; ++i) {
            total_enemy_strength[i] = 0;
        }

        Get_Enemy_Strength_On_Turn(ai, ctx, star_idx, enemy_strength, enemy_strength2);

        if (world->Player_Count() > 0) {
            for (int p = 0; p < world->Player_Count(); ++p) {
                if (p != i_ai) {
                    int8_t treaty = p_ai->treaty[p];
                    if (treaty < 1 || treaty > 3 || world->players[p].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        for (int t = 0; t < ENEMY_STRENGTH_TURN_WINDOW; ++t) {
                            total_enemy_strength[t] += ai->stars[star_idx].possible_strengths[p][i_ai][t];
                        }
                    }
                }
            }
        }

        enum class e_strategy { current_route, redirect_transit, assign_escorts };
        struct s_escort_choice {
            uint32_t colony_position;
            uint16_t turns;
            e_strategy strategy;
            uint32_t destination;
            uint32_t escort_count;
            uint32_t available_escorts;
        };
        std::optional<s_escort_choice> best;
        std::vector<uint32_t> fleet;

        if (!colony_ships->empty()) {
            uint32_t c = 0;
            do {
                int32_t num_escorts_total = 0;
                fleet.assign(1, (*colony_ships)[c]);
                const auto location = legacy_world::Read_Ship_Location(&world->ships[fleet[0]]);
                if (!location) { ++c; continue; }

                if (location->kind == e_ship_location::at_star) {
                    const auto* group = ai->players[i_ai].Find_Combat_Ships(location->star_id);
                    if (group) {
                        int32_t total_escort_strength = 0;
                        bool is_valid_strength = false;
                        if (max_turns < ENEMY_STRENGTH_TURN_WINDOW || enemy_strength2[ENEMY_STRENGTH_TURN_WINDOW - 1] != 0) {
                            is_valid_strength = true;
                        }

                        uint32_t all_ship_idx = group->next;
                        while (all_ship_idx < group->end) {
                            uint32_t s_idx = ai->combat_ships[all_ship_idx];
                            if (world->ships[s_idx].owner == i_ai && legacy_world::Same_Location_Tag(&world->ships[s_idx], &world->ships[fleet[0]])) {
                                all_ship_idx++;
                                total_escort_strength += (int16_t)AIMOVE::Ship_Rough_Strength_At_Star(ai, ctx, s_idx, star_idx);
                                num_escorts_total++;
                            } else {
                                break;
                            }
                        }

                        all_ship_idx = group->next;
                        int32_t strength = 0;
                        while (all_ship_idx < group->end) {
                            uint32_t s_idx = ai->combat_ships[all_ship_idx];
                            if (world->ships[s_idx].owner == i_ai && legacy_world::Same_Location_Tag(&world->ships[s_idx], &world->ships[fleet[0]])) {
                                all_ship_idx++;
                                strength += (int16_t)AIMOVE::Ship_Rough_Strength_At_Star(ai, ctx, s_idx, star_idx);
                                fleet.push_back(s_idx);

                                auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(ctx, fleet, star_idx);
                                if (!route || route->turns > max_turns) {
                                    break;
                                }
                                const uint32_t dest_star = route->next_star;
                                const uint16_t turns = route->turns;

                                if (is_valid_strength || total_escort_strength <= strength * 2) {
                                    if (turns < ENEMY_STRENGTH_TURN_WINDOW && (!best || turns < best->turns)) {
                                        int32_t t2 = turns + 2;
                                        if (t2 >= ENEMY_STRENGTH_TURN_WINDOW) {
                                            t2 = ENEMY_STRENGTH_TURN_WINDOW - 1;
                                        }
                                        if (enemy_strength2[t2] < strength) {
                                            bool can_escort = true;
                                            int32_t t = turns + 1;
                                            if (total_enemy_strength[turns] > strength && total_escort_strength > strength) {
                                                can_escort = false;
                                            }
                                            if (t < ENEMY_STRENGTH_TURN_WINDOW) {
                                                for (; t < ENEMY_STRENGTH_TURN_WINDOW; ++t) {
                                                    if (enemy_strength[t] > strength) {
                                                        can_escort = false;
                                                    }
                                                }
                                            }
                                            if (can_escort) {
                                                best = s_escort_choice{c, turns, e_strategy::assign_escorts,
                                                    dest_star, (uint32_t)fleet.size() - 1, (uint32_t)num_escorts_total};
                                            }
                                        }
                                    }
                                }

                                if (!is_valid_strength && total_escort_strength <= strength * 2) {
                                    break;
                                }
                            } else {
                                break;
                            }
                        }
                    }
                } else {
                    int32_t strength = 0;
                    for (int16_t i = 0; i < world->Ship_Count(); ++i) {
                        s_ship_data* ship = &world->ships[i];
                        if (ship->owner == i_ai && legacy_world::Same_Location_Tag(ship, &world->ships[fleet[0]])) {
                            if (ship->status < 3 && legacy_world::Same_Ship_Position(ship, &world->ships[fleet[0]])) {
                                strength += (int16_t)AIMOVE::Ship_Rough_Strength_At_Star(ai, ctx, i, star_idx);
                                fleet.push_back(i);
                            }
                        }
                    }

                    if (location->kind == e_ship_location::in_flight && location->star_id == star_idx) {
                        uint8_t turns = world->ships[fleet[0]].turns_left;
                        if (turns < ENEMY_STRENGTH_TURN_WINDOW && turns <= max_turns) {
                            if ((!best || turns < best->turns) && enemy_strength2[turns] < strength) {
                                bool can_escort = true;
                                for (int32_t t = turns + 1; t < ENEMY_STRENGTH_TURN_WINDOW; ++t) {
                                    if (enemy_strength[t] > strength) {
                                        can_escort = false;
                                        break;
                                    }
                                }
                                if (can_escort) {
                                    best = s_escort_choice{c, turns, e_strategy::current_route, star_idx,
                                        (uint32_t)fleet.size() - 1, (uint32_t)fleet.size() - 1};
                                }
                            }
                        }
                    } else {
                        auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(ctx, fleet, star_idx);
                        if (route && route->turns < ENEMY_STRENGTH_TURN_WINDOW && route->turns <= max_turns) {
                            const uint32_t dest_star = route->next_star;
                            const uint16_t turns = route->turns;
                            if ((!best || turns < best->turns) && enemy_strength2[turns] < strength) {
                                bool can_escort = true;
                                for (int32_t t = turns + 1; t < ENEMY_STRENGTH_TURN_WINDOW; ++t) {
                                    if (enemy_strength[t] > strength) {
                                        can_escort = false;
                                        break;
                                    }
                                }
                                if (can_escort) {
                                    best = s_escort_choice{c, turns, e_strategy::redirect_transit, dest_star,
                                        (uint32_t)fleet.size() - 1, (uint32_t)fleet.size() - 1};
                                }
                            }
                        }
                    }
                }
                ++c;
            } while (c < colony_ships->size());
        }

        if (best) {
            fleet.assign(1, (*colony_ships)[best->colony_position]);

            if (best->strategy == e_strategy::redirect_transit) {
                for (int16_t i = 0; i < world->Ship_Count(); ++i) {
                    s_ship_data* ship = &world->ships[i];
                    if (ship->owner == i_ai && legacy_world::Same_Location_Tag(ship, &world->ships[fleet[0]])) {
                        if (ship->status < 3 && legacy_world::Same_Ship_Position(ship, &world->ships[fleet[0]])) {
                            ship->mission = 4;
                            fleet.push_back(i);
                        }
                    }
                }
            } else if (best->strategy == e_strategy::assign_escorts) {
                auto location = legacy_world::Orbit_Star(&world->ships[fleet[0]]);
                if (!location) return true;
                auto* group = ai->players[i_ai].Find_Combat_Ships(*location);
                if (group == nullptr) return true;
                uint32_t all_ship_idx = group->next;
                if (best->escort_count > 0) {
                    for (uint32_t i = 0; i < best->escort_count; ++i) {
                        uint32_t s_idx = ai->combat_ships[all_ship_idx++];
                        world->ships[s_idx].mission = 4;
                        fleet.push_back(s_idx);
                    }
                }
                if (best->escort_count < best->available_escorts) {
                    group->next += best->escort_count;
                    if (group->next == group->end) ai->players[i_ai].combat_ship_groups.erase(*location);
                } else {
                    ai->players[i_ai].combat_ship_groups.erase(*location);
                }
            }

            if (best->strategy != e_strategy::current_route) {
                s_ship_move_info move_info;
                SHIPMOVE::Evaluate_Fleet_Move(fleet, best->destination, &move_info);
                SHIPMOVE::Order_Fleet_Move(fleet, best->destination, &move_info);
            }

            (*colony_ships)[best->colony_position] = colony_ships->back();
            colony_ships->pop_back();
            return true;
        }

        return false;
    }

    std::optional<s_attack_choice> Find_Opportunity_Attack_Aux(
        AIDATA::s_state* ai, const game_world::s_view* world,
        int16_t player_id,
        int16_t target_player_idx,
        int16_t min_value,
        uint32_t star_id) {
        auto star_index = legacy_world::Index(star_id);
        if (!star_index) return std::nullopt;

        if (ai->players[player_id].Find_Combat_Ships(star_id) &&
            world->settings->game_difficulty > GAME_DIFFICULTY_EASY) {
            AIDATA::s_movement_workspace work;
            s_ai_movement_context context{player_id, &world->players[player_id], &work, world};
            s_attack_info attack_info{};

            attack_info.target_player = target_player_idx;
            attack_info.minimum_strength_ratio_percent = min_value;

            Find_Best_Attack_From_Star(ai, &context, *star_index, &attack_info, 1, 2);

            if (attack_info.target_planet) {
                return s_attack_choice{*attack_info.target_planet, attack_info.colony_value, (int32_t)attack_info.strength_ratio_percent};
            }

        }
        return std::nullopt;
    }

    int __cdecl Sort_candidate_stars(void* context, const void* a, const void* b) {
        const auto* sort = static_cast<s_candidate_sort_context*>(context);
        const int16_t i_ai = sort->player_idx;
        const auto* candidate_star_a = static_cast<const uint32_t*>(a);
        const auto* candidate_star_b = static_cast<const uint32_t*>(b);
        uint32_t weakest_strength_a = 0xffffffffu;
        uint32_t weakest_strength_b = 0xffffffffu;

        if (sort->player_count > 0) {
            for (int16_t player_idx = 0; player_idx < sort->player_count; ++player_idx) {
                if (player_idx != i_ai) {
                    const uint32_t star_a = *candidate_star_a;
                    if ((sort->stars->at(star_a).in_range >> (player_idx & 0x1f)) & 1) {
                        const uint32_t strength_a =
                            (uint32_t)sort->stars->at(star_a).possible_strengths[i_ai][player_idx][0];
                        if (strength_a < weakest_strength_a) {
                            weakest_strength_a = strength_a;
                        }

                        const uint32_t star_b = *candidate_star_b;
                        const uint32_t strength_b =
                            (uint32_t)sort->stars->at(star_b).possible_strengths[i_ai][player_idx][0];
                        if (strength_b < weakest_strength_b) {
                            weakest_strength_b = strength_b;
                        }
                    }
                }
            }
        }

        if (weakest_strength_a == 0xffffffffu) {
            weakest_strength_a = 0;
        }
        if (weakest_strength_b == 0xffffffffu) {
            weakest_strength_b = 0;
        }

        if (weakest_strength_b > weakest_strength_a) {
            return -1;
        }

        return weakest_strength_b < weakest_strength_a;
    }
} // namespace AIMOVE