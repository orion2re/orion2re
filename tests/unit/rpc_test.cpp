#include "doctest/doctest.h"
#include "rpc/rpc_protocol.h"
#include "rpc/rpc_service.h"
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

    struct s_service_fixture {
        rpc::s_service service;
        ~s_service_fixture() {
            rpc::Stop_Service(&service);
        }
    };
} // namespace

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
    DOCTEST_CHECK(result.find("subscribe") == std::string::npos);
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
    s_service_fixture fixture;
    rpc::s_service_config config;
    std::string diagnostic;
    DOCTEST_CHECK_FALSE(rpc::Start_Service(&fixture.service, &config, &diagnostic));
    DOCTEST_CHECK(fixture.service.state == nullptr);
    DOCTEST_CHECK_FALSE(diagnostic.empty());
    config.port = 0;
    config.key = KEY;
    for (int i = 0; i < 3; ++i) {
        DOCTEST_REQUIRE(rpc::Start_Service(&fixture.service, &config, &diagnostic));
        DOCTEST_CHECK(rpc::Service_Port(&fixture.service) != 0);
        DOCTEST_CHECK_FALSE(rpc::Start_Service(&fixture.service, &config, &diagnostic));
        const auto start = std::chrono::steady_clock::now();
        rpc::Stop_Service(&fixture.service);
        DOCTEST_CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
        DOCTEST_CHECK(fixture.service.state == nullptr);
        rpc::Stop_Service(&fixture.service);
    }
}

DOCTEST_TEST_CASE("RPC bind failure cleans up and does not expose the configured key") {
    s_service_fixture first, second;
    rpc::s_service_config config;
    config.port = 0;
    config.key = KEY;
    std::string diagnostic;
    DOCTEST_REQUIRE(rpc::Start_Service(&first.service, &config, &diagnostic));
    config.port = rpc::Service_Port(&first.service);
    DOCTEST_CHECK_FALSE(rpc::Start_Service(&second.service, &config, &diagnostic));
    DOCTEST_CHECK(second.service.state == nullptr);
    DOCTEST_CHECK_FALSE(diagnostic.empty());
    DOCTEST_CHECK(diagnostic.find(KEY) == std::string::npos);
    rpc::Stop_Service(&first.service);
    DOCTEST_REQUIRE(rpc::Start_Service(&second.service, &config, &diagnostic));
}
