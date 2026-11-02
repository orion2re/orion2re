#include "pch.h"
#include <cstring>
#include "ship_read.h"
#include "colony_read.h"
#include "game_events.h"

namespace SYS {
    constexpr int PLANET_CLIMATE_EXTENT = PLANET_CLIMATE_COUNT;
    constexpr int PLANET_SIZE_EXTENT = PLANET_SIZE_COUNT;
    constexpr int PLANET_PICTURE_COUNT = PLANET_CLIMATE_EXTENT * PLANET_SIZE_EXTENT;

    void* sys_data_buffer;

    uint8_t draw_sun_before_planet;
    int8_t skip_draw_system_planets;
    uint8_t skip_draw_system_display;
    s_animation_header* system_display_sun_seg;
    void* sys_disp_bitmap;
    int8_t remap_gray_percentages[10] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    uint8_t remap_gray_mask_palette[10][256];
    s_animation_header* mask_header;
    char star_description_string[STAR_DESCRIPTION_STRING_CAPACITY];

    void __cdecl Clear_System_Display_Ship_Fields() {
        memset(MOX::system_display_ships, 0, sizeof(MOX::system_display_ships));

        for (int i = 0; i < 15; ++i) {
            MOX::system_display_ships[i].button = -1000;
            MOX::system_display_ships[i].ship_idx = -1;
        }
    }

    bool __cdecl Need_To_Reload_System_Display(int16_t star_idx) {
        char buffer[120];

        snprintf(buffer, sizeof(buffer), "%d> %s", star_idx, MOX::star[star_idx].name);

        size_t len = strlen(buffer);
        if (len >= SYSTEM_DISPLAY_LOADED_FLAG_LENGTH) {
            buffer[SYSTEM_DISPLAY_LOADED_FLAG_LENGTH] = '\0';
        }

        if (MOX::star_name_loaded_string != nullptr) {
            // Re-calculate length after potential truncation for the string compare
            size_t current_len = strlen(buffer);
            if (strncmp(buffer, MOX::star_name_loaded_string, current_len) == 0) {
                return false;
            }
        }

        return true;
    }

    void __cdecl Clear_System_Display_Loaded_Flag() {
        if (MOX::star_name_loaded_string != nullptr) {
            memset(MOX::star_name_loaded_string, 0, SYSTEM_DISPLAY_LOADED_FLAG_LENGTH);
        }
    }

    void __cdecl Set_System_Display_Loaded_Flag(int16_t star_idx) {
        char temp_buffer[120];

        char* allocated = (char*)allocate::Allocate_First_Block((void*)SYS::sys_data_buffer, SYSTEM_DISPLAY_LOADED_FLAG_LENGTH);
        MOX::star_name_loaded_string = allocated;

        memset(allocated, 0, SYSTEM_DISPLAY_LOADED_FLAG_LENGTH);

        s_star_data* star = &MOX::star[star_idx];

        snprintf(temp_buffer, sizeof(temp_buffer), "%d> %s", star_idx, star->name);

        if (strlen(temp_buffer) >= SYSTEM_DISPLAY_LOADED_FLAG_LENGTH) {
            temp_buffer[SYSTEM_DISPLAY_LOADED_FLAG_LENGTH - 1] = '\0';
        }

        strlcpy(MOX::star_name_loaded_string, temp_buffer, SYSTEM_DISPLAY_LOADED_FLAG_LENGTH);
    }

    bool __cdecl Ok_To_View_System(int16_t star_idx, int16_t player_idx) {
        // Collect visibility conditions
        bool has_visited = HAROLD::Player_Has_Visited(player_idx, star_idx);
        bool is_omniscient = HAROLD::Player_Is_Omniscient(player_idx);

        // System cannot be viewed if it is a black hole
        if (MOX::star[star_idx].spectral_class == STAR_CLASS_BLACK_HOLE) {
            return false;
        }

        // Otherwise, it is viewable if the player has visited it or has omniscience
        return (has_visited || is_omniscient);
    }

    void __cdecl Load_New_System_Display(int16_t star_idx) {
        bool has_planets = false;
        palette::Set_Palette_Changes(0, 0xff);
        reduce::Clear_Reduced_Palette_Groups();
        RUSS::Mox_Update();

        MOX::sys_disp = (s_sys_disp*)allocate::Allocate_Next_Block((void*)SYS::sys_data_buffer, 0x46);

        s_animation_header* background_pic = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x49, (void*)MOX::global_cache_seg);
        MOX::moveable_box[0].width = animate::Get_Width(background_pic);
        MOX::moveable_box[0].height = animate::Get_Height(background_pic);

        s_star_data* star = &MOX::star[star_idx];
        int8_t spectral_class = star->spectral_class;

        s_animation_header* sun_anim = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", spectral_class + 0x53, (void*)MOX::global_cache_seg);
        MOX::sun_seg[spectral_class] = sun_anim;
        system_display_sun_seg = MOX::sun_seg[spectral_class];

        RUSS::Mox_Update();
        draw_sun_before_planet = 1;

        for (int16_t orbit_idx = 0; orbit_idx < 5; ++orbit_idx) {
            int16_t planet_idx = star->planet_index[orbit_idx];
            if (planet_idx < 0) {
                MOX::sys_disp[orbit_idx].orbit = -1;
                MOX::sys_disp[orbit_idx].x = 999;
                MOX::sys_disp[orbit_idx].y = 999;
            } else {
                s_planet_data* planet = &MOX::planet[planet_idx];
                int8_t planet_size = planet->size;
                if (planet->planet_type == PLANET_TYPE_GAS_GIANT) {
                    planet_size = PLANET_SIZE_HUGE;
                }

                int32_t x_out, y_out, orbit_pos_out;
                GEO::Get_Orbit_Display_X_Y(star_idx, (int32_t)planet->orbit, &x_out, &y_out, &orbit_pos_out);

                uint8_t dim = MOX::rot_plan_dim[planet_size];
                int32_t offset = dim >> 1;

                if (orbit_idx == 0) {
                    if ((orbit_pos_out >= 0 && orbit_pos_out <= 2) || (orbit_pos_out >= 12 && orbit_pos_out <= 15)) {
                        draw_sun_before_planet = 0;
                    }
                }

                MOX::sys_disp[orbit_idx].x = (int16_t)x_out - (int16_t)offset;
                MOX::sys_disp[orbit_idx].y = (int16_t)y_out - (int16_t)offset;
                MOX::sys_disp[orbit_idx].orbit = planet_idx;
                MOX::sys_disp[orbit_idx].orbit_idx = (int8_t)orbit_idx;
                MOX::sys_disp[orbit_idx].oribit_pos = (int8_t)orbit_pos_out;

                allocate::Get_Remaining_Block_Space((void*)sys_data_buffer);
                MOX::sys_disp[orbit_idx].pic = Get_Rotating_Planet_Segment(planet_idx, star_idx, 0);

                int16_t frame_count = animate::Get_Animation_Frame_Count(MOX::sys_disp[orbit_idx].pic);
                MOX::sys_disp[orbit_idx].current_frame = (int16_t)((orbit_idx * 5) % frame_count);

                reduce::Add_Reduced_Palette_Group(MOX::sys_disp[orbit_idx].pic);
                has_planets = true;
            }
        }

        RUSS::Mox_Update();

        if (has_planets) {
            reduce::Compress_Reduced_Palette_Groups(0xc0, 0x40);
            remap::Update_Glass_Remap_Colors();
            Calculate_Remap_Gray_Palettes();

            if (MOX::current_screen == SCREEN_MAIN && skip_draw_system_planets == 0) {
                if (MOVEBOX::Moveable_Box_Selected(2) != 0) {
                    MOVEBOX::Vanish_Fleet_Movement_Box();
                    fields::Disable_Draw_All_Fields();
                    video::Set_Page_Off();
                    MAINSCR::Draw_Mini_Main_Screen();
                    RUSS::Mox_Sync_Update();
                    fields::Enable_Draw_All_Fields();
                }
            }
        }

        allocate::Get_Remaining_Block_Space((void*)sys_data_buffer);
        MOX::mainscr_palette = 0;
        RUSS::Mox_Update();

        qsort(MOX::sys_disp, 5, sizeof(MOX::sys_disp[0]), qsort_sys_disp);

        if (MOX::look_at_colonies == 0) {
            if (!Ok_To_View_System(star_idx, MOX::PLAYER_NUM)) {
                strlcpy(
                    SYS::star_description_string,
                    HAROLD::H_Message(MOX::star[star_idx].spectral_class + 0xf),
                    sizeof(SYS::star_description_string));
            }
        }
    }

    void __cdecl Init_System_Display(int16_t star_id) {
        if (SYS::Need_To_Reload_System_Display(star_id)) {
            MOX::system_star_id = star_id;
            SYS::Set_System_Display_Loaded_Flag(star_id);
            SYS::skip_draw_system_planets = 1;

            if (MOX::moveable_box[0].id < MOX::box_off_flag[0]) {
                SYS::skip_draw_system_display = 1;
            }

            SYS::Load_New_System_Display(star_id);
            MOX::system_icon_scanned = -1;

            if (HAROLD::Player_Sighted_Monster(MOX::PLAYER_NUM, star_id)) {
                MAINPUPS::Clear_System_Special_Slots();
            }
        }
    }

    s_animation_header* __cdecl Get_Rotating_Planet_Segment(int16_t planet_idx, int16_t unused_star_idx, int32_t unused_arg) {
        (void)unused_star_idx;
        (void)unused_arg;

        s_planet_data* planet = &MOX::planet[planet_idx];
        int16_t colony_idx = planet->colony_index;
        uint16_t climate = (uint16_t)(uint8_t)planet->climate;
        int8_t size = planet->size;

        if (size > PLANET_SIZE_LARGE) {
            size = PLANET_SIZE_HUGE;
        }

        if (colony_idx > -1 && colony_idx < MOX::NUM_COLONIES && planet->planet_type != PLANET_TYPE_GAS_GIANT) {
            if (MOX::colony[colony_idx].outpost_flag == 0) {
                climate = (uint16_t)(uint8_t)MOX::colony[colony_idx].climate;
            }
        }

        int16_t entry_idx;
        if (planet->planet_type == PLANET_TYPE_GAS_GIANT) {
            entry_idx = PLANET_PICTURE_COUNT;
        } else {
            entry_idx = (int16_t)climate * PLANET_SIZE_COUNT + (int16_t)size;
        }

        return (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", (int)entry_idx + 0x5c, MOX::global_cache_seg);
    }

    void __cdecl Potential_Colony_Info_Popup(int16_t planet_idx) {
        char bonus_msg_buf[32];
        char penalty_msg_buf[32];
        char fmt_text[200];
        char line_text[100];
        char val_text[32];
        char food_text[16];
        char title_text[80];
        char main_text[1000];
        s_potential_colony_info info;

        strlcpy(bonus_msg_buf, HAROLD::H_Message(HESTR_16E_BONUS), sizeof(bonus_msg_buf));
        strlcpy(penalty_msg_buf, HAROLD::H_Message(HESTR_16F_PENALTY), sizeof(penalty_msg_buf));

        COLCALC::Compute_Potential_Colony_Info(
            &MOX::player[MOX::PLAYER_NUM],
            &MOX::planet[planet_idx],
            &info);

        /* fmtpara: \aM25,30. adjusts paragraph margins, \x1A left-align/right-align control code prefixes the following digit. */
        snprintf(fmt_text, sizeof(fmt_text), "\aM25,30."
                                             "\x1A"
                                             "0%s",
            HAROLD::H_Message(HESTR_170_MAX_POPULATION));
        strlcat(fmt_text, "\x1A"
                          "1",
            sizeof(fmt_text));
        strlcat(fmt_text, "%d\n", sizeof(fmt_text));

        int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(&MOX::planet[planet_idx], MOX::PLAYER_NUM);
        snprintf(main_text, sizeof(main_text), fmt_text, (int)max_pop);

        /* fmtpara: \x1A control code prefixes the following alignment digit. */
        COLCALC::Format_Food2(
            (uint16_t)info.food,
            food_text,
            sizeof(food_text));
        snprintf(line_text, sizeof(line_text), "\x1A"
                                               "0%s"
                                               "\x1A"
                                               "1%s\n",
            HAROLD::H_Message(HESTR_171_FOOD_PER_FARMER),
            food_text);
        strlcat(main_text, line_text, sizeof(main_text));

        snprintf(line_text, sizeof(line_text), "\x1A"
                                               "0%s"
                                               "\x1A"
                                               "1%d\n",
            HAROLD::H_Message(HESTR_172_INDUSTRY_PER_WORKER),
            (int)info.industry);
        strlcat(main_text, line_text, sizeof(main_text));

        snprintf(line_text, sizeof(line_text), "\x1A"
                                               "0%s"
                                               "\x1A"
                                               "1%d\n",
            HAROLD::H_Message(HESTR_173_RESEARCH_PER_SCIENTIST),
            (int)info.research);
        strlcat(main_text, line_text, sizeof(main_text));

        if (info.maintenance == 0) {
            /* fmtpara: \aM25,27. changes the paragraph margin before the following status line. */
            strlcat(main_text, "\aM25,27.", sizeof(main_text));
            snprintf(line_text, sizeof(line_text), "\x1A"
                                                   "0%s"
                                                   "\x1A"
                                                   "1--\n",
                HAROLD::H_Message(HESTR_174_NO_MAINTENANCE_BONUS));
            strlcat(main_text, line_text, sizeof(main_text));
        } else {
            /* fmtpara: \aM25,17. changes the paragraph margin before the following status line. */
            strlcat(main_text, "\aM25,17.", sizeof(main_text));
            snprintf(line_text, sizeof(line_text), "\x1A"
                                                   "0%s",
                HAROLD::H_Message(HESTR_175_MAINTENANCE));

            int16_t msg_id = (info.maintenance <= 0) ? HESTR_189_MAINTENANCE_BONUS : HESTR_18A_MAINTENANCE_PENALTY;
            snprintf(line_text, sizeof(line_text), "\x1A"
                                                   "0%s"
                                                   "\x1A"
                                                   "1",
                HAROLD::H_Message(msg_id));

            int abs_maint = abs((int)info.maintenance);
            snprintf(val_text, sizeof(val_text), "%d%%\n", abs_maint);

            strlcat(line_text, val_text, sizeof(line_text));
            strlcat(main_text, line_text, sizeof(main_text));
        }

        if (info.worker_bonus == 0) {
            strlcat(main_text, "\aM25,27.", sizeof(main_text));
            snprintf(line_text, sizeof(line_text), "\x1A"
                                                   "0%s"
                                                   "\x1A"
                                                   "1--\n",
                HAROLD::H_Message(HESTR_176_NO_WORKER_BONUS));
            strlcat(main_text, line_text, sizeof(main_text));
        } else {
            strlcat(main_text, "\aM25,17.", sizeof(main_text));
            snprintf(line_text, sizeof(line_text), "\x1A"
                                                   "0%s",
                HAROLD::H_Message(HESTR_177_WORKER));

            int16_t msg_id = (info.worker_bonus > 0) ? HESTR_18B_WORKER_BONUS : HESTR_18C_WORKER_PENALTY;
            snprintf(line_text, sizeof(line_text), "\x1A"
                                                   "0%s"
                                                   "\x1A"
                                                   "1",
                HAROLD::H_Message(msg_id));

            int abs_bonus = abs((int)info.worker_bonus);
            snprintf(val_text, sizeof(val_text), "%d%%\n", abs_bonus);

            strlcat(line_text, val_text, sizeof(line_text));
            strlcat(main_text, line_text, sizeof(main_text));
        }

        strlcpy(fmt_text, HAROLD::H_Message(HESTR_178_COLONY_INFO_FOR_S), sizeof(fmt_text));
        char* planet_name = HACCESS::Get_Planet_Name_No_Prime(planet_idx);
        snprintf(title_text, sizeof(title_text), fmt_text, planet_name);

        TEXTBOX::Text_Box(title_text, main_text);
    }

    int __cdecl qsort_sys_disp(const void* a, const void* b) {
        const s_sys_disp* p1 = (const s_sys_disp*)a;
        const s_sys_disp* p2 = (const s_sys_disp*)b;
        return (int)p1->y - (int)p2->y;
    }

    void __cdecl Draw_System_Display_Border(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t x_offset, int16_t y_offset, int16_t mode) {
        (void)x_offset;
        (void)y_offset;
        MOX::moveable_box_pict[0] = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x49, (void*)MOX::global_cache_seg);
        animate::Draw(content_w + base_x, (int16_t)(content_h + mode + base_y), MOX::moveable_box_pict[0]);
    }

    void __cdecl Draw_Colony_Marker(int16_t x, int16_t y, int16_t owner) {
        uint8_t color = (uint8_t)MOX::player[owner].color;
        s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", (int16_t)color + 0x23, (void*)MOX::global_cache_seg);
        MOX::colony_marker_seg[color] = anim;
        int16_t width = animate::Get_Width(anim);
        animate::Draw(x - width + 2, y, anim);
    }

    void __cdecl Draw_Outpost_Marker(int16_t x, int16_t y, int16_t owner) {
        uint8_t color = (uint8_t)MOX::player[owner].color;
        s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", (int16_t)color + 0x2b, (void*)MOX::global_cache_seg);
        MOX::outpost_marker_seg[color] = anim;
        int16_t width = animate::Get_Width(anim);
        animate::Draw(x - width, y, anim);
    }

    void __cdecl Remap_Draw_With_Mask(int16_t x, int16_t y, s_animation_header* anim_hdr, s_animation_header* mask_hdr) {
        bool is_clipped = false;
        int16_t x2;
        int16_t y2;

        if (animate::clipping_flag == 1) {
            if (video::screen_window_x2 < x) return;
            if (video::screen_window_y2 < y) return;

            x2 = animate::animation_header->width + x - 1;
            if (x2 < video::screen_window_x1) return;

            y2 = animate::animation_header->height + y - 1;
            if (y2 < video::screen_window_y1) return;

            if (x < video::screen_window_x1 ||
                y < video::screen_window_y1 ||
                x2 > video::screen_window_x2 ||
                y2 > video::screen_window_y2) {
                is_clipped = true;
            }
        }

        animate::animation_header = anim_hdr;
        SYS::mask_header = mask_hdr;

        uint8_t* anim_frame = Animation_Frame_Data_(anim_hdr, anim_hdr->current_frame);
        int8_t* mask_frame = (int8_t*)Animation_Frame_Data_(mask_hdr, mask_hdr->current_frame);

        if (!is_clipped) {
            if (video::screen_refresh_flag >= 2) {
                setvesa::Add_Square_To_Scan_List(x, y, anim_hdr->width + x - 1, anim_hdr->height + y - 1);
                setvesa::Add_Square_To_Scan_List(x + 100, y + 100, anim_hdr->width + x + 99, anim_hdr->height + y + 99);
            }

            switch (static_cast<uint8_t>(anim_hdr->flags) & ANIMATION_FLAG_DRAW_MODE_MASK) {
                case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                    SYS::Remap_Draw_Animated_Sprite_Mask(x, y, anim_frame, mask_frame);
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
        } else {
            if (animate::dot_stencil_flag == 0) {
                if (video::screen_refresh_flag >= 2) {
                    setvesa::Add_Square_To_Scan_List(x, y, x2, y2);
                }

                switch (static_cast<uint8_t>(anim_hdr->flags) & ANIMATION_FLAG_DRAW_MODE_MASK) {
                    case ANIMATION_DRAW_MODE_ANIMATED_SPRITE:
                        SYS::Remap_Clipped_Draw_Animated_Sprite_Mask(x, y, anim_frame, mask_frame);
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
        }

        SYS::mask_header->current_frame++;
        animate::animation_header->current_frame++;

        if (static_cast<uint8_t>(animate::animation_header->flags) & 0x20) {
            int16_t loop_frame = animate::animation_header->loop_frame;
            if (animate::animation_header->current_frame > loop_frame) {
                if (loop_frame - animate::animation_header->current_frame == -1) {
                    animate::animation_header->current_frame = 0;
                } else if (animate::animation_header->current_frame >= animate::animation_header->frame_count) {
                    animate::animation_header->current_frame = animate::animation_header->frame_count - 1;
                }
            }
        } else {
            if (animate::animation_header->current_frame >= animate::animation_header->frame_count) {
                animate::animation_header->current_frame = animate::animation_header->loop_frame;
            }
        }

        if (static_cast<uint8_t>(SYS::mask_header->flags) & 0x20) {
            int16_t loop_frame = SYS::mask_header->loop_frame;
            if (SYS::mask_header->current_frame > loop_frame) {
                if (loop_frame - SYS::mask_header->current_frame == -1) {
                    SYS::mask_header->current_frame = 0;
                } else if (SYS::mask_header->current_frame >= SYS::mask_header->frame_count) {
                    SYS::mask_header->current_frame = SYS::mask_header->frame_count - 1;
                }
            }
        } else {
            if (SYS::mask_header->current_frame >= SYS::mask_header->frame_count) {
                SYS::mask_header->current_frame = SYS::mask_header->loop_frame;
            }
        }
    }

    void __cdecl Draw_Planet_With_Pierced_Ring(
        int16_t planet_idx,
        int16_t sys_disp_idx,
        int16_t x_offset_1,
        int16_t y_offset_1,
        int16_t x_offset_2,
        int16_t y_offset_2,
        int16_t y_offset_3,
        int16_t x_offset_3) {
        const int32_t sys_slot = static_cast<int32_t>(sys_disp_idx);
        const int16_t orbit_idx = static_cast<int16_t>(MOX::sys_disp[sys_slot].orbit_idx);
        const int16_t orbit_pos = static_cast<int16_t>(MOX::sys_disp[sys_slot].oribit_pos);
        const int16_t planet_type = static_cast<int16_t>(MOX::planet[planet_idx].planet_type);
        const int32_t planet_x = static_cast<int32_t>(MOX::sys_disp[sys_slot].x);
        const int32_t planet_y = static_cast<int32_t>(MOX::sys_disp[sys_slot].y);

        allocate::Push_Block(MOX::screen_seg);

        int16_t planet_mask_index;
        if (planet_type == PLANET_TYPE_PLANET) {
            planet_mask_index = static_cast<int16_t>(MOX::planet[planet_idx].size);
        } else {
            planet_mask_index = PLANET_SIZE_HUGE;
        }

        int32_t bitmap_size = bitmap::Get_Bitmap_Size(0x120, 0x9a);
        SYS::sys_disp_bitmap = allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size + 10);

        bitmap_size = bitmap::Get_Bitmap_Size(0x22, 0x22);
        void* outline_bitmap = allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size + 10);

        const int16_t ring_base_x = static_cast<int16_t>(x_offset_2 + x_offset_3 + x_offset_1);
        const int16_t ring_draw_x = static_cast<int16_t>(ring_base_x + 0x10);
        const int16_t ring_base_y = static_cast<int16_t>(y_offset_1 + y_offset_3 + y_offset_2);
        const int16_t ring_draw_y = static_cast<int16_t>(ring_base_y + 0x0c);

        MOX::o_ring_seg[orbit_idx] =
            (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", orbit_idx + 0x4d, MOX::global_cache_seg);
        animate::Get_Width(MOX::o_ring_seg[orbit_idx]);
        animate::Get_Height(MOX::o_ring_seg[orbit_idx]);

        s_animation_header* ring_picture =
            (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", orbit_idx + 0x4d, MOX::global_cache_seg);
        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)SYS::sys_disp_bitmap, ring_picture);

        const int16_t planet_screen_x = static_cast<int16_t>(ring_base_x + planet_x + 0x9f);
        const int16_t planet_screen_y = static_cast<int16_t>(ring_base_y + planet_y + 0x58);

        MOX::planet_mask_seg[planet_mask_index] =
            (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", planet_mask_index + 0x8f, MOX::global_cache_seg);
        animate::Set_Animation_Frame(MOX::planet_mask_seg[planet_mask_index], orbit_pos);

        MOX::sys_disp[sys_slot].pic = SYS::Get_Rotating_Planet_Segment(
            MOX::sys_disp[sys_slot].orbit,
            orbit_pos,
            0);
        remap::Calculate_Picture_Remap_Colors(MOX::sys_disp[sys_slot].pic);

        bitmap::Create_Blank_Bitmap(0x23, 0x23, outline_bitmap);
        bitmap::Add_Picture_To_Bitmap(2, 2, MOX::sys_disp[sys_slot].pic, (s_animation_header*)outline_bitmap);
        bitmap::Outline_Bitmap((s_animation_header*)outline_bitmap, (s_animation_header*)outline_bitmap, 0x1f, 0);
        bitmap::Outline_Bitmap((s_animation_header*)outline_bitmap, (s_animation_header*)outline_bitmap, 0x1e, 0);
        bitmap::Mask_Out_Bitmap_With_Bitmap(
            static_cast<int16_t>(planet_screen_x - ring_draw_x - 2),
            static_cast<int16_t>(planet_screen_y - ring_draw_y - 2),
            (s_animation_header*)SYS::sys_disp_bitmap,
            (s_animation_header*)outline_bitmap,
            0);
        animate::Draw(ring_draw_x, ring_draw_y, (s_animation_header*)SYS::sys_disp_bitmap);

        MOX::planet_mask_seg[planet_mask_index] =
            (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", planet_mask_index + 0x8f, MOX::global_cache_seg);
        animate::Set_Animation_Frame(MOX::planet_mask_seg[planet_mask_index], orbit_pos);

        MOX::sys_disp[sys_slot].pic = SYS::Get_Rotating_Planet_Segment(
            MOX::sys_disp[sys_slot].orbit,
            orbit_pos,
            0);

        const int16_t frame_count = animate::Get_Animation_Frame_Count(MOX::sys_disp[sys_slot].pic);
        animate::Set_Animation_Frame(
            MOX::sys_disp[sys_slot].pic,
            static_cast<int32_t>(MOX::ship_frame % frame_count));

        SYS::Remap_Draw_With_Mask(
            planet_screen_x,
            planet_screen_y,
            MOX::sys_disp[sys_slot].pic,
            MOX::planet_mask_seg[planet_mask_index]);

        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Draw_System_Display_Planets(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t y_offset, int16_t x_offset) {
        for (int16_t i = 0; i < 5; ++i) {
            int16_t planet_idx = MOX::sys_disp[i].orbit;
            int8_t orbit_idx = MOX::sys_disp[i].orbit_idx;
            if (planet_idx > -1) {
                if (MOX::planet[planet_idx].planet_type == PLANET_TYPE_PLANET || MOX::planet[planet_idx].planet_type == PLANET_TYPE_GAS_GIANT) {
                    Draw_Planet_With_Pierced_Ring(planet_idx, i, base_x, base_y, content_w, content_h, y_offset, x_offset);
                } else {
                    MOX::asteroid_seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x5b, (void*)MOX::global_cache_seg);
                    animate::Set_Animation_Frame(MOX::asteroid_seg, (int)orbit_idx);
                    animate::Draw(x_offset + content_w + base_x + 0x10, y_offset + content_h + base_y + 0xc, (s_animation_header*)MOX::asteroid_seg);
                }
            }
        }
    }

    void __cdecl Draw_Sun_Seg(int16_t star_idx, int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t y_offset, int16_t x_offset) {
        system_display_sun_seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", MOX::star[star_idx].spectral_class + 0x53, (void*)MOX::global_cache_seg);

        int16_t box_w = MOX::moveable_box[0].width;
        int16_t box_h = MOX::moveable_box[0].height;
        int16_t anim_h = animate::Get_Height(system_display_sun_seg);
        int16_t anim_w = animate::Get_Width(system_display_sun_seg);

        if (MOX::current_screen == SCREEN_OFFICERS) {
            animate::Draw(0x1c0, 0x5c, system_display_sun_seg);
        } else {
            // BUG: original code cross-references widths/heights
            int16_t draw_x = (int16_t)((box_w - anim_h) / 2) + content_w + base_x;
            int16_t draw_y = (int16_t)((box_h - anim_w) / 2) + content_h + base_y;
            animate::Draw(draw_x, draw_y, system_display_sun_seg);
        }
    }

    void __cdecl Draw_System_Special_Popup(int16_t x, int16_t y, int16_t special_idx, int16_t anim_idx) {
        const char* desc = MAINPUPS::System_Special_Desc(special_idx);
        const char* name = MOX::planet_special_string[special_idx];
        s_animation_header* anim = MAINPUPS::System_Special_Anim(anim_idx, special_idx);

        MAINPUPS::Draw_Special_Info_Popup(x, y, anim, name, desc);
    }

    void __cdecl Print_Planet_Info_Short(int16_t planet_idx, int16_t* box_width, int16_t* box_height) {
        Do_Print_Planet_Info(0, 0, 0, 0, planet_idx, 0, 0, box_width, box_height, 0);
    }

    void __cdecl Print_Planet_Info(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t planet_idx, int16_t x_offset, int16_t y_offset) {
        int16_t out_width;
        int16_t out_height;

        Do_Print_Planet_Info(base_x, base_y, content_w, content_h, planet_idx, x_offset, y_offset, &out_width, &out_height, 1);
    }

    void __cdecl Draw_System_Highlight(int16_t planet_idx, int16_t rect_w, int16_t rect_h, int16_t x_offset, int16_t y_offset) {
        int16_t colony_idx = MOX::planet[planet_idx].colony_index;
        MOX::o_highlight_seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x4b, (void*)MOX::global_cache_seg);
        int16_t w = animate::Get_Width(MOX::o_highlight_seg);
        int16_t h = animate::Get_Height(MOX::o_highlight_seg);

        bool replaced = false;
        uint8_t player_color = 0;

        if (colony_idx < 0) {
            for (int i = 0; i < 9; ++i) {
                replace::Replace_Color(MOX::o_highlight_seg, (int16_t)(0x49 + i), (int8_t)(0x06 + i));
            }
            animate::Draw((short)(((int)rect_w - (int)w) / 2) + x_offset, (short)(((int)rect_h - (int)h) / 2) + y_offset, MOX::o_highlight_seg);
            for (int i = 0; i < 9; ++i) {
                replace::Replace_Color(MOX::o_highlight_seg, (int16_t)(0x06 + i), (int8_t)(0x49 + i));
            }
        } else {
            player_color = (uint8_t)MOX::player[MOX::colony[colony_idx].owner].color;
            if (player_color != 0) {
                uint8_t base_color = MOX::main_palette_player_colors[player_color];
                for (int i = 0; i < 9; ++i) {
                    replace::Replace_Color(MOX::o_highlight_seg, (int16_t)(0x49 + i), (int8_t)(base_color + i));
                }
                replaced = true;
            }
            animate::Draw((short)(((int)rect_w - (int)w) / 2) + x_offset, (short)(((int)rect_h - (int)h) / 2) + y_offset, MOX::o_highlight_seg);
            if (replaced) {
                uint8_t base_color = MOX::main_palette_player_colors[player_color];
                for (int i = 0; i < 9; ++i) {
                    replace::Replace_Color(MOX::o_highlight_seg, (int16_t)(base_color + i), (int8_t)(0x49 + i));
                }
            }
        }
    }

    void __cdecl Hide_System_Display(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t x_offset, int16_t y_offset, int8_t fill_mode) {
        int16_t x1, y1, x2, y2;

        if (MOX::current_screen == SCREEN_OFFICERS) {
            x1 = 0x132;
            y1 = 0x11;
            x2 = 0x270;
            y2 = 0xc5;
        } else {
            x2 = x_offset + content_w + base_x + 0x13f;
            x1 = x_offset + content_w + base_x - 1;
            y2 = content_h + base_y + y_offset + 0xb6;
            y1 = content_h + base_y + y_offset - 1;
        }

        s_colors font_colors = *MISC::Font_Colors2(4, 'n', 's');

        if (fill_mode != 0) {
            int16_t clipped_y2 = video::screen_window_y2;
            if (y2 <= clipped_y2) {
                clipped_y2 = y2;
            }
            MISC::Diagonal_Line_Fill(x1, y1, x2 - 5, clipped_y2, 0x10, '=', ';');
            fonts::Set_Font_Style(4, &font_colors);
        }
    }

    void __cdecl Calculate_Remap_Gray_Palettes() {
        remap::Sort_Palette_List();

        for (int i = 0; i < 10; ++i) {
            int scale = 100 - remap_gray_percentages[i];
            for (int j = 0; j < 256; ++j) {
                const s_palette_entry& entry = video::current_palette[j];
                remap_gray_mask_palette[i][j] = remap::Find_Closest_Color(
                    static_cast<uint8_t>((entry.r * scale) / 100),
                    static_cast<uint8_t>((entry.g * scale) / 100),
                    static_cast<uint8_t>((entry.b * scale) / 100));
            }
        }
    }

    void __cdecl Draw_System_Display_Popup(int16_t star_idx, int8_t draw_border, const char* title_text, int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t x_offset, int16_t y_offset) {
        char title_buffer[100];

        uint16_t ship_frame = MOX::ship_frame;
        MOX::ship_frame = (ship_frame + 1) % 32000;
        if (MOX::current_screen == SCREEN_MAIN) {
            graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
            video::Clip_On();
        }

        MOX::system_display_base_x = base_x;
        MOX::system_display_base_y = base_y;

        if ((MOX::look_at_colonies == '\0') && !colony_read::System_Settlements_Visible(star_idx, MOX::PLAYER_NUM)) {
            if (!HAROLD::Player_Sighted_Monster(MOX::PLAYER_NUM, star_idx)) {
                Hide_System_Display(base_x, base_y, content_w, content_h, x_offset, y_offset, '\0');
                if (draw_border == '\0') {
                    return;
                }
                Draw_System_Display_Border(base_x, base_y, content_w, content_h, x_offset, y_offset, 0);
                const char* title_text;
                if (HAROLD::Contact_With_One_Colony(star_idx) == '\0') {
                    title_text = HAROLD::H_Message(0x16b);
                } else {
                    title_text = HAROLD::H_Message(0x16a);
                    snprintf(title_buffer, sizeof(title_buffer), title_text, &MOX::star[star_idx]);
                    title_text = title_buffer;
                }
                Print_System_Display_Box_Title(base_x, base_y, content_w, content_h, title_text);
                Print_Empty_System_Data(star_idx);
                return;
            }

            Hide_System_Display(base_x, base_y, content_w, content_h, x_offset, y_offset, '\x01');
            if (draw_border != '\0') {
                Draw_System_Display_Border(base_x, base_y, content_w, content_h, x_offset, y_offset, 1);
            }

            if (MOX::current_screen == SCREEN_OFFICERS) {
                graphics::Save_Window();
                graphics::Set_Window(0, 0, 0x27f, 0x1df);
                video::Clip_On();
                Draw_System_Special_Popup(content_w + base_x - 5, y_offset + content_h + base_y + 0x28, SYSTEM_SPECIAL_SPACE_MONSTER, star_idx);
                graphics::Restore_Window();
                video::Clip_Off();
                return;
            }

            Draw_System_Special_Popup(content_w + base_x + 5, y_offset + content_h + base_y, SYSTEM_SPECIAL_SPACE_MONSTER, star_idx);
            const char* title_fmt = HAROLD::H_Message(0x16a);
            snprintf(title_buffer, sizeof(title_buffer), title_fmt, &MOX::star[star_idx]);
            Print_System_Display_Box_Title(base_x, base_y, content_w, content_h, title_buffer);
            return;
        }

        if (draw_border != '\0') {
            Draw_System_Display_Border(base_x, base_y, content_w, content_h, x_offset, y_offset, 1);
        }

        MOX::system_star_id_for_auto_select = star_idx;
        int16_t star_index = star_idx;
        if (MOX::star[star_index].wormhole_star_id > -1) {
            int16_t wormhole_star_id = MOX::star[star_index].wormhole_star_id;
            s_colors* wormhole_font_colors = MISC::Font_Colors2(1, 'n', 's');
            s_colors wormhole_title_colors;
            memcpy(&wormhole_title_colors, wormhole_font_colors, sizeof(s_colors));
            fonts::Set_Font_Style(1, &wormhole_title_colors);

            short wormhole_text_right_x;
            short wormhole_text_y;
            if (MOX::current_screen == SCREEN_OFFICERS) {
                wormhole_text_right_x = 0x270;
                wormhole_text_y = 0xbb;
            } else {
                wormhole_text_right_x = base_x + 0x149 + content_w;
                wormhole_text_y = content_h;
                short font_height = fonts::Get_Font_Height();
                wormhole_text_y = (wormhole_text_y - font_height) + 0xe4 + base_y;
            }

            char wormhole_info_text[100];
            wormhole_info_text[0] = '\0';

            if (MOX::star[star_index].wormhole_star_id <= -1) {
                if (HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx)) {
                    const char* special_text = MOX::planet_special_string[MOX::star[star_index].system_special];
                    strlcpy(wormhole_info_text, special_text, sizeof(wormhole_info_text));
                }
            } else {
                if (HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, wormhole_star_id)) {
                    const char* wormhole_fmt = HAROLD::H_Message(0x16c);
                    snprintf(wormhole_info_text, sizeof(wormhole_info_text), wormhole_fmt, &MOX::star[wormhole_star_id]);
                } else {
                    const char* unknown_wormhole_text = HAROLD::H_Message(0x16d);
                    strlcpy(wormhole_info_text, unknown_wormhole_text, sizeof(wormhole_info_text));
                }
            }

            if (wormhole_info_text[0] != '\0') {
                fonts::Print_Right(wormhole_text_right_x, wormhole_text_y, wormhole_info_text);
            }
        }

        if (draw_sun_before_planet != '\0') {
            Draw_Sun_Seg(star_idx, base_x, base_y, content_w, content_h, y_offset, x_offset);
        }

        if (skip_draw_system_planets == '\0') {
            Draw_System_Display_Planets(base_x, base_y, content_w, content_h, y_offset, x_offset);
            animate::clipping_flag = 1;
        } else {
            skip_draw_system_planets = '\0';
        }

        if (draw_sun_before_planet == '\0') {
            Draw_Sun_Seg(star_idx, base_x, base_y, content_w, content_h, y_offset, x_offset);
        }

        for (short orbit_slot = 0; orbit_slot < 5; ++orbit_slot) {
            s_sys_disp* orbit_display = &MOX::sys_disp[orbit_slot];
            if (orbit_display->orbit > -1) {
                s_planet_data* planet = &MOX::planet[orbit_display->orbit];
                int8_t planet_size = planet->size;
                if (planet->planet_type == PLANET_TYPE_GAS_GIANT) {
                    planet_size = PLANET_SIZE_HUGE;
                }
                short marker_x = orbit_display->x + x_offset + content_w + 0x9f + base_x;
                short marker_y = orbit_display->y + y_offset + content_h + 0x58 + base_y;

                if (planet->colony_index > -1) {
                    if (planet->planet_type == PLANET_TYPE_ASTEROID) {
                        short offset = (short)(MOX::rot_plan_dim[planet_size] >> 1);
                        marker_x += offset;
                        marker_y += offset;
                    }
                    short colony_owner = (short)MOX::colony[planet->colony_index].owner;
                    if (MOX::colony[planet->colony_index].outpost_flag == '\0') {
                        Draw_Colony_Marker(marker_x, marker_y, colony_owner);
                    } else {
                        Draw_Outpost_Marker(marker_x, marker_y, colony_owner);
                    }
                }
            }
        }

        for (short orbit_slot = 0; orbit_slot < 5; ++orbit_slot) {
            int16_t planet_idx = MOX::sys_disp[orbit_slot].orbit;
            if (planet_idx > -1) {
                int8_t planet_size = MOX::planet[planet_idx].size;
                if (MOX::planet[planet_idx].planet_type == PLANET_TYPE_GAS_GIANT) {
                    planet_size = PLANET_SIZE_HUGE;
                }
                short highlight_x = (short)(MOX::sys_disp[orbit_slot].x + x_offset + content_w + 0x9f + base_x);
                short highlight_y = (short)(MOX::sys_disp[orbit_slot].y + y_offset + content_h + 0x58 + base_y);

                if (MOX::sys_disp[orbit_slot].orbit_idx == MOX::system_icon_scanned) {
                    if (MOX::planet[planet_idx].planet_type != PLANET_TYPE_ASTEROID) {
                        uint16_t dim = (uint16_t)MOX::rot_plan_dim[planet_size];
                        Draw_System_Highlight(planet_idx, dim, dim, highlight_x, highlight_y);
                    }
                    short info_box_width, info_box_height;
                    Print_Planet_Info_Short(planet_idx, &info_box_width, &info_box_height);
                    MISC::Draw_Darkened_Box(x_offset + content_w + base_x, y_offset + content_h + base_y, info_box_width + 9, info_box_height + 4, 0x3f);
                    Print_Planet_Info(base_x, base_y, content_w, content_h, planet_idx, x_offset, y_offset);
                }
            }
        }

        Draw_System_Ship_Icons(base_x, base_y, content_w, content_h, y_offset, x_offset);
        Draw_Gate_Icons(star_idx, base_x, base_y, content_w, content_h, x_offset);

        if ((MOX::system_icon_scanned > 1999) && (MOX::system_icon_scanned < 2004)) {
            Print_Gate_Info(star_idx, MOX::system_icon_scanned - 2000, base_x, base_y, content_w, content_h, x_offset, y_offset);
        }

        if (MOX::current_screen != SCREEN_OFFICERS) {
            Print_System_Display_Box_Title(base_x, base_y, content_w, content_h, title_text);
        }
    }

    void __cdecl N_Ships_By_Type_In_Stack(int16_t ship_node_idx, int16_t* total_count, int16_t* colony_ships_count, int16_t* outposts_count, int16_t* transports_count, int16_t* ship_sizes) {
        ship_read::s_composition composition;
        while (ship_node_idx != -1) {
            const int16_t ship_idx = MOX::ship_node[ship_node_idx].ship_idx;
            ship_read::Count_Member(&MOX::ship[ship_idx].d, &composition);
            ship_node_idx = MOX::ship_node[ship_node_idx].next_node;
        }
        *total_count = composition.total;
        *colony_ships_count = composition.counts[6];
        *outposts_count = composition.counts[7];
        *transports_count = composition.counts[8];
        memcpy(ship_sizes, composition.counts, (SHIP_SIZE_DOOM_STAR + 1) * sizeof(int16_t));
    }

    void __cdecl Get_Gate_Icon_XY(int16_t x, int16_t y, int16_t off_x, int16_t off_y, int16_t unused_arg, int16_t* out_x, int16_t* out_y) {
        (void)unused_arg;
        *out_x = x + off_x + 0xE9;
        *out_y = y + off_y + 0xF3;

        if (MOX::current_screen == SCREEN_OFFICERS) {
            *out_x += 0x3C;
            *out_y -= 0x46;
        }
    }

    void __cdecl Draw_Gate_Icons(int16_t star_idx, int16_t x, int16_t y, int16_t width, int16_t height, int32_t unused_arg) {
        int16_t out_x;
        int16_t out_y;

        SYS::Get_Gate_Icon_XY(x, y, width, height, (int16_t)unused_arg, &out_x, &out_y);

        if (MOX::current_screen == SCREEN_OFFICERS) {
            out_y -= 6;
        }

        s_star_data* star = &MOX::star[star_idx];

        if (HAROLD::Star_Has_Artemis_Net(star)) {
            out_x -= 12;
            MOX::gate_icon_seg[14] = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 33 /* LBX_ENTRY_ARTEMIS */, MOX::global_cache_seg);
            animate::Draw(out_x, out_y, MOX::gate_icon_seg[14]);
        }

        if (HAROLD::Star_Has_Dimensional_Portal(star)) {
            out_y += 1;
            out_x -= 12;
            MOX::gate_icon_seg[2] = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 21 /* LBX_ENTRY_DIMENSIONAL_PORTAL */, MOX::global_cache_seg);
            animate::Draw(out_x, out_y, MOX::gate_icon_seg[2]);
        }
    }

    void __cdecl Print_System_Display_Box_Title(int16_t x_offset, int16_t y_offset, int16_t box_x, int16_t box_y, const char* text) {
        s_colors colors;
        int16_t text_width;
        int16_t current_spacing;
        int16_t current_style;
        int16_t font_height;
        int16_t image_width;
        int16_t print_x;
        int16_t print_y;

        s_colors* temp_colors = MISC::Font_Colors2(4, 0x3c /* '<' */, 0x41 /* 'A' */);
        colors = *temp_colors;

        fonts::Set_Font_Style(4, &colors);
        text_width = fonts::Get_String_Width(text);

        if (text_width > 265 /* 0x109 */) {
            fonts::Set_Font_Style(3, &colors);
        }

        if (text_width < 60 /* 0x3c */) {
            fonts::Set_Font_Spacing(4);
        } else {
            fonts::Set_Font_Spacing(2);
        }

        current_spacing = MISC::Get_Font_Spacing();
        current_style = fonts::current_font_style;
        font_height = fonts::Get_Font_Height();

        MOX::moveable_box_pict[0] = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x49, (void*)MOX::global_cache_seg);
        image_width = animate::Get_Width(MOX::moveable_box_pict[0]);

        print_x = x_offset + box_x + (image_width / 2);
        print_y = ((20 - (font_height + 1)) / 2) + box_y + y_offset + 17;

        MISC::Set_Fitted_Font_Style((char*)text, &colors, current_style, current_spacing, 0, 0x131, 1);

        current_style = MISC::Get_Font_Style();
        current_spacing = MISC::Get_Font_Spacing();

        temp_colors = MISC::Font_Colors2(current_style, 0x3c /* '<' */, 0x41 /* 'A' */);
        colors = *temp_colors;

        fonts::Set_Font_Style(current_style, &colors);
        fonts::Set_Font_Spacing(current_spacing);

        if (current_style < 3) {
            print_y = (print_y - current_style) + 3;
        }

        MISC::_Print_Centered_Shadow_Down(print_x, print_y, 0x3d /* '=' */, text);
    }

    s_animation_header* __cdecl Buffer_Reload_System_Ships(int16_t display_idx) {
        int16_t node_ship_idx = MOX::ship_node[MOX::system_display_ships[display_idx].ship_idx].ship_idx;
        int16_t owner_idx = HACCESS::Ship_Owner(node_ship_idx);

        if (owner_idx < MAX_PLAYERS) {
            // Reload buffer based on player color
            uint8_t player_color = static_cast<uint8_t>(MOX::player[owner_idx].color);
            return (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", player_color + 273 /* 0x111 */, MOX::global_cache_seg);
        }

        if (owner_idx == MAX_PLAYERS) {
            // Neutral or special owner case
            return (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 281 /* 0x119 */, MOX::global_cache_seg);
        }

        if (owner_idx > MAX_PLAYERS && owner_idx < 15) {
            // Other special owner ranges
            return (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", (owner_idx - 9) + 282 /* 0x11a */, MOX::global_cache_seg);
        }

        return nullptr;
    }

    void __cdecl Print_System_Ship_Data(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t ship_idx) {
        int16_t v1;
        int16_t v2;

        SYS::Do_Print_System_Ship_Data(base_x, base_y, content_w, content_h, ship_idx, &v1, &v2, 1);
    }

    void __cdecl Draw_System_Ship_Icons(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t y_offset, int16_t x_offset) {
        int16_t icon_scanned = (int16_t)MOX::system_icon_scanned;

        for (int16_t i = 0; i < 15; ++i) {
            int16_t ship_idx = MOX::system_display_ships[i].ship_idx;
            if (ship_idx > -1) {
                int16_t target_x = MOX::system_display_ships[i].x + content_w + base_x;
                int16_t target_y = MOX::system_display_ships[i].y + content_h + base_y;

                if (icon_scanned > 999 && (ship_idx - icon_scanned == -1000)) {
                    int16_t text_w, text_h;
                    SYS::Measure_System_Ship_Data(base_x, base_y, content_w, content_h, ship_idx, &text_w, &text_h);

                    int16_t box_x = x_offset + content_w + base_x;
                    int16_t box_y = y_offset + content_h + base_y;
                    MISC::Draw_Darkened_Box(box_x, box_y, text_w + 9, text_h + 4, 0x3F /* '?' */);

                    SYS::Print_System_Ship_Data(base_x, base_y, content_w, content_h, ship_idx);

                    MOX::system_display_ships[i].pic = SYS::Buffer_Reload_System_Ships(i);
                    animate::Get_Current_Frame(MOX::system_display_ships[i].pic);
                } else {
                    MOX::system_display_ships[i].pic = SYS::Buffer_Reload_System_Ships(i);
                    animate::Reset_Animation_Frame(MOX::system_display_ships[i].pic);
                    animate::Get_Current_Frame(MOX::system_display_ships[i].pic);
                }

                animate::Draw(target_x, target_y, MOX::system_display_ships[i].pic);
                icon_scanned = (int16_t)MOX::system_icon_scanned;
            }
        }
    }

    void __cdecl Measure_System_Ship_Data(int16_t x, int16_t y, int16_t w, int16_t h, int16_t ship_node_num, int16_t* out_w, int16_t* out_h) {
        // Mode 0 is used for measurement calculation instead of actual drawing.
        SYS::Do_Print_System_Ship_Data(x, y, w, h, ship_node_num, out_w, out_h, 0);
    }

    int16_t __cdecl Print_Special_Building_Data(
        int16_t colony_idx,
        int16_t x,
        int16_t* line_pos,
        int16_t* max_text_width,
        int16_t* total_height,
        int16_t line_spacing,
        bool draw) {
        int16_t visible_count = 0;
        s_colony* colony = &MOX::colony[colony_idx];

        for (int16_t i = 0; i < 7; ++i) {
            const int16_t building_id = colony_read::Strategic_Defense(colony, i);
            if (building_id < 0) break;

            if (colony->buildings[building_id] != 0) {
                if (visible_count == 0) {
                    *line_pos = static_cast<int16_t>(*line_pos + MISC::Get_Font_Style() + 3);
                    *total_height = static_cast<int16_t>(*total_height + MISC::Get_Font_Style() + 3);
                }

                *line_pos = static_cast<int16_t>(*line_pos + line_spacing);

                if (draw) {
                    fonts::Print(x, *line_pos, TECHDATA::buildings[building_id].name);
                } else {
                    *total_height = static_cast<int16_t>(*total_height + line_spacing);

                    const int16_t text_width =
                        fonts::Get_String_Width(TECHDATA::buildings[building_id].name);

                    if (*max_text_width < text_width) {
                        *max_text_width = text_width;
                    }
                }

                ++visible_count;
            }
        }

        return visible_count;
    }

    void __cdecl Do_Print_System_Ship_Data(
        int16_t base_x,
        int16_t base_y,
        int16_t x_offset,
        int16_t y_offset,
        int16_t ship_node_num,
        int16_t* width_out,
        int16_t* height_out,
        bool draw_text) {
        int16_t print_x;
        int16_t print_y;
        int16_t ship_idx;
        int16_t owner;
        int16_t group_a_count;
        int16_t group_b_count;
        int16_t group_c_count;
        int16_t unknown_count;
        int16_t hull_counts[SHIP_SIZE_DOOM_STAR + 1];
        s_colors font_colors;
        char line_buffer[200];
        char ship_name[80];

        ship_name[0] = '\0';
        *width_out = 0;
        *height_out = 4;

        if (MOX::current_screen == SCREEN_OFFICERS) {
            print_x = static_cast<int16_t>(base_x + x_offset + 8);
            print_y = static_cast<int16_t>(base_y + y_offset + 10);
        } else {
            print_x = static_cast<int16_t>(base_x + x_offset + 0x12);
            print_y = static_cast<int16_t>(base_y + y_offset + 0x36);
        }

        if (ship_node_num >= SHIP_NODE_UI_TAG_BASE) {
            ship_idx = MOX::ship_node[static_cast<int16_t>(ship_node_num - SHIP_NODE_UI_TAG_BASE)].ship_idx;
            strlcpy(ship_name, MOX::ship[ship_idx].d.name, sizeof(ship_name));
        } else {
            ship_idx = MOX::ship_node[ship_node_num].ship_idx;
            owner = HACCESS::Ship_Owner(ship_idx);
            if (owner >= MAX_PLAYERS) {
                strlcpy(ship_name, MOX::ship[ship_idx].d.name, sizeof(ship_name));
            }
        }

        if (static_cast<int8_t>(ship_name[0]) > 0) {
            font_colors = *MISC::Font_Colors2(1, 0x6e, 0x73);
            fonts::Set_Font_Style(2, &font_colors);
            fonts::Set_Font_Spacing(1);
            (void)fonts::Get_Font_Height();

            if (draw_text) {
                fonts::Print(print_x, print_y, ship_name);
                return;
            }

            {
                int16_t text_width = fonts::Get_String_Width(ship_name);
                if (*width_out < text_width) {
                    *width_out = text_width;
                }
            }

            *height_out = fonts::Get_Font_Height();
            return;
        }

        MOX::moveable_box_pict[0] = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x49, MOX::global_cache_seg);
        (void)animate::Get_Width(MOX::moveable_box_pict[0]);
        (void)animate::Get_Height(MOX::moveable_box_pict[0]);

        ship_idx = MOX::ship_node[ship_node_num].ship_idx;
        owner = HACCESS::Ship_Owner(ship_idx);

        {
            s_player* const player = &MOX::player[owner];
            const uint8_t palette_color =
                MOX::main_palette_player_colors[static_cast<uint8_t>(player->color)];
            int16_t visible_detail_lines;

            font_colors = *MISC::Font_Colors2(1, palette_color, static_cast<int32_t>(palette_color) + 5);
            fonts::Set_Font_Style(2, &font_colors);
            fonts::Set_Font_Spacing(1);
            const int8_t line_step =
                static_cast<int8_t>(static_cast<uint8_t>(fonts::Get_Font_Height()) + 1);

            SYS::N_Ships_By_Type_In_Stack(
                ship_node_num,
                &unknown_count,
                &group_a_count,
                &group_b_count,
                &group_c_count,
                hull_counts);

            visible_detail_lines = 0;
            if (group_c_count > 0) {
                ++visible_detail_lines;
            }
            if (group_b_count > 0) {
                ++visible_detail_lines;
            }
            if (group_a_count > 0) {
                ++visible_detail_lines;
            }

            for (int16_t hull_idx = 0; hull_idx < SHIP_SIZE_DOOM_STAR + 1; ++hull_idx) {
                if (hull_counts[hull_idx] > 0) {
                    ++visible_detail_lines;
                }
            }

            snprintf(line_buffer, sizeof(line_buffer), HAROLD::H_Message(0x163), player->race_name);
            fonts::Set_Font_Style(2, &font_colors);
            fonts::Set_Font_Spacing(1);

            if (draw_text) {
                fonts::Print(print_x, print_y, line_buffer);
            } else {
                int16_t text_width = fonts::Get_String_Width(line_buffer);
                if (*width_out < text_width) {
                    *width_out = text_width;
                }
            }

            for (int16_t hull_idx = 0; hull_idx < SHIP_SIZE_DOOM_STAR + 1; ++hull_idx) {
                const int16_t hull_count = hull_counts[hull_idx];
                if (hull_count > 0) {
                    ship_read::Composition_Line(hull_idx, hull_count, line_buffer, sizeof(line_buffer));
                    print_y = static_cast<int16_t>(print_y + static_cast<int16_t>(line_step));

                    if (draw_text) {
                        fonts::Print(static_cast<int16_t>(print_x + 3), print_y, line_buffer);
                    } else {
                        int16_t text_width =
                            static_cast<int16_t>(fonts::Get_String_Width(line_buffer) + 3);
                        if (*width_out < text_width) {
                            *width_out = text_width;
                        }
                    }
                }
            }

            if (group_a_count > 0) {
                ship_read::Composition_Line(6, group_a_count, line_buffer, sizeof(line_buffer));

                print_y = static_cast<int16_t>(print_y + static_cast<int16_t>(line_step));

                if (draw_text) {
                    fonts::Print(static_cast<int16_t>(print_x + 3), print_y, line_buffer);
                } else {
                    int16_t text_width =
                        static_cast<int16_t>(fonts::Get_String_Width(line_buffer) + 3);
                    if (*width_out < text_width) {
                        *width_out = text_width;
                    }
                }
            }

            if (group_b_count > 0) {
                ship_read::Composition_Line(7, group_b_count, line_buffer, sizeof(line_buffer));

                print_y = static_cast<int16_t>(print_y + static_cast<int16_t>(line_step));

                if (draw_text) {
                    fonts::Print(static_cast<int16_t>(print_x + 3), print_y, line_buffer);
                } else {
                    int16_t text_width =
                        static_cast<int16_t>(fonts::Get_String_Width(line_buffer) + 3);
                    if (*width_out < text_width) {
                        *width_out = text_width;
                    }
                }
            }

            if (group_c_count > 0) {
                ship_read::Composition_Line(8, group_c_count, line_buffer, sizeof(line_buffer));

                print_y = static_cast<int16_t>(print_y + static_cast<int16_t>(line_step));

                if (draw_text) {
                    fonts::Print(static_cast<int16_t>(print_x + 3), print_y, line_buffer);
                } else {
                    int16_t text_width =
                        static_cast<int16_t>(fonts::Get_String_Width(line_buffer) + 3);
                    if (*width_out < text_width) {
                        *width_out = text_width;
                    }
                }
            }

            *height_out = static_cast<int16_t>(
                static_cast<int16_t>(line_step) * static_cast<int16_t>(visible_detail_lines + 1));
        }
    }

    void __cdecl Do_Print_Planet_Info(
        int16_t left,
        int16_t top,
        int16_t inner_left,
        int16_t inner_top,
        int16_t planet_idx,
        int16_t text_left,
        int16_t text_top,
        int16_t* max_width,
        int16_t* total_height,
        bool draw) {
        int16_t special_building_data_height = 0;
        int16_t colony_owner = -1;
        s_colors font_colors;
        char title_buffer[60];
        char detail_buffer[80];

        s_planet_data* planet = &MOX::planet[planet_idx];
        const bool can_show_special_buildings = colony_read::Strategic_Defenses_Visible(
            planet->star_index, MOX::PLAYER_NUM, MOX::player_has_ship_in_system != 0);

        *max_width = 0;
        *total_height = 12;

        MOX::moveable_box_pict[0] =
            (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x49, MOX::global_cache_seg);
        (void)animate::Get_Width(MOX::moveable_box_pict[0]);
        (void)animate::Get_Height(MOX::moveable_box_pict[0]);

        font_colors = *MISC::Font_Colors2(2, 0x3c, 0x43);

        const int16_t colony_idx = planet->colony_index;
        s_colony* colony = nullptr;
        if (colony_idx > -1) {
            colony = &MOX::colony[colony_idx];
            colony_owner = static_cast<int8_t>(colony->owner);
        }

        fonts::Set_Font_Style(2, &font_colors);
        fonts::Set_Font_Spacing(1);

        const int16_t line_advance =
            static_cast<int16_t>(static_cast<uint8_t>(fonts::Get_Font_Height()) + 1);
        const int16_t text_x = static_cast<int16_t>(text_left + inner_left + left + 6);
        int16_t current_y = static_cast<int16_t>(text_top + inner_top + top + 7);

        if (planet->planet_type == PLANET_TYPE_GAS_GIANT) {
            strlcpy(title_buffer, HAROLD::H_Message(0x15f), sizeof(title_buffer));

            if (draw) {
                fonts::Print(text_x, current_y, title_buffer);
            } else {
                const int16_t width = fonts::Get_String_Width(title_buffer);
                if (*max_width < width) {
                    *max_width = width;
                }
            }
        } else if (planet->planet_type == PLANET_TYPE_ASTEROID) {
            strlcpy(title_buffer, HAROLD::H_Message(0x160), sizeof(title_buffer));

            if (draw) {
                fonts::Print(text_x, current_y, title_buffer);
            } else {
                const int16_t width = fonts::Get_String_Width(title_buffer);
                if (*max_width < width) {
                    *max_width = width;
                }
            }
        } else {
            snprintf(title_buffer, sizeof(title_buffer), "%s ", HACCESS::Get_Planet_Name_No_Prime(planet_idx));

            if (colony != nullptr) {
                snprintf(detail_buffer, sizeof(detail_buffer), "(%s", MOX::player[colony_owner].race_name);
                strlcat(title_buffer, detail_buffer, sizeof(title_buffer));

                if (colony->outpost_flag != 0) {
                    strlcat(title_buffer, HAROLD::H_Message(0x161), sizeof(title_buffer));
                } else {
                    strlcat(title_buffer, ")", sizeof(title_buffer));
                }
            }

            if (draw) {
                fonts::Print(text_x, current_y, title_buffer);
                fonts::Set_Font_Style(2, &font_colors);
                fonts::Set_Font_Spacing(1);
            } else {
                const int16_t width = fonts::Get_String_Width(title_buffer);
                if (*max_width < width) {
                    *max_width = width;
                }
            }

            colony_read::Strategic_Climate(colony, planet_idx, detail_buffer, sizeof(detail_buffer));

            if (draw) {
                current_y = static_cast<int16_t>(current_y + line_advance);
                fonts::Print(text_x, current_y, detail_buffer);
            } else {
                *total_height = static_cast<int16_t>(*total_height + line_advance);
                const int16_t width = fonts::Get_String_Width(detail_buffer);
                if (*max_width < width) {
                    *max_width = width;
                }
            }

            const int16_t max_population =
                COLCALC::Planet_Max_Population_For_Player(planet, MOX::PLAYER_NUM);

            colony_read::Strategic_Population_Text(colony, max_population, detail_buffer, sizeof(detail_buffer));

            if (draw) {
                current_y = static_cast<int16_t>(current_y + line_advance);
                fonts::Print(text_x, current_y, detail_buffer);
                if (colony != nullptr)
                    game_events::Observe_Settlement_Text(planet_idx, "system-detail", colony->outpost_flag == 0 ? detail_buffer : "", can_show_special_buildings);
            } else {
                *total_height = static_cast<int16_t>(*total_height + line_advance);
                const int16_t width = fonts::Get_String_Width(detail_buffer);
                if (*max_width < width) {
                    *max_width = width;
                }
            }

            current_y = static_cast<int16_t>(current_y + line_advance);
            *total_height = static_cast<int16_t>(*total_height + line_advance);

            if (static_cast<int8_t>(planet->gravity_class) == PLANET_GRAVITY_NORMAL_G) {
                snprintf(
                    detail_buffer,
                    sizeof(detail_buffer),
                    "%s",
                    MOX::mineral_class_string[static_cast<int8_t>(planet->mineral_class)]);
            } else {
                snprintf(
                    detail_buffer,
                    sizeof(detail_buffer),
                    "%s, %s",
                    MOX::mineral_class_string[static_cast<int8_t>(planet->mineral_class)],
                    MOX::planet_gravity_string[static_cast<int8_t>(planet->gravity_class)]);
            }

            if (draw) {
                fonts::Print(text_x, current_y, detail_buffer);
            } else {
                const int16_t width = fonts::Get_String_Width(detail_buffer);
                if (*max_width < width) {
                    *max_width = width;
                }
            }

            if ((colony != nullptr) && can_show_special_buildings) {
                special_building_data_height = SYS::Print_Special_Building_Data(
                    colony_idx,
                    text_x,
                    &current_y,
                    max_width,
                    total_height,
                    line_advance,
                    draw);
            }

            const int32_t planet_special = static_cast<int8_t>(planet->planet_special);
            if ((planet_special != 0) && (planet_special != 0xb)) {
                if (special_building_data_height == 0) {
                    current_y = static_cast<int16_t>(current_y + 4);
                    *total_height = static_cast<int16_t>(*total_height + 4);
                }

                current_y = static_cast<int16_t>(current_y + line_advance);
                *total_height = static_cast<int16_t>(*total_height + line_advance);

                if (draw) {
                    fonts::Print(text_x, current_y, MOX::planet_special_string[planet_special]);
                } else {
                    const int16_t width =
                        fonts::Get_String_Width(MOX::planet_special_string[planet_special]);
                    if (*max_width < width) {
                        *max_width = width;
                    }
                }
            }
        }

        *total_height = static_cast<int16_t>(*total_height + 2);
    }

    void __cdecl Print_Empty_System_Data(int16_t star_idx) {
        (void)star_idx;
        int16_t local_4_stack;
        int16_t paragraph_top;
        int16_t box_width;
        int16_t paragraph_width;
        int16_t paragraph_height;
        int16_t left_padding;
        int16_t origin_x;
        int16_t origin_y;
        int16_t saved_window_status;

        if (MOX::current_screen == SCREEN_MAIN) {
            left_padding = 0x0D;
            paragraph_top = 0x2F;
            box_width = static_cast<int16_t>(MOX::moveable_box[0].width - 0x1A);
            paragraph_width = static_cast<int16_t>(box_width - 0x19);
            paragraph_height = static_cast<int16_t>(MOX::moveable_box[0].height - 0x5B);
            origin_x = static_cast<int16_t>(MOX::moveable_box[0].content_offset_x + MOX::moveable_box[0].x);
            origin_y = static_cast<int16_t>(MOX::moveable_box[0].content_offset_y + MOX::moveable_box[0].y);
        } else if (MOX::current_screen == SCREEN_OFFICERS) {
            box_width = 0x013E;
            paragraph_top = 0;
            paragraph_width = 0x0125;
            paragraph_height = 0x00B4;
            origin_x = 0x0132;
            origin_y = 0x0011;
            left_padding = 0;
        } else {
            // The binary seeds these layout values from one uninitialized stack slot here.
            // Use a defined fallback while preserving the single-source dataflow shape.
            const int16_t safe_fallback = 0;
            paragraph_width = safe_fallback;
            box_width = safe_fallback;
            paragraph_height = safe_fallback;
            left_padding = safe_fallback;
            paragraph_top = safe_fallback;
            origin_x = safe_fallback;
            origin_y = safe_fallback;
        }

        local_4_stack = static_cast<int16_t>(left_padding + origin_x);
        const int16_t text_y = static_cast<int16_t>(paragraph_top + origin_y);

        MISC::Save_Window_Status(&saved_window_status);
        graphics::Set_Window(0x16, 0x16, 0x020F, 0x01A5);
        video::Clip_On();
        MISC::Change_Font_Style(3);

        const int16_t max_width =
            BILL::_Get_Paragraph_Max_Width(paragraph_width, SYS::star_description_string, 3);
        int32_t delta = (int32_t)(int16_t)box_width - (int32_t)(int16_t)max_width;
        int32_t centered = delta / 2;

        ERIC::Print_Paragraph_Centered_Vertically(
            (int16_t)(centered + (int32_t)(int16_t)local_4_stack),
            text_y,
            paragraph_width,
            paragraph_height,
            SYS::star_description_string,
            3);

        MISC::Restore_Window_Status(saved_window_status);
    }

    void __cdecl Print_Gate_Info(
        int16_t star_idx,
        uint16_t gate_type,
        int16_t x_base,
        int16_t y_base,
        int16_t x_offset,
        int16_t y_offset,
        int16_t x_extra,
        int16_t y_extra) {
        int16_t box_x = static_cast<int16_t>(
            static_cast<uint16_t>(x_offset) +
            static_cast<uint16_t>(x_base) +
            static_cast<uint16_t>(x_extra));
        int16_t box_y = static_cast<int16_t>(
            static_cast<uint16_t>(y_offset) +
            static_cast<uint16_t>(y_base) +
            static_cast<uint16_t>(y_extra));

        uint8_t player_color =
            MOX::main_palette_player_colors[static_cast<uint8_t>(MOX::player[MOX::PLAYER_NUM].color)];

        s_colors font_colors = *MISC::Font_Colors2(1, player_color, player_color + 6);
        fonts::Set_Font_Style(1, &font_colors);

        int16_t gate_label_width;
        char info_text[200];
        info_text[0] = '\0';

        switch (gate_type) {
            case 0:
                strlcpy(info_text, HAROLD::H_Message(0x17C), sizeof(info_text));
                gate_label_width = fonts::Get_String_Width(info_text);

                if (MOX::NUM_PLAYERS > 0) {
                    for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                        if (HAROLD::Player_Has_Dimensional_Portal(&MOX::player[player_idx], &MOX::star[star_idx])) {
                            strlcat(info_text, MOX::player[player_idx].race_name, sizeof(info_text));
                            strlcat(info_text, ", ", sizeof(info_text));
                        }
                    }
                }

                info_text[static_cast<int32_t>(strlen(info_text)) - 2] = '\0';
                strlcat(info_text, ")", sizeof(info_text));
                break;

            case 1:
                strlcpy(info_text, HAROLD::H_Message(0x17A), sizeof(info_text));
                gate_label_width = fonts::Get_String_Width(info_text);
                break;

            case 2:
                strlcpy(info_text, HAROLD::H_Message(0x179), sizeof(info_text));
                gate_label_width = fonts::Get_String_Width(info_text);
                break;

            case 3:
                strlcpy(info_text, HAROLD::H_Message(0x17B), sizeof(info_text));
                gate_label_width = fonts::Get_String_Width(info_text);

                if (MOX::NUM_PLAYERS > 0) {
                    for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                        if (HAROLD::Player_Has_Artemis_Net(&MOX::player[player_idx], &MOX::star[star_idx])) {
                            strlcat(info_text, MOX::player[player_idx].race_name, sizeof(info_text));
                            strlcat(info_text, ", ", sizeof(info_text));
                        }
                    }
                }

                info_text[static_cast<int32_t>(strlen(info_text)) - 2] = '\0';
                strlcat(info_text, ")", sizeof(info_text));
                break;

            default:
                break;
        }

        int16_t line_width = static_cast<int16_t>(gate_label_width + 2);
        int16_t paragraph_width = FMTPARA::Get_Formatted_Paragraph_Max_Width(line_width, info_text, 0);
        int16_t paragraph_height = FMTPARA::Get_Formatted_Paragraph_Max_Height(line_width, info_text);

        fonts::Get_Font_Height();

        paragraph_width = static_cast<int16_t>(paragraph_width + 0x0C);
        paragraph_height = static_cast<int16_t>(paragraph_height + 5);

        MISC::Draw_Darkened_Box(box_x, box_y, paragraph_width, paragraph_height, 0x3F);
        BILL::_Print_Paragraph(
            static_cast<int16_t>(box_x + 7),
            static_cast<int16_t>(box_y + 5),
            line_width,
            info_text,
            0);
    }

    void __cdecl Wrapper_For_Normal_System_Popup() {
        const int16_t star_idx = MOX::moveable_box[0].id;
        const int16_t base_x = MOX::moveable_box[0].x;
        const int16_t base_y = MOX::moveable_box[0].y;
        const int16_t content_offset_x = MOX::moveable_box[0].content_offset_x;
        const int16_t content_offset_y = MOX::moveable_box[0].content_offset_y;
        s_star_data* const star = &MOX::star[star_idx];
        char popup_text[80];

        const char* const format = HAROLD::H_Message(0x16A);
        snprintf(popup_text, sizeof(popup_text), format, star);

        SYS::Init_System_Display(star_idx);
        SYS::Draw_System_Display_Popup(
            star_idx,
            1,
            popup_text,
            base_x,
            base_y,
            content_offset_x,
            content_offset_y,
            0x0D,
            0x2F);
    }

    void __cdecl Wrapper_For_Informational_System_Fields(
        int16_t star_idx,
        int16_t base_x,
        int16_t base_y,
        int16_t orbit_offset_x,
        int16_t orbit_offset_y,
        int16_t* out_popup_title_button) {
        int16_t popup_hidden_field;
        int16_t system_field;

        SYS::Init_System_Display(star_idx);
        SYS::Add_System_Display_Popup_Fields(
            star_idx,
            base_x,
            base_y,
            0,
            0,
            orbit_offset_x,
            orbit_offset_y,
            &popup_hidden_field,
            out_popup_title_button,
            &system_field);
    }

    void __cdecl Add_System_Display_Popup_Fields(
        int16_t star_idx,
        int16_t base_x,
        int16_t base_y,
        int16_t popup_offset_x,
        int16_t popup_offset_y,
        int16_t orbit_offset_x,
        int16_t orbit_offset_y,
        int16_t* out_popup_hidden_field,
        int16_t* out_popup_title_button,
        int16_t* out_system_field) {
        int16_t gate_icon_x = 0;
        int16_t gate_icon_y = 0;
        int32_t orbit_display_x = 0;
        int32_t orbit_display_y = 0;
        int32_t orbit_display_frame = 0;

        if (star_idx <= -1) {
            return;
        }

        if (star_idx >= MOX::NUM_STARS) {
            return;
        }

        if (SYS::skip_draw_system_display != 0) {
            return;
        }

        SYS::Build_System_Popup_Ships(star_idx);

        if (out_popup_title_button != nullptr) {
            const int16_t popup_left = static_cast<int16_t>(base_x + popup_offset_x);
            const int16_t popup_top = static_cast<int16_t>(base_y + popup_offset_y);

            *out_popup_title_button = fields::Add_Clipped_Button_Field(
                static_cast<int16_t>(popup_left + 0x107),
                static_cast<int16_t>(popup_top + 0xEC),
                "",
                MOX::moveable_box_pict[4],
                "\x1B",
                0x28);

            *out_popup_hidden_field = fields::Add_Hidden_Field(
                popup_left,
                popup_top,
                static_cast<int16_t>(popup_left + MOX::moveable_box[0].width),
                static_cast<int16_t>(popup_top + 0x2F),
                "",
                0);
        } else {
            *out_popup_hidden_field = -1000;
        }

        for (int16_t orbit_slot = 4; orbit_slot >= 0; --orbit_slot) {
            const int16_t planet_idx = MOX::sys_disp[orbit_slot].orbit;

            if (planet_idx <= -1) {
                MOX::planet_buttons[orbit_slot] = -1000;
                continue;
            }

            const s_planet_data& planet = MOX::planet[planet_idx];
            if (planet.planet_type == PLANET_TYPE_ASTEROID) {
                MOX::planet_buttons[orbit_slot] = -1000;
                continue;
            }

            int8_t planet_size = planet.size;
            if (planet.planet_type == PLANET_TYPE_GAS_GIANT) {
                planet_size = PLANET_SIZE_HUGE;
            }

            GEO::Get_Orbit_Display_X_Y(
                star_idx,
                static_cast<int32_t>(planet.orbit),
                &orbit_display_x,
                &orbit_display_y,
                &orbit_display_frame);

            const uint8_t planet_diameter = MOX::rot_plan_dim[planet_size];
            const int16_t half_diameter = static_cast<int16_t>(planet_diameter / 2);

            const int16_t left = static_cast<int16_t>(
                base_x + popup_offset_x + orbit_offset_x + static_cast<int16_t>(orbit_display_x) + 0xA1 -
                half_diameter);
            const int16_t top = static_cast<int16_t>(
                base_y + popup_offset_y + orbit_offset_y + static_cast<int16_t>(orbit_display_y) + 0x5A -
                half_diameter);

            MOX::planet_buttons[orbit_slot] = fields::Add_Hidden_Field(
                left,
                top,
                static_cast<int16_t>(left + planet_diameter),
                static_cast<int16_t>(top + planet_diameter),
                "",
                0x29);
        }

        for (int16_t ship_slot = 0; ship_slot < 15; ++ship_slot) {
            if (MOX::system_display_ships[ship_slot].ship_idx <= -1) {
                continue;
            }

            const int16_t ship_left = static_cast<int16_t>(
                base_x + popup_offset_x + MOX::system_display_ships[ship_slot].x);
            const int16_t ship_top = static_cast<int16_t>(
                base_y + popup_offset_y + MOX::system_display_ships[ship_slot].y);

            MOX::system_display_ships[ship_slot].pic =
                SYS::Buffer_Reload_System_Ships(ship_slot);

            const int16_t ship_bottom = static_cast<int16_t>(
                ship_top + animate::Get_Height(MOX::system_display_ships[ship_slot].pic));
            const int16_t ship_right = static_cast<int16_t>(
                ship_left + animate::Get_Width(MOX::system_display_ships[ship_slot].pic));

            MOX::system_display_ships[ship_slot].button = fields::Add_Hidden_Field(
                ship_left,
                ship_top,
                ship_right,
                ship_bottom,
                "",
                0x29);
        }

        for (int16_t i = 0; i < 4; ++i) {
            MOX::gate_icon_field[i] = -1000;
        }

        SYS::Get_Gate_Icon_XY(
            base_x,
            base_y,
            popup_offset_x,
            popup_offset_y,
            orbit_offset_x,
            &gate_icon_x,
            &gate_icon_y);

        if (MOX::current_screen == SCREEN_OFFICERS) {
            gate_icon_y = static_cast<int16_t>(gate_icon_y - 6);
        }

        if (HAROLD::Star_Has_Artemis_Net(&MOX::star[star_idx])) {
            gate_icon_x = static_cast<int16_t>(gate_icon_x - 0x0C);
            MOX::gate_icon_field[3] = fields::Add_Hidden_Field(
                gate_icon_x,
                gate_icon_y,
                static_cast<int16_t>(gate_icon_x + 0x0F),
                static_cast<int16_t>(gate_icon_y + 0x0F),
                "",
                0);
        }

        if (HAROLD::Star_Has_Dimensional_Portal(&MOX::star[star_idx])) {
            gate_icon_x = static_cast<int16_t>(gate_icon_x - 0x0C);
            MOX::gate_icon_field[0] = fields::Add_Hidden_Field(
                gate_icon_x,
                gate_icon_y,
                static_cast<int16_t>(gate_icon_x + 0x0F),
                static_cast<int16_t>(gate_icon_y + 0x0F),
                "",
                0);
        }

        if (MOX::current_screen == SCREEN_OFFICERS) {
            *out_system_field = fields::Add_Hidden_Field(
                static_cast<int16_t>(base_x + popup_offset_x),
                static_cast<int16_t>(base_y + popup_offset_y),
                static_cast<int16_t>(base_x + popup_offset_x + 0x13E),
                static_cast<int16_t>(base_y + popup_offset_y + 0xB4),
                "",
                0);
            return;
        }

        MOX::system_display_grid_field = fields::Add_Grid_Field(
            static_cast<int16_t>(base_x + popup_offset_x + 0x0D),
            static_cast<int16_t>(base_y + popup_offset_y + 0x2F),
            1,
            1,
            0x140,
            0xB7,
            &MOX::system_display_grid_x[0],
            &MOX::system_display_grid_y[0],
            0);

        *out_system_field = fields::Add_Hidden_Field(
            static_cast<int16_t>(base_x + popup_offset_x),
            static_cast<int16_t>(base_y + popup_offset_y),
            static_cast<int16_t>(base_x + popup_offset_x + MOX::moveable_box[0].width),
            static_cast<int16_t>(base_y + popup_offset_y + MOX::moveable_box[0].height),
            "",
            0);
    }

    void __cdecl Remap_Draw_Animated_Sprite_Mask(
        int16_t x,
        int16_t y,
        const uint8_t* sprite_data,
        const int8_t* mask_data) {
        const int16_t sprite_width = animate::animation_header->width;
        int32_t current_y_offset = ((const s_packed_frame_header*)sprite_data)->start_y;
        int32_t line_start_offset =
            ((int32_t)y + current_y_offset) * (int32_t)video::g_screen_res_x + (int32_t)x;
        int32_t sprite_idx = 2;
        int32_t screen_offset = line_start_offset;
        const int32_t header_height = animate::animation_header->height;

        if (current_y_offset < header_height) {
            do {
                const s_packed_frame_run* run =
                    (const s_packed_frame_run*)(sprite_data + sprite_idx * 2);
                const int16_t run_len = run->pixel_count;
                const int16_t skip_len = run->skip_count;
                sprite_idx += 2;

                if (run_len == 0) {
                    current_y_offset += skip_len;
                    line_start_offset += skip_len * 640;
                    screen_offset = line_start_offset;
                } else {
                    screen_offset += skip_len;
                    const int32_t end_offset = screen_offset + run_len;
                    int32_t data_offset = sprite_idx * 2;
                    int32_t pixel_index =
                        sprite_width * current_y_offset + (int32_t)skip_len;

                    if (screen_offset < end_offset) {
                        do {
                            const int8_t mask_value = mask_data[pixel_index++];
                            const uint8_t remapped_color =
                                remap::picture_remap_color_list[sprite_data[data_offset++]];

                            uint8_t color = remapped_color;
                            if (mask_value != 0) {
                                color = SYS::remap_gray_mask_palette[mask_value][remapped_color];
                            }

                            video::current_video_page[screen_offset++] = color;
                        } while (screen_offset < end_offset);
                    }

                    sprite_idx += (run_len + 1) / 2;
                }
            } while (current_y_offset < header_height);
        }
    }

    void __cdecl Remap_Clipped_Draw_Animated_Sprite_Mask(
        int16_t x,
        int16_t y,
        const uint8_t* encoded_sprite,
        const int8_t* mask_bitmap) {
        const int32_t sprite_width = animate::animation_header->width;
        const int32_t sprite_height = animate::animation_header->height;

        int16_t draw_x;
        int32_t left_clip;
        if (x < video::screen_window_x1) {
            left_clip = static_cast<int32_t>(video::screen_window_x1) - static_cast<int32_t>(x);
            draw_x = video::screen_window_x1;
        } else {
            left_clip = 0;
            draw_x = x;
        }

        int16_t draw_y;
        int32_t top_clip;
        if (video::screen_window_y1 > y) {
            top_clip = static_cast<int32_t>(video::screen_window_y1) - static_cast<int32_t>(y);
            draw_y = video::screen_window_y1;
        } else {
            top_clip = 0;
            draw_y = y;
        }

        int16_t right = static_cast<int16_t>(static_cast<int32_t>(x) + sprite_width - 1);
        if (video::screen_window_x2 <= right) {
            right = video::screen_window_x2;
        }

        int16_t bottom = static_cast<int16_t>(static_cast<int32_t>(y) + sprite_height - 1);
        if (video::screen_window_y2 <= bottom) {
            bottom = video::screen_window_y2;
        }

        int32_t word_index = 2;
        int32_t current_row = ((const s_packed_frame_header*)encoded_sprite)->start_y;

        int32_t current_dest_row =
            static_cast<int32_t>(draw_y) * static_cast<int32_t>(video::g_screen_res_x) +
            static_cast<int32_t>(x);

        if (current_row < top_clip) {
            do {
                const s_packed_frame_run* run =
                    (const s_packed_frame_run*)(encoded_sprite + word_index * 2);
                const int16_t run_length = run->pixel_count;
                if (run_length == 0) {
                    current_row += static_cast<uint16_t>(run->skip_count);
                } else {
                    word_index += (static_cast<int32_t>(run_length) + 1) / 2;
                }
                word_index += 2;
            } while (current_row < top_clip);
        }

        if (current_row > top_clip) {
            current_dest_row +=
                (current_row - top_clip) * static_cast<int32_t>(video::g_screen_res_x);
        }

        int32_t row_index = current_row - top_clip;
        int32_t row_clip_start = current_dest_row + left_clip;
        const int32_t visible_width =
            static_cast<int32_t>(right) - static_cast<int32_t>(draw_x) + 1;
        int32_t row_clip_end = row_clip_start + visible_width;
        const int32_t visible_height =
            static_cast<int32_t>(bottom) - static_cast<int32_t>(draw_y) + 1;

        while (row_index < visible_height) {
            const s_packed_frame_run* run =
                (const s_packed_frame_run*)(encoded_sprite + word_index * 2);
            const int16_t run_length = run->pixel_count;
            const int16_t run_x = run->skip_count;
            word_index += 2;

            if (run_length == 0) {
                row_index += static_cast<int32_t>(run_x);
                current_dest_row += static_cast<int32_t>(run_x) * 640;
                row_clip_start = current_dest_row + left_clip;
                row_clip_end = row_clip_start + visible_width;
                continue;
            }

            int32_t dest_x = current_dest_row + static_cast<int32_t>(run_x);
            int32_t pixel_byte_index = word_index * 2;
            const int32_t run_end = dest_x + static_cast<int32_t>(run_length);
            int32_t skipped_pixels = 0;

            if (dest_x < row_clip_end) {
                if (dest_x < row_clip_start) {
                    int32_t clip_count = row_clip_start - dest_x;
                    if (clip_count > static_cast<int32_t>(run_length)) {
                        clip_count = static_cast<int32_t>(run_length);
                    }
                    skipped_pixels = clip_count;
                    pixel_byte_index += clip_count;
                    dest_x += clip_count;
                }

                const int32_t clipped_run_end =
                    (row_clip_end < run_end) ? row_clip_end : run_end;

                if (clipped_run_end > dest_x) {
                    int32_t mask_index =
                        sprite_width * row_index + static_cast<int32_t>(run_x) + skipped_pixels;

                    while (dest_x < clipped_run_end) {
                        const int8_t mask_value = mask_bitmap[mask_index];
                        const uint8_t remapped_color =
                            remap::picture_remap_color_list[encoded_sprite[pixel_byte_index]];

                        uint8_t out_color;
                        if (mask_value != 0) {
                            out_color = SYS::remap_gray_mask_palette[mask_value][remapped_color];
                        } else {
                            out_color = remapped_color;
                        }

                        video::current_video_page[dest_x] = out_color;
                        ++pixel_byte_index;
                        ++dest_x;
                        ++skipped_pixels;
                        ++mask_index;
                    }
                }
            }

            word_index += (static_cast<int32_t>(run_length) + 1) / 2;
        }
    }

    void __cdecl Get_System_Display_Ship_Icon_XY(
        int32_t unused_arg,
        int16_t display_slot,
        int16_t* x,
        int16_t* y,
        int16_t* layout_state) {
        (void)unused_arg;

        MOX::moveable_box_pict[0] =
            (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x49, MOX::global_cache_seg);
        (void)animate::Get_Width(MOX::moveable_box_pict[0]);
        (void)animate::Get_Height(MOX::moveable_box_pict[0]);

        s_animation_header** display_ship_pic = &MOX::system_display_ships[display_slot].pic;

        if (MOX::current_screen == SCREEN_OFFICERS) {
            *display_ship_pic = SYS::Buffer_Reload_System_Ships(display_slot);
            *x = static_cast<int16_t>(*layout_state + 5);
            *y = static_cast<int16_t>(0xb1 - animate::Get_Height(*display_ship_pic));
            *layout_state = static_cast<int16_t>(*layout_state + animate::Get_Width(*display_ship_pic) + 3);
            return;
        }

        *display_ship_pic = SYS::Buffer_Reload_System_Ships(display_slot);
        *x = static_cast<int16_t>(*layout_state + 0x12);
        *y = static_cast<int16_t>(((0x19 - animate::Get_Height(*display_ship_pic)) / 2) + 0xec);
        *layout_state = static_cast<int16_t>(*layout_state + animate::Get_Width(*display_ship_pic) + 3);
    }

    void __cdecl Build_System_Popup_Ships(int16_t system_idx) {
        int16_t displayed_ship_count = 0;
        int16_t layout_state = 0;

        SYS::Clear_System_Display_Ship_Fields();
        MOX::player_has_ship_in_system = 0;

        if (MOX::ship_icon_count > 0) {
            for (int16_t ship_icon_index = 0; ship_icon_index < MOX::ship_icon_count; ++ship_icon_index) {
                const int16_t node_idx = MOX::ship_icon[ship_icon_index].node_idx;
                const int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
                s_ship_data* const ship = &MOX::ship[ship_idx];

                if (ship->location == system_idx && ship->status < 3) {
                    const int16_t owner_idx = ship->owner;
                    MOX::system_display_ships[owner_idx].ship_idx = node_idx;

                    if (colony_read::Strategic_Local_Ship(ship, system_idx, MOX::PLAYER_NUM)) {
                        MOX::player_has_ship_in_system = 1;
                    }

                    const int16_t display_index = displayed_ship_count;
                    ++displayed_ship_count;

                    SYS::Get_System_Display_Ship_Icon_XY(
                        display_index,
                        owner_idx,
                        &MOX::system_display_ships[owner_idx].x,
                        &MOX::system_display_ships[owner_idx].y,
                        &layout_state);
                }
            }
        }
    }
} // namespace SYS