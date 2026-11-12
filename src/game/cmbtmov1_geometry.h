#pragma once

#include <cstdint>
#include <cstdlib>

namespace CMBTMOV1 {
    inline int16_t Facing_Distance(int16_t first_facing, int16_t second_facing) {
        int16_t distance = abs(first_facing - second_facing);
        if (distance > 8) {
            distance = 16 - distance;
        }
        return distance;
    }
}
