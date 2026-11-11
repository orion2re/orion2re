#pragma once

#include "consts.h"

namespace NEWGAME {
    enum {
        NEW_GAME_GALAXY_SIZE_HUGE = 3,
        NEW_GAME_GALAXY_SIZE_MAXIMUM = 4,
        NEW_GAME_GALAXY_SIZE_COUNT = 5,
        NEW_GAME_CIVILIZATION_POST_WARP = 2,
        NEW_GAME_CIVILIZATION_ADVANCED = 3,
        NEW_GAME_PLAYER_COUNT_RANDOM = MAX_PLAYERS - 1,
        NEW_GAME_RANDOM_PICTURE_ENTRY = 31,
        NEW_GAME_POST_WARP_PICTURE_ENTRY = 32,
    };

    bool Random_Player_Count_Selected();
} // namespace NEWGAME
