#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "../fixtures/colony_roster.h"

#include "doctest/doctest.h"
#include "fakes/fake_build_queue_runtime.h"
#include "game/build_queue.h"

// Game records are packed, so their members may be misaligned. Assertions
// on them use double parentheses so doctest compares the values directly
// instead of binding references to the packed members.

namespace {
    void Set_List_(int preset, std::initializer_list<uint8_t> instructions) {
        memset(BUILD_QUEUE::presets[preset], BUILD_QUEUE::INSTRUCTION_END,
               BUILD_QUEUE::INSTRUCTION_CAPACITY);
        int index = 0;
        for (uint8_t instruction : instructions) {
            BUILD_QUEUE::presets[preset][index++] = instruction;
        }
    }
}

DOCTEST_TEST_SUITE("automatic build queues") {

DOCTEST_TEST_CASE("append rejects unavailable and duplicate buildings without overflowing") {
    TEST_BUILD_QUEUE::Reset_();
    test_colonies::Record(0).producing[0] = BUILDING_AUTOMATED_FACTORY;
    TEST_BUILD_QUEUE::rejected_product = BUILDING_RESEARCH_LAB;
    Set_List_(0, {
        BUILD_QUEUE::INSTRUCTION_APPEND,
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_AUTOMATED_FACTORY),
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_RESEARCH_LAB),
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_SPACEPORT),
        BUILD_QUEUE::INSTRUCTION_END,
    });

    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 0));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == BUILDING_AUTOMATED_FACTORY));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == BUILDING_SPACEPORT));
    DOCTEST_CHECK((test_colonies::Record(0).producing[2] == COLONY_PRODUCTION_NONE));

    for (int slot = 0; slot < 7; ++slot) {
        test_colonies::Record(0).producing[slot] = BUILDING_ALIEN_CONTROL_CENTER + slot;
    }
    Set_List_(0, {
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_SPACE_ACADEMY),
        BUILD_QUEUE::INSTRUCTION_END,
    });
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 0));
    DOCTEST_CHECK((test_colonies::Record(0).producing[6] == BUILDING_ALIEN_CONTROL_CENTER + 6));
}

DOCTEST_TEST_CASE("continuous projects are unique and can be cleared explicitly") {
    TEST_BUILD_QUEUE::Reset_();
    test_colonies::Record(0).producing[0] = COLONY_PRODUCTION_HOUSING;
    test_colonies::Record(0).producing[1] = BUILDING_AUTOMATED_FACTORY;
    test_colonies::Record(0).producing[2] = COLONY_PRODUCTION_TRADE_GOODS;
    Set_List_(0, {
        BUILD_QUEUE::INSTRUCTION_APPEND,
        BUILD_QUEUE::INSTRUCTION_HOUSING,
        BUILD_QUEUE::INSTRUCTION_TRADE_GOODS,
        BUILD_QUEUE::INSTRUCTION_END,
    });

    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 0));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_HOUSING));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == BUILDING_AUTOMATED_FACTORY));
    DOCTEST_CHECK((test_colonies::Record(0).producing[2] == COLONY_PRODUCTION_TRADE_GOODS));
    DOCTEST_CHECK((test_colonies::Record(0).producing[3] == COLONY_PRODUCTION_NONE));

    Set_List_(0, {
        BUILD_QUEUE::INSTRUCTION_CLEAR_CONTINUOUS,
        BUILD_QUEUE::INSTRUCTION_PREPEND,
        BUILD_QUEUE::INSTRUCTION_HOUSING,
        BUILD_QUEUE::INSTRUCTION_END,
    });
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 0));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_HOUSING));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == BUILDING_AUTOMATED_FACTORY));
    DOCTEST_CHECK((test_colonies::Record(0).producing[2] == COLONY_PRODUCTION_NONE));
}

DOCTEST_TEST_CASE("clear continuous preserves a bought front product") {
    TEST_BUILD_QUEUE::Reset_();
    test_colonies::Record(0).producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
    test_colonies::Record(0).producing[1] = COLONY_PRODUCTION_HOUSING;
    test_colonies::Record(0).bought_outright = 50;
    Set_List_(0, {
        BUILD_QUEUE::INSTRUCTION_CLEAR_CONTINUOUS,
        BUILD_QUEUE::INSTRUCTION_END,
    });

    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 0));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_TRADE_GOODS));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == COLONY_PRODUCTION_NONE));
    DOCTEST_CHECK((test_colonies::Record(0).bought_outright == 50));
}

DOCTEST_TEST_CASE("autobuild queue values encode all presets without changing legacy mode") {
    DOCTEST_CHECK(BUILD_QUEUE::Encode_Autobuild_Preset(0) == 2);
    DOCTEST_CHECK(BUILD_QUEUE::Encode_Autobuild_Preset(10) == 12);
    DOCTEST_CHECK(BUILD_QUEUE::Decode_Autobuild_Preset(2) == 0);
    DOCTEST_CHECK(BUILD_QUEUE::Decode_Autobuild_Preset(12) == 10);
    DOCTEST_CHECK(BUILD_QUEUE::Decode_Autobuild_Preset(BUILD_QUEUE::AUTOBUILD_LEGACY) == -1);
    DOCTEST_CHECK(BUILD_QUEUE::Encode_Autobuild_Preset(11) == BUILD_QUEUE::AUTOBUILD_DISABLED);

    BUILD_QUEUE::autobuild_settings.enabled = 0;
    DOCTEST_CHECK(BUILD_QUEUE::Is_Legacy_Autobuild(12));
    BUILD_QUEUE::autobuild_settings.enabled = 1;
    DOCTEST_CHECK_FALSE(BUILD_QUEUE::Is_Legacy_Autobuild(12));
    DOCTEST_CHECK(BUILD_QUEUE::Is_Legacy_Autobuild(BUILD_QUEUE::AUTOBUILD_LEGACY));
}

DOCTEST_TEST_CASE("autobuild queue input accepts only complete queue numbers") {
    DOCTEST_CHECK(BUILD_QUEUE::Parse_Autobuild_Preset("") == -1);
    DOCTEST_CHECK(BUILD_QUEUE::Parse_Autobuild_Preset("0") == 0);
    DOCTEST_CHECK(BUILD_QUEUE::Parse_Autobuild_Preset("10") == 10);
    DOCTEST_CHECK(BUILD_QUEUE::Parse_Autobuild_Preset("01") == 1);
    DOCTEST_CHECK(BUILD_QUEUE::Parse_Autobuild_Preset("11") == -2);
    DOCTEST_CHECK(BUILD_QUEUE::Parse_Autobuild_Preset("1x") == -2);
}

DOCTEST_TEST_CASE("custom autobuild applies only when the experiment is enabled") {
    TEST_BUILD_QUEUE::Reset_();
    Set_List_(3, {BUILD_QUEUE::INSTRUCTION_HOUSING, BUILD_QUEUE::INSTRUCTION_END});
    test_colonies::Record(0).auto_building = BUILD_QUEUE::Encode_Autobuild_Preset(3);

    BUILD_QUEUE::autobuild_settings.enabled = 0;
    DOCTEST_CHECK_FALSE(BUILD_QUEUE::Apply_Custom_Autobuild(game::id_colony{0}));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_NONE));

    BUILD_QUEUE::autobuild_settings.enabled = 1;
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Custom_Autobuild(game::id_colony{0}));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_HOUSING));
    DOCTEST_CHECK(TEST_BUILD_QUEUE::colony_calculations == 0);
}

DOCTEST_TEST_CASE("custom autobuild applies local presets only to the committing player") {
    TEST_BUILD_QUEUE::Reset_();
    Set_List_(3, {BUILD_QUEUE::INSTRUCTION_HOUSING, BUILD_QUEUE::INSTRUCTION_END});
    BUILD_QUEUE::autobuild_settings.enabled = 1;
    test_colonies::Record(0).auto_building = BUILD_QUEUE::Encode_Autobuild_Preset(3);
    test_colonies::Record(1).auto_building = BUILD_QUEUE::Encode_Autobuild_Preset(3);
    test_colonies::Record(1).outpost_flag = 1;
    test_colonies::Record(2).owner = legacy_world::Decode_Player_Slot(1);
    test_colonies::Record(2).auto_building = BUILD_QUEUE::Encode_Autobuild_Preset(3);

    BUILD_QUEUE::Apply_Custom_Autobuild_For_Player(game::id_player{0});

    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_HOUSING));
    DOCTEST_CHECK((test_colonies::Record(1).producing[0] == COLONY_PRODUCTION_NONE));
    DOCTEST_CHECK((test_colonies::Record(2).producing[0] == COLONY_PRODUCTION_NONE));
    DOCTEST_CHECK((test_colonies::Record(3).producing[0] == COLONY_PRODUCTION_NONE));
}

DOCTEST_TEST_CASE("prepend and merge preserve deterministic build-list order") {
    TEST_BUILD_QUEUE::Reset_();
    test_colonies::Record(0).producing[0] = BUILDING_SPACEPORT;
    Set_List_(1, {
        BUILD_QUEUE::INSTRUCTION_PREPEND,
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_AUTOMATED_FACTORY),
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_RESEARCH_LAB),
        BUILD_QUEUE::INSTRUCTION_END,
    });
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 1));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == BUILDING_AUTOMATED_FACTORY));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == BUILDING_RESEARCH_LAB));
    DOCTEST_CHECK((test_colonies::Record(0).producing[2] == BUILDING_SPACEPORT));

    test_colonies::Record(0).producing[0] = BUILDING_SPACEPORT;
    test_colonies::Record(0).producing[1] = BUILDING_RESEARCH_LAB;
    test_colonies::Record(0).producing[2] = BUILDING_AUTOMATED_FACTORY;
    for (int slot = 3; slot < 7; ++slot) {
        test_colonies::Record(0).producing[slot] = COLONY_PRODUCTION_NONE;
    }
    Set_List_(1, {
        BUILD_QUEUE::INSTRUCTION_MERGE,
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_AUTOMATED_FACTORY),
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_RESEARCH_LAB),
        BUILD_QUEUE::INSTRUCTION_END,
    });
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 1));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == BUILDING_SPACEPORT));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == BUILDING_AUTOMATED_FACTORY));
    DOCTEST_CHECK((test_colonies::Record(0).producing[2] == BUILDING_RESEARCH_LAB));

    test_colonies::Record(0).producing[0] = BUILDING_SPACEPORT;
    test_colonies::Record(0).producing[1] = COLONY_PRODUCTION_NONE;
    test_colonies::Record(0).producing[2] = COLONY_PRODUCTION_NONE;
    Set_List_(1, {
        BUILD_QUEUE::INSTRUCTION_MERGE,
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_AUTOMATED_FACTORY),
        BUILD_QUEUE::INSTRUCTION_END,
    });
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 1));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == BUILDING_AUTOMATED_FACTORY));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == BUILDING_SPACEPORT));
}

DOCTEST_TEST_CASE("clear preserves bought products and refits while clear_refits removes refits") {
    TEST_BUILD_QUEUE::Reset_();
    test_colonies::Record(0).producing[0] = BUILDING_AUTOMATED_FACTORY;
    test_colonies::Record(0).bought_outright = 50;
    ship_slots::Resize(game::current_state, 1);
    const auto refit = game::Queued_Ship_Product(game::id_ship{90000});
    test_ships::Rows(game::current_state)[0].status = 4;
    test_ships::Rows(game::current_state).Set_Key_At_Row(0, game::id_ship{90000});
    test_colonies::Record(0).producing[1] = refit;
    test_colonies::Record(0).producing[2] = BUILDING_RESEARCH_LAB;
    Set_List_(2, {BUILD_QUEUE::INSTRUCTION_CLEAR, BUILD_QUEUE::INSTRUCTION_END});
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 2));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == BUILDING_AUTOMATED_FACTORY));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == refit));
    DOCTEST_CHECK((test_colonies::Record(0).producing[2] == COLONY_PRODUCTION_NONE));

    Set_List_(2, {BUILD_QUEUE::INSTRUCTION_CLEAR_REFITS, BUILD_QUEUE::INSTRUCTION_END});
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 2));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == COLONY_PRODUCTION_NONE));
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].status == 5));
}

DOCTEST_TEST_CASE("clearing a stale queued ship does not delete its row replacement") {
    TEST_BUILD_QUEUE::Reset_();
    auto& state = game::current_state;
    ship_slots::Resize(state, 1);
    test_ships::Rows(state).Set_Key_At_Row(0, game::id_ship{90001});
    test_ships::Rows(state)[0].status = 6;
    test_colonies::Record(0).producing[0] = game::Queued_Ship_Product(game::id_ship{90000});
    Set_List_(2, {BUILD_QUEUE::INSTRUCTION_CLEAR, BUILD_QUEUE::INSTRUCTION_END});
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 2));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_NONE));
    DOCTEST_CHECK(test_ships::Rows(state)[0].id == game::id_ship{90001});
    DOCTEST_CHECK((test_ships::Rows(state)[0].status == 6));
}

DOCTEST_TEST_CASE("merge commands affect only cells selected by the merge list") {
    TEST_BUILD_QUEUE::Reset_();
    test_colonies::Record(0).producing[0] = BUILDING_SPACEPORT;
    test_colonies::Record(0).producing[1] = BUILDING_RESEARCH_LAB;
    test_colonies::Record(0).production_spent = 100;
    Set_List_(2, {
        BUILD_QUEUE::INSTRUCTION_MERGE,
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_RESEARCH_LAB),
        BUILD_QUEUE::INSTRUCTION_BUY,
        BUILD_QUEUE::INSTRUCTION_CLEAR,
        BUILD_QUEUE::INSTRUCTION_END,
    });

    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 2));
    DOCTEST_CHECK((test_colonies::Record(0).bought_outright == 0));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == BUILDING_SPACEPORT));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == COLONY_PRODUCTION_NONE));
}

DOCTEST_TEST_CASE("stop ignores trailing products and buying respects thresholds") {
    TEST_BUILD_QUEUE::Reset_();
    Set_List_(3, {
        BUILD_QUEUE::INSTRUCTION_HOUSING,
        BUILD_QUEUE::INSTRUCTION_STOP,
        BUILD_QUEUE::INSTRUCTION_TRADE_GOODS,
        BUILD_QUEUE::INSTRUCTION_END,
    });
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 3));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_HOUSING));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == COLONY_PRODUCTION_NONE));

    test_colonies::Record(0).producing[0] = BUILDING_AUTOMATED_FACTORY;
    test_colonies::Record(0).production_spent = 9;
    Set_List_(3, {BUILD_QUEUE::INSTRUCTION_BUY10, BUILD_QUEUE::INSTRUCTION_END});
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 3));
    DOCTEST_CHECK((test_colonies::Record(0).bought_outright == 0));
    test_colonies::Record(0).production_spent = 10;
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 3));
    DOCTEST_CHECK((test_colonies::Record(0).bought_outright == 90));
}

DOCTEST_TEST_CASE("repeat follows only repeatable products and ship allocation failure is harmless") {
    TEST_BUILD_QUEUE::Reset_();
    Set_List_(5, {
        static_cast<uint8_t>(BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_RESEARCH_LAB),
        BUILD_QUEUE::INSTRUCTION_REPEAT,
        BUILD_QUEUE::INSTRUCTION_SPY,
        BUILD_QUEUE::INSTRUCTION_REPEAT,
        BUILD_QUEUE::INSTRUCTION_END,
    });
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 5));
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == BUILDING_RESEARCH_LAB));
    DOCTEST_CHECK((test_colonies::Record(0).producing[1] == COLONY_PRODUCTION_SPY));
    DOCTEST_CHECK((test_colonies::Record(0).producing[2] == COLONY_PRODUCTION_REPEAT));

    TEST_BUILD_QUEUE::next_ship = -1;
    Set_List_(5, {
        BUILD_QUEUE::INSTRUCTION_DESIGN1,
        BUILD_QUEUE::INSTRUCTION_END,
    });
    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 5));
    DOCTEST_CHECK((test_colonies::Record(0).producing[3] == COLONY_PRODUCTION_NONE));
}

DOCTEST_TEST_CASE("generated lander designs never overwrite the sixth design") {
    const uint8_t instructions[] = {
        BUILD_QUEUE::INSTRUCTION_COLONY_SHIP,
        BUILD_QUEUE::INSTRUCTION_OUTPOST_SHIP,
        BUILD_QUEUE::INSTRUCTION_TRANSPORT,
    };
    const int8_t ship_types[] = {
        SHIP_TYPE_COLONY,
        SHIP_TYPE_OUTPOST,
        SHIP_TYPE_TRANSPORT,
    };

    for (int i = 0; i < 3; ++i) {
        TEST_BUILD_QUEUE::Reset_();
        s_ship_design& sixth_design =
            game::current_state.players[0].ship_designs[SHIP_SIZE_DOOM_STAR];
        memset(&sixth_design, 0x5a, sizeof(sixth_design));
        const s_ship_design expected = sixth_design;

        game::current_state.players[0].tech_applications[TECH_APP_COLONY_SHIP] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        game::current_state.players[0].tech_applications[TECH_APP_OUTPOST_SHIP] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        game::current_state.players[0].tech_applications[TECH_APP_TRANSPORT] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        test_colonies::Record(0).buildings[BUILDING_MARINE_BARRACKS] = 1;

        Set_List_(5, {instructions[i], BUILD_QUEUE::INSTRUCTION_END});
        DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 5));
        DOCTEST_CHECK(sixth_design == expected);
        DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].d.ship_type == ship_types[i]));
        DOCTEST_CHECK(game::Queued_Ship_Key(test_colonies::Record(0).producing[0]) == game::id_ship{90000});
    }
}

DOCTEST_TEST_CASE("failed generated ship allocation preserves the sixth design") {
    TEST_BUILD_QUEUE::Reset_();
    s_ship_design& sixth_design =
        game::current_state.players[0].ship_designs[SHIP_SIZE_DOOM_STAR];
    memset(&sixth_design, 0xa5, sizeof(sixth_design));
    const s_ship_design expected = sixth_design;

    game::current_state.players[0].tech_applications[TECH_APP_COLONY_SHIP] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    TEST_BUILD_QUEUE::next_ship = -1;
    Set_List_(5, {
        BUILD_QUEUE::INSTRUCTION_COLONY_SHIP,
        BUILD_QUEUE::INSTRUCTION_END,
    });

    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 5));
    DOCTEST_CHECK(sixth_design == expected);
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_NONE));
}

DOCTEST_TEST_CASE("unbuy applies a low-32-bit treasury refund") {
    TEST_BUILD_QUEUE::Reset_();
    test_colonies::Record(0).producing[0] = BUILDING_AUTOMATED_FACTORY;
    test_colonies::Record(0).production_spent = 50;
    test_colonies::Record(0).bought_outright = 50;
    game::current_state.players[0].bc = INT32_MAX;
    Set_List_(6, {BUILD_QUEUE::INSTRUCTION_UNBUY, BUILD_QUEUE::INSTRUCTION_END});

    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 6));
    DOCTEST_CHECK((game::current_state.players[0].bc == INT32_MIN + 99));
    DOCTEST_CHECK((test_colonies::Record(0).bought_outright == 0));
}

DOCTEST_TEST_CASE("unbuy refunds the original purchase snapshot") {
    TEST_BUILD_QUEUE::Reset_();
    test_colonies::Record(0).producing[0] = BUILDING_AUTOMATED_FACTORY;
    test_colonies::Record(0).production_spent = 10;
    test_colonies::Record(0).bought_outright = 40;
    game::current_state.players[0].bc = 100;
    Set_List_(6, {BUILD_QUEUE::INSTRUCTION_UNBUY, BUILD_QUEUE::INSTRUCTION_END});

    DOCTEST_REQUIRE(BUILD_QUEUE::Apply_Preset_To_Colony(game::id_colony{0}, 6));
    DOCTEST_CHECK((game::current_state.players[0].bc == 225));
    DOCTEST_CHECK((test_colonies::Record(0).bought_outright == 0));
}

DOCTEST_TEST_CASE("empire application skips outposts and other players") {
    TEST_BUILD_QUEUE::Reset_();
    test_colonies::Record(1).outpost_flag = 1;
    test_colonies::Record(2).owner = legacy_world::Decode_Player_Slot(1);
    Set_List_(4, {BUILD_QUEUE::INSTRUCTION_HOUSING, BUILD_QUEUE::INSTRUCTION_END});

    BUILD_QUEUE::Apply_Preset_To_Empire(game::id_player{0}, 4);
    DOCTEST_CHECK((test_colonies::Record(0).producing[0] == COLONY_PRODUCTION_HOUSING));
    DOCTEST_CHECK((test_colonies::Record(1).producing[0] == COLONY_PRODUCTION_NONE));
    DOCTEST_CHECK((test_colonies::Record(2).producing[0] == COLONY_PRODUCTION_NONE));
    DOCTEST_CHECK((test_colonies::Record(3).producing[0] == COLONY_PRODUCTION_HOUSING));
    DOCTEST_CHECK(TEST_BUILD_QUEUE::player_calculations == 1);
}

}
