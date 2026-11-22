#include "pch.h"

#include "ai_compat.h"
#include "legacy_world.h"

namespace ai_compat {
    SHIPMOVE::e_fleet_evaluation Evaluate_Fleet_Move(std::span<const game::id_ship> ships,
        game::id_star destination, s_ship_move_info* result, std::optional<game::id_star> origin) {
        return SHIPMOVE::Evaluate_Fleet_Move(ships, {game::e_star_destination::star, destination}, result, origin);
    }

    bool Order_Fleet_Move(std::span<const game::id_ship> ships, game::id_star destination, s_ship_move_info* result) {
        return SHIPMOVE::Order_Fleet_Move(ships, {game::e_star_destination::star, destination}, result);
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
