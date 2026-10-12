#pragma once

#include <cstdint>

namespace portrait_replacement {
    constexpr int32_t X = 54;
    constexpr int32_t Y = 63;
    constexpr int32_t WIDTH = 290;
    constexpr int32_t HEIGHT = 322;
    constexpr int32_t RACE_COUNT = 13;

    struct s_candidate {
        int32_t race = -1;
        uint8_t pixels[WIDTH * HEIGHT];
        uint8_t palette[1024];
    };

    // Called only by the game thread; published race IDs travel with the frame.
    extern s_candidate candidate;
    void Capture(s_candidate* state, int32_t race, const uint8_t* pixels, int32_t width, int32_t height, const void* palette);
    int32_t Match(const s_candidate* state, bool eligible, const uint8_t* pixels, int32_t width, int32_t height, const void* palette);
    const char* Filename(int32_t race);
} // namespace portrait_replacement
