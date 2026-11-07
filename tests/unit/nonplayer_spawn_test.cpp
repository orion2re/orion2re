#include "pch.h"

#include "doctest/doctest.h"

namespace {
    s_star_data stars[1];
    int16_t spawned_x;
    int16_t spawned_y;
    int8_t spawned_owner;
    s_ship_design* spawned_design;

    struct s_spawn_result {
        int16_t x;
        int16_t y;
        uint32_t seed;
        uint32_t next_roll;
    };

    s_spawn_result Spawn_(uint32_t seed, int16_t scale, int16_t type, uint8_t randomize_xy = 1) {
        MOX::cur_map_scale = scale;
        MOX::cur_map_x = 123;
        MOX::cur_map_y = 456;
        game_random::Set_Random_Seed(seed);
        s_ship_design design = {};

        DOCTEST_REQUIRE(SHIPS::Set_Nonplayer_Ship_XY(0, type, &design, randomize_xy) == 17);
        DOCTEST_CHECK(spawned_owner == type);
        DOCTEST_CHECK(spawned_design == &design);
        DOCTEST_CHECK(MOX::cur_map_scale == scale);
        DOCTEST_CHECK(MOX::cur_map_x == 123);
        DOCTEST_CHECK(MOX::cur_map_y == 456);

        return {spawned_x, spawned_y, game_random::Get_Random_Seed(), game_random::Random(100)};
    }
} // namespace

namespace MOX {
    s_star_data* star = stars;
    int16_t cur_map_scale;
    int16_t cur_map_x;
    int16_t cur_map_y;
    int16_t max_map_scale;
    int16_t MAP_MAX_X;
    int16_t MAP_MAX_Y;
    int16_t star_width = 33;
    int16_t star_height = 33;
} // namespace MOX

namespace SHIPMOVE {
    int16_t __cdecl Create_Nonplayer_Ship(int8_t owner, s_ship_design* design, int16_t x, int16_t y) {
        spawned_x = x;
        spawned_y = y;
        spawned_owner = owner;
        spawned_design = design;
        return 17;
    }
} // namespace SHIPMOVE

DOCTEST_TEST_CASE("nonplayer spawning preserves full-map coordinates and random sequence") {
    struct s_case {
        uint32_t seed;
        int16_t x;
        int16_t y;
        int16_t expected_x;
        int16_t expected_y;
        uint32_t expected_seed;
    };
    const s_case cases[] = {
        {0, 759, 600, 816, 633, 0x0DAA96F5},
        {1, 759, 600, 831, 513, 0xD9E2B600},
        {0xDEADBEEF, 759, 600, 804, 555, 0x74134DE8},
        {1, 30, 30, 114, 114, 0xF94BDF32},
        {1, 1488, 1170, 1416, 1101, 0xF94BDF32},
        {UINT32_MAX, 30, 1170, 153, 1086, 0x9F609E64},
    };

    MOX::max_map_scale = 30;
    MOX::MAP_MAX_X = 1518;
    MOX::MAP_MAX_Y = 1200;
    for (const s_case& test_case : cases) {
        DOCTEST_CAPTURE(test_case.seed);
        stars[0].x = test_case.x;
        stars[0].y = test_case.y;
        const s_spawn_result result = Spawn_(test_case.seed, 30, NONPLAYER_SHIP_TYPE_ANTARAN);
        DOCTEST_CHECK(result.x == test_case.expected_x);
        DOCTEST_CHECK(result.y == test_case.expected_y);
        DOCTEST_CHECK(result.seed == test_case.expected_seed);
    }
}

DOCTEST_TEST_CASE("nonplayer spawn coordinates and subsequent randomness are independent of zoom") {
    const int16_t map_scales[] = {30, 10, 15, 20, 36, 54, 100};
    const int16_t classic_scales[] = {10, 15, 20, 30};
    const int16_t types[] = {NONPLAYER_SHIP_TYPE_ANTARAN, NONPLAYER_SHIP_TYPE_AMOEBA};
    const uint32_t seeds[] = {0, 1, 2, 3, 4, 5, 17, 42, 0x12345678, 0xDEADBEEF, UINT32_MAX};

    for (int16_t map_scale : map_scales) {
        DOCTEST_CAPTURE(map_scale);
        MOX::max_map_scale = map_scale;
        MOX::MAP_MAX_X = (int16_t)(506 * map_scale / 10);
        MOX::MAP_MAX_Y = (int16_t)(400 * map_scale / 10);
        const int16_t xs[] = {(int16_t)(MOX::MAP_MAX_X / 2), 30, (int16_t)(MOX::MAP_MAX_X - 30)};
        const int16_t ys[] = {(int16_t)(MOX::MAP_MAX_Y / 2), 30, (int16_t)(MOX::MAP_MAX_Y - 30)};

        for (int16_t x : xs) {
            for (int16_t y : ys) {
                DOCTEST_CAPTURE(x);
                DOCTEST_CAPTURE(y);
                stars[0].x = x;
                stars[0].y = y;
                for (uint32_t seed : seeds) {
                    DOCTEST_CAPTURE(seed);
                    const s_spawn_result expected = Spawn_(seed, map_scale, NONPLAYER_SHIP_TYPE_ANTARAN);
                    for (int16_t zoom = 0; zoom <= 3; ++zoom) {
                        const int16_t scale = map_scale <= 30
                            ? std::min(map_scale, classic_scales[zoom])
                            : MAP_SCALE::Extended_Scale_For_Zoom_Level_(
                                  map_scale, 3, zoom, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF);
                        DOCTEST_CAPTURE(scale);
                        for (int16_t type : types) {
                            const s_spawn_result actual = Spawn_(seed, scale, type);
                            DOCTEST_CHECK(actual.x == expected.x);
                            DOCTEST_CHECK(actual.y == expected.y);
                            DOCTEST_CHECK(actual.seed == expected.seed);
                            DOCTEST_CHECK(actual.next_roll == expected.next_roll);
                        }
                    }
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("nonrandomized nonplayer spawning uses the star coordinates without consuming randomness") {
    stars[0].x = 731;
    stars[0].y = 509;
    MOX::max_map_scale = 30;
    for (int16_t scale : {10, 15, 20, 30}) {
        const s_spawn_result result = Spawn_(0x12345678, scale, NONPLAYER_SHIP_TYPE_GUARDIAN, 0);
        DOCTEST_CHECK(result.x == 731);
        DOCTEST_CHECK(result.y == 509);
        DOCTEST_CHECK(result.seed == 0x12345678);
    }
}
