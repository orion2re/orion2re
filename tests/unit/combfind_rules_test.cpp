#include "pch.h"

#include "doctest/doctest.h"
#include "game/combfind.h"

namespace {
    s_planet_data planets[MAX_PLANETS];
    uint32_t random_result;
    uint32_t last_random_bound;
    int random_call_count;

    void Reset_() {
        memset(planets, 0, sizeof(planets));
        COMBFIND::monsters_stay = false;
        COMBFIND::monsters_stay_chance = 2;
        random_result = 1;
        last_random_bound = 0;
        random_call_count = 0;
    }
} // namespace

namespace MOX {
    s_planet_data* planet = planets;
} // namespace MOX

namespace game_random {
    uint32_t __cdecl Random(uint32_t bound) {
        last_random_bound = bound;
        random_call_count++;
        return random_result;
    }
} // namespace game_random

DOCTEST_TEST_CASE("battle locations include the runtime Antares slot") {
    const int16_t star_counts[] = {71, 99};

    for (int16_t star_count : star_counts) {
        DOCTEST_CHECK(COMBFIND::Is_Star_Or_Antares_Location(
            static_cast<int16_t>(star_count - 1), star_count));
        DOCTEST_CHECK(COMBFIND::Is_Star_Or_Antares_Location(
            star_count, star_count));
        DOCTEST_CHECK_FALSE(COMBFIND::Is_Star_Or_Antares_Location(
            static_cast<int16_t>(star_count + 1), star_count));
    }

    DOCTEST_CHECK_FALSE(COMBFIND::Is_Star_Or_Antares_Location(-1, 99));
}

DOCTEST_TEST_CASE("one-shot discovery cleanup preserves Ancient Artifacts") {
    Reset_();

    s_star_data star = {};
    memset(star.planet_index, 0xFF, sizeof(star.planet_index));
    star.planet_index[0] = 0;

    for (int marker = 50; marker < 90; ++marker) {
        star.system_special = static_cast<int8_t>(marker);
        planets[0].planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
        COMBFIND::Reset_One_Shot_System_Special(&::game::world, &star);
        DOCTEST_CHECK(star.system_special == SYSTEM_SPECIAL_NO_SPECIAL);
        DOCTEST_CHECK(
            planets[0].planet_special == SYSTEM_SPECIAL_NO_SPECIAL);
    }

    for (int marker = 90; marker < 100; ++marker) {
        star.system_special = static_cast<int8_t>(marker);
        planets[0].planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
        COMBFIND::Reset_One_Shot_System_Special(&::game::world, &star);
        DOCTEST_CHECK(star.system_special == SYSTEM_SPECIAL_NO_SPECIAL);
        DOCTEST_CHECK(
            planets[0].planet_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS);
    }
}

DOCTEST_TEST_CASE("forced stay retains every wandering monster") {
    Reset_();
    COMBFIND::monsters_stay = true;

    DOCTEST_CHECK_FALSE(COMBFIND::Wandering_Monster_Retires(
        NONPLAYER_SHIP_TYPE_CRYSTAL));
    DOCTEST_CHECK_FALSE(COMBFIND::Wandering_Monster_Retires(
        NONPLAYER_SHIP_TYPE_DRAGON));
    DOCTEST_CHECK_FALSE(COMBFIND::Wandering_Monster_Retires(
        NONPLAYER_SHIP_TYPE_HYDRA));
    DOCTEST_CHECK_FALSE(COMBFIND::Wandering_Monster_Retires(
        NONPLAYER_SHIP_TYPE_AMOEBA, true));
    DOCTEST_CHECK(random_call_count == 0);
}

DOCTEST_TEST_CASE("wandering monster retirement uses one configured roll") {
    Reset_();
    COMBFIND::monsters_stay_chance = 7;

    DOCTEST_CHECK(COMBFIND::Wandering_Monster_Retires(
        NONPLAYER_SHIP_TYPE_CRYSTAL));
    DOCTEST_CHECK(random_call_count == 1);
    DOCTEST_CHECK(last_random_bound == 7);

    random_result = 2;
    DOCTEST_CHECK_FALSE(COMBFIND::Wandering_Monster_Retires(
        NONPLAYER_SHIP_TYPE_DRAGON));
    DOCTEST_CHECK(random_call_count == 2);
    DOCTEST_CHECK(last_random_bound == 7);
}

DOCTEST_TEST_CASE("zero retirement bound deterministically retires eligible monsters") {
    Reset_();
    COMBFIND::monsters_stay_chance = 0;

    const int8_t eligible_monsters[] = {
        NONPLAYER_SHIP_TYPE_CRYSTAL,
        NONPLAYER_SHIP_TYPE_DRAGON,
        NONPLAYER_SHIP_TYPE_HYDRA,
    };
    for (int8_t monster_type : eligible_monsters) {
        DOCTEST_CHECK(COMBFIND::Wandering_Monster_Retires(monster_type));
    }
    DOCTEST_CHECK(random_call_count == 3);
    DOCTEST_CHECK(last_random_bound == 0);
}

DOCTEST_TEST_CASE("Eels and non-toxicizing Amoebae stay") {
    Reset_();

    DOCTEST_CHECK_FALSE(COMBFIND::Wandering_Monster_Retires(
        NONPLAYER_SHIP_TYPE_EEL));
    DOCTEST_CHECK_FALSE(COMBFIND::Wandering_Monster_Retires(
        NONPLAYER_SHIP_TYPE_AMOEBA));
    DOCTEST_CHECK(random_call_count == 0);
}

DOCTEST_TEST_CASE("toxicizing Amoebae retire under the default policy") {
    Reset_();

    DOCTEST_CHECK(COMBFIND::Wandering_Monster_Retires(
        NONPLAYER_SHIP_TYPE_AMOEBA, true));
    DOCTEST_CHECK(random_call_count == 0);
}