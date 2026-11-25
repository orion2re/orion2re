#include "pch.h"
#include "fleet_movement.h"

#include "legacy_ids.h"
#include "ship_location_queries.h"
#include "ship_ids.h"

namespace AIMOVE {
    std::optional<uint32_t> First_Threat_Turn(const std::array<int32_t, AIDATA::FORECAST_TURN_COUNT>* first, const std::array<int32_t, AIDATA::FORECAST_TURN_COUNT>* second, uint32_t from) {
        for (uint32_t turn = from; turn < AIDATA::FORECAST_TURN_COUNT; ++turn) {
            if ((*first)[turn] != 0 || (*second)[turn] != 0) return turn;
        }
        return std::nullopt;
    }
    using game::e_ship_location;
    using AIDATA::FORECAST_TURN_COUNT;
    using AIDATA::LAST_FORECAST_TURN;
    struct s_candidate_sort_context {
        const std::unordered_map<id_star, AIDATA::s_star_assessment>* stars;
        id_player player_id;
        int16_t player_count;
    };

    int __cdecl Sort_colonizable(const void* a, const void* b) {
        auto p1 = (const AIDATA::s_colonization_candidate*)a;
        auto p2 = (const AIDATA::s_colonization_candidate*)b;
        return p1->contextual_value - p2->contextual_value;
    }

    void __cdecl Move_All_AI(AIDATA::s_state* ai, game::s_game_state* world, int16_t difficulty, int16_t strategic_combat) {
        for (int16_t i = 0; i < world->player_count; ++i) {
            s_player& plr = world->players[i];

            if (plr.objectives != PLAYER_OBJECTIVE_HUMAN && plr.ship_speed != 0) {
                if (plr.traits[TRAIT_TRANS_DIMENSIONAL] != 0 || EVENTS::Event_Check_Hyperspace_Flux() == 0) {
                    Move_AI(ai, world, difficulty, strategic_combat, legacy_world::Player_Id(i));
                }
            }
        }

        Move_NPCs(world);
    }

    void __cdecl Launch_Sneak_Attack(AIDATA::s_state* ai, s_ai_movement_context* ctx) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        s_player* p_ai = ctx->player;
        if (p_ai->sneak_attack_planet != game::NO_PLANET) {
            if (AIBUILD::Player_War_Level(world, legacy_world::Player_Id(i_ai)) != 0) {
                p_ai->sneak_attack_planet = game::NO_PLANET;
            }
        }

        const auto target_id = p_ai->sneak_attack_planet;
        if (world->planets.contains(target_id)) {
            const auto& target = world->planets.at(target_id);
            game::id_colony colony_idx = target.colony_id;
            const auto star = target.star_id;
            uint8_t target_player = (uint8_t)legacy_world::Encode_Player_Slot(p_ai->sneak_attack_player);

            if (colony_idx != game::NO_COLONY && legacy_world::Encode_Player_Slot(world->colonies.at(colony_idx).owner) == target_player) {
                const auto stage_point = game::Star_Key_At_Row(*world, world->players[i_ai].offensive_stagepoint);

                if (stage_point != game::NO_STAR && ai->players[i_ai].Find_Combat_Ships(stage_point)) {
                    std::vector<id_ship> ship_list;
                    s_ship_move_info move_info;

                    Gather_Player_Ships_At_Star(ai, ctx, legacy_world::Player_Id(i_ai), stage_point, &ship_list);
                    SHIPMOVE::Evaluate_Fleet_Move(*world, ship_list, {game::e_star_destination::star, star}, &move_info);

                    if (move_info.moving != 0) {
                        for (id_ship id : ship_list) {
                            if (auto* ship = [&] { const auto entry = world->ships.find(id); return entry == world->ships.end() ? nullptr : &entry->second; }()) ship->mission = 6;
                        }
                        SHIPMOVE::Order_Fleet_Move(*world, ship_list, {game::e_star_destination::star, star}, &move_info);
                        ai->players[i_ai].combat_ship_groups.erase(stage_point);
                    }
                }
            }

            if (ai->stars[star].enroute_strengths[i_ai][target_player][1] != 0) {
                const auto legacy_player = ctx->player_idx.id;
                if (legacy_player < MAX_PLAYERS) diplomac::Declare_War(legacy_world::Decode_Player_Slot(legacy_player), legacy_world::Decode_Player_Slot(target_player));
            }
        }
    }

    void Move_AI(AIDATA::s_state* ai, game::s_game_state* world, int16_t difficulty, int16_t strategic_combat, id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        auto& work = ai->players[player_idx].movement;
        work = {};
        s_ai_movement_context context{player_id, &world->players[player_idx], &work, world, difficulty, strategic_combat};
        s_ai_movement_context* ctx = &context;
        const uint32_t i_ai = player_idx;
        const s_player* p_ai = ctx->player;

        Get_Colonizable_Planets(ai, ctx, &work.colonizable);
        qsort(work.colonizable.data(), work.colonizable.size(), sizeof(AIDATA::s_colonization_candidate), Sort_colonizable);

        Mark_Flee_Stars(world, &work, player_id);

        for (int row = 0; row < static_cast<int16_t>(world->ship_slots.size()); ++row) {
            if (world->ship_slots[row] == ::game::NO_SHIP) continue;
            const auto& ship = ship_slots::At(std::as_const(*world), row);
            if (factions::Legacy_Code(ship.owner) >= MAX_PLAYERS && ship.status == 0) {
                const auto orbit = ships::Orbit_Star(ship.position);
                if (orbit) work.nonplayer_here[*orbit] = legacy_world::Player_Id(factions::Legacy_Code(ship.owner));
            }
        }

        Delete_Already_Assigned_Planets(ctx, &work.colonizable);

        Launch_Sneak_Attack(ai, ctx);

        Launch_Attacks(ai, ctx, 6, 2);

        Intercept_Invaders(ai, ctx);

        Launch_Attacks(ai, ctx, 1, 2);

        Assign_New_Colony_Ship_Destinations(ai, ctx, &work.colonizable);

        Launch_Attacks(ai, ctx, 2, 2);

        Launch_Attacks(ai, ctx, 6, 0);

        Launch_Attacks(ai, ctx, 1, 0);

        Launch_Attacks(ai, ctx, 2, 0);

        Move_Transports(ai, ctx);

        Move_To_Stage_Points(ai, ctx);

        if (ai->players[i_ai].empire.contact_count == 0 ||
            difficulty == GAME_DIFFICULTY_TUTOR) {
            Move_To_Unexplored_Stars(ai, ctx);
        }

        work.complete = true;
    }

    std::optional<s_threat> Player_Threatens_Player(AIDATA::s_state* ai, game::s_game_state* world, int16_t difficulty, int16_t strategic_combat, id_player player, id_player target) {
        const uint32_t plr1 = legacy_world::Slot(player);
        const uint32_t plr2 = legacy_world::Slot(target);

        // Check if player is Trans-Dimensional or if Hyperspace Flux is active
        if (world->players[plr1].traits[TRAIT_TRANS_DIMENSIONAL] == 0) {
            if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
                return std::nullopt;
            }
        }

        if (std::ssize(world->stars) < 1) {
            return std::nullopt;
        }

        for (const auto star_id : world->star_order) {
            uint32_t current_strength = (uint32_t)ai->players[plr1].empire.strength_vs_player[plr2];
            uint32_t possible_strength = (uint32_t)ai->stars[star_id].possible_strengths[plr1][plr2][0];

            // AI logic: check if existing strength ratio allows for a threat/attack opportunity
            if ((current_strength >> 2) <= (possible_strength / 10)) {
                auto attack = Find_Opportunity_Attack_Aux(ai, world, difficulty, strategic_combat, player, target, 150, star_id);
                if (attack) return s_threat{star_id, *attack};
            }
        }
        return std::nullopt;
    }

    std::optional<id_planet> Best_Uncolonized_Undestined_Planet(AIDATA::s_state* ai, game::s_game_state* world, int16_t difficulty, int16_t strategic_combat, id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        AIDATA::s_movement_workspace work;
        s_ai_movement_context context{player_id, &world->players[player_idx], &work, world, difficulty, strategic_combat};
        Get_Colonizable_Planets(ai, &context, &work.colonizable);
        qsort(work.colonizable.data(), work.colonizable.size(), sizeof(AIDATA::s_colonization_candidate), Sort_colonizable);
        Delete_Already_Assigned_Planets(&context, &work.colonizable);

        if (!work.colonizable.empty()) {
            return work.colonizable.back().planet_id;
        }

        return std::nullopt;
    }

    void __cdecl Move_To_Unexplored_Stars(AIDATA::s_state* ai, s_ai_movement_context* ctx) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        std::vector<int8_t> star_flags_0(std::ssize(world->stars), 0);
        int32_t enemy_strength_1[ENEMY_STRENGTH_TURN_WINDOW];
        int32_t enemy_strength_2[ENEMY_STRENGTH_TURN_WINDOW];
        std::vector<id_ship> ship_indices(static_cast<int16_t>(world->ship_slots.size()));
        s_ship_move_info move_info;
        std::array<id_ship, 1> single_ship_list;


        int32_t num_ships_selected = 0;
        if (static_cast<int16_t>(world->ship_slots.size()) > 0) {
            for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
                if (world->ship_slots[i] == ::game::NO_SHIP) continue;
                if (factions::Legacy_Code(ship_slots::At(*world, i).owner) == i_ai) {
                    int8_t status = ship_slots::At(*world, i).status;
                    const auto location = ships::Location(ship_slots::At(*world, i).position);
                    if (!location || !world->stars.contains(location->star_id)) continue;
                    const auto loc = game::Star_Row(*world, location->star_id);

                    if (status == 0) {
                        if ((ship_slots::At(*world, i).mission == 0 || ship_slots::At(*world, i).mission == 3) &&
                            ship_slots::At(*world, i).d.ship_type == SHIP_TYPE_COMBAT) {
                            ship_indices[num_ships_selected] = game::Ship_Key_At_Row(*world, i);
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

        if (std::ssize(world->stars) > 0) {
            for (int16_t i = 0; i < std::ssize(world->star_order); ++i) {
                const auto star_id = world->star_order[i];
                auto& star_record = world->stars.at(star_id);
                if ((ai->stars[star_id].in_extended_range.test_slot(i_ai)) != 0 &&
                    (star_record.visited.test_slot(i_ai)) == 0 &&
                    star_flags_0[i] == 0 &&
                    (ctx->difficulty != GAME_DIFFICULTY_TUTOR || star_record.has_colony.none())) {
                    int32_t best_ship_idx = -1;
                    int32_t min_turns = 255;

                    Get_Enemy_Strength_On_Turn(ai, ctx, star_id, enemy_strength_1, enemy_strength_2);

                    if (num_ships_selected == 0) {
                        return;
                    }

                    for (int32_t j = 0; j < num_ships_selected; ++j) {
                        const auto ship_entry = world->ships.find(ship_indices[j]);
                        const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
                        if (!ship || ship->status == 5) continue;
                        bool has_device = struct_::Read_Bit_Field(ship->d.special_device_flags, 11);

                        if (has_device || (ai->stars[star_id].in_range.test_slot(i_ai)) != 0) {
                            auto orbit = ships::Orbit_Star(ship->position);
                            if (!orbit) continue;
                            auto turns = AIDATA::N_Turns_From_To(&ai->routes, *orbit, star_id, ctx->player_idx);

                            if (turns && *turns < min_turns && *turns < ENEMY_STRENGTH_TURN_WINDOW && enemy_strength_2[*turns] == 0) {
                                min_turns = *turns;
                                best_ship_idx = j;
                            }
                        }
                    }

                    if (best_ship_idx != -1) {
                        single_ship_list[0] = ship_indices[best_ship_idx];
                        num_ships_selected--;
                        ship_indices[best_ship_idx] = ship_indices[num_ships_selected];

                        SHIPMOVE::Evaluate_Fleet_Move(*world, single_ship_list, {game::e_star_destination::star, star_id}, &move_info);
                        if (auto* ship = [&] { const auto entry = world->ships.find(single_ship_list[0]); return entry == world->ships.end() ? nullptr : &entry->second; }()) ship->mission = 7;
                        SHIPMOVE::Order_Fleet_Move(*world, single_ship_list, {game::e_star_destination::star, star_id}, &move_info);
                    }
                }
            }
        }
    }

    void __cdecl Launch_Attacks(AIDATA::s_state* ai, s_ai_movement_context* ctx, int mission_type, int min_pop) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        if (std::ssize(world->stars) > 0) {
            for (const auto star_id : world->star_order) {
                if (ai->players[i_ai].Find_Combat_Ships(star_id)) {
                    AIMOVE::Find_Best_Attack_From_Star(ai, ctx, star_id, nullptr, mission_type, min_pop);
                }
            }
        }
    }

    void Mark_Flee_Stars(game::s_game_state* world, AIDATA::s_movement_workspace* work, id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        // Iterate through all players to check for active blockading durations.
        for (int i = 0; i < world->player_count; ++i) {
            // Check if player i is currently being asked to stop blockading by player_idx.
            if (world->players[player_idx].stop_blockading_duration[i] != 0) {
                // If so, iterate through stars to identify retreat/flee targets.
                for (const auto& [star_id, star] : world->stars) {
                    // Only mark the star if it hasn't been processed yet.
                    if (!AIDATA::Is_Fleeing(work, star_id, 0)) {
                        // Check if player i has a colony at this star (has_colony is a bitmask).
                        if (game::Has_Player_Colony(star, i)) {
                            // Mark the entire forecast window for this star.
                            work->flee_stars[star_id].fill(1);
                        }
                    }
                }
            }
        }
    }


    void Get_Colonizable_Planets(AIDATA::s_state* ai, s_ai_movement_context* ctx, std::vector<AIDATA::s_colonization_candidate>* candidates) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        candidates->clear();
        for (const auto id : world->planet_order) {
            const auto* planet = &world->planets.at(id);
            // Only consider uncolonized planets.
            // PLANET_TYPE_PLANET is a standard colonizable planet (excluding gas giants, asteroids).
            if (planet->colony_id == game::NO_COLONY && planet->planet_type == PLANET_TYPE_PLANET) {
                // Each byte in the array represents a star, with bits corresponding to players
                // who have that star within their extended travel range.
                const auto star_range_mask = ai->stars[planet->star_id].in_extended_range;

                if (star_range_mask.test_slot(i_ai)) {
                    candidates->push_back({id, (int32_t)(uint16_t)ai->planets[id].contextual_values[i_ai]});
                }
            }

        }
    }

    void Delete_Already_Assigned_Planets(s_ai_movement_context* ctx, std::vector<AIDATA::s_colonization_candidate>* candidates) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        s_player* p_ai = ctx->player;
        if (p_ai->tech_applications[TECH_APP_HYPERSPACE_COMMUNICATIONS] != TECH_RESEARCH_STATUS_RESEARCHED) {
            for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
                if (world->ship_slots[i] == ::game::NO_SHIP) continue;
                s_ship_data* ship = &ship_slots::At(*world, i);

                if (factions::Legacy_Code(ship->owner) == i_ai &&
                    ship->d.ship_type == SHIP_TYPE_COLONY &&
                    ship->status == 1 &&
                    !SHIPMOVE::Can_Order_Ship(*world, *ship)) {
                    auto location = ships::Location(ship->position);
                    if (!location || location->target != game::e_ship_target::star) continue;
                    for (uint32_t j = (uint32_t)candidates->size(); j-- > 0;) {
                        if (world->planets.at((*candidates)[j].planet_id).star_id == location->star_id) {
                            candidates->erase(candidates->begin() + j);
                            break;
                        }
                    }
                }
            }
        }

        if (world->players[i_ai].objectives != PLAYER_OBJECTIVE_HUMAN) {
            for (auto& [roster_id, roster_colony] : world->colonies) {
                s_colony* colony = &roster_colony;

                if (legacy_world::Encode_Player_Slot(colony->owner) == i_ai && colony->producing[0] == BUILDING_COLONY_BASE) {
                    const auto colony_star = world->planets.at(colony->planet_id).star_id;
                    bool found = false;

                    for (uint32_t j = (uint32_t)candidates->size(); j-- > 0;) {
                        if (world->planets.at((*candidates)[j].planet_id).star_id == colony_star) {
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

    void __cdecl Get_Enemy_Strength_On_Turn(AIDATA::s_state* ai, s_ai_movement_context* ctx, id_star star_id, int* turn_strengths, int* cumulative_strengths) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        const id_star star_idx = star_id;
        uint8_t in_nebula = world->stars.at(star_idx).in_nebula;

        memset(turn_strengths, 0, sizeof(int[ENEMY_STRENGTH_TURN_WINDOW]));

        for (int ship_idx = 0; ship_idx < static_cast<int16_t>(world->ship_slots.size()); ++ship_idx) {
            if (world->ship_slots[ship_idx] == ::game::NO_SHIP) continue;
            const auto* ship = &ship_slots::At(std::as_const(*world), ship_idx);

            if (factions::Legacy_Code(ship->owner) != i_ai && ship->status < 3) {
                auto location = ships::Location(ship->position);
                if (!location) continue;
                if (location->kind == e_ship_location::at_star && location->star_id == star_idx) {
                    const auto& assessment = ai->ships[world->ship_slots[ship_idx]];
                    turn_strengths[0] += in_nebula ? assessment.strengths_in_nebula[i_ai] : assessment.strengths[i_ai];
                } else {
                    if (location->kind != e_ship_location::at_star && location->star_id == star_idx && ship->turns_left < ENEMY_STRENGTH_TURN_WINDOW) {
                        const auto& assessment = ai->ships[world->ship_slots[ship_idx]];
                        turn_strengths[ship->turns_left] += in_nebula ? assessment.strengths_in_nebula[i_ai] : assessment.strengths[i_ai];
                    }
                }
            }

        }

        cumulative_strengths[0] = turn_strengths[0];
        for (int t = 1; t < ENEMY_STRENGTH_TURN_WINDOW; ++t) {
            cumulative_strengths[t] = turn_strengths[t] + cumulative_strengths[t - 1];
        }
    }

    uint32_t __cdecl Ship_Rough_Strength_At_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, id_ship ship_id, id_star star_id) {
        auto* world = ctx->world;
        const auto ship_entry = world->ships.find(ship_id);
        const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
        if (!ship) return 0;
        const auto& assessment = ai->stars[star_id];
        const auto& ship_assessment = ai->ships[ship_id];
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        uint8_t in_nebula = world->stars.at(star_id).in_nebula;
        uint32_t min_val1 = 0xffffffff;
        uint32_t min_val2 = 0xffffffff;
        uint32_t sum1 = 0;
        uint32_t sum2 = 0;
        int32_t count1 = 0;

        if (world->player_count > 0) {
            for (int16_t i = 0; i < world->player_count; i++) {
                if (i_ai != i) {
                    if (assessment.in_extended_range.test_slot(i)) {
                        if ((assessment.in_range.test_slot(i)) ||
                            world->players[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                            uint16_t strength;
                            if (in_nebula) {
                                strength = ship_assessment.strengths_in_nebula[i];
                            } else {
                                strength = ship_assessment.strengths[i];
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
                            strength = ship_assessment.strengths_in_nebula[i];
                        } else {
                            strength = ship_assessment.strengths[i];
                        }

                        sum2 += strength;
                        if (strength < min_val2) {
                            min_val2 = strength;
                        }
                    }
                }
            }
        }

        const auto nonplayer = AIDATA::Nonplayer_At(ctx->work, star_id);
        if (nonplayer) {
            uint32_t strength;
            if (in_nebula) {
                strength = AIPOWER::Ship_Strength_Vs_Ships_In_Nebula(world, ctx->strategic_combat, ship_id, legacy_world::Player_Id(*nonplayer));
            } else {
                strength = AIPOWER::Ship_Strength_Vs_Ships(world, ctx->strategic_combat, ship_id, legacy_world::Player_Id(*nonplayer));
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

        return (uint32_t)((int32_t)ship->d.size + 1);
    }

    std::optional<s_attack_choice> Find_Opportunity_Attack(AIDATA::s_state* ai, game::s_game_state* world, int16_t difficulty, int16_t strategic_combat, id_player player_id, id_player target_id, int16_t threshold) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        const uint32_t target_idx = legacy_world::Slot(target_id);
        if (ai->initialized) {
            if (world->players[player_idx].traits[TRAIT_TRANS_DIMENSIONAL] == 0) {
                if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
                    return std::nullopt;
                }
            }
            const auto source = game::Star_Key_At_Row(*world, world->players[player_idx].offensive_stagepoint);
            if (source == game::NO_STAR) return std::nullopt;
            return Find_Opportunity_Attack_Aux(
                ai, world, difficulty, strategic_combat, player_id,
                target_id,
                threshold,
                source);
        }
        return std::nullopt;
    }


    int Number_Of_Attacks_Underway(game::s_game_state* world, id_player player_id, id_player target_player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        const uint32_t target_player_idx = legacy_world::Slot(target_player_id);
        // attacked_stars tracks which stars have an active threat from the player.
        // The array is sized to the game's maximum number of stars.
        std::vector<uint8_t> attacked_stars(std::ssize(world->stars), 0);

        for (int row = 0; row < static_cast<int16_t>(world->ship_slots.size()); ++row) {
            if (world->ship_slots[row] == ::game::NO_SHIP) continue;
            const auto* ship = &ship_slots::At(std::as_const(*world), row);
            // Only consider ships owned by the player that are combat ships (type 0)
            if (factions::Legacy_Code(ship->owner) == player_idx && ship->d.ship_type == SHIP_TYPE_COMBAT) {
                const auto location = ships::Location(ship->position);
                if (ship->status >= 0 && ship->status <= 2 && location && location->target == game::e_ship_target::star) {
                    const auto row = game::Star_Row(*world, location->star_id);
                    if (row >= 0 && game::Has_Player_Colony(world->stars.at(location->star_id), target_player_idx)) {
                        attacked_stars[row] = 1;
                    }
                }
            }

        }

        int total_attacks = 0;
        int32_t num_stars = std::ssize(world->stars);
        for (int16_t i = 0; i < num_stars; ++i) {
            total_attacks += attacked_stars[i];
        }

        return total_attacks;
    }

    void __cdecl Move_To_Stage_Points(AIDATA::s_state* ai, s_ai_movement_context* ctx) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
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
        std::unordered_map<id_star, uint32_t> stage_point_map;
        std::vector<s_staging_point> stage_points(std::ssize(world->stars));
        std::vector<id_ship> ship_indices(static_cast<int16_t>(world->ship_slots.size()));

        for (const auto star_id_ : world->star_order) {
            if (!AIDATA::Is_Fleeing(ctx->work, star_id_, LAST_FORECAST_TURN)) {
                if (world->stars.at(star_id_).is_stagepoint.test_slot(i_ai)) {
                    stage_point_map[star_id_] = num_stage_points;
                    stage_points[num_stage_points].star_id = star_id_;
                    stage_points[num_stage_points].strength = 0;
                    total_stage_weight += ai->stars[star_id_].stage_weights[i_ai];
                    num_stage_points++;
                }
            }
        }

        int16_t offensive_stagepoint = p_ai->offensive_stagepoint;
        int16_t war_level = AIBUILD::Player_War_Level(world, ctx->player_idx);
        ai->stars[game::Star_Key_At_Row(*world, offensive_stagepoint)].stage_weights[i_ai] += (war_level + 1) * total_stage_weight;

        for (int16_t ship_idx = 0; ship_idx < static_cast<int16_t>(world->ship_slots.size()); ++ship_idx) {
            if (world->ship_slots[ship_idx] == ::game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(*world, ship_idx);
            if (factions::Legacy_Code(ship->owner) == i_ai &&
                ship->status <= 2 &&
                ship->d.ship_type == SHIP_TYPE_COMBAT) {
                if (ship->mission != 1 && ship->mission != 2 &&
                    ship->mission != 4 && ship->mission != 5 &&
                    ship->mission != 6 && ship->mission != 7) {
                    if (SHIPMOVE::Can_Order_Ship(*world, *ship)) {
                        ship_indices[num_ships] = game::Ship_Key_At_Row(*world, ship_idx);
                        num_ships++;
                    } else {
                        const auto destination = ships::Location(ship->position);
                        if (destination) {
                            auto point = stage_point_map.find(destination->star_id);
                            if (point != stage_point_map.end()) {
                                stage_points[point->second].strength += (int16_t)Ship_Rough_Strength_At_Star(ai, ctx, game::Ship_Key_At_Row(*world, ship_idx), destination->star_id);
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
            std::array<id_ship, 1> ship_list;
            const id_star target_star = stage_points[0].star_id;

            for (int16_t i = 0; i < num_ships; ++i) {
                const auto ship_entry = world->ships.find(ship_indices[i]);
                auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
                if (!ship || ship->status == 5) continue;
                bool has_navigator = struct_::Test_Bit_Field(ship->d.special_device_flags, 11);
                ship_list[0] = ship_indices[i];

                auto location = ships::Location(ship->position);
                if (!location) continue;
                std::optional<id_star> intermediate_star;

                if (location->kind == e_ship_location::at_star) {
                    auto route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                        ai, world, ctx->player_idx, location->star_id, target_star, has_navigator);
                    if (route) intermediate_star = route->next_star;
                } else {
                    auto route = Move_Ships_With_Possible_Intermediate(ctx, ship_list, target_star);
                    if (route) intermediate_star = route->next_star;
                }

                if (intermediate_star) {
                    ship->mission = 3;
                    s_ship_move_info move_info;
                    SHIPMOVE::Evaluate_Fleet_Move(*world, ship_list, {game::e_star_destination::star, *intermediate_star}, &move_info);
                    uint8_t turns_left = move_info.turns_left;
                    if (turns_left <= 1 || warp_beast_caution == 0) {
                        SHIPMOVE::Order_Fleet_Move(*world, ship_list, {game::e_star_destination::star, *intermediate_star}, &move_info);
                    }
                }
            }
            return;
        }

        game_random::Shuffle<game::id_ship>({ship_indices.data(), (size_t)num_ships});

        while (num_ships > 0 && num_stage_points > 0) {
            int32_t best_sp_idx = -1;
            uint32_t best_strength = 0xFFFFFFFF;
            uint32_t best_weight = 1;
            int32_t best_ship_i = -1;
            std::optional<id_star> best_ship_star;

            int32_t best_turns = (warp_beast_caution == 1) ? 2 : 255;

            for (int16_t i = 0; i < num_stage_points; ++i) {
                uint32_t strength = stage_points[i].strength;
                const id_star star_idx = stage_points[i].star_id;
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

            const id_star target_star = stage_points[best_sp_idx].star_id;
            int32_t best_ship_turns = best_turns;

            for (int16_t i = 0; i < num_ships; ++i) {
                const auto ship_entry = world->ships.find(ship_indices[i]);
                const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
                if (!ship || ship->status == 5) continue;
                bool has_navigator = struct_::Read_Bit_Field(ship->d.special_device_flags, 11);
                auto location = ships::Location(ship->position);
                if (!location) continue;

                if (location->kind == e_ship_location::at_star && location->star_id == target_star) {
                    best_ship_i = i;
                    best_ship_star = target_star;
                    best_ship_turns = 0;
                } else if (location->kind == e_ship_location::at_star) {
                    auto route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                        ai, world, ctx->player_idx, location->star_id, target_star, has_navigator);
                    if (route && best_ship_turns > route->turns) {
                        best_ship_i = i;
                        best_ship_star = route->next_star;
                        best_ship_turns = route->turns;
                    }
                } else {
                    const std::array<id_ship, 1> ship_list{ship_indices[i]};
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
                const auto key = ship_indices[best_ship_i];
                const auto ship_entry = world->ships.find(key);
                auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
                if (!ship) {
                    ship_indices[best_ship_i] = ship_indices[--num_ships];
                    continue;
                }
                const std::array<id_ship, 1> ship_list{key};
                ship->mission = 3;
                s_ship_move_info temp_move_info;

                SHIPMOVE::Evaluate_Fleet_Move(*world, ship_list, {game::e_star_destination::star, *best_ship_star}, &temp_move_info);
                SHIPMOVE::Order_Fleet_Move(*world, ship_list, {game::e_star_destination::star, *best_ship_star}, &temp_move_info);

                int16_t rough_strength = (int16_t)Ship_Rough_Strength_At_Star(ai, ctx, key, target_star);
                num_ships--;
                stage_points[best_sp_idx].strength += rough_strength;
                ship_indices[best_ship_i] = ship_indices[num_ships];
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
    void Dispatch_Unescorted_Colony_Ship(s_ai_movement_context* ctx, std::vector<id_ship>* ships, id_star destination) {
        std::optional<uint32_t> best_ship_array_idx;
        std::optional<AIDATA::s_route> best_route;
        std::array<id_ship, 1> ship_list;
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
            SHIPMOVE::Evaluate_Fleet_Move(*ctx->world, ship_list, {game::e_star_destination::star, best_route->next_star}, &move_info);
            SHIPMOVE::Order_Fleet_Move(*ctx->world, ship_list, {game::e_star_destination::star, best_route->next_star}, &move_info);

            // Remove the ship from the array by replacing it with the last element
            (*ships)[*best_ship_array_idx] = ships->back();
            ships->pop_back();
        }
    }

    bool Star_Has_Monster(game::s_game_state* world, id_star star_idx) {
        for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
            if (world->ship_slots[i] == ::game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(*world, i);

            if (factions::Legacy_Code(ship->owner) >= MAX_PLAYERS && ships::Orbit_Star(ship->position) == star_idx && ship->status == 0) {
                return true;
            }
        }

        return false;
    }

    std::optional<uint32_t> I_Soonest_Enemy_Colony_Ship(game::s_game_state* world, std::span<const id_ship> ships, id_star target_location) {
        std::optional<uint32_t> best_idx;
        uint16_t min_turns = 0x7fff;

        for (uint32_t i = 0; i < ships.size(); ++i) {
            const auto ship_entry = world->ships.find(ships[i]);
            const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
            if (!ship || ship->status == 5) continue;

            auto location = ships::Location(ship->position);
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

    int32_t N_Enemy_Stars_Reachable(game::s_game_state* world, id_player player_id, int16_t player_bit, id_star source_star) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        const auto source_position = game::Star_Row(*world, source_star);
        if (source_position < 0) return 0;
        const auto& source = world->stars.at(source_star);
        int32_t reachable_count = 0;

        for (const auto& [id, star] : world->stars) {
            if (star.has_colony.test_slot(player_bit & 0x1f)) {
                int16_t ship_range = world->players[player_idx].ship_range;
                const auto distance = GEO::Parsecs_Between_Points(star.x, star.y, source.x, source.y);

                if (distance <= ship_range) {
                    if (!struct_::Read_Bit_Field(star.black_hole_blocks, source_position)) {
                        reachable_count++;
                    }
                }
            }
        }

        return reachable_count;
    }

    int32_t Enemy_Colony_Worth_To_Player(AIDATA::s_state* ai, game::s_game_state* world, id_colony colony_id, id_player player_id) {
        const game::id_colony colony_idx = colony_id;
        const uint32_t player_idx = legacy_world::Slot(player_id);
        int8_t owner = legacy_world::Encode_Player_Slot(world->colonies.at(colony_idx).owner);
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

        const auto planet_idx = world->colonies.at(colony_idx).planet_id;

        return (
                   ai->planets[planet_idx].base_values[player_idx] * weight_player +
                   ai->planets[planet_idx].base_values[owner] * weight_owner) /
            6;
    }

    void __cdecl Move_Transports(AIDATA::s_state* ai, s_ai_movement_context* ctx) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        s_player* p_ai = ctx->player;
        const uint32_t ai_idx = i_ai;
        uint8_t ai_shift = (uint8_t)i_ai;
        struct s_transport_target {
            uint8_t colony_turn = 0;
            uint8_t outpost_turn = 0;
            std::optional<id_star> destination;
        };
        std::unordered_map<id_star, s_transport_target> targets;
        for (const auto& [key, star] : world->stars) targets.try_emplace(key);

        std::vector<id_ship> transport_ships(static_cast<int16_t>(world->ship_slots.size()));
        int16_t num_transports = 0;

        for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); i++) {
            if (world->ship_slots[i] == ::game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(*world, i);

            if (factions::Legacy_Code(ship->owner) == ai_idx && ship->status < 3) {
                auto location = ships::Location(ship->position);
                if (!location) continue;
                const auto found = targets.find(location->star_id);
                if (found == targets.end()) continue;
                auto& target = found->second;
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
                    const auto& star = world->stars.at(location->star_id);
                    if (location->kind == e_ship_location::at_star) {
                        if (ctx->difficulty > GAME_DIFFICULTY_TUTOR) {
                            if (game::Has_Player_Colony(star, ai_shift)) {
                                if (ctx->difficulty >= GAME_DIFFICULTY_AVERAGE || p_ai->objectives == PLAYER_OBJECTIVE_MILITARIST) {
                                    const game::id_ship transport[] = {ship->id};
                                    for (int j = 4; j >= 0; j--) {
                                        const auto planet_idx = star.orbit_planet_ids[j];
                                        if (planet_idx != game::NO_PLANET) {
                                            game::id_colony colony_idx = world->planets.at(planet_idx).colony_id;
                                            if (colony_idx != game::NO_COLONY) {
                                                s_colony* colony = &world->colonies.at(colony_idx);
                                                if (legacy_world::Encode_Player_Slot(colony->owner) == ai_idx && colony->outpost_flag == 0) {
                                                    if (ai->colonies[colony_idx].other_player_arrivals ||
                                                        colony->military[0] < ai->colonies[colony_idx].conquered_population * 2) {
                                                        if (world == &game::current_state && invasion::Unload_Transports(*colony, transport) == 1) {
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
                        transport_ships[num_transports] = game::Ship_Key_At_Row(*world, i);
                        num_transports++;
                    }
                }
            }
        }

        while (num_transports > 0) {
            const std::array<id_ship, 1> ship_arr{transport_ships[--num_transports]};
            const auto ship_entry = world->ships.find(ship_arr.front());
            auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
            if (!ship || ship->status == 5) continue;
            auto location = ships::Location(ship->position);
            if (!location) continue;
            const auto loc = location->star_id;
            if (!targets.contains(loc)) continue;

            if (location->kind == e_ship_location::at_star) {
                uint8_t need_to_move = 1;
                auto dest_star = targets.at(loc).destination;

                if (dest_star) {
                    auto route = Move_Ships_With_Possible_Intermediate(ctx, ship_arr, *dest_star);
                    if (route) {
                        const auto out_dest = route->next_star;
                        const uint16_t out_eta = route->turns;
                        if (out_eta >= targets.at(*dest_star).outpost_turn - 1) {
                            s_ship_move_info move_info;
                            SHIPMOVE::Evaluate_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, out_dest}, &move_info);
                            SHIPMOVE::Order_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, out_dest}, &move_info);
                        } else if (targets.at(*dest_star).colony_turn - out_eta == 1) {
                            s_ship_move_info move_info;
                            SHIPMOVE::Evaluate_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, out_dest}, &move_info);
                            SHIPMOVE::Order_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, out_dest}, &move_info);
                        }
                        need_to_move = 0;
                    }
                }

                if (need_to_move) {
                    bool found_better = false;
                    std::optional<id_star> best_dest;
                    int16_t best_eta = 0x7FFF;

                    for (const auto star_id : world->star_order) {
                        uint8_t t_colony = targets.at(star_id).colony_turn;
                        uint8_t t_outpost = targets.at(star_id).outpost_turn;

                        if (t_colony == 0 && t_outpost == 0) continue;

                        if (t_colony <= FORECAST_TURN_COUNT && targets.at(loc).colony_turn >= 2) {
                            if (AIDATA::Is_Fleeing(ctx->work, star_id, t_colony - 1)) continue;
                        }
                        if (t_outpost <= FORECAST_TURN_COUNT && targets.at(loc).outpost_turn >= 2) {
                            if (AIDATA::Is_Fleeing(ctx->work, star_id, t_outpost - 1)) continue;
                        }

                        auto route = Move_Ships_With_Possible_Intermediate(ctx, ship_arr, star_id);
                        if (!route) continue;
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
                                if ((t_colony_minus_1 < out_eta && t_outpost == 0) || out_eta >= FORECAST_TURN_COUNT || AIDATA::Is_Fleeing(ctx->work, star_id, out_eta)) {
                                    worse = true;
                                }
                            }
                        }

                        if (!worse && out_eta < best_eta) {
                            best_dest = route->next_star;
                            best_eta = out_eta;
                            found_better = better;
                        }
                    }

                    if (best_dest) {
                        if (!found_better) {
                            s_ship_move_info move_info;
                            SHIPMOVE::Evaluate_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, *best_dest}, &move_info);
                            SHIPMOVE::Order_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, *best_dest}, &move_info);
                        }
                    } else {
                        const auto stagepoint = game::Star_Key_At_Row(*world, p_ai->offensive_stagepoint);
                        need_to_move = 0;
                        if (stagepoint != game::NO_STAR && !AIDATA::Is_Fleeing(ctx->work, stagepoint, LAST_FORECAST_TURN) &&
                            (world->stars.at(stagepoint).blockaded.test_slot(ai_shift)) == 0 &&
                            loc != stagepoint) {
                            s_ship_move_info move_info;
                            SHIPMOVE::Evaluate_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, stagepoint}, &move_info);
                            if (move_info.moving != 0) {
                                SHIPMOVE::Order_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, stagepoint}, &move_info);
                                need_to_move = 1;
                            }
                        }

                        if (!need_to_move && AIDATA::Is_Fleeing(ctx->work, loc, 1)) {
                            int32_t best_eta_32 = 0x7FFF;
                            std::optional<id_star> best_dest_32;
                            for (const auto star_id : world->star_order) {
                                if (!AIDATA::Is_Fleeing(ctx->work, star_id, LAST_FORECAST_TURN) &&
                                    (world->stars.at(star_id).has_colony.test_slot(ai_shift)) != 0) {
                                    s_ship_move_info move_info;
                                    SHIPMOVE::Evaluate_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, star_id}, &move_info);
                                    if (move_info.moving != 0) {
                                        if (ai->stars[star_id].neighborhood.per_player[i_ai].other_player_arrivals) {
                                            move_info.turns_left -= 24;
                                        }
                                        if ((int32_t)move_info.turns_left < best_eta_32) {
                                            best_eta_32 = move_info.turns_left;
                                            best_dest_32 = star_id;
                                        }
                                    }
                                }
                            }

                            if (best_dest_32) {
                                s_ship_move_info move_info;
                                SHIPMOVE::Evaluate_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, *best_dest_32}, &move_info);
                                SHIPMOVE::Order_Fleet_Move(*world, ship_arr, {game::e_star_destination::star, *best_dest_32}, &move_info);
                            }
                        }
                    }
                }
            }
        }
    }

    void __cdecl Gather_Intercepting_Ships(AIDATA::s_state* ai, s_ai_movement_context* ctx, id_ship* ships_list, int32_t* ship_count, const std::unordered_map<id_star, uint32_t>* intercept_map) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        *ship_count = 0;

        for (int32_t i = 0; i < (int32_t)ai->combat_ships.size(); ++i) {
            const auto ship_entry = world->ships.find(ai->combat_ships[i]);
            auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
            if (!ship || ship->status == 5) continue;

            if (factions::Legacy_Code(ship->owner) == i_ai) {
                bool should_gather = false;

                if (SHIPMOVE::Can_Order_Ship(*world, *ship)) {
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
                    const auto destination = ships::Location(ship->position);
                    if (destination && intercept_map->contains(destination->star_id)) {
                        should_gather = true;
                    }
                }

                if (should_gather) {
                    ships_list[*ship_count] = ai->combat_ships[i];
                    *ship_count = *ship_count + 1;
                }
            }
        }
    }

    void Assign_New_Colony_Ship_Destinations(AIDATA::s_state* ai, s_ai_movement_context* ctx, std::vector<AIDATA::s_colonization_candidate>* planets) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        s_player* p_ai = ctx->player;
        std::vector<id_ship> my_colony_ships;
        std::vector<id_ship> competing_colony_ships;

        for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); i++) {
            if (world->ship_slots[i] == ::game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(*world, i);
            if (ship->d.ship_type == SHIP_TYPE_COLONY) {
                if (factions::Legacy_Code(ship->owner) == i_ai) {
                    if (SHIPMOVE::Can_Order_Ship(*world, *ship)) {
                        my_colony_ships.push_back(game::Ship_Key_At_Row(*world, i));
                    } else if (ship->status < 3) {
                        competing_colony_ships.push_back(game::Ship_Key_At_Row(*world, i));
                    }
                } else if (ship->status < 3) {
                    competing_colony_ships.push_back(game::Ship_Key_At_Row(*world, i));
                }
            }
        }

        // The legacy buffer was filled backwards; keep that tie-breaking order.
        std::reverse(competing_colony_ships.begin(), competing_colony_ships.end());

        while (!planets->empty()) {
            if (my_colony_ships.empty()) return;
            const auto parent_star_id = world->planets.at(planets->back().planet_id).star_id;

            if ((ai->stars[parent_star_id].in_extended_range & ~game::player_bitmap::Only_Slot(i_ai)).none() &&
                !ai->stars[parent_star_id].neighborhood.per_player[i_ai].other_player_arrivals &&
                !AIMOVE::Star_Has_Monster(world, parent_star_id)) {
                if (ctx->difficulty >= GAME_DIFFICULTY_AVERAGE ||
                    (ctx->difficulty >= GAME_DIFFICULTY_EASY && p_ai->objectives == PLAYER_OBJECTIVE_EXPANSIONIST) ||
                    (ai->stars[parent_star_id].in_range.test_slot(i_ai)) != 0) {
                    AIMOVE::Dispatch_Unescorted_Colony_Ship(ctx, &my_colony_ships, parent_star_id);
                }
            } else {
                auto competing_ship = AIMOVE::I_Soonest_Enemy_Colony_Ship(world, competing_colony_ships, parent_star_id);
                int16_t max_eta = 0x7FFF;

                if (competing_ship) {
                    if (const auto ship_entry = world->ships.find(competing_colony_ships[*competing_ship]); ship_entry != world->ships.end()) {
                        const auto* ship = &ship_entry->second;
                        max_eta = static_cast<uint8_t>(ship->turns_left);
                    }
                }

                if (!AIMOVE::Dispatch_Escorted_Colony_Ship(ai, ctx, &my_colony_ships, parent_star_id, max_eta)) {
                    if (competing_ship) {
                        competing_colony_ships[*competing_ship] = competing_colony_ships.front();
                        competing_colony_ships.erase(competing_colony_ships.begin());
                    }
                }
            }

            planets->pop_back();
        }

        if (!my_colony_ships.empty()) {
            std::vector<uint8_t> star_added(std::ssize(world->stars), 0);

            int16_t num_candidates = 0;
            std::vector<id_star> candidate_stars(std::ssize(world->stars));

            for (int16_t i = 0; i < std::ssize(world->star_order); i++) {
                const auto star_id = world->star_order[i];
                s_star_data* star = &world->stars.at(star_id);
                if ((star->is_stagepoint.test_slot(i_ai)) != 0 &&
                    (star->blockaded.test_slot(i_ai)) == 0) {
                    if (star_added[i] == 0) {
                        candidate_stars[num_candidates++] = star_id;
                        star_added[i] = 1;
                    }
                }
            }

            for (uint32_t i = 0; i < my_colony_ships.size(); i++) {
                game_random::Shuffle<game::id_star>({candidate_stars.data(), (size_t)num_candidates});

                s_candidate_sort_context sort{&ai->stars, ctx->player_idx, (int16_t)world->player_count};
                qsort(candidate_stars.data(), num_candidates, sizeof(id_star), AIMOVE::Sort_candidate_stars, &sort);

                if (num_candidates > 0) {
                    for (int j = 0; j < num_candidates; j++) {
                        const id_star star_idx = candidate_stars[j];
                        const std::array<id_ship, 1> ship_idx_list{my_colony_ships[i]};
                        s_ship_move_info move_info{};

                        SHIPMOVE::Evaluate_Fleet_Move(*world, ship_idx_list, {game::e_star_destination::star, star_idx}, &move_info);

                        if (move_info.moving != 0) {
                            num_candidates--;
                            SHIPMOVE::Order_Fleet_Move(*world, ship_idx_list, {game::e_star_destination::star, star_idx}, &move_info);
                            candidate_stars[j] = candidate_stars[num_candidates];
                            break;
                        }
                    }
                }
            }
        }
    }

    void __cdecl Interceptions(AIDATA::s_state* ai, s_ai_movement_context* ctx, s_interception_target* intercepts, int* count, const std::unordered_map<id_star, uint32_t>* star_to_intercept_idx, int is_desperate) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        int32_t num_intercepting_ships;
        auto& work = ctx->work->interception;
        work.ships.resize(static_cast<int16_t>(world->ship_slots.size()));
        work.assignments.clear();
        work.stars.clear();

        Gather_Intercepting_Ships(ai, ctx, work.ships.data(), &num_intercepting_ships, star_to_intercept_idx);
        work.ships.resize(num_intercepting_ships);
        Knock_Out_Impossible_Interceptions(ai, ctx, intercepts, count, is_desperate);

        while (true) {
            if (*count <= 0) {
                return;
            }

            for (auto& [id, assignment] : work.assignments) assignment.reset();
            for (auto& [id, star] : work.stars) star = {};

            if (num_intercepting_ships > 0) {
                for (int ship_list_index = 0; ship_list_index < num_intercepting_ships; ++ship_list_index) {
                    const id_ship ship_id = work.ships[ship_list_index];
                    const auto ship_entry = world->ships.find(ship_id);
                    const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
                    if (!ship || ship->status == 5) continue;
                    auto& assignment = work.assignments[ship_id];
                    const bool has_special_bit_11 = struct_::Read_Bit_Field(ship->d.special_device_flags, 11);
                    const auto orbit = ships::Orbit_Star(ship->position);
                    int best_target = -1;

                    if (*count > 0) {
                        for (int target_index = 0; target_index < *count; ++target_index) {
                            s_interception_target* target = &intercepts[target_index];
                            const id_star star_idx = target->star_id;

                            work.stars[star_idx].candidate_strength = Ship_Min_Strength_Versus_Players_At_Star(ai, world, ctx->strategic_combat, ship_id, target->player_mask, star_idx);

                            std::optional<AIDATA::s_route> route;

                            if (orbit) {
                                route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                                    ai, world,
                                    ctx->player_idx,
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
                    const id_star star_idx = target->star_id;
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
                        const id_ship ship_id = work.ships[ship_list_index];
                        const auto entry = work.assignments.find(ship_id);
                        if (entry == work.assignments.end()) continue;
                        const auto& assignment = entry->second;

                        const auto ship_entry = world->ships.find(ship_id);
                        auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
                        if (assignment && ship && SHIPMOVE::Can_Order_Ship(*world, *ship)) {
                            s_ship_move_info move_info;
                            const std::array<id_ship, 1> ship_list{ship_id};
                            const auto target_star = assignment->next_star;
                            if (!world->stars.contains(target_star)) continue;
                            SHIPMOVE::Evaluate_Fleet_Move(*world, ship_list, {game::e_star_destination::star, target_star}, &move_info);

                            const auto& target_work = work.stars.at(assignment->target_star);
                            if (move_info.turns_left <= target_work.surplus_turn) {
                                const auto ship_loc = game::Find_Star_At_Coordinates(*world, ship->x, ship->y);
                                ship->mission = 5;

                                bool issue_order = false;
                                if (move_info.turns_left < target_work.sufficient_turn) {
                                    if (ship->status == 1) {
                                        issue_order = true;
                                    } else if (ship->status == 2) {
                                        SHIPMOVE::Stop_Ship_Transit(*world, ship,
                                            ship_loc ? game::s_star_destination{game::e_star_destination::star, *ship_loc} : game::s_star_destination{});
                                    }
                                } else {
                                    issue_order = true;
                                }

                                if (issue_order) {
                                    SHIPMOVE::Order_Fleet_Move(*world, ship_list, {game::e_star_destination::star, target_star}, &move_info);
                                    // Redirected ships in transit may not be at any star.
                                    if (ship_loc) {
                                        ai->players[i_ai].combat_ship_groups.erase(*ship_loc);
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
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        int i = 0;
        std::vector<s_interception_target> intercepts(std::ssize(world->stars));
        std::unordered_map<id_star, uint32_t> star_to_intercept_idx;

        if (std::ssize(world->stars) > 0) {
            for (const auto star_id_ : world->star_order) {
                if (world->stars.at(star_id_).has_colony.test_slot(i_ai)) {
                    for (int p = 0; p < world->player_count; p++) {
                        if (p != i_ai) {
                            if (ai->stars[star_id_].enroute_ships[p][LAST_FORECAST_TURN] != 0 &&
                                (ai->stars[star_id_].enroute_ships[p][0] != 0 ||
                                    ai->stars[star_id_].enroute_nonstealthy_ships[p] != 0)) {
                                int eta = 0;
                                while (ai->stars[star_id_].enroute_ships[p][eta] == 0) {
                                    eta++;
                                    if (eta >= FORECAST_TURN_COUNT) break;
                                }

                                if (eta < FORECAST_TURN_COUNT) {
                                    auto found = star_to_intercept_idx.find(star_id_);
                                    uint32_t idx;
                                    if (found == star_to_intercept_idx.end()) {
                                        intercepts[i].star_id = star_id_;
                                        intercepts[i].earliest_arrival.reset();
                                        star_to_intercept_idx.emplace(star_id_, i);
                                        idx = i;
                                        i++;
                                    } else {
                                        idx = found->second;
                                    }

                                    intercepts[idx].player_mask.set(p);
                                    if (!intercepts[idx].earliest_arrival || eta < *intercepts[idx].earliest_arrival) {
                                        intercepts[idx].earliest_arrival = eta;
                                    }

                                    if (eta < FORECAST_TURN_COUNT) {
                                        for (int j = 0; j < FORECAST_TURN_COUNT - eta; j++) {
                                            intercepts[idx].strengths[eta + j] +=
                                                ai->stars[star_id_].enroute_strengths[p][i_ai][eta + j];
                                        }
                                    }

                                    intercepts[idx].present_players[p] = true;
                                }
                            }
                        }
                    }

                    auto found = star_to_intercept_idx.find(star_id_);
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
        std::vector<std::array<s_nonplayer_arrival, nonplayer_owner_count>> alien_str(std::ssize(world->stars));

        bool aliens_present = false;

        for (int16_t s = 0; s < static_cast<int16_t>(world->ship_slots.size()); s++) {
            if (world->ship_slots[s] == ::game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(*world, s);
            if (factions::Legacy_Code(ship->owner) >= MAX_PLAYERS && factions::Legacy_Code(ship->owner) <= NONPLAYER_SHIP_TYPE_HYDRA && ship->d.speed != 0) {
                const auto location = ships::Location(ship->position);
                if (!location || !world->stars.contains(location->star_id)) continue;
                if (ship->status == 0) // In orbit
                {
                    const uint32_t star_idx = game::Star_Row(*world, location->star_id);
                    if (game::Has_Player_Colony(world->stars.at(location->star_id), i_ai)) {
                        uint32_t strength = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships(world, ctx->strategic_combat, game::Ship_Key_At_Row(*world, s), ctx->player_idx);
                        alien_str[star_idx][factions::Legacy_Code(ship->owner) - MAX_PLAYERS].strength += strength;
                        if (alien_str[star_idx][factions::Legacy_Code(ship->owner) - MAX_PLAYERS].strength == 0) {
                            alien_str[star_idx][factions::Legacy_Code(ship->owner) - MAX_PLAYERS].strength = 1;
                        }
                        alien_str[star_idx][factions::Legacy_Code(ship->owner) - MAX_PLAYERS].arrival_turn = 0;
                        aliens_present = true;
                    }
                } else if (ship->status > 0 && ship->status <= 2) // En-route
                {
                    const uint32_t star_idx = game::Star_Row(*world, location->star_id);
                    if (game::Has_Player_Colony(world->stars.at(location->star_id), i_ai)) {
                        uint32_t strength = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships(world, ctx->strategic_combat, game::Ship_Key_At_Row(*world, s), ctx->player_idx);
                        alien_str[star_idx][factions::Legacy_Code(ship->owner) - MAX_PLAYERS].strength += strength;
                        if (alien_str[star_idx][factions::Legacy_Code(ship->owner) - MAX_PLAYERS].strength == 0) {
                            alien_str[star_idx][factions::Legacy_Code(ship->owner) - MAX_PLAYERS].strength = 1;
                        }

                        // Preserve the original zero-initialized arrival estimate.
                        if (ship->turns_left < alien_str[star_idx][factions::Legacy_Code(ship->owner) - MAX_PLAYERS].arrival_turn) {
                            alien_str[star_idx][factions::Legacy_Code(ship->owner) - MAX_PLAYERS].arrival_turn = ship->turns_left;
                        }
                        aliens_present = true;
                    }
                }
            }
        }

        if (aliens_present) {
            for (int o = 0; o < nonplayer_owner_count; o++) {
                for (int16_t star_idx = 0; star_idx < std::ssize(world->star_order); star_idx++) {
                    const auto star_id_ = world->star_order[star_idx];
                    if (alien_str[star_idx][o].strength != 0) {
                        auto found = star_to_intercept_idx.find(star_id_);
                        uint32_t idx;
                        if (found == star_to_intercept_idx.end()) {
                            intercepts[i].star_id = star_id_;
                            intercepts[i].earliest_arrival.reset();
                            star_to_intercept_idx.emplace(star_id_, i);
                            idx = i;
                            i++;
                        } else {
                            idx = found->second;
                        }

                        intercepts[idx].player_mask.set(o + MAX_PLAYERS);

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

    void Find_Best_Attack_From_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, id_star star_id, s_attack_info* target_info, int mission_type, int min_pop) {
        auto* world = ctx->world;
        const uint32_t star_idx = game::Star_Row(*world, star_id);
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        s_attack_info target_info_copy;
        s_attack_info* filtered_attack = nullptr;
        uint16_t max_score = 0;
        bool valid_target_found = false;

        s_ship_move_info move_info;
        std::vector<uint8_t> visited_stars(std::ssize(world->stars), 0);
        std::vector<game::id_colony> candidates;
        for (const auto& [id, colony] : world->colonies) candidates.push_back(id);
        std::vector<int8_t> mission_array(candidates.size());
        std::vector<uint16_t> scores(candidates.size(), 0);
        std::vector<id_ship> ship_list;

        // Initialize copy of target_info
        if (target_info != nullptr) {
            target_info_copy = *target_info;

            filtered_attack = &target_info_copy;
        }

        Gather_Player_Ships_At_Star(ai, ctx, ctx->player_idx, star_id, &ship_list);

        if (!candidates.empty()) {
            for (size_t i = 0; i < candidates.size(); ++i) {
                const auto& colony = world->colonies.at(candidates[i]);
                int8_t owner = legacy_world::Encode_Player_Slot(colony.owner);

                if (owner != -1) {
                    if (target_info == nullptr) {
                        if (!AI_May_Attack_Player(world, ctx->player_idx, legacy_world::Player_Id(owner))) {
                            continue;
                        }
                    } else {
                        if (target_info->target_player != legacy_world::Player_Id(owner)) {
                            continue;
                        }
                    }

                    const auto planet_idx = colony.planet_id;
                    const auto target_id = world->planets.at(planet_idx).star_id;
                    const auto target_row = game::Star_Row(*world, target_id);

                    if (target_row >= 0 && visited_stars[target_row] == 0) {
                        if ((ai->stars[target_id].in_range.test_slot(i_ai)) != 0) {
                            SHIPMOVE::Evaluate_Fleet_Move(*world, ship_list, {game::e_star_destination::star, target_id}, &move_info);

                            if (move_info.moving != 0 && (uint8_t)move_info.turns_left < FORECAST_TURN_COUNT) {
                                uint16_t score = Compute_Attack_Worthiness(ai, ctx, candidates[i], ship_list, (uint8_t)move_info.turns_left, &mission_array[i], &visited_stars[target_row], filtered_attack, min_pop);
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
            for (size_t i = 0; i < candidates.size(); ++i) {
                // Drop scores below 2/3 of maximum
                if ((uint32_t)scores[i] * 3 < (uint32_t)max_score * 2) {
                    scores[i] = 0;
                }
            }

            auto best_colony = RUSS::Choose_Weighted_Index_U16(scores.data(), candidates.size());
            if (!best_colony) {
                return;
            }
            const auto best_position = *best_colony;

            int8_t best_mission = mission_array[best_position];
            const auto best_planet_idx = world->colonies.at(candidates[best_position]).planet_id;
            const auto best_star_id = world->planets.at(best_planet_idx).star_id;

            ai->players[i_ai].combat_ship_groups.erase(star_id);

            SHIPMOVE::Evaluate_Fleet_Move(*world, ship_list, {game::e_star_destination::star, best_star_id}, &move_info);
            SHIPMOVE::Order_Fleet_Move(*world, ship_list, {game::e_star_destination::star, best_star_id}, &move_info);

            for (auto ship = ship_list.rbegin(); ship != ship_list.rend(); ++ship) {
                if (auto* record = [&] { const auto entry = world->ships.find(*ship); return entry == world->ships.end() ? nullptr : &entry->second; }()) record->mission = best_mission;
            }
        }
    }

    void __cdecl Knock_Out_Impossible_Interceptions(
        AIDATA::s_state* ai, s_ai_movement_context* ctx,
        s_interception_target* intercept_entries,
        int* num_entries,
        int keep_one_entry) {
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);

        for (int entry_idx = *num_entries - 1; entry_idx >= 0; --entry_idx) {
            s_interception_target* current_entry = &intercept_entries[entry_idx];
            bool can_intercept = false;
            const id_star star_idx = current_entry->star_id;

            for (int class_idx = 0; class_idx < FORECAST_TURN_COUNT; ++class_idx) {
                uint32_t min_possible = 0xFFFFFFFF;
                uint32_t total_enroute = 0;

                if (world->player_count > 0) {
                    for (int p = 0; p < world->player_count; ++p) {
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


    int8_t Player_Is_Hostile_To_Player(game::s_game_state* world, id_player player_a, id_player player_b) {
        const uint32_t player_idx_a = legacy_world::Slot(player_a);
        const uint32_t player_idx_b = legacy_world::Slot(player_b);
        int8_t result = 2;
        const s_player* p_a = &world->players[player_idx_a];

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

    int8_t AI_May_Attack_Player(game::s_game_state* world, id_player player_id, id_player target_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        const uint32_t target_idx = legacy_world::Slot(target_id);
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

    uint16_t Compute_Attack_Worthiness(
        AIDATA::s_state* ai, s_ai_movement_context* ctx,
        id_colony colony_id,
        std::span<const id_ship> ships,
        uint8_t eta,
        int8_t* out_attack_type,
        uint8_t* out_flee,
        s_attack_info* target_info,
        int32_t min_pop) {
        const game::id_colony colony_idx = colony_id;
        auto* world = ctx->world;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        s_player* p_ai = ctx->player;
        s_colony* colony = &world->colonies.at(colony_idx);
        int8_t owner = legacy_world::Encode_Player_Slot(colony->owner);
        const auto planet_idx = colony->planet_id;
        const auto parent_star_id = world->planets.at(planet_idx).star_id;

        uint32_t attack_worthiness = 0;
        uint32_t total_space_strength = 0;
        uint32_t total_bomb_strength = 0;
        bool in_nebula = world->stars.at(parent_star_id).in_nebula != 0;
        uint32_t hostility_bonus = (uint8_t)Player_Is_Hostile_To_Player(world, ctx->player_idx, legacy_world::Player_Id(owner));
        uint32_t total_ship_size = 0;

        bool can_attack = true;
        uint32_t n_pops = (uint8_t)colony->n_pops;
        if ((int32_t)n_pops < min_pop && colony->outpost_flag == 0 && p_ai->personality != PLAYER_PERSONALITY_RUTHLESS && n_pops != 0) {
            for (uint32_t i = 0; i < n_pops; ++i) {
                if (colony->pop[i].group == (uint32_t)i_ai) {
                    can_attack = false;
                }
            }
        }

        if (!can_attack) {
            return 0;
        }

        int16_t shield = COLCALC::Colony_Planetary_Shield(colony);

        for (id_ship ship_id : ships) {
            const auto ship_entry = world->ships.find(ship_id);
            const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
            if (!ship || ship->status == 5) continue;
            const auto& assessment = ai->ships[ship_id];
            uint16_t space_str = in_nebula ? assessment.strengths_in_nebula[owner] : assessment.strengths[owner];
            total_space_strength += space_str;

            uint16_t bomb_str = assessment.bomb_strengths[shield];
            total_bomb_strength += bomb_str;

            total_ship_size += (1 << (ship->d.size & 0x1F));
        }

        if (target_info == nullptr && total_ship_size > 0x96) {
            uint32_t scaled = (total_space_strength / 0x96) * total_ship_size;
            if (total_space_strength < scaled) {
                total_space_strength = scaled;
            }
        }

        uint32_t possible_space_strength = ai->stars[parent_star_id].possible_strengths[owner][i_ai][eta];
        uint32_t added_strength = 0;
        int producing_idx = 0;
        int32_t current_bc = world->players[owner].bc;
        int32_t spent = colony->production_spent;

        for (int i = 0; i < 7; ++i) {
            if (current_bc + spent < 1 || eta <= producing_idx) break;

            game::colony_product prod_id = colony->producing[i];
            if (COLBLDG::Colony_Production_Is_Queued_Ship(prod_id)) {
                const auto* ship = game::Find_Queued_Ship(std::as_const(*world), prod_id);
                if (ship && ship->d.ship_type == SHIP_TYPE_COMBAT) {
                    const auto key = ship->id != game::NO_SHIP ? ship->id
                        : game::Ship_Key_At_Row(*world, game::Legacy_Queued_Ship_Row(prod_id));
                    int32_t cost = ship->d.cost;
                    if (cost > spent) {
                        int32_t buy_cost = COLCALC::Colony_Cost_To_Buy_Given_Product(colony, prod_id, spent);
                        if (buy_cost <= current_bc) {
                            spent = 0;
                            added_strength += AIPOWER::Ship_Strength_Vs_Ships(world, ctx->strategic_combat, key, ctx->player_idx);
                            producing_idx++;
                            current_bc -= buy_cost;
                        }
                    } else {
                        added_strength += AIPOWER::Ship_Strength_Vs_Ships(world, ctx->strategic_combat, key, ctx->player_idx);
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
                    ctx->work->flee_stars[parent_star_id][i] = 1;
                }
            }
        } else {
            uint32_t colony_worth = Enemy_Colony_Worth_To_Player(ai, world, colony_id, ctx->player_idx);
            uint32_t defending_space = possible_space_strength + (AIPOWER::Colony_Space_Strength_Vs_Player(world, ctx->strategic_combat, colony_id, ctx->player_idx) & 0xFFFF);
            uint32_t defending_ground = AIPOWER::Colony_Ground_Strength_Vs_Player(world, ctx->strategic_combat, colony_id, ctx->player_idx) & 0xFFFF;

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

                int32_t reachable = N_Enemy_Stars_Reachable(world, ctx->player_idx, owner, parent_star_id);
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

                if (world->stars.at(parent_star_id).blockaded.test_slot(owner)) {
                    const auto blocked_by = world->stars.at(parent_star_id).blockaded_by[owner];
                    if ((blocked_by & ~game::player_bitmap::Only_Slot(i_ai)).any() || !blocked_by.test_slot(i_ai)) {
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

    uint16_t __cdecl Ship_Min_Strength_Versus_Players_At_Star(AIDATA::s_state* ai, game::s_game_state* world, int16_t strategic_combat, id_ship ship_id, game::owner_bitmap player_mask, id_star star_id) {
        uint16_t min_strength = 0x7fff;
        bool in_nebula = world->stars.at(star_id).in_nebula != 0;

        // Check empire players (indices 0 to 7)
        if ((player_mask << MAX_PLAYERS).any()) {
            for (int32_t i = 0; i < world->player_count; ++i) {
                if (player_mask.test(i)) {
                    const auto& assessment = ai->ships[ship_id];
                    uint16_t strength = in_nebula ? assessment.strengths_in_nebula[i] : assessment.strengths[i];
                    if (strength < min_strength) {
                        min_strength = strength;
                    }
                }
            }
        }

        // Check special entities or high-index players (indices 8 to 14)
        if ((player_mask >> MAX_PLAYERS).any()) {
            for (int32_t i = MAX_PLAYERS; i < 15; ++i) {
                if (player_mask.test(i)) {
                    uint16_t strength;
                    if (in_nebula) {
                        strength = AIPOWER::Ship_Strength_Vs_Ships_In_Nebula(world, strategic_combat, ship_id, legacy_world::Player_Id(i));
                    } else {
                        strength = AIPOWER::Ship_Strength_Vs_Ships(world, strategic_combat, ship_id, legacy_world::Player_Id(i));
                    }

                    if (strength < min_strength) {
                        min_strength = strength;
                    }
                }
            }
        }

        return min_strength;
    }

    bool Dispatch_Escorted_Colony_Ship(AIDATA::s_state* ai, s_ai_movement_context* ctx, std::vector<id_ship>* colony_ships, id_star star_id, int16_t max_turns) {
        auto* world = ctx->world;
        const uint32_t star_idx = game::Star_Row(*world, star_id);
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);
        s_player* p_ai = ctx->player;
        int32_t enemy_strength[ENEMY_STRENGTH_TURN_WINDOW] = {0};
        int32_t enemy_strength2[ENEMY_STRENGTH_TURN_WINDOW];
        int32_t total_enemy_strength[ENEMY_STRENGTH_TURN_WINDOW];

        for (int i = 0; i < ENEMY_STRENGTH_TURN_WINDOW; ++i) {
            total_enemy_strength[i] = 0;
        }

        Get_Enemy_Strength_On_Turn(ai, ctx, star_id, enemy_strength, enemy_strength2);

        if (world->player_count > 0) {
            for (int p = 0; p < world->player_count; ++p) {
                if (p != i_ai) {
                    int8_t treaty = p_ai->treaty[p];
                    if (treaty < 1 || treaty > 3 || world->players[p].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        for (int t = 0; t < ENEMY_STRENGTH_TURN_WINDOW; ++t) {
                            total_enemy_strength[t] += ai->stars[star_id].possible_strengths[p][i_ai][t];
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
            id_star destination;
            uint32_t escort_count;
            uint32_t available_escorts;
        };
        std::optional<s_escort_choice> best;
        std::vector<id_ship> fleet;

        if (!colony_ships->empty()) {
            uint32_t c = 0;
            do {
                int32_t num_escorts_total = 0;
                fleet.assign(1, (*colony_ships)[c]);
                const auto lead_entry = world->ships.find(fleet.front());
                const auto* lead = lead_entry == world->ships.end() ? nullptr : &lead_entry->second;
                if (!lead || lead->status == 5) { ++c; continue; }
                const auto location = ships::Location(lead->position);
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
                            const auto key = ai->combat_ships[all_ship_idx];
                            const auto escort_entry = world->ships.find(key);
                            const auto* escort = escort_entry == world->ships.end() ? nullptr : &escort_entry->second;
                            if (escort && factions::Legacy_Code(escort->owner) == i_ai && ships::Same_Location(escort->position, lead->position)) {
                                all_ship_idx++;
                                total_escort_strength += (int16_t)AIMOVE::Ship_Rough_Strength_At_Star(ai, ctx, key, star_id);
                                num_escorts_total++;
                            } else {
                                break;
                            }
                        }

                        all_ship_idx = group->next;
                        int32_t strength = 0;
                        while (all_ship_idx < group->end) {
                            const auto key = ai->combat_ships[all_ship_idx];
                            const auto escort_entry = world->ships.find(key);
                            const auto* escort = escort_entry == world->ships.end() ? nullptr : &escort_entry->second;
                            if (escort && factions::Legacy_Code(escort->owner) == i_ai && ships::Same_Location(escort->position, lead->position)) {
                                all_ship_idx++;
                                strength += (int16_t)AIMOVE::Ship_Rough_Strength_At_Star(ai, ctx, key, star_id);
                                fleet.push_back(key);

                                auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(ctx, fleet, star_id);
                                if (!route || route->turns > max_turns) {
                                    break;
                                }
                                const id_star dest_star = route->next_star;
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
                    for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
                        if (world->ship_slots[i] == ::game::NO_SHIP) continue;
                        s_ship_data* ship = &ship_slots::At(*world, i);
                        if (factions::Legacy_Code(ship->owner) == i_ai && ships::Same_Location(ship->position, lead->position)) {
                            if (ship->status < 3 && (ship->x == lead->x && ship->y == lead->y)) {
                                const auto key = game::Ship_Key_At_Row(*world, i);
                                if (key == game::NO_SHIP) continue;
                                strength += (int16_t)AIMOVE::Ship_Rough_Strength_At_Star(ai, ctx, key, star_id);
                                fleet.push_back(key);
                            }
                        }
                    }

                    if (location->kind == e_ship_location::in_flight && location->star_id == star_id) {
                        uint8_t turns = lead->turns_left;
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
                                    best = s_escort_choice{c, turns, e_strategy::current_route, star_id,
                                        (uint32_t)fleet.size() - 1, (uint32_t)fleet.size() - 1};
                                }
                            }
                        }
                    } else {
                        auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(ctx, fleet, star_id);
                        if (route && route->turns < ENEMY_STRENGTH_TURN_WINDOW && route->turns <= max_turns) {
                            const id_star dest_star = route->next_star;
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
            const auto lead_entry = world->ships.find(fleet.front());
            const auto* lead = lead_entry == world->ships.end() ? nullptr : &lead_entry->second;
            if (!lead || lead->status == 5) return false;

            if (best->strategy == e_strategy::redirect_transit) {
                for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
                    if (world->ship_slots[i] == ::game::NO_SHIP) continue;
                    s_ship_data* ship = &ship_slots::At(*world, i);
                    if (factions::Legacy_Code(ship->owner) == i_ai && ships::Same_Location(ship->position, lead->position)) {
                        if (ship->status < 3 && (ship->x == lead->x && ship->y == lead->y)) {
                            const auto key = game::Ship_Key_At_Row(*world, i);
                            if (key == game::NO_SHIP) continue;
                            ship->mission = 4;
                            fleet.push_back(key);
                        }
                    }
                }
            } else if (best->strategy == e_strategy::assign_escorts) {
                const auto location = ships::Orbit_Star(lead->position);
                if (!location) return true;
                auto* group = ai->players[i_ai].Find_Combat_Ships(*location);
                if (group == nullptr) return true;
                uint32_t all_ship_idx = group->next;
                if (best->escort_count > 0) {
                    for (uint32_t i = 0; i < best->escort_count; ++i) {
                        const auto key = ai->combat_ships[all_ship_idx++];
                        if (const auto escort_entry = world->ships.find(key); escort_entry != world->ships.end()) {
                            auto* escort = &escort_entry->second;
                            escort->mission = 4;
                            fleet.push_back(key);
                        }
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
                SHIPMOVE::Evaluate_Fleet_Move(*world, fleet, {game::e_star_destination::star, best->destination}, &move_info);
                SHIPMOVE::Order_Fleet_Move(*world, fleet, {game::e_star_destination::star, best->destination}, &move_info);
            }

            (*colony_ships)[best->colony_position] = colony_ships->back();
            colony_ships->pop_back();
            return true;
        }

        return false;
    }

    std::optional<s_attack_choice> Find_Opportunity_Attack_Aux(
        AIDATA::s_state* ai, game::s_game_state* world, int16_t difficulty, int16_t strategic_combat,
        id_player player,
        id_player target_player,
        int16_t min_value,
        id_star star_id) {
        const uint32_t player_id = legacy_world::Slot(player);
        if (ai->players[player_id].Find_Combat_Ships(star_id) &&
            difficulty > GAME_DIFFICULTY_EASY) {
            AIDATA::s_movement_workspace work;
            s_ai_movement_context context{player, &world->players[player_id], &work, world, difficulty, strategic_combat};
            s_attack_info attack_info{};

            attack_info.target_player = target_player;
            attack_info.minimum_strength_ratio_percent = min_value;

            Find_Best_Attack_From_Star(ai, &context, star_id, &attack_info, 1, 2);

            if (attack_info.target_planet) {
                return s_attack_choice{*attack_info.target_planet, attack_info.colony_value, (int32_t)attack_info.strength_ratio_percent};
            }

        }
        return std::nullopt;
    }

    int __cdecl Sort_candidate_stars(void* context, const void* a, const void* b) {
        const auto* sort = static_cast<s_candidate_sort_context*>(context);
        const uint32_t i_ai = legacy_world::Slot(sort->player_id);
        const id_star candidate_star_a = legacy_world::Read_Star_Id(a);
        const id_star candidate_star_b = legacy_world::Read_Star_Id(b);
        uint32_t weakest_strength_a = 0xffffffffu;
        uint32_t weakest_strength_b = 0xffffffffu;

        if (sort->player_count > 0) {
            for (int16_t player_idx = 0; player_idx < sort->player_count; ++player_idx) {
                if (player_idx != i_ai) {
                    const id_star star_a = candidate_star_a;
                    if (sort->stars->at(star_a).in_range.test_slot(player_idx & 0x1f)) {
                        const uint32_t strength_a =
                            (uint32_t)sort->stars->at(star_a).possible_strengths[i_ai][player_idx][0];
                        if (strength_a < weakest_strength_a) {
                            weakest_strength_a = strength_a;
                        }

                        const id_star star_b = candidate_star_b;
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
