#pragma once

#include "fleet_movement.h"
#include "object_ids.h"

namespace ai_compat {
    // Keep unsigned-ID conversion on the AI side of the legacy movement API.
    SHIPMOVE::e_fleet_evaluation Evaluate_Fleet_Move(std::span<const game::id_ship> ships,
        game::id_star destination, s_ship_move_info* result, std::optional<game::id_star> origin = std::nullopt);
    bool Order_Fleet_Move(std::span<const game::id_ship> ships, game::id_star destination, s_ship_move_info* result);
    void Shuffle_Ships(uint32_t* seed, std::span<game::id_ship> ships);
    void Shuffle_Stars(uint32_t* seed, std::span<game::id_star> stars);
} // namespace ai_compat
