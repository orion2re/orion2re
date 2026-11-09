#include "pch.h"

namespace animate {
    s_palette_header* palette_header;
    s_animation_header* animation_header;
    int16_t clipping_flag;
    int16_t dot_stencil_flag;

    char s_Refresh_Mask_cant_be_drawn[28] = "Refresh Mask can't be drawn";

    short __cdecl Get_Full_Store_Flag(s_animation_header* header) {
        animation_header = header;
        return (header->flags & ANIMATION_FLAG_FULL_STORE) != 0;
    }

    void __cdecl Draw_Palette(s_animation_header* header) {
        animation_header = header;

        if (header->flags & ANIMATION_FLAG_HAS_PALETTE) {
            Set_Animation_Palette();
        }
    }

    void __cdecl Draw_Screen_Background_Off(uint8_t* sprite_data) {
        uint32_t* sprite = (uint32_t*)sprite_data;
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();
        uint32_t* vpage = (uint32_t*)video::current_video_page;

        int sprite_idx = 0;
        for (int row_base = 0; row_base < 0x14000; row_base += 0x4000) {
            while (true) {
                const s_packed_screen_background_run32* run =
                    reinterpret_cast<const s_packed_screen_background_run32*>(sprite + sprite_idx);
                uint32_t skip = run->skip_count;
                uint32_t count = run->pixel_count;
                sprite_idx += 2;

                if (skip == 0 && count == 0) {
                    break;
                }

                int dest_idx = row_base + skip;
                int sprite_end = sprite_idx + count;

                while (sprite_idx < sprite_end) {
                    if (dest_idx >= 0x12C00) {
                        break;
                    }

                    vpage[dest_idx] = sprite[sprite_idx];

                    dest_idx++;
                    sprite_idx++;
                }
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Draw(int16_t x, int16_t y, s_animation_header* anim) {
        animation_header = anim;

        if (anim->flags & ANIMATION_FLAG_HAS_PALETTE) {
            Set_Animation_Palette();
        }

        uint8_t* frame_data = Animation_Frame_Data_(anim, anim->current_frame);

        int16_t x_end = anim->width + x - 1;
        int16_t y_end = anim->height + y - 1;
        bool needs_clipping = false;

        if (clipping_flag == 1) {
            if (x > video::screen_window_x2 || y > video::screen_window_y2 ||
                x_end < video::screen_window_x1 || y_end < video::screen_window_y1) {
                goto update_frame;
            }

            if (x < video::screen_window_x1 || y < video::screen_window_y1 ||
                x_end > video::screen_window_x2 || y_end > video::screen_window_y2) {
                needs_clipping = true;
            }
        }

        if (needs_clipping) {
            if (dot_stencil_flag == 0) {
                if (video::screen_refresh_flag > 1) {
                    setvesa::Add_Square_To_Scan_List(x, y, x_end, y_end);
                }

                switch (anim->flags & ANIMATION_FLAG_DRAW_MODE_MASK) {
                    case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                        if (anim->flags & ANIMATION_FLAG_GLASSED)
                            clipped::Clipped_Draw_Glassed_Animated_Sprite(x, y, frame_data);
                        else
                            clipped::Clipped_Draw_Animated_Sprite(x, y, frame_data);
                        break;
                    case ANIMATION_DRAW_MODE_BITMAP:
                        if (anim->flags & ANIMATION_FLAG_GLASSED)
                            clipped::Clipped_Draw_Glassed_Bitmap_Sprite(x, y, frame_data);
                        else
                            clipped::Clipped_Draw_Bitmap_Sprite(x, y, frame_data);
                        break;
                    case ANIMATION_DRAW_MODE_SCREEN_BACKGROUND:
                        exitm::Exit_With_Message("Screen Background can't be clipped");
                        break;
                    case ANIMATION_DRAW_MODE_REFRESH_MASK:
                        exitm::Exit_With_Message(s_Refresh_Mask_cant_be_drawn);
                        break;
                }
            } else if ((anim->flags & ANIMATION_FLAG_DRAW_MODE_MASK) == ANIMATION_DRAW_MODE_ANIMATED_SPRITE) {
                clipped::Clipped_Draw_Stenciled_Animated_Sprite(x, y, frame_data);
            }
        } else {
            if (video::screen_refresh_flag > 1) {
                if (anim->width == 0 || anim->height == 0) {
                    return;
                }

                setvesa::Add_Square_To_Scan_List(x, y, x_end, y_end);
            }

            uint8_t type = anim->flags & ANIMATION_FLAG_DRAW_MODE_MASK;
            bool glassed = (anim->flags & ANIMATION_FLAG_GLASSED) != 0;

            if (!glassed) {
                switch (type) {
                    case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();
                        draw::Draw_Animated_Sprite(x, y, frame_data);
                        platform::Unlock_Appropriate_Render_Buffer();
                        break;
                    case ANIMATION_DRAW_MODE_BITMAP:
                        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();
                        draw::Draw_Bitmap_Sprite(x, y, frame_data);
                        platform::Unlock_Appropriate_Render_Buffer();
                        break;
                    case ANIMATION_DRAW_MODE_SCREEN_BACKGROUND:
                        Draw_Screen_Background_Off(frame_data);

                        if (video::screen_refresh_flag > 1) {
                            setvesa::Add_Square_To_Scan_List(0, 0, 0x27F, 0x1DF);
                        }
                        break;
                    case ANIMATION_DRAW_MODE_REFRESH_MASK:
                        exitm::Exit_With_Message(s_Refresh_Mask_cant_be_drawn);
                        break;
                }
            } else {
                switch (type) {
                    case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                        Draw_Glassed_Animated_Sprite(x, y, frame_data);
                        break;
                    case ANIMATION_DRAW_MODE_BITMAP:
                        Draw_Glassed_Bitmap_Sprite(x, y, frame_data);
                        break;
                    case ANIMATION_DRAW_MODE_SCREEN_BACKGROUND:
                        Draw_Glassed_Screen_Background_Off(frame_data);

                        if (video::screen_refresh_flag > 1) {
                            setvesa::Add_Square_To_Scan_List(0, 0, 0x27F, 0x1DF);
                        }
                        break;
                    case ANIMATION_DRAW_MODE_REFRESH_MASK:
                        exitm::Exit_With_Message(s_Refresh_Mask_cant_be_drawn);
                        break;
                }
            }
        }

    update_frame:
        anim->current_frame++;
        if (anim->flags & 0x20) {
            if (anim->current_frame >= anim->frame_count) {
                anim->current_frame = anim->loop_frame;
                return;
            }
        } else {
            if (anim->current_frame >= anim->frame_count) {
                anim->current_frame = anim->loop_frame;
            }
        }
    }

    void __cdecl Draw_No_Glassed_Animated_Sprite(int16_t x, int16_t y, uint8_t* frame_data) {
        int y_offset = reinterpret_cast<const s_packed_frame_header*>(frame_data)->start_y;
        int dest_line_base = (y + y_offset) * (int)video::g_screen_res_x + x;
        int dest_ptr = dest_line_base;
        int frame_idx = 2;
        int height = animation_header->height;

        while (y_offset < height) {
            const s_packed_frame_run* run =
                reinterpret_cast<const s_packed_frame_run*>(frame_data + frame_idx * 2);
            int16_t run_len = run->pixel_count;
            int16_t skip_len = run->skip_count;
            frame_idx += 2;

            if (run_len == 0) {
                y_offset += skip_len;
                dest_line_base += skip_len * 640;
                dest_ptr = dest_line_base;
            } else {
                dest_ptr += skip_len;
                int src_idx = frame_idx * 2;
                int end_dest = dest_ptr + run_len;

                while (dest_ptr < end_dest) {
                    uint8_t pixel = frame_data[src_idx++];
                    if (pixel < 0xf0) {
                        video::current_video_page[dest_ptr] = pixel;
                    }
                    dest_ptr++;
                }

                frame_idx += (run_len + 1) / 2;
            }
        }
    }

    void __cdecl Draw_Glassed_Animated_Sprite(int16_t x, int16_t y, uint8_t* frame_data) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        int32_t current_y_offset = reinterpret_cast<const s_packed_frame_header*>(frame_data)->start_y;
        int32_t word_idx = 2;

        int32_t dest_idx = (int32_t)x + ((int32_t)y + current_y_offset) * video::g_screen_res_x;
        int32_t line_start_idx = dest_idx;

        int32_t total_height = (int32_t)animation_header->height;
        uint8_t* video_ptr = video::current_video_page;

        while (current_y_offset < total_height) {
            const s_packed_frame_run* run =
                reinterpret_cast<const s_packed_frame_run*>(frame_data + word_idx * 2);
            int16_t pixel_count = run->pixel_count;
            int16_t skip_val = run->skip_count;
            word_idx += 2;

            if (pixel_count == 0) {
                current_y_offset += skip_val;
                line_start_idx += (int32_t)skip_val * 640;
                dest_idx = line_start_idx;
            } else {
                dest_idx += skip_val;
                uint8_t* pixel_data = &frame_data[word_idx * 2];

                for (int32_t i = 0; i < (int32_t)pixel_count; ++i) {
                    uint8_t pixel = pixel_data[i];
                    if (pixel < 0xF0) {
                        video_ptr[dest_idx] = pixel;
                    } else {
                        uint8_t background_pixel = video_ptr[dest_idx];
                        video_ptr[dest_idx] = video::glass_remap_colors[pixel - 0xF0][background_pixel];
                    }
                    dest_idx++;
                }

                word_idx += ((int32_t)pixel_count + 1) / 2;
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Draw_Glassed_Bitmap_Sprite(int16_t x, int16_t y, uint8_t* sprite) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        s_animation_header* header = animate::animation_header;
        int16_t sprite_w = header->width;
        int16_t sprite_h = header->height;

        int32_t screen_stride = video::g_screen_res_x - (int32_t)sprite_w;
        uint8_t* v_page = video::current_video_page;
        uint32_t screen_offset = (int32_t)y * video::g_screen_res_x + (int32_t)x;
        uint32_t sprite_idx = 0;

        if (sprite_h > 0) {
            int32_t rows_remaining = (int32_t)sprite_h;
            do {
                if (sprite_w > 0) {
                    int32_t cols_remaining = (int32_t)sprite_w;
                    do {
                        uint8_t pixel = sprite[sprite_idx];
                        if (pixel != 0) {
                            if (pixel < 0xF0) {
                                v_page[screen_offset] = pixel;
                            } else {
                                uint8_t existing_pixel = v_page[screen_offset];
                                v_page[screen_offset] = video::glass_remap_colors[pixel - 0xF0][existing_pixel];
                            }
                        }
                        sprite_idx++;
                        screen_offset++;
                        cols_remaining--;
                    } while (cols_remaining != 0);
                }
                screen_offset += screen_stride;
                rows_remaining--;
            } while (rows_remaining != 0);
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Draw_Glassed_Screen_Background_Off(uint8_t* sprite) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        int32_t sprite_idx = 0;

        for (int32_t page_offset = 0; page_offset < 0x50000; page_offset += 0x10000) {
            int32_t current_out_pos = page_offset;

            while (true) {
                const s_packed_screen_background_run8* run =
                    reinterpret_cast<const s_packed_screen_background_run8*>(sprite + sprite_idx);
                int32_t skip_count = run->skip_count;
                int32_t run_length = run->pixel_count;
                sprite_idx += 2;

                if (run_length == 0) {
                    break;
                }

                current_out_pos += skip_count;

                int32_t end = sprite_idx + run_length * 4;
                while (sprite_idx < end) {
                    video::current_video_page[current_out_pos++] = sprite[sprite_idx++];
                }
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Set_Animation_Palette() {
        palette_header = Animation_Palette_Header_(animation_header);

        const int16_t start_index = palette_header->start_index;
        const int16_t count = palette_header->count;

        if (count > 0) {
            s_palette_entry* dst = &video::current_palette[start_index];
            const uint8_t* src =
                reinterpret_cast<const uint8_t*>(palette_header) + sizeof(s_palette_header);

            for (int16_t i = 0; i < count; ++i) {
                const uint32_t src_val = unaligned::Load<uint32_t>(src + i * sizeof(uint32_t));
                const uint32_t dst_val = unaligned::Load<uint32_t>(&dst[i]);

                if (dst_val != src_val) {
                    unaligned::Store<uint32_t>(&dst[i], src_val + 1);
                    dst[i].changed = 1;
                }
            }
        }
    }

    void __cdecl Draw_Animated_Sprite_Back_Diff(int16_t x, int16_t y, uint8_t* frame_data) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();
        platform::Lock_Offscreen_Surface();

        int32_t current_y = reinterpret_cast<const s_packed_frame_header*>(frame_data)->start_y;
        int32_t draw_y = y + current_y;
        int32_t data_offset_words = 2;
        int32_t draw_x = x;
        int32_t screen_offset = draw_y * video::g_screen_res_x + x;
        int32_t line_start_offset = screen_offset;
        int32_t height = animation_header->height;

        while (current_y < height) {
            const s_packed_frame_run* run =
                reinterpret_cast<const s_packed_frame_run*>(frame_data + data_offset_words * 2);
            int32_t count = run->pixel_count;
            int32_t skip = run->skip_count;
            data_offset_words += 2;

            if (count == 0) {
                draw_x = x;
                draw_y += skip;
                current_y += skip;
                line_start_offset += skip * 640;
                screen_offset = line_start_offset;
            } else {
                draw_x += skip;
                screen_offset += skip;

                setvesa::Add_Square_To_Scan_List(draw_x, draw_y, draw_x + count, draw_y);

                int32_t end_screen_offset = screen_offset + count;
                if (screen_offset < end_screen_offset) {
                    draw_x += count;
                    int32_t data_byte_offset = data_offset_words * 2;
                    do {
                        uint8_t pixel = frame_data[data_byte_offset++];
                        platform::offscreen_surface[screen_offset] = pixel;
                        video::current_video_page[screen_offset] = pixel;
                        screen_offset++;
                    } while (screen_offset < end_screen_offset);
                }

                data_offset_words += (count + 1) / 2;
            }
        }

        platform::Unlock_Offscreen_Surface();
        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Remap_Draw(int16_t x, int16_t y, s_animation_header* anim) {
        animation_header = anim;
        int16_t x2 = anim->width + x - 1;
        int16_t y2 = anim->height + y - 1;
        bool clipped = false;

        if (clipping_flag == 1) {
            if (video::screen_window_x2 < x) return;
            if (video::screen_window_y2 < y) return;
            if (x2 < video::screen_window_x1) return;
            if (y2 < video::screen_window_y1) return;

            if (x < video::screen_window_x1 ||
                y < video::screen_window_y1 ||
                video::screen_window_x2 < x2 ||
                video::screen_window_y2 < y2) {
                clipped = true;
                if (video::screen_window_x2 < x2) {
                    x2 = video::screen_window_x2;
                }
                if (video::screen_window_y2 < y2) {
                    y2 = video::screen_window_y2;
                }
            }
        }

        uint8_t* frame_ptr = Animation_Frame_Data_(anim, anim->current_frame);

        if (clipped) {
            if (dot_stencil_flag == 0) {
                if (video::screen_refresh_flag >= 2) {
                    setvesa::Add_Square_To_Scan_List(x, y, x2, y2);
                }

                switch (anim->flags & ANIMATION_FLAG_DRAW_MODE_MASK) {
                    case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                        Remap_Clipped_Draw_Animated_Sprite(x, y, frame_ptr);
                        return;
                    case ANIMATION_DRAW_MODE_BITMAP:
                        exitm::Exit_With_Message("Remap draw not support bitmaps");
                        break;
                    case ANIMATION_DRAW_MODE_SCREEN_BACKGROUND:
                        exitm::Exit_With_Message("Remap draw not support screen");
                        break;
                    case ANIMATION_DRAW_MODE_REFRESH_MASK:
                        exitm::Exit_With_Message("Remap draw not support map");
                        break;
                }
            }
        } else {
            if (video::screen_refresh_flag >= 2) {
                setvesa::Add_Square_To_Scan_List(x, y, x + anim->width - 1, y + anim->height - 1);
            }

            switch (anim->flags & ANIMATION_FLAG_DRAW_MODE_MASK) {
                case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                    Remap_Draw_Animated_Sprite(x, y, frame_ptr);
                    break;
                case ANIMATION_DRAW_MODE_BITMAP:
                    exitm::Exit_With_Message("Remap draw not support bitmaps");
                    break;
                case ANIMATION_DRAW_MODE_SCREEN_BACKGROUND:
                    exitm::Exit_With_Message("Remap draw not support screen");
                    break;
                case ANIMATION_DRAW_MODE_REFRESH_MASK:
                    exitm::Exit_With_Message("Remap draw not support map");
                    break;
            }
        }

        anim->current_frame++;

        if ((anim->flags & 0x20) == 0) {
            if (anim->current_frame >= anim->frame_count) {
                anim->current_frame = anim->loop_frame;
            }
        } else {
            int32_t loop32 = anim->loop_frame;
            int32_t curr32 = anim->current_frame;

            if (loop32 < curr32) {
                if (loop32 - curr32 == -1) {
                    anim->current_frame = 0;
                    return;
                }
                if (curr32 >= anim->frame_count) {
                    anim->current_frame = anim->frame_count - 1;
                    return;
                }
            }
        }
    }

    void __cdecl Draw_No_Glass(int16_t x, int16_t y, s_animation_header* header) {
        animation_header = header;

        int16_t x2 = x + header->width - 1;
        int16_t y2 = y + header->height - 1;

        if ((header->flags & ANIMATION_FLAG_HAS_PALETTE) != 0) {
            Set_Animation_Palette();
        }

        uint8_t* sprite = Animation_Frame_Data_(header, header->current_frame);

        bool is_clipped = false;

        if (clipping_flag != 0 && (x > video::screen_window_x2 || y > video::screen_window_y2 || x2 < video::screen_window_x1 || y2 < video::screen_window_y1)) {
            // Out of bounds, skip drawing entirely
        } else {
            if (clipping_flag != 0) {
                if (x < video::screen_window_x1 || y < video::screen_window_y1 ||
                    x2 > video::screen_window_x2 || y2 > video::screen_window_y2) {
                    is_clipped = true;
                }
            }

            if (!is_clipped) {
                if (video::screen_refresh_flag > 1) {
                    setvesa::Add_Square_To_Scan_List(x, y, x2, y2);
                }

                if ((header->flags & ANIMATION_FLAG_GLASSED) != 0) {
                    switch (header->flags & ANIMATION_FLAG_DRAW_MODE_MASK) {
                        case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                            Draw_No_Glassed_Animated_Sprite(x, y, sprite);
                            break;
                        case ANIMATION_DRAW_MODE_BITMAP:
                            Draw_No_Glassed_Bitmap_Sprite(x, y, sprite);
                            break;
                        case ANIMATION_DRAW_MODE_SCREEN_BACKGROUND:
                            Draw_Glassed_Screen_Background_Off(sprite);
                            if (video::screen_refresh_flag > 1) {
                                setvesa::Add_Square_To_Scan_List(0, 0, 0x27F, 0x1DF);
                            }
                            break;
                        case ANIMATION_DRAW_MODE_REFRESH_MASK:
                            exitm::Exit_With_Message(s_Refresh_Mask_cant_be_drawn);
                            break;
                    }
                } else {
                    switch (header->flags & ANIMATION_FLAG_DRAW_MODE_MASK) {
                        case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                            draw::Draw_Animated_Sprite(x, y, sprite);
                            break;
                        case ANIMATION_DRAW_MODE_BITMAP:
                            draw::Draw_Bitmap_Sprite(x, y, sprite);
                            break;
                        case ANIMATION_DRAW_MODE_SCREEN_BACKGROUND:
                            Draw_Screen_Background_Off(sprite);
                            if (video::screen_refresh_flag > 1) {
                                setvesa::Add_Square_To_Scan_List(0, 0, 0x27F, 0x1DF);
                            }
                            break;
                        case ANIMATION_DRAW_MODE_REFRESH_MASK:
                            exitm::Exit_With_Message(s_Refresh_Mask_cant_be_drawn);
                            break;
                    }
                }
            } else {
                if (dot_stencil_flag != 0) {
                    if ((header->flags & ANIMATION_FLAG_DRAW_MODE_MASK) == ANIMATION_DRAW_MODE_ANIMATED_SPRITE) {
                        clipped::Clipped_Draw_Stenciled_Animated_Sprite(x, y, sprite);
                    }
                } else {
                    if (video::screen_refresh_flag > 1) {
                        setvesa::Add_Square_To_Scan_List(x, y, x2, y2);
                    }

                    switch (header->flags & ANIMATION_FLAG_DRAW_MODE_MASK) {
                        case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                            if ((header->flags & ANIMATION_FLAG_GLASSED) != 0) {
                                clipped::Clipped_Draw_Glassed_Animated_Sprite(x, y, sprite);
                            } else {
                                clipped::Clipped_Draw_Animated_Sprite(x, y, sprite);
                            }
                            break;
                        case ANIMATION_DRAW_MODE_BITMAP:
                            if ((header->flags & ANIMATION_FLAG_GLASSED) != 0) {
                                clipped::Clipped_Draw_Glassed_Bitmap_Sprite(x, y, sprite);
                            } else {
                                clipped::Clipped_Draw_Bitmap_Sprite(x, y, sprite);
                            }
                            break;
                        case ANIMATION_DRAW_MODE_SCREEN_BACKGROUND:
                            exitm::Exit_With_Message("Screen Background can't be clipped");
                            break;
                        case ANIMATION_DRAW_MODE_REFRESH_MASK:
                            exitm::Exit_With_Message(s_Refresh_Mask_cant_be_drawn);
                            break;
                    }
                }
            }
        }

        animation_header->current_frame++;

        if ((animation_header->flags & 0x20) != 0) {
            int8_t loop_frame = animation_header->loop_frame;
            if (loop_frame < animation_header->current_frame) {
                if (loop_frame - animation_header->current_frame == -1) {
                    animation_header->current_frame = 0;
                    return;
                }
                if (animation_header->current_frame >= animation_header->frame_count) {
                    animation_header->current_frame = animation_header->frame_count - 1;
                    return;
                }
            }
        } else {
            if (animation_header->current_frame >= animation_header->frame_count) {
                animation_header->current_frame = animation_header->loop_frame;
            }
        }
    }

    void __cdecl Remap_Draw_Animated_Sprite(int16_t x, int16_t y, uint8_t* sprite_data) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        int32_t current_y_offset = reinterpret_cast<const s_packed_frame_header*>(sprite_data)->start_y;
        int32_t line_start_offset = (y + current_y_offset) * video::g_screen_res_x + x;
        int32_t sprite_idx = 2;
        int32_t screen_offset = line_start_offset;
        int32_t header_height = animation_header->height;

        if (current_y_offset < header_height) {
            do {
                const s_packed_frame_run* run =
                    reinterpret_cast<const s_packed_frame_run*>(sprite_data + sprite_idx * 2);
                int16_t run_len = run->pixel_count;
                int16_t skip_len = run->skip_count;
                sprite_idx += 2;

                if (run_len == 0) {
                    current_y_offset += skip_len;
                    line_start_offset += skip_len * 640;
                    screen_offset = line_start_offset;
                } else {
                    screen_offset += skip_len;
                    int32_t end_offset = screen_offset + run_len;
                    int32_t data_offset = sprite_idx * 2;

                    if (screen_offset < end_offset) {
                        do {
                            uint8_t color_idx = sprite_data[data_offset++];
                            video::current_video_page[screen_offset++] = remap::picture_remap_color_list[color_idx];
                        } while (screen_offset < end_offset);
                    }

                    sprite_idx += (run_len + 1) / 2;
                }
            } while (current_y_offset < header_height);
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }

    void __cdecl Draw_No_Glassed_Bitmap_Sprite(int16_t x, int16_t y, uint8_t* bitmap) {
        int16_t width = animation_header->width;
        int16_t height = animation_header->height;
        int32_t src_offset = 0;
        int32_t dest_offset = y * video::g_screen_res_x + x;
        int32_t row_skip = video::g_screen_res_x - width;

        if (height > 0) {
            for (int32_t h = 0; h < height; ++h) {
                if (width > 0) {
                    for (int32_t w = 0; w < width; ++w) {
                        uint8_t pixel = bitmap[src_offset];
                        if (pixel != 0 && pixel < 0xf0) {
                            video::current_video_page[dest_offset] = pixel;
                        }
                        ++src_offset;
                        ++dest_offset;
                    }
                }
                dest_offset += row_skip;
            }
        }
    }

    void __cdecl Remap_Clipped_Draw_Animated_Sprite(int16_t x, int16_t y, uint8_t* frame_data) {
        video::g_screen_res_x = platform::Lock_Appropriate_Render_Buffer();

        int16_t x_off;
        int16_t draw_x;
        if (x < video::screen_window_x1) {
            x_off = video::screen_window_x1 - x;
            draw_x = video::screen_window_x1;
        } else {
            x_off = 0;
            draw_x = x;
        }

        int16_t y_off;
        int16_t draw_y;
        if (y < video::screen_window_y1) {
            y_off = video::screen_window_y1 - y;
            draw_y = video::screen_window_y1;
        } else {
            y_off = 0;
            draw_y = y;
        }

        int16_t max_x = animation_header->width + x - 1;
        int16_t max_y = animation_header->height + y - 1;

        if (video::screen_window_x2 <= max_x) {
            max_x = video::screen_window_x2;
        }
        int32_t draw_width = max_x - draw_x + 1;

        if (video::screen_window_y2 <= max_y) {
            max_y = video::screen_window_y2;
        }
        int32_t draw_height = max_y - draw_y + 1;

        int32_t current_y = reinterpret_cast<const s_packed_frame_header*>(frame_data)->start_y;
        int32_t screen_offset = x + draw_y * video::g_screen_res_x;
        int32_t start_y = y_off;
        int32_t idx = 2;

        while (current_y < start_y) {
            const s_packed_frame_run* run =
                reinterpret_cast<const s_packed_frame_run*>(frame_data + idx * 2);
            int16_t run_length = run->pixel_count;
            if (run_length == 0) {
                current_y += static_cast<uint16_t>(run->skip_count);
            } else {
                idx += (run_length + 1) / 2;
            }
            idx += 2;
        }

        if (start_y < current_y) {
            screen_offset += (current_y - start_y) * video::g_screen_res_x;
        }

        current_y -= start_y;
        int32_t right_limit_offset = x_off + draw_width;
        int32_t line_start_offset = screen_offset;
        int32_t clip_left = line_start_offset + x_off;
        int32_t clip_right = line_start_offset + right_limit_offset;

        while (current_y < draw_height) {
            const s_packed_frame_run* run =
                reinterpret_cast<const s_packed_frame_run*>(frame_data + idx * 2);
            int16_t run_length = run->pixel_count;
            int16_t skip = run->skip_count;
            idx += 2;

            if (run_length == 0) {
                current_y += skip;
                // Assembly explicitly uses constant 640 instead of g_screen_res_x inside this branch
                line_start_offset += skip * 640;
                screen_offset = line_start_offset;
                clip_left = x_off + line_start_offset;
                clip_right = right_limit_offset + line_start_offset;
            } else {
                screen_offset += skip;
                int32_t data_idx = idx * 2;
                int32_t end_x = run_length + screen_offset;

                if (screen_offset < clip_right) {
                    if (screen_offset < clip_left) {
                        int16_t left_clip = clip_left - screen_offset;
                        if (run_length <= left_clip) {
                            left_clip = run_length;
                        }
                        data_idx += left_clip;
                        screen_offset += left_clip;
                    }

                    int16_t visible_run_length = clip_right - screen_offset;
                    if (end_x <= visible_run_length + screen_offset) {
                        visible_run_length = end_x - screen_offset;
                    }
                    end_x = visible_run_length + screen_offset;

                    while (screen_offset < end_x) {
                        video::current_video_page[screen_offset] = remap::picture_remap_color_list[frame_data[data_idx]];
                        data_idx++;
                        screen_offset++;
                    }
                }
                idx += (run_length + 1) / 2;
            }
        }

        platform::Unlock_Appropriate_Render_Buffer();
    }
} // namespace animate