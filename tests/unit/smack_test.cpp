#include "pch.h"

#include "doctest/doctest.h"

namespace {
    constexpr uint8_t background_pixel = 7;
    constexpr uint8_t movie_pixel = 42;
    uint8_t off_pixel;
    uint8_t back_pixel;
    uint8_t published_pixel;
    bool movie_palette;
    bool mismatched_frame;
    int restore_count;
    int exit_mode;
    uint32_t frame;
    uint16_t test_fade_percent;
    MOX2::action_func_t auto_function;

    void Check_Published_Frame_() {
        if (!movie_palette && published_pixel == movie_pixel) {
            mismatched_frame = true;
        }
    }
} // namespace

namespace MOX {
    s_settings settings;
    void* global_cache_seg;
    uint32_t global_cache_size;
    uint16_t FULL_DRAW_SCREEN_FLAG;
} // namespace MOX

namespace video {
    void __cdecl Set_Page_Off() {}
    void Set_Refresh_Stencil() {}
    void __cdecl Set_Refresh_Differential() {}
    void __cdecl Clip_On() {}
    void __cdecl Copy_Off_To_Back() {
        back_pixel = off_pixel;
    }
    void __cdecl Copy_Back_To_Off() {
        ++restore_count;
        off_pixel = back_pixel;
    }
    void __cdecl Publish_Off_Page() {
        published_pixel = off_pixel;
        Check_Published_Frame_();
    }
    void __cdecl Submit_Palette() {
        Check_Published_Frame_();
    }
} // namespace video

namespace platform {
    void __cdecl Toggle_Pages_Stub() {
        video::Publish_Off_Page();
    }
} // namespace platform

namespace smckplay {
    int8_t __cdecl Open_Smacker_Animation(const char*, char, uint32_t) {
        return 1;
    }
    int32_t __cdecl Get_Smacker_Animation_Width() {
        return 320;
    }
    int32_t __cdecl Get_Smacker_Animation_Height() {
        return 200;
    }
    uint32_t __cdecl Get_Smacker_Animation_Total_Frames() {
        return 4;
    }
    uint32_t __cdecl Get_Smacker_Animation_Frame() {
        return frame;
    }
    void __cdecl Draw_Smacker_Animation() {
        off_pixel = movie_pixel;
        movie_palette = true;
        ++frame;
    }
    void __cdecl Close_Smacker_Animation() {
        movie_palette = false;
    }
} // namespace smckplay

namespace buffer {
    void __cdecl Free_Buffer_Space() {}
    uint8_t* __cdecl Allocate_Buffer_Space(uint32_t) {
        return nullptr;
    }
} // namespace buffer

namespace sound {
    void __cdecl Fade_Stream_Down(uint8_t) {}
    void __cdecl Stop_Stream() {}
} // namespace sound

namespace fields {
    void __cdecl Deactivate_Auto_Function() {
        auto_function = nullptr;
    }
    void __cdecl Assign_Auto_Function(MOX2::action_func_t fn, int16_t) {
        auto_function = fn;
    }
    void __cdecl Enable_Draw_All_Fields() {}
    void __cdecl Draw_Visible_Fields() {}
    void __cdecl Clear_Fields() {}
    void __cdecl Deactivate_Help_List() {}
} // namespace fields

namespace MOX2 {
    void __cdecl Get_Auto_Function(action_func_t* slot) {
        *slot = auto_function;
    }
    void __cdecl Put_Auto_Function(action_func_t* slot) {
        auto_function = *slot;
    }
} // namespace MOX2

namespace mouse {
    void __cdecl Set_Mouse_List(s_mouse_list*, int16_t) {}
    int16_t __cdecl Mouse_Button() {
        return exit_mode == 2 ? 1 : 0;
    }
} // namespace mouse

namespace key {
    uint8_t Keyboard_Status() {
        return exit_mode == 1 ? 1 : 0;
    }
    int32_t Read_Key() {
        return 27;
    }
} // namespace key

namespace graphics {
    void __cdecl Set_Window(int16_t, int16_t, int16_t, int16_t) {}
    void __cdecl Fill(int16_t, int16_t, int16_t, int16_t, uint8_t color) {
        off_pixel = color;
    }
} // namespace graphics

namespace palette {
    void __cdecl Clear_Palette_Changes(int16_t, int16_t) {}
    void __cdecl Refresh_Palette() {}
} // namespace palette

namespace palstore {
    void __cdecl Darken_Palette(int32_t) {}
} // namespace palstore

namespace ERIC {
    uint16_t __cdecl Get_Fade_Percent() {
        return test_fade_percent;
    }
    void __cdecl Set_Fade_Percent(int16_t percent) {
        test_fade_percent = percent;
    }
    void __cdecl Set_Fade_Speed_Percent(uint8_t) {}
    void Fast_Fade_Out() {
        video::Submit_Palette();
    }
} // namespace ERIC

namespace RUSS {
    void __cdecl Mox_Update() {}
} // namespace RUSS

namespace JIM {
    int16_t jim_current_stream;
    void Play_Background_Music() {}
} // namespace JIM

DOCTEST_TEST_CASE("cinematic cleanup leaves published pixels compatible with the restored palette") {
    for (int mode = 0; mode < 3; ++mode) {
        for (char use_fade = 0; use_fade <= 1; ++use_fade) {
            DOCTEST_CAPTURE(mode);
            DOCTEST_CAPTURE(use_fade);
            off_pixel = background_pixel;
            back_pixel = 0;
            published_pixel = background_pixel;
            movie_palette = false;
            mismatched_frame = false;
            restore_count = 0;
            exit_mode = mode;
            frame = 0;
            test_fade_percent = 0;
            auto_function = nullptr;

            SMACK::Play_Cinematic("test.smk", 0, use_fade);

            DOCTEST_CHECK_FALSE(movie_palette);
            DOCTEST_CHECK(restore_count == (use_fade == 0 ? 1 : 0));
            DOCTEST_CHECK(published_pixel == (use_fade == 0 ? background_pixel : 0));
            if (use_fade == 0) {
                DOCTEST_CHECK(frame == (mode == 0 ? 3 : 1));
            }

            // Science-room entry fades the last published image, not the Off page.
            ERIC::Fast_Fade_Out();
            DOCTEST_CHECK_FALSE(mismatched_frame);
        }
    }
}
