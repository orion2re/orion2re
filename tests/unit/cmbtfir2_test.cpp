#include "pch.h"

#include "doctest/doctest.h"
#include "game/cmbtfir2.h"
#include "game/random.h"

namespace TEST_STASIS_AREA_EFFECTS {
    void Reset_();
    void Set_Special_(int16_t ship_idx, int16_t special_id);
    void Set_In_Stasis_(int16_t ship_idx, bool in_stasis);
    int Final_Fire_Count_();
    int Artemis_Damage_Count_();
    uint16_t Artemis_Inherent_Modifications_();
} // namespace TEST_STASIS_AREA_EFFECTS

DOCTEST_TEST_CASE("stasis disables wide area jammer") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    MOX::combat_data[0].owner = factions::From_Legacy(0);
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_WIDE_AREA_JAMMER);

    int16_t base_bonus = 0;
    DOCTEST_CHECK(CMBTFIR2::Fleet_Defensive_Bonus(game::owner_slot{0}, 1, &base_bonus) == 70);

    TEST_STASIS_AREA_EFFECTS::Set_In_Stasis_(0, true);
    base_bonus = 0;
    DOCTEST_CHECK(CMBTFIR2::Fleet_Defensive_Bonus(game::owner_slot{0}, 1, &base_bonus) == 0);
}

DOCTEST_TEST_CASE("breaking an unused duplicate Stasis Field preserves its target") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& source = MOX::combat_data[0];
    source.weapons[0].weapon_id = WEAPON_STASIS_FIELD;
    source.weapons[0].count = 1;
    source.weapon_hits[0] = 1;
    source.weapon_hits_per_unit[0] = 1;
    source.weapons[1].weapon_id = WEAPON_STASIS_FIELD;
    source.weapons[1].count = 1;
    source.weapon_hits[1] = 1;
    source.weapon_hits_per_unit[1] = 1;
    MOX::combat_data[1].stasis_source_idx = 0;
    COMBAT1::Init_Ship_For_Start_Of_Turn(0);
    DOCTEST_REQUIRE(source.weapons[0].shots_left == 0);
    DOCTEST_REQUIRE(source.weapons[1].shots_left == 1);

    int16_t damage = 1;
    CMBTFIR2::Apply_Internal_Damage(0, &damage, 3, 1, 0);

    DOCTEST_CHECK((MOX::combat_data[1].stasis_source_idx == 0));
    DOCTEST_CHECK(source.weapons[1].shots_left == 0);
}

DOCTEST_TEST_CASE("breaking a committed duplicate Stasis Field releases one target") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& source = MOX::combat_data[0];
    source.weapons[0].weapon_id = WEAPON_STASIS_FIELD;
    source.weapons[0].count = 1;
    source.weapon_hits[0] = 1;
    source.weapon_hits_per_unit[0] = 1;
    source.weapons[1].weapon_id = WEAPON_STASIS_FIELD;
    source.weapons[1].count = 1;
    source.weapon_hits[1] = 1;
    source.weapon_hits_per_unit[1] = 1;
    MOX::combat_data[1].stasis_source_idx = 0;
    COMBAT1::Init_Ship_For_Start_Of_Turn(0);
    DOCTEST_REQUIRE(source.weapons[0].shots_left == 0);

    int16_t damage = 1;
    CMBTFIR2::Apply_Internal_Damage(0, &damage, 3, 0, 0);

    DOCTEST_CHECK((MOX::combat_data[1].stasis_source_idx == COMBAT_SHIP_NONE));
}

DOCTEST_TEST_CASE("breaking duplicate Black Hole Generators follows slot reservation") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    COMBAT1::continuous_bhg = 1;
    s_combat_data& source = MOX::combat_data[0];
    source.weapons[0].weapon_id = WEAPON_BLACK_HOLE_GENERATOR;
    source.weapons[0].count = 1;
    source.weapon_hits[0] = 1;
    source.weapon_hits_per_unit[0] = 1;
    source.weapons[1].weapon_id = WEAPON_BLACK_HOLE_GENERATOR;
    source.weapons[1].count = 1;
    source.weapon_hits[1] = 1;
    source.weapon_hits_per_unit[1] = 1;
    MOX::combat_data[1].black_hole_flag = 1;
    MOX::combat_data[1].black_hole_source_idx = 0;
    COMBAT1::Init_Ship_For_Start_Of_Turn(0);
    DOCTEST_REQUIRE(source.weapons[0].shots_left == 0);
    DOCTEST_REQUIRE(source.weapons[1].shots_left == 1);

    int16_t damage = 1;
    CMBTFIR2::Apply_Internal_Damage(0, &damage, 3, 1, 0);
    DOCTEST_CHECK((MOX::combat_data[1].black_hole_source_idx == 0));
    DOCTEST_CHECK(MOX::combat_data[1].black_hole_flag == 1);

    damage = 1;
    CMBTFIR2::Apply_Internal_Damage(0, &damage, 3, 0, 0);
    DOCTEST_CHECK((MOX::combat_data[1].black_hole_source_idx == COMBAT_SHIP_NONE));
    DOCTEST_CHECK(MOX::combat_data[1].black_hole_flag == 0);
}

DOCTEST_TEST_CASE("onboard missile jammers use priority and stack with inertial defenses") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_ECM_JAMMER);
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_MULTI_WAVE_ECM_JAMMER);
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_WIDE_AREA_JAMMER);
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_INERTIAL_NULLIFIER);
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_INERTIAL_STABILIZER);

    int16_t field_bonus = 0;
    DOCTEST_CHECK(CMBTFIR2::Ship_Specials_Defensive_Bonus(
                      0, 1, &field_bonus) == 205);
    DOCTEST_CHECK(field_bonus == 130);
}

DOCTEST_TEST_CASE("fleet jammer de-duplication retains onboard inertial defense") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    MOX::combat_data[0].owner = factions::From_Legacy(0);
    MOX::combat_data[1].owner = factions::From_Legacy(0);
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_ECM_JAMMER);
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_INERTIAL_NULLIFIER);
    TEST_STASIS_AREA_EFFECTS::Set_Special_(1, SPECIAL_WIDE_AREA_JAMMER);

    DOCTEST_CHECK(CMBTFIR2::Defensive_Combat_Bonus(0, 1) == 120);
}

DOCTEST_TEST_CASE("planetary defenses optionally receive racial defense") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    MOX::combat_data[0].owner = factions::From_Legacy(0);
    MOX::combat_data[0].ship_id = game::NO_SHIP;
    game::current_state.players[0].traits[TRAIT_SHIP_DEFENSE] = 20;
    CMBTFIR2::satellite_racial_defense_bonus = false;

    DOCTEST_CHECK(CMBTFIR2::Defensive_Combat_Bonus(0, 1) == 0);

    CMBTFIR2::satellite_racial_defense_bonus = true;
    DOCTEST_CHECK(CMBTFIR2::Defensive_Combat_Bonus(0, 1) == 20);
    CMBTFIR2::satellite_racial_defense_bonus = false;
}

DOCTEST_TEST_CASE("stasis disables satellite attack bonus") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& station = MOX::combat_data[1];
    station.owner = factions::From_Legacy(0);
    station.ship_id = game::NO_SHIP;
    station.ftl_type = DRIVE_NO_DRIVE;
    station.size_class = SHIP_SIZE_HUGE;

    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{0}) == 10);

    TEST_STASIS_AREA_EFFECTS::Set_In_Stasis_(1, true);
    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{0}) == 0);
}

DOCTEST_TEST_CASE("Star Base fleet attack bonus is configurable and disabled by stasis") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& station = MOX::combat_data[1];
    station.owner = factions::From_Legacy(0);
    station.ship_id = game::NO_SHIP;
    station.ftl_type = DRIVE_NO_DRIVE;
    station.size_class = SHIP_SIZE_LARGE;

    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{0}) == 0);

    CMBTFIR2::combat_bonus.starbase_fleet_beam_offense = 17;
    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{0}) == 17);

    TEST_STASIS_AREA_EFFECTS::Set_In_Stasis_(1, true);
    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{0}) == 0);
}

DOCTEST_TEST_CASE("Battle Scanner tactical offense uses the configured bonus") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_BATTLE_SCANNER);

    DOCTEST_CHECK(CMBTFIR2::Ship_Specials_Offensive_Bonus(0) == 50);

    CMBTFIR2::combat_bonus.battle_scanner_beam_offense = 37;
    DOCTEST_CHECK(CMBTFIR2::Ship_Specials_Offensive_Bonus(0) == 37);

    CMBTFIR2::combat_bonus.battle_scanner_beam_offense = 50;
}

DOCTEST_TEST_CASE("ordinary no-drive ships do not receive satellite attack bonuses") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& ship = MOX::combat_data[1];
    ship.owner = factions::From_Legacy(0);
    ship.ship_id = game::id_ship{static_cast<uint32_t>(4)};
    ship.ftl_type = DRIVE_NO_DRIVE;
    ship.size_class = SHIP_SIZE_HUGE;

    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{0}) == 0);
}

DOCTEST_TEST_CASE("generated no-drive Antaran ships do not receive satellite attack bonuses") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& ship = MOX::combat_data[1];
    ship.owner = factions::From_Legacy(MAX_PLAYERS);
    ship.previous_owner = factions::From_Legacy(MAX_PLAYERS);
    ship.ship_id = game::NO_SHIP;
    ship.officer_id = game::NO_LEADER;
    ship.ftl_type = DRIVE_NO_DRIVE;
    ship.size_class = SHIP_SIZE_HUGE;

    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{MAX_PLAYERS}) == 0);

    TEST_STASIS_AREA_EFFECTS::Set_In_Stasis_(1, true);
    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{MAX_PLAYERS}) == 0);
}

DOCTEST_TEST_CASE("satellite attack bonuses remain tied to station tier") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& station = MOX::combat_data[1];
    station.owner = factions::From_Legacy(0);
    station.ship_id = game::NO_SHIP;
    station.ftl_type = DRIVE_NO_DRIVE;
    station.size_class = SHIP_SIZE_TITAN;
    COMBINIT::satellite_size_class_plus_one = 1;

    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{0}) == 10);

    COMBINIT::satellite_size_class_plus_one = 0;
}

DOCTEST_TEST_CASE("stasis disables scout lab") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    MOX::attacking_faction = factions::From_Legacy(9);
    MOX::combat_data[0].owner = factions::From_Legacy(0);
    MOX::combat_data[0].ftl_type = 1;
    TEST_STASIS_AREA_EFFECTS::Set_Special_(0, SPECIAL_SCOUT_LAB);

    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{0}) == 20);

    TEST_STASIS_AREA_EFFECTS::Set_In_Stasis_(0, true);
    DOCTEST_CHECK(CMBTFIR2::Fleet_Offensive_Bonus(game::owner_slot{0}) == 0);
}

DOCTEST_TEST_CASE("final fire selects missile defenses by weapon identity and active state") {
    struct s_final_fire_case {
        const char* description;
        int16_t weapon_id;
        bool point_defense;
        int8_t active;
        bool is_seeking;
        int expected_fire_count;
    };

    const s_final_fire_case test_cases[] = {
        {"enabled point-defense beam", WEAPON_LASER_CANNON, true, 1, false, 1},
        {"zero-active point-defense beam", WEAPON_LASER_CANNON, true, 0, false, 1},
        {"disabled point-defense beam outside seeking", WEAPON_LASER_CANNON, true, -1, false, 0},
        {"disabled point-defense beam during seeking", WEAPON_LASER_CANNON, true, -1, true, 1},
        {"enabled ordinary beam", WEAPON_LASER_CANNON, false, 1, false, 0},
        {"disabled ordinary beam during seeking", WEAPON_LASER_CANNON, false, -1, true, 0},
        {"enabled Anti-Missile Rocket", WEAPON_ANTI_MISSILE_ROCKET, false, 1, false, 1},
        {"disabled Anti-Missile Rocket outside seeking", WEAPON_ANTI_MISSILE_ROCKET, false, -1, false, 0},
        {"disabled Anti-Missile Rocket during seeking", WEAPON_ANTI_MISSILE_ROCKET, false, -1, true, 1},
        {"enabled Spatial Compressor", WEAPON_SPATIAL_COMPRESSOR, false, 1, false, 1},
        {"disabled Spatial Compressor outside seeking", WEAPON_SPATIAL_COMPRESSOR, false, -1, false, 0},
        {"disabled Spatial Compressor during seeking", WEAPON_SPATIAL_COMPRESSOR, false, -1, true, 0},
        {"enabled Pulsar", WEAPON_PULSAR, false, 1, false, 0},
        {"disabled Pulsar during seeking", WEAPON_PULSAR, false, -1, true, 0},
        {"enabled Black Hole Generator", WEAPON_BLACK_HOLE_GENERATOR, false, 1, false, 0},
        {"disabled Black Hole Generator during seeking", WEAPON_BLACK_HOLE_GENERATOR, false, -1, true, 0},
    };

    for (const s_final_fire_case& test_case : test_cases) {
        DOCTEST_CAPTURE(test_case.description);
        TEST_STASIS_AREA_EFFECTS::Reset_();

        const int8_t initial_active[8] = {-1, 0, 1, -1, 1, 0, -1, 1};
        for (int i = 0; i < 8; ++i) {
            MOX::combat_data[0].weapons[i].active = initial_active[i];
        }

        s_combat_weapon_slot& weapon = MOX::combat_data[0].weapons[0];
        weapon.weapon_id = test_case.weapon_id;
        weapon.specials = test_case.point_defense ? WEAPON_MOD_FLAG_POINT_DEFENSE : 0;
        weapon.active = test_case.active;
        TECHDATA::weapons[test_case.weapon_id].type =
            test_case.point_defense || test_case.weapon_id == WEAPON_LASER_CANNON
            ? WEAPON_TYPE_BEAM
            : WEAPON_TYPE_SPECIAL;

        CMBTFIR2::Final_Fire(0, 7, 9, 0, test_case.is_seeking);

        DOCTEST_CHECK(
            TEST_STASIS_AREA_EFFECTS::Final_Fire_Count_() ==
            test_case.expected_fire_count);
        DOCTEST_CHECK(weapon.active == test_case.active);
        for (int i = 1; i < 8; ++i) {
            DOCTEST_CHECK(MOX::combat_data[0].weapons[i].active == initial_active[i]);
        }
    }
}

DOCTEST_TEST_CASE("artemis restores the network turn random seed before rolling") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    MOX::game_type = 3;
    MOX::qcombat_data[1].size = 0;
    MOX::qcombat_data[1].status = 1;
    CMBTFIR2::artemis_system_net.trigger_chance[0] = 100;

    constexpr uint32_t synchronized_seed = 0x13572468u;
    game_random::Set_Random_Seed(synchronized_seed);
    CMBTFIR2::Save_Artemis_Random_Seed();

    game_random::Set_Random_Seed(synchronized_seed);
    game_random::Random(100);
    game_random::Random(201);
    const uint32_t expected_seed = game_random::Get_Random_Seed();

    game_random::Set_Random_Seed(synchronized_seed);
    game_random::Random(5);
    game_random::Random(5);

    CMBTFIR2::Apply_Combat_Artemis_Net(1);

    DOCTEST_CHECK(game_random::Get_Random_Seed() == expected_seed);

    CMBTFIR2::artemis_system_net.trigger_chance[0] = 20;
}

DOCTEST_TEST_CASE("artemis widens inherent natural properties without sign extension") {
    const uint8_t inherent_values[] = {1, 127, 128, 255};

    for (uint8_t inherent_value : inherent_values) {
        TEST_STASIS_AREA_EFFECTS::Reset_();
        MOX::combat_data[1].size_class = SHIP_SIZE_SMALL;
        CMBTFIR2::artemis_system_net.random_total_damage = 1;
        CMBTFIR2::artemis_system_net.minimum_total_damage = 0;
        CMBTFIR2::artemis_system_net.counter = 1;
        CMBTFIR2::artemis_system_net.inherent_modifications = inherent_value;
        CMBTFIR2::artemis_system_net.trigger_chance[SHIP_SIZE_SMALL] = 100;

        CMBTFIR2::Apply_Combat_Artemis_Net(0);

        DOCTEST_CHECK(TEST_STASIS_AREA_EFFECTS::Artemis_Damage_Count_() == 1);
        DOCTEST_CHECK(
            TEST_STASIS_AREA_EFFECTS::Artemis_Inherent_Modifications_() ==
            static_cast<uint16_t>(inherent_value));
    }
}

DOCTEST_TEST_CASE("artemis damage pool advances by the configured counter") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    MOX::combat_data[1].size_class = SHIP_SIZE_SMALL;
    CMBTFIR2::artemis_system_net.random_total_damage = 1;
    CMBTFIR2::artemis_system_net.minimum_total_damage = 4;
    CMBTFIR2::artemis_system_net.counter = 2;
    CMBTFIR2::artemis_system_net.trigger_chance[SHIP_SIZE_SMALL] = 100;

    CMBTFIR2::Apply_Combat_Artemis_Net(0);

    DOCTEST_CHECK(TEST_STASIS_AREA_EFFECTS::Artemis_Damage_Count_() == 3);
}
