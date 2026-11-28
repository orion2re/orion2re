#pragma once

#include <cstdint>

namespace game { struct s_game_state; struct id_player; }

namespace lostship {
    /**
     * Removes ships that end the turn outside their allowed travel range.
     */
    void __cdecl Lose_Out_Of_Range_Ships();

    void Mark_Out_Of_Range_Ships(game::s_game_state& state, game::id_player player);
} // namespace lostship