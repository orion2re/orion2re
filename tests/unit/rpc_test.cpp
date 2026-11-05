#include "doctest/doctest.h"
#include "rpc/rpc_protocol.h"
#include "rpc/rpc_service.h"
#include "rpc/rpc_event_queue.h"
#include "rpc/rpc_read_queue.h"
#include "rpc/rpc_operation_queue.h"
#include "yyjson.h"

#include <chrono>
#include <string>

namespace {
    constexpr char KEY[] = "rpc-test-key-not-a-real-secret";
    constexpr char HELLO[] = R"({"jsonrpc":"2.0","id":1,"method":"rpc.hello","params":{"key":"rpc-test-key-not-a-real-secret"}})";

    std::string Process(rpc::s_session* session, const std::string& input, bool control = true) {
        std::string result;
        DOCTEST_REQUIRE(rpc::Process_Request(input, KEY, control, session, &result));
        if (!result.empty()) {
            auto* document = yyjson_read(result.data(), result.size(), YYJSON_READ_NOFLAG);
            DOCTEST_REQUIRE(document != nullptr);
            yyjson_doc_free(document);
        }
        return result;
    }

    std::string Retreat_Request(const std::string* common, const char* extra) {
        return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"combat.retreat\",\"params\":{" + *common + extra + "}}";
    }
    std::string Boarding_Request(const std::string* common, const char* mode, const char* marines) {
        return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"combat.board\",\"params\":{" + *common + ",\"mode\":" + mode + ",\"marines\":" + marines + "}}";
    }
    std::string Selection_Request(const std::string* common, const char* banks) {
        return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"combat.setFireSelection\",\"params\":{" + *common + ",\"banks\":" + banks + "}}";
    }
} // namespace

DOCTEST_TEST_CASE("retreat is a strict context-bound mutation and its event uses the existing bounded delivery") {
    rpc::s_session session;
    session.authenticated = session.controller = true;
    session.events = 7;
    const std::string common = R"("battleId":"0123456789abcdef0123456789abcdef","actorUnitId":"0123456789abcdef0123456789abcdef","decisionId":"0123456789abcdef0123456789abcdef","operationSeq":1)";
    std::string response;
    rpc::s_operation_request request;
    for (const char* extra : {",\"destination\":{}", ",\"targetUnitId\":\"fedcba9876543210fedcba9876543210\"", ",\"force\":true"}) {
        DOCTEST_REQUIRE(rpc::Process_Request(Retreat_Request(&common, extra), KEY, true, &session, &response, nullptr, nullptr, &request));
        DOCTEST_CHECK(response.find("-32602") != std::string::npos);
        DOCTEST_CHECK(session.operation_sequence == 0);
    }
    DOCTEST_REQUIRE(rpc::Process_Request(Retreat_Request(&common, ""), KEY, true, &session, &response, nullptr, nullptr, &request));
    DOCTEST_CHECK(response.empty());
    DOCTEST_CHECK(request.method == rpc::OP_RETREAT);
    DOCTEST_CHECK(Process(&session, R"({"jsonrpc":"2.0","id":2,"method":"rpc.subscribe","params":{"events":["combat.retreatResolved"]}})").find("combat.retreatResolved") != std::string::npos);
    DOCTEST_CHECK(session.events == rpc::EVENT_COMBAT_RETREAT);
    rpc::s_event_queue queue;
    const std::string battle(32, 'a'), unit(32, 'b');
    const std::string detail = R"("outcome":"escaped")";
    for (size_t i = 0; i < rpc::MAX_PENDING_EVENTS; ++i)
        DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_RETREAT, battle, unit, detail));
    DOCTEST_CHECK(queue.reserved_response_bytes == rpc::MAX_PENDING_EVENTS * detail.size());
    auto batch = rpc::Take_Events(&queue);
    DOCTEST_CHECK(batch.count == rpc::MAX_PENDING_EVENTS);
    rpc::Release_Event_Batch(&queue, &batch);
    DOCTEST_CHECK(queue.reserved_response_bytes == 0);
    DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_RETREAT, battle, unit, detail));
    rpc::Revoke_Events(&queue);
    DOCTEST_CHECK(queue.reserved_response_bytes == 0);
    DOCTEST_CHECK(rpc::Take_Events(&queue).count == 0);
}

DOCTEST_TEST_CASE("boarding acknowledgement child never releases or recursively takes its executing parent") {
    rpc::s_operation_queue queue;
    rpc::s_operation_request parent, child, taken, cancelled;
    parent.method = rpc::OP_BOARD;
    parent.sequence = 1;
    parent.request.connection = 7;
    parent.request.deadline_ms = rpc::Monotonic_Ms() + 2000;
    DOCTEST_REQUIRE(rpc::Enqueue_Operation(&queue, parent));
    DOCTEST_REQUIRE(rpc::Take_Operation(&queue, &taken));
    child = parent;
    child.method = rpc::OP_BOARDING_ACK;
    child.report_id = std::string(32, 'a');
    rpc::Set_Boarding_Report(&queue, child.report_id);
    child.sequence = 2;
    child.request.connection = 8;
    DOCTEST_CHECK_FALSE(rpc::Enqueue_Operation(&queue, child));
    child.request.connection = 7;
    DOCTEST_REQUIRE(rpc::Enqueue_Operation(&queue, child));
    DOCTEST_CHECK_FALSE(rpc::Enqueue_Operation(&queue, child));
    DOCTEST_CHECK_FALSE(rpc::Take_Operation(&queue, &taken));
    DOCTEST_REQUIRE(rpc::Take_Operation(&queue, &taken, true));
    DOCTEST_CHECK(taken.child);
    DOCTEST_CHECK_FALSE(rpc::Cancel_Operation(&queue, 7, 2, &cancelled));
    rpc::Finish_Operation(&queue, true);
    DOCTEST_CHECK(queue.executing);
    DOCTEST_CHECK_FALSE(rpc::Enqueue_Operation(&queue, parent));
    child.request.deadline_ms = 0;
    DOCTEST_REQUIRE(rpc::Enqueue_Operation(&queue, child));
    DOCTEST_CHECK_FALSE(rpc::Take_Operation(&queue, &taken, true));
    DOCTEST_REQUIRE(rpc::Cancel_Operation(&queue, 7, 2, &cancelled, false, true));
    DOCTEST_CHECK(queue.executing);
    rpc::Finish_Operation(&queue);
    DOCTEST_CHECK_FALSE(queue.executing);
}

DOCTEST_TEST_CASE("boarding mode and commitment are explicit strictly typed and sequence protected") {
    rpc::s_session session;
    session.authenticated = session.controller = true;
    session.events = 7;
    const std::string common = R"("battleId":"0123456789abcdef0123456789abcdef","actorUnitId":"0123456789abcdef0123456789abcdef","decisionId":"0123456789abcdef0123456789abcdef","operationSeq":1,"targetUnitId":"fedcba9876543210fedcba9876543210")";
    std::string response;
    rpc::s_operation_request request;
    for (const char* invalid : {"-1", "129", "1.5", "true", "null", "\"4\""}) {
        DOCTEST_REQUIRE(rpc::Process_Request(Boarding_Request(&common, "\"capture\"", invalid), KEY, true, &session, &response, nullptr, nullptr, &request));
        DOCTEST_CHECK(response.find("-32602") != std::string::npos);
        DOCTEST_CHECK(session.operation_sequence == 0);
    }
    DOCTEST_REQUIRE(rpc::Process_Request(Boarding_Request(&common, "\"best\"", "4"), KEY, true, &session, &response, nullptr, nullptr, &request));
    DOCTEST_CHECK(response.find("-32602") != std::string::npos);
    DOCTEST_REQUIRE(rpc::Process_Request(Boarding_Request(&common, "\"raid\"", "0"), KEY, true, &session, &response, nullptr, nullptr, &request));
    DOCTEST_CHECK(response.empty());
    DOCTEST_CHECK(request.method == rpc::OP_BOARD);
    DOCTEST_CHECK(request.marines == 0);
    DOCTEST_REQUIRE(rpc::Process_Request(Boarding_Request(&common, "\"capture\"", "4"), KEY, true, &session, &response, nullptr, nullptr, &request));
    DOCTEST_CHECK(response.find("-32012") != std::string::npos);
}

DOCTEST_TEST_CASE("detailed boarding delivery accounts and releases its bounded payload budget") {
    rpc::s_event_queue queue;
    const std::string battle(32, 'a');
    DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_BOARDING, battle, {}, std::string(20000, 'x')));
    DOCTEST_CHECK(queue.reserved_response_bytes == 20000);
    auto batch = rpc::Take_Events(&queue);
    DOCTEST_CHECK(batch.response_bytes == 20000);
    for (auto& event : batch.items) std::string().swap(event.response);
    rpc::Release_Event_Batch(&queue, &batch);
    DOCTEST_CHECK(queue.reserved_response_bytes == 0);
    DOCTEST_CHECK_FALSE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_BOARDING, battle, {}, std::string(rpc::MAX_RESPONSE_BYTES + 1, 'x')));
    DOCTEST_CHECK(rpc::Take_Events(&queue).overflow_position != 0);
}

DOCTEST_TEST_CASE("tactical mutations require controller sequence and strictly typed combat parameters") {
    rpc::s_session session;
    session.authenticated = true;
    session.events = 7;
    const std::string input = R"({"jsonrpc":"2.0","id":1,"method":"combat.move","params":{"battleId":"0123456789abcdef0123456789abcdef","actorUnitId":"0123456789abcdef0123456789abcdef","decisionId":"0123456789abcdef0123456789abcdef","operationSeq":1,"destination":{"x":4,"y":5}}})";
    std::string response;
    rpc::s_operation_request request;
    DOCTEST_REQUIRE(rpc::Process_Request(input, KEY, true, &session, &response, nullptr, nullptr, &request));
    DOCTEST_CHECK(response.find("-32007") != std::string::npos);
    session.controller = true;
    DOCTEST_REQUIRE(rpc::Process_Request(input, KEY, true, &session, &response, nullptr, nullptr, &request));
    DOCTEST_CHECK(response.empty());
    DOCTEST_CHECK(request.method == rpc::OP_MOVE);
    DOCTEST_CHECK(request.x == 4);
    DOCTEST_REQUIRE(rpc::Process_Request(input, KEY, true, &session, &response, nullptr, nullptr, &request));
    DOCTEST_CHECK(response.find("-32012") != std::string::npos);
    DOCTEST_CHECK(request.method == rpc::OP_NONE);
    DOCTEST_CHECK(Process(&session, R"({"jsonrpc":"2.0","id":2,"method":"move","params":{}})").find("-32601") != std::string::npos);
}

DOCTEST_TEST_CASE("mutation dispatch atomically defeats expiry cancellation disconnect and shutdown") {
    rpc::s_operation_queue queue;
    rpc::s_operation_request request;
    request.method = rpc::OP_MOVE;
    request.sequence = 1;
    request.request.connection = 9;
    request.request.deadline_ms = rpc::Monotonic_Ms() + 2000;
    DOCTEST_REQUIRE(rpc::Enqueue_Operation(&queue, request));
    DOCTEST_CHECK_FALSE(rpc::Enqueue_Operation(&queue, request));
    rpc::s_operation_request taken, cancelled;
    DOCTEST_CHECK_FALSE(rpc::Cancel_Operation(&queue, 8, 0, &cancelled));
    DOCTEST_REQUIRE(rpc::Take_Operation(&queue, &taken));
    queue.pending.request.deadline_ms = 0;
    DOCTEST_CHECK_FALSE(rpc::Cancel_Operation(&queue, 9, 1, &cancelled));
    DOCTEST_CHECK_FALSE(rpc::Cancel_Operation(&queue, 0, 0, &cancelled, true, true));
    DOCTEST_CHECK(queue.executing);
    rpc::Finish_Operation(&queue);
    DOCTEST_CHECK_FALSE(rpc::Enqueue_Operation(&queue, request));
    rpc::s_operation_queue expired;
    request.request.deadline_ms = rpc::Monotonic_Ms();
    DOCTEST_REQUIRE(rpc::Enqueue_Operation(&expired, request));
    DOCTEST_CHECK_FALSE(rpc::Take_Operation(&expired, &taken));
    DOCTEST_REQUIRE(rpc::Cancel_Operation(&expired, 0, 0, &cancelled, false, true));
    DOCTEST_CHECK(cancelled.sequence == 1);
    DOCTEST_CHECK_FALSE(rpc::Take_Operation(&expired, &taken));
}

DOCTEST_TEST_CASE("bank selection and missile targeting validate bounded semantic parameters") {
    rpc::s_session session;
    session.authenticated = session.controller = true;
    session.events = 7;
    const std::string common = R"("battleId":"0123456789abcdef0123456789abcdef","actorUnitId":"0123456789abcdef0123456789abcdef","decisionId":"0123456789abcdef0123456789abcdef","operationSeq":1)";
    std::string response;
    rpc::s_operation_request operation;
    for (const char* invalid : {"[]", "null", "{}", R"([{"slot":8,"state":"on"}])", R"([{"slot":0,"state":"best"}])", R"([{"slot":0,"state":"on"},{"slot":0,"state":"off"}])", R"([{"slot":0,"state":"on","extra":1}])"}) {
        DOCTEST_REQUIRE(rpc::Process_Request(Selection_Request(&common, invalid), KEY, true, &session, &response, nullptr, nullptr, &operation));
        DOCTEST_CHECK(response.find("-32602") != std::string::npos);
        DOCTEST_CHECK(operation.method == rpc::OP_NONE);
        DOCTEST_CHECK(session.operation_sequence == 0);
    }
    DOCTEST_REQUIRE(rpc::Process_Request(Selection_Request(&common, R"([{"slot":0,"state":"temporaryOff"},{"slot":7,"state":"off"}])"), KEY, true, &session, &response, nullptr, nullptr, &operation));
    DOCTEST_CHECK(response.empty());
    DOCTEST_CHECK(operation.method == rpc::OP_FIRE_SELECTION);
    DOCTEST_CHECK(operation.bank_mask == 129);
    DOCTEST_CHECK(operation.bank_states[0] == 0);
    DOCTEST_CHECK(operation.bank_states[7] == -1);
    session.operation_sequence = 0;
    const auto fire = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"combat.fire\",\"params\":{" + common + ",\"targetMissileId\":\"fedcba9876543210fedcba9876543210\"}}";
    DOCTEST_REQUIRE(rpc::Process_Request(fire, KEY, true, &session, &response, nullptr, nullptr, &operation));
    DOCTEST_CHECK(response.empty());
    DOCTEST_CHECK(operation.method == rpc::OP_FIRE);
    DOCTEST_CHECK(operation.missile_target);
    DOCTEST_CHECK(operation.target_id == "fedcba9876543210fedcba9876543210");
    session.controller = false;
    DOCTEST_REQUIRE(rpc::Process_Request(fire, KEY, true, &session, &response, nullptr, nullptr, &operation));
    DOCTEST_CHECK(response.find("-32007") != std::string::npos);
}

DOCTEST_TEST_CASE("RPC authenticates before discovery and advertises transport methods only") {
    rpc::s_session session;
    const std::string describe = R"({"jsonrpc":"2.0","id":"query","method":"rpc.describe"})";
    DOCTEST_CHECK(Process(&session, describe).find("-32001") != std::string::npos);
    DOCTEST_CHECK(Process(&session, HELLO).find("reader") != std::string::npos);
    DOCTEST_CHECK(session.authenticated);
    const auto result = Process(&session, describe);
    DOCTEST_CHECK(result.find("rpc.hello") != std::string::npos);
    DOCTEST_CHECK(result.find("rpc.describe") != std::string::npos);
    DOCTEST_CHECK(result.find("game.") == std::string::npos);
    DOCTEST_CHECK(result.find("rpc.subscribe") != std::string::npos);
    DOCTEST_CHECK(Process(&session, R"({"jsonrpc":"2.0","id":2,"method":"game.getState"})").find("-32601") != std::string::npos);
}

DOCTEST_TEST_CASE("RPC hello validates roles and preserves exclusive control") {
    rpc::s_session session;
    const std::string controller = R"({"jsonrpc":"2.0","id":1,"method":"rpc.hello","params":{"key":"rpc-test-key-not-a-real-secret","requestedRole":"controller"}})";
    DOCTEST_CHECK(Process(&session, controller, false).find("-32007") != std::string::npos);
    DOCTEST_CHECK_FALSE(session.authenticated);
    DOCTEST_CHECK(Process(&session, controller).find("controller") != std::string::npos);
    DOCTEST_CHECK(session.controller);
    DOCTEST_CHECK(Process(&session, controller, false).find("result") != std::string::npos);
    DOCTEST_CHECK(Process(&session, HELLO).find("-32602") != std::string::npos);
    DOCTEST_CHECK(session.controller);
    DOCTEST_CHECK(Process(&session, R"({"jsonrpc":"2.0","id":1,"method":"rpc.hello","params":{"key":"wrong"}})").find("-32001") != std::string::npos);
    DOCTEST_CHECK(session.close_after_output);
}

DOCTEST_TEST_CASE("RPC event selection is authenticated bounded replaceable and notification-free") {
    rpc::s_session session;
    const char* subscribe = R"({"jsonrpc":"2.0","id":1,"method":"rpc.subscribe","params":{"events":["combat.opened","combat.closed"]}})";
    DOCTEST_CHECK(Process(&session, subscribe).find("-32001") != std::string::npos);
    DOCTEST_CHECK(session.events == 0);
    Process(&session, HELLO);
    DOCTEST_CHECK(Process(&session, subscribe).find("result") != std::string::npos);
    DOCTEST_CHECK(session.events == 3);
    const char* invalid[] = {
        R"({"events":["object.changed"]})",
        R"({"events":["combat.opened","combat.opened"]})",
        R"({"events":["combat.opened"],"topics":["ships"]})",
        R"({"events":"combat.opened"})",
        R"({})",
    };
    for (const auto* params : invalid) {
        DOCTEST_CHECK(Process(&session, std::string(R"({"jsonrpc":"2.0","id":2,"method":"rpc.subscribe","params":)") + params + "}").find("-32602") != std::string::npos);
        DOCTEST_CHECK(session.events == 3);
    }
    Process(&session, R"({"jsonrpc":"2.0","method":"rpc.unsubscribe"})");
    DOCTEST_CHECK(session.events == 3);
    Process(&session, R"({"jsonrpc":"2.0","id":3,"method":"rpc.subscribe","params":{"events":["combat.closed"]}})");
    DOCTEST_CHECK(session.events == rpc::EVENT_COMBAT_CLOSED);
    Process(&session, R"({"jsonrpc":"2.0","id":4,"method":"rpc.unsubscribe"})");
    DOCTEST_CHECK(session.events == 0);
}

DOCTEST_TEST_CASE("RPC lifecycle queue copies tokens preserves order and records overflow explicitly") {
    rpc::s_event_queue queue;
    std::string token(32, 'a');
    DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_OPENED, token));
    token.assign(32, 'b');
    DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_CLOSED, token));
    auto batch = rpc::Take_Events(&queue);
    DOCTEST_REQUIRE(batch.count == 2);
    DOCTEST_CHECK(std::string(batch.items[0].battle_id.data()) == std::string(32, 'a'));
    DOCTEST_CHECK(batch.items[0].sequence < batch.items[1].sequence);
    DOCTEST_CHECK(rpc::Take_Events(&queue).count == 0);
    for (size_t i = 0; i < rpc::MAX_PENDING_EVENTS; ++i) DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_OPENED, token));
    DOCTEST_CHECK_FALSE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_CLOSED, token));
    batch = rpc::Take_Events(&queue);
    DOCTEST_CHECK(batch.count == 0);
    DOCTEST_CHECK(batch.overflow_position == batch.position.sequence);
    DOCTEST_CHECK(batch.overflow_position != 0);
    rpc::s_session subscribed;
    subscribed.authenticated = true;
    subscribed.auth_epoch = batch.position.epoch;
    subscribed.events = 3;
    DOCTEST_CHECK(rpc::Event_Delivery_Expired(&subscribed, &batch));
    subscribed.events_after = batch.position.sequence;
    DOCTEST_CHECK_FALSE(rpc::Event_Delivery_Expired(&subscribed, &batch));
    subscribed.events = 0;
    subscribed.events_after = 0;
    DOCTEST_CHECK_FALSE(rpc::Event_Delivery_Expired(&subscribed, &batch));
}

DOCTEST_TEST_CASE("RPC lifecycle revocation drops pending events and advances authorization without replay") {
    rpc::s_event_queue queue;
    const auto before = rpc::Event_Position(&queue);
    DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_OPENED, std::string(32, 'a')));
    rpc::Revoke_Events(&queue);
    const auto after = rpc::Take_Events(&queue);
    DOCTEST_CHECK(after.count == 0);
    DOCTEST_CHECK(after.position.epoch != before.epoch);
    DOCTEST_CHECK(after.position.sequence > before.sequence);
    rpc::s_session session;
    session.authenticated = true;
    session.auth_epoch = before.epoch;
    DOCTEST_CHECK(rpc::Event_Delivery_Expired(&session, &after));
    rpc::Stop_Events(&queue);
    DOCTEST_CHECK_FALSE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_CLOSED, std::string(32, 'a')));
}

DOCTEST_TEST_CASE("RPC rejects malformed ambiguous and oversized requests without echoing secrets") {
    rpc::s_session session;
    const char* invalid[] = {
        R"([])",
        R"({"jsonrpc":"2.0","id":null,"method":"rpc.hello"})",
        R"({"jsonrpc":"2.0","id":1.0,"method":"rpc.hello"})",
        R"({"jsonrpc":"2.0","id":9007199254740992,"method":"rpc.hello"})",
        R"({"jsonrpc":"2.0","id":1,"id":2,"method":"rpc.hello"})",
        R"({"jsonrpc":"2.0","method":"rpc.hello","extra":true})",
    };
    for (const auto* input : invalid) DOCTEST_CHECK(Process(&session, input).find("-32600") != std::string::npos);
    DOCTEST_CHECK(Process(&session, "{").find("-32700") != std::string::npos);
    DOCTEST_CHECK(Process(&session, std::string(33, '[')).find("-32005") != std::string::npos);
    DOCTEST_CHECK(Process(&session, std::string(rpc::MAX_INPUT_BYTES + 1, ' ')).find("-32005") != std::string::npos);
    DOCTEST_CHECK(Process(&session, R"({"jsonrpc":"2.0","id":1,"method":"rpc.hello","params":{"key":"private-test-value","extra":1}})").find("private-test-value") == std::string::npos);
    DOCTEST_CHECK_FALSE(session.authenticated);
}

DOCTEST_TEST_CASE("RPC preserves encoded request IDs and ignores notifications without authenticating") {
    rpc::s_session session;
    DOCTEST_CHECK(Process(&session, R"({"jsonrpc":"2.0","method":"rpc.hello","params":{"key":"rpc-test-key-not-a-real-secret"}})").empty());
    DOCTEST_CHECK_FALSE(session.authenticated);
    Process(&session, HELLO);
    DOCTEST_CHECK(Process(&session, R"({"jsonrpc":"2.0","id":"a\"b","method":"rpc.describe"})").find(R"("id":"a\"b")") != std::string::npos);
    DOCTEST_CHECK(Process(&session, R"({"jsonrpc":"2.0","id":2,"method":"rpc.describe","params":[]})").find("-32602") != std::string::npos);
}

DOCTEST_TEST_CASE("RPC service requires a configured key and starts stops and restarts without game polling") {
    rpc::s_service service;
    rpc::s_service_config config;
    std::string diagnostic;
    DOCTEST_CHECK_FALSE(rpc::Start_Service(&service, &config, &diagnostic));
    DOCTEST_CHECK(service.state == nullptr);
    DOCTEST_CHECK_FALSE(diagnostic.empty());
    config.port = 0;
    config.key = KEY;
    for (int i = 0; i < 3; ++i) {
        DOCTEST_REQUIRE(rpc::Start_Service(&service, &config, &diagnostic));
        DOCTEST_CHECK(rpc::Service_Port(&service) != 0);
        DOCTEST_CHECK_FALSE(rpc::Start_Service(&service, &config, &diagnostic));
        const auto start = std::chrono::steady_clock::now();
        rpc::Stop_Service(&service);
        DOCTEST_CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
        DOCTEST_CHECK(service.state == nullptr);
        rpc::Stop_Service(&service);
    }
}

DOCTEST_TEST_CASE("RPC bind failure cleans up and does not expose the configured key") {
    rpc::s_service first, second;
    rpc::s_service_config config;
    config.port = 0;
    config.key = KEY;
    std::string diagnostic;
    DOCTEST_REQUIRE(rpc::Start_Service(&first, &config, &diagnostic));
    config.port = rpc::Service_Port(&first);
    DOCTEST_CHECK_FALSE(rpc::Start_Service(&second, &config, &diagnostic));
    DOCTEST_CHECK(second.state == nullptr);
    DOCTEST_CHECK_FALSE(diagnostic.empty());
    DOCTEST_CHECK(diagnostic.find(KEY) == std::string::npos);
    rpc::Stop_Service(&first);
    DOCTEST_REQUIRE(rpc::Start_Service(&second, &config, &diagnostic));
    rpc::Stop_Service(&second);
}

DOCTEST_TEST_CASE("tactical read envelopes are typed bounded and subscription guarded") {
    rpc::s_session session;
    Process(&session, HELLO);
    DOCTEST_CHECK(Process(&session, R"({"jsonrpc":"2.0","id":2,"method":"combat.getView"})").find("-32008") != std::string::npos);
    Process(&session, R"({"jsonrpc":"2.0","id":3,"method":"rpc.subscribe","params":{"events":["combat.opened","combat.closed","combat.viewInvalidated","combat.unitSelected"]}})");
    rpc::s_read_request read;
    std::string response;
    DOCTEST_REQUIRE(rpc::Process_Request(R"({"jsonrpc":"2.0","id":"current","method":"combat.getView","params":{}})", KEY, true, &session, &response, nullptr, &read));
    DOCTEST_CHECK(response.empty());
    DOCTEST_CHECK(read.method == rpc::READ_VIEW);
    DOCTEST_CHECK(read.battle_id.empty());
    const std::string unit = R"({"jsonrpc":"2.0","id":4,"method":"combat.getUnit","params":{"battleId":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","unitId":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}})";
    DOCTEST_REQUIRE(rpc::Process_Request(unit, KEY, true, &session, &response, nullptr, &read));
    DOCTEST_CHECK(read.method == rpc::READ_UNIT);
    DOCTEST_CHECK(read.battle_id == std::string(32, 'a'));
    const char* invalid[] = {R"({"offset":0})", R"({"battleId":"bad"})", R"({"battleId":null})", R"({"viewer":1})", "[]"};
    for (const auto* params : invalid) {
        DOCTEST_CHECK(Process(&session, std::string(R"({"jsonrpc":"2.0","id":5,"method":"combat.getView","params":)") + params + "}").find("-32602") != std::string::npos);
    }
    DOCTEST_CHECK(rpc::MAX_RESPONSE_BYTES == 16 * 1024 * 1024);
    DOCTEST_CHECK(rpc::MAX_OUTPUT_BYTES == 32 * 1024 * 1024);
}

DOCTEST_TEST_CASE("game read queue is bounded FIFO cancellable and expires executing work") {
    rpc::s_read_queue queue;
    rpc::s_read_request request;
    request.method = rpc::READ_VIEW;
    request.connection = 1;
    request.deadline_ms = rpc::Monotonic_Ms() + 10000;
    for (unsigned i = 0; i < rpc::MAX_PENDING_READS; ++i) DOCTEST_REQUIRE(rpc::Enqueue_Read(&queue, request));
    DOCTEST_CHECK_FALSE(rpc::Enqueue_Read(&queue, request));
    rpc::s_read_request first, second;
    DOCTEST_REQUIRE(rpc::Take_Read(&queue, &first));
    DOCTEST_REQUIRE(rpc::Take_Read(&queue, &second));
    DOCTEST_CHECK(first.ticket < second.ticket);
    rpc::Cancel_Reads(&queue, 1);
    DOCTEST_CHECK_FALSE(rpc::Finish_Read(&queue, first.ticket));
    DOCTEST_CHECK_FALSE(rpc::Take_Read(&queue, &first));
    request.deadline_ms = 0;
    DOCTEST_REQUIRE(rpc::Enqueue_Read(&queue, request));
    DOCTEST_REQUIRE(rpc::Take_Read(&queue, &first));
    DOCTEST_REQUIRE(rpc::Expire_Read(&queue, &second));
    DOCTEST_CHECK(first.ticket == second.ticket);
    DOCTEST_CHECK_FALSE(rpc::Finish_Read(&queue, first.ticket));
    rpc::Cancel_Reads(&queue, 0, true);
    DOCTEST_CHECK_FALSE(rpc::Enqueue_Read(&queue, request));
}

DOCTEST_TEST_CASE("refresh coalescing does not cross responses selection or lifecycle boundaries") {
    rpc::s_event_queue queue;
    const std::string battle(32, 'a');
    auto position = rpc::Event_Position(&queue);
    DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_OPENED, battle));
    for (int i = 0; i < 100; ++i) DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_INVALIDATED, battle));
    DOCTEST_REQUIRE(rpc::Push_Response(&queue, 1, position.epoch, position.read_epoch, "response"));
    DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_INVALIDATED, battle));
    DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_SELECTED, battle, std::string(32, 'b')));
    DOCTEST_REQUIRE(rpc::Push_Event(&queue, rpc::EVENT_COMBAT_CLOSED, battle));
    const auto batch = rpc::Take_Events(&queue);
    DOCTEST_REQUIRE(batch.count == 6);
    DOCTEST_CHECK(batch.items[2].response == "response");
    DOCTEST_CHECK(batch.items[2].connection == 1);
    for (size_t i = 1; i < batch.count; ++i) DOCTEST_CHECK(batch.items[i - 1].sequence < batch.items[i].sequence);
    rpc::Release_Event_Batch(&queue, &batch);
    rpc::Invalidate_Read_Output(&queue);
    DOCTEST_CHECK(rpc::Event_Position(&queue).read_epoch != position.read_epoch);
    DOCTEST_CHECK(rpc::Event_Position(&queue).epoch == position.epoch);
}

DOCTEST_TEST_CASE("pending response payload budget is independent of connection output") {
    rpc::s_event_queue queue;
    const auto position = rpc::Event_Position(&queue);
    const std::string response(rpc::MAX_RESPONSE_BYTES, 'x');
    for (unsigned i = 0; i < 8; ++i) DOCTEST_REQUIRE(rpc::Push_Response(&queue, i + 1, position.epoch, 0, response));
    DOCTEST_CHECK_FALSE(rpc::Push_Response(&queue, 1, position.epoch, 0, "x"));
    const auto batch = rpc::Take_Events(&queue);
    DOCTEST_CHECK(batch.count == 0);
    DOCTEST_CHECK(batch.response_bytes == 0);
    DOCTEST_CHECK(batch.overflow_position > 0);
}
