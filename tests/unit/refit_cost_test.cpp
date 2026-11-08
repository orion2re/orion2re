#include "pch.h"

#include "doctest/doctest.h"

namespace TEST_AIBUILD_CONTEXT {
    AIDATA::s_state* State_();
    const game_world::s_view* World_();
    void Reset_();
}

namespace {
    struct s_refit_runtime {
        s_player* saved_players = MOX::player;
        s_colony* saved_colonies = MOX::colony;
        s_planet_data* saved_planets = MOX::planet;
        s_current_design* saved_design = MOX::design;
        int16_t saved_num_players = MOX::NUM_PLAYERS;
        int32_t saved_stardate = MOX::stardate;
        decltype(AIDATA::ai_colony_info) saved_ai_colonies = AIDATA::ai_colony_info;
        decltype(AIDATA::ai_star_info) saved_ai_stars = AIDATA::ai_star_info;
        decltype(AIDATA::enroute_human_ships) saved_enroute = AIDATA::enroute_human_ships;

        s_refit_runtime() { TEST_AIBUILD_CONTEXT::Reset_(); }

        ~s_refit_runtime() {
            MOX::player = saved_players;
            MOX::colony = saved_colonies;
            MOX::planet = saved_planets;
            MOX::design = saved_design;
            MOX::NUM_PLAYERS = saved_num_players;
            MOX::stardate = saved_stardate;
            AIDATA::ai_colony_info = saved_ai_colonies;
            AIDATA::ai_star_info = saved_ai_stars;
            AIDATA::enroute_human_ships = saved_enroute;
        }
    };
}

DOCTEST_TEST_CASE("production facts use the supplied colony identity, not its address") {
    TEST_AIBUILD_CONTEXT::Reset_();
    auto* ai = TEST_AIBUILD_CONTEXT::State_();
    constexpr uint32_t id = 0x12345678u;
    ai->colonies[id] = ai->colonies.at(0);
    ai->colonies[id].population_limit = 21;
    s_colony detached = MOX::colony[0];
    production_rules::s_context context;
    AIBUILD::Build_Production_Context(ai, TEST_AIBUILD_CONTEXT::World_(), id, &detached, 0, &context);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_POPULATION_LIMIT] == 21);
    DOCTEST_CHECK(ai->colonies.size() == 2);
}

DOCTEST_TEST_CASE_FIXTURE(s_refit_runtime, "Food Replicator defaults do not gate eligibility on local food") {
    TEST_AIBUILD_CONTEXT::Reset_();
    production_rules::s_context context;
    production_rules::s_candidate* candidate =
        &AIBUILD::rules.autobuild.candidates[BUILDING_FOOD_REPLICATORS];

    MOX::colony[0].production[ECON_FOOD] = 7;
    MOX::colony[0].n_food_replicated = 3;
    AIBUILD::Build_Production_Context(TEST_AIBUILD_CONTEXT::State_(), TEST_AIBUILD_CONTEXT::World_(), 0, &MOX::colony[0], 0, &context);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_COLONY_FOOD] == 4);
    DOCTEST_CHECK(production_rules::Evaluate_Eligibility(
        &context, &candidate->eligibility));

    MOX::colony[0].production[ECON_FOOD] = 6;
    AIBUILD::Build_Production_Context(TEST_AIBUILD_CONTEXT::State_(), TEST_AIBUILD_CONTEXT::World_(), 0, &MOX::colony[0], 0, &context);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_COLONY_FOOD] == 3);
    DOCTEST_CHECK(production_rules::Evaluate_Eligibility(
        &context, &candidate->eligibility));
}

DOCTEST_TEST_CASE("named AI production needs distinguish totals from additional demand") {
    TEST_AIBUILD_CONTEXT::Reset_();
    auto* ai = TEST_AIBUILD_CONTEXT::State_();
    auto& needs = ai->players[0].build;
    needs.combat_strength.total = 40;
    needs.combat_strength.excluding_queued_builds = 30;
    needs.combat_strength.additional_wanted = 15;
    needs.spies.current = 8;
    needs.spies.additional_wanted = 3;
    needs.transports.current = 5;
    needs.transports.additional_wanted = 2;
    needs.colony_ships.current = 4;
    needs.colony_ships.enroute = 1;

    production_rules::s_context context{};
    AIBUILD::Build_Production_Context(ai, TEST_AIBUILD_CONTEXT::World_(), 0, &MOX::colony[0], 0, &context);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_CURRENT_STRENGTH] == 40);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_COMBAT_NEED] == 15);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_DESIRED_STRENGTH] == 55);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_SPY_NEED] == 3);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_TRANSPORT_NEED] == 2);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_EXISTING_COLONY_SHIPS] == 4);
}

DOCTEST_TEST_CASE("public presence metrics distinguish hostile neutral and treaty-partner players") {
    TEST_AIBUILD_CONTEXT::Reset_();
    auto* ai = TEST_AIBUILD_CONTEXT::State_();
    auto& neighborhood = ai->stars[0].neighborhood.per_player[0];
    neighborhood.treaty_partners_in_range = 2;
    neighborhood.neutral_players_in_range = 1;
    neighborhood.hostile_players_in_range = 3;
    neighborhood.players_only_in_extended_range = 4;
    production_rules::s_context context;
    AIBUILD::Build_Production_Context(ai, TEST_AIBUILD_CONTEXT::World_(), 0, &MOX::colony[0], 0, &context);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_ENEMY_PRESENCE] == 3);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_NEUTRAL_PRESENCE] == 1);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_ALLIED_PRESENCE] == 2);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_OUT_OF_RANGE_PRESENCE] == 4);
    DOCTEST_CHECK(context.metrics[production_rules::METRIC_LOCAL_THREAT] == 31);

    production_rules::s_eligibility eligibility{};
    eligibility.all.thresholds[production_rules::METRIC_ENEMY_PRESENCE] = {production_rules::COMPARISON_GT, 2};
    eligibility.all.thresholds[production_rules::METRIC_ALLIED_PRESENCE] = {production_rules::COMPARISON_EQ, 2};
    DOCTEST_CHECK(production_rules::Evaluate_Eligibility(&context, &eligibility));
}

DOCTEST_TEST_CASE("colony ship and freighter needs use current and enroute ship counts") {
    TEST_AIBUILD_CONTEXT::Reset_();
    AIDATA::s_build_needs needs{};
    AIBUILD::Compute_Colony_Ship_Need(&needs);
    DOCTEST_CHECK(needs.colony_ships.additional_wanted == 1);
    needs.colony_ships.enroute = 2;
    MOX::player[0].surplus_freighters = -1;
    AIBUILD::Compute_Freighter_Need(&needs, MOX::player[0].surplus_freighters);
    DOCTEST_CHECK(needs.freighter_groups_wanted == 1);
    DOCTEST_CHECK(needs.colony_ships.current == 0);
    DOCTEST_CHECK(needs.colony_ships.enroute == 2);
}

DOCTEST_TEST_CASE_FIXTURE(s_refit_runtime, "Battle Pods combat speed uses the configured overload rules") {
    s_tech_drive_data saved_drive = TECHDATA::drives[DRIVE_NUCLEAR_DRIVE];
    s_tech_specials_data saved_battle_pods = TECHDATA::specials[SPECIAL_BATTLE_PODS];
    bool saved_speed_bonus = DESIGN::battle_pods_speed_bonus;
    uint8_t saved_minimum_speed = DESIGN::minimum_combat_speed;

    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE].combat_speed_min[SHIP_SIZE_SMALL] = 10;
    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE].combat_speed_max[SHIP_SIZE_SMALL] = 20;
    TECHDATA::specials[SPECIAL_BATTLE_PODS].space_per_ship_type[SHIP_SIZE_SMALL] =
        static_cast<uint16_t>(-20);

    s_current_design design = {};
    design.ftl_type = DRIVE_NUCLEAR_DRIVE;
    design.ship_size = SHIP_SIZE_SMALL;
    design.hull_space = 100;
    design.space_used = 40;
    design.special_devices[0] = SPECIAL_BATTLE_PODS;

    DESIGN::battle_pods_speed_bonus = true;
    DOCTEST_CHECK(DESIGN::Current_Design_Base_Combat_Speed(&design) == 16);

    DESIGN::battle_pods_speed_bonus = false;
    DESIGN::minimum_combat_speed = 255;
    DOCTEST_CHECK(DESIGN::Current_Design_Base_Combat_Speed(&design) == 14);

    design.space_used = 150;
    DOCTEST_CHECK(DESIGN::Current_Design_Base_Combat_Speed(&design) == 10);

    DESIGN::minimum_combat_speed = 5;
    DOCTEST_CHECK(DESIGN::Current_Design_Base_Combat_Speed(&design) == 5);

    TECHDATA::specials[SPECIAL_BATTLE_PODS].space_per_ship_type[SHIP_SIZE_SMALL] = 0;
    DESIGN::minimum_combat_speed = 0;
    design.space_used = 25951464;
    DOCTEST_CHECK(DESIGN::Current_Design_Base_Combat_Speed(&design) == 0);

    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE] = saved_drive;
    TECHDATA::specials[SPECIAL_BATTLE_PODS] = saved_battle_pods;
    DESIGN::battle_pods_speed_bonus = saved_speed_bonus;
    DESIGN::minimum_combat_speed = saved_minimum_speed;
}

DOCTEST_TEST_CASE_FIXTURE(s_refit_runtime, "design templates store base speed before Augmented Engines") {
    s_tech_drive_data saved_drive = TECHDATA::drives[DRIVE_NUCLEAR_DRIVE];
    uint8_t saved_use_augmented_engines = DESIGN::use_augmented_engines;
    uint8_t saved_augmented_bonus = DESIGN::augmented_engines_speed_bonus;
    s_current_design* saved_design = MOX::design;

    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE]
        .combat_speed_min[SHIP_SIZE_SMALL] = 10;
    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE]
        .combat_speed_max[SHIP_SIZE_SMALL] = 10;
    DESIGN::use_augmented_engines = 1;
    DESIGN::augmented_engines_speed_bonus = 5;

    s_current_design current = {};
    current.ftl_type = DRIVE_NUCLEAR_DRIVE;
    current.ship_size = SHIP_SIZE_SMALL;
    current.hull_space = 100;
    current.special_devices[0] = SPECIAL_AUGMENTED_ENGINES;
    MOX::design = &current;

    s_ship_design stored = {};
    DESIGN::Build_Design_Template(&stored);
    DOCTEST_CHECK(stored.combat_speed == 10);
    DOCTEST_CHECK(DESIGN::Current_Design_Combat_Speed(&current) == 15);

    MOX::design = saved_design;
    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE] = saved_drive;
    DESIGN::use_augmented_engines = saved_use_augmented_engines;
    DESIGN::augmented_engines_speed_bonus = saved_augmented_bonus;
}

DOCTEST_TEST_CASE_FIXTURE(s_refit_runtime, "refit cost uses the corrected shared formulas") {
    int16_t saved_hull_cost = TECHDATA::hull_data[HULL_FRIGATE].cost;
    int8_t saved_player_0_government =
        MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT];
    int8_t saved_player_1_government =
        MOX::player[1].traits[TRAIT_CURRENT_GOVERNMENT];
    TECHDATA::hull_data[HULL_FRIGATE].cost = 100;
    MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_DICTATORSHIP;
    MOX::player[1].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_FEUDAL;

    s_ship_design old_design = {};
    old_design.size = HULL_FRIGATE;
    old_design.cost = 100;

    s_ship_design small_upgrade = old_design;
    small_upgrade.cost = 105;
    s_ship_design large_upgrade = old_design;
    large_upgrade.cost = 130;
    s_ship_design downgrade = old_design;
    downgrade.cost = 80;

    DESIGN::alternative_refit = 0;
    DOCTEST_CHECK(AIBUILD::Refit_Cost(&MOX::player[0], &old_design, &small_upgrade) == 25);
    DOCTEST_CHECK(AIBUILD::Refit_Cost(&MOX::player[1], &old_design, &small_upgrade) == 17);
    DOCTEST_CHECK(AIBUILD::Refit_Cost(&MOX::player[0], &old_design, &large_upgrade) == 60);
    DOCTEST_CHECK(AIBUILD::Refit_Cost(&MOX::player[0], &old_design, &downgrade) == 25);

    DESIGN::alternative_refit = 1;
    DOCTEST_CHECK(AIBUILD::Refit_Cost(&MOX::player[0], &old_design, &small_upgrade) == 30);
    DOCTEST_CHECK(AIBUILD::Refit_Cost(&MOX::player[1], &old_design, &small_upgrade) == 22);
    DOCTEST_CHECK(AIBUILD::Refit_Cost(&MOX::player[0], &old_design, &large_upgrade) == 55);
    DOCTEST_CHECK(AIBUILD::Refit_Cost(&MOX::player[0], &old_design, &downgrade) == 25);

    DESIGN::alternative_refit = 0;
    TECHDATA::hull_data[HULL_FRIGATE].cost = saved_hull_cost;
    MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = saved_player_0_government;
    MOX::player[1].traits[TRAIT_CURRENT_GOVERNMENT] = saved_player_1_government;
}

DOCTEST_TEST_CASE_FIXTURE(s_refit_runtime, "government cost reductions apply to normally neutral governments") {
    int8_t saved_government =
        MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT];
    s_ratio16 saved_reduction =
        COLCALC::government_bonuses[GOVERNMENT_DICTATORSHIP].cost_reduction;
    int32_t cost = 101;

    MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_DICTATORSHIP;
    COLCALC::government_bonuses[GOVERNMENT_DICTATORSHIP].cost_reduction = { 1, 4 };
    DESIGN::Cost_Reduction_For_Govt_Type(0, &cost);
    DOCTEST_CHECK(cost == 76);

    cost = 101;
    COLCALC::government_bonuses[GOVERNMENT_DICTATORSHIP].cost_reduction = { 0, 1 };
    DESIGN::Cost_Reduction_For_Govt_Type(0, &cost);
    DOCTEST_CHECK(cost == 101);

    MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = saved_government;
    COLCALC::government_bonuses[GOVERNMENT_DICTATORSHIP].cost_reduction =
        saved_reduction;
}

DOCTEST_TEST_CASE_FIXTURE(s_refit_runtime, "refit minimum reduction is limited to Feudal governments") {
    int16_t saved_hull_cost = TECHDATA::hull_data[HULL_FRIGATE].cost;
    int8_t saved_government =
        MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT];
    s_ratio16 saved_reduction =
        COLCALC::government_bonuses[GOVERNMENT_DEMOCRACY].cost_reduction;

    TECHDATA::hull_data[HULL_FRIGATE].cost = 100;
    MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_DEMOCRACY;
    COLCALC::government_bonuses[GOVERNMENT_DEMOCRACY].cost_reduction = { 1, 2 };
    DESIGN::alternative_refit = 1;

    s_ship_design old_design = {};
    old_design.size = HULL_FRIGATE;
    old_design.cost = 100;
    s_ship_design new_design = old_design;
    new_design.cost = 105;

    DOCTEST_CHECK(AIBUILD::Refit_Cost(&MOX::player[0], &old_design, &new_design) == 30);

    DESIGN::alternative_refit = 0;
    TECHDATA::hull_data[HULL_FRIGATE].cost = saved_hull_cost;
    MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = saved_government;
    COLCALC::government_bonuses[GOVERNMENT_DEMOCRACY].cost_reduction =
        saved_reduction;
}