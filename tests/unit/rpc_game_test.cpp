#include "doctest/doctest.h"
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
