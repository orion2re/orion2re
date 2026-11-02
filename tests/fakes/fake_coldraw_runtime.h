#pragma once

#include <cstdint>

namespace TEST_COLDRAW {
    enum e_animation {
        PRODUCTION,
        IMPORT,
        SHORTAGE,
        POPULATION,
        DARK_POPULATION,
        SHADOW,
        GOVERNMENT,
        MORALE
    };

    struct s_draw {
        int16_t x, y;
        e_animation kind;
        int16_t id;
    };

    struct s_runtime {
        s_draw draws[512];
        int draw_count;
        int reset_count;
        int wait_count;
        int shadow_count;
        int paragraph_count;
        int16_t pointer_x;
        int16_t cycler;
    };

    extern s_runtime runtime;
    void Reset_();
    int Short_Anim_Calls_();
    int Draw_Calls_();
} // namespace TEST_COLDRAW
