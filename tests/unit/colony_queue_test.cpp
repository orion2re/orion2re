#include "pch.h"

#include "doctest/doctest.h"
#include "game/colony.h"
#include "../fixtures/game_state.h"

namespace {
    struct s_queue_state : test::s_game_state {
    };

    std::array<int16_t, 7> Queue_Snapshot_(int16_t colony_idx) {
        std::array<int16_t, 7> snapshot{};
        for (int16_t i = 0; i < 7; ++i) {
            snapshot[static_cast<size_t>(i)] = MOX::colony[colony_idx].producing[i];
        }
        return snapshot;
    }

    void Set_Queue_(int16_t colony_idx, std::array<int16_t, 7> queue) {
        for (int16_t i = 0; i < 7; ++i) {
            MOX::colony[colony_idx].producing[i] = queue[static_cast<size_t>(i)];
        }
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_queue_state, "defragment removes consecutive leading NONE holes") {
    const int16_t none = COLONY_PRODUCTION_NONE;
    const int16_t factory = BUILDING_AUTOMATED_FACTORY;
    const int16_t trade = COLONY_PRODUCTION_TRADE_GOODS;

    Set_Queue_(0, {none, none, factory, none, trade, none, none});

    COLONY::Defragment_Colony_Queue(0);

    DOCTEST_CHECK(Queue_Snapshot_(0) == std::array<int16_t, 7>{factory, trade, none, none, none, none, none});
}

DOCTEST_TEST_CASE_FIXTURE(s_queue_state, "defragment leaves an all-NONE queue all NONE") {
    const int16_t none = COLONY_PRODUCTION_NONE;

    Set_Queue_(0, {none, none, none, none, none, none, none});

    COLONY::Defragment_Colony_Queue(0);

    DOCTEST_CHECK(Queue_Snapshot_(0) == std::array<int16_t, 7>{none, none, none, none, none, none, none});
}

DOCTEST_TEST_CASE_FIXTURE(s_queue_state, "defragment leaves a queue without holes unchanged") {
    const int16_t none = COLONY_PRODUCTION_NONE;
    const int16_t factory = BUILDING_AUTOMATED_FACTORY;
    const int16_t trade = COLONY_PRODUCTION_TRADE_GOODS;
    const int16_t housing = COLONY_PRODUCTION_HOUSING;

    Set_Queue_(0, {factory, trade, housing, factory, trade, housing, factory});

    COLONY::Defragment_Colony_Queue(0);

    DOCTEST_CHECK(Queue_Snapshot_(0) == std::array<int16_t, 7>{factory, trade, housing, factory, trade, housing, factory});
    (void)none;
}

DOCTEST_TEST_CASE_FIXTURE(s_queue_state, "Fix_Colony_Queue clears the double-NONE stall left by android production") {
    const int16_t none = COLONY_PRODUCTION_NONE;
    const int16_t factory = BUILDING_AUTOMATED_FACTORY;

    // Android pop production with producing[1] == REPEAT and
    // removed_existing_pop sets producing[0] and producing[1] to NONE;
    // the next Fix_Colony_Queue must not leave NONE at [0].
    Set_Queue_(0, {none, none, factory, none, none, none, none});

    COLONY::Fix_Colony_Queue(0);

    DOCTEST_CHECK(Queue_Snapshot_(0) == std::array<int16_t, 7>{factory, none, none, none, none, none, none});
}
