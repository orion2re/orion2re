#include "pch.h"

#include "doctest/doctest.h"

namespace {
    s_player players[1];
    s_star_data stars[3];
    s_ship_data ships[1];

    void Prepare(AIDATA::s_state* ai) {
        *ai = {};
        ai->world.players = players;
        ai->world.ships = ships;
        ai->world.stars = stars;
        ai->world.star_count = &MOX::NUM_STARS;
        memset(players, 0, sizeof(players));
        memset(stars, 0, sizeof(stars));
        memset(ships, 0, sizeof(ships));
        players[0].ship_speed = 3;
        for (uint32_t star = 0; star < 3; ++star) {
            stars[star].wormhole_star_id = -1;
            ai->stars[star].in_range = 1;
            ai->stars[star].in_extended_range = 1;
        }
        ai->routes.direct.Store(0, 0, 0, 0);
        ai->routes.direct.Store(0, 1, 0, 2);
        ai->routes.direct.Store(1, 2, 0, 2);
        ai->routes.direct.Store(2, 2, 0, 0);
        ai->routes.direct.Store(0, 2, 0, 10);
    }
} // namespace

namespace MOX {
    s_player* player = players;
    s_star_data* star = stars;
    s_ship_data* ship = ships;
    int16_t NUM_STARS = 3;
} // namespace MOX

namespace GEO {
    int32_t __cdecl Parsecs_Between_Stars(int16_t source, int16_t destination) {
        return source == destination ? 0 : 6;
    }
    bool __cdecl Black_Hole_Blocks_Points(int32_t, int32_t, int32_t, int32_t) {
        return false;
    }
} // namespace GEO

namespace HAROLD {
    int16_t __cdecl XY_To_Star_Id(int16_t, int16_t) {
        return 0;
    }
} // namespace HAROLD

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void* bits, uint16_t index) {
        return (static_cast<const uint8_t*>(bits)[index / 8] >> (index % 8)) & 1;
    }
} // namespace struct_

DOCTEST_TEST_CASE("intermediate route computations use the supplied AI workspace") {
    AIDATA::s_state first;
    AIDATA::s_state second;
    Prepare(&first);
    Prepare(&second);
    second.routes.direct.Store(0, 1, 0, 7);
    auto first_route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&first, 0, 0, 2, false);
    auto second_route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&second, 0, 0, 2, false);
    DOCTEST_REQUIRE(first_route);
    DOCTEST_REQUIRE(second_route);
    DOCTEST_CHECK(first_route->turns == 4);
    DOCTEST_CHECK(first_route->next_star == 1);
    DOCTEST_CHECK(second_route->turns == 9);
    DOCTEST_CHECK(second_route->next_star == 1);

    first.routes.direct.Store(0, 1, 0, 8);
    first_route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&first, 0, 0, 2, false);
    DOCTEST_REQUIRE(first_route);
    DOCTEST_CHECK(first_route->turns == 4);
    DOCTEST_CHECK(first.routes.intermediate.size() == 1);
    DOCTEST_CHECK(second.routes.intermediate.size() == 1);
}

DOCTEST_TEST_CASE("one route entry retains distinct normal and extended results") {
    AIDATA::s_state ai;
    Prepare(&ai);
    ai.stars[1].in_range = 0;
    auto extended = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 2, true);
    auto normal = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 2, false);
    DOCTEST_REQUIRE(extended);
    DOCTEST_REQUIRE(normal);
    DOCTEST_CHECK(extended->turns == 4);
    DOCTEST_CHECK(extended->next_star == 1);
    DOCTEST_CHECK(normal->turns == 10);
    DOCTEST_CHECK(normal->next_star == 2);
    DOCTEST_CHECK(ai.routes.intermediate.size() == 1);
}

DOCTEST_TEST_CASE("unreachable intermediate routes are cached rather than left uncomputed") {
    AIDATA::s_state ai;
    Prepare(&ai);
    ai.routes.direct.Clear();
    DOCTEST_CHECK_FALSE(AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 2, false));
    DOCTEST_REQUIRE(ai.routes.intermediate.size() == 1);
    DOCTEST_CHECK_FALSE(ai.routes.intermediate.begin()->second.normal);
    DOCTEST_CHECK_FALSE(ai.routes.intermediate.begin()->second.extended);
}

DOCTEST_TEST_CASE("ship intermediate routes reuse the same explicit route workspace") {
    AIDATA::s_state ai;
    Prepare(&ai);
    auto route = AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, 0, 2);
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 4);
    DOCTEST_CHECK(route->next_star == 1);
    route = AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, 0, 2);
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 4);
    DOCTEST_CHECK(ai.routes.ship_intermediate.size() == 1);
}

DOCTEST_TEST_CASE("direct routes preserve a zero-turn route and absent normal range") {
    AIDATA::s_state ai;
    Prepare(&ai);
    auto same_star = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 0, false);
    DOCTEST_REQUIRE(same_star);
    DOCTEST_CHECK(same_star->next_star == 0);
    DOCTEST_CHECK(same_star->turns == 0);

    ai.routes.direct.Store(0, 2, 0, 2);
    ai.stars[2].in_range = 0;
    auto extended = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 2, true);
    DOCTEST_REQUIRE(extended);
    DOCTEST_CHECK(extended->next_star == 2);
    DOCTEST_CHECK(extended->turns == 2);
    DOCTEST_CHECK_FALSE(AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 2, false));
}

DOCTEST_TEST_CASE("moving ship routes retain computed-unreachable cache entries") {
    AIDATA::s_state ai;
    Prepare(&ai);
    ships[0].location = SHIP_LOCATION_MOVING_OFFSET;
    DOCTEST_CHECK_FALSE(AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, 0, 2));
    DOCTEST_REQUIRE(ai.routes.ship_intermediate.size() == 1);
    DOCTEST_CHECK_FALSE(ai.routes.ship_intermediate.begin()->second);

    ai.routes.ship_direct.Store(0, 1, 0, 2);
    DOCTEST_CHECK_FALSE(AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, 0, 2));
    ai.routes.ship_intermediate.clear();
    auto route = AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, 0, 2);
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->next_star == 1);
    DOCTEST_CHECK(route->turns == 4);
}
