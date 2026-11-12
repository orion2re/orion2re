#include "pch.h"
#include "string_overrides.h"

namespace MAPGEN {
    namespace {
        int16_t Generation_Progress_Interval() {
            return MOX::NUM_STARS <= ORIGINAL_MAX_STARS ? 1 : 16;
        }

        bool Should_Update_Generation_Progress(int16_t star_idx) {
            return star_idx % Generation_Progress_Interval() == 0;
        }

        int16_t Generation_Progress_Max() {
            int16_t interval = Generation_Progress_Interval();
            return (int16_t)(((MOX::NUM_STARS + interval - 1) / interval) * 2);
        }

        int16_t Maximum_Galaxy_Grid_Y() {
            int16_t grid_y = 8;
            while ((((grid_y * 5) + 3) / 4) * grid_y < maximum_star_count) {
                ++grid_y;
            }
            return grid_y;
        }

        int16_t Maximum_Galaxy_Grid_X() {
            return (int16_t)(((Maximum_Galaxy_Grid_Y() * 5) + 3) / 4);
        }

        constexpr int16_t MAXIMUM_GALAXY_MAP_SCALE = 30;
        constexpr int16_t MAXIMUM_GALAXY_CELL_SIZE = 50;

        int16_t Maximum_Galaxy_Display_Scale() {
            const int32_t raw_cell_size = (MAXIMUM_GALAXY_CELL_SIZE * MAXIMUM_GALAXY_MAP_SCALE) / 10;
            const int32_t map_width = Maximum_Galaxy_Grid_X() * raw_cell_size;
            const int32_t map_height = Maximum_Galaxy_Grid_Y() * raw_cell_size;
            const int32_t scale_x = (map_width * 10 + 0x1FA - 1) / 0x1FA;
            const int32_t scale_y = (map_height * 10 + 0x190 - 1) / 0x190;
            return (int16_t)((scale_x > scale_y) ? scale_x : scale_y);
        }
    } // namespace

    bool __cdecl Set_Star_XYs(int16_t x_cells, int16_t y_cells, int16_t galaxy_age, int8_t generate_spectral, int8_t use_names) {
        int16_t x_cell_size = MOX::MAP_MAX_X / x_cells;
        int16_t y_cell_size = MOX::MAP_MAX_Y / y_cells;
        int16_t scaled_max_x = HAROLD::Get_Scaled_Value(MOX::MAP_MAX_X);
        int16_t scaled_max_y = HAROLD::Get_Scaled_Value(MOX::MAP_MAX_Y);
        int8_t y_collision_padding = 0;

        s_colors font_colors_main;
        s_colors* font_colors_ptr = MISC::Font_Colors2(4, 'w', 'y');
        font_colors_main = *font_colors_ptr;

        s_colors font_colors_alt;
        font_colors_ptr = MISC::Font_Colors2(4, 'w', '|');
        font_colors_alt = *font_colors_ptr;

        HAROLD::Set_Player_Font_Style(0, 0);
        HAROLD::Map_Scale_To_Zoom_Level();

        int16_t collision_h = 30;
        y_collision_padding = 4;
        switch (MOX::cur_map_scale) {
            case 10:
            case 15:
                collision_h = 50;
                y_collision_padding = 8;
                break;
            case 20:
                collision_h = 36;
                y_collision_padding = 6;
                break;
            case 30:
                break;
        }

        if (MOX::NUM_STARS <= 0) return false;

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            MAPGEN::star_count = i;
            bool update_progress = (generate_spectral || use_names) && Should_Update_Generation_Progress(i);
            if (update_progress) {
                RUSS::Mox_Update();
            }

            if (MAPGEN::num_maps_to_generate == 1 && update_progress) {
                fonts::Set_Font_Style(4, &font_colors_main);
                int16_t str_w = fonts::Get_String_Width(MAPGEN::fill_msg);
                MISC::Draw_Advancing_Fill_Message(4, &MAPGEN::fill_msg_ctr, Generation_Progress_Max(), &font_colors_main, &font_colors_alt, (0x280 - str_w) / 2, 0xD7, MAPGEN::fill_msg, MAPGEN::fill_msg_bitmap, 0);
                RUSS::Mox_Sync_Update();
            }

            if (generate_spectral) {
                MOX::star[i].spectral_class = MAPGEN::Generate_Spectral_Class(galaxy_age);
            }

            int16_t collision_w;
            if (use_names) {
                MAPGEN::Get_Star_Name(i);
                HAROLD::Set_Player_Font_Style(0, 0);
                char formatted_name[32];
                snprintf(formatted_name, sizeof(formatted_name), HAROLD::s___s__00556ae4, MOX::star[i].name);

                // Manual check for short names to append "w"
                uint32_t name_len = 0;
                while (MOX::star[i].name[name_len] != '\0') name_len++;

                if (name_len < 7) {
                    strlcat(formatted_name, "w", sizeof(formatted_name));
                }

                collision_w = fonts::Get_String_Width(formatted_name);
                if (MAPGEN::n_maps_generated <= 2) goto START_POS_GEN;
            }

            collision_w = (int16_t)((int16_t)game_random::Random(8) +
                (int16_t)game_random::Random(8) +
                0x20 +
                (int16_t)game_random::Random(8));

        START_POS_GEN:
            int16_t retry_count = 0;
            HAROLD::Set_Player_Font_Style(0, 0);
            HAROLD::Map_Scale_To_Zoom_Level();

            bool collision;
            do {
                int16_t rx, scaled_x;
                do {
                    rx = (int16_t)(game_random::Random(x_cell_size * 2) + (i % x_cells) * x_cell_size) % MOX::MAP_MAX_X;
                    scaled_x = HAROLD::Get_Scaled_Value(rx);
                } while (scaled_x < 0x18 || (scaled_max_x - (MOX::star_width / 2)) < scaled_x);
                MOX::star[i].x = rx;

                int16_t ry, scaled_y;
                do {
                    ry = (int16_t)(game_random::Random(y_cell_size * 2) + (y_cell_size * i) / x_cells) % MOX::MAP_MAX_Y;
                    scaled_y = HAROLD::Get_Scaled_Value(ry);
                } while (scaled_y < 0x16 || ((scaled_max_y - (MOX::star_height / 2)) - y_collision_padding) < scaled_y);
                MOX::star[i].y = ry;

                collision = MAPGEN::Star_XY_Invalid(i, collision_w, collision_h, scaled_x);
                if (!collision) break;

                if (++retry_count > 0x96) {
                    bool fallback_found = false;
                    int32_t fallback_min_x = 0x18;
                    int32_t fallback_min_y = 0x16;
                    int32_t fallback_range_x = scaled_max_x - (MOX::star_width / 2) - fallback_min_x + 1;
                    int32_t fallback_range_y = scaled_max_y - (MOX::star_height / 2) - y_collision_padding - fallback_min_y + 1;
                    int32_t fallback_attempt_limit = std::max<int32_t>(0x96, MOX::NUM_STARS * 8);

                    for (int32_t fallback_attempt = 0; fallback_attempt < fallback_attempt_limit; ++fallback_attempt) {
                        int32_t fx = game_random::Random(fallback_range_x) + fallback_min_x - 1;
                        int32_t fy = game_random::Random(fallback_range_y) + fallback_min_y - 1;
                        MOX::star[i].x = (int16_t)HAROLD::Get_Up_Scaled_Value((int16_t)fx);
                        MOX::star[i].y = (int16_t)HAROLD::Get_Up_Scaled_Value((int16_t)fy);

                        if (!MAPGEN::Star_XY_Invalid(i, collision_w, collision_h, (int16_t)fx)) {
                            fallback_found = true;
                            break;
                        }
                    }

                    if (!fallback_found) return true;
                    collision = false;
                }
            } while (collision);
        }
        return false;
    }

    int8_t __cdecl Generate_Spectral_Class(int16_t star_type_index) {
        uint8_t weights[STAR_CLASS_COUNT];

        for (int32_t i = 0; i < STAR_CLASS_COUNT; ++i) {
            weights[i] = MOX::star_class_table[i][star_type_index];
        }
        if (!special_generation_config.generate_black_holes) {
            weights[STAR_CLASS_BLACK_HOLE] = 0;
        }

        int16_t choice = RUSS::Get_Weighted_Choice_Char(weights, STAR_CLASS_COUNT);
        return choice >= 0 ? (int8_t)choice : STAR_CLASS_M;
    }

    int8_t __cdecl Generate_Non_Black_Hole_Spectral_Class(int16_t star_type_index) {
        for (int16_t attempt = 0; attempt < 200; ++attempt) {
            int8_t spectral_class = Generate_Spectral_Class(star_type_index);
            if (spectral_class < STAR_CLASS_BLACK_HOLE) {
                return spectral_class;
            }
        }

        return STAR_CLASS_M;
    }

    void __cdecl Set_Black_Hole_Visited_Flags() {
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if (MOX::star[i].spectral_class == STAR_CLASS_BLACK_HOLE) {
                MOX::star[i].visited = -1;
            }
        }
    }

    void __cdecl Generate_Wormhole_Links() {
        if (!special_generation_config.generate_wormholes) return;

        uint8_t star_used[MAX_STARS];
        memset(star_used, 0, sizeof(star_used));

        if (MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                int16_t home_planet_id = MOX::player[i].home_planet_id;
                int16_t star_index = MOX::planet[home_planet_id].star_index;
                star_used[star_index] = 1;
            }
        }

        if (n_wormholes != 0) {
            for (int16_t wormhole_idx = 0; wormhole_idx < (int16_t)n_wormholes; ++wormhole_idx) {
                int16_t found_star_idx = -1;
                for (int attempt = 0; attempt <= 200; ++attempt) {
                    int16_t star_idx = game_random::Random(MOX::NUM_STARS - 1);
                    if (star_idx != 0 &&
                        star_used[star_idx] == 0 &&
                        MOX::star[star_idx].system_special == SYSTEM_SPECIAL_NO_SPECIAL &&
                        !HAROLD::Empty_System(star_idx) &&
                        MOX::star[star_idx].spectral_class != STAR_CLASS_BLACK_HOLE &&
                        MOX::star[star_idx].wormhole_star_id == -1) {
                        found_star_idx = star_idx;
                        break;
                    }
                }

                if (found_star_idx != -1) {
                    HAROLD::Set_Wormhole_Id(found_star_idx, star_used);
                }
            }
        }
    }

    void __cdecl Assign_Marooned_Heroes() {
        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            if (MOX::star[star_idx].system_special == SYSTEM_SPECIAL_MAROONED_HERO) {
                int16_t attempts = 0;
                bool stop_searching = false;
                int16_t selected_hero_idx = -1;

                do {
                    int16_t hero_candidate = (int16_t)game_random::Random(38) - 1;

                    if (hero_candidate == 0x41 || MOX::leaders[hero_candidate].status != -1) {
                        attempts++;
                    } else {
                        selected_hero_idx = hero_candidate;
                        stop_searching = true;
                    }

                    if (attempts == 100) {
                        stop_searching = true;
                    }
                } while (!stop_searching);

                if (selected_hero_idx == -1) {
                    MOX::star[star_idx].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                } else {
                    MOX::leaders[selected_hero_idx].status = 5;
                    MOX::leaders[selected_hero_idx].location = star_idx;
                }
            }
        }
    }
    void __cdecl Generate_Number_Of_Nebulas(int16_t galaxy_size, int16_t unused_nebula_freq) {
        (void)unused_nebula_freq;
        MOX::NUM_NEBULAS = 0;

        if (MOX::nebula_screen_seg == nullptr ||
            !special_generation_config.generate_nebulas ||
            galaxy_size < 0 || galaxy_size >= GALAXY_SIZE_COUNT) {
            return;
        }

        const s_nebula_count_config& range = nebula_count[galaxy_size];
        int32_t count = range.minimum;
        if (range.maximum > range.minimum) {
            count += game_random::Random(range.maximum - range.minimum + 1) - 1;
        }
        if (range.scale_with_star_count) {
            count = (count * MOX::NUM_STARS + ORIGINAL_MAX_STARS * 2 - 1) /
                (ORIGINAL_MAX_STARS * 2);
        }
        MOX::NUM_NEBULAS = (uint8_t)std::min<int32_t>(MAX_NEBULAS, count);
    }

    void __cdecl Generate_Nebulas() {
        int16_t widths[12];
        int16_t heights[12];
        uint8_t nebula_types[MAX_NEBULAS];
        int16_t min_dist_param = 0x14;
        int16_t attempts = 0;
        const int32_t point_buffer_value_count = 30000 * 2;
        const int32_t coord_buffer_value_count = 10000 * 2;
        const int32_t weight_buffer_value_count = 10000;

        allocate::Push_Block(MOX::screen_seg);
        int16_t* point_buffer = (int16_t*)allocate::Allocate_Next_Block(MOX::screen_seg, (int32_t)(sizeof(*point_buffer) * point_buffer_value_count));
        int16_t* coord_buffer = (int16_t*)allocate::Allocate_Next_Block(MOX::screen_seg, (int32_t)(sizeof(*coord_buffer) * coord_buffer_value_count));
        uint16_t* weight_buffer = (uint16_t*)allocate::Allocate_Next_Block(MOX::screen_seg, (int32_t)(sizeof(*weight_buffer) * weight_buffer_value_count));

        RUSS::Mox_Update();

        for (int i = 0; i < 12; ++i) {
            s_animation_header* anim = (s_animation_header*)farload::Far_Reload("starbg.lbx", i * 4 + 6, MOX::nebula_screen_seg);
            widths[i] = animate::Get_Width(anim);
            heights[i] = animate::Get_Height(anim);
        }

        if (MOX::NUM_NEBULAS != 0) {
            for (int16_t i = 0; i < (int16_t)(uint16_t)MOX::NUM_NEBULAS; ++i) {
                RUSS::Mox_Update();
                // The original reads an uninitialized stack byte array before accepting
                // a type. Use a direct random choice here instead of encoding UB.
                nebula_types[i] = (uint8_t)((int16_t)game_random::Random(12) - 1);
                RUSS::Mox_Update();
            }
        }

        if (MOX::NUM_STARS > ORIGINAL_MAX_STARS) {
            int16_t scaled_map_max_x = HAROLD::Get_Scaled_Value(MOX::MAP_MAX_X);
            int16_t scaled_map_max_y = HAROLD::Get_Scaled_Value(MOX::MAP_MAX_Y);
            int16_t nebula_width = HAROLD::Get_Scaled_Value(0xA5);
            int16_t nebula_height = HAROLD::Get_Scaled_Value(0x82);
            int16_t min_x = 10;
            int16_t min_y = 10;
            int16_t max_x = scaled_map_max_x - nebula_width;
            int16_t max_y = scaled_map_max_y - nebula_height;
            int16_t placed_x[MAX_NEBULAS];
            int16_t placed_y[MAX_NEBULAS];
            int32_t initial_dx = nebula_width + 40;
            int32_t initial_dy = nebula_height + 40;
            int32_t initial_min_dist_sq = initial_dx * initial_dx + initial_dy * initial_dy;

            for (int16_t i = 0; i < (int16_t)(uint16_t)MOX::NUM_NEBULAS; ++i) {
                bool placed = false;
                int32_t min_dist_sq = initial_min_dist_sq;

                for (int16_t relaxation = 0; relaxation < 8 && !placed; ++relaxation) {
                    for (int16_t attempt = 0; attempt < 500 && !placed; ++attempt) {
                        int16_t x = (int16_t)(game_random::Random(max_x - min_x + 1) + min_x - 1);
                        int16_t y = (int16_t)(game_random::Random(max_y - min_y + 1) + min_y - 1);
                        bool too_close = false;

                        for (int16_t j = 0; j < i; ++j) {
                            if (HOMEGEN::Dist_Between_Points(x, y, placed_x[j], placed_y[j]) < min_dist_sq) {
                                too_close = true;
                                break;
                            }
                        }

                        if (!too_close) {
                            placed_x[i] = x;
                            placed_y[i] = y;
                            placed = true;
                        }
                    }

                    min_dist_sq = min_dist_sq * 3 / 4;
                }

                if (!placed) {
                    placed_x[i] = (int16_t)(game_random::Random(max_x - min_x + 1) + min_x - 1);
                    placed_y[i] = (int16_t)(game_random::Random(max_y - min_y + 1) + min_y - 1);
                }

                MOX::nebula[i].type = (int8_t)nebula_types[i];
                MOX::nebula[i].x = (int16_t)HAROLD::Get_Up_Scaled_Value(placed_x[i]);
                MOX::nebula[i].y = (int16_t)HAROLD::Get_Up_Scaled_Value(placed_y[i]);
            }

            RUSS::Mox_Update();
            allocate::Pop_Block(MOX::screen_seg);
            Load_Nebula_Pictures();
            return;
        }

        int16_t scaled_x_limit = HAROLD::Get_Scaled_Value(0xA5);
        int16_t scaled_y_limit = HAROLD::Get_Scaled_Value(0x82);
        int16_t scaled_x_limit_plus_20 = scaled_x_limit + 0x14;
        int16_t scaled_y_limit_plus_20 = scaled_y_limit + 0x14;

        while (true) {
            RUSS::Mox_Update();
            ++attempts;
            if (attempts > 100) {
                MOX::NUM_NEBULAS = 1;
                break;
            }

            memset(point_buffer, 0, sizeof(*point_buffer) * point_buffer_value_count);
            memset(coord_buffer, 0, sizeof(*coord_buffer) * coord_buffer_value_count);
            memset(weight_buffer, 0, sizeof(*weight_buffer) * weight_buffer_value_count);
            RUSS::Mox_Update();

            int16_t start_x = (int16_t)game_random::Random(0x1FA - widths[0]);
            int16_t start_y = (int16_t)game_random::Random(0x190 - heights[0]);

            MOX::nebula[0].type = (int8_t)nebula_types[0];
            MOX::nebula[0].x = HAROLD::Get_Up_Scaled_Value(start_x);
            MOX::nebula[0].y = HAROLD::Get_Up_Scaled_Value(start_y);

            if (MOX::NUM_NEBULAS < 2) break;

            int16_t valid_points_count = 0;
            int32_t dx = (int32_t)(scaled_x_limit_plus_20 + min_dist_param);
            int32_t dy = (int32_t)(scaled_y_limit_plus_20 + min_dist_param);
            int32_t dist_sq_threshold = dx * dx + dy * dy;

            Build_Min_Point_Distances(start_x, start_y, 10, 10, 0x1FA - scaled_x_limit, 0x190 - scaled_y_limit, dist_sq_threshold, point_buffer, &valid_points_count, 30000);
            RUSS::Mox_Update();

            if (valid_points_count < (int16_t)(uint16_t)MOX::NUM_NEBULAS) {
                min_dist_param -= 10;
                continue;
            }

            coord_buffer[0] = start_x;
            coord_buffer[1] = start_y;
            weight_buffer[0] = 1;
            int16_t placed_count = 1;
            int16_t buffer_full = 0;

            for (int16_t i = 0; i < valid_points_count && buffer_full == 0; ++i) {
                int16_t px = point_buffer[i * 2];
                int16_t py = point_buffer[i * 2 + 1];

                int16_t too_close = 0;
                for (int16_t j = 0; j < placed_count; ++j) {
                    int16_t coord_x = coord_buffer[j * 2];
                    int16_t coord_y = coord_buffer[j * 2 + 1];

                    if (px != coord_x || py != coord_y) {
                        if (HOMEGEN::Dist_Between_Points(px, py, coord_x, coord_y) < dist_sq_threshold) {
                            too_close = 1;
                            break;
                        }
                    }
                }

                if (too_close == 0) {
                    coord_buffer[placed_count * 2] = point_buffer[i * 2];
                    coord_buffer[placed_count * 2 + 1] = point_buffer[i * 2 + 1];
                    weight_buffer[placed_count] = 1;
                    ++placed_count;
                    if (placed_count >= 10000) {
                        buffer_full = 1;
                    }
                }
            }

            RUSS::Mox_Update();
            if ((int16_t)(uint16_t)MOX::NUM_NEBULAS <= placed_count) {
                for (int16_t i = 0; i < (int16_t)(uint16_t)MOX::NUM_NEBULAS; ++i) {
                    int16_t choice = RUSS::Get_Weighted_Choice_Short(weight_buffer, placed_count);

                    MOX::nebula[i].type = (int8_t)nebula_types[i];
                    MOX::nebula[i].x = HAROLD::Get_Up_Scaled_Value(coord_buffer[choice * 2]);
                    MOX::nebula[i].y = HAROLD::Get_Up_Scaled_Value(coord_buffer[choice * 2 + 1]);
                    weight_buffer[choice] = 0;
                }
                break;
            }

            if (attempts % 20 == 0) {
                min_dist_param -= 10;
            }
        }

        RUSS::Mox_Update();
        allocate::Pop_Block(MOX::screen_seg);
        Load_Nebula_Pictures();
    }

    /** Initializes main-screen map dimensions and zoom levels for a galaxy preset. */
    void __cdecl Init_Main_Screen(uint8_t gal_size) {
        if (gal_size >= GALAXY_SIZE_COUNT) return;

#if MAX_STARS > ORIGINAL_MAX_STARS
        if (gal_size == GALAXY_SIZE_MAXIMUM && layout[gal_size].automatic) {
            const int16_t raw_cell_size = (MAXIMUM_GALAXY_CELL_SIZE * MAXIMUM_GALAXY_MAP_SCALE) / 10;
            MOX::MAP_MAX_X = (int16_t)(Maximum_Galaxy_Grid_X() * raw_cell_size);
            MOX::MAP_MAX_Y = (int16_t)(Maximum_Galaxy_Grid_Y() * raw_cell_size);
            MOX::cur_map_scale = MAXIMUM_GALAXY_MAP_SCALE;
            MOX::max_map_scale = Maximum_Galaxy_Display_Scale();
            MOX::max_zoom_count = 3;
            MOX::zoom_count = 3;
            return;
        }
#endif

        const s_map_layout& layout = MAPGEN::layout[gal_size];
        MOX::MAP_MAX_X = (int16_t)layout.width;
        MOX::MAP_MAX_Y = (int16_t)layout.height;
        MOX::cur_map_scale = (int16_t)layout.star_spacing;
        MOX::max_map_scale = (int16_t)layout.maximum_display_scale;
        MOX::max_zoom_count = (int16_t)layout.maximum_zoom_level;
        MOX::zoom_count = (int16_t)layout.initial_zoom_level;
    }

    void __cdecl Load_Nebula_Pictures() {
        bool first_load = true;

        if (MOX::NUM_NEBULAS == 0) {
            return;
        }

        for (int16_t type_val = 0; type_val < 12; ++type_val) {
            for (int16_t frame_idx = 0; frame_idx < 4; ++frame_idx) {
                RUSS::Mox_Update();
                int entry_num = type_val * 4 + frame_idx + 6;

                if (first_load) {
                    MOX::nebula_pict_seg[type_val][frame_idx] =
                        (s_animation_header*)farload::Far_Reload("starbg.lbx", entry_num, MOX::nebula_screen_seg);
                    first_load = false;
                } else {
                    MOX::nebula_pict_seg[type_val][frame_idx] =
                        (s_animation_header*)farload::Far_Reload_Next("starbg.lbx", entry_num, MOX::nebula_screen_seg);
                }
            }
        }
    }

    bool __cdecl Star_XY_Invalid(int16_t target_idx, int16_t limit_x, int16_t limit_y, int16_t offset_val) {
        int16_t scaled_max_x = HAROLD::Get_Scaled_Value(MOX::MAP_MAX_X);
        HAROLD::Get_Scaled_Value(MOX::MAP_MAX_Y);

        bool is_invalid = false;
        if (target_idx <= 0) {
            return false;
        }

        for (int16_t i = 0; i < target_idx; ++i) {
            if (is_invalid) {
                break;
            }

            const s_star_data& star_current = MOX::star[i];
            const s_star_data& star_target = MOX::star[target_idx];

            if (star_current.spectral_class == STAR_CLASS_BLACK_HOLE || star_target.spectral_class == STAR_CLASS_BLACK_HOLE) {
                if (GEO::Parsecs_Between_Stars(target_idx, i) < 5) {
                    is_invalid = true;
                }
            } else {
                int32_t x1 = HAROLD::Get_Scaled_Value(star_current.x);
                int32_t x2 = HAROLD::Get_Scaled_Value(star_target.x);
                int32_t y1 = HAROLD::Get_Scaled_Value(star_current.y);
                int32_t y2 = HAROLD::Get_Scaled_Value(star_target.y);

                int32_t dx = x1 - x2;
                int32_t dy = y1 - y2;
                int32_t dist_sq = dx * dx + dy * dy;

                int32_t abs_dx = (dx < 0) ? -dx : dx;
                int32_t abs_dy = (dy < 0) ? -dy : dy;

                int16_t scaled_60 = HAROLD::Get_Scaled_Value(60);

                bool test0 = (dist_sq <= 800);
                bool test1 = (abs_dy < limit_y && abs_dx < limit_x);
                bool test2 = (abs_dy < 15 && abs_dx < scaled_60);
                bool test3 = ((limit_x * 2 / 3) + offset_val > scaled_max_x);

                if (test0 || test1 || test2 || test3) {
                    tests_failed[0] += (uint16_t)test0;
                    tests_failed[1] += (uint16_t)test1;
                    tests_failed[2] += (uint16_t)test2;
                    tests_failed[3] += (uint16_t)test3;
                    is_invalid = true;
                }
            }
        }

        return is_invalid;
    }

    /**
     * Corrects black holes that overlap nebulae by converting them to habitable stars.
     */
    void __cdecl Black_Hole_Fix(int16_t galaxy_age) {
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            s_star_data& star = MOX::star[i];

            if (star.spectral_class == STAR_CLASS_BLACK_HOLE) {
                int16_t x = star.x;
                int16_t y = star.y;

                bool in_nebula_1 = GEO::Point_Is_In_Some_Nebula(x, y);
                bool in_nebula_2 = GEO::Point_Is_In_Some_Nebula(x + 24, y);
                bool in_nebula_3 = GEO::Point_Is_In_Some_Nebula(x, y + 24);
                bool in_nebula_4 = GEO::Point_Is_In_Some_Nebula(x + 24, y + 24);
                bool in_nebula_5 = GEO::Point_Is_In_Some_Nebula(x + 12, y);
                bool in_nebula_6 = GEO::Point_Is_In_Some_Nebula(x + 24, y + 12);
                bool in_nebula_7 = GEO::Point_Is_In_Some_Nebula(x + 12, y + 24);
                bool in_nebula_8 = GEO::Point_Is_In_Some_Nebula(x, y + 12);

                if (in_nebula_1 || in_nebula_2 || in_nebula_3 || in_nebula_4 ||
                    in_nebula_5 || in_nebula_6 || in_nebula_7 || in_nebula_8) {
                    star.spectral_class = Generate_Non_Black_Hole_Spectral_Class(galaxy_age);

                    int16_t satellite_count = Generate_Number_Of_Satellites(i);
                    for (int16_t satellite = 0; satellite < satellite_count; ++satellite) {
                        Planet_Generation(i);
                    }
                    star.visited = 0;
                }
            }
        }
    }

    void __cdecl Build_Min_Point_Distances(int16_t x, int16_t y, int16_t min_x, int16_t min_y, int16_t max_x, int16_t max_y, int32_t dist_sq, int16_t* out_points, int16_t* count, int16_t max_count) {
        int16_t x_coords[640];
        int16_t y_coords[480];
        bool finished = false;

        memset(x_coords, 0xFF, sizeof(x_coords));
        memset(y_coords, 0xFF, sizeof(y_coords));

        for (int16_t i = min_x; i < max_x; ++i) {
            x_coords[i] = i;
        }
        for (int16_t i = min_y; i < max_y; ++i) {
            y_coords[i] = i;
        }

        HAROLD::Randomize_List(x_coords, 640, 0);
        HAROLD::Randomize_List(y_coords, 480, 0);

        for (int i = 0; i < 640; ++i) {
            if (finished) break;

            int16_t rx = x_coords[i];
            if (rx > -1) {
                for (int j = 0; j < 480; ++j) {
                    int16_t ry = y_coords[j];
                    if (ry > -1) {
                        int32_t dist = HOMEGEN::Dist_Between_Points(x, y, rx, ry);
                        if (dist >= dist_sq) {
                            int16_t current_count = *count;
                            out_points[current_count * 2] = rx;
                            out_points[current_count * 2 + 1] = ry;

                            *count = current_count + 1;

                            if (*count >= max_count) {
                                finished = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    void __cdecl Universe_Generation(uint8_t gal_size, uint8_t gal_age, int8_t nebula_freq) {
        Reset_Generated_Star_Names();
        s_colors local_colors1;
        s_colors local_colors2;
        uint32_t last_stat_clock;
        uint32_t stats_start_clock = 0;
        char planets_per_star[MAX_STARS];
        char error_buf[80];
        int16_t grid_x, grid_y;
        int16_t unused_wormhole_flag;
        int32_t galaxy_size = (int32_t)gal_size;

        s_colors* font_colors_ptr = MISC::Font_Colors2(4, 'w', 'y');
        local_colors1 = *font_colors_ptr;

        font_colors_ptr = MISC::Font_Colors2(4, 'w', '|');
        local_colors2 = *font_colors_ptr;
        RUSS::Mox_Update();

        last_stat_clock = clock();
        MOX::g_galaxy_size = Galaxy_Size_For_Classic_Rules_(gal_size);
        MOX::g_galaxy_age = gal_age;
        n_maps_generated = 0;
        memset(tests_failed, 0, sizeof(tests_failed));
        RUSS::Mox_Update();

        while (true) {
            evan_ptr = MOX::climate_modifier_table;
            make_food_planet = 0;
            num_maps_to_generate = 1;
            ++n_maps_generated;
            RUSS::Mox_Update();
            RUSS::Mox_Update();
            RUSS::Mox_Update();

            MOX::star_width = 37;
            MOX::star_height = 37;
            companion_star = 0;
            memset(MOX::planet, 0, MAX_PLANETS * sizeof(*MOX::planet));
            RUSS::Mox_Update();

            name_seg = farload::Far_Reload_Data("starname.lbx", 1, MOX::screen_seg, 0, 0x33D, 0xF);
            for (int i = 0; i < STAR_NAME_COUNT; ++i) {
                string_overrides::Apply_String("starname.lbx", 13 + i + 1, (char*)name_seg + i * 15, 15);
            }
            RUSS::Mox_Update();
            name_index = (uint16_t*)allocate::Allocate_Next_Block(MOX::screen_seg, 0xCF4);
            RUSS::Mox_Update();
            RUSS::Mox_Update();

            if (MOX::n_stats_to_generate != 0) {
                num_maps_to_generate = MOX::n_stats_to_generate;
                stats_start_clock = clock();
            }

            RUSS::Mox_Update();
            Randomize_Star_Names(name_index);
            Prepare_Generated_Star_Names();

            if (num_maps_to_generate == 1) {
                if (ERIC::Get_Fade_Percent() != 0) {
                    ERIC::Fast_Fade_In();
                }
                video::Set_Page_Back();
                RUSS::Mox_Update();
                s_animation_header* raceopt_ptr = (s_animation_header*)buffer::Buffer_Reload("raceopt.lbx", 0, MOX::global_cache_seg);
                animate::Draw(0, 0, raceopt_ptr);
                RUSS::Mox_Update();
                video::Copy_Back_To_Off();

                char* msg = (char*)HAROLD::H_Message(0xF2);
                strlcpy(fill_msg, msg, sizeof(fill_msg));

                uint32_t bsize = bitmap::Get_Bitmap_Size(325, 30);
                fill_msg_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bsize);
                bitmap::Create_Blank_Bitmap(325, 30, fill_msg_bitmap);
                fill_msg_ctr = 0;
                RUSS::Mox_Update();
                fonts::Set_Font_Style_Outline_Heavy(4, &local_colors1);
                fonts::Set_Outline_Color(2, 1);
                fonts::Print_To_Bitmap(0, 0, fill_msg, fill_msg_bitmap);
                RUSS::Mox_Update();
            }

            if (first_time_map_is_generated != 0) {
                game_random::Set_Random_Seed(MOX::settings.start_of_game_seed);
                first_time_map_is_generated = 0;
            }

            switch (Galaxy_Size_For_Classic_Rules_((int8_t)galaxy_size)) {
                case GALAXY_SIZE_SMALL:
                    MOX::NUM_STARS = star_count_by_galaxy_size[galaxy_size];
                    grid_x = 5;
                    grid_y = 4;
                    break;
                case GALAXY_SIZE_MEDIUM:
                    MOX::NUM_STARS = star_count_by_galaxy_size[galaxy_size];
                    grid_x = 6;
                    grid_y = 6;
                    break;
                case GALAXY_SIZE_LARGE:
                    MOX::NUM_STARS = star_count_by_galaxy_size[galaxy_size];
                    grid_x = 9;
                    grid_y = 6;
                    break;
                case GALAXY_SIZE_HUGE:
                    MOX::NUM_STARS = star_count_by_galaxy_size[galaxy_size];
                    grid_x = 9;
                    grid_y = 8;
                    break;
#if MAX_STARS > ORIGINAL_MAX_STARS
                case GALAXY_SIZE_MAXIMUM:
                    MOX::NUM_STARS = star_count_by_galaxy_size[galaxy_size];
                    grid_x = Maximum_Galaxy_Grid_X();
                    grid_y = Maximum_Galaxy_Grid_Y();
                    break;
#endif
                default:
                    snprintf(error_buf, sizeof(error_buf), "Galaxy size %d is undefined", galaxy_size);
                    exitm::Exit_With_Message(error_buf);
                    return;
            }

            Init_Main_Screen(gal_size);
            RUSS::Mox_Update();
            RUSS::Mox_Update();

            MOX::planet_count = 0;
            Generate_Number_Of_Nebulas(gal_size, nebula_freq);
            if (MOX::NUM_NEBULAS != 0) {
                Generate_Nebulas();
            }

            HAROLD::Get_Scaled_Value(MOX::MAP_MAX_X);
            HAROLD::Get_Scaled_Value(MOX::MAP_MAX_Y);

            int16_t map_idx = 0;
            bool restart_setup = false;

            while ((int32_t)map_idx < (int32_t)num_maps_to_generate) {
                if (num_maps_to_generate != 1) {
                    MOX::settings.start_of_game_seed = game_random::Get_Random_Seed();
                }

                n_space_monsters = 0;
                if (MOX::n_stats_to_generate != 0) {
                    Generate_Number_Of_Nebulas(gal_size, nebula_freq);
                }

                n_planets = 0;
                n_stars = 0;
                n_marooned_heroes = 0;
                n_wormholes = 0;
                MOX::NUM_COLONIES = 0;
                memset(special_count, 0, sizeof(special_count));
                MOX::planet_count = 0;
                memset(MOX::planet, 0, MAX_PLANETS * sizeof(*MOX::planet));
                memset(MOX::star, 0, MAX_STARS * sizeof(*MOX::star));
                memset(planets_per_star, 0, sizeof(planets_per_star));

                if (Set_Star_XYs(grid_x, grid_y, gal_age, 1, 1)) {
                    fonts::Set_Font_Style(4, &local_colors1);
                    int16_t swidth = fonts::Get_String_Width(fill_msg);
                    MISC::Draw_Advancing_Fill_Message(4, &fill_msg_ctr, MOX::NUM_STARS * 2, &local_colors1, &local_colors2, (640 - swidth) / 2, 215, fill_msg, fill_msg_bitmap, 1);
                    RUSS::Mox_Sync_Update();
                    restart_setup = true;
                    break;
                }

                for (star_count = 0; star_count < MOX::NUM_STARS; ++star_count) {
                    RUSS::Mox_Update();
                    memset(MOX::star[star_count].relocate_ship_to, 0xFF, sizeof(MOX::star[star_count].relocate_ship_to));
                    memset(MOX::star[star_count].officer_index, 0xFF, sizeof(MOX::star[star_count].officer_index));
                    MOX::star[star_count].wormhole_star_id = -1;

                    if (num_maps_to_generate == 1 && map_idx < 1) {
                        if (Should_Update_Generation_Progress(star_count)) {
                            fonts::Set_Font_Style(4, &local_colors1);
                            int16_t swidth = fonts::Get_String_Width(fill_msg);
                            MISC::Draw_Advancing_Fill_Message(4, &fill_msg_ctr, Generation_Progress_Max(), &local_colors1, &local_colors2, (640 - swidth) / 2, 215, fill_msg, fill_msg_bitmap, 0);
                            RUSS::Mox_Sync_Update();
                        }
                    }

                    MOX::star[star_count].owner = -1;
                    int16_t r_size = RUSS::Get_Weighted_Choice_Char(
                        star_visual_size_weight, 3);
                    MOX::star[star_count].size = (int8_t)(r_size >= 0 ? r_size : 1);

                    MOX::star[star_count].pict_type = (int8_t)(game_random::Random(3) - 1);
                    memset(MOX::star[star_count].planet_index, 0xFF, sizeof(MOX::star[star_count].planet_index));

                    companion_star = 0;
                    if (HAROLD::Habitable_Spectral_Class(star_count)) {
                        int8_t n_sat = (int8_t)Generate_Number_Of_Satellites(star_count);
                        planets_per_star[star_count] = n_sat;
                        if (n_sat > 0) {
                            int16_t planet_ctr = n_sat;
                            do {
                                RUSS::Mox_Update();
                                Planet_Generation(star_count);
                                --planet_ctr;
                            } while (planet_ctr != 0);
                        }
                        if (MOX::n_stats_to_generate != 0) {
                            ++satellite_stat[(uint8_t)n_sat];
                        }
                    }

                    if (MOX::star[star_count].spectral_class != STAR_CLASS_BLACK_HOLE && planets_per_star[star_count] > 0 && star_count > 0) {
                        Generate_Star_Special(star_count, &unused_wormhole_flag, special_count);
                    }

                    MOX::star[star_count].wormhole_star_id = -1;
                    ++n_stars;
                }

                bool connected = false;
                do {
                    if (MOX::NUM_NEBULAS != 0) {
                        Black_Hole_Fix(gal_age);
                    }
                    GEO::Initialize_Black_Hole_Blocks(45);
                    if (GEO::Map_Is_Connected()) {
                        connected = true;
                    } else {
                        Set_Star_XYs(grid_x, grid_y, gal_age, 0, 0);
                    }
                } while (!connected);

                if (MOX::n_stats_to_generate != 0) {
                    ++nebula_stat[MOX::NUM_NEBULAS];

                    if (MOX::n_stats_to_generate < 100 ||
                        (map_idx % (MOX::n_stats_to_generate / 100)) == 0) {
                        uint32_t prev_stat_clock = last_stat_clock;
                        last_stat_clock = clock();
                        RUSS::PFMT(0, 0, "                                                                      ");

                        uint32_t total_time = last_stat_clock - stats_start_clock;
                        RUSS::PFMT(0, 0, "Stat %d  (start: %d)  (diff %d)  (total: %d.%d)", (int32_t)map_idx, (int32_t)stats_start_clock, (int32_t)(last_stat_clock - prev_stat_clock), (int32_t)(total_time / 100), (int32_t)(total_time % 100));
                    }
                }

                ++map_idx;
            }

            if (!restart_setup) {
                return;
            }
        }
    }
} // namespace MAPGEN
