#include "pch.h"
#include "fleet_movement.h"

#include "legacy_ids.h"
#include "ship_location_queries.h"
#include "ship_ids.h"

namespace AIMOVE {
    void Gather_Player_Ships_At_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, id_player player_id, id_star star_id, std::vector<id_ship>* ships) {
        auto* world = ctx->world;
        ships->clear();
        s_player* p_ai = ctx->player;
        const auto* group = ai->players[legacy_world::Slot(player_id)].Find_Combat_Ships(star_id);
        if (group == nullptr || group->next == group->end) {
            return;
        }
        uint32_t index = group->next;
        if (const auto key = ai->combat_ships[index]; game::Find_Ship(*world, key)) {
            ships->push_back(key);
        }

        while (true) {
            index++;
            if (index >= group->end) {
                break;
            }

            const auto ship_id = ai->combat_ships[index];
            const auto* ship = game::Find_Ship(*world, ship_id);
            if (!ship) continue;

            if (factions::Legacy_Code(ship->owner) != legacy_world::Slot(player_id) || ships::Orbit_Star(ship->position) != star_id) {
                break;
            }

            // Skip ships on mission 5 (potentially 'In transit') unless we are at an offensive stagepoint
            if (p_ai->offensive_stagepoint == game::Star_Row(*world, star_id) || ship->mission != 5) {
                ships->push_back(ship_id);
            }
        }

    }

    std::optional<AIDATA::s_route> Move_Ships_With_Possible_Intermediate(s_ai_movement_context* ctx, std::span<const id_ship> ships, id_star destination_star) {
        auto* world = ctx->world;
        if (ships.empty()) return std::nullopt;
        const uint32_t i_ai = legacy_world::Slot(ctx->player_idx);

        const auto* lead = game::Find_Ship(*world, ships.front());
        if (!lead) return std::nullopt;
        s_ship_move_info move_info;
        if (SHIPMOVE::Evaluate_Fleet_Move(*world, ships, {game::e_star_destination::star, destination_star}, &move_info) == SHIPMOVE::e_fleet_evaluation::invalid_input) return std::nullopt;
        const auto orbit = ships::Orbit_Star(lead->position);

        if (move_info.moving != 0) {
            return AIDATA::s_route{destination_star, (uint8_t)move_info.turns_left};
        }

        std::optional<AIDATA::s_route> best;

        for (auto& [roster_id, roster_colony] : world->colonies) {
            const auto& colony = roster_colony;
            if (legacy_world::Encode_Player_Slot(colony.owner) != i_ai) continue;
            const auto star = world->planets.at(colony.planet_id).star_id;
            if (orbit == star || star == destination_star) continue;
            SHIPMOVE::Evaluate_Fleet_Move(*world, ships, {game::e_star_destination::star, star}, &move_info);

            // Preserve the original comparison of the moving flag against the best ETA.
            if (move_info.moving == 0 || (best && move_info.moving >= best->turns)) continue;
            uint16_t eta = static_cast<uint8_t>(move_info.turns_left);
            SHIPMOVE::Evaluate_Fleet_Move(*world, ships, {game::e_star_destination::star, destination_star}, &move_info, star);
            if (move_info.moving == 0) continue;
            eta += static_cast<uint8_t>(move_info.turns_left);
            if (!best || eta < best->turns) best = AIDATA::s_route{star, eta};
        }

        return best;
    }
} // namespace AIMOVE
