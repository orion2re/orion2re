#include "pch.h"

#include "doctest/doctest.h"
#include "game/officer.h"
#include "game/report.h"

namespace TEST_REPORT {
    extern bool gnn_ready, interactive_diplomacy;
    extern int gnn_calls;
    extern int request_order, dialog_order, end_order, update_count;
    extern int16_t requested_player, dialog_player;
    extern int movie_calls, room_calls;
    extern int16_t room_message;
    extern int evolutionary_calls, discovery_calls;
    extern int16_t evolutionary_player, discovery_star;
    extern uint8_t discovery_special;
    extern int science_calls, science_kind;
    extern uint16_t science_tech;
    extern int16_t science_source;
    extern std::string message;
    extern int8_t level_report_result, level_report_show;
    extern int level_report_calls;
    extern int16_t level_report_player;
    extern int16_t relocation_destination;
    extern bool relocation_allowed;
    extern int move_attempts, move_calls, recalculation_calls, autosave_calls;
    extern int16_t moved_ship, moved_destination;
    extern int16_t colonizable_planet_count;
    extern int16_t confirmation_result;
    extern bool accept_officer_offer;
    extern s_ship_data* cleared_auto_colonization_ship;

    void Reset_();
    void Add_Colony_Ship_();
    void Add_Colony_Base_();
}

DOCTEST_TEST_CASE("GNN probes do not consume a notification and display happens once per player") {
    TEST_REPORT::Reset_();
    TEST_REPORT::gnn_ready = true;
    DOCTEST_CHECK(REPORT::Has_GNN(0, false) == 7);
    DOCTEST_CHECK(TEST_REPORT::gnn_calls == 0);
    DOCTEST_CHECK(REPORT::Has_GNN(0, true) == 7);
    DOCTEST_CHECK(TEST_REPORT::gnn_calls == 1);
    DOCTEST_CHECK(REPORT::Has_GNN(0, true) == 0);
    DOCTEST_CHECK(REPORT::Has_GNN(1, true) == 7);
    DOCTEST_CHECK(TEST_REPORT::gnn_calls == 2);
}

DOCTEST_TEST_CASE("interactive diplomacy waits for network permission before consuming the message") {
    TEST_REPORT::Reset_();
    MOX::NUM_PLAYERS = 2;
    MOX::game_type = 2;
    MOX::player[1].diplomacy_message[0] = 10;
    DOCTEST_CHECK(REPORT::Has_Diplomacy_Messages(0, false) == 5);
    DOCTEST_CHECK(TEST_REPORT::request_order == 0);
    DOCTEST_CHECK((MOX::player[1].diplomacy_message[0] == 10));
    DOCTEST_CHECK(REPORT::Has_Diplomacy_Messages(0, true) == 5);
    DOCTEST_CHECK(TEST_REPORT::requested_player == 1);
    DOCTEST_CHECK(TEST_REPORT::dialog_player == 1);
    DOCTEST_CHECK(TEST_REPORT::request_order < TEST_REPORT::dialog_order);
    DOCTEST_CHECK(TEST_REPORT::dialog_order < TEST_REPORT::end_order);
    DOCTEST_CHECK(TEST_REPORT::update_count == 1);
    DOCTEST_CHECK((MOX::player[1].diplomacy_message[0] == 0));
    TEST_REPORT::Reset_();
    MOX::NUM_PLAYERS = 2;
    MOX::game_type = 2;
    MOX::player[1].diplomacy_message[0] = 10;
    TEST_REPORT::interactive_diplomacy = false;
    DOCTEST_CHECK(REPORT::Has_Diplomacy_Messages(0, true) == 5);
    DOCTEST_CHECK(TEST_REPORT::dialog_player == 1);
    DOCTEST_CHECK(TEST_REPORT::request_order == 0);
    DOCTEST_CHECK(TEST_REPORT::end_order == 0);
}

DOCTEST_TEST_CASE("Antaran notifications choose a movie or a targeted room only once") {
    TEST_REPORT::Reset_();
    MOX::stardate = 35001;
    MOX::antarans.invasion_stardate = 35000;
    DOCTEST_CHECK(REPORT::Antarans(0, 0) == 6);
    DOCTEST_CHECK(TEST_REPORT::movie_calls == 0);
    DOCTEST_CHECK(REPORT::Antarans(0, 1) == 1);
    DOCTEST_CHECK(TEST_REPORT::movie_calls == 1);
    DOCTEST_CHECK(REPORT::Antarans(0, 1) == 0);
    TEST_REPORT::Reset_();
    MOX::stardate = 35001;
    MOX::antarans.invasion_stardate = 35000;
    MOX::ship[0].owner = MAX_PLAYERS;
    MOX::ship[0].status = 1;
    MOX::ship[0].location = SHIP_LOCATION_MOVING_OFFSET;
    MOX::ship[0].turns_left = 3;
    MOX::star[0].has_colony = 1;
    const uint32_t seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(650);
    DOCTEST_CHECK(REPORT::Antarans(0, 1) == 6);
    DOCTEST_CHECK(TEST_REPORT::room_calls == 1);
    DOCTEST_CHECK(TEST_REPORT::room_message == 0);
    DOCTEST_CHECK(TEST_REPORT::movie_calls == 0);
    game_random::Set_Random_Seed(seed);
}

DOCTEST_TEST_CASE("discovery and evolutionary prompts preserve query-only behavior") {
    TEST_REPORT::Reset_();
    MOX::player[0].tech_applications[TECH_APP_EVOLUTIONARY_MUTATION] = TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_CHECK(REPORT::Has_Evolutionary_Upgrade(0, 0) == 1);
    DOCTEST_CHECK(TEST_REPORT::evolutionary_calls == 0);
    DOCTEST_CHECK(REPORT::Has_Evolutionary_Upgrade(0, 1) == 1);
    DOCTEST_CHECK(TEST_REPORT::evolutionary_player == 0);
    DOCTEST_CHECK(TEST_REPORT::evolutionary_calls == 1);
    DOCTEST_CHECK(REPORT::Has_Evolutionary_Upgrade(0, 1) == 0);
    MOX::star[0].just_visited = 1;
    MOX::star[0].system_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
    DOCTEST_CHECK(REPORT::Explored_New_Star(0, 0) == 1);
    DOCTEST_CHECK(TEST_REPORT::discovery_calls == 0);
    DOCTEST_CHECK(REPORT::Explored_New_Star(0, 1) == 1);
    DOCTEST_CHECK(TEST_REPORT::discovery_star == 0);
    DOCTEST_CHECK(TEST_REPORT::discovery_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS);
    DOCTEST_CHECK(TEST_REPORT::discovery_calls == 1);
    DOCTEST_CHECK((MOX::star[0].just_visited == 0));
}

DOCTEST_TEST_CASE("technology reports dispatch their payload before being consumed") {
    TEST_REPORT::Reset_();
    s_report report = {};
    report.player_idx = 0;
    report.type = 1;
    report.d.steal.tech_app_id = TECH_APP_NUCLEAR_DRIVE;
    report.d.steal.victim_player_idx = 1;
    REPORT::Add_Report(&report);
    DOCTEST_CHECK(REPORT::Has_Report(0, false) == 1);
    DOCTEST_CHECK(TEST_REPORT::science_calls == 0);
    DOCTEST_CHECK(REPORT::Has_Report(0, true) == 2);
    DOCTEST_CHECK(TEST_REPORT::science_kind == 1);
    DOCTEST_CHECK(TEST_REPORT::science_tech == TECH_APP_NUCLEAR_DRIVE);
    DOCTEST_CHECK(TEST_REPORT::science_source == 1);
    DOCTEST_CHECK((REPORT::reports[0].player_idx == -1));
    report = {};
    report.type = 3;
    report.d.artifacts.tech_app_id = TECH_APP_NUCLEAR_BOMB;
    report.d.artifacts.planet_idx = 4;
    REPORT::Add_Report(&report);
    DOCTEST_CHECK(REPORT::Has_Report(0, true) == 2);
    DOCTEST_CHECK(TEST_REPORT::science_kind == 3);
    DOCTEST_CHECK(TEST_REPORT::science_tech == TECH_APP_NUCLEAR_BOMB);
    DOCTEST_CHECK(TEST_REPORT::science_source == 4);
    DOCTEST_CHECK(TEST_REPORT::science_calls == 2);
}

DOCTEST_TEST_CASE("ordinary spy reports format their message before consumption") {
    TEST_REPORT::Reset_();
    char race[] = "Bulrathi";
    char technology[] = "Nuclear drive";
    snprintf(MOX::player[1].race_name, sizeof(MOX::player[1].race_name), "%s", race);
    TECHDATA::technology_applications[TECH_APP_NUCLEAR_DRIVE].name = technology;
    s_report report = {};
    report.type = 2;
    report.d.enemy_steal.enemy_player_idx = 1;
    report.d.enemy_steal.tech_app_id = TECH_APP_NUCLEAR_DRIVE;
    REPORT::Add_Report(&report);
    DOCTEST_CHECK(REPORT::Has_Report(0, true) == 1);
    DOCTEST_CHECK(TEST_REPORT::message == "Bulrathi from Nuclear drive");
    DOCTEST_CHECK((REPORT::reports[0].player_idx == -1));
    TEST_REPORT::Reset_();
}

DOCTEST_TEST_CASE("relocation consumes new-ship markers but sends only feasible move orders") {
    for (bool allowed : {false, true}) {
        TEST_REPORT::Reset_();
        TEST_REPORT::relocation_destination = 3;
        TEST_REPORT::relocation_allowed = allowed;
        MOX::ship[0].just_built = 1;
        REPORT::Redirect_Newly_Built_Ships(0);
        DOCTEST_CHECK((MOX::ship[0].just_built == 0));
        DOCTEST_CHECK(TEST_REPORT::move_attempts == 1);
        DOCTEST_CHECK(TEST_REPORT::move_calls == (allowed ? 1 : 0));
        DOCTEST_CHECK(TEST_REPORT::recalculation_calls == 1);
        if (allowed) {
            DOCTEST_CHECK(TEST_REPORT::moved_ship == 0);
            DOCTEST_CHECK(TEST_REPORT::moved_destination == 3);
        }
    }
}

DOCTEST_TEST_CASE("officer-level reports defer multiplayer autosave until reporting finishes") {
    TEST_REPORT::Reset_();
    MOX::stardate = 35001;
    MOX::NUM_STARS = 0;
    MOX::game_type = 2;
    MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    MOX::player[0].officer_for_hire = -1;
    TEST_REPORT::level_report_result = 3;
    DOCTEST_CHECK(REPORT::Must_Display_Reports(0) == 3);
    DOCTEST_CHECK(TEST_REPORT::level_report_calls == 1);
    DOCTEST_CHECK(TEST_REPORT::level_report_player == 0);
    DOCTEST_CHECK(TEST_REPORT::level_report_show == 0);
    DOCTEST_CHECK(TEST_REPORT::autosave_calls == 0);
    TEST_REPORT::level_report_result = 0;
    DOCTEST_CHECK(REPORT::Must_Display_Reports(0) == 0);
    DOCTEST_CHECK(TEST_REPORT::autosave_calls == 1);
}

DOCTEST_TEST_CASE("native occupation reports follow the gameplay setting") {
    TEST_REPORT::Reset_();
    MOX::NUM_COLONIES = 1;
    MOX::colony[0].owner = 0;
    MOX::colony[0].occupation_policy = 4;
    MOX::colony[0].n_pops = 1;
    POP::Set_Race(&MOX::colony[0].pop[0], POP::RACE_NATIVE);
    POP::Set_Assigned(&MOX::colony[0].pop[0], true);

    COLONY::allow_native_annihilation = 0;
    DOCTEST_CHECK(REPORT::Need_To_Set_Occupation_Policy(0, 0) == 0);

    COLONY::allow_native_annihilation = 1;
    DOCTEST_CHECK(REPORT::Need_To_Set_Occupation_Policy(0, 0) == 1);
}

DOCTEST_TEST_CASE("outpost prompt remains enabled by default") {
    TEST_REPORT::Reset_();

    DOCTEST_CHECK(REPORT::Colonization(0, 0) == 1);
}

DOCTEST_TEST_CASE("leader offers preserve classic decline behavior by default") {
    TEST_REPORT::Reset_();
    MOX::player[0].officer_for_hire = 3;
    MOX::player[0].stardate_last_offered_hero = 35005;
    MOX::leaders[3].player_index = 0;

    DOCTEST_CHECK(REPORT::Has_Officer_For_Hire(0, 0) == 1);
    DOCTEST_CHECK(MOX::player[0].officer_for_hire == 3);
    DOCTEST_CHECK(REPORT::Has_Officer_For_Hire(0, 1) == 1);
    DOCTEST_CHECK(MOX::leaders[3].status == 4);
    DOCTEST_CHECK(MOX::leaders[3].player_index == 0);
    DOCTEST_CHECK((MOX::player[0].stardate_last_offered_hero == 35005));
    DOCTEST_CHECK(MOX::player[0].officer_for_hire == -1);
}

DOCTEST_TEST_CASE("declining a leader offer keeps it hireable without resetting the timer") {
    TEST_REPORT::Reset_();
    OFFICER::rejected_offer_keeps_timer = 1;
    MOX::player[0].officer_for_hire = 3;
    MOX::player[0].stardate_last_offered_hero = 35001;
    MOX::leaders[3].player_index = 0;
    MOX::leaders[3].xp = 60;

    DOCTEST_CHECK(REPORT::Has_Officer_For_Hire(0, 1) == 1);
    DOCTEST_CHECK(MOX::leaders[3].status == 4);
    DOCTEST_CHECK(MOX::leaders[3].player_index == 0);
    DOCTEST_CHECK((MOX::leaders[3].xp == 60));
    DOCTEST_CHECK(MOX::leaders[3].eta == 0);
    DOCTEST_CHECK((MOX::player[0].stardate_last_offered_hero == 35001));
    DOCTEST_CHECK(MOX::player[0].officer_for_hire == -1);
}

DOCTEST_TEST_CASE("accepting a leader into the pool records its offer date") {
    TEST_REPORT::Reset_();
    OFFICER::rejected_offer_keeps_timer = 1;
    TEST_REPORT::accept_officer_offer = true;
    MOX::stardate = 35006;
    MOX::player[0].officer_for_hire = 3;
    MOX::player[0].stardate_last_offered_hero = 35001;
    MOX::leaders[3].player_index = 0;

    DOCTEST_CHECK(REPORT::Has_Officer_For_Hire(0, 1) == 1);
    DOCTEST_CHECK(MOX::leaders[3].status == 0);
    DOCTEST_CHECK(MOX::leaders[3].player_index == 0);
    DOCTEST_CHECK((MOX::leaders[3].xp == 0));
    DOCTEST_CHECK((MOX::player[0].stardate_last_offered_hero == 35005));
    DOCTEST_CHECK(MOX::player[0].officer_for_hire == -1);
}

DOCTEST_TEST_CASE("outpost prompt can be skipped") {
    TEST_REPORT::Reset_();
    REPORT::no_outpost_prompt = 1;

    DOCTEST_CHECK(REPORT::Colonization(0, 0) == 0);
}

DOCTEST_TEST_CASE("initial research prompt can be suppressed") {
    TEST_REPORT::Reset_();
    MOX::stardate = 35001;
    MOX::player[0].current_research_field = TECH_FIELD_STARTING_TECH;
    MOX::player[0].tech_fields[TECH_FIELD_STARTING_TECH] = 3;

    REPORT::initial_tech_prompt = 1;
    DOCTEST_CHECK(REPORT::Set_Initial_Tech(0, 0) == 2);

    REPORT::initial_tech_prompt = 0;
    DOCTEST_CHECK(REPORT::Set_Initial_Tech(0, 0) == 0);

    REPORT::initial_tech_prompt = 1;
}

DOCTEST_TEST_CASE("skipping outpost prompt retains valid colony prompt") {
    TEST_REPORT::Reset_();
    TEST_REPORT::Add_Colony_Ship_();
    TEST_REPORT::colonizable_planet_count = 1;
    REPORT::no_outpost_prompt = 1;

    DOCTEST_CHECK(REPORT::Colonization(0, 0) == 1);
}

DOCTEST_TEST_CASE("skipping outpost prompt suppresses unusable colony prompt") {
    TEST_REPORT::Reset_();
    TEST_REPORT::Add_Colony_Ship_();
    REPORT::no_outpost_prompt = 1;

    DOCTEST_CHECK(REPORT::Colonization(0, 0) == 0);
}

DOCTEST_TEST_CASE("serious-first summary order preserves queue order within each group") {
    TEST_REPORT::Reset_();
    MSG_::msgs[5].msg_type = MSG_TYPE_COLONY;
    MSG_::msgs[5].u.colony.subtype = MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED;
    MSG_::msgs[6].msg_type = MSG_TYPE_LOST_ATTACKING_SPY;
    MSG_::msgs[7].msg_type = MSG_TYPE_COLONY;
    MSG_::msgs[7].u.colony.subtype = MSG_COLONY_SUBTYPE_REBELLION;
    MSG_::msgs[8].msg_type = MSG_TYPE_HYPERSPACE_FLUX;
    MSG_::msgs[9].msg_type = MSG_TYPE_LOST_DEFENDING_SPY;

    uint8_t order[5] = {};
    REPORT::Order_Turn_Summary_Messages(order, 5, 5, false);
    const uint8_t original[] = {5, 6, 7, 8, 9};
    for (int i = 0; i < 5; ++i) DOCTEST_CHECK(order[i] == original[i]);

    REPORT::Order_Turn_Summary_Messages(order, 5, 5, true);
    const uint8_t sorted[] = {6, 7, 9, 5, 8};
    for (int i = 0; i < 5; ++i) DOCTEST_CHECK(order[i] == sorted[i]);
}

DOCTEST_TEST_CASE("summary sorting uses serious-only trigger classification") {
    TEST_REPORT::Reset_();
    s_MSG msg = {};
    msg.msg_type = MSG_TYPE_COLONY;
    msg.u.colony.subtype = MSG_COLONY_SUBTYPE_STARVATION;
    DOCTEST_CHECK(REPORT::Is_Serious_Msg(msg));
    msg.u.colony.subtype = MSG_COLONY_SUBTYPE_PLAGUE;
    DOCTEST_CHECK_FALSE(REPORT::Is_Serious_Msg(msg));
    msg.msg_type = MSG_TYPE_HYPERSPACE_BEAST;
    msg.u.hyperspace_beast.ship_idx = -1;
    DOCTEST_CHECK_FALSE(REPORT::Is_Serious_Msg(msg));
    msg.u.hyperspace_beast.ship_idx = 1;
    msg.u.hyperspace_beast.fate = 1;
    DOCTEST_CHECK(REPORT::Is_Serious_Msg(msg));
}

DOCTEST_TEST_CASE("summary sorting config does not change serious-only triggers") {
    TEST_REPORT::Reset_();
    MSG_::first_msg[0] = 0;
    MSG_::n_msgs[0] = 1;
    MSG_::msgs[0].msg_type = MSG_TYPE_COLONY;
    MSG_::msgs[0].u.colony.subtype = MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED;
    MOX::settings.end_of_turn_summary = 1;
    MOX::settings.only_show_serious_turn_summary_reports = 1;
    REPORT::sort_serious_turn_summary_first = true;
    REPORT::displayed_msgs = 0;
    DOCTEST_CHECK(REPORT::Has_Msg(0, 0) == 0);

    MSG_::msgs[0].msg_type = MSG_TYPE_LOST_ATTACKING_SPY;
    DOCTEST_CHECK(REPORT::Has_Msg(0, 0) == 4);
    MSG_::msgs[0].msg_type = MSG_TYPE_COLONY;
    MSG_::msgs[0].u.colony.subtype = MSG_COLONY_SUBTYPE_REBELLION;
    DOCTEST_CHECK(REPORT::Has_Msg(0, 0) == 4);
    REPORT::sort_serious_turn_summary_first = false;
}

DOCTEST_TEST_CASE("filtered colony prompt retains its ship for assignment cleanup") {
    TEST_REPORT::Reset_();
    TEST_REPORT::Add_Colony_Ship_();
    TEST_REPORT::Add_Colony_Base_();
    TEST_REPORT::confirmation_result = 1;
    REPORT::no_outpost_prompt = 1;

    DOCTEST_CHECK(REPORT::Colonization(0, 1) == 1);
    DOCTEST_CHECK(TEST_REPORT::cleared_auto_colonization_ship == &MOX::ship[1]);
}
