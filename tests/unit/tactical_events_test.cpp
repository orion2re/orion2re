#include "doctest/doctest.h"
#include "game/tactical_events.h"

#include <array>

namespace {
    struct s_observer {
        std::array<tactical_events::s_event, 16> events;
        size_t count = 0;
        uint64_t subscription = 0;
        static void Receive(const tactical_events::s_event* event, void* context) noexcept {
            auto* self = (s_observer*)context;
            if (self->count < self->events.size()) self->events[self->count++] = *event;
        }
        s_observer() {
            subscription = tactical_events::Subscribe(Receive, this);
        }
        ~s_observer() {
            tactical_events::Unsubscribe(subscription);
        }
    };
} // namespace

DOCTEST_TEST_CASE("tactical scope publishes only after initialized and closes exactly once") {
    s_observer observer;
    DOCTEST_REQUIRE(observer.subscription != 0);
    {
        tactical_events::s_battle_scope battle;
        DOCTEST_CHECK(observer.count == 0);
        battle.Open(1);
        battle.Open(1);
        DOCTEST_CHECK(observer.count == 1);
        battle.Close(1);
        battle.Close(1);
    }
    DOCTEST_REQUIRE(observer.count == 2);
    DOCTEST_CHECK(observer.events[0].kind == tactical_events::OPENED);
    DOCTEST_CHECK(observer.events[1].kind == tactical_events::CLOSED);
    DOCTEST_CHECK(observer.events[0].viewer == 1);
    DOCTEST_CHECK(observer.events[1].viewer == 1);
}

DOCTEST_TEST_CASE("hidden and aborted tactical initialization produce no events") {
    s_observer observer;
    {
        tactical_events::s_battle_scope aborted;
        aborted.Close(0);
    }
    {
        tactical_events::s_battle_scope hidden;
        hidden.Open(-1);
        hidden.Open(0);
        hidden.Close(-1);
    }
    DOCTEST_CHECK(observer.count == 0);
}

DOCTEST_TEST_CASE("unfinished tactical teardown and observer loss revoke instead of fabricating closure") {
    for (bool finish : {false, true}) {
        s_observer observer;
        {
            tactical_events::s_battle_scope battle;
            battle.Open(0);
            if (finish) battle.Close(1);
        }
        DOCTEST_REQUIRE(observer.count == 2);
        DOCTEST_CHECK(observer.events[1].kind == tactical_events::REVOKED);
    }
}

DOCTEST_TEST_CASE("tactical unsubscription detaches and old registration tokens cannot detach replacements") {
    s_observer observer;
    const auto old = observer.subscription;
    tactical_events::Unsubscribe(old);
    s_observer replacement;
    DOCTEST_REQUIRE(replacement.subscription != 0);
    tactical_events::Unsubscribe(old);
    tactical_events::Revoke();
    DOCTEST_CHECK(observer.count == 0);
    DOCTEST_CHECK(replacement.count == 1);
}

DOCTEST_TEST_CASE("tactical listener storage is bounded") {
    s_observer first, second, third, fourth, fifth;
    DOCTEST_CHECK(first.subscription != 0);
    DOCTEST_CHECK(fourth.subscription != 0);
    DOCTEST_CHECK(fifth.subscription == 0);
}

DOCTEST_TEST_CASE("replaced battle owners cannot close or revoke the replacement") {
    s_observer observer;
    tactical_events::s_battle_scope old;
    old.Open(0);
    {
        tactical_events::s_battle_scope replacement;
        replacement.Open(0);
        old.Close(0);
        DOCTEST_CHECK(tactical_events::Current_Viewer() == 0);
        replacement.Close(0);
    }
    DOCTEST_REQUIRE(observer.count == 4);
    DOCTEST_CHECK(observer.events[0].kind == tactical_events::OPENED);
    DOCTEST_CHECK(observer.events[1].kind == tactical_events::REVOKED);
    DOCTEST_CHECK(observer.events[2].kind == tactical_events::OPENED);
    DOCTEST_CHECK(observer.events[3].kind == tactical_events::CLOSED);
}
