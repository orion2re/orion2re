#include "doctest/doctest.h"
#include "game/portrait_replacement.h"
#include <array>
#include <vector>

TEST_CASE("Portrait replacements require an unchanged eligible frame") {
    using namespace portrait_replacement;
    s_candidate state{};
    std::vector<uint8_t> pixels(640 * 480, 7);
    std::array<uint8_t, 1024> palette{};
    Capture(&state, 5, pixels.data(), 640, 480, palette.data());
    CHECK(Match(&state, true, pixels.data(), 640, 480, palette.data()) == 5);
    CHECK(Match(&state, false, pixels.data(), 640, 480, palette.data()) == -1);
    pixels[0] = 9;
    CHECK(Match(&state, true, pixels.data(), 640, 480, palette.data()) == 5);
    pixels[(Y + HEIGHT - 1) * 640 + X + WIDTH - 1] = 9;
    CHECK(Match(&state, true, pixels.data(), 640, 480, palette.data()) == -1);
    Capture(&state, 12, pixels.data(), 640, 480, palette.data());
    CHECK(Match(&state, true, pixels.data(), 640, 480, palette.data()) == 12);
    palette[100] = 1;
    CHECK(Match(&state, true, pixels.data(), 640, 480, palette.data()) == -1);
}

TEST_CASE("Portrait replacements reject missing buffers and custom races") {
    using namespace portrait_replacement;
    s_candidate state{};
    std::vector<uint8_t> pixels(640 * 480);
    std::array<uint8_t, 1024> palette{};
    for (int race : {-1, 13, 100}) {
        CHECK(Filename(race) == nullptr);
        Capture(&state, race, pixels.data(), 640, 480, palette.data());
        CHECK(state.race == -1);
    }
    Capture(&state, 0, nullptr, 640, 480, palette.data());
    CHECK(state.race == -1);
    Capture(&state, 0, pixels.data(), X + WIDTH - 1, 480, palette.data());
    CHECK(state.race == -1);
    Capture(&state, 0, pixels.data(), 640, Y + HEIGHT - 1, palette.data());
    CHECK(state.race == -1);
    Capture(&state, 0, pixels.data(), 640, 480, nullptr);
    CHECK(state.race == -1);
    Capture(&state, 0, pixels.data(), 640, 480, palette.data());
    CHECK(Match(&state, true, nullptr, 640, 480, palette.data()) == -1);
    CHECK(Match(&state, true, pixels.data(), 640, 480, nullptr) == -1);
    CHECK(std::string(Filename(0)) == "alkari.png");
    CHECK(std::string(Filename(12)) == "trilarian.png");
}
