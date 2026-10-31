#include "doctest/doctest.h"

#include <chrono>
#include <string>

#include "rpc/rpc_service.h"

DOCTEST_TEST_CASE("RPC service requires a configured nonempty key") {
    rpc::s_service service;
    rpc::s_service_config config;
    std::string diagnostic;
    DOCTEST_CHECK_FALSE(rpc::Start_Service(&service, &config, &diagnostic));
    DOCTEST_CHECK(service.state == nullptr);
    DOCTEST_CHECK_FALSE(diagnostic.empty());
    rpc::Stop_Service(&service);
}

DOCTEST_TEST_CASE("RPC libuv service starts and stops without a future game poll") {
    for (int iteration = 0; iteration < 3; ++iteration) {
        rpc::s_service service;
        rpc::s_service_config config;
        config.port = 0;
        config.key = "rpc-test-key-not-a-real-secret";
        std::string diagnostic;
        DOCTEST_REQUIRE(rpc::Start_Service(&service, &config, &diagnostic));
        DOCTEST_CHECK(rpc::Service_Port(&service) != 0);
        const auto begin = std::chrono::steady_clock::now();
        rpc::Stop_Service(&service);
        DOCTEST_CHECK(std::chrono::steady_clock::now() - begin < std::chrono::seconds(2));
        DOCTEST_CHECK(service.state == nullptr);
        rpc::Stop_Service(&service);
    }
}

DOCTEST_TEST_CASE("RPC bind failure disables only RPC with a redacted diagnostic") {
    rpc::s_service first, second;
    rpc::s_service_config config;
    config.port = 0;
    config.key = "rpc-test-key-not-a-real-secret";
    std::string diagnostic;
    DOCTEST_REQUIRE(rpc::Start_Service(&first, &config, &diagnostic));
    config.port = rpc::Service_Port(&first);
    const bool started = rpc::Start_Service(&second, &config, &diagnostic);
    DOCTEST_CHECK_FALSE(started);
    DOCTEST_CHECK(diagnostic.find(config.key) == std::string::npos);
    rpc::Stop_Service(&second);
    rpc::Stop_Service(&first);
}
