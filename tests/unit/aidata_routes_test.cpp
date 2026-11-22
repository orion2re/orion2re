#include "pch.h"
#include "../fixtures/star_roster.h"

#include "doctest/doctest.h"

using legacy_world::Star_Id;
using legacy_world::Ship_Id;
using legacy_world::Player_Id;

namespace {
    s_player players[1];
    test_stars::Slots<3> stars;
    s_ship_data ships[1];
    int16_t player_count = 1;

    const game::s_view* World_() {
        static const game::s_view world{
            .players = players,
            .ships = ships,
            .state = &game::current_state,
            .player_count = &player_count,

        };
        return &world;
    }

    void Prepare(AIDATA::s_state* ai) {
        *ai = {};
        memset(players, 0, sizeof(players));
        stars.Reset();
        memset(ships, 0, sizeof(ships));
        players[0].ship_speed = 3;
        for (uint32_t star = 0; star < 3; ++star) {
            stars[star].x = 90 * star;
            stars[star].wormhole_star_id = game::NO_STAR;
            ai->stars[Star_Id(star)].in_range = 1;
            ai->stars[Star_Id(star)].in_extended_range = 1;
        }
        ai->routes.direct.Store(Star_Id(0), Star_Id(0), Player_Id(0), 0);
        ai->routes.direct.Store(Star_Id(0), Star_Id(1), Player_Id(0), 2);
        ai->routes.direct.Store(Star_Id(1), Star_Id(2), Player_Id(0), 2);
        ai->routes.direct.Store(Star_Id(2), Star_Id(2), Player_Id(0), 0);
        ai->routes.direct.Store(Star_Id(0), Star_Id(2), Player_Id(0), 10);
    }
} // namespace

namespace MOX {
    s_player* player = players;
    s_ship_data* ship = ships;

} // namespace MOX

namespace GEO {
    int32_t __cdecl Parsecs_Between_Points(int16_t x1, int16_t, int16_t x2, int16_t) {
        return (std::abs(x1 - x2) + 29) / 30;
    }
    bool Black_Hole_Blocks_Points(const game::s_game_state& state, int32_t, int32_t, int32_t, int32_t) {
        return std::ranges::any_of(state.stars, [](const auto& entry) {
            return entry.second.spectral_class == STAR_CLASS_BLACK_HOLE;
        });
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
    second.routes.direct.Store(Star_Id(0), Star_Id(1), Player_Id(0), 7);
    auto first_route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&first, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false);
    auto second_route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&second, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false);
    DOCTEST_REQUIRE(first_route);
    DOCTEST_REQUIRE(second_route);
    DOCTEST_CHECK(first_route->turns == 4);
    DOCTEST_CHECK(first_route->next_star == Star_Id(1));
    DOCTEST_CHECK(second_route->turns == 9);
    DOCTEST_CHECK(second_route->next_star == Star_Id(1));

    first.routes.direct.Store(Star_Id(0), Star_Id(1), Player_Id(0), 8);
    first_route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&first, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false);
    DOCTEST_REQUIRE(first_route);
    DOCTEST_CHECK(first_route->turns == 4);
    DOCTEST_CHECK(first.routes.intermediate.size() == 1);
    DOCTEST_CHECK(second.routes.intermediate.size() == 1);
}

DOCTEST_TEST_CASE("intermediate routes and wormhole departures use the supplied sparse roster") {
    AIDATA::s_state ai;
    Prepare(&ai);
    ai = {};
    game::s_game_state state;
    state.next_star_id = 90000;
    const auto source = *state.Create_Star({.x = 120});
    const auto stop = *state.Create_Star({.x = 180});
    const auto target = *state.Create_Star({.x = 240});
    std::ranges::reverse(state.star_order);
    state.star_order_sorted = false;
    auto world = *World_();
    world.state = &state;
    ships[0].x = 120;
    for (const auto key : {source, stop, target}) {
        ai.stars[key].in_range = ai.stars[key].in_extended_range = 1;
    }
    ai.routes.direct.Store(source, stop, Player_Id(0), 2);
    ai.routes.direct.Store(stop, target, Player_Id(0), 2);
    ai.routes.direct.Store(source, target, Player_Id(0), 10);
    DOCTEST_SUBCASE("colony fleet") {
        const auto route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, &world, Player_Id(0), source, target, false);
        DOCTEST_REQUIRE(route);
        DOCTEST_CHECK(route->next_star == stop);
        DOCTEST_CHECK(route->turns == 4);
    }
    DOCTEST_SUBCASE("recent wormhole departure") {
        ships[0].position.Set({game::e_ship_location::in_wormhole, target});
        const auto route = AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, &world, Ship_Id(0), target);
        DOCTEST_REQUIRE(route);
        DOCTEST_CHECK(route->next_star == stop);
        DOCTEST_CHECK(route->turns == 4);
    }
    DOCTEST_SUBCASE("black holes on an in-flight route come from the supplied roster") {
        ships[0].position.Set({game::e_ship_location::in_flight, target});
        ai.routes.ship_direct.Store(Ship_Id(0), stop, Player_Id(0), 2);
        stars[1].spectral_class = STAR_CLASS_BLACK_HOLE;
        auto route = AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, &world, Ship_Id(0), target);
        DOCTEST_REQUIRE(route);
        DOCTEST_CHECK(route->next_star == stop);
        state.stars.at(source).spectral_class = STAR_CLASS_BLACK_HOLE;
        ai.routes.ship_intermediate.clear();
        DOCTEST_CHECK_FALSE(AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, &world, Ship_Id(0), target));
    }
}

DOCTEST_TEST_CASE("one route entry retains distinct normal and extended results") {
    AIDATA::s_state ai;
    Prepare(&ai);
    ai.stars[Star_Id(1)].in_range = 0;
    auto extended = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(2), true);
    auto normal = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false);
    DOCTEST_REQUIRE(extended);
    DOCTEST_REQUIRE(normal);
    DOCTEST_CHECK(extended->turns == 4);
    DOCTEST_CHECK(extended->next_star == Star_Id(1));
    DOCTEST_CHECK(normal->turns == 10);
    DOCTEST_CHECK(normal->next_star == Star_Id(2));
    DOCTEST_CHECK(ai.routes.intermediate.size() == 1);
}

DOCTEST_TEST_CASE("unreachable intermediate routes are cached rather than left uncomputed") {
    AIDATA::s_state ai;
    Prepare(&ai);
    ai.routes.direct.Clear();
    DOCTEST_CHECK_FALSE(AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false));
    DOCTEST_REQUIRE(ai.routes.intermediate.size() == 1);
    DOCTEST_CHECK_FALSE(ai.routes.intermediate.begin()->second.normal);
    DOCTEST_CHECK_FALSE(ai.routes.intermediate.begin()->second.extended);
}

DOCTEST_TEST_CASE("ship intermediate routes reuse the same explicit route workspace") {
    AIDATA::s_state ai;
    Prepare(&ai);
    auto route = AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, World_(), Ship_Id(0), Star_Id(2));
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 4);
    DOCTEST_CHECK(route->next_star == Star_Id(1));
    route = AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, World_(), Ship_Id(0), Star_Id(2));
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 4);
    DOCTEST_CHECK(ai.routes.ship_intermediate.size() == 1);
}

DOCTEST_TEST_CASE("direct routes preserve a zero-turn route and absent normal range") {
    AIDATA::s_state ai;
    Prepare(&ai);
    auto same_star = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(0), false);
    DOCTEST_REQUIRE(same_star);
    DOCTEST_CHECK(same_star->next_star == Star_Id(0));
    DOCTEST_CHECK(same_star->turns == 0);

    ai.routes.direct.Store(Star_Id(0), Star_Id(2), Player_Id(0), 2);
    ai.stars[Star_Id(2)].in_range = 0;
    auto extended = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(2), true);
    DOCTEST_REQUIRE(extended);
    DOCTEST_CHECK(extended->next_star == Star_Id(2));
    DOCTEST_CHECK(extended->turns == 2);
    DOCTEST_CHECK_FALSE(AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false));
}

DOCTEST_TEST_CASE("moving ship routes retain computed-unreachable cache entries") {
    AIDATA::s_state ai;
    Prepare(&ai);
    ships[0].position.Set({game::e_ship_location::in_flight, Star_Id(0)});
    DOCTEST_CHECK_FALSE(AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, World_(), Ship_Id(0), Star_Id(2)));
    DOCTEST_REQUIRE(ai.routes.ship_intermediate.size() == 1);
    DOCTEST_CHECK_FALSE(ai.routes.ship_intermediate.begin()->second);

    ai.routes.ship_direct.Store(Ship_Id(0), Star_Id(1), Player_Id(0), 2);
    DOCTEST_CHECK_FALSE(AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, World_(), Ship_Id(0), Star_Id(2)));
    ai.routes.ship_intermediate.clear();
    auto route = AIDATA::Ship_N_Turns_To_Star_With_Intermediate(&ai, World_(), Ship_Id(0), Star_Id(2));
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->next_star == Star_Id(1));
    DOCTEST_CHECK(route->turns == 4);
}

DOCTEST_TEST_CASE("prepared and ID-based route searches agree for both range modes") {
    AIDATA::s_state regular;
    Prepare(&regular);
    regular.stars[Star_Id(1)].in_range = 0;
    AIDATA::s_state prepared = regular;
    prepared.routes.direct.Prepare_Live_Indices(World_()->Star_Count(), World_()->Player_Count());
    DOCTEST_REQUIRE(prepared.routes.direct.live_rows.size() == 1);
    DOCTEST_CHECK(prepared.routes.direct.live_rows[0].size() == 3);
    for (uint32_t source = 0; source < 3; ++source) {
        for (uint32_t destination = 0; destination < 3; ++destination) {
            for (bool extended : {false, true}) {
                const auto expected = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                    &regular, World_(), Player_Id(0), Star_Id(source), Star_Id(destination), extended);
                const auto actual = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                    &prepared, World_(), Player_Id(0), Star_Id(source), Star_Id(destination), extended);
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

DOCTEST_TEST_CASE("route searches read canonical star assessments after rehash") {
    AIDATA::s_state ai;
    Prepare(&ai);
    ai.routes.direct.Prepare_Live_Indices(World_()->Star_Count(), World_()->Player_Count());
    ai.stars.reserve(512);
    ai.stars[Star_Id(1)].in_range = 0;
    const auto normal = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false);
    const auto extended = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(2), true);
    DOCTEST_REQUIRE(normal);
    DOCTEST_REQUIRE(extended);
    DOCTEST_CHECK(normal->turns == 10);
    DOCTEST_CHECK(extended->turns == 4);
}

DOCTEST_TEST_CASE("owned route indices follow their table across copying and replacement") {
    AIDATA::s_state first;
    Prepare(&first);
    first.routes.direct.Prepare_Live_Indices(World_()->Star_Count(), World_()->Player_Count());
    AIDATA::s_state second = first;
    second.routes.direct.Store(Star_Id(0), Star_Id(1), Player_Id(0), 7);
    auto route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&second, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false);
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 9);

    first.routes.direct.Clear();
    DOCTEST_CHECK_FALSE(AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&first, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false));
    first.routes.direct.Prepare_Live_Indices(World_()->Star_Count(), World_()->Player_Count());
    first.routes.intermediate.clear();
    first.routes.direct.Store(Star_Id(0), Star_Id(2), Player_Id(0), 2);
    route = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&first, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false);
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 2);
}

DOCTEST_TEST_CASE("equal-turn routes retain descending-star tie breaks in both lookup paths") {
    for (bool prepared : {false, true}) {
        AIDATA::s_state ai;
        Prepare(&ai);
        players[0].ship_speed = 0;
        ai.stars[Star_Id(2)].in_range = 0;
        ai.routes.direct.Store(Star_Id(0), Star_Id(2), Player_Id(0), 4);
        if (prepared) ai.routes.direct.Prepare_Live_Indices(World_()->Star_Count(), World_()->Player_Count());
        const auto extended = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(2), true);
        const auto normal = AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(&ai, World_(), Player_Id(0), Star_Id(0), Star_Id(2), false);
        DOCTEST_REQUIRE(extended);
        DOCTEST_REQUIRE(normal);
        DOCTEST_CHECK(extended->turns == 4);
        DOCTEST_CHECK(extended->next_star == Star_Id(2));
        DOCTEST_CHECK(normal->turns == 4);
        DOCTEST_CHECK(normal->next_star == Star_Id(1));
    }
}
