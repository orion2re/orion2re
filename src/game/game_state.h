#pragma once

#include <array>

#include "consts.h"
#include "leader_data.h"

namespace game {
    struct s_game_state {
        // A leader's ID is its fixed catalog slot, including inactive leaders.
        std::array<s_leader_data, MAX_LEADERS> leaders{};

        bool operator==(const s_game_state&) const = default;
    };

    inline s_game_state current_state;
} // namespace game
