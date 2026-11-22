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
            if (p_ai->offensive_stagepoint == world->state->Star_Position(star_id) || ship->mission != 5) {
                ships->push_back(ship_id);
            }
        }

    }

    std::optional<AIDATA::s_route> Move_Ships_With_Possible_Intermediate(s_ai_movement_context* ctx, std::span<const id_ship> ships, id_star destination_star) {
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

        for (auto& [roster_id, roster_colony] : world->state->colonies) {
            const auto& colony = roster_colony;
            if (colony.owner != i_ai) continue;
            const auto star = world->state->planets.at(colony.planet_id).star_id;
            if (orbit == star || star == destination_star) continue;
            ai_compat::Evaluate_Fleet_Move(ships, star, &move_info);

            // Preserve the original comparison of the moving flag against the best ETA.
            if (move_info.moving == 0 || (best && move_info.moving >= best->turns)) continue;
            uint16_t eta = static_cast<uint8_t>(move_info.turns_left);
            ai_compat::Evaluate_Fleet_Move(ships, destination_star, &move_info, star);
            if (move_info.moving == 0) continue;
            eta += static_cast<uint8_t>(move_info.turns_left);
            if (!best || eta < best->turns) best = AIDATA::s_route{star, eta};
        }

        return best;
    }
} // namespace AIMOVE
