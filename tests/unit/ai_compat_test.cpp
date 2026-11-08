#include "pch.h"

#include "doctest/doctest.h"
#include "game/legacy_world.h"

DOCTEST_TEST_CASE("typed ship locations round-trip legacy orbit and transit encodings") {
    using game_world::e_ship_location;
    for (auto kind : {e_ship_location::at_star, e_ship_location::in_flight, e_ship_location::in_wormhole}) {
        for (uint32_t star : {0u, 1u, (uint32_t)MAX_STARS}) {
            const game_world::s_ship_location location{kind, star};
            auto encoded = legacy_world::Encode_Ship_Location(&location);
            DOCTEST_REQUIRE(encoded);
            auto decoded = legacy_world::Decode_Ship_Location(*encoded);
            DOCTEST_REQUIRE(decoded);
            DOCTEST_CHECK(decoded->kind == kind);
            DOCTEST_CHECK(decoded->star_id == star);
        }
    }
    for (int32_t invalid : {-1, MAX_STARS + 1, SHIP_LOCATION_STEP - 1, SHIP_LOCATION_LIMIT}) {
        DOCTEST_CHECK_FALSE(legacy_world::Decode_Ship_Location(invalid));
    }
    const game_world::s_ship_location high{e_ship_location::at_star, UINT32_MAX};
    DOCTEST_CHECK_FALSE(legacy_world::Encode_Ship_Location(&high));
}

DOCTEST_TEST_CASE("location compatibility preserves transit kind without exposing offset arithmetic") {
    s_ship_data ship{};
    ship.status = 1;
    ship.location = SHIP_LOCATION_MOVING_OFFSET + 9;
    auto location = legacy_world::Read_Ship_Location(&ship);
    DOCTEST_REQUIRE(location);
    DOCTEST_CHECK(location->kind == game_world::e_ship_location::in_flight);
    DOCTEST_CHECK(location->star_id == 9);
    DOCTEST_CHECK_FALSE(legacy_world::Orbit_Star(&ship));
    ship.status = 4;
    ship.location = 0;
    DOCTEST_CHECK(legacy_world::Orbit_Star(&ship) == 0);
}

DOCTEST_TEST_CASE("movement presence and flee lookups do not insert zero assessments") {
    AIDATA::s_movement_workspace work;
    DOCTEST_CHECK_FALSE(AIDATA::Nonplayer_At(&work, 0));
    DOCTEST_CHECK_FALSE(AIDATA::Is_Fleeing(&work, UINT32_MAX, 0));
    DOCTEST_CHECK(work.nonplayer_here.empty());
    DOCTEST_CHECK(work.flee_stars.empty());
    work.nonplayer_here[UINT32_MAX] = MAX_PLAYERS;
    work.flee_stars[0][AIDATA::LAST_FORECAST_TURN] = 1;
    DOCTEST_CHECK(AIDATA::Nonplayer_At(&work, UINT32_MAX) == MAX_PLAYERS);
    DOCTEST_CHECK(AIDATA::Is_Fleeing(&work, 0, AIDATA::LAST_FORECAST_TURN));
    DOCTEST_CHECK_FALSE(AIDATA::Is_Fleeing(&work, 0, 0));
    DOCTEST_CHECK(work.nonplayer_here.size() == 1);
    DOCTEST_CHECK(work.flee_stars.size() == 1);
}

DOCTEST_TEST_CASE("strategic AI input validation rejects missing session data") {
    game_world::s_view world;
    DOCTEST_CHECK_FALSE(game_world::Has_Strategic_AI_Inputs(nullptr));
    DOCTEST_CHECK_FALSE(game_world::Has_Strategic_AI_Inputs(&world));
}

DOCTEST_TEST_CASE("production queue removal keeps colony identities and queue positions distinct") {
    s_colony colonies[3] = {};
    const game_world::s_view world{.colonies = colonies};
    uint32_t queue[] = {2, 0, 1};
    int32_t count = 3;
    AIBUILD::Force_Produce(&world, queue, 1, &count, COLONY_PRODUCTION_HOUSING);
    DOCTEST_CHECK(count == 2);
    DOCTEST_CHECK(queue[0] == 2);
    DOCTEST_CHECK(queue[1] == 1);
    DOCTEST_CHECK((int16_t)colonies[0].producing[0] == COLONY_PRODUCTION_HOUSING);
}
