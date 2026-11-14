#include "pch.h"

namespace graphics {

    int16_t old_screen_window_y2;
    int16_t old_screen_window_y1;
    int16_t old_screen_window_x2;
    int16_t old_screen_window_x1;

    void __cdecl Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        if (x2 < x1) {
            strings::Swap_Short(&x1, &x2);
        }
        if (y2 < y1) {
            strings::Swap_Short(&y1, &y2);
        }

        const int16_t max_x = (int16_t)(video::g_screen_res_x - 1);
        const int16_t max_y = (int16_t)(video::g_screen_res_y - 1);

        if (x1 > max_x || x2 < 0 || y1 > max_y || y2 < 0) {
            platform::Unlock_Appropriate_Render_Buffer();
            return;
        }

        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > max_x) x2 = max_x;
        if (y2 > max_y) y2 = max_y;

        if (animate::clipping_flag == 1) {
            if (x1 >= video::screen_window_x2 || x2 <= video::screen_window_x1 ||
                y1 >= video::screen_window_y2 || y2 <= video::screen_window_y1) {
                platform::Unlock_Appropriate_Render_Buffer();
                return;
            }

            if (x1 < video::screen_window_x1) x1 = video::screen_window_x1;
            if (y1 < video::screen_window_y1) y1 = video::screen_window_y1;
            if (x2 > video::screen_window_x2) x2 = video::screen_window_x2;
            if (y2 > video::screen_window_y2) y2 = video::screen_window_y2;
        }

        if (video::screen_refresh_flag >= 2) {
            setvesa::Add_Square_To_Scan_List(x1, y1, x2, y2);
        }

        int32_t left_block = x1 / 4;
        int32_t right_block = x2 / 4;

        if (right_block == left_block) {
            uint8_t* surface = (uint8_t*)(video::current_video_page + (y1 * video::g_screen_res_x) + x1);
            regions::Fill_Column(color, surface, (x2 - x1) + 1, (y2 - y1) + 1);
        } else {
            uint32_t left_partial_width = 4 - (x1 & 3);
            uint32_t right_partial_width = (x2 & 3) + 1;
            int32_t mid_blocks = (right_block - left_block) - 1;

            if (left_partial_width == 4) {
                left_partial_width = 0;
                mid_blocks++;
            }
            if (right_partial_width == 4) {
                right_partial_width = 0;
                mid_blocks++;
            }

            int32_t y_offset = y1 * video::g_screen_res_x;
            uint32_t height = (y2 - y1) + 1;

            if (left_partial_width == 0) {
                if (mid_blocks != 0) {
                    uint8_t* surface = (uint8_t*)(video::current_video_page + (left_block * 4) + y_offset);
                    regions::Fill_Region(color, surface, (uint8_t)mid_blocks, height);
                }
            } else {
                uint8_t* col_surface = (uint8_t*)(video::current_video_page + y_offset + x1);
                regions::Fill_Column(color, col_surface, left_partial_width, height);

                uint8_t* reg_surface = (uint8_t*)(video::current_video_page + (left_block * 4) + y_offset + 4);
                regions::Fill_Region(color, reg_surface, (uint8_t)mid_blocks, height);
            }

            if (right_partial_width != 0) {
                uint8_t* surface = (uint8_t*)(video::current_video_page + (right_block * 4) + y_offset);
                regions::Fill_Column(color, surface, right_partial_width, height);
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void Reset_Window() {
        video::screen_window_x1 = 0;
        video::screen_window_y1 = 0;
        video::screen_window_x2 = (short)video::g_screen_res_x + -1;
        video::screen_window_y2 = (short)video::g_screen_res_y + -1;
        return;
    }

    void __cdecl Set_Window(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        if (x1 < 0) {
            x1 = 0;
        }

        if (y1 < 0) {
            y1 = 0;
        }

        if ((int32_t)x2 >= video::g_screen_res_x) {
            x2 = (int16_t)(video::g_screen_res_x - 1);
        }

        if ((int32_t)y2 >= video::g_screen_res_y) {
            y2 = (int16_t)(video::g_screen_res_y - 1);
        }

        video::screen_window_x1 = x1;
        video::screen_window_y1 = y1;
        video::screen_window_x2 = x2;
        video::screen_window_y2 = y2;
    }

    void __cdecl Save_Window() {
        old_screen_window_x1 = video::screen_window_x1;
        old_screen_window_y1 = video::screen_window_y1;
        old_screen_window_x2 = video::screen_window_x2;
        old_screen_window_y2 = video::screen_window_y2;
    }

    void __cdecl Restore_Window() {
        video::screen_window_x1 = old_screen_window_x1;
        video::screen_window_y1 = old_screen_window_y1;
        video::screen_window_x2 = old_screen_window_x2;
        video::screen_window_y2 = old_screen_window_y2;
    }

    void __cdecl Darken_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        Tint_Fill(x1, y1, x2, y2, 0);
    }

    void __cdecl Box(int16_t x, int16_t y, int16_t width, int16_t height, int16_t thickness, uint8_t color) {
        if (thickness <= 0) {
            return;
        }

        for (int32_t i = 0; i < (int32_t)thickness; ++i) {
            int16_t offset = (int16_t)i;
            int16_t x1 = x + offset;
            int16_t y1 = y + offset;
            int16_t x2 = x + width - offset - 1;
            int16_t y2 = y + height - offset - 1;

            line::Line(x1, y1, x2, y1, color);
            line::Line(x1, y1, x1, y2, color);
            line::Line(x2, y1, x2, y2, color);
            line::Line(x1, y2, x2, y2, color);
        }
    }

    void __cdecl Dithered_Box(int16_t x, int16_t y, int16_t width,
        int16_t height, int16_t thickness, uint8_t base, uint8_t speckle,
        uint8_t density) {
        // A border with the colony's bitmap feel: the same
        // 1 px outline Box draws, but each side is a
        // Dithered_Fill, so the border carries the colony's
        // irregular speckle (~10-15% of a neighbouring shade)
        // instead of reading as a smooth flat line. The
        // speckle is screen-space, so the four sides share one
        // continuous pattern.
        if (thickness <= 0) {
            return;
        }

        for (int32_t i = 0; i < (int32_t)thickness; ++i) {
            int16_t offset = (int16_t)i;
            int16_t x1 = x + offset;
            int16_t y1 = y + offset;
            int16_t x2 = x + width - offset - 1;
            int16_t y2 = y + height - offset - 1;

            Dithered_Fill(x1, y1, x2, y1, base, speckle, density);
            Dithered_Fill(x1, y1, x1, y2, base, speckle, density);
            Dithered_Fill(x2, y1, x2, y2, base, speckle, density);
            Dithered_Fill(x1, y2, x2, y2, base, speckle, density);
        }
    }

    void __cdecl Dot(int16_t x, int16_t y, uint8_t color) {
        if (animate::clipping_flag == 1) {
            if (x < video::screen_window_x1 || y < video::screen_window_y1 ||
                x > video::screen_window_x2 || y > video::screen_window_y2) {
                return;
            }
        }

        video::g_screen_res_x = (int32_t)platform::Lock_Appropriate_Render_Buffer();

        if (video::screen_refresh_flag >= 2) {
            video::Add_Dot_To_Scan_List((int32_t)x, (int32_t)y);
        }

        uint8_t* framebuffer = video::current_video_page;
        int32_t offset = static_cast<int32_t>(y) * video::g_screen_res_x + static_cast<int32_t>(x);
        framebuffer[offset] = color;

        platform::Unlock_Appropriate_Render_Buffer();
    }

    // Irregular speckle threshold: a deterministic integer hash of
    // the absolute screen coordinates, uniform over 0..15. Used by
    // Dithered_Fill so the speckle has no repeat distance, like the
    // colony's baked speckle (see Dithered_Fill).
    static uint8_t Speckle_Threshold(int16_t x, int16_t y) {
        uint32_t h = (uint32_t)((uint32_t)(uint16_t)x * 374761393u +
            (uint32_t)(uint16_t)y * 668265263u);
        h = (h ^ (h >> 13)) * 1274126177u;
        h = h ^ (h >> 16);
        return (uint8_t)(h & 15);
    }

    void __cdecl Dithered_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
        uint8_t base, uint8_t speckle, uint8_t density) {
        // The speckle threshold is a deterministic hash of the
        // absolute screen coordinates, so the pattern is
        // irregular -- no repeat distance -- like the colony's
        // baked speckle: the 4x4 Bayer grid it replaces has
        // autocorrelation 1.00 at lag 4, a visible repeating
        // pattern, while the colony's panel interior has no
        // lag-4 peak (its autocorrelation decays smoothly to
        // ~0 by lag 8). A speckle lands where the hash is below
        // the density, so density 5 is 5/16 speckles (~31%),
        // 6 is 6/16 (~38%), 2 is 2/16 (~13%). Screen-space,
        // so the pattern is continuous across adjacent rects.
        if (density == 0) {
            Fill(x1, y1, x2, y2, base);
            return;
        }
        if (density >= 16) {
            Fill(x1, y1, x2, y2, speckle);
            return;
        }
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        if (x2 < x1) {
            strings::Swap_Short(&x1, &x2);
        }
        if (y2 < y1) {
            strings::Swap_Short(&y1, &y2);
        }

        const int16_t max_x = (int16_t)(video::g_screen_res_x - 1);
        const int16_t max_y = (int16_t)(video::g_screen_res_y - 1);

        if (x1 > max_x || x2 < 0 || y1 > max_y || y2 < 0) {
            platform::Unlock_Appropriate_Render_Buffer();
            return;
        }

        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > max_x) x2 = max_x;
        if (y2 > max_y) y2 = max_y;

        if (animate::clipping_flag == 1) {
            if (x1 >= video::screen_window_x2 || x2 <= video::screen_window_x1 ||
                y1 >= video::screen_window_y2 || y2 <= video::screen_window_y1) {
                platform::Unlock_Appropriate_Render_Buffer();
                return;
            }

            if (x1 < video::screen_window_x1) x1 = video::screen_window_x1;
            if (y1 < video::screen_window_y1) y1 = video::screen_window_y1;
            if (x2 > video::screen_window_x2) x2 = video::screen_window_x2;
            if (y2 > video::screen_window_y2) y2 = video::screen_window_y2;
        }

        if (video::screen_refresh_flag >= 2) {
            setvesa::Add_Square_To_Scan_List(x1, y1, x2, y2);
        }

        uint8_t* framebuffer = video::current_video_page;
        const int32_t stride = video::g_screen_res_x;
        for (int16_t y = y1; y <= y2; ++y) {
            uint8_t* row = framebuffer + (int32_t)y * stride + x1;
            for (int16_t x = x1; x <= x2; ++x) {
                row[x - x1] =
                    (Speckle_Threshold(x, y) < density) ? speckle : base;
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Tint_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t tint_color) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        if (x2 < x1) {
            strings::Swap_Short(&x1, &x2);
        }
        if (y2 < y1) {
            strings::Swap_Short(&y1, &y2);
        }

        if (animate::clipping_flag == 1) {
            if (x1 < video::screen_window_x1) {
                x1 = video::screen_window_x1;
            }
            if (y1 < video::screen_window_y1) {
                y1 = video::screen_window_y1;
            }
            if (video::screen_window_x2 < x2) {
                x2 = video::screen_window_x2;
            }
            if (video::screen_window_y2 < y2) {
                y2 = video::screen_window_y2;
            }
        }

        if (video::screen_refresh_flag >= 2) {
            setvesa::Add_Square_To_Scan_List(x1, y1, x2, y2);
        }

        int32_t start_col = x1 / 4;
        int32_t end_col = x2 / 4;

        if (start_col == end_col) {
            uint8_t* dest = video::current_video_page + (y1 * video::g_screen_res_x) + x1;
            int32_t height = y2 - y1 + 1;
            int32_t width = x2 - x1 + 1;
            uint8_t* color_map = (uint8_t*)&video::glass_remap_colors + (tint_color * 256);
            regions::Tint_Fill_Column(color_map, dest, width, height);
        } else {
            // Exact binary behavior: split-path uses base glass remap table; tint_color offset is only used in the single-block path.
            int32_t start_pixels = 4 - (x1 & 3);
            int32_t middle_blocks = end_col - start_col - 1;
            int32_t height = y2 - y1 + 1;
            int32_t end_pixels = x2 & 3;
            int32_t offset_y = y1 * video::g_screen_res_x;

            if (start_pixels == 4) {
                // Exact binary behavior: if x1 is 4-byte aligned and there are no middle dword blocks, no region call is made.
                if (middle_blocks != 0) {
                    regions::Tint_Fill_Region(
                        (uint8_t*)&video::glass_remap_colors,
                        video::current_video_page + offset_y + start_col * 4,
                        middle_blocks + 1,
                        height);
                }
            } else {
                regions::Tint_Fill_Column(
                    (uint8_t*)&video::glass_remap_colors,
                    video::current_video_page + offset_y + x1,
                    start_pixels,
                    height);
                regions::Tint_Fill_Region(
                    (uint8_t*)&video::glass_remap_colors,
                    video::current_video_page + offset_y + start_col * 4 + 4,
                    middle_blocks,
                    height);
            }

            // Exact binary behavior: trailing partial width is (x2 & 3); zero suppresses the tail call.
            if (end_pixels != 0) {
                regions::Tint_Fill_Column(
                    (uint8_t*)&video::glass_remap_colors,
                    video::current_video_page + offset_y + end_col * 4,
                    end_pixels,
                    height);
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Interlaced_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color1, int16_t color2) {
        int32_t start_col;
        int32_t end_col;
        int32_t left_rem;
        int32_t right_rem;
        int32_t mid_cols;
        int32_t offset;
        int32_t height;
        uint32_t fill_val;

        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();
        fill_val = (color2 << 8) + color1;

        if (animate::clipping_flag == 1) {
            if (x1 < video::screen_window_x1) {
                x1 = video::screen_window_x1;
            }
            if (y1 < video::screen_window_y1) {
                y1 = video::screen_window_y1;
            }
            if (x2 > video::screen_window_x2) {
                x2 = video::screen_window_x2;
            }
            if (y2 > video::screen_window_y2) {
                y2 = video::screen_window_y2;
            }
        }

        if (x2 < x1) {
            platform::Unlock_Appropriate_Render_Buffer();
            return;
        }

        if (y2 < y1) {
            platform::Unlock_Appropriate_Render_Buffer();
            return;
        }

        if (video::screen_refresh_flag >= 2) {
            setvesa::Add_Square_To_Scan_List(x1, y1, x2, y2);
        }

        start_col = x1 / 4;
        end_col = x2 / 4;

        if (end_col == start_col) {
            regions::Interlaced_Fill_Column(
                fill_val,
                video::current_video_page + y1 * video::g_screen_res_x + x1,
                x2 - x1 + 1,
                y2 - y1 + 1);
        } else {
            left_rem = 4 - (x1 & 3);
            right_rem = (x2 & 3) + 1;
            mid_cols = end_col - start_col - 1;

            if (left_rem == 4) {
                left_rem = 0;
                mid_cols++;
            }

            if (right_rem == 4) {
                right_rem = 0;
                mid_cols++;
            }

            offset = y1 * video::g_screen_res_x;
            height = y2 - y1 + 1;

            if (left_rem == 0) {
                if (mid_cols != 0) {
                    regions::Interlaced_Fill_Region(
                        fill_val,
                        video::current_video_page + offset + start_col * 4,
                        mid_cols,
                        height);
                }
            } else {
                regions::Interlaced_Fill_Column(
                    fill_val,
                    video::current_video_page + offset + x1,
                    left_rem,
                    height);
                regions::Interlaced_Fill_Region(
                    fill_val,
                    video::current_video_page + offset + start_col * 4 + 4,
                    mid_cols,
                    height);
            }

            if (right_rem != 0) {
                regions::Interlaced_Fill_Column(
                    fill_val,
                    video::current_video_page + offset + end_col * 4,
                    right_rem,
                    height);
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Tri_Interlaced_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color1, int16_t color2, int16_t color3) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        uint32_t color = ((uint32_t)color3 << 16) + ((uint32_t)color2 << 8) + (uint32_t)color1;

        if (x1 > x2) {
            strings::Swap_Short(&x1, &x2);
        }

        if (y1 > y2) {
            strings::Swap_Short(&y1, &y2);
        }

        if (animate::clipping_flag == 1) {
            if (x1 < video::screen_window_x1) {
                x1 = video::screen_window_x1;
            }
            if (y1 < video::screen_window_y1) {
                y1 = video::screen_window_y1;
            }
            if (x2 > video::screen_window_x2) {
                x2 = video::screen_window_x2;
            }
            if (y2 > video::screen_window_y2) {
                y2 = video::screen_window_y2;
            }
        }

        if (video::screen_refresh_flag >= 2) {
            setvesa::Add_Square_To_Scan_List(x1, y1, x2, y2);
        }

        int32_t x1_blk = x1 / 4;
        int32_t x2_blk = x2 / 4;

        if (x1_blk == x2_blk) {
            int32_t y_offset = y1 * video::g_screen_res_x;
            uint8_t* dest_ptr = video::current_video_page + y_offset + x1;
            int32_t height = y2 - y1 + 1;
            int32_t width = x2 - x1 + 1;
            regions::Tri_Interlaced_Fill_Column(color, dest_ptr, width, height);
        } else {
            int32_t left_cols = 4 - (x1 & 3);
            int32_t middle_blocks = x2_blk - x1_blk - 1;
            int32_t right_cols = (x2 & 3) + 1;

            if (left_cols == 4) {
                left_cols = 0;
                middle_blocks++;
            }

            if (right_cols == 4) {
                right_cols = 0;
                middle_blocks++;
            }

            int32_t height = y2 - y1 + 1;
            int32_t y_offset = y1 * video::g_screen_res_x;

            if (left_cols != 0) {
                uint8_t* dest_ptr = video::current_video_page + y_offset + x1;
                regions::Tri_Interlaced_Fill_Column(color, dest_ptr, left_cols, height);

                uint8_t* region_ptr = video::current_video_page + y_offset + x1_blk * 4 + 4;
                regions::Tri_Interlaced_Fill_Region(color, region_ptr, middle_blocks, height);
            } else {
                if (middle_blocks != 0) {
                    uint8_t* region_ptr = video::current_video_page + y_offset + x1_blk * 4;
                    regions::Tri_Interlaced_Fill_Region(color, region_ptr, middle_blocks, height);
                }
            }

            if (right_cols != 0) {
                uint8_t* dest_ptr = video::current_video_page + y_offset + x2_blk * 4;
                regions::Tri_Interlaced_Fill_Column(color, dest_ptr, right_cols, height);
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Gray_Scale_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t* palette_map, int32_t gradient_scale, int32_t invert_flag) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        if (x2 < x1) {
            strings::Swap_Short(&x1, &x2);
        }
        if (y2 < y1) {
            strings::Swap_Short(&y1, &y2);
        }

        if (animate::clipping_flag == 1) {
            if (x1 < video::screen_window_x1) {
                x1 = video::screen_window_x1;
            }
            if (y1 < video::screen_window_y1) {
                y1 = video::screen_window_y1;
            }
            if (x2 > video::screen_window_x2) {
                x2 = video::screen_window_x2;
            }
            if (y2 > video::screen_window_y2) {
                y2 = video::screen_window_y2;
            }
        }

        if (video::screen_refresh_flag >= 2) {
            setvesa::Add_Square_To_Scan_List(x1, y1, x2, y2);
        }

        replace::palette_color_list = replace::graphics_buffer;
        replace::Establish_Color_Gradient((int16_t)gradient_scale, (int16_t)invert_flag);

        int32_t row_advance = video::g_screen_res_x - (x2 - x1) - 1;

        for (int32_t i = 0; i < 256; ++i) {
            uint8_t color_idx = replace::palette_color_list[i];
            replace::palette_color_list[i] = palette_map[color_idx];
        }

        uint8_t* dest_ptr = video::current_video_page + (y1 * video::g_screen_res_x) + x1;
        int32_t width = (x2 - x1) + 1;
        int32_t height = (y2 - y1) + 1;

        shear::Gray_Scale_Fill_Pixels(dest_ptr, replace::palette_color_list, row_advance, width, height);

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Gradient_Fill(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t mode, uint8_t* palette, int32_t gradient_scale, s_animation_header* pic, s_colors* colors) {
        switch (mode) {
            case 1:
                Fill((int16_t)x1, (int16_t)y1, (int16_t)x2, (int16_t)y2, (int8_t)*palette);
                break;
            case 3:
                Tint_Fill((int16_t)x1, (int16_t)y1, (int16_t)x2, (int16_t)y2, 0);
                break;
            case 15:
                Tint_Fill((int16_t)x1, (int16_t)y1, (int16_t)x2, (int16_t)y2, (int8_t)*palette);
                break;
            case 16:
                Gray_Scale_Fill((int16_t)x1, (int16_t)y1, (int16_t)x2, (int16_t)y2, palette, gradient_scale, 0);
                break;
        }
    }
} // namespace graphics