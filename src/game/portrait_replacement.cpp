#include "portrait_replacement.h"

#include <cstring>

namespace portrait_replacement {
    s_candidate candidate;

    static bool Valid_Buffer_(const uint8_t* pixels, int32_t width, int32_t height, const void* palette) {
        return pixels != nullptr && palette != nullptr && width >= X + WIDTH && height >= Y + HEIGHT;
    }

    void Capture(s_candidate* state, int32_t race, const uint8_t* pixels, int32_t width, int32_t height, const void* palette) {
        state->race = -1;
        if (Filename(race) == nullptr || !Valid_Buffer_(pixels, width, height, palette)) {
            return;
        }
        for (int32_t row = 0; row < HEIGHT; ++row) {
            memcpy(state->pixels + row * WIDTH, pixels + (Y + row) * width + X, WIDTH);
        }
        memcpy(state->palette, palette, sizeof(state->palette));
        state->race = race;
    }

    int32_t Match(const s_candidate* state, bool eligible, const uint8_t* pixels, int32_t width, int32_t height, const void* palette) {
        if (!eligible || Filename(state->race) == nullptr || !Valid_Buffer_(pixels, width, height, palette) ||
            memcmp(state->palette, palette, sizeof(state->palette)) != 0) {
            return -1;
        }
        // A later popup or drawing operation must not be covered by the replacement.
        for (int32_t row = 0; row < HEIGHT; ++row) {
            if (memcmp(state->pixels + row * WIDTH, pixels + (Y + row) * width + X, WIDTH) != 0) {
                return -1;
            }
        }
        return state->race;
    }

    const char* Filename(int32_t race) {
        static const char* names[RACE_COUNT] = {
            "alkari.png",
            "bulrathi.png",
            "darlok.png",
            "elerian.png",
            "gnolam.png",
            "human.png",
            "klackon.png",
            "meklar.png",
            "mrrshan.png",
            "psilon.png",
            "sakkra.png",
            "silicoid.png",
            "trilarian.png",
        };
        return race >= 0 && race < RACE_COUNT ? names[race] : nullptr;
    }
} // namespace portrait_replacement
