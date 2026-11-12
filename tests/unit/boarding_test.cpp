#include "pch.h"

#include "doctest/doctest.h"
#include "game/boarding.h"
#include "game/cmbtai.h"
#include "game/cmbtfir2.h"
#include "game/combat1.h"
#include "game/techdata.h"

namespace TEST_BOARDING {
    void Reset_();
    int16_t Killed_Officer_();
    int16_t Pooled_Officer_();
    void Set_Combat_Shield_Max_(int16_t value);
    void Set_Raid_Choice_(int16_t value);
    int Raid_Selection_Count_();
    void Prepare_Long_Raid_();
    void Prepare_Oversized_Raid_();
    const char* Last_Raid_Message_();
    int16_t Boarding_Attacker_Bonus_();
    int16_t Boarding_Defender_Bonus_();
    int16_t Paragraph_Value_(int16_t index);
    bool Last_Result_Was_Timed_();
    void Set_Random_Value_(uint32_t value);
    int Boarding_Init_Count_();
    int Warning_Count_();
    const char* Last_Warning_();
    int Network_Message_Count_();
    int16_t Network_Marines_();
    int16_t Network_Action_();
    int Network_Wait_Count_();
}

DOCTEST_TEST_CASE("boarding defensive range uses real movement and turning costs") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].x = 3;
    MOX::combat_data[1].y = 4;
    MOX::combat_data[1].facing_dir = CMBTMOV1::Get_Facing(3, 4, 0, 0);
    MOX::combat_data[0].weapons[0].weapon_id = WEAPON_LASER_CANNON;
    MOX::combat_data[0].weapons[0].shots_left = 1;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    DOCTEST_CHECK(CMBTMOV1::Move_Cost(1, 0, 0) == 5);
    DOCTEST_CHECK(CMBTFIR2::Max_Defensive_Fire_Range(0) == 5);
    MOX::combat_data[1].facing_dir = (MOX::combat_data[1].facing_dir + 8) % 16;
    DOCTEST_CHECK(CMBTMOV1::Turn_Cost(1) == 2);
    DOCTEST_CHECK(CMBTMOV1::Move_Cost(1, 0, 0) == 21);
    struct_::Set_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_INERTIAL_STABILIZER);
    DOCTEST_CHECK(CMBTMOV1::Turn_Cost(1) == 1);
    DOCTEST_CHECK(CMBTMOV1::Move_Cost(1, 0, 0) == 13);
    struct_::Clear_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_INERTIAL_STABILIZER);
    struct_::Set_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_INERTIAL_NULLIFIER);
    DOCTEST_CHECK(CMBTMOV1::Turn_Cost(1) == 0);
    DOCTEST_CHECK(CMBTMOV1::Move_Cost(1, 0, 0) == 5);
}

DOCTEST_TEST_CASE("boarding range helpers preserve missile coordinates and initiative stasis rules") {
    TEST_BOARDING::Reset_();
    MOX::missile[0].x = 60;
    MOX::missile[0].y = 80;
    DOCTEST_CHECK(CMBTAI::Range_To_Missile(0, 0) == 2);
    MOX::combat_data[1].stasis_source_idx = 0;
    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Ship_Participates_In_Turn_Order(1, false));
    MOX::combat_init = 1;
    DOCTEST_CHECK(COMBAT1::Combat_Ship_Participates_In_Turn_Order(1, false));
    MOX::combat_data[1].is_captured = 1;
    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Ship_Participates_In_Turn_Order(1, false));
}

DOCTEST_TEST_CASE("boarding rejects targets in stasis") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[1].stasis_source_idx = 0;

    DOCTEST_CHECK(
        BOARDING::Filter_Boarding(0, 1) ==
        BOARDING::e_boarding_filter::target_in_stasis);
}

DOCTEST_TEST_CASE("classic boarding permits black-hole affected ships") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].black_hole_source_idx = 1;
    MOX::combat_data[1].black_hole_source_idx = 0;

    DOCTEST_CHECK(
        BOARDING::Filter_Boarding(0, 1) ==
        BOARDING::e_boarding_filter::allowed);
}

DOCTEST_TEST_CASE("disabled black-hole boarding filters both participants") {
    TEST_BOARDING::Reset_();
    BOARDING::board_in_black_hole = 0;
    MOX::combat_data[1].black_hole_source_idx = 0;

    DOCTEST_CHECK(
        BOARDING::Filter_Boarding(0, 1) ==
        BOARDING::e_boarding_filter::target_in_black_hole);

    MOX::combat_data[1].black_hole_source_idx = COMBAT_SHIP_NONE;
    MOX::combat_data[0].black_hole_source_idx = 1;
    DOCTEST_CHECK(
        BOARDING::Filter_Boarding(0, 1) ==
        BOARDING::e_boarding_filter::attacker_in_black_hole);
}

DOCTEST_TEST_CASE("boarding rejects phased-out targets") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[1].special_status_flag = 4;

    DOCTEST_CHECK(
        BOARDING::Filter_Boarding(0, 1) ==
        BOARDING::e_boarding_filter::target_phased_out);
}

DOCTEST_TEST_CASE("manual boarding warns when the target is phased out") {
    TEST_BOARDING::Reset_();
    MOX::cur_ship = 0;
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[0].marine_count = 1;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].ftl_type = DRIVE_NO_DRIVE;
    MOX::combat_data[1].special_status_flag = 4;

    COMBAT1::Initiate_Boarding_Action(0, 1);

    DOCTEST_CHECK(TEST_BOARDING::Warning_Count_() == 1);
    DOCTEST_CHECK(
        strcmp(TEST_BOARDING::Last_Warning_(),
               "Phased out ships may not be boarded.") == 0);
    DOCTEST_CHECK(TEST_BOARDING::Boarding_Init_Count_() == 0);
}

DOCTEST_TEST_CASE("manual black-hole warning precedes the no-marines rejection") {
    TEST_BOARDING::Reset_();
    BOARDING::board_in_black_hole = 0;
    MOX::cur_ship = 0;
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[0].marine_count = 0;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].ftl_type = DRIVE_NO_DRIVE;
    MOX::combat_data[1].black_hole_source_idx = 0;

    COMBAT1::Initiate_Boarding_Action(0, 1);

    DOCTEST_CHECK(TEST_BOARDING::Warning_Count_() == 1);
    DOCTEST_CHECK(
        strcmp(TEST_BOARDING::Last_Warning_(),
               "Ships trapped in a black hole may not be boarded.") == 0);
    DOCTEST_CHECK(TEST_BOARDING::Boarding_Init_Count_() == 0);
}

DOCTEST_TEST_CASE("AI boarding silently rejects phased-out targets") {
    TEST_BOARDING::Reset_();
    MOX::cur_ship = 0;
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[0].marine_count = 1;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].ftl_type = DRIVE_NO_DRIVE;
    MOX::combat_data[1].special_status_flag = 4;

    DOCTEST_CHECK(CMBTAI::Boarding_Action_Type(0, 1) == 0);
    DOCTEST_CHECK(TEST_BOARDING::Warning_Count_() == 0);
    DOCTEST_CHECK(TEST_BOARDING::Boarding_Init_Count_() == 0);
}

DOCTEST_TEST_CASE("AI rejects transporter-only Damper boarding when divisor is zero") {
    TEST_BOARDING::Reset_();
    MOX::cur_ship = 0;
    MOX::combat_data[0].owner = 8;
    MOX::combat_data[0].marine_count = 12;
    MOX::combat_data[0].x = 0;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].current_speed = 1;
    MOX::combat_data[1].x = 6;
    struct_::Set_Bit_Field(
        MOX::combat_data[0].special_device_flags, SPECIAL_TRANSPORTERS);
    struct_::Set_Bit_Field(
        MOX::combat_data[1].special_device_flags, SPECIAL_DAMPER_FIELD);
    BOARDING::boarding.damper_survival_divisor = 0;

    DOCTEST_CHECK(CMBTAI::Boarding_Action_Type(0, 1) == 0);

    MOX::combat_data[1].current_speed = 0;
    MOX::combat_data[1].x = 3;
    DOCTEST_CHECK(CMBTAI::Boarding_Action_Type(0, 1) == 1);
}

DOCTEST_TEST_CASE("AI uses the configured positive Damper survival divisor") {
    TEST_BOARDING::Reset_();
    MOX::cur_ship = 0;
    MOX::combat_data[0].owner = 8;
    MOX::combat_data[0].marine_count = 12;
    MOX::combat_data[0].x = 0;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].current_speed = 1;
    MOX::combat_data[1].x = 6;
    struct_::Set_Bit_Field(
        MOX::combat_data[0].special_device_flags, SPECIAL_TRANSPORTERS);
    struct_::Set_Bit_Field(
        MOX::combat_data[1].special_device_flags, SPECIAL_DAMPER_FIELD);
    BOARDING::boarding.damper_survival_divisor = 2;
    MOX::PLAYER_NUM = 8;
    MOX::net_destination = 1;
    MOX::net_flag = 1;
    MOX::combat_data[1].owner = 8;

    CMBTAI::Resolve_Ai_Boarding(0, 1, 1);

    DOCTEST_CHECK(TEST_BOARDING::Network_Message_Count_() == 1);
    DOCTEST_CHECK(TEST_BOARDING::Network_Marines_() == 3);
    DOCTEST_CHECK(TEST_BOARDING::Network_Action_() == 1);
    DOCTEST_CHECK(TEST_BOARDING::Network_Wait_Count_() == 1);
    DOCTEST_CHECK(MOX::combat_data[0].marine_count == 9);
}

DOCTEST_TEST_CASE("AI cancels boarding when positive Damper attrition leaves no survivor") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].marine_count = 6;
    MOX::combat_data[0].x = 0;
    MOX::combat_data[1].current_speed = 1;
    MOX::combat_data[1].x = 6;
    struct_::Set_Bit_Field(
        MOX::combat_data[0].special_device_flags, SPECIAL_TRANSPORTERS);
    struct_::Set_Bit_Field(
        MOX::combat_data[1].special_device_flags, SPECIAL_DAMPER_FIELD);
    BOARDING::boarding.damper_survival_divisor = 7;

    CMBTAI::Resolve_Ai_Boarding(0, 1, 1);

    DOCTEST_CHECK(TEST_BOARDING::Network_Message_Count_() == 0);
    DOCTEST_CHECK(MOX::combat_data[0].marine_count == 3);
}

DOCTEST_TEST_CASE("AI resolver preserves marines for a prohibited zero divisor") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].marine_count = 12;
    MOX::combat_data[0].x = 0;
    MOX::combat_data[1].current_speed = 1;
    MOX::combat_data[1].x = 6;
    struct_::Set_Bit_Field(
        MOX::combat_data[0].special_device_flags, SPECIAL_TRANSPORTERS);
    struct_::Set_Bit_Field(
        MOX::combat_data[1].special_device_flags, SPECIAL_DAMPER_FIELD);
    BOARDING::boarding.damper_survival_divisor = 0;

    CMBTAI::Resolve_Ai_Boarding(0, 1, 1);

    DOCTEST_CHECK(MOX::combat_data[0].marine_count == 12);
}

DOCTEST_TEST_CASE("classic captures retain marines above hull capacity") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[1].ship_idx = 1;
    MOX::combat_data[1].size_class = HULL_CRUISER;
    TECHDATA::hull_data[HULL_CRUISER].marines = 12;

    DOCTEST_CHECK(BOARDING::Captured_Marine_Count(1, 20) == 20);
}

DOCTEST_TEST_CASE("enabled capture limits account for working troop pods") {
    TEST_BOARDING::Reset_();
    BOARDING::kill_excess_marines_after_capture = 1;
    MOX::combat_data[1].ship_idx = 1;
    MOX::combat_data[1].ftl_type = DRIVE_HYPER_DRIVE;
    MOX::combat_data[1].size_class = HULL_CRUISER;
    TECHDATA::hull_data[HULL_CRUISER].marines = 12;

    DOCTEST_CHECK(BOARDING::Captured_Marine_Count(1, 30) == 12);

    struct_::Set_Bit_Field(
        MOX::combat_data[1].special_device_flags, SPECIAL_TROOP_PODS);
    DOCTEST_CHECK(BOARDING::Captured_Marine_Count(1, 30) == 24);

    struct_::Set_Bit_Field(
        MOX::combat_data[1].special_device_damage_flags, SPECIAL_TROOP_PODS);
    DOCTEST_CHECK(BOARDING::Captured_Marine_Count(1, 30) == 12);
}

DOCTEST_TEST_CASE("captured bases use orbital hull marine capacity") {
    TEST_BOARDING::Reset_();
    BOARDING::kill_excess_marines_after_capture = 1;
    MOX::combat_data[1].ship_idx = -1;
    MOX::combat_data[1].ftl_type = DRIVE_NO_DRIVE;
    MOX::combat_data[1].size_class = HULL_CRUISER;
    TECHDATA::hull_data[HULL_STAR_BASE].marines = 20;

    DOCTEST_CHECK(BOARDING::Captured_Marine_Count(1, 30) == 20);
}

DOCTEST_TEST_CASE("persistent driveless ships use their own marine capacity") {
    TEST_BOARDING::Reset_();
    BOARDING::kill_excess_marines_after_capture = 1;
    MOX::combat_data[1].ship_idx = 1;
    MOX::combat_data[1].ftl_type = DRIVE_NO_DRIVE;
    MOX::combat_data[1].size_class = HULL_CRUISER;
    TECHDATA::hull_data[HULL_CRUISER].marines = 12;
    TECHDATA::hull_data[HULL_STAR_BASE].marines = 20;

    DOCTEST_CHECK(BOARDING::Captured_Marine_Count(1, 30) == 12);
}

DOCTEST_TEST_CASE("capture limits preserve unsigned hull marine capacities") {
    TEST_BOARDING::Reset_();
    BOARDING::kill_excess_marines_after_capture = 1;
    MOX::combat_data[1].ship_idx = 1;
    MOX::combat_data[1].ftl_type = DRIVE_HYPER_DRIVE;
    MOX::combat_data[1].size_class = HULL_CRUISER;
    TECHDATA::hull_data[HULL_CRUISER].marines = UINT16_MAX;

    DOCTEST_CHECK(BOARDING::Captured_Marine_Count(1, 30) == 30);

    struct_::Set_Bit_Field(
        MOX::combat_data[1].special_device_flags, SPECIAL_TROOP_PODS);
    DOCTEST_CHECK(BOARDING::Captured_Marine_Count(1, 30) == 30);
}

DOCTEST_TEST_CASE("generated Antaran ships use normal hull marine capacity") {
    TEST_BOARDING::Reset_();
    BOARDING::kill_excess_marines_after_capture = 1;
    MOX::combat_data[1].ship_idx = -1;
    MOX::combat_data[1].ftl_type = DRIVE_HYPER_DRIVE;
    MOX::combat_data[1].size_class = HULL_CRUISER;
    TECHDATA::hull_data[HULL_CRUISER].marines = 12;
    TECHDATA::hull_data[HULL_STAR_BASE].marines = 20;

    DOCTEST_CHECK(BOARDING::Captured_Marine_Count(1, 30) == 12);
}

DOCTEST_TEST_CASE("capture includes both ships' crew experience bonuses") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[0].crew_quality = CREW_QUALITY_VETERAN;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].crew_quality = CREW_QUALITY_ELITE;
    MOX::crew_data[CREW_QUALITY_VETERAN].ground = 10;
    MOX::crew_data[CREW_QUALITY_ELITE].ground = 15;

    CMBTFIR2::Resolve_Capture(0, 1, 1, 0, 0);

    DOCTEST_CHECK(TEST_BOARDING::Boarding_Attacker_Bonus_() == 10);
    DOCTEST_CHECK(TEST_BOARDING::Boarding_Defender_Bonus_() == 15);
}

DOCTEST_TEST_CASE("capture report strengths include AI difficulty modifiers") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;

    CMBTFIR2::Resolve_Capture(0, 1, 1, 0, 0);

    DOCTEST_CHECK(TEST_BOARDING::Paragraph_Value_(6) == 100);
    DOCTEST_CHECK(TEST_BOARDING::Paragraph_Value_(7) == 0);
}

DOCTEST_TEST_CASE("classic telepathic recapture retains captured state") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].is_captured = 1;
    MOX::player[0].traits[TRAIT_TELEPATHIC] = 1;

    CMBTFIR2::Capture_Ship(1, 0, 0);

    DOCTEST_CHECK(MOX::combat_data[1].owner == 0);
    DOCTEST_CHECK(MOX::combat_data[1].is_captured == 1);
}

DOCTEST_TEST_CASE("enabled telepathic recapture clears captured state") {
    TEST_BOARDING::Reset_();
    BOARDING::telepathic_multiple_capture = 1;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].is_captured = 1;
    MOX::player[0].traits[TRAIT_TELEPATHIC] = 1;

    CMBTFIR2::Capture_Ship(1, 0, 0);

    DOCTEST_CHECK(MOX::combat_data[1].owner == 0);
    DOCTEST_CHECK(MOX::combat_data[1].is_captured == 0);
}

DOCTEST_TEST_CASE("enabled non-telepathic capture sets captured state") {
    TEST_BOARDING::Reset_();
    BOARDING::telepathic_multiple_capture = 1;
    MOX::combat_data[1].owner = 1;

    CMBTFIR2::Capture_Ship(1, 0, 0);

    DOCTEST_CHECK(MOX::combat_data[1].is_captured == 1);
}

DOCTEST_TEST_CASE("Quantum Detonator capture threshold is configurable") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[1].owner = 1;
    struct_::Set_Bit_Field(
        MOX::combat_data[1].special_device_flags, SPECIAL_QUANTUM_DETONATOR);
    TEST_BOARDING::Set_Random_Value_(6);
    CMBTFIR2::quantum_detonator.explode_chance = 6;

    CMBTFIR2::Capture_Ship(1, 0, 0);
    DOCTEST_CHECK(MOX::combat_data[1].owner == 0);

    TEST_BOARDING::Reset_();
    MOX::combat_data[1].owner = 1;
    struct_::Set_Bit_Field(
        MOX::combat_data[1].special_device_flags, SPECIAL_QUANTUM_DETONATOR);
    TEST_BOARDING::Set_Random_Value_(6);
    CMBTFIR2::quantum_detonator.explode_chance = 7;

    CMBTFIR2::Capture_Ship(1, 0, 0);
    DOCTEST_CHECK(MOX::combat_data[1].owner == 1);
}

DOCTEST_TEST_CASE("capture preserves Antaran origin") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].previous_owner = MAX_PLAYERS;

    CMBTFIR2::Capture_Ship(1, 0, 0);

    DOCTEST_CHECK(MOX::combat_data[1].owner == 0);
    DOCTEST_CHECK(MOX::combat_data[1].previous_owner == MAX_PLAYERS);
}

DOCTEST_TEST_CASE("captured ships retain an existing turn-order entry") {
    int16_t turn_order[4] = { 3, 1, -10, -1 };

    COMBAT1::Restore_Captured_Ship_To_Turn_Order(
        turn_order, 4, 1);

    DOCTEST_CHECK(turn_order[0] == 3);
    DOCTEST_CHECK(turn_order[1] == 1);
    DOCTEST_CHECK(turn_order[2] == -10);
}

DOCTEST_TEST_CASE("captured ships reuse a vacant turn-order entry") {
    int16_t turn_order[4] = { 3, -1, -10, -1 };

    COMBAT1::Restore_Captured_Ship_To_Turn_Order(
        turn_order, 4, 1);

    DOCTEST_CHECK(turn_order[0] == 3);
    DOCTEST_CHECK(turn_order[1] == 1);
    DOCTEST_CHECK(turn_order[2] == -10);
}

DOCTEST_TEST_CASE("captured ships append when turn order has capacity") {
    int16_t turn_order[4] = { 3, -10, -1, -1 };

    COMBAT1::Restore_Captured_Ship_To_Turn_Order(
        turn_order, 4, 1);

    DOCTEST_CHECK(turn_order[0] == 3);
    DOCTEST_CHECK(turn_order[1] == 1);
    DOCTEST_CHECK(turn_order[2] == -10);
}

DOCTEST_TEST_CASE("captured ships cannot overflow a full turn order") {
    struct {
        int16_t turn_order[210];
        int16_t canary;
    } state;

    for (int16_t i = 0; i < 209; ++i) {
        state.turn_order[i] = 3;
    }
    state.turn_order[209] = -10;
    state.canary = 0x1234;

    COMBAT1::Restore_Captured_Ship_To_Turn_Order(
        state.turn_order, 210, 1);

    DOCTEST_CHECK(state.turn_order[209] == -10);
    DOCTEST_CHECK(state.canary == 0x1234);
}

DOCTEST_TEST_CASE("capture returns a strategic leader to the officer pool") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[1].owner = 0;
    MOX::combat_data[1].ship_idx = 1;
    MOX::combat_data[1].officer_idx = -1;
    MOX::ship[1].officer_index = 3;

    CMBTFIR2::Capture_Ship(1, 0, 0);

    DOCTEST_CHECK(TEST_BOARDING::Killed_Officer_() == -1);
    DOCTEST_CHECK(TEST_BOARDING::Pooled_Officer_() == 3);
}

DOCTEST_TEST_CASE("shield generator damage refreshes stored shield capacity") {
    TEST_BOARDING::Reset_();
    TEST_BOARDING::Set_Combat_Shield_Max_(100);
    s_combat_data& ship = MOX::combat_data[1];
    ship.shield_current_hits = 2;
    ship.shield_arc_max = 150;
    ship.shield_arc_current[0] = 150;
    ship.shield_arc_current[1] = 120;
    ship.shield_arc_current[2] = 100;
    ship.shield_arc_current[3] = 80;
    int16_t remaining_damage = 1;

    CMBTFIR2::Apply_Internal_Damage(1, &remaining_damage, 2, 0, 0);

    DOCTEST_CHECK((ship.shield_arc_max == 100));
    DOCTEST_CHECK((ship.shield_arc_current[0] == 100));
    DOCTEST_CHECK((ship.shield_arc_current[1] == 100));
    DOCTEST_CHECK((ship.shield_arc_current[2] == 100));
    DOCTEST_CHECK((ship.shield_arc_current[3] == 80));
}

DOCTEST_TEST_CASE("subsystem damage floors unsigned hit points and returns excess damage") {
    struct s_damage_case {
        int16_t system;
        uint16_t s_combat_data::* current_hits;
    };
    const s_damage_case cases[] = {
        {2, &s_combat_data::shield_current_hits},
        {5, &s_combat_data::drive_current_hits},
        {8, &s_combat_data::computer_current_hits},
    };

    for (const s_damage_case& test_case : cases) {
        TEST_BOARDING::Reset_();
        s_combat_data& ship = MOX::combat_data[1];
        ship.*test_case.current_hits = UINT16_MAX;
        int16_t remaining_damage = 100;

        CMBTFIR2::Apply_Internal_Damage(
            1, &remaining_damage, test_case.system, 0, 0);

        DOCTEST_CHECK(ship.*test_case.current_hits == UINT16_MAX - 100);
        DOCTEST_CHECK(remaining_damage == 0);

        ship.*test_case.current_hits = 50;
        remaining_damage = 75;

        CMBTFIR2::Apply_Internal_Damage(
            1, &remaining_damage, test_case.system, 0, 0);

        DOCTEST_CHECK(ship.*test_case.current_hits == 0);
        DOCTEST_CHECK(remaining_damage == (test_case.system == 5 ? 0 : 25));
    }
}

DOCTEST_TEST_CASE("destroyed multi-phased shields reduce shield capacity") {
    TEST_BOARDING::Reset_();
    TEST_BOARDING::Set_Combat_Shield_Max_(100);
    s_combat_data& ship = MOX::combat_data[1];
    struct_::Set_Bit_Field(
        ship.special_device_flags, SPECIAL_MULTI_PHASED_SHIELDS);
    ship.special_hits[0] = 10;
    ship.shield_arc_max = 150;
    ship.shield_arc_current[0] = 150;
    ship.shield_arc_current[1] = 120;
    ship.shield_arc_current[2] = 100;
    ship.shield_arc_current[3] = 80;
    int16_t remaining_damage = 10;

    CMBTFIR2::Apply_Internal_Damage(1, &remaining_damage, 9, 0, 0);

    DOCTEST_CHECK(struct_::Test_Bit_Field(
        ship.special_device_damage_flags,
        SPECIAL_MULTI_PHASED_SHIELDS) == 1);
    DOCTEST_CHECK((ship.shield_arc_max == 100));
    DOCTEST_CHECK((ship.shield_arc_current[0] == 100));
    DOCTEST_CHECK((ship.shield_arc_current[1] == 100));
    DOCTEST_CHECK((ship.shield_arc_current[2] == 100));
    DOCTEST_CHECK((ship.shield_arc_current[3] == 80));
}

DOCTEST_TEST_CASE("destroying the sole enabled Stealth Field decloaks its ship") {
    TEST_BOARDING::Reset_();
    s_combat_data& ship = MOX::combat_data[1];
    struct_::Set_Bit_Field(
        ship.special_device_flags, SPECIAL_STEALTH_FIELD);
    ship.special_hits[0] = 1;
    ship.special_status_flag = 1;
    CMBTFIR2::combat_bonus.stealth_field_beam_defense = 50;
    int16_t remaining_damage = 1;

    CMBTFIR2::Apply_Internal_Damage(1, &remaining_damage, 9, 0, 0);

    DOCTEST_CHECK(ship.special_status_flag == 2);
}

DOCTEST_TEST_CASE("destroying one ordinary cloak preserves another working cloak") {
    TEST_BOARDING::Reset_();
    s_combat_data& ship = MOX::combat_data[1];
    struct_::Set_Bit_Field(
        ship.special_device_flags, SPECIAL_CLOAKING_DEVICE);
    struct_::Set_Bit_Field(
        ship.special_device_flags, SPECIAL_STEALTH_FIELD);
    ship.special_hits[0] = 1;
    ship.special_status_flag = 1;
    CMBTFIR2::combat_bonus.stealth_field_beam_defense = 50;
    int16_t remaining_damage = 1;

    CMBTFIR2::Apply_Internal_Damage(1, &remaining_damage, 9, 0, 0);

    DOCTEST_CHECK(ship.special_status_flag == 1);
}

DOCTEST_TEST_CASE("raid reports retain all damaged systems and correct special status") {
    TEST_BOARDING::Reset_();
    TEST_BOARDING::Prepare_Long_Raid_();
    int16_t raiders = 10;

    CMBTFIR2::Resolve_Raid(0, 1, &raiders);

    const char* message = TEST_BOARDING::Last_Raid_Message_();
    DOCTEST_CHECK(strlen(message) > 500);
    DOCTEST_CHECK(strstr(message, "Special device seven final marker") != nullptr);
    DOCTEST_CHECK(strstr(message, "1 Special device zero with a deliberately long raid report name damaged") != nullptr);
    DOCTEST_CHECK(strstr(message, "3 Special device zero with a deliberately long raid report name") == nullptr);
}

DOCTEST_TEST_CASE("raid reports retain configured names beyond the old body limit") {
    TEST_BOARDING::Reset_();
    TEST_BOARDING::Prepare_Oversized_Raid_();
    int16_t raiders = 10;

    CMBTFIR2::Resolve_Raid(0, 1, &raiders);

    const char* message = TEST_BOARDING::Last_Raid_Message_();
    DOCTEST_CHECK(strlen(message) > 9999);
    DOCTEST_CHECK(strstr(message, "oversized raid report final marker") != nullptr);
}

DOCTEST_TEST_CASE("successful raiders damage the configured number of components") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;
    for (int16_t weapon_idx = 0; weapon_idx < 2; ++weapon_idx) {
        MOX::combat_data[1].weapons[weapon_idx].weapon_id = weapon_idx + 1;
        MOX::combat_data[1].weapons[weapon_idx].count = 1;
        MOX::combat_data[1].weapon_hits[weapon_idx] = 100;
        MOX::combat_data[1].weapon_hits_per_unit[weapon_idx] = 100;
    }
    BOARDING::boarding.raid_items_per_marine = 1;
    int16_t raiders = 1;

    CMBTFIR2::Resolve_Raid(0, 1, &raiders);

    DOCTEST_CHECK(MOX::combat_data[1].weapon_hits[0] == 50);
    DOCTEST_CHECK(MOX::combat_data[1].weapon_hits[1] == 100);

    TEST_BOARDING::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;
    for (int16_t weapon_idx = 0; weapon_idx < 2; ++weapon_idx) {
        MOX::combat_data[1].weapons[weapon_idx].weapon_id = weapon_idx + 1;
        MOX::combat_data[1].weapons[weapon_idx].count = 1;
        MOX::combat_data[1].weapon_hits[weapon_idx] = 100;
        MOX::combat_data[1].weapon_hits_per_unit[weapon_idx] = 100;
    }
    BOARDING::boarding.raid_items_per_marine = 2;
    raiders = 1;

    CMBTFIR2::Resolve_Raid(0, 1, &raiders);

    DOCTEST_CHECK(MOX::combat_data[1].weapon_hits[0] == 50);
    DOCTEST_CHECK(MOX::combat_data[1].weapon_hits[1] == 50);
}

DOCTEST_TEST_CASE("successful raids preserve zero through maximum item counts") {
    struct s_case {
        uint8_t configured_items;
        int expected_attempts;
    };
    const s_case cases[] = {
        {0, 1},
        {1, 1},
        {2, 2},
        {127, 127},
    };

    for (const s_case& test_case : cases) {
        TEST_BOARDING::Reset_();
        TEST_BOARDING::Prepare_Long_Raid_();
        TEST_BOARDING::Set_Raid_Choice_(2);
        MOX::combat_data[1].weapon_hits[0] = 1000;
        MOX::combat_data[1].weapon_hits_per_unit[0] = 1000;
        MOX::combat_data[1].weapon_total_hits[0] = 1000;
        BOARDING::boarding.raid_damage_per_marine = 1;
        BOARDING::boarding.raid_items_per_marine = test_case.configured_items;
        int16_t raiders = 1;

        CMBTFIR2::Resolve_Raid(0, 1, &raiders);

        DOCTEST_CHECK(TEST_BOARDING::Raid_Selection_Count_() ==
                      test_case.expected_attempts);
        DOCTEST_CHECK(MOX::combat_data[1].weapon_hits[0] ==
                      1000 - test_case.expected_attempts);
        const std::string report_count =
            std::to_string(test_case.expected_attempts) + " " +
            (test_case.expected_attempts == 1
                 ? "Weapon zero with a deliberately long raid report name"
                 : "Weapon zeroes with a deliberately long raid report name");
        DOCTEST_CHECK(strstr(TEST_BOARDING::Last_Raid_Message_(),
                             report_count.c_str()) != nullptr);
    }
}

DOCTEST_TEST_CASE("successful raiders use the configured damage per component") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[1].weapons[0].weapon_id = 1;
    MOX::combat_data[1].weapons[0].count = 1;
    MOX::combat_data[1].weapon_hits[0] = 100;
    MOX::combat_data[1].weapon_hits_per_unit[0] = 100;
    BOARDING::boarding.raid_damage_per_marine = 25;
    BOARDING::boarding.raid_items_per_marine = 1;
    int16_t raiders = 1;

    CMBTFIR2::Resolve_Raid(0, 1, &raiders);

    DOCTEST_CHECK(MOX::combat_data[1].weapon_hits[0] == 75);
}

DOCTEST_TEST_CASE("super-fast combat suppresses raid reports") {
    TEST_BOARDING::Reset_();
    TEST_BOARDING::Prepare_Long_Raid_();
    MOX::super_fast_flag = 1;
    int16_t raiders = 10;

    CMBTFIR2::Resolve_Raid(0, 1, &raiders);

    DOCTEST_CHECK(TEST_BOARDING::Last_Raid_Message_()[0] == '\0');
}

DOCTEST_TEST_CASE("boarding result auto-close is disabled by default") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;

    CMBTFIR2::Resolve_Capture(0, 1, 1, 0, 0);

    DOCTEST_CHECK_FALSE(TEST_BOARDING::Last_Result_Was_Timed_());
}

DOCTEST_TEST_CASE("boarding result auto-close uses the automatic-combat timeout") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;
    BOARDING::boarding.auto_close_results = 1;

    CMBTFIR2::Resolve_Capture(0, 1, 1, 0, 0);

    DOCTEST_CHECK(TEST_BOARDING::Last_Result_Was_Timed_());
}

DOCTEST_TEST_CASE("super-fast combat suppresses capture reports") {
    TEST_BOARDING::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;
    MOX::super_fast_flag = 1;

    CMBTFIR2::Resolve_Capture(0, 1, 1, 0, 0);

    DOCTEST_CHECK(MOX::combat_data[1].owner == 0);
    DOCTEST_CHECK(TEST_BOARDING::Last_Raid_Message_()[0] == '\0');
}
