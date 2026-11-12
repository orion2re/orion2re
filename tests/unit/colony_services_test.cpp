#include "pch.h"

#include "doctest/doctest.h"
#include "../fakes/fake_colony_services.h"

DOCTEST_TEST_CASE("colony summaries pass the selected entity and complete text to the dialog") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    COLCALC::Show_Command_Points(1);
    COLCALC::Show_Industry_Production(0);
    COLCALC::Show_Research_Production(0);
    DOCTEST_CHECK(services.maintenance_summary_player == 1);
    DOCTEST_CHECK(services.industry_summary_colony == 0);
    DOCTEST_CHECK(services.research_summary_colony == 0);
    DOCTEST_CHECK((services.texts == std::vector<std::string>{"Maintenance detail", "Industry detail", "Research detail"}));
    DOCTEST_CHECK((services.titles == std::vector<std::string>{COLCALC::command_summary_msg, COLCALC::industry_summary_msg, COLCALC::research_summary_msg}));
}

DOCTEST_TEST_CASE("advanced homeworld initialization updates contacts ranges and design requests") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    MOX::NUM_PLAYERS = 1;
    MOX::NUM_SHIPS = 0;
    MOX::colony[0].outpost_flag = 1;
    MOX::player[0].home_planet_id = 0;
    MOX::player[0].command_points = 2;
    MOX::settings.starting_civilization_level = STARTING_CIVILIZATION_LEVEL_ADVANCED;
    COLCALC::Twiddle_Initial_Homeworlds();
    DOCTEST_CHECK(services.contact_updates == 1);
    DOCTEST_CHECK(services.range_updates == 1);
    DOCTEST_CHECK(services.settings_checks == 2);
    DOCTEST_CHECK(services.screen_updates > 0);
    DOCTEST_CHECK((services.homeworlds == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.designs == std::vector<int16_t>{0, 0}));
    DOCTEST_REQUIRE(services.creations.size() == 2);
    DOCTEST_CHECK((services.creations[0] == std::array<int16_t, 3>{0, 0, 0}));
    DOCTEST_CHECK((services.normal_ranges == std::vector<std::array<int16_t, 2>>{{0, 0}}));
    DOCTEST_CHECK((MOX::star[0].visited & 1) != 0);
    DOCTEST_CHECK((MOX::player[0].bc == 200));
    DOCTEST_CHECK((MOX::player[0].n_freighters == 10));
}

DOCTEST_TEST_CASE("player turn accounting skips eliminated players and reports shortages") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    MOX::player[0].bc = 40;
    MOX::player[0].surplus_bc = -5;
    MOX::player[0].research_produced = 3;
    MOX::player[0].surplus_food = -1;
    MOX::player[1].eliminated = 1;
    COLCALC::Apply_All_Player_Changes();
    DOCTEST_CHECK((services.maintenance == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.stats == std::vector<int16_t>{0}));
    DOCTEST_CHECK((MOX::player[0].bc == 35));
    DOCTEST_CHECK((MOX::player[0].research_accumulated == 3));
    DOCTEST_CHECK((REPORT::something_interesting_happened[0] == 1));
    DOCTEST_REQUIRE(services.messages.size() == 1);
    DOCTEST_CHECK((services.message_players == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.messages[0].msg_type == MSG_TYPE_FINANCIAL));
    DOCTEST_CHECK((services.messages[0].u.financial.a == 35));
    DOCTEST_CHECK((services.messages[0].u.financial.b == -5));
}

DOCTEST_TEST_CASE("blockades report the attacking and defending empires to diplomacy") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    MOX::NUM_SHIPS = 1;
    MOX::ship[0].owner = 0;
    MOX::ship[0].location = 0;
    MOX::ship[0].d.ship_type = SHIP_TYPE_COMBAT;
    MOX::player[0].treaty[1] = 4;
    MOX::star[0].has_colony = 2;
    COLCALC::Compute_Blockades();
    DOCTEST_CHECK((MOX::star[0].blockaded == 2));
    DOCTEST_CHECK((MOX::star[0].blockaded_by[1] == 1));
    DOCTEST_REQUIRE(services.relations.size() == 1);
    DOCTEST_CHECK((services.relations[0] == std::array<int16_t, 6>{-1, 0, 1, 7, 0, 0}));
}

DOCTEST_TEST_CASE("elimination releases leaders destroys owned ships and clears foreign relations") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    MOX::colony[0].owner = 1;
    MOX::ship[0].owner = 0;
    MOX::ship[0].officer_index = 0;
    MOX::ship[1].owner = 1;
    MOX::leaders[0].player_index = 0;
    MOX::leaders[0].type = LEADER_TYPE_SHIP;
    MOX::leaders[0].location = 0;
    MOX::player[0].last_attacker = 1;
    MOX::player[1].treaty[0] = 6;
    COLCALC::Check_For_Eliminated_Players();
    DOCTEST_CHECK((MOX::player[0].eliminated == 1));
    DOCTEST_CHECK((MOX::player[1].eliminated == 0));
    DOCTEST_CHECK((MOX::leaders[0].status == -2));
    DOCTEST_CHECK((MOX::leaders[0].player_index == -1));
    DOCTEST_CHECK((MOX::ship[0].officer_index == -1));
    DOCTEST_CHECK((services.killed == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.eliminated == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.spy_writes == std::vector<std::array<int16_t, 3>>{{0, 0, 0}, {0, 1, 0}}));
    DOCTEST_CHECK((services.spies_home == std::vector<std::array<int16_t, 2>>{{1, 0}}));
    DOCTEST_CHECK(services.icon_updates == 1);
    DOCTEST_CHECK((MOX::player[1].treaty[0] == 0));
    DOCTEST_CHECK((MOX::player[1].player_defeated[0] == MOX::stardate));
    DOCTEST_CHECK((MOX::player[0].last_attacker == -1));
}

DOCTEST_TEST_CASE("empire surrender transfers assets caps spies and recalculates interdicted fleets") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    MOX::colony[0].buildings[BUILDING_CAPITOL] = 1;
    MOX::colony[0].buildings[BUILDING_WARP_INTERDICTOR] = 1;
    MOX::colony[0].n_turns_existed = 3;
    MOX::ship[0].owner = 0;
    MOX::ship[0].officer_index = 0;
    MOX::ship[1].owner = 1;
    MOX::leaders[0].player_index = 0;
    MOX::leaders[0].type = LEADER_TYPE_SHIP;
    MOX::leaders[0].location = 0;
    MOX::player[0].bc = 100;
    MOX::player[0].n_freighters = 2;
    MOX::player[1].bc = 25;
    MOX::player[1].n_freighters = 3;
    services.spies[1][1] = 50;
    services.spies[0][0] = 10;
    services.spies[0][1] = 20;
    COLCALC::Do_Surrender_Empire(0, 1);
    DOCTEST_CHECK((MOX::ship[0].owner == 1));
    DOCTEST_CHECK((MOX::ship[0].officer_index == -1));
    DOCTEST_CHECK((services.deassigned == std::vector<int16_t>{0}));
    DOCTEST_CHECK((MOX::colony[0].owner == 1));
    DOCTEST_CHECK((MOX::colony[0].buildings[BUILDING_CAPITOL] == 0));
    DOCTEST_CHECK((MOX::colony[0].n_turns_existed == 0));
    DOCTEST_CHECK((services.queues == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.population_limits == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.fleet_etas == std::vector<int16_t>{-1}));
    DOCTEST_CHECK((services.spy_reads == std::vector<std::array<int16_t, 2>>{{1, 1}, {0, 0}, {0, 1}}));
    DOCTEST_CHECK((services.spies[1][1] == 63));
    DOCTEST_CHECK((MOX::player[1].bc == 125));
    DOCTEST_CHECK((MOX::player[1].n_freighters == 5));
    DOCTEST_CHECK((MOX::player[0].eliminated == 1));
    DOCTEST_CHECK(services.killed.empty());
    DOCTEST_CHECK(services.icon_updates == 2);
}

DOCTEST_TEST_CASE("star surrender returns the old colony leader to the pool") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    MOX::star[0].officer_index[0] = 1;
    MOX::leaders[1].player_index = 0;
    MOX::leaders[1].type = LEADER_TYPE_COLONY;
    MOX::leaders[1].location = 0;
    COLCALC::Do_Surrender_Star(0, 0, 1);
    DOCTEST_CHECK((services.pooled == std::vector<int16_t>{1}));
    DOCTEST_CHECK((MOX::star[0].officer_index[0] == -1));
    DOCTEST_CHECK((MOX::star[0].surrender_to[0] == -1));
    DOCTEST_CHECK((MOX::star[0].has_colony == 2));
    DOCTEST_CHECK((MOX::star[0].visited & 2) != 0);
}

DOCTEST_TEST_CASE("surrender eligibility uses recipient range and excludes the capitol system") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    MOX::NUM_STARS = 2;
    MOX::NUM_COLONIES = 2;
    MOX::player[0].home_planet_id = 1;
    MOX::player[1].ship_range = 9;
    services.in_normal_range = false;
    DOCTEST_CHECK_FALSE(COLCALC::May_Surrender_Star(0, 0, 1));
    services.in_normal_range = true;
    DOCTEST_CHECK(COLCALC::May_Surrender_Star(0, 0, 1));
    DOCTEST_CHECK((services.normal_ranges == std::vector<std::array<int16_t, 2>>{{0, 1}, {0, 1}}));
    int16_t stars[2] = {-1, -1};
    DOCTEST_CHECK(COLCALC::Get_Possible_Surrender_Systems(0, 1, stars) == 1);
    DOCTEST_CHECK(stars[0] == 0);
    DOCTEST_CHECK((services.ranges == std::vector<std::array<int16_t, 3>>{{0, 1, 9}, {1, 1, 9}}));
    services.in_range = false;
    DOCTEST_CHECK(COLCALC::Get_Possible_Surrender_Systems(0, 1, stars) == 0);
}

DOCTEST_TEST_CASE("only arrived colony leaders query their star anomaly") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    auto& leader = MOX::leaders[0];
    leader.status = 1;
    leader.type = LEADER_TYPE_COLONY;
    leader.location = 0;
    services.anomaly_result = 1;
    DOCTEST_CHECK(COLCALC::Leader_At_Anomaly(&leader) == 1);
    services.anomaly_result = 0;
    DOCTEST_CHECK(COLCALC::Leader_At_Anomaly(&leader) == 0);
    leader.eta = 1;
    DOCTEST_CHECK(COLCALC::Leader_At_Anomaly(&leader) == 0);
    DOCTEST_CHECK((services.anomalies == std::vector<int16_t>{0, 0}));
}
