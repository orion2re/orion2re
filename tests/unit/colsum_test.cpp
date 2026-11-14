#include "pch.h"

#include "doctest/doctest.h"
#include "game/colsum.h"

namespace TEST_COLONY_SCAN_INFO {
    void Reset_();
    const char* Output_();
    void Set_Product_Cost_(int32_t product_cost);
    void Set_Breakthrough_Chance_(int32_t chance);
    void Capture_Empire_Info_();
}

namespace {
    s_colony* Prepare_Colony_() {
        TEST_COLONY_SCAN_INFO::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->planet = 0;
        colony->climate = PLANET_CLIMATE_TERRAN;
        colony->n_pops = 7;
        colony->pop_roundoff[0] = 250;
        colony->pop_growth[0] = 1500;
        colony->production[ECON_INDUSTRY] = 18;
        colony->maintenance[ECON_INDUSTRY] = 5;
        colony->production_spent = 40;
        colony->bought_outright = 10;

        s_planet_data* planet = &MOX::planet[0];
        planet->size = PLANET_SIZE_MEDIUM;
        planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
        planet->mineral_class = MINERAL_RESOURCE_RICH;

        s_player* player = &MOX::player[0];
        player->settlers_freighted = 3;
        SETTLER::Set_Dest_Planet(&player->settlers[0].val, 0);
        SETTLER::Set_Dest_Planet(&player->settlers[1].val, 0);
        SETTLER::Set_Dest_Planet(&player->settlers[2].val, 1);

        TEST_COLONY_SCAN_INFO::Set_Product_Cost_(120);
        return colony;
    }

    void Set_Colony_Count_(int16_t count) {
        TEST_COLONY_SCAN_INFO::Reset_();
        for (int16_t i = 0; i < MAX_COLONIES; ++i) {
            MOX::colony[i].owner = (i < count) ? 0 : 1;
            MOX::colony[i].outpost_flag = 0;
        }
    }
}

DOCTEST_TEST_CASE("stored production comparison is descending and ignores purchased production") {
    TEST_COLONY_SCAN_INFO::Reset_();
    COLSUM::g_sort_index = 7;
    MOX::colony[0].production_spent = 32767;
    MOX::colony[1].production_spent = 0;
    MOX::colony[1].bought_outright = 32767;

    DOCTEST_CHECK(COLSUM::Switched_cmp(0, 1) < 0);
    DOCTEST_CHECK(COLSUM::Switched_cmp(1, 0) > 0);
    DOCTEST_CHECK(COLSUM::Switched_cmp(0, 0) == 0);
    DOCTEST_CHECK(COLSUM::Switched_cmp(-1, 0) == 0);
    DOCTEST_CHECK(COLSUM::Switched_cmp(0, -1) == 0);

    COLSUM::g_sort_index = 0;
}

DOCTEST_TEST_CASE("stored production sort keeps ties stable and allows switching to industry") {
    TEST_COLONY_SCAN_INFO::Reset_();
    int16_t colony_list[MAX_COLONIES];
    std::fill_n(colony_list, MAX_COLONIES, static_cast<int16_t>(-1));
    for (int16_t i = 0; i < 4; ++i) {
        colony_list[i] = i;
        MOX::colony[i].production[ECON_INDUSTRY] = i;
    }
    MOX::colony[0].production_spent = 0;
    MOX::colony[1].production_spent = 100;
    MOX::colony[2].production_spent = 32767;
    MOX::colony[3].production_spent = 100;
    MOX::colony[0].bought_outright = 32767;

    int16_t* saved_list = COLXPORT::g_colony_list_ptr;
    COLXPORT::g_colony_list_ptr = colony_list;
    COLSUM::g_sort_index = 7;
    COLSUM::Sort_Col_List();

    DOCTEST_CHECK(colony_list[0] == 2);
    DOCTEST_CHECK(colony_list[1] == 1);
    DOCTEST_CHECK(colony_list[2] == 3);
    DOCTEST_CHECK(colony_list[3] == 0);
    for (int i = 4; i < MAX_COLONIES; ++i) {
        DOCTEST_CHECK(colony_list[i] == -1);
    }

    COLSUM::g_sort_index = 3;
    COLSUM::Sort_Col_List();
    for (int i = 0; i < 4; ++i) {
        DOCTEST_CHECK(colony_list[i] == 3 - i);
    }

    COLXPORT::g_colony_list_ptr = saved_list;
    COLSUM::g_sort_index = 0;
}

DOCTEST_TEST_CASE("colony summary displays inbound population and production details") {
    Prepare_Colony_();

    COLSUM::Draw_Colony_Scan_Info();

    DOCTEST_CHECK(std::string(TEST_COLONY_SCAN_INFO::Output_()) ==
        "Medium Terran \nNormal Gravity\nMineral Rich\n"
        "Population (7/20)\nEnroute (2)\n"
        "250k +1500k (1)\n50/120 +13");
}

DOCTEST_TEST_CASE("colony summary preserves negative population and net production behavior") {
    s_colony* colony = Prepare_Colony_();
    colony->pop_roundoff[0] = -250;
    colony->pop_growth[0] = -750;
    colony->production[ECON_INDUSTRY] = 3;
    colony->maintenance[ECON_INDUSTRY] = 5;

    COLSUM::Draw_Colony_Scan_Info();

    DOCTEST_CHECK(std::string(TEST_COLONY_SCAN_INFO::Output_()) ==
        "\x1B" "2Medium Terran \nNormal Gravity\nMineral Rich\n"
        "Population (7/20)\nEnroute (\x1B" "22)\n"
        "-250k -750k (-2)\n50/120 +0");
}

DOCTEST_TEST_CASE("empire info preserves the classic population row without freighted population") {
    TEST_COLONY_SCAN_INFO::Reset_();
    TEST_COLONY_SCAN_INFO::Capture_Empire_Info_();
    MOX::player[0].total_pop = 42;

    COLSUM::Draw_Empire_Info();

    DOCTEST_CHECK(std::string(TEST_COLONY_SCAN_INFO::Output_()) ==
        "BC: 0\nIncome: 0\nPopulation: 42\n"
        "Freighters: 0\nFood: 0\nResearch: 0");
}

DOCTEST_TEST_CASE("empire info displays resident and freighted population with fitted labels") {
    struct s_population_case {
        int16_t total_population;
        int16_t freighted_population;
        const char* expected_row;
    };

    const s_population_case cases[] = {
        {42, 3, "Population: 42+3"},
        {999, 9, "Population: 999+9"},
        {1000, 9, "Pop: 1000+9"},
        {50, 10, "Population: 50+10"},
        {99, 99, "Population: 99+99"},
        {99, 100, "Pop: 99+100"},
    };

    for (const s_population_case& test_case : cases) {
        TEST_COLONY_SCAN_INFO::Reset_();
        TEST_COLONY_SCAN_INFO::Capture_Empire_Info_();
        MOX::player[0].total_pop = test_case.total_population;
        MOX::player[0].settlers_freighted = test_case.freighted_population;

        COLSUM::Draw_Empire_Info();

        DOCTEST_CHECK(std::string(TEST_COLONY_SCAN_INFO::Output_()).find(test_case.expected_row) !=
                      std::string::npos);
    }
}

DOCTEST_TEST_CASE("Empire Info displays configured research values") {
    Prepare_Colony_();
    s_player* player = &MOX::player[0];
    player->research_produced = 25;
    player->research_accumulated = 60;
    player->current_research_field = 1;
    TEST_COLONY_SCAN_INFO::Set_Product_Cost_(100);
    TEST_COLONY_SCAN_INFO::Set_Breakthrough_Chance_(15);

    struct s_research_value_case {
        uint8_t mode;
        const char* expected;
    };

    const s_research_value_case cases[] = {
        {COLSUM::RESEARCH_VALUE_CLASSIC, "25"},
        {COLSUM::RESEARCH_VALUE_ACCUMULATED, "85"},
        {COLSUM::RESEARCH_VALUE_REMAINING, "115"},
        {COLSUM::RESEARCH_VALUE_PERCENTAGE_1, "15%"},
        {COLSUM::RESEARCH_VALUE_PERCENTAGE_2, "15%"},
    };

    for (const s_research_value_case& test_case : cases) {
        COLSUM::print_research_value = test_case.mode;
        COLSUM::Draw_Empire_Info();
        DOCTEST_CHECK(std::string(TEST_COLONY_SCAN_INFO::Output_()) ==
                      test_case.expected);
    }
}

DOCTEST_TEST_CASE("Empire Info percentage modes preserve zero-chance fallbacks") {
    Prepare_Colony_();
    s_player* player = &MOX::player[0];
    player->research_produced = 25;
    player->research_accumulated = 60;
    player->current_research_field = 1;
    TEST_COLONY_SCAN_INFO::Set_Product_Cost_(100);
    TEST_COLONY_SCAN_INFO::Set_Breakthrough_Chance_(0);

    COLSUM::print_research_value = COLSUM::RESEARCH_VALUE_PERCENTAGE_1;
    COLSUM::Draw_Empire_Info();
    DOCTEST_CHECK(std::string(TEST_COLONY_SCAN_INFO::Output_()) == "25");

    COLSUM::print_research_value = COLSUM::RESEARCH_VALUE_PERCENTAGE_2;
    COLSUM::Draw_Empire_Info();
    DOCTEST_CHECK(std::string(TEST_COLONY_SCAN_INFO::Output_()) == "15");
}

DOCTEST_TEST_CASE("colonies list opens at the top when there are more colonies than visible rows") {
    Set_Colony_Count_(21);
    DOCTEST_CHECK(COLXPORT::N_Colonies(MOX::PLAYER_NUM) == 21);

    COLSUM::first = 0;
    COLSUM::slider_bar_position = 0;

    COLSUM::Reset_List_Scroll();
    DOCTEST_CHECK(COLSUM::first == 0);
    DOCTEST_CHECK(COLSUM::slider_bar_position == 11);

    COLSUM::Update_First(10, &COLSUM::first, &COLSUM::slider_bar_position);
    DOCTEST_CHECK(COLSUM::first == 0);

    TEST_COLONY_SCAN_INFO::Reset_();
}

DOCTEST_TEST_CASE("colonies list scroll stays disabled with fewer colonies than visible rows") {
    Set_Colony_Count_(5);
    DOCTEST_CHECK(COLXPORT::N_Colonies(MOX::PLAYER_NUM) == 5);

    COLSUM::Reset_List_Scroll();
    DOCTEST_CHECK(COLSUM::first == 0);
    DOCTEST_CHECK(COLSUM::slider_bar_position == -1);

    COLSUM::Update_First(10, &COLSUM::first, &COLSUM::slider_bar_position);
    DOCTEST_CHECK(COLSUM::first == 0);
    DOCTEST_CHECK(COLSUM::slider_bar_position == -1);

    TEST_COLONY_SCAN_INFO::Reset_();
}
