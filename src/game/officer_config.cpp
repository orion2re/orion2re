#include "pch.h"
#include "game/officer.h"

namespace OFFICER {
    uint8_t loknar_gives_no_tech = 0;
    uint8_t rejected_offer_keeps_timer = 0;
    s_leader_progression progression = {
        {60, 150, 300, 500, 1000},
        {10, 5, 10},
        {5, 5, 10},
        {47, 41, 52},
    };
    s_orion_reward_technology orion_reward_technology = {
        TECH_APP_DEATH_RAY,
    };
} // namespace OFFICER
