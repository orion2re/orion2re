#include "doctest/doctest.h"

#include <atomic>
#include <string>
#include <thread>

#include "rpc/rpc_bridge.h"

namespace {
    rpc::s_request Request() {
        rpc::s_request request;
        request.id_json = "1";
        request.method = "game.getSession";
        request.params_json = "{}";
        return request;
    }
} // namespace

DOCTEST_TEST_CASE("RPC bridge owns bounded copies and immutable completions") {
    rpc::s_bridge bridge;
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&bridge, 1, &epoch));
    rpc::s_request request = Request();
    for (int count = 0; count < 16; ++count) {
        DOCTEST_CHECK(rpc::Submit_Work(&bridge, 1, epoch, &request, 100) == 0);
    }
    DOCTEST_CHECK(rpc::Submit_Work(&bridge, 1, epoch, &request, 100) == rpc::RPC_LIMIT_EXCEEDED);
    request.params_json = "changed";
    rpc::s_work work;
    DOCTEST_REQUIRE(rpc::Take_Work(&bridge, 0, &work));
    DOCTEST_CHECK(work.request.params_json == "{}");
    std::string result = "{\"immutable\":true}";
    DOCTEST_REQUIRE(rpc::Complete_Work(&bridge, &work, &result));
    DOCTEST_CHECK_FALSE(rpc::Complete_Work(&bridge, &work, &result));
    result = "changed";
    rpc::s_publication publication;
    DOCTEST_REQUIRE(rpc::Take_Publication(&bridge, &publication));
    DOCTEST_CHECK(publication.json == "{\"immutable\":true}");
}

DOCTEST_TEST_CASE("RPC bridge revocation cancels pending results and late completions") {
    rpc::s_bridge bridge;
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&bridge, 1, &epoch));
    rpc::s_request request = Request();
    DOCTEST_REQUIRE(rpc::Submit_Work(&bridge, 1, epoch, &request, 100) == 0);
    rpc::s_work work;
    DOCTEST_REQUIRE(rpc::Take_Work(&bridge, 0, &work));
    rpc::Revoke_Bridge(&bridge);
    const std::string result = "old perspective";
    DOCTEST_CHECK_FALSE(rpc::Complete_Work(&bridge, &work, &result));
    DOCTEST_CHECK_FALSE(rpc::Client_Is_Current(&bridge, 1, epoch));
    rpc::s_publication publication;
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&bridge, &publication));
    uint64_t next_epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&bridge, 1, &next_epoch));
    DOCTEST_CHECK(next_epoch != epoch);
    DOCTEST_CHECK(rpc::Submit_Work(&bridge, 1, epoch, &request, 100) == rpc::RPC_UNAUTHORIZED);
    rpc::Stop_Bridge(&bridge);
    DOCTEST_CHECK_FALSE(rpc::Register_Client(&bridge, 2, &next_epoch));
}

DOCTEST_TEST_CASE("RPC bridge expires unstarted work and disconnects explicitly on output overflow") {
    rpc::s_bridge bridge;
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&bridge, 1, &epoch));
    rpc::s_request request = Request();
    DOCTEST_REQUIRE(rpc::Submit_Work(&bridge, 1, epoch, &request, 10) == 0);
    rpc::s_work work;
    rpc::Expire_Work(&bridge, 10);
    DOCTEST_CHECK_FALSE(rpc::Take_Work(&bridge, 10, &work));
    rpc::s_publication publication;
    DOCTEST_REQUIRE(rpc::Take_Publication(&bridge, &publication));
    DOCTEST_CHECK(publication.json.find("-32006") != std::string::npos);
    DOCTEST_REQUIRE(rpc::Submit_Work(&bridge, 1, epoch, &request, 100) == 0);
    DOCTEST_REQUIRE(rpc::Take_Work(&bridge, 11, &work));
    const std::string huge(1048577, 'x');
    DOCTEST_CHECK_FALSE(rpc::Complete_Work(&bridge, &work, &huge));
    DOCTEST_CHECK_FALSE(rpc::Client_Is_Current(&bridge, 1, epoch));
    DOCTEST_CHECK_FALSE(rpc::Take_Publication(&bridge, &publication));
}

DOCTEST_TEST_CASE("RPC bridge completes work across threads without native or socket access") {
    rpc::s_bridge bridge;
    uint64_t epoch;
    DOCTEST_REQUIRE(rpc::Register_Client(&bridge, 1, &epoch));
    rpc::s_request request = Request();
    std::atomic<bool> finished = false;
    std::thread game([&] {
        rpc::s_work work;
        while (!finished) {
            if (rpc::Take_Work(&bridge, 0, &work)) {
                const std::string result = "copied result";
                rpc::Complete_Work(&bridge, &work, &result);
            } else {
                std::this_thread::yield();
            }
        }
    });
    int completed = 0;
    for (int submitted = 0; submitted < 100;) {
        if (rpc::Submit_Work(&bridge, 1, epoch, &request, 100) == 0) {
            ++submitted;
        }
        rpc::s_publication publication;
        while (rpc::Take_Publication(&bridge, &publication)) {
            DOCTEST_CHECK(publication.json == "copied result");
            ++completed;
        }
    }
    while (completed < 100) {
        rpc::s_publication publication;
        if (rpc::Take_Publication(&bridge, &publication)) {
            DOCTEST_CHECK(publication.json == "copied result");
            ++completed;
        } else {
            std::this_thread::yield();
        }
    }
    finished = true;
    game.join();
    DOCTEST_CHECK(completed == 100);
}
