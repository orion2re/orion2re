#include "pch.h"

#include <string>
#include <vector>

#include "config/config_loader.h"
#include "doctest/doctest.h"
#include "game/mainscr.h"
#include "fakes/fake_main_screen_hud_runtime.h"

namespace {
    static_assert(std::size(MAINSCR::mouse_list_relocation) == 2);

    std::vector<std::string> registered_hotkeys;
    int reload_count;
    int full_reload_count;
    bool reload_result;
    bool observed_network_game;
    std::string user_box_message;
} // namespace

namespace MOX {
    uint8_t draw_main_screen_to_back;
    int8_t game_type;
    int16_t cur_map_scale;
    int16_t cur_map_x;
    int16_t cur_map_y;
    int16_t MAP_MAX_X;
    int16_t MAP_MAX_Y;
    int16_t near_star_x;
    int16_t near_star_y;
    int16_t mid_star_x;
    int16_t mid_star_y;
    int16_t far_star_x;
    int16_t far_star_y;
} // namespace MOX

namespace fields {
    int16_t fields_count;

    int16_t __cdecl Add_Multi_Hot_Key_Field(const char* hotkey) {
        registered_hotkeys.emplace_back(hotkey);
        return fields_count++;
    }
} // namespace fields

namespace config {
    bool Reload_All_Active_Config_Groups(s_cfg_diagnostic* diagnostic) {
        full_reload_count++;
        if (!reload_result) {
            diagnostic->message = "reload failed";
        }
        return reload_result;
    }

    bool Reload_Active_Config_Groups(bool network_game,
        s_cfg_diagnostic* diagnostic,
        t_config_group_merger,
        t_config_group_reload_policy) {
        reload_count++;
        observed_network_game = network_game;
        if (!reload_result) {
            diagnostic->message = "reload failed";
        }
        return reload_result;
    }
} // namespace config

namespace HAROLD {
    int32_t __cdecl Get_Up_Scaled_Value(int16_t value) {
        return (static_cast<int32_t>(value) * MOX::cur_map_scale) / 10;
    }

    int16_t __cdecl User_Box(uint8_t*, const char* message, int16_t) {
        user_box_message = message;
        return 0;
    }
} // namespace HAROLD

DOCTEST_TEST_CASE("main-screen centering clamps each axis without changing zoom") {
    struct s_center_case {
        int16_t width;
        int16_t height;
        int16_t scale;
        int16_t target_x;
        int16_t target_y;
        int16_t expected_x;
        int16_t expected_y;
    };
    const s_center_case cases[] = {
        {1800, 1350, 36, 150, 150, 0, 0},
        {1800, 1350, 36, 1650, 1200, 0, 0},
        {1518, 1200, 30, 900, 675, 0, 0},
        {1800, 1350, 30, 900, 675, 141, 75},
        {1800, 1350, 30, 150, 150, 0, 0},
        {1800, 1350, 30, 1650, 1200, 282, 150},
        {1500, 1350, 30, 900, 675, 0, 75},
        {1800, 1100, 30, 900, 675, 141, 0},
    };
    for (const auto& test_case : cases) {
        DOCTEST_CAPTURE(test_case.width);
        DOCTEST_CAPTURE(test_case.height);
        DOCTEST_CAPTURE(test_case.scale);
        MOX::MAP_MAX_X = test_case.width;
        MOX::MAP_MAX_Y = test_case.height;
        MOX::cur_map_scale = test_case.scale;
        MOX::cur_map_x = 0;
        MOX::cur_map_y = 0;
        MOX::draw_main_screen_to_back = 0;
        MAINSCR::Center_Map(test_case.target_x, test_case.target_y);
        DOCTEST_CHECK(MOX::cur_map_x == test_case.expected_x);
        DOCTEST_CHECK(MOX::cur_map_y == test_case.expected_y);
        DOCTEST_CHECK(MOX::cur_map_scale == test_case.scale);
        DOCTEST_CHECK(MOX::draw_main_screen_to_back == 1);
    }
}

DOCTEST_TEST_CASE("main-screen tax percentage matches research font and line spacing") {
    const bool saved_setting = MAINSCR::main_screen_tax_rate;
    MAINSCR::main_screen_tax_rate = true;
    for (uint8_t rate = 10; rate <= 50; rate += 10) {
        TEST_MAIN_SCREEN_HUD::Reset();
        MOX::player[0].tax_rate = rate;
        MAINSCR::Print_Main_Screen_Data();
        const auto& draws = TEST_MAIN_SCREEN_HUD::draws;
        DOCTEST_REQUIRE(draws.size() >= 4);
        const auto& tax = draws[1];
        const auto& balance = draws[2];
        const auto& income = draws[3];
        DOCTEST_CHECK(tax.text == std::to_string(rate) + "%");
        DOCTEST_CHECK(balance.text == "500 BC");
        DOCTEST_CHECK(income.text == "+120 BC");
        DOCTEST_CHECK(tax.x == 579);
        DOCTEST_CHECK(balance.x == tax.x);
        DOCTEST_CHECK(income.x == tax.x);
        DOCTEST_CHECK(tax.y == 84);
        DOCTEST_CHECK(balance.y == 96);
        DOCTEST_CHECK(income.y == 108);
        DOCTEST_CHECK(balance.y - tax.y == income.y - balance.y);
        for (const auto* draw : {&tax, &balance, &income}) {
            DOCTEST_CHECK(draw->style == 2);
            DOCTEST_CHECK(draw->spacing == 2);
            DOCTEST_CHECK(draw->color == 14);
        }
        bool found_research_percentage = false;
        for (const auto& draw : draws) {
            if (draw.y == 379 && draw.text == "20%") {
                found_research_percentage = true;
                DOCTEST_CHECK(draw.x == tax.x);
                DOCTEST_CHECK(draw.style == tax.style);
                DOCTEST_CHECK(draw.spacing == tax.spacing);
                DOCTEST_CHECK(draw.color == tax.color);
            }
        }
        DOCTEST_CHECK(found_research_percentage);
        for (const auto& fit : TEST_MAIN_SCREEN_HUD::fits) {
            if (fit.text == tax.text || fit.text == balance.text || fit.text == income.text) {
                DOCTEST_CHECK(fit.initial_style == 2);
                DOCTEST_CHECK(fit.initial_spacing == 2);
                DOCTEST_CHECK(fit.variant == 4);
                DOCTEST_CHECK(fit.max_width == 67);
                DOCTEST_CHECK(fit.min_style == 1);
            }
        }
    }
    MAINSCR::main_screen_tax_rate = saved_setting;
}

DOCTEST_TEST_CASE("main-screen hides tax when disabled or zero and preserves negative colors") {
    const bool saved_setting = MAINSCR::main_screen_tax_rate;
    for (bool enabled : {false, true}) {
        for (uint8_t rate : {0, 20}) {
            TEST_MAIN_SCREEN_HUD::Reset();
            MAINSCR::main_screen_tax_rate = enabled;
            MOX::player[0].tax_rate = rate;
            MOX::player[0].bc = -500;
            MOX::player[0].surplus_bc = -120;
            MOX::settings.language = 1;
            MAINSCR::Print_Main_Screen_Data();
            const auto& draws = TEST_MAIN_SCREEN_HUD::draws;
            const bool shows_tax = enabled && rate != 0;
            DOCTEST_REQUIRE(draws.size() >= 4);
            if (shows_tax) {
                DOCTEST_CHECK(draws[1].text == "20%");
                DOCTEST_CHECK(draws[1].color == 14);
            }
            const auto& balance = draws[shows_tax ? 2 : 1];
            const auto& income = draws[shows_tax ? 3 : 2];
            DOCTEST_CHECK(balance.text == "-500 MC");
            DOCTEST_CHECK(income.text == "-120 MC");
            DOCTEST_CHECK(balance.y == 96);
            DOCTEST_CHECK(income.y == 108);
            DOCTEST_CHECK(balance.color == 75);
            DOCTEST_CHECK(income.color == 75);
        }
    }
    MAINSCR::main_screen_tax_rate = saved_setting;
}

DOCTEST_TEST_CASE("main-screen relocation mode uses the restored destination cursor") {
    const s_mouse_list& relocation = MAINSCR::mouse_list_relocation[0];
    DOCTEST_CHECK(relocation.icon == 2);
    DOCTEST_CHECK(relocation.x1 == 0);
    DOCTEST_CHECK(relocation.y1 == 0);
    DOCTEST_CHECK(relocation.x2 == 639);
    DOCTEST_CHECK(relocation.y2 == 479);

    const s_mouse_list& destination = MAINSCR::mouse_list_relocation[1];
    DOCTEST_CHECK(destination.icon == 18);
    DOCTEST_CHECK(destination.x1 == 22);
    DOCTEST_CHECK(destination.y1 == 22);
    DOCTEST_CHECK(destination.x2 == 526);
    DOCTEST_CHECK(destination.y2 == 420);
}

DOCTEST_TEST_CASE("main-screen RI registration reloads without consuming R") {
    registered_hotkeys.clear();
    reload_count = 0;
    full_reload_count = 0;
    reload_result = true;
    observed_network_game = false;
    user_box_message.clear();
    fields::fields_count = 100;
    MOX::game_type = 0;
    MOX::draw_main_screen_to_back = 0;

    MAINSCR::Add_Cheats();

    DOCTEST_CHECK(registered_hotkeys == std::vector<std::string>{
                                            "EINSTEIN",
                                            "MOOLA",
                                            "MENLO",
                                            "ISEEALL",
                                            "RE",
                                            "RI",
                                            "X",
                                            "ALE",
                                            "ALLAI",
                                            "SCOR",
                                        });
    DOCTEST_CHECK_FALSE(MAINSCR::Check_Config_Reload_Hot_Key(106));
    DOCTEST_CHECK(reload_count == 0);
    DOCTEST_CHECK(MAINSCR::Check_Config_Reload_Hot_Key(105));
    DOCTEST_CHECK(reload_count == 1);
    DOCTEST_CHECK(full_reload_count == 0);
    DOCTEST_CHECK_FALSE(observed_network_game);
    DOCTEST_CHECK(MOX::draw_main_screen_to_back == 1);

    MOX::draw_main_screen_to_back = 0;
    DOCTEST_CHECK(MAINSCR::Check_Config_Reload_Hot_Key(104));
    DOCTEST_CHECK(full_reload_count == 1);
    DOCTEST_CHECK(MOX::draw_main_screen_to_back == 1);
}

DOCTEST_TEST_CASE("main-screen RI reports reload failure once") {
    registered_hotkeys.clear();
    reload_count = 0;
    full_reload_count = 0;
    reload_result = false;
    user_box_message.clear();
    fields::fields_count = 200;
    MOX::game_type = 3;
    MOX::draw_main_screen_to_back = 0;
    MAINSCR::Add_Cheats();

    DOCTEST_CHECK(registered_hotkeys == std::vector<std::string>{
                                            "EINSTEIN",
                                            "MOOLA",
                                            "MENLO",
                                            "ISEEALL",
                                            "RE",
                                            "RI",
                                            "ALE",
                                            "ALLAI",
                                            "SCOR",
                                        });
    DOCTEST_CHECK(MAINSCR::Check_Config_Reload_Hot_Key(205));
    DOCTEST_CHECK(reload_count == 1);
    DOCTEST_CHECK(observed_network_game);
    DOCTEST_CHECK(user_box_message == "reload failed");
    DOCTEST_CHECK(MOX::draw_main_screen_to_back == 0);

    reload_result = true;
    user_box_message.clear();
    DOCTEST_CHECK(MAINSCR::Check_Config_Reload_Hot_Key(204));
    DOCTEST_CHECK(full_reload_count == 0);
    DOCTEST_CHECK(user_box_message ==
        "You may not reload config in a multiplayer game.");
}