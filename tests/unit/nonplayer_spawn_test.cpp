#include "pch.h"

#include "doctest/doctest.h"
#include "lua/lua_game_ship_identity.h"

namespace {
    s_star_data stars[2]; // One normal star and the runtime Antares slot.
    constexpr int16_t spawn_slot = 17;

    struct s_spawn_fixture {
        std::vector<s_ship_data> ships{MAX_SHIPS};
        std::vector<uint64_t> generations = std::vector<uint64_t>(MAX_SHIPS);
        orion2re::lua::game::s_ship_generation_scope generation_scope{generations.data()};
        s_star_data* saved_stars = MOX::star;
        s_ship_data* saved_ships = MOX::ship;
        int16_t saved_num_stars = MOX::NUM_STARS;
        int16_t saved_num_ships = MOX::NUM_SHIPS;
        int16_t saved_scale = MOX::cur_map_scale;
        int16_t saved_x = MOX::cur_map_x;
        int16_t saved_y = MOX::cur_map_y;
        int16_t saved_max_scale = MOX::max_map_scale;
        int16_t saved_width = MOX::MAP_MAX_X;
        int16_t saved_height = MOX::MAP_MAX_Y;
        int16_t saved_star_width = MOX::star_width;
        int16_t saved_star_height = MOX::star_height;
        uint32_t saved_seed = game_random::Get_Random_Seed();

        s_spawn_fixture() {
            memset(stars, 0, sizeof(stars));
            stars[1].x = -1;
            stars[1].y = -1;
            MOX::star = stars;
            MOX::ship = ships.data();
            MOX::NUM_STARS = 1;
            MOX::NUM_SHIPS = spawn_slot + 1;
            MOX::star_width = 33;
            MOX::star_height = 33;
        }

        ~s_spawn_fixture() {
            MOX::star = saved_stars;
            MOX::ship = saved_ships;
            MOX::NUM_STARS = saved_num_stars;
            MOX::NUM_SHIPS = saved_num_ships;
            MOX::cur_map_scale = saved_scale;
            MOX::cur_map_x = saved_x;
            MOX::cur_map_y = saved_y;
            MOX::max_map_scale = saved_max_scale;
            MOX::MAP_MAX_X = saved_width;
            MOX::MAP_MAX_Y = saved_height;
            MOX::star_width = saved_star_width;
            MOX::star_height = saved_star_height;
            game_random::Set_Random_Seed(saved_seed);
        }
    };

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
        design.cost = 321;
        design.ship_type = SHIP_TYPE_COMBAT;
        strcpy(design.name, "Spawn fixture");
        const s_ship_design original_design = design;
        MOX::ship[spawn_slot].owner = MAX_PLAYERS;
        MOX::ship[spawn_slot].status = 5;
        const uint64_t generation = orion2re::lua::game::Ship_Generation(spawn_slot);

        DOCTEST_REQUIRE(SHIPS::Set_Nonplayer_Ship_XY(0, type, &design, randomize_xy) == spawn_slot);
        DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(spawn_slot) == generation + 1);
        const s_ship_data& spawned = MOX::ship[spawn_slot];
        DOCTEST_CHECK((spawned.owner == type));
        DOCTEST_CHECK((spawned.d.cost == design.cost));
        DOCTEST_CHECK((spawned.d.ship_type == design.ship_type));
        DOCTEST_CHECK(strcmp(spawned.d.name, design.name) == 0);
        DOCTEST_CHECK(memcmp(&design, &original_design, sizeof(design)) == 0);
        DOCTEST_CHECK(MOX::cur_map_scale == scale);
        DOCTEST_CHECK(MOX::cur_map_x == 123);
        DOCTEST_CHECK(MOX::cur_map_y == 456);

        return {spawned.x, spawned.y, game_random::Get_Random_Seed(), game_random::Random(100)};
    }
} // namespace

DOCTEST_TEST_CASE_FIXTURE(s_spawn_fixture, "nonplayer spawning preserves full-map coordinates and random sequence") {
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

DOCTEST_TEST_CASE_FIXTURE(s_spawn_fixture, "nonplayer spawn coordinates and subsequent randomness are independent of zoom") {
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

DOCTEST_TEST_CASE_FIXTURE(s_spawn_fixture, "nonrandomized nonplayer spawning uses the star coordinates without consuming randomness") {
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
