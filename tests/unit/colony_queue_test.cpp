#include "pch.h"
#include "doctest/doctest.h"
#include "../fixtures/game_state.h"

DOCTEST_TEST_CASE_FIXTURE(test::s_game_state, "colony queue compaction preserves survivors and removes every hole") {
    const int16_t none = COLONY_PRODUCTION_NONE;
    const int16_t factory = BUILDING_AUTOMATED_FACTORY;
    const int16_t trade = COLONY_PRODUCTION_TRADE_GOODS;
    const std::array<int16_t, 7> inputs[] = {
        {none, none, factory, none, trade, none, none},
        {none, none, none, none, none, none, none},
        {factory, trade, factory, trade, factory, trade, factory},
    };
    const std::array<int16_t, 7> expected[] = {
        {factory, trade, none, none, none, none, none},
        inputs[1],
        inputs[2],
    };
    for (size_t i = 0; i < std::size(inputs); ++i) {
        for (size_t j = 0; j < 7; ++j) colonies[0].producing[j] = inputs[i][j];
        COLONY::Fix_Colony_Queue(0);
        for (size_t j = 0; j < 7; ++j) DOCTEST_CHECK((colonies[0].producing[j] == expected[i][j]));
    }
}
