#include "pch.h"

#include "doctest/doctest.h"
#include "game/ericnet.h"

namespace test::fakes {
    extern int16_t instructor_bonuses[MAX_LEADERS][2];
    extern int32_t last_played_sound;
    extern int32_t environmental_refresh_count;
    extern int32_t colony_can_build_product_count;
    extern s_colony* last_environmental_colony;
}

namespace {
    const int16_t default_reengage_buildings[] = {
        BUILDING_ARTEMIS_SYSTEM_NET,
        BUILDING_BATTLESTATION,
        BUILDING_MISSILE_BASE,
        BUILDING_GROUND_BATTERIES,
        BUILDING_STAR_BASE,
        BUILDING_STAR_FORTRESS,
        BUILDING_STELLAR_CONVERTER,
        BUILDING_FIGHTER_GARRISON,
    };

    const int16_t planetary_shields[] = {
        BUILDING_RADIATION_SHIELD,
        BUILDING_FLUX_SHIELD,
        BUILDING_BARRIER_SHIELD,
    };

    void Reset_Colony_() {
        memset(MOX::colony, 0, sizeof(s_colony) * 2);
        memset(MOX::planet, 0, sizeof(s_planet_data) * 2);
        memset(MOX::star, 0, sizeof(s_star_data));
        memset(MOX::ship, 0, sizeof(s_ship_data) * 8);
        memset(MOX::player, 0, sizeof(s_player) * MAX_PLAYERS);
        MOX::NUM_COLONIES = 0;
        MOX::NUM_SHIPS = 0;
        MOX::colony[0].planet = 0;
        MOX::planet[0].star_index = 0;
        test::fakes::environmental_refresh_count = 0;
        test::fakes::colony_can_build_product_count = 0;
        test::fakes::last_environmental_colony = nullptr;
    }

    void Reset_Homeworld_(uint8_t civilization_level) {
        Reset_Colony_();
        MOX::settings = {};
        MOX::settings.starting_civilization_level = civilization_level;
        MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_UNIFICATION;
        MOX::player[0].ship_designs[0].ship_type = SHIP_TYPE_COMBAT;
        ERICNET::starting_ships = 1;
    }

    void Restore_Default_Reengage_Buildings_() {
        memset(ERICNET::combat_reengage_buildings, BUILDING_NO_BUILDING,
               sizeof(ERICNET::combat_reengage_buildings));
        for (size_t index = 0;
             index < sizeof(default_reengage_buildings) / sizeof(default_reengage_buildings[0]);
             ++index) {
            ERICNET::combat_reengage_buildings[index] =
                static_cast<uint8_t>(default_reengage_buildings[index]);
        }
    }
}

DOCTEST_TEST_CASE("gameplay sound playback follows settings and combat speed") {
    MOX::settings.sound_fx_on = 1;
    MOX::super_fast_flag = 0;

    test::fakes::last_played_sound = -1;
    DOCTEST_CHECK(ERICNET::Mox_Play_Sound(68) == 68);
    DOCTEST_CHECK(test::fakes::last_played_sound == 68);

    test::fakes::last_played_sound = -1;
    DOCTEST_CHECK(ERICNET::Mox_Play_Sound(69) == 69);
    DOCTEST_CHECK(test::fakes::last_played_sound == 69);

    MOX::super_fast_flag = 1;
    DOCTEST_CHECK(ERICNET::Mox_Play_Sound(33) == 0);
    DOCTEST_CHECK(test::fakes::last_played_sound == 69);

    MOX::super_fast_flag = 0;
    MOX::settings.sound_fx_on = 0;
    DOCTEST_CHECK(ERICNET::Mox_Play_Sound(33) == 0);
    DOCTEST_CHECK(test::fakes::last_played_sound == 69);
}

DOCTEST_TEST_CASE("starting Outpost Ships follow the configured civilization levels") {
    Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_AVERAGE);
    ERICNET::starting_outpost_ship = 0;
    ERICNET::Init_Homeworld_Colony(0, 0);
    DOCTEST_CHECK(MOX::NUM_SHIPS == 3);
    DOCTEST_CHECK(MOX::ship[2].d.ship_type == SHIP_TYPE_COLONY);

    Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_AVERAGE);
    ERICNET::starting_outpost_ship = 1;
    ERICNET::Init_Homeworld_Colony(0, 0);
    DOCTEST_CHECK(MOX::NUM_SHIPS == 4);
    DOCTEST_CHECK(MOX::ship[2].d.ship_type == SHIP_TYPE_OUTPOST);
    DOCTEST_CHECK(MOX::ship[3].d.ship_type == SHIP_TYPE_COLONY);

    Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_POST_WARP);
    ERICNET::Init_Homeworld_Colony(0, 0);
    DOCTEST_CHECK(MOX::NUM_SHIPS == 4);

    Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_ADVANCED);
    ERICNET::Init_Homeworld_Colony(0, 0);
    DOCTEST_CHECK(MOX::NUM_SHIPS == 0);

    ERICNET::starting_outpost_ship = 0;
}

DOCTEST_TEST_CASE("starting Outpost Ships are allocated for every player") {
    Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_AVERAGE);
    ERICNET::starting_outpost_ship = 1;
    MOX::player[1].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_UNIFICATION;
    MOX::player[1].ship_designs[0].ship_type = SHIP_TYPE_COMBAT;
    MOX::planet[1].star_index = 0;

    ERICNET::Init_Homeworld_Colony(0, 0);
    ERICNET::Init_Homeworld_Colony(1, 1);

    DOCTEST_REQUIRE(MOX::NUM_SHIPS == 8);
    for (int16_t ship_idx = 0; ship_idx < 4; ++ship_idx) {
        DOCTEST_CHECK(MOX::ship[ship_idx].owner == 0);
    }
    for (int16_t ship_idx = 4; ship_idx < 8; ++ship_idx) {
        DOCTEST_CHECK(MOX::ship[ship_idx].owner == 1);
    }
    DOCTEST_CHECK(MOX::ship[6].d.ship_type == SHIP_TYPE_OUTPOST);
    DOCTEST_CHECK(MOX::ship[7].d.ship_type == SHIP_TYPE_COLONY);

    ERICNET::starting_outpost_ship = 0;
}

DOCTEST_TEST_CASE("starting ships can be disabled without changing civilization eligibility") {
    Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_AVERAGE);
    ERICNET::starting_ships = 0;
    ERICNET::Init_Homeworld_Colony(0, 0);
    DOCTEST_CHECK(MOX::NUM_SHIPS == 0);

    Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_POST_WARP);
    ERICNET::starting_ships = 0;
    ERICNET::Init_Homeworld_Colony(0, 0);
    DOCTEST_CHECK(MOX::NUM_SHIPS == 0);

    ERICNET::starting_ships = 1;
}

DOCTEST_TEST_CASE("configured starting population stays within colony storage") {
    const INITGAME::s_start saved = INITGAME::start;
    const uint8_t populations[] = {0, 42};

    for (uint8_t population : populations) {
        Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_AVERAGE);
        ERICNET::starting_ships = 0;
        INITGAME::start.population = population;
        MOX::colony[0].pop_roundoff[0] = 0;

        ERICNET::Init_Homeworld_Colony(0, 0);

        DOCTEST_CHECK((uint8_t)MOX::colony[0].n_pops == population);
        for (uint8_t index = 0; index < population; ++index) {
            DOCTEST_CHECK(MOX::colony[0].pop[index] != 0);
        }
        DOCTEST_CHECK(MOX::colony[0].pop_roundoff[0] == 0);
    }

    INITGAME::start = saved;
    ERICNET::starting_ships = 1;
}

DOCTEST_TEST_CASE("configured starting marines use the complete byte domain") {
    const INITGAME::s_start saved = INITGAME::start;
    const uint8_t marines[] = {0, 255};

    for (uint8_t marine_count : marines) {
        Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_AVERAGE);
        ERICNET::starting_ships = 0;
        INITGAME::start.marines = marine_count;

        ERICNET::Init_Homeworld_Colony(0, 0);

        DOCTEST_CHECK(MOX::colony[0].military[0] == marine_count);
    }

    INITGAME::start = saved;
    ERICNET::starting_ships = 1;
}

DOCTEST_TEST_CASE("Advanced civilization derives population independently of configuration") {
    const INITGAME::s_start saved = INITGAME::start;
    const uint8_t populations[] = {0, 42};

    for (uint8_t population : populations) {
        Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_ADVANCED);
        INITGAME::start.population = population;

        ERICNET::Init_Homeworld_Colony(0, 0);

        DOCTEST_CHECK((uint8_t)MOX::colony[0].n_pops == 8);
    }

    INITGAME::start = saved;
}

DOCTEST_TEST_CASE("crew turn updates use the best active colony instructor for each owner") {
    struct s_crew_scope {
        std::vector<s_ship_data> saved_ships{MAX_SHIPS};
        std::vector<s_leader_data> saved_leaders{MAX_LEADERS};
        decltype(test::fakes::instructor_bonuses) saved_bonuses;
        uint8_t saved_turn_xp = ERICNET::crew_gains_turn_xp;
        uint16_t saved_cap = MOX::crew_data[CREW_QUALITY_ELITE].experience;

        s_crew_scope() {
            memcpy(saved_ships.data(), MOX::ship, sizeof(s_ship_data) * MAX_SHIPS);
            memcpy(saved_leaders.data(), MOX::leaders, sizeof(s_leader_data) * MAX_LEADERS);
            memcpy(saved_bonuses, test::fakes::instructor_bonuses, sizeof(saved_bonuses));
        }

        ~s_crew_scope() {
            memcpy(MOX::ship, saved_ships.data(), sizeof(s_ship_data) * MAX_SHIPS);
            memcpy(MOX::leaders, saved_leaders.data(), sizeof(s_leader_data) * MAX_LEADERS);
            memcpy(test::fakes::instructor_bonuses, saved_bonuses, sizeof(saved_bonuses));
            ERICNET::crew_gains_turn_xp = saved_turn_xp;
            MOX::crew_data[CREW_QUALITY_ELITE].experience = saved_cap;
        }
    } crew_scope;
    Reset_Colony_();
    memset(MOX::leaders, 0, sizeof(s_leader_data) * MAX_LEADERS);
    memset(test::fakes::instructor_bonuses, 0, sizeof(test::fakes::instructor_bonuses));
    for (int i = 0; i < MAX_LEADERS; ++i) MOX::leaders[i].player_index = -1;
    for (int i = 0; i < 6; ++i) {
        MOX::leaders[i].player_index = i < 2 || i == 5 ? 0 : 1;
        MOX::leaders[i].type = LEADER_TYPE_COLONY;
    }
    test::fakes::instructor_bonuses[0][0] = 3;
    test::fakes::instructor_bonuses[1][1] = 5;
    test::fakes::instructor_bonuses[2][0] = 7;
    MOX::leaders[2].eta = 1;
    test::fakes::instructor_bonuses[3][0] = 9;
    MOX::leaders[3].type = LEADER_TYPE_SHIP;
    test::fakes::instructor_bonuses[4][0] = 2;
    test::fakes::instructor_bonuses[5][0] = 99;
    MOX::leaders[5].status = -1;
    int16_t bonuses[MAX_PLAYERS] = {};
    ERICNET::Best_Instructor_Bonus(bonuses);
    DOCTEST_CHECK(bonuses[0] == 5);
    DOCTEST_CHECK(bonuses[1] == 2);

    for (int i = 0; i < MAX_SHIPS; ++i) MOX::ship[i].status = 5;
    for (int i = 0; i < 4; ++i) MOX::ship[i].crew_experience = 10;
    MOX::ship[0].status = 0;
    MOX::ship[0].owner = 0;
    MOX::ship[1].status = 0;
    MOX::ship[1].owner = 1;
    MOX::ship[2].owner = 0;
    MOX::ship[3].status = 0;
    MOX::ship[3].owner = MAX_PLAYERS;
    ERICNET::crew_gains_turn_xp = 1;
    MOX::crew_data[CREW_QUALITY_ELITE].experience = 1000;
    ERICNET::Do_All_Ships_XP_Check();
    DOCTEST_CHECK((MOX::ship[0].crew_experience == 16));
    DOCTEST_CHECK((MOX::ship[1].crew_experience == 13));
    DOCTEST_CHECK((MOX::ship[2].crew_experience == 10));
    DOCTEST_CHECK((MOX::ship[3].crew_experience == 10));
}

DOCTEST_TEST_CASE("crew turn experience can be disabled without suppressing other gains") {
    Reset_Colony_();
    MOX::ship[0].crew_experience = 10;
    ERICNET::crew_gains_turn_xp = 1;
    ERICNET::Do_XP_For_Ship(0, 2);
    DOCTEST_CHECK(MOX::ship[0].crew_experience == 13);

    MOX::ship[0].crew_experience = 10;
    ERICNET::crew_gains_turn_xp = 0;
    ERICNET::Do_XP_For_Ship(0, 2);
    DOCTEST_CHECK(MOX::ship[0].crew_experience == 12);

    ERICNET::crew_gains_turn_xp = 1;
}

DOCTEST_TEST_CASE("crew progression preserves the full unsigned experience domain") {
    Reset_Colony_();
    MOX::crew_data[CREW_QUALITY_GREEN].experience = 0;
    MOX::crew_data[CREW_QUALITY_REGULAR].experience = 32768;
    MOX::crew_data[CREW_QUALITY_VETERAN].experience = 40000;
    MOX::crew_data[CREW_QUALITY_ELITE].experience = UINT16_MAX;
    MOX::crew_data[CREW_QUALITY_ULTRA_ELITE].experience = UINT16_MAX;
    MOX::ship[0].crew_experience = 32768;

    ERICNET::Calc_Ship_Level(0);
    DOCTEST_CHECK(MOX::ship[0].crew_quality == CREW_QUALITY_REGULAR);

    ERICNET::crew_gains_turn_xp = 1;
    MOX::ship[0].crew_experience = UINT16_MAX;
    ERICNET::Do_XP_For_Ship(0, 2);
    DOCTEST_CHECK(MOX::ship[0].crew_experience == UINT16_MAX);
}

DOCTEST_TEST_CASE("Warlord promotion stays within the crew quality table") {
    Reset_Colony_();
    MOX::crew_data[CREW_QUALITY_GREEN].experience = 0;
    MOX::crew_data[CREW_QUALITY_REGULAR].experience = 50;
    MOX::crew_data[CREW_QUALITY_VETERAN].experience = 150;
    MOX::crew_data[CREW_QUALITY_ELITE].experience = 500;
    MOX::crew_data[CREW_QUALITY_ULTRA_ELITE].experience = 501;
    MOX::ship[0].owner = 0;
    MOX::ship[0].crew_experience = 500;
    MOX::player[0].traits[TRAIT_WARLORD] = 1;

    ERICNET::Calc_Ship_Level(0);

    DOCTEST_CHECK(MOX::ship[0].crew_quality == CREW_QUALITY_ULTRA_ELITE);
}

DOCTEST_TEST_CASE("initial building priority stops at none and stays within its capacity") {
    int16_t saved[ERICNET::INITIAL_BUILDING_CAPACITY];
    memcpy(saved, ERICNET::buildlist, sizeof(saved));

    for (int index = 0; index < ERICNET::INITIAL_BUILDING_CAPACITY; ++index) {
        ERICNET::buildlist[index] = BUILDING_AUTOMATED_FACTORY;
    }
    ERICNET::buildlist[2] = BUILDING_NO_BUILDING;
    Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_ADVANCED);
    ERICNET::Init_Homeworld_Colony(0, 0);
    DOCTEST_CHECK(test::fakes::colony_can_build_product_count == 2);

    for (int index = 0; index < ERICNET::INITIAL_BUILDING_CAPACITY; ++index) {
        ERICNET::buildlist[index] = BUILDING_AUTOMATED_FACTORY;
    }
    Reset_Homeworld_(STARTING_CIVILIZATION_LEVEL_ADVANCED);
    ERICNET::Init_Homeworld_Colony(0, 0);
    DOCTEST_CHECK(test::fakes::colony_can_build_product_count ==
                  ERICNET::INITIAL_BUILDING_CAPACITY);

    memcpy(ERICNET::buildlist, saved, sizeof(saved));
}

DOCTEST_TEST_CASE("Artificial Planets use configured population capacity") {
    const COLCALC::s_population_capacity saved = COLCALC::population_capacity;

    struct s_case {
        int8_t source_type;
        int8_t expected_size;
        uint8_t capacity;
    };
    const s_case cases[] = {
        {PLANET_TYPE_GAS_GIANT, PLANET_SIZE_HUGE, 255},
        {PLANET_TYPE_ASTEROID, PLANET_SIZE_LARGE, 128},
    };

    for (const s_case& test_case : cases) {
        Reset_Colony_();
        MOX::planet[1] = {};
        for (int orbit = 0; orbit < 5; ++orbit) {
            MOX::star[0].planet_index[orbit] = -1;
        }
        MOX::star[0].planet_index[0] = 1;
        MOX::planet[1].planet_type = test_case.source_type;
        COLCALC::population_capacity.size[test_case.expected_size] =
            test_case.capacity;

        ERICNET::Add_Building(0, BUILDING_ARTIFICIAL_PLANET);

        DOCTEST_CHECK(MOX::planet[1].planet_type == PLANET_TYPE_PLANET);
        DOCTEST_CHECK(MOX::planet[1].size == test_case.expected_size);
        DOCTEST_CHECK(MOX::planet[1].max_population == test_case.capacity);
    }

    COLCALC::population_capacity = saved;
}

DOCTEST_TEST_SUITE("building combat re-engagement") {

DOCTEST_TEST_CASE("adding an environmental building refreshes the affected colony") {
    Reset_Colony_();
    MOX::colony[0].climate = PLANET_CLIMATE_ARID;
    MOX::planet[0].climate = PLANET_CLIMATE_ARID;

    ERICNET::Add_Building(0, BUILDING_TERRAFORMING);

    DOCTEST_CHECK(MOX::planet[0].climate == PLANET_CLIMATE_TERRAN);
    DOCTEST_CHECK(test::fakes::environmental_refresh_count == 1);
    DOCTEST_CHECK(test::fakes::last_environmental_colony == &MOX::colony[0]);
}

DOCTEST_TEST_CASE("default defenses invalidate ignored combat when added or removed") {
    Restore_Default_Reengage_Buildings_();
    for (int16_t building_id : default_reengage_buildings) {
        Reset_Colony_();
        MOX::star[0].ignore_combat_ships = 0x3F;
        ERICNET::Add_Building(0, building_id);
        DOCTEST_CHECK(MOX::star[0].ignore_combat_ships == 0);

        Reset_Colony_();
        MOX::colony[0].buildings[building_id] = 1;
        MOX::star[0].ignore_combat_ships = 0x3F;
        ERICNET::Remove_Building(0, building_id);
        DOCTEST_CHECK(MOX::star[0].ignore_combat_ships == 0);
    }
}

DOCTEST_TEST_CASE("planetary shields preserve ignored combat by default") {
    Restore_Default_Reengage_Buildings_();
    for (int16_t building_id : planetary_shields) {
        Reset_Colony_();
        MOX::star[0].ignore_combat_ships = 0x3F;
        ERICNET::Add_Building(0, building_id);
        DOCTEST_CHECK(MOX::star[0].ignore_combat_ships == 0x3F);

        Reset_Colony_();
        MOX::colony[0].buildings[building_id] = 1;
        MOX::star[0].ignore_combat_ships = 0x3F;
        ERICNET::Remove_Building(0, building_id);
        DOCTEST_CHECK(MOX::star[0].ignore_combat_ships == 0x3F);
    }
}

DOCTEST_TEST_CASE("configured building list controls combat invalidation") {
    memset(ERICNET::combat_reengage_buildings, BUILDING_NO_BUILDING,
           sizeof(ERICNET::combat_reengage_buildings));
    ERICNET::combat_reengage_buildings[0] = BUILDING_RADIATION_SHIELD;

    Reset_Colony_();
    MOX::star[0].ignore_combat_ships = 0x3F;
    ERICNET::Add_Building(0, BUILDING_RADIATION_SHIELD);
    DOCTEST_CHECK(MOX::star[0].ignore_combat_ships == 0);

    Reset_Colony_();
    MOX::star[0].ignore_combat_ships = 0x3F;
    ERICNET::Add_Building(0, BUILDING_MISSILE_BASE);
    DOCTEST_CHECK(MOX::star[0].ignore_combat_ships == 0x3F);

    Reset_Colony_();
    MOX::colony[0].buildings[BUILDING_RADIATION_SHIELD] = 1;
    MOX::star[0].ignore_combat_ships = 0x3F;
    ERICNET::Remove_Building(0, BUILDING_RADIATION_SHIELD);
    DOCTEST_CHECK(MOX::star[0].ignore_combat_ships == 0);

    Restore_Default_Reengage_Buildings_();
}

}