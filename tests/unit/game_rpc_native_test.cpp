#include "pch.h"
#include "doctest/doctest.h"
#include "rpc/game_rpc.h"
#include "rpc/rpc_game.h"

#include <chrono>
#include <iostream>
#include <vector>

namespace {
    struct s_world {
        s_player* old_players = MOX::player;
        s_star_data* old_stars = MOX::star;
        s_planet_data* old_planets = MOX::planet;
        s_colony* old_colonies = MOX::colony;
        s_nebula* old_nebulas = MOX::nebula;
        int16_t old_viewer = MOX::PLAYER_NUM;
        int16_t old_players_count = MOX::NUM_PLAYERS;
        int16_t old_stars_count = MOX::NUM_STARS;
        uint8_t old_nebulas_count = MOX::NUM_NEBULAS;
        int32_t old_turn = MOX::stardate;
        std::vector<s_player> players = std::vector<s_player>(MAX_PLAYERS);
        std::vector<s_star_data> stars = std::vector<s_star_data>(MAX_STARS);
        std::vector<s_planet_data> planets = std::vector<s_planet_data>(MAX_PLANETS);
        std::vector<s_colony> colonies = std::vector<s_colony>(MAX_COLONIES);
        std::vector<s_nebula> nebulas = std::vector<s_nebula>(MAX_NEBULAS);
        s_world() {
            MOX::player = players.data();
            MOX::star = stars.data();
            MOX::planet = planets.data();
            MOX::colony = colonies.data();
            MOX::nebula = nebulas.data();
            MOX::PLAYER_NUM = 0;
            MOX::NUM_PLAYERS = 2;
            MOX::NUM_STARS = 3;
            MOX::NUM_NEBULAS = 1;
            MOX::stardate = 35000;
            players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
            players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
            for (s_star_data& star : stars) {
                for (int orbit = 0; orbit < 5; ++orbit) star.planet_index[orbit] = -1;
                memcpy(star.name, "ABC\x82", 5);
            }
            stars[0].visited = 1;
            stars[1].visited = 2; // The other perspective only.
            stars[2].visited = 1;
            stars[2].spectral_class = STAR_CLASS_BLACK_HOLE;
        }
        ~s_world() {
            MOX::player = old_players;
            MOX::star = old_stars;
            MOX::planet = old_planets;
            MOX::colony = old_colonies;
            MOX::nebula = old_nebulas;
            MOX::PLAYER_NUM = old_viewer;
            MOX::NUM_PLAYERS = old_players_count;
            MOX::NUM_STARS = old_stars_count;
            MOX::NUM_NEBULAS = old_nebulas_count;
            MOX::stardate = old_turn;
        }
    };
} // namespace

DOCTEST_TEST_CASE("native map projection uses the local visited bit and native name formatting without RNG") {
    s_world world;
    world.stars[0].planet_index[0] = 0;
    world.planets[0].colony_index = 0;
    world.colonies[0].outpost_flag = 1;
    const uint32_t seed = game_random::Get_Random_Seed();
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_REQUIRE(observed.systems.size() == 3);
    DOCTEST_CHECK(observed.viewer == 0);
    DOCTEST_CHECK(observed.systems[0].name == "abc\xc3\xa9");
    DOCTEST_CHECK(observed.systems[0].name_available);
    DOCTEST_CHECK_FALSE(observed.systems[1].visited);
    DOCTEST_CHECK(observed.systems[1].name.empty());
    DOCTEST_CHECK_FALSE(observed.systems[2].name_available);
    memcpy(world.stars[1].name, "HIDDEN", 7);
    world.players[1].research_accumulated = 999;
    rpc::s_map_observation changed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&changed));
    DOCTEST_CHECK(changed.systems[1].name == observed.systems[1].name);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    world.players[0].objectives = 0;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&changed));
}

DOCTEST_TEST_CASE("maximum initial native projection fits the bridge response bound and profiles without game assets") {
    s_world world;
    MOX::NUM_STARS = MAX_STARS;
    MOX::NUM_NEBULAS = MAX_NEBULAS;
    for (s_star_data& star : world.stars) {
        star.visited = 1;
        star.x = -32768;
        star.y = -32768;
        memset(star.name, 0xb0, sizeof(star.name) - 1); // Three UTF-8 bytes per native CP850 glyph.
        star.name[sizeof(star.name) - 1] = 0;
    }
    for (s_nebula& nebula : world.nebulas) {
        nebula.x = -32768;
        nebula.y = -32768;
    }
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    rpc::Begin_Game_Reset(&store);
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    const auto started = std::chrono::steady_clock::now();
    rpc::s_map_observation observation;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observation));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &observation));
    const auto captured = std::chrono::steady_clock::now();
    rpc::s_request request;
    request.method = "game.getState";
    request.id_json = "1";
    request.params_json = "{\"include\":[\"session\",\"systems\",\"nebulas\"]}";
    const std::string response = rpc::Game_Query(&store, &request);
    const auto encoded = std::chrono::steady_clock::now();
    DOCTEST_CHECK(response.size() < 1048576);
    DOCTEST_CHECK(response.find("\"error\"") == std::string::npos);
    std::cout << "initial-map-profile systems=" << observation.systems.size() << " nebulas=" << observation.nebulas.size()
              << " bytes=" << response.size()
              << " capture_us=" << std::chrono::duration_cast<std::chrono::microseconds>(captured - started).count()
              << " encode_us=" << std::chrono::duration_cast<std::chrono::microseconds>(encoded - captured).count() << '\n';
    rpc::Shutdown_Game_Store(&store);
}
