#include "pch.h"
#include "tactical_events.h"
#include "tactical_control.h"

namespace TEXTBOX {
    int16_t bottom_y;
    uint8_t* textbox_seg;
    s_animation_header* help_anim_bitmap;
    uint16_t field_save_ix;
    s_fieldsav_data textbox_field_save[2];
    uint8_t reload_saved_block;
    int16_t box_x;
    int16_t box_y;
    uint16_t old_draw_fields_flag;
    MOX2::action_func_t textbox_af;
    uint16_t old_help_list_active;
    int32_t base_colors[3] = {160, 192, 224};

    void __cdecl Get_Help_Lbx_Name(char* help_lbx_name, size_t help_lbx_name_size) {
        const char* selected_name;

        switch (MOX::settings.language) {
            case 1: /* German */
                selected_name = "GER_HELP.LBX";
                break;
            case 2: /* French */
                selected_name = "FRE_HELP.LBX";
                break;
            case 3: /* Spanish */
                selected_name = "SPA_HELP.LBX";
                break;
            case 4: /* Italian */
                selected_name = "ITA_HELP.LBX";
                break;
            default: /* English / Default */
                selected_name = "HELP.LBX";
                break;
        }

        strlcpy(help_lbx_name, selected_name, help_lbx_name_size);
    }
    void __cdecl Box_Centered_On_XY(int16_t* x_ptr, int16_t* y_ptr, int16_t height) {
        int16_t total_height = height + 85;
        int16_t centered_y;

        if (MOX::current_screen == SCREEN_MAIN) {
            *x_ptr = 84;
            centered_y = (400 - total_height) / 2 + 21;
            if (centered_y < 6) {
                centered_y = 5;
            }
        } else {
            *x_ptr = 130;
            centered_y = (480 - total_height) / 2;
        }
        *y_ptr = (int16_t)centered_y;

        s_animation_header* header_top = (s_animation_header*)buffer::Buffer_Reload("textbox.lbx", 0, MOX::global_cache_seg);
        remap::Calculate_Picture_Remap_Colors(header_top);

        graphics::Tri_Interlaced_Fill(
            *x_ptr + 5,
            *y_ptr + 5,
            *x_ptr + 375,
            *y_ptr + total_height - 5,
            remap::picture_remap_color_list[2],
            remap::picture_remap_color_list[3],
            remap::picture_remap_color_list[4]);

        int16_t y_mid = *y_ptr;
        y_mid += 42;
        animate::Remap_Draw(*x_ptr, *y_ptr, header_top);

        s_animation_header* header_mid = (s_animation_header*)buffer::Buffer_Reload("textbox.lbx", 1, MOX::global_cache_seg);
        int16_t y_bot = height;
        y_bot += y_mid;

        int16_t window_status;
        MISC::Save_Window_Status(&window_status);

        graphics::Set_Window(*x_ptr, y_mid, *x_ptr + 380, y_bot);
        video::Clip_On();
        animate::Remap_Draw(*x_ptr, y_mid, header_mid);
        MISC::Restore_Window_Status(window_status);

        s_animation_header* header_bot = (s_animation_header*)buffer::Buffer_Reload("textbox.lbx", 2, MOX::global_cache_seg);
        animate::Remap_Draw(*x_ptr, y_bot, header_bot);

        bottom_y = y_bot + 43;
    }
    void __cdecl Text_Box_Startup(int32_t required_buffer_size) {
        HAROLD::Save_Mouse_List();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

        old_help_list_active = fields::help_list_active;
        fields::help_list_active = 0;

        MOX2::Get_Auto_Function(&textbox_af);
        fields::Deactivate_Auto_Function();

        video::Set_Refresh_Stencil();

        MOX2::Execute_Auto_Function(&textbox_af);

        old_draw_fields_flag = fields::draw_fields_flag;
        fields::Enable_Draw_All_Fields();
        fields::Draw_Visible_Fields();
        fields::Disable_Draw_All_Fields();

        FIELDSAV::Save_Field_Stats(&textbox_field_save[field_save_ix]);
        field_save_ix++;

        int32_t remaining_space = allocate::Get_Remaining_Block_Space(MOX::screen_seg);
        if ((remaining_space - 0x200) < required_buffer_size) {
            swap::Save_Block_To_Drive((uint8_t*)MOX::screen_seg, required_buffer_size + 0x200);
            textbox_seg = (uint8_t*)allocate::Allocate_First_Block(MOX::screen_seg, required_buffer_size + 0x180);
            reload_saved_block = 1;
        } else {
            allocate::Push_Block(MOX::screen_seg);
            reload_saved_block = 0;
            textbox_seg = (uint8_t*)allocate::Allocate_Next_Block(MOX::screen_seg, required_buffer_size + 0x180);
        }

        s_animation_header* pic = (s_animation_header*)buffer::Buffer_Reload("textbox.lbx", 0, MOX::global_cache_seg);
        remap::Calculate_Picture_Remap_Colors(pic);
    }

    void __cdecl Text_Box_Get_Input(uint16_t ticks) {
        if (ticks != 0) {
            fields::Timed_Wait_For_Input(ticks);
            fields::Clear_Mouse_Buffer();
            fields::Set_Input_Delay(3);
            return;
        }

        // Wait for mouse button release
        while (mouse::Mouse_Button() != 0) {
            timer::Mark_Time();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
        }

        fields::Clear_Mouse_Buffer();

        // Wait for any keyboard or mouse input
        do {
            timer::Mark_Time();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
        } while (fields::Get_Input() == 0);
    }

    void __cdecl Text_Box_Cleanup() {
        RUSS::Mox_Sync_Update();
        field_save_ix--;
        FIELDSAV::Restore_Field_Stats(textbox_field_save);
        fields::Enable_Draw_All_Fields();

        if (reload_saved_block != 0) {
            swap::Restore_Block_From_Drive();
        } else {
            allocate::Pop_Block(MOX::screen_seg);
        }

        MOX2::Put_Auto_Function(&textbox_af);
        fields::Activate_Auto_Function();
        fields::Clear_Mouse_Buffer();
        fields::Set_Input_Delay(3);

        fields::help_list_active = old_help_list_active;
        fields::draw_fields_flag = old_draw_fields_flag;

        HAROLD::Restore_Mouse_List();
    }

    static void Draw_Text_Box(const char* title, const char* text, uint16_t input_ticks, bool boarding) {
        s_colors primary_colors;
        s_animation_header* anim;
        s_colors secondary_colors;
        uint8_t color_2 = 0;
        uint8_t color_1 = 0;
        int16_t font_spacing;
        int16_t title_x;
        int16_t window_status;
        int16_t font_style;
        uint16_t width;
        int16_t y = 0;
        int16_t x = 0;

        Get_Mox_Text_Box_Base_Colors(0, &color_1, &color_2);
        Text_Box_Startup(60000);

        primary_colors = *MISC::Font_Colors2(5, color_1, color_2);
        secondary_colors = *MISC::Font_Colors2(3, color_1, color_2);

        fonts::Set_Font_Style(3, &primary_colors);

        uint16_t max_height = (uint16_t)FMTPARA::Get_Formatted_Paragraph_Max_Height(339, text);
        if (max_height > 374) {
            max_height = 374;
        }

        if (FMTPARA::Get_Last_Formatted_Paragraph_Max_Width() < 339) {
            width = (uint16_t)FMTPARA::Get_Last_Formatted_Paragraph_Max_Width();
        } else {
            width = 339;
        }

        Box_Centered_On_XY(&x, &y, max_height);

        anim = (s_animation_header*)farload::Far_Reload_Next("textbox.lbx", 3, textbox_seg);

        if (title != nullptr) {
            fonts::Set_Remap_Font_Style(5, &primary_colors);
            int16_t title_y_base = y;
            title_x = x + 190;
            fonts::Get_String_Width(title);
            MISC::Set_Fitted_Font_Style((char*)title, &primary_colors, 5, 2, -1, 380, 3);
            font_style = MISC::Get_Font_Style();
            font_spacing = MISC::Get_Font_Spacing();
            primary_colors = *MISC::Font_Colors2(font_style, color_1, color_2);
            fonts::Set_Remap_Font_Style(font_style, &primary_colors);
            fonts::Set_Font_Spacing(font_spacing);
            fonts::Print_Centered(title_x, title_y_base + 11, title);
        }

        fonts::Set_Remap_Font_Style(3, &secondary_colors);

        int16_t print_x = x + 20;
        int16_t print_y = y + 42;

        if (title == nullptr) {
            print_x = x + 8 + (362 - width) / 2;
            print_y = y + 12 + ((max_height + 43) - max_height) / 2;
        }

        MISC::Save_Window_Status(&window_status);

        graphics::Set_Window(x + 20, y + 42, x + 359, y + 418);

        video::Clip_On();

        FMTPARA::Print_Formatted_Paragraph(print_x, print_y, width, text, 0);

        MISC::Restore_Window_Status(window_status);

        video::Copy_Off_To_Back();

        /* "\x1B" is fmtpara's color-attribute control byte; used here as a non-printing field label. */
        fields::Add_Hidden_Field(0, 0, 639, 479, "\x1B", 41);

        bool acknowledged = false;
        if (boarding) {
            tactical_events::Open_Boarding_Report(input_ticks != 0);
            const auto generation = tactical_events::Boarding_Generation();
            if (input_ticks != 0) {
                // Same native timed wait, with only this report's explicit intent checked.
                uint32_t ticks = 0;
                bool has_input = false;
                do {
                    platform::Check_Exit();
                    has_input = key::Keyboard_Status() || mouse::Mouse_Buffer() == 1;
                    if (!has_input) acknowledged = tactical_control::Take_Report_Acknowledgement(generation);
                    ++ticks;
                    fields::Quick_Call_Auto_Function();
                    if (ticks == input_ticks) has_input = true;
                } while (!has_input && !acknowledged);
                fields::Clear_Mouse_Buffer();
                fields::Set_Input_Delay(3);
            } else {
                while (mouse::Mouse_Button() != 0) {
                    timer::Mark_Time();
                    RUSS::Mox_Sync_Update();
                    timer::Release_Time(1);
                }
                fields::Clear_Mouse_Buffer();
                do {
                    timer::Mark_Time();
                    RUSS::Mox_Sync_Update();
                    timer::Release_Time(1);
                    if (fields::input_delay <= 0) acknowledged = tactical_control::Take_Report_Acknowledgement(generation);
                } while (!acknowledged && fields::Get_Input() == 0);
            }
        } else
            Text_Box_Get_Input(input_ticks);

        animate::Set_Animation_Frame(anim, 1);
        animate::Remap_Draw(x + 158, bottom_y - 27, anim);

        Text_Box_Cleanup();
        if (boarding) {
            tactical_events::Close_Boarding_Report();
            if (acknowledged) tactical_control::Complete_Report_Acknowledgement();
        }

        MOX::FULL_DRAW_SCREEN_FLAG = 1;
    }

    void __cdecl Do_Text_Box(const char* title, const char* text, uint16_t input_ticks) {
        Draw_Text_Box(title, text, input_ticks, false);
    }
    void Boarding_Result_Box(const char* title, const char* text, uint16_t ticks, bool authorized) {
        Draw_Text_Box(title, text, ticks, authorized);
    }

    void __cdecl Text_Box(const char* title, const char* text) {
        Do_Text_Box(title, text, 0);
    }

    void __cdecl Timed_Text_Box(const char* title, const char* message, uint16_t duration) {
        Do_Text_Box(title, message, duration);
    }

    void __cdecl Get_Mox_Text_Box_Base_Colors(int16_t color_set, uint8_t* out_color_a, uint8_t* out_color_b) {
        uint8_t base_color;

        if (color_set == 1) {
            base_color = (uint8_t)TEXTBOX::base_colors[1];
        } else if (color_set == 2) {
            base_color = (uint8_t)TEXTBOX::base_colors[2];
        } else {
            base_color = (uint8_t)TEXTBOX::base_colors[0];
        }

        int16_t color_group;
        switch (MOX::current_screen) {
            case SCREEN_COLONY:
            case SCREEN_COLONY_SUMMARY:
            case SCREEN_QUEUE_POPUP:
            case SCREEN_COLONY_LANDING:
                color_group = 2;
                break;
            case SCREEN_INFO:
                color_group = 3;
                break;
            case SCREEN_MAIN_MENU:
                color_group = 1;
                break;
            default:
                color_group = 0;
                break;
        }

        uint8_t color = (uint8_t)(base_color + color_group * 3);
        *out_color_a = color;
        *out_color_b = (uint8_t)(color + 2);
    }

    void __cdecl Draw_Help_Entry(int16_t help_id) {
        box_x = 0;
        box_y = 0;

        uint8_t primary_color = 0;
        uint8_t secondary_color = 0;

        Text_Box_Startup(0x11B34);
        Get_Mox_Text_Box_Base_Colors(0, &primary_color, &secondary_color);

        s_colors* title_color_ptr = MISC::Font_Colors2(4, primary_color, secondary_color);
        s_colors title_colors = *title_color_ptr;

        s_colors* body_color_ptr = MISC::Font_Colors2(2, primary_color, secondary_color);
        s_colors body_colors = *body_color_ptr;

        fonts::Set_Remap_Font_Style(4, &title_colors);
        fonts::Get_Font_Height();
        fonts::Set_Remap_Font_Style(2, &body_colors);

        allocate::Push_Block(textbox_seg);

        uint16_t record_count = 0;
        uint32_t help_idx = (uint32_t)help_id;
        uint32_t total_lbx_size = 0;

        char help_lbx_name[20];
        Get_Help_Lbx_Name(help_lbx_name, sizeof(help_lbx_name));

        s_help_record records[9];
        bool has_anim[9];

        do {
            allocate::Push_Block(textbox_seg);
            void* record_data = farload::Far_Reload_Next_Data(help_lbx_name, 0, textbox_seg, help_idx, 1, sizeof(s_help_record));

            memcpy(&records[record_count], record_data, sizeof(s_help_record));

            FMTPARA::Get_Formatted_Paragraph_Max_Height(0x153, records[record_count].body);
            allocate::Pop_Block(textbox_seg);

            has_anim[record_count] = (records[record_count].anim_lbx[0] != '\0');
            if (has_anim[record_count]) {
                uint16_t rec_idx = (uint16_t)(records[record_count].anim_info & 0xFFFF);
                s_vfs_file* anim_file = vfs::fopen_lbx(records[record_count].anim_lbx, rec_idx);
                if (anim_file == nullptr) {
                    farload::Error_Handler(records[record_count].anim_lbx, 1, rec_idx);
                }

                int32_t anim_size = vfs::fsize(anim_file);
                if (anim_size < 0) {
                    vfs::fclose(anim_file);
                    farload::Error_Handler(records[record_count].anim_lbx, 7, rec_idx);
                }

                total_lbx_size += (uint32_t)anim_size + 0x400;
                vfs::fclose(anim_file);
            }

            uint32_t next_idx = records[record_count].next_help_idx;
            if (next_idx == 0xFFFFFFFF) {
                help_idx++;
            } else {
                help_idx = next_idx;
            }

            record_count++;
        } while (help_idx != 0 && record_count <= 8);

        allocate::Pop_Block(textbox_seg);

        if (allocate::Get_Remaining_Block_Space(textbox_seg) < (int32_t)total_lbx_size) {
            swap::Save_Block_To_Drive(textbox_seg, total_lbx_size + 0x11B34);
            textbox_seg = (uint8_t*)allocate::Allocate_First_Block(MOX::screen_seg, total_lbx_size + 0x11B34);
            reload_saved_block = 1;
        }

        s_animation_header* textbox_lbx_ptr = (s_animation_header*)farload::Far_Reload_Next("textbox.lbx", 3, textbox_seg);

        help_anim_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(textbox_seg, 0x30D4);

        int16_t title_y[9];
        uint8_t anim_frames[9];
        int16_t draw_x[9];
        int16_t format_width[9];
        s_animation_header* anim_ptrs[9];
        int16_t body_y[9];
        int16_t draw_y[9];

        int16_t current_y = 11;

        for (int i = 0; i < record_count; ++i) {
            title_y[i] = current_y;
            format_width[i] = 0x153;
            int16_t bmp_y_val = 0;

            if (has_anim[i]) {
                int32_t anim_info = (int32_t)records[i].anim_info;
                uint16_t rec_idx = (uint16_t)anim_info;
                uint8_t frame = (uint8_t)(anim_info >> 16);

                anim_frames[i] = frame;
                anim_ptrs[i] = (s_animation_header*)farload::Far_Reload_Next(records[i].anim_lbx, rec_idx, textbox_seg);
                animate::Set_Animation_Frame(anim_ptrs[i], frame);
                bitmap::Draw_Picture_To_Bitmap(help_anim_bitmap, anim_ptrs[i]);

                int16_t min_x, min_y, actual_width;
                bitmap::Get_Bitmap_Actual_Size(help_anim_bitmap, &min_x, &min_y, &actual_width, &bmp_y_val);

                draw_x[i] = 0x162 - actual_width - min_x;
                draw_y[i] = current_y - min_y;
                format_width[i] = 0x14D - actual_width;
            }

            fonts::Set_Font_Style(4, &title_colors);

            int16_t title_height = 0;
            if (records[i].title[0] != '\x14') {
                title_height = FMTPARA::Get_Formatted_Paragraph_Max_Height(format_width[i], records[i].title);
            }

            if (bmp_y_val > 0 && title_height < bmp_y_val) {
                title_height = bmp_y_val;
            }

            current_y += title_height + 4;
            if (i == 0 && current_y < 42) {
                current_y = 42;
            }

            body_y[i] = current_y;

            fonts::Set_Font_Style(2, &body_colors);
            int16_t body_height = FMTPARA::Get_Formatted_Paragraph_Max_Height(0x153, records[i].body);

            current_y += body_height + 8;
        }

        int16_t box_x, box_y;
        Box_Centered_On_XY(&box_x, &box_y, current_y - 42);

        for (int i = 0; i < record_count; ++i) {
            if (records[i].title[0] != '\x14') {
                fonts::Set_Remap_Font_Style(4, &title_colors);
                FMTPARA::Print_Formatted_Paragraph(
                    box_x + 20,
                    title_y[i] + box_y,
                    format_width[i],
                    records[i].title,
                    0);
                fonts::Set_Remap_Font_Style(2, &body_colors);
            }
            FMTPARA::Print_Formatted_Paragraph(
                box_x + 20,
                body_y[i] + box_y,
                0x153,
                records[i].body,
                0);

            if (has_anim[i]) {
                animate::Set_Animation_Frame(anim_ptrs[i], anim_frames[i]);
                animate::Draw(
                    draw_x[i] + box_x,
                    draw_y[i] + box_y,
                    anim_ptrs[i]);
            }
        }

        video::Copy_Off_To_Back();
        /* "\x1B" is fmtpara's color-attribute control byte; used here as a non-printing field label. */
        fields::Add_Hidden_Field(0, 0, 639, 479, "\x1B", 41);
        Text_Box_Get_Input(0);

        animate::Set_Animation_Frame(textbox_lbx_ptr, 1);
        animate::Remap_Draw(
            box_x + 158,
            bottom_y - 27,
            textbox_lbx_ptr);

        Text_Box_Cleanup();
        MOX::FULL_DRAW_SCREEN_FLAG = 1;
    }
} // namespace TEXTBOX