#include "pch.h"

#include "doctest/doctest.h"
#include "game/cmbtspec.h"

namespace TEST_CMBTSPEC {
    void Reset_();
    int32_t Random_Call_Count_();
    int32_t Apply_Ship_Damage_Call_Count_();
    int32_t Applied_Ship_Damage_();
    int32_t Cloak_Frame_Count_();
    int16_t Damage_Indicator_Total_();
    int32_t Missile_Move_Call_Count_();
    int32_t Missile_Resolve_Call_Count_();
    void Set_Random_Result_(uint32_t value);
    int16_t Bitmap_Line_X1_();
    int16_t Bitmap_Line_Y1_();
    int16_t Bitmap_Line_X2_();
    int16_t Bitmap_Line_Y2_();
}

namespace CMBTSPEC {
    bool __cdecl Draw_Clipped_Tractor_Line(
        int16_t x1, int16_t y1, int16_t x2, int16_t y2, s_animation_header* bitmap);
}

DOCTEST_TEST_CASE("tractor beam keeps emitter-to-target direction after clipping") {
    s_animation_header bitmap = {};
    struct s_target {
        int16_t x;
        int16_t y;
    };
    const s_target targets[] = {
        {130, 140},
        {70, 140},
        {70, 60},
        {130, 60},
        {100, 140},
        {100, 60},
        {130, 100},
        {70, 100},
    };

    for (const s_target& target : targets) {
        TEST_CMBTSPEC::Reset_();

        DOCTEST_REQUIRE(CMBTSPEC::Draw_Clipped_Tractor_Line(
            100, 100, target.x, target.y, &bitmap));

        DOCTEST_CHECK(TEST_CMBTSPEC::Bitmap_Line_X1_() == 100);
        DOCTEST_CHECK(TEST_CMBTSPEC::Bitmap_Line_Y1_() == 100);
        DOCTEST_CHECK(TEST_CMBTSPEC::Bitmap_Line_X2_() == target.x);
        DOCTEST_CHECK(TEST_CMBTSPEC::Bitmap_Line_Y2_() == target.y);
    }
}

DOCTEST_TEST_CASE("Stellar Converter optionally uses Displacement Device beam miss chance") {
    struct s_case {
        uint8_t enabled;
        bool has_device;
        bool damaged_device;
        uint8_t miss_percent;
        uint32_t roll;
        bool hits;
    };
    const s_case cases[] = {
        {0, true, false, 100, 100, true},
        {1, true, false, 100, 100, false},
        {1, false, false, 100, 100, true},
        {1, true, true, 100, 100, true},
        {1, true, false, 0, 1, true},
        {1, true, false, 30, 30, false},
        {1, true, false, 30, 31, true},
    };

    for (const s_case& value : cases) {
        DOCTEST_CAPTURE(value.enabled);
        DOCTEST_CAPTURE(value.has_device);
        DOCTEST_CAPTURE(value.damaged_device);
        DOCTEST_CAPTURE(value.miss_percent);
        DOCTEST_CAPTURE(value.roll);
        TEST_CMBTSPEC::Reset_();
        TEST_CMBTSPEC::Set_Random_Result_(value.roll);
        CMBTFIR2::combat_bonus.displacement_device_affects_stellar_converter = value.enabled;
        CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent = value.miss_percent;
        if (value.has_device) {
            MOX::combat_data[1].special_device_flags[SPECIAL_DISPLACEMENT_DEVICE / 8] |=
                (uint8_t)(1u << (SPECIAL_DISPLACEMENT_DEVICE % 8));
        }
        if (value.damaged_device) {
            MOX::combat_data[1].special_device_damage_flags[SPECIAL_DISPLACEMENT_DEVICE / 8] |=
                (uint8_t)(1u << (SPECIAL_DISPLACEMENT_DEVICE % 8));
        }
        MOX::combat_data[1].tac_val_defense = 30000;
        TECHDATA::weapons[WEAPON_STELLAR_CONVERTER].type = WEAPON_TYPE_SPECIAL;
        TECHDATA::weapons[WEAPON_STELLAR_CONVERTER].max_damage = 100;

        CMBTSPEC::Stellar_Converter(0, 1, 2);

        DOCTEST_CHECK(TEST_CMBTSPEC::Applied_Ship_Damage_() == (value.hits ? 800 : 0));
        DOCTEST_CHECK(TEST_CMBTSPEC::Apply_Ship_Damage_Call_Count_() == (value.hits ? 4 : 0));
        DOCTEST_CHECK(TEST_CMBTSPEC::Random_Call_Count_() ==
            (value.enabled && value.has_device && !value.damaged_device ? 2 : 0));
        DOCTEST_CHECK(TECHDATA::weapons[WEAPON_STELLAR_CONVERTER].type == WEAPON_TYPE_SPECIAL);
    }
}

DOCTEST_TEST_CASE("Spatial Compressor damage scales with every ship size class") {
    for (int16_t size_class = SHIP_SIZE_SMALL;
         size_class <= SHIP_SIZE_DOOM_STAR;
         ++size_class) {
        TEST_CMBTSPEC::Reset_();
        CMBTSPEC::compressor_per_size_class_damage = 1;
        MOX::combat_data[1].size_class = (int8_t)size_class;

        CMBTSPEC::Do_Spherical_SFX(0, 0, 0, 2, 1);

        DOCTEST_CHECK(
            MOX::combat_data[1].structure_damage ==
            10 * (size_class + 1));
        DOCTEST_CHECK(
            TEST_CMBTSPEC::Random_Call_Count_() ==
            1 + 2 * (size_class + 1));
        DOCTEST_CHECK(
            TEST_CMBTSPEC::Apply_Ship_Damage_Call_Count_() == 0);
    }
}

DOCTEST_TEST_CASE("disabled Spatial Compressor scaling keeps classic damage") {
    TEST_CMBTSPEC::Reset_();
    CMBTSPEC::compressor_per_size_class_damage = 0;
    MOX::combat_data[1].size_class = SHIP_SIZE_DOOM_STAR;

    CMBTSPEC::Do_Spherical_SFX(0, 0, 0, 2, 1);

    DOCTEST_CHECK(MOX::combat_data[1].structure_damage == 10);
    DOCTEST_CHECK(TEST_CMBTSPEC::Random_Call_Count_() == 13);
    DOCTEST_CHECK(TEST_CMBTSPEC::Apply_Ship_Damage_Call_Count_() == 0);
}

DOCTEST_TEST_CASE("Anti-Missile Rockets use the executable hit threshold") {
    struct s_case {
        uint32_t roll;
        int16_t expected_quantity;
    };
    const s_case cases[] = {
        {64, 0},
        {65, 1},
    };

    for (const s_case& test_case : cases) {
        TEST_CMBTSPEC::Reset_();
        TEST_CMBTSPEC::Set_Random_Result_(test_case.roll);
        CMBTSPEC::anti_missile_rocket_accuracy = {70, 10, 3};
        MOX::combat_data[0].weapons[0].shots_left = 1;
        MOX::combat_data[0].weapons[0].ammo = 1;
        MOX::missile[0].type = WEAPON_MERCULITE_MISSILE;
        MOX::missile[0].quantity = 1;

        CMBTSPEC::Anti_Missile_Rockets(0, 0, 0, 0, 0, 0);

        DOCTEST_CHECK((MOX::missile[0].quantity == test_case.expected_quantity));
        DOCTEST_CHECK(MOX::combat_data[0].weapons[0].shots_left == 0);
        DOCTEST_CHECK(MOX::combat_data[0].weapons[0].ammo == 0);
        DOCTEST_CHECK(TEST_CMBTSPEC::Random_Call_Count_() == 1);
    }
}

DOCTEST_TEST_CASE("Proton Torpedo launch distance follows speed-base mode") {
    struct s_case {
        uint8_t apply_speed_base;
        int16_t super_fast;
        int16_t expected_travel_distance;
    };
    const s_case cases[] = {
        {0, 0, 10000},
        {1, 0, 37},
        {0, 1, 10000},
        {1, 1, 37},
    };

    for (const s_case& test_case : cases) {
        TEST_CMBTSPEC::Reset_();
        CMBTMIS::proton_torpedo_apply_speed_base =
            test_case.apply_speed_base;
        MOX::super_fast_flag = test_case.super_fast;
        MOX::missile[0].type = WEAPON_PROTON_TORPEDO;
        MOX::missile[0].travel_dist = 37;

        CMBTSPEC::Missile_Launch_FX(0, 0, 0, 0);

        DOCTEST_CHECK((MOX::missile[0].travel_dist ==
            test_case.expected_travel_distance));
        DOCTEST_CHECK(TEST_CMBTSPEC::Missile_Move_Call_Count_() == 1);
        DOCTEST_CHECK(TEST_CMBTSPEC::Missile_Resolve_Call_Count_() == 1);
    }
}

DOCTEST_TEST_CASE("Dragon Breath keeps its independent instant launch distance") {
    TEST_CMBTSPEC::Reset_();
    CMBTMIS::proton_torpedo_apply_speed_base = 1;
    MOX::missile[0].type = WEAPON_DRAGON_BREATH;
    MOX::missile[0].travel_dist = 37;

    CMBTSPEC::Missile_Launch_FX(0, 0, 0, 0);

    DOCTEST_CHECK((MOX::missile[0].travel_dist == 10000));
    DOCTEST_CHECK(TEST_CMBTSPEC::Missile_Move_Call_Count_() == 1);
    DOCTEST_CHECK(TEST_CMBTSPEC::Missile_Resolve_Call_Count_() == 1);
}

DOCTEST_TEST_CASE("Spatial Compressor overkill reports the full rolled damage") {
    struct s_case {
        uint8_t size_scaled;
        int16_t expected_damage;
    };
    const s_case cases[] = {
        {0, 10},
        {1, 60},
    };

    for (const s_case& test_case : cases) {
        TEST_CMBTSPEC::Reset_();
        CMBTSPEC::compressor_per_size_class_damage = test_case.size_scaled;
        MOX::combat_data[1].size_class = SHIP_SIZE_DOOM_STAR;
        MOX::combat_data[1].structure_damage = 2;
        MOX::combat_data[1].structure_max = 3;
        MOX::apply_damage_to_planet = 1;
        MOX::super_fast_flag = 0;

        CMBTSPEC::Do_Spherical_SFX(0, 0, 0, 2, 1);

        DOCTEST_CHECK(MOX::combat_data[1].structure_damage == 3);
        DOCTEST_CHECK(
            TEST_CMBTSPEC::Damage_Indicator_Total_() ==
            test_case.expected_damage);
    }
}

DOCTEST_TEST_CASE("cloak animation accepts the positive uint7 step domain") {
    struct s_case {
        uint8_t normal;
        uint8_t fast;
        int16_t speedx2;
        int16_t network;
        int32_t expected_frames;
    };
    const s_case cases[] = {
        {1, 2, 0, 0, 24},
        {12, 2, 0, 0, 2},
        {1, 13, 1, 0, 2},
        {1, 127, 0, 1, 1},
    };

    for (const s_case& test_case : cases) {
        TEST_CMBTSPEC::Reset_();
        CMBTSPEC::animation.cloak_steps.normal = test_case.normal;
        CMBTSPEC::animation.cloak_steps.fast = test_case.fast;
        MOX::speedx2_flag = test_case.speedx2;
        MOX::net_flag = test_case.network;

        CMBTSPEC::Draw_Cloak(0, 1);

        DOCTEST_CHECK(
            TEST_CMBTSPEC::Cloak_Frame_Count_() == test_case.expected_frames);
        DOCTEST_CHECK(MOX::dont_draw_ship == -1);
    }
}

DOCTEST_TEST_CASE("Spatial Compressor damage saturates at remaining structure") {
    TEST_CMBTSPEC::Reset_();
    CMBTSPEC::compressor_per_size_class_damage = 1;
    MOX::combat_data[1].size_class = SHIP_SIZE_DOOM_STAR;
    MOX::combat_data[1].structure_max = INT16_MAX;
    TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].min_damage = INT16_MAX;
    TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].max_damage = INT16_MAX;

    CMBTSPEC::Do_Spherical_SFX(0, 0, 0, 2, 1);

    DOCTEST_CHECK(MOX::combat_data[1].structure_damage == INT16_MAX);
    DOCTEST_CHECK(TEST_CMBTSPEC::Random_Call_Count_() == 13);
    DOCTEST_CHECK(TEST_CMBTSPEC::Apply_Ship_Damage_Call_Count_() == 0);
}