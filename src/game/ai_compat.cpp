#include "pch.h"

#include "ai_compat.h"
#include "legacy_world.h"

namespace ai_compat {
    SHIPMOVE::e_fleet_evaluation Evaluate_Fleet_Move(std::span<const game_world::t_ship_id> ships,
        game_world::t_star_id destination, s_ship_move_info* result, std::optional<game_world::t_star_id> origin) {
        *result = {};
        const auto target = legacy_world::Index(destination);
        const auto source = origin ? legacy_world::Index(*origin) : std::optional<int16_t>{};
        if (!target || (origin && !source)) return SHIPMOVE::e_fleet_evaluation::invalid_input;
        auto ship_list = legacy_world::Ship_List(ships);
        if (!ship_list) return SHIPMOVE::e_fleet_evaluation::invalid_input;
        return SHIPMOVE::Evaluate_Fleet_Move(*ship_list, *target, result, source);
    }

    bool Order_Fleet_Move(std::span<const game_world::t_ship_id> ships, game_world::t_star_id destination, s_ship_move_info* result) {
        const auto target = legacy_world::Index(destination);
        if (!target) return false;
        auto ship_list = legacy_world::Ship_List(ships);
        return ship_list && SHIPMOVE::Order_Fleet_Move(*ship_list, *target, result);
    }

    void Shuffle_Ships(uint32_t* seed, std::span<game_world::t_ship_id> ships) {
        for (uint32_t i = 0; i < ships.size(); ++i) {
            const uint32_t target = i + game_random::Random_From_Seed(seed, (uint32_t)ships.size() - i) - 1;
            std::swap(ships[i], ships[target]);
        }
    }

    void Shuffle_Stars(uint32_t* seed, std::span<game_world::t_star_id> stars) {
        for (uint32_t i = 0; i < stars.size(); ++i) {
            const uint32_t target = i + game_random::Random_From_Seed(seed, (uint32_t)stars.size() - i) - 1;
            std::swap(stars[i], stars[target]);
        }
    }
} // namespace ai_compat
