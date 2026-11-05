#include "pch.h"

#include "doctest/doctest.h"
#include "game/combinit.h"
#include "game/combat.h"

namespace TEST_COMBINIT {
    void Reset_();
    void Set_Strategic_Missile_Ammo_(uint8_t ammo);
    int16_t Tactical_Missile_Ammo_();
    void Set_Black_Hole_Target_(int16_t ship_idx, int8_t owner, bool time_warp);
    void Enable_Initiative_();
    bool Destroyed_(int16_t ship_idx);
    uint8_t Black_Hole_Count_(int16_t ship_idx);
    bool Destroyed_With_Black_Hole_State_(int16_t ship_idx);
    uint16_t Black_Hole_Source_(int16_t ship_idx);
    void Set_Maintenance_Ship_(int16_t ship_idx, bool time_warp);
    int16_t Shield_Arc_Total_(int16_t ship_idx);
    int Repair_Count_(int16_t ship_idx);
    void Set_Exhausted_Missile_();
    int16_t Missile_Travel_Distance_();
    int8_t Missile_Is_Active_();
    void Set_Depleted_Fighter_Garrison_();
    uint8_t Fighter_Count_();
    int16_t Fighter_Ammo_();
    void Set_Deployment_Fleet_(
        int16_t attacker_count,
        int16_t defender_count,
        int8_t size_class,
        bool with_station);
    int Count_Ships_At_(int8_t owner, int8_t x, int8_t y);
    bool Defender_Placements_Are_Valid_();
    void Set_Captured_Ship_(
        int8_t owner, uint8_t previous_owner, uint8_t strategic_speed);
    bool Strategic_Ship_Killed_();
    int8_t Strategic_Ship_Owner_();
    uint8_t Strategic_Design_Speed_();
    void Set_Combat_Planet_Size_(int8_t size_class);
    int8_t Combat_Planet_Size_();
    void Prepare_Scroll_(uint8_t interval);
    int Scroll_Draw_Count_();
    int16_t Combat_X_();
    void Set_Defender_Recaptured_Satellite_();
    void Set_Attacker_Captured_Satellite_(bool destroyed = false);
    bool Satellite_Building_Removed_(
        int16_t colony_idx, int16_t building_idx);
} // namespace TEST_COMBINIT

namespace TEST_COMBAT_REPAIR {
    void Reset_();
    void Set_Zero_Speed_Unload_();
    void Set_Special_(int16_t ship_idx, int16_t special_id);
    void Damage_Special_(int16_t ship_idx, int16_t special_id);
    void Set_Cybernetic_(int16_t player_idx);
    void Set_Engineer_Bonus_(int16_t player_idx, int16_t amount);
    int Repair_Count_(int16_t ship_idx);
    int16_t Repair_Amount_(int16_t ship_idx);
    int Speed_Calc_Count_();
    uint16_t Drive_Current_Hits_();
    uint8_t Stored_Drive_Damage_();
    void Set_Subsystem_Unload_(uint16_t maximum_hits, uint16_t current_hits);
    uint8_t Stored_Shield_Damage_();
    uint8_t Stored_Computer_Damage_();
} // namespace TEST_COMBAT_REPAIR

DOCTEST_TEST_CASE("ordinary combat loading zero-extends strategic ammo") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Strategic_Missile_Ammo_(UINT8_MAX);

    COMBINIT::Load_Combat_Ship(0, 0);

    DOCTEST_CHECK(TEST_COMBINIT::Tactical_Missile_Ammo_() == UINT8_MAX);
}

DOCTEST_TEST_CASE("native combat slot reinitialization retires observation identity") {
    TEST_COMBINIT::Reset_();
    struct s_probe {
        unsigned retired = 0;
        uint64_t subscription = 0;
        ~s_probe() {
            COMBAT::Unsubscribe(subscription);
        }
        static void Receive(const COMBAT::s_event* event, void* context) noexcept {
            if (event->kind == COMBAT::UNIT_RETIRED && event->unit == 1) ++((s_probe*)context)->retired;
        }
    } probe;
    probe.subscription = COMBAT::Subscribe(s_probe::Receive, &probe);
    COMBAT::s_battle_scope battle;
    battle.Open(0);
    COMBINIT::Load_Combat_Ship(0, 1);
    DOCTEST_CHECK(probe.retired == 1);
    battle.Close(0);
}

DOCTEST_TEST_CASE("tactical lifecycle observer requires the current participating local human") {
    TEST_COMBINIT::Reset_();
    MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    RUSS::all_ai_game = 0;
    const auto player = MOX::player[0];
    const auto ship = MOX::combat_data[0];
    DOCTEST_CHECK(COMBINIT::Local_Tactical_Observer() == 0);
    DOCTEST_CHECK(std::memcmp(&player, &MOX::player[0], sizeof(player)) == 0);
    DOCTEST_CHECK(std::memcmp(&ship, &MOX::combat_data[0], sizeof(ship)) == 0);
    MOX::PLAYER_NUM = 1;
    MOX::player[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    DOCTEST_CHECK(COMBINIT::Local_Tactical_Observer() == 1);
    MOX::combat_attacker_player = 2;
    MOX::combat_defender_player = 3;
    DOCTEST_CHECK(COMBINIT::Local_Tactical_Observer() == -1);
    MOX::PLAYER_NUM = 0;
    MOX::combat_attacker_player = 0;
    RUSS::all_ai_game = 1;
    DOCTEST_CHECK(COMBINIT::Local_Tactical_Observer() == -1);
    RUSS::all_ai_game = 0;
    MOX::player[0].objectives = 0;
    DOCTEST_CHECK(COMBINIT::Local_Tactical_Observer() == -1);
    MOX::player[0] = player;
    MOX::player[0].eliminated = 1;
    DOCTEST_CHECK(COMBINIT::Local_Tactical_Observer() == -1);
    MOX::player[0] = player;
    DOCTEST_CHECK(std::memcmp(&player, &MOX::player[0], sizeof(player)) == 0);
    DOCTEST_CHECK(std::memcmp(&ship, &MOX::combat_data[0], sizeof(ship)) == 0);
    TEST_COMBINIT::Reset_();
}

DOCTEST_TEST_CASE("display speed scales from half to full drive integrity") {
    DOCTEST_CHECK(COMBINIT::Damaged_Drive_Combat_Speed(12, 10, 10) == 12);
    DOCTEST_CHECK(COMBINIT::Damaged_Drive_Combat_Speed(12, 8, 10) == 7);
    DOCTEST_CHECK(COMBINIT::Damaged_Drive_Combat_Speed(12, 5, 10) == 0);
    DOCTEST_CHECK(COMBINIT::Damaged_Drive_Combat_Speed(12, 3, 10) == 0);
}

DOCTEST_TEST_CASE("display speed safely handles destroyed and absent drives") {
    DOCTEST_CHECK(COMBINIT::Damaged_Drive_Combat_Speed(12, 0, 10) == 0);
    DOCTEST_CHECK(COMBINIT::Damaged_Drive_Combat_Speed(12, 0, 0) == 0);
}

DOCTEST_TEST_CASE("crystal keep retains monster captures and immobilizes retained Crystal ships") {
    struct s_case {
        uint8_t crystal_keep;
        int8_t owner;
        bool retained;
        uint8_t expected_speed;
    };
    const s_case cases[] = {
        {0, NONPLAYER_SHIP_TYPE_CRYSTAL, false, 4},
        {1, NONPLAYER_SHIP_TYPE_CRYSTAL, true, 0},
        {0, NONPLAYER_SHIP_TYPE_DRAGON, false, 4},
        {1, NONPLAYER_SHIP_TYPE_DRAGON, true, 4},
        {0, 1, true, 4},
        {1, 1, true, 4},
    };

    for (const s_case& value : cases) {
        TEST_COMBINIT::Reset_();
        TEST_COMBINIT::Set_Captured_Ship_(value.owner, 0, 4);
        COMBINIT::crystal_keep = value.crystal_keep;

        COMBINIT::End_Of_Combat(0, 0, 1);

        DOCTEST_CHECK(
            TEST_COMBINIT::Strategic_Ship_Killed_() == !value.retained);
        DOCTEST_CHECK(
            TEST_COMBINIT::Strategic_Design_Speed_() ==
            value.expected_speed);
        if (value.retained) {
            DOCTEST_CHECK(
                TEST_COMBINIT::Strategic_Ship_Owner_() == value.owner);
        }
    }
}

DOCTEST_TEST_CASE("attacker victory removes a satellite recaptured by the defender") {
    for (uint8_t capture_enabled = 0; capture_enabled <= 1; ++capture_enabled) {
        TEST_COMBINIT::Reset_();
        TEST_COMBINIT::Set_Defender_Recaptured_Satellite_();
        COMBINIT::satellite_capture_enabled = capture_enabled;

        COMBINIT::End_Of_Combat(0, 0, 1);

        DOCTEST_CHECK(TEST_COMBINIT::Satellite_Building_Removed_(
            0, BUILDING_STAR_BASE));
        DOCTEST_CHECK(COMBINIT::Take_Captured_Satellite(0) == -1);
    }
}

DOCTEST_TEST_CASE("attacker-captured satellite is retained only when enabled") {
    for (uint8_t capture_enabled = 0; capture_enabled <= 1; ++capture_enabled) {
        TEST_COMBINIT::Reset_();
        TEST_COMBINIT::Set_Attacker_Captured_Satellite_();
        COMBINIT::satellite_capture_enabled = capture_enabled;

        COMBINIT::End_Of_Combat(0, 0, 1);

        DOCTEST_CHECK(TEST_COMBINIT::Satellite_Building_Removed_(
            0, BUILDING_STAR_BASE));
        DOCTEST_CHECK(COMBINIT::Take_Captured_Satellite(0) ==
            (capture_enabled != 0 ? BUILDING_STAR_BASE : -1));
    }
}

DOCTEST_TEST_CASE("destroyed attacker-captured satellite is not retained") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Attacker_Captured_Satellite_(true);
    COMBINIT::satellite_capture_enabled = 1;

    COMBINIT::End_Of_Combat(0, 0, 1);

    DOCTEST_CHECK(TEST_COMBINIT::Satellite_Building_Removed_(
        0, BUILDING_STAR_BASE));
    DOCTEST_CHECK(COMBINIT::Take_Captured_Satellite(0) == -1);
}

DOCTEST_TEST_CASE("survival pods recover more than eight leaders after an expanded battle") {
    TEST_COMBINIT::Reset_();
    const int16_t leader_count = MAX_LEADERS - 1;
    MOX::combat_total_ship_count = leader_count * 2 + 1;
    MOX::player[0].tech_applications[TECH_APP_SURVIVAL_PODS] = TECH_RESEARCH_STATUS_RESEARCHED;
    for (int16_t i = 0; i < leader_count; ++i) {
        const int16_t destroyed_idx = i + 1;
        const int16_t surviving_idx = leader_count + i + 1;
        s_combat_data& destroyed = MOX::combat_data[destroyed_idx];
        destroyed.ship_idx = destroyed_idx;
        destroyed.owner = 0;
        destroyed.unit_status = 5;
        destroyed.officer_idx = i;
        MOX::ship[destroyed_idx].officer_index = i;
        MOX::leaders[i].location = destroyed_idx;
        MOX::leaders[i].player_index = 0;
        s_combat_data& surviving = MOX::combat_data[surviving_idx];
        surviving.ship_idx = surviving_idx;
        surviving.owner = 0;
        surviving.officer_idx = -1;
        MOX::ship[surviving_idx].officer_index = -1;
    }

    COMBINIT::End_Of_Combat(0, 0, 1);

    for (int16_t i = 0; i < leader_count; ++i) {
        const int16_t surviving_idx = leader_count + i + 1;
        DOCTEST_CHECK((MOX::leaders[i].location == surviving_idx));
        DOCTEST_CHECK((MOX::ship[surviving_idx].officer_index == i));
        DOCTEST_CHECK((MOX::combat_data[surviving_idx].officer_idx == i));
    }
}

DOCTEST_TEST_CASE("large attacker deployment fills both halves before advancing") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Deployment_Fleet_(18, 0, SHIP_SIZE_HUGE, false);

    COMBINIT::Deploy_Ships();

    DOCTEST_CHECK(TEST_COMBINIT::Count_Ships_At_(0, 25, 2) == 1);
    DOCTEST_CHECK(TEST_COMBINIT::Count_Ships_At_(0, 22, 30) == 1);
    DOCTEST_CHECK(TEST_COMBINIT::Count_Ships_At_(0, 22, 34) == 1);
}

DOCTEST_TEST_CASE("large defender deployment commits only valid candidates") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Deployment_Fleet_(0, 18, SHIP_SIZE_HUGE, true);

    COMBINIT::Deploy_Ships();

    DOCTEST_CHECK(TEST_COMBINIT::Defender_Placements_Are_Valid_());
    DOCTEST_CHECK(TEST_COMBINIT::Count_Ships_At_(1, 58, 30) == 1);
    DOCTEST_CHECK(TEST_COMBINIT::Count_Ships_At_(1, 58, 34) == 1);
}

DOCTEST_TEST_CASE("deployment preserves the configured tactical planet size class") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Deployment_Fleet_(1, 0, SHIP_SIZE_SMALL, false);
    TEST_COMBINIT::Set_Combat_Planet_Size_(SHIP_SIZE_LARGE);

    COMBINIT::Deploy_Ships();

    DOCTEST_CHECK(TEST_COMBINIT::Combat_Planet_Size_() == SHIP_SIZE_LARGE);
}

DOCTEST_TEST_CASE("500 ships per side deploy without overlap in either orientation") {
    for (int16_t local_player = 0; local_player <= 1; ++local_player) {
        for (int8_t size = SHIP_SIZE_SMALL; size <= SHIP_SIZE_DOOM_STAR; ++size) {
            TEST_COMBINIT::Reset_();
            TEST_COMBINIT::Set_Deployment_Fleet_(500, 500, size, true);
            MOX::PLAYER_NUM = local_player;
            COMBINIT::Deploy_Ships();

            DOCTEST_REQUIRE(MOX::combat_total_ship_count == 1002);
            DOCTEST_CHECK(MOX::combat_grid_width == 81);
            DOCTEST_CHECK(MOX::combat_grid_height == 340);
            DOCTEST_CHECK(TEST_COMBINIT::Defender_Placements_Are_Valid_());
            bool occupied[MAX_COMBAT_GRID_WIDTH][MAX_COMBAT_GRID_HEIGHT] = {};
            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                const s_combat_data& ship = MOX::combat_data[i];
                const int footprint = ship.size_class >= SHIP_SIZE_HUGE ? 3
                    : ship.size_class == SHIP_SIZE_SMALL                ? 1
                                                                        : 2;
                DOCTEST_REQUIRE((ship.x >= 0));
                DOCTEST_REQUIRE((ship.y >= 0));
                DOCTEST_REQUIRE(ship.x + footprint <= MOX::combat_grid_width);
                DOCTEST_REQUIRE(ship.y + footprint <= MOX::combat_grid_height);
                for (int x = ship.x; x < ship.x + footprint; ++x) {
                    for (int y = ship.y; y < ship.y + footprint; ++y) {
                        DOCTEST_CHECK_FALSE(occupied[x][y]);
                        occupied[x][y] = true;
                    }
                }
            }
        }
    }
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Deployment_Fleet_(100, 100, SHIP_SIZE_DOOM_STAR, true);
    COMBINIT::Deploy_Ships();
    DOCTEST_CHECK(MOX::combat_grid_height == ORIGINAL_COMBAT_GRID_HEIGHT);
}

DOCTEST_TEST_CASE("large battlefield camera clamps and wraps parallax without truncation") {
    TEST_COMBINIT::Reset_();
    MOX::combat_grid_height = MAX_COMBAT_GRID_HEIGHT;
    MOX::cur_combat_x = 0;
    MOX::cur_combat_y = 0;
    MOX::combat_near_y = 0;
    MOX::combat_mid_y = 0;
    MOX::combat_far_y = 0;
    COMBINIT::Snap_Center_Combat_Screen(80, 339);
    DOCTEST_CHECK(MOX::cur_combat_x == 49);
    DOCTEST_CHECK(MOX::cur_combat_y == 322);
    DOCTEST_CHECK(MOX::combat_near_y == (322 * 160) % 480);
    DOCTEST_CHECK(MOX::combat_mid_y == (322 * 80) % 480);
    DOCTEST_CHECK(MOX::combat_far_y == (322 * 20) % 480);
    COMBINIT::Snap_Center_Combat_Screen(0, 0);
    DOCTEST_CHECK(MOX::combat_near_y == 0);
    DOCTEST_CHECK(MOX::combat_mid_y == 0);
    DOCTEST_CHECK(MOX::combat_far_y == 0);
    TEST_COMBINIT::Reset_();
}

DOCTEST_TEST_CASE("special references retain ship index 255 and larger indices") {
    TEST_COMBINIT::Reset_();
    COMBINIT::Init_Special_Devices(500);
    s_combat_data& ship = MOX::combat_data[500];
    DOCTEST_CHECK((ship.stasis_source_idx == COMBAT_SHIP_NONE));
    DOCTEST_CHECK((ship.black_hole_source_idx == COMBAT_SHIP_NONE));
    DOCTEST_CHECK((ship.special_cooldown == COMBAT_SHIP_NONE));
    ship.stasis_source_idx = 255;
    ship.black_hole_source_idx = 999;
    ship.special_cooldown = 1000;
    DOCTEST_CHECK((ship.stasis_source_idx == 255));
    DOCTEST_CHECK((ship.black_hole_source_idx == 999));
    DOCTEST_CHECK((ship.special_cooldown == 1000));
}

DOCTEST_TEST_CASE("time warp BHG phase excludes ordinary ships") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Enable_Initiative_();
    TEST_COMBINIT::Set_Black_Hole_Target_(1, 0, true);
    TEST_COMBINIT::Set_Black_Hole_Target_(2, 0, false);

    COMBINIT::Increment_Bhg(0, true);

    DOCTEST_CHECK(TEST_COMBINIT::Destroyed_(1));
    DOCTEST_CHECK_FALSE(TEST_COMBINIT::Destroyed_(2));
    DOCTEST_CHECK(TEST_COMBINIT::Black_Hole_Count_(2) == 2);
}

DOCTEST_TEST_CASE("non-initiative BHG phase includes ordinary ships from the active side") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Black_Hole_Target_(1, 0, false);
    TEST_COMBINIT::Set_Black_Hole_Target_(2, 1, false);

    COMBINIT::Increment_Bhg(0, true);

    DOCTEST_CHECK(TEST_COMBINIT::Destroyed_(1));
    DOCTEST_CHECK_FALSE(TEST_COMBINIT::Destroyed_(2));
    DOCTEST_CHECK(TEST_COMBINIT::Black_Hole_Count_(2) == 2);
}

DOCTEST_TEST_CASE("simultaneous BHG collapses are collected before destruction") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Enable_Initiative_();
    TEST_COMBINIT::Set_Black_Hole_Target_(1, 0, false);
    TEST_COMBINIT::Set_Black_Hole_Target_(2, 1, false);

    COMBINIT::Increment_Bhg(0);

    DOCTEST_CHECK(TEST_COMBINIT::Destroyed_(1));
    DOCTEST_CHECK(TEST_COMBINIT::Destroyed_(2));
    DOCTEST_CHECK(TEST_COMBINIT::Destroyed_With_Black_Hole_State_(1));
    DOCTEST_CHECK(TEST_COMBINIT::Destroyed_With_Black_Hole_State_(2));
    DOCTEST_CHECK(TEST_COMBINIT::Black_Hole_Count_(1) == 0);
    DOCTEST_CHECK(TEST_COMBINIT::Black_Hole_Count_(2) == 0);
    DOCTEST_CHECK(TEST_COMBINIT::Black_Hole_Source_(1) == COMBAT_SHIP_NONE);
    DOCTEST_CHECK(TEST_COMBINIT::Black_Hole_Source_(2) == COMBAT_SHIP_NONE);
}

DOCTEST_TEST_CASE("intermediate initiative maintenance applies only to TWF ships") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Maintenance_Ship_(1, true);
    TEST_COMBINIT::Set_Maintenance_Ship_(2, false);

    COMBINIT::Recharge_Shields(true);
    COMBINIT::Repair_All_Combat_Ships(true);

    DOCTEST_CHECK(TEST_COMBINIT::Shield_Arc_Total_(1) == 3);
    DOCTEST_CHECK(TEST_COMBINIT::Shield_Arc_Total_(2) == 0);
    DOCTEST_CHECK(TEST_COMBINIT::Repair_Count_(1) == 1);
    DOCTEST_CHECK(TEST_COMBINIT::Repair_Count_(2) == 0);
}

DOCTEST_TEST_CASE("intermediate Time Warp pass does not reactivate ordinary missiles") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Exhausted_Missile_();

    COMBINIT::Reactivate_Combat_Missiles_For_Pass(true);

    DOCTEST_CHECK(TEST_COMBINIT::Missile_Travel_Distance_() == 0);
    DOCTEST_CHECK(TEST_COMBINIT::Missile_Is_Active_() == 0);

    COMBINIT::Reactivate_Combat_Missiles_For_Pass(false);

    DOCTEST_CHECK(TEST_COMBINIT::Missile_Travel_Distance_() == 120);
    DOCTEST_CHECK(TEST_COMBINIT::Missile_Is_Active_() == 1);
}

DOCTEST_TEST_CASE("fighter garrison replacement restores fixed ammunition") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Set_Depleted_Fighter_Garrison_();

    COMBINIT::End_Of_Turn_Bookeeping();

    DOCTEST_CHECK(TEST_COMBINIT::Fighter_Count_() == 5);
    DOCTEST_CHECK(TEST_COMBINIT::Fighter_Ammo_() == 1);
}

DOCTEST_TEST_CASE("zero-base-speed ships unload without an endless drive repair loop") {
    TEST_COMBAT_REPAIR::Reset_();
    TEST_COMBAT_REPAIR::Set_Zero_Speed_Unload_();

    COMBINIT::Unload_Combat_Ship(0);

    DOCTEST_CHECK(TEST_COMBAT_REPAIR::Speed_Calc_Count_() == 0);
    DOCTEST_CHECK(TEST_COMBAT_REPAIR::Drive_Current_Hits_() == 10);
    DOCTEST_CHECK(TEST_COMBAT_REPAIR::Stored_Drive_Damage_() == 0);
}

DOCTEST_TEST_CASE("subsystem damage writeback accepts the unsigned word domain") {
    const uint16_t maximum_hits[] = {32767, 32768, UINT16_MAX};

    for (uint16_t maximum : maximum_hits) {
        TEST_COMBAT_REPAIR::Reset_();
        TEST_COMBAT_REPAIR::Set_Subsystem_Unload_(maximum, maximum / 2);

        COMBINIT::Unload_Combat_Ship(0);

        DOCTEST_CHECK(TEST_COMBAT_REPAIR::Stored_Drive_Damage_() == 50);
        DOCTEST_CHECK(TEST_COMBAT_REPAIR::Stored_Shield_Damage_() == 50);
        DOCTEST_CHECK(TEST_COMBAT_REPAIR::Stored_Computer_Damage_() == 100);
    }
}

DOCTEST_TEST_CASE("regeneration remains disabled by default") {
    TEST_COMBAT_REPAIR::Reset_();
    TEST_COMBAT_REPAIR::Set_Special_(0, SPECIAL_REGENERATION);

    COMBINIT::Repair_All_Combat_Ships();

    DOCTEST_CHECK(TEST_COMBAT_REPAIR::Repair_Count_(0) == 0);
}

DOCTEST_TEST_CASE("denied scroll polls still advance acceleration") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Prepare_Scroll_(1);
    COMBINIT::Scroll_Combat_Screen();

    TEST_COMBINIT::Prepare_Scroll_(1);
    COMBINIT::Scroll_Combat_Screen();

    DOCTEST_CHECK(TEST_COMBINIT::Scroll_Draw_Count_() == 1);
    DOCTEST_CHECK(TEST_COMBINIT::Combat_X_() == 7);
}

DOCTEST_TEST_CASE("zero scroll interval remains unrestricted") {
    TEST_COMBINIT::Reset_();
    TEST_COMBINIT::Prepare_Scroll_(0);

    COMBINIT::Scroll_Combat_Screen();

    DOCTEST_CHECK(TEST_COMBINIT::Scroll_Draw_Count_() == 1);
    DOCTEST_CHECK(TEST_COMBINIT::Combat_X_() == 9);
}

DOCTEST_TEST_CASE("working regeneration uses its configured repair rate") {
    TEST_COMBAT_REPAIR::Reset_();
    COMBINIT::amoeba_regeneration = 1;
    COMBINIT::repair_rate.regen = 27;
    TEST_COMBAT_REPAIR::Set_Special_(0, SPECIAL_REGENERATION);

    COMBINIT::Repair_All_Combat_Ships();

    DOCTEST_CHECK(TEST_COMBAT_REPAIR::Repair_Amount_(0) == 27);
}

DOCTEST_TEST_CASE("regeneration is additive with other repair sources") {
    TEST_COMBAT_REPAIR::Reset_();
    COMBINIT::amoeba_regeneration = 1;
    COMBINIT::repair_rate = {7, 11, 13};
    TEST_COMBAT_REPAIR::Set_Special_(0, SPECIAL_AUTOMATED_REPAIR_UNIT);
    TEST_COMBAT_REPAIR::Set_Special_(0, SPECIAL_REGENERATION);
    TEST_COMBAT_REPAIR::Set_Cybernetic_(0);
    TEST_COMBAT_REPAIR::Set_Engineer_Bonus_(0, 4);

    COMBINIT::Repair_All_Combat_Ships();

    DOCTEST_CHECK(TEST_COMBAT_REPAIR::Repair_Amount_(0) == 35);
}

DOCTEST_TEST_CASE("Cybernetic repair rate uses the complete unsigned byte domain") {
    const uint8_t rates[] = {0, 127, 128, 255};

    for (uint8_t rate : rates) {
        TEST_COMBAT_REPAIR::Reset_();
        COMBINIT::repair_rate.cyber = rate;
        TEST_COMBAT_REPAIR::Set_Cybernetic_(0);

        COMBINIT::Repair_All_Combat_Ships();

        DOCTEST_CHECK(TEST_COMBAT_REPAIR::Repair_Count_(0) == (rate != 0));
        if (rate != 0) {
            DOCTEST_CHECK(TEST_COMBAT_REPAIR::Repair_Amount_(0) == rate);
        }
    }
}

DOCTEST_TEST_CASE("unsigned repair sources accumulate without low-word wrapping") {
    TEST_COMBAT_REPAIR::Reset_();
    COMBINIT::amoeba_regeneration = 1;
    COMBINIT::repair_rate = {255, 255, 255};
    TEST_COMBAT_REPAIR::Set_Special_(0, SPECIAL_AUTOMATED_REPAIR_UNIT);
    TEST_COMBAT_REPAIR::Set_Special_(0, SPECIAL_REGENERATION);
    TEST_COMBAT_REPAIR::Set_Cybernetic_(0);
    TEST_COMBAT_REPAIR::Set_Engineer_Bonus_(0, 4);

    COMBINIT::Repair_All_Combat_Ships();

    DOCTEST_CHECK(TEST_COMBAT_REPAIR::Repair_Amount_(0) == 769);
}

DOCTEST_TEST_CASE("repair accumulation saturates at the repair API boundary") {
    TEST_COMBAT_REPAIR::Reset_();
    COMBINIT::repair_rate.cyber = 255;
    TEST_COMBAT_REPAIR::Set_Cybernetic_(0);
    TEST_COMBAT_REPAIR::Set_Engineer_Bonus_(0, INT16_MAX);

    COMBINIT::Repair_All_Combat_Ships();

    DOCTEST_CHECK(TEST_COMBAT_REPAIR::Repair_Amount_(0) == INT16_MAX);
}

DOCTEST_TEST_CASE("damaged regeneration does not repair") {
    TEST_COMBAT_REPAIR::Reset_();
    COMBINIT::amoeba_regeneration = 1;
    TEST_COMBAT_REPAIR::Set_Special_(0, SPECIAL_REGENERATION);
    TEST_COMBAT_REPAIR::Damage_Special_(0, SPECIAL_REGENERATION);

    COMBINIT::Repair_All_Combat_Ships();

    DOCTEST_CHECK(TEST_COMBAT_REPAIR::Repair_Count_(0) == 0);
}