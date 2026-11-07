#include "pch.h"

#include "doctest/doctest.h"
#include "game/eric.h"

namespace test_eric_runtime {
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