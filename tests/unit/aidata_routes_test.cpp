#include "pch.h"

#include "doctest/doctest.h"

namespace {
    s_player players[1];
    s_star_data stars[3];
    s_ship_data ships[1];
    int16_t player_count = 1;

    void Prepare(AIDATA::s_state* ai) {
        *ai = {};
        ai->world.players = players;
        ai->world.ships = ships;
        ai->world.stars = stars;
        ai->world.star_count = &MOX::NUM_STARS;
        ai->world.player_count = &player_count;
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

DOCTEST_TEST_CASE("prepared and ID-based route searches agree for both range modes") {
    AIDATA::s_state regular;
    Prepare(&regular);
    regular.stars[1].in_range = 0;
    AIDATA::s_state prepared = regular;
    AIDATA::s_route_search_lookup lookup;
    lookup.Prepare(&prepared);
    DOCTEST_REQUIRE(lookup.routes.rows.size() == 1);
    DOCTEST_CHECK(lookup.routes.rows[0].size() == 3);
    for (uint32_t source = 0; source < 3; ++source) {
        for (uint32_t destination = 0; destination < 3; ++destination) {
            for (bool extended : {false, true}) {
                const auto expected = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                    &regular, 0, source, destination, extended);
                const auto actual = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                    &prepared, 0, source, destination, extended, &lookup);
                DOCTEST_REQUIRE(actual.has_value() == expected.has_value());
                if (expected) {
                    DOCTEST_CHECK(actual->next_star == expected->next_star);
                    DOCTEST_CHECK(actual->turns == expected->turns);
                }
            }
        }
    }
    DOCTEST_CHECK(prepared.routes.intermediate.size() == regular.routes.intermediate.size());
}

DOCTEST_TEST_CASE("prepared star references remain live across assessment-map rehash") {
    AIDATA::s_state ai;
    Prepare(&ai);
    AIDATA::s_route_search_lookup lookup;
    lookup.Prepare(&ai);
    ai.stars.reserve(512);
    ai.stars[1].in_range = 0;
    const auto normal = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 2, false, &lookup);
    const auto extended = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 2, true, &lookup);
    DOCTEST_REQUIRE(normal);
    DOCTEST_REQUIRE(extended);
    DOCTEST_CHECK(normal->turns == 10);
    DOCTEST_CHECK(extended->turns == 4);
}

DOCTEST_TEST_CASE("route search rejects prepared lookups from another or changed table") {
    AIDATA::s_state first;
    Prepare(&first);
    AIDATA::s_route_search_lookup lookup;
    lookup.Prepare(&first);
    AIDATA::s_state second = first;
    second.routes.direct.Store(0, 1, 0, 7);
    auto route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&second, 0, 0, 2, false, &lookup);
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 9);

    first.routes.direct.Clear();
    DOCTEST_CHECK_FALSE(AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&first, 0, 0, 2, false, &lookup));
    lookup.Prepare(&first);
    first.routes.intermediate.clear();
    first.routes.direct.Store(0, 2, 0, 2);
    route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&first, 0, 0, 2, false, &lookup);
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 2);
}

DOCTEST_TEST_CASE("equal-turn routes retain descending-star tie breaks in both lookup paths") {
    for (bool prepared : {false, true}) {
        AIDATA::s_state ai;
        Prepare(&ai);
        players[0].ship_speed = 0;
        ai.stars[2].in_range = 0;
        ai.routes.direct.Store(0, 2, 0, 4);
        AIDATA::s_route_search_lookup lookup;
        lookup.Prepare(&ai);
        const auto* context = prepared ? &lookup : nullptr;
        const auto extended = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 2, true, context);
        const auto normal = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, 0, 0, 2, false, context);
        DOCTEST_REQUIRE(extended);
        DOCTEST_REQUIRE(normal);
        DOCTEST_CHECK(extended->turns == 4);
        DOCTEST_CHECK(extended->next_star == 2);
        DOCTEST_CHECK(normal->turns == 4);
        DOCTEST_CHECK(normal->next_star == 1);
    }
}
