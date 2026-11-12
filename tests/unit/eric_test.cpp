#include "pch.h"

#include "doctest/doctest.h"
#include "game/eric.h"
#include <stdexcept>

namespace test_eric_runtime {
    void Reset_Utilities_();
    extern int colony_init_order, events_init_order;
    extern int draw_count;
    extern int16_t draw_x, draw_y;
    extern bool draw_no_glass;
    extern int16_t paragraph_x, paragraph_y, paragraph_width, paragraph_alignment;
    extern int16_t measured_width;
    extern std::string paragraph_text;
    extern int16_t box_args[6], cycle_args[8];
    extern int box_order, cycle_order;
    extern int16_t refresh_mode;
    extern int resource_load_count;
    extern bool block_pushed;
    extern int16_t assignment_delay;
    extern MOX2::action_func_t auto_function;
    extern int movie_count;
    extern std::string movie_name;
    extern uint32_t movie_flags;
    extern char movie_fade;
    void Reset_();
    s_leader_data* Defender_Leader_();
    int16_t Last_Help_String_();
    void Reset_Fade_();
    int16_t Fade_Level_Count_();
    int32_t Fade_Level_(int16_t index);
    void Reset_Cinematic_();
    bool Cinematic_Dismissed_();
    bool Popup_Visible_During_Fade_();
}

DOCTEST_TEST_CASE("Eric initialization delegates colony initialization before events") {
    test_eric_runtime::Reset_Utilities_();
    ERIC::Eric_Init();
    DOCTEST_CHECK(test_eric_runtime::colony_init_order == 1);
    DOCTEST_CHECK(test_eric_runtime::events_init_order == 2);
}

DOCTEST_TEST_CASE("centered drawing uses real animation dimensions") {
    test_eric_runtime::Reset_Utilities_();
    s_animation_header picture = {};
    picture.width = 31;
    picture.height = 18;
    ERIC::Draw_Centered(100, 80, &picture);
    DOCTEST_CHECK(test_eric_runtime::draw_x == 85);
    DOCTEST_CHECK(test_eric_runtime::draw_y == 71);
    ERIC::Draw_Bottom_Centered(100, 80, &picture);
    DOCTEST_CHECK(test_eric_runtime::draw_y == 62);
    DOCTEST_CHECK_FALSE(test_eric_runtime::draw_no_glass);
    ERIC::Draw_Bottom_Centered_No_Glass(100, 80, &picture);
    DOCTEST_CHECK(test_eric_runtime::draw_x == 85);
    DOCTEST_CHECK(test_eric_runtime::draw_y == 62);
    DOCTEST_CHECK(test_eric_runtime::draw_no_glass);
    DOCTEST_CHECK(test_eric_runtime::draw_count == 3);
}

DOCTEST_TEST_CASE("paragraph helpers center the measured text and forward alignment") {
    test_eric_runtime::Reset_Utilities_();
    char text[] = "Paragraph";
    ERIC::Print_Paragraph_Centered(10, 30, 100, 50, text);
    DOCTEST_CHECK(test_eric_runtime::measured_width == 100);
    DOCTEST_CHECK(test_eric_runtime::paragraph_x == 10);
    DOCTEST_CHECK(test_eric_runtime::paragraph_y == 45);
    DOCTEST_CHECK(test_eric_runtime::paragraph_width == 100);
    DOCTEST_CHECK(test_eric_runtime::paragraph_alignment == 2);
    DOCTEST_CHECK(test_eric_runtime::paragraph_text == text);
    ERIC::Print_Paragraph_Centered_Vertically(10, 30, 100, 50, text, 3);
    DOCTEST_CHECK(test_eric_runtime::paragraph_y == 45);
    DOCTEST_CHECK(test_eric_runtime::paragraph_alignment == 3);
}

DOCTEST_TEST_CASE("cinematic setup installs its frame callback and restores the prior runtime") {
    test_eric_runtime::Reset_Cinematic_();
    auto saved_auto = test_eric_runtime::auto_function;
    MOX::settings.sound_fx_on = 1;
    MOX::settings.music_on = 1;
    MOX::settings.music_level = 20;
    ERIC::Do_Cinematic_Popup_Setup();
    DOCTEST_CHECK(test_eric_runtime::resource_load_count == 3);
    DOCTEST_CHECK(test_eric_runtime::draw_x == 307);
    DOCTEST_CHECK(test_eric_runtime::draw_y == 232);
    DOCTEST_CHECK(test_eric_runtime::block_pushed);
    DOCTEST_CHECK(test_eric_runtime::assignment_delay == 1);
    DOCTEST_CHECK(test_eric_runtime::auto_function == ERIC::Cinematic_Autofunction);
    DOCTEST_CHECK(MOX::settings.music_level == 55);
    ERIC::Do_Cinematic_Popup_Cleanup();
    DOCTEST_CHECK(test_eric_runtime::auto_function == saved_auto);
    DOCTEST_CHECK(MOX::settings.music_level == 20);
    DOCTEST_CHECK(test_eric_runtime::Cinematic_Dismissed_());
}

DOCTEST_TEST_CASE("portal cinematics use the expected asset and clean up the popup") {
    test_eric_runtime::Reset_Cinematic_();
    ERIC::Play_Dimensional_Portal_Flic();
    DOCTEST_CHECK(test_eric_runtime::movie_count == 1);
    DOCTEST_CHECK(test_eric_runtime::movie_name == "DIMTVFIN.LBX");
    DOCTEST_CHECK(test_eric_runtime::movie_flags == 0);
    DOCTEST_CHECK(test_eric_runtime::movie_fade == 0);
    DOCTEST_CHECK(test_eric_runtime::Cinematic_Dismissed_());
}

DOCTEST_TEST_CASE("pulsing boxes configure their palette after drawing the border") {
    test_eric_runtime::Reset_Utilities_();
    ERIC::Pulsing_Box(1, 2, 3, 4, 5, 0, 9);
    const int16_t box[] = {1, 2, 3, 4, 5, 255};
    const int16_t cycle[] = {255, 19, 16, 9, 32, 40, 21, 9};
    for (int i = 0; i < 6; ++i) DOCTEST_CHECK(test_eric_runtime::box_args[i] == box[i]);
    for (int i = 0; i < 8; ++i) DOCTEST_CHECK(test_eric_runtime::cycle_args[i] == cycle[i]);
    DOCTEST_CHECK(test_eric_runtime::box_order < test_eric_runtime::cycle_order);
    for (int16_t mode : {0, 2, 3}) {
        ERIC::Set_Refresh_Mode(mode);
        DOCTEST_CHECK(test_eric_runtime::refresh_mode == mode);
    }
}

DOCTEST_TEST_CASE("string building appends lines and reports oversized formatting") {
    char buffer[64] = {};
    ERIC::String_Builder2(buffer, sizeof(buffer), 0, "%s", "First");
    ERIC::String_Builder2(buffer, sizeof(buffer), 1, "%s", "Second");
    DOCTEST_CHECK(std::string(buffer) == "First\rSecond");
    const std::string oversized(1000, 'x');
    DOCTEST_CHECK_THROWS_AS(ERIC::String_Builder2(buffer, sizeof(buffer), 0, "%s", oversized.c_str()), std::runtime_error);
    DOCTEST_CHECK(test_eric_runtime::Last_Help_String_() == 408);
}

DOCTEST_TEST_CASE("colony choice prefers the capitol and handles an empty owner set") {
    test_eric_runtime::Reset_();
    const uint32_t saved_seed = game_random::Get_Random_Seed();
    MOX::NUM_COLONIES = 4;
    for (int i = 0; i < 4; ++i) MOX::colony[i].owner = 1;
    MOX::colony[1].owner = 0;
    MOX::colony[3].owner = 0;
    game_random::Set_Random_Seed(650);
    DOCTEST_CHECK(ERIC::Pick_Random_Colony(0) == 3);
    MOX::colony[1].buildings[BUILDING_CAPITOL] = 1;
    DOCTEST_CHECK(ERIC::Player_Capitol_World(0) == 1);
    DOCTEST_CHECK(ERIC::Pick_Random_Colony(2) == -1);
    game_random::Set_Random_Seed(saved_seed);
    test_eric_runtime::Reset_();
}

DOCTEST_TEST_CASE("Cinematic cleanup dismisses the frame before returning to the map or fading") {
    test_eric_runtime::Reset_Cinematic_();

    ERIC::Do_Cinematic_Popup_Cleanup();

    DOCTEST_CHECK(test_eric_runtime::Cinematic_Dismissed_());
    DOCTEST_CHECK(ERIC::Get_Fade_Percent() == 0);
    DOCTEST_CHECK(test_eric_runtime::Fade_Level_Count_() == 0);
    DOCTEST_CHECK(MOX::settings.sound_fx_level == 35);
    DOCTEST_CHECK(MOX::settings.music_level == 20);

    // Science-room entry fades the published image, not the writable Off page.
    ERIC::Fast_Fade_Out();

    DOCTEST_CHECK(test_eric_runtime::Fade_Level_Count_() == 5);
    DOCTEST_CHECK_FALSE(test_eric_runtime::Popup_Visible_During_Fade_());
}

DOCTEST_TEST_CASE("Temporary fade-out speed does not change configured fade-in steps") {
    test_eric_runtime::Reset_Fade_();
    ERIC::Set_Fade_Speed_Percent(10);

    DOCTEST_CHECK(ERIC::fade_speed.fade_in == 25);
    DOCTEST_CHECK(ERIC::fade_speed.fade_out == 10);

    ERIC::Fast_Fade_In();

    const int32_t expected[] = {100, 75, 50, 25, 0};
    DOCTEST_REQUIRE(test_eric_runtime::Fade_Level_Count_() == 5);
    for (int16_t index = 0; index < 5; ++index) {
        DOCTEST_CHECK(test_eric_runtime::Fade_Level_(index) == expected[index]);
    }
}

DOCTEST_TEST_CASE("Telepath leaders protect colonies from mind control") {
    const uint32_t telepath_skills[] = {
        LEADER_GENERAL_SKILL_TELEPATH,
        LEADER_GENERAL_SKILL_TELEPATH2,
    };

    for (uint32_t skill : telepath_skills) {
        test_eric_runtime::Reset_();
        test_eric_runtime::Defender_Leader_()->general_skills = skill;

        DOCTEST_CHECK(ERIC::Player_Can_Mind_Control_Colony(0, 0) == 0);
        DOCTEST_CHECK(ERIC::Player_Can_Mind_Control_Colony_With_Help(0, 0) == 0);
        DOCTEST_CHECK(test_eric_runtime::Last_Help_String_() == 0x200);
    }
}

DOCTEST_TEST_CASE("Assassin leaders do not protect colonies from mind control") {
    test_eric_runtime::Reset_();
    test_eric_runtime::Defender_Leader_()->general_skills =
        LEADER_GENERAL_SKILL_ASSASSIN;

    DOCTEST_CHECK(ERIC::Player_Can_Mind_Control_Colony(0, 0) == 1);
    DOCTEST_CHECK(ERIC::Player_Can_Mind_Control_Colony_With_Help(0, 0) == 1);
    DOCTEST_CHECK(test_eric_runtime::Last_Help_String_() == -1);
}