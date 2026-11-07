#include "pch.h"

namespace SMACK {
    s_mouse_list smack_mouse = {0, 0, 0, 0, 639, 479};
    MOX2::action_func_t smack_auto_function;
    int16_t smack_x;
    int16_t smack_y;

    void __cdecl Smack_Fade(int16_t fade_mode) {
        int16_t fade_step;
        int16_t palette_level;
        int16_t fade_percent;

        if (fade_mode == 3) {
            fade_step = -10;
            fade_percent = ERIC::Get_Fade_Percent();
            palette_level = 100;
            if (fade_percent == 0) {
                return;
            }
        } else {
            palette_level = 0;
            fade_percent = ERIC::Get_Fade_Percent();
            fade_step = 10;
            if (fade_percent == 100) {
                return;
            }
        }

        do {
            if (palette_level >= 0 && palette_level < 100) {
                palstore::Darken_Palette((int)palette_level);
                video::Submit_Palette();
                RUSS::Mox_Update();
            }
            palette_level = (int16_t)(palette_level + fade_step);
        } while (palette_level != 110 && palette_level != -10);

        palette::Clear_Palette_Changes(0, 0xff);
    }

    void __cdecl Play_Cinematic(const char* smacker_name, uint32_t play_flags, char use_fade) {
        int16_t exit_requested = 0;
        int32_t animation_width;
        int32_t animation_height;
        int16_t total_frames;
        int16_t current_frame;

        buffer::Free_Buffer_Space();
        video::Set_Page_Off();
        sound::Fade_Stream_Down(0x19);

        if (use_fade == 1) {
            SMACK::Smack_Fade(4);
            mouse::Set_Mouse_List(&SMACK::smack_mouse, 1);
        } else {
            MOX2::Get_Auto_Function(&SMACK::smack_auto_function);
            fields::Deactivate_Auto_Function();
            video::Set_Refresh_Stencil();
            fields::Enable_Draw_All_Fields();
            fields::Draw_Visible_Fields();
            video::Copy_Off_To_Back();
        }

        video::Set_Refresh_Differential();
        fields::Assign_Auto_Function(smckplay::Draw_Smacker_Animation, 0);
        fields::Clear_Fields();
        fields::Deactivate_Help_List();
        graphics::Set_Window(0, 0, 0x27f, 0x1df);
        video::Clip_On();
        sound::Stop_Stream();

        const char audio_enabled =
            (MOX::settings.music_on != 0 || MOX::settings.sound_fx_on != 0) ? 1 : 0;
        smckplay::Open_Smacker_Animation(smacker_name, audio_enabled, play_flags);

        animation_width = smckplay::Get_Smacker_Animation_Width();
        SMACK::smack_x = (int16_t)((0x280 - animation_width) / 2);

        animation_height = smckplay::Get_Smacker_Animation_Height();
        if ((play_flags & 0x100000U) == 0) {
            SMACK::smack_y = (int16_t)((0x1e0 - animation_height) / 2);
        } else {
            SMACK::smack_y = (int16_t)((0x1e0 - animation_height) / 4);
        }

        if (use_fade == 1) {
            video::Set_Page_Off();
            graphics::Fill(0, 0, 0x27f, 0x1df, 0);
            platform::Toggle_Pages_Stub();
            graphics::Fill(0, 0, 0x27f, 0x1df, 0);
            smckplay::Draw_Smacker_Animation();
            palette::Refresh_Palette();
            palstore::Darken_Palette(100);
            platform::Toggle_Pages_Stub();
            ERIC::Set_Fade_Percent(100);
            SMACK::Smack_Fade(3);
        }

        total_frames = smckplay::Get_Smacker_Animation_Total_Frames();

        do {
            if (key::Keyboard_Status() == 1) {
                exit_requested = 1;
                key::Read_Key();
            }

            if (mouse::Mouse_Button() != 0) {
                exit_requested = 1;
            }

            video::Set_Page_Off();
            smckplay::Draw_Smacker_Animation();
            platform::Toggle_Pages_Stub();
            RUSS::Mox_Update();

            current_frame = smckplay::Get_Smacker_Animation_Frame();
            if (total_frames - current_frame == 1) {
                exit_requested = 1;
            }
        } while (exit_requested == 0);

        fields::Deactivate_Auto_Function();

        if (use_fade == 1) {
            SMACK::Smack_Fade(4);
            video::Set_Page_Off();
            graphics::Fill(0, 0, 0x27f, 0x1df, 0);
            platform::Toggle_Pages_Stub();
            ERIC::Set_Fade_Percent(0);
            ERIC::Set_Fade_Speed_Percent(10);
            ERIC::Fast_Fade_Out();
        }

        smckplay::Close_Smacker_Animation();
        if (use_fade == 0) {
            // Palette-only fades reuse the published pixels. Replace the movie
            // frame with its saved background now that the game palette is back.
            video::Copy_Back_To_Off();
            video::Publish_Off_Page();
        }
        MOX::global_cache_seg = (void*)buffer::Allocate_Buffer_Space(MOX::global_cache_size);
        RUSS::Mox_Update();
        JIM::jim_current_stream = (uint16_t)-1;
        JIM::Play_Background_Music();

        if (use_fade == 0) {
            MOX::FULL_DRAW_SCREEN_FLAG = 1;
            MOX2::Put_Auto_Function(&SMACK::smack_auto_function);
        }

        video::Set_Refresh_Stencil();
    }
} // namespace SMACK