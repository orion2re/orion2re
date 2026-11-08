#include "pch.h"

#include "doctest/doctest.h"
#include "game/nextturn.h"

namespace TEST_NEXTTURN {
    extern bool invalid_inputs;
    extern int32_t turn_side_effects;
    extern int32_t observed_event_bc;
    extern int32_t observed_event_research;
    extern bool colony_changes_saw_current_economy;
    extern bool pirate_raid_was_eligible;
    extern bool computer_virus_was_eligible;
    extern int16_t officer_check_order[MAX_PLAYERS];
    extern int16_t officer_check_count;
    extern int16_t officer_offer_player;
    extern bool colony_calculation_population_growth_scope[2];

    void Reset_();
}

DOCTEST_TEST_CASE("invalid AI inputs reject a turn before any turn side effects") {
    TEST_NEXTTURN::Reset_();
    TEST_NEXTTURN::invalid_inputs = true;
    MOX::game_type = 0;
    const int32_t stardate = MOX::stardate;
    DOCTEST_CHECK_FALSE(NEXTTURN::Next_Turn_Calc());
    DOCTEST_CHECK(TEST_NEXTTURN::turn_side_effects == 0);
    DOCTEST_CHECK(MOX::stardate == stardate);
}

DOCTEST_TEST_CASE("network population growth is scoped to the elected host calculation") {
    TEST_NEXTTURN::Reset_();
    NETMOX::net.local_net_id = 17;
    NETMOX::net.net_host_id = 17;

    NEXTTURN::Next_Turn_Calc();

    DOCTEST_CHECK(
        TEST_NEXTTURN::colony_calculation_population_growth_scope[0]);
    DOCTEST_CHECK_FALSE(
        TEST_NEXTTURN::colony_calculation_population_growth_scope[1]);

    TEST_NEXTTURN::Reset_();
    NETMOX::net.local_net_id = 17;
    NETMOX::net.net_host_id = 23;

    NEXTTURN::Next_Turn_Calc();

    DOCTEST_CHECK_FALSE(
        TEST_NEXTTURN::colony_calculation_population_growth_scope[0]);
    DOCTEST_CHECK_FALSE(
        TEST_NEXTTURN::colony_calculation_population_growth_scope[1]);
}

DOCTEST_TEST_CASE("next turn applies current economy before colony changes and event selection") {
    TEST_NEXTTURN::Reset_();

    NEXTTURN::Next_Turn_Calc();

    DOCTEST_CHECK(TEST_NEXTTURN::colony_changes_saw_current_economy);
    DOCTEST_CHECK(TEST_NEXTTURN::observed_event_bc == 110);
    DOCTEST_CHECK(TEST_NEXTTURN::observed_event_research == 12);
    DOCTEST_CHECK(TEST_NEXTTURN::pirate_raid_was_eligible);
    DOCTEST_CHECK(TEST_NEXTTURN::computer_virus_was_eligible);
}

DOCTEST_TEST_CASE("leader hire selection preserves interleaved RNG order") {
    TEST_NEXTTURN::Reset_();
    MOX::NUM_PLAYERS = 4;
    game_random::Set_Random_Seed(3);

    NEXTTURN::Next_Turn_Calc();

    DOCTEST_REQUIRE(TEST_NEXTTURN::officer_check_count == 4);
    DOCTEST_CHECK(TEST_NEXTTURN::officer_check_order[0] == 3);
    DOCTEST_CHECK(TEST_NEXTTURN::officer_check_order[1] == 0);
    DOCTEST_CHECK(TEST_NEXTTURN::officer_check_order[2] == 2);
    DOCTEST_CHECK(TEST_NEXTTURN::officer_check_order[3] == 1);
    DOCTEST_CHECK(TEST_NEXTTURN::officer_offer_player == -1);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == 1018653819u);
}