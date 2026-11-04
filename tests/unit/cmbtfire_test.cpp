#include "pch.h"

#include "doctest/doctest.h"
#include "game/cmbtmis.h"
#include "game/cmbtfire.h"
#include "game/tactical_events.h"
#include "lua/lua_game.h"

namespace TEST_STASIS {
    void Reset_();
    int16_t Last_Cleanup_Source_();
    int16_t Last_Missile_Hits_Owner_();
    int16_t Last_Retargeted_Missile_();
    int16_t Retargeted_Ship_Status_();
    void Set_Missile_Hits_(int16_t hits);
    int16_t Crew_Kills_();
    int16_t Last_Crew_Kill_Count_();
    void Capture_Internal_Damage_();
    int16_t Captured_Internal_Damage_();
} // namespace TEST_STASIS

namespace TEST_CRYSTAL_FLEET {
    void Reset_();
    int16_t Cleanup_Count_();
    int16_t Cleanup_Source_(int16_t index);
} // namespace TEST_CRYSTAL_FLEET

namespace TEST_BEAM_MODIFIERS {
    void Reset_();
    void Set_Has_HEF_(bool has_hef);
} // namespace TEST_BEAM_MODIFIERS

namespace TEST_FAIR_BEAMS {
    void Reset_();
    void Set_Random_Value_(uint32_t value);
} // namespace TEST_FAIR_BEAMS

namespace TEST_WEAPON_RANGE {
    void Reset_();
    void Set_Center_Offset_(int16_t ship_idx, int16_t offset);
} // namespace TEST_WEAPON_RANGE

namespace TEST_BOMBING_RANGE {
    void Reset_();
    void Set_Has_Transporters_(bool value);
} // namespace TEST_BOMBING_RANGE

namespace TEST_ENGINE_EXPLOSION {
    void Reset_();
    int16_t Destroy_FX_Type_();
    int16_t Explosion_Damage_Source_();
    int16_t Spherical_Effect_Type_();
    int16_t Spherical_Damage_();
} // namespace TEST_ENGINE_EXPLOSION

namespace TEST_FIGHTER_ORDNANCE {
    void Reset_();
}

namespace TEST_LUA_EVENT {
    void Reset_();
    int Count_();
    const orion2re::lua::game::s_ship_destroyed_event& Last_Event_();
} // namespace TEST_LUA_EVENT

DOCTEST_TEST_CASE("bombing uses unrounded point distance") {
    TEST_BOMBING_RANGE::Reset_();
    MOX::combat_data[1].weapons[0].weapon_id = WEAPON_NUCLEAR_BOMB;
    TECHDATA::weapons[WEAPON_NUCLEAR_BOMB].type = WEAPON_TYPE_BOMB;

    MOX::combat_data[1].x = 3;
    DOCTEST_CHECK(
        CMBTFIRE::Weapon_In_Range(0, 1, 0, 0, 0) == 1);

    MOX::combat_data[1].y = 2;
    DOCTEST_CHECK(
        CMBTFIRE::Weapon_In_Range(0, 1, 0, 0, 0) == 0);
}

DOCTEST_TEST_CASE("Transporters use configured bombing range") {
    TEST_BOMBING_RANGE::Reset_();
    TEST_BOMBING_RANGE::Set_Has_Transporters_(true);
    BOARDING::board_range.transporters = 5;
    MOX::combat_data[1].weapons[0].weapon_id = WEAPON_NUCLEAR_BOMB;
    TECHDATA::weapons[WEAPON_NUCLEAR_BOMB].type = WEAPON_TYPE_BOMB;

    MOX::combat_data[1].x = 5;
    DOCTEST_CHECK(
        CMBTFIRE::Weapon_In_Range(0, 1, 0, 0, 0) == 1);

    MOX::combat_data[1].y = 2;
    DOCTEST_CHECK(
        CMBTFIRE::Weapon_In_Range(0, 1, 0, 0, 0) == 0);
}

DOCTEST_TEST_CASE("destroyed Crystal releases captures and invalidates effects") {
    TEST_CRYSTAL_FLEET::Reset_();

    MOX::combat_data[1].owner = 11;
    MOX::combat_data[1].previous_owner = 11;

    MOX::combat_data[2].owner = 11;
    MOX::combat_data[2].previous_owner = 0;
    MOX::combat_data[2].ship_image_seg =
        reinterpret_cast<s_animation_header*>(1);

    CMBTFIRE::Destroy_Ship(1, 0, 0, 1);

    DOCTEST_CHECK(MOX::combat_data[2].owner == 0);
    DOCTEST_CHECK((MOX::combat_data[2].ship_image_seg == nullptr));
    DOCTEST_CHECK(TEST_CRYSTAL_FLEET::Cleanup_Count_() == 2);
    DOCTEST_CHECK(TEST_CRYSTAL_FLEET::Cleanup_Source_(0) == 1);
    DOCTEST_CHECK(TEST_CRYSTAL_FLEET::Cleanup_Source_(1) == 2);
}

DOCTEST_TEST_CASE("ship destruction publishes one semantic Lua event") {
    TEST_ENGINE_EXPLOSION::Reset_();
    TEST_LUA_EVENT::Reset_();
    MOX::apply_damage_to_planet = 0;
    MOX::combat_data[0].unit_status = 0;
    MOX::combat_data[1].unit_status = 0;

    struct s_probe {
        unsigned retired = 0;
        uint64_t subscription = 0;
        ~s_probe() {
            tactical_events::Unsubscribe(subscription);
        }
        static void Receive(const tactical_events::s_event* event, void* context) noexcept {
            if (event->kind == tactical_events::UNIT_RETIRED && event->unit == 1) ++((s_probe*)context)->retired;
        }
    } probe;
    probe.subscription = tactical_events::Subscribe(s_probe::Receive, &probe);
    tactical_events::s_battle_scope battle;
    battle.Open(0);

    CMBTFIRE::Destroy_Ship_By(1, 20, 20, 3, 0);

    DOCTEST_REQUIRE(TEST_LUA_EVENT::Count_() == 1);
    DOCTEST_CHECK(TEST_LUA_EVENT::Last_Event_().destroyed_combat_ship == 1);
    DOCTEST_CHECK(TEST_LUA_EVENT::Last_Event_().killer_combat_ship == 0);
    DOCTEST_CHECK(TEST_LUA_EVENT::Last_Event_().destruction_cause == 3);

    CMBTFIRE::Destroy_Ship_By(1, 20, 20, 3, 0);
    DOCTEST_CHECK(TEST_LUA_EVENT::Count_() == 1);
    DOCTEST_CHECK(probe.retired == 1);
    battle.Close(0);
}

DOCTEST_TEST_CASE("ship destruction invalidates target before retargeting missiles") {
    TEST_ENGINE_EXPLOSION::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;
    MOX::missile[0].type = WEAPON_INTERCEPTOR;
    MOX::missile[0].source_unit_idx = 0;
    MOX::missile[0].target_unit_idx = 1;

    CMBTFIRE::Destroy_Ship(1, 20, 20, 1);

    DOCTEST_CHECK(TEST_STASIS::Last_Retargeted_Missile_() == 0);
    DOCTEST_CHECK(TEST_STASIS::Retargeted_Ship_Status_() == 5);
}

DOCTEST_TEST_CASE("destroyed engines follow the configured explosion rule") {
    TEST_ENGINE_EXPLOSION::Reset_();
    s_combat_data& ship = MOX::combat_data[1];
    ship.owner = 0;
    ship.ftl_type = DRIVE_NUCLEAR_DRIVE;
    ship.drive_current_hits = 0;
    ship.structure_max = 100;

    CMBTFIRE::broken_engine_explodes = 1;
    DOCTEST_CHECK(CMBTFIRE::Is_Combat_Ship_Dead(1) == 3);

    CMBTFIRE::broken_engine_explodes = 0;
    DOCTEST_CHECK(CMBTFIRE::Is_Combat_Ship_Dead(1) == 0);
}

DOCTEST_TEST_CASE("disabled engine explosions make self destruct ordinary") {
    TEST_ENGINE_EXPLOSION::Reset_();
    CMBTFIRE::broken_engine_explodes = 0;

    CMBTFIRE::Destroy_Ship(1, 20, 20, 0);

    DOCTEST_CHECK(TEST_ENGINE_EXPLOSION::Destroy_FX_Type_() == 1);
}

DOCTEST_TEST_CASE("super fast drive explosions resolve spherical damage") {
    TEST_ENGINE_EXPLOSION::Reset_();
    MOX::super_fast_flag = 1;
    MOX::combat_data[1].previous_owner = 0;

    CMBTFIRE::Destroy_Ship(1, 20, 20, 3);

    DOCTEST_CHECK(MOX::combat_data[1].unit_status == 5);
    DOCTEST_CHECK(TEST_ENGINE_EXPLOSION::Explosion_Damage_Source_() == 1);
    DOCTEST_CHECK(TEST_ENGINE_EXPLOSION::Spherical_Effect_Type_() == 3);
    DOCTEST_CHECK(TEST_ENGINE_EXPLOSION::Spherical_Damage_() == 42);
}

DOCTEST_TEST_CASE("placing a ship in stasis cuts its continuing effects") {
    TEST_STASIS::Reset_();
    MOX::missile[0].type = WEAPON_NUCLEAR_MISSILE;
    MOX::missile[0].source_unit_idx = 0;
    MOX::missile[0].target_unit_idx = 1;

    CMBTFIRE::Place_Ship_In_Stasis(1, 0);

    DOCTEST_CHECK((MOX::combat_data[1].stasis_source_idx == 0));
    DOCTEST_CHECK(TEST_STASIS::Last_Retargeted_Missile_() == 0);
    DOCTEST_CHECK(TEST_STASIS::Last_Cleanup_Source_() == 1);
}

DOCTEST_TEST_CASE("missile damage keeps launch armor after source ship capture") {
    TEST_STASIS::Reset_();
    MOX::combat_data[1].owner = 2;
    MOX::missile[0].type = WEAPON_INTERCEPTOR;
    MOX::missile[0].owner = 1;
    MOX::missile[0].source_unit_idx = 1;

    int16_t damage = 1;
    CMBTFIRE::Apply_Damage_To_Missile(0, &damage, 0, 0);

    DOCTEST_CHECK(TEST_STASIS::Last_Missile_Hits_Owner_() == 1);
}

DOCTEST_TEST_CASE("heavy fighter missile targeting follows scramble setting") {
    TEST_STASIS::Reset_();
    TECHDATA::weapons[WEAPON_HEAVY_FIGHTER].type = WEAPON_TYPE_FIGHTER;
    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;

    CMBTFIRE::heavy_fighter_can_scramble = 0;
    DOCTEST_CHECK(
        CMBTFIRE::Weapon_Can_Fire_At_Missiles(
            WEAPON_HEAVY_FIGHTER, 0) == 0);
    DOCTEST_CHECK(
        CMBTFIRE::Weapon_Can_Fire_At_Missiles(
            WEAPON_INTERCEPTOR, 0) == 1);

    CMBTFIRE::heavy_fighter_can_scramble = 1;
    DOCTEST_CHECK(
        CMBTFIRE::Weapon_Can_Fire_At_Missiles(
            WEAPON_HEAVY_FIGHTER, 0) == 1);

    CMBTFIRE::heavy_fighter_can_scramble = 0;
}

DOCTEST_TEST_CASE("fighter beams apply the fleet ordnance bonus") {
    struct s_case {
        int16_t ordnance_bonus;
        int16_t expected_destroyed;
        int16_t expected_quantity;
        int16_t expected_hits_taken;
    };
    const s_case cases[] = {
        {100, 0, 2, 18},
        {150, 1, 1, 3},
    };

    for (const s_case& value : cases) {
        TEST_FIGHTER_ORDNANCE::Reset_();
        MOX::combat_data[1].owner = 2;
        MOX::ordnance_bonus[2] = value.ordnance_bonus;
        MOX::fighter_beam_type[2] = WEAPON_LASER_CANNON;
        TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 10;
        TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 10;

        MOX::missile[0].source_unit_idx = 1;
        MOX::missile[0].target_unit_idx = 1;
        MOX::missile[0].quantity = 1;
        MOX::missile[1].type = WEAPON_NUCLEAR_MISSILE;
        MOX::missile[1].quantity = 2;
        MOX::missile[1].hits_taken = 8;

        int16_t total_damage = 0;
        int16_t total_struct_damage = 0;
        CMBTFIRE::Fire_Fighter_Beam(
            0, 0, 1, &total_damage, &total_struct_damage);

        DOCTEST_CHECK(total_damage == value.expected_destroyed);
        DOCTEST_CHECK((
            MOX::missile[1].quantity == value.expected_quantity));
        DOCTEST_CHECK((
            MOX::missile[1].hits_taken == value.expected_hits_taken));
    }
}

DOCTEST_TEST_CASE("fighter point defense uses widened ordnance damage") {
    struct s_case {
        uint8_t point_defense;
        uint16_t modifier;
        int16_t expected_destroyed;
        int16_t expected_hits_taken;
    };
    const s_case cases[] = {
        {0, 50, 1, 0},
        {1, 50, 0, 23756},
        {1, 33, 0, 15679},
    };

    for (const s_case& value : cases) {
        TEST_FIGHTER_ORDNANCE::Reset_();
        TEST_STASIS::Set_Missile_Hits_(INT16_MAX);
        CMBTFIRE::fighters_use_point_defense = value.point_defense;
        CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].pd = value.modifier;
        MOX::combat_data[1].owner = 2;
        MOX::ordnance_bonus[2] = 145;
        MOX::fighter_beam_type[2] = WEAPON_LASER_CANNON;
        TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = INT16_MAX;
        TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = INT16_MAX;

        MOX::missile[0].source_unit_idx = 1;
        MOX::missile[0].target_unit_idx = 1;
        MOX::missile[0].quantity = 1;
        MOX::missile[1].type = WEAPON_NUCLEAR_MISSILE;
        MOX::missile[1].quantity = 2;

        int16_t total_damage = 0;
        int16_t total_struct_damage = 0;
        CMBTFIRE::Fire_Fighter_Beam(
            0, 0, 1, &total_damage, &total_struct_damage);

        DOCTEST_CHECK(total_damage == value.expected_destroyed);
        DOCTEST_CHECK((
            MOX::missile[1].hits_taken == value.expected_hits_taken));
    }

    CMBTFIRE::fighters_use_point_defense = 0;
}

DOCTEST_TEST_CASE("fighter beam maximum damage follows the clip setting") {
    struct s_case {
        uint8_t clip_damage;
        int16_t expected_hits_taken;
    };
    const s_case cases[] = {
        {0, 11},
        {1, 10},
    };

    for (const s_case& value : cases) {
        TEST_FIGHTER_ORDNANCE::Reset_();
        TEST_STASIS::Set_Missile_Hits_(100);
        CMBTFIRE::fighter_beam_no_plus_one = value.clip_damage;
        MOX::combat_data[1].owner = 2;
        MOX::ordnance_bonus[2] = 100;
        MOX::fighter_beam_type[2] = WEAPON_LASER_CANNON;
        TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 10;
        TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 20;

        MOX::missile[0].source_unit_idx = 1;
        MOX::missile[0].target_unit_idx = 1;
        MOX::missile[0].quantity = 1;
        MOX::missile[1].type = WEAPON_NUCLEAR_MISSILE;
        MOX::missile[1].quantity = 1;
        MOX::missile[1].hits_taken = -10;

        int16_t total_damage = 0;
        int16_t total_struct_damage = 0;
        CMBTFIRE::Fire_Fighter_Beam(
            0, 0, 1, &total_damage, &total_struct_damage);

        DOCTEST_CHECK(total_damage == 0);
        DOCTEST_CHECK((MOX::missile[1].hits_taken ==
            value.expected_hits_taken));
    }

    CMBTFIRE::fighter_beam_no_plus_one = 0;
}

DOCTEST_TEST_CASE("planet non-enveloping setting only suppresses planetary multiplier") {
    CMBTFIRE::planet_non_enveloping = 0;
    DOCTEST_CHECK(CMBTFIRE::Enveloping_Applies_To_Target(0));
    DOCTEST_CHECK(CMBTFIRE::Enveloping_Applies_To_Target(1));

    CMBTFIRE::planet_non_enveloping = 1;
    DOCTEST_CHECK_FALSE(CMBTFIRE::Enveloping_Applies_To_Target(0));
    DOCTEST_CHECK(CMBTFIRE::Enveloping_Applies_To_Target(1));

    CMBTFIRE::planet_non_enveloping = 0;
}

DOCTEST_TEST_CASE("marine-killing weapons honor the configured blocker") {
    TEST_STASIS::Reset_();
    s_combat_data& ship = MOX::combat_data[1];
    ship.marine_count = 10;
    ship.structure_max = INT16_MAX;

    CMBTFIRE::marine_kill_damage_blocker = SPECIAL_DAMPER_FIELD;
    int16_t damage = 10;
    int16_t shield_damage = 0;
    CMBTFIRE::Apply_Ship_Damage(
        1, &damage, &shield_damage, 0, 0, WEAPON_NATURAL_FLAG_KILLS_MARINES, 0, 0, 0);
    DOCTEST_CHECK(TEST_STASIS::Crew_Kills_() == 1);

    TEST_STASIS::Reset_();
    ship = MOX::combat_data[1];
    ship.marine_count = 10;
    ship.structure_max = INT16_MAX;
    CMBTFIRE::marine_kill_damage_blocker = SPECIAL_TRANSPORTERS;
    TEST_BOMBING_RANGE::Set_Has_Transporters_(true);
    damage = 10;
    shield_damage = 0;
    CMBTFIRE::Apply_Ship_Damage(
        1, &damage, &shield_damage, 0, 0, WEAPON_NATURAL_FLAG_KILLS_MARINES, 0, 0, 0);
    DOCTEST_CHECK(TEST_STASIS::Crew_Kills_() == 0);

    CMBTFIRE::marine_kill_damage_blocker = SPECIAL_DAMPER_FIELD;
}

DOCTEST_TEST_CASE("marine-killing weapons use the full positive byte divisor") {
    struct s_case {
        uint8_t divisor;
        int16_t expected_kills;
    };
    const s_case cases[] = {
        {1, 255},
        {127, 2},
        {128, 1},
        {255, 1},
    };

    for (const s_case& test_case : cases) {
        TEST_STASIS::Reset_();
        s_combat_data& ship = MOX::combat_data[1];
        ship.marine_count = 100;
        ship.structure_max = INT16_MAX;
        CMBTFIRE::marine_kill_damage = test_case.divisor;
        CMBTFIRE::marine_kill_damage_blocker = SPECIAL_DAMPER_FIELD;

        int16_t damage = 255;
        int16_t shield_damage = 0;
        CMBTFIRE::Apply_Ship_Damage(
            1, &damage, &shield_damage, 0, 0, WEAPON_NATURAL_FLAG_KILLS_MARINES, 0, 0, 0);

        DOCTEST_CHECK(TEST_STASIS::Crew_Kills_() == 1);
        DOCTEST_CHECK(TEST_STASIS::Last_Crew_Kill_Count_() ==
            test_case.expected_kills);
    }

    CMBTFIRE::marine_kill_damage = 5;
}

DOCTEST_TEST_CASE("planetary shields block biological weapons according to the setting") {
    TEST_STASIS::Reset_();
    MOX::combat_defender_player = 0;
    int16_t damage = 100;

    MOX::combat_data[0].shield_type = BUILDING_BARRIER_SHIELD;
    CMBTFIRE::Apply_Damage_To_Planet(
        &damage, 0, WEAPON_NATURAL_FLAG_BIOLOGICAL);
    DOCTEST_CHECK((MOX::combat_bomb_result.bio_hits_remaining == 0));

    MOX::combat_data[0].shield_type = BUILDING_FLUX_SHIELD;
    damage = 100;
    COMBINIT::planet.shields_block_bioweapons = 0;
    CMBTFIRE::Apply_Damage_To_Planet(
        &damage, 0, WEAPON_NATURAL_FLAG_BIOLOGICAL);
    DOCTEST_CHECK((MOX::combat_bomb_result.bio_hits_remaining == 1));

    MOX::combat_bomb_result = {};
    damage = 100;
    COMBINIT::planet.shields_block_bioweapons = 1;
    CMBTFIRE::Apply_Damage_To_Planet(
        &damage, 0, WEAPON_NATURAL_FLAG_BIOLOGICAL);
    DOCTEST_CHECK((MOX::combat_bomb_result.bio_hits_remaining == 0));

    COMBINIT::planet.shields_block_bioweapons = 0;
}

DOCTEST_TEST_CASE("structural analyzer applies its configured damage bonus") {
    struct s_case {
        int16_t has_structural_analyzer;
        uint16_t analyzer_bonus;
        int16_t expected_damage;
    };
    const s_case cases[] = {
        {0, 100, 103},
        {1, 0, 103},
        {1, 60, 164},
        {1, 100, 206},
    };

    for (const s_case& value : cases) {
        TEST_STASIS::Reset_();
        MOX::combat_data[1].structure_max = INT16_MAX;
        CMBTFIRE::analyzer_bonus = value.analyzer_bonus;

        int16_t damage = 103;
        int16_t shield_damage = 0;
        CMBTFIRE::Apply_Ship_Damage(
            1, &damage, &shield_damage, 0, 0, 0, value.has_structural_analyzer, 0, 0);

        DOCTEST_CHECK(damage == value.expected_damage);
        DOCTEST_CHECK(shield_damage == 0);
    }

    CMBTFIRE::analyzer_bonus = 100;
}

DOCTEST_TEST_CASE("absorber and damper percentage calculations preserve classic behavior") {
    CMBTFIRE::absorber_damage_reduction = 25;
    CMBTFIRE::damper_field_ratio = 25;
    DOCTEST_CHECK(CMBTFIRE::Energy_Absorber_Blocked_Damage(103) == 25);
    DOCTEST_CHECK(CMBTFIRE::Damper_Field_Remaining_Damage(103) == 25);

    CMBTFIRE::absorber_damage_reduction = 40;
    CMBTFIRE::damper_field_ratio = 60;
    DOCTEST_CHECK(CMBTFIRE::Energy_Absorber_Blocked_Damage(103) == 41);
    DOCTEST_CHECK(CMBTFIRE::Damper_Field_Remaining_Damage(103) == 61);

    CMBTFIRE::absorber_damage_reduction = 25;
    CMBTFIRE::damper_field_ratio = 25;
}

DOCTEST_TEST_CASE("absorber charge addition wraps at the signed word boundary") {
    TEST_STASIS::Reset_();
    s_combat_data& ship = MOX::combat_data[1];
    ship.special_device_flags[SPECIAL_ENERGY_ABSORBER / 8] |=
        (uint8_t)(1u << (SPECIAL_ENERGY_ABSORBER % 8));
    ship.reflected_damage_pool = 30000;
    ship.structure_max = INT16_MAX;
    CMBTFIRE::absorber_damage_reduction = 25;
    int16_t damage = 12000;
    int16_t system_damage = 0;

    CMBTFIRE::Apply_Ship_Damage(
        1, &damage, &system_damage, 0, 0, 0, 0, 0, 0);

    DOCTEST_CHECK(ship.reflected_damage_pool == -32536);
    DOCTEST_CHECK(CMBTFIRE::Is_Combat_Ship_Dead(1) != 4);
}

DOCTEST_TEST_CASE("ship ESD applies and reports the configured bonus once") {
    TEST_STASIS::Reset_();
    s_combat_data& ship = MOX::combat_data[1];
    ship.owner = 0;
    ship.structure_max = 100;
    TECHDATA::esd_bonus = 50;

    int16_t damage = 7;
    int16_t system_damage = 0;
    CMBTFIRE::Apply_Ship_Damage(
        1, &damage, &system_damage, 0, 0, WEAPON_NATURAL_FLAG_EXTRA_STRUCTURAL_DAMAGE, 0, 0, 0);

    DOCTEST_CHECK(ship.structure_damage == 3);
    DOCTEST_CHECK(damage == 10);
    DOCTEST_CHECK(system_damage == 0);
}

DOCTEST_TEST_CASE("ground defense structure applies the configured armor multiplier") {
    CMBTFIRE::ground_defense_armor_multiplier = 100;
    DOCTEST_CHECK(CMBTFIRE::Ground_Defense_Structure(0) == 100);
    DOCTEST_CHECK(CMBTFIRE::Ground_Defense_Structure(100) == 200);

    CMBTFIRE::ground_defense_armor_multiplier = 150;
    DOCTEST_CHECK(CMBTFIRE::Ground_Defense_Structure(0) == 150);
    DOCTEST_CHECK(CMBTFIRE::Ground_Defense_Structure(100) == 300);

    CMBTFIRE::ground_defense_armor_multiplier = 10000;
    DOCTEST_CHECK(CMBTFIRE::Ground_Defense_Structure(900) == 100000);

    CMBTFIRE::ground_defense_armor_multiplier = 100;
}

DOCTEST_TEST_CASE("beam mount damage defaults match classic HEF combinations") {
    TEST_BEAM_MODIFIERS::Reset_();

    struct s_case {
        uint16_t flags;
        bool has_hef;
        int16_t expected_hit_bonus;
        int16_t expected_damage;
    };
    const s_case cases[] = {
        {WEAPON_MOD_FLAG_POINT_DEFENSE, false, 25, 50},
        {0, false, 0, 100},
        {WEAPON_MOD_FLAG_HEAVY_MOUNT, false, 0, 150},
        {WEAPON_MOD_FLAG_POINT_DEFENSE, true, 25, 100},
        {0, true, 0, 150},
        {WEAPON_MOD_FLAG_HEAVY_MOUNT, true, 0, 200},
    };

    for (const s_case& value : cases) {
        TEST_BEAM_MODIFIERS::Set_Has_HEF_(value.has_hef);
        int32_t hit_bonus = 0;
        int32_t damage = 0;
        CMBTFIRE::Get_Beam_Weapon_Modifiers(
            0, 0, value.flags, 0, &hit_bonus, &damage);
        DOCTEST_CHECK(hit_bonus == value.expected_hit_bonus);
        DOCTEST_CHECK(damage == value.expected_damage);
    }
}

DOCTEST_TEST_CASE("beam modifiers use configured mount damage") {
    TEST_BEAM_MODIFIERS::Reset_();
    CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_PRESENT].heavy = 175;
    TEST_BEAM_MODIFIERS::Set_Has_HEF_(true);

    int32_t hit_bonus = 0;
    int32_t damage = 0;
    CMBTFIRE::Get_Beam_Weapon_Modifiers(
        0, 0, WEAPON_MOD_FLAG_HEAVY_MOUNT, 0, &hit_bonus, &damage);

    DOCTEST_CHECK(hit_bonus == 0);
    DOCTEST_CHECK(damage == 175);
}

DOCTEST_TEST_CASE("beam mount damage preserves the full configured word") {
    struct s_mount_case {
        uint16_t flags;
        bool has_hef;
    };
    const s_mount_case mount_cases[] = {
        {WEAPON_MOD_FLAG_POINT_DEFENSE, false},
        {0, false},
        {WEAPON_MOD_FLAG_HEAVY_MOUNT, false},
        {WEAPON_MOD_FLAG_POINT_DEFENSE, true},
        {0, true},
        {WEAPON_MOD_FLAG_HEAVY_MOUNT, true},
    };
    const uint16_t configured_values[] = {
        0,
        32767,
        32768,
        UINT16_MAX,
    };

    for (const s_mount_case& mount_case : mount_cases) {
        for (uint16_t configured_value : configured_values) {
            TEST_BEAM_MODIFIERS::Reset_();
            TEST_BEAM_MODIFIERS::Set_Has_HEF_(mount_case.has_hef);
            CMBTFIRE::s_mount_damage& mount_damage =
                CMBTFIRE::mount_dmg[mount_case.has_hef
                        ? CMBTFIRE::HIGH_ENERGY_FOCUS_PRESENT
                        : CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT];
            if (mount_case.flags & WEAPON_MOD_FLAG_POINT_DEFENSE) {
                mount_damage.pd = configured_value;
            } else if (mount_case.flags & WEAPON_MOD_FLAG_HEAVY_MOUNT) {
                mount_damage.heavy = configured_value;
            } else {
                mount_damage.nomod = configured_value;
            }

            int32_t hit_bonus = 0;
            int32_t damage = 0;
            CMBTFIRE::Get_Beam_Weapon_Modifiers(
                0, 0, mount_case.flags, 0, &hit_bonus, &damage);

            DOCTEST_CHECK(damage == configured_value);
        }
    }
}

DOCTEST_TEST_CASE("wide beam mount damage retains range penalties") {
    TEST_BEAM_MODIFIERS::Reset_();
    CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].nomod =
        UINT16_MAX;
    MOX::ranged_damage_penalty[2] = -30;

    int32_t hit_bonus = 0;
    int32_t damage = 0;
    CMBTFIRE::Get_Beam_Weapon_Modifiers(
        0, 2, 0, 0, &hit_bonus, &damage);

    DOCTEST_CHECK(damage == 65505);
}

DOCTEST_TEST_CASE("double-range settings independently control beam penalties") {
    struct s_case {
        uint16_t natural_mods;
        uint8_t accuracy_enabled;
        uint8_t damage_enabled;
        int16_t expected_accuracy;
        int16_t expected_damage;
    };
    const s_case cases[] = {
        {WEAPON_NATURAL_FLAG_DOUBLE_RANGE_PENALTY, 1, 0, -20, 90},
        {WEAPON_NATURAL_FLAG_DOUBLE_RANGE_PENALTY, 0, 1, -10, 80},
        {WEAPON_NATURAL_FLAG_DOUBLE_RANGE_PENALTY, 1, 1, -20, 80},
        {0, 1, 1, -10, 80},
    };

    for (const s_case& value : cases) {
        TEST_BEAM_MODIFIERS::Reset_();
        MOX::ranged_to_hit_penalty[2] = -10;
        MOX::ranged_damage_penalty[2] = -10;
        TECHDATA::weapons[0].natural_mods = value.natural_mods;
        CMBTFIRE::double_range_accuracy_penalty = value.accuracy_enabled;
        CMBTFIRE::double_range_damage_penalty = value.damage_enabled;

        int32_t hit_bonus = 0;
        int32_t damage = 0;
        CMBTFIRE::Get_Beam_Weapon_Modifiers(
            0, 2, 0, 0, &hit_bonus, &damage);

        DOCTEST_CHECK(hit_bonus == value.expected_accuracy);
        DOCTEST_CHECK(damage == value.expected_damage);
    }

    CMBTFIRE::double_range_accuracy_penalty = 1;
    CMBTFIRE::double_range_damage_penalty = 0;
}

DOCTEST_TEST_CASE("configured beam mount damage clamps negative modifiers") {
    TEST_BEAM_MODIFIERS::Reset_();
    CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].pd = 20;
    MOX::ranged_damage_penalty[0] = -30;

    int32_t hit_bonus = 0;
    int32_t damage = 0;
    CMBTFIRE::Get_Beam_Weapon_Modifiers(
        0, 0, WEAPON_MOD_FLAG_POINT_DEFENSE, 0, &hit_bonus, &damage);

    DOCTEST_CHECK(hit_bonus == 25);
    DOCTEST_CHECK(damage == 0);
}

DOCTEST_TEST_CASE("simplified beam formula keeps range penalties effective") {
    TEST_FAIR_BEAMS::Reset_();

    int16_t hit_x = 0;
    int16_t hit_y = 0;
    int16_t reflected_damage = 0;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 0, WEAPON_LASER_CANNON, 0, TARGET_TYPE_MISSILE, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x == 1);

    CMBTFIRE::simplified_beam_formula = 1;
    MOX::missile[0].hits_taken = 0;
    hit_x = 0;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 0, WEAPON_LASER_CANNON, 0, TARGET_TYPE_MISSILE, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x == 0);

    CMBTFIRE::simplified_beam_formula = 0;
}

DOCTEST_TEST_CASE("beam accuracy uses wide arithmetic before saturation") {
    int16_t hit_x = 0;
    int16_t hit_y = 0;
    int16_t reflected_damage = 0;

    TEST_FAIR_BEAMS::Reset_();
    CMBTFIRE::simplified_beam_formula = 1;
    MOX::combat_data[0].tac_val_attack = INT16_MAX;
    MOX::ranged_to_hit_penalty[8] = -1;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 0, WEAPON_LASER_CANNON, 0, TARGET_TYPE_MISSILE, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x == 1);

    TEST_FAIR_BEAMS::Reset_();
    TEST_STASIS::Set_Missile_Hits_(1);
    CMBTFIRE::simplified_beam_formula = 1;
    MOX::combat_data[0].tac_val_attack = 0;
    MOX::ranged_to_hit_penalty[8] = INT16_MAX;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 0, WEAPON_LASER_CANNON, WEAPON_MOD_FLAG_POINT_DEFENSE, TARGET_TYPE_MISSILE, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x == 1);

    TEST_FAIR_BEAMS::Reset_();
    CMBTFIRE::simplified_beam_formula = 1;
    MOX::combat_data[0].tac_val_attack = INT16_MAX;
    MOX::combat_data[1].tac_val_defense = INT16_MIN;
    MOX::combat_data[1].structure_max = INT16_MAX;
    MOX::ranged_to_hit_penalty[8] = 0;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 1, WEAPON_LASER_CANNON, 0, TARGET_TYPE_SHIP, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x > 0);

    TEST_FAIR_BEAMS::Reset_();
    TEST_STASIS::Set_Missile_Hits_(1);
    MOX::combat_data[0].tac_val_attack = 0;
    MOX::ranged_to_hit_penalty[8] = INT16_MIN;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 0, WEAPON_LASER_CANNON, 0, TARGET_TYPE_MISSILE, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x == 0);
}

DOCTEST_TEST_CASE("beam hit threshold uses the configured fixed value") {
    TEST_FAIR_BEAMS::Reset_();
    MOX::combat_data[0].tac_val_attack = 20;
    TEST_STASIS::Set_Missile_Hits_(20);
    MOX::ranged_to_hit_penalty[8] = 0;

    int16_t hit_x = 0;
    int16_t hit_y = 0;
    int16_t reflected_damage = 0;
    CMBTFIRE::hit_threshold_fixed_value = 40;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 0, WEAPON_LASER_CANNON, 0, TARGET_TYPE_MISSILE, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x == 1);

    TEST_FAIR_BEAMS::Reset_();
    MOX::combat_data[0].tac_val_attack = 20;
    TEST_STASIS::Set_Missile_Hits_(20);
    MOX::ranged_to_hit_penalty[8] = 0;
    hit_x = 0;
    CMBTFIRE::hit_threshold_fixed_value = 41;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 0, WEAPON_LASER_CANNON, 0, TARGET_TYPE_MISSILE, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x == 0);
}

DOCTEST_TEST_CASE("beam hit threshold cap applies in both formula modes") {
    struct s_case {
        uint8_t simplified;
        uint8_t threshold;
        int16_t expected_destroyed;
        int16_t expected_hits_taken;
    };
    const s_case cases[] = {
        {0, 40, 1, 0},
        {1, 40, 1, 0},
        {0, 100, 0, 10},
        {1, 100, 0, 10},
    };

    for (const s_case& value : cases) {
        TEST_FAIR_BEAMS::Reset_();
        TEST_FAIR_BEAMS::Set_Random_Value_(95);
        TEST_STASIS::Set_Missile_Hits_(20);
        CMBTFIRE::simplified_beam_formula = value.simplified;
        CMBTFIRE::hit_threshold_fixed_value = value.threshold;
        MOX::combat_data[0].tac_val_attack = 0;
        MOX::ranged_to_hit_penalty[8] = 0;
        TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 10;
        TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 20;

        int16_t hit_x = 0;
        int16_t hit_y = 0;
        int16_t reflected_damage = 0;
        CMBTFIRE::Fire_Beam_Weapon(
            0, 0, WEAPON_LASER_CANNON, 0, TARGET_TYPE_MISSILE, &hit_x, &hit_y, &reflected_damage);

        DOCTEST_CHECK(hit_x == value.expected_destroyed);
        DOCTEST_CHECK((
            MOX::missile[0].hits_taken == value.expected_hits_taken));
    }
}

DOCTEST_TEST_CASE("ion weapons target Antarans only when enabled") {
    TEST_STASIS::Reset_();
    MOX::combat_data[1].previous_owner = NONPLAYER_SHIP_TYPE_ANTARAN;
    TECHDATA::weapons[WEAPON_ION_PULSE_CANNON].natural_mods =
        WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE;

    DOCTEST_CHECK_FALSE(CMBTFIRE::Target_Is_Legal_For_Weapon(
        WEAPON_ION_PULSE_CANNON, 0, 1, TARGET_TYPE_SHIP));

    CMBTFIRE::ion_affects_antarans = 1;
    DOCTEST_CHECK(CMBTFIRE::Target_Is_Legal_For_Weapon(
        WEAPON_ION_PULSE_CANNON, 0, 1, TARGET_TYPE_SHIP));

    CMBTFIRE::ion_affects_antarans = 0;
}

DOCTEST_TEST_CASE("Displacement Device beam misses use their independent setting") {
    TEST_FAIR_BEAMS::Reset_();
    MOX::combat_data[1].special_device_flags[SPECIAL_DISPLACEMENT_DEVICE / 8] |=
        (uint8_t)(1u << (SPECIAL_DISPLACEMENT_DEVICE % 8));
    CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent = 10;
    CMBTMIS::missile_defense.displacement_device_missiles_miss_percent = 100;

    int16_t hit_x = 0;
    int16_t hit_y = 0;
    int16_t reflected_damage = 0;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 1, WEAPON_LASER_CANNON, 0, TARGET_TYPE_SHIP, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x > 0);

    TEST_FAIR_BEAMS::Reset_();
    MOX::combat_data[1].special_device_flags[SPECIAL_DISPLACEMENT_DEVICE / 8] |=
        (uint8_t)(1u << (SPECIAL_DISPLACEMENT_DEVICE % 8));
    CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent = 30;
    CMBTMIS::missile_defense.displacement_device_missiles_miss_percent = 0;

    hit_x = 0;
    CMBTFIRE::Fire_Beam_Weapon(
        0, 1, WEAPON_LASER_CANNON, 0, TARGET_TYPE_SHIP, &hit_x, &hit_y, &reflected_damage);
    DOCTEST_CHECK(hit_x == 0);
}

DOCTEST_TEST_CASE("Energy Absorber uses the Displacement Device beam setting") {
    TEST_STASIS::Reset_();
    TEST_STASIS::Capture_Internal_Damage_();
    MOX::apply_damage_to_planet = 0;
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[0].reflected_damage_pool = 10;
    MOX::combat_data[1].structure_max = 100;
    MOX::combat_data[1].special_device_flags[SPECIAL_DISPLACEMENT_DEVICE / 8] |=
        (uint8_t)(1u << (SPECIAL_DISPLACEMENT_DEVICE % 8));
    MOX::ordnance_bonus[0] = 100;
    CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent = 10;
    CMBTMIS::missile_defense.displacement_device_missiles_miss_percent = 100;

    CMBTFIRE::Energy_Absorber_Attack(0, 1);

    DOCTEST_CHECK(TEST_STASIS::Captured_Internal_Damage_() == 10);

    TEST_STASIS::Reset_();
    TEST_STASIS::Capture_Internal_Damage_();
    MOX::apply_damage_to_planet = 0;
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[0].reflected_damage_pool = 10;
    MOX::combat_data[1].structure_max = 100;
    MOX::combat_data[1].special_device_flags[SPECIAL_DISPLACEMENT_DEVICE / 8] |=
        (uint8_t)(1u << (SPECIAL_DISPLACEMENT_DEVICE % 8));
    MOX::ordnance_bonus[0] = 100;
    CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent = 30;
    CMBTMIS::missile_defense.displacement_device_missiles_miss_percent = 0;
    TEST_FAIR_BEAMS::Set_Random_Value_(20);

    CMBTFIRE::Energy_Absorber_Attack(0, 1);

    DOCTEST_CHECK(TEST_STASIS::Captured_Internal_Damage_() == 0);
}

DOCTEST_TEST_CASE("defensive installation range bonus is clamped and identity based") {
    TEST_WEAPON_RANGE::Reset_();
    s_combat_data& station = MOX::combat_data[1];
    station.ship_idx = -1;
    station.ftl_type = DRIVE_NO_DRIVE;
    station.weapons[0].weapon_id = WEAPON_LASER_CANNON;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;

    CMBTFIRE::defensive_range_bonus = 0;
    DOCTEST_CHECK(CMBTFIRE::Weapon_In_Range(0, 1, 2, 0, 9) == 0);

    CMBTFIRE::defensive_range_bonus = 1;
    DOCTEST_CHECK(CMBTFIRE::Weapon_In_Range(0, 1, 2, 0, 9) != 0);

    CMBTFIRE::defensive_range_bonus = UINT8_MAX;
    DOCTEST_CHECK(CMBTFIRE::Weapon_In_Range(0, 1, 2, 0, 9) != 0);

    station.ship_idx = 4;
    DOCTEST_CHECK(CMBTFIRE::Weapon_In_Range(0, 1, 2, 0, 9) == 0);
}

DOCTEST_TEST_CASE("configured weapon ranges replace fixed special limits") {
    TEST_WEAPON_RANGE::Reset_();
    s_combat_weapon_slot& weapon = MOX::combat_data[0].weapons[0];

    weapon.weapon_id = WEAPON_TRACTOR_BEAM;
    TECHDATA::weapons[WEAPON_TRACTOR_BEAM].type = WEAPON_TYPE_SPECIAL;
    CMBTFIRE::weapon_range.tractor_beam = 7;
    DOCTEST_CHECK(CMBTFIRE::Weapon_In_Range(0, 0, 1, 0, 7) == 1);
    DOCTEST_CHECK(CMBTFIRE::Weapon_In_Range(0, 0, 1, 0, 8) == 0);

    weapon.weapon_id = WEAPON_PROTON_TORPEDO;
    TECHDATA::weapons[WEAPON_PROTON_TORPEDO].type = WEAPON_TYPE_TORPEDO;
    CMBTFIRE::weapon_range.proton_torpedo = 10;
    DOCTEST_CHECK(CMBTFIRE::Weapon_In_Range(0, 0, 1, 0, 10) == 1);
    DOCTEST_CHECK(CMBTFIRE::Weapon_In_Range(0, 0, 1, 0, 11) == 0);
}

DOCTEST_TEST_CASE("spherical ship range uses configured center-to-edge boundary") {
    TEST_WEAPON_RANGE::Reset_();
    TEST_WEAPON_RANGE::Set_Center_Offset_(0, 0);
    TEST_WEAPON_RANGE::Set_Center_Offset_(1, 10);
    CMBTFIRE::spherical_range_to_ship.pulsar = 6;

    MOX::combat_data[1].x = 4;
    MOX::combat_data[1].y = 2;
    DOCTEST_CHECK(CMBTFIRE::Spherical_Effect_In_Range(
        0, 1, TARGET_TYPE_SHIP, CMBTFIRE::SPHERICAL_EFFECT_PULSAR));

    MOX::combat_data[1].x = 5;
    DOCTEST_CHECK_FALSE(CMBTFIRE::Spherical_Effect_In_Range(
        0, 1, TARGET_TYPE_SHIP, CMBTFIRE::SPHERICAL_EFFECT_PULSAR));
}

DOCTEST_TEST_CASE("spherical flyer range includes its configured point boundary") {
    TEST_WEAPON_RANGE::Reset_();
    TEST_WEAPON_RANGE::Set_Center_Offset_(0, 0);
    CMBTFIRE::spherical_range_to_missile.spatial_compressor = 96;

    MOX::missile[0].x = 80;
    MOX::missile[0].y = 32;
    DOCTEST_CHECK(CMBTFIRE::Spherical_Effect_In_Range(
        0, 0, TARGET_TYPE_MISSILE, CMBTFIRE::SPHERICAL_EFFECT_SPATIAL_COMPRESSOR));

    MOX::missile[0].x = 81;
    DOCTEST_CHECK_FALSE(CMBTFIRE::Spherical_Effect_In_Range(
        0, 0, TARGET_TYPE_MISSILE, CMBTFIRE::SPHERICAL_EFFECT_SPATIAL_COMPRESSOR));
}

DOCTEST_TEST_CASE("spherical effects select independent configured radii") {
    TEST_WEAPON_RANGE::Reset_();
    TEST_WEAPON_RANGE::Set_Center_Offset_(0, 0);
    MOX::missile[0].x = 100;
    CMBTFIRE::spherical_range_to_missile = {
        99,
        100,
        101,
        102,
    };

    DOCTEST_CHECK_FALSE(CMBTFIRE::Spherical_Effect_In_Range(
        0, 0, TARGET_TYPE_MISSILE, CMBTFIRE::SPHERICAL_EFFECT_PULSAR));
    DOCTEST_CHECK(CMBTFIRE::Spherical_Effect_In_Range(
        0, 0, TARGET_TYPE_MISSILE, CMBTFIRE::SPHERICAL_EFFECT_SPATIAL_COMPRESSOR));
    DOCTEST_CHECK(CMBTFIRE::Spherical_Effect_In_Range(
        0, 0, TARGET_TYPE_MISSILE, CMBTFIRE::SPHERICAL_EFFECT_PLASMA_FLUX));
    DOCTEST_CHECK(CMBTFIRE::Spherical_Effect_In_Range(
        0, 0, TARGET_TYPE_MISSILE, CMBTFIRE::SPHERICAL_EFFECT_DRIVE_EXPLOSION));
}