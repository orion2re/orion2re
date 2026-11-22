#include "pch.h"

#include "ai_compat.h"
#include "legacy_world.h"

namespace ai_compat {
    SHIPMOVE::e_fleet_evaluation Evaluate_Fleet_Move(std::span<const game::id_ship> ships,
        game::id_star destination, s_ship_move_info* result, std::optional<game::id_star> origin) {
        *result = {};
        const auto target = game::current_state.Star_Position(destination);
        const auto start = origin ? game::current_state.Star_Position(*origin) : int16_t{-1};
        if (target < 0 || (origin && start < 0)) {
            return SHIPMOVE::e_fleet_evaluation::invalid_input;
        }
        std::optional<int16_t> source;
        if (origin) source = start;
        auto ship_list = legacy_world::Ship_List(ships);
        if (!ship_list) return SHIPMOVE::e_fleet_evaluation::invalid_input;
        return SHIPMOVE::Evaluate_Fleet_Move(*ship_list, target, result, source);
    }

    bool Order_Fleet_Move(std::span<const game::id_ship> ships, game::id_star destination, s_ship_move_info* result) {
        const auto target = game::current_state.Star_Position(destination);
        if (target < 0) return false;
        auto ship_list = legacy_world::Ship_List(ships);
        return ship_list && SHIPMOVE::Order_Fleet_Move(*ship_list, target, result);
    }

    void Shuffle_Ships(std::span<game::id_ship> ships) {
        for (uint32_t i = 0; i < ships.size(); ++i) {
            const uint32_t target = i + game_random::Random((uint32_t)ships.size() - i) - 1;
            std::swap(ships[i], ships[target]);
        }
    }

    void Shuffle_Stars(std::span<game::id_star> stars) {
        for (uint32_t i = 0; i < stars.size(); ++i) {
            const uint32_t target = i + game_random::Random((uint32_t)stars.size() - i) - 1;
            std::swap(stars[i], stars[target]);
        }
    }
} // namespace ai_compat
