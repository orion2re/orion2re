#pragma once

#include <cstdint>

namespace history_read {
    struct s_curve {
        int16_t player = -1;
        int32_t height[350] = {};
    };

    struct s_axis {
        int16_t x_step = 1;
        char labels[8][16] = {};
    };

    struct s_chart {
        int16_t samples = 0;
        int16_t count = 0;
        int32_t top_scale = 25;
        s_curve curves[8];
    };

    int16_t Copy_Players(int16_t viewer, uint8_t* players);
    void Read_Axis(s_axis* out);
    bool Read_Chart(int16_t viewer, uint8_t series, s_chart* out);
} // namespace history_read
