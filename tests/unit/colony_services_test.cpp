#include "pch.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"

#include "doctest/doctest.h"
#include "../fakes/fake_colony_services.h"

DOCTEST_TEST_CASE("colony summaries pass the selected entity and complete text to the dialog") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    COLCALC::Show_Command_Points(game::id_player{1});
    COLCALC::Show_Industry_Production(game::id_colony{0});
    COLCALC::Show_Research_Production(game::id_colony{0});
    DOCTEST_CHECK(services.maintenance_summary_player == 1);
    DOCTEST_CHECK(services.industry_summary_colony == 0);
    DOCTEST_CHECK(services.research_summary_colony == 0);
    DOCTEST_CHECK((services.texts == std::vector<std::string>{"Maintenance detail", "Industry detail", "Research detail"}));
    DOCTEST_CHECK((services.titles == std::vector<std::string>{COLCALC::command_summary_msg, COLCALC::industry_summary_msg, COLCALC::research_summary_msg}));
}

DOCTEST_TEST_CASE("advanced homeworld initialization updates contacts ranges and design requests") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    game::current_state.colonies.erase(game::id_colony{1});
    game::current_state.player_count = 1;
    game::current_state.ship_count = 0;
    test_colonies::Record(0).outpost_flag = 1;
    game::current_state.players[0].home_planet_id = game::id_planet{0};
    game::current_state.players[0].command_points = 2;
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
    DOCTEST_CHECK((services.normal_ranges == std::vector<std::array<int16_t, 2>>{{0, 0}, {1, 0}}));
    DOCTEST_CHECK(test_stars::Record(0).visited.test(game::id_player{0}));
    DOCTEST_CHECK((game::current_state.players[0].bc == 200));
    DOCTEST_CHECK((game::current_state.players[0].n_freighters == 10));
}

DOCTEST_TEST_CASE("player turn accounting skips eliminated players and reports shortages") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    game::current_state.players[0].bc = 40;
    game::current_state.players[0].surplus_bc = -5;
    game::current_state.players[0].research_produced = 3;
    game::current_state.players[0].surplus_food = -1;
    game::current_state.players[1].eliminated = 1;
    COLCALC::Apply_All_Player_Changes();
    DOCTEST_CHECK((services.maintenance == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.stats == std::vector<int16_t>{0}));
    DOCTEST_CHECK((game::current_state.players[0].bc == 35));
    DOCTEST_CHECK((game::current_state.players[0].research_accumulated == 3));
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
    auto& state = game::current_state;
    state.ship_records.resize(3);
    state.ship_count = 2;
    state.next_ship_id = 90003;
    s_ship_data ship{};
    ship.id = game::id_ship{90000};
    ship.position.Set({game::e_ship_location::at_star, game::Star_Key_At_Row(state, 0)});
    ship.d.ship_type = SHIP_TYPE_COMBAT;
    state.ship_records.Replace_Row(0, ship);
    ship.id = game::id_ship{90001};
    ship.position.Set(game::NO_SHIP_LOCATION);
    state.ship_records.Replace_Row(1, ship);
    state.ship_records.Swap_Rows(0, 1);
    const auto spare_star = game::Create_Star(state, {.has_colony = game::player_bitmap::From_Byte(1)});
    DOCTEST_REQUIRE(spare_star);
    ship.id = game::id_ship{90002};
    ship.owner = factions::From_Legacy(NONPLAYER_SHIP_TYPE_GUARDIAN);
    ship.position.Set({game::e_ship_location::at_star, *spare_star});
    state.ship_records.Replace_Row(2, ship);
    game::current_state.players[0].treaty[1] = 4;
    test_stars::Record(0).has_colony = game::player_bitmap::From_Byte(2);
    COLCALC::Compute_Blockades();
    DOCTEST_CHECK((test_stars::Record(0).blockaded == game::player_bitmap::From_Byte(2)));
    DOCTEST_CHECK((test_stars::Record(0).blockaded_by[1] == game::player_bitmap::From_Byte(1)));
    DOCTEST_CHECK((state.stars.at(*spare_star).blockaded.none()));
    DOCTEST_REQUIRE(services.relations.size() == 1);
    DOCTEST_CHECK((services.relations[0] == std::array<int16_t, 6>{-1, 0, 1, 7, 0, 0}));
}

DOCTEST_TEST_CASE("elimination releases leaders destroys owned ships and clears foreign relations") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    game::current_state.colonies.erase(game::id_colony{1});
    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(1);
    game::current_state.ship_records[0].owner = factions::From_Legacy(0);
    game::current_state.ship_records[0].officer_index = 0;
    game::current_state.ship_records[1].owner = factions::From_Legacy(1);
    auto& leader = game::current_state.leaders[0];
    leader.employer = game::id_player{0};
    leader.type = LEADER_TYPE_SHIP;
    leader.location = 0;
    game::current_state.players[0].last_attacker = game::id_faction{1};
    game::current_state.players[1].treaty[0] = 6;
    COLCALC::Check_For_Eliminated_Players();
    DOCTEST_CHECK((game::current_state.players[0].eliminated == 1));
    DOCTEST_CHECK((game::current_state.players[1].eliminated == 0));
    DOCTEST_CHECK(leader.status == -2);
    DOCTEST_CHECK(leader.employer == game::NO_PLAYER);
    DOCTEST_CHECK((game::current_state.ship_records[0].officer_index == -1));
    DOCTEST_CHECK((services.killed == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.eliminated == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.spy_writes == std::vector<std::array<int16_t, 3>>{{0, 0, 0}, {0, 1, 0}}));
    DOCTEST_CHECK((services.spies_home == std::vector<std::array<game::id_player, 2>>{{game::id_player{1}, game::id_player{0}}}));
    DOCTEST_CHECK(services.icon_updates == 1);
    DOCTEST_CHECK((game::current_state.players[1].treaty[0] == 0));
    DOCTEST_CHECK((game::current_state.players[1].player_defeated[0] == MOX::stardate));
    DOCTEST_CHECK(game::current_state.players[0].last_attacker == game::id_faction{-1});
}

DOCTEST_TEST_CASE("empire surrender transfers assets caps spies and recalculates interdicted fleets") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    game::current_state.colonies.erase(game::id_colony{1});
    test_colonies::Record(0).buildings[BUILDING_CAPITOL] = 1;
    test_colonies::Record(0).buildings[BUILDING_WARP_INTERDICTOR] = 1;
    test_colonies::Record(0).n_turns_existed = 3;
    game::current_state.ship_records[0].owner = factions::From_Legacy(0);
    game::current_state.ship_records[0].officer_index = 0;
    game::current_state.ship_records[1].owner = factions::From_Legacy(1);
    auto& leader = game::current_state.leaders[0];
    leader.employer = game::id_player{0};
    leader.type = LEADER_TYPE_SHIP;
    leader.location = 0;
    game::current_state.players[0].bc = 100;
    game::current_state.players[0].n_freighters = 2;
    game::current_state.players[1].bc = 25;
    game::current_state.players[1].n_freighters = 3;
    services.spies[1][1] = 50;
    services.spies[0][0] = 10;
    services.spies[0][1] = 20;
    COLCALC::Do_Surrender_Empire(game::id_player{0}, game::id_player{1});
    DOCTEST_CHECK((factions::Legacy_Code(game::current_state.ship_records[0].owner) == 1));
    DOCTEST_CHECK((game::current_state.ship_records[0].officer_index == -1));
    DOCTEST_CHECK((services.deassigned == std::vector<int16_t>{0}));
    DOCTEST_CHECK((legacy_world::Encode_Player_Slot(test_colonies::Record(0).owner) == 1));
    DOCTEST_CHECK((test_colonies::Record(0).buildings[BUILDING_CAPITOL] == 0));
    DOCTEST_CHECK((test_colonies::Record(0).n_turns_existed == 0));
    DOCTEST_CHECK((services.queues == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.population_limits == std::vector<int16_t>{0}));
    DOCTEST_CHECK((services.fleet_etas == std::vector<int16_t>{-1}));
    DOCTEST_CHECK((services.spy_reads == std::vector<std::array<int16_t, 2>>{{1, 1}, {0, 0}, {0, 1}}));
    DOCTEST_CHECK((services.spies[1][1] == spy::MAX_SPIES_PER_TARGET));
    DOCTEST_CHECK((game::current_state.players[1].bc == 125));
    DOCTEST_CHECK((game::current_state.players[1].n_freighters == 5));
    DOCTEST_CHECK((game::current_state.players[0].eliminated == 1));
    DOCTEST_CHECK(services.killed.empty());
    DOCTEST_CHECK(services.icon_updates == 2);
}

DOCTEST_TEST_CASE("star surrender returns the old colony leader to the pool") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    test_stars::Record(0).officer_index[0] = 1;
    auto& leader = game::current_state.leaders[1];
    leader.employer = game::id_player{0};
    leader.type = LEADER_TYPE_COLONY;
    leader.location = 0;
    COLCALC::Do_Surrender_Star(0, game::id_player{0}, game::id_player{1});
    DOCTEST_CHECK((services.pooled == std::vector<int16_t>{1}));
    DOCTEST_CHECK((test_stars::Record(0).officer_index[0] == -1));
    DOCTEST_CHECK((factions::Legacy_Code(test_stars::Record(0).surrender_to[0]) == -1));
    DOCTEST_CHECK((test_stars::Record(0).has_colony == game::player_bitmap::From_Byte(2)));
    DOCTEST_CHECK(test_stars::Record(0).visited.test(game::id_player{1}));
}

DOCTEST_TEST_CASE("surrender eligibility uses recipient range and excludes the capitol system") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    test_stars::Resize(2);
    game::current_state.players[0].home_planet_id = game::id_planet{1};
    game::current_state.players[1].ship_range = 9;
    services.in_normal_range = false;
    DOCTEST_CHECK_FALSE(COLCALC::May_Surrender_Star(game::current_state, game::Star_Key_At_Row(game::current_state, 0), game::id_player{0}, game::id_player{1}));
    services.in_normal_range = true;
    DOCTEST_CHECK(COLCALC::May_Surrender_Star(game::current_state, game::Star_Key_At_Row(game::current_state, 0), game::id_player{0}, game::id_player{1}));
    DOCTEST_CHECK((services.normal_ranges == std::vector<std::array<int16_t, 2>>{{0, 1}, {0, 1}}));
    int16_t stars[2] = {-1, -1};
    DOCTEST_CHECK(COLCALC::Get_Possible_Surrender_Systems(game::id_player{0}, game::id_player{1}, stars) == 1);
    DOCTEST_CHECK(stars[0] == 0);
    DOCTEST_CHECK((services.ranges == std::vector<std::array<int16_t, 3>>{{0, 1, 9}, {1, 1, 9}}));
    services.in_range = false;
    DOCTEST_CHECK(COLCALC::Get_Possible_Surrender_Systems(game::id_player{0}, game::id_player{1}, stars) == 0);
}

DOCTEST_TEST_CASE("only arrived colony leaders query their star anomaly") {
    using namespace TEST_COLONY_POP_GROWTH;
    Reset_Services_();
    auto& leader = game::current_state.leaders[0];
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
