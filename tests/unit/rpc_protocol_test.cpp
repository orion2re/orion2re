#include "doctest/doctest.h"

#include <cstring>
#include <string>

#include "rpc/rpc_protocol.h"

namespace {
    int32_t Parse(const std::string& text, rpc::s_request* request) {
        return rpc::Parse_Request(text.data(), text.size(), request);
    }
} // namespace

DOCTEST_TEST_CASE("RPC strict envelope parsing rejects malformed and ambiguous input") {
    rpc::s_request request;
    const char* malformed[] = {
        "",
        "{} {}",
        "{/* comment */}",
        "{\"x\":1,}",
        "{\"id\":NaN}",
        "{\"id\":1e999}",
        "{\"id\":\"\\ud800\"}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.hello\",\"m\\u0065thod\":\"rpc.describe\",\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.hello\",\"id\":1,\"params\":{\"key\":\"a\",\"key\":\"b\"}}",
    };
    for (const char* text : malformed) {
        DOCTEST_CHECK(Parse(text, &request) == rpc::RPC_PARSE_ERROR);
    }
    const char* invalid[] = {
        "[]",
        "{}",
        "null",
        "true",
        "{\"jsonrpc\":2,\"method\":\"rpc.hello\",\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.hello\",\"id\":null}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.hello\",\"id\":true}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.hello\",\"id\":1.0}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.hello\",\"id\":9007199254740992}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.hello\",\"id\":1,\"extra\":true}",
    };
    for (const char* text : invalid) {
        DOCTEST_CHECK(Parse(text, &request) == rpc::RPC_INVALID_REQUEST);
        DOCTEST_CHECK(request.id_json.empty());
    }
    DOCTEST_CHECK(rpc::Parse_Request("\xff", 1, &request) == rpc::RPC_PARSE_ERROR);
    DOCTEST_CHECK(Parse("\xef\xbb\xbf{}", &request) == rpc::RPC_PARSE_ERROR);
}

DOCTEST_TEST_CASE("RPC envelope byte depth and ID limits precede queue admission") {
    rpc::s_request request;
    DOCTEST_CHECK(Parse(std::string(65537, ' '), &request) == rpc::RPC_LIMIT_EXCEEDED);
    DOCTEST_CHECK(Parse(std::string(33, '[') + std::string(33, ']'), &request) == rpc::RPC_LIMIT_EXCEEDED);
    DOCTEST_CHECK(Parse("{\"jsonrpc\":\"2.0\",\"method\":\"rpc.describe\",\"id\":\"[{\\\"[{\"}", &request) == 0);
    DOCTEST_CHECK(Parse("{\"jsonrpc\":\"2.0\",\"method\":\"rpc.describe\",\"id\":-9007199254740991}", &request) == 0);
    DOCTEST_CHECK(request.id_json == "-9007199254740991");
    const std::string prefix = "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.describe\",\"id\":\"";
    DOCTEST_CHECK(Parse(prefix + std::string(64, 'x') + "\"}", &request) == 0);
    DOCTEST_CHECK(Parse(prefix + std::string(65, 'x') + "\"}", &request) == rpc::RPC_INVALID_REQUEST);
}

DOCTEST_TEST_CASE("RPC client notifications never become acknowledged work") {
    rpc::s_request request;
    DOCTEST_REQUIRE(Parse("{\"jsonrpc\":\"2.0\",\"method\":\"rpc.hello\",\"extra\":true}", &request) == 0);
    DOCTEST_CHECK(request.notification);
    DOCTEST_CHECK(request.id_json.empty());
}

DOCTEST_TEST_CASE("RPC configured-key handshake and discovery use generated parameter types") {
    rpc::s_protocol protocol;
    DOCTEST_REQUIRE(rpc::Initialize_Protocol(&protocol));
    DOCTEST_CHECK_FALSE(rpc::Initialize_Protocol(&protocol));
    rpc::s_request request;
    DOCTEST_REQUIRE(Parse("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"rpc.hello\",\"params\":{\"key\":\"configured-key\"}}", &request) == 0);
    DOCTEST_REQUIRE(rpc::Validate_Params(&protocol, &request));
    std::string key;
    bool controller = true;
    DOCTEST_REQUIRE(rpc::Read_Hello(&request, &key, &controller));
    DOCTEST_CHECK(key == "configured-key");
    DOCTEST_CHECK_FALSE(controller);
    DOCTEST_CHECK(rpc::Keys_Match("configured-key", key));
    DOCTEST_CHECK_FALSE(rpc::Keys_Match("different-key", key));
    DOCTEST_CHECK_FALSE(rpc::Keys_Match("", ""));
    DOCTEST_CHECK_FALSE(rpc::Keys_Match("a", std::string("a\0b", 3)));
    const char* invalid_params[] = {
        "null",
        "[]",
        "{}",
        "{\"key\":true}",
        "{\"key\":\"\"}",
        "{\"key\":\"a\",\"viewerPlayerId\":2}",
        "{\"key\":\"a\",\"requestedRole\":\"omniscient\"}",
        "{\"key\":\"a\",\"contractVersion\":\"unsupported\"}",
    };
    for (const char* params : invalid_params) {
        request.params_json = params;
        DOCTEST_CHECK_FALSE(rpc::Validate_Params(&protocol, &request));
    }
    request.params_json = "{\"key\":\"a\",\"requestedRole\":\"controller\"}";
    DOCTEST_REQUIRE(rpc::Validate_Params(&protocol, &request));
    DOCTEST_REQUIRE(rpc::Read_Hello(&request, &key, &controller));
    DOCTEST_CHECK(controller);
    request.method = "rpc.describe";
    request.params_json = "{}";
    DOCTEST_REQUIRE(rpc::Validate_Params(&protocol, &request));
    std::string described = rpc::Describe_Methods(&request);
    DOCTEST_CHECK(described.find("rpc.hello") != std::string::npos);
    DOCTEST_CHECK(described.find("rpc.describe") != std::string::npos);
    DOCTEST_CHECK(described.find("game.getState") == std::string::npos);
    request.params_json = "{\"methods\":[\"rpc.describe\"]}";
    DOCTEST_REQUIRE(rpc::Validate_Params(&protocol, &request));
    described = rpc::Describe_Methods(&request);
    DOCTEST_CHECK(described.find("rpc.hello") == std::string::npos);
    request.params_json = "{\"methods\":[\"rpc.describe\",\"rpc.describe\"]}";
    DOCTEST_CHECK_FALSE(rpc::Validate_Params(&protocol, &request));
    DOCTEST_REQUIRE(rpc::Find_Method("game.getState") != nullptr);
    DOCTEST_CHECK_FALSE(rpc::Find_Method("game.getState")->implemented);
    rpc::Shutdown_Protocol(&protocol);
    rpc::Shutdown_Protocol(&protocol);
}

DOCTEST_TEST_CASE("RPC errors preserve correlation but contain no request payload") {
    rpc::s_request request;
    DOCTEST_REQUIRE(Parse("{\"jsonrpc\":\"2.0\",\"id\":\"a\\\"b\",\"method\":\"rpc.hello\",\"params\":{\"key\":\"private-value\"}}", &request) == 0);
    const std::string error = rpc::Error_Response(&request, rpc::RPC_UNAUTHORIZED);
    DOCTEST_CHECK(error == "{\"jsonrpc\":\"2.0\",\"id\":\"a\\\"b\",\"error\":{\"code\":-32001,\"message\":\"Unauthorized\"}}");
    DOCTEST_CHECK(error.find("private-value") == std::string::npos);
    DOCTEST_CHECK(rpc::Error_Response(nullptr, rpc::RPC_PARSE_ERROR).find("\"id\":null") != std::string::npos);
}
