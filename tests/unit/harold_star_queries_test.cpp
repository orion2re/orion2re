#include "pch.h"

#include "doctest/doctest.h"
#include "../fixtures/game_state.h"

DOCTEST_TEST_CASE("fleet visibility reads Antares without indexing the star roster") {
    test::s_game_state fixture;
    constexpr game::id_player player{0};
    const auto star = game::Create_Star(game::current_state);
    DOCTEST_REQUIRE(star);
    game::current_state.stars.at(*star).visited.set(player);
    DOCTEST_CHECK(HAROLD::Player_Has_Visited(player, 0));
    DOCTEST_CHECK_FALSE(HAROLD::Player_Has_Visited(player, game::ANTARES_POSITION));
    game::current_state.antares_destination.visited.set(player);
    DOCTEST_CHECK(HAROLD::Player_Has_Visited(player, game::ANTARES_POSITION));
    DOCTEST_CHECK_FALSE(HAROLD::Player_Has_Visited(player, -1));
}

DOCTEST_TEST_CASE("monster lookup uses ship slots and exact orbit keys") {
    test::s_game_state fixture;
    auto& state = game::current_state;
    const auto other = game::Create_Star(state, {.x = 100, .y = 100});
    state.next_star_id = 90000;
    const auto target = game::Create_Star(state, {.x = 100, .y = 100});
    DOCTEST_REQUIRE(other);
    DOCTEST_REQUIRE(target);
    ship_slots::Resize(game::current_state, 5);
    const game::s_ship_location orbit{game::e_ship_location::at_star, *target};
    fixture.ships[0].position.Set(orbit); // Player fleet.
    fixture.ships[1].owner = factions::From_Legacy(MAX_PLAYERS);
    fixture.ships[1].position.Set({game::e_ship_location::at_star, *other});
    fixture.ships[2].owner = factions::From_Legacy(MAX_PLAYERS);
    fixture.ships[2].status = 1;
    fixture.ships[2].position.Set({game::e_ship_location::in_flight, *target});
    fixture.ships[3].owner = factions::From_Legacy(MAX_PLAYERS);
    fixture.ships[3].status = 5;
    fixture.ships[3].position.Set(orbit);

    DOCTEST_CHECK(HAROLD::First_Monster_Orbiting_Star(*target) == -1);
    fixture.ships[4].owner = factions::From_Legacy(MAX_PLAYERS);
    fixture.ships[4].position.Set(orbit);
    DOCTEST_CHECK(HAROLD::First_Monster_Orbiting_Star(*target) == 4);
}

DOCTEST_TEST_CASE("coordinate lookup breaks ties by row after draft replacement") {
    test::s_game_state fixture;
    auto& state = game::current_state;
    DOCTEST_REQUIRE(game::Create_Star(state));
    DOCTEST_REQUIRE(game::Create_Star(state, {.x = 100, .y = 100}));
    state.next_star_id = 90000;
    const auto replacement = game::Replace_Draft_Star(state, 0);
    DOCTEST_REQUIRE(replacement);
    state.stars.at(*replacement).x = 100;
    state.stars.at(*replacement).y = 100;
    state.antares_destination.x = 100;
    state.antares_destination.y = 100;

    DOCTEST_CHECK(game::Find_Star_At_Coordinates(state, 100, 100) == replacement);
    DOCTEST_CHECK(HAROLD::XY_To_Star_Id(100, 100) == 0);
    DOCTEST_CHECK(HAROLD::XY_To_Star_Id(101, 100) == -1);
}
