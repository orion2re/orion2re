#include "pch.h"

#include "doctest/doctest.h"

namespace {
    struct s_ship_stats_fixture {
        s_player player{};
        s_ship_data ship{};
        s_player* saved_player = MOX::player;
        s_ship_data* saved_ship = MOX::ship;
        s_combat_data saved_display = MOX::global_combat_data;
        uint8_t saved_strategic_combat = MOX::settings.strategic_combat_flag;
        s_ratio16 saved_multiplier = SHIPMOVE::phase_shifter_speed_multiplier;

        s_ship_stats_fixture() {
            MOX::player = &player;
            MOX::ship = &ship;
            MOX::settings.strategic_combat_flag = 0;
            SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
            ship.d.ftl_type = DRIVE_NUCLEAR_DRIVE;
        }

        ~s_ship_stats_fixture() {
            MOX::player = saved_player;
            MOX::ship = saved_ship;
            MOX::global_combat_data = saved_display;
            MOX::settings.strategic_combat_flag = saved_strategic_combat;
            SHIPMOVE::phase_shifter_speed_multiplier = saved_multiplier;
        }
    };
}

DOCTEST_TEST_CASE("Ship details show nuclear drive speed on support ships") {
    s_ship_stats_fixture fixture;
    for (const auto type : {SHIP_TYPE_COLONY, SHIP_TYPE_OUTPOST, SHIP_TYPE_TRANSPORT}) {
        fixture.ship.d.ship_type = type;
        COMBINIT::Load_Display_Combat_Ship(0);
        DOCTEST_CHECK(CMBTDRW1::Ship_View_FTL_Speed(MOX::global_combat_data) == 2);
        DOCTEST_CHECK(static_cast<int>(MOX::global_combat_data.current_speed) == 0);
    }
}

DOCTEST_TEST_CASE("Ship details reflect the Phase Shifter strategic speed ratio") {
    s_ship_stats_fixture fixture;
    fixture.ship.d.ship_type = SHIP_TYPE_COMBAT;
    SHIPMOVE::phase_shifter_speed_multiplier = {2, 1};
    COMBINIT::Load_Display_Combat_Ship(0);
    auto& display = MOX::global_combat_data;
    DOCTEST_CHECK(CMBTDRW1::Ship_View_FTL_Speed(display) == 2);

    struct_::Set_Bit_Field(fixture.ship.d.special_device_flags, SPECIAL_PHASE_SHIFTER);
    COMBINIT::Load_Display_Combat_Ship(0);
    DOCTEST_CHECK(CMBTDRW1::Ship_View_FTL_Speed(display) == 4);

    fixture.player.traits[TRAIT_TRANS_DIMENSIONAL] = 1;
    DOCTEST_CHECK(CMBTDRW1::Ship_View_FTL_Speed(display) == 2 * (2 + INITSHIP::td_ftl_bonus));
    fixture.player.traits[TRAIT_TRANS_DIMENSIONAL] = 0;

    SHIPMOVE::phase_shifter_speed_multiplier = {1, 2};
    DOCTEST_CHECK(CMBTDRW1::Ship_View_FTL_Speed(display) == 1);

    display.ftl_type = DRIVE_NO_DRIVE;
    DOCTEST_CHECK(CMBTDRW1::Ship_View_FTL_Speed(display) == 0);
    display.ftl_type = DRIVE_NUCLEAR_DRIVE;
    display.ship_idx = -1;
    DOCTEST_CHECK(CMBTDRW1::Ship_View_FTL_Speed(display) == 0);
}
