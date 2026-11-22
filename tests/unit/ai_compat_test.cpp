#include "pch.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"

#include "doctest/doctest.h"
#include "game/legacy_world.h"
#include "../fixtures/game_state.h"

using legacy_world::Star_Id;

DOCTEST_TEST_CASE("coverage counts do not wrap at the old byte boundary") {
    test::s_game_state game_state;
    test_stars::Resize(52);
    for (int16_t count : {0, 1, 255, 256, 257}) {
        AIDATA::s_state ai;
        s_player player{};
        player.ship_range = 6;
        s_settings settings{};
        settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
        auto& stars = game_state.stars;
        for (auto& star : stars) star.wormhole_star_id = game::NO_STAR;
        std::vector<s_colony> colonies(257);
        game::s_game_state roster;
        for (uint32_t i = 0; i < 257; ++i) {
            colonies[i].owner = 0;
            colonies[i].planet_id = SERIAL::Decode_Legacy_Planet_Reference((int16_t)i);
            roster.Create_Planet({ .star_id = game::id_star{static_cast<uint32_t>(static_cast<int16_t>(i / 5))}});
            if (i < static_cast<uint32_t>(count)) roster.Create_Colony(colonies[i]);
        }
        for (uint32_t i = 0; i < 52; ++i) test_stars::Set(i, stars[i], roster);
        int16_t players = 1;
        int16_t star_count = 52;
        const game::s_view world{
            .players = &player,
            .state = &roster,
            .settings = &settings,
            .player_count = &players,

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
    ship.position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 9, game::Current_Star_Count()));
    auto location = legacy_world::Read_Ship_Location(&ship);
    DOCTEST_REQUIRE(location);
    DOCTEST_CHECK(location->kind == game::e_ship_location::in_flight);
    DOCTEST_CHECK(location->star_id == Star_Id(9));
    DOCTEST_CHECK_FALSE(legacy_world::Orbit_Star(&ship));
    ship.status = 4;
    ship.position.Set({game::e_ship_location::at_star, Star_Id(0)});
    DOCTEST_CHECK(legacy_world::Orbit_Star(&ship) == Star_Id(0));
}

DOCTEST_TEST_CASE("a shared location tag does not imply a shared physical position") {
    s_ship_data first{};
    s_ship_data second{};
    first.position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 3, game::Current_Star_Count())); second.position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 3, game::Current_Star_Count()));
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
    game::s_game_state state;
    s_settings settings{};
    int16_t players = 1;
    int16_t count = 0;
    int32_t date = 35000;
    game::s_view world{
        .players = &player,
        .ships = &ship,
        .state = &state,
        .settings = &settings,
        .player_count = &players,
        .ship_count = &count,

        .stardate = &date,
    };
    DOCTEST_CHECK(AIDATA::Validate_Inputs(&world) == nullptr);
    world.ship_count = nullptr;
    DOCTEST_CHECK(std::string(AIDATA::Validate_Inputs(&world)) == "invalid ship count");
    world.ship_count = &count;
    DOCTEST_CHECK(AIDATA::Validate_Inputs(&world) == nullptr);
}

DOCTEST_TEST_CASE("production queue removal keeps colony identities and queue positions distinct") {
    game::s_game_state state;
    for (int i = 0; i < 3; ++i) state.Create_Colony({});
    const game::s_view world{.state = &state};
    game::id_colony queue[] = {game::id_colony{2}, game::id_colony{0}, game::id_colony{1}};
    int32_t count = 3;
    AIBUILD::Force_Produce(&world, queue, 1, &count, COLONY_PRODUCTION_HOUSING);
    DOCTEST_CHECK(count == 2);
    DOCTEST_CHECK(queue[0] == game::id_colony{2});
    DOCTEST_CHECK(queue[1] == game::id_colony{1});
    DOCTEST_CHECK(state.colonies.at(game::id_colony{0}).producing[0] == COLONY_PRODUCTION_HOUSING);
}
