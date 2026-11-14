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

    int16_t g_anim_width = 20;
    int16_t g_anim_height = 10;
    int16_t g_anim_full_store_flag = 0;

    int16_t g_font_style = 3;
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

        g_anim_width = 20;
        g_anim_height = 10;
        g_anim_full_store_flag = 0;

        g_font_style = 3;
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

        animate::clipping_flag = 0;

        video::screen_window_x1 = 0;
        video::screen_window_y1 = 0;
        video::screen_window_x2 = 639;
        video::screen_window_y2 = 479;

        platform::Reset_Exit();
        fields::Reset_Mouse_Wheel_Input();
        fields::Clear_Fields();
        fields::help_list_active = 0;
        fields::help_list_count = 0;
        fields::help_struct_pointer = nullptr;
        fields::mouse_cancel_disabled = 0;
        fields::field_box_mode = 0;
        fields::help_box_mode = 0;
        fields::draw_fields_flag = 1;
        fields::dynamic_field_refresh = 1;
        fields::mouse_auto_exit = 0;
        fields::draw_override_flag = 0;
        fields::selected_count = 0;
        fields::list_field_on = 0;
        fields::list_field_scan_flag = 1;
        fields::pressed_button_flag = 0;
        fields::last_button_number = 0;
        fields::last_button_x = 0;
        fields::last_button_y = 0;
    }
}

namespace fonts {
    uint16_t PSTR_style = 0;
    uint8_t* current_font_blob = nullptr;
    void* palette_block = nullptr;
    char font_name[80] = "";
    int16_t print_xpos = 0;
    int16_t print_ypos = 0;
    int16_t current_font_style = 0;
    uint8_t* font_data = nullptr;
    s_font font_header{};
    int16_t draw_alias_flag = 0;
    s_colors current_font_color_array{};
    uint16_t outline_color = 0;
    uint16_t alias_outline_color = 0;

    void __cdecl Print_Character_ASM(void* screen, const uint8_t* glyph_data, const uint8_t* selected_colors, uint32_t height) {
        (void)screen;
        (void)glyph_data;
        (void)selected_colors;
        (void)height;
    }

    void __cdecl Load_Palette(int16_t palette_id, int16_t start_index, int16_t end_index) {
        (void)palette_id;
        (void)start_index;
        (void)end_index;
    }

    void __cdecl Set_PSTR_Style(uint16_t style) {
        PSTR_style = style;
    }

    void __cdecl Set_Font_Style(int32_t font_index, const s_colors* new_colors) {
        test::fakes::g_font_style = static_cast<int16_t>(font_index);
        if (new_colors != nullptr) {
            test::fakes::g_font_color = *new_colors;
        }
    }

    void __cdecl Set_Remap_Font_Style(int32_t font_index, s_colors* colors) {
        Set_Font_Style(font_index, colors);
    }

    void __cdecl Load_Font_File(char* filename) {
        (void)filename;
    }

    int16_t __cdecl Get_String_Width(const char* str) {
        (void)str;
        return test::fakes::g_string_width;
    }

    int16_t __cdecl Print_Character(int16_t x, int16_t y, int8_t c) {
        (void)x;
        (void)y;
        (void)c;
        return 0;
    }

    void __cdecl Print_Integer(int16_t x, int16_t y, int32_t value) {
        (void)x;
        (void)y;
        (void)value;
    }

    void __cdecl Print_Character_To_Bitmap(int16_t x, int16_t y, char ch, s_animation_header* seg) {
        (void)x;
        (void)y;
        (void)ch;
        (void)seg;
    }

    void __cdecl Print_Clipped_Letter_To_Bitmap(int16_t x, int16_t y, char letter, int16_t clip_x, int16_t clip_width, int16_t clip_y, uint16_t clip_height, s_animation_header* anim) {
        (void)x;
        (void)y;
        (void)letter;
        (void)clip_x;
        (void)clip_width;
        (void)clip_y;
        (void)clip_height;
        (void)anim;
    }

    int16_t __cdecl Print_Character_No_Alias(int16_t x, int16_t y, char character) {
        (void)x;
        (void)y;
        (void)character;
        return 0;
    }

    void __cdecl Set_Normal_Colors_On(void) {
    }

    void __cdecl Set_Highlight_Colors_On(void) {
    }

    void __cdecl Set_Special_Colors_On(void) {
    }

    int16_t __cdecl Print_String(uint16_t x, int16_t y, const char* str, int16_t process_colors, int16_t justified_width) {
        (void)x;
        (void)y;
        (void)str;
        (void)process_colors;
        (void)justified_width;
        return 0;
    }

    void __cdecl Print_Display(int16_t x, int16_t y, const char* str, int16_t width) {
        (void)x;
        (void)y;
        (void)str;
        (void)width;
    }

    int16_t __cdecl Print_String_Clipped(int16_t x, int16_t y, const char* str, int16_t process_colors, int16_t justified_width) {
        (void)x;
        (void)y;
        (void)str;
        (void)process_colors;
        (void)justified_width;
        return 0;
    }

    void __cdecl Print_Clipped_Letter(int16_t x, int16_t y, int8_t letter, int16_t clip_left, int16_t clip_width, int16_t start_y, int16_t end_y) {
        (void)x;
        (void)y;
        (void)letter;
        (void)clip_left;
        (void)clip_width;
        (void)start_y;
        (void)end_y;
    }

    int16_t __cdecl Print_Character_Clipped(int16_t x, int16_t y, int8_t c) {
        (void)x;
        (void)y;
        (void)c;
        return 0;
    }

    void __cdecl Print(int16_t x, int16_t y, const char* str) {
        test::fakes::g_print_calls.push_back({x, y, str, str != nullptr ? std::string(str) : std::string()});
    }

    void __cdecl PSTRS(int16_t x, int16_t y, const char* str) {
        (void)x;
        (void)y;
        (void)str;
    }

    void __cdecl Set_Font_Style_Outline_Heavy(int16_t font_index, s_colors* style_data) {
        Set_Font_Style(font_index, style_data);
    }

    void __cdecl Set_Outline_Color(uint16_t color, uint16_t alias_color) {
        outline_color = color;
        alias_outline_color = alias_color;
    }

    void __cdecl Print_String_To_Bitmap(int16_t x, int16_t y, const char* string, int16_t color_flags, int16_t max_width, s_animation_header* bitmap) {
        (void)x;
        (void)y;
        (void)string;
        (void)color_flags;
        (void)max_width;
        (void)bitmap;
    }

    void __cdecl Print_Display_To_Bitmap(int16_t x, int16_t y, const char* str, int16_t max_width, s_animation_header* bitmap) {
        (void)x;
        (void)y;
        (void)str;
        (void)max_width;
        (void)bitmap;
    }

    void __cdecl Print_To_Bitmap(int16_t x, int16_t y, const char* text, void* bitmap) {
        (void)x;
        (void)y;
        (void)text;
        (void)bitmap;
    }

    void __cdecl Print_Centered_To_Bitmap(int16_t x, int16_t y, const char* text, s_animation_header* dest) {
        (void)x;
        (void)y;
        (void)text;
        (void)dest;
    }

    int16_t __cdecl Get_Current_Font_Style() {
        return test::fakes::g_font_style;
    }

    s_colors* __cdecl Get_Current_Font_Color() {
        return &test::fakes::g_font_color;
    }

    s_colors* __cdecl Get_Current_Highlight_Color() {
        return &test::fakes::g_font_highlight_color;
    }

    s_colors* __cdecl Get_Current_Special_Color() {
        return &test::fakes::g_font_special_color;
    }

    int16_t __cdecl Get_Font_Height() {
        return test::fakes::g_font_height;
    }

    uint16_t __cdecl Get_Font_Vertical_Spacing() {
        return test::fakes::g_font_vertical_spacing;
    }

    void __cdecl Set_Font_Style_Outline(int16_t style, s_colors* colors) {
        Set_Font_Style(style, colors);
    }

    void __cdecl Print_Centered(int16_t x, int16_t y, const char* text) {
        test::fakes::g_print_centered_calls.push_back({x, y, text, text != nullptr ? std::string(text) : std::string()});
    }

    void __cdecl Print_Integer_Centered(int16_t x, int16_t y, int32_t value) {
        (void)x;
        (void)y;
        (void)value;
    }

    void __cdecl Print_Right(int16_t x, int16_t y, const char* text) {
        (void)x;
        (void)y;
        (void)text;
    }

    void __cdecl Print_Integer_Right(int16_t x, int16_t y, int32_t value) {
        (void)x;
        (void)y;
        (void)value;
    }

    void __cdecl Set_Font_Spacing(int16_t spacing) {
        (void)spacing;
    }

    void __cdecl Set_Font_LF(uint16_t line_height) {
        (void)line_height;
    }

    void __cdecl Set_Font_Spacing_Width(int8_t spacing_width) {
        (void)spacing_width;
    }

    void __cdecl Get_Colors(s_colors* colors) {
        if (colors != nullptr) {
            *colors = test::fakes::g_font_color;
        }
    }

    void __cdecl Set_Font_Special_Colors(s_colors* colors) {
        if (colors != nullptr) {
            test::fakes::g_font_special_color = *colors;
        }
    }

    void __cdecl Set_Font_Highlight_Colors(s_colors* colors) {
        if (colors != nullptr) {
            test::fakes::g_font_highlight_color = *colors;
        }
    }

    void __cdecl Set_Font_Style_Shadow_Down(int16_t style, const s_colors* colors) {
        Set_Font_Style(style, colors);
    }

    void __cdecl Set_Font_Style_Shadow_Up(int16_t font_idx, s_colors* colors) {
        Set_Font_Style(font_idx, colors);
    }

    void __cdecl Set_Alias_Color(uint8_t color) {
        (void)color;
    }

    void __cdecl Disp_PSTR(int32_t x, int32_t y, const char* format, ...) {
        (void)x;
        (void)y;
        (void)format;
    }
}

namespace graphics {
    int16_t old_screen_window_y2 = 0;
    int16_t old_screen_window_y1 = 0;
    int16_t old_screen_window_x2 = 0;
    int16_t old_screen_window_x1 = 0;

    void Reset_Window() {
    }

    void __cdecl Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
        test::fakes::g_fill_calls.push_back({x1, y1, x2, y2, color});
    }

    void __cdecl Dithered_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t base, uint8_t speckle, uint8_t density) {
        test::fakes::g_dithered_fill_calls.push_back({x1, y1, x2, y2, base, speckle, density});
    }

    void __cdecl Darken_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
    }

    void __cdecl Set_Window(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
    }

    void __cdecl Save_Window() {
    }

    void __cdecl Restore_Window() {
    }

    void __cdecl Tint_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t tint_color) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        (void)tint_color;
    }

    void __cdecl Gray_Scale_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t* palette_map, int32_t gradient_scale, int32_t invert_flag) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        (void)palette_map;
        (void)gradient_scale;
        (void)invert_flag;
    }

    void __cdecl Gradient_Fill(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t mode, uint8_t* palette, int32_t gradient_scale, s_animation_header* pic, s_colors* colors) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        (void)mode;
        (void)palette;
        (void)gradient_scale;
        (void)pic;
        (void)colors;
    }

    void __cdecl Interlaced_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color1, int16_t color2) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        (void)color1;
        (void)color2;
    }

    void __cdecl Tri_Interlaced_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color1, int16_t color2, int16_t color3) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        (void)color1;
        (void)color2;
        (void)color3;
    }

    void __cdecl Box(int16_t x, int16_t y, int16_t width, int16_t height, int16_t thickness, uint8_t color) {
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        (void)thickness;
        (void)color;
    }

    void __cdecl Dot(int16_t x, int16_t y, uint8_t color) {
        (void)x;
        (void)y;
        (void)color;
    }
}

namespace line {
    void __cdecl Multi_Colored_Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, s_colors* colors, int16_t offset) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        (void)colors;
        (void)offset;
    }

    int16_t __cdecl Clip_Line(int16_t* x1, int16_t* y1, int16_t* x2, int16_t* y2) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        return 0;
    }

    void __cdecl Draw_Horizontal_Run_Multi(uint8_t** dest_ptr, int16_t step, int16_t length, s_colors* colors, uint16_t* color_index) {
        (void)dest_ptr;
        (void)step;
        (void)length;
        (void)colors;
        (void)color_index;
    }

    void __cdecl Draw_Vertical_Run_Multi(uint8_t** screen_ptr, int16_t post_offset, int16_t height, s_colors* colors, uint16_t* color_index) {
        (void)screen_ptr;
        (void)post_offset;
        (void)height;
        (void)colors;
        (void)color_index;
    }

    void __cdecl Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
        test::fakes::g_line_calls.push_back({x1, y1, x2, y2, color});
    }

    void __cdecl Draw_Vertical_Run(uint8_t** buf, int16_t post_offset, int16_t count, uint8_t color) {
        (void)buf;
        (void)post_offset;
        (void)count;
        (void)color;
    }

    void __cdecl Draw_Horizontal_Run(uint8_t** buf, int16_t step_x, int16_t count, uint8_t color) {
        (void)buf;
        (void)step_x;
        (void)count;
        (void)color;
    }

    void __cdecl Plasma_Box(int16_t x, int16_t y, uint16_t width, int16_t height, int16_t thickness, int16_t seed, uint8_t start_value, const uint8_t* color_map) {
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        (void)thickness;
        (void)seed;
        (void)start_value;
        (void)color_map;
    }

    void __cdecl Create_Plasma_Line(int16_t seed, uint16_t start_value, const uint8_t* color_map) {
        (void)seed;
        (void)start_value;
        (void)color_map;
    }

    void __cdecl Plasma_Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t* color_map, uint16_t color_index) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        (void)color_map;
        (void)color_index;
    }

    void __cdecl Draw_Horizontal_Run_Plasma(uint8_t** dest_ptr, int16_t step, int16_t length, uint8_t* plasma_table, uint16_t* plasma_index_ptr) {
        (void)dest_ptr;
        (void)step;
        (void)length;
        (void)plasma_table;
        (void)plasma_index_ptr;
    }

    void __cdecl Draw_Vertical_Run_Plasma(uint8_t** pixel_ptr_ref, int16_t post_offset, int16_t length, uint8_t* plasma_table, uint16_t* plasma_index_ptr) {
        (void)pixel_ptr_ref;
        (void)post_offset;
        (void)length;
        (void)plasma_table;
        (void)plasma_index_ptr;
    }
}

namespace timer {
    uint32_t time_mark = 0;

    void __cdecl Mark_Time() {
        test::fakes::g_mark_time_calls++;
    }

    int32_t __cdecl Get_Time_Since_Mark() {
        return 0;
    }

    void __cdecl Release_Time(int32_t ticks) {
        test::fakes::g_release_time_calls.push_back(ticks);
    }
}

namespace capture {
    uint16_t release_flag = 0;

    void Release_Version() {
        release_flag = 1;
    }

    void __cdecl Screen_Flic_Capture() {
    }

    uint16_t __cdecl Check_Release_Version() {
        return release_flag;
    }
}

namespace palette {
    s_palette_entry saved_palette[256]{};
    int32_t cycle_direction_flag = 0;
    int32_t cycle_step_value = 0;
    int32_t cycle_color_value = 0;

    void __cdecl Set_Palette_Gradient(int32_t start_index, int32_t count, int32_t start_r, int32_t start_g, int32_t start_b, int32_t end_r, int32_t end_g, int32_t end_b) {
        (void)start_index;
        (void)count;
        (void)start_r;
        (void)start_g;
        (void)start_b;
        (void)end_r;
        (void)end_g;
        (void)end_b;
    }

    void __cdecl Save_Palette() {
    }

    void __cdecl Set_Palette_Changes(int16_t start_index, int16_t end_index) {
        (void)start_index;
        (void)end_index;
    }

    void __cdecl Clear_Palette_Changes(int16_t start_index, int16_t end_index) {
        (void)start_index;
        (void)end_index;
    }

    void __cdecl Reset_Cycle_Palette_Color() {
    }

    void __cdecl Restore_Palette() {
    }

    void __cdecl Refresh_Palette() {
    }

    void __cdecl Cycle_Palette_Color(int16_t palette_index, int16_t start_r, int16_t start_g, int16_t start_b, int16_t end_r, int16_t end_g, int16_t end_b, int16_t step_value) {
        (void)palette_index;
        (void)start_r;
        (void)start_g;
        (void)start_b;
        (void)end_r;
        (void)end_g;
        (void)end_b;
        (void)step_value;
    }

    void __cdecl Update_Cycle(int16_t* start_value, int16_t* end_value) {
        (void)start_value;
        (void)end_value;
    }

    void __cdecl Save_Glass_Remap_Block(void* remap_buffer) {
        (void)remap_buffer;
    }

    void* __cdecl Allocate_Glass_Remap_Next_Block(void* current_block) {
        return current_block;
    }

    void __cdecl Set_Glass_Remap_Color(int16_t color_index, uint8_t red, uint8_t green, uint8_t blue, uint8_t intensity) {
        (void)color_index;
        (void)red;
        (void)green;
        (void)blue;
        (void)intensity;
    }

    void __cdecl Restore_Glass_Remap_Block(void* remap_buffer) {
        (void)remap_buffer;
    }

    void __cdecl RGB_To_HLS(int8_t red, int8_t green, int8_t blue, int32_t* hue, int32_t* lightness, int32_t* saturation) {
        (void)red;
        (void)green;
        (void)blue;
        if (hue != nullptr) *hue = 0;
        if (lightness != nullptr) *lightness = 0;
        if (saturation != nullptr) *saturation = 0;
    }

    void __cdecl HLS_To_RGB(int32_t hue, int32_t lightness, int32_t saturation, uint8_t* red, uint8_t* green, uint8_t* blue) {
        (void)hue;
        (void)lightness;
        (void)saturation;
        if (red != nullptr) *red = 0;
        if (green != nullptr) *green = 0;
        if (blue != nullptr) *blue = 0;
    }

    int32_t __cdecl Value(int32_t m1, int32_t m2, int32_t hue) {
        (void)m1;
        (void)m2;
        (void)hue;
        return 0;
    }
}

namespace platform {
    uint32_t g_os_ver = 0;
    uint32_t g_win_minor = 0;
    uint32_t g_win_major = 0;
    uint32_t g_win_ver = 0;
    char* g_command_line = nullptr;
    void* g_env_strings = nullptr;
    SDL_Thread* g_worker_thread_handle = nullptr;
    uint32_t updating_cursor_img = 0;
    char g_exit_message[k_exit_message_size] = "";
    void* g_main_window = nullptr;
    volatile bool g_pending_toggle_pages = false;
    int16_t g_video_mode = 0;
    uint32_t g_available_mem = 0;
    uint8_t* g_present_surface = nullptr;
    uint8_t* g_cursor_surface_1 = nullptr;
    uint8_t* cursor_surface = nullptr;
    uint32_t g_frame_count = 0;
    int8_t g_render_buffer = 0;
    uint8_t* offscreen_surface = nullptr;
    int32_t g_last_cursor_shape = 0;
    int16_t g_mouse_x = 0;
    int16_t g_mouse_y = 0;

    namespace {
        bool g_exit_requested = false;
        int g_exit_status = EXIT_SUCCESS;
        bool g_thread_exited = false;

        void Copy_Exit_Message(const char* message) {
            if (message == nullptr) {
                g_exit_message[0] = '\0';
                return;
            }

            strlcpy(g_exit_message, message, sizeof(g_exit_message));
        }
    }

    bool __cdecl Init_Exit() {
        Reset_Exit();
        return true;
    }

    void __cdecl Shutdown_Exit() {
        Reset_Exit();
    }

    void __cdecl Reset_Exit() {
        g_exit_requested = false;
        g_exit_status = EXIT_SUCCESS;
        g_thread_exited = false;
        g_exit_message[0] = '\0';
    }

    void __cdecl Request_Exit(const char* message) {
        if (!g_exit_requested) {
            Copy_Exit_Message(message);
            g_exit_requested = true;
        } else if (g_exit_message[0] == '\0' && message != nullptr && message[0] != '\0') {
            Copy_Exit_Message(message);
        }
    }

    void __cdecl Request_Fatal_Exit(const char* message) {
        if (!g_exit_requested || (message != nullptr && message[0] != '\0')) {
            Copy_Exit_Message(message);
        }
        g_exit_status = EXIT_FAILURE;
        g_exit_requested = true;
    }

    bool __cdecl Exit_Requested() {
        return g_exit_requested;
    }

    const char* __cdecl Exit_Message() {
        return g_exit_message;
    }

    int __cdecl Exit_Status() {
        return g_exit_status;
    }

    void __cdecl Check_Exit() {
        if (g_exit_requested) {
            throw process_exit_requested{};
        }
    }

    void __cdecl Mark_Game_Thread_Exited() {
        g_thread_exited = true;
    }

    bool __cdecl Game_Thread_Exited() {
        return g_thread_exited;
    }

    bool Create_Video_Buffers(int16_t video_mode) {
        (void)video_mode;
        return true;
    }

    void __cdecl Service_Pending_Window_Events() {
    }

    void __cdecl Pump_Game_Input_Queue() {
    }

    int16_t __cdecl Get_Mouse_X() {
        return 0;
    }

    int16_t __cdecl Get_Mouse_Y() {
        return 0;
    }

    void __cdecl Set_Logical_Mouse_Position(int16_t x, int16_t y) {
        (void)x;
        (void)y;
    }

    bool __cdecl Update_Active_World_Frame_() {
        return true;
    }

    uint32_t Lock_Appropriate_Render_Buffer() {
        return 0;
    }

    void Unlock_Appropriate_Render_Buffer() {
    }

    void Copy_Off_Page_To_Present_Surface() {
    }

    void Force_Cursor_Pos(int16_t x, int16_t y) {
        test::fakes::g_forced_cursor_pos_calls.push_back({x, y});
    }

    void __cdecl Create_Cursor_Surface() {
    }

    int32_t __cdecl Lock_Offscreen_Surface() {
        return 0;
    }

    void __cdecl Unlock_Offscreen_Surface() {
    }

    void Cleanup_Surfaces() {
    }

    void Destroy_Main_Window() {
    }

    void __cdecl Prepare_Exit() {
        Request_Exit();
    }

    void __cdecl Update_Cursor_Image() {
    }

    void __cdecl Toggle_Pages_Stub() {
    }

    int Run_Main_Loop(const char* command_line) {
        (void)command_line;
        return 0;
    }

    void __cdecl Copy_Dwords(uint32_t* destination, const uint32_t* source, uint32_t dword_count) {
        if (destination != nullptr && source != nullptr) {
            std::memcpy(destination, source, dword_count * sizeof(uint32_t));
        }
    }
}

void Query_Memory_Status(s_memorystatus* memory_status) {
    if (memory_status != nullptr) {
        std::memset(memory_status, 0, sizeof(*memory_status));
    }
}


int Show_Message_Box(void* window_handle, const char* text, const char* caption, bool is_error) {
    (void)window_handle;
    test::fakes::g_last_message_box_text = text != nullptr ? text : "";
    test::fakes::g_last_message_box_caption = caption != nullptr ? caption : "";
    test::fakes::g_last_message_box_is_error = is_error;
    return 0;
}