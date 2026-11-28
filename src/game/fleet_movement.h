#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "star_destination.h"
#include "planning_context.h"

struct s_ship_move_info;
struct s_nebula;
namespace game { struct s_game_state; }

namespace SHIPMOVE {
    enum class e_fleet_evaluation : uint8_t {
        invalid_input,
        blocked,
        can_move
    };

    // The first ship determines departure conditions. Evaluation leaves the roster unchanged.
    e_fleet_evaluation Evaluate_Fleet_Move(game::s_game_state& state, std::span<const game::id_ship> ships,
        game::s_star_destination destination, s_ship_move_info* result, const game::s_planning_context& context,
        std::optional<game::id_star> origin = std::nullopt);
    e_fleet_evaluation Evaluate_Fleet_Move(game::s_game_state& state, std::span<const game::id_ship> ships,
        game::s_star_destination destination, s_ship_move_info* result,
        std::optional<game::id_star> origin = std::nullopt);
    // Explicit map geometry for a detached route; an empty span means clear space.
    e_fleet_evaluation Evaluate_Fleet_Move(game::s_game_state& state, std::span<const game::id_ship> ships,
        game::s_star_destination destination, s_ship_move_info* result,
        std::optional<game::id_star> origin, std::span<const s_nebula> nebulas);
    bool Order_Fleet_Move(game::s_game_state& state, std::span<const game::id_ship> ships,
        game::s_star_destination destination, const s_ship_move_info* result);
    bool Apply_Player_Movement_Order(std::span<const game::id_ship> ships,
        game::s_star_destination destination, s_ship_move_info* result);
} // namespace SHIPMOVE
