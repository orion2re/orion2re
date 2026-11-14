#pragma once

#include <string>
#include <vector>

namespace test::fakes {
    void Reset_All_();

    struct Draw_Call {
        int16_t x;
        int16_t y;
        s_animation_header* pic;
    };

    struct Print_Centered_Call {
        int16_t x;
        int16_t y;
        const char* text;
        std::string text_copy;
    };

    struct Print_Call {
        int16_t x;
        int16_t y;
        const char* text;
        std::string text_copy;
    };

    struct Forced_Cursor_Pos_Call {
        int16_t x;
        int16_t y;
    };

    struct Line_Call {
        int16_t x1;
        int16_t y1;
        int16_t x2;
        int16_t y2;
        uint8_t color;
    };

    struct Fill_Call {
        int16_t x1;
        int16_t y1;
        int16_t x2;
        int16_t y2;
        uint8_t color;
    };

    struct Dithered_Fill_Call {
        int16_t x1;
        int16_t y1;
        int16_t x2;
        int16_t y2;
        uint8_t base;
        uint8_t speckle;
        uint8_t density;
    };

    struct Dithered_Box_Call {
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;
        int16_t thickness;
        uint8_t base;
        uint8_t speckle;
        uint8_t density;
    };

    struct Box_Call {
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;
        int16_t thickness;
        uint8_t color;
    };

    extern int16_t g_anim_width;
    extern int16_t g_anim_height;
    extern int16_t g_anim_full_store_flag;

    extern int16_t g_font_style;
    extern int16_t g_font_spacing;
    extern int16_t g_font_height;
    extern uint16_t g_font_vertical_spacing;
    extern int16_t g_string_width;
    extern s_colors g_font_color;
    extern s_colors g_font_highlight_color;
    extern s_colors g_font_special_color;

    extern int16_t g_mouse_pointer_x;
    extern int16_t g_mouse_pointer_y;
    extern int g_mouse_pointer_offset;
    extern int16_t g_mouse_button;
    extern int16_t g_mouse_buffer_flag;
    extern uint16_t g_mouse_buffer_flag2;
    extern int16_t g_mouse_buffer_button;
    extern int16_t g_mouse_buffer_x;
    extern int16_t g_mouse_buffer_y;
    extern std::vector<int16_t> g_mouse_button_sequence;

    extern uint8_t g_keyboard_status;
    extern int32_t g_read_key_value;
    extern int16_t g_scan_key_value;
    extern std::vector<int16_t> g_scan_key_sequence;

    extern int32_t g_last_played_sound;
    extern int16_t g_last_help_id;
    extern int g_mark_time_calls;
    extern std::string g_last_message_box_text;
    extern std::string g_last_message_box_caption;
    extern bool g_last_message_box_is_error;
    extern std::vector<int32_t> g_release_time_calls;
    extern std::vector<Draw_Call> g_draw_calls;
    extern std::vector<Print_Centered_Call> g_print_centered_calls;
    extern std::vector<Print_Call> g_print_calls;
    extern std::vector<Forced_Cursor_Pos_Call> g_forced_cursor_pos_calls;
    extern std::vector<Line_Call> g_line_calls;
    extern std::vector<Fill_Call> g_fill_calls;
    extern std::vector<Dithered_Fill_Call> g_dithered_fill_calls;
    extern std::vector<Dithered_Box_Call> g_dithered_box_calls;
    extern std::vector<Box_Call> g_box_calls;
}