#pragma once
#include <cstdint>
#include <span>
#include <utility>

namespace game_random {
    extern uint32_t random_seed;
    extern uint32_t saved_random_seed;

    /** Returns the current random seed value.
     *  @param None.
     */
    uint32_t __cdecl Get_Random_Seed();

    /** Saves the current random seed so it can be restored later.
     *  @param None.
     */
    void __cdecl Save_Random_Seed();

    /** Sets the current random seed.
     *  @param seed New seed value to store.
     */
    uint32_t __cdecl Set_Random_Seed(uint32_t seed);

    /** Seeds the generator from the current time value.
     *  @param None.
     */
    uint32_t __cdecl Randomize();

    /** Returns a random value in the range 1 through the supplied maximum.
     *  A zero maximum returns one without advancing the random seed.
     *  @param max_val Inclusive upper bound for the generated value.
     */
    uint32_t __cdecl Random(uint32_t max_val);

    template <class T>
    void Shuffle(std::span<T> values) {
        for (uint32_t i = 0; i < values.size(); ++i) {
            const auto target = i + Random(static_cast<uint32_t>(values.size()) - i) - 1;
            std::swap(values[i], values[target]);
        }
    }

    /** Restores the previously saved random seed.
     *  @param None.
     */
    void __cdecl Restore_Random_Seed();
} // namespace game_random