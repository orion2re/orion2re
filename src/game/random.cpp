#include "pch.h"

namespace game_random {
    uint32_t random_seed;
    uint32_t saved_random_seed;

    uint32_t __cdecl Get_Random_Seed() {
        return random_seed;
    }

    void __cdecl Save_Random_Seed() {
        saved_random_seed = random_seed;
    }

    uint32_t __cdecl Set_Random_Seed(uint32_t seed) {
        random_seed = seed;
        return seed;
    }

    uint32_t __cdecl Randomize() {
        uint32_t current_time = (uint32_t)time(nullptr);
        return Set_Random_Seed(current_time);
    }

    uint32_t __cdecl Random(uint32_t max_val) {
        if (max_val == 0) {
            return 1;
        }

        uint32_t quotient = 0xFFFFFFFFu / max_val;
        uint32_t limit = max_val * quotient;

        uint32_t seed = random_seed;
        do {
            seed = seed * 0x41C64E6D + 0x3039;
        } while (limit <= seed);

        random_seed = seed;
        return (seed / quotient) + 1;
    }

    void __cdecl Restore_Random_Seed() {
        random_seed = saved_random_seed;
    }
} // namespace game_random