#include "pch.h"

#include <cstring>

#include "game/build_queue.h"
#include "doctest/doctest.h"

namespace TEST_COLCALC_BASE {
    void Reset_();
    void Set_Breakthrough_(int32_t chance, uint32_t roll, int32_t cost);
    int Give_Player_Field_Calls_();
}

DOCTEST_TEST_SUITE("colony production helpers") {

DOCTEST_TEST_CASE("food half-units retain fractional output") {
    struct s_food_case {
        uint16_t food2;
        const char* expected;
    };

    const s_food_case cases[] = {
        {0, "0"},
        {1, "0.5"},
        {2, "1"},
        {3, "1.5"},
    };

    for (const s_food_case& test_case : cases) {
        char text[16];
        COLCALC::Format_Food2(test_case.food2, text, sizeof(text));
        DOCTEST_CHECK(std::strcmp(text, test_case.expected) == 0);
    }
}

DOCTEST_TEST_CASE("production purchase cost follows the original tiered curve") {
    DOCTEST_CHECK(COLCALC::Production_Cost_To_Buy(100, 0) == 400);
    DOCTEST_CHECK(COLCALC::Production_Cost_To_Buy(100, 10) == 300);
    DOCTEST_CHECK(COLCALC::Production_Cost_To_Buy(100, 49) == 105);
    DOCTEST_CHECK(COLCALC::Production_Cost_To_Buy(100, 50) == 100);
    DOCTEST_CHECK(COLCALC::Production_Cost_To_Buy(100, 99) == 2);
    DOCTEST_CHECK(COLCALC::Production_Cost_To_Buy(100, 100) == 0);
}

DOCTEST_TEST_CASE("building product classification excludes sentinels and upper boundary") {
    DOCTEST_CHECK_FALSE(COLBLDG::Colony_Production_Is_Building(BUILDING_NO_BUILDING));
    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Building(BUILDING_ALIEN_CONTROL_CENTER));
    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Building(BUILDING_ARTIFICIAL_PLANET));
    DOCTEST_CHECK_FALSE(COLBLDG::Colony_Production_Is_Building(BUILDING_COUNT));
    DOCTEST_CHECK_FALSE(COLBLDG::Colony_Production_Is_Building(COLONY_PRODUCTION_NONE));
}

DOCTEST_TEST_CASE("ship product classification preserves queued and design ranges") {
    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Queued_Ship(-101));
    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Queued_Ship(-100));
    DOCTEST_CHECK_FALSE(COLBLDG::Colony_Production_Is_Queued_Ship(-99));

    DOCTEST_CHECK_FALSE(COLBLDG::Colony_Production_Is_Ship_Design(-100));
    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Ship_Design(-99));
    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Ship_Design(-50));
    DOCTEST_CHECK_FALSE(COLBLDG::Colony_Production_Is_Ship_Design(-49));

    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Ship(-101));
    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Ship(-100));
    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Ship(-99));
    DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Ship(-50));
    DOCTEST_CHECK_FALSE(COLBLDG::Colony_Production_Is_Ship(-49));
}

DOCTEST_TEST_CASE("ship product classification matches the original binary range") {
    for (int32_t production_id = INT16_MIN; production_id <= INT16_MAX; ++production_id) {
        const bool original_is_ship = production_id <= COLONY_PRODUCTION_SHIP_DESIGN_BASE;
        DOCTEST_CHECK(
            COLBLDG::Colony_Production_Is_Ship(static_cast<int16_t>(production_id)) == original_is_ship);
    }
}

DOCTEST_TEST_CASE("ship product encodings round trip through their matching ranges") {
    for (int16_t ship_idx = 0; ship_idx < MAX_SHIPS; ++ship_idx) {
        const int16_t production_id = COLBLDG::Colony_Production_From_Ship_Index(ship_idx);
        DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Queued_Ship(production_id));
        DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Ship(production_id));
        DOCTEST_CHECK(COLBLDG::Colony_Production_Ship_Index(production_id) == ship_idx);
    }

    for (int16_t design_idx : {int16_t{0}, int16_t{1}, int16_t{49}}) {
        const int16_t production_id = COLBLDG::Colony_Production_From_Ship_Design_Index(design_idx);
        DOCTEST_CHECK(COLBLDG::Colony_Production_Is_Ship_Design(production_id));
        DOCTEST_CHECK(COLBLDG::Colony_Production_Ship_Design_Index(production_id) == design_idx);
    }
}

DOCTEST_TEST_CASE("planet summary gravity penalties match colony production rules") {
    struct s_gravity_case {
        int8_t low_g_trait;
        int8_t heavy_g_trait;
        int16_t planet_gravity;
        int16_t expected_penalty;
    };

    const s_gravity_case cases[] = {
        {1, 0, PLANET_GRAVITY_LOW_G, 0},
        {1, 0, PLANET_GRAVITY_NORMAL_G, 25},
        {1, 0, PLANET_GRAVITY_HEAVY_G, 50},
        {0, 0, PLANET_GRAVITY_LOW_G, 25},
        {0, 0, PLANET_GRAVITY_NORMAL_G, 0},
        {0, 0, PLANET_GRAVITY_HEAVY_G, 50},
        {0, 1, PLANET_GRAVITY_LOW_G, 25},
        {0, 1, PLANET_GRAVITY_NORMAL_G, 0},
        {0, 1, PLANET_GRAVITY_HEAVY_G, 0},
    };

    TEST_COLCALC_BASE::Reset_();

    for (const s_gravity_case& test_case : cases) {
        game::current_state.players[0].traits[TRAIT_LOW_G_WORLD] = test_case.low_g_trait;
        game::current_state.players[0].traits[TRAIT_HEAVY_G_WORLD] = test_case.heavy_g_trait;

        DOCTEST_CHECK(
            PLNTSUM::Productivity_Penalty(game::id_player{0}, test_case.planet_gravity) ==
            test_case.expected_penalty);
    }
}

DOCTEST_TEST_CASE("base production reflects recalculated colony fields") {
    TEST_COLCALC_BASE::Reset_();

    s_colony colony = {};
    colony.owner = legacy_world::Decode_Player_Slot(0);
    colony.food2_per_farmer = 8;
    colony.industry_per_worker = 7;
    colony.research_per_scientist = 6;

    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Food2_Produced(&colony, game::id_player{0}, 0, nullptr) == 8);
    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Industry_Produced(&colony, game::id_player{0}, 0, nullptr) == 7);
    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Research_Produced(&colony, game::id_player{0}, 0, nullptr) == 6);

    colony.food2_per_farmer = 4;
    colony.industry_per_worker = 3;
    colony.research_per_scientist = 2;

    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Food2_Produced(&colony, game::id_player{0}, 0, nullptr) == 4);
    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Industry_Produced(&colony, game::id_player{0}, 0, nullptr) == 3);
    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Research_Produced(&colony, game::id_player{0}, 0, nullptr) == 2);
}

DOCTEST_TEST_CASE("Aquatic Tundra food uses the configured climate treatment") {
    TEST_COLCALC_BASE::Reset_();

    game::current_state.players[0].traits[TRAIT_AQUATIC] = 1;
    MOX::food_per_farmer_table[PLANET_CLIMATE_TUNDRA] = 2;
    MOX::food_per_farmer_table[PLANET_CLIMATE_SWAMP] = 4;
    MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN] = 6;

    s_colony colony = {};
    colony.owner = legacy_world::Decode_Player_Slot(0);
    colony.climate = PLANET_CLIMATE_TUNDRA;
    colony.food2_per_farmer = 4;

    DOCTEST_CHECK(
        COLCALC::Colony_Empire_Base_Food2_Produced(&colony, game::id_player{0}, 0, nullptr) == 8);

    COLCALC::aquatic_treats_tundra_as_swamp = 1;

    DOCTEST_CHECK(
        COLCALC::Colony_Empire_Base_Food2_Produced(&colony, game::id_player{0}, 0, nullptr) == 6);
}

DOCTEST_TEST_CASE("configured native and Android farmer bonuses use half-food units") {
    TEST_COLCALC_BASE::Reset_();

    s_colony colony = {};
    colony.owner = legacy_world::Decode_Player_Slot(0);
    colony.food2_per_farmer = 2;
    COLCALC::food_farmer_bonus.native = 3;
    COLCALC::food_farmer_bonus.android_farmer = 5;

    DOCTEST_CHECK(
        COLCALC::Colony_Empire_Base_Food2_Produced(&colony, game::id_player{0}, 9, nullptr) == 5);
    DOCTEST_CHECK(
        COLCALC::Colony_Empire_Base_Food2_Produced(&colony, game::id_player{0}, 8, nullptr) == 7);
}

DOCTEST_TEST_CASE("research production is banked when no field is selected") {
    struct s_research_case {
        int32_t accumulated;
        int16_t produced;
        int32_t expected;
    };

    const s_research_case cases[] = {
        {0, 6, 6},
        {17, 6, 23},
        {17, -6, 11},
    };

    for (const s_research_case& test_case : cases) {
        s_player player = {};
        player.current_research_field = 0;
        player.research_accumulated = test_case.accumulated;
        player.research_produced = test_case.produced;

        COLCALC::Check_For_Research_Breakthrough(&player);

        DOCTEST_CHECK((player.research_accumulated == test_case.expected));
    }
}

DOCTEST_TEST_CASE("research completion preserves configured points and marks selection pending") {
    s_player player = {};
    player.research_accumulated = 275;

    COLCALC::Resolve_Research_Completion(&player, true, 200);

    DOCTEST_CHECK(player.research_breakthrough == 1);
    DOCTEST_CHECK((player.research_accumulated == 75));

    COLCALC::Resolve_Research_Completion(&player, true, 100);
    DOCTEST_CHECK((player.research_accumulated == 0));

    player.research_accumulated = 75;
    COLCALC::Resolve_Research_Completion(&player, false, 0);
    DOCTEST_CHECK((player.research_accumulated == 0));
}

DOCTEST_TEST_CASE("configured breakthroughs retain research above twice the field cost") {
    TEST_COLCALC_BASE::Reset_();
    TEST_COLCALC_BASE::Set_Breakthrough_(100, 1, 100);
    COLCALC::keep_research_on_breakthrough = 1;

    s_player player = {};
    player.current_research_field = 1;
    player.research_accumulated = 250;
    player.research_produced = 25;

    COLCALC::Check_For_Research_Breakthrough(&player);

    DOCTEST_CHECK(player.research_breakthrough == 1);
    DOCTEST_CHECK((player.research_accumulated == 75));
    DOCTEST_CHECK(TEST_COLCALC_BASE::Give_Player_Field_Calls_() == 1);
}

DOCTEST_TEST_CASE("high-cost breakthroughs clamp retained research without overflow") {
    TEST_COLCALC_BASE::Reset_();
    TEST_COLCALC_BASE::Set_Breakthrough_(100, 1, 1200000000);
    COLCALC::keep_research_on_breakthrough = 1;

    s_player player = {};
    player.current_research_field = 1;
    player.research_accumulated = 1211975000;
    player.research_produced = 25000;

    COLCALC::Check_For_Research_Breakthrough(&player);

    DOCTEST_CHECK(player.research_breakthrough == 1);
    DOCTEST_CHECK((player.research_accumulated == 0));
    DOCTEST_CHECK(TEST_COLCALC_BASE::Give_Player_Field_Calls_() == 1);
}

DOCTEST_TEST_CASE("classic breakthroughs spend all accumulated research") {
    TEST_COLCALC_BASE::Reset_();
    TEST_COLCALC_BASE::Set_Breakthrough_(100, 1, 100);

    s_player player = {};
    player.current_research_field = 1;
    player.research_accumulated = 250;
    player.research_produced = 25;

    COLCALC::Check_For_Research_Breakthrough(&player);

    DOCTEST_CHECK(player.research_breakthrough == 1);
    DOCTEST_CHECK((player.research_accumulated == 0));
}

DOCTEST_TEST_CASE("fixed research cost completes only at twice the field cost") {
    TEST_COLCALC_BASE::Reset_();
    TEST_COLCALC_BASE::Set_Breakthrough_(100, 1, 100);
    COLCALC::fixed_research_cost = 1;

    s_player player = {};
    player.current_research_field = 1;
    player.research_accumulated = 174;
    player.research_produced = 25;

    COLCALC::Check_For_Research_Breakthrough(&player);

    DOCTEST_CHECK(player.research_breakthrough == 0);
    DOCTEST_CHECK((player.research_accumulated == 199));
    DOCTEST_CHECK(TEST_COLCALC_BASE::Give_Player_Field_Calls_() == 0);

    player.research_produced = 1;
    COLCALC::Check_For_Research_Breakthrough(&player);

    DOCTEST_CHECK(player.research_breakthrough == 1);
    DOCTEST_CHECK((player.research_accumulated == 0));
    DOCTEST_CHECK(TEST_COLCALC_BASE::Give_Player_Field_Calls_() == 1);
    COLCALC::fixed_research_cost = 0;
}

DOCTEST_TEST_CASE("pending research selection survives turns without a breakthrough") {
    TEST_COLCALC_BASE::Reset_();
    TEST_COLCALC_BASE::Set_Breakthrough_(0, 100, 100);

    s_player player = {};
    player.current_research_field = 1;
    player.research_breakthrough = 1;
    player.research_accumulated = 25;
    player.research_produced = 5;

    COLCALC::Check_For_Research_Breakthrough(&player);

    DOCTEST_CHECK(player.research_breakthrough == 1);
    DOCTEST_CHECK((player.research_accumulated == 30));
    DOCTEST_CHECK(TEST_COLCALC_BASE::Give_Player_Field_Calls_() == 0);
}

DOCTEST_TEST_CASE("build queue hotkeys map to all eleven numeric presets") {
    DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Hotkey('q') == 0);
    DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Hotkey('Q') == 0);
    for (char digit = '1'; digit <= '9'; ++digit) {
        DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Hotkey(digit) == digit - '0');
    }
    DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Hotkey('0') == 10);
    DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Hotkey('x') == -1);
}

DOCTEST_TEST_CASE("shifted digits map to empire build queue presets") {
    DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Empire_Hotkey('q') == 0);
    DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Empire_Hotkey('Q') == 0);
    constexpr char shifted[] = "!@#$%^&*()";
    for (int index = 0; index < 9; ++index) {
        DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Empire_Hotkey(shifted[index]) == index + 1);
    }
    DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Empire_Hotkey(')') == 10);
    DOCTEST_CHECK(BUILD_QUEUE::Preset_For_Empire_Hotkey('1') == -1);
}

}
