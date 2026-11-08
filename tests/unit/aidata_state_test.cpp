#include "pch.h"

#include "game/ai_legacy.h"

#include "doctest/doctest.h"

DOCTEST_TEST_CASE("AI workspaces own independent assessments and operation data") {
    AIDATA::s_state first;
    AIDATA::s_state second;
    AIDATA::Begin_Turn(&first, 35010);
    AIDATA::Begin_Turn(&second, 35020);

    first.ships[1000000].strengths[2] = 42;
    first.players[0].build.transports.additional_wanted = 3;
    first.players[0].refits.push_back({7, 100});
    first.players[0].movement.flee_stars[900000][AIDATA::LAST_FORECAST_TURN] = 1;
    first.combat_ships.push_back(7);

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
    DOCTEST_CHECK(first.ships.at(1000000).strengths[2] == 42);

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
    DOCTEST_CHECK(movement.flee_stars[0].size() == AIDATA::FORECAST_TURN_COUNT);
    AIMOVE::s_interception_target interception{};
    DOCTEST_CHECK(std::size(interception.strengths) == AIDATA::FORECAST_TURN_COUNT);
}

DOCTEST_TEST_CASE("route cache keys preserve full object IDs and lookup does not insert") {
    AIDATA::s_route_cache routes;
    const uint32_t source = 0xf0000000u;
    const uint32_t destination = 0xe0000000u;
    DOCTEST_CHECK_FALSE(AIDATA::N_Turns_From_To(&routes, source, destination, 3));
    DOCTEST_CHECK_FALSE(AIDATA::Ship_N_Turns_To(&routes, source, destination));
    DOCTEST_CHECK(routes.direct.rows.empty());
    DOCTEST_CHECK(routes.ship_direct.rows.empty());

    routes.direct.Store(source, destination, 3, 9);
    routes.direct.Store(destination, source, 3, UINT8_MAX);
    routes.ship_direct.Store(source, destination, 0, 6);
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, source, destination, 3) == 9);
    DOCTEST_CHECK_FALSE(AIDATA::N_Turns_From_To(&routes, source, destination, 2));
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, destination, source, 3) == UINT8_MAX);
    DOCTEST_CHECK(AIDATA::Ship_N_Turns_To(&routes, source, destination) == 6);
    DOCTEST_CHECK_FALSE(AIDATA::Ship_N_Turns_To(&routes, destination, source));
    DOCTEST_CHECK(routes.direct.rows.size() == 2);
    DOCTEST_CHECK(routes.direct.destinations.size() == 2);
    DOCTEST_CHECK(routes.ship_direct.rows.size() == 1);
    DOCTEST_CHECK(routes.ship_direct.destinations.size() == 1);

    DOCTEST_CHECK(routes.intermediate.empty());
    routes.intermediate.emplace(AIDATA::s_route_key{3, source, destination}, AIDATA::s_route_choices{});
    DOCTEST_REQUIRE(routes.intermediate.size() == 1);
    auto& intermediate = routes.intermediate.begin()->second;
    DOCTEST_CHECK_FALSE(intermediate.normal);
    DOCTEST_CHECK_FALSE(intermediate.extended);
    intermediate.normal = AIDATA::s_route{destination, 6};
    DOCTEST_CHECK(intermediate.normal->next_star == destination);

    routes.Clear();
    DOCTEST_CHECK(routes.direct.rows.empty());
    DOCTEST_CHECK(routes.intermediate.empty());
    DOCTEST_CHECK(routes.ship_direct.rows.empty());
}

DOCTEST_TEST_CASE("AI entity lookup does not alias deleted IDs or grow with ID high water") {
    AIDATA::s_state ai;
    ai.ships[1].refit_desirability = 50;
    ai.ships.erase(1);
    ai.ships[2000000000].refit_desirability = 70;
    DOCTEST_CHECK(ai.ships.find(1) == ai.ships.end());
    DOCTEST_CHECK(ai.ships.size() == 1);
    DOCTEST_CHECK(ai.ships.at(2000000000).refit_desirability == 70);
    ai.combat_ships.push_back(2000000000);
    ai.players[0].refits.push_back({2000000000, 70});
    DOCTEST_CHECK(ai.combat_ships[0] == 2000000000);
    DOCTEST_CHECK(ai.players[0].refits[0].ship_id == 2000000000);

    DOCTEST_CHECK(ai.players[0].Find_Combat_Ships(2000000000) == nullptr);
    DOCTEST_CHECK(ai.players[0].combat_ship_groups.empty());
    ai.players[0].combat_ship_groups.emplace(2000000000, AIDATA::s_combat_ship_group{4, 5});
    DOCTEST_REQUIRE(ai.players[0].Find_Combat_Ships(2000000000));
    DOCTEST_CHECK(ai.players[0].Find_Combat_Ships(2000000000)->next == 4);
    DOCTEST_CHECK(ai.players[0].combat_ship_groups.size() == 1);
}

DOCTEST_TEST_CASE("dense route rows use compact offsets for sparse IDs") {
    AIDATA::s_route_cache routes;
    for (uint32_t source = 0; source < 32; ++source) {
        for (uint32_t destination = 0; destination < 32; ++destination) {
            routes.direct.Store(1000000000u + source * 1000,
                2000000000u + destination * 1000,
                7,
                (uint8_t)destination);
        }
    }
    DOCTEST_CHECK(routes.direct.rows.size() == 32);
    DOCTEST_CHECK(routes.direct.destinations.size() == 32);
    uint32_t cells = 0;
    for (const auto& [id, row] : routes.direct.rows) cells += (uint32_t)row.turns.size();
    DOCTEST_CHECK(cells == 32 * 32);
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, 1000001000u, 2000031000u, 7) == 31);
    DOCTEST_CHECK_FALSE(AIDATA::N_Turns_From_To(&routes, 1000001000u, 2000031000u, 6));
}

DOCTEST_TEST_CASE("route availability does not reserve a duration or object ID") {
    AIDATA::s_route_cache routes;
    routes.direct.Store(0, UINT32_MAX, 0, UINT8_MAX);
    routes.direct.Store(UINT32_MAX, 0, 0, 0);
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, 0, UINT32_MAX, 0) == UINT8_MAX);
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&routes, UINT32_MAX, 0, 0) == 0);
    DOCTEST_CHECK_FALSE(AIDATA::N_Turns_From_To(&routes, 0, 0, 0));
}

DOCTEST_TEST_CASE("legacy boundary conversions handle absence and narrowing explicitly") {
    DOCTEST_CHECK(AI_LEGACY::Object_Id(0) == 0);
    DOCTEST_CHECK_FALSE(AI_LEGACY::Object_Id(-1));
    DOCTEST_CHECK(AI_LEGACY::Index(INT16_MAX) == INT16_MAX);
    DOCTEST_CHECK_FALSE(AI_LEGACY::Index((uint32_t)INT16_MAX + 1));
    DOCTEST_CHECK_FALSE(AI_LEGACY::Index(UINT32_MAX));
    DOCTEST_CHECK(AI_LEGACY::Travel_Turns(0) == 0);
    DOCTEST_CHECK(AI_LEGACY::Travel_Turns(254) == 254);
    DOCTEST_CHECK_FALSE(AI_LEGACY::Travel_Turns(-1));
    DOCTEST_CHECK_FALSE(AI_LEGACY::Travel_Turns(UINT8_MAX));
    DOCTEST_CHECK(AI_LEGACY::Travel_Turns(256) == 0);
    DOCTEST_CHECK_FALSE(AI_LEGACY::Travel_Turns(511));
}

DOCTEST_TEST_CASE("optional attack targets preserve entity ID zero") {
    AIMOVE::s_attack_info info{};
    DOCTEST_CHECK_FALSE(info.target_planet);
    info.target_planet = 0;
    DOCTEST_REQUIRE(info.target_planet);
    DOCTEST_CHECK(*info.target_planet == 0);
}

DOCTEST_TEST_CASE("direct route row availability survives growth across bitmap words") {
    AIDATA::s_direct_route_table routes;
    routes.Store(0, 0, 0, 1);
    for (uint32_t destination = 1; destination <= 128; ++destination) {
        routes.Store(1, destination, 0, 2);
    }
    routes.Store(0, 128, 0, 3);
    DOCTEST_CHECK(routes.Find(0, 0, 0) == 1);
    DOCTEST_CHECK_FALSE(routes.Find(0, 64, 0));
    DOCTEST_CHECK(routes.Find(0, 128, 0) == 3);
}

DOCTEST_TEST_CASE("resolved route rows retain availability and full-width sparse IDs") {
    AIDATA::s_direct_route_table routes;
    routes.Store(UINT32_MAX, 0, 7, 0);
    routes.Store(UINT32_MAX, UINT32_MAX, 7, UINT8_MAX);
    const auto* row = routes.Find_Row(UINT32_MAX, 7);
    DOCTEST_REQUIRE(row != nullptr);
    DOCTEST_CHECK(row->Find(routes.Find_Destination(0)) == 0);
    DOCTEST_CHECK(row->Find(routes.Find_Destination(UINT32_MAX)) == UINT8_MAX);
    DOCTEST_CHECK_FALSE(row->Find(routes.Find_Destination(1234)));
    DOCTEST_CHECK(routes.Find_Row(UINT32_MAX, 6) == nullptr);

    for (uint32_t id = 1; id <= 128; ++id) routes.Store(id, id, 7, 2);
    routes.Store(UINT32_MAX, 128, 7, 3);
    DOCTEST_CHECK(row->Find(routes.Find_Destination(0)) == 0);
    DOCTEST_CHECK(row->Find(routes.Find_Destination(UINT32_MAX)) == UINT8_MAX);
    DOCTEST_CHECK_FALSE(row->Find(routes.Find_Destination(64)));
    DOCTEST_CHECK(row->Find(routes.Find_Destination(128)) == 3);
}

DOCTEST_TEST_CASE("prepared route lookups are bounded by live indices and detect mutation") {
    AIDATA::s_direct_route_table routes;
    routes.Store(0, 0, 0, 0);
    routes.Store(UINT32_MAX, UINT32_MAX, 7, UINT8_MAX);
    AIDATA::s_direct_route_lookup lookup;
    lookup.Prepare(&routes, 3, 1);
    DOCTEST_CHECK(lookup.Matches(&routes));
    DOCTEST_CHECK(lookup.destinations.size() == 3);
    DOCTEST_REQUIRE(lookup.rows.size() == 1);
    DOCTEST_CHECK(lookup.rows[0].size() == 3);
    DOCTEST_CHECK(lookup.Find_Row(0, 0)->Find(lookup.Find_Destination(0)) == 0);
    DOCTEST_CHECK(lookup.Find_Row(UINT32_MAX, 7)->Find(lookup.Find_Destination(UINT32_MAX)) == UINT8_MAX);
    DOCTEST_CHECK(lookup.Find_Row(1, 0) == nullptr);
    DOCTEST_CHECK_FALSE(lookup.Find_Destination(1));
    DOCTEST_CHECK(routes.rows.size() == 2);
    DOCTEST_CHECK(routes.destinations.size() == 2);

    auto copy = routes;
    DOCTEST_CHECK_FALSE(lookup.Matches(&copy));
    copy.Store(0, 0, 0, 9);
    DOCTEST_CHECK(routes.Find(0, 0, 0) == 0);
    DOCTEST_CHECK(copy.Find(0, 0, 0) == 9);
    routes.Store(0, 1, 0, 2);
    DOCTEST_CHECK_FALSE(lookup.Matches(&routes));
    lookup.Prepare(&routes, 3, 1);
    DOCTEST_CHECK(lookup.Find_Row(0, 0)->Find(lookup.Find_Destination(1)) == 2);

    routes.Clear();
    DOCTEST_CHECK_FALSE(lookup.Matches(&routes));
    routes.Store(2, 2, 0, 3);
    lookup.Prepare(&routes, 3, 1);
    DOCTEST_CHECK(lookup.Matches(&routes));
    DOCTEST_CHECK(lookup.Find_Row(0, 0) == nullptr);
    DOCTEST_CHECK_FALSE(lookup.Find_Destination(0));
    DOCTEST_CHECK(lookup.Find_Row(2, 0)->Find(lookup.Find_Destination(2)) == 3);
}
