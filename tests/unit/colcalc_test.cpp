#include "pch.h"

#include "doctest/doctest.h"

#include "game/build_queue.h"
#include "game/colcalc.h"
#include "lua/lua_game.h"

namespace TEST_COLONY_POP_GROWTH {
    void Reset_();
    int32_t Destroy_Colony_Reason_();
    void Set_Add_Spy_Result_(int16_t result);
    int16_t Deleted_Product_();
    void Set_Population_Boom_(bool enabled);
    void Set_Plague_(bool enabled);
    int Custom_Autobuild_Calls_();
    void Set_Legacy_Autobuild_Product_(int16_t product);
    int Calc_Ship_Level_Calls_();
    int Init_Ship_Designs_Calls_();
    void Set_Population_Growth_Adjustment_(int16_t adjustment);
    int Population_Growth_Calls_();
    const orion2re::lua::game::s_population_growth_context&
    Population_Growth_Context_();
}

namespace {
    const AIDATA::s_world_view* AI_World_() {
        static const AIDATA::s_world_view world{
            .players = MOX::player,
            .stars = MOX::star,
            .colonies = MOX::colony,
            .planets = MOX::planet,
            .settings = &MOX::settings,
            .player_count = &MOX::NUM_PLAYERS,
            .star_count = &MOX::NUM_STARS,
            .colony_count = &MOX::NUM_COLONIES,
            .rules = {
                .valuation = &AIDATA::planet_valuation_rules,
                .colony = {
                    .race_picks = &raceopt::pick_rules,
                    .population_bonus = &COLCALC::population_bonus,
                    .food_farmer_bonus = &COLCALC::food_farmer_bonus,
                    .minerals_per_mine = COLCALC::minerals_per_mine,
                    .climate_maintenance_modifiers = COLCALC::climate_maintenance_modifiers,
                },
            },
        };
        return &world;
    }

    s_colony* Prepare_Growth_Modifier_() {
        TEST_COLONY_POP_GROWTH::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->planet = 0;
        colony->climate = PLANET_CLIMATE_GAIA;
        colony->n_pops = 1;
        colony->pop[0] = 0;
        MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        return colony;
    }

    s_colony* Prepare_Housing_Growth_() {
        TEST_COLONY_POP_GROWTH::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->planet = 0;
        colony->occupation_policy = 4;
        colony->climate = PLANET_CLIMATE_GAIA;
        colony->n_pops = 1;
        colony->pop[0] = 1u << 7;
        colony->pop_growth[0] = 1000;
        colony->food2_per_farmer = 2;
        colony->food2_needed_for_our_empire = 2;
        colony->maintenance[ECON_INDUSTRY] = 1;
        colony->producing[0] = COLONY_PRODUCTION_HOUSING;
        return colony;
    }

    s_colony* Prepare_Housing_Growth_Modifier_() {
        s_colony* colony = Prepare_Growth_Modifier_();
        colony->n_pops = 2;
        colony->pop[1] = 0;
        colony->producing[0] = COLONY_PRODUCTION_HOUSING;
        colony->production[ECON_INDUSTRY] = 11;
        colony->maintenance[ECON_INDUSTRY] = 1;
        COLCALC::housing_formula_factor = 40;
        return colony;
    }

    s_colony* Prepare_Starving_Colony_() {
        TEST_COLONY_POP_GROWTH::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->planet = 0;
        colony->occupation_policy = 4;
        colony->n_pops = 1;
        colony->pop_growth[0] = -1000;
        return colony;
    }

    s_colony* Prepare_Mixed_Race_Colony_() {
        TEST_COLONY_POP_GROWTH::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->n_pops = 2;
        colony->pop[0] = 0;
        colony->pop[1] = 1;
        MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_FEDERATION;
        return colony;
    }

    s_colony* Prepare_Weather_Controller_() {
        TEST_COLONY_POP_GROWTH::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->climate = PLANET_CLIMATE_TOXIC;
        colony->food2_per_farmer = 2;
        TECHDATA::buildings[BUILDING_WEATHER_CONTROLLER].tech_app =
            TECH_APP_WEATHER_CONTROL_SYSTEM;
        MOX::player[0].tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        return colony;
    }

    uint32_t New_Colonist_Job_(const s_colony* colony) {
        return (colony->pop[1] & 0x180u) >> 7;
    }

    s_colony* Prepare_Zero_Colony_With_Inbound_Settlers_() {
        TEST_COLONY_POP_GROWTH::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->planet = 0;
        SETTLER::Set_Dest_Planet(&MOX::player[0].settlers[0].val, colony->planet);
        MOX::player[0].settlers_freighted = 1;
        return colony;
    }

    s_colony* Prepare_Completed_Product_(int16_t product, int16_t cost) {
        TEST_COLONY_POP_GROWTH::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->planet = 0;
        colony->n_pops = 1;
        colony->producing[0] = product;
        colony->producing[1] = COLONY_PRODUCTION_NONE;
        colony->production_spent = cost;
        return colony;
    }

    s_colony* Prepare_Completed_Ship_(int8_t status) {
        s_colony* colony = Prepare_Completed_Product_(
            COLBLDG::Colony_Production_From_Ship_Index(0), 50);
        s_ship_data* ship = &MOX::ship[0];
        ship->owner = 0;
        ship->status = status;
        ship->location = 0;
        ship->d.ship_type = SHIP_TYPE_COMBAT;
        ship->d.cost = 50;
        return colony;
    }

    s_colony* Prepare_Multiple_Products_(int16_t production_spent) {
        TEST_COLONY_POP_GROWTH::Reset_();

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->planet = 0;
        colony->n_pops = 1;
        colony->producing[0] = BUILDING_MARINE_BARRACKS;
        colony->producing[1] = BUILDING_RESEARCH_LAB;
        colony->producing[2] = COLONY_PRODUCTION_NONE;
        colony->production_spent = production_spent;
        TECHDATA::buildings[BUILDING_MARINE_BARRACKS].cost = 60;
        TECHDATA::buildings[BUILDING_RESEARCH_LAB].cost = 50;
        return colony;
    }
}

DOCTEST_TEST_CASE("government mixed-race policy preserves the classic default") {
    s_colony* colony = Prepare_Mixed_Race_Colony_();

    DOCTEST_CHECK(COLCALC::Mixed_Race_Morale_Penalty(colony) == -4);

    COLCALC::ignore_mixed_race_penalty[GOVERNMENT_FEDERATION] = 1;

    DOCTEST_CHECK(COLCALC::Mixed_Race_Morale_Penalty(colony) == 0);

    MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_DEMOCRACY;

    DOCTEST_CHECK(COLCALC::Mixed_Race_Morale_Penalty(colony) == -4);

    COLCALC::ignore_mixed_race_penalty[GOVERNMENT_DEMOCRACY] = 1;

    DOCTEST_CHECK(COLCALC::Mixed_Race_Morale_Penalty(colony) == 0);
}

DOCTEST_TEST_CASE("Alien Control Center mixed-race restriction is Federation-only") {
    struct Test_Case {
        uint8_t government;
        uint8_t classic_result;
        uint8_t configured_result;
    };

    constexpr Test_Case test_cases[] = {
        {GOVERNMENT_FEDERATION, 1, 2},
        {GOVERNMENT_DICTATORSHIP, 1, 1},
        {GOVERNMENT_UNIFICATION, 2, 2},
    };

    for (const Test_Case& test_case : test_cases) {
        s_colony* colony = Prepare_Mixed_Race_Colony_();
        TECHDATA::buildings[BUILDING_ALIEN_CONTROL_CENTER].tech_app =
            TECH_APP_ALIEN_MANAGEMENT_CENTER;
        MOX::player[0].tech_applications[TECH_APP_ALIEN_MANAGEMENT_CENTER] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] =
            test_case.government;

        DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
            colony, BUILDING_ALIEN_CONTROL_CENTER) ==
            test_case.classic_result);

        COLCALC::ignore_mixed_race_penalty[test_case.government] = 1;

        DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
            colony, BUILDING_ALIEN_CONTROL_CENTER) ==
            test_case.configured_result);

        colony->pop[1] |= 0x400;

        DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
            colony, BUILDING_ALIEN_CONTROL_CENTER) == 1);
    }
}

DOCTEST_TEST_CASE("Dimensional Portal is limited to one per player per system") {
    TEST_COLONY_POP_GROWTH::Reset_();

    for (int i = 0; i < 5; ++i) {
        MOX::star[0].planet_index[i] = -1;
    }
    MOX::star[0].planet_index[0] = 0;
    MOX::star[0].planet_index[1] = 1;

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    MOX::planet[0].star_index = 0;
    MOX::planet[0].colony_index = 0;

    s_colony* other_colony = &MOX::colony[1];
    other_colony->owner = 1;
    other_colony->planet = 1;
    MOX::planet[1].star_index = 0;
    MOX::planet[1].colony_index = 1;
    for (int i = 0; i < 7; ++i) {
        other_colony->producing[i] = COLONY_PRODUCTION_NONE;
    }

    TECHDATA::buildings[BUILDING_DIMENSIONAL_PORTAL].tech_app =
        TECH_APP_DIMENSIONAL_PORTAL;
    MOX::player[0].tech_applications[TECH_APP_DIMENSIONAL_PORTAL] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    other_colony->buildings[BUILDING_DIMENSIONAL_PORTAL] = 1;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, BUILDING_DIMENSIONAL_PORTAL) == 1);

    other_colony->owner = 0;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, BUILDING_DIMENSIONAL_PORTAL) == 2);

    other_colony->buildings[BUILDING_DIMENSIONAL_PORTAL] = 0;
    other_colony->producing[0] = BUILDING_DIMENSIONAL_PORTAL;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, BUILDING_DIMENSIONAL_PORTAL) == 2);
}

DOCTEST_TEST_CASE("Weather Controller preserves classic climate eligibility") {
    s_colony* colony = Prepare_Weather_Controller_();

    colony->food2_per_farmer = 0;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, BUILDING_WEATHER_CONTROLLER) == 2);

    colony->food2_per_farmer = 2;
    colony->climate = PLANET_CLIMATE_RADIATED;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, BUILDING_WEATHER_CONTROLLER) == 1);
}

DOCTEST_TEST_CASE("Weather Controller uses the configured climate") {
    s_colony* colony = Prepare_Weather_Controller_();
    COLCALC::weather_control_climate = PLANET_CLIMATE_DESERT;

    colony->climate = PLANET_CLIMATE_BARREN;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, BUILDING_WEATHER_CONTROLLER) == 2);

    colony->climate = PLANET_CLIMATE_DESERT;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, BUILDING_WEATHER_CONTROLLER) == 1);

    COLCALC::weather_control_climate = PLANET_CLIMATE_RADIATED;
    colony->climate = PLANET_CLIMATE_TOXIC;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, BUILDING_WEATHER_CONTROLLER) == 1);

    colony->climate = PLANET_CLIMATE_RADIATED;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, BUILDING_WEATHER_CONTROLLER) == 2);
}

DOCTEST_TEST_CASE("configured building converts derived Toxic climate to Barren") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    MOX::planet[0].climate = PLANET_CLIMATE_TOXIC;

    COLCALC::Colony_Environmental_Stuff(colony);
    DOCTEST_CHECK(colony->climate == PLANET_CLIMATE_TOXIC);

    COLCALC::terraform_toxic_building = BUILDING_ATMOSPHERE_RENEWER;
    colony->buildings[BUILDING_ATMOSPHERE_RENEWER] = 1;
    COLCALC::Colony_Environmental_Stuff(colony);

    DOCTEST_CHECK(colony->climate == PLANET_CLIMATE_BARREN);
    DOCTEST_CHECK(MOX::planet[0].climate == PLANET_CLIMATE_TOXIC);

    colony->buildings[BUILDING_ATMOSPHERE_RENEWER] = 0;
    COLCALC::Colony_Environmental_Stuff(colony);
    DOCTEST_CHECK(colony->climate == PLANET_CLIMATE_TOXIC);
}

DOCTEST_TEST_CASE("population growth preserves the classic base modifier") {
    s_colony* colony = Prepare_Growth_Modifier_();

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] == 100);
}

DOCTEST_TEST_CASE("population growth passes the final native value through the Lua rule boundary") {
    s_colony* colony = Prepare_Growth_Modifier_();
    colony->n_pops = 2;
    colony->pop[1] = 0;
    TEST_COLONY_POP_GROWTH::Set_Population_Growth_Adjustment_(25);

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] == 125);
    DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Population_Growth_Calls_() == 1);
    const auto& context =
        TEST_COLONY_POP_GROWTH::Population_Growth_Context_();
    DOCTEST_CHECK(context.colony_id == 0);
    DOCTEST_CHECK(context.planet_id == 0);
    DOCTEST_CHECK(context.owner_id == 0);
    DOCTEST_CHECK(context.race_id == 0);
    DOCTEST_CHECK(context.race_population == 2);
    DOCTEST_CHECK(context.total_population == 2);
    DOCTEST_CHECK(context.population_limit == 25);
    DOCTEST_CHECK(context.climate == PLANET_CLIMATE_GAIA);
    DOCTEST_CHECK_FALSE(context.housing);
    DOCTEST_CHECK_FALSE(context.population_boom);
    DOCTEST_CHECK_FALSE(context.plague);
}

DOCTEST_TEST_CASE("population growth rules receive races at capacity") {
    s_colony* colony = Prepare_Growth_Modifier_();
    colony->n_pops = 25;
    for (int index = 1; index < 25; ++index) {
        colony->pop[index] = 0;
    }
    TEST_COLONY_POP_GROWTH::Set_Population_Growth_Adjustment_(25);

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] == 25);
    DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Population_Growth_Calls_() == 1);
    const auto& context =
        TEST_COLONY_POP_GROWTH::Population_Growth_Context_();
    DOCTEST_CHECK(context.race_population == 25);
    DOCTEST_CHECK(context.total_population == 25);
    DOCTEST_CHECK(context.population_limit == 25);
}

DOCTEST_TEST_CASE("population growth widens the configured formula before taking its square root") {
    s_colony* colony = Prepare_Growth_Modifier_();
    colony->n_pops = 22;
    for (int index = 0; index < 21; ++index) {
        colony->pop[index] = 0;
    }
    colony->pop[21] = 1;
    MOX::NUM_PLAYERS = 2;
    MOX::player[0].traits[TRAIT_POP_GROWTH_BONUS] = -75;
    MOX::player[1].traits[TRAIT_POP_GROWTH_BONUS] = -100;
    COLCALC::growth_formula_factor = INT32_MAX;

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Population_Growth_Calls_() == 2);
    DOCTEST_CHECK(colony->pop_growth[0] == 18390);
}

DOCTEST_TEST_CASE("Cloning Center growth uses the configured bonus") {
    s_colony* colony = Prepare_Growth_Modifier_();
    colony->buildings[BUILDING_CLONING_CENTER] = 1;

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] == 200);

    colony = Prepare_Growth_Modifier_();
    colony->buildings[BUILDING_CLONING_CENTER] = 1;
    COLCALC::cloning_center_growth_bonus = 37;

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] == 137);
}

DOCTEST_TEST_CASE("general population growth modifier is independent of Housing") {
    s_colony* colony = Prepare_Growth_Modifier_();
    COLCALC::growth_rate_bonus.general_modifier = 40;

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK((colony->producing[0] != COLONY_PRODUCTION_HOUSING));
    DOCTEST_CHECK(colony->pop_growth[0] == 140);
}

DOCTEST_TEST_CASE("Housing may ignore the total population divisor") {
    s_colony* colony = Prepare_Housing_Growth_Modifier_();

    COLCALC::Colony_Pop_Grows(colony);
    const int16_t classic_growth = colony->pop_growth[0];

    colony = Prepare_Housing_Growth_Modifier_();
    COLCALC::housing_no_popagg = 1;
    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] - classic_growth == 200);
}

DOCTEST_TEST_CASE("population growth uses configured technology modifiers") {
    s_colony* colony = Prepare_Growth_Modifier_();
    COLCALC::growth_rate_bonus.microbiotics_tech = 7;
    MOX::player[0].tech_applications[TECH_APP_MICROBIOTICS] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] == 107);

    colony = Prepare_Growth_Modifier_();
    COLCALC::growth_rate_bonus.microbiotics_tech = 7;
    COLCALC::growth_rate_bonus.antidote_tech = 13;
    MOX::player[0].tech_applications[TECH_APP_MICROBIOTICS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_UNIVERSAL_ANTIDOTE] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] == 113);

    colony = Prepare_Growth_Modifier_();
    COLCALC::growth_rate_bonus.microbiotics_tech = 7;
    COLCALC::growth_rate_bonus.antidote_tech = 13;
    COLCALC::cumulative_growth_tech_bonuses = 1;
    MOX::player[0].tech_applications[TECH_APP_MICROBIOTICS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_UNIVERSAL_ANTIDOTE] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] == 120);
}

DOCTEST_TEST_CASE("population growth uses configured event modifiers") {
    s_colony* colony = Prepare_Growth_Modifier_();
    COLCALC::growth_rate_bonus.boom_event = 11;
    COLCALC::growth_rate_bonus.plague_event = -22;
    TEST_COLONY_POP_GROWTH::Set_Population_Boom_(true);
    TEST_COLONY_POP_GROWTH::Set_Plague_(true);

    COLCALC::Colony_Pop_Grows(colony);

    DOCTEST_CHECK(colony->pop_growth[0] == 89);
}

DOCTEST_TEST_CASE("classic Housing growth may create a farmer") {
    s_colony* colony = Prepare_Housing_Growth_();

    COLCALC::Apply_Colony_Pop_Growth(colony);

    DOCTEST_REQUIRE(colony->n_pops == 2);
    DOCTEST_CHECK(New_Colonist_Job_(colony) == 0);
}

DOCTEST_TEST_CASE("configured Housing growth bypasses farmer assignment") {
    s_colony* colony = Prepare_Housing_Growth_();
    COLCALC::housing_creates_no_farmers = 1;

    COLCALC::Apply_Colony_Pop_Growth(colony);

    DOCTEST_REQUIRE(colony->n_pops == 2);
    DOCTEST_CHECK(New_Colonist_Job_(colony) == 1);
}

DOCTEST_TEST_CASE("new farmer food threshold requires strictly greater base food") {
    s_colony* colony = Prepare_Housing_Growth_();
    colony->producing[0] = COLONY_PRODUCTION_NONE;
    COLCALC::new_farmer_food_threshold = 2;

    COLCALC::Apply_Colony_Pop_Growth(colony);

    DOCTEST_REQUIRE(colony->n_pops == 2);
    DOCTEST_CHECK(New_Colonist_Job_(colony) == 1);

    colony = Prepare_Housing_Growth_();
    colony->producing[0] = COLONY_PRODUCTION_NONE;
    colony->food2_per_farmer = 3;
    COLCALC::new_farmer_food_threshold = 2;

    COLCALC::Apply_Colony_Pop_Growth(colony);

    DOCTEST_REQUIRE(colony->n_pops == 2);
    DOCTEST_CHECK(New_Colonist_Job_(colony) == 0);
}

DOCTEST_TEST_CASE("farmer selection uses unsigned food before applying Housing suppression") {
    struct s_growth_case {
        uint8_t food2_per_farmer;
        uint8_t housing_creates_no_farmers;
        uint32_t expected_job;
    };
    const s_growth_case cases[] = {
        {127, 0, 1},
        {127, 1, 1},
        {128, 0, 0},
        {128, 1, 1},
        {255, 0, 0},
        {255, 1, 1},
    };

    for (const s_growth_case& test_case : cases) {
        s_colony* colony = Prepare_Housing_Growth_();
        colony->food2_per_farmer = test_case.food2_per_farmer;
        COLCALC::new_farmer_food_threshold = 127;
        COLCALC::housing_creates_no_farmers =
            test_case.housing_creates_no_farmers;

        COLCALC::Apply_Colony_Pop_Growth(colony);

        DOCTEST_REQUIRE(colony->n_pops == 2);
        DOCTEST_CHECK(New_Colonist_Job_(colony) == test_case.expected_job);
    }
}

DOCTEST_TEST_CASE("reported population capacities do not exceed colony storage") {
    TEST_COLONY_POP_GROWTH::Reset_();

    const uint8_t previous_max_population =
        COLCALC::population_capacity.size[PLANET_SIZE_HUGE];
    COLCALC::population_capacity.size[PLANET_SIZE_HUGE] = 50;

    s_planet_data* planet = &MOX::planet[0];
    planet->colony_index = -1;
    planet->size = PLANET_SIZE_HUGE;
    planet->climate = PLANET_CLIMATE_GAIA;

    DOCTEST_CHECK(COLCALC::Planet_Max_Population_For_Player(planet, 0) == 42);

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    colony->climate = PLANET_CLIMATE_GAIA;

    DOCTEST_CHECK(COLCALC::Colony_Race_Pop_Limit(colony, 0) == 42);

    COLCALC::population_capacity.size[PLANET_SIZE_HUGE] =
        previous_max_population;
}

DOCTEST_TEST_CASE("population capacity tables reproduce every classic formula result") {
    TEST_COLONY_POP_GROWTH::Reset_();
    const bool previous_enabled = COLCALC::population_capacity_table_enabled;
    const uint8_t expected[PLANET_CLIMATE_COUNT][PLANET_SIZE_COUNT] = {
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

    for (int climate = 0; climate < PLANET_CLIMATE_COUNT; ++climate) {
        for (int8_t size = 0; size < PLANET_SIZE_COUNT; ++size) {
            for (uint8_t immune = 0; immune <= 1; ++immune) {
                COLCALC::population_capacity_table_enabled = false;
                const int formula = COLCALC::Size_And_Climate_Max_Population(
                    size, climate, immune);
                COLCALC::population_capacity_table_enabled = true;
                DOCTEST_CHECK(COLCALC::Size_And_Climate_Max_Population(
                                  size, climate, immune) == formula);
                if (immune == 0) {
                    DOCTEST_CHECK(formula == expected[climate][size]);
                }
            }
        }
    }
    COLCALC::population_capacity_table_enabled = previous_enabled;
}

DOCTEST_TEST_CASE("population table overrides are independent of formula settings") {
    TEST_COLONY_POP_GROWTH::Reset_();
    const bool previous_enabled = COLCALC::population_capacity_table_enabled;
    const auto previous_capacity = COLCALC::population_capacity;
    const int8_t previous_tolerant = COLCALC::population_bonus.tolerant;
    uint8_t& normal = COLCALC::population_capacity_table
        [PLANET_CLIMATE_TOXIC][PLANET_SIZE_SMALL];
    uint8_t& tolerant = COLCALC::population_capacity_table_tolerant
        [PLANET_CLIMATE_TOXIC][PLANET_SIZE_SMALL];
    const uint8_t previous_normal = normal;
    const uint8_t previous_immune = tolerant;
    normal = 7;
    tolerant = 11;
    COLCALC::population_capacity.size[PLANET_SIZE_SMALL] = 20;
    COLCALC::population_capacity.climate[PLANET_CLIMATE_TOXIC] = 40;
    COLCALC::population_bonus.tolerant = 50;

    COLCALC::population_capacity_table_enabled = false;
    DOCTEST_CHECK(COLCALC::Size_And_Climate_Max_Population(
                      PLANET_SIZE_SMALL, PLANET_CLIMATE_TOXIC, 0) == 8);
    DOCTEST_CHECK(COLCALC::Size_And_Climate_Max_Population(
                      PLANET_SIZE_SMALL, PLANET_CLIMATE_TOXIC, 1) == 18);

    COLCALC::population_capacity_table_enabled = true;
    DOCTEST_CHECK(COLCALC::Size_And_Climate_Race_Pop_Limit(
                      PLANET_SIZE_SMALL, PLANET_CLIMATE_TOXIC, 0) == 7);
    MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] = 1;
    DOCTEST_CHECK(COLCALC::Size_And_Climate_Race_Pop_Limit(
                      PLANET_SIZE_SMALL, PLANET_CLIMATE_TOXIC, 0) == 11);
    DOCTEST_CHECK(COLCALC::Size_And_Climate_Race_Pop_Limit(
                      PLANET_SIZE_SMALL, PLANET_CLIMATE_TOXIC, MAX_PLAYERS) == 11);
    DOCTEST_CHECK(COLCALC::Size_And_Climate_Max_Population(
                      PLANET_SIZE_SMALL, PLANET_CLIMATE_BARREN, 0) == 3);

    normal = previous_normal;
    tolerant = previous_immune;
    COLCALC::population_capacity = previous_capacity;
    COLCALC::population_bonus.tolerant = previous_tolerant;
    COLCALC::population_capacity_table_enabled = previous_enabled;
}

DOCTEST_TEST_CASE("population tables preserve climate adjustments bonuses and storage cap") {
    TEST_COLONY_POP_GROWTH::Reset_();
    const bool previous_enabled = COLCALC::population_capacity_table_enabled;
    const uint8_t previous_aquatic_treatment = COLCALC::aquatic_treats_tundra_as_swamp;
    uint8_t& normal = COLCALC::population_capacity_table
        [PLANET_CLIMATE_TERRAN][PLANET_SIZE_SMALL];
    uint8_t& tolerant = COLCALC::population_capacity_table_tolerant
        [PLANET_CLIMATE_TERRAN][PLANET_SIZE_SMALL];
    const uint8_t previous_normal = normal;
    const uint8_t previous_immune = tolerant;
    normal = 13;
    tolerant = 35;
    COLCALC::population_capacity_table_enabled = true;
    MOX::player[0].traits[TRAIT_AQUATIC] = 1;

    s_planet_data* planet = &MOX::planet[0];
    planet->colony_index = -1;
    planet->size = PLANET_SIZE_SMALL;
    planet->climate = PLANET_CLIMATE_TUNDRA;
    DOCTEST_CHECK(COLCALC::Planet_Max_Population_For_Player(planet, 0) == 13);
    MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] = 1;
    DOCTEST_CHECK(COLCALC::Planet_Max_Population_For_Player(planet, 0) == 35);
    MOX::player[0].traits[TRAIT_SUBTERRANEAN] = 1;
    DOCTEST_CHECK(COLCALC::Planet_Max_Population_For_Player(planet, 0) == 39);

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    colony->climate = PLANET_CLIMATE_TUNDRA;
    colony->buildings[BUILDING_BIOSPHERES] = 1;
    DOCTEST_CHECK(COLCALC::Colony_Race_Pop_Limit(colony, 0) == 41);
    MOX::player[0].tech_applications[TECH_APP_ADVANCED_CITY_PLANNING] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_CHECK(COLCALC::Colony_Race_Pop_Limit(colony, 0) == 42);
    DOCTEST_CHECK(COLCALC::Planet_Max_Population_For_Player(planet, 0) == 42);

    COLCALC::aquatic_treats_tundra_as_swamp = 1;
    DOCTEST_CHECK(COLCALC::Size_And_Climate_Race_Pop_Limit(
                      PLANET_SIZE_SMALL, PLANET_CLIMATE_TUNDRA, 0) == 11);

    normal = previous_normal;
    tolerant = previous_immune;
    COLCALC::aquatic_treats_tundra_as_swamp = previous_aquatic_treatment;
    COLCALC::population_capacity_table_enabled = previous_enabled;
}

DOCTEST_TEST_CASE("Android room check supports a capacity of exactly 42") {
    TEST_COLONY_POP_GROWTH::Reset_();

    const uint8_t previous_max_population =
        COLCALC::population_capacity.size[PLANET_SIZE_HUGE];
    COLCALC::population_capacity.size[PLANET_SIZE_HUGE] = 42;

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    colony->climate = PLANET_CLIMATE_GAIA;
    MOX::planet[colony->planet].size = PLANET_SIZE_HUGE;

    DOCTEST_REQUIRE(
        COLCALC::Colony_Race_Pop_Limit(
            colony, (int16_t)POP::RACE_ANDROID) == 42);
    DOCTEST_CHECK(
        COLCALC::Room_For_Another_Dude(
            colony, (int)POP::RACE_ANDROID));

    colony->n_pops = 42;
    for (int32_t i = 0; i < 42; ++i) {
        POP::Set_Race(&colony->pop[i], POP::RACE_ANDROID);
    }

    DOCTEST_CHECK_FALSE(
        COLCALC::Room_For_Another_Dude(
            colony, (int)POP::RACE_ANDROID));

    COLCALC::population_capacity.size[PLANET_SIZE_HUGE] =
        previous_max_population;
}

DOCTEST_TEST_CASE("Biospheres population bonus is configurable") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    colony->climate = PLANET_CLIMATE_GAIA;
    colony->buildings[BUILDING_BIOSPHERES] = 1;

    DOCTEST_CHECK(COLCALC::Colony_Race_Pop_Limit(colony, 0) == 27);

    COLCALC::pop_bonus_biospheres = 7;

    DOCTEST_CHECK(COLCALC::Colony_Race_Pop_Limit(colony, 0) == 32);
}

DOCTEST_TEST_CASE("Advanced City Planning population bonus is configurable") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    colony->climate = PLANET_CLIMATE_GAIA;
    MOX::player[0].tech_applications[TECH_APP_ADVANCED_CITY_PLANNING] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(COLCALC::Colony_Race_Pop_Limit(colony, 0) == 30);

    COLCALC::population_bonus.advanced_city_planning = 7;

    DOCTEST_CHECK(COLCALC::Colony_Race_Pop_Limit(colony, 0) == 32);
}

DOCTEST_TEST_CASE("Tolerant population capacity uses the configured percentage") {
    TEST_COLONY_POP_GROWTH::Reset_();
    MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] = 1;
    DOCTEST_REQUIRE(MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] == 1);

    DOCTEST_CHECK(COLCALC::Size_And_Climate_Race_Pop_Limit(
        PLANET_SIZE_SMALL, PLANET_CLIMATE_TOXIC, 0) == 5);

    COLCALC::population_bonus.tolerant = 50;

    DOCTEST_CHECK(COLCALC::Size_And_Climate_Race_Pop_Limit(
        PLANET_SIZE_SMALL, PLANET_CLIMATE_TOXIC, 0) == 8);
}

DOCTEST_TEST_CASE("Subterranean population bonus is configurable by planet size") {
    TEST_COLONY_POP_GROWTH::Reset_();
    MOX::player[0].traits[TRAIT_SUBTERRANEAN] = 1;

    for (int size = 0; size < PLANET_SIZE_COUNT; ++size) {
        DOCTEST_CHECK(
            COLCALC::Size_And_Climate_Race_Pop_Limit(
                (int8_t)size, PLANET_CLIMATE_GAIA, 0) ==
            COLCALC::population_capacity.size[size] +
                COLCALC::pop_bonus_subterranean[size]);
    }

    COLCALC::pop_bonus_subterranean[PLANET_SIZE_SMALL] = 9;

    DOCTEST_CHECK(
        COLCALC::Size_And_Climate_Race_Pop_Limit(
            PLANET_SIZE_SMALL, PLANET_CLIMATE_GAIA, 0) == 19);
}

DOCTEST_TEST_CASE("uncolonized planet AI separates size weight from Subterranean capacity") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_planet_data* planet = &MOX::planet[0];
    planet->colony_index = -1;
    planet->planet_type = PLANET_TYPE_PLANET;
    planet->size = PLANET_SIZE_MEDIUM;
    planet->climate = PLANET_CLIMATE_GAIA;
    planet->mineral_class = 0;
    planet->gravity_class = PLANET_GRAVITY_NORMAL_G;

    DOCTEST_CHECK(AIDATA::Uncolonized_Planet_Worth_To_Player(
        AI_World_(),
        planet, &MOX::player[0]) == 27);

    MOX::player[0].traits[TRAIT_SUBTERRANEAN] = 1;
    COLCALC::pop_bonus_subterranean[PLANET_SIZE_MEDIUM] = 9;

    DOCTEST_CHECK(AIDATA::Uncolonized_Planet_Worth_To_Player(
        AI_World_(),
        planet, &MOX::player[0]) == 43);
}

DOCTEST_TEST_CASE("configured Growth values drive both AI planet-worth consumers") {
    struct s_growth_case {
        raceopt::e_primary_choice choice;
    };
    const s_growth_case cases[] = {
        {raceopt::PRIMARY_CHOICE_PENALTY},
        {raceopt::PRIMARY_CHOICE_BONUS},
        {raceopt::PRIMARY_CHOICE_MAJOR_BONUS},
    };

    for (const s_growth_case& test_case : cases) {
        TEST_COLONY_POP_GROWTH::Reset_();
        AIDATA::s_state ai;
        ai.world = *AI_World_();
        ai.colonies[0].dominant_race = 0;
        MOX::NUM_PLAYERS = 1;
        MOX::settings.game_difficulty = GAME_DIFFICULTY_TUTOR;

        s_planet_data* planet = &MOX::planet[0];
        planet->planet_type = PLANET_TYPE_PLANET;
        planet->size = PLANET_SIZE_MEDIUM;
        planet->climate = PLANET_CLIMATE_GAIA;
        planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
        planet->mineral_class = MINERAL_RESOURCE_ABUNDANT;
        planet->food_per_farmer = 4;
        planet->colony_index = 0;

        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->planet = 0;
        colony->climate = PLANET_CLIMATE_GAIA;
        colony->n_pops = 4;
        for (int pop = 0; pop < colony->n_pops; ++pop) {
            colony->pop[pop] = 1u << 7;
        }

        int8_t& configured_value = raceopt::pick_rules.primary
            [raceopt::PRIMARY_PICK_GROWTH][test_case.choice].value;
        const int8_t original_value = configured_value;
        const int8_t replacement_value = 127;

        MOX::player[0].traits[TRAIT_POP_GROWTH_BONUS] = original_value;
        const int16_t original_colony_worth =
            AIDATA::Colony_Worth_To_Player(&ai, colony, &MOX::player[0]);
        const int32_t original_planet_worth =
            AIDATA::Uncolonized_Planet_Worth_To_Player(
                AI_World_(),
                planet, &MOX::player[0]);

        MOX::player[0].traits[TRAIT_POP_GROWTH_BONUS] = replacement_value;
        const int16_t unmatched_colony_worth =
            AIDATA::Colony_Worth_To_Player(&ai, colony, &MOX::player[0]);
        const int32_t unmatched_planet_worth =
            AIDATA::Uncolonized_Planet_Worth_To_Player(
                AI_World_(),
                planet, &MOX::player[0]);
        DOCTEST_CHECK(unmatched_colony_worth != original_colony_worth);
        DOCTEST_CHECK(unmatched_planet_worth != original_planet_worth);

        configured_value = replacement_value;
        const int16_t configured_colony_worth =
            AIDATA::Colony_Worth_To_Player(&ai, colony, &MOX::player[0]);
        const int32_t configured_planet_worth =
            AIDATA::Uncolonized_Planet_Worth_To_Player(
                AI_World_(),
                planet, &MOX::player[0]);
        configured_value = original_value;

        DOCTEST_CHECK(configured_colony_worth == original_colony_worth);
        DOCTEST_CHECK(configured_planet_worth == original_planet_worth);
    }
}

DOCTEST_TEST_CASE("Aquatic Tundra population uses the configured climate treatment") {
    TEST_COLONY_POP_GROWTH::Reset_();
    MOX::player[0].traits[TRAIT_AQUATIC] = 1;

    DOCTEST_CHECK(
        COLCALC::Size_And_Climate_Race_Pop_Limit(
            PLANET_SIZE_SMALL, PLANET_CLIMATE_TUNDRA, 0) == 8);

    COLCALC::aquatic_treats_tundra_as_swamp = 1;

    DOCTEST_CHECK(
        COLCALC::Size_And_Climate_Race_Pop_Limit(
            PLANET_SIZE_SMALL, PLANET_CLIMATE_TUNDRA, 0) == 4);
}

DOCTEST_TEST_CASE("potential colony food uses the configured Aquatic Tundra treatment") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_player player = {};
    player.traits[TRAIT_AQUATIC] = 1;

    s_planet_data planet = {};
    planet.climate = PLANET_CLIMATE_TUNDRA;
    planet.food_per_farmer = 2;

    MOX::food_per_farmer_table[PLANET_CLIMATE_TUNDRA] = 2;
    MOX::food_per_farmer_table[PLANET_CLIMATE_SWAMP] = 4;
    MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN] = 6;

    s_potential_colony_info info = {};
    COLCALC::Compute_Potential_Colony_Info(&player, &planet, &info);
    DOCTEST_CHECK(info.food == 6);

    COLCALC::aquatic_treats_tundra_as_swamp = 1;
    COLCALC::Compute_Potential_Colony_Info(&player, &planet, &info);
    DOCTEST_CHECK(info.food == 4);
}

DOCTEST_TEST_CASE("planet maintenance consumers preserve unsigned modifiers") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_planet_data* planet = &MOX::planet[0];
    planet->planet_type = PLANET_TYPE_PLANET;
    planet->size = PLANET_SIZE_MEDIUM;
    planet->climate = PLANET_CLIMATE_TOXIC;
    planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
    planet->mineral_class = MINERAL_RESOURCE_ULTRA_POOR;

    const uint8_t modifiers[] = {0, 127, 128, 255};
    int32_t worth[4] = {};
    for (int i = 0; i < 4; ++i) {
        COLCALC::climate_maintenance_modifiers[PLANET_CLIMATE_TOXIC] =
            modifiers[i];

        s_potential_colony_info info = {};
        COLCALC::Compute_Potential_Colony_Info(
            &MOX::player[0], planet, &info);
        DOCTEST_CHECK(info.maintenance == modifiers[i]);

        worth[i] = AIDATA::Uncolonized_Planet_Worth_To_Player(
            AI_World_(),
            planet, &MOX::player[0]);
    }

    DOCTEST_CHECK(worth[0] > worth[2]);
    DOCTEST_CHECK(worth[2] > worth[3]);
}

DOCTEST_TEST_CASE("Biomorphic Fungi climate policy is shared by colony and AI food calculations") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    s_planet_data* planet = &MOX::planet[0];
    planet->planet_type = PLANET_TYPE_PLANET;
    planet->size = PLANET_SIZE_MEDIUM;
    planet->climate = PLANET_CLIMATE_BARREN;
    planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
    planet->food_per_farmer = 0;
    MOX::player[0].tech_applications[TECH_APP_BIOMORPHIC_FUNGI] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(COLCALC::Colony_Food2_Per_Farmer(colony, 0, nullptr) == 2);

    COLCALC::fungi_climate = COLCALC::FUNGI_CLIMATE_REVERSED;
    DOCTEST_CHECK(COLCALC::Colony_Food2_Per_Farmer(colony, 0, nullptr) == 0);

    planet->food_per_farmer = 4;
    DOCTEST_CHECK(COLCALC::Colony_Food2_Per_Farmer(colony, 0, nullptr) == 6);

    COLCALC::fungi_climate = COLCALC::FUNGI_CLIMATE_UNITED;
    COLCALC::food_farmer_bonus.biomorphic_fungi = 3;
    DOCTEST_CHECK(COLCALC::Colony_Food2_Per_Farmer(colony, 0, nullptr) == 7);
}

DOCTEST_TEST_CASE("uncolonized planet AI food worth is configurable per half-food unit") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_planet_data* planet = &MOX::planet[0];
    planet->planet_type = PLANET_TYPE_PLANET;
    planet->size = PLANET_SIZE_MEDIUM;
    planet->climate = PLANET_CLIMATE_GAIA;
    planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
    planet->food_per_farmer = 2;

    int32_t biological_worth = AIDATA::Uncolonized_Planet_Worth_To_Player(
        AI_World_(),
        planet, &MOX::player[0]);
    AIDATA::planet_valuation_rules.food.biological = 100;
    int32_t configured_biological_worth =
        AIDATA::Uncolonized_Planet_Worth_To_Player(AI_World_(), planet, &MOX::player[0]);
    DOCTEST_CHECK(configured_biological_worth > biological_worth);

    MOX::player[0].traits[TRAIT_CYBERNETIC] = 1;
    int32_t cybernetic_worth = AIDATA::Uncolonized_Planet_Worth_To_Player(
        AI_World_(),
        planet, &MOX::player[0]);
    AIDATA::planet_valuation_rules.food.cybernetic = 100;
    int32_t configured_cybernetic_worth =
        AIDATA::Uncolonized_Planet_Worth_To_Player(AI_World_(), planet, &MOX::player[0]);
    DOCTEST_CHECK(configured_cybernetic_worth > cybernetic_worth);
}

DOCTEST_TEST_CASE("uncolonized planet special worth saturates without signed overflow") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_planet_data* planet = &MOX::planet[0];
    planet->planet_type = PLANET_TYPE_PLANET;
    planet->size = PLANET_SIZE_HUGE;
    planet->climate = PLANET_CLIMATE_GAIA;
    planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
    planet->mineral_class = MINERAL_RESOURCE_ULTRA_RICH;
    planet->planet_special = SYSTEM_SPECIAL_GEM_DEPOSITS;
    COLCALC::minerals_per_mine[MINERAL_RESOURCE_ULTRA_RICH] = UINT8_MAX;
    AIDATA::planet_valuation_rules.special.gem_deposits = INT32_MAX;

    DOCTEST_CHECK(AIDATA::Uncolonized_Planet_Worth_To_Player(
        AI_World_(),
        planet, &MOX::player[0]) == UINT16_MAX);
}

DOCTEST_TEST_CASE("classic starvation preserves the final population") {
    s_colony* colony = Prepare_Starving_Colony_();

    COLCALC::Apply_Colony_Pop_Growth(colony);

    DOCTEST_CHECK(colony->n_pops == 1);
}

DOCTEST_TEST_CASE("configured extinction allows a human colony to starve out") {
    s_colony* colony = Prepare_Starving_Colony_();
    colony->pop_growth[0] = -2000;
    MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    COLCALC::planet_extinction = 1;

    COLCALC::Apply_Colony_Pop_Growth(colony);

    DOCTEST_CHECK(colony->n_pops == 0);
}

DOCTEST_TEST_CASE("configured extinction preserves the final AI population") {
    s_colony* colony = Prepare_Starving_Colony_();
    COLCALC::planet_extinction = 1;

    COLCALC::Apply_Colony_Pop_Growth(colony);

    DOCTEST_CHECK(colony->n_pops == 1);
}

DOCTEST_TEST_CASE("classic zero colony survives while settlers are inbound") {
    s_colony* colony = Prepare_Zero_Colony_With_Inbound_Settlers_();

    COLCALC::Ghost_Colony_Stuff(colony);

    DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Destroy_Colony_Reason_() == -1);
}

DOCTEST_TEST_CASE("configured zero colony is lost despite inbound settlers") {
    s_colony* colony = Prepare_Zero_Colony_With_Inbound_Settlers_();
    COLCALC::zero_colony_lost = 1;

    COLCALC::Ghost_Colony_Stuff(colony);

    DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Destroy_Colony_Reason_() == 3);
}

DOCTEST_TEST_CASE("zero research field is not advanced") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_player player = {};
    player.tech_fields[TECH_FIELD_STARTING_TECH] = 2;

    COLCALC::Advance_Player_Field(&player, TECH_FIELD_STARTING_TECH, false);

    DOCTEST_CHECK(player.tech_fields[TECH_FIELD_STARTING_TECH] == 2);
}

DOCTEST_TEST_CASE("completed fields use independent ship-design rebuild policies") {
    struct s_case {
        int16_t field_id;
        bool rebuild;
    };
    const s_case cases[] = {
        {TECH_FIELD_CHEMISTRY, true},
        {TECH_FIELD_NUCLEAR_FISSION, true},
        {TECH_FIELD_PHYSICS, false},
        {TECH_FIELD_COLD_FUSION, true},
    };

    for (const s_case& test_case : cases) {
        TEST_COLONY_POP_GROWTH::Reset_();
        s_player* player = &MOX::player[0];
        player->current_research_application = TECH_APP_LASER_CANNON;
        player->tech_applications[TECH_APP_LASER_CANNON] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        TECHDATA::technology_fields[test_case.field_id].tech[0] =
            TECH_APP_LASER_CANNON;
        TECHDATA::technology_fields[test_case.field_id].next_field_id =
            TECH_FIELD_BIOLOGY;
        DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_PHYSICS] = false;
        DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_COLD_FUSION] = true;

        COLCALC::Give_Player_Field(player, test_case.field_id, 0);

        DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Init_Ship_Designs_Calls_() ==
            static_cast<int>(test_case.rebuild));
    }
}

DOCTEST_TEST_CASE("Capitol eligibility follows the configured technology table") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_player player = {};
    TECHDATA::technology_applications[TECH_APP_CAPITOL].tech_field_id = 1;

    DOCTEST_CHECK(COLCALC::Player_May_Get_Tech_App(
        &player, TECH_APP_CAPITOL));

    player.tech_applications[TECH_APP_CAPITOL] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK_FALSE(COLCALC::Player_May_Get_Tech_App(
        &player, TECH_APP_CAPITOL));
}

DOCTEST_TEST_CASE("Capitol can be built when the player has no homeworld") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;

    s_player* player = &MOX::player[0];
    player->home_planet_id = -1;
    player->tech_applications[TECH_APP_CAPITOL] = TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(colony, BUILDING_CAPITOL) == 1);
}

DOCTEST_TEST_CASE("Spy production requires Spy Network technology") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;

    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, COLONY_PRODUCTION_SPY) == 0);

    MOX::player[0].tech_applications[TECH_APP_SPY_NETWORK] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(
        colony, COLONY_PRODUCTION_SPY) == 1);
}

DOCTEST_TEST_CASE("landing ship costs are selected independently") {
    TEST_COLONY_POP_GROWTH::Reset_();

    COLCALC::lander_ship_cost.colony_ship = 600;
    COLCALC::lander_ship_cost.outpost_ship = 200;
    COLCALC::lander_ship_cost.transport_ship = 80;

    DOCTEST_CHECK(COLCALC::Ship_Type_Cost_For_Player(SHIP_TYPE_COLONY, 0) == 600);
    DOCTEST_CHECK(COLCALC::Ship_Type_Cost_For_Player(SHIP_TYPE_OUTPOST, 0) == 200);
    DOCTEST_CHECK(COLCALC::Ship_Type_Cost_For_Player(SHIP_TYPE_TRANSPORT, 0) == 80);
}

DOCTEST_TEST_CASE("Android production costs are selected independently") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    COLCALC::misc_costs.android_farmer = 30;
    COLCALC::misc_costs.android_worker = 40;
    COLCALC::misc_costs.android_scientist = 60;

    DOCTEST_CHECK(COLCALC::Colony_Product_Cost(colony, COLONY_PRODUCTION_FARMER) == 30);
    DOCTEST_CHECK(COLCALC::Colony_Product_Cost(colony, COLONY_PRODUCTION_WORKER) == 40);
    DOCTEST_CHECK(COLCALC::Colony_Product_Cost(colony, COLONY_PRODUCTION_SCIENTIST) == 60);
}

DOCTEST_TEST_CASE("Spy and terraforming costs use configured unsigned values") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->planet = 0;
    TECHDATA::buildings[BUILDING_TERRAFORMING].cost = 40;
    COLCALC::misc_costs.spy = 120;
    COLCALC::misc_costs.terraforming_step = 250;

    DOCTEST_CHECK(COLCALC::Colony_Product_Cost(
        colony, COLONY_PRODUCTION_SPY) == 120);

    struct s_cost_case {
        uint8_t terraform_count;
        int32_t expected_cost;
    };
    const s_cost_case cases[] = {
        {127, 31790},
        {128, 32040},
        {255, 63790},
    };

    for (const s_cost_case& test_case : cases) {
        MOX::planet[0].n_terraforms = test_case.terraform_count;
        DOCTEST_CHECK(COLCALC::Colony_Product_Cost(
            colony, BUILDING_TERRAFORMING) == test_case.expected_cost);
    }
}

DOCTEST_TEST_CASE("freighter reduction applies only to Feudal governments") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;

    DOCTEST_CHECK(COLCALC::Colony_Product_Cost(
        colony, COLONY_PRODUCTION_FREIGHTERS) == 50);

    COLCALC::misc_costs.freighters = 48;
    for (int government = 0; government < GOVERNMENT_COUNT; ++government) {
        COLCALC::government_bonuses[government].cost_reduction = {1, 3};
    }

    struct s_cost_case {
        int8_t government;
        int32_t enabled_cost;
    };
    const s_cost_case cases[] = {
        {GOVERNMENT_FEUDAL, 32},
        {GOVERNMENT_CONFEDERATION, 32},
        {GOVERNMENT_DICTATORSHIP, 48},
        {GOVERNMENT_IMPERIUM, 48},
        {GOVERNMENT_DEMOCRACY, 48},
        {GOVERNMENT_FEDERATION, 48},
        {GOVERNMENT_UNIFICATION, 48},
        {GOVERNMENT_GALACTIC_UNIFICATION, 48},
    };

    for (const s_cost_case& test_case : cases) {
        MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] =
            test_case.government;

        COLCALC::feudal_affects_freighters = 0;
        DOCTEST_CHECK(COLCALC::Colony_Product_Cost(
            colony, COLONY_PRODUCTION_FREIGHTERS) == 48);

        COLCALC::feudal_affects_freighters = 1;
        DOCTEST_CHECK(COLCALC::Colony_Product_Cost(
            colony, COLONY_PRODUCTION_FREIGHTERS) ==
            test_case.enabled_cost);
    }
}

DOCTEST_TEST_CASE("completed building grants no maintenance cash") {
    s_colony* colony = Prepare_Completed_Product_(BUILDING_MARINE_BARRACKS, 60);
    TECHDATA::buildings[BUILDING_MARINE_BARRACKS].cost = 60;
    TECHDATA::buildings[BUILDING_MARINE_BARRACKS].maintenance = 3;
    MOX::player[0].bc = 20;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK((MOX::player[0].bc == 20));
}

DOCTEST_TEST_CASE("new ship crew is normalized before same-turn combat") {
    struct s_crew_case {
        bool space_academy;
        bool warlord;
        int8_t expected_quality;
        uint16_t expected_experience;
    };
    const s_crew_case cases[] = {
        {false, false, CREW_QUALITY_GREEN, 32768},
        {false, true, CREW_QUALITY_REGULAR, 32768},
        {true, false, CREW_QUALITY_REGULAR, 65535},
        {true, true, CREW_QUALITY_VETERAN, 65535},
    };

    for (const s_crew_case& test_case : cases) {
        s_colony* colony = Prepare_Completed_Ship_(6);
        colony->buildings[BUILDING_SPACE_ACADEMY] = test_case.space_academy;
        MOX::player[0].traits[TRAIT_WARLORD] = test_case.warlord;
        MOX::crew_data[CREW_QUALITY_GREEN].experience = 32768;
        MOX::crew_data[CREW_QUALITY_REGULAR].experience = 65535;

        COLCALC::Apply_Production(colony);

        const s_ship_data& ship = MOX::ship[0];
        DOCTEST_CHECK(ship.crew_quality == test_case.expected_quality);
        DOCTEST_CHECK(ship.crew_experience == test_case.expected_experience);
        DOCTEST_CHECK(ship.just_built == 1);
        DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Calc_Ship_Level_Calls_() == 0);
    }
}

DOCTEST_TEST_CASE("refit completion preserves existing crew state") {
    s_colony* colony = Prepare_Completed_Ship_(4);
    colony->buildings[BUILDING_SPACE_ACADEMY] = 1;
    MOX::player[0].traits[TRAIT_WARLORD] = 1;
    MOX::ship[0].crew_quality = CREW_QUALITY_ELITE;
    MOX::ship[0].crew_experience = UINT16_MAX;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(MOX::ship[0].crew_quality == CREW_QUALITY_ELITE);
    DOCTEST_CHECK(MOX::ship[0].crew_experience == UINT16_MAX);
    DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Calc_Ship_Level_Calls_() == 0);
}

DOCTEST_TEST_CASE("freighter completion widens configured unsigned cash bonus") {
    struct s_bonus_case {
        uint8_t bonus;
        int32_t expected_bc;
    };
    const s_bonus_case cases[] = {
        {0, 20},
        {5, 25},
        {127, 147},
        {128, 148},
        {255, 275},
    };

    for (const s_bonus_case& test_case : cases) {
        s_colony* colony =
            Prepare_Completed_Product_(COLONY_PRODUCTION_FREIGHTERS, 50);
        COLCALC::freighters_cash_bonus = test_case.bonus;
        MOX::player[0].bc = 20;

        COLCALC::Apply_Production(colony);

        DOCTEST_CHECK((MOX::player[0].bc == test_case.expected_bc));
        DOCTEST_CHECK(MOX::player[0].n_freighters == 5);
    }
}

DOCTEST_TEST_CASE("successful spy completion grants no maintenance cash") {
    s_colony* colony = Prepare_Completed_Product_(COLONY_PRODUCTION_SPY, 100);
    TEST_COLONY_POP_GROWTH::Set_Add_Spy_Result_(1);
    MOX::player[0].bc = 20;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK((MOX::player[0].bc == 20));
}

DOCTEST_TEST_CASE("failed spy completion refunds half its production cost") {
    s_colony* colony = Prepare_Completed_Product_(COLONY_PRODUCTION_SPY, 100);
    TEST_COLONY_POP_GROWTH::Set_Add_Spy_Result_(0);
    MOX::player[0].bc = 20;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK((MOX::player[0].bc == 70));
    DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Deleted_Product_() == COLONY_PRODUCTION_SPY);
}

DOCTEST_TEST_CASE("classic production completes at most one queued item") {
    s_colony* colony = Prepare_Multiple_Products_(120);

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(TEST_COLONY_POP_GROWTH::Custom_Autobuild_Calls_() == 0);
    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 1);
    DOCTEST_CHECK(colony->buildings[BUILDING_RESEARCH_LAB] == 0);
    DOCTEST_CHECK((colony->producing[0] == BUILDING_RESEARCH_LAB));
    DOCTEST_CHECK((colony->production_spent == 60));
}

DOCTEST_TEST_CASE("multibuild spends one production pool across queued items") {
    s_colony* colony = Prepare_Multiple_Products_(120);
    COLCALC::multibuild = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 1);
    DOCTEST_CHECK(colony->buildings[BUILDING_RESEARCH_LAB] == 1);
    DOCTEST_CHECK((colony->producing[0] == COLONY_PRODUCTION_NONE));
    DOCTEST_CHECK((colony->production_spent == 10));
}

DOCTEST_TEST_CASE("non-building multibuild stops after a building") {
    s_colony* colony = Prepare_Multiple_Products_(120);
    COLCALC::multibuild = 1;
    COLCALC::multibuild_nonbuilding_products_only = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 1);
    DOCTEST_CHECK(colony->buildings[BUILDING_RESEARCH_LAB] == 0);
    DOCTEST_CHECK((colony->producing[0] == BUILDING_RESEARCH_LAB));
    DOCTEST_CHECK((colony->production_spent == 60));
}

DOCTEST_TEST_CASE("non-building multibuild flag has no effect without multibuild") {
    s_colony* colony = Prepare_Multiple_Products_(120);
    COLCALC::multibuild_nonbuilding_products_only = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 1);
    DOCTEST_CHECK(colony->buildings[BUILDING_RESEARCH_LAB] == 0);
    DOCTEST_CHECK((colony->production_spent == 60));
}

DOCTEST_TEST_CASE("non-building multibuild stops when a building follows freighters") {
    s_colony* colony = Prepare_Multiple_Products_(120);
    colony->producing[0] = COLONY_PRODUCTION_FREIGHTERS;
    COLCALC::misc_costs.freighters = 50;
    COLCALC::multibuild = 1;
    COLCALC::multibuild_nonbuilding_products_only = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(MOX::player[0].n_freighters == 5);
    DOCTEST_CHECK(colony->buildings[BUILDING_RESEARCH_LAB] == 0);
    DOCTEST_CHECK((colony->producing[0] == BUILDING_RESEARCH_LAB));
    DOCTEST_CHECK((colony->production_spent == 70));
}

DOCTEST_TEST_CASE("non-building multibuild completes successive non-building products") {
    s_colony* colony = Prepare_Multiple_Products_(120);
    colony->producing[0] = COLONY_PRODUCTION_FREIGHTERS;
    colony->producing[1] = COLONY_PRODUCTION_SPY;
    COLCALC::misc_costs.freighters = 50;
    COLCALC::misc_costs.spy = 60;
    COLCALC::multibuild = 1;
    COLCALC::multibuild_nonbuilding_products_only = 1;
    TEST_COLONY_POP_GROWTH::Set_Add_Spy_Result_(1);

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(MOX::player[0].n_freighters == 5);
    DOCTEST_CHECK((colony->producing[0] != COLONY_PRODUCTION_SPY));
    DOCTEST_CHECK((colony->production_spent == 10));
}

DOCTEST_TEST_CASE("non-building multibuild stops after a building before freighters") {
    s_colony* colony = Prepare_Multiple_Products_(120);
    colony->producing[1] = COLONY_PRODUCTION_FREIGHTERS;
    COLCALC::multibuild = 1;
    COLCALC::multibuild_nonbuilding_products_only = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 1);
    DOCTEST_CHECK(MOX::player[0].n_freighters == 0);
    DOCTEST_CHECK((colony->producing[0] == COLONY_PRODUCTION_FREIGHTERS));
    DOCTEST_CHECK((colony->production_spent == 60));
}

DOCTEST_TEST_CASE("multibuild preserves production insufficient for the next item") {
    s_colony* colony = Prepare_Multiple_Products_(100);
    COLCALC::multibuild = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 1);
    DOCTEST_CHECK(colony->buildings[BUILDING_RESEARCH_LAB] == 0);
    DOCTEST_CHECK((colony->producing[0] == BUILDING_RESEARCH_LAB));
    DOCTEST_CHECK((colony->production_spent == 40));
}

DOCTEST_TEST_CASE("multibuild banks the entire remainder when reaching Trade Goods") {
    s_colony* colony = Prepare_Multiple_Products_(0);
    colony->producing[1] = COLONY_PRODUCTION_TRADE_GOODS;
    colony->production[ECON_INDUSTRY] = 101;
    colony->maintenance[ECON_INDUSTRY] = 10;
    COLCALC::multibuild = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 1);
    DOCTEST_CHECK((colony->producing[0] == COLONY_PRODUCTION_TRADE_GOODS));
    DOCTEST_CHECK((colony->production_spent == 31));
    DOCTEST_CHECK((colony->production[ECON_INDUSTRY] == 101));
    DOCTEST_CHECK((MOX::player[0].bc == 0));
}

DOCTEST_TEST_CASE("multibuild retains stored and current production at Trade Goods") {
    s_colony* colony = Prepare_Multiple_Products_(120);
    colony->producing[1] = COLONY_PRODUCTION_TRADE_GOODS;
    colony->production[ECON_INDUSTRY] = 100;
    colony->maintenance[ECON_INDUSTRY] = 10;
    COLCALC::multibuild = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 1);
    DOCTEST_CHECK((colony->producing[0] == COLONY_PRODUCTION_TRADE_GOODS));
    DOCTEST_CHECK((colony->production_spent == 150));
    DOCTEST_CHECK((colony->production[ECON_INDUSTRY] == 100));
    DOCTEST_CHECK((MOX::player[0].bc == 0));
}

DOCTEST_TEST_CASE("multibuild banks unused PP from four builds without converting it") {
    TEST_COLONY_POP_GROWTH::Reset_();
    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    colony->n_pops = 1;
    colony->production_spent = 5000;
    colony->production[ECON_INDUSTRY] = 1000;
    COLCALC::misc_costs.freighters = 240;
    for (int i = 0; i < 4; ++i) {
        colony->producing[i] = COLONY_PRODUCTION_FREIGHTERS;
    }
    colony->producing[4] = COLONY_PRODUCTION_TRADE_GOODS;
    COLCALC::multibuild = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(MOX::player[0].n_freighters == 20);
    DOCTEST_CHECK((colony->producing[0] == COLONY_PRODUCTION_TRADE_GOODS));
    DOCTEST_CHECK((colony->production_spent == 5040));
    DOCTEST_CHECK((colony->production[ECON_INDUSTRY] == 1000));
    DOCTEST_CHECK((MOX::player[0].bc == 0));

    COLCALC::Apply_Production(colony);
    DOCTEST_CHECK((colony->production_spent == 5040));
    colony->producing[0] = COLONY_PRODUCTION_FREIGHTERS;
    colony->producing[1] = COLONY_PRODUCTION_NONE;
    colony->production[ECON_INDUSTRY] = 0;
    COLCALC::Apply_Production(colony);
    DOCTEST_CHECK(MOX::player[0].n_freighters == 25);
    DOCTEST_CHECK((colony->production_spent == 4800));
}

DOCTEST_TEST_CASE("multibuild does not bank industry while already producing Trade Goods") {
    TEST_COLONY_POP_GROWTH::Reset_();

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    colony->n_pops = 1;
    colony->producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
    colony->production_spent = 20;
    colony->production[ECON_INDUSTRY] = 100;
    colony->maintenance[ECON_INDUSTRY] = 10;
    COLCALC::multibuild = 1;

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK((colony->production_spent == 20));
    DOCTEST_CHECK((colony->production[ECON_INDUSTRY] == 100));
    DOCTEST_CHECK((MOX::player[0].bc == 0));
    COLCALC::Apply_Production(colony);
    DOCTEST_CHECK((colony->production_spent == 20));
    DOCTEST_CHECK((colony->production[ECON_INDUSTRY] == 100));
    DOCTEST_CHECK((MOX::player[0].bc == 0));

    colony->producing[0] = BUILDING_MARINE_BARRACKS;
    TECHDATA::buildings[BUILDING_MARINE_BARRACKS].cost = 60;
    colony->production[ECON_INDUSTRY] = 10;
    colony->maintenance[ECON_INDUSTRY] = 10;
    COLCALC::Apply_Production(colony);
    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 0);
    DOCTEST_CHECK((colony->production_spent == 20));
}

DOCTEST_TEST_CASE("multibuild does not bank industry with no product or AI Trade Goods") {
    for (int16_t product : {COLONY_PRODUCTION_NONE, COLONY_PRODUCTION_AI_TRADE_GOODS}) {
        TEST_COLONY_POP_GROWTH::Reset_();
        s_colony* colony = &MOX::colony[0];
        colony->owner = 0;
        colony->producing[0] = product;
        colony->production_spent = 20;
        colony->production[ECON_INDUSTRY] = 100;
        COLCALC::multibuild = 1;

        COLCALC::Apply_Production(colony);

        DOCTEST_CHECK((colony->production_spent == 20));
        DOCTEST_CHECK((colony->production[ECON_INDUSTRY] == 100));
        DOCTEST_CHECK((MOX::player[0].bc == 0));
    }
}

DOCTEST_TEST_CASE("legacy autobuild cannot spend production on a Trade Goods income turn") {
    for (int16_t product : {COLONY_PRODUCTION_NONE, COLONY_PRODUCTION_TRADE_GOODS}) {
        s_colony* colony = Prepare_Multiple_Products_(20);
        colony->producing[0] = product;
        colony->producing[1] = COLONY_PRODUCTION_NONE;
        BUILD_QUEUE::autobuild_settings.enabled = 0;
        colony->auto_building = BUILD_QUEUE::AUTOBUILD_LEGACY;
        colony->production[ECON_INDUSTRY] = 100;
        COLCALC::multibuild = 1;
        TEST_COLONY_POP_GROWTH::Set_Legacy_Autobuild_Product_(BUILDING_MARINE_BARRACKS);

        COLCALC::Apply_Production(colony);

        DOCTEST_CHECK((colony->producing[0] == BUILDING_MARINE_BARRACKS));
        DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 0);
        DOCTEST_CHECK((colony->production_spent == 20));

        colony->production[ECON_INDUSTRY] = 0;
        COLCALC::Apply_Production(colony);
        DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 0);
        DOCTEST_CHECK((colony->production_spent == 20));
    }
}

DOCTEST_TEST_CASE("legacy autobuild Trade Goods after a building starts next turn") {
    s_colony* colony = Prepare_Multiple_Products_(0);
    colony->producing[1] = COLONY_PRODUCTION_NONE;
    BUILD_QUEUE::autobuild_settings.enabled = 0;
    colony->auto_building = BUILD_QUEUE::AUTOBUILD_LEGACY;
    colony->production[ECON_INDUSTRY] = 100;
    COLCALC::multibuild = 1;
    TEST_COLONY_POP_GROWTH::Set_Legacy_Autobuild_Product_(COLONY_PRODUCTION_TRADE_GOODS);

    COLCALC::Apply_Production(colony);

    DOCTEST_CHECK(colony->buildings[BUILDING_MARINE_BARRACKS] == 1);
    DOCTEST_CHECK((colony->producing[0] == COLONY_PRODUCTION_TRADE_GOODS));
    DOCTEST_CHECK((colony->production_spent == 40));
    DOCTEST_CHECK((MOX::player[0].bc == 0));

    COLCALC::Apply_Production(colony);
    DOCTEST_CHECK((colony->production_spent == 40));
    DOCTEST_CHECK((MOX::player[0].bc == 0));
}

DOCTEST_TEST_CASE("Format_Quotient writes the fraction without overrunning the buffer") {
    // Regression test for the colony-screen crash: the fraction snprintf was
    // passed one byte more than the buffer had left, tripping the fortified
    // snprintf check. Any value with a non-zero remainder exercises it.
    DOCTEST_CHECK(std::string(COLCALC::Format_Quotient(100, 40)) == "+2.5");
    DOCTEST_CHECK(std::string(COLCALC::Format_Quotient(-100, 40)) == "-2.5");
    DOCTEST_CHECK(std::string(COLCALC::Format_Quotient(1, 3)) == "+0.33333");
    DOCTEST_CHECK(std::string(COLCALC::Format_Quotient(999999999, 40)) == "+24999999.975");
}
