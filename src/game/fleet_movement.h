#pragma once

#include <cstdint>
#include <optional>
#include <span>

struct s_ship_move_info;

namespace SHIPMOVE {
    // Typed fleet operations over the legacy movement engine. A proposed origin
    // is simulated temporarily and restored before evaluation returns.
    void Evaluate_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result, std::optional<uint32_t> origin = std::nullopt);
    void Order_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result);
} // namespace SHIPMOVE
