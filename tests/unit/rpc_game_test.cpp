#include "doctest/doctest.h"
#include <algorithm>
#include <string>
#include "rpc/rpc_game.h"

namespace {
    bool Id(std::string* id, void* context) {
        *id = "opaque-" + std::to_string(++*(uint64_t*)context);
        return true;
    }
    struct s_fixture {
        rpc::s_bridge bridge;
        rpc::s_game_store store;
        uint64_t serial = 0;
        s_fixture() {
            rpc::Initialize_Game_Store(&store, &bridge, Id, &serial);
        }
        ~s_fixture() {
            rpc::Shutdown_Game_Store(&store);
        }
    };
    rpc::s_request Request(const char* method, const char* params = "{}") {
        rpc::s_request request;
        request.method = method;
        request.id_json = "1";
        request.params_json = params;
        return request;
    }
    rpc::s_map_observation Map() {
        rpc::s_map_observation map;
        map.viewer = 0;
        map.turn = 35000;
        map.systems = {{10, 20, true, true, "Home"}, {30, 40, false, false, "hidden"}, {50, 60, true, true, ""}};
        map.nebulas = {{70, 80}};
        return map;
    }
    rpc::s_map_observation Ships() {
        auto map = Map();
        map.ships_captured = true;
        map.ships = {{0, 0, true, "Own"}, {30, 0, false, "hidden enemy name"}, {60, 0, false, "another hidden name"}};
        return map;
    }
    rpc::s_map_observation Players() {
        auto map = Map();
        map.players_captured = true;
        rpc::s_player_observation own;
        own.slot = 0;
        own.visible = true;
        own.name = "Local";
        own.treasury = -50;
        own.hud = {-3, -4, 10, -2, 7, -1};
        rpc::s_player_observation contact;
        contact.slot = 2;
        contact.visible = true;
        contact.contacted = true;
        contact.color = 2;
        contact.name = "Contact";
        contact.treasury = 1234567;
        contact.hud.food_surplus = 1000000; // Invalid if authorized; must be discarded before validation for foreign rows.
        rpc::s_player_observation hidden;
        hidden.slot = 3;
        hidden.name = "Hidden empire";
        map.players = {hidden, contact, own};
        return map;
    }
    rpc::s_map_observation Colonies() {
        auto map = Players();
        map.colonies_captured = true;
        map.colonies = {{0, 0, 1, 0}, {0, 0, 5, 42}, {2, 99999, -1, 99999}};
        return map;
    }
    std::string Dispatch(s_fixture* fixture, uint64_t client, uint64_t epoch, const rpc::s_request* request) {
        DOCTEST_REQUIRE(rpc::Submit_Work(&fixture->bridge, client, epoch, request, UINT64_MAX) == 0);
        rpc::Pump_Game_Queries(&fixture->store);
        rpc::s_publication result;
        DOCTEST_REQUIRE(rpc::Take_Publication(&fixture->bridge, &result));
        DOCTEST_CHECK(result.ticket != 0);
        return result.json;
    }
} // namespace

DOCTEST_TEST_CASE("game queries expose an authorized coherent capture rather than live raw world state") {
    s_fixture fixture;
    auto session = Request("game.getSession");
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &session).find("\"menu\"") != std::string::npos);
    rpc::Begin_Game_Reset(&fixture.store);
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Map();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto state = Request("game.getState", "{\"include\":[\"session\",\"systems\",\"nebulas\"]}");
    const std::string baseline = rpc::Game_Query(&fixture.store, &state);
    DOCTEST_CHECK(baseline.find("Home") != std::string::npos);
    DOCTEST_CHECK(baseline.find("hidden") == std::string::npos);
    DOCTEST_CHECK(baseline.find("last-coherent-capture") != std::string::npos);
    DOCTEST_CHECK(baseline.find("\"availability\":\"current\",\"value\":\"\"") != std::string::npos);
    map.systems[1].name = "different hidden world";
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) == baseline);
    DOCTEST_CHECK(fixture.store.revision == 1);
    rpc::Pause_Game_Queries(&fixture.store);
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state).find("-32002") != std::string::npos);
    const std::string busy_session = rpc::Game_Query(&fixture.store, &session);
    DOCTEST_CHECK(busy_session.find("resolving") != std::string::npos);
    DOCTEST_CHECK(busy_session.find("35000") != std::string::npos);
    rpc::Resume_Game_Queries(&fixture.store);
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) == baseline);
}

DOCTEST_TEST_CASE("perspective and load boundaries revoke handles requests cursors and old publications") {
    s_fixture fixture;
    rpc::Begin_Game_Reset(&fixture.store);
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Map();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    const std::string game = fixture.store.game_id;
    const std::string perspective = fixture.store.perspective_id;
    const std::string entity = fixture.store.system_ids[0];
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    auto request = Request("game.getSession");
    DOCTEST_REQUIRE(rpc::Submit_Work(&fixture.bridge, 1, epoch, &request, UINT64_MAX) == 0);
    rpc::s_work work;
    DOCTEST_REQUIRE(rpc::Take_Work(&fixture.bridge, 0, &work));
    rpc::Revoke_Game_View(&fixture.store);
    map.viewer = 1;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.game_id == game);
    DOCTEST_CHECK(fixture.store.perspective_id != perspective);
    DOCTEST_CHECK_FALSE(rpc::Client_Is_Current(&fixture.bridge, 1, epoch));
    const std::string old = "old world";
    DOCTEST_CHECK_FALSE(rpc::Complete_Work(&fixture.bridge, &work, &old));
    auto lookup = Request("game.getSystem");
    lookup.params_json = "{\"id\":" + rpc::Json_String(entity) + "}";
    const std::string unavailable = rpc::Game_Query(&fixture.store, &lookup);
    lookup.params_json = "{\"id\":\"nonexistent\"}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &lookup) == unavailable);
    rpc::Begin_Game_Reset(&fixture.store);
    DOCTEST_CHECK(fixture.store.snapshots.empty());
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &request).find("unavailable") != std::string::npos);
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.game_id != game);
}

DOCTEST_TEST_CASE("pagination pins a retained immutable snapshot and never silently mixes captures") {
    s_fixture fixture;
    rpc::Begin_Game_Reset(&fixture.store);
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Map();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto list = Request("game.listSystems", "{\"limit\":1}");
    const std::string first = rpc::Game_Query(&fixture.store, &list);
    DOCTEST_REQUIRE(fixture.store.cursors.size() == 1);
    const std::string cursor = fixture.store.cursors[0].id;
    map.turn++;
    map.systems[1].x = 99;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    list.params_json = "{\"cursor\":" + rpc::Json_String(cursor) + "}";
    const std::string second = rpc::Game_Query(&fixture.store, &list);
    DOCTEST_CHECK(first.find("35000") != std::string::npos);
    DOCTEST_CHECK(second.find("35000") != std::string::npos);
    DOCTEST_CHECK(second.find("\"x\":30") != std::string::npos);
    for (int index = 0; index < 4; ++index) {
        ++map.turn;
        DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    }
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("-32004") != std::string::npos);
}

DOCTEST_TEST_CASE("subscriptions establish an atomic baseline and invalidate only selected public projections") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Map();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    auto subscribe = Request("rpc.subscribe", "{\"topics\":[\"systems\"],\"events\":[\"state.changed\"]}");
    const std::string baseline = Dispatch(&fixture, 1, epoch, &subscribe);
    DOCTEST_CHECK(baseline.find("\"cursor\":\"0\"") != std::string::npos);
    DOCTEST_CHECK(baseline.find("Home") != std::string::npos);
    DOCTEST_REQUIRE(fixture.store.subscriptions.size() == 1);
    const std::string id = fixture.store.subscriptions.front().id;
    rpc::s_publication publication;
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    map.systems[1].name = "another hidden name";
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    ++map.turn;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    map.systems[0].name = "New name";
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    rpc::Pause_Game_Queries(&fixture.store);
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    rpc::Resume_Game_Queries(&fixture.store);
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_CHECK(publication.ticket == 0);
    DOCTEST_CHECK(publication.json.find("state.changed") != std::string::npos);
    DOCTEST_CHECK(publication.json.find("\"cursor\":\"1\"") != std::string::npos);
    DOCTEST_CHECK(publication.json.find(id) != std::string::npos);
    auto unsubscribe = Request("rpc.unsubscribe");
    unsubscribe.params_json = "{\"subscriptionId\":" + rpc::Json_String(id) + "}";
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &unsubscribe).find("\"removed\":true") != std::string::npos);
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &unsubscribe).find("\"removed\":false") != std::string::npos);
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK_FALSE(rpc::Needs_Game_Capture(&fixture.store));
}

DOCTEST_TEST_CASE("subscription ownership bounds reconnect and revocation fail closed") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Map();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 2, &epoch));
    auto subscribe = Request("rpc.subscribe", "{\"topics\":[\"session\"],\"events\":[\"state.changed\"]}");
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &subscribe).find("baseline") != std::string::npos);
    const std::string id = fixture.store.subscriptions.front().id;
    auto unsubscribe = Request("rpc.unsubscribe");
    unsubscribe.params_json = "{\"subscriptionId\":" + rpc::Json_String(id) + "}";
    DOCTEST_CHECK(Dispatch(&fixture, 2, epoch, &unsubscribe).find("\"removed\":false") != std::string::npos);
    for (int index = 1; index < 16; ++index) {
        DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &subscribe).find("baseline") != std::string::npos);
    }
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &subscribe).find("-32005") != std::string::npos);
    rpc::Cancel_Client(&fixture.bridge, 1);
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK_FALSE(rpc::Needs_Game_Capture(&fixture.store));
    DOCTEST_CHECK(fixture.store.subscriptions.empty());
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 3, &epoch));
    DOCTEST_CHECK(Dispatch(&fixture, 3, epoch, &subscribe).find("\"cursor\":\"0\"") != std::string::npos);
    DOCTEST_CHECK(fixture.store.subscriptions.front().id != id);
    rpc::Revoke_Game_View(&fixture.store);
    DOCTEST_CHECK(fixture.store.subscriptions.empty());
    DOCTEST_CHECK_FALSE(rpc::Client_Is_Current(&fixture.bridge, 3, epoch));
}

DOCTEST_TEST_CASE("turn notifications wait for coherent committed captures and remain ordered and client scoped") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Map();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    DOCTEST_REQUIRE(rpc::Authorize_Client(&fixture.bridge, 1, epoch));
    auto subscribe = Request("rpc.subscribe", "{\"topics\":[\"session\"],\"events\":[\"game.turnAdvanced\"]}");
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &subscribe).find("baseline") != std::string::npos);
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &subscribe).find("baseline") != std::string::npos);
    rpc::s_publication publication;
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    rpc::Pause_Game_Queries(&fixture.store);
    ++map.turn;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    rpc::Resume_Game_Queries(&fixture.store);
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_CHECK(publication.json.find("game.turnAdvanced") != std::string::npos);
    DOCTEST_CHECK(publication.json.find("35001") != std::string::npos);
    DOCTEST_CHECK(publication.state_key.empty());
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    auto session = Request("game.getSession");
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &session).find("35001") != std::string::npos);
    map.systems[0].name = "Renamed";
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    for (int index = 0; index < 2; ++index) {
        ++map.turn;
        DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
        rpc::Pump_Game_Queries(&fixture.store);
    }
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_CHECK(publication.json.find("35002") != std::string::npos);
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_CHECK(publication.json.find("35003") != std::string::npos);
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    rpc::Revoke_Game_View(&fixture.store);
    DOCTEST_CHECK(fixture.store.subscriptions.empty());
}

DOCTEST_TEST_CASE("default turn notifications require hello authorization but no explicit subscription") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Map();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 2, &epoch));
    DOCTEST_REQUIRE(rpc::Authorize_Client(&fixture.bridge, 1, epoch));
    ++map.turn;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    rpc::Pump_Game_Queries(&fixture.store);
    rpc::s_publication publication;
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &publication));
    DOCTEST_CHECK(publication.client == 1);
    DOCTEST_CHECK(publication.json.find("35001") != std::string::npos);
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
    rpc::Cancel_Client(&fixture.bridge, 1);
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_CHECK(fixture.store.turn_observers.empty());
    DOCTEST_REQUIRE(rpc::Authorize_Client(&fixture.bridge, 2, epoch));
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &publication));
}

DOCTEST_TEST_CASE("ship reads use authorized sorted copies and do not mistake an absent capture for known empty") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Map();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto list = Request("game.listShips");
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("-32002") != std::string::npos);
    map = Ships();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    const std::string baseline = rpc::Game_Query(&fixture.store, &list);
    DOCTEST_CHECK(baseline.find("last-native-ship-refresh") != std::string::npos);
    DOCTEST_CHECK(baseline.find("Own") != std::string::npos);
    DOCTEST_CHECK(baseline.find("hidden") == std::string::npos);
    DOCTEST_CHECK(baseline.find("\"owned\":false") != std::string::npos);
    std::reverse(map.ships.begin(), map.ships.end());
    map.ships[0].name = "different secret";
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list) == baseline);
    rpc::Pause_Game_Queries(&fixture.store);
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("-32002") != std::string::npos);
    rpc::Resume_Game_Queries(&fixture.store);
    map.ships.clear();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("\"items\":[]") != std::string::npos);
}

DOCTEST_TEST_CASE("combined ship state never mixes an older native refresh with newer map or session fields") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto full = Ships();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &full));
    auto state = Request("game.getState", "{\"include\":[\"ships\",\"session\",\"systems\"]}");
    const std::string baseline = rpc::Game_Query(&fixture.store, &state);
    auto map = Map();
    ++map.turn;
    map.systems[0].name = "Later map";
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) == baseline);
    auto session = Request("game.getSession");
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &session).find("35001") != std::string::npos);
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    auto subscribe = Request("rpc.subscribe", "{\"topics\":[\"ships\",\"session\"],\"events\":[\"state.changed\"]}");
    const std::string subscribed = Dispatch(&fixture, 1, epoch, &subscribe);
    DOCTEST_CHECK(subscribed.find("35000") != std::string::npos);
    DOCTEST_CHECK(subscribed.find("Later map") == std::string::npos);
    full.turn = map.turn;
    full.systems[0].name = map.systems[0].name;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &full));
    rpc::Pump_Game_Queries(&fixture.store);
    rpc::s_publication event;
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &event));
    DOCTEST_CHECK(event.json.find("state.changed") != std::string::npos);
    DOCTEST_CHECK(event.json.find("35001") != std::string::npos);
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state).find("Later map") != std::string::npos);
}

DOCTEST_TEST_CASE("ship handles are projection scoped and pagination stays pinned through later refreshes") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Ships();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    const std::string id = fixture.store.snapshots.back().ship_ids[0];
    auto get = Request("game.getShip");
    get.params_json = "{\"id\":" + rpc::Json_String(id) + "}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &get).find("Own") != std::string::npos);
    auto list = Request("game.listShips", "{\"limit\":1}");
    const std::string first = rpc::Game_Query(&fixture.store, &list);
    DOCTEST_REQUIRE(fixture.store.cursors.size() == 1);
    const std::string cursor = fixture.store.cursors.front().id;
    map.ships[0].x = 99;
    ++map.turn;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    const std::string unavailable = rpc::Game_Query(&fixture.store, &get);
    get.params_json = "{\"id\":\"never-existed\"}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &get) == unavailable);
    DOCTEST_CHECK(unavailable.find("-32004") != std::string::npos);
    list.params_json = "{\"cursor\":" + rpc::Json_String(cursor) + "}";
    const std::string second = rpc::Game_Query(&fixture.store, &list);
    DOCTEST_CHECK(first.find("35000") != std::string::npos);
    DOCTEST_CHECK(second.find("35000") != std::string::npos);
    DOCTEST_CHECK(second.find("\"x\":30") != std::string::npos);
    map.ships_captured = false;
    for (int index = 0; index < 4; ++index) {
        ++map.turn;
        DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    }
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("-32004") != std::string::npos);
}

DOCTEST_TEST_CASE("ship subscriptions ignore hidden names and unselected map changes and revoke with the view") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Ships();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    auto subscribe = Request("rpc.subscribe", "{\"topics\":[\"ships\"],\"events\":[\"state.changed\"]}");
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &subscribe).find("Own") != std::string::npos);
    map.systems[0].name = "Renamed star";
    map.ships[1].name = "secret change";
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    rpc::Pump_Game_Queries(&fixture.store);
    rpc::s_publication event;
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &event));
    map.ships[1].x = 31;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &event));
    DOCTEST_CHECK(event.json.find("state.changed") != std::string::npos);
    DOCTEST_CHECK(event.json.find("secret") == std::string::npos);
    rpc::Revoke_Game_View(&fixture.store);
    DOCTEST_CHECK(fixture.store.snapshots.empty());
    DOCTEST_CHECK(fixture.store.subscriptions.empty());
    DOCTEST_CHECK_FALSE(rpc::Client_Is_Current(&fixture.bridge, 1, epoch));
}

DOCTEST_TEST_CASE("maximum ship collections paginate within bounds and oversized full baselines return explicit limits") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Ships();
    map.ships.resize(10000, {32767, -32768, true, std::string(45, 'x')});
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto state = Request("game.getState", "{\"include\":[\"ships\"]}");
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state).find("-32005") != std::string::npos);
    auto list = Request("game.listShips", "{\"limit\":256}");
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).size() < 1048576);
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    auto subscribe = Request("rpc.subscribe", "{\"topics\":[\"ships\"],\"events\":[\"state.changed\"]}");
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &subscribe).find("-32005") != std::string::npos);
    DOCTEST_CHECK(fixture.store.subscriptions.empty());
    DOCTEST_CHECK(rpc::Client_Is_Current(&fixture.bridge, 1, epoch));
}

DOCTEST_TEST_CASE("player IDs and revisions depend only on authorized roster and own HUD state") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Players();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto list = Request("game.listPlayers");
    const std::string baseline = rpc::Game_Query(&fixture.store, &list);
    DOCTEST_CHECK(baseline.find("Local") != std::string::npos);
    DOCTEST_CHECK(baseline.find("Contact") != std::string::npos);
    DOCTEST_CHECK(baseline.find("Hidden") == std::string::npos);
    DOCTEST_CHECK(baseline.find("1234567") == std::string::npos);
    DOCTEST_CHECK(baseline.find("\"value\":-50") != std::string::npos);
    DOCTEST_CHECK(baseline.find("\"foodSurplus\":-4") != std::string::npos);
    DOCTEST_CHECK(baseline.find("\"commandPointsAvailable\":-1") != std::string::npos);
    const auto own_id = fixture.store.snapshots.back().player_ids[0];
    const auto contact_id = fixture.store.snapshots.back().player_ids[1];
    map.players[0].name = std::string(500, 'x');
    map.players[1].treasury = -99999;
    std::reverse(map.players.begin(), map.players.end());
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list) == baseline);
    map.players[0].treasury = -51;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.snapshots.back().player_ids[0] == own_id);
    DOCTEST_CHECK(fixture.store.snapshots.back().player_ids[1] == contact_id);
    auto get = Request("game.getPlayer");
    get.params_json = "{\"id\":" + rpc::Json_String(contact_id) + "}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &get).find("Contact") != std::string::npos);
    map.players[1].visible = false;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    const std::string unavailable = rpc::Game_Query(&fixture.store, &get);
    get.params_json = "{\"id\":\"never-known\"}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &get) == unavailable);
    DOCTEST_CHECK(unavailable.find("-32004") != std::string::npos);
    DOCTEST_CHECK(fixture.store.player_handles.size() == 2);
    map.players[1].visible = true;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.snapshots.back().player_ids[1] == contact_id);
    get.params_json = "{\"id\":" + rpc::Json_String(contact_id) + "}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &get).find("Contact") != std::string::npos);
    rpc::Pause_Game_Queries(&fixture.store);
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("-32002") != std::string::npos);
    rpc::Resume_Game_Queries(&fixture.store);
}

DOCTEST_TEST_CASE("player pages baselines and combined ship state remain coherent and revoke on perspective change") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Players();
    map.ships_captured = true;
    map.ships = Ships().ships;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto state = Request("game.getState", "{\"include\":[\"players\",\"ships\",\"session\"]}");
    const std::string baseline = rpc::Game_Query(&fixture.store, &state);
    auto list = Request("game.listPlayers", "{\"limit\":1}");
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("nextCursor") != std::string::npos);
    const std::string cursor = fixture.store.cursors.front().id;
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    auto subscribe = Request("rpc.subscribe", "{\"topics\":[\"players\"],\"events\":[\"state.changed\"]}");
    DOCTEST_CHECK(Dispatch(&fixture, 1, epoch, &subscribe).find("Local") != std::string::npos);
    map.ships_captured = false;
    ++map.turn;
    map.players[2].treasury = 77;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) == baseline);
    rpc::Pump_Game_Queries(&fixture.store);
    rpc::s_publication event;
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &event));
    DOCTEST_CHECK(event.json.find("state.changed") != std::string::npos);
    list.params_json = "{\"cursor\":" + rpc::Json_String(cursor) + "}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("35000") != std::string::npos);
    auto wrong_kind = Request("game.listSystems");
    wrong_kind.params_json = list.params_json;
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &wrong_kind).find("-32004") != std::string::npos);
    for (int index = 0; index < 4; ++index) {
        ++map.turn;
        DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    }
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("-32004") != std::string::npos);
    rpc::Revoke_Game_View(&fixture.store);
    DOCTEST_CHECK(fixture.store.player_handles.empty());
    DOCTEST_CHECK(fixture.store.snapshots.empty());
    DOCTEST_CHECK_FALSE(rpc::Client_Is_Current(&fixture.bridge, 1, epoch));
}

DOCTEST_TEST_CASE("player capture requires a valid local record and rejects duplicate public bindings") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Map();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto list = Request("game.listPlayers");
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("-32002") != std::string::npos);
    map = Players();
    map.players.back().visible = false;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map = Players();
    map.players[0] = map.players[1];
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map = Players();
    map.players.back().hud.income = 32768;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
}

DOCTEST_TEST_CASE("ship owners resolve only through the same capture's visible player roster") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Players();
    map.ships_captured = true;
    map.ships = Ships().ships;
    map.ships[1].owner_slot = 2;
    map.ships[2].owner_slot = 3; // Hidden empire, not an authorized parent.
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    const auto& snapshot = fixture.store.snapshots.back();
    DOCTEST_CHECK(snapshot.map.ships[0].owner_id == snapshot.player_ids[0]);
    DOCTEST_CHECK(snapshot.map.ships[1].owner_id == snapshot.player_ids[1]);
    DOCTEST_CHECK(snapshot.map.ships[2].owner_id.empty());
    DOCTEST_CHECK(snapshot.map.ships[1].owner_slot == -1);
    auto list = Request("game.listShips");
    const std::string baseline = rpc::Game_Query(&fixture.store, &list);
    map.ships[2].owner_slot = 7; // Another unlisted identity must not alter public IDs/revisions.
    map.ships[2].owner_id = "forged-parent";
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list) == baseline);
    const auto ship_ids = fixture.store.snapshots.back().ship_ids;
    DOCTEST_REQUIRE(map.players[2].slot == map.viewer);
    map.players[2].treasury++;
    --map.players[2].hud.food_surplus;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.snapshots.back().ship_ids == ship_ids);
    map.players[1].color = map.players[2].color; // Public ambiguity is not resolved using hidden slots.
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.snapshots.back().map.ships[1].owner_id.empty());
    DOCTEST_CHECK_FALSE(fixture.store.snapshots.back().map.ships[0].owner_id.empty());
}

DOCTEST_TEST_CASE("a newer roster never retroactively discloses ownership in an older ship capture") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Players();
    map.ships_captured = true;
    map.ships = Ships().ships;
    map.ships[1].owner_slot = 2;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto state = Request("game.getState", "{\"include\":[\"ships\",\"players\"]}");
    const std::string baseline = rpc::Game_Query(&fixture.store, &state);
    map.ships_captured = false;
    map.players[1].visible = false;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) == baseline);
    map.ships_captured = true;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.snapshots.back().map.ships[1].owner_id.empty());
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) != baseline);
}

DOCTEST_TEST_CASE("ship routes discard unavailable values before identity equality subscriptions and pagination") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Ships();
    map.ships[0].destination_available = rpc::SHIP_CURRENT;
    map.ships[0].destination_system = 1;
    map.ships[0].eta_available = rpc::SHIP_CURRENT;
    map.ships[0].eta = 255;
    map.ships[1].destination_available = map.ships[1].eta_available = rpc::SHIP_CONTEXT_REQUIRED;
    map.ships[2].owned = true;
    map.ships[2].name = "Stationary";
    map.ships[2].destination_available = map.ships[2].eta_available = rpc::SHIP_KNOWN_EMPTY;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    const auto& snapshot = fixture.store.snapshots.back();
    DOCTEST_CHECK(snapshot.map.ships[0].destination_system == -1);
    DOCTEST_CHECK(snapshot.map.ships[0].destination_id == fixture.store.system_ids[1]);
    auto state = Request("game.getState", "{\"include\":[\"session\",\"systems\",\"ships\"]}");
    const std::string baseline = rpc::Game_Query(&fixture.store, &state);
    DOCTEST_CHECK(baseline.find("\"destination\":{\"availability\":\"current\",\"value\":" + rpc::Json_String(fixture.store.system_ids[1])) != std::string::npos);
    DOCTEST_CHECK(baseline.find("\"eta\":{\"availability\":\"current\",\"value\":255}") != std::string::npos);
    DOCTEST_CHECK(baseline.find("known-empty") != std::string::npos);
    const auto ids = snapshot.ship_ids;
    auto list = Request("game.listShips", "{\"limit\":1}");
    rpc::Game_Query(&fixture.store, &list);
    const auto cursor = fixture.store.cursors.front().id;
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    auto subscribe = Request("rpc.subscribe", "{\"topics\":[\"ships\"],\"events\":[\"state.changed\"]}");
    Dispatch(&fixture, 1, epoch, &subscribe);
    map.ships[1].destination_system = 999999;
    map.ships[1].destination_id = "forged hidden system";
    map.ships[1].eta = -999999;
    map.ships[2].destination_system = -999999;
    map.ships[2].eta = 999999;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) == baseline);
    DOCTEST_CHECK(fixture.store.revision == 1);
    DOCTEST_CHECK(fixture.store.snapshots.back().ship_ids == ids);
    rpc::Pump_Game_Queries(&fixture.store);
    rpc::s_publication event;
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &event));
    map.ships[0].eta = 0;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.snapshots.back().ship_ids != ids);
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &event));
    DOCTEST_CHECK(event.json.find("state.changed") != std::string::npos);
    list.params_json = "{\"cursor\":" + rpc::Json_String(cursor) + "}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("\"revision\":\"1\"") != std::string::npos);
    auto lookup = Request("game.getShip");
    lookup.params_json = "{\"id\":" + rpc::Json_String(ids[0]) + "}";
    const auto stale = rpc::Game_Query(&fixture.store, &lookup);
    lookup.params_json = "{\"id\":\"unknown\"}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &lookup) == stale);
    const std::string current = rpc::Game_Query(&fixture.store, &state);
    map.ships_captured = false;
    map.systems[1].visited = map.systems[1].name_available = true;
    map.systems[1].name = "Newly visited";
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) == current);
}

DOCTEST_TEST_CASE("ship route validation rejects invalid authorized destinations and cached ETA without changing captures") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Ships();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto list = Request("game.listShips");
    const std::string baseline = rpc::Game_Query(&fixture.store, &list);
    map.ships[0].destination_available = rpc::SHIP_CURRENT;
    map.ships[0].destination_system = 3;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map.ships[0].destination_system = -1;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map.ships[0].destination_system = 0;
    map.ships[0].eta_available = rpc::SHIP_CURRENT;
    map.ships[0].eta = 256;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map.ships[0].eta = -1;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map.ships[0].eta = 0;
    map.ships[0].destination_available = rpc::SHIP_UNAUDITED;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map.ships[0].destination_available = map.ships[0].eta_available = rpc::SHIP_KNOWN_EMPTY;
    map.ships[0].owned = false;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list) == baseline);
}

DOCTEST_TEST_CASE("own colonies filter foreign payloads before validation handles revisions and subscriptions") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Colonies();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    const auto& snapshot = fixture.store.snapshots.back();
    DOCTEST_REQUIRE(snapshot.map.colonies.size() == 2);
    DOCTEST_CHECK(snapshot.map.colonies[0].system == -1);
    DOCTEST_CHECK(snapshot.map.colonies[0].owner_slot == -1);
    DOCTEST_CHECK(snapshot.map.colonies[0].system_id == fixture.store.system_ids[0]);
    DOCTEST_CHECK(snapshot.map.colonies[0].owner_id == snapshot.player_ids[0]);
    auto state = Request("game.getState", "{\"include\":[\"session\",\"systems\",\"players\",\"colonies\"]}");
    const auto baseline = rpc::Game_Query(&fixture.store, &state);
    DOCTEST_CHECK(baseline.find("\"population\":{\"availability\":\"current\",\"value\":0}") != std::string::npos);
    DOCTEST_CHECK(baseline.find("\"planetId\":{\"availability\":\"unaudited\"}") != std::string::npos);
    const auto ids = snapshot.colony_ids;
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&fixture.bridge, 1, &epoch));
    auto subscribe = Request("rpc.subscribe", "{\"topics\":[\"colonies\"],\"events\":[\"state.changed\"]}");
    Dispatch(&fixture, 1, epoch, &subscribe);
    map.colonies.back().system = -99999;
    map.colonies.back().system_id = "hidden-parent";
    map.colonies.back().owner_id = "forged-owner";
    map.colonies.back().population = -99999;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) == baseline);
    rpc::Pump_Game_Queries(&fixture.store);
    rpc::s_publication event;
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&fixture.bridge, &event));
    std::reverse(map.colonies.begin(), map.colonies.end());
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.snapshots.back().colony_ids == ids);
    ++map.players.back().treasury;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.snapshots.back().colony_ids == ids);
    auto list = Request("game.listColonies", "{\"limit\":1}");
    rpc::Game_Query(&fixture.store, &list);
    const auto cursor = fixture.store.cursors.back().id;
    ++map.colonies.back().population;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(fixture.store.snapshots.back().colony_ids != ids);
    rpc::Pump_Game_Queries(&fixture.store);
    DOCTEST_REQUIRE(rpc::Take_Publication(&fixture.bridge, &event));
    DOCTEST_CHECK(event.json.find("state.changed") != std::string::npos);
    list.params_json = "{\"cursor\":" + rpc::Json_String(cursor) + "}";
    const auto pinned = rpc::Game_Query(&fixture.store, &list);
    DOCTEST_CHECK(pinned.find("\"population\":{\"availability\":\"current\",\"value\":42}") != std::string::npos);
    auto lookup = Request("game.getColony");
    lookup.params_json = "{\"id\":" + rpc::Json_String(ids[0]) + "}";
    const auto stale = rpc::Game_Query(&fixture.store, &lookup);
    lookup.params_json = "{\"id\":\"unknown\"}";
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &lookup) == stale);
    rpc::Revoke_Game_View(&fixture.store);
    DOCTEST_CHECK(fixture.store.snapshots.empty());
    DOCTEST_CHECK_FALSE(rpc::Client_Is_Current(&fixture.bridge, 1, epoch));
}

DOCTEST_TEST_CASE("colonies share one ship/map/player capture and distinguish busy from an own known-empty roster") {
    s_fixture fixture;
    DOCTEST_REQUIRE(rpc::Commit_Game_Reset(&fixture.store));
    auto map = Players();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto list = Request("game.listColonies");
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("-32002") != std::string::npos);
    map = Colonies();
    map.ships_captured = true;
    map.ships = Ships().ships;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    auto state = Request("game.getState", "{\"include\":[\"session\",\"ships\",\"systems\",\"players\",\"colonies\"]}");
    const auto baseline = rpc::Game_Query(&fixture.store, &state);
    map.ships_captured = false;
    ++map.turn;
    ++map.colonies[0].population;
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state) == baseline);
    for (int index = 0; index < 4; ++index) {
        ++map.turn;
        DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    }
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &state).find("-32002") != std::string::npos);
    map.colonies.clear();
    DOCTEST_REQUIRE(rpc::Capture_Map(&fixture.store, &map));
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("\"items\":[]") != std::string::npos);
    rpc::Pause_Game_Queries(&fixture.store);
    DOCTEST_CHECK(rpc::Game_Query(&fixture.store, &list).find("-32002") != std::string::npos);
    rpc::Resume_Game_Queries(&fixture.store);
    map = Colonies();
    map.colonies[0].system = 99;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map = Colonies();
    map.colonies[0].orbit = 0;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map = Colonies();
    map.colonies[0].population = 43;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map = Colonies();
    map.colonies[0].orbit = 5;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
    map = Colonies();
    map.players_captured = false;
    DOCTEST_CHECK_FALSE(rpc::Capture_Map(&fixture.store, &map));
}
