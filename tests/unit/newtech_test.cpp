#include "pch.h"

#include "doctest/doctest.h"

namespace TEST_NEWTECH_CHOOSE {
void Reset_();
s_player* Player_();
uint32_t Selected_Weight_();
}

namespace {
constexpr int16_t kApplication = 1;
constexpr int16_t kField = TECH_FIELD_ADVANCED_ENGINEERING;

void Prepare_Choice_(int8_t category_modifier = 1) {
    TEST_NEWTECH_CHOOSE::Reset_();
    s_player* player = TEST_NEWTECH_CHOOSE::Player_();
    player->objectives = PLAYER_OBJECTIVE_MILITARIST;
    player->research_produced = 100;
    player->tech_applications[kApplication] = TECH_RESEARCH_STATUS_AVAILABLE;
    player->tech_fields[kField] = 2;
    player->tech_fields[TECH_FIELD_NUCLEAR_FISSION] = 3;
    player->tech_fields[TECH_FIELD_COLD_FUSION] = 3;
    player->tech_fields[TECH_FIELD_PHYSICS] = 3;
    player->tech_fields[TECH_FIELD_ENGINEERING] = 3;
    player->tech_fields[TECH_FIELD_CHEMISTRY] = 3;
    player->tech_fields[TECH_FIELD_ELECTRONICS] = 3;

    MOX::tech_categories[0][0] = category_modifier;
    MOX::tech_categories[0][1] = 1;
    TECHDATA::technology_applications[kApplication].tech_field_id = kField;
    TECHDATA::technology_applications[kApplication].ai_group = 0;
    TECHDATA::technology_applications[TECH_APP_ADAMANTIUM_ARMOR].tech_field_id =
        kField;
    TECHDATA::technology_fields[kField].ai_group = 1;
    TECHDATA::technology_fields[kField].cost = 100;

    newtech::tech_priority = newtech::s_ai_tech_priority{
        TECH_FIELD_ADVANCED_BIOLOGY,
        TECH_FIELD_ADVANCED_CHEMISTRY,
        TECH_APP_ADAMANTIUM_ARMOR,
        TECH_APP_ANTI_GRAV_HARNESS,
    };
}

uint32_t Choose_Weight_() {
    DOCTEST_CHECK(newtech::Choose_Tech_Application(game::id_player{0}) == kApplication);
    return TEST_NEWTECH_CHOOSE::Selected_Weight_();
}
}

DOCTEST_TEST_SUITE("AI technology choice") {

DOCTEST_TEST_CASE("research ignores missing and unoccupied player IDs") {
    Prepare_Choice_();
    for (const auto id : {game::NO_PLAYER, game::id_player{1}, game::id_player{65536}}) {
        DOCTEST_CHECK(newtech::Choose_Tech_Application(id) == TECH_APP_NO_TECH);
        DOCTEST_CHECK(newtech::Calc_Tech_Value(id, kApplication) == 0);
    }
    DOCTEST_CHECK(TEST_NEWTECH_CHOOSE::Selected_Weight_() == 0);
    DOCTEST_CHECK(Choose_Weight_() == 15);
}

DOCTEST_TEST_CASE("priority selectors apply independently and stack in runtime order") {
    Prepare_Choice_();
    DOCTEST_CHECK(Choose_Weight_() == 15);

    Prepare_Choice_();
    newtech::tech_priority.double_field_1 = kField;
    DOCTEST_CHECK(Choose_Weight_() == 30);

    Prepare_Choice_();
    newtech::tech_priority.double_application = kApplication;
    DOCTEST_CHECK(Choose_Weight_() == 30);

    Prepare_Choice_();
    newtech::tech_priority.quintuple_application = kApplication;
    DOCTEST_CHECK(Choose_Weight_() == 75);

    Prepare_Choice_();
    newtech::tech_priority.double_field_2 = kField;
    DOCTEST_CHECK(Choose_Weight_() == 30);

    Prepare_Choice_();
    newtech::tech_priority = {kField, kField, kApplication, kApplication};
    DOCTEST_CHECK(Choose_Weight_() == 600);
}

DOCTEST_TEST_CASE("zero technology value is normalized after priority comparisons") {
    Prepare_Choice_(0);
    newtech::tech_priority = {kField, kField, kApplication, kApplication};
    DOCTEST_CHECK(Choose_Weight_() == 15);
}

}
