#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"
#include "game/combfind.h"
#include "game/combat_fleets.h"

namespace {

    uint32_t random_result;
    uint32_t last_random_bound;
    int random_call_count;

    void Reset_() {
        test_planets::Reset(0);
        COMBFIND::monsters_stay = false;
        COMBFIND::monsters_stay_chance = 2;
        random_result = 1;
        last_random_bound = 0;
        random_call_count = 0;
    }
} // namespace

namespace MOX {

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
            game::ANTARES_POSITION, star_count));
        DOCTEST_CHECK_FALSE(COMBFIND::Is_Star_Or_Antares_Location(star_count, star_count));
        DOCTEST_CHECK_FALSE(COMBFIND::Is_Star_Or_Antares_Location(
            static_cast<int16_t>(star_count + 1), star_count));
    }

    DOCTEST_CHECK_FALSE(COMBFIND::Is_Star_Or_Antares_Location(-1, 99));
}

DOCTEST_TEST_CASE("combat fleets preserve side order and ignore reused rows") {
    game::s_game_state state;
    state.next_star_id = 70000;
    const auto star = game::Create_Star(state);
    DOCTEST_REQUIRE(star);
    ship_slots::Resize(state, 5);
    state.next_ship_id = 90006;
    test_ships::Rows(state).resize(5);
    for (int16_t row = 0; row < 5; ++row) {
        s_ship_data ship{};
        ship.id = game::id_ship{90000u + static_cast<uint32_t>(row)};
        ship.owner = factions::From_Legacy(row % 2);
        ship.status = row == 4 ? 5 : 0;
        ship.position.Set({game::e_ship_location::at_star, *star});
        test_ships::Rows(state).Replace_Row(row, ship);
    }
    const auto fleets = game::Select_Combat_Fleets(state, 0, 0, 1);
    DOCTEST_CHECK((fleets.attacker == std::vector{game::id_ship{90000}, game::id_ship{90002}}));
    DOCTEST_CHECK((fleets.defender == std::vector{game::id_ship{90003}, game::id_ship{90001}}));

    test_ships::Rows(state).Swap_Rows(0, 2);
    auto& world = state;
    DOCTEST_CHECK(game::Active_Combat_Keys(world, fleets.attacker, 0, 0) == fleets.attacker);
    auto keys = fleets.attacker;
    test_ships::Rows(state)[0].d.size = SHIP_SIZE_LARGE;
    test_ships::Rows(state)[2].d.size = SHIP_SIZE_LARGE;
    int16_t rows[] = {2, 0};
    Legacy_Sort(std::span<int16_t>(rows), [&](int16_t a, int16_t b) {
        return test_ships::Rows(state)[b].d.size - test_ships::Rows(state)[a].d.size;
    });
    game::Sort_Combat_Ship_Keys(world, keys);
    DOCTEST_CHECK((keys == std::vector{test_ships::Rows(state)[rows[0]].id, test_ships::Rows(state)[rows[1]].id}));
    test_ships::Rows(state).Set_Key_At_Row(0, game::id_ship{90005});
    DOCTEST_CHECK(game::Active_Combat_Keys(world, keys, 0, 0) == std::vector{game::id_ship{90000}});
}

DOCTEST_TEST_CASE("one-shot discovery cleanup preserves Ancient Artifacts") {
    Reset_();

    s_star_data star = {};
    for (auto& link : star.orbit_planet_ids) link = game::NO_PLANET;
    star.orbit_planet_ids[0] = game::id_planet{0};

    for (int marker = 50; marker < 90; ++marker) {
        star.system_special = static_cast<int8_t>(marker);
        test_planets::Record(0).planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
        COMBFIND::Reset_One_Shot_System_Special(&star);
        DOCTEST_CHECK(star.system_special == SYSTEM_SPECIAL_NO_SPECIAL);
        DOCTEST_CHECK(
            test_planets::Record(0).planet_special == SYSTEM_SPECIAL_NO_SPECIAL);
    }

    for (int marker = 90; marker < 100; ++marker) {
        star.system_special = static_cast<int8_t>(marker);
        test_planets::Record(0).planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
        COMBFIND::Reset_One_Shot_System_Special(&star);
        DOCTEST_CHECK(star.system_special == SYSTEM_SPECIAL_NO_SPECIAL);
        DOCTEST_CHECK(
            test_planets::Record(0).planet_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS);
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
