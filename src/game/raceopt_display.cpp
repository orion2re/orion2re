#include "pch.h"

namespace raceopt {
    void Format_Numeric_Trait(char* output, size_t output_size, int16_t trait_idx, int16_t value) {
        const char* sign = value < 0 ? "-" : "+";
        const char* units = trait_idx == TRAIT_TAX_BONUS ? " BC" : "";
        const int magnitude = value < 0 ? -(int)value : value;

        if (trait_idx == TRAIT_FOOD_BONUS || trait_idx == TRAIT_TAX_BONUS) {
            snprintf(
                output,
                output_size,
                "%s%i%s%s",
                sign,
                magnitude / 2,
                magnitude % 2 != 0 ? ".5" : "",
                units);
        } else {
            snprintf(output, output_size, "%s%i%s", sign, magnitude, units);
        }
    }

    s_race_stat_layout Race_Stat_Layout(const int8_t* traits) {
        int trait_count = 1;

        for (int trait_idx = TRAIT_POP_GROWTH_BONUS;
             trait_idx < TRAIT_COUNT;
             ++trait_idx) {
            if (traits[trait_idx] != 0) {
                ++trait_count;
            }
        }

        if (trait_count <= 11) {
            return {2, 13};
        }
        if (trait_count <= 13) {
            return {1, 11};
        }
        return {0, 7};
    }
} // namespace raceopt
