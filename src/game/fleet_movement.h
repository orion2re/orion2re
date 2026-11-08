#pragma once

#include <cstdint>
#include <optional>
#include <span>

struct s_ship_move_info;

namespace SHIPMOVE {
    enum class e_fleet_evaluation : uint8_t {
        invalid_input,
        blocked,
        can_move
    };

    // Planning reads a local proposed position; it never modifies world ships.
    e_fleet_evaluation Evaluate_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result, std::optional<uint32_t> origin = std::nullopt);
    bool Order_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result);
} // namespace SHIPMOVE
