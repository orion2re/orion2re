#include "pch.h"

#include "doctest/doctest.h"
#include "game/cmbtmis.h"

namespace {
    s_combat_data combat_data[6];
    s_missile missiles[MAX_COMBAT_FLYERS];
    s_player players[MAX_PLAYERS];
    int16_t target_ranges[6];
    bool planet_has_defenses = true;
    int16_t fleet_pilot_bonus;
    int16_t fleet_offensive_bonus;
    int16_t fleet_defensive_bonus;
    int16_t fleet_defensive_player;
    int16_t fleet_defensive_mode;
    int16_t best_warp_drive = 1;
    int16_t bearing_target;
    int16_t bearing_x;
    int16_t bearing_y;
}

namespace TEST_FIGHTER_RETARGETING {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(missiles, 0, sizeof(missiles));
        memset(players, 0, sizeof(players));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        memset(target_ranges, 0, sizeof(target_ranges));
        CMBTMIS::Reset_Fighter_Pursuit_History();
        MOX::combat_turn = 0;
        MOX::apply_damage_to_planet = 1;
        CMBTMIS::missile_cross_round_merge = true;
        CMBTMIS::smarter_fighter_retargeting = false;
        CMBTMIS::fighter_max_pursuit_turns = 3;
        CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
        planet_has_defenses = true;
    }

    void Set_Up_Pursuit_() {
        Reset_();
        CMBTMIS::smarter_fighter_retargeting = true;
        CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_RETURN;
        planet_has_defenses = false;
        MOX::combat_attacker_player = 0;
        MOX::combat_defender_player = 1;
        MOX::combat_total_ship_count = 4;
        MOX::combat_data[1].owner = 0;
        MOX::combat_data[2].owner = 1;
        MOX::combat_data[3].owner = 1;
        TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
        s_missile& fighter = MOX::missile[0];
        fighter.type = WEAPON_INTERCEPTOR;
        fighter.source_unit_idx = 1;
        fighter.target_unit_idx = 2;
        fighter.fighter_count = 2;
        fighter.speed = 27;
        MOX::combat_data[2].x = 2;
        MOX::combat_data[2].current_speed = 27;
        MOX::combat_data[3].x = 3;
        MOX::combat_data[3].current_speed = 13;
        MOX::combat_data[3].size_class = 3;
    }

    void Set_Range_(int16_t ship_idx, int16_t range) {
        target_ranges[ship_idx] = range;
    }
}

namespace MOX {
    int16_t PLAYER_NUM = -1;
    int16_t NUM_PLAYERS = 0;
    s_combat_data* combat_data = ::combat_data;
    s_missile* missile = missiles;
    s_player* player = players;
    int16_t combat_total_ship_count;
    int16_t combat_attacker_player;
    int16_t combat_defender_player;
    int16_t combat_turn;
    uint16_t apply_damage_to_planet;
    int16_t missile_hit_radius[6] = {12, 20, 26, 28, 30, 30};
}

namespace RUSS {
    int8_t all_ai_game = 0;
}

namespace TECHDATA {
    s_tech_armor_data armor[ARMOR_COUNT];
    s_tech_computer_data computers[COMPUTER_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];
}

namespace CMBTFIRE {
    int16_t __cdecl Does_Combat_Planet_Have_Defenses() {
        return planet_has_defenses;
    }

    int16_t __cdecl Target_Is_Legal_For_Weapon(int16_t, uint16_t, int16_t, int16_t) {
        return 1;
    }

    int16_t __cdecl Weapon_Can_Fire_At_Missiles(int16_t weapon, uint16_t) {
        return weapon == WEAPON_INTERCEPTOR;
    }
}

namespace special {
    int16_t __cdecl Range(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int16_t dx = abs(x1 - x2);
        int16_t dy = abs(y1 - y2);
        return dx > dy ? dx + dy / 2 : dy + dx / 2;
    }
}

namespace CMBTMOV1 {
    int8_t __cdecl Ship_Is_In_Stasis(int16_t) {
        return 0;
    }
}

namespace CMBTDRW1 {
    int16_t __cdecl Relative_Bearing_XY(int16_t target_idx, int16_t x, int16_t y) {
        bearing_target = target_idx;
        bearing_x = x;
        bearing_y = y;
        return 137;
    }
} // namespace CMBTDRW1

namespace COMBINIT {
    void __cdecl Ship_Center_Offsets(int16_t, int16_t* x, int16_t* y) {
        *x = 0;
        *y = 0;
    }
}

namespace CMBTAI {
    int16_t __cdecl Range_To_Missile(int16_t unit_idx, int16_t) {
        return target_ranges[unit_idx];
    }
}

namespace INITSHIP {
    int16_t __cdecl Best_Armor(int16_t) {
        return ARMOR_NO_ARMOR;
    }

    int16_t __cdecl Best_Warp_Drive(int16_t) {
        return best_warp_drive;
    }

    int16_t __cdecl Best_Computer(int16_t) {
        return COMPUTER_NO_COMPUTER;
    }
}

namespace CMBTFIR2 {
    int16_t __cdecl Get_Fleet_Pilot_Bonus(int16_t) {
        return fleet_pilot_bonus;
    }

    int16_t __cdecl Fleet_Defensive_Bonus(int16_t player_idx, int16_t mode, int16_t*) {
        fleet_defensive_player = player_idx;
        fleet_defensive_mode = mode;
        return fleet_defensive_bonus;
    }

    int16_t __cdecl Fleet_Offensive_Bonus(int16_t) {
        return fleet_offensive_bonus;
    }
}

namespace TEST_MISSILE_MERGING {
    void Set_Up_() {
        TEST_FIGHTER_RETARGETING::Reset_();

        s_missile* existing = &MOX::missile[0];
        existing->type = WEAPON_NUCLEAR_MISSILE;
        existing->owner = 1;
        existing->target_unit_idx = 2;
        existing->x = 100;
        existing->y = 120;
        existing->quantity = 3;
        existing->specials = WEAPON_MOD_FLAG_FAST;
        existing->fighter_count = 2;

        MOX::missile[1] = *existing;
        MOX::missile[1].quantity = 4;
        MOX::missile[1].fighter_count = 0;
    }
}

DOCTEST_TEST_CASE("flyer caps preserve the classic shared limit") {
    TEST_FIGHTER_RETARGETING::Reset_();
    DOCTEST_CHECK(COMBINIT::max_flyers_per_side == 300);
    DOCTEST_CHECK(COMBINIT::max_flyers_total == 300);
    int16_t slot;
    for (int16_t i = 0; i < 300; ++i) {
        CMBTMIS::Next_Missile_Id(&slot, 0);
        DOCTEST_REQUIRE(slot == i);
        MOX::missile[slot].type = WEAPON_NUCLEAR_MISSILE;
        MOX::missile[slot].owner = 0;
    }
    CMBTMIS::Next_Missile_Id(&slot, 1);
    DOCTEST_CHECK(slot == -1);
    CMBTMIS::Kill_Missile(150);
    CMBTMIS::Next_Missile_Id(&slot, 1);
    DOCTEST_CHECK(slot == 150);
}

DOCTEST_TEST_CASE("flyer quotas allow 2500 groups per side and reuse high slots") {
    TEST_FIGHTER_RETARGETING::Reset_();
    COMBINIT::max_flyers_per_side = 2500;
    COMBINIT::max_flyers_total = 5000;
    int16_t slot;
    for (int16_t owner = 0; owner < 2; ++owner) {
        for (int16_t i = 0; i < 2500; ++i) {
            CMBTMIS::Next_Missile_Id(&slot, owner);
            DOCTEST_REQUIRE(slot == owner * 2500 + i);
            MOX::missile[slot].type = WEAPON_INTERCEPTOR;
            MOX::missile[slot].owner = owner;
        }
        CMBTMIS::Next_Missile_Id(&slot, owner);
        DOCTEST_CHECK(slot == -1);
    }
    CMBTMIS::Kill_Missile(4999);
    CMBTMIS::Next_Missile_Id(&slot, 0);
    DOCTEST_CHECK(slot == -1);
    CMBTMIS::Next_Missile_Id(&slot, 1);
    DOCTEST_CHECK(slot == 4999);
    COMBINIT::max_flyers_per_side = 300;
    COMBINIT::max_flyers_total = 300;
}

DOCTEST_TEST_CASE("flyer total and side quotas restrict launches independently") {
    TEST_FIGHTER_RETARGETING::Reset_();
    COMBINIT::max_flyers_per_side = 2;
    COMBINIT::max_flyers_total = 3;
    MOX::missile[0].type = WEAPON_NUCLEAR_MISSILE;
    MOX::missile[1].type = WEAPON_ASSAULT_SHUTTLE;
    int16_t slot;
    CMBTMIS::Next_Missile_Id(&slot, 0);
    DOCTEST_CHECK(slot == -1);
    CMBTMIS::Next_Missile_Id(&slot, 1);
    DOCTEST_CHECK(slot == 2);
    MOX::missile[2].type = WEAPON_PLASMA_TORPEDO;
    MOX::missile[2].owner = 1;
    CMBTMIS::Next_Missile_Id(&slot, 1);
    DOCTEST_CHECK(slot == -1);
    COMBINIT::max_flyers_per_side = 300;
    COMBINIT::max_flyers_total = 300;
}

DOCTEST_TEST_CASE("missile shield bearing supports the full flyer slot domain") {
    TEST_FIGHTER_RETARGETING::Reset_();
    const int16_t slots[] = {
        ORIGINAL_MAX_COMBAT_FLYERS - 1,
        ORIGINAL_MAX_COMBAT_FLYERS,
        MAX_COMBAT_FLYERS - 1,
    };

    for (int16_t slot : slots) {
        CMBTMIS::Kill_Missile(slot);
        MOX::missile[slot].type = WEAPON_NUCLEAR_MISSILE;
        MOX::missile[slot].x = 100 + slot;
        MOX::missile[slot].y = 200 + slot;

        DOCTEST_CHECK(CMBTMIS::Missile_Shield_Bearing(slot, 2) == 137);
        DOCTEST_CHECK(bearing_target == 2);
        DOCTEST_CHECK(bearing_x == 100 + slot);
        DOCTEST_CHECK(bearing_y == 200 + slot);

        CMBTMIS::Kill_Missile(slot);
        DOCTEST_CHECK(MOX::missile[slot].type == WEAPON_NO_WEAPONS);
        MOX::missile[slot].type = WEAPON_NUCLEAR_MISSILE;
        MOX::missile[slot].x = 700;
        MOX::missile[slot].y = 900;

        DOCTEST_CHECK(CMBTMIS::Missile_Shield_Bearing(slot, 3) == 137);
        DOCTEST_CHECK(bearing_target == 3);
        DOCTEST_CHECK(bearing_x == 700);
        DOCTEST_CHECK(bearing_y == 900);
        CMBTMIS::Kill_Missile(slot);
    }
}

DOCTEST_TEST_CASE("fighter defense includes the fleet helmsman bonus") {
    memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
    memset(players, 0, sizeof(players));

    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
    players[3].traits[TRAIT_SHIP_DEFENSE] = 7;
    fleet_pilot_bonus = 10;
    fleet_defensive_bonus = 15;
    fleet_defensive_player = -1;
    fleet_defensive_mode = -1;

    DOCTEST_CHECK(CMBTMIS::Missile_Dcv(WEAPON_INTERCEPTOR, 3) == 82);
    DOCTEST_CHECK(fleet_defensive_player == 3);
    DOCTEST_CHECK(fleet_defensive_mode == 0);
}

DOCTEST_TEST_CASE("flyer combat values preserve configured width") {
    memset(players, 0, sizeof(players));
    memset(TECHDATA::computers, 0, sizeof(TECHDATA::computers));
    memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
    const CMBTMIS::s_flyer_combat_modifier original =
        CMBTMIS::flyer_combat_modifier;
    const CMBTMIS::s_missile_speed_base original_speed_base =
        CMBTMIS::speed_base;
    fleet_pilot_bonus = 0;
    fleet_offensive_bonus = 0;
    best_warp_drive = 1;

    CMBTMIS::flyer_combat_modifier.fighters_offense_bonus = INT32_MAX;
    DOCTEST_CHECK(CMBTMIS::Fighter_Ocv(0) == INT32_MAX);
    CMBTMIS::flyer_combat_modifier.fighters_offense_bonus = INT32_MIN;
    DOCTEST_CHECK(CMBTMIS::Fighter_Ocv(0) == INT32_MIN);

    CMBTMIS::speed_base.nuclear_missile = 12;
    CMBTMIS::flyer_combat_modifier.nuclear_missile_defense_bonus = -128;
    DOCTEST_CHECK(CMBTMIS::Missile_Dcv(WEAPON_NUCLEAR_MISSILE, 0) == -68);

    CMBTMIS::speed_base.nuclear_missile = UINT16_MAX;
    CMBTMIS::flyer_combat_modifier.nuclear_missile_defense_bonus = 127;
    DOCTEST_CHECK(CMBTMIS::Missile_Dcv(WEAPON_NUCLEAR_MISSILE, 0) == 327802);

    CMBTMIS::flyer_combat_modifier = original;
    CMBTMIS::speed_base = original_speed_base;
}

DOCTEST_TEST_CASE("missile speed table preserves classic flyer speeds") {
    memset(players, 0, sizeof(players));
    best_warp_drive = 1;
    COMBAT1::td_combat_speed_bonus = 4;
    CMBTMIS::speed_fast_bonus = 4;

    struct s_speed_case {
        int16_t weapon;
        int32_t expected;
    };
    const s_speed_case cases[] = {
        { WEAPON_NUCLEAR_MISSILE, 12 },
        { WEAPON_MERCULITE_MISSILE, 12 },
        { WEAPON_PULSON_MISSILE, 12 },
        { WEAPON_ZEON_MISSILE, 12 },
        { WEAPON_ANTI_MATTER_TORP, 20 },
        { WEAPON_PROTON_TORPEDO, 20 },
        { WEAPON_PLASMA_TORPEDO, 24 },
        { WEAPON_DRAGON_BREATH, 24 },
        { WEAPON_INTERCEPTOR, 10 },
        { WEAPON_ASSAULT_SHUTTLE, 6 },
        { WEAPON_BOMBER, 8 },
        { WEAPON_HEAVY_FIGHTER, 8 },
    };

    for (const s_speed_case& speed_case : cases) {
        DOCTEST_CHECK(CMBTMIS::Missile_Speed(speed_case.weapon, 0, 0) ==
                      speed_case.expected);
    }
}

DOCTEST_TEST_CASE("missile speed uses configured base and bonus values") {
    memset(players, 0, sizeof(players));
    const CMBTMIS::s_missile_speed_base original_speed_base = CMBTMIS::speed_base;
    const uint8_t original_fast_bonus = CMBTMIS::speed_fast_bonus;
    const uint8_t original_td_bonus = COMBAT1::td_combat_speed_bonus;
    const uint8_t original_proton_speed_limit =
        CMBTMIS::proton_torpedo_apply_speed_base;

    CMBTMIS::speed_base.nuclear_missile = 15;
    CMBTMIS::speed_base.interceptor = 13;
    CMBTMIS::speed_fast_bonus = 7;
    COMBAT1::td_combat_speed_bonus = 6;
    best_warp_drive = 3;
    players[0].traits[TRAIT_TRANS_DIMENSIONAL] = 1;

    DOCTEST_CHECK(CMBTMIS::Missile_Speed(WEAPON_NUCLEAR_MISSILE, 0, 0) == 19);
    DOCTEST_CHECK(CMBTMIS::Missile_Speed(
                      WEAPON_NUCLEAR_MISSILE, WEAPON_MOD_FLAG_FAST, 0) == 26);
    DOCTEST_CHECK(CMBTMIS::Missile_Speed(WEAPON_INTERCEPTOR, 0, 0) == 23);

    CMBTMIS::speed_base = original_speed_base;
    CMBTMIS::speed_fast_bonus = original_fast_bonus;
    COMBAT1::td_combat_speed_bonus = original_td_bonus;
    CMBTMIS::proton_torpedo_apply_speed_base = original_proton_speed_limit;
    best_warp_drive = 1;
}

DOCTEST_TEST_CASE("fast missile movement bonus does not increase missile defense") {
    memset(players, 0, sizeof(players));
    memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
    const CMBTMIS::s_missile_speed_base original_speed_base = CMBTMIS::speed_base;
    const uint8_t original_fast_bonus = CMBTMIS::speed_fast_bonus;
    const CMBTMIS::s_flyer_combat_modifier original_combat_modifier =
        CMBTMIS::flyer_combat_modifier;

    best_warp_drive = 1;
    CMBTMIS::speed_base.nuclear_missile = 12;
    CMBTMIS::speed_fast_bonus = 4;
    CMBTMIS::flyer_combat_modifier.nuclear_missile_defense_bonus = 0;

    DOCTEST_CHECK(CMBTMIS::Missile_Speed(
                      WEAPON_NUCLEAR_MISSILE, WEAPON_MOD_FLAG_FAST, 0) == 16);
    DOCTEST_CHECK(CMBTMIS::Missile_Dcv(WEAPON_NUCLEAR_MISSILE, 0) == 60);

    CMBTMIS::speed_base = original_speed_base;
    CMBTMIS::speed_fast_bonus = original_fast_bonus;
    CMBTMIS::flyer_combat_modifier = original_combat_modifier;
}

DOCTEST_TEST_CASE("missile speed preserves uint16 bases and 32-bit bonus sums") {
    memset(players, 0, sizeof(players));
    const CMBTMIS::s_missile_speed_base original_speed_base = CMBTMIS::speed_base;
    const uint8_t original_fast_bonus = CMBTMIS::speed_fast_bonus;
    const uint8_t original_td_bonus = COMBAT1::td_combat_speed_bonus;

    best_warp_drive = 1;
    CMBTMIS::speed_base.plasma_torpedo = 32768;
    CMBTMIS::speed_base.nuclear_missile = 65535;

    DOCTEST_CHECK(CMBTMIS::Missile_Speed(WEAPON_PLASMA_TORPEDO, 0, 0) == 32768);
    DOCTEST_CHECK(CMBTMIS::Missile_Speed(WEAPON_NUCLEAR_MISSILE, 0, 0) == 65535);

    CMBTMIS::speed_base.interceptor = 65535;
    CMBTMIS::speed_fast_bonus = 255;
    COMBAT1::td_combat_speed_bonus = 255;
    best_warp_drive = 3;
    players[0].traits[TRAIT_TRANS_DIMENSIONAL] = 1;

    DOCTEST_CHECK(CMBTMIS::Missile_Speed(
                      WEAPON_INTERCEPTOR, WEAPON_MOD_FLAG_FAST, 0) == 66049);

    CMBTMIS::speed_base = original_speed_base;
    CMBTMIS::speed_fast_bonus = original_fast_bonus;
    COMBAT1::td_combat_speed_bonus = original_td_bonus;
    best_warp_drive = 1;
}

DOCTEST_TEST_CASE("Plasma Torpedo dissipation preserves wide configured arithmetic") {
    const CMBTMIS::s_missile_speed_base original_speed_base = CMBTMIS::speed_base;
    const uint8_t original_dissipation = CMBTMIS::plasma_torpedo_dissipation;

    s_missile missile = {};
    missile.type = WEAPON_PLASMA_TORPEDO;
    missile.owner = 0;
    missile.travel_dist = 2540;

    CMBTMIS::speed_base.plasma_torpedo = 65535;
    struct s_dissipation_case {
        uint8_t dissipation;
        int64_t expected_damage;
    };
    const s_dissipation_case cases[] = {
        {0, 180},
        {127, -8306636},
        {128, -8372044},
        {255, -16678860},
    };

    for (const s_dissipation_case& test_case : cases) {
        CMBTMIS::plasma_torpedo_dissipation = test_case.dissipation;
        DOCTEST_CHECK(CMBTMIS::Plasma_Torpedo_Remaining_Damage(
                          &missile, 180, false) == test_case.expected_damage);
    }

    CMBTMIS::plasma_torpedo_dissipation = 255;
    DOCTEST_CHECK(CMBTMIS::Plasma_Torpedo_Remaining_Damage(
                      &missile, 180, true) == -16678605);

    CMBTMIS::speed_base = original_speed_base;
    CMBTMIS::plasma_torpedo_dissipation = original_dissipation;
}

DOCTEST_TEST_CASE("missile defense percentage rolls include their threshold") {
    DOCTEST_CHECK_FALSE(CMBTMIS::Missile_Defense_Roll_Succeeds(1, 0));

    DOCTEST_CHECK(CMBTMIS::Missile_Defense_Roll_Succeeds(29, 30));
    DOCTEST_CHECK(CMBTMIS::Missile_Defense_Roll_Succeeds(30, 30));
    DOCTEST_CHECK_FALSE(CMBTMIS::Missile_Defense_Roll_Succeeds(31, 30));

    DOCTEST_CHECK(CMBTMIS::Missile_Defense_Roll_Succeeds(49, 50));
    DOCTEST_CHECK(CMBTMIS::Missile_Defense_Roll_Succeeds(50, 50));
    DOCTEST_CHECK_FALSE(CMBTMIS::Missile_Defense_Roll_Succeeds(51, 50));
}

DOCTEST_TEST_CASE("missile hit points never fall to zero") {
    memset(TECHDATA::armor, 0, sizeof(TECHDATA::armor));
    TECHDATA::armor[ARMOR_NO_ARMOR].ships_bonus = -100;

    const int16_t fighter_types[] = {
        WEAPON_ASSAULT_SHUTTLE,
        WEAPON_HEAVY_FIGHTER,
        WEAPON_BOMBER,
        WEAPON_INTERCEPTOR,
    };

    for (int16_t fighter_type : fighter_types) {
        DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(fighter_type, 0, 0) == 1);
    }

    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_NO_WEAPONS, 0, 0) == 1);
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_NO_WEAPONS, 0, WEAPON_MOD_FLAG_HEAVILY_ARMORED) == 1);
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_NUCLEAR_MISSILE, 0, 0) == 4);
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_NUCLEAR_MISSILE, 0, WEAPON_MOD_FLAG_HEAVILY_ARMORED) == 8);
}

DOCTEST_TEST_CASE("flyer hit points use configured missile and fighter values") {
    const CMBTMIS::s_flyer_hit_points original = CMBTMIS::flyer_hit_points;
    CMBTMIS::flyer_hit_points = { 9, 10, 11, 12, 8, 12, 16, 20 };
    TECHDATA::armor[ARMOR_NO_ARMOR].ships_bonus = 0;

    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_NUCLEAR_MISSILE, 0, 0) == 9);
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_ZEON_MISSILE, 0, WEAPON_MOD_FLAG_HEAVILY_ARMORED) == 24);
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_INTERCEPTOR, 0, 0) == 4);
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_ASSAULT_SHUTTLE, 0, 0) == 6);
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_BOMBER, 0, 0) == 8);
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(WEAPON_HEAVY_FIGHTER, 0, 0) == 10);

    CMBTMIS::flyer_hit_points = original;
}

DOCTEST_TEST_CASE("fighter hit points preserve the unsigned byte domain") {
    const CMBTMIS::s_flyer_hit_points original = CMBTMIS::flyer_hit_points;
    const int16_t original_armor_bonus =
        TECHDATA::armor[ARMOR_NO_ARMOR].ships_bonus;
    TECHDATA::armor[ARMOR_NO_ARMOR].ships_bonus = 0;

    struct s_hit_point_case {
        uint8_t configured;
        int32_t ordinary;
        int32_t heavily_armored;
    };
    const s_hit_point_case cases[] = {
        {0, 1, 1},
        {127, 63, 126},
        {128, 64, 128},
        {255, 127, 254},
    };

    for (const s_hit_point_case& test_case : cases) {
        CMBTMIS::flyer_hit_points.interceptor = test_case.configured;
        DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(
                          WEAPON_INTERCEPTOR, 0, 0) == test_case.ordinary);
        DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(
                          WEAPON_INTERCEPTOR, 0,
                          WEAPON_MOD_FLAG_HEAVILY_ARMORED) ==
                      test_case.heavily_armored);
    }

    CMBTMIS::flyer_hit_points.interceptor = 255;
    TECHDATA::armor[ARMOR_NO_ARMOR].ships_bonus = INT16_MAX;
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(
                      WEAPON_INTERCEPTOR, 0, 0) == 41905);
    DOCTEST_CHECK(CMBTMIS::Get_Missile_Hits(
                      WEAPON_INTERCEPTOR, 0,
                      WEAPON_MOD_FLAG_HEAVILY_ARMORED) == 83810);

    CMBTMIS::flyer_hit_points = original;
    TECHDATA::armor[ARMOR_NO_ARMOR].ships_bonus = original_armor_bonus;
}

DOCTEST_TEST_CASE("flyer targeting follows anti-missile chains to the final ship") {
    s_missile missiles[3] = {};

    missiles[0].target_is_missile = 1;
    missiles[0].target_unit_idx = 1;
    missiles[1].target_is_missile = 1;
    missiles[1].target_unit_idx = 2;
    missiles[2].target_unit_idx = 7;

    DOCTEST_CHECK(CMBTMIS::Final_Flyer_Target(missiles, 0) == 7);
    DOCTEST_CHECK(CMBTMIS::Final_Flyer_Target(missiles, 2) == 7);
}

DOCTEST_TEST_CASE("ship movement excludes flyers targeting another missile") {
    s_missile missile = {};
    missile.type = WEAPON_NUCLEAR_MISSILE;
    missile.target_unit_idx = 4;

    DOCTEST_CHECK(CMBTMIS::Missile_Targets_Ship(&missile, 4));

    missile.target_is_missile = 1;
    DOCTEST_CHECK_FALSE(CMBTMIS::Missile_Targets_Ship(&missile, 4));

    missile.target_is_missile = 0;
    missile.type = WEAPON_NO_WEAPONS;
    DOCTEST_CHECK_FALSE(CMBTMIS::Missile_Targets_Ship(&missile, 4));
}

DOCTEST_TEST_CASE("returning fighters refill surviving bays in slot order") {
    TEST_FIGHTER_RETARGETING::Reset_();

    s_combat_data* source = &MOX::combat_data[1];
    source->weapons[0].weapon_id = WEAPON_INTERCEPTOR;
    source->weapons[0].count = 2;
    source->weapons[0].ammo = 0;
    source->weapons[0].shots_left = 2;
    source->weapon_hits[0] = 13;
    source->weapon_hits_per_unit[0] = 3;

    source->weapons[1].weapon_id = WEAPON_INTERCEPTOR;
    source->weapons[1].count = 4;
    source->weapons[1].ammo = 0;
    source->weapons[1].shots_left = 4;
    source->weapon_hits[1] = 10;
    source->weapon_hits_per_unit[1] = 3;

    source->weapons[2].weapon_id = WEAPON_BOMBER;
    source->weapons[2].count = 3;
    source->weapons[2].ammo = 0;

    CMBTMIS::Land_Returning_Fighters(source, WEAPON_INTERCEPTOR, 6);

    DOCTEST_CHECK(source->weapons[0].count == 5);
    DOCTEST_CHECK(source->weapons[0].ammo == 1);
    DOCTEST_CHECK(source->weapons[0].shots_left == 0);
    DOCTEST_CHECK(source->weapons[1].count == 1);
    DOCTEST_CHECK((source->weapons[1].ammo == 1));
    DOCTEST_CHECK(source->weapons[1].shots_left == 0);
    DOCTEST_CHECK(source->weapons[2].count == 3);
    DOCTEST_CHECK(source->weapons[2].ammo == 0);
}

DOCTEST_TEST_CASE("returning fighter bays preserve unsigned byte counts") {
    struct s_case {
        int16_t weapon_id;
        uint8_t count;
    };
    const s_case cases[] = {
        {WEAPON_HEAVY_FIGHTER, 127},
        {WEAPON_HEAVY_FIGHTER, 128},
        {WEAPON_HEAVY_FIGHTER, 255},
        {WEAPON_BOMBER, 127},
        {WEAPON_BOMBER, 128},
        {WEAPON_BOMBER, 255},
        {WEAPON_INTERCEPTOR, 127},
        {WEAPON_INTERCEPTOR, 128},
        {WEAPON_INTERCEPTOR, 255},
    };

    for (const s_case& test_case : cases) {
        TEST_FIGHTER_RETARGETING::Reset_();
        s_combat_data* source = &MOX::combat_data[1];
        s_combat_weapon_slot* slot = &source->weapons[0];
        slot->weapon_id = test_case.weapon_id;
        slot->count = test_case.count;
        slot->ammo = 1;
        slot->shots_left = test_case.count;
        source->weapon_hits[0] = test_case.count;
        source->weapon_hits_per_unit[0] = 1;

        CMBTMIS::Land_Returning_Fighters(
            source, test_case.weapon_id, 1);

        DOCTEST_CHECK(slot->count == test_case.count);
        DOCTEST_CHECK(slot->shots_left == test_case.count);
    }
}

DOCTEST_TEST_CASE("returning fighters cross the signed byte count boundary") {
    TEST_FIGHTER_RETARGETING::Reset_();

    s_combat_data* source = &MOX::combat_data[1];
    s_combat_weapon_slot* slot = &source->weapons[0];
    slot->weapon_id = WEAPON_INTERCEPTOR;
    slot->count = 127;
    slot->ammo = 1;
    slot->shots_left = 12;
    source->weapon_hits[0] = 128;
    source->weapon_hits_per_unit[0] = 1;

    CMBTMIS::Land_Returning_Fighters(source, WEAPON_INTERCEPTOR, 1);

    DOCTEST_CHECK(slot->count == 128);
    DOCTEST_CHECK(slot->ammo == 1);
    DOCTEST_CHECK(slot->shots_left == 0);
}

DOCTEST_TEST_CASE("anti-missile fighter can retarget a ship sharing its missile target index") {
    TEST_FIGHTER_RETARGETING::Reset_();

    MOX::combat_attacker_player = 0;
    MOX::combat_defender_player = 1;
    MOX::combat_total_ship_count = 3;
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].owner = 1;
    MOX::combat_data[2].owner = 1;

    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
    MOX::missile[0].type = WEAPON_INTERCEPTOR;
    MOX::missile[0].source_unit_idx = 0;
    MOX::missile[0].target_unit_idx = 1;
    MOX::missile[0].target_is_missile = 1;
    MOX::missile[0].is_active = 1;

    TEST_FIGHTER_RETARGETING::Set_Range_(1, 5);
    TEST_FIGHTER_RETARGETING::Set_Range_(2, 10);

    CMBTMIS::Retarget_Missile(0);

    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 1);
    DOCTEST_CHECK(MOX::missile[0].target_is_missile == 0);
}

DOCTEST_TEST_CASE("smarter fighters favor slower ships within the same contact-round band") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    s_missile& fighter = MOX::missile[0];
    for (int16_t speed : {26, 27, 28}) {
        MOX::combat_data[2].current_speed = speed;
        CMBTMIS::Retarget_Missile(0);
        DOCTEST_CHECK(fighter.target_unit_idx == 3);
        DOCTEST_CHECK(fighter.target_is_missile == 0);
    }
}

DOCTEST_TEST_CASE("smarter fighters prioritize same-pass contact even with faster targets") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    s_missile& fighter = MOX::missile[0];
    MOX::combat_data[2].current_speed = 30;
    fighter.travel_dist = 29;
    CMBTMIS::fighter_max_pursuit_turns = 0;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);

    fighter.travel_dist = 28;
    CMBTMIS::fighter_max_pursuit_turns = 2;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 3);
}

DOCTEST_TEST_CASE("smarter fighter pursuit horizon is configurable including zero and uint16 max") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    s_missile& fighter = MOX::missile[0];
    MOX::combat_total_ship_count = 3;
    MOX::combat_data[2].current_speed = 26;
    MOX::combat_data[2].x = 30;
    fighter.travel_dist = 8;

    CMBTMIS::fighter_max_pursuit_turns = 1;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 1);

    fighter.target_unit_idx = 2;
    CMBTMIS::fighter_max_pursuit_turns = 2;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);

    CMBTMIS::fighter_max_pursuit_turns = 0;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 1);

    fighter.target_unit_idx = 2;
    CMBTMIS::fighter_max_pursuit_turns = UINT16_MAX;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
}

DOCTEST_TEST_CASE("smarter fighters can return or pursue when no worthwhile target exists") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    s_missile& fighter = MOX::missile[0];
    MOX::combat_data[3].current_speed = 30;
    CMBTMIS::fighter_max_pursuit_turns = 0;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == fighter.source_unit_idx);
    DOCTEST_CHECK(fighter.fighter_count == 2);

    fighter.target_unit_idx = 2;
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);

    MOX::combat_data[3].current_speed = 13;
    CMBTMIS::fighter_max_pursuit_turns = 3;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 3);

    MOX::combat_data[2].unit_status = 5;
    MOX::combat_data[3].unit_status = 5;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == fighter.source_unit_idx);
}

DOCTEST_TEST_CASE("smarter fighters prefer slower alternatives in the same pursuit band") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_data[2].current_speed = 26;
    MOX::combat_data[3].x = 5;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 3);
}

DOCTEST_TEST_CASE("smarter fighter contact estimates include diagonal travel and strict impact radius") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_total_ship_count = 3;
    MOX::combat_data[2].y = 2;
    CMBTMIS::fighter_max_pursuit_turns = 0;
    s_missile& fighter = MOX::missile[0];
    fighter.travel_dist = 31;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == fighter.source_unit_idx);

    fighter.target_unit_idx = 2;
    fighter.travel_dist = 32;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
}

DOCTEST_TEST_CASE("smarter fighter contact estimates preserve off-axis seeking rounding") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_total_ship_count = 3;
    MOX::combat_data[2].x = 1;
    MOX::combat_data[2].y = 1;
    CMBTMIS::fighter_max_pursuit_turns = 0;
    s_missile& fighter = MOX::missile[0];
    fighter.x = 9;
    fighter.y = 12;
    fighter.speed = 1;
    fighter.travel_dist = 2;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == fighter.source_unit_idx);

    fighter.target_unit_idx = 2;
    fighter.travel_dist = 3;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);

    fighter.speed = 0;
    fighter.target_unit_idx = 2;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == fighter.source_unit_idx);
}

DOCTEST_TEST_CASE("smarter fighters attack fast ships that hold position or close distance") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_total_ship_count = 3;
    MOX::combat_data[2].current_speed = 40;
    s_missile& fighter = MOX::missile[0];
    fighter.speed = 13;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);

    for (int round = 1; round <= 3; ++round) {
        MOX::combat_turn = round;
        CMBTMIS::Reconsider_Fighter_Target(0);
        DOCTEST_CHECK(fighter.target_unit_idx == 2);
    }
    MOX::combat_data[2].x = 1;
    ++MOX::combat_turn;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
}

DOCTEST_TEST_CASE("movable pursuit fallback always precedes reachable stationary defenses") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_data[2].x = 100;
    MOX::combat_data[2].current_speed = 40;
    MOX::combat_data[3].ship_idx = -1;
    MOX::combat_data[3].ftl_type = DRIVE_NO_DRIVE;
    MOX::combat_data[3].current_speed = 0;
    MOX::combat_data[3].x = 0;
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);

    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_RETURN;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 3);

    MOX::combat_data[2].current_speed = 0;
    MOX::combat_data[2].x = 3;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);
}

DOCTEST_TEST_CASE("observed kiting penalizes a chase once per round and redirects to alternatives") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_total_ship_count = 3;
    s_missile& fighter = MOX::missile[0];
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);

    fighter.x += 20;
    ++MOX::combat_data[2].x;
    ++MOX::combat_turn;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);

    fighter.x += 20;
    ++MOX::combat_data[2].x;
    ++MOX::combat_turn;
    MOX::combat_total_ship_count = 4;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 3);

    ++MOX::combat_turn;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 3);
}

DOCTEST_TEST_CASE("observed kiting uses return or pursue fallback but never defeats immediate contact") {
    for (auto fallback : {CMBTMIS::FIGHTER_PURSUIT_RETURN, CMBTMIS::FIGHTER_PURSUIT_PURSUE}) {
        TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
        CMBTMIS::fighter_pursuit_fallback = fallback;
        MOX::combat_total_ship_count = 3;
        s_missile& fighter = MOX::missile[0];
        CMBTMIS::Reconsider_Fighter_Target(0);
        for (int round = 1; round <= 2; ++round) {
            MOX::combat_turn = round;
            ++MOX::combat_data[2].x;
            fighter.x += 20;
            CMBTMIS::Reconsider_Fighter_Target(0);
        }
        DOCTEST_CHECK(fighter.target_unit_idx ==
            (fallback == CMBTMIS::FIGHTER_PURSUIT_RETURN ? 1 : 2));
        fighter.target_unit_idx = 2;
        fighter.travel_dist = 100;
        ++MOX::combat_turn;
        CMBTMIS::Reconsider_Fighter_Target(0);
        DOCTEST_CHECK(fighter.target_unit_idx == 2);
    }
}

DOCTEST_TEST_CASE("outward targets making progress remain eligible and crossing motion is not kiting") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_total_ship_count = 3;
    MOX::combat_data[2].x = 10;
    s_missile& fighter = MOX::missile[0];
    CMBTMIS::Reconsider_Fighter_Target(0);
    for (int round = 1; round <= 2; ++round) {
        MOX::combat_turn = round;
        ++MOX::combat_data[2].x;
        fighter.x += 100;
        CMBTMIS::Reconsider_Fighter_Target(0);
        DOCTEST_CHECK(fighter.target_unit_idx == 2);
    }

    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_total_ship_count = 3;
    CMBTMIS::Reconsider_Fighter_Target(0);
    ++MOX::combat_turn;
    ++MOX::combat_data[2].y;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);
}

DOCTEST_TEST_CASE("smaller hulls break similar pursuit ties without displacing immediate attacks") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_data[2].current_speed = 13;
    MOX::combat_data[2].x = 4;
    MOX::combat_data[3].x = 2;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);

    MOX::missile[0].travel_dist = 27;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 3);
}

DOCTEST_TEST_CASE("pursuit observations reset for a new battle") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_total_ship_count = 3;
    CMBTMIS::Reconsider_Fighter_Target(0);
    for (int round = 1; round <= 2; ++round) {
        MOX::combat_turn = round;
        ++MOX::combat_data[2].x;
        MOX::missile[0].x += 20;
        CMBTMIS::Reconsider_Fighter_Target(0);
    }
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 1);
    CMBTMIS::Reset_Fighter_Pursuit_History();
    MOX::combat_turn = 0;
    MOX::missile[0].target_unit_idx = 2;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);
}

DOCTEST_TEST_CASE("failed pursuit memory survives event-driven target changes") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_total_ship_count = 3;
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
    CMBTMIS::Reconsider_Fighter_Target(0);
    for (int round = 1; round <= 2; ++round) {
        MOX::combat_turn = round;
        ++MOX::combat_data[2].x;
        MOX::missile[0].x += 20;
        CMBTMIS::Reconsider_Fighter_Target(0);
    }
    MOX::combat_total_ship_count = 4;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 3);
    MOX::combat_data[3].unit_status = 5;
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_RETURN;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 1);
}

DOCTEST_TEST_CASE("destroyed sortie slots cannot transfer pursuit history to replacement fighters") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    MOX::combat_total_ship_count = 3;
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
    CMBTMIS::Reconsider_Fighter_Target(0);
    for (int round = 1; round <= 2; ++round) {
        MOX::combat_turn = round;
        ++MOX::combat_data[2].x;
        MOX::missile[0].x += 20;
        CMBTMIS::Reconsider_Fighter_Target(0);
    }
    const s_missile replacement = MOX::missile[0];
    CMBTMIS::Kill_Missile(0);
    MOX::missile[0] = replacement;
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_RETURN;
    ++MOX::combat_turn;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);
}

DOCTEST_TEST_CASE("failed runner memory prevents alternating pursuit for three subsequent rounds") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
    MOX::missile[0].speed = 13;
    MOX::combat_data[2].x = 3;
    MOX::combat_data[3].x = 20;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);
    for (int round = 1; round <= 4; ++round) {
        MOX::combat_turn = round;
        MOX::combat_data[2].x += 2;
        MOX::combat_data[3].x += 2;
        MOX::missile[0].x += 40;
        CMBTMIS::Reconsider_Fighter_Target(0);
        DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 3);
    }
    MOX::combat_turn = 5;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);
}

DOCTEST_TEST_CASE("pursue retains a legal poor assignment until a worthwhile alternative appears") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
    CMBTMIS::fighter_max_pursuit_turns = 0;
    MOX::missile[0].target_unit_idx = 3;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 3);

    MOX::missile[0].travel_dist = 29;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);

    MOX::missile[0].travel_dist = 0;
    MOX::combat_data[2].unit_status = 5;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 3);
}

DOCTEST_TEST_CASE("failed-target cooldown does not prevent an immediate attack after switching") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
    MOX::combat_total_ship_count = 3;
    CMBTMIS::Reconsider_Fighter_Target(0);
    for (int round = 1; round <= 2; ++round) {
        MOX::combat_turn = round;
        ++MOX::combat_data[2].x;
        MOX::missile[0].x += 20;
        CMBTMIS::Reconsider_Fighter_Target(0);
    }
    MOX::combat_total_ship_count = 4;
    MOX::combat_data[3].x = 20;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 3);
    MOX::missile[0].travel_dist = 100;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);
}

DOCTEST_TEST_CASE("failed-target cooldown clears when a retried chase starts closing") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
    MOX::combat_total_ship_count = 3;
    CMBTMIS::Reconsider_Fighter_Target(0);
    for (int round = 1; round <= 2; ++round) {
        MOX::combat_turn = round;
        ++MOX::combat_data[2].x;
        MOX::missile[0].x += 20;
        CMBTMIS::Reconsider_Fighter_Target(0);
    }
    MOX::combat_turn = 3;
    ++MOX::combat_data[2].x;
    MOX::missile[0].x += 40;
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_RETURN;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 2);
}

DOCTEST_TEST_CASE("failed enemy fighter slot memory clears when the target slot is reused") {
    TEST_FIGHTER_RETARGETING::Set_Up_Pursuit_();
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_PURSUE;
    MOX::combat_data[2].unit_status = 5;
    MOX::combat_data[3].unit_status = 5;
    s_missile& enemy = MOX::missile[1];
    enemy.type = WEAPON_INTERCEPTOR;
    enemy.owner = 1;
    enemy.quantity = 4;
    enemy.source_unit_idx = 2;
    enemy.target_unit_idx = 1;
    enemy.speed = 27;
    enemy.x = 60;
    MOX::missile[0].target_unit_idx = 1;
    MOX::missile[0].target_is_missile = 1;
    CMBTMIS::Reconsider_Fighter_Target(0);
    for (int round = 1; round <= 2; ++round) {
        MOX::combat_turn = round;
        enemy.x += 20;
        MOX::missile[0].x += 20;
        CMBTMIS::Reconsider_Fighter_Target(0);
    }
    const s_missile replacement = enemy;
    CMBTMIS::fighter_pursuit_fallback = CMBTMIS::FIGHTER_PURSUIT_RETURN;
    CMBTMIS::Kill_Missile(1);
    DOCTEST_CHECK(MOX::missile[0].target_unit_idx == 1);
    DOCTEST_CHECK(MOX::missile[0].target_is_missile == 0);
    enemy = replacement;
    MOX::missile[0].target_unit_idx = 1;
    MOX::missile[0].target_is_missile = 1;
    ++MOX::combat_turn;
    CMBTMIS::Reconsider_Fighter_Target(0);
    DOCTEST_CHECK(MOX::missile[0].target_is_missile == 1);
}

DOCTEST_TEST_CASE("disabled smarter targeting keeps classic nearest-ship selection") {
    TEST_FIGHTER_RETARGETING::Reset_();
    MOX::combat_attacker_player = 0;
    MOX::combat_defender_player = 1;
    MOX::combat_total_ship_count = 4;
    MOX::combat_data[0].owner = 1;
    MOX::combat_data[1].owner = 0;
    MOX::combat_data[2].owner = 1;
    MOX::combat_data[3].owner = 1;
    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
    s_missile& fighter = MOX::missile[0];
    fighter.type = WEAPON_INTERCEPTOR;
    fighter.source_unit_idx = 1;
    fighter.fighter_count = 2;
    fighter.speed = 10;
    MOX::combat_data[2].current_speed = 12;
    TEST_FIGHTER_RETARGETING::Set_Range_(0, 20);
    TEST_FIGHTER_RETARGETING::Set_Range_(2, 2);
    TEST_FIGHTER_RETARGETING::Set_Range_(3, 5);

    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
}

DOCTEST_TEST_CASE("smarter fighter targeting chooses fighters but never missiles") {
    TEST_FIGHTER_RETARGETING::Reset_();
    CMBTMIS::smarter_fighter_retargeting = true;
    MOX::combat_attacker_player = 0;
    MOX::combat_defender_player = 1;
    MOX::combat_total_ship_count = 3;
    MOX::combat_data[0].owner = 1;
    MOX::combat_data[1].owner = 0;
    MOX::combat_data[2].owner = 1;
    MOX::combat_data[2].current_speed = 12;
    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    s_missile& fighter = MOX::missile[0];
    fighter.type = WEAPON_INTERCEPTOR;
    fighter.source_unit_idx = 1;
    fighter.owner = 0;
    fighter.fighter_count = 2;
    fighter.speed = 10;
    MOX::missile[1].type = WEAPON_NUCLEAR_MISSILE;
    MOX::missile[1].owner = 1;
    MOX::missile[1].quantity = 4;
    MOX::missile[2].type = WEAPON_INTERCEPTOR;
    MOX::missile[2].owner = 1;
    MOX::missile[2].source_unit_idx = 2;
    MOX::missile[2].target_unit_idx = 1;
    MOX::missile[2].quantity = 4;
    MOX::missile[2].speed = 9;
    MOX::missile[2].x = 5;
    MOX::combat_data[2].x = 2;

    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
    DOCTEST_CHECK(fighter.target_is_missile == 1);

    MOX::combat_data[2].current_speed = 9;
    MOX::combat_data[2].x = 0;
    MOX::missile[2].x = 60;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
    DOCTEST_CHECK(fighter.target_is_missile == 0);

    MOX::combat_data[2].x = 1;
    MOX::missile[2].x = 5;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
    DOCTEST_CHECK(fighter.target_is_missile == 1);

    MOX::missile[2].target_is_missile = 1;
    MOX::missile[2].target_unit_idx = 0;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
    DOCTEST_CHECK(fighter.target_is_missile == 0);

    fighter.type = WEAPON_BOMBER;
    TECHDATA::weapons[WEAPON_BOMBER].type = WEAPON_TYPE_FIGHTER;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
    DOCTEST_CHECK(fighter.target_is_missile == 0);
}

DOCTEST_TEST_CASE("smarter fighter targeting leaves returning fighters alone and defers defenses") {
    TEST_FIGHTER_RETARGETING::Reset_();
    CMBTMIS::smarter_fighter_retargeting = true;
    MOX::combat_attacker_player = 0;
    MOX::combat_defender_player = 1;
    MOX::combat_total_ship_count = 4;
    MOX::combat_data[0].owner = 1;
    MOX::combat_data[1].owner = 0;
    MOX::combat_data[2].owner = 1;
    MOX::combat_data[3].owner = 1;
    MOX::combat_data[2].ship_idx = -1;
    MOX::combat_data[2].ftl_type = DRIVE_NO_DRIVE;
    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
    s_missile& fighter = MOX::missile[0];
    fighter.type = WEAPON_INTERCEPTOR;
    fighter.source_unit_idx = 1;
    fighter.fighter_count = 2;
    fighter.speed = 10;
    MOX::combat_data[0].x = 1;
    MOX::combat_data[2].x = 2;
    MOX::combat_data[3].x = 5;

    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 3);
    MOX::combat_data[3].unit_status = 5;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 2);
    MOX::combat_data[2].unit_status = 5;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 0);
    planet_has_defenses = false;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 1);

    MOX::combat_data[3].unit_status = 0;
    fighter.fighter_count = 2;
    CMBTMIS::Retarget_Missile(0);
    DOCTEST_CHECK(fighter.target_unit_idx == 1);
}

DOCTEST_TEST_CASE("classic missile merging combines stacks from different rounds") {
    TEST_MISSILE_MERGING::Set_Up_();

    CMBTMIS::Merge_Missile(1);

    DOCTEST_CHECK((MOX::missile[0].quantity == 7));
    DOCTEST_CHECK(MOX::missile[1].type == WEAPON_NO_WEAPONS);
}

DOCTEST_TEST_CASE("missile merging preserves oversized groups and reserves MIRV expansion") {
    const uint16_t mods[] = {0, WEAPON_MOD_FLAG_MIRV};
    for (uint16_t mod : mods) {
        TEST_MISSILE_MERGING::Set_Up_();
        const int16_t limit = mod == WEAPON_MOD_FLAG_MIRV ? INT16_MAX / 4 : INT16_MAX;
        MOX::missile[0].specials = mod;
        MOX::missile[1].specials = mod;
        MOX::missile[0].quantity = limit - 3;
        MOX::missile[1].quantity = 4;

        CMBTMIS::Merge_Missile(1);

        DOCTEST_CHECK((MOX::missile[0].quantity == limit - 3));
        DOCTEST_CHECK((MOX::missile[1].quantity == 4));
        DOCTEST_CHECK(MOX::missile[1].type == WEAPON_NUCLEAR_MISSILE);

        MOX::missile[1].quantity = 3;
        CMBTMIS::Merge_Missile(1);
        DOCTEST_CHECK((MOX::missile[0].quantity == limit));
        DOCTEST_CHECK(MOX::missile[1].type == WEAPON_NO_WEAPONS);
    }
}

DOCTEST_TEST_CASE("cross-round missile merging can be disabled") {
    TEST_MISSILE_MERGING::Set_Up_();
    CMBTMIS::missile_cross_round_merge = false;

    CMBTMIS::Merge_Missile(1);

    DOCTEST_CHECK((MOX::missile[0].quantity == 3));
    DOCTEST_CHECK((MOX::missile[1].quantity == 4));
}

DOCTEST_TEST_CASE("same-round missile stacks still merge") {
    TEST_MISSILE_MERGING::Set_Up_();
    CMBTMIS::missile_cross_round_merge = false;
    MOX::missile[1].fighter_count = MOX::missile[0].fighter_count;

    CMBTMIS::Merge_Missile(1);

    DOCTEST_CHECK((MOX::missile[0].quantity == 7));
    DOCTEST_CHECK(MOX::missile[1].type == WEAPON_NO_WEAPONS);
}

DOCTEST_TEST_CASE("missile stacks from different owners do not merge") {
    TEST_MISSILE_MERGING::Set_Up_();
    MOX::missile[1].owner = 2;

    CMBTMIS::Merge_Missile(1);

    DOCTEST_CHECK((MOX::missile[0].quantity == 3));
    DOCTEST_CHECK((MOX::missile[1].quantity == 4));
}

DOCTEST_TEST_CASE("dead missile stacks are not merged") {
    TEST_MISSILE_MERGING::Set_Up_();
    MOX::missile[1].type = WEAPON_NO_WEAPONS;

    CMBTMIS::Merge_Missile(1);

    DOCTEST_CHECK((MOX::missile[0].quantity == 3));
}

DOCTEST_TEST_CASE("bombardment cleanup removes lingering missile stacks") {
    TEST_MISSILE_MERGING::Set_Up_();
    MOX::apply_damage_to_planet = 0;

    CMBTMIS::Merge_Missile(1);

    DOCTEST_CHECK((MOX::missile[0].quantity == 3));
    DOCTEST_CHECK(MOX::missile[1].type == WEAPON_NO_WEAPONS);
}