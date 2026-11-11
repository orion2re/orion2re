#include "pch.h"

#include "doctest/doctest.h"
#include "game/npcdiplo.h"

namespace TEST_NPCDIPLO {
    void Reset_();
    void Set_Player_May_Get_Tech_(bool may_get);
    void Set_Tech_Value_(int32_t value);
    void Set_Tech_Value_For_App_(int16_t app, int32_t value);
}

namespace {
    void Prepare_Human_Trade_() {
        TEST_NPCDIPLO::Reset_();
        MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        MOX::player[1].objectives = PLAYER_OBJECTIVE_HUMAN;
        MOX::player[1].tech_applications[0] = TECH_RESEARCH_STATUS_RESEARCHED;
    }

    int16_t Get_Differential_Tech_Count_(
        uint8_t* tech_indices,
        int32_t* tech_values
    ) {
        int16_t tech_count = 0;
        npcdiplo::Get_Differential_Tech_List(
            0, 1, 0, UINT32_MAX, tech_indices, tech_values, &tech_count, 0);
        return tech_count;
    }
}

DOCTEST_TEST_CASE("human technology trade includes eligible zero-valued technology") {
    Prepare_Human_Trade_();
    uint8_t tech_indices[TECH_APP_COUNT] = {};
    int32_t tech_values[TECH_APP_COUNT] = {};

    int16_t tech_count = Get_Differential_Tech_Count_(tech_indices, tech_values);

    DOCTEST_REQUIRE(tech_count == 1);
    DOCTEST_CHECK(tech_indices[0] == 0);
    DOCTEST_CHECK(tech_values[0] == 1);
}

DOCTEST_TEST_CASE("human technology trade preserves hard zero results") {
    Prepare_Human_Trade_();
    uint8_t tech_indices[TECH_APP_COUNT] = {};
    int32_t tech_values[TECH_APP_COUNT] = {};

    TEST_NPCDIPLO::Set_Player_May_Get_Tech_(false);
    DOCTEST_CHECK(Get_Differential_Tech_Count_(tech_indices, tech_values) == 0);

    TEST_NPCDIPLO::Set_Player_May_Get_Tech_(true);
    MOX::player[0].tech_applications[0] = TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_CHECK(Get_Differential_Tech_Count_(tech_indices, tech_values) == 0);
}

DOCTEST_TEST_CASE("AI technology trade still omits zero-valued technology") {
    Prepare_Human_Trade_();
    MOX::player[1].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    uint8_t tech_indices[TECH_APP_COUNT] = {};
    int32_t tech_values[TECH_APP_COUNT] = {};

    DOCTEST_CHECK(Get_Differential_Tech_Count_(tech_indices, tech_values) == 0);
}

DOCTEST_TEST_CASE("human technology trade preserves nonzero valuation") {
    Prepare_Human_Trade_();
    TEST_NPCDIPLO::Set_Tech_Value_(137);
    uint8_t tech_indices[TECH_APP_COUNT] = {};
    int32_t tech_values[TECH_APP_COUNT] = {};

    int16_t tech_count = Get_Differential_Tech_Count_(tech_indices, tech_values);

    DOCTEST_REQUIRE(tech_count == 1);
    DOCTEST_CHECK(tech_values[0] == 137);
}

DOCTEST_TEST_CASE("AI surrender delay gates surrender evaluation") {
    struct s_delay_scope {
        uint16_t saved = npcdiplo::ai_surrender_delay;
        ~s_delay_scope() { npcdiplo::ai_surrender_delay = saved; }
    } scope;
    TEST_NPCDIPLO::Reset_();
    MOX::player[0].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    MOX::player[1].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    MOX::player[0].total_pop = 1;
    MOX::player[1].total_pop = 100;
    MOX::player[0].contact[1] = 1;

    npcdiplo::ai_surrender_delay = 150;
    MOX::stardate = 35149;
    DOCTEST_CHECK(npcdiplo::NPC_Surrender_Check(0) == -1);

    npcdiplo::ai_surrender_delay = 149;
    DOCTEST_CHECK(npcdiplo::NPC_Surrender_Check(0) == 1);
}

DOCTEST_TEST_CASE("technology trade sorts multiple eligible applications by value") {
    TEST_NPCDIPLO::Reset_();
    const int16_t apps[] = {TECH_APP_NUCLEAR_DRIVE, TECH_APP_NUCLEAR_MISSILE, TECH_APP_NUCLEAR_BOMB};
    const int32_t values[] = {10, 30, 20};
    for (int i = 0; i < 3; ++i) {
        MOX::player[1].tech_applications[apps[i]] = TECH_RESEARCH_STATUS_RESEARCHED;
        TEST_NPCDIPLO::Set_Tech_Value_For_App_(apps[i], values[i]);
    }
    uint8_t indices[TECH_APP_COUNT] = {};
    int32_t sorted_values[TECH_APP_COUNT] = {};
    int16_t count = 0;
    npcdiplo::Get_Differential_Tech_List(0, 1, 0, UINT32_MAX, indices, sorted_values, &count, 1);
    DOCTEST_REQUIRE(count == 3);
    DOCTEST_CHECK(indices[0] == apps[1]);
    DOCTEST_CHECK(indices[1] == apps[2]);
    DOCTEST_CHECK(indices[2] == apps[0]);
    DOCTEST_CHECK(sorted_values[0] == 30);
    DOCTEST_CHECK(sorted_values[1] == 20);
    DOCTEST_CHECK(sorted_values[2] == 10);
}