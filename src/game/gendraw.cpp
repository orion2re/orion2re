#include "pch.h"
#include "game_events.h"

namespace GENDRAW {
    s_colors message_text_colors = {{0xA0, 0xA3, 0xA3, 0xA5, 0xAB, 0xAB, 0xAB, 0xAB}};
    uint8_t warning_box;
    int16_t message_box_y;
    s_animation_header* box_bitmap;
    int16_t animation_frame;
    int16_t direction;
    int16_t first_field;
    s_animation_header* image_seg[4];
    s_fieldsav_data message_field_save;
    MOX2::action_func_t gendraw_af;
    int16_t save_refresh_mode;
    int16_t old_draw_fields_flag;
    int16_t old_help_list_active;

    void __cdecl Message_Box(const char* text) {
        TEXTBOX::Do_Text_Box(nullptr, text, 0);
    }

    void __cdecl Help(const char* message) {
        Message_Box(message);
    }

    void __cdecl Warning_Box(const char* message) {
        warning_box = 1;
        Message_Box_Exploding(message);
    }

    void __cdecl Message_Box_Exploding_Star(const char* text, int16_t star_idx) {
        Message_Box_Exploding(text);
    }

    void __cdecl Draw_Confirm_Box() {
        animate::Set_Animation_Frame(image_seg[1], 0);
        animate::Set_Animation_Frame(image_seg[2], 0);

        int16_t input = fields::Auto_Input();

        if (first_field == input) {
            animate::Set_Animation_Frame(image_seg[1], 1);
        } else if (first_field - input == -1) {
            animate::Set_Animation_Frame(image_seg[2], 1);
        }

        animate::Remap_Draw(0xeb, 0x12e, image_seg[1]);
        animate::Remap_Draw(0x159, 0x12e, image_seg[2]);
    }

    void __cdecl Message_Box_Startup() {
        HAROLD::Save_Mouse_List();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        ERIC::Fast_Fade_In();
        allocate::Push_Block(MOX::screen_seg);
        MOX2::Get_Auto_Function(&gendraw_af);
        old_help_list_active = fields::help_list_active;
        fields::help_list_active = 0;
        MOX2::Execute_Auto_Function(&gendraw_af);
        old_draw_fields_flag = fields::draw_fields_flag;
        fields::Enable_Draw_All_Fields();
        fields::Draw_Visible_Fields();
        fields::Disable_Draw_All_Fields();
        FIELDSAV::Save_Field_Stats(&message_field_save);
        video::Set_Page_Off();
        save_refresh_mode = video::Get_Refresh_Mode();
        video::Set_Refresh_Stencil();
        animation_frame = 0;
        direction = 1;
    }

    void __cdecl Draw_Animating_Message_Box() {
        int16_t box_y = message_box_y + 0x25;

        animate::Set_Animation_Frame(image_seg[0], animation_frame);

        if (animation_frame == 0) {
            direction = 1;
        }
        if (animation_frame == 6) {
            direction = -1;
        }

        animation_frame += direction;

        animate::Remap_Draw(0x9A, message_box_y, image_seg[0]);
        animate::Draw(0xD0, box_y, box_bitmap);
    }

    void __cdecl Message_Box_Exploding(const char* str) {
        game_events::s_leader_interruption_scope leader_interruption;
        game_events::s_relations_interruption_scope relations_interruption;
        Message_Box_Startup();

        fonts::Set_Font_Style(4, &message_text_colors);
        fonts::Set_Font_LF(0x12);

        image_seg[0] = (s_animation_header*)buffer::Buffer_Reload("warning.lbx", 0, MOX::global_cache_seg);
        remap::Calculate_Picture_Remap_Colors(image_seg[0]);

        uint32_t size = bitmap::Get_Bitmap_Size(0x14B, 0xBF);
        box_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, size);
        bitmap::Create_Blank_Bitmap(0x14B, 0xBF, box_bitmap);

        message_box_y = 0x90;
        int mode = 4;
        first_field = fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "\x1B", 0x29);

        s_colors font_colors;
        fonts::Set_Remap_Font_Style(4, &font_colors);
        fonts::Set_Font_LF(0x12);

        int16_t height = FMTPARA::Get_Formatted_Paragraph_Max_Height(0xE2, str);
        if (height > 0x78) {
            mode = 3;
            fonts::Set_Remap_Font_Style(3, &font_colors);
            fonts::Set_Font_LF(0x12);
        }

        uint8_t base_color1;
        uint8_t base_color2;
        TEXTBOX::Get_Mox_Text_Box_Base_Colors(1, &base_color1, &base_color2);
        s_colors* psVar2 = MISC::Font_Colors2(mode, base_color1, base_color2);
        font_colors = *psVar2;

        fonts::Set_Remap_Font_Style(mode, &font_colors);
        fonts::Set_Font_LF(0x12);

        image_seg[0] = (s_animation_header*)buffer::Buffer_Reload("warning.lbx", 0, MOX::global_cache_seg);
        animate::Remap_Draw(0x9A, message_box_y, image_seg[0]);

        Do_Print_1_Or_More_Formatted_Lines_Centered(0xD0, 0xE8, 0xE2, str);
        video::Copy_Off_To_Back();

        setvesa::Add_Square_To_Scan_List(0, 0, 0x27F, 0x1DF);
        RUSS::Mox_Sync_Update();

        bitmap::Screen_Bitmap_Capture(0xD0, message_box_y + 0x25, 0x1B2, message_box_y + 0x9D, box_bitmap);

        fields::Assign_Auto_Function(Draw_Animating_Message_Box, 2);

        int16_t input = fields::Get_Input();
        while (input == 0) {
            timer::Mark_Time();
            Draw_Animating_Message_Box();
            setvesa::Add_Square_To_Scan_List(5, 0x14, 0x24D, 0x1CB);
            RUSS::Mox_Sync_Update();
            timer::Release_Time(2);
            input = fields::Get_Input();
        }

        Message_Box_Cleanup();
    }

    int16_t __cdecl Confirmation_Box(const char* text) {
        game_events::s_leader_interruption_scope leader_interruption;
        game_events::s_relations_interruption_scope relations_interruption;
        uint8_t color_1;
        uint8_t color_2;
        s_colors font_colors;

        TEXTBOX::Get_Mox_Text_Box_Base_Colors(2, &color_1, &color_2);
        s_colors* colors_ptr = MISC::Font_Colors2(4, color_1, color_2);

        font_colors = *colors_ptr;
        fonts::Set_Remap_Font_Style(4, &font_colors);
        Message_Box_Startup();

        for (int i = 0; i < 3; ++i) {
            image_seg[i] = (s_animation_header*)buffer::Buffer_Reload("confirm.lbx", i, (uint8_t*)MOX::global_cache_seg);
            if (i == 0) {
                remap::Calculate_Picture_Remap_Colors(image_seg[0]);
            }
        }

        first_field = fields::Add_Hidden_Field(0xeb, 0x12e, 0x11e, 0x143, "Y", 0x29);
        int16_t second_field = fields::Add_Hidden_Field(0x159, 0x12e, 0x18c, 0x143, "N", 0x29);

        for (int i = 0; i < 3; ++i) {
            image_seg[i] = (s_animation_header*)buffer::Buffer_Reload("confirm.lbx", i, (uint8_t*)MOX::global_cache_seg);
        }

        int16_t font_idx = 4;
        animate::Remap_Draw(0xa1, 0x75, image_seg[0]);

        do {
            TEXTBOX::Get_Mox_Text_Box_Base_Colors(2, &color_1, &color_2);
            s_colors* colors_ptr2 = MISC::Font_Colors2(font_idx, color_1, color_2);

            font_colors = *colors_ptr2;
            fonts::Set_Remap_Font_Style(font_idx, &font_colors);
            font_idx--;

            fonts::Set_Font_LF(0x12);

            if (FMTPARA::Get_Formatted_Paragraph_Max_Height(0xe0, text) <= 0x7e) {
                break;
            }
        } while (font_idx > 1);

        Do_Print_1_Or_More_Formatted_Lines_Centered(0xcc, 0xd0, 0xe0, text);
        Draw_Confirm_Box();
        video::Copy_Off_To_Back();
        setvesa::Add_Square_To_Scan_List(0, 0, 0x27f, 0x1df);
        RUSS::Mox_Sync_Update();
        fields::Assign_Auto_Function(Draw_Confirm_Box, 2);

        int16_t input;
        do {
            timer::Mark_Time();
            input = fields::Get_Input();
            if (input == 0) {
                RUSS::Mox_Sync_Update();
            }
            timer::Release_Time(2);
        } while (input != first_field && input != second_field);

        Message_Box_Cleanup();
        MOX::FULL_DRAW_SCREEN_FLAG = 1;

        return (input == first_field) ? 1 : 0;
    }

    int16_t __cdecl Do_Confirmation_Box(const char* msg) {
        return Confirmation_Box(msg);
    }

    void __cdecl _Print_1_Or_More_Formatted_Lines_Centered(int16_t x, int16_t y, int16_t width, const char* text, s_animation_header* bitmap) {
        int16_t max_height = FMTPARA::Get_Formatted_Paragraph_Max_Height(width, text);
        int16_t centered_y = y + (static_cast<int16_t>(fonts::font_header.current_height) - max_height) / 2;

        if (bitmap == nullptr) {
            FMTPARA::Print_Formatted_Paragraph(x, centered_y, width, text, 2);
        } else {
            FMTPARA::Print_Formatted_Paragraph_To_Bitmap(x, centered_y, width, text, 2, bitmap);
        }
    }

    void __cdecl Do_Print_1_Or_More_Formatted_Lines_Centered(int16_t x, int16_t y, int16_t width, const char* text) {
        _Print_1_Or_More_Formatted_Lines_Centered(x, y, width, text, nullptr);
    }

    void __cdecl Message_Box_Cleanup() {
        FIELDSAV::Restore_Field_Stats(&message_field_save);
        MOX2::Put_Auto_Function(&gendraw_af);
        allocate::Pop_Block(MOX::screen_seg);

        switch (save_refresh_mode) {
            case 0:
                video::Set_Refresh_Full();
                break;
            case 2:
                video::Set_Refresh_Differential();
                break;
            case 3:
                video::Set_Refresh_Stencil();
                break;
            default:
                break;
        }

        fields::draw_fields_flag = old_draw_fields_flag;
        fields::help_list_active = old_help_list_active;

        fields::Clear_Mouse_Buffer();
        fields::Set_Input_Delay(3);

        HAROLD::Restore_Mouse_List();
    }
} // namespace GENDRAW