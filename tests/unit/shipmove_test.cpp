#include "pch.h"

#include "doctest/doctest.h"
#include "game/shipmove.h"
#include "lua/lua_game_ship_identity.h"

namespace TEST_SHIPMOVE {
    void Reset_();
    void Set_Hyperspace_Flux_(bool active);
    int16_t Movement_Speed_();
    bool Navigator_Pointer_Supplied_();
} // namespace TEST_SHIPMOVE

DOCTEST_TEST_CASE("movement to assigned planet star keeps auto-colonization") {
    struct s_case {
        int16_t initial_location;
        int8_t turns_left;
        int16_t expected_location;
    };

    const s_case cases[] = {
        {0, 0, 7},
        {0, 1, 7 + SHIP_LOCATION_WORMHOLE_OFFSET},
        {SHIP_LOCATION_MOVING_OFFSET, 1, 7 + SHIP_LOCATION_MOVING_OFFSET},
    };

    for (const s_case& test_case : cases) {
        TEST_SHIPMOVE::Reset_();

        MOX::planet[3].star_index = 7;
        MOX::ship[0].d.ship_type = SHIP_TYPE_COLONY;
        MOX::ship[0].d.ship_weapon[0].type = 3;
        MOX::ship[0].d.ship_weapon[1].type = 1;
        MOX::ship[0].location = test_case.initial_location;

        int16_t ship_list[] = {0, -1};
        s_ship_move_info move_info = {};
        move_info.turns_left = test_case.turns_left;

        SHIPMOVE::Make_Ships_Move_To(ship_list, 7, &move_info);

        DOCTEST_CHECK((MOX::ship[0].location == test_case.expected_location));
        DOCTEST_CHECK(MOX::ship[0].d.ship_weapon[0].type == 3);
        DOCTEST_CHECK(MOX::ship[0].d.ship_weapon[1].type == 1);
    }
}

DOCTEST_TEST_CASE("new nonplayer ship lifetime advances Lua slot generation") {
    TEST_SHIPMOVE::Reset_();
    MOX::NUM_SHIPS = 0;
    MOX::ship[0].owner = MAX_PLAYERS;
    MOX::ship[0].status = 5;

    DOCTEST_REQUIRE(SHIPMOVE::Create_Nonplayer_Ship(
                        MAX_PLAYERS, nullptr, 0, 0) == 0);
    DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(0) == 1);

    MOX::ship[0].status = 5;
    DOCTEST_REQUIRE(SHIPMOVE::Create_Nonplayer_Ship(
                        MAX_PLAYERS, nullptr, 0, 0) == 0);
    DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(0) == 2);
}

DOCTEST_TEST_CASE("movement away from assigned planet clears auto-colonization") {
    TEST_SHIPMOVE::Reset_();

    MOX::planet[3].star_index = 7;
    MOX::ship[0].d.ship_type = SHIP_TYPE_OUTPOST;
    MOX::ship[0].d.ship_weapon[0].type = 3;
    MOX::ship[0].d.ship_weapon[1].type = 1;

    int16_t ship_list[] = {0, -1};
    s_ship_move_info move_info = {};

    SHIPMOVE::Make_Ships_Move_To(ship_list, 8, &move_info);

    DOCTEST_CHECK(MOX::ship[0].d.ship_weapon[0].type == PLANET_INDEX_NONE);
    DOCTEST_CHECK(MOX::ship[0].d.ship_weapon[1].type == 0);
}

DOCTEST_TEST_CASE("in-flight recalculation preserves departure speed during hyperspace flux") {
    TEST_SHIPMOVE::Reset_();
    TEST_SHIPMOVE::Set_Hyperspace_Flux_(true);

    MOX::player[0].ship_speed = 8;
    MOX::ship[0].status = 1;
    MOX::ship[0].location = SHIP_LOCATION_MOVING_OFFSET + 1;
    MOX::ship[0].x = 30;
    MOX::ship[0].travelling_speed = 4;

    int16_t ship_list[] = {0, -1};
    s_ship_move_info move_info;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);

    DOCTEST_CHECK(move_info.moving == 1);
    DOCTEST_CHECK(move_info.hyperspace_flux == 0);
    DOCTEST_CHECK(move_info.traveling_speed == 4);
    DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 4);
}

DOCTEST_TEST_CASE("in-flight recalculation keeps route after fuel range is lost") {
    TEST_SHIPMOVE::Reset_();

    MOX::player[0].ship_range = 0;
    MOX::ship[0].status = 1;
    MOX::ship[0].location = SHIP_LOCATION_MOVING_OFFSET + 1;
    MOX::ship[0].x = 30;
    MOX::ship[0].travelling_speed = 4;

    int16_t ship_list[] = {0, -1};
    s_ship_move_info move_info;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);

    DOCTEST_CHECK(move_info.moving == 1);
    DOCTEST_CHECK(move_info.traveling_speed == 4);
}

DOCTEST_TEST_CASE("extended fuel range rounds odd ranges up") {
    TEST_SHIPMOVE::Reset_();

    MOX::player[0].ship_range = 9;
    MOX::star[1].x = 14 * 30;

    DOCTEST_CHECK_FALSE(SHIPMOVE::Star_In_Normal_Range_Of_Player(&MOX::star[1], 0));
    DOCTEST_CHECK(SHIPMOVE::Star_In_Extended_Range_Of_Player(&MOX::star[1], 0));
}

DOCTEST_TEST_CASE("movement stores the numeric navigator bonus") {
    TEST_SHIPMOVE::Reset_();

    MOX::leaders[0].eta = 0;
    MOX::leaders[0].xp = 5;
    MOX::leaders[0].special_skills = LEADER_SHIP_SKILL_NAVIGATOR2;
    MOX::ship[0].officer_index = 0;

    int16_t ship_list[] = {0, -1};
    s_ship_move_info move_info;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);
    SHIPMOVE::Make_Ships_Move_To(ship_list, 1, &move_info);

    DOCTEST_CHECK(move_info.has_navigator == 2);
    DOCTEST_CHECK(MOX::ship[0].group_has_navigator == 2);
    DOCTEST_CHECK(MOX::ship[0].travelling_speed == 6);
}

DOCTEST_TEST_CASE("Phase Shifter scales departing fleet speed using an exact ratio") {
    struct s_case {
        s_ratio16 multiplier;
        uint8_t base_speed;
        bool first_has_phase_shifter;
        bool second_has_phase_shifter;
        int16_t expected_speed;
    };
    const s_case cases[] = {
        {{1, 1}, 4, true, true, 4},
        {{2, 1}, 4, true, true, 8},
        {{1, 2}, 4, true, true, 2},
        {{2, 1}, 4, false, false, 4},
        {{2, 1}, 4, true, false, 4},
        {{2, 1}, 4, false, true, 4},
        {{1, 2}, 4, true, false, 2},
        {{1, 2}, 4, false, true, 2},
        {{1, 2}, 5, true, true, 2},
        {{1, 2}, 1, true, true, 1},
        {{32767, 1}, 255, true, true, 254},
    };

    for (const s_case& test_case : cases) {
        TEST_SHIPMOVE::Reset_();
        SHIPMOVE::phase_shifter_speed_multiplier = test_case.multiplier;
        MOX::player[0].ship_speed = test_case.base_speed;
        MOX::NUM_SHIPS = 2;
        MOX::ship[1] = MOX::ship[0];
        const bool equipped[] = {
            test_case.first_has_phase_shifter,
            test_case.second_has_phase_shifter,
        };
        for (int i = 0; i < 2; ++i) {
            if (equipped[i]) {
                MOX::ship[i].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
                    1 << (SPECIAL_PHASE_SHIFTER % 8);
            }
        }
        MOX::ship[0].d.combat_speed = 7;

        int16_t ship_list[] = {0, 1, -1};
        s_ship_move_info move_info;
        SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);

        DOCTEST_CHECK_FALSE(move_info.immobile);
        DOCTEST_CHECK(move_info.traveling_speed == test_case.expected_speed);
        DOCTEST_CHECK(move_info.turns_left == (4 + test_case.expected_speed - 1) / test_case.expected_speed);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == test_case.expected_speed);
        SHIPMOVE::Make_Ships_Move_To(ship_list, 1, &move_info);
        DOCTEST_CHECK(MOX::ship[0].travelling_speed == test_case.expected_speed);
        DOCTEST_CHECK(MOX::ship[1].travelling_speed == test_case.expected_speed);
        DOCTEST_CHECK(MOX::ship[0].d.combat_speed == 7);
    }
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("Phase Shifter does not create propulsion or permit nonpositive speed") {
    struct s_case {
        s_ratio16 multiplier;
        uint8_t base_speed;
    };
    const s_case cases[] = {
        {{2, 1}, 0},
        {{0, 1}, 4},
        {{-1, 2}, 4},
    };
    for (const s_case& test_case : cases) {
        TEST_SHIPMOVE::Reset_();
        SHIPMOVE::phase_shifter_speed_multiplier = test_case.multiplier;
        MOX::player[0].ship_speed = test_case.base_speed;
        MOX::ship[0].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
            1 << (SPECIAL_PHASE_SHIFTER % 8);

        int16_t ship_list[] = {0, -1};
        s_ship_move_info move_info;
        SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);
        DOCTEST_CHECK(move_info.immobile == 1);
        DOCTEST_CHECK(move_info.moving == 0);
        DOCTEST_CHECK(move_info.turns_left == UINT8_MAX);
    }
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("Phase Shifter leaves Navigator and Jump Gate bonuses additive") {
    TEST_SHIPMOVE::Reset_();
    SHIPMOVE::phase_shifter_speed_multiplier = {2, 1};
    MOX::ship[0].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
        1 << (SPECIAL_PHASE_SHIFTER % 8);
    MOX::leaders[0].xp = 5;
    MOX::leaders[0].special_skills = LEADER_SHIP_SKILL_NAVIGATOR2;
    MOX::ship[0].officer_index = 0;
    MOX::star[0].has_jumpgate = 1;
    MOX::star[1].has_jumpgate = 1;

    int16_t ship_list[] = {0, -1};
    s_ship_move_info move_info;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);
    DOCTEST_CHECK(move_info.traveling_speed == 13);
    DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 13);

    MOX::player[0].ship_speed = 127;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);
    DOCTEST_CHECK(move_info.traveling_speed == 254);
    DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 254);

    SHIPMOVE::jump_gate_speed_bonus = UINT8_MAX;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);
    DOCTEST_CHECK(move_info.traveling_speed == 254);
    SHIPMOVE::Make_Ships_Move_To(ship_list, 1, &move_info);
    MOX::ship[0].status = 1;
    MOX::ship[0].location = SHIP_LOCATION_MOVING_OFFSET + 1;
    MOX::ship[0].x = 30;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);
    DOCTEST_CHECK(move_info.traveling_speed == 254);
    DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 254);
    SHIPMOVE::jump_gate_speed_bonus = 3;
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("Phase Shifter is not reapplied to in-flight departure speed") {
    TEST_SHIPMOVE::Reset_();
    SHIPMOVE::phase_shifter_speed_multiplier = {2, 1};
    MOX::ship[0].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
        1 << (SPECIAL_PHASE_SHIFTER % 8);
    MOX::star[1].x = 600;
    MOX::NUM_STARS = 3;
    MOX::star[2].x = 540;

    int16_t ship_list[] = {0, -1};
    s_ship_move_info move_info;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);
    DOCTEST_REQUIRE(move_info.traveling_speed == 8);
    SHIPMOVE::Make_Ships_Move_To(ship_list, 1, &move_info);
    MOX::ship[0].status = 1;
    MOX::ship[0].location = SHIP_LOCATION_MOVING_OFFSET + 1;
    MOX::ship[0].x = 30;
    MOX::player[0].ship_speed = 6;

    for (int16_t target : {1, 2}) {
        SHIPMOVE::Ships_Try_To_Move_To(ship_list, target, &move_info);
        DOCTEST_CHECK(move_info.traveling_speed == 8);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 8);
    }
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("Phase Shifter does not change instantaneous travel routes") {
    TEST_SHIPMOVE::Reset_();
    SHIPMOVE::phase_shifter_speed_multiplier = {2, 1};
    MOX::ship[0].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
        1 << (SPECIAL_PHASE_SHIFTER % 8);
    MOX::star[0].wormhole_star_id = 1;

    int16_t ship_list[] = {0, -1};
    s_ship_move_info move_info;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);
    DOCTEST_CHECK(move_info.uses_wormhole == 1);
    DOCTEST_CHECK(move_info.traveling_speed == UINT8_MAX);
    DOCTEST_CHECK(move_info.turns_left == 1);

    MOX::star[0].wormhole_star_id = -1;
    MOX::star[0].has_stargate = 1;
    MOX::star[1].has_stargate = 1;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);
    DOCTEST_CHECK(move_info.has_stargate_route == 1);
    DOCTEST_CHECK(move_info.traveling_speed == UINT8_MAX);
    DOCTEST_CHECK(move_info.turns_left == 1);
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("jump gate speed preserves the complete unsigned byte domain") {
    struct s_speed_case {
        uint8_t bonus;
        int16_t route_speed;
        uint8_t stored_speed;
    };
    const s_speed_case cases[] = {
        {0, 4, 4},
        {127, 131, 131},
        {128, 132, 132},
        {255, 259, 3},
    };

    for (const s_speed_case& test_case : cases) {
        TEST_SHIPMOVE::Reset_();

        SHIPMOVE::jump_gate_speed_bonus = test_case.bonus;
        SHIPMOVE::jump_gate_pass_nebula = true;
        MOX::star[0].has_jumpgate = 1;
        MOX::star[1].has_jumpgate = 1;

        int16_t ship_list[] = {0, -1};
        s_ship_move_info move_info;
        SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);

        DOCTEST_CHECK(move_info.has_jumpgate_route == 1);
        DOCTEST_CHECK(move_info.traveling_speed == test_case.stored_speed);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == test_case.route_speed);
        DOCTEST_CHECK_FALSE(TEST_SHIPMOVE::Navigator_Pointer_Supplied_());

        MOX::ship[0].status = 1;
        MOX::ship[0].location = SHIP_LOCATION_MOVING_OFFSET + 1;
        MOX::ship[0].x = 30;
        MOX::ship[0].travelling_speed = test_case.stored_speed;
        MOX::ship[0].special_device_damage_flags[0] = 1;
        SHIPMOVE::Ships_Try_To_Move_To(ship_list, 1, &move_info);

        DOCTEST_CHECK(move_info.has_jumpgate_route == 1);
        DOCTEST_CHECK(move_info.traveling_speed == test_case.stored_speed);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == test_case.route_speed);

        DOCTEST_CHECK(SHIPMOVE::Player_N_Turns_From_Star_To_Star(0, 0, 1) == 1);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == test_case.route_speed);
    }

    SHIPMOVE::jump_gate_speed_bonus = 3;
    SHIPMOVE::jump_gate_pass_nebula = false;
}

DOCTEST_TEST_CASE("communications relay ranges use configured parsec limits") {
    TEST_SHIPMOVE::Reset_();

    MOX::ship[0].status = 1;
    MOX::ship[0].x = 90;
    MOX::player[0].tech_applications[TECH_APP_TACHYON_COMMUNICATIONS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::star[0].has_tachyon = 1;

    DOCTEST_CHECK(SHIPMOVE::Can_Order_Ship(&MOX::ship[0]));

    MOX::ship[0].x = 120;
    DOCTEST_CHECK_FALSE(SHIPMOVE::Can_Order_Ship(&MOX::ship[0]));

    MOX::ship[0].x = 180;
    MOX::star[0].has_tachyon = 0;
    MOX::star[0].has_subspace = 1;
    MOX::player[0].tech_applications[TECH_APP_SUB_SPACE_COMMUNICATIONS] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(SHIPMOVE::Can_Order_Ship(&MOX::ship[0]));

    MOX::ship[0].x = 210;
    DOCTEST_CHECK_FALSE(SHIPMOVE::Can_Order_Ship(&MOX::ship[0]));

    SHIPMOVE::ship_communication_range = {3, 6};
}

DOCTEST_TEST_CASE("splitting an in-flight fleet recalculates the remaining ships") {
    TEST_SHIPMOVE::Reset_();

    MOX::NUM_SHIPS = 2;
    MOX::NUM_STARS = 3;
    MOX::star[2].x = 180;
    MOX::star[2].y = 0;
    MOX::leaders[0].eta = 0;
    MOX::leaders[0].xp = 5;
    MOX::leaders[0].special_skills = LEADER_SHIP_SKILL_NAVIGATOR2;

    for (int16_t i = 0; i < 2; ++i) {
        MOX::ship[i].owner = 0;
        MOX::ship[i].status = 1;
        MOX::ship[i].location = SHIP_LOCATION_MOVING_OFFSET + 1;
        MOX::ship[i].x = 30;
        MOX::ship[i].y = 0;
        MOX::ship[i].group_has_navigator = 2;
        MOX::ship[i].travelling_speed = 6;
        MOX::ship[i].turns_left = 10;
        MOX::ship[i].officer_index = -1;
    }
    MOX::ship[0].officer_index = 0;

    int16_t ship_list[] = {0, -1};
    s_ship_move_info move_info;
    SHIPMOVE::Ships_Try_To_Move_To(ship_list, 2, &move_info);

    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(ship_list, 2, &move_info));
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&MOX::ship[0]) == 2);
    DOCTEST_CHECK(MOX::ship[0].travelling_speed == 6);
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&MOX::ship[1]) == 1);
    DOCTEST_CHECK(MOX::ship[1].group_has_navigator == 0);
    DOCTEST_CHECK(MOX::ship[1].travelling_speed == 4);
}

DOCTEST_TEST_CASE("joining an in-flight fleet keeps its target list terminated") {
    TEST_SHIPMOVE::Reset_();

    MOX::NUM_SHIPS = 4;
    MOX::NUM_STARS = 3;
    MOX::star[2].x = 180;
    for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
        s_ship_data& ship = MOX::ship[i];
        ship.owner = 0;
        ship.officer_index = -1;
        ship.x = 30;
        ship.y = 0;
    }
    for (int16_t i = 1; i < MOX::NUM_SHIPS; ++i) {
        MOX::ship[i].status = 1;
        MOX::ship[i].location = SHIP_LOCATION_MOVING_OFFSET + (i == 3 ? 1 : 2);
        MOX::ship[i].travelling_speed = 4;
    }

    int16_t ship_list[] = {0, -1};
    s_ship_move_info move_info = {};
    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(ship_list, 2, &move_info));
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&MOX::ship[0]) == 2);
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&MOX::ship[1]) == 2);
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&MOX::ship[2]) == 2);
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&MOX::ship[3]) == 1);
}

DOCTEST_TEST_CASE("complete in-flight fleet can return to its prior jump gate destination once in the same turn") {
    TEST_SHIPMOVE::Reset_();

    MOX::NUM_SHIPS = 2;
    MOX::NUM_STARS = 3;
    MOX::star[1].x = 120;
    MOX::star[2].x = 240;
    for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
        s_ship_data& ship = MOX::ship[i];
        ship.owner = 0;
        ship.status = 1;
        ship.location = SHIP_LOCATION_MOVING_OFFSET + 1;
        ship.x = 30;
        ship.travelling_speed = 7;
        ship.officer_index = -1;
        ship.special_device_damage_flags[0] = 1;
    }

    int16_t complete_fleet[] = {0, 1, -1};
    s_ship_move_info move_info;
    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(complete_fleet, 2, &move_info));
    DOCTEST_CHECK(move_info.has_jumpgate_route == 0);
    DOCTEST_CHECK((MOX::ship[0].prior_jump_gate_destination == 1));
    DOCTEST_CHECK((MOX::ship[1].prior_jump_gate_destination == 1));

    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(complete_fleet, 1, &move_info));
    DOCTEST_CHECK(move_info.has_jumpgate_route == 1);
    DOCTEST_CHECK(move_info.traveling_speed == 7);
    DOCTEST_CHECK((MOX::ship[0].prior_jump_gate_destination == -1));
    DOCTEST_CHECK((MOX::ship[1].prior_jump_gate_destination == -1));
}

DOCTEST_TEST_CASE("prior jump gate destination does not grant a partial fleet or survive the turn") {
    TEST_SHIPMOVE::Reset_();

    MOX::NUM_SHIPS = 2;
    MOX::NUM_STARS = 3;
    MOX::star[1].x = 120;
    MOX::star[2].x = 240;
    for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
        s_ship_data& ship = MOX::ship[i];
        ship.owner = 0;
        ship.status = 1;
        ship.location = SHIP_LOCATION_MOVING_OFFSET + 2;
        ship.x = 30;
        ship.travelling_speed = 4;
        ship.officer_index = -1;
        ship.prior_jump_gate_destination = 1;
        ship.prior_jump_gate_stardate = MOX::stardate;
    }

    int16_t partial_fleet[] = {0, -1};
    s_ship_move_info move_info;
    SHIPMOVE::Ships_Try_To_Move_To(partial_fleet, 1, &move_info);
    DOCTEST_CHECK(move_info.has_jumpgate_route == 0);
    DOCTEST_CHECK(move_info.traveling_speed == 4);

    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(partial_fleet, 1, &move_info));
    DOCTEST_CHECK(move_info.has_jumpgate_route == 0);
    DOCTEST_CHECK((MOX::ship[0].prior_jump_gate_destination == -1));
    DOCTEST_CHECK((MOX::ship[1].prior_jump_gate_destination == -1));

    int16_t complete_fleet[] = {0, 1, -1};
    for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
        MOX::ship[i].status = 1;
        MOX::ship[i].location = SHIP_LOCATION_MOVING_OFFSET + 2;
        MOX::ship[i].x = 30;
        MOX::ship[i].prior_jump_gate_destination = 1;
        MOX::ship[i].prior_jump_gate_stardate = MOX::stardate;
    }
    MOX::stardate++;
    SHIPMOVE::Ships_Try_To_Move_To(complete_fleet, 1, &move_info);
    DOCTEST_CHECK(move_info.has_jumpgate_route == 0);
    DOCTEST_CHECK(move_info.traveling_speed == 4);
}

DOCTEST_TEST_CASE("unstable wormhole accepts a fleet with one turn remaining") {
    TEST_SHIPMOVE::Reset_();
    MOX::ship[0].status = 1;
    MOX::ship[0].turns_left = 1;
    MOX::ship[0].location = SHIP_LOCATION_MOVING_OFFSET + 1;

    s_wormhole_data result = {};
    SHIPMOVE::Do_Wormhole_Event(0, &result);

    DOCTEST_CHECK(result.success == 1);
    DOCTEST_CHECK(MOX::ship[0].status == 0);
    DOCTEST_CHECK((MOX::ship[0].location == 1));
}

DOCTEST_TEST_CASE("unstable wormhole excludes a fleet already arriving") {
    TEST_SHIPMOVE::Reset_();
    MOX::ship[0].status = 1;
    MOX::ship[0].turns_left = 0;
    MOX::ship[0].location = SHIP_LOCATION_MOVING_OFFSET + 1;

    s_wormhole_data result = {};
    SHIPMOVE::Do_Wormhole_Event(0, &result);

    DOCTEST_CHECK(result.success == 0);
    DOCTEST_CHECK(MOX::ship[0].status == 1);
}