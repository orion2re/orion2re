#include "pch.h"
#include "doctest/doctest.h"
#include "../fixtures/game_state.h"
#include "game/notice_serialization.h"

DOCTEST_TEST_CASE_FIXTURE(test::s_game_state, "historical scrap notices cannot name a replacement ship") {
    auto& world = game::current_state;
    ships.resize(1);
    s_MSG notice{};
    notice.msg_type = MSG_TYPE_SCRAP_SHIP;
    notice.u.scrap_ship.ship_idx = 0;
    SERIAL::Import_Notice_Ship_Target(world, notice);
    s_ship_data replacement{.id = game::id_ship{90001}};
    strlcpy(replacement.d.name, "Replacement", sizeof(replacement.d.name));
    ships.Replace_Row(0, replacement);
    char text[128]{};
    MSG_::sprintf_msg(text, sizeof(text), &notice);
    DOCTEST_CHECK(std::string_view{text} == "Scrapped ship (record unavailable).");

    // Old production notices still describe a completed ship order, but carry no key.
    DOCTEST_CHECK(game::Find_Production_Notice_Ship(world, COLONY_PRODUCTION_QUEUED_SHIP_BASE, game::NO_SHIP) == nullptr);
}

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
        COLONY::Fix_Colony_Queue(game::id_colony{0});
        for (size_t j = 0; j < 7; ++j) DOCTEST_CHECK((colonies[0].producing[j] == expected[i][j]));
    }
}
