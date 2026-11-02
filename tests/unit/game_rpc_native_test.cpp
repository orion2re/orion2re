#include "pch.h"
#include "doctest/doctest.h"
#include "rpc/game_rpc.h"
#include "rpc/rpc_game.h"
#include "rpc/tech_rpc.h"
#include "rpc/officer_rpc.h"
#include "rpc/relations_rpc.h"
#include "rpc/players_rpc.h"
#include "game/relations_read.h"
#include "game/game_events.h"
#include "game/history_read.h"
#include "rpc/turn_review_rpc.h"
#include "../support/rpc_native_turn_review.h"
#include "../support/rpc_native_colonies.h"
#include "game/colony_read.h"
#include "lua/lua_game_ship_identity.h"
#include "config/text_encoding.h"

#include <chrono>
#include <iostream>
#include <type_traits>
#include <vector>

namespace {
    struct s_review_globals {
        s_player* players = MOX::player;
        s_ship_data* ships = MOX::ship;
        s_leader_data* leaders = MOX::leaders;
        s_star_data* stars = MOX::star;
        s_planet_data* planets = MOX::planet;
        s_colony* colonies = MOX::colony;
        int16_t viewer = MOX::PLAYER_NUM;
        int16_t n_players = MOX::NUM_PLAYERS;
        int16_t n_ships = MOX::NUM_SHIPS;
        int16_t n_stars = MOX::NUM_STARS;
        int16_t n_planets = MOX::planet_count;
        int16_t n_colonies = MOX::NUM_COLONIES;
        int16_t selected_colony = COLONY::colony_handle;
        int32_t turn = MOX::stardate;
        bool smoothing = INFO::history_graph_smoothing;
        bool sorting = REPORT::sort_serious_turn_summary_first;
        decltype(MOX::settings) settings = MOX::settings;
        std::vector<char> h_strings = MOX::h_strings;
        std::vector<int16_t> bill = {std::begin(MOX::bill_savegame), std::end(MOX::bill_savegame)};
        std::vector<s_MSG> messages = {std::begin(MSG_::msgs), std::end(MSG_::msgs)};
        std::vector<uint8_t> first = {std::begin(MSG_::first_msg), std::end(MSG_::first_msg)};
        std::vector<uint8_t> counts = {std::begin(MSG_::n_msgs), std::end(MSG_::n_msgs)};
        std::vector<uint8_t> capacities = {std::begin(MSG_::max_msgs), std::end(MSG_::max_msgs)};
        char* message_storage = MSG_::native_msg;
        s_review_globals() {
            test_native_turn_review::Seed();
        }
        ~s_review_globals() {
            MOX::player = players;
            MOX::ship = ships;
            MOX::leaders = leaders;
            MOX::star = stars;
            MOX::planet = planets;
            MOX::colony = colonies;
            MOX::PLAYER_NUM = viewer;
            MOX::NUM_PLAYERS = n_players;
            MOX::NUM_SHIPS = n_ships;
            MOX::NUM_STARS = n_stars;
            MOX::planet_count = n_planets;
            MOX::NUM_COLONIES = n_colonies;
            COLONY::colony_handle = selected_colony;
            MOX::stardate = turn;
            MOX::settings = settings;
            MOX::h_strings = std::move(h_strings);
            INFO::history_graph_smoothing = smoothing;
            REPORT::sort_serious_turn_summary_first = sorting;
            std::copy(bill.begin(), bill.end(), MOX::bill_savegame);
            std::copy(messages.begin(), messages.end(), MSG_::msgs);
            std::copy(first.begin(), first.end(), MSG_::first_msg);
            std::copy(counts.begin(), counts.end(), MSG_::n_msgs);
            std::copy(capacities.begin(), capacities.end(), MSG_::max_msgs);
            MSG_::native_msg = message_storage;
        }
    };
} // namespace

DOCTEST_TEST_CASE("turn review shares all four native producers and graph precision") {
    s_review_globals fixture;
    history_read::s_chart chart;
    const int expected[] = {100, 150, 200};
    DOCTEST_REQUIRE(history_read::Read_Chart(0, 1, &chart));
    DOCTEST_CHECK(chart.count == 3);
    DOCTEST_CHECK(chart.curves[0].player == 0);
    DOCTEST_CHECK(chart.curves[1].player == 1);
    DOCTEST_CHECK(chart.curves[2].player == 3);
    DOCTEST_CHECK(chart.samples == 3);
    DOCTEST_CHECK(chart.top_scale == 50);
    for (int sample = 0; sample < 3; ++sample) DOCTEST_CHECK(chart.curves[0].height[sample] == expected[sample]);
    for (int series : {2, 4, 8}) {
        DOCTEST_REQUIRE(history_read::Read_Chart(0, (uint8_t)series, &chart));
        DOCTEST_CHECK(chart.top_scale == 50);
        DOCTEST_CHECK(chart.curves[0].height[0] == (series == 2 ? 150 : 200));
    }
    DOCTEST_REQUIRE(history_read::Read_Chart(0, 15, &chart));
    DOCTEST_CHECK(chart.top_scale == 250);
    DOCTEST_CHECK(chart.curves[0].height[0] == 130);
    DOCTEST_CHECK(chart.curves[0].height[2] == 150);
    MOX::player[2].population_history[0] = (int8_t)250;
    DOCTEST_REQUIRE(history_read::Read_Chart(0, 1, &chart));
    DOCTEST_CHECK(chart.top_scale == 50);
    MOX::player[0].contact[2] = 1;
    DOCTEST_REQUIRE(history_read::Read_Chart(0, 1, &chart));
    DOCTEST_CHECK(chart.count == 4);
    DOCTEST_CHECK(chart.top_scale == 500);
    MOX::player[0].contact[2] = 0;
    MOX::player[0].traits[TRAIT_OMNISCIENCE] = 1;
    DOCTEST_REQUIRE(history_read::Read_Chart(0, 1, &chart));
    DOCTEST_CHECK(chart.count == 4);
}

DOCTEST_TEST_CASE("turn review preserves native rescaling loss smoothing axes and ring windows") {
    s_review_globals fixture;
    MOX::player[0].total_pop = 503;
    BILL::Record_History();
    ++MOX::stardate;
    DOCTEST_CHECK(MOX::bill_savegame[2] == 3);
    DOCTEST_CHECK((uint8_t)MOX::player[0].population_history[0] == 6);
    DOCTEST_CHECK((uint8_t)MOX::player[0].population_history[3] == 167);
    history_read::s_chart chart;
    DOCTEST_REQUIRE(history_read::Read_Chart(0, 1, &chart));
    DOCTEST_CHECK(chart.top_scale == 1000);
    DOCTEST_CHECK(chart.curves[0].height[0] == 4); // Quantized 18, not original 20.
    int32_t smoothed[] = {18, 30, 39, 501};
    for (int pass = 0; pass < 10; ++pass) {
        for (int16_t sample = 0; sample < 4; ++sample) INFO::Smooth_History_Data(smoothed, sample, 4);
    }
    INFO::history_graph_smoothing = true;
    DOCTEST_REQUIRE(history_read::Read_Chart(0, 1, &chart));
    int32_t top = 25;
    const int scales[] = {25, 50, 100, 250, 500, 1000};
    for (int scale : scales)
        if (*std::max_element(std::begin(smoothed), std::end(smoothed)) < scale) {
            top = scale;
            break;
        }
    DOCTEST_CHECK(chart.top_scale == top);
    for (int sample = 0; sample < 4; ++sample) DOCTEST_CHECK(chart.curves[0].height[sample] == smoothed[sample] * 100 / (top * 100 / 250));
    history_read::s_axis axis;
    for (const auto& window : std::vector<std::pair<int, int>>{{35, 10}, {36, 5}, {70, 5}, {71, 2}, {175, 2}, {176, 1}, {351, 1}}) {
        MOX::stardate = 35000 + window.first;
        history_read::Read_Axis(&axis);
        DOCTEST_CHECK(axis.x_step == window.second);
    }
    MOX::stardate = 35035;
    MOX::settings.language = 0;
    history_read::Read_Axis(&axis);
    DOCTEST_CHECK(std::string(axis.labels[1]) == "3500.5");
    MOX::settings.language = 1;
    history_read::Read_Axis(&axis);
    DOCTEST_CHECK(std::string(axis.labels[1]) == "3500,5");
    MOX::stardate = 35351;
    MOX::bill_savegame[1] = 349;
    INFO::history_graph_smoothing = false;
    MOX::player[0].population_history[349] = 1;
    MOX::player[0].population_history[0] = 2;
    DOCTEST_REQUIRE(history_read::Read_Chart(0, 1, &chart));
    DOCTEST_CHECK(chart.samples == 350);
    DOCTEST_CHECK(chart.curves[0].height[0] == 300 / (chart.top_scale * 100 / 250));
    DOCTEST_CHECK(chart.curves[0].height[1] == 600 / (chart.top_scale * 100 / 250));
    MOX::bill_savegame[2] = 0;
    DOCTEST_CHECK_FALSE(history_read::Read_Chart(0, 1, &chart));
    MOX::bill_savegame[2] = -16384;
    MOX::player[0].population_history[349] = 1;
    rpc::s_map_observation observed;
    observed.viewer = 0;
    DOCTEST_REQUIRE(players_rpc::Read_Players(&observed));
    DOCTEST_REQUIRE(turn_review_rpc::Read_Turn_Review(&observed));
    DOCTEST_REQUIRE(observed.history_captured);
    DOCTEST_REQUIRE(history_read::Read_Chart(0, 1, &chart));
    const int16_t native_y = (int16_t)(390 - (int16_t)chart.curves[0].height[0]);
    DOCTEST_CHECK(native_y == -32378);
    DOCTEST_CHECK(observed.history.charts[0].curves[0].y[0] == native_y);
}

DOCTEST_TEST_CASE("turn review formats the full retained native family without consumption or scratch changes") {
    s_review_globals fixture;
    test_native_turn_review::Add_Variants();
    DOCTEST_REQUIRE(MSG_::n_msgs[0] == 33);
    rpc::s_map_observation observation;
    observation.viewer = 0;
    DOCTEST_REQUIRE(players_rpc::Read_Players(&observation));
    strlcpy(MOX::temp_string, "scratch sentinel", sizeof(MOX::temp_string));
    const auto messages = std::vector<s_MSG>(std::begin(MSG_::msgs), std::end(MSG_::msgs));
    const uint32_t seed = game_random::Get_Random_Seed();
    const auto displayed = REPORT::displayed_msgs;
    const auto selection = INFO::g_turn_message_ix;
    for (int pass = 0; pass < 2; ++pass) {
        DOCTEST_REQUIRE(turn_review_rpc::Read_Turn_Review(&observation));
        DOCTEST_REQUIRE(observation.messages_captured);
        DOCTEST_REQUIRE(observation.history_captured);
        DOCTEST_REQUIRE(observation.messages.size() == 33);
        for (int row = 0; row < 33; ++row) {
            char native[256];
            MSG_::sprintf_msg(native, sizeof(native), &MSG_::msgs[row]);
            DOCTEST_CHECK(observation.messages[row].text == native);
            DOCTEST_CHECK_FALSE(observation.messages[row].text.empty());
        }
    }
    DOCTEST_CHECK(std::string(MOX::temp_string) == "scratch sentinel");
    DOCTEST_CHECK(memcmp(messages.data(), MSG_::msgs, sizeof(MSG_::msgs)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    DOCTEST_CHECK(REPORT::displayed_msgs == displayed);
    DOCTEST_CHECK(INFO::g_turn_message_ix == selection);
    DOCTEST_CHECK(observation.messages[0].text == "Scrapped Native building at Home Prime for 0");
    DOCTEST_CHECK(observation.messages[31].text == "April notice");
    REPORT::sort_serious_turn_summary_first = true;
    DOCTEST_REQUIRE(turn_review_rpc::Read_Turn_Review(&observation));
    DOCTEST_CHECK(observation.messages.front().serious);
    DOCTEST_CHECK(memcmp(messages.data(), MSG_::msgs, sizeof(MSG_::msgs)) == 0);
    char text[256];
    DOCTEST_CHECK_FALSE(MSG_::Read_Message(1, 0, text, sizeof(text), nullptr));
    MOX::player[2].population_history[0] = (int8_t)250;
    DOCTEST_REQUIRE(turn_review_rpc::Read_Turn_Review(&observation));
    DOCTEST_CHECK(observation.history.charts[0].top_scale == 50);
    MSG_::Allocate_Msg_Slots();
    s_MSG unknown = {};
    unknown.msg_type = MSG_TYPE_UNKNOWN_0x3;
    MSG_::Add_Msg(unknown, 0);
    DOCTEST_REQUIRE(turn_review_rpc::Read_Turn_Review(&observation));
    DOCTEST_REQUIRE(observation.messages.size() == 1);
    DOCTEST_CHECK_FALSE(observation.messages[0].text_available);
    char* storage = MSG_::native_msg;
    MSG_::native_msg = nullptr;
    DOCTEST_REQUIRE(turn_review_rpc::Read_Turn_Review(&observation));
    DOCTEST_CHECK_FALSE(observation.messages_captured);
    MSG_::native_msg = storage;
    MSG_::Allocate_Msg_Slots();
    DOCTEST_REQUIRE(turn_review_rpc::Read_Turn_Review(&observation));
    DOCTEST_CHECK(observation.messages_captured);
    DOCTEST_CHECK(observation.messages.empty());
}

DOCTEST_TEST_CASE("turn review retains native formatting branches localization and truncation") {
    s_review_globals fixture;
    auto Check = [](s_MSG message, const char* expected) {
        MSG_::Allocate_Msg_Slots();
        MSG_::Add_Msg(message, 0);
        char text[256];
        DOCTEST_REQUIRE(MSG_::Read_Message(0, 0, text, sizeof(text), nullptr));
        DOCTEST_INFO("native text: ", text, "; expected substring: ", expected);
        DOCTEST_CHECK(std::string(text).find(expected) != std::string::npos);
    };
    s_MSG message = {};
    message.msg_type = MSG_TYPE_COLONY;
    message.u.colony.subtype = MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED;
    const std::pair<int, const char*> products[] = {
        {COLONY_PRODUCTION_FREIGHTERS, "freighters"}, {COLONY_PRODUCTION_SPY, "spy"}, {COLONY_PRODUCTION_SCIENTIST, "scientist"}, {COLONY_PRODUCTION_WORKER, "worker"}, {COLONY_PRODUCTION_FARMER, "farmer"}};
    message.u.colony.c.build.product_id = COLONY_PRODUCTION_NONE;
    for (const auto& product : products) {
        message.u.colony.c.build.build_id = (int16_t)product.first;
        Check(message, product.second);
    }
    message.u.colony.c.build.build_id = COLBLDG::Colony_Production_From_Ship_Index(0);
    Check(message, "built ship Native ship");
    message.u.colony.c.build.is_refit = 1;
    Check(message, "refitted Native ship");
    message.u.colony.c.build.product_id = BUILDING_MARINE_BARRACKS;
    Check(message, "; next Native building");
    message.u.colony.c.build.append_fmt = 1;
    Check(message, "; automatic Native building");
    message.u.colony.c.build.product_id = COLBLDG::Colony_Production_From_Ship_Index(0);
    MOX::ship[0].status = 4;
    COLBLDG::last_setting = 7;
    COLBLDG::last_app = 8;
    Check(message, "; automatic Refit Native ship");
    DOCTEST_CHECK(COLBLDG::last_setting == 7);
    DOCTEST_CHECK(COLBLDG::last_app == 8);
    strlcpy(MOX::player[0].ship_designs[0].name, "Own design", sizeof(MOX::player[0].ship_designs[0].name));
    COLONY::colony_handle = -1;
    message.u.colony.c.build.product_id = COLBLDG::Colony_Production_From_Ship_Design_Index(0);
    Check(message, "; automatic Own design");
    DOCTEST_CHECK(COLONY::colony_handle == -1);
    message = {};
    message.msg_type = MSG_TYPE_COLONY;
    message.u.colony.subtype = MSG_COLONY_SUBTYPE_STARVATION;
    message.u.colony.c.starvation.a = 2;
    Check(message, "lost 2 food");
    message.u.colony.c.starvation.b = 3;
    Check(message, "lost 2 food and 3 industry");
    message.u.colony.subtype = MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED;
    message.u.colony.c = {};
    message.u.colony.c.settlers.settlers = 2;
    message.u.colony.c.settlers.over_limit = 1;
    Check(message, "2 settlers at Home Prime; 1 excess settler");
    message.u.colony.c.settlers.over_limit = 2;
    Check(message, "2 excess settlers");
    message.u.colony.c.settlers.blockaded = 1;
    Check(message, "; blockaded");
    message.u.colony.subtype = MSG_COLONY_SUBTYPE_REBELLION;
    message.u.colony.c = {};
    for (int owner : {0, 1}) {
        for (int count : {1, 2}) {
            for (int lost : {0, count}) {
                message.u.colony.c.rebels.owner_player = (int8_t)owner;
                message.u.colony.c.rebels.rebels = (uint8_t)count;
                message.u.colony.c.rebels.lost = (uint8_t)lost;
                Check(message, lost == 0 ? "won" : "lost");
            }
        }
    }
    message = {};
    message.msg_type = MSG_TYPE_HYPERSPACE_BEAST;
    message.u.hyperspace_beast.ship_idx = -1;
    Check(message, "hungry");
    message.u.hyperspace_beast.ship_idx = 0;
    Check(message, "Race 0 ship");
    message.u.hyperspace_beast.fate = 1;
    Check(message, "Native ship at Home");
    for (int fate = 0; fate < 3; ++fate) {
        message.u.hyperspace_beast.leader_status = (int8_t)fate;
        Check(message, "Native leader");
    }
    message = {};
    message.msg_type = MSG_TYPE_LOST_DEFENDING_SPY;
    message.u.lost_defending_spy.count = 1;
    Check(message, "Lost defending spy");
    message.u.lost_defending_spy.count = 2;
    Check(message, "Lost 2 defending spies");
    message = {};
    message.msg_type = MSG_TYPE_SCRAP_SHIP;
    message.u.scrap_ship.where = MOX::NUM_STARS;
    Check(message, "in transit");
    message = {};
    message.msg_type = MSG_TYPE_LOST_COLONY;
    message.u.lost_colony.leader_idx = -1;
    for (int reason = 0; reason < 4; ++reason) {
        message.u.lost_colony.reason = (uint8_t)reason;
        Check(message, MSG_::lost_colony_reason_strings[reason]);
    }
    message = {};
    message.msg_type = MSG_TYPE_DIPLOMACY;
    for (int variant = 0; variant < 5; ++variant) {
        message.u.diplomacy.diplomacy_idx = (int8_t)variant;
        Check(message, "Race 0");
    }
    message = {};
    message.msg_type = MSG_TYPE_SPY_ASSASSINATION;
    message.u.spy_assasination.victim_id = 1;
    Check(message, "Assassinated defensive spy");
    message.u.spy_assasination.victim_id = 0;
    message.u.spy_assasination.plr_a = 1;
    Check(message, "Leader Native leader lost to Race 1");
    MOX::settings.language = 4;
    MSG_::msg_spy_assassinated_victim_fmt = (char*)"Leader %s, %s, lost to %s";
    Check(message, "spy against Race 0, lost to Race 1");
    static std::string long_text(400, 'x');
    MSG_::msg_april1_fmt = long_text.data();
    message.msg_type = MSG_TYPE_APRIL1;
    MSG_::Allocate_Msg_Slots();
    MSG_::Add_Msg(message, 0);
    char text[256];
    DOCTEST_REQUIRE(MSG_::Read_Message(0, 0, text, sizeof(text), nullptr));
    DOCTEST_CHECK(strlen(text) == 255);
}

DOCTEST_TEST_CASE("turn review queue lifetime publication precedes accepted writes and ignores rejected inserts") {
    s_review_globals fixture;
    MSG_::max_msgs[0] = 1;
    struct s_observer {
        int calls = 0;
        int count = -1;
        int recipient = -1;
    } observer;
    const auto token = game_events::Subscribe([](game_events::e_event event, void* context) {
        if (event != game_events::MESSAGES_REPLACING) return;
        auto* value = (s_observer*)context;
        ++value->calls;
        value->recipient = game_events::Message_Recipient();
        value->count = MSG_::n_msgs[value->recipient];
    },
        &observer);
    s_MSG message = {};
    message.msg_type = MSG_TYPE_APRIL1;
    MSG_::Add_Msg(message, 0);
    DOCTEST_CHECK(observer.calls == 1);
    DOCTEST_CHECK(observer.count == 0);
    DOCTEST_CHECK(observer.recipient == 0);
    MSG_::Add_Msg(message, 0);
    DOCTEST_CHECK(observer.calls == 1);
    message.priority = 1;
    MSG_::Add_Msg(message, 0);
    DOCTEST_CHECK(observer.calls == 2);
    DOCTEST_CHECK(observer.count == 1);
    MSG_::Add_Msg(message, 2);
    DOCTEST_CHECK(observer.recipient == 2);
    game_events::Unsubscribe(token);
    DOCTEST_CHECK(game_events::Message_Recipient() == -1);
}

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
        int16_t old_colonies_count = MOX::NUM_COLONIES;
        int16_t old_planet_count = MOX::planet_count;
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
            MOX::NUM_COLONIES = 0;
            MOX::planet_count = MAX_PLANETS;
            MOX::NUM_NEBULAS = 1;
            MOX::stardate = 35000;
            players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
            players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
            for (s_star_data& star : stars) {
                for (int orbit = 0; orbit < 5; ++orbit) star.planet_index[orbit] = -1;
                star.wormhole_star_id = -1;
                memcpy(star.name, "ABC\x82", 5);
            }
            for (auto& planet : planets) {
                planet.colony_index = -1;
                planet.planet_type = PLANET_TYPE_PLANET;
                planet.climate = PLANET_CLIMATE_TERRAN;
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
            MOX::NUM_COLONIES = old_colonies_count;
            MOX::planet_count = old_planet_count;
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
        uint8_t old_enemy_moves = MOX::settings.enemy_moves;
        std::vector<s_ship_node> old_nodes = {std::begin(MOX::ship_node), std::end(MOX::ship_node)};
        std::vector<int16_t> old_heads = {std::begin(MOX::ship_stack_start), std::end(MOX::ship_stack_start)};
        std::vector<s_ship_icon> old_icons = {std::begin(MOX::ship_icon), std::end(MOX::ship_icon)};
        std::vector<s_ship_data> ships = std::vector<s_ship_data>(MAX_SHIPS);
        s_ship_world() {
            MOX::ship = ships.data();
            MOX::NUM_SHIPS = 3;
            MOX::NUM_COLONIES = 0;
            MOX::settings.enemy_moves = 0;
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
            MOX::settings.enemy_moves = old_enemy_moves;
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

namespace {
    struct s_research_world {
        s_ship_world ships;
        std::vector<s_tech_field_data> old_fields = {std::begin(TECHDATA::technology_fields), std::end(TECHDATA::technology_fields)};
        std::vector<s_tech_app_data> old_applications = {std::begin(TECHDATA::technology_applications), std::end(TECHDATA::technology_applications)};
        std::vector<int16_t> old_roots = {std::begin(MOX::first_field_in_group), std::end(MOX::first_field_in_group)};
        char* old_title = MOX::hyper_field_title;
        char title[16] = "Hyper research";
        char name[32] = "Synthetic tech\x82";
        s_research_world() {
            std::fill(std::begin(MOX::first_field_in_group), std::end(MOX::first_field_in_group), 0);
            for (int16_t id = 0; id < TECH_FIELD_COUNT; ++id) {
                TECHDATA::technology_fields[id] = {};
                TECHDATA::technology_fields[id].tech_field_id = id;
                TECHDATA::technology_fields[id].name = name;
            }
            for (int16_t id = 0; id < TECH_APP_COUNT; ++id) {
                TECHDATA::technology_applications[id] = {};
                TECHDATA::technology_applications[id].tech_app_id = id;
                TECHDATA::technology_applications[id].name = name;
            }
            MOX::hyper_field_title = title;
            MOX::first_field_in_group[4] = 1;
            MOX::first_field_in_group[2] = 2;
            MOX::first_field_in_group[6] = TECH_FIELD_BIOLOGY;
            auto& player = ships.world.players[0];
            player.current_research_field = 1;
            player.current_research_application = 1;
            player.research_accumulated = 125;
            const int16_t available_fields[] = {1, 2, TECH_FIELD_BIOLOGY};
            for (int16_t field : available_fields) player.tech_fields[field] = 2;
            TECHDATA::technology_fields[1].cost = 600;
            TECHDATA::technology_fields[2].cost = 900;
            TECHDATA::technology_fields[TECH_FIELD_BIOLOGY].cost = 1000;
            const int16_t applications[] = {1, 2, 3, 203};
            std::copy(std::begin(applications), std::end(applications), TECHDATA::technology_fields[1].tech);
            for (int16_t app : applications) {
                player.tech_applications[app] = TECH_RESEARCH_STATUS_AVAILABLE;
                TECHDATA::technology_applications[app].tech_field_id = 1;
            }
            TECHDATA::technology_applications[TECH_APP_BIOLOGY].tech_field_id = TECH_FIELD_BIOLOGY;
            player.hyper_advanced_tech[0] = 20;
        }
        ~s_research_world() {
            std::copy(old_fields.begin(), old_fields.end(), TECHDATA::technology_fields);
            std::copy(old_applications.begin(), old_applications.end(), TECHDATA::technology_applications);
            std::copy(old_roots.begin(), old_roots.end(), MOX::first_field_in_group);
            MOX::hyper_field_title = old_title;
        }
    };
} // namespace

namespace {
    struct s_leader_world {
        s_research_world world;
        s_leader_data* old_leaders = MOX::leaders;
        std::vector<s_leader_data> leaders = std::vector<s_leader_data>(MAX_LEADERS);
        std::vector<s_skill_data> skills = {std::begin(MOX::skill_data), std::end(MOX::skill_data)};
        std::vector<char*> old_ship_ranks = {std::begin(MOX::officer_level_names), std::end(MOX::officer_level_names)};
        std::vector<char*> old_star_ranks = {std::begin(MOX::star_officer_level_names), std::end(MOX::star_officer_level_names)};
        std::vector<char*> old_strings = {std::begin(ESTRINGS::strings), std::end(ESTRINGS::strings)};
        std::vector<char*> old_governments = {std::begin(MOX::government_labels), std::end(MOX::government_labels)};
        OFFICER::s_leader_progression old_progression = OFFICER::progression;
        uint8_t old_warlord = OFFICER::leader_warlord_bonus;
        uint8_t old_no_limit = OFFICER::leaders_no_limit;
        char name[32] = "Native ability\x82";
        char ship_rank[32] = "Ship rank";
        char star_rank[32] = "Star rank";
        char format[3] = "%s";
        char empty[1] = "";
        char prefix[8] = "Tech: ";
        s_leader_world() {
            MOX::leaders = leaders.data();
            for (auto& ship : world.ships.ships) ship.officer_index = -1;
            for (auto& leader : leaders) {
                leader.player_index = -1;
                leader.status = -1;
                leader.location = -1;
                strlcpy(leader.name, "Leader\x82", sizeof(leader.name));
            }
            for (auto& skill : MOX::skill_data) skill.name = name;
            for (auto& rank : MOX::officer_level_names) rank = ship_rank;
            for (auto& rank : MOX::star_officer_level_names) rank = star_rank;
            const uint16_t thresholds[] = {60, 150, 300, 500, 1000};
            std::copy(std::begin(thresholds), std::end(thresholds), OFFICER::progression.experience_thresholds);
            OFFICER::leader_warlord_bonus = 1;
            OFFICER::progression.special_leaders.advanced_government_leader_id = 2;
            ESTRINGS::strings[7] = ESTRINGS::strings[207] = format;
            ESTRINGS::strings[12] = empty;
            ESTRINGS::strings[208] = prefix;
            for (auto& government : MOX::government_labels) government = star_rank;
            world.ships.world.players[0].officer_for_hire = -1;
            world.ships.world.players[1].officer_for_hire = -1;
        }
        ~s_leader_world() {
            MOX::leaders = old_leaders;
            std::copy(skills.begin(), skills.end(), MOX::skill_data);
            std::copy(old_ship_ranks.begin(), old_ship_ranks.end(), MOX::officer_level_names);
            std::copy(old_star_ranks.begin(), old_star_ranks.end(), MOX::star_officer_level_names);
            std::copy(old_strings.begin(), old_strings.end(), ESTRINGS::strings);
            std::copy(old_governments.begin(), old_governments.end(), MOX::government_labels);
            OFFICER::progression = old_progression;
            OFFICER::leader_warlord_bonus = old_warlord;
            OFFICER::leaders_no_limit = old_no_limit;
        }
        void Own(int16_t id, uint8_t type, int8_t status) {
            leaders[id].player_index = 0;
            leaders[id].type = type;
            leaders[id].status = status;
            leaders[id].xp = 150;
            leaders[id].skill_value = 5;
        }
    };
} // namespace

DOCTEST_TEST_CASE("leader projection uses native disclosure skill formatting and role semantics without UI or RNG") {
    s_leader_world fixture;
    fixture.Own(0, LEADER_TYPE_COLONY, 0);
    fixture.Own(1, LEADER_TYPE_SHIP, 1);
    fixture.Own(2, LEADER_TYPE_SHIP, -1);
    fixture.Own(3, LEADER_TYPE_SHIP, 4);
    fixture.Own(4, LEADER_TYPE_SHIP, 2);
    fixture.Own(5, LEADER_TYPE_COLONY, 3);
    fixture.Own(MAX_LEADERS - 1, LEADER_TYPE_SHIP, 0);
    fixture.leaders[0].special_skills = 1;
    fixture.leaders[0].general_skills = 0x100; // Fixed native displayed Megawealth value.
    fixture.leaders[1].special_skills = 1;
    fixture.leaders[1].general_skills = 1;
    fixture.leaders[1].location = 0;
    fixture.leaders[1].eta = 3;
    fixture.leaders[3].eta = 29;
    fixture.leaders[2].tech_application[0] = 1;
    fixture.leaders[4].location = 30000; // Limbo is known-empty, not a live assignment.
    fixture.leaders[4].eta = 120;
    fixture.leaders[5].location = 30000;
    fixture.world.ships.world.players[0].officer_for_hire = 2;
    fixture.world.ships.world.players[0].traits[TRAIT_WARLORD] = 1;
    const uint32_t seed = game_random::Get_Random_Seed();
    const auto screen = MOX::current_screen;
    const int16_t fields_before = fields::Get_Field_Counter();
    rpc::s_map_observation map;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_REQUIRE(map.leaders_captured);
    DOCTEST_REQUIRE(map.leaders.size() == 7);
    DOCTEST_CHECK(map.leaders[0].role == "colony-leader");
    DOCTEST_CHECK(map.leaders[0].rank == "Star rank");
    DOCTEST_CHECK(map.leaders[0].skills[1].value == "+10BC");
    DOCTEST_CHECK(map.leaders[0].skills[0].value == "-40%");
    DOCTEST_CHECK(OFFICER::Owned_Officer_Level(0) == OFFICER::Get_Officer_Level(0) + 1);
    char benefits[500];
    MAINPUPS::Leader_Technology_String(0, 2, benefits, sizeof(benefits));
    std::string decoded_benefits;
    config::Decode_Cp850_To_Utf8(benefits, strlen(benefits), &decoded_benefits);
    DOCTEST_CHECK(map.leaders[2].benefits_available);
    DOCTEST_CHECK(map.leaders[2].benefits == decoded_benefits);
    for (const auto& leader : map.leaders) {
        DOCTEST_CHECK(OFFICER::Officer_Roster_Disclosure(0, (int16_t)leader.slot) != OFFICER::ROSTER_HIDDEN);
        size_t index = 0;
        for (bool general : {false, true}) {
            for (int16_t skill = 0; skill < 54; ++skill) {
                if (!OFFICER::Officer_Displays_Skill((int16_t)leader.slot, skill, general)) continue;
                char value[80];
                const int16_t bonus = (int16_t)OFFICER::Officer_Skill_Bonus(0, (int16_t)leader.slot, skill);
                DOCTEST_REQUIRE(OFFICER::Copy_Officer_Skill_Value(skill, bonus, value, sizeof(value)));
                DOCTEST_REQUIRE(index < leader.skills.size());
                DOCTEST_CHECK(leader.skills[index++].value == value);
            }
        }
        DOCTEST_CHECK(leader.hire_cost == (leader.hire_cost_available ? OFFICER::Calculate_Officer_Cost(0, (int16_t)leader.slot) : 0));
        DOCTEST_CHECK(leader.upkeep == OFFICER::Officer_Maintenance(0, (int16_t)leader.slot));
    }
    DOCTEST_CHECK(map.leaders[1].status == "travelling");
    DOCTEST_CHECK(map.leaders[1].assignment_kind == "ship");
    DOCTEST_CHECK(map.leaders[1].eta == 3);
    DOCTEST_CHECK(map.leaders[2].disclosure == "pending-offer");
    DOCTEST_CHECK_FALSE(map.leaders[2].xp_available);
    DOCTEST_CHECK(map.leaders[3].disclosure == "hiring-pool");
    DOCTEST_CHECK(map.leaders[3].offer_turns == 1);
    DOCTEST_CHECK(map.leaders[4].assignment_available == rpc::EXPLORATION_EMPTY);
    DOCTEST_CHECK(map.leaders[5].status == "unavailable");
    DOCTEST_CHECK(map.leaders[5].assignment_available == rpc::EXPLORATION_UNKNOWN);
    fixture.name[0] = 'X';
    DOCTEST_CHECK(map.leaders[0].skills[0].name.starts_with("Native"));
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    DOCTEST_CHECK(MOX::current_screen == screen);
    DOCTEST_CHECK(fields::Get_Field_Counter() == fields_before);
    DOCTEST_CHECK(fixture.world.ships.world.players[0].officer_for_hire == 2);
}

DOCTEST_TEST_CASE("shared leader result projections preserve native rescue and ground disclosure limits") {
    s_leader_world fixture;
    OFFICER::leaders_no_limit = 0;
    for (int16_t id = 10; id < 14; ++id) fixture.Own(id, LEADER_TYPE_SHIP, 0);
    fixture.leaders[6].status = 5;
    fixture.leaders[6].location = 0;
    fixture.leaders[6].special_skills = 1;
    int16_t rescued = -1;
    DOCTEST_CHECK_FALSE(OFFICER::Marooned_Hero_At_Star_Can_Join(0, &rescued, 0));
    DOCTEST_REQUIRE(rescued == 6);
    DOCTEST_CHECK(OFFICER::Officer_Roster_Disclosure(0, rescued) == OFFICER::ROSTER_HIDDEN);
    leader_read::s_leader rescue;
    DOCTEST_REQUIRE(leader_read::Read_Result(0, rescued, &rescue));
    DOCTEST_CHECK(rescue.skill_count == 1);
    fixture.leaders[13].player_index = -1;
    fixture.leaders[13].status = -1;
    fixture.leaders[6].status = 5;
    fixture.leaders[6].location = 0;
    DOCTEST_REQUIRE(OFFICER::Marooned_Hero_At_Star_Can_Join(0, &rescued, 0));
    DOCTEST_CHECK(OFFICER::Officer_Roster_Disclosure(0, rescued) == OFFICER::ROSTER_OWNED);
    OFFICER::Give_Officer_XP(0, rescued, 150);
    DOCTEST_CHECK(OFFICER::Officer_Made_Level_Report(0, 0) == 1);
    DOCTEST_REQUIRE(leader_read::Read_Result(0, rescued, &rescue));
    DOCTEST_CHECK(fixture.leaders[6].display_level_popup == 1);
    DOCTEST_CHECK(rescue.eta == -1);
    fixture.leaders[6].status = 1;
    fixture.leaders[6].eta = 3;
    DOCTEST_REQUIRE(leader_read::Read_Result(0, rescued, &rescue));
    DOCTEST_CHECK(rescue.eta == 3);
    fixture.Own(4, LEADER_TYPE_SHIP, 1);
    fixture.leaders[4].player_index = 1;
    fixture.leaders[4].general_skills = LEADER_GENERAL_SKILL_COMMANDO;
    MOX::ship[1].officer_index = 4;
    const int16_t ship = 1;
    s_invasion_side_info side{};
    side.player_id = 1;
    int16_t ships[] = {ship};
    invasion::Compute_Attacker_Officer_Bonus(ships, 1, &side.combat_modifiers);
    DOCTEST_REQUIRE(side.combat_modifiers.officer_idx == 4);
    leader_read::s_leader ground;
    DOCTEST_REQUIRE(leader_read::Read_Ground(0, &side, leader_read::GROUND_SUMMARY, &ground));
    char ranked[52];
    OFFICER::Officer_Name(ranked, sizeof(ranked), 1, 4);
    DOCTEST_CHECK(strcmp(ground.name, ranked) == 0);
    DOCTEST_CHECK(ground.bonus == side.combat_modifiers.officer_bonus);
    DOCTEST_CHECK(ground.role == -1);
    DOCTEST_CHECK(ground.rank[0] == '\0');
    DOCTEST_CHECK(ground.skill_count == 0);
    const auto seed = game_random::Get_Random_Seed();
    const auto screen = MOX::current_screen;
    const auto fields_before = fields::Get_Field_Counter();
    fixture.leaders[4].special_skills = UINT32_MAX;
    fixture.leaders[4].location = 30000;
    strlcpy(fixture.leaders[4].title, "Private", sizeof(fixture.leaders[4].title));
    leader_read::s_leader paired;
    DOCTEST_REQUIRE(leader_read::Read_Ground(0, &side, leader_read::GROUND_SUMMARY, &paired));
    DOCTEST_CHECK(strcmp(ground.name, paired.name) == 0);
    DOCTEST_CHECK(ground.bonus == paired.bonus);
    DOCTEST_CHECK(paired.title[0] == '\0');
    DOCTEST_CHECK(paired.skill_count == 0);
    DOCTEST_CHECK_FALSE(leader_read::Read_Ground(1, &side, leader_read::GROUND_SUMMARY, &paired));
    side.combat_modifiers.officer_idx = 0;
    DOCTEST_CHECK_FALSE(leader_read::Read_Ground(0, &side, leader_read::GROUND_SUMMARY, &paired));
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    DOCTEST_CHECK(MOX::current_screen == screen);
    DOCTEST_CHECK(fields::Get_Field_Counter() == fields_before);
}

DOCTEST_TEST_CASE("native leader owner leases publish copied result captures without splicing busy state") {
    s_leader_world fixture;
    fixture.Own(4, LEADER_TYPE_SHIP, 1);
    fixture.leaders[4].player_index = 1;
    fixture.leaders[4].general_skills = LEADER_GENERAL_SKILL_COMMANDO;
    MOX::ship[1].officer_index = 4;
    int16_t ships[] = {1};
    s_invasion_side_info side{};
    side.player_id = 1;
    invasion::Compute_Attacker_Officer_Bonus(ships, 1, &side.combat_modifiers);
    leader_read::s_leader disclosed;
    DOCTEST_REQUIRE(leader_read::Read_Ground(0, &side, leader_read::GROUND_SUMMARY, &disclosed));
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    rpc::s_map_observation ambient;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&ambient));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &ambient));
    struct s_observer {
        rpc::s_game_store* store;
        const rpc::s_map_observation* ambient;
        int count = 0;
    } observer{&store, &ambient};
    const auto subscription = game_events::Subscribe([](game_events::e_event event, void* context) {
        auto* observer = (s_observer*)context;
        if (event == game_events::LEADER_VIEW_CLEARED) rpc::Revoke_Game_Context(observer->store);
        if (event == game_events::LEADER_VIEW_READY) {
            ++observer->count;
            DOCTEST_REQUIRE(game_rpc::Capture_Leader_View(observer->store, observer->ambient, game_events::Published_Leader_View()));
        }
    },
        &observer);
    DOCTEST_REQUIRE(subscription != 0);
    rpc::s_request request;
    request.method = "game.listLeaders";
    request.id_json = "1";
    request.params_json = "{}";
    {
        game_events::s_leader_view_scope owner(leader_read::GROUND_SUMMARY);
        game_events::Publish_Leader_View(leader_read::GROUND_SUMMARY, &disclosed, 1);
        DOCTEST_CHECK(observer.count == 0);
        game_events::Ready_Leader_View(leader_read::GROUND_SUMMARY);
        game_events::Publish_Leader_View(leader_read::GROUND_SUMMARY, &disclosed, 1);
        DOCTEST_CHECK(observer.count == 1);
        const auto first = rpc::Game_Query(&store, &request);
        DOCTEST_CHECK(first.find("ground-summary") != std::string::npos);
        DOCTEST_CHECK(first.find("\"abilities\":{\"availability\":\"unknown\"}") != std::string::npos);
        const auto first_id = store.snapshots.back().leader_ids.front();
        {
            game_events::s_leader_interruption_scope child;
            game_events::Ready_Leader_View(leader_read::GROUND_SUMMARY);
            game_events::Publish_Leader_View(leader_read::GROUND_SUMMARY, &disclosed, 1);
            DOCTEST_CHECK(observer.count == 1);
            DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("ground-summary") == std::string::npos);
        }
        game_events::Publish_Leader_View(leader_read::GROUND_SUMMARY, &disclosed, 1);
        DOCTEST_CHECK(observer.count == 1);
        rpc::Pause_Game_Queries(&store);
        game_events::Ready_Leader_View(leader_read::GROUND_SUMMARY);
        game_events::Publish_Leader_View(leader_read::GROUND_SUMMARY, &disclosed, 1);
        DOCTEST_CHECK(observer.count == 2);
        DOCTEST_CHECK(store.snapshots.back().map.leaders_only);
        DOCTEST_CHECK(store.snapshots.back().leader_ids.front() != first_id);
        DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("ground-summary") != std::string::npos);
        request.method = "game.getState";
        request.params_json = "{\"include\":[\"leaders\",\"systems\"]}";
        DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("Busy") != std::string::npos);
        uint64_t epoch;
        DOCTEST_REQUIRE(rpc::Register_Client(&bridge, 1, &epoch));
        request.method = "game.listLeaders";
        request.params_json = "{}";
        DOCTEST_REQUIRE(rpc::Submit_Work(&bridge, 1, epoch, &request, UINT64_MAX) == 0);
        rpc::Pump_Game_Queries(&store);
        DOCTEST_REQUIRE(!bridge.ready.empty());
        DOCTEST_CHECK(bridge.ready.front().context_epoch != 0);
        DOCTEST_CHECK(bridge.ready.front().json.find("ground-summary") != std::string::npos);
        rpc::Resume_Game_Queries(&store);
        owner.Close();
        rpc::s_publication publication;
        DOCTEST_REQUIRE(rpc::Take_Publication(&bridge, &publication));
        DOCTEST_CHECK(publication.context_epoch == 0);
        DOCTEST_CHECK(publication.json == rpc::Error_Response(&request, rpc::RPC_OBJECT_UNAVAILABLE));
        DOCTEST_CHECK(publication.json.find("ground-summary") == std::string::npos);
    }
    game_events::Unsubscribe(subscription);
    DOCTEST_CHECK(game_events::Published_Leader_View() == nullptr);
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("native leader offers expire without disclosing candidate pools and support every modded slot") {
    s_leader_world fixture;
    rpc::s_map_observation map;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.leaders_captured);
    DOCTEST_CHECK(map.leaders.empty());
    for (int16_t id = 0; id < MAX_LEADERS; ++id) fixture.Own(id, LEADER_TYPE_SHIP, 4);
    OFFICER::leaders_no_limit = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.leaders.size() == MAX_LEADERS);
    fixture.leaders[0].eta = 29;
    OFFICER::Decrement_Officer_ETA();
    DOCTEST_CHECK(OFFICER::Officer_Roster_Disclosure(0, 0) == OFFICER::ROSTER_HIDDEN);
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.leaders.size() == MAX_LEADERS - 1);
    OFFICER::Kill_Officer(1);
    DOCTEST_CHECK(OFFICER::Officer_Roster_Disclosure(0, 1) == OFFICER::ROSTER_HIDDEN);
    OFFICER::Deassign_Officer(2);
    DOCTEST_CHECK(OFFICER::Officer_Roster_Disclosure(0, 2) == OFFICER::ROSTER_HIDDEN);
    OFFICER::Set_Officer_To_Player(2, 0, 0);
    DOCTEST_CHECK(OFFICER::Officer_Roster_Disclosure(0, 2) == OFFICER::ROSTER_HIRING_POOL);
    fixture.leaders[0].type = 255;
    memset(fixture.leaders[0].name, 'X', sizeof(fixture.leaders[0].name));
    fixture.leaders[0].special_skills = UINT32_MAX;
    fixture.leaders[0].location = 32000;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.leaders.size() == MAX_LEADERS - 2);
    MOX::leaders = nullptr;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK_FALSE(map.leaders_captured);
}

DOCTEST_TEST_CASE("leader projection retains every native displayed ability for either role") {
    s_leader_world fixture;
    fixture.Own(0, LEADER_TYPE_SHIP, 0);
    fixture.Own(1, LEADER_TYPE_COLONY, 0);
    for (int id = 0; id < 2; ++id) {
        fixture.leaders[id].general_skills = UINT32_MAX;
        fixture.leaders[id].special_skills = UINT32_MAX;
    }
    rpc::s_map_observation map;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    for (int id = 0; id < 2; ++id) {
        size_t count = 0;
        for (int16_t skill = 0; skill < 54; ++skill) {
            if (MOX::skill_data[skill].skill_type == 0 || MOX::skill_data[skill].skill_type == (id == 0 ? 1 : 2)) ++count;
        }
        DOCTEST_CHECK(map.leaders[(size_t)id].skills.size() == count);
    }
}

DOCTEST_TEST_CASE("native leader ship and system assignments resolve only within a coherent capture") {
    s_leader_world fixture;
    fixture.Own(0, LEADER_TYPE_COLONY, 0);
    fixture.Own(1, LEADER_TYPE_SHIP, 0);
    OFFICER::Assign_Leader_To_Star(&MOX::player[0], &fixture.leaders[0], &MOX::star[0], 0);
    OFFICER::Assign_Leader_To_Ship(&MOX::player[0], &fixture.leaders[1], &MOX::ship[0], 3);
    SHIPS::Build_Ship_Icons();
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    rpc::s_map_observation map;
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&map));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &map));
    for (const auto& leader : store.snapshots.back().map.leaders) {
        DOCTEST_CHECK_FALSE(leader.assignment_id.empty());
        DOCTEST_CHECK(leader.assignment_slot == -1);
    }
    rpc::s_request state;
    state.method = "game.getState";
    state.id_json = "1";
    state.params_json = "{\"include\":[\"session\",\"leaders\",\"systems\",\"players\",\"colonies\",\"ships\",\"research\"]}";
    const auto baseline = rpc::Game_Query(&store, &state);
    fixture.leaders[1].eta = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&store, &state) == baseline);
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&map));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&store, &state) != baseline);
    DOCTEST_CHECK(fixture.leaders[0].status == 1);
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("research projection shares native offered choices and costs with modded fields without UI or RNG") {
    s_research_world world;
    auto& player = world.ships.world.players[0];
    const uint32_t seed = game_random::Get_Random_Seed();
    const int16_t fields_before = fields::Get_Field_Counter();
    const auto old_screen = MOX::current_screen;
    const auto old_player = MOX::cur_plyr_ptr;
    rpc::s_map_observation map;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_REQUIRE(map.research_captured);
    DOCTEST_CHECK(map.research.mode == "researching");
    DOCTEST_REQUIRE(map.research.options.size() == 6);
    size_t index = 0;
    for (int16_t group : TECH::entry_to_group) {
        TECH::s_research_choice choice;
        DOCTEST_REQUIRE(TECH::Read_Research_Choice(&player, group, true, &choice));
        for (int app = 0; app < choice.count; ++app, ++index) {
            const auto& option = map.research.options[index];
            DOCTEST_CHECK(option.remaining == TECH::New_Get_Tech_Cost(choice.field, false, 0) - player.research_accumulated);
            DOCTEST_CHECK(map.research.technologies[(size_t)option.field].cost == COLCALC::Player_Research_Cost(&player, choice.field));
            if (choice.applications[app] == 0) {
                DOCTEST_CHECK(option.application == -1);
            } else {
                char name[128];
                TECH::Copy_Application_Name(choice.applications[app], 0, name, sizeof(name));
                std::string decoded;
                config::Decode_Cp850_To_Utf8(name, strlen(name), &decoded);
                DOCTEST_CHECK(map.research.technologies[(size_t)option.application].name == decoded);
            }
        }
    }
    TECH::s_research_choice excluded;
    DOCTEST_REQUIRE(TECH::Read_Research_Choice(&player, 4, false, &excluded));
    DOCTEST_CHECK(excluded.field == 0);
    DOCTEST_CHECK(player.current_research_field == 1);
    DOCTEST_CHECK(player.current_research_application == 1);
    DOCTEST_CHECK((int32_t)player.research_accumulated == 125);
    DOCTEST_CHECK(fields::Get_Field_Counter() == fields_before);
    DOCTEST_CHECK(MOX::current_screen == old_screen);
    DOCTEST_CHECK(MOX::cur_plyr_ptr == old_player);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    DOCTEST_CHECK(map.research.technologies.back().name.ends_with(" 21"));
    world.name[0] = 'X';
    DOCTEST_CHECK(map.research.technologies[0].name.starts_with("Synthetic"));
}

DOCTEST_TEST_CASE("native research modes preserve completion precedence no-selection hyper and known-empty choices") {
    s_research_world world;
    auto& player = world.ships.world.players[0];
    rpc::s_map_observation map;
    player.current_research_field = 0;
    player.current_research_application = -1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.research.mode == "no-selection");
    DOCTEST_CHECK(map.research.project == -1);
    DOCTEST_CHECK(map.research.application == -1);
    COLCALC::Resolve_Research_Completion(&player, true, 0);
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.research.mode == "completed");
    player.research_breakthrough = 0;
    player.current_research_field = TECH_FIELD_BIOLOGY;
    player.current_research_application = (int8_t)TECH_APP_BIOLOGY;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.research.mode == "hyper");
    player.current_research_field = 1;
    player.current_research_application = 1;
    player.tech_fields[1] = 3;
    DOCTEST_REQUIRE(COLCALC::Player_N_Turns_Until_Research_Complete(&player) == 0);
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.research.mode == "completed");
    player.research_breakthrough = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.research.mode == "completed");
    std::fill(std::begin(player.tech_fields), std::end(player.tech_fields), 0);
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.research.options.empty());
}

DOCTEST_TEST_CASE("native research filters hidden apps before copying and follows configured chains with bounded failure") {
    s_research_world world;
    auto& player = world.ships.world.players[0];
    player.tech_applications[203] = TECH_RESEARCH_STATUS_UNAVAILABLE;
    TECHDATA::technology_applications[203].name = nullptr;
    TECHDATA::technology_applications[203].tech_field_id = -32000;
    world.ships.world.players[1].current_research_field = -1;
    world.ships.world.players[1].research_accumulated = INT32_MIN;
    rpc::s_map_observation map;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.research.options.size() == 5);
    player.tech_fields[1] = 1;
    TECHDATA::technology_fields[1].next_field_id = 3;
    player.tech_fields[3] = 2;
    TECH::s_research_choice choice;
    DOCTEST_REQUIRE(TECH::Read_Research_Choice(&player, 4, true, &choice));
    DOCTEST_CHECK(choice.field == 3);
    DOCTEST_CHECK(choice.count == 1);
    DOCTEST_CHECK(choice.applications[0] == 0);
    player.research_accumulated = 1000000;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    for (const auto& option : map.research.options) DOCTEST_CHECK(option.remaining == 0);
    player.tech_fields[3] = 1;
    TECHDATA::technology_fields[3].next_field_id = 1;
    DOCTEST_CHECK_FALSE(TECH::Read_Research_Choice(&player, 4, true, &choice));
    TECHDATA::technology_fields[3].next_field_id = TECH_FIELD_COUNT;
    DOCTEST_CHECK_FALSE(TECH::Read_Research_Choice(&player, 4, true, &choice));
    DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&map));
}

DOCTEST_TEST_CASE("native research and ship map captures remain coherent and hidden-only mutations do not revise research") {
    s_research_world world;
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    SHIPS::Build_Ship_Icons();
    rpc::s_map_observation map;
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&map));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &map));
    rpc::s_request request;
    request.id_json = "1";
    request.method = "game.getState";
    request.params_json = "{\"include\":[\"session\",\"research\",\"ships\",\"systems\",\"players\"]}";
    const auto before = rpc::Game_Query(&store, &request);
    world.ships.world.players[1].research_accumulated = 99999;
    world.ships.world.players[1].tech_applications[3] = TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&map));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&store, &request) == before);
    world.ships.world.players[0].research_accumulated = 300;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&store, &request) == before);
    world.ships.ships[0].x = 111;
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&map));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &map));
    const auto after = rpc::Game_Query(&store, &request);
    DOCTEST_CHECK(after != before);
    DOCTEST_CHECK(after.find("\"remainingCost\":300") != std::string::npos);
    DOCTEST_CHECK(after.find("\"x\":111") != std::string::npos);
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("research supports every native option slot and the final hyper field and application") {
    s_research_world world;
    auto& player = world.ships.world.players[0];
    for (int16_t group = 1; group <= 8; ++group) {
        MOX::first_field_in_group[group] = group;
        player.tech_fields[group] = 2;
        for (int slot = 0; slot < 4; ++slot) {
            const int16_t app = (int16_t)((group - 1) * 4 + slot + 1);
            TECHDATA::technology_fields[group].tech[slot] = app;
            TECHDATA::technology_applications[app].tech_field_id = group;
            player.tech_applications[app] = TECH_RESEARCH_STATUS_AVAILABLE;
        }
    }
    rpc::s_map_observation map;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.research.options.size() == 32);
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &map));
    const int16_t field = TECH_FIELD_COUNT - 1;
    const int16_t app = TECH_APP_COUNT - 1;
    MOX::first_field_in_group[8] = field;
    player.tech_fields[field] = 2;
    player.current_research_field = (int8_t)field;
    player.current_research_application = (int8_t)app;
    TECHDATA::technology_applications[app].tech_field_id = field;
    player.hyper_advanced_tech[field - TECH_FIELD_BIOLOGY] = 255;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
    DOCTEST_CHECK(map.research.mode == "hyper");
    DOCTEST_CHECK(map.research.technologies[(size_t)map.research.application].name.ends_with(" 256"));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &map));
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("native map projection uses the local visited bit and native name formatting without RNG") {
    s_world world;
    world.stars[0].planet_index[0] = 0;
    world.planets[0].colony_index = 0;
    world.colonies[0].outpost_flag = 1;
    MOX::NUM_COLONIES = 1;
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

DOCTEST_TEST_CASE("exploration copies native planet knowledge and suitability without UI caches foreign intelligence or mutation") {
    s_world world;
    MOX::NUM_COLONIES = 1;
    world.stars[0].planet_index[0] = 0;
    world.stars[0].planet_index[2] = 1;
    world.stars[0].planet_index[4] = 2;
    world.stars[0].has_colony = 1;
    world.planets[0].planet_type = PLANET_TYPE_ASTEROID;
    world.planets[1].orbit = 2;
    world.planets[1].size = PLANET_SIZE_LARGE;
    world.planets[1].gravity_class = PLANET_GRAVITY_HEAVY_G;
    world.planets[1].mineral_class = MINERAL_RESOURCE_RICH;
    world.planets[1].planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
    world.planets[1].food_per_farmer = 5;
    world.planets[2].orbit = 4;
    world.planets[2].colony_index = 0;
    world.colonies[0].owner = 1;
    world.colonies[0].planet = 2;
    world.colonies[0].n_pops = 255; // Never passed to a foreign population calculation.
    world.colonies[0].climate = -99;
    world.stars[1].planet_index[1] = 3;
    world.planets[3].star_index = 1;
    world.planets[3].orbit = 1;
    world.planets[3].planet_type = 99; // Hidden malformed ecology is never validated.
    memset(world.stars[1].name, 'X', sizeof(world.stars[1].name));
    const auto stars = world.stars;
    const auto planets = world.planets;
    const auto colonies = world.colonies;
    const uint32_t seed = game_random::Get_Random_Seed();
    const auto old_screen = MOX::current_screen;
    const auto old_look = MOX::look_at_colonies;
    MOX::current_screen = SCREEN_PLANET_SUMMARY;
    MOX::look_at_colonies = 1;
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_REQUIRE(observed.planets.size() == 3);
    DOCTEST_CHECK(memcmp(stars.data(), world.stars.data(), stars.size() * sizeof(s_star_data)) == 0);
    DOCTEST_CHECK(memcmp(planets.data(), world.planets.data(), planets.size() * sizeof(s_planet_data)) == 0);
    DOCTEST_CHECK(memcmp(colonies.data(), world.colonies.data(), colonies.size() * sizeof(s_colony)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    DOCTEST_CHECK(observed.systems[0].explored == SYS::Ok_To_View_System(0, 0));
    DOCTEST_CHECK_FALSE(observed.systems[1].explored);
    DOCTEST_CHECK_FALSE(PLNTSUM::Planet_Has_Been_Visited(3));
    char name[200];
    HACCESS::Do_Get_Planet_Name(1, name, 0);
    DOCTEST_CHECK(observed.planets[1].name == "ABC\xc3\xa9 II");
    DOCTEST_CHECK(std::string(name) == "ABC\x82 II");
    DOCTEST_CHECK(observed.planets[1].orbit == 3);
    DOCTEST_CHECK(observed.planets[1].food2 == 5);
    DOCTEST_CHECK(observed.planets[1].gravity_penalty == PLNTSUM::Productivity_Penalty(0, PLANET_GRAVITY_HEAVY_G));
    DOCTEST_CHECK(observed.planets[1].industry == COLCALC::minerals_per_mine[MINERAL_RESOURCE_RICH]);
    DOCTEST_CHECK(observed.planets[1].max_population == COLCALC::Planet_Max_Population_For_Player(&world.planets[1], 0));
    DOCTEST_CHECK_FALSE(observed.planets[2].max_population_available);
    DOCTEST_CHECK(observed.planets[2].climate == PLANET_CLIMATE_TERRAN);
    world.colonies[0].n_pops = 0;
    world.colonies[0].climate = 9;
    world.colonies[0].buildings[BUILDING_BIOSPHERES] = 1;
    world.planets[3].climate = -127;
    world.stars[1].system_special = -99;
    world.stars[1].in_nebula = 255;
    rpc::s_map_observation changed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&changed));
    DOCTEST_CHECK(changed.planets[2].climate == observed.planets[2].climate);
    DOCTEST_CHECK_FALSE(changed.planets[2].max_population_available);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    world.planets[1].climate = PLANET_CLIMATE_GAIA;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&changed));
    DOCTEST_CHECK(changed.planets[1].climate == PLANET_CLIMATE_GAIA);
    DOCTEST_CHECK(observed.planets[1].climate == PLANET_CLIMATE_TERRAN);
    MOX::current_screen = old_screen;
    MOX::look_at_colonies = old_look;
}

DOCTEST_TEST_CASE("exploration range uses actual native rounding block wormhole and alliance predicates") {
    s_world world;
    world.stars[0].has_colony = 1;
    world.stars[1].visited = 1;
    world.stars[1].x = 14 * 30;
    world.stars[2].spectral_class = STAR_CLASS_G;
    world.stars[2].x = 30;
    world.players[0].ship_range = 9;
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK_FALSE(SHIPMOVE::Star_In_Normal_Range_Of_Player(&world.stars[1], 0));
    DOCTEST_CHECK(observed.systems[1].in_extended_range);
    DOCTEST_CHECK(observed.systems[1].in_extended_range == SHIPMOVE::Star_In_Extended_Range_Of_Player(&world.stars[1], 0));
    struct_::Set_Bit_Field(world.stars[0].black_hole_blocks, 1);
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK_FALSE(observed.systems[1].in_extended_range);
    world.stars[1].wormhole_star_id = 2;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.systems[1].in_extended_range);
    DOCTEST_CHECK(observed.systems[1].wormhole_system == 2);
    world.stars[2].visited = 0;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK_FALSE(observed.systems[1].in_extended_range);
    world.stars[1].wormhole_star_id = -1;
    world.stars[2].has_colony = 2;
    world.players[0].treaty[1] = 2;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.systems[1].in_extended_range);
    world.players[0].treaty[1] = 0;
    world.stars[1].wormhole_star_id = MAX_STARS;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&observed));
    world.stars[1].visited = 0;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.systems[1].wormhole_available == rpc::EXPLORATION_UNKNOWN);
}

DOCTEST_TEST_CASE("exploration contact Lore mixed owner and special object knowledge are independently gated") {
    s_world world;
    MOX::NUM_COLONIES = 1;
    world.stars[1].planet_index[0] = 0;
    world.planets[0].star_index = 1;
    world.planets[0].colony_index = 0;
    world.colonies[0].planet = 0;
    world.colonies[0].owner = 1;
    world.colonies[0].outpost_flag = 1;
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK_FALSE(observed.systems[1].name_available);
    DOCTEST_CHECK(observed.planets.empty());
    world.players[0].contact[1] = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.systems[1].name == "abc\xc3\xa9");
    DOCTEST_CHECK(observed.systems[1].owner_slot == HAROLD::Star_Owner(1, 0));
    DOCTEST_CHECK_FALSE(observed.systems[1].explored);
    DOCTEST_CHECK(observed.planets.empty());
    world.players[0].contact[1] = 0;
    world.players[0].traits[TRAIT_OMNISCIENCE] = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.systems[1].name == "(abc\xc3\xa9)");
    DOCTEST_REQUIRE(observed.planets.size() == 1);
    DOCTEST_CHECK_FALSE(observed.systems[2].explored); // Black hole even under omniscience.
    world.planets[0].planet_type = PLANET_TYPE_GAS_GIANT;
    world.planets[0].climate = -99;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.planets[0].kind == PLANET_TYPE_GAS_GIANT);
    DOCTEST_CHECK(observed.planets[0].name.empty());
    world.planets[0].planet_type = PLANET_TYPE_PLANET;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&observed));
    world.planets[0].climate = PLANET_CLIMATE_GAIA;
    world.planets[0].planet_special = SYSTEM_SPECIAL_ORION_STAR;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.planets[0].special == 0);
    world.planets[0].orbit = 2;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&observed));
}

DOCTEST_TEST_CASE("nebula projection preserves exactly the native rendered unsigned modulo variant") {
    s_world world;
    const int values[] = {-128, -1, 0, 11, 12, 127};
    for (int value : values) {
        world.nebulas[0].type = (int8_t)value;
        rpc::s_map_observation observed;
        DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
        DOCTEST_CHECK(observed.nebulas[0].variant == (uint8_t)value % 12);
        DOCTEST_CHECK((int)world.nebulas[0].type == value);
    }
}

DOCTEST_TEST_CASE("native mixed system ownership keeps self preference and contact filtering without foreign colony scans") {
    s_world world;
    MOX::NUM_COLONIES = 3;
    for (int index = 0; index < 3; ++index) {
        world.stars[0].planet_index[index * 2] = (int16_t)index;
        world.planets[index].orbit = (int8_t)(index * 2);
        world.planets[index].colony_index = (int16_t)index;
        world.colonies[index].planet = (int16_t)index;
        world.colonies[index].owner = index == 2 ? 0 : 1;
        if (index != 2) world.colonies[index].n_pops = 255;
    }
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.systems[0].owner_slot == HAROLD::Star_Owner(0, 0));
    DOCTEST_CHECK(observed.systems[0].owner_slot == 0);
    DOCTEST_CHECK(observed.colonies.size() == 1);
    world.colonies[2].owner = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.systems[0].owner_slot == -1);
    DOCTEST_CHECK(observed.colonies.empty());
    world.players[0].contact[1] = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.systems[0].owner_slot == HAROLD::Star_Owner(0, 0));
    DOCTEST_CHECK(observed.systems[0].owner_slot == 1);
    for (const auto& planet : observed.planets) DOCTEST_CHECK_FALSE(planet.max_population_available);
    MOX::NUM_PLAYERS = 3;
    world.players[0].contact[2] = 1;
    world.players[2].color = 2;
    world.colonies[1].owner = 2;
    world.colonies[2].owner = -1;
    world.colonies[2].planet = -1;
    world.planets[2].colony_index = -1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.systems[0].owner_slot == HAROLD::Star_Owner(0, 0));
    DOCTEST_CHECK(observed.systems[0].owner_slot == 1); // Native tie order, not exclusive ownership.
}

DOCTEST_TEST_CASE("actual native exploration grants expire through copied store and bridge lifecycle boundaries") {
    s_world world;
    world.stars[0].planet_index[0] = 0;
    world.stars[1].planet_index[0] = 1;
    world.planets[1].star_index = 1;
    world.players[0].traits[TRAIT_OMNISCIENCE] = 1;
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &observed));
    const auto retained = store.snapshots.back();
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&bridge, 1, &epoch));
    DOCTEST_REQUIRE(rpc::Authorize_Client(&bridge, 1, epoch));
    rpc::s_request request;
    request.method = "game.listPlanets";
    request.id_json = "1";
    request.params_json = "{\"limit\":1}";
    rpc::Game_Query(&store, &request);
    DOCTEST_REQUIRE(store.cursors.size() == 1);
    const auto cursor = store.cursors[0].id;
    DOCTEST_REQUIRE(rpc::Submit_Work(&bridge, 1, epoch, &request, UINT64_MAX) == 0);
    rpc::s_work late;
    DOCTEST_REQUIRE(rpc::Take_Work(&bridge, 0, &late));
    const auto copied_response = rpc::Game_Query(&store, &late.request);
    DOCTEST_SUBCASE("resolution or world update begins before native mutation") {
        rpc::Pause_Game_Queries(&store);
        DOCTEST_CHECK(store.snapshots.empty());
        rpc::Resume_Game_Queries(&store);
    }
    DOCTEST_SUBCASE("native omniscience is withdrawn at fresh capture") {
        world.players[0].traits[TRAIT_OMNISCIENCE] = 0;
        DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
        DOCTEST_REQUIRE(rpc::Capture_Map(&store, &observed));
    }
    DOCTEST_SUBCASE("perspective boundary") {
        rpc::Revoke_Game_View(&store);
    }
    DOCTEST_SUBCASE("world replacement boundary") {
        rpc::Begin_Game_Reset(&store);
        DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    }
    DOCTEST_CHECK_FALSE(rpc::Client_Is_Current(&bridge, 1, epoch));
    DOCTEST_CHECK_FALSE(rpc::Complete_Work(&bridge, &late, &copied_response));
    DOCTEST_CHECK(store.cursors.empty());
    world.players[0].traits[TRAIT_OMNISCIENCE] = 0;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &observed));
    request.params_json = "{\"cursor\":" + rpc::Json_String(cursor) + "}";
    DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("-32004") != std::string::npos);
    DOCTEST_CHECK(retained.map.planets.size() == 2);
    DOCTEST_CHECK(store.snapshots.back().map.planets.size() == 1);
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("only active own colony links authorize partially known planet locations") {
    s_world world;
    MOX::planet_count = 2;
    MOX::NUM_COLONIES = 1;
    world.colonies[0].owner = -1;
    world.stars[1].planet_index[0] = 1;
    world.planets[1].star_index = 1;
    world.planets[1].colony_index = 1;
    world.colonies[1].planet = 1;
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(observed.planets.empty()); // Stale inactive bytes do not grant ownership.
    MOX::NUM_COLONIES = 2;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_REQUIRE(observed.planets.size() == 1);
    DOCTEST_CHECK_FALSE(observed.systems[1].explored);
    DOCTEST_CHECK(observed.planets[0].system == 1);
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &observed));
    const auto& snapshot = store.snapshots.back();
    DOCTEST_CHECK(snapshot.map.systems[1].planet_ids.empty());
    DOCTEST_CHECK(snapshot.map.colonies[0].planet_id == snapshot.planet_ids[0]);
    DOCTEST_CHECK(snapshot.map.planets[0].colony_id == snapshot.colony_ids[0]);
    DOCTEST_CHECK(snapshot.map.exploration_sensitive);
    MOX::planet_count = 1;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&observed)); // Malformed authorized own link.
    world.colonies[1].owner = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed)); // Hidden invalid foreign link.
    DOCTEST_CHECK(observed.planets.empty());
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

DOCTEST_TEST_CASE("fresh native ship ownership binds only to authorized player records without authorizing enemy details") {
    s_ship_world fixture;
    fixture.world.players[0].color = 0;
    fixture.world.players[1].color = 1;
    memcpy(fixture.world.players[1].name, "Contact", 8);
    SHIPS::Build_Ship_Icons();
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_CHECK(observed.ships[0].owner_slot == 0);
    DOCTEST_CHECK(observed.ships[1].owner_slot == -1);
    fixture.world.players[0].contact[1] = 1;
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_CHECK(observed.ships[1].owner_slot == 1);
    DOCTEST_CHECK(observed.ships[1].name.empty());
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &observed));
    const auto& snapshot = store.snapshots.back();
    DOCTEST_CHECK(snapshot.map.ships[1].owner_id == snapshot.player_ids[1]);
    DOCTEST_CHECK(snapshot.map.ships[1].owner_slot == -1);
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("fresh map routes reuse native ambient disclosure and only the displayed head cached ETA") {
    s_ship_world fixture;
    const uint32_t seed = game_random::Get_Random_Seed();
    fixture.ships[0].status = 1;
    fixture.ships[0].location = SHIP_LOCATION_MOVING_OFFSET + 1;
    fixture.ships[0].turns_left = -1; // Native map renders this as unsigned 255, not an estimate.
    fixture.ships[1].status = 1;
    fixture.ships[1].location = SHIP_LOCATION_MOVING_OFFSET;
    fixture.ships[1].turns_left = 0;
    SHIPS::Build_Ship_Icons();
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_REQUIRE(observed.ships.size() == 2);
    DOCTEST_CHECK(observed.ships[0].destination_available == rpc::SHIP_CURRENT);
    DOCTEST_CHECK(observed.ships[0].destination_system == 1); // Unvisited map position is still drawn by a route line.
    DOCTEST_CHECK(observed.ships[0].eta == 255);
    DOCTEST_CHECK(observed.ships[1].destination_available == rpc::SHIP_CONTEXT_REQUIRED);
    DOCTEST_CHECK(observed.ships[1].destination_system == -1);
    DOCTEST_CHECK(observed.ships[1].eta_available == rpc::SHIP_CONTEXT_REQUIRED);

    DOCTEST_SUBCASE("enemy-moves presentation, not contact or omniscience") {
        fixture.world.players[0].contact[1] = 1;
        fixture.world.players[0].traits[TRAIT_OMNISCIENCE] = 1;
        DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
        DOCTEST_CHECK(observed.ships[1].destination_available == rpc::SHIP_CONTEXT_REQUIRED);
        MOX::settings.enemy_moves = 1;
    }
    DOCTEST_SUBCASE("non-hostile foreign incoming to own colony") {
        fixture.world.players[0].contact[1] = 1;
        fixture.world.stars[0].has_colony = 1;
    }
    DOCTEST_SUBCASE("native outpost predicate ignores its viewer argument") {
        MOX::NUM_COLONIES = 1;
        fixture.world.stars[0].planet_index[0] = 0;
        fixture.world.planets[0].colony_index = 0;
        fixture.world.colonies[0].outpost_flag = 1;
        fixture.world.colonies[0].owner = 1;
    }
    int16_t enemy_node = -1;
    for (int index = 0; index < MOX::ship_icon_count; ++index) {
        const int16_t node = MOX::ship_icon[index].node_idx;
        if (MOX::ship_node[node].ship_idx == 1) enemy_node = node;
    }
    DOCTEST_REQUIRE(enemy_node >= 0);
    DOCTEST_CHECK((MOX::settings.enemy_moves != 0 || SHIPS::Enemy_Ship_Heading_Toward_Our_Colony(enemy_node)));
    const auto ships_before = fixture.ships;
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_CHECK(observed.ships[1].destination_available == rpc::SHIP_CURRENT);
    DOCTEST_CHECK(observed.ships[1].destination_system == 0);
    DOCTEST_CHECK(observed.ships[1].eta_available == rpc::SHIP_CURRENT);
    DOCTEST_CHECK(observed.ships[1].eta == 0);
    DOCTEST_CHECK(memcmp(ships_before.data(), fixture.ships.data(), ships_before.size() * sizeof(s_ship_data)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE("route capture distinguishes stationary, wormhole, special and undisclosed stack-member ETA") {
    s_ship_world fixture;
    rpc::s_map_observation observed;
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_CHECK(observed.ships[0].destination_available == rpc::SHIP_KNOWN_EMPTY);
    DOCTEST_CHECK(observed.ships[0].eta_available == rpc::SHIP_KNOWN_EMPTY);
    fixture.ships[0].location = SHIP_LOCATION_WORMHOLE_OFFSET + 1;
    fixture.ships[0].status = 1;
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_CHECK(observed.ships[0].destination_available == rpc::SHIP_CURRENT);
    DOCTEST_CHECK(observed.ships[0].eta_available == rpc::SHIP_CONTEXT_REQUIRED);
    fixture.ships[0].location = SHIP_LOCATION_MOVING_OFFSET + MOX::NUM_STARS;
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_CHECK(observed.ships[0].destination_available == rpc::SHIP_UNAUDITED);
    DOCTEST_CHECK(observed.ships[0].destination_system == -1);
    fixture.ships[0].location = SHIP_LOCATION_MOVING_OFFSET + 1;
    fixture.ships[0].turns_left = 7;
    fixture.ships[2] = fixture.ships[0];
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_REQUIRE(observed.ships.size() == 3);
    DOCTEST_CHECK(std::count_if(observed.ships.begin(), observed.ships.end(), [](const auto& ship) { return ship.eta_available == rpc::SHIP_CURRENT; }) == 1);
    DOCTEST_CHECK(std::count_if(observed.ships.begin(), observed.ships.end(), [](const auto& ship) { return ship.destination_available == rpc::SHIP_CURRENT; }) == 2);
    const auto head_eta = std::find_if(observed.ships.begin(), observed.ships.end(), [](const auto& ship) { return ship.eta_available == rpc::SHIP_CURRENT; })->eta;
    for (int index = 0; index < MOX::ship_icon_count; ++index) {
        const int16_t head = MOX::ship_icon[index].node_idx;
        const int16_t next = MOX::ship_node[head].next_node;
        if (next != -1) fixture.ships[MOX::ship_node[next].ship_idx].turns_left = 99;
    }
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_CHECK(std::count_if(observed.ships.begin(), observed.ships.end(), [head_eta](const auto& ship) { return ship.eta_available == rpc::SHIP_CURRENT && ship.eta == head_eta; }) == 1);
    // A malformed unvisited route must not make the existing native predicate read out of bounds.
    fixture.ships[1].status = 1;
    fixture.ships[1].location = SHIP_LOCATION_MOVING_OFFSET + 1;
    fixture.world.stars[1].planet_index[0] = MAX_PLANETS;
    SHIPS::Build_Ship_Icons();
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&observed));
    DOCTEST_CHECK(observed.ships.back().destination_available == rpc::SHIP_CONTEXT_REQUIRED);
}

DOCTEST_TEST_CASE("native fleet info publishes only produced cached ETA and never a movement preview") {
    s_ship_world fixture;
    struct s_info_state {
        s_fleet_box box = MOX::fleet_box;
        s_fleet_movement_icon icon = MOX::fleet_movement_icon[0];
        s_movable_box moveable = MOX::moveable_box[2];
        int16_t eta = MOX::eta_star_id;
        int16_t scanned = MOX::fleet_ship_scanned;
        int16_t chosen = MAINSCR::fleet_icon_chosen;
        s_ship_move_info movement = MOX::g_ship_move_info;
        std::vector<char> strings = MOX::h_strings;
        std::vector<size_t> offsets = {std::begin(MOX::h_string_ix), std::end(MOX::h_string_ix)};
        std::vector<game_events::s_fleet_box_info> copied;
        int clears = 0;
        uint64_t info_token = game_events::Subscribe_Fleet_Box_Info([](const game_events::s_fleet_box_info* info, void* context) {
            ((s_info_state*)context)->copied.push_back(*info);
            ((s_info_state*)context)->copied.back().members = nullptr;
            ((s_info_state*)context)->copied.back().selected = nullptr;
        },
            this);
        uint64_t event_token = game_events::Subscribe([](game_events::e_event event, void* context) {
            if (event == game_events::FLEET_BOX_INFO_CLEARED) ++((s_info_state*)context)->clears;
        },
            this);
        ~s_info_state() {
            game_events::Unsubscribe(info_token);
            game_events::Unsubscribe(event_token);
            MOX::fleet_box = box;
            MOX::fleet_movement_icon[0] = icon;
            MOX::moveable_box[2] = moveable;
            MOX::eta_star_id = eta;
            MOX::fleet_ship_scanned = scanned;
            MAINSCR::fleet_icon_chosen = chosen;
            MOX::g_ship_move_info = movement;
            MOX::h_strings = std::move(strings);
            std::copy(offsets.begin(), offsets.end(), MOX::h_string_ix);
        }
    } state;
    DOCTEST_REQUIRE(state.info_token != 0);
    DOCTEST_REQUIRE(state.event_token != 0);
    // Empty synthetic localized formats stop before rendering; the native owner
    // still selects and produces its real cached-route result.
    MOX::h_strings = {'\0'};
    std::fill(std::begin(MOX::h_string_ix), std::end(MOX::h_string_ix), 0);
    MOX::fleet_box.count = 10;
    MOX::moveable_box[2].id = MOX::box_off_flag[2];
    MOX::fleet_movement_icon[0].ship_node_idx = 0;
    MOX::ship_node[0].ship_idx = 1;
    MOX::eta_star_id = MOX::fleet_ship_scanned = MAINSCR::fleet_icon_chosen = -1;
    fixture.ships[1].status = 1;
    fixture.ships[1].location = SHIP_LOCATION_WORMHOLE_OFFSET + 1;
    fixture.ships[1].turns_left = -1;
    const auto ships_before = fixture.ships;
    const uint32_t seed = game_random::Get_Random_Seed();
    FLEETPOP::Print_Fleet_Box_Info();
    DOCTEST_REQUIRE(state.copied.size() == 1);
    DOCTEST_CHECK(state.copied[0].ship == 1);
    DOCTEST_CHECK(state.copied[0].node == 0);
    DOCTEST_CHECK(state.copied[0].turns_left == 255);
    DOCTEST_CHECK(state.copied[0].star == -1); // The producer withholds the unvisited name.
    DOCTEST_CHECK(memcmp(ships_before.data(), fixture.ships.data(), ships_before.size() * sizeof(s_ship_data)) == 0);
    fixture.world.stars[1].visited = 1;
    fixture.ships[1].turns_left = 0;
    FLEETPOP::Print_Fleet_Box_Info();
    DOCTEST_REQUIRE(state.copied.size() == 2);
    DOCTEST_CHECK(state.copied[1].star == 1);
    DOCTEST_CHECK(state.copied[1].turns_left == 0);
    DOCTEST_CHECK(state.copied[0].turns_left == 255); // No borrowed native lifetime.
    MOX::fleet_movement_icon[0].ship_node_idx = 2;
    MOX::ship_node[2].ship_idx = 1;
    FLEETPOP::Print_Fleet_Box_Info();
    DOCTEST_REQUIRE(state.copied.size() == 3);
    DOCTEST_CHECK(state.copied[2].ship == 1);
    DOCTEST_CHECK(state.copied[2].node == 2); // Same ship slot, different producer node.
    DOCTEST_CHECK(state.copied[0].node == 0);
    MOX::fleet_movement_icon[0].ship_node_idx = 0;
    for (s_ship_node& node : MOX::ship_node) {
        node.ship_idx = 0;
        node.selected = 0;
    }
    MOX::ship_node[0].ship_idx = 1;
    MOX::ship_node[1].selected = 1;
    MOX::moveable_box[2].id = MOX::box_off_flag[2] + 1;
    MOX::g_ship_move_info = {};
    MOX::g_ship_move_info.moving = 1;
    MOX::g_ship_move_info.turns_left = 12;
    MOX::eta_star_id = 1;
    FLEETPOP::Print_Fleet_Box_Info();
    DOCTEST_CHECK(state.copied.size() == 3); // A candidate target never becomes cached ship state.
    MOX::eta_star_id = -1;
    fixture.ships[1].status = 0;
    fixture.ships[1].location = 1;
    FLEETPOP::Print_Fleet_Box_Info();
    DOCTEST_CHECK(state.copied.size() == 3);
    DOCTEST_CHECK(state.clears == 5);
    SHIPS::Build_Ship_Icons();
    DOCTEST_CHECK(state.clears == 6); // A rebuilt node/slot mapping cannot inherit the result.
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

namespace {
    // Synthetic dimensions/cache and a real software drawing surface;
    // no game assets or renderer stubs are used by the route producer.
    struct s_route_surface {
        s_file_buffer_entry* old_files = buffer::file_buffer;
        int16_t old_file_count = buffer::file_buffer_count;
        int32_t old_load_count = buffer::global_load_count;
        int32_t old_size = buffer::buffer_space_size;
        void* old_buffer = buffer::buffer_space_seg;
        void* old_cache = MOX::global_cache_seg;
        uint8_t* old_page = video::current_video_page;
        video::s_page_buffer old_off_page = video::g_page_off;
        int32_t old_render_buffer = platform::g_render_buffer;
        int32_t old_screen_x = video::g_screen_res_x;
        int32_t old_screen_y = video::g_screen_res_y;
        int16_t old_scale = MOX::cur_map_scale;
        int16_t old_x = MOX::cur_map_x;
        int16_t old_y = MOX::cur_map_y;
        int16_t old_phase = MOX::multi_colored_line_start;
        std::vector<s_animation_header*> old_pictures = {std::begin(MOX::ship_icon_seg), std::end(MOX::ship_icon_seg)};
        std::vector<uint8_t> old_widths = {std::begin(MOX::ship_icon_width), std::end(MOX::ship_icon_width)};
        std::vector<uint8_t> old_heights = {std::begin(MOX::ship_icon_height), std::end(MOX::ship_icon_height)};
        allocate::s_alloc_header header = {};
        s_file_buffer_entry file = {};
        s_animation_header picture = {};
        s_font old_font = fonts::font_header;
        int16_t old_font_style = fonts::current_font_style;
        s_colors old_font_colors = fonts::current_font_color_array;
        uint8_t* old_font_data = fonts::font_data;
        int16_t old_print_x = fonts::print_xpos;
        int16_t old_print_y = fonts::print_ypos;
        int16_t old_alias = fonts::draw_alias_flag;
        uint8_t glyph[2] = {1, 0x80};
        std::vector<uint8_t> pixels = std::vector<uint8_t>(640 * 480);
        s_route_surface() {
            fonts::font_header = {};
            for (size_t style = 0; style < std::size(fonts::font_header.font_glyph_widths); ++style) {
                fonts::font_header.font_heights[style] = 1;
                std::fill(std::begin(fonts::font_header.font_glyph_widths[style]), std::end(fonts::font_header.font_glyph_widths[style]), 1);
            }
            fonts::font_data = glyph;
            picture.width = picture.height = 8;
            strlcpy(file.file_name, "BUFFER0.LBX", sizeof(file.file_name));
            file.segment_count = 32;
            for (int index = 0; index < 32; ++index) {
                file.segments[index].buffer_seg_id = 205 + index;
                file.segments[index].is_loaded = 1;
                file.segments[index].data_ptr = &picture;
            }
            buffer::file_buffer = &file;
            buffer::file_buffer_count = 1;
            buffer::buffer_space_size = 0;
            buffer::buffer_space_seg = MOX::global_cache_seg = &header + 1;
            video::current_video_page = pixels.data();
            video::g_page_off = {pixels.data(), 640, 480};
            platform::g_render_buffer = 1;
            video::g_screen_res_x = 640;
            video::g_screen_res_y = 480;
            MOX::cur_map_scale = 10;
            MOX::cur_map_x = MOX::cur_map_y = MOX::multi_colored_line_start = 0;
        }
        ~s_route_surface() {
            fonts::font_header = old_font;
            fonts::current_font_style = old_font_style;
            fonts::current_font_color_array = old_font_colors;
            fonts::font_data = old_font_data;
            fonts::print_xpos = old_print_x;
            fonts::print_ypos = old_print_y;
            fonts::draw_alias_flag = old_alias;
            buffer::file_buffer = old_files;
            buffer::file_buffer_count = old_file_count;
            buffer::global_load_count = old_load_count;
            buffer::buffer_space_size = old_size;
            buffer::buffer_space_seg = old_buffer;
            MOX::global_cache_seg = old_cache;
            video::current_video_page = old_page;
            video::g_page_off = old_off_page;
            platform::g_render_buffer = old_render_buffer;
            video::g_screen_res_x = old_screen_x;
            video::g_screen_res_y = old_screen_y;
            MOX::cur_map_scale = old_scale;
            MOX::cur_map_x = old_x;
            MOX::cur_map_y = old_y;
            MOX::multi_colored_line_start = old_phase;
            std::copy(old_pictures.begin(), old_pictures.end(), MOX::ship_icon_seg);
            std::copy(old_widths.begin(), old_widths.end(), MOX::ship_icon_width);
            std::copy(old_heights.begin(), old_heights.end(), MOX::ship_icon_height);
        }
    };

    struct s_eta_world {
        s_ship_world native;
        rpc::s_bridge bridge;
        rpc::s_game_store store;
        game_rpc::s_fleet_eta_adapter adapter;
        s_leader_data* old_leaders = MOX::leaders;
        std::vector<s_leader_data> leaders = std::vector<s_leader_data>(MAX_LEADERS);
        std::vector<uint64_t> generations = std::vector<uint64_t>(MAX_SHIPS);
        s_fleet_box old_box = MOX::fleet_box;
        s_movable_box old_moveable = MOX::moveable_box[2];
        int16_t old_stack = MOX::fleet_box_ship_stack;
        int16_t old_eta = MOX::eta_star_id;
        int16_t old_scanned = MOX::fleet_ship_scanned;
        int16_t old_chosen = MAINSCR::fleet_icon_chosen;
        std::vector<s_fleet_movement_icon> icons = {std::begin(MOX::fleet_movement_icon), std::end(MOX::fleet_movement_icon)};
        std::vector<char> strings = MOX::h_strings;
        std::vector<size_t> offsets = {std::begin(MOX::h_string_ix), std::end(MOX::h_string_ix)};
        bool foreign = false;
        bool contextual = false;
        bool ordinary = false;
        s_route_surface surface;
        s_eta_world(int path = 0) : foreign(path != 0), contextual(path >= 2), ordinary(path >= 3) {
            MOX::leaders = leaders.data();
            for (auto& leader : leaders) {
                leader.player_index = -1;
                leader.status = -1;
                leader.location = -1;
            }
            DOCTEST_REQUIRE(orion2re::lua::game::Capture_Ship_Generation_State(generations.data(), MAX_SHIPS));
            native.world.stars[1].x = 100;
            native.world.stars[1].visited = 1;
            native.world.stars[2].x = 200;
            MOX::NUM_SHIPS = 11;
            for (int index = 0; index < 10; ++index) {
                native.ships[index] = {};
                native.ships[index].owner = foreign ? 1 : 0;
                native.ships[index].status = 2;
                native.ships[index].location = SHIP_LOCATION_WORMHOLE_OFFSET + 1;
                native.ships[index].turns_left = 0;
                snprintf(native.ships[index].d.name, sizeof(native.ships[index].d.name), "Own-%d", index);
                if (foreign) memset(native.ships[index].d.name, 0xff, sizeof(native.ships[index].d.name));
            }
            if (foreign) MOX::settings.enemy_moves = contextual ? 0 : 1;
            native.ships[10] = {};
            native.ships[10].owner = foreign ? 0 : 1;
            native.ships[10].x = 30;
            if (ordinary) {
                for (auto& star : native.world.stars) star.wormhole_star_id = -1;
                native.ships[10].x = 0; // Stationary scanner at the real origin system.
                int16_t list[11];
                for (int index = 0; index < 10; ++index) {
                    list[index] = (int16_t)index;
                    native.ships[index].location = 0;
                    native.ships[index].status = 0;
                }
                list[10] = -1;
                s_ship_move_info movement = {};
                movement.turns_left = 3;
                movement.traveling_speed = 1;
                SHIPMOVE::Make_Ships_Move_To(list, 1, &movement);
                DOCTEST_REQUIRE((native.ships[0].status == 2));
                DOCTEST_REQUIRE((native.ships[0].location == SHIP_LOCATION_WORMHOLE_OFFSET + 1));
                if (path == 4) {
                    const auto nebulas = MOX::NUM_NEBULAS;
                    MOX::NUM_NEBULAS = 0; // This movement fixture has no nebula artwork.
                    SHIPMOVE::Move_All_Ships_Toward_Stars();
                    MOX::NUM_NEBULAS = nebulas;
                    DOCTEST_REQUIRE((native.ships[0].x == 30));
                    DOCTEST_REQUIRE((native.ships[0].turns_left == 2));
                }
                DOCTEST_REQUIRE((native.ships[0].status == (path == 3 ? 2 : 1)));
                DOCTEST_REQUIRE((native.ships[0].location == (path == 3 ? SHIP_LOCATION_WORMHOLE_OFFSET : SHIP_LOCATION_MOVING_OFFSET) + 1));
                // Zero is a valid cached presentation value; no movement is
                // calculated by any query or observation.
                for (int index = 0; index < 10; ++index) native.ships[index].turns_left = 0;
            }
            orion2re::lua::game::Restore_Legacy_Ship_Generations();
            MOX::h_strings = {'\0', '%', 'u', '\0'};
            std::fill(std::begin(MOX::h_string_ix), std::end(MOX::h_string_ix), 0);
            MOX::h_string_ix[307] = 1; // Exercise the actual map-icon ETA renderer.
            MOX::fleet_box.name_seg = &surface.picture;
            MOX::eta_star_id = MOX::fleet_ship_scanned = MAINSCR::fleet_icon_chosen = -1;
            DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
            DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
            DOCTEST_REQUIRE(game_rpc::Initialize_Fleet_Eta_Adapter(&adapter, &store));
        }
        ~s_eta_world() {
            game_rpc::Shutdown_Fleet_Eta_Adapter(&adapter);
            rpc::Shutdown_Game_Store(&store);
            orion2re::lua::game::Restore_Ship_Generation_State(generations.data(), MAX_SHIPS);
            MOX::fleet_box = old_box;
            MOX::moveable_box[2] = old_moveable;
            MOX::fleet_box_ship_stack = old_stack;
            MOX::eta_star_id = old_eta;
            MOX::fleet_ship_scanned = old_scanned;
            MAINSCR::fleet_icon_chosen = old_chosen;
            std::copy(icons.begin(), icons.end(), MOX::fleet_movement_icon);
            MOX::h_strings = std::move(strings);
            std::copy(offsets.begin(), offsets.end(), MOX::h_string_ix);
            MOX::leaders = old_leaders;
        }
        void Refresh() {
            SHIPS::Build_Ship_Icons();
            DOCTEST_REQUIRE(!adapter.associations.empty());
            const auto head = std::find_if(adapter.associations.begin(), adapter.associations.end(), [](const game_rpc::s_ship_association& item) { return item.slot < 10 && item.head == item.node; });
            DOCTEST_REQUIRE(head != adapter.associations.end());
            MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Ship_Id(head->slot);
            MOX::moveable_box[2].id = MOX::box_off_flag[2] + 1;
            MOX::fleet_box.count = 10; // Synthetic empty formats avoid asset-dependent drawing.
            int index = 0;
            for (int16_t node = head->head; node != -1; node = MOX::ship_node[node].next_node) {
                MOX::fleet_movement_icon[index++].ship_node_idx = node;
            }
            DOCTEST_REQUIRE(index == 10);
            MOX::ship_node[head->head].selected = foreign ? 0 : 1;
        }
        void Print() {
            game_events::s_observation_scope draw(game_events::MAIN_VIEW_DRAW_ENTERED, game_events::MAIN_VIEW_DRAW_LEAVING);
            game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
            if (contextual) SHIPS::Do_Ship_Destination_Lines();
            FLEETPOP::Print_Fleet_Box_Info();
        }
        std::string Query(const char* method = "game.listShips", const char* params = "{}", uint64_t* context = nullptr) {
            rpc::s_request request;
            request.id_json = "1";
            request.method = method;
            request.params_json = params;
            return rpc::Game_Query(&store, &request, context);
        }
        size_t Current_Etas() {
            const auto& map = store.snapshots.back().map;
            return std::count_if(map.ships.begin(), map.ships.end(), [](const rpc::s_ship_observation& ship) { return ship.eta_available == rpc::SHIP_CURRENT; });
        }
    };
} // namespace

DOCTEST_TEST_CASE_TEMPLATE("real cached producer authorizes only an initialized surviving route head including zero", Fleet, std::false_type, std::true_type, std::integral_constant<int, 2>, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    const auto ambient = fixture.Query();
    if (fixture.contextual) {
        DOCTEST_CHECK(MOX::settings.enemy_moves == 0);
        DOCTEST_CHECK_FALSE(SHIPS::Enemy_Ship_Heading_Toward_Our_Colony(MOX::fleet_movement_icon[0].ship_node_idx));
        for (const auto& item : fixture.adapter.associations) {
            if (item.slot < 10) DOCTEST_CHECK(item.ship.destination_available == rpc::SHIP_CONTEXT_REQUIRED);
        }
    }
    fixture.Print();
    DOCTEST_CHECK(fixture.Query() == ambient);
    owner.Initialized();
    DOCTEST_CHECK(fixture.Query() == ambient); // Never promote the pre-initialization result.
    const auto records = fixture.native.ships;
    const uint32_t seed = game_random::Get_Random_Seed();
    fixture.native.world.players[0].bc = 123;
    fixture.Print();
    uint64_t context = 0;
    const auto disclosed = fixture.Query("game.listShips", "{}", &context);
    DOCTEST_REQUIRE(context != 0);
    DOCTEST_CHECK(fixture.Current_Etas() == 1);
    DOCTEST_CHECK(disclosed.find("\"eta\":{\"availability\":\"current\",\"value\":0}") != std::string::npos);
    const auto retained = fixture.store.snapshots.back();
    if (fixture.contextual) {
        DOCTEST_CHECK(std::count_if(retained.map.ships.begin(), retained.map.ships.end(), [](const rpc::s_ship_observation& ship) { return ship.destination_available == rpc::SHIP_CURRENT; }) == 1);
        const auto head = std::find_if(retained.map.ships.begin(), retained.map.ships.end(), [](const rpc::s_ship_observation& ship) { return ship.eta_available == rpc::SHIP_CURRENT; });
        DOCTEST_REQUIRE(head != retained.map.ships.end());
        DOCTEST_CHECK(head->destination_id == fixture.store.system_ids[1]);
        DOCTEST_CHECK(head->destination_system == -1);
    }
    DOCTEST_CHECK(retained.map.players[0].treasury == 123);
    fixture.native.world.players[0].bc = 456;
    fixture.Print();
    DOCTEST_CHECK(retained.map.players[0].treasury == 123);
    DOCTEST_CHECK(fixture.store.snapshots.back().map.players[0].treasury == 456);
    DOCTEST_CHECK(fixture.store.snapshots.back().ship_ids == retained.ship_ids);
    DOCTEST_CHECK(memcmp(records.data(), fixture.native.ships.data(), records.size() * sizeof(s_ship_data)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    const auto repeated = fixture.Query();
    fixture.Print();
    DOCTEST_CHECK(fixture.Query() == repeated); // Ordinary redraw is usable, not epoch churn.
}

DOCTEST_TEST_CASE("non-own cached producer requires an independent native ambient route gate and never a foreign name") {
    s_eta_world fixture(true);
    MOX::settings.enemy_moves = 0;
    bool allowed = false;
    DOCTEST_SUBCASE("enemy moves") {
        MOX::settings.enemy_moves = 1;
        allowed = true;
    }
    DOCTEST_SUBCASE("incoming to own full colony without war") {
        fixture.native.world.stars[1].has_colony = 1;
        allowed = true;
    }
    DOCTEST_SUBCASE("native outpost gate includes a foreign outpost") {
        MOX::NUM_COLONIES = 1;
        fixture.native.world.stars[1].planet_index[0] = 0;
        fixture.native.world.planets[0].star_index = 1;
        fixture.native.world.planets[0].colony_index = 0;
        fixture.native.world.colonies[0].outpost_flag = 1;
        fixture.native.world.colonies[0].owner = 1;
        allowed = true;
    }
    DOCTEST_SUBCASE("detection alone") {}
    DOCTEST_SUBCASE("known contact and ownership alone") {
        fixture.native.world.players[0].contact[1] = 1;
        fixture.native.world.players[1].color = 1;
    }
    DOCTEST_SUBCASE("native omniscience alone") {
        fixture.native.world.players[0].traits[TRAIT_OMNISCIENCE] = 1;
    }
    DOCTEST_SUBCASE("malformed incoming links") {
        fixture.native.world.stars[1].visited = 0;
        fixture.native.world.stars[1].planet_index[0] = MAX_PLANETS;
    }
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    const int16_t node = MOX::fleet_movement_icon[0].ship_node_idx;
    if (fixture.native.world.stars[1].planet_index[0] != MAX_PLANETS) {
        DOCTEST_CHECK((MOX::settings.enemy_moves != 0 || SHIPS::Enemy_Ship_Heading_Toward_Our_Colony(node)) == allowed);
    }
    owner.Initialized();
    fixture.Print();
    DOCTEST_CHECK(fixture.store.context_active == allowed);
    DOCTEST_CHECK(fixture.Current_Etas() == (allowed ? 1 : 0));
    for (const auto& ship : fixture.store.snapshots.back().map.ships) {
        DOCTEST_CHECK(ship.name.empty());
        if (ship.eta_available == rpc::SHIP_CURRENT) DOCTEST_CHECK_FALSE(ship.owned);
    }
    DOCTEST_CHECK(fixture.Query().find("\"name\":{\"availability\":\"unaudited\"}") != std::string::npos);
}

DOCTEST_TEST_CASE_TEMPLATE("contextual destination requires an actual current same-frame selected-box route presentation", Fleet, std::integral_constant<int, 2>, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    fixture.Print();
    DOCTEST_REQUIRE(fixture.store.context_active);
    const auto retained = fixture.store.snapshots.back();
    fixture.Query("game.listShips", "{\"limit\":1}");
    const auto cursor = fixture.store.cursors.back().id;
    DOCTEST_SUBCASE("open box and cached producer without route drawing") {
        game_events::s_main_view_input_scope input;
        game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
        FLEETPOP::Print_Fleet_Box_Info();
    }
    DOCTEST_SUBCASE("route drawn outside an active native frame cannot be promoted") {
        game_events::s_main_view_input_scope input;
        SHIPS::Do_Ship_Destination_Lines();
        game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
        FLEETPOP::Print_Fleet_Box_Info();
    }
    DOCTEST_SUBCASE("previous frame line cannot authorize a subsequent print") {
        game_events::s_main_view_input_scope input;
        {
            game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
            SHIPS::Do_Ship_Destination_Lines();
        }
        {
            game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
            FLEETPOP::Print_Fleet_Box_Info();
        }
    }
    DOCTEST_SUBCASE("frame without any fleet-box print withdraws contextual authority") {
        game_events::s_main_view_input_scope input;
        game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
        SHIPS::Do_Ship_Destination_Lines();
    }
    DOCTEST_SUBCASE("background line and producer cannot restore authority") {
        game_events::Publish(game_events::MAIN_VIEW_INPUT_INTERRUPTED);
        game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
        SHIPS::Do_Ship_Destination_Lines();
        FLEETPOP::Print_Fleet_Box_Info();
    }
    DOCTEST_SUBCASE("route drawn for a different open head cannot associate the cached producer") {
        game_events::s_main_view_input_scope input;
        game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
        const int16_t old_stack = MOX::fleet_box_ship_stack;
        MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Ship_Id(10);
        SHIPS::Do_Ship_Destination_Lines();
        MOX::fleet_box_ship_stack = old_stack;
        FLEETPOP::Print_Fleet_Box_Info();
    }
    DOCTEST_CHECK_FALSE(fixture.store.context_active);
    uint64_t context = 99;
    const auto ambient = fixture.Query("game.listShips", "{}", &context);
    DOCTEST_CHECK(context == 0);
    DOCTEST_CHECK(ambient.find("\"destination\":{\"availability\":\"current\"") == std::string::npos);
    DOCTEST_CHECK(ambient.find("\"eta\":{\"availability\":\"current\"") == std::string::npos);
    DOCTEST_CHECK(retained.map.context_epoch != 0);
    fixture.Print();
    DOCTEST_CHECK(fixture.store.snapshots.back().ship_ids == retained.ship_ids);
    DOCTEST_CHECK(fixture.store.snapshots.back().revision == retained.revision);
    DOCTEST_CHECK(fixture.Query("game.listShips", ("{\"cursor\":" + rpc::Json_String(cursor) + "}").c_str()).find("\"error\"") != std::string::npos);
}

DOCTEST_TEST_CASE_TEMPLATE("contextual route revocation retains independently authorized native ambient destinations", Fleet, std::integral_constant<int, 2>, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    fixture.native.ships[10].status = 2;
    fixture.native.ships[10].location = SHIP_LOCATION_WORMHOLE_OFFSET + 2;
    fixture.native.ships[10].x = 0; // Keep the own scanner at origin 0; its ambient destination is 2.
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    fixture.Print();
    DOCTEST_REQUIRE(fixture.store.context_active);
    MOVEBOX::Vanish_Fleet_Movement_Box();
    DOCTEST_CHECK_FALSE(fixture.store.context_active);
    uint64_t context = 99;
    fixture.Query("game.listShips", "{}", &context);
    DOCTEST_CHECK(context == 0);
    const auto& ambient = fixture.store.snapshots.front().map;
    const auto own = std::find_if(ambient.ships.begin(), ambient.ships.end(), [](const rpc::s_ship_observation& ship) { return ship.owned; });
    DOCTEST_REQUIRE(own != ambient.ships.end());
    DOCTEST_CHECK(own->destination_available == rpc::SHIP_CURRENT);
    DOCTEST_CHECK(own->destination_id == fixture.store.system_ids[2]);
    for (const auto& ship : ambient.ships) {
        if (!ship.owned) {
            DOCTEST_CHECK(ship.destination_available == rpc::SHIP_CONTEXT_REQUIRED);
            DOCTEST_CHECK(ship.eta_available == rpc::SHIP_CONTEXT_REQUIRED);
        }
    }
}

DOCTEST_TEST_CASE_TEMPLATE("ordinary contextual ETA comes from the later cached fleet-box producer not the map-icon print", Fleet, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    game_events::s_main_view_input_scope input;
    game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
    SHIPS::Do_Ship_Destination_Lines();
    DOCTEST_CHECK_FALSE(fixture.store.context_active); // Neither a line nor map ETA grants a fleet-box capture.
    const int16_t slot = MOX::ship_node[MOX::fleet_movement_icon[0].ship_node_idx].ship_idx;
    fixture.native.ships[slot].turns_left = 255;
    FLEETPOP::Print_Fleet_Box_Info();
    DOCTEST_REQUIRE(fixture.store.context_active);
    DOCTEST_CHECK(fixture.Query().find("\"eta\":{\"availability\":\"current\",\"value\":255}") != std::string::npos);
}

DOCTEST_TEST_CASE("ordinary native departure advances to transit without inheriting its old contextual page") {
    s_eta_world fixture(3);
    for (int index = 0; index < 10; ++index) fixture.native.ships[index].turns_left = 3;
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    fixture.Print();
    DOCTEST_REQUIRE(fixture.store.context_active);
    const auto retained = fixture.store.snapshots.back();
    fixture.Query("game.listShips", "{\"limit\":1}");
    const auto cursor = fixture.store.cursors.back().id;
    game_events::Publish(game_events::RESOLUTION_BEGIN);
    rpc::Pause_Game_Queries(&fixture.store);
    const auto nebulas = MOX::NUM_NEBULAS;
    MOX::NUM_NEBULAS = 0;
    SHIPMOVE::Move_All_Ships_Toward_Stars();
    MOX::NUM_NEBULAS = nebulas;
    DOCTEST_CHECK((fixture.native.ships[0].status == 1));
    DOCTEST_CHECK((fixture.native.ships[0].location == SHIP_LOCATION_MOVING_OFFSET + 1));
    DOCTEST_CHECK_FALSE(fixture.store.context_active);
    DOCTEST_CHECK(fixture.Query().find("\"error\"") != std::string::npos);
    rpc::Resume_Game_Queries(&fixture.store);
    game_events::Publish(game_events::RESOLUTION_END);
    fixture.Refresh();
    fixture.Print();
    DOCTEST_REQUIRE(fixture.store.context_active);
    DOCTEST_CHECK(fixture.Query().find("\"eta\":{\"availability\":\"current\",\"value\":2}") != std::string::npos);
    DOCTEST_CHECK(std::count_if(retained.map.ships.begin(), retained.map.ships.end(), [](const rpc::s_ship_observation& ship) { return ship.eta_available == rpc::SHIP_CURRENT && ship.eta == 3; }) == 1);
    DOCTEST_CHECK(fixture.Query("game.listShips", ("{\"cursor\":" + rpc::Json_String(cursor) + "}").c_str()).find("\"error\"") != std::string::npos);
}

DOCTEST_TEST_CASE_TEMPLATE("contextual admission rejects mixed native movement status and location encodings", Fleet, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    for (int index = 0; index < 10; ++index) {
        fixture.native.ships[index].status = Fleet::value == 3 ? 1 : 2;
        fixture.native.ships[index].x = 0; // Keep the malformed orbiting status safe for native drawing.
    }
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    fixture.Print();
    DOCTEST_CHECK_FALSE(fixture.store.context_active);
    DOCTEST_CHECK(fixture.Query().find("\"destination\":{\"availability\":\"current\"") == std::string::npos);
}

DOCTEST_TEST_CASE("non-own cached ETA cannot acquire ambient authority after refresh or keep a withdrawn gate") {
    s_eta_world fixture(true);
    bool withdrawn = false;
    DOCTEST_SUBCASE("withdrawn enemy moves") {
        withdrawn = true;
    }
    DOCTEST_SUBCASE("newly enabled enemy moves needs a fresh association") {
        MOX::settings.enemy_moves = 0;
    }
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    fixture.Print();
    DOCTEST_CHECK(fixture.store.context_active == withdrawn);
    MOX::settings.enemy_moves = withdrawn ? 0 : 1;
    fixture.Print();
    DOCTEST_CHECK_FALSE(fixture.store.context_active);
    fixture.Refresh();
    fixture.Print();
    DOCTEST_CHECK(fixture.store.context_active == !withdrawn);
}

DOCTEST_TEST_CASE_TEMPLATE("child parent draws cannot reauthorize and ordinary native redraw restores fresh ETA", Fleet, std::false_type, std::true_type, std::integral_constant<int, 2>, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    fixture.Print();
    const auto ids = fixture.store.snapshots.back().ship_ids;
    const auto old_capture = fixture.store.snapshots.back();
    {
        game_events::s_main_view_input_scope input;
        game_events::Publish(game_events::MAIN_VIEW_INPUT_INTERRUPTED);
        game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
        if (fixture.contextual) SHIPS::Do_Ship_Destination_Lines();
        FLEETPOP::Print_Fleet_Box_Info(); // The child's saved parent draw, before child Get_Input.
        uint64_t context = 99;
        fixture.Query("game.listShips", "{}", &context);
        DOCTEST_CHECK(context == 0);
    }
    DOCTEST_CHECK(fixture.store.snapshots.back().id == old_capture.id);
    fixture.Print();
    DOCTEST_CHECK(fixture.Current_Etas() == 1);
    DOCTEST_CHECK(fixture.store.snapshots.back().ship_ids == ids);
    DOCTEST_CHECK(fixture.store.snapshots.back().revision == old_capture.revision);
    DOCTEST_CHECK(fixture.store.snapshots.back().id != old_capture.id);
    game_events::Publish(game_events::FLEET_BOX_VIEW_ENTERED); // Replacement does not restore its parent.
    fixture.Print();
    DOCTEST_CHECK_FALSE(fixture.store.context_active);
    DOCTEST_CHECK(fixture.adapter.associations.empty());
}

DOCTEST_TEST_CASE_TEMPLATE("fleet box ETA preserves unsigned bytes and cannot manufacture destination knowledge", Fleet, std::false_type, std::true_type, std::integral_constant<int, 2>, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    bool special = false;
    bool no_selection = false;
    DOCTEST_SUBCASE("unsigned 255") {
        for (int index = 0; index < 10; ++index) fixture.native.ships[index].turns_left = -1;
    }
    DOCTEST_SUBCASE("withheld producer name but independently current ambient destination") {
        fixture.native.world.stars[1].visited = 0;
    }
    DOCTEST_SUBCASE("native open-box presentation also permits an empty selection") {
        no_selection = true;
    }
    DOCTEST_SUBCASE("special destination has no system handle") {
        special = true;
        for (int index = 0; index < 10; ++index) fixture.native.ships[index].location = (Fleet::value == 4 ? SHIP_LOCATION_MOVING_OFFSET : SHIP_LOCATION_WORMHOLE_OFFSET) + MOX::NUM_STARS;
    }
    {
        game_events::s_fleet_box_view_scope owner;
        fixture.Refresh();
        if (no_selection) {
            for (auto& node : MOX::ship_node) node.selected = 0;
        }
        owner.Initialized();
        fixture.Print();
        DOCTEST_CHECK(fixture.store.context_active == !special);
        if (!special) {
            const auto copied = fixture.Query();
            DOCTEST_CHECK(fixture.Current_Etas() == 1);
            if (fixture.native.ships[0].turns_left == 255) DOCTEST_CHECK(copied.find("\"availability\":\"current\",\"value\":255") != std::string::npos);
            if (fixture.native.world.stars[1].visited == 0) DOCTEST_CHECK_FALSE(fixture.store.snapshots.back().map.systems[1].name_available);
            MOX::ship_node[MOX::fleet_movement_icon[0].ship_node_idx].ship_idx = MAX_SHIPS - 1;
            fixture.native.ships[0].turns_left = 17;
            DOCTEST_CHECK(fixture.Query() == copied); // Retained values own their lifetimes; reads never traverse nodes.
        }
    }
    DOCTEST_CHECK_FALSE(fixture.store.context_active);
    DOCTEST_CHECK(fixture.adapter.associations.empty());
}

DOCTEST_TEST_CASE_TEMPLATE("native stale selection node reuse and slot retirement cannot inherit cached ETA", Fleet, std::false_type, std::true_type, std::integral_constant<int, 2>, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    fixture.Print();
    DOCTEST_REQUIRE(fixture.store.context_active);
    const int16_t first_node = MOX::fleet_movement_icon[0].ship_node_idx;
    const int16_t first_ship = MOX::ship_node[first_node].ship_idx;
    DOCTEST_SUBCASE("unreachable stale selection") {
        MOX::ship_node[MAX_SHIPS - 1] = {first_ship, -1, 1};
        fixture.Print();
    }
    DOCTEST_SUBCASE("stale movement node points at same slot") {
        MOX::ship_node[MAX_SHIPS - 1] = {first_ship, -1, 0};
        MOX::fleet_movement_icon[0].ship_node_idx = MAX_SHIPS - 1;
        fixture.Print();
    }
    DOCTEST_SUBCASE("current box member no longer matches refresh") {
        const int16_t member = MOX::ship_node[first_node].next_node;
        DOCTEST_REQUIRE(member >= 0);
        fixture.native.ships[MOX::ship_node[member].ship_idx].location = SHIP_LOCATION_WORMHOLE_OFFSET + 2;
        fixture.Print();
    }
    DOCTEST_SUBCASE("current head ownership changed") {
        fixture.native.ships[first_ship].owner = 7;
        fixture.Print();
    }
    DOCTEST_SUBCASE("duplicate box member") {
        MOX::fleet_movement_icon[1].ship_node_idx = first_node;
        fixture.Print();
    }
    DOCTEST_SUBCASE("incomplete box membership") {
        --MOX::fleet_box.count;
        fixture.Print();
    }
    DOCTEST_SUBCASE("movement preview is not cached state") {
        for (auto& node : MOX::ship_node) node.selected = 0;
        MOX::eta_star_id = 1;
        fixture.Print();
    }
    DOCTEST_SUBCASE("missing generation") {
        orion2re::lua::game::Reset_Ship_Generations();
        fixture.Print();
    }
    DOCTEST_SUBCASE("slot retired and reused with byte-identical public data") {
        const auto original = fixture.native.ships[first_ship];
        SHIPS::Kill_Ship(first_ship);
        DOCTEST_CHECK_FALSE(fixture.store.context_active);
        DOCTEST_REQUIRE(orion2re::lua::game::Advance_Ship_Generation(first_ship));
        fixture.native.ships[first_ship] = original;
        fixture.Print();
    }
    DOCTEST_SUBCASE("box closed") {
        MOVEBOX::Vanish_Fleet_Movement_Box();
        fixture.Print();
    }
    DOCTEST_SUBCASE("box replacement clear") {
        game_events::Publish(game_events::FLEET_BOX_INFO_CLEARED);
        DOCTEST_CHECK_FALSE(fixture.store.context_active);
        return;
    }
    DOCTEST_CHECK_FALSE(fixture.store.context_active);
    uint64_t context = 99;
    fixture.Query("game.listShips", "{}", &context);
    DOCTEST_CHECK(context == 0);
}

DOCTEST_TEST_CASE_TEMPLATE("native rebuild and world boundaries revoke ETA without exposing hidden identity changes", Fleet, std::false_type, std::true_type, std::integral_constant<int, 2>, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    fixture.Print();
    const auto before = fixture.Query();
    const auto before_ids = fixture.store.snapshots.back().ship_ids;
    const auto before_revision = fixture.store.snapshots.back().revision;
    fixture.Query("game.listShips", "{\"limit\":1}");
    const auto cursor = fixture.store.cursors.back().id;
    const auto context_epoch = rpc::Bridge_Context_Epoch(&fixture.bridge);
    DOCTEST_REQUIRE(orion2re::lua::game::Advance_Ship_Generation(MAX_SHIPS - 1));
    fixture.native.ships[MAX_SHIPS - 1].owner = 7;
    fixture.native.ships[MAX_SHIPS - 1].turns_left = 99;
    if (fixture.foreign) {
        for (int index = 0; index < 10; ++index) {
            memset(fixture.native.ships[index].d.name, 'x', sizeof(fixture.native.ships[index].d.name));
        }
    }
    fixture.Print();
    DOCTEST_CHECK(fixture.Query() == before);
    DOCTEST_CHECK(rpc::Bridge_Context_Epoch(&fixture.bridge) == context_epoch);
    DOCTEST_SUBCASE("icon rebuild") {
        fixture.Refresh();
        DOCTEST_CHECK_FALSE(fixture.store.context_active);
        fixture.Print();
        DOCTEST_CHECK(fixture.Current_Etas() == 1);
        DOCTEST_CHECK(fixture.store.snapshots.back().ship_ids == before_ids);
        DOCTEST_CHECK(fixture.store.snapshots.back().revision == before_revision);
        DOCTEST_CHECK(fixture.Query("game.listShips", ("{\"cursor\":" + rpc::Json_String(cursor) + "}").c_str()).find("\"error\"") != std::string::npos);
    }
    DOCTEST_SUBCASE("resolution") {
        game_events::Publish(game_events::RESOLUTION_BEGIN);
        fixture.Print();
        DOCTEST_CHECK_FALSE(fixture.store.context_active);
    }
    DOCTEST_SUBCASE("network update") {
        game_events::Publish(game_events::WORLD_UPDATE_BEGIN);
        fixture.Print();
        DOCTEST_CHECK_FALSE(fixture.store.context_active);
    }
    DOCTEST_SUBCASE("load reset") {
        game_events::Publish(game_events::WORLD_REPLACE_BEGIN);
        fixture.Print();
        DOCTEST_CHECK_FALSE(fixture.store.context_active);
    }
    DOCTEST_SUBCASE("perspective") {
        game_events::Publish(game_events::PERSPECTIVE_SELECTION_BEGIN);
        fixture.Print();
        DOCTEST_CHECK_FALSE(fixture.store.context_active);
    }
}

DOCTEST_TEST_CASE_TEMPLATE("actual non-own retirement revokes native ETA pages and ready late and pending bridge reads", Fleet, std::true_type, std::integral_constant<int, 2>, std::integral_constant<int, 3>, std::integral_constant<int, 4>) {
    s_eta_world fixture(Fleet::value);
    game_events::s_fleet_box_view_scope owner;
    fixture.Refresh();
    owner.Initialized();
    fixture.Print();
    uint64_t context = 0;
    const auto result = fixture.Query("game.listShips", "{}", &context);
    DOCTEST_REQUIRE(context != 0);
    fixture.Query("game.listShips", "{\"limit\":1}");
    DOCTEST_REQUIRE(!fixture.store.cursors.empty());
    const auto cursor = fixture.store.cursors.back().id;
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    DOCTEST_REQUIRE(rpc::Authorize_Client(&fixture.bridge, 1, epoch));
    rpc::s_request request;
    request.method = "game.listShips";
    request.id_json = "1";
    request.params_json = "{}";
    DOCTEST_REQUIRE(rpc::Submit_Work(&fixture.bridge, 1, epoch, &request, UINT64_MAX) == 0);
    rpc::s_work late;
    DOCTEST_REQUIRE(rpc::Take_Work(&fixture.bridge, 0, &late));
    DOCTEST_REQUIRE(rpc::Submit_Work(&fixture.bridge, 1, epoch, &request, UINT64_MAX) == 0);
    rpc::Pump_Game_Queries(&fixture.store); // Ready, but not delivered.
    request.params_json = "{\"cursor\":" + rpc::Json_String(cursor) + "}";
    DOCTEST_REQUIRE(rpc::Submit_Work(&fixture.bridge, 1, epoch, &request, UINT64_MAX) == 0);
    request.params_json = "{}";
    DOCTEST_REQUIRE(rpc::Submit_Work(&fixture.bridge, 1, epoch, &request, UINT64_MAX) == 0);
    const int16_t slot = MOX::ship_node[MOX::fleet_movement_icon[0].ship_node_idx].ship_idx;
    SHIPS::Kill_Ship(slot);
    DOCTEST_CHECK_FALSE(fixture.store.context_active);
    DOCTEST_CHECK(rpc::Bridge_Context_Epoch(&fixture.bridge) != context);
    rpc::s_publication publication;
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_CHECK(publication.json == rpc::Error_Response(&request, rpc::RPC_OBJECT_UNAVAILABLE));
    DOCTEST_REQUIRE(rpc::Complete_Work(&fixture.bridge, &late, &result, context));
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_CHECK(publication.json == rpc::Error_Response(&request, rpc::RPC_OBJECT_UNAVAILABLE));
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_CHECK(publication.json == rpc::Error_Response(&request, rpc::RPC_OBJECT_UNAVAILABLE));
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_CHECK(publication.context_epoch == 0);
    DOCTEST_CHECK(publication.json.find("\"eta\":{\"availability\":\"current\"") == std::string::npos);
    if (fixture.contextual) DOCTEST_CHECK(publication.json.find("\"destination\":{\"availability\":\"current\"") == std::string::npos);
    uint64_t ambient = 99;
    fixture.Query("game.listShips", "{}", &ambient);
    DOCTEST_CHECK(ambient == 0);
    DOCTEST_CHECK(rpc::Client_Is_Current(&fixture.bridge, 1, epoch));
}

DOCTEST_TEST_CASE("ship icon rebuild invalidates transient node associations before reusing nodes") {
    s_ship_world fixture;
    struct s_rebuild_observer {
        std::vector<game_events::e_event> events;
        int16_t old_ship = -1;
        int16_t new_ship = -1;
        uint64_t token = game_events::Subscribe([](game_events::e_event event, void* context) {
            auto* observer = (s_rebuild_observer*)context;
            if (event == game_events::SHIP_ICONS_REBUILD_BEGIN) {
                observer->old_ship = MOX::ship_node[0].ship_idx;
                observer->events.push_back(event);
            } else if (event == game_events::SHIP_ICONS_REFRESHED) {
                observer->new_ship = MOX::ship_node[0].ship_idx;
                observer->events.push_back(event);
            } else if (event == game_events::FLEET_BOX_INFO_CLEARED) {
                observer->events.push_back(event);
            }
        },
            this);
        ~s_rebuild_observer() {
            game_events::Unsubscribe(token);
        }
    } observer;
    DOCTEST_REQUIRE(observer.token != 0);
    MOX::ship_node[0].ship_idx = MAX_SHIPS - 1;
    const uint32_t seed = game_random::Get_Random_Seed();
    SHIPS::Build_Ship_Icons();
    DOCTEST_CHECK(observer.old_ship == MAX_SHIPS - 1);
    DOCTEST_CHECK(observer.new_ship == 0);
    const std::vector<game_events::e_event> expected = {
        game_events::SHIP_ICONS_REBUILD_BEGIN, game_events::FLEET_BOX_INFO_CLEARED, game_events::SHIP_ICONS_REFRESHED};
    DOCTEST_CHECK(observer.events == expected);
    observer.events.clear();
    fixture.ships[0].status = 5;
    SHIPS::Build_Ship_Icons();
    DOCTEST_CHECK(observer.old_ship == 0);
    DOCTEST_CHECK(observer.new_ship == 1); // Reused node zero is not the old ship.
    DOCTEST_CHECK(observer.events == expected);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE("fresh ship reads ignore stale fleet selection scan and movement globals") {
    s_ship_world fixture;
    SHIPS::Build_Ship_Icons();
    rpc::s_map_observation baseline;
    DOCTEST_REQUIRE(game_rpc::Read_Refreshed_Ship_Observation(&baseline));
    const int16_t old_stack = MOX::fleet_box_ship_stack;
    const int16_t old_scan = MOX::fleet_ship_scanned;
    const int16_t old_eta = MOX::eta_star_id;
    const int16_t old_chosen = MAINSCR::fleet_icon_chosen;
    const s_ship_move_info old_movement = MOX::g_ship_move_info;
    MOX::fleet_box_ship_stack = MAX_SHIPS;
    MOX::fleet_ship_scanned = MAX_SHIPS;
    MAINSCR::fleet_icon_chosen = MAX_SHIPS;
    MOX::eta_star_id = MAX_STARS;
    MOX::g_ship_move_info.moving = 1;
    MOX::g_ship_move_info.turns_left = 99;
    rpc::s_map_observation changed;
    const bool valid = game_rpc::Read_Refreshed_Ship_Observation(&changed);
    MOX::fleet_box_ship_stack = old_stack;
    MOX::fleet_ship_scanned = old_scan;
    MOX::eta_star_id = old_eta;
    MAINSCR::fleet_icon_chosen = old_chosen;
    MOX::g_ship_move_info = old_movement;
    DOCTEST_REQUIRE(valid);
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &baseline));
    rpc::s_request request;
    request.method = "game.listShips";
    request.params_json = "{}";
    request.id_json = "1";
    const std::string response = rpc::Game_Query(&store, &request);
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &changed));
    DOCTEST_CHECK(rpc::Game_Query(&store, &request) == response);
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("own colony capture follows the native full-colony roster and copies only cached resident population") {
    s_world world;
    MOX::NUM_COLONIES = 4;
    world.colonies[0].planet = 0;
    world.colonies[0].n_pops = MAX_COLONY_POPULATION;
    world.planets[0].star_index = 0;
    world.planets[0].orbit = 2;
    world.planets[0].colony_index = 0;
    world.stars[0].planet_index[2] = 0;
    world.colonies[1].owner = 1;
    world.colonies[1].planet = MAX_PLANETS; // Hidden invalid links and population are never read.
    world.colonies[1].n_pops = 255;
    world.colonies[2].outpost_flag = 1;
    world.colonies[2].planet = -1;
    world.colonies[3].owner = -1;
    struct s_native_list {
        int16_t* old = COLXPORT::g_colony_list_ptr;
        std::vector<int16_t> entries = std::vector<int16_t>(MAX_COLONIES);
        s_native_list() {
            COLXPORT::g_colony_list_ptr = entries.data();
        }
        ~s_native_list() {
            COLXPORT::g_colony_list_ptr = old;
        }
    } list;
    COLXPORT::Build_Global_Colony_List();
    DOCTEST_REQUIRE(list.entries[0] == 0);
    DOCTEST_CHECK(list.entries[1] == -1);
    const auto before = world.colonies;
    const uint32_t seed = game_random::Get_Random_Seed();
    const auto count = COLONY::colonists_count;
    rpc::s_map_observation observation;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observation));
    DOCTEST_REQUIRE(observation.colonies_captured);
    DOCTEST_REQUIRE(observation.colonies.size() == 1);
    DOCTEST_CHECK(observation.colonies[0].system == 0);
    DOCTEST_CHECK(observation.colonies[0].orbit == 3);
    DOCTEST_CHECK(observation.colonies[0].population == MAX_COLONY_POPULATION);
    DOCTEST_CHECK(memcmp(before.data(), world.colonies.data(), before.size() * sizeof(s_colony)) == 0);
    DOCTEST_CHECK(COLONY::colonists_count == count);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    world.players[0].traits[TRAIT_OMNISCIENCE] = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observation));
    DOCTEST_CHECK(observation.colonies.size() == 1);
    world.colonies[0].n_pops = 0;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observation));
    DOCTEST_CHECK(observation.colonies[0].population == 0);
    world.colonies[0].n_pops = MAX_COLONY_POPULATION + 1;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&observation));
    world.colonies[0].n_pops = 1;
    world.planets[0].colony_index = 1;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&observation));
}

DOCTEST_TEST_CASE("owned operations project native producers without changing selection jobs queues messages or RNG") {
    s_review_globals globals;
    COLONY::colony_handle = -1;
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    test_native_colonies::Initialize(&store, &observed);
    DOCTEST_REQUIRE(test_native_colonies::Command("colony-native"));
    DOCTEST_REQUIRE(observed.colonies.size() == 3);
    const auto& operations = observed.colonies[0].operations;
    DOCTEST_REQUIRE(operations.available);
    DOCTEST_CHECK(operations.population_text == "Pop 3.250 M (-125 k/turn)");
    DOCTEST_CHECK(operations.heading == "Colony of Operations Prime");
    DOCTEST_CHECK(observed.colonies[2].operations.heading == "Annihilating Operations III");
    DOCTEST_CHECK(operations.workforce.size() == 3);
    DOCTEST_CHECK(operations.workforce[2].state == "android");
    DOCTEST_CHECK(operations.structures.size() == 2);
    DOCTEST_CHECK(operations.queue[3].kind == "refit");
    DOCTEST_CHECK(operations.queue[4].label == "Own design");
    DOCTEST_CHECK(operations.cost == COLCALC::Colony_Producing_Product_Cost(&MOX::colony[0]));
    DOCTEST_CHECK(operations.buy_cost == COLCALC::Production_Cost_To_Buy(30, 9));
    DOCTEST_CHECK(operations.turns == COLONY::Calculate_Current_Production_Turn_Count(0));
    DOCTEST_CHECK(operations.resources[0].shortage == 1);
    DOCTEST_CHECK(operations.resources[1].local == 12);
    DOCTEST_CHECK(operations.resources[1].secondary == 4);
    DOCTEST_CHECK(operations.morale_icons == -2);
    s_invasion_side_info garrison;
    invasion::Compute_Colony_Ground_Combat_Info(&MOX::colony[0], &garrison, false);
    DOCTEST_REQUIRE(operations.garrison_available);
    for (int i = 0; i < 3; ++i) {
        DOCTEST_CHECK(operations.garrison[i].count == garrison.unit_counts[i]);
        DOCTEST_CHECK(operations.garrison[i].strength == garrison.unit_strengths[i]);
        DOCTEST_CHECK(operations.garrison[i].hp == (uint8_t)garrison.unit_hps[i]);
    }
    DOCTEST_CHECK(observed.colonies[1].operations.inbound == 1);
    DOCTEST_CHECK(observed.colonies[1].operations.event == "Population Boom");
    DOCTEST_CHECK(observed.colonies[1].operations.autobuild == "Auto Build Queue 3");
    const std::vector<s_colony> colonies(MOX::colony, MOX::colony + MOX::NUM_COLONIES);
    const std::vector<s_MSG> messages(std::begin(MSG_::msgs), std::end(MSG_::msgs));
    const auto random = game_random::Get_Random_Seed();
    const auto selection = COLONY::colony_handle;
    const auto counted = COLONY::colonists_count;
    const auto scratch = std::string(COLBLDG::g_msg);
    for (int i = 0; i < 3; ++i) {
        rpc::s_map_observation reread;
        DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&reread));
        DOCTEST_CHECK(reread.colonies[0].operations == operations);
    }
    DOCTEST_CHECK(memcmp(colonies.data(), MOX::colony, colonies.size() * sizeof(s_colony)) == 0);
    DOCTEST_CHECK(memcmp(messages.data(), MSG_::msgs, messages.size() * sizeof(s_MSG)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == random);
    DOCTEST_CHECK(COLONY::colony_handle == selection);
    DOCTEST_CHECK(COLONY::colonists_count == counted);
    DOCTEST_CHECK(std::string(COLBLDG::g_msg) == scratch);
    colony_read::s_product invalid_storage;
    const auto ship_count = MOX::NUM_SHIPS;
    MOX::NUM_SHIPS = MAX_SHIPS + 1;
    colony_read::Product(&MOX::colony[0], COLBLDG::Colony_Production_From_Ship_Index(0), &invalid_storage);
    DOCTEST_CHECK_FALSE(invalid_storage.available);
    MOX::NUM_SHIPS = ship_count;
    const auto prime_offset = MOX::h_string_ix[0xa0];
    MOX::h_string_ix[0xa0] = (int16_t)MOX::h_strings.size();
    char heading[104];
    DOCTEST_CHECK_FALSE(colony_read::Heading(0, heading, sizeof(heading)));
    MOX::h_string_ix[0xa0] = prime_offset;
    char species_name[sizeof(MOX::player[2].race_name)];
    memcpy(species_name, MOX::player[2].race_name, sizeof(species_name));
    memset(MOX::player[2].race_name, 'X', sizeof(species_name));
    rpc::s_map_observation invalid_name;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&invalid_name));
    DOCTEST_CHECK_FALSE(invalid_name.colonies[2].operations.available);
    memcpy(MOX::player[2].race_name, species_name, sizeof(species_name));
    auto suppressed_estimate = observed;
    suppressed_estimate.colonies[0].operations.turns = 10000;
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &suppressed_estimate));
    const auto suppressed_revision = store.revision;
    suppressed_estimate.colonies[0].operations.turns = 20000;
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &suppressed_estimate));
    DOCTEST_CHECK(store.revision == suppressed_revision);
    DOCTEST_CHECK(store.snapshots.back().map.colonies[0].operations.turns == 0);
    {
        game_events::s_colony_player_scope display(0);
        MOX::PLAYER_NUM = 1;
        rpc::s_map_observation foreign;
        DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&foreign));
        MOX::PLAYER_NUM = 0;
        DOCTEST_CHECK(game_rpc::Read_Map_Observation(&foreign));
    }
    MOX::colony[0].producing[0] = BUILDING_BARRIER_SHIELD;
    TECHDATA::buildings[BUILDING_BARRIER_SHIELD].name = (char*)"Barrier Shield";
    TECHDATA::buildings[BUILDING_BARRIER_SHIELD].cost = 150;
    TECHDATA::buildings[BUILDING_FLUX_SHIELD].name = (char*)"Flux Shield";
    TECHDATA::buildings[BUILDING_FLUX_SHIELD].cost = 100;
    MOX::colony[0].buildings[BUILDING_FLUX_SHIELD] = 1;
    rpc::s_map_observation upgrade;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&upgrade));
    DOCTEST_CHECK(upgrade.colonies[0].operations.cost == 50);
    MOX::colony[0].morale = -127;
    bool suppressed = false;
    DOCTEST_CHECK(colony_read::Morale_Icons(&MOX::colony[0], &suppressed) == -63);
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&upgrade));
    DOCTEST_CHECK(upgrade.colonies[0].operations.morale_icons == -20);
    const int16_t events[] = {EVENT_COMET, EVENT_PLAGUE, EVENT_POPULATION_BOOM, EVENT_TIME_SPACE_ANOMALY, EVENT_PIRATE_ACTIVITY, EVENT_SUPERNOVA};
    for (const int16_t label : {0xf0, 0x19f, 0xce, 0xb6, 0x19e, 0x18e}) ESTRINGS::strings[label] = (char*)"Native event";
    for (auto event : events) {
        EVENTS::Init_Events();
        auto& data = EVENTS::event_data[event];
        data.target_id = 0;
        for (int status : {2, 4, 6}) {
            data.status = status;
            DOCTEST_CHECK(EVENTS::Colony_Has_Event(0) == event);
            DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&upgrade));
            DOCTEST_CHECK(upgrade.colonies[0].operations.event_present);
        }
        data.status = 1;
        DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&upgrade));
        DOCTEST_CHECK_FALSE(upgrade.colonies[0].operations.event_present);
    }
    EVENTS::Init_Events();
    for (int i = 0; i < 15; ++i) {
        DOCTEST_REQUIRE(test_native_colonies::Command("colony-product-" + std::to_string(i)));
        const auto& product = observed.colonies[0].operations;
        DOCTEST_CHECK(product.queue[0].available == (i < 13));
        if (i < 13) DOCTEST_CHECK(product.cost == COLCALC::Colony_Producing_Product_Cost(&MOX::colony[0]));
    }
    DOCTEST_REQUIRE(test_native_colonies::Command("colony-staged"));
    rpc::s_map_observation blocked;
    DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&blocked));
    DOCTEST_REQUIRE(test_native_colonies::Command("colony-restored"));
    DOCTEST_REQUIRE(test_native_colonies::Command("colony-unification"));
    DOCTEST_CHECK(observed.colonies[0].operations.morale_suppressed);
    DOCTEST_CHECK(observed.colonies[0].operations.morale_icons == 0);
    DOCTEST_REQUIRE(test_native_colonies::Command("colony-powered"));
    for (auto trait : {TRAIT_LOW_G_WORLD, TRAIT_HEAVY_G_WORLD}) {
        MOX::player[0].traits[TRAIT_LOW_G_WORLD] = 0;
        MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 0;
        MOX::player[0].traits[trait] = 1;
        DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&upgrade));
        invasion::Compute_Colony_Ground_Combat_Info(&MOX::colony[0], &garrison, false);
        DOCTEST_REQUIRE(upgrade.colonies[0].operations.garrison_available);
        for (int i = 0; i < 3; ++i) {
            DOCTEST_CHECK(upgrade.colonies[0].operations.garrison[i].strength == garrison.unit_strengths[i]);
            DOCTEST_CHECK(upgrade.colonies[0].operations.garrison[i].hp == (uint8_t)garrison.unit_hps[i]);
        }
    }
    test_native_colonies::Shutdown();
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("maximum own colony capture is bounded and oversized complete views require pagination") {
    s_world world;
    MOX::NUM_STARS = MAX_STARS;
    MOX::NUM_COLONIES = MAX_COLONIES;
    for (int index = 0; index < MAX_COLONIES; ++index) {
        world.colonies[index].planet = (int16_t)index;
        world.colonies[index].n_pops = MAX_COLONY_POPULATION;
        world.colonies[index].military[0] = 32767;
        world.colonies[index].military[1] = 32767;
        world.planets[index].star_index = (int16_t)(index / 5);
        world.planets[index].orbit = (int8_t)(index % 5);
        world.planets[index].colony_index = (int16_t)index;
        world.stars[index / 5].planet_index[index % 5] = (int16_t)index;
    }
    rpc::s_map_observation observation;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observation));
    DOCTEST_REQUIRE(observation.colonies.size() == MAX_COLONIES);
    DOCTEST_REQUIRE(observation.planets.size() == MAX_PLANETS);
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &observation));
    rpc::s_request request;
    request.method = "game.listColonies";
    request.id_json = "1";
    request.params_json = "{\"limit\":256}";
    const auto page = rpc::Game_Query(&store, &request);
    DOCTEST_CHECK(page.size() < 1048576);
    DOCTEST_CHECK(page.find("\"error\"") == std::string::npos);
    DOCTEST_CHECK(page.find("nextCursor") != std::string::npos);
    request.method = "game.listPlanets";
    const auto planets_page = rpc::Game_Query(&store, &request);
    DOCTEST_CHECK(planets_page.size() < 1048576);
    DOCTEST_CHECK(planets_page.find("\"error\"") == std::string::npos);
    DOCTEST_CHECK(planets_page.find("nextCursor") != std::string::npos);
    request.method = "game.getState";
    request.params_json = "{\"include\":[\"colonies\"]}";
    DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("-32005") != std::string::npos);
    request.params_json = "{\"include\":[\"systems\",\"planets\",\"colonies\"]}";
    DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("-32005") != std::string::npos);
    rpc::Shutdown_Game_Store(&store);
}

DOCTEST_TEST_CASE("own colony troop counts copy displayed cached units without militia strength or foreign intelligence") {
    s_world world;
    MOX::NUM_COLONIES = 3;
    world.colonies[0].planet = 0;
    world.planets[0].star_index = 0;
    world.planets[0].orbit = 0;
    world.planets[0].colony_index = 0;
    world.stars[0].planet_index[0] = 0;
    world.colonies[0].military[0] = 0;
    world.colonies[0].military[1] = 32767;
    world.colonies[0].ground_strength = -123;
    world.colonies[0].tank_roundoff = 99;
    world.colonies[1].owner = 1;
    world.colonies[1].planet = MAX_PLANETS;
    world.colonies[1].military[0] = world.colonies[1].military[1] = -1;
    world.colonies[2].outpost_flag = 1;
    world.colonies[2].planet = MAX_PLANETS;
    world.colonies[2].military[0] = world.colonies[2].military[1] = -1;
    world.players[0].traits[TRAIT_OMNISCIENCE] = 1;
    const auto before = world.colonies;
    const uint32_t seed = game_random::Get_Random_Seed();
    rpc::s_map_observation observed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_REQUIRE(observed.colonies.size() == 1);
    DOCTEST_CHECK(observed.colonies[0].infantry == 0);
    DOCTEST_CHECK(observed.colonies[0].armor == 32767);
    DOCTEST_CHECK(memcmp(before.data(), world.colonies.data(), before.size() * sizeof(s_colony)) == 0);
    world.colonies[0].military[0] = 32767;
    world.colonies[0].military[1] = 0;
    rpc::s_map_observation changed;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&changed));
    DOCTEST_CHECK(changed.colonies[0].infantry == 32767);
    DOCTEST_CHECK(changed.colonies[0].armor == 0);
    DOCTEST_CHECK(observed.colonies[0].infantry == 0);
    DOCTEST_CHECK(observed.colonies[0].armor == 32767);
    for (int type = 0; type < 2; ++type) {
        world.colonies[0].military[type] = -1;
        DOCTEST_CHECK_FALSE(game_rpc::Read_Map_Observation(&changed));
        world.colonies[0].military[type] = 0;
    }
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE("known war status follows the viewer treaty direction only for living current contacts") {
    s_world world;
    MOX::NUM_PLAYERS = 4;
    world.players[0].traits[TRAIT_OMNISCIENCE] = 1;
    world.players[0].contact[1] = 1;
    world.players[0].contact[2] = 1;
    world.players[2].eliminated = 1;
    world.players[0].treaty[0] = 6;
    world.players[0].treaty[2] = 6;
    world.players[0].treaty[3] = 6;
    world.players[1].treaty[0] = 0; // The foreign direction does not replace the native viewer's summary.
    const uint32_t seed = game_random::Get_Random_Seed();
    rpc::s_map_observation observation;
    for (int8_t treaty : {0, 3, 4, 5, 6}) {
        world.players[0].treaty[1] = treaty;
        const auto before = world.players;
        DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observation));
        DOCTEST_REQUIRE(observation.players.size() == 4);
        DOCTEST_CHECK_FALSE(observation.players[0].at_war_available);
        DOCTEST_CHECK(observation.players[1].at_war_available);
        DOCTEST_CHECK(observation.players[1].at_war == (treaty >= 4));
        DOCTEST_CHECK_FALSE(observation.players[2].at_war_available);
        DOCTEST_CHECK_FALSE(observation.players[3].at_war_available);
        DOCTEST_CHECK(memcmp(before.data(), world.players.data(), before.size() * sizeof(s_player)) == 0);
    }
    world.players[0].contact[1] = 0;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observation));
    DOCTEST_CHECK_FALSE(observation.players[1].at_war_available);
    DOCTEST_CHECK_FALSE(observation.players[1].at_war);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE("research treaty presence uses only the viewers flag for living current contacts") {
    s_world world;
    MOX::NUM_PLAYERS = 4;
    world.players[0].has_galactic_lore = 1;
    world.players[0].contact[1] = world.players[0].contact[2] = 1;
    world.players[2].eliminated = 1;
    world.players[0].research_treaty[0] = 1;
    world.players[0].research_treaty[2] = 1;
    world.players[0].research_treaty[3] = 1;
    world.players[1].research_treaty[0] = 1; // Reciprocal state never grants a treaty in the viewer's direction.
    const uint32_t seed = game_random::Get_Random_Seed();
    rpc::s_map_observation observed;
    for (int8_t flag : {0, 1, 2, -1}) {
        world.players[0].research_treaty[1] = flag;
        const auto before = world.players;
        DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
        DOCTEST_REQUIRE(observed.players.size() == 4);
        DOCTEST_CHECK_FALSE(observed.players[0].research_treaty_available);
        DOCTEST_CHECK(observed.players[1].research_treaty_available);
        DOCTEST_CHECK(observed.players[1].research_treaty == (flag != 0));
        DOCTEST_CHECK_FALSE(observed.players[2].research_treaty_available);
        DOCTEST_CHECK_FALSE(observed.players[3].research_treaty_available);
        DOCTEST_CHECK(memcmp(before.data(), world.players.data(), before.size() * sizeof(s_player)) == 0);
    }
    const auto copied = observed;
    world.players[0].research_treaty[1] = 0;
    world.players[0].current_research_agreement_level[1] = -32768;
    world.players[0].research_agreement_goal[1] = 32767;
    world.players[1].research_accumulated = 999999;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK_FALSE(observed.players[1].research_treaty);
    DOCTEST_CHECK(copied.players[1].research_treaty);
    world.players[0].contact[1] = 0;
    world.players[0].research_treaty[1] = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK_FALSE(observed.players[1].research_treaty_available);
    DOCTEST_CHECK_FALSE(observed.players[1].research_treaty);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE("trade treaty presence reads only the viewers directed flag after contact authorization") {
    s_world world;
    MOX::NUM_PLAYERS = 4;
    world.players[0].has_galactic_lore = 1;
    world.players[0].contact[1] = world.players[0].contact[2] = 1;
    world.players[2].eliminated = 1;
    world.players[0].trade_treaty[0] = world.players[0].trade_treaty[2] = world.players[0].trade_treaty[3] = 1;
    world.players[1].trade_treaty[0] = 1;
    world.players[0].research_treaty[1] = 1; // Neither another treaty nor the reciprocal flag authorizes presence.
    const uint32_t seed = game_random::Get_Random_Seed();
    rpc::s_map_observation observed;
    for (int8_t flag : {0, 1, 2, -1, -128, 127}) {
        world.players[0].trade_treaty[1] = flag;
        const auto before = world.players;
        DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
        DOCTEST_REQUIRE(observed.players.size() == 4);
        DOCTEST_CHECK_FALSE(observed.players[0].trade_treaty_available);
        DOCTEST_CHECK(observed.players[1].trade_treaty_available);
        DOCTEST_CHECK(observed.players[1].trade_treaty == (flag != 0));
        DOCTEST_CHECK_FALSE(observed.players[2].trade_treaty_available);
        DOCTEST_CHECK_FALSE(observed.players[3].trade_treaty_available);
        DOCTEST_CHECK(memcmp(before.data(), world.players.data(), before.size() * sizeof(s_player)) == 0);
    }
    const auto copied = observed;
    world.players[0].trade_treaty[1] = 0;
    world.players[0].current_trade_agreement_level[1] = -32768;
    world.players[0].trade_agreement_goal[1] = 32767;
    world.players[1].bc = 999999;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK_FALSE(observed.players[1].trade_treaty);
    DOCTEST_CHECK(copied.players[1].trade_treaty);
    world.players[0].contact[1] = 0;
    world.players[0].trade_treaty[1] = 1;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK_FALSE(observed.players[1].trade_treaty_available);
    DOCTEST_CHECK_FALSE(observed.players[1].trade_treaty);
    world.players[0].has_galactic_lore = 0;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&observed));
    DOCTEST_CHECK(std::none_of(observed.players.begin(), observed.players.end(), [](const rpc::s_player_observation& player) { return player.slot == 1; }));
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE("relations shared projection preserves native buckets signed levels tribute and own missions without mutations") {
    s_leader_world fixture;
    auto& players = fixture.world.ships.world.players;
    players[0].contact[1] = 1;
    const auto old_labels = std::vector<char*>(std::begin(MOX::treaty_labels), std::end(MOX::treaty_labels));
    char labels[7][30] = {"None", "Pact", "Alliance", "Peace", "War", "Final War", "PRIVATE"};
    const char* displayed_labels[] = {"None", "PACT", "ALLIANCE", "PEACE", "WAR", "FINAL WAR"};
    for (int i = 0; i < 7; ++i) MOX::treaty_labels[i] = labels[i];
    for (int i = 0; i < 17; ++i) snprintf(RACESCRN::relation_text[i], sizeof(RACESCRN::relation_text[i]), "Bucket %i", i);
    for (int value = -100; value <= 100; ++value) {
        players[0].relations[1] = (int8_t)value;
        int bucket = 0;
        while (RACESCRN::relation_name_table[bucket] < value + 100) ++bucket;
        relations_read::s_relation native;
        relations_read::Read_Relation(0, 1, &native);
        DOCTEST_CHECK(native.available);
        DOCTEST_CHECK(std::string(native.relationship) == "Bucket " + std::to_string(bucket));
        DOCTEST_CHECK(native.relationship_offset == RACESCRN::slider_remap_list[(value + 100) / 8] - 6);
    }
    for (int16_t treaty = 0; treaty <= 6; ++treaty) {
        diplomac::Start_Treaty(0, 1, treaty);
        relations_read::s_relation native;
        relations_read::Read_Relation(0, 1, &native);
        DOCTEST_CHECK(std::string(native.treaty) == displayed_labels[std::min<int16_t>(treaty, 5)]);
    }
    char long_treaty[200];
    memset(long_treaty, 'x', sizeof(long_treaty) - 1);
    long_treaty[199] = 0;
    MOX::treaty_labels[5] = long_treaty;
    relations_read::s_relation long_relation;
    relations_read::Read_Relation(0, 1, &long_relation);
    DOCTEST_CHECK(strlen(long_relation.treaty) == 199);
    DOCTEST_CHECK(long_relation.treaty[198] == 'X');
    MOX::treaty_labels[5] = labels[5];
    char empty_label[] = "";
    MOX::treaty_labels[5] = empty_label;
    char summary[200];
    relations_read::Copy_Treaty_Summary(0, 1, summary, sizeof(summary));
    DOCTEST_CHECK(std::string(summary) == "None");
    MOX::treaty_labels[5] = labels[5];
    diplomac::Start_Tribute_Treaty(1, 0, 1);
    players[0].research_treaty[1] = players[0].trade_treaty[1] = 1;
    players[0].ignoring = 2;
    for (int16_t level : {-32768, -1, 0, 32767}) {
        players[0].current_research_agreement_level[1] = level;
        players[0].current_trade_agreement_level[1] = level;
        for (int16_t mission = 0; mission < 4; ++mission) {
            BILL::Set_My_Spy_Number(1, mission == 0 ? 0 : 63);
            BILL::Set_My_Spy_Mission(1, (int8_t)mission);
            BILL::Set_My_Agent_Number(7);
            const auto before = players;
            const auto leaders_before = fixture.leaders;
            const uint32_t seed = game_random::Get_Random_Seed();
            const auto screen = MOX::current_screen;
            const auto fields_before = fields::Get_Field_Counter();
            rpc::s_map_observation map;
            DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&map));
            DOCTEST_REQUIRE(map.relations.size() == 1);
            const auto& native = map.relations[0];
            DOCTEST_CHECK(native.research_level == level);
            DOCTEST_CHECK(native.trade_level == level);
            DOCTEST_CHECK(native.ignoring);
            DOCTEST_CHECK(native.tribute_percent == 5);
            DOCTEST_CHECK_FALSE(native.giving);
            DOCTEST_CHECK(native.spies == (mission == 0 ? 0 : 63));
            DOCTEST_CHECK(native.mission == (mission < 3 ? mission : -1));
            int16_t spy_bonus, agent_bonus;
            spy::Compute_Spy_Bonuses(&players[0], &spy_bonus, &agent_bonus);
            DOCTEST_CHECK(map.intelligence.agents == 7);
            DOCTEST_CHECK(map.intelligence.spy_bonus == spy_bonus);
            DOCTEST_CHECK(map.intelligence.agent_bonus == agent_bonus);
            DOCTEST_CHECK(memcmp(before.data(), players.data(), before.size() * sizeof(s_player)) == 0);
            DOCTEST_CHECK(memcmp(leaders_before.data(), fixture.leaders.data(), fixture.leaders.size() * sizeof(s_leader_data)) == 0);
            DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
            DOCTEST_CHECK(MOX::current_screen == screen);
            DOCTEST_CHECK(fields::Get_Field_Counter() == fields_before);
        }
    }
    diplomac::Start_Tribute_Treaty(0, 1, 2);
    relations_read::s_relation native;
    relations_read::Read_Relation(0, 1, &native);
    DOCTEST_CHECK(native.giving);
    DOCTEST_CHECK(native.tribute_percent == 10);
    players[0].contact[1] = 0;
    players[0].traits[TRAIT_OMNISCIENCE] = 1;
    relations_read::Read_Relation(0, 1, &native);
    DOCTEST_CHECK_FALSE(native.available);
    DOCTEST_CHECK(native.research_level == 0);
    relations_read::s_assignment assignment;
    relations_read::Read_Assignment(0, 1, &assignment);
    DOCTEST_CHECK_FALSE(assignment.available);
    players[0].contact[1] = 1;
    players[1].eliminated = 1;
    relations_read::Read_Assignment(0, 1, &assignment);
    DOCTEST_CHECK_FALSE(assignment.available);
    std::copy(old_labels.begin(), old_labels.end(), MOX::treaty_labels);
}

DOCTEST_TEST_CASE("relations initialized recipient scopes copy observations and revoke contexts without destroying own intelligence") {
    s_leader_world fixture;
    auto& players = fixture.world.ships.world.players;
    MOX::NUM_PLAYERS = 3;
    players[0].contact[1] = 1;
    players[1].treaty[2] = 4;
    strlcpy(players[2].race_name, "Reported race", sizeof(players[2].race_name));
    BILL::Set_Their_Spy_Number(1, 0, 2);
    const auto old_format = TEXT_FORMAT::format[TEXT_FORMAT::ROW_RACE_SPIES];
    const auto old_noreport = DIP_SCRN::noreport;
    DIP_SCRN::noreport = 0;
    relations_read::s_disclosure disclosed;
    for (const char* format : {"", "literal", "$$SPIES$$"}) {
        strlcpy(TEXT_FORMAT::format[TEXT_FORMAT::ROW_RACE_SPIES].template_text, format, 256);
        DOCTEST_CHECK_FALSE(relations_read::Read_Overlay(0, 1, &disclosed));
    }
    strlcpy(TEXT_FORMAT::format[TEXT_FORMAT::ROW_RACE_SPIES].template_text, "$SPIES$", 256);
    DOCTEST_REQUIRE(relations_read::Read_Overlay(0, 1, &disclosed));
    DOCTEST_CHECK(disclosed.incoming == 2);
    BILL::Set_Their_Spy_Number(1, 0, 0);
    DOCTEST_CHECK_FALSE(relations_read::Read_Overlay(0, 1, &disclosed));
    DOCTEST_REQUIRE(relations_read::Read_Report(0, 1, &disclosed));
    DOCTEST_CHECK(disclosed.incoming_available);
    DOCTEST_CHECK(disclosed.incoming == 0);
    DOCTEST_CHECK(disclosed.count == 1);
    DOCTEST_CHECK(disclosed.objective_empty);
    const auto old_personality = MOX::personality_labels[0];
    const auto old_objective = MOX::objective_labels[0];
    char long_personality[128];
    memset(long_personality, 'P', sizeof(long_personality) - 1);
    long_personality[127] = 0;
    char objective[] = "Hidden suffix";
    MOX::personality_labels[0] = long_personality;
    MOX::objective_labels[0] = objective;
    players[1].objectives = 0;
    relations_read::s_disclosure clipped;
    DOCTEST_REQUIRE(relations_read::Read_Report(0, 1, &clipped));
    DOCTEST_CHECK_FALSE(clipped.objective_empty);
    DOCTEST_CHECK(clipped.objective[0] == 0);
    long_personality[123] = 0;
    DOCTEST_REQUIRE(relations_read::Read_Report(0, 1, &clipped));
    DOCTEST_CHECK(std::string(clipped.objective) == "Hid");
    MOX::personality_labels[0] = old_personality;
    MOX::objective_labels[0] = old_objective;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    DIP_SCRN::noreport = 1;
    relations_read::s_disclosure withheld;
    DOCTEST_CHECK_FALSE(relations_read::Read_Report(0, 1, &withheld));
    DIP_SCRN::noreport = 0;
    rpc::s_bridge bridge;
    rpc::s_game_store store;
    DOCTEST_REQUIRE(rpc::Initialize_Game_Store(&store, &bridge));
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&store));
    rpc::s_map_observation ambient;
    DOCTEST_REQUIRE(game_rpc::Read_Map_Observation(&ambient));
    DOCTEST_REQUIRE(rpc::Capture_Map(&store, &ambient));
    struct s_listener {
        rpc::s_game_store* store;
        rpc::s_map_observation* ambient;
        int calls = 0;
    } listener{&store, &ambient};
    const auto token = game_events::Subscribe([](game_events::e_event event, void* data) {
        auto* state = (s_listener*)data;
        if (event == game_events::RELATIONS_VIEW_CLEARED) rpc::Revoke_Game_Context(state->store);
        if (event == game_events::RELATIONS_VIEW_READY) {
            ++state->calls;
            DOCTEST_CHECK(game_rpc::Capture_Relations_View(state->store, state->ambient, game_events::Published_Relations_View()));
        }
    },
        &listener);
    DOCTEST_REQUIRE(token != 0);
    rpc::s_request request;
    request.id_json = "1";
    request.params_json = "{}";
    request.method = "game.getRelations";
    game_events::Publish_Relations_View(true, &disclosed, 1);
    DOCTEST_CHECK(listener.calls == 0);
    {
        game_events::s_relations_view_scope owner(true);
        game_events::Publish_Relations_View(true, &disclosed, 1);
        DOCTEST_CHECK(listener.calls == 0);
        game_events::Ready_Relations_View(true);
        game_events::Publish_Relations_View(true, &disclosed, 1);
        DOCTEST_CHECK(listener.calls == 1);
        uint64_t epoch = 0;
        const auto result = rpc::Game_Query(&store, &request, &epoch);
        DOCTEST_CHECK(epoch != 0);
        DOCTEST_CHECK(result.find("Reported race") != std::string::npos);
        DOCTEST_CHECK(result.find("\"incomingSpies\":{\"availability\":\"current\",\"value\":0}") != std::string::npos);
        const auto revision = store.revision;
        game_events::Publish_Relations_View(true, &disclosed, 1);
        DOCTEST_CHECK(store.revision == revision);
        uint64_t client_epoch = 0;
        DOCTEST_REQUIRE(rpc::Register_Client(&bridge, 91, &client_epoch));
        DOCTEST_REQUIRE(rpc::Authorize_Client(&bridge, 91, client_epoch));
        DOCTEST_REQUIRE(rpc::Submit_Work(&bridge, 91, client_epoch, &request, UINT64_MAX) == 0);
        rpc::s_work queued;
        DOCTEST_REQUIRE(rpc::Take_Work(&bridge, 0, &queued));
        DOCTEST_REQUIRE(rpc::Complete_Work(&bridge, &queued, &result, epoch));
        disclosed.entries[0].name[0] = 'X';
        DOCTEST_CHECK(rpc::Game_Query(&store, &request) == result);
        {
            game_events::s_relations_interruption_scope child;
            game_events::Ready_Relations_View(true);
            game_events::Publish_Relations_View(true, &disclosed, 1);
            DOCTEST_CHECK(listener.calls == 2);
            DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("Reported race") == std::string::npos);
            rpc::s_publication publication;
            DOCTEST_REQUIRE(rpc::Take_Publication(&bridge, &publication));
            DOCTEST_CHECK(publication.json.find("Reported race") == std::string::npos);
            DOCTEST_CHECK(publication.json.find("\"error\"") != std::string::npos);
        }
        game_events::Publish_Relations_View(true, &disclosed, 1);
        DOCTEST_CHECK(listener.calls == 2);
        game_events::Ready_Relations_View(true);
        game_events::Publish_Relations_View(true, &disclosed, 1);
        DOCTEST_CHECK(listener.calls == 3);
    }
    DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("eported race") == std::string::npos);
    request.method = "game.getIntelligence";
    DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("\"allocations\"") != std::string::npos);
    rpc::Pause_Game_Queries(&store);
    DOCTEST_CHECK(rpc::Game_Query(&store, &request).find("-32002") != std::string::npos);
    rpc::Resume_Game_Queries(&store);
    game_events::Unsubscribe(token);
    rpc::Shutdown_Game_Store(&store);
    TEXT_FORMAT::format[TEXT_FORMAT::ROW_RACE_SPIES] = old_format;
    DIP_SCRN::noreport = old_noreport;
}
