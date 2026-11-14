#include "pch.h"

#include "doctest/doctest.h"
#include "game/legacy_world.h"

using legacy_world::Star_Id;
using legacy_world::Colony_Id;

DOCTEST_TEST_CASE("coverage counts do not wrap at the old byte boundary") {
    for (int16_t count : {0, 1, 255, 256, 257}) {
        AIDATA::s_state ai;
        s_player player{};
        player.ship_range = 6;
        s_settings settings{};
        settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
        std::vector<s_star_data> stars(52);
        for (auto& star : stars) star.wormhole_star_id = -1;
        std::vector<s_colony> colonies(257);
        std::vector<s_planet_data> planets(257);
        for (uint32_t i = 0; i < 257; ++i) {
            colonies[i].owner = 0;
            colonies[i].planet = (int16_t)i;
            planets[i].star_index = (int16_t)(i / 5);
        }
        int16_t players = 1;
        int16_t star_count = 52;
        const game::s_view world{
            .players = &player,
            .stars = stars.data(),
            .colonies = colonies.data(),
            .planets = planets.data(),
            .settings = &settings,
            .player_count = &players,
            .star_count = &star_count,
            .colony_count = &count,
        };
        AIDATA::Compute_Star_Player_Range_Info(&ai, &world);
        DOCTEST_CHECK(ai.stars.at(Star_Id(0)).neighborhood.per_player[0].covering_colony_count == (uint32_t)count);
        const int16_t worth = AIDATA::Proximity_Worth_To_Player(&ai, &world, &colonies[0], &player);
        DOCTEST_CHECK(worth == (count == 1 ? 52 * 80 : 0));
    }
}

DOCTEST_TEST_CASE("interception threat scans stay inside the forecast horizon") {
    std::array<int32_t, AIDATA::FORECAST_TURN_COUNT> first{};
    std::array<int32_t, AIDATA::FORECAST_TURN_COUNT> second{};
    DOCTEST_CHECK_FALSE(AIMOVE::First_Threat_Turn(&first, &second, 0));
    DOCTEST_CHECK_FALSE(AIMOVE::First_Threat_Turn(&first, &second, AIDATA::FORECAST_TURN_COUNT));
    second[AIDATA::LAST_FORECAST_TURN] = 1;
    DOCTEST_CHECK(AIMOVE::First_Threat_Turn(&first, &second, 0) == AIDATA::LAST_FORECAST_TURN);
    first[3] = 1;
    DOCTEST_CHECK(AIMOVE::First_Threat_Turn(&first, &second, 0) == 3);
    DOCTEST_CHECK(AIMOVE::First_Threat_Turn(&first, &second, 4) == AIDATA::LAST_FORECAST_TURN);
}

DOCTEST_TEST_CASE("typed ship locations round-trip legacy orbit and transit encodings") {
    using game::e_ship_location;
    for (auto kind : {e_ship_location::at_star, e_ship_location::in_flight, e_ship_location::in_wormhole}) {
        for (uint32_t star : {0u, 1u, (uint32_t)MAX_STARS}) {
            const game::s_ship_location location{kind, Star_Id(star)};
            auto encoded = legacy_world::Encode_Ship_Location(&location);
            DOCTEST_REQUIRE(encoded);
            auto decoded = legacy_world::Decode_Ship_Location(*encoded);
            DOCTEST_REQUIRE(decoded);
            DOCTEST_CHECK(decoded->kind == kind);
            DOCTEST_CHECK(decoded->star_id == Star_Id(star));
        }
    }
    for (int32_t invalid : {-1, MAX_STARS + 1, SHIP_LOCATION_STEP - 1, SHIP_LOCATION_LIMIT}) {
        DOCTEST_CHECK_FALSE(legacy_world::Decode_Ship_Location(invalid));
    }
    const game::s_ship_location high{e_ship_location::at_star, Star_Id(UINT32_MAX)};
    DOCTEST_CHECK_FALSE(legacy_world::Encode_Ship_Location(&high));
}

DOCTEST_TEST_CASE("location compatibility preserves transit kind without exposing offset arithmetic") {
    s_ship_data ship{};
    ship.status = 1;
    ship.location = SHIP_LOCATION_MOVING_OFFSET + 9;
    auto location = legacy_world::Read_Ship_Location(&ship);
    DOCTEST_REQUIRE(location);
    DOCTEST_CHECK(location->kind == game::e_ship_location::in_flight);
    DOCTEST_CHECK(location->star_id == Star_Id(9));
    DOCTEST_CHECK_FALSE(legacy_world::Orbit_Star(&ship));
    ship.status = 4;
    ship.location = 0;
    DOCTEST_CHECK(legacy_world::Orbit_Star(&ship) == Star_Id(0));
}

DOCTEST_TEST_CASE("a shared location tag does not imply a shared physical position") {
    s_ship_data first{};
    s_ship_data second{};
    first.location = second.location = SHIP_LOCATION_MOVING_OFFSET + 3;
    first.x = 10;
    second.x = 20;
    DOCTEST_CHECK(legacy_world::Same_Location_Tag(&first, &second));
    DOCTEST_CHECK_FALSE(legacy_world::Same_Ship_Position(&first, &second));
    second.x = 10;
    DOCTEST_CHECK(legacy_world::Same_Ship_Position(&first, &second));
}

DOCTEST_TEST_CASE("location codecs set and clear representation without changing activity") {
    s_ship_data ship{};
    ship.status = 4;
    const game::s_ship_location orbit{game::e_ship_location::at_star, Star_Id(0)};
    DOCTEST_REQUIRE(legacy_world::Set_Ship_Location(&ship, &orbit));
    DOCTEST_CHECK(legacy_world::Orbit_Star(&ship) == Star_Id(0));
    DOCTEST_CHECK((int8_t)ship.status == 4);
    const game::s_ship_location invalid{game::e_ship_location::at_star, Star_Id(UINT32_MAX)};
    DOCTEST_CHECK_FALSE(legacy_world::Set_Ship_Location(&ship, &invalid));
    DOCTEST_CHECK(legacy_world::Orbit_Star(&ship) == Star_Id(0));
    legacy_world::Clear_Ship_Location(&ship);
    DOCTEST_CHECK_FALSE(legacy_world::Read_Ship_Location(&ship));
    DOCTEST_CHECK((int8_t)ship.status == 4);
}

DOCTEST_TEST_CASE("movement presence and flee lookups do not insert zero assessments") {
    AIDATA::s_movement_workspace work;
    DOCTEST_CHECK_FALSE(AIDATA::Nonplayer_At(&work, Star_Id(0)));
    DOCTEST_CHECK_FALSE(AIDATA::Is_Fleeing(&work, Star_Id(UINT32_MAX), 0));
    DOCTEST_CHECK(work.nonplayer_here.empty());
    DOCTEST_CHECK(work.flee_stars.empty());
    work.nonplayer_here[Star_Id(UINT32_MAX)] = legacy_world::Player_Id(MAX_PLAYERS);
    work.flee_stars[Star_Id(0)][AIDATA::LAST_FORECAST_TURN] = 1;
    DOCTEST_CHECK(AIDATA::Nonplayer_At(&work, Star_Id(UINT32_MAX)) == legacy_world::Player_Id(MAX_PLAYERS));
    DOCTEST_CHECK(AIDATA::Is_Fleeing(&work, Star_Id(0), AIDATA::LAST_FORECAST_TURN));
    DOCTEST_CHECK_FALSE(AIDATA::Is_Fleeing(&work, Star_Id(0), 0));
    DOCTEST_CHECK(work.nonplayer_here.size() == 1);
    DOCTEST_CHECK(work.flee_stars.size() == 1);
}

DOCTEST_TEST_CASE("strategic AI input validation rejects missing session data") {
    game::s_view world;
    DOCTEST_CHECK(AIDATA::Validate_Inputs(nullptr) != nullptr);
    DOCTEST_CHECK(AIDATA::Validate_Inputs(&world) != nullptr);
}

DOCTEST_TEST_CASE("strategic AI input diagnostics identify the failing requirement") {
    s_player player{};
    s_ship_data ship{};
    s_star_data star{};
    s_colony colony{};
    s_planet_data planet{};
    s_leader_data leader{};
    s_settings settings{};
    int16_t players = 1;
    int16_t count = 0;
    int32_t date = 35000;
    game::s_view world{
        .players = &player,
        .ships = &ship,
        .stars = &star,
        .colonies = &colony,
        .planets = &planet,
        .leaders = &leader,
        .settings = &settings,
        .player_count = &players,
        .ship_count = &count,
        .star_count = &count,
        .colony_count = &count,
        .planet_count = &count,
        .stardate = &date,
    };
    DOCTEST_CHECK(AIDATA::Validate_Inputs(&world) == nullptr);
    world.ship_count = nullptr;
    DOCTEST_CHECK(std::string(AIDATA::Validate_Inputs(&world)) == "invalid ship count");
    world.ship_count = &count;
    DOCTEST_CHECK(AIDATA::Validate_Inputs(&world) == nullptr);
}

DOCTEST_TEST_CASE("production queue removal keeps colony identities and queue positions distinct") {
    s_colony colonies[3] = {};
    const game::s_view world{.colonies = colonies};
    game::id_colony queue[] = {Colony_Id(2), Colony_Id(0), Colony_Id(1)};
    int32_t count = 3;
    AIBUILD::Force_Produce(&world, queue, 1, &count, COLONY_PRODUCTION_HOUSING);
    DOCTEST_CHECK(count == 2);
    DOCTEST_CHECK(queue[0] == Colony_Id(2));
    DOCTEST_CHECK(queue[1] == Colony_Id(1));
    DOCTEST_CHECK((int16_t)colonies[0].producing[0] == COLONY_PRODUCTION_HOUSING);
}
