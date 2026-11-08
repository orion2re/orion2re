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

    // Lists include the legacy -1 terminator. Planning never modifies world ships.
    e_fleet_evaluation Evaluate_Fleet_Move(std::span<int16_t> ship_list, int16_t destination, s_ship_move_info* result, std::optional<int16_t> origin = std::nullopt);
    bool Order_Fleet_Move(std::span<int16_t> ship_list, int16_t destination, s_ship_move_info* result);
} // namespace SHIPMOVE
