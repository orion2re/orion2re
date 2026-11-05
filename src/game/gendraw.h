#pragma once

namespace GENDRAW {
    void __cdecl Message_Box(const char* text);

    void __cdecl Help(const char* message);

    int16_t __cdecl Confirmation_Box(const char* text);
    void Open_Confirmation_Box(const char* text);
    void Close_Confirmation_Box();
    void Open_Warning_Box(const char* text);

    int16_t __cdecl Do_Confirmation_Box(const char* msg);

    void __cdecl Warning_Box(const char* message);

    extern s_colors message_text_colors;
    extern uint8_t warning_box;
    extern int16_t message_box_y;
    extern s_animation_header* box_bitmap;
    extern int16_t animation_frame;
    extern int16_t direction;
    extern int16_t first_field;
    extern s_animation_header* image_seg[4];

    extern s_fieldsav_data message_field_save;

    extern MOX2::action_func_t gendraw_af;

    extern int16_t save_refresh_mode;

    extern int16_t old_draw_fields_flag;

    extern int16_t old_help_list_active;

    void __cdecl Draw_Confirm_Box();

    void __cdecl Message_Box_Startup();

    void __cdecl _Print_1_Or_More_Formatted_Lines_Centered(int16_t x, int16_t y, int16_t width, const char* text, s_animation_header* bitmap);

    void __cdecl Do_Print_1_Or_More_Formatted_Lines_Centered(int16_t x, int16_t y, int16_t width, const char* text);

    void __cdecl Draw_Animating_Message_Box();

    void __cdecl Message_Box_Cleanup();

    void __cdecl Message_Box_Exploding(const char* text);

    void __cdecl Message_Box_Exploding_Star(const char* text, int16_t star_idx);
} // namespace GENDRAW