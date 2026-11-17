#include "pch.h"

#include "ai_compat.h"
#include "legacy_world.h"

namespace AIMOVE {
    void Gather_Player_Ships_At_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, id_player player_id, id_star star_id, std::vector<id_ship>* ships) {
        const auto* world = ctx->world;
        ships->clear();
        s_player* p_ai = ctx->player;
        const auto* group = ai->players[legacy_world::Slot(player_id)].Find_Combat_Ships(star_id);
        if (group == nullptr || group->next == group->end) {
            return;
        }
        uint32_t index = group->next;
        id_ship ship_id = ai->combat_ships[index];
        ships->push_back(ship_id);

        while (true) {
            index++;
            if (index >= group->end) {
                break;
            }

            ship_id = ai->combat_ships[index];
            s_ship_data* ship = &world->ships[legacy_world::Slot(ship_id)];

            if (ship->owner != legacy_world::Slot(player_id) || legacy_world::Orbit_Star(ship) != star_id) {
                break;
            }

            // Skip ships on mission 5 (potentially 'In transit') unless we are at an offensive stagepoint
            if (p_ai->offensive_stagepoint == legacy_world::Slot(star_id) || ship->mission != 5) {
                ships->push_back(ship_id);
            }
        }

    }

    std::optional<AIDATA::s_route> Move_Ships_With_Possible_Intermediate(::game::s_world_state* world_state, s_ai_movement_context* ctx, std::span<const id_ship> ships, id_star destination_star) {
        const auto* world = ctx->world;
        if (ships.empty()) return std::nullopt;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);

        s_ship_move_info move_info;
        if (ai_compat::Evaluate_Fleet_Move(ships, destination_star, &move_info) == SHIPMOVE::e_fleet_evaluation::invalid_input) return std::nullopt;
        auto orbit = legacy_world::Orbit_Star(&world->ships[legacy_world::Slot(ships.front())]);

        if (move_info.moving != 0) {
            return AIDATA::s_route{destination_star, (uint8_t)move_info.turns_left};
        }

        std::optional<AIDATA::s_route> best;

        s_colony* colony_ptr = world->colonies;
        if (colony_ptr < world->colonies + world->Colony_Count()) {
            do {
                if (colony_ptr->owner == i_ai) {
                    int16_t current_star_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(colony_ptr->planet))).star_index;

                    if (orbit != legacy_world::Star_Id(current_star_idx) && legacy_world::Star_Id(current_star_idx) != destination_star) {
                        ai_compat::Evaluate_Fleet_Move(ships, legacy_world::Star_Id(current_star_idx), &move_info);

                        if (move_info.moving != 0) {
                            // Bug in original game: compares 'moving' flag with 'best_eta' instead of using 'turns_left'
                            if (!best || move_info.moving < best->turns) {
                                int16_t leg1_turns_left = (uint8_t)move_info.turns_left;
                                ai_compat::Evaluate_Fleet_Move(ships, destination_star, &move_info, legacy_world::Star_Id(current_star_idx));

                                if (move_info.moving != 0) {
                                    leg1_turns_left += (uint8_t)move_info.turns_left;

                                    if (!best || leg1_turns_left < best->turns) {
                                        best = AIDATA::s_route{legacy_world::Star_Id(current_star_idx), (uint16_t)leg1_turns_left};
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
} // namespace AIMOVE
