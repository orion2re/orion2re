#include "pch.h"
#include "doctest/doctest.h"
#include "rpc/game_rpc.h"
#include "rpc/rpc_game.h"
#include "game/game_events.h"

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
        int16_t old_active_count = MOX::active_count;
        std::vector<uint8_t> old_active_players = {std::begin(MOX::active_player), std::end(MOX::active_player)};
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
            MOX::active_count = old_active_count;
            std::copy(old_active_players.begin(), old_active_players.end(), MOX::active_player);
        }
    };

    struct s_ship_world {
        s_world world;
        s_ship_data* old_ships = MOX::ship;
        void* old_pool = MOX::global_data_seg;
        int16_t old_count = MOX::NUM_SHIPS;
        int16_t old_stack_count = MOX::ship_stack_count;
        int16_t old_icon_count = MOX::ship_icon_count;
        int16_t old_free_node = MOX::next_free_node;
        SHIPSTAK::s_scan_bonus old_scan = SHIPSTAK::scan_bonus;
        SHIPSTAK::s_stealth_bonus old_stealth = SHIPSTAK::stealth_bonus;
        int16_t old_colonies = MOX::NUM_COLONIES;
        std::vector<s_ship_node> old_nodes = {std::begin(MOX::ship_node), std::end(MOX::ship_node)};
        std::vector<int16_t> old_heads = {std::begin(MOX::ship_stack_start), std::end(MOX::ship_stack_start)};
        std::vector<s_ship_icon> old_icons = {std::begin(MOX::ship_icon), std::end(MOX::ship_icon)};
        std::vector<s_ship_data> ships = std::vector<s_ship_data>(MAX_SHIPS);
        s_ship_world() {
            MOX::ship = ships.data();
            MOX::NUM_SHIPS = 3;
            MOX::NUM_COLONIES = 0;
            MOX::global_data_seg = allocate::Allocate_Space(1 << 20);
            SHIPSTAK::scan_bonus = {};
            SHIPSTAK::scan_bonus.no_scanner = 2;
            SHIPSTAK::stealth_bonus = {};
            SHIPSTAK::stealth_bonus.stealth_field = 10;
            ships[0].owner = 0;
            memcpy(ships[0].d.name, "Own\x82", 5);
            ships[1].owner = 1;
            ships[1].x = 30;
            ships[2].owner = 1;
            ships[2].x = 30000;
        }
        ~s_ship_world() {
            free((allocate::s_alloc_header*)MOX::global_data_seg - 1);
            MOX::global_data_seg = old_pool;
            MOX::ship = old_ships;
            MOX::NUM_SHIPS = old_count;
            MOX::NUM_COLONIES = old_colonies;
            MOX::ship_stack_count = old_stack_count;
            MOX::ship_icon_count = old_icon_count;
            MOX::next_free_node = old_free_node;
            SHIPSTAK::scan_bonus = old_scan;
            SHIPSTAK::stealth_bonus = old_stealth;
            std::copy(old_nodes.begin(), old_nodes.end(), MOX::ship_node);
            std::copy(old_heads.begin(), old_heads.end(), MOX::ship_stack_start);
            std::copy(old_icons.begin(), old_icons.end(), MOX::ship_icon);
        }
    };

    struct s_ship_listener {
        rpc::s_map_observation observed;
        bool valid = false;
        int calls = 0;
        uint64_t token = game_events::Subscribe([](game_events::e_event event, void* context) {
            if (event != game_events::SHIP_ICONS_REFRESHED) return;
            auto* listener = (s_ship_listener*)context;
            ++listener->calls;
            listener->valid = game_rpc::Read_Refreshed_Ship_Observation(&listener->observed);
        },
            this);
        ~s_ship_listener() {
            game_events::Unsubscribe(token);
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

DOCTEST_TEST_CASE("native ship refresh publishes only surviving displayed links without changing rules or RNG") {
    s_ship_world fixture;
    s_ship_listener listener;
    DOCTEST_REQUIRE(listener.token != 0);
    const uint32_t seed = game_random::Get_Random_Seed();
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(listener.calls == 1);
    DOCTEST_REQUIRE(listener.valid);
    DOCTEST_REQUIRE(listener.observed.ships_captured);
    DOCTEST_REQUIRE(listener.observed.ships.size() == 2);
    DOCTEST_CHECK(listener.observed.ships[0].owned);
    DOCTEST_CHECK(listener.observed.ships[0].name == "Own\xc3\xa9");
    DOCTEST_CHECK_FALSE(listener.observed.ships[1].owned);
    DOCTEST_CHECK(listener.observed.ships[1].name.empty());
    DOCTEST_CHECK(listener.observed.ships[1].x == 30);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &listener.observed));
    rpc::s_request list;
    list.method = "game.listShips";
    list.params_json = "{}";
    list.id_json = "1";
    const std::string baseline = rpc::Game_Query(&store, &list);

    // Neither an unlinked ship's fields nor a detected enemy's secret fields are copied.
    memset(fixture.ships[2].d.name, 'x', sizeof(fixture.ships[2].d.name));
    memset(fixture.ships[1].d.name, 'y', sizeof(fixture.ships[1].d.name));
    fixture.ships[1].turns_left = 99;
    fixture.ships[1].location = SHIP_LOCATION_MOVING_OFFSET + 1;
    MOX::NUM_SHIPS = 4;
    fixture.ships[3].status = 5;
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(listener.valid);
    DOCTEST_REQUIRE(listener.observed.ships.size() == 2);
    DOCTEST_CHECK(listener.observed.ships[1].name.empty());
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &listener.observed));
    DOCTEST_CHECK(rpc::Game_Query(&store, &list) == baseline);

    struct_::Set_Bit_Field(fixture.ships[1].d.special_device_flags, SPECIAL_STEALTH_FIELD);
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(listener.valid);
    DOCTEST_CHECK(listener.observed.ships.size() == 1);
    fixture.world.players[0].traits[TRAIT_OMNISCIENCE] = 1;
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(listener.valid);
    DOCTEST_CHECK(listener.observed.ships.size() == 3);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("ship projection fails closed on malformed fresh links and never retains native pointers") {
    s_ship_world fixture;
    SHIPS::Build_Ship_Icons();
    const std::vector<s_ship_node> original_nodes(std::begin(MOX::ship_node), std::end(MOX::ship_node));
    const std::vector<s_ship_icon> original_icons(std::begin(MOX::ship_icon), std::end(MOX::ship_icon));
    rpc::s_map_observation captured;
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&captured));
    DOCTEST_CHECK(memcmp(original_nodes.data(), MOX::ship_node, sizeof(MOX::ship_node)) == 0);
    DOCTEST_CHECK(memcmp(original_icons.data(), MOX::ship_icon, sizeof(MOX::ship_icon)) == 0);
    const int16_t head = MOX::ship_icon[0].node_idx;
    const s_ship_node original = MOX::ship_node[head];
    MOX::ship_node[head].next_node = head;
    rpc::s_map_observation invalid;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Refreshed_Ship_Observation(&invalid));
    DOCTEST_CHECK_FALSE(invalid.ships_captured);
    DOCTEST_CHECK(invalid.ships.empty());
    MOX::ship_node[head] = original;
    MOX::ship_node[head].ship_idx = MOX::NUM_SHIPS;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Refreshed_Ship_Observation(&invalid));
    MOX::ship_node[head] = original;
    MOX::ship_icon[0].node_idx = MAX_SHIPS - 1; // Not a surviving stack head.
    DOCTEST_CHECK_FALSE(game_rpc::Read_Refreshed_Ship_Observation(&invalid));
    memset(fixture.ships[0].d.name, 'z', sizeof(fixture.ships[0].d.name));
    DOCTEST_CHECK(captured.ships[0].name == "Own\xc3\xa9");
}

DOCTEST_TEST_CASE("maximum precomputed ship display copies bounded projected records and pages without game assets") {
    s_ship_world fixture;
    MOX::NUM_SHIPS = MAX_SHIPS;
    MOX::ship_stack_count = 1;
    MOX::ship_stack_start[0] = 0;
    MOX::ship_icon_count = 1;
    MOX::ship_icon[0].node_idx = 0;
    for (int index = 0; index < MAX_SHIPS; ++index) {
        MOX::ship_node[index] = {(int16_t)index, (int16_t)(index + 1 == MAX_SHIPS ? -1 : index + 1), 0};
        fixture.ships[index].owner = 0;
        fixture.ships[index].x = -32768;
        fixture.ships[index].y = 32767;
        memset(fixture.ships[index].d.name, 0xb0, sizeof(fixture.ships[index].d.name) - 1);
        fixture.ships[index].d.name[sizeof(fixture.ships[index].d.name) - 1] = 0;
    }
    const uint32_t seed = game_random::Get_Random_Seed();
    const auto started = std::chrono::steady_clock::now();
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_REQUIRE(observed.ships.size() == MAX_SHIPS);
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &observed));
    const auto captured = std::chrono::steady_clock::now();
    rpc::s_request request;
    request.method = "game.listShips";
    request.id_json = "1";
    request.params_json = "{\"limit\":256}";
    const std::string page = rpc::Game_Query(&store, &request);
    const auto encoded = std::chrono::steady_clock::now();
    DOCTEST_CHECK(page.size() < 1048576);
    DOCTEST_CHECK(page.find("\"error\"") == std::string::npos);
    request.method = "game.getState";
    request.params_json = "{\"include\":[\"ships\"]}";
    DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("-32005") != std::string::npos);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    std::cout << "ship-refresh-profile ships=" << observed.ships.size() << " page_bytes=" << page.size()
              << " capture_us=" << std::chrono::duration_cast<std::chrono::microseconds>(captured - started).count()
              << " page_encode_us=" << std::chrono::duration_cast<std::chrono::microseconds>(encoded - captured).count() << '\n';
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("player projection follows the native race roster while excluding hidden names and foreign economy") {
    s_world world;
    MOX::NUM_PLAYERS = MAX_PLAYERS;
    for (int index = 0; index < MAX_PLAYERS; ++index) {
        world.players[index].color = (uint8_t)index;
        memcpy(world.players[index].name, "Empire", 7);
    }
    memcpy(world.players[0].name, "Own\x82", 5);
    world.players[0].contact[1] = 1;
    world.players[2].eliminated = 1; // The native race screen includes eliminated empires without contact.
    world.players[0].n_times_established_contact[3] = 1;
    memset(world.players[4].name, 'x', sizeof(world.players[4].name)); // Must never be inspected without authorization.
    world.players[4].color = 255;
    world.players[0].bc = -123;
    world.players[0].surplus_bc = -5;
    world.players[0].surplus_food = -7;
    world.players[0].n_freighters = 10;
    world.players[0].surplus_freighters = -2;
    world.players[0].command_points = 10;
    world.players[0].command_points_used = 15;
    world.players[1].bc = 999999;
    MOX::active_count = 77;
    const uint32_t seed = game_random::Get_Random_Seed();
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(MOX::active_count == 77);
    DOCTEST_REQUIRE(observed.players_captured);
    DOCTEST_REQUIRE(observed.players.size() == 3);
    DOCTEST_CHECK(observed.players[0].local);
    DOCTEST_CHECK(observed.players[0].name == "Own\xc3\xa9");
    DOCTEST_CHECK(observed.players[0].treasury == -123);
    DOCTEST_CHECK(observed.players[0].hud.income == -5);
    DOCTEST_CHECK(observed.players[0].hud.food_surplus == -7);
    DOCTEST_CHECK(observed.players[0].hud.freighters == 10);
    DOCTEST_CHECK(observed.players[0].hud.freighters_available == -2);
    DOCTEST_CHECK(observed.players[0].hud.command_points_available == -5);
    DOCTEST_CHECK(observed.players[1].contacted);
    DOCTEST_CHECK(observed.players[1].treasury == 0);
    DOCTEST_CHECK(observed.players[2].eliminated);
    DOCTEST_CHECK_FALSE(observed.players[2].contacted);
    BILL::Get_Players_Dead_Or_Alive_Or_Omniscient(0, 0);
    DOCTEST_REQUIRE(MOX::active_count == 2);
    for (int index = 0; index < MOX::active_count; ++index) {
        DOCTEST_CHECK(std::any_of(observed.players.begin(), observed.players.end(), [index](const rpc::s_player_observation& player) {
            return player.slot == MOX::active_player[index];
        }));
    }
    world.players[1].bc = -999999;
    world.players[1].surplus_food = 30000;
    world.players[0].contact[2] = 1; // Historical dead-empire flags are not current contact.
    world.players[4].research_accumulated = 777;
    rpc::s_map_observation changed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&changed));
    DOCTEST_CHECK(changed.players.size() == observed.players.size());
    DOCTEST_CHECK(changed.players[1].treasury == 0);
    DOCTEST_CHECK(changed.players[1].hud.food_surplus == 0);
    DOCTEST_CHECK_FALSE(changed.players[2].contacted);
    world.players[0].command_points = 32767;
    world.players[0].command_points_used = -32768;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&changed));
    DOCTEST_CHECK(changed.players[0].hud.command_points_available == -1);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE("native omniscience and cached Galactic Lore disclose player names but never foreign HUD state") {
    s_world world;
    memcpy(world.players[1].name, "Other", 6);
    world.players[1].bc = 7654321;
    world.players[1].surplus_bc = 1234;
    rpc::s_map_observation observed;
    DOCTEST_SUBCASE("omniscience trait") {
        world.players[0].traits[TRAIT_OMNISCIENCE] = 1;
    }
    DOCTEST_SUBCASE("Galactic Lore") {
        world.players[0].has_galactic_lore = 1;
    }
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_REQUIRE(observed.players.size() == 2);
    DOCTEST_CHECK(observed.players[1].name == "Other");
    DOCTEST_CHECK_FALSE(observed.players[1].contacted);
    DOCTEST_CHECK(observed.players[1].treasury == 0);
    DOCTEST_CHECK(observed.players[1].hud.income == 0);
    world.players[1].name[0] = 0;
    DOCTEST_CHECK(observed.players[1].name == "Other");
}
