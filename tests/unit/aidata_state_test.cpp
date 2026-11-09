#include "pch.h"

#include "game/legacy_world.h"

#include "doctest/doctest.h"

using legacy_world::Ship_Id;
using legacy_world::Star_Id;
using legacy_world::Player_Id;
using legacy_world::Planet_Id;

DOCTEST_TEST_CASE("AI workspaces own independent assessments and operation data") {
    AIDATA::s_state first;
    AIDATA::s_state second;
    AIDATA::Begin_Turn(&first, 35010);
    AIDATA::Begin_Turn(&second, 35020);

    first.ships[Ship_Id(1000000)].strengths[2] = 42;
    first.players[0].build.transports.additional_wanted = 3;
    first.players[0].refits.push_back({Ship_Id(7), 100});
    first.players[0].movement.flee_stars[Star_Id(900000)][AIDATA::LAST_FORECAST_TURN] = 1;
    first.combat_ships.push_back(Ship_Id(7));

    DOCTEST_CHECK(second.ships.empty());
    DOCTEST_CHECK(second.players[0].build.transports.additional_wanted == 0);
    DOCTEST_CHECK(second.players[0].refits.empty());
    DOCTEST_CHECK(second.players[0].movement.flee_stars.empty());
    DOCTEST_CHECK(second.combat_ships.empty());
    DOCTEST_CHECK(first.ships.size() == 1);
    DOCTEST_CHECK(first.players[0].movement.flee_stars.size() == 1);

    AIDATA::Complete_Turn(&first);
    DOCTEST_CHECK(first.complete);
    DOCTEST_CHECK_FALSE(second.complete);
    DOCTEST_CHECK(first.ships.at(Ship_Id(1000000)).strengths[2] == 42);

    AIDATA::Begin_Turn(&first, 35030);
    DOCTEST_CHECK(first.initialized);
    DOCTEST_CHECK_FALSE(first.complete);
    DOCTEST_CHECK(first.ships.empty());
    DOCTEST_CHECK(first.players[0].refits.empty());
    DOCTEST_CHECK(first.players[0].movement.flee_stars.empty());
    DOCTEST_CHECK(first.stardate == 35030);
    DOCTEST_CHECK(second.stardate == 35020);

    AIDATA::Reset(&first);
    DOCTEST_CHECK_FALSE(first.initialized);
}

DOCTEST_TEST_CASE("AI forecast tables share the same turn horizon") {
    AIDATA::s_star_assessment star;
    AIDATA::s_movement_workspace movement;
    DOCTEST_CHECK(std::size(star.enroute_ships[0]) == AIDATA::FORECAST_TURN_COUNT);
    DOCTEST_CHECK(std::size(star.enroute_strengths[0][0]) == AIDATA::FORECAST_TURN_COUNT);
    DOCTEST_CHECK(std::size(star.possible_strengths[0][0]) == AIDATA::FORECAST_TURN_COUNT);
    DOCTEST_CHECK(std::size(star.enroute_human_ships) == AIDATA::FORECAST_TURN_COUNT);
    DOCTEST_CHECK(movement.flee_stars[Star_Id(0)].size() == AIDATA::FORECAST_TURN_COUNT);
    AIMOVE::s_interception_target interception{};
    DOCTEST_CHECK(std::size(interception.strengths) == AIDATA::FORECAST_TURN_COUNT);
}

DOCTEST_TEST_CASE("route cache keys preserve full object IDs and lookup does not insert") {
    AIDATA::s_route_cache routes;
    const uint32_t source = 0xf0000000u;
    const uint32_t destination = 0xe0000000u;
    DOCTEST_CHECK_FALSE(AIDATA::N_Turns_From_To(&routes, Star_Id(source), Star_Id(destination), Player_Id(3)));
    DOCTEST_CHECK_FALSE(AIDATA::Ship_N_Turns_To(&routes, Ship_Id(source), Star_Id(destination)));
    DOCTEST_CHECK(routes.direct.rows.empty());
    DOCTEST_CHECK(routes.ship_direct.rows.empty());

    routes.direct.Store(Star_Id(source), Star_Id(destination), Player_Id(3), 9);
    routes.direct.Store(Star_Id(destination), Star_Id(source), Player_Id(3), UINT8_MAX);
    routes.ship_direct.Store(Ship_Id(source), Star_Id(destination), Player_Id(0), 6);
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, Star_Id(source), Star_Id(destination), Player_Id(3)) == 9);
    DOCTEST_CHECK_FALSE(AIDATA::N_Turns_From_To(&routes, Star_Id(source), Star_Id(destination), Player_Id(2)));
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, Star_Id(destination), Star_Id(source), Player_Id(3)) == UINT8_MAX);
    DOCTEST_CHECK(AIDATA::Ship_N_Turns_To(&routes, Ship_Id(source), Star_Id(destination)) == 6);
    DOCTEST_CHECK_FALSE(AIDATA::Ship_N_Turns_To(&routes, Ship_Id(destination), Star_Id(source)));
    DOCTEST_CHECK(routes.direct.rows.size() == 2);
    DOCTEST_CHECK(routes.direct.destinations.size() == 2);
    DOCTEST_CHECK(routes.ship_direct.rows.size() == 1);
    DOCTEST_CHECK(routes.ship_direct.destinations.size() == 1);

    DOCTEST_CHECK(routes.intermediate.empty());
    routes.intermediate.emplace(AIDATA::s_route_key{Player_Id(3), Star_Id(source), Star_Id(destination)}, AIDATA::s_route_choices{});
    DOCTEST_REQUIRE(routes.intermediate.size() == 1);
    auto& intermediate = routes.intermediate.begin()->second;
    DOCTEST_CHECK_FALSE(intermediate.normal);
    DOCTEST_CHECK_FALSE(intermediate.extended);
    intermediate.normal = AIDATA::s_route{Star_Id(destination), 6};
    DOCTEST_CHECK(intermediate.normal->next_star == Star_Id(destination));

    routes.Clear();
    DOCTEST_CHECK(routes.direct.rows.empty());
    DOCTEST_CHECK(routes.intermediate.empty());
    DOCTEST_CHECK(routes.ship_direct.rows.empty());
}

DOCTEST_TEST_CASE("AI entity lookup does not alias deleted IDs or grow with ID high water") {
    AIDATA::s_state ai;
    ai.ships[Ship_Id(1)].refit_desirability = 50;
    ai.ships.erase(Ship_Id(1));
    ai.ships[Ship_Id(2000000000)].refit_desirability = 70;
    DOCTEST_CHECK(ai.ships.find(Ship_Id(1)) == ai.ships.end());
    DOCTEST_CHECK(ai.ships.size() == 1);
    DOCTEST_CHECK(ai.ships.at(Ship_Id(2000000000)).refit_desirability == 70);
    ai.combat_ships.push_back(Ship_Id(2000000000));
    ai.players[0].refits.push_back({Ship_Id(2000000000), 70});
    DOCTEST_CHECK(ai.combat_ships[0] == Ship_Id(2000000000));
    DOCTEST_CHECK(ai.players[0].refits[0].ship_id == Ship_Id(2000000000));

    DOCTEST_CHECK(ai.players[0].Find_Combat_Ships(Star_Id(2000000000)) == nullptr);
    DOCTEST_CHECK(ai.players[0].combat_ship_groups.empty());
    ai.players[0].combat_ship_groups.emplace(Star_Id(2000000000), AIDATA::s_combat_ship_group{4, 5});
    DOCTEST_REQUIRE(ai.players[0].Find_Combat_Ships(Star_Id(2000000000)));
    DOCTEST_CHECK(ai.players[0].Find_Combat_Ships(Star_Id(2000000000))->next == 4);
    DOCTEST_CHECK(ai.players[0].combat_ship_groups.size() == 1);
}

DOCTEST_TEST_CASE("dense route rows use compact offsets for sparse IDs") {
    AIDATA::s_route_cache routes;
    for (uint32_t source = 0; source < 32; ++source) {
        for (uint32_t destination = 0; destination < 32; ++destination) {
            routes.direct.Store(Star_Id(1000000000u + source * 1000),
                Star_Id(2000000000u + destination * 1000),
                Player_Id(7),
                (uint8_t)destination);
        }
    }
    DOCTEST_CHECK(routes.direct.rows.size() == 32);
    DOCTEST_CHECK(routes.direct.destinations.size() == 32);
    uint32_t cells = 0;
    for (const auto& row : routes.direct.rows) cells += (uint32_t)row.turns.size();
    DOCTEST_CHECK(cells == 32 * 32);
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, Star_Id(1000001000u), Star_Id(2000031000u), Player_Id(7)) == 31);
    DOCTEST_CHECK_FALSE(AIDATA::N_Turns_From_To(&routes, Star_Id(1000001000u), Star_Id(2000031000u), Player_Id(6)));
}

DOCTEST_TEST_CASE("route availability does not reserve a duration or object ID") {
    AIDATA::s_route_cache routes;
    routes.direct.Store(Star_Id(0), Star_Id(UINT32_MAX), Player_Id(0), UINT8_MAX);
    routes.direct.Store(Star_Id(UINT32_MAX), Star_Id(0), Player_Id(0), 0);
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, Star_Id(0), Star_Id(UINT32_MAX), Player_Id(0)) == UINT8_MAX);
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, Star_Id(UINT32_MAX), Star_Id(0), Player_Id(0)) == 0);
    DOCTEST_CHECK_FALSE(AIDATA::N_Turns_From_To(&routes, Star_Id(0), Star_Id(0), Player_Id(0)));
}

DOCTEST_TEST_CASE("legacy boundary conversions handle absence and narrowing explicitly") {
    DOCTEST_CHECK(legacy_world::Nonnegative_Slot(0) == 0);
    DOCTEST_CHECK_FALSE(legacy_world::Nonnegative_Slot(-1));
    DOCTEST_CHECK(legacy_world::Index(Ship_Id(INT16_MAX)) == INT16_MAX);
    DOCTEST_CHECK_FALSE(legacy_world::Index(Ship_Id((uint32_t)INT16_MAX + 1)));
    DOCTEST_CHECK_FALSE(legacy_world::Index(Ship_Id(UINT32_MAX)));
    DOCTEST_CHECK(legacy_world::Travel_Turns(0) == 0);
    DOCTEST_CHECK(legacy_world::Travel_Turns(254) == 254);
    DOCTEST_CHECK_FALSE(legacy_world::Travel_Turns(-1));
    DOCTEST_CHECK_FALSE(legacy_world::Travel_Turns(UINT8_MAX));
    DOCTEST_CHECK(legacy_world::Travel_Turns(256) == 0);
    DOCTEST_CHECK_FALSE(legacy_world::Travel_Turns(511));
}

DOCTEST_TEST_CASE("optional attack targets preserve entity ID zero") {
    AIMOVE::s_attack_info info{};
    DOCTEST_CHECK_FALSE(info.target_planet);
    info.target_planet = Planet_Id(0);
    DOCTEST_REQUIRE(info.target_planet);
    DOCTEST_CHECK(*info.target_planet == Planet_Id(0));
}

DOCTEST_TEST_CASE("direct route row availability survives growth across bitmap words") {
    AIDATA::s_direct_route_table<game::t_star_id> routes;
    routes.Store(Star_Id(0), Star_Id(0), Player_Id(0), 1);
    for (uint32_t destination = 1; destination <= 128; ++destination) {
        routes.Store(Star_Id(1), Star_Id(destination), Player_Id(0), 2);
    }
    routes.Store(Star_Id(0), Star_Id(128), Player_Id(0), 3);
    DOCTEST_CHECK(routes.Find(Star_Id(0), Star_Id(0), Player_Id(0)) == 1);
    DOCTEST_CHECK_FALSE(routes.Find(Star_Id(0), Star_Id(64), Player_Id(0)));
    DOCTEST_CHECK(routes.Find(Star_Id(0), Star_Id(128), Player_Id(0)) == 3);
}

DOCTEST_TEST_CASE("resolved route rows retain availability and full-width sparse IDs") {
    AIDATA::s_direct_route_table<game::t_star_id> routes;
    routes.Store(Star_Id(UINT32_MAX), Star_Id(0), Player_Id(7), 0);
    routes.Store(Star_Id(UINT32_MAX), Star_Id(UINT32_MAX), Player_Id(7), UINT8_MAX);
    const auto row = routes.Find_Row_Index(Star_Id(UINT32_MAX), Player_Id(7));
    DOCTEST_REQUIRE(row);
    DOCTEST_CHECK(routes.Find_Cell(row, routes.Find_Destination(Star_Id(0))) == 0);
    DOCTEST_CHECK(routes.Find_Cell(row, routes.Find_Destination(Star_Id(UINT32_MAX))) == UINT8_MAX);
    DOCTEST_CHECK_FALSE(routes.Find_Cell(row, routes.Find_Destination(Star_Id(1234))));
    DOCTEST_CHECK_FALSE(routes.Find_Row_Index(Star_Id(UINT32_MAX), Player_Id(6)));

    for (uint32_t id = 1; id <= 128; ++id) routes.Store(Star_Id(id), Star_Id(id), Player_Id(7), 2);
    routes.Store(Star_Id(UINT32_MAX), Star_Id(128), Player_Id(7), 3);
    DOCTEST_CHECK(routes.Find_Cell(row, routes.Find_Destination(Star_Id(0))) == 0);
    DOCTEST_CHECK(routes.Find_Cell(row, routes.Find_Destination(Star_Id(UINT32_MAX))) == UINT8_MAX);
    DOCTEST_CHECK_FALSE(routes.Find_Cell(row, routes.Find_Destination(Star_Id(64))));
    DOCTEST_CHECK(routes.Find_Cell(row, routes.Find_Destination(Star_Id(128))) == 3);
}

DOCTEST_TEST_CASE("owned route indices stay bounded and follow growth, copying and clearing") {
    AIDATA::s_direct_route_table<game::t_star_id> routes;
    routes.Store(Star_Id(0), Star_Id(0), Player_Id(0), 0);
    routes.Store(Star_Id(UINT32_MAX), Star_Id(UINT32_MAX), Player_Id(7), UINT8_MAX);
    routes.Prepare_Live_Indices(3, 1);
    DOCTEST_CHECK(routes.live_destinations.size() == 3);
    DOCTEST_REQUIRE(routes.live_rows.size() == 1);
    DOCTEST_CHECK(routes.live_rows[0].size() == 3);
    DOCTEST_CHECK(routes.Find(Star_Id(0), Star_Id(0), Player_Id(0)) == 0);
    DOCTEST_CHECK(routes.Find(Star_Id(UINT32_MAX), Star_Id(UINT32_MAX), Player_Id(7)) == UINT8_MAX);
    DOCTEST_CHECK_FALSE(routes.Find_Row_Index(Star_Id(1), Player_Id(0)));
    DOCTEST_CHECK_FALSE(routes.Find_Destination(Star_Id(1)));
    DOCTEST_CHECK(routes.rows.size() == 2);
    DOCTEST_CHECK(routes.destinations.size() == 2);

    auto copy = routes;
    copy.Store(Star_Id(0), Star_Id(0), Player_Id(0), 9);
    DOCTEST_CHECK(routes.Find(Star_Id(0), Star_Id(0), Player_Id(0)) == 0);
    DOCTEST_CHECK(copy.Find(Star_Id(0), Star_Id(0), Player_Id(0)) == 9);
    routes.Store(Star_Id(0), Star_Id(1), Player_Id(0), 2);
    DOCTEST_CHECK(routes.Find(Star_Id(0), Star_Id(1), Player_Id(0)) == 2);
    DOCTEST_CHECK(copy.Find(Star_Id(0), Star_Id(0), Player_Id(0)) == 9);
    for (uint32_t id = 3; id < 128; ++id) routes.Store(Star_Id(id), Star_Id(id), Player_Id(0), 4);
    DOCTEST_CHECK(routes.Find(Star_Id(0), Star_Id(0), Player_Id(0)) == 0);
    DOCTEST_CHECK(routes.Find(Star_Id(0), Star_Id(1), Player_Id(0)) == 2);
    auto moved = std::move(copy);
    DOCTEST_CHECK(moved.Find(Star_Id(0), Star_Id(0), Player_Id(0)) == 9);
    DOCTEST_CHECK(moved.Find(Star_Id(UINT32_MAX), Star_Id(UINT32_MAX), Player_Id(7)) == UINT8_MAX);

    routes.Clear();
    routes.Store(Star_Id(2), Star_Id(2), Player_Id(0), 3);
    routes.Prepare_Live_Indices(3, 1);
    DOCTEST_CHECK_FALSE(routes.Find_Row_Index(Star_Id(0), Player_Id(0)));
    DOCTEST_CHECK_FALSE(routes.Find_Destination(Star_Id(0)));
    DOCTEST_CHECK(routes.Find(Star_Id(2), Star_Id(2), Player_Id(0)) == 3);
}
