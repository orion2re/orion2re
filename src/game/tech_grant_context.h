#pragma once

#include <cstdint>

namespace game {
    struct s_tech_grant_context {
        int16_t difficulty;
        int16_t strategic_combat;
        int32_t stardate;
        int16_t combat_x;
        bool xenons_exist;
    };
}
