#include "pch.h"

#include "doctest/doctest.h"
#include "game/flt1.h"

namespace TEST_FLEET_SUMMARY {
    void Reset_Scroll_Draw_();
    void Reset_Tech_Plunder_();
    void Set_Best_Fuel_(int16_t fuel_id);
    void Set_Random_Percentage_(uint32_t value);
    int16_t Window_Left_();
    int16_t Window_Right_();
    int16_t Line_Count_();
    int16_t Animation_Draw_Count_();
    int16_t Animation_X_();
    uint32_t Candidate_Count_();
    int16_t Grant_Count_();
    int16_t Granted_Tech_(int16_t index);
}

DOCTEST_TEST_CASE("current fleet summary follows the selected stack") {
    MOX::ship_icon_count = 2;
    MOX::ship_stack_count = 2;
    MOX::small_ship_stack_ptr = 1;
    MOX::ship_stack_start[1] = 42;
    MOX::ship_icon[0].node_idx = 7;
    MOX::ship_icon[1].node_idx = 42;
    MOX::scanned_small_ship = 0;
    MOX::galaxy_map_scanned_star = game::id_star{70000};

    FLT1::Restore_Current_Fleet_Summary();

    DOCTEST_CHECK(MOX::scanned_small_ship == 1);
    DOCTEST_CHECK(MOX::galaxy_map_scanned_star == game::NO_STAR);
}

DOCTEST_TEST_CASE("current fleet summary clears invalid stack state") {
    MOX::ship_icon_count = 1;
    MOX::ship_stack_count = 1;
    MOX::small_ship_stack_ptr = -1;
    MOX::scanned_small_ship = 0;
    MOX::galaxy_map_scanned_star = game::id_star{70000};

    FLT1::Restore_Current_Fleet_Summary();

    DOCTEST_CHECK(MOX::scanned_small_ship == -1);
    DOCTEST_CHECK(MOX::galaxy_map_scanned_star == game::NO_STAR);
}

DOCTEST_TEST_CASE("fleet scrollbar uses screen-specific geometry") {
    s_scroll_bar scroll_bar{};
    scroll_bar.thumb_height_px = 10;
    scroll_bar.track_len_px = 121;
    scroll_bar.thumb_center_y = 100;
    MOX::scroll_bar = &scroll_bar;

    MOX::current_screen = SCREEN_OFFICERS;
    TEST_FLEET_SUMMARY::Reset_Scroll_Draw_();
    FLT1::Fill_FltScrn_Scroll_Bar();

    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Window_Left_() == 0x267);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Window_Right_() == 0x26D);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Line_Count_() == 6);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Animation_Draw_Count_() == 0);

    MOX::current_screen = SCREEN_FLEET;
    TEST_FLEET_SUMMARY::Reset_Scroll_Draw_();
    FLT1::Fill_FltScrn_Scroll_Bar();

    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Window_Left_() == 0x25E);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Window_Right_() == 0x26A);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Line_Count_() == 12);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Animation_Draw_Count_() == 1);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Animation_X_() == 0x25D);
}

DOCTEST_TEST_CASE("scrap plunder includes shields and skips absent components") {
    TEST_FLEET_SUMMARY::Reset_Tech_Plunder_();

    s_ship_data ship{};
    ship.d.shield_type = SHIELD_CLASS_I_SHIELD;
    TECHDATA::shields[SHIELD_CLASS_I_SHIELD].tech_app_id = TECH_APP_CLASS_I_SHIELD;

    FLT1::Get_Technology_From_Scrapped_Ship(&ship, 0, 0);

    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Candidate_Count_() == 1);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Grant_Count_() == 1);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Granted_Tech_(0) == TECH_APP_CLASS_I_SHIELD);
}

DOCTEST_TEST_CASE("scrap plunder inspects all eight weapon slots") {
    for (int16_t slot = 0; slot < SCRAP_TECH_WEAPON_SLOT_COUNT; ++slot) {
        TEST_FLEET_SUMMARY::Reset_Tech_Plunder_();

        s_ship_data ship{};
        ship.d.ship_weapon[slot].type = WEAPON_MASS_DRIVER;
        TECHDATA::weapons[WEAPON_MASS_DRIVER].tech_app_id = TECH_APP_MASS_DRIVER;

        FLT1::Get_Technology_From_Scrapped_Ship(&ship, 0, 0);

        DOCTEST_INFO("weapon slot: " << slot);
        DOCTEST_CHECK(TEST_FLEET_SUMMARY::Candidate_Count_() == 1);
        DOCTEST_CHECK(TEST_FLEET_SUMMARY::Grant_Count_() == 1);
        DOCTEST_CHECK(TEST_FLEET_SUMMARY::Granted_Tech_(0) == TECH_APP_MASS_DRIVER);
    }
}

DOCTEST_TEST_CASE("scrap plunder grants duplicate applications once") {
    TEST_FLEET_SUMMARY::Reset_Tech_Plunder_();
    TEST_FLEET_SUMMARY::Set_Random_Percentage_(1);

    s_ship_data ship{};
    ship.d.ship_weapon[0].type = WEAPON_MASS_DRIVER;
    ship.d.ship_weapon[1].type = WEAPON_MASS_DRIVER;
    TECHDATA::weapons[WEAPON_MASS_DRIVER].tech_app_id = TECH_APP_MASS_DRIVER;

    FLT1::Get_Technology_From_Scrapped_Ship(&ship, 0, 0);

    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Candidate_Count_() == 2);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Grant_Count_() == 1);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Granted_Tech_(0) == TECH_APP_MASS_DRIVER);
}

DOCTEST_TEST_CASE("scrap plunder stores every accepted candidate safely") {
    TEST_FLEET_SUMMARY::Reset_Tech_Plunder_();
    TEST_FLEET_SUMMARY::Set_Best_Fuel_(FUEL_STANDARD_FUEL_CELLS);
    TEST_FLEET_SUMMARY::Set_Random_Percentage_(1);

    s_ship_data ship{};
    TECHDATA::fuel[FUEL_STANDARD_FUEL_CELLS].tech_app_id = 1;

    ship.d.shield_type = SHIELD_CLASS_I_SHIELD;
    TECHDATA::shields[SHIELD_CLASS_I_SHIELD].tech_app_id = 2;
    ship.d.ftl_type = DRIVE_NUCLEAR_DRIVE;
    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE].tech_app_id = 3;
    ship.d.computer_type = COMPUTER_ELECTRONIC_COMPUTER;
    TECHDATA::computers[COMPUTER_ELECTRONIC_COMPUTER].tech_app_id = 4;
    ship.d.armor_type = ARMOR_TITANIUM_ARMOR;
    TECHDATA::armor[ARMOR_TITANIUM_ARMOR].tech_app_id = 5;

    for (int16_t slot = 0; slot < SCRAP_TECH_WEAPON_SLOT_COUNT; ++slot) {
        int16_t weapon_type = slot + 1;
        ship.d.ship_weapon[slot].type = weapon_type;
        TECHDATA::weapons[weapon_type].tech_app_id = slot + 6;
    }

    for (int16_t special = 1; special < SPECIAL_REGENERATION; ++special) {
        ship.d.special_device_flags[special / 8] |= (int8_t)(1 << (special % 8));
        TECHDATA::specials[special].tech_app_id = special + 13;
    }

    FLT1::Get_Technology_From_Scrapped_Ship(&ship, 0, 0);

    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Candidate_Count_() == SCRAP_TECH_CANDIDATE_CAPACITY);
    DOCTEST_CHECK(TEST_FLEET_SUMMARY::Grant_Count_() == SCRAP_TECH_ACQUIRED_CAPACITY);
}