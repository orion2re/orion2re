#include "pch.h"

#include "doctest/doctest.h"
#include "game/smckplay.h"

namespace test::fakes {
    extern int playback_stream_open_count;
    extern int32_t last_playback_stream_volume;

    void Reset_Smacker_Audio_Fake_();
}

namespace {
    struct s_video_fixture {
        uint8_t pixels[32] = {};
        video::s_scan_list_entry rows[4] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
        video::s_page_buffer saved_page = video::g_page_off;
        video::s_scan_list_entry* saved_rows = video::refresh_scan_list;
        int32_t saved_width = video::g_screen_res_x;
        int32_t saved_height = video::g_screen_res_y;
        int32_t saved_refresh = video::screen_refresh_flag;
        int16_t saved_x = SMACK::smack_x;
        int16_t saved_y = SMACK::smack_y;
        decltype(video::current_palette) saved_palette;

        s_video_fixture() {
            memcpy(saved_palette, video::current_palette, sizeof(saved_palette));
            memset(video::current_palette, 0, sizeof(video::current_palette));
            video::g_page_off.pixels = pixels;
            video::refresh_scan_list = rows;
            video::g_screen_res_x = 8;
            video::g_screen_res_y = 4;
            video::screen_refresh_flag = 2;
            SMACK::smack_x = 2;
            SMACK::smack_y = 1;
        }

        ~s_video_fixture() {
            smckplay::Close_Smacker_Animation();
            video::g_page_off = saved_page;
            video::refresh_scan_list = saved_rows;
            video::g_screen_res_x = saved_width;
            video::g_screen_res_y = saved_height;
            video::screen_refresh_flag = saved_refresh;
            SMACK::smack_x = saved_x;
            SMACK::smack_y = saved_y;
            memcpy(video::current_palette, saved_palette, sizeof(saved_palette));
        }
    };
}

DOCTEST_TEST_CASE("Smacker frames preserve palette indices and dirty the destination") {
    test::fakes::Reset_Smacker_Audio_Fake_();
    s_video_fixture fixture;
    video::current_palette[9].r = 77;
    video::current_palette[246].r = 88;
    DOCTEST_REQUIRE(smckplay::Open_Smacker_Animation("frames.smk", 0, 0) == 1);
    DOCTEST_CHECK(smckplay::Get_Smacker_Animation_Total_Frames() == 2);

    smckplay::Draw_Smacker_Animation();
    DOCTEST_CHECK(fixture.pixels[10] == 40);
    DOCTEST_CHECK(fixture.pixels[9] == 0);
    DOCTEST_CHECK(fixture.pixels[11] == 0);
    DOCTEST_CHECK(fixture.rows[1].min_x == 0);
    DOCTEST_CHECK(fixture.rows[1].max_x == 0);
    DOCTEST_CHECK(video::current_palette[10].r == 30);
    DOCTEST_CHECK(video::current_palette[10].g == 20);
    DOCTEST_CHECK(video::current_palette[10].b == 10);
    DOCTEST_CHECK(video::current_palette[9].r == 77);
    DOCTEST_CHECK(video::current_palette[246].r == 88);
    DOCTEST_CHECK(smckplay::Get_Smacker_Animation_Frame() == 1);

    smckplay::Draw_Smacker_Animation();
    DOCTEST_CHECK(fixture.pixels[10] == 41);
    DOCTEST_CHECK(smckplay::Get_Smacker_Animation_Frame() == 1);
}

DOCTEST_TEST_CASE("embedded Smacker audio follows the enabled channel volume") {
    struct s_audio_policy_case {
        uint8_t music_on;
        int8_t music_level;
        uint8_t sound_fx_on;
        int8_t sound_fx_level;
        int expected_open_count;
        int32_t expected_volume;
    };

    const s_audio_policy_case cases[] = {
        {0, 25, 0, 75, 0, 0},
        {0, 25, 1, 75, 1, 75},
        {1, 25, 0, 75, 1, 25},
        {1, 25, 1, 75, 1, 25},
    };

    for (const s_audio_policy_case& test_case : cases) {
        test::fakes::Reset_Smacker_Audio_Fake_();
        MOX::settings.music_on = test_case.music_on;
        MOX::settings.music_level = test_case.music_level;
        MOX::settings.sound_fx_on = test_case.sound_fx_on;
        MOX::settings.sound_fx_level = test_case.sound_fx_level;

        const char audio_enabled =
            (test_case.music_on != 0 || test_case.sound_fx_on != 0) ? 1 : 0;

        DOCTEST_REQUIRE(smckplay::Open_Smacker_Animation("policy.smk", audio_enabled, 0) == 1);
        DOCTEST_CHECK(test::fakes::playback_stream_open_count == test_case.expected_open_count);
        if (test_case.expected_open_count != 0) {
            DOCTEST_CHECK(test::fakes::last_playback_stream_volume == test_case.expected_volume);
        }

        smckplay::Close_Smacker_Animation();
    }
}