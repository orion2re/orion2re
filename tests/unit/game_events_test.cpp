#include "doctest/doctest.h"
#include <type_traits>
#include <vector>
#include "game/game_events.h"

namespace {
    void Record(game_events::e_event event, void* context) {
        ((std::vector<game_events::e_event>*)context)->push_back(event);
    }
} // namespace

DOCTEST_TEST_CASE("identity observations are callback-local and share the neutral listener bound") {
    std::vector<int16_t> slots;
    const auto capture = [](int16_t ship, void* context) { ((std::vector<int16_t>*)context)->push_back(ship); };
    const uint64_t token = game_events::Subscribe_Ship_Identity(capture, &slots);
    DOCTEST_REQUIRE(token != 0);
    game_events::Publish_Ship_Identity_Changing(7);
    game_events::Publish(game_events::SHIP_IDENTITIES_RESETTING);
    DOCTEST_CHECK(slots == std::vector<int16_t>{7});
    std::vector<uint64_t> extra;
    for (int index = 0; index < 7; ++index) extra.push_back(game_events::Subscribe_Ship_Identity(capture, &slots));
    DOCTEST_CHECK(game_events::Subscribe_Ship_Identity(capture, &slots) == 0);
    DOCTEST_CHECK(game_events::Subscribe_Ship_Identity(nullptr, nullptr) == 0);
    for (const auto id : extra) game_events::Unsubscribe(id);
    game_events::Unsubscribe(token);
    game_events::Publish_Ship_Identity_Changing(8);
    DOCTEST_CHECK(slots == std::vector<int16_t>{7});
}

DOCTEST_TEST_CASE("destination line observations are synchronous copied values with bounded listener lifetimes") {
    std::vector<game_events::s_ship_destination_line> lines;
    const auto capture = [](const game_events::s_ship_destination_line* line, void* context) {
        ((std::vector<game_events::s_ship_destination_line>*)context)->push_back(*line);
    };
    const auto token = game_events::Subscribe_Ship_Destination(capture, &lines);
    DOCTEST_REQUIRE(token != 0);
    {
        const game_events::s_ship_destination_line line = {4, 7, 2, true};
        game_events::Publish_Ship_Destination(&line);
    }
    game_events::Publish_Ship_Destination(nullptr);
    game_events::Publish(game_events::SHIP_DESTINATION_LINE_DRAWN);
    DOCTEST_REQUIRE(lines.size() == 1);
    DOCTEST_CHECK(lines[0].node == 4);
    DOCTEST_CHECK(lines[0].ship == 7);
    DOCTEST_CHECK(lines[0].destination == 2);
    DOCTEST_CHECK(lines[0].box_selected);
    std::vector<uint64_t> extra;
    for (int i = 0; i < 7; ++i) extra.push_back(game_events::Subscribe_Ship_Destination(capture, &lines));
    DOCTEST_CHECK(game_events::Subscribe_Ship_Destination(capture, &lines) == 0);
    DOCTEST_CHECK(game_events::Subscribe_Ship_Destination(nullptr, nullptr) == 0);
    for (const auto id : extra) game_events::Unsubscribe(id);
    game_events::Unsubscribe(token);
    const game_events::s_ship_destination_line line = {0, 0, 0, false};
    game_events::Publish_Ship_Destination(&line);
    DOCTEST_CHECK(lines.size() == 1);
}

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
        DOCTEST_REQUIRE(events.size() == 2);
    }
    DOCTEST_CHECK(events == std::vector<game_events::e_event>{game_events::FLEET_BOX_VIEW_ENTERED, game_events::FLEET_BOX_INFO_CLEARED, game_events::FLEET_BOX_VIEW_LEAVING, game_events::FLEET_BOX_INFO_CLEARED});
    game_events::Unsubscribe(token);
}

DOCTEST_TEST_CASE("fleet view initialization is explicit and distinct from info replacement") {
    std::vector<game_events::e_event> events;
    const uint64_t token = game_events::Subscribe(Record, &events);
    DOCTEST_REQUIRE(token != 0);
    {
        game_events::s_fleet_box_view_scope scope;
        scope.Initialized();
        scope.Initialized();
        game_events::Publish(game_events::FLEET_BOX_INFO_CLEARED);
        game_events::Publish(game_events::SHIP_ICONS_REBUILD_BEGIN);
    }
    DOCTEST_CHECK(events == std::vector<game_events::e_event>{game_events::FLEET_BOX_VIEW_ENTERED, game_events::FLEET_BOX_INFO_CLEARED, game_events::FLEET_BOX_VIEW_INITIALIZED, game_events::FLEET_BOX_INFO_CLEARED, game_events::SHIP_ICONS_REBUILD_BEGIN, game_events::FLEET_BOX_VIEW_LEAVING, game_events::FLEET_BOX_INFO_CLEARED});
    game_events::Unsubscribe(token);
}

DOCTEST_TEST_CASE("fleet view scope pairs leaving across initialization failure and unwinding") {
    std::vector<game_events::e_event> events;
    const uint64_t token = game_events::Subscribe(Record, &events);
    DOCTEST_REQUIRE(token != 0);
    for (bool initialize : {false, true}) {
        events.clear();
        try {
            game_events::s_fleet_box_view_scope scope;
            if (initialize) scope.Initialized();
            throw 1;
        } catch (int) {
        }
        std::vector<game_events::e_event> expected = {game_events::FLEET_BOX_VIEW_ENTERED, game_events::FLEET_BOX_INFO_CLEARED};
        if (initialize) expected.push_back(game_events::FLEET_BOX_VIEW_INITIALIZED);
        expected.push_back(game_events::FLEET_BOX_VIEW_LEAVING);
        expected.push_back(game_events::FLEET_BOX_INFO_CLEARED);
        DOCTEST_CHECK(events == expected);
    }
    game_events::Unsubscribe(token);
}

DOCTEST_TEST_CASE("replacement fleet view starts uninitialized without restoring an earlier owner") {
    std::vector<game_events::e_event> events;
    const uint64_t token = game_events::Subscribe(Record, &events);
    DOCTEST_REQUIRE(token != 0);
    {
        game_events::s_fleet_box_view_scope first;
        first.Initialized();
        events.clear();
        { game_events::s_fleet_box_view_scope replacement; }
        DOCTEST_CHECK(events == std::vector<game_events::e_event>{game_events::FLEET_BOX_VIEW_ENTERED, game_events::FLEET_BOX_INFO_CLEARED, game_events::FLEET_BOX_VIEW_LEAVING, game_events::FLEET_BOX_INFO_CLEARED});
    }
    DOCTEST_CHECK(events == std::vector<game_events::e_event>{game_events::FLEET_BOX_VIEW_ENTERED, game_events::FLEET_BOX_INFO_CLEARED, game_events::FLEET_BOX_VIEW_LEAVING, game_events::FLEET_BOX_INFO_CLEARED, game_events::FLEET_BOX_VIEW_LEAVING, game_events::FLEET_BOX_INFO_CLEARED});
    game_events::Unsubscribe(token);
}

DOCTEST_TEST_CASE("main input lifetime ends before child dispatch while the initialized view survives") {
    static_assert(!std::is_copy_constructible_v<game_events::s_main_view_input_scope>);
    static_assert(!std::is_copy_assignable_v<game_events::s_main_view_input_scope>);
    std::vector<game_events::e_event> events;
    const uint64_t token = game_events::Subscribe(Record, &events);
    DOCTEST_REQUIRE(token != 0);
    {
        game_events::s_fleet_box_view_scope owner;
        owner.Initialized();
        events.clear();
        {
            game_events::s_main_view_input_scope input;
            game_events::Publish(game_events::MAP_OBSERVATION_POINT);
            game_events::Publish(game_events::IDLE_OBSERVATION_POINT);
        }
        // A child may draw or poll while the main-view owner is still alive.
        game_events::Publish(game_events::FLEET_BOX_INFO_READY);
        game_events::Publish(game_events::IDLE_OBSERVATION_POINT);
        DOCTEST_CHECK(events == std::vector<game_events::e_event>{game_events::MAIN_VIEW_INPUT_ENTERED, game_events::MAP_OBSERVATION_POINT, game_events::IDLE_OBSERVATION_POINT, game_events::MAIN_VIEW_INPUT_LEAVING, game_events::FLEET_BOX_INFO_READY, game_events::IDLE_OBSERVATION_POINT});
    }
    DOCTEST_CHECK(events[events.size() - 2] == game_events::FLEET_BOX_VIEW_LEAVING);
    game_events::Unsubscribe(token);
}

DOCTEST_TEST_CASE("help interruption keeps earlier and child results distinct from main input entry") {
    std::vector<game_events::e_event> events;
    const uint64_t token = game_events::Subscribe(Record, &events);
    DOCTEST_REQUIRE(token != 0);
    {
        game_events::s_fleet_box_view_scope owner;
        game_events::Publish(game_events::FLEET_BOX_INFO_READY);
        owner.Initialized();
        {
            game_events::s_main_view_input_scope input;
            game_events::Publish(game_events::MAIN_VIEW_INPUT_INTERRUPTED);
            game_events::Publish(game_events::FLEET_BOX_INFO_READY);
            game_events::Publish(game_events::IDLE_OBSERVATION_POINT);
            // Returning from help is not a new main input lifetime.
            game_events::Publish(game_events::FLEET_BOX_INFO_READY);
        }
        { game_events::s_main_view_input_scope next_input; }
    }
    DOCTEST_CHECK(events == std::vector<game_events::e_event>{game_events::FLEET_BOX_VIEW_ENTERED, game_events::FLEET_BOX_INFO_CLEARED, game_events::FLEET_BOX_INFO_READY, game_events::FLEET_BOX_VIEW_INITIALIZED, game_events::MAIN_VIEW_INPUT_ENTERED, game_events::MAIN_VIEW_INPUT_INTERRUPTED, game_events::FLEET_BOX_INFO_READY, game_events::IDLE_OBSERVATION_POINT, game_events::FLEET_BOX_INFO_READY, game_events::MAIN_VIEW_INPUT_LEAVING, game_events::MAIN_VIEW_INPUT_ENTERED, game_events::MAIN_VIEW_INPUT_LEAVING, game_events::FLEET_BOX_VIEW_LEAVING, game_events::FLEET_BOX_INFO_CLEARED});
    game_events::Unsubscribe(token);
}

DOCTEST_TEST_CASE("main input scope pairs leaving on early return and interrupted exception unwinding") {
    std::vector<game_events::e_event> events;
    const uint64_t token = game_events::Subscribe(Record, &events);
    DOCTEST_REQUIRE(token != 0);
    const auto return_early = [] { game_events::s_main_view_input_scope input; };
    return_early();
    DOCTEST_CHECK(events == std::vector<game_events::e_event>{game_events::MAIN_VIEW_INPUT_ENTERED, game_events::MAIN_VIEW_INPUT_LEAVING});
    events.clear();
    try {
        game_events::s_main_view_input_scope input;
        game_events::Publish(game_events::MAIN_VIEW_INPUT_INTERRUPTED);
        throw 1;
    } catch (int) {
    }
    DOCTEST_CHECK(events == std::vector<game_events::e_event>{game_events::MAIN_VIEW_INPUT_ENTERED, game_events::MAIN_VIEW_INPUT_INTERRUPTED, game_events::MAIN_VIEW_INPUT_LEAVING});
    game_events::Unsubscribe(token);
}
