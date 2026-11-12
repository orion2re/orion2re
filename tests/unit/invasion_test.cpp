#include "pch.h"

#include "doctest/doctest.h"
#include "game/colony.h"
#include "game/invasion.h"
#include "game/officer_roster.h"
#include "game/pop.h"

namespace test_ground_commando_runtime {
    extern std::vector<int16_t> killed_ships;
    extern std::vector<s_MSG> messages;
    extern std::vector<int16_t> message_players;
    extern int stats_updates;
    extern int16_t stats_player;
    extern int icon_updates;
    extern int8_t owner_at_icon_update;
    extern int seed_queries;
    extern uint32_t random_seed;
    extern int rebel_choices;
    extern uint8_t rebel_weights[MAX_PLAYERS];
    void Reset_();
    s_colony* Colony_(int index = 0);
    s_leader_data* Leader_(int index);
    s_ship_data* Ship_(int index);
    void Set_Colony_Officer_(int leader_idx);
    void Set_Officer_Level_(int16_t level);
    void Set_Leader_(int index, int8_t player, int8_t type, int8_t status);
    const int16_t* Officer_Ids_();
    int32_t Last_Buffer_Entry_();
    void Set_Random_Result_(uint32_t value);
    int Random_Call_Count_();
    int Granted_Tech_Count_();
} // namespace test_ground_commando_runtime

DOCTEST_TEST_CASE("transport unloading consumes only enough ships to meet the infantry limit") {
    using namespace test_ground_commando_runtime;
    Reset_();
    MOX::planet[0].size = PLANET_SIZE_MEDIUM;
    Colony_()->military[0] = 3;
    Ship_(0)->d.ship_type = Ship_(1)->d.ship_type = SHIP_TYPE_TRANSPORT;
    int16_t ships[] = {0, 1};
    DOCTEST_CHECK(invasion::Unload_Transports(Colony_(), ships, 2) == 1);
    DOCTEST_CHECK((killed_ships == std::vector<int16_t>{0}));
    DOCTEST_CHECK((Colony_()->military[0] == 7));
    DOCTEST_CHECK((Ship_(1)->status == 0));
    DOCTEST_CHECK(stats_updates == 1);
    DOCTEST_CHECK(stats_player == 0);
    DOCTEST_CHECK(invasion::Unload_Transports(Colony_(), ships + 1, 1) == 0);
    DOCTEST_CHECK(killed_ships.size() == 1);
    DOCTEST_CHECK(stats_updates == 2);
}

DOCTEST_TEST_CASE("unopposed invasion saves its seed and refreshes icons after capture") {
    using namespace test_ground_commando_runtime;
    Reset_();
    Ship_(0)->owner = 1;
    Ship_(0)->d.ship_type = SHIP_TYPE_TRANSPORT;
    Ship_(0)->officer_index = -1;
    Colony_()->n_pops = 1;
    int16_t ships[] = {0};
    s_battle_results_info results = {};
    invasion::Invade(ships, 1, 0, &results);
    DOCTEST_CHECK(seed_queries == 1);
    DOCTEST_CHECK((results.ground_defense_strength == (int32_t)random_seed));
    DOCTEST_CHECK((results.attacker_invasion.player_id == 1));
    DOCTEST_CHECK((results.defender_invasion.player_id == 0));
    DOCTEST_CHECK((Colony_()->owner == 1));
    DOCTEST_CHECK((Colony_()->military[0] == 2));
    DOCTEST_CHECK((killed_ships == std::vector<int16_t>{0}));
    DOCTEST_CHECK(icon_updates == 1);
    DOCTEST_CHECK(owner_at_icon_update == 1);
}

DOCTEST_TEST_CASE("rebellion weights and reports preserve both participants") {
    using namespace test_ground_commando_runtime;
    Reset_();
    Colony_()->n_pops = 2;
    Colony_()->pop[0] = Colony_()->pop[1] = 1 | 0x400;
    invasion::Check_Rebellion(Colony_());
    DOCTEST_CHECK(rebel_choices == 1);
    DOCTEST_CHECK((rebel_weights[0] == 0));
    DOCTEST_CHECK((rebel_weights[1] == 2));
    DOCTEST_REQUIRE(messages.size() == 2);
    DOCTEST_CHECK((message_players == std::vector<int16_t>{0, 1}));
    DOCTEST_CHECK((messages[0].msg_type == MSG_TYPE_COLONY));
    DOCTEST_CHECK((messages[0].u.colony.subtype == MSG_COLONY_SUBTYPE_REBELLION));
    DOCTEST_CHECK((messages[0].u.colony.c.rebels.owner_player == 1));
    DOCTEST_CHECK((messages[0].u.colony.c.rebels.rebels == 1));
    DOCTEST_CHECK((messages[0].u.colony.c.rebels.lost == 0));
    DOCTEST_CHECK((Colony_()->owner == 1));
    DOCTEST_CHECK((Colony_()->just_produced == COLONY_PRODUCTION_JUST_LIBERATED));
    Reset_();
    Colony_()->n_pops = 1;
    Colony_()->pop[0] = 1 | 0x400;
    Colony_()->occupation_policy = 4;
    invasion::Check_Rebellion(Colony_());
    DOCTEST_CHECK(rebel_choices == 0);
    DOCTEST_CHECK(messages.empty());
}

DOCTEST_TEST_CASE("leader roster pagination falls back when its page becomes empty") {
    using namespace test_ground_commando_runtime;
    Reset_();
    OFFICER::leaders_no_limit = 1;
    for (int i = 0; i < 5; ++i) {
        Set_Leader_(i, 0, LEADER_TYPE_SHIP, 0);
    }
    OFFICER::leader_page = 1;
    OFFICER::Build_Captain_Id_List();
    DOCTEST_CHECK(MOX::n_fltscrn_officers == 1);
    DOCTEST_CHECK((MOX::officer_id_list[0] == 0));
    OFFICER::leader_page = 2;
    OFFICER::Build_Captain_Id_List();
    DOCTEST_CHECK(OFFICER::leader_page == 0);
    DOCTEST_CHECK(MOX::n_fltscrn_officers == 4);
    DOCTEST_CHECK((MOX::officer_id_list[0] == 4));
    Reset_();
}

DOCTEST_TEST_CASE("manual building and satellite scraps share a per-colony turn limit") {
    s_colony colony = {};
    s_colony other_colony = {};
    constexpr uint16_t turn = 35000;

    COLONY::max_buildings_scrapped_per_turn = 1;
    DOCTEST_CHECK(COLONY::Can_Scrap_Building(colony, turn));
    COLONY::Record_Building_Scrap(&colony, turn);
    DOCTEST_CHECK_FALSE(COLONY::Can_Scrap_Building(colony, turn));
    DOCTEST_CHECK(COLONY::Can_Scrap_Building(other_colony, turn));

    COLONY::max_buildings_scrapped_per_turn = 3;
    DOCTEST_CHECK(COLONY::Can_Scrap_Building(colony, turn));
    COLONY::Record_Building_Scrap(&colony, turn);
    COLONY::Record_Building_Scrap(&colony, turn);
    DOCTEST_CHECK(colony.buildings_scrapped_this_turn == 3);
    DOCTEST_CHECK_FALSE(COLONY::Can_Scrap_Building(colony, turn));
    DOCTEST_CHECK(COLONY::Can_Scrap_Building(colony, turn + 1));
    COLONY::Record_Building_Scrap(&colony, turn + 1);
    DOCTEST_CHECK(colony.buildings_scrapped_this_turn == 1);
    DOCTEST_CHECK((colony.last_turn_building_destroyed == turn + 1));
    COLONY::max_buildings_scrapped_per_turn = 1;
}

DOCTEST_TEST_CASE("native colony detection excludes androids") {
    test_ground_commando_runtime::Reset_();
    s_colony* colony = test_ground_commando_runtime::Colony_();
    colony->n_pops = 2;
    POP::Set_Race(&colony->pop[0], POP::RACE_ANDROID);
    POP::Set_Race(&colony->pop[1], 0);

    DOCTEST_CHECK_FALSE(COLONY::Colony_Has_Natives(0));

    POP::Set_Race(&colony->pop[1], POP::RACE_NATIVE);
    DOCTEST_CHECK(COLONY::Colony_Has_Natives(0));
}

DOCTEST_TEST_CASE("native occupation policy follows the gameplay setting") {
    test_ground_commando_runtime::Reset_();
    s_colony* colony = test_ground_commando_runtime::Colony_();
    colony->occupation_policy = 4;
    colony->n_pops = 1;
    POP::Set_Race(&colony->pop[0], POP::RACE_NATIVE);
    POP::Set_Assigned(&colony->pop[0], true);

    COLONY::allow_native_annihilation = 0;
    DOCTEST_CHECK_FALSE(COLONY::Colony_Needs_Occupation_Policy(0));

    COLONY::allow_native_annihilation = 1;
    DOCTEST_CHECK(COLONY::Colony_Needs_Occupation_Policy(0));

    colony->occupation_policy = 3;
    DOCTEST_CHECK_FALSE(COLONY::Colony_Needs_Occupation_Policy(0));
}

DOCTEST_TEST_CASE("native annihilation follows the gameplay setting") {
    test_ground_commando_runtime::Reset_();
    s_colony* colony = test_ground_commando_runtime::Colony_();
    colony->owner = 0;
    uint32_t native_pop = 0;
    POP::Set_Race(&native_pop, POP::RACE_NATIVE);

    COLONY::allow_native_annihilation = 0;
    DOCTEST_CHECK_FALSE(COLONY::Colony_Pop_Is_Annihilation_Target(colony, native_pop));

    COLONY::allow_native_annihilation = 1;
    DOCTEST_CHECK(COLONY::Colony_Pop_Is_Annihilation_Target(colony, native_pop));

    uint32_t conquered_pop = 0;
    POP::Set_Race(&conquered_pop, 1);
    POP::Set_Conquered(&conquered_pop, true);
    COLONY::allow_native_annihilation = 0;
    DOCTEST_CHECK(COLONY::Colony_Pop_Is_Annihilation_Target(colony, conquered_pop));
}

DOCTEST_TEST_CASE("home planet reassignment excludes outposts") {
    test_ground_commando_runtime::Reset_();
    MOX::NUM_COLONIES = 2;

    s_colony* colony = test_ground_commando_runtime::Colony_(0);
    colony->owner = 0;
    colony->planet = 1;
    colony->n_pops = 1;

    s_colony* outpost = test_ground_commando_runtime::Colony_(1);
    outpost->owner = 0;
    outpost->planet = 2;
    outpost->outpost_flag = 1;

    invasion::Assign_New_Home_Planet(0, 0);

    DOCTEST_CHECK((MOX::player[0].home_planet_id == -1));
}

DOCTEST_TEST_CASE("colony ground combat info preserves the resolved Commando officer") {
    test_ground_commando_runtime::Reset_();
    test_ground_commando_runtime::Set_Colony_Officer_(1);
    test_ground_commando_runtime::Set_Officer_Level_(1);

    s_colony* colony = test_ground_commando_runtime::Colony_();
    colony->owner = 0;
    colony->officer = 0;
    colony->military[0] = 1;

    s_leader_data* leader = test_ground_commando_runtime::Leader_(1);
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO;

    invasion::ground_commando_defender_x5 = 0;
    s_invasion_side_info side = {};
    invasion::Compute_Colony_Ground_Combat_Info(colony, &side, false);

    DOCTEST_CHECK(side.combat_modifiers.officer_bonus == 4);
    DOCTEST_CHECK(side.combat_modifiers.officer_idx == 1);
}

DOCTEST_TEST_CASE("leader hire costs use configurable Commando and Megawealth rules") {
    test_ground_commando_runtime::Reset_();
    s_leader_data* leader = test_ground_commando_runtime::Leader_(0);
    leader->type = LEADER_TYPE_SHIP;
    MOX::skill_data[2].cost = 5;
    MOX::skill_data[3].cost = 7;

    OFFICER::commando_hire_cost = {1, 3};
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO;
    DOCTEST_CHECK(OFFICER::Officer_Skill_Value(0) == 3);
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO2;
    DOCTEST_CHECK(OFFICER::Officer_Skill_Value(0) == 6);
    leader->type = LEADER_TYPE_COLONY;
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO;
    DOCTEST_CHECK(OFFICER::Officer_Skill_Value(0) == 1);
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO2;
    DOCTEST_CHECK(OFFICER::Officer_Skill_Value(0) == 2);

    leader->type = LEADER_TYPE_SHIP;
    OFFICER::commando_hire_cost = {4, 6};
    DOCTEST_CHECK(OFFICER::Officer_Skill_Value(0) == 12);

    OFFICER::commando_hire_cost = {128, 255};
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO;
    DOCTEST_CHECK(OFFICER::Officer_Skill_Value(0) == 255);
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO2;
    DOCTEST_CHECK(OFFICER::Officer_Skill_Value(0) == 510);
    leader->type = LEADER_TYPE_COLONY;
    DOCTEST_CHECK(OFFICER::Officer_Skill_Value(0) == 256);

    leader->general_skills = LEADER_GENERAL_SKILL_MEGAWEALTH;
    leader->skill_value = 5;
    OFFICER::megawealth_doubles_hire_cost = 0;
    DOCTEST_CHECK(OFFICER::Officer_Cost(0, 0) == 50);
    OFFICER::megawealth_doubles_hire_cost = 1;
    DOCTEST_CHECK(OFFICER::Officer_Cost(0, 0) == 100);

    leader->general_skills = LEADER_GENERAL_SKILL_ASSASSIN;
    MOX::skill_data[0].cost = 255;
    OFFICER::megawealth_doubles_hire_cost = 0;
    DOCTEST_CHECK(OFFICER::Officer_Skill_Value(0) == 255);

    leader->skill_value = 1530;
    leader->xp = 1000;
    DOCTEST_CHECK(OFFICER::Officer_Cost(0, 0) == 76500);
}

DOCTEST_TEST_CASE("defending ground Commando setting selects the configured multipliers") {
    test_ground_commando_runtime::Reset_();
    test_ground_commando_runtime::Set_Colony_Officer_(0);
    s_colony colony = {};
    colony.officer = 0;
    s_invasion_player_bonuses bonuses = {};

    s_leader_data* leader = test_ground_commando_runtime::Leader_(0);
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO;
    test_ground_commando_runtime::Set_Officer_Level_(1);

    invasion::ground_commando_defender_x5 = 0;
    invasion::Compute_Colony_Officer_Bonus(&colony, &bonuses);
    DOCTEST_CHECK(bonuses.officer_bonus == 4);

    bonuses = {};
    invasion::ground_commando_defender_x5 = 1;
    invasion::Compute_Colony_Officer_Bonus(&colony, &bonuses);
    DOCTEST_CHECK(bonuses.officer_bonus == 10);

    bonuses = {};
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO2;
    test_ground_commando_runtime::Set_Officer_Level_(2);
    invasion::ground_commando_defender_x5 = 0;
    invasion::Compute_Colony_Officer_Bonus(&colony, &bonuses);
    DOCTEST_CHECK(bonuses.officer_bonus == 9);

    bonuses = {};
    invasion::ground_commando_defender_x5 = 1;
    invasion::Compute_Colony_Officer_Bonus(&colony, &bonuses);
    DOCTEST_CHECK(bonuses.officer_bonus == 22);
}

DOCTEST_TEST_CASE("attacking ground Commando setting selects the configured multipliers") {
    test_ground_commando_runtime::Reset_();
    int16_t ship_indexes[] = {0};
    s_invasion_player_bonuses bonuses = {};

    s_ship_data* ship = test_ground_commando_runtime::Ship_(0);
    ship->officer_index = 0;
    s_leader_data* leader = test_ground_commando_runtime::Leader_(0);
    leader->eta = 0;
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO;
    test_ground_commando_runtime::Set_Officer_Level_(1);

    invasion::ground_commando_attacker_x2 = 0;
    invasion::Compute_Attacker_Officer_Bonus(ship_indexes, 1, &bonuses);
    DOCTEST_CHECK(bonuses.officer_bonus == 10);

    bonuses = {};
    invasion::ground_commando_attacker_x2 = 1;
    invasion::Compute_Attacker_Officer_Bonus(ship_indexes, 1, &bonuses);
    DOCTEST_CHECK(bonuses.officer_bonus == 4);

    bonuses = {};
    leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO2;
    test_ground_commando_runtime::Set_Officer_Level_(2);
    invasion::ground_commando_attacker_x2 = 0;
    invasion::Compute_Attacker_Officer_Bonus(ship_indexes, 1, &bonuses);
    DOCTEST_CHECK(bonuses.officer_bonus == 23);

    bonuses = {};
    invasion::ground_commando_attacker_x2 = 1;
    invasion::Compute_Attacker_Officer_Bonus(ship_indexes, 1, &bonuses);
    DOCTEST_CHECK(bonuses.officer_bonus == 9);
}

DOCTEST_TEST_CASE("attacking ground Commando ties select the last listed officer") {
    test_ground_commando_runtime::Reset_();
    int16_t ship_indexes[] = {0, 1};
    s_invasion_player_bonuses bonuses = {};

    test_ground_commando_runtime::Ship_(0)->officer_index = 0;
    test_ground_commando_runtime::Ship_(1)->officer_index = 1;

    s_leader_data* first_leader = test_ground_commando_runtime::Leader_(0);
    first_leader->eta = 0;
    first_leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO;
    test_ground_commando_runtime::Set_Colony_Officer_(0);
    test_ground_commando_runtime::Set_Officer_Level_(1);

    s_leader_data* second_leader = test_ground_commando_runtime::Leader_(1);
    second_leader->eta = 0;
    second_leader->general_skills = LEADER_GENERAL_SKILL_COMMANDO;
    test_ground_commando_runtime::Set_Colony_Officer_(1);
    test_ground_commando_runtime::Set_Officer_Level_(1);

    invasion::ground_commando_attacker_x2 = 0;
    invasion::Compute_Attacker_Officer_Bonus(ship_indexes, 2, &bonuses);

    DOCTEST_CHECK(bonuses.officer_bonus == 10);
    DOCTEST_CHECK(bonuses.officer_idx == 1);
}

DOCTEST_TEST_CASE("troop search stops at the exhausted-side sentinel") {
    for (int8_t start = 0; start <= UNIT_COUNT; ++start) {
        s_invasion_side_info side = {};
        side.current_troop_type_idx = start;
        side.current_unit_damage = 1;

        invasion::Find_Troops(&side);

        DOCTEST_CHECK(side.current_troop_type_idx == UNIT_COUNT);
        DOCTEST_CHECK(side.current_unit_damage == 0);
        for (int16_t count : side.unit_counts) {
            DOCTEST_CHECK(count == 0);
        }
    }
}

DOCTEST_TEST_CASE("troop search selects each available troop type") {
    for (int8_t type = 0; type < UNIT_COUNT; ++type) {
        s_invasion_side_info side = {};
        side.unit_counts[type] = 1;

        invasion::Find_Troops(&side);

        DOCTEST_CHECK(side.current_troop_type_idx == type);
        DOCTEST_CHECK(side.unit_counts[type] == 1);
    }
}

DOCTEST_TEST_CASE("undefended ground combat preserves all invading infantry") {
    test_ground_commando_runtime::Reset_();
    s_invasion_player_bonuses bonuses = {};
    s_invasion_side_info attacker = {};
    s_invasion_side_info defender = {};
    attacker.unit_counts[1] = 33;

    invasion::Compute_Ground_Combat_Info(&attacker, &bonuses, nullptr);
    invasion::Compute_Ground_Combat_Info(&defender, &bonuses, nullptr);
    DOCTEST_REQUIRE(defender.current_troop_type_idx == UNIT_COUNT);
    invasion::Resolve_Ground_Combat(&attacker, &defender);

    DOCTEST_CHECK(attacker.current_troop_type_idx == 1);
    DOCTEST_CHECK(attacker.unit_counts[1] == 33);
    DOCTEST_CHECK(defender.current_troop_type_idx == UNIT_COUNT);
    DOCTEST_CHECK(test_ground_commando_runtime::Random_Call_Count_() == 0);
    for (int16_t count : defender.unit_counts) {
        DOCTEST_CHECK(count == 0);
    }
}

DOCTEST_TEST_CASE("ground combat stops after the last defending unit is killed") {
    test_ground_commando_runtime::Reset_();
    s_invasion_player_bonuses bonuses = {};
    s_invasion_side_info attacker = {};
    s_invasion_side_info defender = {};
    attacker.unit_counts[1] = 33;
    defender.unit_counts[1] = 1;

    invasion::Compute_Ground_Combat_Info(&attacker, &bonuses, nullptr);
    invasion::Compute_Ground_Combat_Info(&defender, &bonuses, nullptr);
    attacker.unit_strengths[1] = 100;
    invasion::Resolve_Ground_Combat(&attacker, &defender);

    DOCTEST_CHECK(attacker.unit_counts[1] == 33);
    DOCTEST_CHECK(defender.current_troop_type_idx == UNIT_COUNT);
    DOCTEST_CHECK(test_ground_commando_runtime::Random_Call_Count_() == 2);
    for (int16_t count : defender.unit_counts) {
        DOCTEST_CHECK(count == 0);
    }
}

DOCTEST_TEST_CASE("ground combat uses the configured troop type modifiers") {
    s_invasion_player_bonuses bonuses = {};
    s_invasion_side_info side = {};
    const invasion::s_troop_rules saved_rules = invasion::troop;
    invasion::troop.type = {14, -8, -16};

    invasion::Compute_Ground_Combat_Info(&side, &bonuses, nullptr);

    DOCTEST_CHECK(side.unit_strengths[0] == 14);
    DOCTEST_CHECK(side.unit_strengths[1] == 0);
    DOCTEST_CHECK(side.unit_strengths[2] == -8);
    DOCTEST_CHECK(side.unit_strengths[3] == -16);
    invasion::troop = saved_rules;
}

DOCTEST_TEST_CASE("ground combat uses configured troop bonuses") {
    s_invasion_player_bonuses bonuses = {};
    s_invasion_side_info side = {};
    const invasion::s_troop_rules saved_rules = invasion::troop;
    invasion::troop.bonus.low_g = -6;
    invasion::troop.bonus.high_g_hits = 3;
    invasion::troop.bonus.battleoids = 12;
    invasion::troop.bonus.battleoids_hits = 2;
    bonuses.low_g_penalty = invasion::troop.bonus.low_g;
    bonuses.heavy_g_bonus = (int8_t)invasion::troop.bonus.high_g_hits;
    bonuses.battleoids_bonus = invasion::troop.bonus.battleoids;
    bonuses.has_battleoids = 1;

    invasion::Compute_Ground_Combat_Info(&side, &bonuses, nullptr);

    DOCTEST_CHECK(side.unit_strengths[0] == 16);
    DOCTEST_CHECK(side.unit_strengths[1] == -6);
    DOCTEST_CHECK(side.unit_hps[0] == 7);
    DOCTEST_CHECK(side.unit_hps[1] == 4);
    invasion::troop = saved_rules;
}

DOCTEST_TEST_CASE("ground combat preserves unsigned rifle bonuses") {
    const uint8_t saved_bonus = TECHDATA::rifles[RIFLE_PLASMA_RIFLE].bonus;
    const uint8_t rifle_bonuses[] = {0, 127, 128, 255};

    for (uint8_t rifle_bonus : rifle_bonuses) {
        s_invasion_player_bonuses bonuses = {};
        s_invasion_side_info side = {};
        TECHDATA::rifles[RIFLE_PLASMA_RIFLE].bonus = rifle_bonus;
        bonuses.rifle_bonus = TECHDATA::rifles[RIFLE_PLASMA_RIFLE].bonus;

        invasion::Compute_Ground_Combat_Info(&side, &bonuses, nullptr);

        DOCTEST_CHECK(side.unit_strengths[1] == rifle_bonus);
    }

    TECHDATA::rifles[RIFLE_PLASMA_RIFLE].bonus = saved_bonus;
}

DOCTEST_TEST_CASE("ground-unit limits use configured planet-size capacity") {
    test_ground_commando_runtime::Reset_();
    s_colony* colony = test_ground_commando_runtime::Colony_();
    colony->owner = 0;
    MOX::planet[colony->planet].size = PLANET_SIZE_HUGE;
    const uint8_t saved_capacity =
        COLCALC::population_capacity.size[PLANET_SIZE_HUGE];

    const uint8_t capacities[] = {0, 127, 128, 255};
    for (uint8_t capacity : capacities) {
        COLCALC::population_capacity.size[PLANET_SIZE_HUGE] = capacity;

        MOX::player[0].traits[TRAIT_WARLORD] = 0;
        DOCTEST_CHECK(invasion::Colony_Infantry_Limit(colony) ==
                      (capacity / 2 == 0 ? 1 : capacity / 2));
        DOCTEST_CHECK(invasion::Colony_Tank_Limit(colony) ==
                      (capacity / 4 == 0 ? 1 : capacity / 4));

        MOX::player[0].traits[TRAIT_WARLORD] = 1;
        DOCTEST_CHECK(invasion::Colony_Infantry_Limit(colony) ==
                      (capacity == 0 ? 1 : capacity));
        DOCTEST_CHECK(invasion::Colony_Tank_Limit(colony) ==
                      (capacity / 2 == 0 ? 1 : capacity / 2));
    }

    COLCALC::population_capacity.size[PLANET_SIZE_HUGE] = saved_capacity;
}

DOCTEST_TEST_CASE("rebel display uses colony modifiers only when requested") {
    test_ground_commando_runtime::Reset_();
    s_colony* colony = test_ground_commando_runtime::Colony_();
    colony->owner = 0;

    const int8_t saved_armor_bonus = TECHDATA::armor[0].troops_bonus;
    const uint8_t saved_rifle_bonus = TECHDATA::rifles[0].bonus;
    const invasion::s_troop_rules saved_rules = invasion::troop;
    TECHDATA::armor[0].troops_bonus = 15;
    TECHDATA::rifles[0].bonus = 5;
    invasion::troop.type.rebels = -20;

    s_invasion_side_info classic = {};
    invasion::Compute_Colony_Ground_Combat_Info(colony, &classic, false);
    DOCTEST_CHECK(classic.unit_strengths[3] == -22);

    s_invasion_side_info displayed = {};
    invasion::Compute_Colony_Ground_Combat_Info(colony, &displayed, true);
    DOCTEST_CHECK(displayed.unit_strengths[3] == -2);

    TECHDATA::armor[0].troops_bonus = saved_armor_bonus;
    TECHDATA::rifles[0].bonus = saved_rifle_bonus;
    invasion::troop = saved_rules;
}

DOCTEST_TEST_CASE("rebel display selects the dedicated race animation") {
    test_ground_commando_runtime::Reset_();
    constexpr int16_t race = 4;

    DOCTEST_CHECK(COLONY::Player_Troop_Anim(0, 3, race) != nullptr);
    DOCTEST_CHECK(test_ground_commando_runtime::Last_Buffer_Entry_() ==
        race * 13);

    DOCTEST_CHECK(COLONY::Player_Troop_Anim(0, 1, race) != nullptr);
    DOCTEST_CHECK(test_ground_commando_runtime::Last_Buffer_Entry_() ==
        race * 13 + 7);

    MOX::player[0].tech_applications[TECH_APP_POWERED_ARMOR] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_CHECK(COLONY::Player_Troop_Anim(0, 1, race) != nullptr);
    DOCTEST_CHECK(test_ground_commando_runtime::Last_Buffer_Entry_() ==
        race * 13 + 8);
}

DOCTEST_TEST_CASE("AI invasion technology weights preserve unsigned configuration values") {
    struct s_case {
        uint8_t configured_weight;
        int difficulty;
        int32_t expected_chance;
    };
    const s_case cases[] = {
        {127, 0, 127},
        {127, 1, 132},
        {127, 2, 137},
        {127, 3, 142},
        {127, 4, 147},
        {128, 0, 128},
        {128, 1, 133},
        {128, 2, 138},
        {128, 3, 143},
        {128, 4, 148},
        {255, 0, 255},
        {255, 1, 260},
        {255, 2, 265},
        {255, 3, 270},
        {255, 4, 275},
    };

    for (const s_case& test_case : cases) {
        for (bool population_path : {false, true}) {
            test_ground_commando_runtime::Reset_();
            s_colony* colony = test_ground_commando_runtime::Colony_();
            colony->n_pops = population_path ? 1 : 0;
            MOX::player[1].objectives = PLAYER_OBJECTIVE_TECHNOLOGIST;
            MOX::settings.game_difficulty = test_case.difficulty;
            invasion::settings.technology_chance.building_ai =
                population_path ? 0 : test_case.configured_weight;
            invasion::settings.technology_chance.population_ai =
                population_path ? test_case.configured_weight : 0;

            constexpr int16_t tech_app = TECH_APP_RESEARCH_LABORATORY;
            if (population_path) {
                MOX::player[0].tech_applications[tech_app] =
                    TECH_RESEARCH_STATUS_RESEARCHED;
            } else {
                constexpr int16_t building = BUILDING_RESEARCH_LAB;
                colony->buildings[building] = 1;
                TECHDATA::buildings[building].tech_app = tech_app;
            }

            test_ground_commando_runtime::Set_Random_Result_(
                static_cast<uint32_t>(test_case.expected_chance));
            s_battle_results_info results = {};

            invasion::Change_Colony_Ownership(colony, 1, &results);

            DOCTEST_CHECK(test_ground_commando_runtime::Random_Call_Count_() ==
                1);
            DOCTEST_CHECK(test_ground_commando_runtime::Granted_Tech_Count_() ==
                1);
            DOCTEST_CHECK(results.captured_tech_id == 1);
            DOCTEST_CHECK(results.captured_techs_data[0] == tech_app);
        }
    }
}

DOCTEST_TEST_CASE("leader Warlord bonus applies at every XP tier when enabled") {
    struct s_level_case {
        int16_t xp;
        int16_t base_level;
    };
    const s_level_case cases[] = {
        {0, 0},
        {60, 1},
        {150, 2},
        {300, 3},
        {500, 4},
        {1000, 4},
    };

    OFFICER::leader_warlord_bonus = 0;
    for (const s_level_case& test_case : cases) {
        DOCTEST_CHECK(OFFICER::Get_Officer_Base_Level(test_case.xp, 0) ==
                      test_case.base_level);
        DOCTEST_CHECK(OFFICER::Get_Officer_Base_Level(test_case.xp, 1) ==
                      test_case.base_level + (test_case.xp >= 1000 ? 1 : 0));
    }

    OFFICER::leader_warlord_bonus = 1;
    for (const s_level_case& test_case : cases) {
        DOCTEST_CHECK(OFFICER::Get_Officer_Base_Level(test_case.xp, 1) ==
                      test_case.base_level + 1);
    }
}

DOCTEST_TEST_CASE("population enforcement accepts the physical colony capacity") {
    test_ground_commando_runtime::Reset_();
    s_colony* colony = test_ground_commando_runtime::Colony_();
    colony->owner = 0;
    colony->n_pops = MAX_COLONY_POPULATION;
    for (int i = 0; i < MAX_COLONY_POPULATION; ++i) {
        colony->pop[i] = 0;
    }

    invasion::Enforce_Population_Limits_At_Colony(colony);

    DOCTEST_CHECK(colony->n_pops == MAX_COLONY_POPULATION);
}

DOCTEST_TEST_CASE("leader levels use the highest qualifying configured threshold") {
    const auto saved_progression = OFFICER::progression;
    const uint8_t saved_warlord_bonus = OFFICER::leader_warlord_bonus;
    const uint16_t thresholds[5] = {60, 500, 150, 1000, 300};
    for (int index = 0; index < 5; ++index) {
        OFFICER::progression.experience_thresholds[index] = thresholds[index];
    }

    OFFICER::leader_warlord_bonus = 0;
    DOCTEST_CHECK(OFFICER::Get_Officer_Base_Level(200, 0) == 3);
    DOCTEST_CHECK(OFFICER::Get_Officer_Base_Level(350, 0) == 4);
    DOCTEST_CHECK(OFFICER::Get_Officer_Base_Level(350, 1) == 5);

    OFFICER::progression = saved_progression;
    OFFICER::leader_warlord_bonus = saved_warlord_bonus;
}

DOCTEST_TEST_CASE("leader XP advancement ignores the effective Warlord bonus") {
    struct s_advance_case {
        int16_t initial_xp;
        int16_t expected_xp;
    };
    const s_advance_case cases[] = {
        {0, 60},
        {60, 150},
        {150, 300},
        {300, 500},
        {500, 1000},
        {1000, 1000},
    };

    test_ground_commando_runtime::Reset_();
    OFFICER::leader_warlord_bonus = 1;
    for (const s_advance_case& test_case : cases) {
        s_leader_data* leader = test_ground_commando_runtime::Leader_(0);
        leader->xp = test_case.initial_xp;
        OFFICER::Increase_Officer_Level_By_One(0);
        DOCTEST_CHECK(leader->xp == test_case.expected_xp);
    }
}

DOCTEST_TEST_CASE("leader XP advancement does not narrow unreachable thresholds") {
    const auto saved_progression = OFFICER::progression;
    test_ground_commando_runtime::Reset_();
    s_leader_data* leader = test_ground_commando_runtime::Leader_(0);
    leader->xp = 500;
    OFFICER::progression.experience_thresholds[4] = 65535;

    OFFICER::Increase_Officer_Level_By_One(0);

    DOCTEST_CHECK(leader->xp == 500);
    OFFICER::progression = saved_progression;
}

DOCTEST_TEST_CASE("unlimited leaders lifts the classic total recruitment cap") {
    test_ground_commando_runtime::Reset_();
    for (int i = 0; i < 8; ++i) {
        test_ground_commando_runtime::Set_Leader_(
            i, 0, static_cast<int8_t>(i % 2), 0);
    }

    OFFICER::leaders_no_limit = 0;
    DOCTEST_CHECK_FALSE(OFFICER::Player_Can_Hire_More_Officers(0));

    OFFICER::leaders_no_limit = 1;
    DOCTEST_CHECK(OFFICER::Player_Can_Hire_More_Officers(0));

    for (int i = 8; i < MAX_LEADERS; ++i) {
        test_ground_commando_runtime::Set_Leader_(
            i, 0, static_cast<int8_t>(i % 2), 0);
    }
    DOCTEST_CHECK_FALSE(OFFICER::Player_Can_Hire_More_Officers(0));
}

DOCTEST_TEST_CASE("leader paging uses descending leader order on the first page") {
    test_ground_commando_runtime::Reset_();
    for (int i = 0; i < 6; ++i) {
        test_ground_commando_runtime::Set_Leader_(
            i, 0, LEADER_TYPE_SHIP, 0);
    }

    OFFICER::leaders_no_limit = 1;
    OFFICER::Build_Captain_Id_List();

    DOCTEST_CHECK(MOX::n_fltscrn_officers == 4);
    const int16_t* ids = test_ground_commando_runtime::Officer_Ids_();
    for (int i = 0; i < 4; ++i) {
        DOCTEST_CHECK(ids[i] == 5 - i);
    }
}