#include "pch.h"
#include "../fixtures/game_state.h"

namespace {
    struct s_shield_state : test::s_game_state {
        test::s_saved_value<decltype(MOX::combat_data)> combat{MOX::combat_data};
        test::s_saved_value<decltype(MOX::combat_total_ship_count)> count{MOX::combat_total_ship_count};
        test::s_saved_value<decltype(MOX::combat_defender_player)> defender{MOX::combat_defender_player};
        test::s_saved_value<decltype(MOX::in_nebula_flag)> nebula{MOX::in_nebula_flag};
        test::s_saved_array<s_tech_armor_data, ARMOR_COUNT> armor{TECHDATA::armor};
        test::s_saved_array<s_tech_computer_data, COMPUTER_COUNT> computers{TECHDATA::computers};
        test::s_saved_array<s_tech_drive_data, DRIVE_COUNT> drives{TECHDATA::drives};
        test::s_saved_array<s_tech_fuel_data, FUEL_COUNT> fuel{TECHDATA::fuel};
        test::s_saved_array<s_tech_hull_data, HULL_CLASS_COUNT> hulls{TECHDATA::hull_data};
        test::s_saved_array<s_tech_shield_data, SHIELD_COUNT> shields{TECHDATA::shields};
        test::s_saved_array<s_tech_specials_data, SPECIAL_COUNT> specials{TECHDATA::specials};
        test::s_saved_array<s_tech_weapon_data, WEAPON_COUNT> weapons{TECHDATA::weapons};
        test::s_saved_array<s_tech_weapon_mod_data, WEAPON_MOD_COUNT> mods{TECHDATA::weapon_modifications};
        test::s_saved_array<s_tech_app_data, TECH_APP_COUNT> applications{TECHDATA::technology_applications};
        test::s_saved_array<s_tech_field_data, TECH_FIELD_COUNT> fields{TECHDATA::technology_fields};
        test::s_saved_array<uint16_t, HULL_CLASS_COUNT> hull_multipliers{TECHDATA::shield_hull_multiplier};
        test::s_saved_value<decltype(TECHDATA::multi_phased_shields_bonus)> multi{TECHDATA::multi_phased_shields_bonus};
        test::s_saved_value<decltype(TECHDATA::esd_bonus)> esd{TECHDATA::esd_bonus};
        test::s_saved_value<decltype(TECHDATA::hard_shields_damage_reduction)> hard{TECHDATA::hard_shields_damage_reduction};
        test::s_saved_value<decltype(COMBINIT::defenses_allowed_mods)> allowed{COMBINIT::defenses_allowed_mods};
        test::s_saved_value<decltype(COMBINIT::satellite_space_allocation)> allocation{COMBINIT::satellite_space_allocation};
        test::s_saved_value<decltype(COMBINIT::reinforced_hull_multiplier)> reinforced{COMBINIT::reinforced_hull_multiplier};
        test::s_saved_value<decltype(COMBINIT::antaran_fortress_armor_multiplier)> fortress_armor{COMBINIT::antaran_fortress_armor_multiplier};
        test::s_saved_value<decltype(COMBINIT::planet)> planet_options{COMBINIT::planet};
        test::s_saved_value<decltype(COMBINIT::satellite_size_class_plus_one)> satellite_size{COMBINIT::satellite_size_class_plus_one};
        test::s_saved_value<decltype(COMBINIT::satellite_ammo_basis)> satellite_ammo{COMBINIT::satellite_ammo_basis};
        test::s_saved_array<DESIGN::s_weapon_arc_cost_and_space, DESIGN::WEAPON_ARC_MODIFIER_COUNT> arcs{DESIGN::arc_cost_and_space};
        test::s_saved_value<decltype(DESIGN::miniaturization)> miniaturization{DESIGN::miniaturization};
        test::s_saved_array<uint8_t, DESIGN::MINIATURIZATION_TECHNOLOGY_COUNT> miniaturization_categories{DESIGN::miniaturization_categories};
        test::s_saved_value<decltype(DESIGN::megafluxers_tactical_bonus)> megafluxers{DESIGN::megafluxers_tactical_bonus};
    };
}

#include "doctest/doctest.h"
#include "game/aipower.h"
#include "game/combinit.h"
#include "game/design.h"
#include "game/ship_config.h"

namespace {
    const game::s_view* AI_World_() {
        static const game::s_view world{
            .players = MOX::player,
            .settings = &MOX::settings,
        };
        return &world;
    }
}

namespace TEST_SHIELD_TABLE {
    void Reset_();
    s_combat_data* Combat_Ship_(int16_t ship_idx = 0);
    s_colony* Colony_();
    s_current_design* Design_();
    s_player* Player_();
    void Research_Special_(int16_t special_id);
    void Set_Combat_Special_(int16_t special_id);
    void Set_Design_Special_(int16_t special_id);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "Antaran Fortress loader uses configured weapon and special data") {
    TEST_SHIELD_TABLE::Reset_();
    s_combat_data* fortress = TEST_SHIELD_TABLE::Combat_Ship_();

    SHIP_CONFIG::antaran_fortress.primary_weapons[0] = {
        {
            WEAPON_LASER_CANNON,
            WEAPON_FIRING_ARC_FORWARD |
                WEAPON_FIRING_ARC_FORWARD_EXTENDED,
            WEAPON_MOD_FLAG_HEAVY_MOUNT,
        },
        WEAPON_MOD_FLAG_HEAVY_MOUNT,
    };
    SHIP_CONFIG::antaran_fortress.overflow_weapons[0] = {
        WEAPON_MASS_DRIVER,
        WEAPON_FIRING_ARC_ALL_SECTORS,
        WEAPON_MOD_FLAG_POINT_DEFENSE,
    };
    SHIP_CONFIG::antaran_fortress.primary_weapons[1] = {
        {WEAPON_PARTICLE_BEAM, WEAPON_FIRING_ARC_ALL_SECTORS, 0},
        0,
    };
    SHIP_CONFIG::antaran_fortress.primary_weapons[2] = {
        {WEAPON_FUSION_BEAM, WEAPON_FIRING_ARC_FORWARD, 0},
        0,
    };
    SHIP_CONFIG::antaran_fortress.primary_weapons[3] = {
        {WEAPON_ION_PULSE_CANNON, WEAPON_FIRING_ARC_ALL_SECTORS, 0},
        0,
    };
    SHIP_CONFIG::antaran_fortress.overflow_weapons[1] = {
        WEAPON_GAUSS_CANNON,
        WEAPON_FIRING_ARC_REAR,
        WEAPON_MOD_FLAG_AUTO_FIRE,
    };
    SHIP_CONFIG::antaran_fortress.specials[7] = SPECIAL_REGENERATION;

    TECHDATA::hull_data[HULL_STAR_FORTRESS].structure_hp = 100;
    TECHDATA::hull_data[HULL_STAR_FORTRESS].armor_hp = 100;
    TECHDATA::hull_data[HULL_STAR_FORTRESS].marines = 10;
    TECHDATA::armor[ARMOR_XENTRONIUM_ARMOR].ships_bonus = 0;
    COMBINIT::reinforced_hull_multiplier.antaran_fortress = 128;
    COMBINIT::antaran_fortress_armor_multiplier = 255;
    TECHDATA::weapons[WEAPON_LASER_CANNON].size = 10;
    TECHDATA::weapons[WEAPON_PARTICLE_BEAM].size = 1;
    TECHDATA::weapons[WEAPON_MASS_DRIVER].size = 10;
    TECHDATA::weapons[WEAPON_FUSION_BEAM].size = 1;
    TECHDATA::weapons[WEAPON_GAUSS_CANNON].size = 1;
    TECHDATA::weapons[WEAPON_ION_PULSE_CANNON].size = 1;
    COMBINIT::antaran_star_fortress_space = 1500;

    COMBINIT::Load_Antaran_Star_Fortress();

    DOCTEST_CHECK(fortress->weapons[0].weapon_id == WEAPON_LASER_CANNON);
    DOCTEST_CHECK(fortress->weapons[0].arc ==
                  (WEAPON_FIRING_ARC_FORWARD |
                   WEAPON_FIRING_ARC_FORWARD_EXTENDED));
    DOCTEST_CHECK(fortress->weapons[0].specials ==
                  WEAPON_MOD_FLAG_HEAVY_MOUNT);
    DOCTEST_CHECK(fortress->weapons[0].count == 18);
    DOCTEST_CHECK(fortress->weapons[2].weapon_id == WEAPON_MASS_DRIVER);
    DOCTEST_CHECK(fortress->weapons[2].specials ==
                  WEAPON_MOD_FLAG_POINT_DEFENSE);
    DOCTEST_CHECK((fortress->weapons[1].weapon_id == WEAPON_PARTICLE_BEAM));
    DOCTEST_CHECK(fortress->weapons[1].count == 99);
    DOCTEST_CHECK(fortress->weapons[2].count == 88);
    DOCTEST_CHECK((fortress->weapons[3].weapon_id == WEAPON_FUSION_BEAM));
    DOCTEST_CHECK(fortress->weapons[3].arc == WEAPON_FIRING_ARC_FORWARD);
    DOCTEST_CHECK(fortress->weapons[3].count == 99);
    DOCTEST_CHECK(fortress->weapons[4].weapon_id == WEAPON_GAUSS_CANNON);
    DOCTEST_CHECK(fortress->weapons[4].arc == WEAPON_FIRING_ARC_REAR);
    DOCTEST_CHECK(fortress->weapons[4].specials ==
                  WEAPON_MOD_FLAG_AUTO_FIRE);
    DOCTEST_CHECK(fortress->weapons[4].count == 88);
    DOCTEST_CHECK((fortress->weapons[5].weapon_id == WEAPON_ION_PULSE_CANNON));
    DOCTEST_CHECK(fortress->weapons[5].count == 99);
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        fortress->special_device_flags, SPECIAL_REGENERATION));
    DOCTEST_CHECK(fortress->structure_max == 12800);
    DOCTEST_CHECK(fortress->armor_remaining == 25500);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "orbital Reinforced Hull multipliers remain positive above 127") {
    TEST_SHIELD_TABLE::Reset_();
    s_player* defender = TEST_SHIELD_TABLE::Player_();
    s_combat_data* satellite = TEST_SHIELD_TABLE::Combat_Ship_(1);

    TECHDATA::hull_data[HULL_STAR_BASE].structure_hp = 1;
    TECHDATA::hull_data[HULL_STAR_BASE].armor_hp = 1;
    TECHDATA::hull_data[HULL_STAR_BASE].size = 100;
    TECHDATA::armor[ARMOR_NO_ARMOR].ships_bonus = 0;
    TECHDATA::specials[SPECIAL_REINFORCED_HULL].tech_app_id =
        TECH_APP_REINFORCED_HULL;
    defender->tech_applications[TECH_APP_REINFORCED_HULL] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    COMBINIT::starbase_special[0][0] = SPECIAL_REINFORCED_HULL;
    COMBINIT::reinforced_hull_multiplier.regular_satellites = 255;
    TECHDATA::weapons[WEAPON_LASER_CANNON].size = 1;
    TECHDATA::weapons[WEAPON_LASER_CANNON].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].size = 1;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].tech_app_id =
        TECH_APP_NUCLEAR_MISSILE;

    MOX::combat_total_ship_count = 1;
    COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);

    DOCTEST_CHECK(satellite->structure_max == 255);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "satellite priority uses grouped slot 6 and slot 14") {
    TEST_SHIELD_TABLE::Reset_();
    s_combat_data* satellite = TEST_SHIELD_TABLE::Combat_Ship_(1);

    COMBINIT::starbase_special[5][0] = SPECIAL_REFLECTION_FIELD;
    COMBINIT::starbase_special[5][1] = SPECIAL_LIGHTNING_FIELD;
    COMBINIT::starbase_special[5][2] = SPECIAL_STEALTH_FIELD;
    COMBINIT::starbase_special[13][0] = SPECIAL_ENERGY_ABSORBER;

    TEST_SHIELD_TABLE::Research_Special_(SPECIAL_LIGHTNING_FIELD);
    TEST_SHIELD_TABLE::Research_Special_(SPECIAL_STEALTH_FIELD);
    TEST_SHIELD_TABLE::Research_Special_(SPECIAL_ENERGY_ABSORBER);

    COMBINIT::Load_Combat_Satellite_Specials(
        satellite, TEST_SHIELD_TABLE::Player_());

    DOCTEST_CHECK(struct_::Test_Bit_Field(
        satellite->special_device_flags, SPECIAL_LIGHTNING_FIELD));
    DOCTEST_CHECK_FALSE(struct_::Test_Bit_Field(
        satellite->special_device_flags, SPECIAL_STEALTH_FIELD));
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        satellite->special_device_flags, SPECIAL_ENERGY_ABSORBER));
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "heavy satellite beams use all-round weapon pricing") {
    TEST_SHIELD_TABLE::Reset_();
    s_player* defender = TEST_SHIELD_TABLE::Player_();

    TECHDATA::hull_data[HULL_STAR_BASE].size = 400;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].size = 10;
    TECHDATA::weapons[WEAPON_LASER_CANNON].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_LASER_CANNON].available_mods =
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
        WEAPON_MOD_FLAG_ARMOR_PIERCING |
        WEAPON_MOD_FLAG_CONTINUOUS |
        WEAPON_MOD_FLAG_EMISSION_GUIDANCE;

    TECHDATA::technology_applications[TECH_APP_LASER_CANNON].tech_field_id =
        TECH_FIELD_ADVANCED_BIOLOGY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].next_field_id =
        TECH_FIELD_ADVANCED_CHEMISTRY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_CHEMISTRY].next_field_id = 0;
    defender->tech_fields[TECH_FIELD_ADVANCED_CHEMISTRY] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    TECHDATA::weapon_modifications[WEAPON_MOD_ARMOR_PIERCING].level = 1;
    TECHDATA::weapon_modifications[WEAPON_MOD_ARMOR_PIERCING].space = 100;
    TECHDATA::weapon_modifications[WEAPON_MOD_CONTINUOUS].level = 2;
    TECHDATA::weapon_modifications[WEAPON_MOD_EMISSIONS_GUIDANCE].level = 1;
    COMBINIT::defenses_allowed_mods.heavy =
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
        WEAPON_MOD_FLAG_ARMOR_PIERCING |
        WEAPON_MOD_FLAG_CONTINUOUS |
        WEAPON_MOD_FLAG_EMISSION_GUIDANCE;

    COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);

    const s_combat_weapon_slot& heavy =
        TEST_SHIELD_TABLE::Combat_Ship_()->weapons[0];
    DOCTEST_CHECK(heavy.arc == WEAPON_FIRING_ARC_ALL_SECTORS);
    DOCTEST_CHECK(heavy.specials ==
                  (WEAPON_MOD_FLAG_HEAVY_MOUNT |
                   WEAPON_MOD_FLAG_ARMOR_PIERCING));
    const int expected_space = DESIGN::Weapon_Space(
        0, WEAPON_LASER_CANNON, 1, WEAPON_FIRING_ARC_360,
        0, heavy.specials);
    DOCTEST_CHECK(heavy.count ==
                  COMBINIT::Satellite_Allocated_Space(
                      200, COMBINIT::satellite_space_allocation.heavy) /
                      expected_space);

    defender->tech_applications[TECH_APP_EMISSIONS_GUIDANCE_SYSTEM] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::combat_total_ship_count = 1;
    COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);
    DOCTEST_CHECK(
        (TEST_SHIELD_TABLE::Combat_Ship_(1)->weapons[0].specials &
         WEAPON_MOD_FLAG_EMISSION_GUIDANCE) != 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "normal satellite beam banks share costed allowed modifications") {
    TEST_SHIELD_TABLE::Reset_();
    s_player* defender = TEST_SHIELD_TABLE::Player_();

    TECHDATA::hull_data[HULL_STAR_BASE].size = 20000;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].size = 10;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 10;
    TECHDATA::weapons[WEAPON_LASER_CANNON].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_LASER_CANNON].available_mods =
        WEAPON_MOD_FLAG_ARMOR_PIERCING |
        WEAPON_MOD_FLAG_AUTO_FIRE;

    TECHDATA::technology_applications[TECH_APP_LASER_CANNON].tech_field_id =
        TECH_FIELD_ADVANCED_BIOLOGY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].next_field_id =
        TECH_FIELD_ADVANCED_CHEMISTRY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_CHEMISTRY].next_field_id = 0;
    defender->tech_fields[TECH_FIELD_ADVANCED_CHEMISTRY] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    TECHDATA::weapon_modifications[WEAPON_MOD_ARMOR_PIERCING].level = 1;
    TECHDATA::weapon_modifications[WEAPON_MOD_ARMOR_PIERCING].space = 100;
    TECHDATA::weapon_modifications[WEAPON_MOD_AUTO_FIRE].level = 2;
    COMBINIT::defenses_allowed_mods.normal =
        WEAPON_MOD_FLAG_ARMOR_PIERCING |
        WEAPON_MOD_FLAG_AUTO_FIRE;

    COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);

    const uint16_t expected_mods = WEAPON_MOD_FLAG_ARMOR_PIERCING;
    const int normal_space = DESIGN::Weapon_Space(
        0,
        WEAPON_LASER_CANNON,
        1,
        WEAPON_FIRING_ARC_360,
        0,
        expected_mods);
    const int expected_count =
        COMBINIT::Satellite_Allocated_Space(
            10000, COMBINIT::satellite_space_allocation.normal) /
        normal_space;
    DOCTEST_REQUIRE(expected_count > 99);
    DOCTEST_REQUIRE(expected_count <= 198);

    const s_combat_weapon_slot* normal =
        &TEST_SHIELD_TABLE::Combat_Ship_()->weapons[1];
    DOCTEST_CHECK((normal[0].specials == expected_mods));
    DOCTEST_CHECK(normal[0].count == 99);
    DOCTEST_CHECK((normal[1].weapon_id == WEAPON_LASER_CANNON));
    DOCTEST_CHECK(normal[1].specials == expected_mods);
    DOCTEST_CHECK(normal[1].count == expected_count - 99);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "point-defense satellite banks share costed allowed modifications") {
    TEST_SHIELD_TABLE::Reset_();
    s_player* defender = TEST_SHIELD_TABLE::Player_();

    TECHDATA::hull_data[HULL_STAR_BASE].size = 10000;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].size = 10;
    TECHDATA::weapons[WEAPON_LASER_CANNON].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_LASER_CANNON].available_mods =
        WEAPON_MOD_FLAG_POINT_DEFENSE |
        WEAPON_MOD_FLAG_CONTINUOUS |
        WEAPON_MOD_FLAG_AUTO_FIRE;

    TECHDATA::technology_applications[TECH_APP_LASER_CANNON].tech_field_id =
        TECH_FIELD_ADVANCED_BIOLOGY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].next_field_id =
        TECH_FIELD_ADVANCED_CHEMISTRY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_CHEMISTRY].next_field_id = 0;
    defender->tech_fields[TECH_FIELD_ADVANCED_CHEMISTRY] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].space = -50;
    TECHDATA::weapon_modifications[WEAPON_MOD_CONTINUOUS].level = 1;
    TECHDATA::weapon_modifications[WEAPON_MOD_CONTINUOUS].space = 100;
    TECHDATA::weapon_modifications[WEAPON_MOD_AUTO_FIRE].level = 2;
    COMBINIT::defenses_allowed_mods.pd =
        WEAPON_MOD_FLAG_POINT_DEFENSE |
        WEAPON_MOD_FLAG_CONTINUOUS |
        WEAPON_MOD_FLAG_AUTO_FIRE;

    COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);

    int pd_slot = -1;
    for (int slot = 0; slot < 8; ++slot) {
        if ((TEST_SHIELD_TABLE::Combat_Ship_()->weapons[slot].specials &
             WEAPON_MOD_FLAG_POINT_DEFENSE) != 0) {
            pd_slot = slot;
            break;
        }
    }
    DOCTEST_REQUIRE(pd_slot >= 0);
    DOCTEST_REQUIRE(pd_slot < 7);

    const uint16_t expected_mods =
        WEAPON_MOD_FLAG_POINT_DEFENSE | WEAPON_MOD_FLAG_CONTINUOUS;
    const int pd_space = DESIGN::Weapon_Space(
        0,
        WEAPON_LASER_CANNON,
        1,
        WEAPON_FIRING_ARC_360,
        0,
        expected_mods);
    const int expected_count =
        COMBINIT::Satellite_Allocated_Space(
            5000, COMBINIT::satellite_space_allocation.pd) /
        pd_space;
    DOCTEST_REQUIRE(expected_count > 99);
    DOCTEST_REQUIRE(expected_count <= 198);

    const s_combat_weapon_slot* pd =
        &TEST_SHIELD_TABLE::Combat_Ship_()->weapons[pd_slot];
    DOCTEST_CHECK(pd[0].specials == expected_mods);
    DOCTEST_CHECK(pd[0].count == 99);
    DOCTEST_CHECK((pd[1].weapon_id == WEAPON_LASER_CANNON));
    DOCTEST_CHECK((pd[1].specials == expected_mods));
    DOCTEST_CHECK(pd[1].count == expected_count - 99);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "satellite missile banks distribute allowed mods and price ECCM as free") {
    TEST_SHIELD_TABLE::Reset_();
    s_player* defender = TEST_SHIELD_TABLE::Player_();

    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].size = 1;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].available_mods =
        WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
        WEAPON_MOD_FLAG_ECCM |
        WEAPON_MOD_FLAG_HEAVILY_ARMORED |
        WEAPON_MOD_FLAG_FAST |
        WEAPON_MOD_FLAG_EMISSION_GUIDANCE |
        WEAPON_MOD_FLAG_OVERLOADED;

    TECHDATA::technology_applications[TECH_APP_LASER_CANNON].tech_field_id =
        TECH_FIELD_ADVANCED_BIOLOGY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].next_field_id =
        TECH_FIELD_ADVANCED_CHEMISTRY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_CHEMISTRY].next_field_id = 0;
    defender->tech_fields[TECH_FIELD_ADVANCED_CHEMISTRY] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    TECHDATA::weapon_modifications[WEAPON_MOD_NO_RANGE_DISSIPATION].level = 1;
    TECHDATA::weapon_modifications[WEAPON_MOD_ECCM].level = 1;
    TECHDATA::weapon_modifications[WEAPON_MOD_ECCM].space = 1000;
    TECHDATA::weapon_modifications[WEAPON_MOD_HEAVILY_ARMORED].level = 1;
    TECHDATA::weapon_modifications[WEAPON_MOD_FAST].level = 2;
    TECHDATA::weapon_modifications[WEAPON_MOD_EMISSIONS_GUIDANCE].level = 1;
    TECHDATA::weapon_modifications[WEAPON_MOD_OVERLOADED].level = 1;
    COMBINIT::defenses_allowed_mods.missile_torp =
        WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
        WEAPON_MOD_FLAG_ECCM |
        WEAPON_MOD_FLAG_HEAVILY_ARMORED |
        WEAPON_MOD_FLAG_FAST |
        WEAPON_MOD_FLAG_EMISSION_GUIDANCE |
        WEAPON_MOD_FLAG_OVERLOADED;

    const uint16_t expected_mods =
        WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
        WEAPON_MOD_FLAG_ECCM |
        WEAPON_MOD_FLAG_HEAVILY_ARMORED |
        WEAPON_MOD_FLAG_OVERLOADED;
    const uint16_t costed_mods =
        expected_mods & (uint16_t)~WEAPON_MOD_FLAG_ECCM;
    const int missile_space = DESIGN::Weapon_Space(
        0,
        WEAPON_NUCLEAR_MISSILE,
        1,
        WEAPON_FIRING_ARC_360,
        20,
        costed_mods);
    const int charged_eccm_space = DESIGN::Weapon_Space(
        0,
        WEAPON_NUCLEAR_MISSILE,
        1,
        WEAPON_FIRING_ARC_360,
        20,
        expected_mods);
    DOCTEST_REQUIRE(charged_eccm_space > missile_space);

    TECHDATA::hull_data[HULL_STAR_BASE].size =
        (int16_t)(missile_space * 16);
    COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);

    int missile_count = 0;
    uint8_t missile_counts[3] = {};
    int missile_slot_count = 0;
    for (int slot = 0; slot < 8; ++slot) {
        const s_combat_weapon_slot& weapon =
            TEST_SHIELD_TABLE::Combat_Ship_()->weapons[slot];
        if (weapon.weapon_id == WEAPON_NUCLEAR_MISSILE) {
            DOCTEST_REQUIRE(missile_slot_count < 3);
            DOCTEST_CHECK(weapon.arc == WEAPON_FIRING_ARC_ALL_SECTORS);
            DOCTEST_CHECK((weapon.specials == expected_mods));
            missile_counts[missile_slot_count] = weapon.count;
            ++missile_slot_count;
            missile_count += weapon.count;
        }
    }

    int expected_count =
        COMBINIT::Satellite_Allocated_Space(
            missile_space * 8,
            COMBINIT::satellite_space_allocation.missile_torp) /
        missile_space;
    if (expected_count < 1) expected_count = 1;
    if (expected_count > 297) expected_count = 297;
    DOCTEST_CHECK(missile_count == expected_count);
    DOCTEST_CHECK(missile_slot_count == 3);
    DOCTEST_CHECK(missile_counts[0] == 2);
    DOCTEST_CHECK(missile_counts[1] == 2);
    DOCTEST_CHECK(missile_counts[2] == 3);

    DOCTEST_REQUIRE(missile_space <= INT16_MAX / 300);
    COMBINIT::satellite_space_allocation = {0, 0, 0, 1};
    TECHDATA::hull_data[HULL_STAR_BASE].size =
        (int16_t)(missile_space * 300);
    MOX::combat_total_ship_count = 1;
    COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);

    missile_count = 0;
    missile_slot_count = 0;
    for (int slot = 0; slot < 8; ++slot) {
        const s_combat_weapon_slot& weapon =
            TEST_SHIELD_TABLE::Combat_Ship_(1)->weapons[slot];
        if (weapon.weapon_id == WEAPON_NUCLEAR_MISSILE) {
            DOCTEST_CHECK(weapon.count == 99);
            missile_count += weapon.count;
            ++missile_slot_count;
        }
    }
    DOCTEST_CHECK(missile_slot_count == 3);
    DOCTEST_CHECK(missile_count == 297);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "defense missile ECCM installation is unconditional") {
    struct s_case {
        bool allowed;
        bool supported;
        uint8_t required_level;
    };
    const s_case cases[] = {
        {false, true, 0},
        {true, false, 0},
        {true, true, 1},
        {true, true, 0},
    };

    for (const s_case& test : cases) {
        TEST_SHIELD_TABLE::Reset_();
        s_colony* colony = TEST_SHIELD_TABLE::Colony_();
        s_player* defender = TEST_SHIELD_TABLE::Player_();
        colony->owner = 0;
        colony->planet = 0;
        colony->buildings[BUILDING_MISSILE_BASE] = 1;

        TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
        TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].size = 10;
        TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].tech_app_id =
            TECH_APP_LASER_CANNON;
        TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].available_mods =
            test.supported ? WEAPON_MOD_FLAG_ECCM : 0;
        TECHDATA::technology_applications[TECH_APP_LASER_CANNON].tech_field_id =
            TECH_FIELD_ADVANCED_BIOLOGY;
        TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].next_field_id = 0;
        defender->tech_applications[TECH_APP_LASER_CANNON] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        TECHDATA::weapon_modifications[WEAPON_MOD_ECCM].level =
            test.required_level;
        TECHDATA::weapon_modifications[WEAPON_MOD_ECCM].space = 1000;
        COMBINIT::defenses_allowed_mods.missile_torp =
            test.allowed ? WEAPON_MOD_FLAG_ECCM : 0;

        COMBINIT::Load_Tactical_Colony(0, 0);
        bool found_missile_base_weapon = false;
        for (int slot = 0; slot < 8; ++slot) {
            const s_combat_weapon_slot& weapon =
                TEST_SHIELD_TABLE::Combat_Ship_()->weapons[slot];
            if (weapon.weapon_id == WEAPON_NUCLEAR_MISSILE) {
                DOCTEST_CHECK(
                    (weapon.specials & WEAPON_MOD_FLAG_ECCM) != 0);
                found_missile_base_weapon = true;
            }
        }
        DOCTEST_CHECK(found_missile_base_weapon);

        TECHDATA::hull_data[HULL_STAR_BASE].size = 400;
        MOX::combat_total_ship_count = 1;
        COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);
        bool found_orbital_weapon = false;
        for (int slot = 0; slot < 8; ++slot) {
            const s_combat_weapon_slot& weapon =
                TEST_SHIELD_TABLE::Combat_Ship_(1)->weapons[slot];
            if (weapon.weapon_id == WEAPON_NUCLEAR_MISSILE) {
                DOCTEST_CHECK(
                    (weapon.specials & WEAPON_MOD_FLAG_ECCM) != 0);
                found_orbital_weapon = true;
            }
        }
        DOCTEST_CHECK(found_orbital_weapon);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "planet extra specials follow building and technology gates") {
    struct s_case {
        bool has_ground_batteries;
        bool has_stellar_converter;
        bool has_technology;
        bool expect_ground_battery_specials;
        bool expect_shared_specials;
    };
    const s_case cases[] = {
        {false, true, true, false, true},
        {true, false, true, true, true},
        {true, false, false, false, false},
    };

    for (const s_case& test : cases) {
        TEST_SHIELD_TABLE::Reset_();
        s_colony* colony = TEST_SHIELD_TABLE::Colony_();
        s_player* defender = TEST_SHIELD_TABLE::Player_();
        colony->owner = 0;
        colony->planet = 0;
        colony->buildings[BUILDING_GROUND_BATTERIES] =
            test.has_ground_batteries ? 1 : 0;
        colony->buildings[BUILDING_STELLAR_CONVERTER] =
            test.has_stellar_converter ? 1 : 0;

        TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
        TECHDATA::weapons[WEAPON_LASER_CANNON].size = 10;
        TECHDATA::weapons[WEAPON_LASER_CANNON].tech_app_id =
            TECH_APP_LASER_CANNON;
        TECHDATA::weapons[WEAPON_LASER_CANNON].available_mods =
            WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_POINT_DEFENSE;
        defender->tech_applications[TECH_APP_LASER_CANNON] =
            TECH_RESEARCH_STATUS_RESEARCHED;

        COMBINIT::planet = {1, 1, 1, 1};
        defender->tech_applications[TECH_APP_BATTLE_SCANNER] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        if (test.has_technology) {
            defender->tech_applications[TECH_APP_RANGEMASTER_UNIT] =
                TECH_RESEARCH_STATUS_RESEARCHED;
            defender->tech_applications[TECH_APP_HIGH_ENERGY_FOCUS] =
                TECH_RESEARCH_STATUS_RESEARCHED;
            defender->tech_applications[TECH_APP_STRUCTURAL_ANALYZER] =
                TECH_RESEARCH_STATUS_RESEARCHED;
            defender->tech_applications[TECH_APP_ACHILLES_TARGETING_UNIT] =
                TECH_RESEARCH_STATUS_RESEARCHED;
        }

        COMBINIT::Load_Tactical_Colony(0, 0);

        s_combat_data* planet = TEST_SHIELD_TABLE::Combat_Ship_();
        DOCTEST_CHECK(struct_::Test_Bit_Field(
            planet->special_device_flags, SPECIAL_RANGEMASTER_UNIT) ==
            test.expect_ground_battery_specials);
        DOCTEST_CHECK(struct_::Test_Bit_Field(
            planet->special_device_flags, SPECIAL_HIGH_ENERGY_FOCUS) ==
            test.expect_ground_battery_specials);
        DOCTEST_CHECK(struct_::Test_Bit_Field(
            planet->special_device_flags, SPECIAL_STRUCTURAL_ANALYZER) ==
            test.expect_shared_specials);
        DOCTEST_CHECK(struct_::Test_Bit_Field(
            planet->special_device_flags, SPECIAL_ACHILLES_TARGETING_UNIT) ==
            test.expect_shared_specials);
        DOCTEST_CHECK(struct_::Test_Bit_Field(
            planet->special_device_flags, SPECIAL_BATTLE_SCANNER));
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "planet defenses use configured eligible modifications") {
    TEST_SHIELD_TABLE::Reset_();
    s_colony* colony = TEST_SHIELD_TABLE::Colony_();
    s_player* defender = TEST_SHIELD_TABLE::Player_();
    colony->owner = 0;
    colony->planet = 0;
    colony->buildings[BUILDING_GROUND_BATTERIES] = 1;
    colony->buildings[BUILDING_MISSILE_BASE] = 1;

    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].size = 10;
    TECHDATA::weapons[WEAPON_LASER_CANNON].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_LASER_CANNON].available_mods =
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
        WEAPON_MOD_FLAG_POINT_DEFENSE |
        WEAPON_MOD_FLAG_ARMOR_PIERCING |
        WEAPON_MOD_FLAG_CONTINUOUS |
        WEAPON_MOD_FLAG_AUTO_FIRE |
        WEAPON_MOD_FLAG_EMISSION_GUIDANCE;

    TECHDATA::weapons[WEAPON_FUSION_BEAM].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_FUSION_BEAM].size = 1;
    TECHDATA::weapons[WEAPON_FUSION_BEAM].max_damage = 8;
    TECHDATA::weapons[WEAPON_FUSION_BEAM].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_FUSION_BEAM].available_mods =
        WEAPON_MOD_FLAG_POINT_DEFENSE |
        WEAPON_MOD_FLAG_AUTO_FIRE |
        WEAPON_MOD_FLAG_ENVELOPING;

    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].size = 10;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].available_mods =
        WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
        WEAPON_MOD_FLAG_ECCM |
        WEAPON_MOD_FLAG_HEAVILY_ARMORED |
        WEAPON_MOD_FLAG_FAST |
        WEAPON_MOD_FLAG_OVERLOADED;

    TECHDATA::technology_applications[TECH_APP_LASER_CANNON].tech_field_id =
        TECH_FIELD_ADVANCED_BIOLOGY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].next_field_id =
        TECH_FIELD_ADVANCED_CHEMISTRY;
    TECHDATA::technology_fields[TECH_FIELD_ADVANCED_CHEMISTRY].next_field_id = 0;
    defender->tech_fields[TECH_FIELD_ADVANCED_CHEMISTRY] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    defender->tech_applications[TECH_APP_LASER_CANNON] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    const int mod_ids[] = {
        WEAPON_MOD_POINT_DEFENSE,
        WEAPON_MOD_ARMOR_PIERCING,
        WEAPON_MOD_CONTINUOUS,
        WEAPON_MOD_NO_RANGE_DISSIPATION,
        WEAPON_MOD_AUTO_FIRE,
        WEAPON_MOD_ENVELOPING,
        WEAPON_MOD_ECCM,
        WEAPON_MOD_HEAVILY_ARMORED,
        WEAPON_MOD_FAST,
        WEAPON_MOD_EMISSIONS_GUIDANCE,
        WEAPON_MOD_OVERLOADED,
    };
    for (int mod : mod_ids) {
        TECHDATA::weapon_modifications[mod].level = 0;
        TECHDATA::weapon_modifications[mod].space = 0;
    }
    TECHDATA::weapon_modifications[WEAPON_MOD_ECCM].space = 1000;

    COMBINIT::defenses_allowed_mods.heavy =
        WEAPON_MOD_FLAG_ARMOR_PIERCING |
        WEAPON_MOD_FLAG_AUTO_FIRE |
        WEAPON_MOD_FLAG_EMISSION_GUIDANCE;
    COMBINIT::defenses_allowed_mods.pd =
        WEAPON_MOD_FLAG_CONTINUOUS |
        WEAPON_MOD_FLAG_AUTO_FIRE |
        WEAPON_MOD_FLAG_ENVELOPING;
    COMBINIT::defenses_allowed_mods.missile_torp =
        WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
        WEAPON_MOD_FLAG_ECCM |
        WEAPON_MOD_FLAG_HEAVILY_ARMORED |
        WEAPON_MOD_FLAG_OVERLOADED;

    const uint16_t expected_missile_mods =
        WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
        WEAPON_MOD_FLAG_ECCM |
        WEAPON_MOD_FLAG_HEAVILY_ARMORED |
        WEAPON_MOD_FLAG_OVERLOADED;
    const uint16_t missile_cost_mods =
        expected_missile_mods & (uint16_t)~WEAPON_MOD_FLAG_ECCM;
    const int missile_space = DESIGN::Weapon_Space(
        0, WEAPON_NUCLEAR_MISSILE, 1, WEAPON_FIRING_ARC_360,
        5, missile_cost_mods);
    const int charged_eccm_space = DESIGN::Weapon_Space(
        0, WEAPON_NUCLEAR_MISSILE, 1, WEAPON_FIRING_ARC_360,
        5, expected_missile_mods);
    DOCTEST_REQUIRE(missile_space > 0);
    DOCTEST_REQUIRE(charged_eccm_space > missile_space);

    COMBINIT::Load_Tactical_Colony(0, 0);

    DOCTEST_CHECK(TEST_SHIELD_TABLE::Combat_Ship_()->weapons[0].specials ==
                  (WEAPON_MOD_FLAG_HEAVY_MOUNT |
                   WEAPON_MOD_FLAG_ARMOR_PIERCING |
                   WEAPON_MOD_FLAG_AUTO_FIRE));
    DOCTEST_CHECK((TEST_SHIELD_TABLE::Combat_Ship_()->weapons[1].weapon_id ==
        WEAPON_FUSION_BEAM));
    DOCTEST_CHECK((TEST_SHIELD_TABLE::Combat_Ship_()->weapons[1].specials ==
        (WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_AUTO_FIRE |
            WEAPON_MOD_FLAG_ENVELOPING)));

    int missile_count = 0;
    for (int slot = 2; slot < 8; ++slot) {
        const s_combat_weapon_slot& weapon =
            TEST_SHIELD_TABLE::Combat_Ship_()->weapons[slot];
        if (weapon.weapon_id == WEAPON_NUCLEAR_MISSILE) {
            DOCTEST_CHECK((weapon.specials == expected_missile_mods));
            missile_count += weapon.count;
        }
    }
    const int total_count = 300 / missile_space;
    const int expected_count =
        total_count > 10 ? (total_count / 2) * 2 : total_count;
    DOCTEST_CHECK(missile_count == expected_count);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "defense point-defense selection uses unsigned modification levels") {
    struct s_case {
        uint8_t required_level;
        uint8_t miniaturization;
        int16_t expected_weapon;
    };
    const s_case cases[] = {
        {127, 126, WEAPON_LASER_CANNON},
        {127, 127, WEAPON_FUSION_BEAM},
        {128, 127, WEAPON_LASER_CANNON},
        {128, 128, WEAPON_FUSION_BEAM},
        {255, 254, WEAPON_LASER_CANNON},
        {255, 255, WEAPON_FUSION_BEAM},
    };

    for (const s_case& test : cases) {
        TEST_SHIELD_TABLE::Reset_();
        s_colony* colony = TEST_SHIELD_TABLE::Colony_();
        s_player* defender = TEST_SHIELD_TABLE::Player_();
        colony->owner = 0;
        colony->planet = 0;
        colony->buildings[BUILDING_GROUND_BATTERIES] = 1;

        TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
        TECHDATA::weapons[WEAPON_LASER_CANNON].size = 20;
        TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 100;
        TECHDATA::weapons[WEAPON_LASER_CANNON].tech_app_id =
            TECH_APP_LASER_CANNON;
        TECHDATA::weapons[WEAPON_LASER_CANNON].available_mods =
            WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_POINT_DEFENSE;

        TECHDATA::weapons[WEAPON_FUSION_BEAM].type = WEAPON_TYPE_BEAM;
        TECHDATA::weapons[WEAPON_FUSION_BEAM].size = 20;
        TECHDATA::weapons[WEAPON_FUSION_BEAM].max_damage = 40;
        TECHDATA::weapons[WEAPON_FUSION_BEAM].tech_app_id =
            TECH_APP_LASER_CANNON;
        TECHDATA::weapons[WEAPON_FUSION_BEAM].available_mods =
            WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_AUTO_FIRE;

        defender->tech_applications[TECH_APP_LASER_CANNON] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        TECHDATA::technology_applications[TECH_APP_LASER_CANNON].tech_field_id =
            TECH_FIELD_ADVANCED_BIOLOGY;
        TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].next_field_id =
            TECH_FIELD_BIOLOGY;
        defender->hyper_advanced_tech[0] = test.miniaturization;

        TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].space = -50;
        TECHDATA::weapon_modifications[WEAPON_MOD_AUTO_FIRE].level =
            test.required_level;
        TECHDATA::weapon_modifications[WEAPON_MOD_AUTO_FIRE].space = 0;
        COMBINIT::defenses_allowed_mods.pd =
            WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_AUTO_FIRE;

        COMBINIT::Load_Tactical_Colony(0, 0);
        const s_combat_weapon_slot& ground_pd =
            TEST_SHIELD_TABLE::Combat_Ship_()->weapons[1];
        DOCTEST_CHECK((ground_pd.weapon_id == test.expected_weapon));
        DOCTEST_CHECK(
            ((ground_pd.specials & WEAPON_MOD_FLAG_AUTO_FIRE) != 0) ==
            (test.miniaturization >= test.required_level));

        TECHDATA::hull_data[HULL_STAR_BASE].size = 400;
        MOX::combat_total_ship_count = 1;
        COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);
        bool found_orbital_pd = false;
        for (int slot = 0; slot < 8; ++slot) {
            const s_combat_weapon_slot& weapon =
                TEST_SHIELD_TABLE::Combat_Ship_(1)->weapons[slot];
            if ((weapon.specials & WEAPON_MOD_FLAG_POINT_DEFENSE) != 0) {
                DOCTEST_CHECK(weapon.weapon_id == test.expected_weapon);
                DOCTEST_CHECK(
                    ((weapon.specials & WEAPON_MOD_FLAG_AUTO_FIRE) != 0) ==
                    (test.miniaturization >= test.required_level));
                found_orbital_pd = true;
            }
        }
        DOCTEST_CHECK(found_orbital_pd);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "satellite weapon space follows configured allocation") {
    TEST_SHIELD_TABLE::Reset_();

    DOCTEST_CHECK(COMBINIT::Satellite_Allocated_Space(
                      200, COMBINIT::satellite_space_allocation.heavy) == 104);
    DOCTEST_CHECK(COMBINIT::Satellite_Allocated_Space(
                      200, COMBINIT::satellite_space_allocation.normal) == 52);
    DOCTEST_CHECK(COMBINIT::Satellite_Allocated_Space(
                      200, COMBINIT::satellite_space_allocation.pd) == 48);
    DOCTEST_CHECK(COMBINIT::Satellite_Allocated_Space(
                      200,
                      COMBINIT::satellite_space_allocation.missile_torp) == 196);

    COMBINIT::satellite_space_allocation = {40, 20, 10, 30};
    DOCTEST_CHECK(COMBINIT::Satellite_Allocated_Space(
                      200, COMBINIT::satellite_space_allocation.heavy) == 160);
    DOCTEST_CHECK(COMBINIT::Satellite_Allocated_Space(
                      200,
                      COMBINIT::satellite_space_allocation.missile_torp) == 120);

    COMBINIT::satellite_space_allocation = {};
    DOCTEST_CHECK(COMBINIT::Satellite_Allocated_Space(200, 1) == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "odd satellite hull allocation matches tactical and AI counts") {
    TEST_SHIELD_TABLE::Reset_();
    s_player* defender = TEST_SHIELD_TABLE::Player_();

    MOX::combat_total_ship_count = 1;
    defender->tech_applications[TECH_APP_MEGAFLUXERS] = 0;
    defender->tech_applications[TECH_APP_LASER_CANNON] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    TECHDATA::hull_data[HULL_STAR_BASE].size = 7;
    TECHDATA::hull_data[HULL_STAR_BASE].structure_hp = 100;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].size = 1;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 10;
    TECHDATA::weapons[WEAPON_LASER_CANNON].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_LASER_CANNON].available_mods =
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
        WEAPON_MOD_FLAG_POINT_DEFENSE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].size = 1;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 10;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].tech_app_id =
        TECH_APP_LASER_CANNON;

    TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].level = 0;
    TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].space = 0;
    TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].level = 0;
    TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].space = 0;
    DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_ALL_ROUND] =
        {0, 0};
    COMBINIT::satellite_ammo_basis = 0;
    COMBINIT::satellite_space_allocation = {1, 1, 1, 1};
    COMBINIT::defenses_allowed_mods = {
        WEAPON_MOD_FLAG_HEAVY_MOUNT,
        0,
        WEAPON_MOD_FLAG_POINT_DEFENSE,
        0,
    };

    COMBINIT::Load_Combat_Satelite(HULL_STAR_BASE);

    const s_combat_data* satellite =
        TEST_SHIELD_TABLE::Combat_Ship_(1);
    DOCTEST_CHECK(satellite->weapons[0].count == 1);
    DOCTEST_CHECK(satellite->weapons[1].count == 1);
    DOCTEST_CHECK(satellite->weapons[2].count == 1);
    DOCTEST_CHECK(satellite->weapons[3].count == 2);

    int accuracy = AIPOWER::Base_Generic_Ocv(AI_World_(), legacy_world::Player_Id(0)) -
                   AIPOWER::Base_Generic_Dcv(AI_World_(), legacy_world::Player_Id(0)) + 100;
    if (accuracy < 10) accuracy = 10;
    if (accuracy > 100) accuracy = 100;
    const int shield = AIPOWER::Get_Best_Ship_Shield_Value(TECHDATA::shields, legacy_world::Player_Id(0));
    const int heavy_damage = 15 - shield > 0 ? 15 - shield : 0;
    const int normal_damage = 10 - shield > 0 ? 10 - shield : 0;
    const int missile_damage = 10 - shield > 0 ? 10 - shield : 0;
    uint32_t tactical_strength =
        heavy_damage * satellite->weapons[0].count * accuracy / 100;
    tactical_strength +=
        normal_damage * satellite->weapons[1].count * accuracy / 100;
    tactical_strength +=
        missile_damage * satellite->weapons[3].count;
    uint32_t expected_strength =
        AIPOWER::Strength_Hits_Modifier(100) * tactical_strength / 100;
    if (expected_strength <= 1) expected_strength = 1;
    expected_strength = expected_strength * 75 / 100;

    DOCTEST_CHECK(AIPOWER::Satelite_Strength_Vs_Ships(AI_World_(), 1, legacy_world::Player_Id(0), legacy_world::Player_Id(0)) ==
                  expected_strength);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "Megafluxers keeps all orbital allocation sizes wide") {
    TEST_SHIELD_TABLE::Reset_();
    s_player* defender = TEST_SHIELD_TABLE::Player_();
    defender->tech_applications[TECH_APP_MEGAFLUXERS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    defender->tech_applications[TECH_APP_LASER_CANNON] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].size = 1000;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 10;
    TECHDATA::weapons[WEAPON_LASER_CANNON].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[WEAPON_LASER_CANNON].available_mods =
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
        WEAPON_MOD_FLAG_POINT_DEFENSE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].size = 1000;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 10;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].tech_app_id =
        TECH_APP_LASER_CANNON;

    TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].level = 0;
    TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].space = 0;
    TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].level = 0;
    TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].space = 0;
    DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_ALL_ROUND] =
        {0, 0};
    DESIGN::megafluxers_tactical_bonus = 32768;
    COMBINIT::satellite_ammo_basis = 0;
    COMBINIT::satellite_space_allocation = {0, 0, 0, 1};
    COMBINIT::defenses_allowed_mods = {
        WEAPON_MOD_FLAG_HEAVY_MOUNT,
        0,
        WEAPON_MOD_FLAG_POINT_DEFENSE,
        0,
    };

    const int16_t station_types[] = {
        HULL_STAR_BASE,
        HULL_BATTLESTATION,
        HULL_STAR_FORTRESS,
    };
    for (int16_t station_type : station_types) {
        TECHDATA::hull_data[station_type].size = 100;
        MOX::combat_total_ship_count = 1;
        COMBINIT::Load_Combat_Satelite(station_type);

        int missile_count = 0;
        for (int slot = 0; slot < 8; ++slot) {
            const s_combat_weapon_slot& weapon =
                TEST_SHIELD_TABLE::Combat_Ship_(1)->weapons[slot];
            if (weapon.weapon_id == WEAPON_NUCLEAR_MISSILE) {
                missile_count += weapon.count;
            }
        }
        DOCTEST_CHECK(missile_count == 32);
    }

    DESIGN::megafluxers_tactical_bonus = 25;
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "weapon arc cost and space modifiers are independent") {
    TEST_SHIELD_TABLE::Reset_();
    const int16_t weapon = WEAPON_LASER_CANNON;
    TECHDATA::weapons[weapon].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[weapon].size = 100;
    TECHDATA::weapons[weapon].cost = 100;
    TECHDATA::weapons[weapon].tech_app_id = TECH_APP_NO_TECH;
    DESIGN::miniaturization_categories[TECH_APP_NO_TECH] =
        DESIGN::MINIATURIZATION_CATEGORY_GROUP_1;
    DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_EXTENDED] =
        {1, 2};
    DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_ALL_ROUND] =
        {3, 4};

    const int16_t extended = WEAPON_FIRING_ARC_FORWARD_EXTENDED;
    const int16_t all_round = WEAPON_FIRING_ARC_360;
    DOCTEST_CHECK(DESIGN::Weapon_Cost(weapon, 1, extended, 0, 0) == 101);
    DOCTEST_CHECK(DESIGN::Weapon_Space(0, weapon, 1, extended, 0, 0) == 102);
    DOCTEST_CHECK(DESIGN::One_Weapon_Space_Without_M_Level(
                      weapon, extended, 0, 0) == 102);
    DOCTEST_CHECK(DESIGN::Weapon_Cost(weapon, 1, all_round, 0, 0) == 103);
    DOCTEST_CHECK(DESIGN::Weapon_Space(0, weapon, 1, all_round, 0, 0) == 104);
    DOCTEST_CHECK(DESIGN::One_Weapon_Space_Without_M_Level(
                      weapon, all_round, 0, 0) == 104);

    const int16_t combined = extended | all_round;
    DOCTEST_CHECK(DESIGN::Weapon_Cost(weapon, 1, combined, 0, 0) == 101);
    DOCTEST_CHECK(DESIGN::Weapon_Space(0, weapon, 1, combined, 0, 0) == 102);
    DOCTEST_CHECK(DESIGN::One_Weapon_Space_Without_M_Level(
                      weapon, combined, 0, 0) == 102);

    DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_EXTENDED] =
        {0, 255};
    DOCTEST_CHECK(DESIGN::Weapon_Cost(weapon, 1, extended, 0, 0) == 100);
    DOCTEST_CHECK(DESIGN::Weapon_Space(0, weapon, 1, extended, 0, 0) == 355);
    DOCTEST_CHECK(DESIGN::One_Weapon_Space_Without_M_Level(
                      weapon, extended, 0, 0) == 355);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "shield capacity uses configured hull multipliers") {
    TEST_SHIELD_TABLE::Reset_();
    TECHDATA::shield_hull_multiplier[HULL_CRUISER] = 23;
    TECHDATA::shields[SHIELD_CLASS_V_SHIELD].shield_class = 5;

    DOCTEST_CHECK(
        TECHDATA::Shield_Strength(HULL_CRUISER, SHIELD_CLASS_V_SHIELD) == 115);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "combat satellites select orbital defense shield rows") {
    TEST_SHIELD_TABLE::Reset_();
    s_combat_data* ship = TEST_SHIELD_TABLE::Combat_Ship_(1);
    ship->ship_idx = -1;
    ship->ftl_type = DRIVE_NO_DRIVE;
    ship->size_class = SHIP_SIZE_LARGE;
    ship->shield_type = SHIELD_CLASS_V_SHIELD;

    DOCTEST_CHECK(COMBINIT::Calc_Combat_Shield_Max(1) ==
                  TECHDATA::shield_hull_multiplier[HULL_STAR_BASE] *
                      TECHDATA::shields[SHIELD_CLASS_V_SHIELD].shield_class);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "satellite shield rows remain stable across configured tactical sizes") {
    TEST_SHIELD_TABLE::Reset_();
    s_combat_data* ship = TEST_SHIELD_TABLE::Combat_Ship_(1);
    ship->ship_idx = -1;
    ship->ftl_type = DRIVE_NO_DRIVE;
    ship->shield_type = SHIELD_CLASS_V_SHIELD;

    COMBINIT::satellite_size_class_plus_one = 0;
    ship->size_class = SHIP_SIZE_LARGE;
    DOCTEST_CHECK(COMBINIT::Calc_Combat_Shield_Max(1) ==
                  TECHDATA::shield_hull_multiplier[HULL_STAR_BASE] *
                      TECHDATA::shields[SHIELD_CLASS_V_SHIELD].shield_class);

    COMBINIT::satellite_size_class_plus_one = 1;
    ship->size_class = SHIP_SIZE_HUGE;
    DOCTEST_CHECK(COMBINIT::Calc_Combat_Shield_Max(1) ==
                  TECHDATA::shield_hull_multiplier[HULL_STAR_BASE] *
                      TECHDATA::shields[SHIELD_CLASS_V_SHIELD].shield_class);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "combat shield capacity keeps nebula and special behavior") {
    TEST_SHIELD_TABLE::Reset_();
    s_combat_data* ship = TEST_SHIELD_TABLE::Combat_Ship_();
    ship->ship_idx = 4;
    ship->size_class = SHIP_SIZE_HUGE;
    ship->shield_type = SHIELD_CLASS_III_SHIELD;
    TECHDATA::shield_hull_multiplier[HULL_BATTLESHIP] = 22;

    MOX::in_nebula_flag = 1;
    DOCTEST_CHECK(COMBINIT::Calc_Combat_Shield_Max(0) == 0);

    TEST_SHIELD_TABLE::Set_Combat_Special_(SPECIAL_HARD_SHIELDS);
    TEST_SHIELD_TABLE::Set_Combat_Special_(SPECIAL_MULTI_PHASED_SHIELDS);
    TECHDATA::multi_phased_shields_bonus = 25;
    DOCTEST_CHECK(COMBINIT::Calc_Combat_Shield_Max(0) == 82);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "design shield capacity applies Multi-Phased Shields after configured base") {
    TEST_SHIELD_TABLE::Reset_();
    s_current_design* design = TEST_SHIELD_TABLE::Design_();
    design->ship_size = SHIP_SIZE_TITAN;
    design->shield = SHIELD_CLASS_VII_SHIELD;
    TEST_SHIELD_TABLE::Set_Design_Special_(SPECIAL_MULTI_PHASED_SHIELDS);

    const int16_t base =
        (int16_t)TECHDATA::Shield_Strength(HULL_TITAN, SHIELD_CLASS_VII_SHIELD);
    TECHDATA::multi_phased_shields_bonus = 75;
    DOCTEST_CHECK(DESIGN::Shield_Strength() == base * 7 / 4);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "design Hard Shields use configured damage reduction") {
    TEST_SHIELD_TABLE::Reset_();
    TEST_SHIELD_TABLE::Design_()->shield = SHIELD_CLASS_I_SHIELD;
    TEST_SHIELD_TABLE::Set_Design_Special_(SPECIAL_HARD_SHIELDS);
    TECHDATA::hard_shields_damage_reduction = 8;

    DOCTEST_CHECK(DESIGN::Current_Shield_Damage_Points_Blocked() == 9);
}

DOCTEST_TEST_CASE_FIXTURE(s_shield_state, "ESD bonus scales additional structural damage") {
    TEST_SHIELD_TABLE::Reset_();

    TECHDATA::esd_bonus = 50;
    DOCTEST_CHECK(TECHDATA::Extra_Structural_Damage(7) == 3);

    TECHDATA::esd_bonus = 100;
    DOCTEST_CHECK(TECHDATA::Extra_Structural_Damage(7) == 7);

    TECHDATA::esd_bonus = 200;
    DOCTEST_CHECK(TECHDATA::Extra_Structural_Damage(7) == 14);

    TECHDATA::esd_bonus = 65535;
    DOCTEST_CHECK(TECHDATA::Extra_Structural_Damage(49150) == 32210452);
}