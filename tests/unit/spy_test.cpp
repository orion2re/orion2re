#include "pch.h"

#include "doctest/doctest.h"

namespace TEST_SPY {
    void Reset_();
    s_player* Player_();
    s_player* Player_(int index);
    s_leader_data* Leader_(int index);
    void Set_Assassin_Spies_(uint8_t count);
    void Set_Random_Results_(const uint32_t* values, int count);
    int Random_Call_Count_();
    uint32_t Random_Bound_(int index);
    int Level_Call_Count_();
    int16_t Level_Leader_(int index);
    uint8_t Assassin_Spies_();
}

DOCTEST_TEST_CASE("technology spy bonuses include configured Xeno Psychology") {
    spy::spy_tech_bonus = {10, 0, 5, 10, 10, 10};
    s_player player = {};
    player.tech_applications[TECH_APP_XENO_PSYCHOLOGY] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(spy::Tech_Spy_Bonus(&player) == 0);

    spy::spy_tech_bonus.xeno_psychology = 7;
    DOCTEST_CHECK(spy::Tech_Spy_Bonus(&player) == 7);

    spy::spy_tech_bonus = {10, 0, 5, 10, 10, 10};
}

DOCTEST_TEST_CASE("returning spies preserves the home count cap and clears the foreign assignment") {
    TEST_SPY::Reset_();
    BILL::Set_Their_Spy_Number(0, 0, 10);
    BILL::Set_Their_Spy_Number(0, 1, 5);
    spy::Bring_Home_Spies_Vs(0, 1);
    DOCTEST_CHECK(BILL::Get_Their_Spy_Number(0, 0) == 15);
    DOCTEST_CHECK(BILL::Get_Their_Spy_Number(0, 1) == 0);

    BILL::Set_Their_Spy_Number(0, 0, 50);
    BILL::Set_Their_Spy_Number(0, 1, 20);
    spy::Bring_Home_Spies_Vs(0, 1);
    DOCTEST_CHECK(BILL::Get_Their_Spy_Number(0, 0) == 63);
    DOCTEST_CHECK(BILL::Get_Their_Spy_Number(0, 1) == 0);
    spy::Bring_Home_Spies_Vs(0, 1);
    DOCTEST_CHECK(BILL::Get_Their_Spy_Number(0, 0) == 63);
    DOCTEST_CHECK(TEST_SPY::Random_Call_Count_() == 0);
}

DOCTEST_TEST_CASE("spy count bonuses retain their configured diminishing-return boundaries") {
    struct s_count_bonus_scope {
        spy::s_spy_count_bonus saved = spy::count_bonus;
        ~s_count_bonus_scope() { spy::count_bonus = saved; }
    } scope;
    spy::count_bonus = {2, 4};
    const int32_t expected[] = {0, 2, 4, 5, 6, 6, 7};
    for (int32_t count = 0; count < 7; ++count) {
        DOCTEST_CHECK(spy::N_Spies_Bonus(count) == expected[count]);
    }
}

DOCTEST_TEST_CASE("Assassins resolve in descending leader and RNG order") {
    TEST_SPY::Reset_();
    s_leader_data* ordinary = TEST_SPY::Leader_(3);
    ordinary->player_index = 0;
    ordinary->status = 0;
    ordinary->general_skills = LEADER_GENERAL_SKILL_ASSASSIN;

    s_leader_data* improved = TEST_SPY::Leader_(60);
    improved->player_index = 0;
    improved->status = 1;
    improved->general_skills =
        LEADER_GENERAL_SKILL_ASSASSIN | LEADER_GENERAL_SKILL_ASSASSIN2;

    TEST_SPY::Set_Assassin_Spies_(2);
    const uint32_t rolls[] = {3, 1, 3};
    TEST_SPY::Set_Random_Results_(rolls, 3);

    spy::Resolve_Assassins();

    DOCTEST_REQUIRE(TEST_SPY::Level_Call_Count_() == 2);
    DOCTEST_CHECK(TEST_SPY::Level_Leader_(0) == 60);
    DOCTEST_CHECK(TEST_SPY::Level_Leader_(1) == 3);
    DOCTEST_REQUIRE(TEST_SPY::Random_Call_Count_() == 3);
    DOCTEST_CHECK(TEST_SPY::Random_Bound_(0) == 100);
    DOCTEST_CHECK(TEST_SPY::Random_Bound_(1) == 2);
    DOCTEST_CHECK(TEST_SPY::Random_Bound_(2) == 100);
    DOCTEST_CHECK(TEST_SPY::Assassin_Spies_() == 1);
}

DOCTEST_TEST_CASE("Telepathic spying bonus uses signed immediate-byte semantics") {
    struct s_bonus_case {
        uint8_t configured;
        int16_t expected;
    };
    const s_bonus_case cases[] = {
        {0, 0},
        {10, 10},
        {127, 127},
        {128, -128},
        {255, -1},
    };

    for (const s_bonus_case& test_case : cases) {
        TEST_SPY::Reset_();
        s_player* player = TEST_SPY::Player_();
        spy::telepathic_bonus = test_case.configured;

        int16_t defense = 0;
        int16_t attack = 0;
        spy::Compute_Spy_Bonuses(player, &defense, &attack);
        DOCTEST_CHECK(defense == 0);
        DOCTEST_CHECK(attack == 0);

        player->traits[TRAIT_TELEPATHIC] = 1;
        spy::Compute_Spy_Bonuses(player, &defense, &attack);
        DOCTEST_CHECK(defense == test_case.expected);
        DOCTEST_CHECK(attack == test_case.expected);
    }

    spy::telepathic_bonus = 10;
}

DOCTEST_TEST_CASE("spy theft policy filters current research as configured") {
    constexpr int16_t current_field = 4;
    constexpr int16_t other_field = 5;
    constexpr int16_t current_application = 20;
    constexpr int16_t other_application = 21;

    spy::steal_mode = spy::STEAL_MODE_EXCEPT_CURRENT_FIELD;
    DOCTEST_CHECK_FALSE(spy::Current_Research_Allows_Theft(
        current_field, other_application, current_field, current_application));
    DOCTEST_CHECK(spy::Current_Research_Allows_Theft(
        other_field, current_application, current_field, current_application));

    spy::steal_mode = spy::STEAL_MODE_EXCEPT_CURRENT_TECH;
    DOCTEST_CHECK_FALSE(spy::Current_Research_Allows_Theft(
        other_field, current_application, current_field, current_application));
    DOCTEST_CHECK(spy::Current_Research_Allows_Theft(
        current_field, other_application, current_field, current_application));
    DOCTEST_CHECK_FALSE(spy::Current_Research_Allows_Theft(
        other_field, 200, current_field, (int8_t)200));

    spy::steal_mode = spy::STEAL_MODE_ANYTHING;
    DOCTEST_CHECK(spy::Current_Research_Allows_Theft(
        current_field, current_application, current_field, current_application));

    spy::steal_mode = spy::STEAL_MODE_EXCEPT_CURRENT_FIELD;
}

DOCTEST_TEST_CASE("technology theft can frame a player with no spies") {
    TEST_SPY::Reset_();
    constexpr int16_t attacker_idx = 0;
    constexpr int16_t victim_idx = 1;
    constexpr int16_t framed_idx = 2;

    s_player* framed_player = TEST_SPY::Player_(framed_idx);
    framed_player->contact[attacker_idx] = 1;
    framed_player->contact[victim_idx] = 1;

    int16_t blamed_player_idx = attacker_idx;
    DOCTEST_CHECK(spy::Frame_Another(
        attacker_idx, victim_idx, &blamed_player_idx, -1));
    DOCTEST_CHECK(blamed_player_idx == framed_idx);
    DOCTEST_CHECK(BILL::Get_Their_Spy_Number(framed_idx, victim_idx) == 0);
}

DOCTEST_TEST_CASE("technology spy bonus uses every configured field") {
    spy::spy_tech_bonus = {10, 0, 5, 10, 10, 10};
    s_player player = {};
    player.tech_applications[TECH_APP_NEURAL_SCANNER] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    player.tech_applications[TECH_APP_XENO_PSYCHOLOGY] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    player.tech_applications[TECH_APP_TELEPATHIC_TRAINING] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    player.tech_applications[TECH_APP_CYBER_SECURITY_LINK] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    player.tech_applications[TECH_APP_STEALTH_SUIT] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    player.tech_applications[TECH_APP_PSIONICS] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(spy::Tech_Spy_Bonus(&player) == 45);

    spy::spy_tech_bonus = {1, 2, 3, 4, 5, 6};
    DOCTEST_CHECK(spy::Tech_Spy_Bonus(&player) == 21);

    spy::spy_tech_bonus = {10, 0, 5, 10, 10, 10};
}