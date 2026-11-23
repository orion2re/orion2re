#include "pch.h"
#include "string_overrides.h"

namespace ANTAROOM {
    s_colors antaran_room_normal_colors = {{250, 254, 255, 251, 252, 253, 254, 255}};
    char* antaran_room_message;
    uint8_t* antaran_room_mouse_bitmap;
    s_mouse_list antaran_room_blank_mouse[1] = {{0, 0, 0, 0, 639, 479}};

    void __cdecl Draw_Main_Antaran_Room_Screen() {
        video::Set_Page_Off();
        fonts::Set_Outline_Color(0xFA, 0xFA);

        if (MOX::settings.animations_on == 1) {
            file_ani::Draw_File_Animation_Stencil();
        }

        fonts::Set_Outline_Color(0x0, 0xFA);
        fonts::Set_Font_Style_Outline(4, &antaran_room_normal_colors);

        int16_t max_height = BILL::_Get_Paragraph_Max_Height(0x226, antaran_room_message);
        BILL::_Print_Paragraph(0x37, 0x1D6 - max_height, 0x226, antaran_room_message, 2);

        JIM::Play_Streaming_Music(0x78, -1, 1);
    }

    void __cdecl Main_Antaran_Room_Screen(int16_t message_set_index) {
        fields::Clear_Fields();
        fields::Deactivate_Help_List();
        fields::Enable_Draw_All_Fields();
        graphics::Reset_Window();
        video::Clip_Off();
        mouse::Set_Mouse_List(antaran_room_blank_mouse, 1);
        JIM::Play_Streaming_Music(120, -1, 1);
        ERIC::Set_Fade_Percent(0);
        ERIC::Fast_Fade_Out();
        fields::Clear_Fields();
        video::Set_Refresh_Stencil();

        MOX2::Get_Auto_Function(&EVENTS::jim_saved_autofunction);
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();

        Reload_Main_Antaran_Room_Screen();
        Get_Antaran_Room_Message(1 - message_set_index);

        bool is_done = false;
        fields::Assign_Auto_Function(Draw_Main_Antaran_Room_Screen, 2);

        while (!is_done) {
            timer::Mark_Time();
            fields::Clear_Fields();

            int16_t hidden_field = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);

            if (fields::Get_Input() == hidden_field || key::Keyboard_Status()) {
                is_done = true;
            }

            if (!is_done) {
                Draw_Main_Antaran_Room_Screen();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(2);
            }
        }

        MOX::auto_turn_exit = 1;
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        file_ani::Reset_File_Animation_Buffer();
        mouse::Set_Mouse_List(antaran_room_blank_mouse, 1);
        JIM::Play_Background_Music();
        ERIC::Set_Fade_Percent(0);
        ERIC::Fast_Fade_Out();

        RUSS::Release_All_Blocks(MOX::screen_seg);
        MOX2::Put_Auto_Function(&EVENTS::jim_saved_autofunction);
    }

    void __cdecl Reload_Main_Antaran_Room_Screen() {
        antaran_room_mouse_bitmap = (uint8_t*)allocate::Allocate_First_Block(MOX::screen_seg, 0x395);
        s_animation_header* pic = (s_animation_header*)farload::Far_Reload_Next("antaroom.LBX", 0, MOX::screen_seg);
        bitmap::Clear_Mouse_Image(0);
        bitmap::Create_Blank_Bitmap(30, 30, antaran_room_mouse_bitmap);
        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)antaran_room_mouse_bitmap, pic);
        video::Set_Page_Off();
        animate::Draw_Palette(pic);
        palette::Set_Palette_Changes(0, 255);
        graphics::Fill(0, 0, 639, 479, 0);
        antaran_room_message = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, ANTARAN_ROOM_MESSAGE_CAPACITY);

        int32_t remaining = allocate::Get_Remaining_Block_Space(MOX::screen_seg);
        uint8_t* anim_buf = (uint8_t*)allocate::Allocate_Next_Block(MOX::screen_seg, remaining - 100);
        file_ani::Set_File_Animation_Buffer(anim_buf);

        video::Set_Refresh_Stencil();
        file_ani::Open_File_Animation("antaroom.LBX", 1);
        video::Set_Page_Off();
        file_ani::Draw_File_Animation();
        palette::Refresh_Palette();
        video::Copy_Off_To_Back();

        bitmap::Draw_Bitmap_To_Mouse(0, 0, (s_animation_header*)antaran_room_mouse_bitmap, 0);
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
    }

    void __cdecl Get_Antaran_Room_Message(int16_t message_index) {
        char buffer[200];

        uint32_t random_val = game_random::Random(4);
        int16_t message_lbx_index = (int16_t)(random_val + (message_index + (uint8_t)MOX::settings.language * 2) * 4 - 1);

        string_overrides::Load_Message("antarmsg.LBX", message_lbx_index, 0, sizeof(buffer),
            message_index * 4 + random_val, buffer, sizeof(buffer));

        strlcpy(antaran_room_message, buffer, ANTARAN_ROOM_MESSAGE_CAPACITY);
    }
} // namespace ANTAROOM