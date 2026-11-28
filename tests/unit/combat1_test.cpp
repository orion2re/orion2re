#include "pch.h"

#include "doctest/doctest.h"
#include "game/combat1.h"

namespace TEST_COMBAT1 {
    void Reset_();
    int Retarget_Count_();
    void Set_Time_Warp_Facilitator_();
    void Set_In_Stasis_();
    void Set_Phasing_Cloak_(int16_t ship_idx, bool time_warp);
    uint8_t Phasing_Timer_(int16_t ship_idx);
    void Set_Ordinary_Cloak_(
        int16_t ship_idx,
        int16_t special_id,
        int8_t cloak_state,
        bool damaged = false);
    int8_t Cloak_State_(int16_t ship_idx);
    int Cloak_Draw_Count_();
} // namespace TEST_COMBAT1

namespace TEST_STASIS_AREA_EFFECTS {
    void Reset_();
    void Set_Special_(int16_t ship_idx, int16_t special_id);
    void Set_In_Stasis_(int16_t ship_idx, bool in_stasis);
} // namespace TEST_STASIS_AREA_EFFECTS

DOCTEST_TEST_CASE("initiative missile pass retargets fighters from both sides") {
    TEST_COMBAT1::Reset_();
    CMBTMIS::smarter_fighter_retargeting = true;
    MOX::combat_init = 1;
    for (int16_t i = 0; i < 2; ++i) {
        s_missile& fighter = MOX::missile[i];
        fighter.type = WEAPON_INTERCEPTOR;
        fighter.quantity = 4;
        fighter.owner = factions::From_Legacy(i);
        fighter.source_unit_idx = i;
        fighter.target_unit_idx = 1 - i;
        fighter.fighter_count = 2;
    }
    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;

    COMBAT1::Seeking_Missiles(game::owner_slot{0}, false);

    DOCTEST_CHECK(TEST_COMBAT1::Retarget_Count_() == 2);
    CMBTMIS::smarter_fighter_retargeting = false;
}

DOCTEST_TEST_CASE("fighter reaching a destroyed ship retargets instead of disappearing") {
    TEST_COMBAT1::Reset_();
    CMBTMIS::smarter_fighter_retargeting = true;
    MOX::combat_init = 1;
    MOX::combat_data[1].unit_status = 5;
    s_missile& fighter = MOX::missile[0];
    fighter.type = WEAPON_INTERCEPTOR;
    fighter.quantity = 4;
    fighter.owner = factions::From_Legacy(0);
    fighter.target_unit_idx = 1;
    fighter.fighter_count = 2;
    fighter.travel_dist = 1;
    fighter.is_active = 1;
    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;

    COMBAT1::Seeking_Missiles(game::owner_slot{0}, false);

    DOCTEST_CHECK(TEST_COMBAT1::Retarget_Count_() == 2);
    DOCTEST_CHECK(fighter.type == WEAPON_INTERCEPTOR);
    CMBTMIS::smarter_fighter_retargeting = false;
}

DOCTEST_TEST_CASE("time warp pass includes only functional TWF ships") {
    TEST_COMBAT1::Reset_();

    DOCTEST_CHECK(COMBAT1::Combat_Ship_Participates_In_Pass(0, false));
    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Ship_Participates_In_Pass(0, true));

    TEST_COMBAT1::Set_Time_Warp_Facilitator_();
    DOCTEST_CHECK(COMBAT1::Combat_Ship_Participates_In_Pass(0, true));

    TEST_COMBAT1::Set_In_Stasis_();
    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Ship_Participates_In_Pass(0, true));
}

DOCTEST_TEST_CASE("intermediate initiative cloak maintenance applies only to TWF ships") {
    TEST_COMBAT1::Reset_();
    TEST_COMBAT1::Set_Phasing_Cloak_(0, true);
    TEST_COMBAT1::Set_Phasing_Cloak_(1, false);

    COMBAT1::Update_Phasing_Cloak_Timers(true);

    DOCTEST_CHECK(TEST_COMBAT1::Phasing_Timer_(0) == 1);
    DOCTEST_CHECK(TEST_COMBAT1::Phasing_Timer_(1) == 2);
}

DOCTEST_TEST_CASE("ordinary recloaking accepts every enabled working cloak") {
    const int16_t cloak_types[] = {
        SPECIAL_CLOAKING_DEVICE,
        SPECIAL_PHASING_CLOAK,
        SPECIAL_STEALTH_FIELD,
    };

    for (int16_t special_id : cloak_types) {
        TEST_COMBAT1::Reset_();
        CMBTFIR2::combat_bonus.stealth_field_beam_defense = 50;
        TEST_COMBAT1::Set_Ordinary_Cloak_(0, special_id, 2);
        COMBAT1::recloak_next_turn = 1;

        COMBAT1::Update_Ordinary_Cloak(0);

        DOCTEST_CHECK(TEST_COMBAT1::Cloak_State_(0) == 1);
        DOCTEST_CHECK(TEST_COMBAT1::Cloak_Draw_Count_() == 1);
    }
}

DOCTEST_TEST_CASE("classic ordinary recloaking waits one start of turn") {
    TEST_COMBAT1::Reset_();
    TEST_COMBAT1::Set_Ordinary_Cloak_(
        0, SPECIAL_PHASING_CLOAK, 2);

    COMBAT1::Update_Ordinary_Cloak(0);
    DOCTEST_CHECK(TEST_COMBAT1::Cloak_State_(0) == 2);

    COMBAT1::Init_Ship_For_Start_Of_Turn(0);
    DOCTEST_CHECK(TEST_COMBAT1::Cloak_State_(0) == 3);

    COMBAT1::Update_Ordinary_Cloak(0);
    DOCTEST_CHECK(TEST_COMBAT1::Cloak_State_(0) == 1);
}

DOCTEST_TEST_CASE("ordinary recloaking rejects disabled or damaged cloaks") {
    TEST_COMBAT1::Reset_();
    TEST_COMBAT1::Set_Ordinary_Cloak_(
        0, SPECIAL_STEALTH_FIELD, 2);
    COMBAT1::recloak_next_turn = 1;
    CMBTFIR2::combat_bonus.stealth_field_beam_defense = 0;
    COMBAT1::Update_Ordinary_Cloak(0);
    DOCTEST_CHECK(TEST_COMBAT1::Cloak_State_(0) == 2);

    TEST_COMBAT1::Reset_();
    TEST_COMBAT1::Set_Ordinary_Cloak_(
        0, SPECIAL_CLOAKING_DEVICE, 2, true);
    COMBAT1::recloak_next_turn = 1;
    COMBAT1::Update_Ordinary_Cloak(0);
    DOCTEST_CHECK(TEST_COMBAT1::Cloak_State_(0) == 2);
    DOCTEST_CHECK(TEST_COMBAT1::Cloak_Draw_Count_() == 0);
}

DOCTEST_TEST_CASE("generated Antaran ships retain tactical movement") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& ship = MOX::combat_data[0];
    ship.owner = factions::From_Legacy(MAX_PLAYERS);
    ship.previous_owner = factions::From_Legacy(MAX_PLAYERS);
    ship.ship_id = game::NO_SHIP;
    ship.ftl_type = DRIVE_HYPER_DRIVE;
    ship.combat_speed_base = 12;
    ship.drive_max_hits = 10;
    ship.drive_current_hits = 10;

    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 12);

    ship.ftl_type = DRIVE_NO_DRIVE;
    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 0);
}

DOCTEST_TEST_CASE("stasis disables warp dissipator") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    MOX::combat_data[1].owner = factions::From_Legacy(1);
    TEST_STASIS_AREA_EFFECTS::Set_Special_(1, SPECIAL_WARP_DISSIPATOR);

    DOCTEST_CHECK(COMBAT1::Enemy_Dissipator_Exists(game::owner_slot{0}) == 1);

    TEST_STASIS_AREA_EFFECTS::Set_In_Stasis_(1, true);
    DOCTEST_CHECK(COMBAT1::Enemy_Dissipator_Exists(game::owner_slot{0}) == 0);
}

DOCTEST_TEST_CASE("continuing effect cleanup clears outgoing and incoming links") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& source = MOX::combat_data[0];
    s_combat_data& target = MOX::combat_data[1];
    source.special_cooldown = 1;
    target.special_cooldown = 0;
    target.stasis_source_idx = 0;
    target.black_hole_flag = 1;
    target.black_hole_source_idx = 0;

    COMBAT1::Remove_Continuing_Effect_Specials(0);

    DOCTEST_CHECK((source.special_cooldown == COMBAT_SHIP_NONE));
    DOCTEST_CHECK((target.special_cooldown == COMBAT_SHIP_NONE));
    DOCTEST_CHECK((target.stasis_source_idx == COMBAT_SHIP_NONE));
    DOCTEST_CHECK(target.black_hole_flag == 0);
    DOCTEST_CHECK((target.black_hole_source_idx == COMBAT_SHIP_NONE));
}

DOCTEST_TEST_CASE("continuous BHG keeps one shot committed to an active target") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    MOX::combat_data[0].weapons[0].weapon_id = WEAPON_BLACK_HOLE_GENERATOR;
    MOX::combat_data[0].weapons[0].count = 2;
    MOX::combat_data[1].black_hole_source_idx = 0;

    COMBAT1::Init_Ship_For_Start_Of_Turn(0);
    DOCTEST_CHECK(MOX::combat_data[0].weapons[0].shots_left == 2);

    COMBAT1::continuous_bhg = 1;
    COMBAT1::Init_Ship_For_Start_Of_Turn(0);
    DOCTEST_CHECK(MOX::combat_data[0].weapons[0].shots_left == 1);
}

DOCTEST_TEST_CASE("start-of-turn initialization refreshes defensive-fire range") {
    TEST_STASIS_AREA_EFFECTS::Reset_();
    s_combat_data& attacker = MOX::combat_data[0];
    attacker.owner = factions::From_Legacy(0);
    attacker.weapons[0].weapon_id = WEAPON_LASER_CANNON;
    attacker.weapons[0].count = 1;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;

    COMBAT1::Init_Ship_For_Start_Of_Turn(0);

    DOCTEST_CHECK(attacker.weapons[0].shots_left == 1);
    DOCTEST_CHECK(attacker.ai_range_tactic == 6);

    attacker.weapons[0].count = 0;
    COMBAT1::Init_Ship_For_Start_Of_Turn(0);
    DOCTEST_CHECK(attacker.ai_range_tactic == -1);
}
