#pragma once

#include "fleet_movement.h"

namespace ai_compat {
    // Keep unsigned-ID conversion on the AI side of the legacy movement API.
    SHIPMOVE::e_fleet_evaluation Evaluate_Fleet_Move(std::span<const uint32_t> ships,
        uint32_t destination, s_ship_move_info* result, std::optional<uint32_t> origin = std::nullopt);
    bool Order_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result);
} // namespace ai_compat
