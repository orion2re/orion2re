#include "pch.h"

#include "fakes/fake_test_support.h"

#include <cstring>

int16_t g_cached_mouse_x = 0;
int16_t g_cached_mouse_y = 0;
int32_t g_window_focus_state = 0;
uint32_t g_last_scan_code = 0;

namespace test::fakes {
    int32_t g_last_played_sound = -1;
    int16_t g_last_help_id = -1;
    int g_mark_time_calls = 0;
    std::string g_last_message_box_text;
    std::string g_last_message_box_caption;
    bool g_last_message_box_is_error = false;
    std::vector<int32_t> g_release_time_calls;
    std::vector<Draw_Call> g_draw_calls;
    std::vector<Print_Centered_Call> g_print_centered_calls;
    std::vector<Print_Call> g_print_calls;
    std::vector<Forced_Cursor_Pos_Call> g_forced_cursor_pos_calls;
    std::vector<Line_Call> g_line_calls;
    std::vector<Fill_Call> g_fill_calls;
    std::vector<Dithered_Fill_Call> g_dithered_fill_calls;
    std::vector<Dithered_Box_Call> g_dithered_box_calls;
    std::vector<Box_Call> g_box_calls;

    int16_t g_anim_width = 20;
    int16_t g_anim_height = 10;
    int16_t g_anim_full_store_flag = 0;

    int16_t g_font_style = 3;
    int16_t g_font_spacing = -1;
    int16_t g_font_height = 8;
    uint16_t g_font_vertical_spacing = 2;
    int16_t g_string_width = 6;
    s_colors g_font_color{};
    s_colors g_font_highlight_color{};
    s_colors g_font_special_color{};

    int16_t g_mouse_pointer_x = 0;
    int16_t g_mouse_pointer_y = 0;
    int g_mouse_pointer_offset = 0;
    int16_t g_mouse_button = 0;
    int16_t g_mouse_buffer_flag = 0;
    uint16_t g_mouse_buffer_flag2 = 0;
    int16_t g_mouse_buffer_button = 0;
    int16_t g_mouse_buffer_x = 0;
    int16_t g_mouse_buffer_y = 0;
    std::vector<int16_t> g_mouse_button_sequence;

    uint8_t g_keyboard_status = 0;
    int32_t g_read_key_value = 0;
    int16_t g_scan_key_value = 0;
    std::vector<int16_t> g_scan_key_sequence;

    void Reset_All_() {
        g_last_played_sound = -1;
        g_last_help_id = -1;
        g_mark_time_calls = 0;
        g_last_message_box_text.clear();
        g_last_message_box_caption.clear();
        g_last_message_box_is_error = false;
        g_release_time_calls.clear();
        g_draw_calls.clear();
        g_print_centered_calls.clear();
        g_print_calls.clear();
        g_forced_cursor_pos_calls.clear();
        g_line_calls.clear();
        g_fill_calls.clear();
        g_dithered_fill_calls.clear();
        g_dithered_box_calls.clear();
        g_box_calls.clear();

        g_anim_width = 20;
        g_anim_height = 10;
        g_anim_full_store_flag = 0;

        g_font_style = 3;
        g_font_spacing = -1;
        g_font_height = 8;
        g_font_vertical_spacing = 2;
        g_string_width = 6;
        std::memset(&g_font_color, 0, sizeof(g_font_color));
        std::memset(&g_font_highlight_color, 0, sizeof(g_font_highlight_color));
        std::memset(&g_font_special_color, 0, sizeof(g_font_special_color));

        g_mouse_pointer_x = 0;
        g_mouse_pointer_y = 0;
        g_mouse_pointer_offset = 0;
        g_mouse_button = 0;
        g_mouse_buffer_flag = 0;
        g_mouse_buffer_flag2 = 0;
        g_mouse_buffer_button = 0;
        g_mouse_buffer_x = 0;
        g_mouse_buffer_y = 0;
        g_mouse_button_sequence.clear();

        g_keyboard_status = 0;
        g_read_key_value = 0;
        g_scan_key_value = 0;
        g_scan_key_sequence.clear();
    }
}