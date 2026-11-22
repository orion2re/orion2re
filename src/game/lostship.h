#pragma once

#include <cstdint>

namespace game { struct s_view; }

namespace lostship {
    /**
     * Removes ships that end the turn outside their allowed travel range.
     */
    void __cdecl Lose_Out_Of_Range_Ships();

    void Mark_Out_Of_Range_Ships(const game::s_view& world, int16_t player_idx);
} // namespace lostship