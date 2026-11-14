#include "pch.h"

namespace fonts {
    uint16_t PSTR_style;

    uint8_t* current_font_blob;

    void* palette_block;

    char font_name[80];

    int16_t print_xpos;
    int16_t print_ypos;

    int16_t current_font_style;

    uint8_t* font_data;

    s_font font_header;

    int16_t draw_alias_flag = 1;

    s_colors current_font_color_array;

    uint16_t outline_color;
    uint16_t alias_outline_color;

    void __cdecl Set_Normal_Colors_On(void) {
        for (int i = 0; i < 8; ++i) {
            font_header.selected_colors.colors[i] = font_header.normal_colors.colors[i];
        }
    }

    void __cdecl Set_Highlight_Colors_On(void) {
        for (int i = 0; i < 8; ++i) {
            font_header.selected_colors.colors[i] = font_header.highlight_colors.colors[i];
        }
    }

    void __cdecl Set_Special_Colors_On(void) {
        for (int i = 0; i < 8; ++i) {
            font_header.selected_colors.colors[i] = font_header.special_colors.colors[i];
        }
    }

    void __cdecl Print_Character_ASM(void* screen, const uint8_t* glyph_data, const uint8_t* selected_colors, uint32_t height) {
        uint8_t* current_row = (uint8_t*)screen;
        const uint8_t* data = glyph_data;
        uint32_t rows = height;

        do {
            uint8_t* dest = current_row;

            while (true) {
                uint8_t cmd = *data++;

                if (cmd == 0x80) {
                    break;
                }

                if (cmd < 0x80) {
                    *dest++ = selected_colors[cmd - 1];
                } else {
                    dest += (cmd & 0x7F);
                }
            }

            current_row += video::g_screen_res_x;
        } while (--rows != 0);
    }

    void Load_Palette(int16_t palette_id, int16_t start_index, int16_t end_index) {
        void* loaded_palette = farload::Far_Reload(font_name, (int32_t)palette_id + 1, palette_block);
        s_palette_entry* src = (s_palette_entry*)loaded_palette + start_index;
        s_palette_entry* dst = &video::current_palette[start_index];
        uint32_t byte_count = (uint32_t)(end_index - start_index + 1) * sizeof(*dst);

        struct_::Copy_Structure(dst, src, byte_count);

        video::mouse_palette = (uint8_t*)loaded_palette + sizeof(video::current_palette);

        // Signal that the specified color range has been modified.
        palette::Set_Palette_Changes(start_index, end_index);

        // Internal state update.
        platform::Update_Cursor_Image();
    }

    void Set_PSTR_Style(uint16_t style) {
        PSTR_style = style;
    }

    void __cdecl Set_Font_Spacing(int16_t spacing) {
        font_header.font_style_spacing = spacing;
    }

    void __cdecl Set_Font_LF(uint16_t line_height) {
        font_header.line_height = line_height;
        font_header.font_style_height = font_header.current_height + line_height;
    }

    void __cdecl Set_Font_Style_Outline_Heavy(int16_t font_index, s_colors* style_data) {
        Set_Font_Style((int32_t)font_index, style_data);
        font_header.font_style = 5;
    }

    void __cdecl Set_Outline_Color(uint16_t color, uint16_t alias_color) {
        outline_color = color;
        alias_outline_color = alias_color;
    }

    void __cdecl Set_Remap_Font_Style(int32_t font_index, s_colors* colors) {
        current_font_style = (int16_t)font_index;

        for (int i = 0; i < 8; i++) {
            uint8_t color = remap::picture_remap_color_list[colors->colors[i]];
            font_header.normal_colors.colors[i] = color;
            font_header.selected_colors.colors[i] = color;
            current_font_color_array.colors[i] = color;
        }

        font_header.current_height = font_header.font_heights[font_index];
        font_header.font_style_height = font_header.font_heights[font_index];
        font_header.font_style = 0;
        font_header.leading_offset = (uint16_t)font_header.leading_extra[font_index];
        font_header.font_style_spacing = font_header.font_spacing[font_index];
        font_header.line_height = font_header.font_style_height + font_header.leading_offset;

        for (int i = 0; i < 255; i++) {
            font_header.font_style_widths[i] = (char)font_header.font_glyph_widths[font_index][i];
            font_header.font_style_offsets[i] = font_header.font_glyph_offsets[font_index][i];
        }
    }

    void __cdecl Set_Font_Style(int32_t font_index, const s_colors* new_colors) {
        current_font_style = (uint16_t)font_index;

        for (int i = 0; i < 8; ++i) {
            uint8_t color = new_colors->colors[i];
            font_header.selected_colors.colors[i] = color;
            font_header.normal_colors.colors[i] = color;
            current_font_color_array.colors[i] = color;
        }

        font_header.current_height = font_header.font_heights[font_index];
        font_header.font_style_height = font_header.font_heights[font_index];
        font_header.font_style = 0;
        font_header.leading_offset = font_header.leading_extra[font_index];
        font_header.font_style_spacing = font_header.font_spacing[font_index];
        font_header.line_height = font_header.font_style_height + font_header.leading_offset;

        for (int i = 0; i < 255; ++i) {
            font_header.font_style_widths[i] = font_header.font_glyph_widths[font_index][i];
            font_header.font_style_offsets[i] = font_header.font_glyph_offsets[font_index][i];
        }
    }

    int16_t __cdecl Get_String_Width(const char* str) {
        int16_t total_width = 0;
        uint16_t i = 0;

        if (*str == '\0') {
            return 0;
        }

        while (true) {
            int8_t c = (int8_t)str[(int16_t)i];

            if (c == 0x15) {
                break;
            }

            if (c >= 0x20) {
                total_width = (int16_t)(total_width +
                    font_header.font_style_widths[(uint8_t)c] +
                    font_header.font_style_spacing);
            }

            ++i;
            if (str[(int16_t)i] == '\0') {
                break;
            }
        }

        return total_width;
    }

    int16_t __cdecl Print_String_Clipped(int16_t x, int16_t y, const char* str, int16_t process_colors, int16_t justified_width) {
        if (y > video::screen_window_y2) {
            return 1;
        }

        if (y + font_header.font_style_height < video::screen_window_y1) {
            return 1;
        }

        print_xpos = x;
        print_ypos = y;

        int32_t spaces_remainder = 0;
        int32_t spaces_processed = 0;
        int32_t space_extra_width = 0;

        if (justified_width != 0) {
            int32_t str_len = 0;
            int32_t spaces_count = 0;

            if (str[0] != '\0') {
                do {
                    char c = str[str_len];
                    if (c == '\r' || c == '\x14' || c == '\x19' || c == '\x15' || c == '\x1d') {
                        break;
                    }
                    if (c == ' ') {
                        spaces_count++;
                    }
                    str_len++;
                } while (str[str_len] != '\0');
            }

            if (spaces_count == 0) {
                justified_width = 0;
            }

            justified_width -= Get_String_Width(str);

            if (justified_width > 0) {
                spaces_remainder = justified_width % spaces_count;
                space_extra_width = justified_width / spaces_count;
            } else {
                justified_width = 0;
            }
        }

        int32_t str_idx = 0;
        while (true) {
            char c = str[str_idx];
            if (c == '\0') {
                break;
            }

            switch (c) {
                case '\x01':
                    if (process_colors != 0) {
                        Set_Normal_Colors_On();
                    }
                    break;
                case '\x02':
                case '\x04':
                    if (process_colors != 0) {
                        Set_Highlight_Colors_On();
                    }
                    break;
                case '\x03':
                    if (process_colors != 0) {
                        Set_Special_Colors_On();
                    }
                    break;
                case '\x08':
                    break;
                case '\r':
                case '\x14':
                    print_ypos += font_header.line_height;
                    print_xpos = x;
                    break;
                case '\x15':
                    return print_xpos;
                case '\x19':
                case '\x1d':
                    print_xpos = (int16_t)((uint16_t)x + (uint8_t)str[str_idx + 1]);
                    str_idx++;
                    break;
                default:
                    print_xpos = Print_Character_Clipped(print_xpos, print_ypos, c);
                    if (justified_width != 0 && c == ' ') {
                        print_xpos += (int16_t)space_extra_width;
                        if (spaces_processed < spaces_remainder) {
                            print_xpos++;
                        }
                        spaces_processed++;
                    }
                    break;
            }
            str_idx++;
        }

        return print_xpos;
    }

    int16_t __cdecl Print_String(uint16_t x, int16_t y, const char* str, int16_t process_colors, int16_t justified_width) {
        print_xpos = (int16_t)x;
        print_ypos = (int16_t)y;

        int32_t pixels_per_space = 0;
        int32_t remainder_pixels = 0;
        int32_t space_index = 0;

        if (justified_width != 0) {
            int32_t space_count = 0;
            int32_t k = 0;
            while (str[k] != '\0' && str[k] != '\r' && str[k] != 0x14 &&
                str[k] != 0x19 && str[k] != 0x15 && str[k] != 0x1D) {
                if (str[k] == ' ') {
                    space_count++;
                }
                k++;
            }

            if (space_count == 0) {
                justified_width = 0;
            }

            justified_width = (int16_t)(justified_width - Get_String_Width(str));

            if (justified_width < 1) {
                justified_width = 0;
            } else {
                pixels_per_space = justified_width / space_count;
                remainder_pixels = justified_width % space_count;
            }
        }

        int32_t i = 0;
        while (str[i] != '\0') {
            int8_t c = (int8_t)str[i];

            switch (c) {
                case '\x01':
                    if (process_colors != 0) Set_Normal_Colors_On();
                    break;
                case '\x02':
                    if (process_colors != 0) Set_Highlight_Colors_On();
                    break;
                case '\x03':
                    if (process_colors != 0) Set_Special_Colors_On();
                    break;
                case '\x04':
                    if (process_colors != 0) Set_Highlight_Colors_On();
                    break;
                case '\x0D': // '\r'
                case '\x14':
                    print_ypos += font_header.line_height;
                    print_xpos = x;
                    break;
                case '\x15':
                    return print_xpos;
                case '\x19':
                case '\x1D':
                    print_xpos = (int16_t)((uint16_t)x + (uint8_t)str[i + 1]);
                    i++;
                    break;
                case '\x08':
                    break;
                default:
                    if (draw_alias_flag == 0) {
                        print_xpos = Print_Character(print_xpos, print_ypos, c);
                    } else {
                        print_xpos = Print_Character_No_Alias(print_xpos, print_ypos, c);
                    }

                    if (justified_width != 0 && c == ' ') {
                        print_xpos += (int16_t)pixels_per_space;
                        if (space_index < remainder_pixels) {
                            print_xpos++;
                        }
                        space_index++;
                    }
                    break;
            }
            i++;
        }

        return print_xpos;
    }

    int16_t __cdecl Print_Character(int16_t x, int16_t y, int8_t c) {
        const int16_t advance = x + (int16_t)font_header.font_style_widths[(int16_t)c] + font_header.font_style_spacing;
        uint32_t res_x = (uint32_t)platform::Lock_Appropriate_Render_Buffer();
        video::g_screen_res_x = (int32_t)res_x;

        if (x < 0 ||
            y < 0 ||
            x + font_header.font_style_widths[(int16_t)c] > video::g_screen_res_x ||
            y + font_header.font_style_height > video::g_screen_res_y) {
            platform::Unlock_Appropriate_Render_Buffer();

            if (x >= video::g_screen_res_x ||
                y >= video::g_screen_res_y ||
                x + font_header.font_style_widths[(int16_t)c] <= 0 ||
                y + font_header.font_style_height <= 0) {
                return advance;
            }

            Print_Character_Clipped(x, y, c);
            return advance;
        }

        uint8_t* dest_ptr = (uint8_t*)video::current_video_page + ((int32_t)y * res_x) + x;

        uint32_t offset = font_header.font_style_offsets[(int16_t)c];
        uint8_t* glyph_ptr = (uint8_t*)font_data + offset;

        Print_Character_ASM(dest_ptr, glyph_ptr, font_header.selected_colors.colors, (uint32_t)font_header.current_height);

        platform::Unlock_Appropriate_Render_Buffer();

        return advance;
    }

    int16_t __cdecl Print_Character_No_Alias(int16_t x, int16_t y, char character) {
        const int16_t advance = x + (int16_t)font_header.font_style_widths[(int32_t)character] + font_header.font_style_spacing;
        video::g_screen_res_x = (int32_t)platform::Lock_Appropriate_Render_Buffer();

        int32_t char_idx = (int32_t)character;

        if (x < 0 ||
            y < 0 ||
            x + font_header.font_style_widths[char_idx] > video::g_screen_res_x ||
            y + font_header.font_style_height > video::g_screen_res_y) {
            platform::Unlock_Appropriate_Render_Buffer();

            if (x >= video::g_screen_res_x ||
                y >= video::g_screen_res_y ||
                x + font_header.font_style_widths[char_idx] <= 0 ||
                y + font_header.font_style_height <= 0) {
                return advance;
            }

            Print_Character_Clipped(x, y, (int8_t)character);
            return advance;
        }

        int32_t pixel_offset = (int32_t)y * (int32_t)video::g_screen_res_x + (int32_t)x;
        uint8_t* dest_ptr = (uint8_t*)video::current_video_page + pixel_offset;

        uint8_t* glyph_ptr = (uint8_t*)font_data + font_header.font_style_offsets[char_idx];

        fontdraw::Print_Character_No_Alias_ASM(
            dest_ptr,
            glyph_ptr,
            font_header.selected_colors.colors,
            (int)font_header.current_height);

        platform::Unlock_Appropriate_Render_Buffer();

        return advance;
    }

    void __cdecl Print_String_To_Bitmap(int16_t x, int16_t y, const char* string, int16_t color_flags, int16_t max_width, s_animation_header* bitmap) {
        print_xpos = x;
        print_ypos = y;

        int16_t string_width = Get_String_Width(string);
        int16_t bitmap_height = animate::Get_Height(bitmap);

        if (y > bitmap_height) return;
        if (y + font_header.font_style_height <= 0) return;

        int16_t bitmap_width = animate::Get_Width(bitmap);
        if (x > bitmap_width) return;
        if (x + string_width <= 0) return;

        int32_t space_index = 0;
        int32_t remainder = 0;
        int32_t quotient = 0;
        int16_t justify_diff = 0;

        if (max_width != 0) {
            int i = 0;
            int space_count = 0;
            if (string[0] != '\0') {
                do {
                    uint8_t c = (uint8_t)string[i];
                    if (c == 0x0D || c == 0x14 || c == 0x19 || c == 0x15 || c == 0x1D) {
                        break;
                    }
                    if (c == 0x20) {
                        space_count++;
                    }
                    i++;
                } while (string[i] != '\0');
            }

            int16_t justify_width = max_width;
            if (space_count == 0) {
                justify_width = 0;
            }

            justify_diff = justify_width - Get_String_Width(string);

            if (justify_diff <= 0) {
                justify_diff = 0;
            } else {
                remainder = justify_diff % space_count;
                quotient = justify_diff / space_count;
            }
        }

        uint8_t c = (uint8_t)string[0];
        if (c != '\0') {
            int index = 0;
            do {
                switch (c) {
                    case 1:
                        if (color_flags != 0) {
                            Set_Normal_Colors_On();
                        }
                        break;
                    case 2:
                    case 4:
                        if (color_flags != 0) {
                            Set_Highlight_Colors_On();
                        }
                        break;
                    case 3:
                        if (color_flags != 0) {
                            Set_Special_Colors_On();
                        }
                        break;
                    case 8:
                        break;
                    case 0x0D:
                    case 0x14:
                    case 0x15:
                    case 0x19:
                    case 0x1D:
                        return;
                    default:
                        Print_Character_To_Bitmap(print_xpos, print_ypos, (char)c, bitmap);
                        print_xpos += (int16_t)font_header.font_style_widths[c] + font_header.font_style_spacing;
                        if (justify_diff != 0 && c == 0x20) {
                            print_xpos += (int16_t)quotient;
                            if (space_index < remainder) {
                                print_xpos++;
                            }
                            space_index++;
                        }
                        break;
                }
                index++;
                c = (uint8_t)string[index];
            } while (c != '\0');
        }
    }

    void __cdecl Print_Display_To_Bitmap(int16_t x, int16_t y, const char* str, int16_t max_width, s_animation_header* bitmap) {
        animate::animation_header = bitmap;

        if ((bitmap->flags & ANIMATION_FLAG_DRAW_MODE_MASK) != ANIMATION_DRAW_MODE_BITMAP) {
            exitm::Exit_With_Message("Print only works on bitmaps");
        }

        if (font_header.font_style != 0) {
            font_header.selected_colors.colors[0] = (uint8_t)alias_outline_color;
            draw_alias_flag = 1;

            for (int i = 1; i < 8; ++i) {
                font_header.selected_colors.colors[i] = (uint8_t)outline_color;
            }

            if (font_header.font_style != 2) {
                Print_String_To_Bitmap(x + 1, y + 1, str, 0, max_width, bitmap);
                Print_String_To_Bitmap(x, y + 1, str, 0, max_width, bitmap);
                Print_String_To_Bitmap(x + 1, y, str, 0, max_width, bitmap);
            }

            if (font_header.font_style != 1 && font_header.font_style != 3) {
                Print_String_To_Bitmap(x - 1, y, str, 0, max_width, bitmap);
                Print_String_To_Bitmap(x - 1, y - 1, str, 0, max_width, bitmap);
                Print_String_To_Bitmap(x, y - 1, str, 0, max_width, bitmap);
            }

            if (font_header.font_style == 3 || font_header.font_style == 5) {
                Print_String_To_Bitmap(x + 2, y + 2, str, 0, max_width, bitmap);
                Print_String_To_Bitmap(x + 1, y + 2, str, 0, max_width, bitmap);
                Print_String_To_Bitmap(x + 2, y + 1, str, 0, max_width, bitmap);
            }

            if (font_header.font_style > 3) {
                Print_String_To_Bitmap(x + 1, y - 1, str, 0, max_width, bitmap);
                Print_String_To_Bitmap(x - 1, y + 1, str, 0, max_width, bitmap);
            }

            if (font_header.font_style == 5) {
                Print_String_To_Bitmap(x + 2, y, str, 0, max_width, bitmap);
                Print_String_To_Bitmap(x, y + 2, str, 0, max_width, bitmap);
            }

            for (int i = 0; i < 8; ++i) {
                font_header.selected_colors.colors[i] = font_header.normal_colors.colors[i];
            }
        }

        draw_alias_flag = 0;
        Print_String_To_Bitmap(x, y, str, 1, max_width, bitmap);
    }

    void __cdecl Print_Character_To_Bitmap(int16_t x, int16_t y, char ch, s_animation_header* bitmap) {
        if ((int16_t)(int8_t)ch > 0x80) {
            return;
        }

        int16_t font_h = font_header.font_style_height;
        int16_t glyph_w = (int16_t)font_header.font_style_widths[(int16_t)(int8_t)ch];

        int16_t anim_w = animate::Get_Width(bitmap);
        int16_t anim_h = animate::Get_Height(bitmap);

        int16_t max_x_idx = anim_w - 1;
        int16_t max_y_idx = anim_h - 1;

        int16_t src_x = 0;
        int16_t draw_x = x;
        int16_t clipped_w = glyph_w;

        if (draw_x < 0) {
            src_x = -draw_x;
            if (src_x >= glyph_w) {
                return;
            }
            draw_x = 0;
            clipped_w = glyph_w - src_x;
        }

        if ((int)draw_x + (int)clipped_w > (int)max_x_idx) {
            clipped_w = max_x_idx - draw_x + 1;
            if (clipped_w < 1) {
                return;
            }
        }

        int16_t src_y = 0;
        int16_t draw_y = y;

        if (draw_y < 0) {
            src_y = -draw_y;
            draw_y = 0;
        }

        int16_t clipped_h = font_h;
        if ((int)draw_y + (int)clipped_h > (int)max_y_idx) {
            clipped_h = max_y_idx - draw_y + 1;
        }

        Print_Clipped_Letter_To_Bitmap(draw_x, draw_y, ch, src_x, clipped_w, src_y, clipped_h, bitmap);
    }

    void __cdecl Print_To_Bitmap(int16_t x, int16_t y, const char* text, void* bitmap) {
        Print_Display_To_Bitmap(x, y, text, 0, (s_animation_header*)bitmap);
    }

    void __cdecl Print_Display(int16_t x, int16_t y, const char* str, int16_t width) {
        int32_t offset = 0;

        if (font_header.font_style != 0) {
            for (int i = 1; i < 8; ++i) {
                font_header.selected_colors.colors[i] = (uint8_t)outline_color;
            }
            font_header.selected_colors.colors[0] = (uint8_t)alias_outline_color;
            draw_alias_flag = 1;

            if (font_header.font_style == 3 || font_header.font_style == 5) {
                offset = 2;
            } else {
                offset = 1;
            }

            if (font_header.font_style != 2) {
                if (animate::clipping_flag == 1) {
                    Print_String_Clipped(x + 1, y + 1, str, 0, width);
                    Print_String_Clipped(x, y + 1, str, 0, width);
                    Print_String_Clipped(x + 1, y, str, 0, width);
                } else {
                    Print_String(x + 1, y + 1, str, 0, width);
                    Print_String(x, y + 1, str, 0, width);
                    Print_String(x + 1, y, str, 0, width);
                }
            }

            if (font_header.font_style != 1 && font_header.font_style != 3) {
                if (animate::clipping_flag == 1) {
                    Print_String_Clipped(x - 1, y, str, 0, width);
                    Print_String_Clipped(x - 1, y - 1, str, 0, width);
                    Print_String_Clipped(x, y - 1, str, 0, width);
                } else {
                    Print_String(x - 1, y, str, 0, width);
                    Print_String(x - 1, y - 1, str, 0, width);
                    Print_String(x, y - 1, str, 0, width);
                }
            }

            if (font_header.font_style == 3 || font_header.font_style == 5) {
                if (animate::clipping_flag == 1) {
                    Print_String_Clipped(x + 2, y + 2, str, 0, width);
                    Print_String_Clipped(x + 1, y + 2, str, 0, width);
                    Print_String_Clipped(x + 2, y + 1, str, 0, width);
                } else {
                    Print_String(x + 2, y + 2, str, 0, width);
                    Print_String(x + 1, y + 2, str, 0, width);
                    Print_String(x + 2, y + 1, str, 0, width);
                }
            }

            if (font_header.font_style > 3) {
                if (animate::clipping_flag == 1) {
                    Print_String_Clipped(x + 1, y - 1, str, 0, width);
                    Print_String_Clipped(x - 1, y + 1, str, 0, width);
                } else {
                    Print_String(x + 1, y - 1, str, 0, width);
                    Print_String(x - 1, y + 1, str, 0, width);
                }
            }

            if (font_header.font_style == 5) {
                if (animate::clipping_flag == 1) {
                    Print_String_Clipped(x + 2, y, str, 0, width);
                    Print_String_Clipped(x, y + 2, str, 0, width);
                } else {
                    Print_String(x + 2, y, str, 0, width);
                    Print_String(x, y + 2, str, 0, width);
                }
            }

            for (int i = 0; i < 8; ++i) {
                font_header.selected_colors.colors[i] = font_header.normal_colors.colors[i];
            }
        }

        draw_alias_flag = 0;

        if (animate::clipping_flag == 1) {
            Print_String_Clipped(x, y, str, 1, width);
        } else {
            Print_String(x, y, str, 1, width);
        }

        int32_t x_start = x - (int16_t)offset;
        int32_t y_start = y - (int16_t)offset;

        int32_t w_val = width;
        if (w_val == 0) {
            w_val = Get_String_Width(str);
        }

        int32_t x_end = x + w_val + (int16_t)offset;
        int32_t y_end = y + font_header.font_style_height + (int16_t)offset;

        if (x_start < 0) {
            x_start = 0;
        }
        if (y_start < 0) {
            y_start = 0;
        }

        if (x_end >= video::g_screen_res_x) {
            x_end = video::g_screen_res_x - 1;
        }
        if (y_end >= video::g_screen_res_y) {
            y_end = video::g_screen_res_y - 1;
        }

        if (video::screen_refresh_flag >= 2) {
            setvesa::Add_Square_To_Scan_List(x_start, y_start, x_end, y_end);
        }
    }

    void __cdecl Print(int16_t x, int16_t y, const char* text) {
        Print_Display(x, y, text, 0);
    }

    void __cdecl Print_Centered(int16_t x, int16_t y, const char* str) {
        int16_t width = Get_String_Width(str);
        Print((int16_t)(x - (width / 2)), y, str);
    }

    void __cdecl Print_Right(int16_t x, int16_t y, const char* text) {
        int16_t width = Get_String_Width(text);
        int16_t adjusted_x = (int16_t)(x - (width - 1));
        Print(adjusted_x, y, text);
    }

    void __cdecl Get_Colors(s_colors* colors) {
        for (int i = 0; i < 8; ++i) {
            colors->colors[i] = font_header.selected_colors.colors[i];
        }
    }

    int16_t __cdecl Get_Current_Font_Style() {
        return current_font_style;
    }

    s_colors* __cdecl Get_Current_Font_Color() {
        return &current_font_color_array;
    }

    s_colors* __cdecl Get_Current_Highlight_Color() {
        return &font_header.highlight_colors;
    }

    s_colors* __cdecl Get_Current_Special_Color() {
        return &font_header.special_colors;
    }

    int16_t __cdecl Get_Font_Height() {
        return font_header.font_style_height;
    }

    void __cdecl Get_String_Ink_Rows(const char* str, int16_t* top,
        int16_t* bottom) {
        int16_t ink_top = -1;
        int16_t ink_bottom = -1;
        if (str != nullptr && font_data != nullptr) {
            const int16_t height = font_header.current_height;
            for (const char* p = str; *p != '\0'; ++p) {
                const int16_t char_idx = (int16_t)(uint8_t)*p;
                const uint8_t* data =
                    font_data + font_header.font_style_offsets[char_idx];
                // A glyph is a row stream: each row is bytes until
                // 0x80. A byte below 0x80 (and not 0, which is
                // transparent) draws a pixel; a byte above 0x80
                // skips pixels. This mirrors Print_Character_ASM.
                for (int16_t row = 0; row < height; ++row) {
                    bool row_has_ink = false;
                    while (true) {
                        const uint8_t cmd = *data++;
                        if (cmd == 0x80) {
                            break;
                        }
                        if (cmd != 0 && cmd < 0x80) {
                            row_has_ink = true;
                        }
                    }
                    if (row_has_ink) {
                        if (ink_top < 0) {
                            ink_top = row;
                        }
                        ink_bottom = row;
                    }
                }
            }
        }
        if (top != nullptr) {
            *top = ink_top;
        }
        if (bottom != nullptr) {
            *bottom = ink_bottom;
        }
    }

    uint16_t __cdecl Get_Font_Vertical_Spacing() {
        return font_header.leading_offset;
    }

    void __cdecl Set_Font_Style_Outline(int16_t style, s_colors* colors) {
        Set_Font_Style(style, colors);
        font_header.font_style = 4;
    }

    void __cdecl Set_Font_Style_Shadow_Down(int16_t style, const s_colors* colors) {
        Set_Font_Style(style, colors);
        font_header.font_style = 1;
    }

    void __cdecl Set_Font_Style_Shadow_Up(int16_t font_idx, s_colors* colors) {
        Set_Font_Style(font_idx, colors);
        font_header.font_style = 2;
    }

    void __cdecl Set_Font_Special_Colors(s_colors* colors) {
        for (int i = 0; i < 8; ++i) {
            font_header.special_colors.colors[i] = colors->colors[i];
        }
    }

    void __cdecl Set_Font_Highlight_Colors(s_colors* colors) {
        for (int i = 0; i < 8; ++i) {
            font_header.highlight_colors.colors[i] = colors->colors[i];
        }
    }

    void __cdecl Print_Integer(int16_t x, int16_t y, int32_t value) {
        char buffer[12];

        snprintf(buffer, sizeof(buffer), "%i", value);
        Print(x, y, buffer);
    }

    void __cdecl Print_Integer_Right(int16_t x, int16_t y, int32_t value) {
        char buffer[12];
        snprintf(buffer, sizeof(buffer), "%i", value);
        Print_Right(x, y, buffer);
    }

    void __cdecl Print_Integer_Centered(int16_t x, int16_t y, int32_t value) {
        char buffer[12];

        snprintf(buffer, sizeof(buffer), "%i", value);
        Print_Centered(x, y, buffer);
    }

    void __cdecl Set_Font_Spacing_Width(int8_t spacing_width) {
        // Sets the width of the space character (ASCII 32)
        font_header.font_style_widths[32] = spacing_width;
    }

    void __cdecl Set_Alias_Color(uint8_t color) {
        font_header.normal_colors.colors[0] = (uint8_t)color;
        font_header.highlight_colors.colors[0] = (uint8_t)color;
        font_header.special_colors.colors[0] = (uint8_t)color;
        font_header.selected_colors.colors[0] = (uint8_t)color;
    }

    void __cdecl Print_Centered_To_Bitmap(int16_t x, int16_t y, const char* text, s_animation_header* bitmap) {
        int16_t width = Get_String_Width(text);
        Print_To_Bitmap(x - (width / 2), y, text, bitmap);
    }

    void __cdecl PSTRS(int16_t x, int16_t y, const char* str) {
        Disp_PSTR(x, y, str);
    }

    void __cdecl Load_Font_File(char* filename) {
        uint8_t* font_blob;

        if (current_font_blob == nullptr) {
            font_blob = (uint8_t*)farload::Farload(filename, 0);
            palette_block = allocate::Allocate_Space(0x3164);
        } else {
            free(current_font_blob);
            current_font_blob = nullptr;
            font_blob = (uint8_t*)farload::Farload(filename, 0);
        }

        current_font_blob = font_blob;

        struct_::Copy_Structure(video::glass_colors, font_blob, sizeof(video::glass_colors));
        font_blob += sizeof(video::glass_colors);

        struct_::Copy_Structure(&font_header, font_blob, sizeof(font_header));
        font_blob += sizeof(font_header);

        font_data = font_blob;

        strlcpy(font_name, filename, sizeof(font_name));
    }

    void __cdecl Disp_PSTR(int32_t x, int32_t y, const char* format, ...) {
        char buffer[100];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        Print_Display((int16_t)x, (int16_t)y, buffer, 0);
    }

    void __cdecl Print_Clipped_Letter_To_Bitmap(int16_t x, int16_t y, char letter, int16_t clip_x, int16_t clip_width, int16_t clip_y, uint16_t clip_height, s_animation_header* bitmap) {
        animate::animation_header = bitmap;

        int16_t current_frame = bitmap->current_frame;
        int16_t width = bitmap->width;
        int32_t dest_offset = x + y * width - clip_x;

        int16_t font_offset = (int16_t)font_header.font_style_offsets[(int8_t)letter];

        if ((int16_t)font_header.current_height < (int16_t)clip_height) {
            clip_height = font_header.current_height;
        }

        for (int16_t i = 0; i < clip_y; i++) {
            while (font_data[font_offset] != 0x80) {
                font_offset++;
            }
            font_offset++;
        }

        int16_t rows = clip_height - clip_y;

        if (rows > 0) {
            uint8_t* frame_ptr = Animation_Frame_Data_(bitmap, current_frame);
            do {
                int32_t dest_clip_start = clip_x + dest_offset;
                int32_t dest_clip_end = dest_clip_start + clip_width;

                uint8_t data = font_data[font_offset];
                if (data != 0x80) {
                    int32_t current_dest_offset = dest_offset;
                    do {
                        if (data <= 0x80) {
                            if (data != 0 && dest_clip_start <= current_dest_offset && current_dest_offset < dest_clip_end) {
                                frame_ptr[current_dest_offset] = font_header.selected_colors.colors[data - 1];
                            }
                            current_dest_offset++;
                        } else {
                            current_dest_offset += (data & 0x7F);
                        }
                        font_offset++;
                        data = font_data[font_offset];
                    } while (data != 0x80);
                }
                font_offset++;
                dest_offset += width;
                rows--;
            } while (rows != 0);
        }
    }

    void __cdecl Print_Clipped_Letter(int16_t x, int16_t y, int8_t letter, int16_t clip_left, int16_t clip_width, int16_t start_y, int16_t end_y) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        int screen_idx_start = y * video::g_screen_res_x - clip_left + x;

        if ((int16_t)font_header.current_height < end_y) {
            end_y = (int16_t)font_header.current_height;
        }

        int16_t offset = (int16_t)font_header.font_style_offsets[(int16_t)letter];

        for (int i = 0; i < start_y; ++i) {
            while (font_data[offset] != 0x80) {
                ++offset;
            }
            ++offset;
        }

        int16_t rows_to_draw = end_y - start_y;
        if (rows_to_draw > 0) {
            for (int16_t row = 0; row < rows_to_draw; ++row) {
                int screen_idx = screen_idx_start;
                int clip_start_idx = screen_idx_start + clip_left;
                int clip_end_idx = clip_start_idx + clip_width;

                uint8_t pixel = font_data[offset];
                if (pixel != 0x80) {
                    do {
                        if (pixel > 0x80) {
                            screen_idx += (pixel & 0x7F);
                        } else {
                            if (screen_idx >= clip_start_idx && screen_idx < clip_end_idx) {
                                video::current_video_page[screen_idx] = font_header.selected_colors.colors[pixel - 1];
                            }
                            ++screen_idx;
                        }
                        ++offset;
                        pixel = font_data[offset];
                    } while (pixel != 0x80);
                }
                ++offset;
                screen_idx_start += video::g_screen_res_x;
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    int16_t __cdecl Print_Character_Clipped(int16_t x, int16_t y, int8_t c) {
        int16_t spacing = font_header.font_style_spacing;

        if ((int16_t)c > 0x80) {
            return x;
        }

        int16_t char_width = (int16_t)font_header.font_style_widths[(int16_t)c];

        if (x >= video::screen_window_x1 &&
            x + char_width <= video::screen_window_x2 &&
            y >= video::screen_window_y1 &&
            y + font_header.font_style_height <= video::screen_window_y2) {
            Print_Character(x, y, c);
        } else {
            int16_t orig_ret = spacing + char_width + x;
            int16_t clip_x = 0;

            if (x < video::screen_window_x1) {
                clip_x = video::screen_window_x1 - x;
                if (char_width <= clip_x) {
                    return orig_ret;
                }
                char_width -= clip_x;
                x = video::screen_window_x1;
            }

            if (x + char_width > video::screen_window_x2) {
                char_width = video::screen_window_x2 - x + 1;
                if (char_width < 1) {
                    return orig_ret;
                }
            }

            int16_t clip_y = 0;
            if (y < video::screen_window_y1) {
                clip_y = video::screen_window_y1 - y;
                y = video::screen_window_y1;
            }

            int16_t print_height = font_header.font_style_height;
            if (y + font_header.font_style_height > video::screen_window_y2) {
                print_height = video::screen_window_y2 - y + 1;
            }

            Print_Clipped_Letter(x, y, c, clip_x, char_width, clip_y, print_height);
        }

        return spacing + char_width + x;
    }
} // namespace fonts