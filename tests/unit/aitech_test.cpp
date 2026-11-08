#include "pch.h"

#include "doctest/doctest.h"

namespace TEST_AI_HYPERTECH {
    void Reset_();
    s_player* Player_();
    void Set_Selected_Tech_Application_(int16_t tech_application);
    void Set_Hypertech_Choices_(const int32_t* choices, int32_t count);
    int32_t Hypertech_Choice_Calls_();
}

namespace {
    const game_world::s_view* AI_World_() {
        static const game_world::s_view world{
            .players = MOX::player,
        };
        return &world;
    }

    void Set_Tech_Field_(int32_t tech_application, int32_t tech_field) {
        TECHDATA::technology_applications[tech_application].tech_field_id = (int16_t)tech_field;
    }

    void Request_Selection_(s_player* player) {
        player->got_new_app = 1;
    }
}

DOCTEST_TEST_SUITE("AI hypertech") {

DOCTEST_TEST_CASE("AI accepts a hypertech branch below its configured threshold") {
    TEST_AI_HYPERTECH::Reset_();
    s_player* player = TEST_AI_HYPERTECH::Player_();
    Request_Selection_(player);
    Set_Tech_Field_(TECH_APP_BIOLOGY, TECH_FIELD_BIOLOGY);
    aitech::rules.hypertech_threshold[TECH_APP_BIOLOGY - TECH_APP_BIOLOGY] = 1;
    TEST_AI_HYPERTECH::Set_Selected_Tech_Application_(TECH_APP_BIOLOGY);

    aitech::AI_Tech_Select(AI_World_(), player);

    DOCTEST_CHECK(player->current_research_application == (int8_t)TECH_APP_BIOLOGY);
    DOCTEST_CHECK(player->current_research_field == TECH_FIELD_BIOLOGY);
    DOCTEST_CHECK(player->do_not_research == 0);
    DOCTEST_CHECK(TEST_AI_HYPERTECH::Hypertech_Choice_Calls_() == 0);
}

DOCTEST_TEST_CASE("AI rerolls an exhausted hypertech branch") {
    TEST_AI_HYPERTECH::Reset_();
    s_player* player = TEST_AI_HYPERTECH::Player_();
    Request_Selection_(player);
    Set_Tech_Field_(TECH_APP_BIOLOGY, TECH_FIELD_BIOLOGY);
    Set_Tech_Field_(TECH_APP_POWER, TECH_FIELD_POWER);
    aitech::rules.hypertech_threshold[TECH_APP_POWER - TECH_APP_BIOLOGY] = 1;
    TEST_AI_HYPERTECH::Set_Selected_Tech_Application_(TECH_APP_BIOLOGY);
    const int32_t choices[] = {TECH_APP_POWER};
    TEST_AI_HYPERTECH::Set_Hypertech_Choices_(choices, 1);

    aitech::AI_Tech_Select(AI_World_(), player);

    DOCTEST_CHECK(player->current_research_application == (int8_t)TECH_APP_POWER);
    DOCTEST_CHECK(player->current_research_field == TECH_FIELD_POWER);
    DOCTEST_CHECK(player->do_not_research == 0);
    DOCTEST_CHECK(TEST_AI_HYPERTECH::Hypertech_Choice_Calls_() == 1);
}

DOCTEST_TEST_CASE("all-zero thresholds preserve classic AI research shutdown") {
    TEST_AI_HYPERTECH::Reset_();
    s_player* player = TEST_AI_HYPERTECH::Player_();
    Request_Selection_(player);
    Set_Tech_Field_(TECH_APP_PHYSICS, TECH_FIELD_PHYSICS_2);
    TEST_AI_HYPERTECH::Set_Selected_Tech_Application_(TECH_APP_PHYSICS);

    aitech::AI_Tech_Select(AI_World_(), player);

    DOCTEST_CHECK(player->current_research_application == (int8_t)TECH_APP_PHYSICS);
    DOCTEST_CHECK(player->current_research_field == TECH_FIELD_PHYSICS_2);
    DOCTEST_CHECK(player->do_not_research == 1);
    DOCTEST_CHECK(TEST_AI_HYPERTECH::Hypertech_Choice_Calls_() == 0);
}

DOCTEST_TEST_CASE("hypertech breakthroughs request the next AI research selection") {
    TEST_AI_HYPERTECH::Reset_();
    s_player* player = TEST_AI_HYPERTECH::Player_();
    player->current_research_field = TECH_FIELD_BIOLOGY;
    player->current_research_application = TECH_APP_BIOLOGY;
    player->research_breakthrough = 1;
    player->hyper_advanced_tech[TECH_APP_BIOLOGY - TECH_APP_BIOLOGY] = 1;
    aitech::rules.hypertech_threshold[TECH_APP_BIOLOGY - TECH_APP_BIOLOGY] = 2;
    Set_Tech_Field_(TECH_APP_BIOLOGY, TECH_FIELD_BIOLOGY);
    TEST_AI_HYPERTECH::Set_Selected_Tech_Application_(TECH_APP_BIOLOGY);

    aitech::AI_Tech_Select(AI_World_(), player);

    DOCTEST_CHECK(player->current_research_application == (int8_t)TECH_APP_BIOLOGY);
    DOCTEST_CHECK(player->research_breakthrough == 0);
    DOCTEST_CHECK(player->do_not_research == 0);
}

DOCTEST_TEST_CASE("ordinary technology selection bypasses hypertech thresholds") {
    TEST_AI_HYPERTECH::Reset_();
    s_player* player = TEST_AI_HYPERTECH::Player_();
    Request_Selection_(player);
    Set_Tech_Field_(TECH_APP_LASER_CANNON, TECH_FIELD_PHYSICS);
    TEST_AI_HYPERTECH::Set_Selected_Tech_Application_(TECH_APP_LASER_CANNON);

    aitech::AI_Tech_Select(AI_World_(), player);

    DOCTEST_CHECK(player->current_research_application == TECH_APP_LASER_CANNON);
    DOCTEST_CHECK(player->current_research_field == TECH_FIELD_PHYSICS);
    DOCTEST_CHECK(player->do_not_research == 0);
    DOCTEST_CHECK(TEST_AI_HYPERTECH::Hypertech_Choice_Calls_() == 0);
}

}