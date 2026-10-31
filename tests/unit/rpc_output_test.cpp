#include "doctest/doctest.h"

#include "rpc/rpc_output.h"
#include "rpc/rpc_protocol.h"

DOCTEST_TEST_CASE("unsent state coalesces to the latest invalidation without discarding ordered results") {
    rpc::s_output_queue queue;
    size_t global_bytes = 0;
    const std::string first = "subscription-one";
    const std::string second = "subscription-two";
    DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "old-state", &first, &global_bytes));
    DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "operation-result", nullptr, &global_bytes));
    DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "other-state", &second, &global_bytes));
    for (int index = 0; index < 100; ++index) {
        DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "latest-state", &first, &global_bytes));
    }
    DOCTEST_REQUIRE(queue.messages.size() == 3);
    DOCTEST_CHECK(queue.messages[0].json == "operation-result");
    DOCTEST_CHECK(queue.messages[1].json == "other-state");
    DOCTEST_CHECK(queue.messages[2].json == "latest-state");
    DOCTEST_CHECK(queue.bytes == 16 + 11 + 12);
    DOCTEST_CHECK(global_bytes == queue.bytes);
}

DOCTEST_TEST_CASE("partially transmitted messages are immutable while later state remains replaceable") {
    rpc::s_output_queue queue;
    size_t global_bytes = 0;
    const std::string key = "subscription";
    const std::string original(8192, 'a');
    DOCTEST_REQUIRE(rpc::Queue_Output(&queue, original, &key, &global_bytes));
    DOCTEST_REQUIRE(rpc::Advance_Output(&queue, 4096, &global_bytes));
    DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "state-2", &key, &global_bytes));
    DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "result", nullptr, &global_bytes));
    DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "state-3", &key, &global_bytes));
    DOCTEST_REQUIRE(queue.messages.size() == 3);
    DOCTEST_CHECK(queue.messages.front().json == original);
    DOCTEST_CHECK(queue.offset == 4096);
    DOCTEST_CHECK(queue.messages[1].json == "result");
    DOCTEST_CHECK(queue.messages[2].json == "state-3");
    DOCTEST_CHECK_FALSE(rpc::Advance_Output(&queue, 4097, &global_bytes));
    DOCTEST_REQUIRE(rpc::Advance_Output(&queue, 4096, &global_bytes));
    DOCTEST_CHECK(queue.offset == 0);
    DOCTEST_CHECK(global_bytes == 13);
    DOCTEST_CHECK(queue.bytes == 13);
}

DOCTEST_TEST_CASE("output bounds reject overflow atomically and account for replacement and other clients") {
    rpc::s_output_queue queue;
    size_t global_bytes = 0;
    const std::string key = "subscription";
    const auto* limits = rpc::Protocol_Limits();
    for (uint32_t index = 0; index < limits->output_messages_per_connection; ++index) {
        DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "result", nullptr, &global_bytes));
    }
    DOCTEST_CHECK_FALSE(rpc::Queue_Output(&queue, "overflow", &key, &global_bytes));
    DOCTEST_CHECK(queue.messages.size() == limits->output_messages_per_connection);
    DOCTEST_CHECK(global_bytes == queue.bytes);

    DOCTEST_REQUIRE(rpc::Advance_Output(&queue, 6, &global_bytes));
    DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "old-state", &key, &global_bytes));
    DOCTEST_REQUIRE(rpc::Queue_Output(&queue, "new-state", &key, &global_bytes));
    DOCTEST_CHECK(queue.messages.size() == limits->output_messages_per_connection);
    DOCTEST_CHECK(queue.messages.back().json == "new-state");

    rpc::s_output_queue state;
    size_t shared_bytes = limits->output_bytes_global - 10;
    DOCTEST_REQUIRE(rpc::Queue_Output(&state, "1234567890", &key, &shared_bytes));
    DOCTEST_CHECK_FALSE(rpc::Queue_Output(&state, "12345678901", &key, &shared_bytes));
    DOCTEST_CHECK(state.messages.front().json == "1234567890");
    DOCTEST_REQUIRE(rpc::Queue_Output(&state, "x", &key, &shared_bytes));
    DOCTEST_CHECK(shared_bytes == limits->output_bytes_global - 9);
    DOCTEST_CHECK(state.bytes == 1);
    DOCTEST_CHECK_FALSE(rpc::Queue_Output(&state, std::string(limits->output_bytes_per_connection + 1, 'x'), &key, &shared_bytes));
    DOCTEST_CHECK(state.messages.front().json == "x");
}
