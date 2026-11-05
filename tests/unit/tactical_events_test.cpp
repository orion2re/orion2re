#include "pch.h"
#include "doctest/doctest.h"
#include "game/combat.h"

#include <array>

namespace {
    struct s_observer {
        std::array<COMBAT::s_event, 16> events;
        size_t count = 0;
        uint64_t subscription = 0;
    };
    void Receive(const COMBAT::s_event* event, void* context) noexcept {
        auto* observer = (s_observer*)context;
        if (observer->count < observer->events.size()) observer->events[observer->count++] = *event;
    }
    void Observe(s_observer* observer) {
        observer->subscription = COMBAT::Subscribe(Receive, observer);
    }
    void Stop_Observing(const s_observer* observer) {
        COMBAT::Unsubscribe(observer->subscription);
    }
} // namespace

DOCTEST_TEST_CASE("tactical scope publishes only after initialized and closes exactly once") {
    s_observer observer;
    Observe(&observer);
    DOCTEST_REQUIRE(observer.subscription != 0);
    {
        COMBAT::s_battle_state battle;
        DOCTEST_CHECK(observer.count == 0);
        COMBAT::Open_Battle(&battle, 1);
        COMBAT::Open_Battle(&battle, 1);
        DOCTEST_CHECK(observer.count == 1);
        COMBAT::Close_Battle(&battle, 1);
        COMBAT::Close_Battle(&battle, 1);
        COMBAT::Release_Battle(&battle);
    }
    DOCTEST_REQUIRE(observer.count == 2);
    DOCTEST_CHECK(observer.events[0].kind == COMBAT::OPENED);
    DOCTEST_CHECK(observer.events[1].kind == COMBAT::CLOSED);
    DOCTEST_CHECK(observer.events[0].viewer == 1);
    DOCTEST_CHECK(observer.events[1].viewer == 1);
    DOCTEST_CHECK(COMBAT::Current_Battle() == nullptr);
    Stop_Observing(&observer);
}

DOCTEST_TEST_CASE("hidden and aborted tactical initialization produce no events") {
    s_observer observer;
    Observe(&observer);
    {
        COMBAT::s_battle_state aborted;
        COMBAT::Close_Battle(&aborted, 0);
        COMBAT::Release_Battle(&aborted);
    }
    {
        COMBAT::s_battle_state hidden;
        COMBAT::Open_Battle(&hidden, -1);
        COMBAT::Open_Battle(&hidden, 0);
        COMBAT::Close_Battle(&hidden, -1);
        COMBAT::Release_Battle(&hidden);
    }
    DOCTEST_CHECK(observer.count == 0);
    Stop_Observing(&observer);
}

DOCTEST_TEST_CASE("unfinished tactical teardown and observer loss revoke instead of fabricating closure") {
    for (bool finish : {false, true}) {
        s_observer observer;
        Observe(&observer);
        {
            COMBAT::s_battle_state battle;
            COMBAT::Open_Battle(&battle, 0);
            if (finish) COMBAT::Close_Battle(&battle, 1);
            COMBAT::Release_Battle(&battle);
        }
        DOCTEST_REQUIRE(observer.count == 2);
        DOCTEST_CHECK(observer.events[1].kind == COMBAT::REVOKED);
        DOCTEST_CHECK(COMBAT::Current_Battle() == nullptr);
        Stop_Observing(&observer);
    }
}

DOCTEST_TEST_CASE("tactical unsubscription detaches and old registration tokens cannot detach replacements") {
    s_observer observer;
    Observe(&observer);
    const auto old = observer.subscription;
    COMBAT::Unsubscribe(old);
    s_observer replacement;
    Observe(&replacement);
    DOCTEST_REQUIRE(replacement.subscription != 0);
    COMBAT::Unsubscribe(old);
    COMBAT::Revoke();
    DOCTEST_CHECK(observer.count == 0);
    DOCTEST_CHECK(replacement.count == 1);
    Stop_Observing(&replacement);
}

DOCTEST_TEST_CASE("tactical listener storage is bounded") {
    s_observer first, second, third, fourth, fifth;
    Observe(&first);
    Observe(&second);
    Observe(&third);
    Observe(&fourth);
    Observe(&fifth);
    DOCTEST_CHECK(first.subscription != 0);
    DOCTEST_CHECK(fourth.subscription != 0);
    DOCTEST_CHECK(fifth.subscription == 0);
    Stop_Observing(&first);
    Stop_Observing(&second);
    Stop_Observing(&third);
    Stop_Observing(&fourth);
    Stop_Observing(&fifth);
}

DOCTEST_TEST_CASE("replaced battle owners cannot close or revoke the replacement") {
    s_observer observer;
    Observe(&observer);
    COMBAT::s_battle_state old;
    COMBAT::Open_Battle(&old, 0);
    {
        COMBAT::s_battle_state replacement;
        COMBAT::Open_Battle(&replacement, 0);
        COMBAT::Close_Battle(&old, 0);
        COMBAT::Release_Battle(&old);
        DOCTEST_CHECK(COMBAT::Current_Viewer() == 0);
        COMBAT::Close_Battle(&replacement, 0);
        COMBAT::Release_Battle(&replacement);
    }
    DOCTEST_REQUIRE(observer.count == 4);
    DOCTEST_CHECK(observer.events[0].kind == COMBAT::OPENED);
    DOCTEST_CHECK(observer.events[1].kind == COMBAT::REVOKED);
    DOCTEST_CHECK(observer.events[2].kind == COMBAT::OPENED);
    DOCTEST_CHECK(observer.events[3].kind == COMBAT::CLOSED);
    Stop_Observing(&observer);
}

DOCTEST_TEST_CASE("released battle state can be reused without retaining hits or authority") {
    s_observer observer;
    Observe(&observer);
    COMBAT::s_battle_state battle;
    COMBAT::Open_Battle(&battle, 0);
    COMBAT::s_hit hit;
    hit.amount = 1;
    COMBAT::Append_Hit(&hit);
    COMBAT::Open_Control_Decision(1);
    COMBAT::Release_Battle(&battle);
    DOCTEST_CHECK(COMBAT::Current_Battle() == nullptr);
    DOCTEST_CHECK(COMBAT::Current_Control_Actor() == -1);
    DOCTEST_CHECK(battle.hits.capacity() == 0);
    DOCTEST_CHECK_FALSE(battle.initialized);
    COMBAT::Open_Battle(&battle, 1);
    DOCTEST_CHECK(COMBAT::Current_Viewer() == 1);
    DOCTEST_CHECK(battle.next_hit_sequence == 1);
    COMBAT::Close_Battle(&battle, 1);
    COMBAT::Release_Battle(&battle);
    DOCTEST_CHECK(observer.count == 4);
    Stop_Observing(&observer);
}
