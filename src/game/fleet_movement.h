#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "star_destination.h"

struct s_ship_move_info;

namespace SHIPMOVE {
    enum class e_fleet_evaluation : uint8_t {
        invalid_input,
        blocked,
        can_move
    };

    // The first ship determines departure conditions. Evaluation leaves the roster unchanged.
    e_fleet_evaluation Evaluate_Fleet_Move(std::span<const game::id_ship> ships,
        game::s_star_destination destination, s_ship_move_info* result,
        std::optional<game::id_star> origin = std::nullopt);
    bool Order_Fleet_Move(std::span<const game::id_ship> ships,
        game::s_star_destination destination, const s_ship_move_info* result);
    bool Apply_Player_Movement_Order(std::span<const game::id_ship> ships,
        game::s_star_destination destination, s_ship_move_info* result);
} // namespace SHIPMOVE
