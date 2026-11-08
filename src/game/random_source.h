#pragma once

#include <cstdint>

namespace game_random {
    struct s_source {
        uint32_t* seed = nullptr;
        uint32_t (*draw)(uint32_t*, uint32_t) = nullptr;

        uint32_t Next(uint32_t maximum) const {
            return draw(seed, maximum);
        }
    };
} // namespace game_random
