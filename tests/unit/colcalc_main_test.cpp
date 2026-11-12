#include "pch.h"
#include "../fixtures/game_state.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "game/allocate.h"
#include "game/vfs.h"

namespace {
    struct s_production_state : test::s_game_state {
        test::s_saved_array<s_tech_building_data, BUILDING_COUNT> buildings{TECHDATA::buildings};
        test::s_saved_array<s_tech_hull_data, HULL_CLASS_COUNT> hulls{TECHDATA::hull_data};
        test::s_saved_array<s_tech_app_data, TECH_APP_COUNT> applications{TECHDATA::technology_applications};
        test::s_saved_array<s_tech_field_data, TECH_FIELD_COUNT> fields{TECHDATA::technology_fields};
        test::s_saved_array<uint8_t, PLANET_CLIMATE_COUNT> food{MOX::food_per_farmer_table};
        test::s_saved_array<s_event_data, EVENT_COUNT> events{EVENTS::event_data};
        test::s_saved_array<uint8_t, PLANET_CLIMATE_COUNT> climate_maintenance{COLCALC::climate_maintenance_modifiers};
        test::s_saved_array<uint8_t, SHIP_SIZE_COUNT> scout_lab{COLCALC::scout_lab_research_bonus};
        test::s_saved_array<uint8_t, SHIP_SIZE_COUNT> command_usage{COLCALC::ship_command_points_usage};
        decltype(COLCALC::replicator_food_pp) food_pp = COLCALC::replicator_food_pp;
        decltype(COLCALC::replicator_food_bc) food_bc = COLCALC::replicator_food_bc;
        decltype(COLCALC::trade_goods_ratio) trade_ratio = COLCALC::trade_goods_ratio;
        decltype(COLCALC::planet_bc_bonus) planet_bc = COLCALC::planet_bc_bonus;
        decltype(COLCALC::android_morale) morale = COLCALC::android_morale;
        decltype(COLCALC::research_colony_bonus) colony_research = COLCALC::research_colony_bonus;
        decltype(COLCALC::research_scientist_bonus) scientist_research = COLCALC::research_scientist_bonus;
        decltype(COLCALC::command_points) command = COLCALC::command_points;
        int16_t mineral = COLCALC::minerals_per_mine[0];

        s_production_state() {
            memset(TECHDATA::buildings, 0, sizeof(TECHDATA::buildings));
            memset(TECHDATA::hull_data, 0, sizeof(TECHDATA::hull_data));
            memset(TECHDATA::technology_applications, 0, sizeof(TECHDATA::technology_applications));
            memset(TECHDATA::technology_fields, 0, sizeof(TECHDATA::technology_fields));
            memset(MOX::food_per_farmer_table, 0, sizeof(MOX::food_per_farmer_table));
            memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
        }

        ~s_production_state() {
            COLCALC::replicator_food_pp = food_pp;
            COLCALC::replicator_food_bc = food_bc;
            COLCALC::trade_goods_ratio = trade_ratio;
            COLCALC::planet_bc_bonus = planet_bc;
            COLCALC::android_morale = morale;
            COLCALC::research_colony_bonus = colony_research;
            COLCALC::research_scientist_bonus = scientist_research;
            COLCALC::command_points = command;
            COLCALC::minerals_per_mine[0] = mineral;
        }
    };
}

#include "doctest/doctest.h"
#include "game/strbuild.h"

namespace TEST_COLONY_REPLICATORS {
    void Reset_();
}

namespace TEST_PLAYER_STATS {
    void Reset_();
}

namespace TEST_GOVERNMENT_BONUS {
    void Reset_();
    void Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "government economy bonuses apply generically to every government") {
    COLCALC::s_government_bonus saved_bonuses[GOVERNMENT_COUNT];
    memcpy(saved_bonuses, COLCALC::government_bonuses, sizeof(saved_bonuses));

    for (int government = 0; government < GOVERNMENT_COUNT; ++government) {
        TEST_GOVERNMENT_BONUS::Reset_();
        s_colony* colony = &MOX::colony[0];
        colony->n_pops = 1;
        colony->pop[0] = 0x200;

        MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = (int8_t)government;
        COLCALC::government_bonuses[government] = {
            { 0, 1 }, 5, 20, 15, 10,
        };

        s_colony_job_production breakdown = {};
        DOCTEST_CHECK(COLCALC::Colony_Job_Production(
            colony, ECON_FOOD, &breakdown) == 2);
        DOCTEST_CHECK((breakdown.govt_bonus == 30));

        colony->pop[0] = 0x280;
        DOCTEST_CHECK(COLCALC::Colony_Job_Production(
            colony, ECON_INDUSTRY, &breakdown) == 2);
        DOCTEST_CHECK((breakdown.govt_bonus == 10));

        colony->pop[0] = 0x300;
        DOCTEST_CHECK(COLCALC::Colony_Job_Production(
            colony, ECON_RESEARCH, &breakdown) == 4);
        DOCTEST_CHECK((breakdown.govt_bonus == 15));

        colony->pop[0] = 0;
        COLCALC::Colony_BC_Production(colony, nullptr);
        DOCTEST_CHECK((colony->production[ECON_BC] == 2));
    }

    memcpy(COLCALC::government_bonuses, saved_bonuses, sizeof(saved_bonuses));
    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "Android morale policy applies to the selected jobs") {
    struct s_policy {
        uint8_t value;
        bool applies[3];
    };
    const s_policy policies[] = {
        {COLCALC::ANDROID_MORALE_WORKERS_ONLY, {false, true, false}},
        {COLCALC::ANDROID_MORALE_FARMERS_ONLY, {true, false, false}},
        {COLCALC::ANDROID_MORALE_SCIENTISTS_ONLY, {false, false, true}},
        {COLCALC::ANDROID_MORALE_FARMERS_AND_WORKERS, {true, true, false}},
        {COLCALC::ANDROID_MORALE_WORKERS_AND_SCIENTISTS, {false, true, true}},
        {COLCALC::ANDROID_MORALE_FARMERS_AND_SCIENTISTS, {true, false, true}},
        {COLCALC::ANDROID_MORALE_ALL_DROIDS, {true, true, true}},
        {COLCALC::ANDROID_MORALE_NONE, {false, false, false}},
    };
    const int16_t jobs[] = {ECON_FOOD, ECON_INDUSTRY, ECON_RESEARCH};

    for (const s_policy& policy : policies) {
        for (int index = 0; index < 3; ++index) {
            TEST_GOVERNMENT_BONUS::Reset_();
            s_colony* colony = &MOX::colony[0];
            colony->n_pops = 1;
            colony->morale = 10;
            colony->pop[0] = 0x200u | (static_cast<uint32_t>(jobs[index]) << 7) | 8u;
            MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_FEUDAL;
            COLCALC::android_morale = COLCALC::ANDROID_MORALE_ALL_DROIDS;

            s_colony_job_production all_droids = {};
            COLCALC::Colony_Job_Production(colony, jobs[index], &all_droids);
            DOCTEST_REQUIRE((all_droids.morale_bonus != 0));

            COLCALC::android_morale = policy.value;
            s_colony_job_production configured = {};
            COLCALC::Colony_Job_Production(colony, jobs[index], &configured);
            DOCTEST_CHECK((configured.morale_bonus ==
                (policy.applies[index] ? all_droids.morale_bonus : 0)));

            MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_UNIFICATION;
            s_colony_job_production unification = {};
            COLCALC::Colony_Job_Production(colony, jobs[index], &unification);
            DOCTEST_CHECK((unification.morale_bonus == 0));
        }
    }

    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured planet special BC bonuses feed colony income") {
    TEST_GOVERNMENT_BONUS::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->n_pops = 5;
    COLCALC::planet_bc_bonus = {7, 13};

    MOX::planet[0].planet_special = SYSTEM_SPECIAL_GOLD_DEPOSITS;
    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 12));

    colony->buildings[BUILDING_SPACEPORT] = 1;
    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 18));

    colony->buildings[BUILDING_SPACEPORT] = 0;
    MOX::planet[0].planet_special = SYSTEM_SPECIAL_GEM_DEPOSITS;
    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 18));

    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured Trade Goods ratios apply by Fantastic Traders trait") {
    TEST_GOVERNMENT_BONUS::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->n_pops = 1;
    colony->producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
    colony->production[ECON_INDUSTRY] = 9;

    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 6));

    MOX::player[0].traits[TRAIT_FANTASTIC_TRADERS] = 1;
    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 10));

    COLCALC::trade_goods_ratio = {40, 175};
    MOX::player[0].traits[TRAIT_FANTASTIC_TRADERS] = 0;
    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 5));

    MOX::player[0].traits[TRAIT_FANTASTIC_TRADERS] = 1;
    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 17));

    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured colony BC bonuses apply independently") {
    TEST_GOVERNMENT_BONUS::Reset_();
    const COLCALC::s_bc_bonus saved_bonus = COLCALC::bc_bonus;

    s_colony* colony = &MOX::colony[0];
    colony->n_pops = 5;
    COLCALC::bc_bonus = {75, 125, 50};

    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 5));

    colony->buildings[BUILDING_SPACEPORT] = 1;
    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 8));

    colony->buildings[BUILDING_SPACEPORT] = 0;
    colony->buildings[BUILDING_STOCK_EXCHANGE] = 1;
    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 11));

    colony->buildings[BUILDING_STOCK_EXCHANGE] = 0;
    MOX::player[0].tech_applications[TECH_APP_GALACTIC_CURRENCY_EXCHANGE] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    COLCALC::Colony_BC_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_BC] == 7));

    COLCALC::bc_bonus = saved_bonus;
    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured research bonuses drive production and predictions") {
    TEST_GOVERNMENT_BONUS::Reset_();

    COLCALC::research_colony_bonus = {41, 43, 47, 53};
    COLCALC::research_scientist_bonus = {
        7, 11, 13, 17, 19, 23, 29, 31, 37,
    };

    s_colony* colony = &MOX::colony[0];
    s_planet_data* planet = &MOX::planet[0];
    colony->n_pops = 1;
    colony->buildings[BUILDING_ASTRO_UNIVERSITY] = 1;
    colony->buildings[BUILDING_SUPERCOMPUTER] = 1;
    colony->buildings[BUILDING_GALACTIC_CYBERNET] = 1;
    colony->buildings[BUILDING_RESEARCH_LAB] = 1;
    planet->planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;

    s_colony_job_production breakdown = {};
    DOCTEST_CHECK(COLCALC::Colony_Research_Per_Scientist(
        colony, 0, &breakdown) == 106);
    DOCTEST_CHECK(breakdown.base_prod == 7);
    DOCTEST_CHECK((breakdown.artifacts_bonus == 11));
    DOCTEST_CHECK((breakdown.buildings_prod[BUILDING_ASTRO_UNIVERSITY] == 29));
    DOCTEST_CHECK((breakdown.buildings_prod[BUILDING_SUPERCOMPUTER] == 19));
    DOCTEST_CHECK(breakdown.buildings_prod[BUILDING_GALACTIC_CYBERNET] == 23);
    DOCTEST_CHECK(breakdown.buildings_prod[BUILDING_RESEARCH_LAB] == 17);

    planet->planet_special = SYSTEM_SPECIAL_ORION_STAR;
    DOCTEST_CHECK(COLCALC::Colony_Research_Per_Scientist(
        colony, 0, nullptr) == 108);

    memset(colony->buildings, 0, sizeof(colony->buildings));
    planet->planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    colony->research_per_scientist = 7;
    MOX::player[0].tech_applications[TECH_APP_HEIGHTENED_INTELLIGENCE] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Research_Produced(
        colony, 0, 0, nullptr) == 44);
    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Research_Produced(
        colony, 0, 8, nullptr) == 38);

    s_potential_colony_info info = {};
    planet->planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
    COLCALC::Compute_Potential_Colony_Info(&MOX::player[0], planet, &info);
    DOCTEST_CHECK(info.research == 55);
    planet->planet_special = SYSTEM_SPECIAL_ORION_STAR;
    COLCALC::Compute_Potential_Colony_Info(&MOX::player[0], planet, &info);
    DOCTEST_CHECK(info.research == 57);

    MOX::player[0].tech_applications[TECH_APP_HEIGHTENED_INTELLIGENCE] = 0;
    colony->pop[0] = 0;
    colony->buildings[BUILDING_RESEARCH_LAB] = 1;
    colony->buildings[BUILDING_SUPERCOMPUTER] = 1;
    colony->buildings[BUILDING_GALACTIC_CYBERNET] = 1;
    colony->buildings[BUILDING_AUTOLAB] = 1;
    COLCALC::Colony_Research_Production(colony, nullptr);
    DOCTEST_CHECK((colony->production[ECON_RESEARCH] == 184));

    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "hyper-advanced research costs use the configured increment") {
    const uint16_t saved_increment = COLCALC::hyper_advanced_cost_increment;
    const uint32_t saved_base_cost =
        TECHDATA::technology_fields[TECH_FIELD_BIOLOGY].cost;
    COLCALC::hyper_advanced_cost_increment = 12000;
    TECHDATA::technology_fields[TECH_FIELD_BIOLOGY].cost = 25000;

    s_player player = {};
    player.hyper_advanced_tech[0] = 2;
    DOCTEST_CHECK(COLCALC::Player_Research_Cost(
                      &player, TECH_FIELD_BIOLOGY) == 49000);

    COLCALC::Compute_Player_Total_Known_Tech_Cost(&player);
    DOCTEST_CHECK(player.total_known_tech_cost == 37000);

    COLCALC::hyper_advanced_cost_increment = saved_increment;
    TECHDATA::technology_fields[TECH_FIELD_BIOLOGY].cost = saved_base_cost;
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "research projection counts a first-turn breakthrough once") {
    const int field = TECH_FIELD_CHEMISTRY;
    const uint32_t saved_cost = TECHDATA::technology_fields[field].cost;
    TECHDATA::technology_fields[field].cost = 100;

    s_player player = {};
    player.current_research_field = (int8_t)field;
    player.tech_fields[field] = 2;
    player.research_accumulated = 100;
    player.research_produced = 100;

    s_player saved_player;
    memcpy(&saved_player, &player, sizeof(player));

    DOCTEST_CHECK(
        COLCALC::Player_N_Turns_Until_Research_Complete(&player) == 1);
    DOCTEST_CHECK(memcmp(&player, &saved_player, sizeof(player)) == 0);

    TECHDATA::technology_fields[field].cost = saved_cost;
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "breakthrough chance uses widened arithmetic") {
    DOCTEST_CHECK(COLCALC::Chance_For_Research_Breakthrough_Aux(
        INT32_MAX, 1, 1) == 100);
    DOCTEST_CHECK(COLCALC::Chance_For_Research_Breakthrough_Aux(
        (int64_t)INT32_MAX + 1, 1, INT32_MAX) == 1);
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured pollution reductions preserve order and upward rounding") {
    struct s_pollution_case {
        uint8_t exempt_production;
        uint8_t nano_coefficient;
        uint16_t processor_ratio;
        uint16_t renewer_ratio;
        bool has_nano_disassemblers;
        bool has_pollution_processor;
        bool has_atmospheric_renewer;
        int16_t expected_pollution;
    };

    const s_pollution_case cases[] = {
        {2, 2, 50, 25, false, false, false, 9},
        {6, 2, 50, 25, false, false, false, 7},
        {6, 3, 50, 25, true, false, false, 1},
        {2, 2, 40, 25, false, true, false, 3},
        {2, 2, 50, 30, false, false, true, 2},
        {2, 2, 40, 30, false, true, true, 1},
    };

    for (const s_pollution_case& test_case : cases) {
        TEST_GOVERNMENT_BONUS::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->n_pops = 1;
        colony->pop[0] = 0x280;
        colony->industry_per_worker = 20;
        colony->buildings[BUILDING_POLLUTION_PROCESSOR] =
            test_case.has_pollution_processor;
        colony->buildings[BUILDING_ATMOSPHERE_RENEWER] =
            test_case.has_atmospheric_renewer;

        if (test_case.has_nano_disassemblers) {
            MOX::player[0].tech_applications[TECH_APP_NANO_DISASSEMBLERS] =
                TECH_RESEARCH_STATUS_RESEARCHED;
        }

        COLCALC::planet_no_pollution_prod[0] = test_case.exempt_production;
        COLCALC::nano_disassemblers_coeff = test_case.nano_coefficient;
        COLCALC::pollution_processor_ratio = test_case.processor_ratio;
        COLCALC::atmospheric_renewer_ratio = test_case.renewer_ratio;

        COLCALC::Colony_Industry_Production(colony, nullptr);

        DOCTEST_CHECK(colony->pollution == test_case.expected_pollution);
        DOCTEST_CHECK((
            colony->production[ECON_INDUSTRY] ==
            20 - test_case.expected_pollution));
    }

    COLCALC::planet_no_pollution_prod[0] = 2;
    COLCALC::nano_disassemblers_coeff = 2;
    COLCALC::pollution_processor_ratio = 50;
    COLCALC::atmospheric_renewer_ratio = 25;
    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "high Pollution Processor ratios preserve wide arithmetic") {
    struct s_ratio_case {
        uint16_t ratio;
        int16_t expected_pollution;
        int16_t expected_industry;
    };
    const s_ratio_case cases[] = {
        {0, 0, 1},
        {32767, 164, -163},
        {32768, 164, -163},
        {65535, 328, -327},
    };

    for (const s_ratio_case& test_case : cases) {
        TEST_GOVERNMENT_BONUS::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->n_pops = 1;
        colony->pop[0] = 0x280;
        colony->industry_per_worker = 1;
        colony->buildings[BUILDING_POLLUTION_PROCESSOR] = 1;
        COLCALC::planet_no_pollution_prod[0] = 0;
        COLCALC::pollution_processor_ratio = test_case.ratio;

        COLCALC::Colony_Industry_Production(colony, nullptr);

        DOCTEST_CHECK(colony->pollution == test_case.expected_pollution);
        DOCTEST_CHECK((
            colony->production[ECON_INDUSTRY] == test_case.expected_industry));
    }

    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured worker production stays consistent across calculations") {
    TEST_GOVERNMENT_BONUS::Reset_();
    uint8_t saved_worker_base[8];
    memcpy(saved_worker_base, COLCALC::minerals_per_mine,
           sizeof(saved_worker_base));
    const COLCALC::s_production_worker_bonus saved_worker_bonus =
        COLCALC::production_worker_bonus;

    COLCALC::minerals_per_mine[MINERAL_RESOURCE_ULTRA_POOR] = 4;
    COLCALC::production_worker_bonus = {2, 3, 4, 5, 1, 7, 5};

    s_colony* colony = &MOX::colony[0];
    colony->buildings[BUILDING_ASTRO_UNIVERSITY] = 1;
    colony->buildings[BUILDING_AUTOMATED_FACTORY] = 1;
    colony->buildings[BUILDING_ROBO_MINER_PLANT] = 1;
    colony->buildings[BUILDING_DEEP_CORE_MINE] = 1;
    MOX::player[0].tech_applications[TECH_APP_MICROLITE_CONSTRUCTION] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    s_colony_job_production breakdown = {};
    DOCTEST_CHECK(COLCALC::Colony_Industry_Per_Worker(
        colony, 0, &breakdown) == 19);
    DOCTEST_CHECK(breakdown.base_prod == 4);
    DOCTEST_CHECK(
        breakdown.buildings_prod[BUILDING_ASTRO_UNIVERSITY] == 1);
    DOCTEST_CHECK(
        breakdown.buildings_prod[BUILDING_AUTOMATED_FACTORY] == 2);
    DOCTEST_CHECK(
        breakdown.buildings_prod[BUILDING_ROBO_MINER_PLANT] == 3);
    DOCTEST_CHECK(
        breakdown.buildings_prod[BUILDING_DEEP_CORE_MINE] == 4);
    DOCTEST_CHECK(
        breakdown.tech_apps[TECH_APP_MICROLITE_CONSTRUCTION] == 5);

    colony->industry_per_worker = 255;
    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Industry_Produced(
        colony, 0, 0, nullptr) == 255);

    colony->industry_per_worker = 6;
    DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Industry_Produced(
        colony, 0, 8, nullptr) == 13);

    s_potential_colony_info potential = {};
    COLCALC::Compute_Potential_Colony_Info(
        &MOX::player[0], &MOX::planet[0], &potential);
    DOCTEST_CHECK(potential.industry == 9);

    memcpy(COLCALC::minerals_per_mine, saved_worker_base,
           sizeof(saved_worker_base));
    COLCALC::production_worker_bonus = saved_worker_bonus;
    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "worker production cache preserves the configured low byte") {
    const uint8_t values[] = {0, 127, 128, 255};

    for (uint8_t value : values) {
        TEST_GOVERNMENT_BONUS::Reset_();
        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->planet = 0;
        MOX::planet[0].mineral_class = MINERAL_RESOURCE_ULTRA_POOR;
        COLCALC::minerals_per_mine[MINERAL_RESOURCE_ULTRA_POOR] = value;
        COLCALC::production_worker_bonus = {};

        COLCALC::Pre_Import_Computing(colony);

        DOCTEST_CHECK(colony->industry_per_worker == value);
        DOCTEST_CHECK(COLCALC::Colony_Empire_Base_Industry_Produced(
            colony, 0, 0, nullptr) == value);
    }

    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured fixed production and Recyclotron bonuses are applied") {
    TEST_GOVERNMENT_BONUS::Reset_();
    const COLCALC::s_production_worker_bonus saved_worker_bonus =
        COLCALC::production_worker_bonus;
    const COLCALC::s_production_colony_bonus saved_colony_bonus =
        COLCALC::production_colony_bonus;

    COLCALC::production_worker_bonus.recyclotron = 5;
    COLCALC::production_colony_bonus = {2, 3, 4, {7, 8, 9, 10, 11}};

    s_colony* colony = &MOX::colony[0];
    colony->n_pops = 3;
    colony->pop[0] = 0x200;
    colony->pop[1] = 0x200;
    colony->pop[2] = 0x200;
    MOX::planet[0].mineral_class = MINERAL_RESOURCE_RICH;
    colony->buildings[BUILDING_AUTOMATED_FACTORY] = 1;
    colony->buildings[BUILDING_ROBO_MINER_PLANT] = 1;
    colony->buildings[BUILDING_DEEP_CORE_MINE] = 1;
    colony->buildings[BUILDING_ROBOTIC_FACTORY] = 1;
    colony->buildings[BUILDING_RECYCLOTRON] = 1;
    colony->buildings[BUILDING_CORE_WASTE_DUMP] = 1;

    COLCALC::Colony_Industry_Production(colony, nullptr);

    DOCTEST_CHECK((colony->production[ECON_INDUSTRY] == 34));
    DOCTEST_CHECK(colony->n_industry_recyclers == 15);

    COLCALC::production_worker_bonus = saved_worker_bonus;
    COLCALC::production_colony_bonus = saved_colony_bonus;
    TEST_GOVERNMENT_BONUS::Cleanup_();
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "Scout Lab research uses the configured bonus for every ship size") {
    for (uint8_t size_class = 0; size_class < SHIP_SIZE_COUNT; ++size_class) {
        TEST_PLAYER_STATS::Reset_();
        uint8_t expected = (uint8_t)(40 + size_class);
        COLCALC::scout_lab_research_bonus[size_class] = expected;

        s_ship_data* ship = &MOX::ship[0];
        ship->owner = 0;
        ship->status = 0;
        ship->d.size = size_class;
        ship->d.special_device_flags[SPECIAL_SCOUT_LAB >> 3] |=
            (uint8_t)(1U << (SPECIAL_SCOUT_LAB & 7));

        COLCALC::Update_Player_Stats(&MOX::player[0]);

        DOCTEST_CHECK(MOX::player[0].research_produced == expected);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured planet climate modifiers change building maintenance") {
    TEST_COLONY_REPLICATORS::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->n_pops = 1;
    colony->buildings[BUILDING_AUTOMATED_FACTORY] = 1;

    struct s_case {
        uint8_t modifier;
        uint8_t expected_maintenance;
    };
    const s_case cases[] = {
        {0, 2}, {127, 5}, {128, 5}, {255, 7},
    };
    for (const s_case& test_case : cases) {
        COLCALC::climate_maintenance_modifiers[PLANET_CLIMATE_TOXIC] =
            test_case.modifier;
        colony->climate = PLANET_CLIMATE_TOXIC;
        COLCALC::Colony_BC_Maintenance(colony);
        DOCTEST_CHECK((uint8_t)colony->maintenance[ECON_BC] ==
                      test_case.expected_maintenance);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured command-point values and ship usage drive maintenance") {
    TEST_PLAYER_STATS::Reset_();
    MOX::NUM_COLONIES = 3;
    MOX::NUM_SHIPS = 2;

    MOX::player[0].traits[TRAIT_WARLORD] = 1;
    MOX::player[0].tech_applications[TECH_APP_HYPERSPACE_COMMUNICATIONS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::colony[0].buildings[BUILDING_STAR_BASE] = 1;
    MOX::colony[1].buildings[BUILDING_BATTLESTATION] = 1;
    MOX::colony[2].buildings[BUILDING_STAR_FORTRESS] = 1;

    COLCALC::command_points = {7, 2, 4, 6, 1, 3, 5, 3};
    const uint8_t usage[SHIP_SIZE_COUNT] = {2, 5, 8, 11, 14, 17};
    memcpy(COLCALC::ship_command_points_usage, usage, sizeof(usage));

    MOX::ship[0].owner = 0;
    MOX::ship[0].d.ship_type = SHIP_TYPE_COMBAT;
    MOX::ship[0].d.size = SHIP_SIZE_LARGE;
    MOX::ship[1].owner = 0;
    MOX::ship[1].d.ship_type = SHIP_TYPE_COLONY;
    MOX::ship[1].d.size = SHIP_SIZE_MEDIUM;

    COLCALC::Compute_Player_Maintenance(&MOX::player[0], nullptr);

    DOCTEST_CHECK(MOX::player[0].command_points == 43);
    DOCTEST_CHECK(MOX::player[0].command_points_used == 13);
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "negative command-point settings retain signed capacity and summary semantics") {
    const int8_t values[] = {-1, -128};
    char starting_msg[] = "starting";
    char used_msg[] = "used";

    for (int8_t value : values) {
        TEST_PLAYER_STATS::Reset_();
        MOX::NUM_SHIPS = 0;
        COLCALC::command_points.starting = value;
        COLCALC::starting_command_points_msg = starting_msg;
        COLCALC::total_command_points_used_msg = used_msg;

        char output[128];
        STRBUILD::s_builder builder = STRBUILD::Init(output, sizeof(output));
        COLCALC::Compute_Player_Maintenance(&MOX::player[0], &builder);

        char expected[128];
        snprintf(expected, sizeof(expected),
            "%d starting\n-------------------\n0 used\n-------------------\n",
            value);
        DOCTEST_CHECK(MOX::player[0].command_points == value);
        DOCTEST_CHECK(strcmp(output, expected) == 0);

        TEST_PLAYER_STATS::Reset_();
        MOX::NUM_COLONIES = 1;
        MOX::NUM_SHIPS = 0;
        MOX::colony[0].buildings[BUILDING_STAR_BASE] = 1;
        MOX::player[0].tech_applications[TECH_APP_TACHYON_COMMUNICATIONS] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        COLCALC::command_points = {};
        COLCALC::command_points.tachyon_communications = value;

        COLCALC::Compute_Player_Maintenance(&MOX::player[0], nullptr);
        DOCTEST_CHECK(MOX::player[0].command_points == value);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "configured Food Replicator PP costs cap spending without negative production") {
    struct s_case {
        uint8_t pp_cost;
        int16_t expected_food;
        int16_t expected_industry;
    };

    const s_case cases[] = {
        {0, 2, 3},
        {1, 2, 1},
        {2, 1, 1},
        {3, 1, 0},
    };

    for (const s_case& test_case : cases) {
        TEST_COLONY_REPLICATORS::Reset_();
        COLCALC::replicator_food_pp = test_case.pp_cost;

        s_colony* colony = &MOX::colony[0];
        colony->buildings[BUILDING_FOOD_REPLICATORS] = 1;
        colony->production[ECON_INDUSTRY] = 3;
        colony->food2_needed_for_conquered = 3;

        COLCALC::Colony_Replicators(colony);

        DOCTEST_CHECK((uint8_t)colony->n_food_replicated == test_case.expected_food);
        DOCTEST_CHECK((colony->production[ECON_FOOD] == test_case.expected_food));
        DOCTEST_CHECK((colony->production[ECON_INDUSTRY] == test_case.expected_industry));
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "Food Replicator BC cost is applied after climate-adjusted building maintenance") {
    struct s_case {
        uint8_t bc_cost;
        uint8_t expected_maintenance;
    };

    const s_case cases[] = {
        {0, 3},
        {1, 5},
        {3, 9},
    };

    for (const s_case& test_case : cases) {
        TEST_COLONY_REPLICATORS::Reset_();
        COLCALC::replicator_food_bc = test_case.bc_cost;

        s_colony* colony = &MOX::colony[0];
        colony->n_pops = 1;
        colony->climate = PLANET_CLIMATE_TOXIC;
        colony->buildings[BUILDING_AUTOMATED_FACTORY] = 1;
        colony->n_food_replicated = 2;

        COLCALC::Colony_BC_Maintenance(colony);

        DOCTEST_CHECK((uint8_t)colony->maintenance[ECON_BC] == test_case.expected_maintenance);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "tech app and field guards reject out-of-range ids") {
    s_player* player = &MOX::player[0];
    uint8_t before[TECH_APP_COUNT];
    memcpy(before, player->tech_applications, sizeof(before));

    // Out-of-range tech ids and a null player return without touching state.
    COLCALC::Player_Gets_Tech_App(player, -1);
    COLCALC::Player_Gets_Tech_App(player, TECH_APP_COUNT);
    COLCALC::Player_Gets_Tech_App(player, 5000);
    COLCALC::Player_Gets_Tech_App(nullptr, 0);
    DOCTEST_CHECK(memcmp(before, player->tech_applications, sizeof(before)) == 0);

    // Out-of-range field ids return without indexing the field tables
    // (ASan would fail the test on the old out-of-bounds access).
    COLCALC::Ensure_Uncreative_Field_OK(player, -1);
    COLCALC::Ensure_Uncreative_Field_OK(player, TECH_FIELD_COUNT);
    COLCALC::Ensure_Uncreative_Field_OK(nullptr, 0);
    COLCALC::Advance_Player_Field(player, -1, false);
    COLCALC::Advance_Player_Field(player, TECH_FIELD_COUNT, false);
    COLCALC::Advance_Player_Field(nullptr, 0, false);
    COLCALC::Give_Player_Field(player, -1, 0);
    COLCALC::Give_Player_Field(player, TECH_FIELD_COUNT, 0);
    COLCALC::Give_Player_Field(nullptr, 0, 0);
    DOCTEST_CHECK(memcmp(before, player->tech_applications, sizeof(before)) == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_production_state, "Phase Shifter grant skips its missing field") {
    namespace fs = std::filesystem;
    s_player* player = &MOX::player[0];
    // The fixture zeroes the tech tables, so restore the real Phase
    // Shifter row: application 125 has no field (TECH_FIELD_INVALID).
    TECHDATA::technology_applications[TECH_APP_PHASE_SHIFTER].tech_field_id = TECH_FIELD_INVALID;
    player->current_research_application = 0;
    player->current_research_field = 0;

    // Player_Gets_Tech_App updates ship designs, which names them from
    // shipname.lbx via MOX::global_data_seg; the fixture leaves it null,
    // so Push_Block would dereference null (UBSan). Allocate it as
    // resource_reload_test does and release it on scope exit.
    MOX::global_data_seg = allocate::Allocate_Space(0x2800 + MAX_SHIPS - ORIGINAL_MAX_SHIPS);
    struct Release_Global_Data_Seg {
        ~Release_Global_Data_Seg() {
            free((allocate::s_alloc_header*)MOX::global_data_seg - 1);
            MOX::global_data_seg = nullptr;
        }
    } release_global_data_seg;

    // The design-name lookup reads shipname.lbx through the VFS; give it
    // a synthetic table in a temp dir, as resource_reload_test does for
    // its LBX fixtures (64 records of 16 bytes cover every entry the
    // six hulls and eight variants can address).
    const fs::path root = fs::temp_directory_path() /
        ("orion-phase-shifter-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "SHIPNAME.LBX");
    {
        std::string table(64 * sizeof(MOX::design->name), '\0');
        for (int i = 0; i < 64; ++i) {
            snprintf(table.data() + i * sizeof(MOX::design->name), sizeof(MOX::design->name), "Ship%d", i);
        }
        const uint16_t count = 64;
        const uint16_t width = (uint16_t)sizeof(MOX::design->name);
        std::ofstream out(root / "SHIPNAME.LBX" / "000", std::ios::binary);
        out.write((const char*)&count, sizeof(count));
        out.write((const char*)&width, sizeof(width));
        out.write(table.data(), (std::streamsize)table.size());
        DOCTEST_REQUIRE(out.good());
    }
    DOCTEST_REQUIRE(vfs::Init(root.string().c_str()));
    struct Shutdown_Vfs {
        fs::path root;
        ~Shutdown_Vfs() {
            vfs::Shutdown();
            std::error_code error;
            fs::remove_all(root, error);
        }
    } shutdown_vfs{root};

    // The fixture zeroes the hull table, so Total_Design_Space() returns
    // 0 and the autodesign combat-speed calculation divides by hull_space
    // (design.cpp). Give every hull a nonzero size, as aidesign_test
    // does; the fixture restores the table on teardown.
    for (int i = 0; i < HULL_CLASS_COUNT; ++i) {
        TECHDATA::hull_data[i].size = 100;
    }

    uint8_t before[TECH_APP_COUNT];
    memcpy(before, player->tech_applications, sizeof(before));

    // On the old code this indexed technology_fields[-1]; ASan would fail.
    COLCALC::Player_Gets_Tech_App(player, TECH_APP_PHASE_SHIFTER);

    DOCTEST_CHECK(player->tech_applications[TECH_APP_PHASE_SHIFTER] == TECH_RESEARCH_STATUS_RESEARCHED);
    DOCTEST_CHECK(player->got_new_app == 1);
    before[TECH_APP_PHASE_SHIFTER] = TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_CHECK(memcmp(before, player->tech_applications, sizeof(before)) == 0);
}
