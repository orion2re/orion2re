#include "doctest/doctest.h"
#include <vector>
#include "game/game_events.h"

namespace {
    void Record(game_events::e_event event, void* context) {
        ((std::vector<game_events::e_event>*)context)->push_back(event);
    }
} // namespace

DOCTEST_TEST_CASE("neutral game events publish committed failed and nested resolution boundaries") {
    std::vector<game_events::e_event> events;
    const uint64_t id = game_events::Subscribe(Record, &events);
    DOCTEST_REQUIRE(id != 0);
    {
        game_events::s_world_update_scope update(true);
        update.Commit();
    }
    { game_events::s_world_update_scope update(false); }
    { game_events::s_resolution_scope resolution; }
    const std::vector<game_events::e_event> expected = {
        game_events::WORLD_REPLACE_BEGIN, game_events::WORLD_REPLACE_COMMITTED, game_events::WORLD_UPDATE_BEGIN, game_events::WORLD_UPDATE_FAILED, game_events::RESOLUTION_BEGIN, game_events::RESOLUTION_END};
    DOCTEST_CHECK(events == expected);
    game_events::Unsubscribe(id);
    game_events::Publish(game_events::THREAD_STARTED);
    DOCTEST_CHECK(events == expected);
}

DOCTEST_TEST_CASE("subscriptions removed during publication are not called with expired context") {
    struct s_context {
        uint64_t remove = 0;
        int calls = 0;
    } context;
    const auto remove = [](game_events::e_event, void* pointer) {
        auto* data = (s_context*)pointer;
        ++data->calls;
        game_events::Unsubscribe(data->remove);
    };
    const auto call = [](game_events::e_event, void* pointer) { ++((s_context*)pointer)->calls; };
    const uint64_t first = game_events::Subscribe(remove, &context);
    context.remove = game_events::Subscribe(call, &context);
    DOCTEST_REQUIRE(first != 0);
    DOCTEST_REQUIRE(context.remove != 0);
    game_events::Publish(game_events::MAP_OBSERVATION_POINT);
    DOCTEST_CHECK(context.calls == 1);
    game_events::Unsubscribe(first);
    game_events::Unsubscribe(context.remove);
}

DOCTEST_TEST_CASE("new listeners wait for the next event and observer storage is bounded") {
    struct s_context {
        uint64_t added = 0;
        int calls = 0;
    } context;
    const auto add = [](game_events::e_event, void* pointer) {
        auto* data = (s_context*)pointer;
        if (data->added == 0) {
            data->added = game_events::Subscribe([](game_events::e_event, void* value) { ++((s_context*)value)->calls; }, data);
        }
    };
    const uint64_t first = game_events::Subscribe(add, &context);
    DOCTEST_REQUIRE(first != 0);
    game_events::Publish(game_events::IDLE_OBSERVATION_POINT);
    DOCTEST_CHECK(context.calls == 0);
    game_events::Publish(game_events::IDLE_OBSERVATION_POINT);
    DOCTEST_CHECK(context.calls == 1);
    std::vector<uint64_t> extras;
    for (int index = 0; index < 6; ++index) extras.push_back(game_events::Subscribe(add, &context));
    DOCTEST_CHECK(game_events::Subscribe(add, &context) == 0);
    DOCTEST_CHECK(game_events::Subscribe(nullptr, nullptr) == 0);
    for (uint64_t id : extras) game_events::Unsubscribe(id);
    game_events::Unsubscribe(first);
    game_events::Unsubscribe(context.added);
}

DOCTEST_TEST_CASE("fleet info publication borrows a synchronous value and shares the bounded listener lifetime") {
    struct s_copy {
        game_events::s_fleet_box_info value = {};
        int calls = 0;
    } copy;
    const auto capture = [](const game_events::s_fleet_box_info* info, void* context) {
        auto* copy = (s_copy*)context;
        copy->value = *info;
        ++copy->calls;
    };
    const uint64_t token = game_events::Subscribe_Fleet_Box_Info(capture, &copy);
    DOCTEST_REQUIRE(token != 0);
    {
        const game_events::s_fleet_box_info info = {7, 255, -1, 3};
        game_events::Publish_Fleet_Box_Info(&info);
    }
    game_events::Publish_Fleet_Box_Info(nullptr);
    game_events::Publish(game_events::IDLE_OBSERVATION_POINT);
    DOCTEST_CHECK(copy.calls == 1);
    DOCTEST_CHECK(copy.value.ship == 7);
    DOCTEST_CHECK(copy.value.turns_left == 255);
    DOCTEST_CHECK(copy.value.star == -1);
    DOCTEST_CHECK(copy.value.node == 3);
    std::vector<uint64_t> extra;
    for (int i = 0; i < 7; ++i) extra.push_back(game_events::Subscribe_Fleet_Box_Info(capture, &copy));
    DOCTEST_CHECK(game_events::Subscribe_Fleet_Box_Info(capture, &copy) == 0);
    DOCTEST_CHECK(game_events::Subscribe(Record, nullptr) == 0);
    DOCTEST_CHECK(game_events::Subscribe_Fleet_Box_Info(nullptr, nullptr) == 0);
    for (uint64_t id : extra) game_events::Unsubscribe(id);
    game_events::Unsubscribe(token);
    const game_events::s_fleet_box_info info = {0, 0, 0, 0};
    game_events::Publish_Fleet_Box_Info(&info);
    DOCTEST_CHECK(copy.calls == 1);
}

DOCTEST_TEST_CASE("fleet view scope clears observations on entry and every exit") {
    std::vector<game_events::e_event> events;
    const uint64_t token = game_events::Subscribe(Record, &events);
    DOCTEST_REQUIRE(token != 0);
    {
        game_events::s_fleet_box_view_scope scope;
        DOCTEST_REQUIRE(events.size() == 1);
    }
    DOCTEST_CHECK(events == std::vector<game_events::e_event>{game_events::FLEET_BOX_INFO_CLEARED, game_events::FLEET_BOX_INFO_CLEARED});
    game_events::Unsubscribe(token);
}
