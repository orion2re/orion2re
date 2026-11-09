#pragma once

#include "fleet_movement.h"
#include "object_ids.h"

namespace ai_compat {
    // Keep unsigned-ID conversion on the AI side of the legacy movement API.
    SHIPMOVE::e_fleet_evaluation Evaluate_Fleet_Move(std::span<const game_world::t_ship_id> ships,
        game_world::t_star_id destination, s_ship_move_info* result, std::optional<game_world::t_star_id> origin = std::nullopt);
    bool Order_Fleet_Move(std::span<const game_world::t_ship_id> ships, game_world::t_star_id destination, s_ship_move_info* result);
    void Shuffle_Ships(uint32_t* seed, std::span<game_world::t_ship_id> ships);
    void Shuffle_Stars(uint32_t* seed, std::span<game_world::t_star_id> stars);
} // namespace ai_compat
