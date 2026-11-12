#include "pch.h"

namespace COLCALC {
    int8_t morale_lookup_table[GOVERNMENT_COUNT] = {-4, -4, -4, 0, 0, 0, 0, 0};
    int8_t non_capital_morale_lookup[GOVERNMENT_COUNT] = {-10, -7, -4, 0, 0, 0, 0, 0};
    int8_t barracks_morale_lookup[GOVERNMENT_COUNT] = {4, 4, 4, 4, 0, 0, 0, 0};
    int8_t colony_job_production_difficulty_lookup[8] = {-10, 0, 10, 20, 40, 0, 0, 0};
    int8_t difficulty_bc_lookup_table[8] = {0, 0, 1, 2, 3, 0, 0, 0};
    int8_t assimilation_rate_by_government[GOVERNMENT_COUNT] = {30, 60, 30, 60, 60, 120, 12, 16};

    s_command_points command_points = {
        5,
        1,
        2,
        3,
        1,
        2,
        3,
        2,
    };

    uint8_t ship_command_points_usage[SHIP_SIZE_COUNT] = {
        1,
        2,
        3,
        4,
        5,
        6,
    };

    s_government_bonus government_bonuses[GOVERNMENT_COUNT] = {
        {{1, 3}, -10, 0, 0, 0},
        {{2, 3}, -5, 0, 0, 0},
        {{0, 1}, 0, 0, 0, 0},
        {{0, 1}, 0, 0, 0, 0},
        {{0, 1}, 10, 10, 0, 0},
        {{0, 1}, 15, 15, 0, 0},
        {{0, 1}, 0, 0, 10, 10},
        {{0, 1}, 0, 0, 20, 20},
    };

    s_lander_ship_cost lander_ship_cost = {
        500,
        100,
        100,
    };

    s_population_capacity population_capacity = {
        {5, 10, 15, 20, 25},
        {25, 25, 25, 25, 25, 25, 40, 60, 80, 100},
    };

    bool population_capacity_table_enabled = false;
    uint8_t population_capacity_table[PLANET_CLIMATE_COUNT][PLANET_SIZE_COUNT] = {
        {1, 3, 4, 5, 6},
        {1, 3, 4, 5, 6},
        {1, 3, 4, 5, 6},
        {1, 3, 4, 5, 6},
        {1, 3, 4, 5, 6},
        {1, 3, 4, 5, 6},
        {2, 4, 6, 8, 10},
        {3, 6, 9, 12, 15},
        {4, 8, 12, 16, 20},
        {5, 10, 15, 20, 25},
    };
    uint8_t population_capacity_table_tolerant[PLANET_CLIMATE_COUNT][PLANET_SIZE_COUNT] = {
        {3, 5, 8, 10, 13},
        {3, 5, 8, 10, 13},
        {3, 5, 8, 10, 13},
        {3, 5, 8, 10, 13},
        {3, 5, 8, 10, 13},
        {3, 5, 8, 10, 13},
        {3, 7, 10, 13, 16},
        {4, 9, 13, 17, 21},
        {5, 10, 15, 20, 25},
        {5, 10, 15, 20, 25},
    };

    s_population_bonus population_bonus = {
        5,
        25,
    };

    s_misc_costs misc_costs = {
        50,
        50,
        50,
        50,
        100,
        250,
    };

    s_growth_rate_bonus growth_rate_bonus = {
        0,
        25,
        50,
        100,
        -200,
    };

    s_trade_goods_ratio trade_goods_ratio = {
        50,
        100,
    };

    s_bc_bonus bc_bonus = {
        50,
        100,
        50,
    };

    s_planet_bc_bonus planet_bc_bonus = {
        5,
        10,
    };

    uint8_t climate_maintenance_modifiers[PLANET_CLIMATE_COUNT] = {
        50,
        25,
        0,
        25,
        0,
        0,
        0,
        0,
        0,
        0,
    };

    s_food_colony_bonus food_colony_bonus = {
        2,
        4,
    };

    s_food_farmer_bonus food_farmer_bonus = {
        2,
        2,
        4,
        2,
        4,
        6,
    };

    s_morale_bonus morale_bonus = {
        4,
        6,
        2,
        4,
    };

    e_psionics_government_limit psionics_government_limit =
        e_psionics_government_limit::imperium;
    uint8_t fungi_climate = FUNGI_CLIMATE_CLASSIC;
    uint8_t android_morale = ANDROID_MORALE_WORKERS_ONLY;

    s_research_colony_bonus research_colony_bonus = {
        5,
        10,
        15,
        30,
    };

    s_research_scientist_bonus research_scientist_bonus = {
        3,
        2,
        5,
        1,
        2,
        3,
        1,
        3,
        1,
    };

    s_production_worker_bonus production_worker_bonus = {
        1,
        2,
        3,
        1,
        1,
        3,
        1,
    };

    s_production_colony_bonus production_colony_bonus = {
        5,
        10,
        15,
        {5, 8, 10, 15, 20},
    };

    uint8_t minerals_per_mine[8] = {
        1,
        2,
        3,
        5,
        8,
        0,
        0,
        0,
    };

    uint8_t planet_no_pollution_prod[PLANET_SIZE_COUNT] = {
        2,
        4,
        6,
        8,
        10,
    };
    uint8_t nano_disassemblers_coeff = 2;
    uint16_t pollution_processor_ratio = 50;
    uint16_t atmospheric_renewer_ratio = 25;

    uint8_t pop_bonus_biospheres = 2;
    uint8_t pop_bonus_subterranean[PLANET_SIZE_COUNT] = {
        2,
        4,
        6,
        8,
        10,
    };
    uint8_t weather_control_climate = PLANET_CLIMATE_TOXIC;
    uint8_t terraform_toxic_building = BUILDING_NO_BUILDING;
    uint8_t aquatic_treats_tundra_as_swamp = 0;
    uint8_t ignore_mixed_race_penalty[GOVERNMENT_COUNT] = {};
    s_colony_morale morale = {
        morale_lookup_table,
        non_capital_morale_lookup,
        ignore_mixed_race_penalty,
    };
    uint8_t housing_creates_no_farmers = 0;
    uint8_t housing_no_popagg = 0;
    uint8_t new_farmer_food_threshold = 0;
    uint8_t cumulative_growth_tech_bonuses = 0;
    uint8_t disable_housing = 0;
    uint8_t androids_available = 0;
    uint8_t planet_extinction = 0;
    uint8_t zero_colony_lost = 0;
    uint8_t feudal_affects_freighters = 0;
    uint8_t keep_research_on_breakthrough = 0;
    uint8_t keep_research_on_finds = 0;
    uint8_t fixed_research_cost = 0;
    uint16_t hyper_advanced_cost_increment = 10000;
    uint8_t scout_lab_research_bonus[SHIP_SIZE_COUNT] = {
        1,
        2,
        4,
        8,
        16,
        32,
    };
    uint8_t replicator_food_pp = 2;
    uint8_t replicator_food_bc = 1;
    uint8_t freighters_cash_bonus = 0;
    uint8_t multibuild = 0;
    uint8_t multibuild_nonbuilding_products_only = 0;
    uint8_t minimum_orbital_base_ship_class = SHIP_SIZE_LARGE;
    uint32_t growth_formula_factor = 2000;
    uint8_t housing_formula_factor = 40;
    uint8_t cloning_center_growth_bonus = 100;

} // namespace COLCALC