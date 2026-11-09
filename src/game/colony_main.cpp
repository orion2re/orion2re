#include "pch.h"

#include "build_queue.h"

namespace COLMOVE {
    void __cdecl Draw_Cluster(int16_t x, int16_t y) {
        if (cluster_colony_n != -1 && !COLONY::full_draw) {
            if (Get_First_Unemployed(cluster_colony_n) != -1) {
                if (!HAROLD::Was_Mouse_List_Saved() && g_hacky_dont_draw_cluster_flag != 1) {
                    fields::Enable_Draw_All_Fields();
                    fields::Draw_Visible_Fields();
                    fields::Disable_Draw_All_Fields();

                    graphics::Set_Window(0, 0, 639 /* 0x27f */, 479 /* 0x1df */);
                    video::Clip_On();

                    s_animation_header* anim = COLONY::C_Anims(15 /* 0xf */);
                    animate::Draw(x, y, anim);

                    s_colony& col = MOX::colony[cluster_colony_n];
                    if (col.n_pops > 0) {
                        int16_t current_x = x + 5;
                        for (int16_t i = 0; i < col.n_pops; ++i) {
                            if (!POP::Is_Assigned(col.pop[i])) {
                                s_animation_header* pop_anim = COLONY::Colony_Pop_Anim(cluster_colony_n, i);
                                animate::Draw(current_x, y - 10, pop_anim);
                                current_x += 20; // 0x14
                            }
                        }
                    }

                    video::Clip_Off();
                }
            }
        }
    }
} // namespace COLMOVE

namespace COLONY {
    int16_t _shortcut_previous_colony;
    int16_t _shortcut_next_colony;
    int16_t _shortcut_return;

    bool __cdecl Prompt_For_Autobuild_Queue(int16_t colony_idx) {
        if (colony_idx < 0 || colony_idx >= MOX::NUM_COLONIES) {
            return false;
        }

        s_colony& colony = MOX::colony[colony_idx];
        int preset = BUILD_QUEUE::Decode_Autobuild_Preset(colony.auto_building);
        if (preset < 0) {
            preset = BUILD_QUEUE::autobuild_settings.default_preset;
        }

        char input[4];
        snprintf(input, sizeof(input), "%d", preset);
        char prompt[] = "Auto Build Queue: 0-10";
        char invalid[] = "Enter a queue number from 0 to 10, or leave it blank to disable autobuild.";

        fonts::Set_Font_Style(3, &COLONY::font_color_array);
        const int16_t input_width = fonts::Get_String_Width("000");
        fields::Clear_Mouse_Buffer();

        while (namestar::Input_Box_Popup_Animated(
                   input, sizeof(input), 2, input_width, prompt, namestar::Star_Name_Popup_Screen_Center_X(), namestar::Star_Name_Popup_Screen_Center_Y()) != 0) {
            const int selected = BUILD_QUEUE::Parse_Autobuild_Preset(input);
            if (selected == -1) {
                colony.auto_building = BUILD_QUEUE::AUTOBUILD_DISABLED;
                return true;
            }
            if (selected >= 0) {
                colony.auto_building = BUILD_QUEUE::Encode_Autobuild_Preset(selected);
                BUILD_QUEUE::Apply_Custom_Autobuild(colony_idx);
                return true;
            }
            GENDRAW::Help(invalid);
        }
        return false;
    }

    void __cdecl Draw_Colony_Bldg(int16_t col, int16_t row) {
        if (col >= 0 && col < 6 && row >= 0 && row < 6) {
            int32_t building_id = colony_bldgs[col].buildings[row];
            if (building_id != 0) {
                s_animation_header* seg = COLBCACH::Cache_Load_Bldg(col, row, building_id);

                if (full_draw) {
                    int16_t prev_frame = animate::Get_Current_Frame(seg);
                    animate::Reset_Animation_Frame(seg);

                    while (animate::Get_Current_Frame(seg) < prev_frame) {
                        animate::Draw(0, 0, seg);
                    }

                    if (full_draw) {
                        animate::Draw(0, 0, seg);
                        return;
                    }
                }

                if (scanned_bldg_C == col && scanned_bldg_R == row) {
                    if (ERIC::Global_Cycler(30, 2)) {
                        Draw_Bldg_CR_Dark(col, row);
                        return;
                    }
                }

                animate::Draw_No_Glass(0, 0, seg);
            }
        }
    }

    void __cdecl Draw_Colony_Bldgs() {
        bool is_scanning_valid = (scanned_bldg_C != -1 && scanned_bldg_R != -1);

        for (int16_t i = 0; i < 36 /* 0x24 */; ++i) {
            int16_t row, col;
            Bldg_Back_To_Front(i, &col, &row);

            if (full_draw != 0 || (scanned_bldg_C != -1 && scanned_bldg_R != -1)) {
                COLDRAW::Draw_Veggie_CR(col, row, !is_scanning_valid);

                int32_t building_id = colony_bldgs[col].buildings[row];
                if (building_id != 0) {
                    s_animation_header* anim = COLBCACH::Cache_Load_Bldg(col, row, building_id);
                    int16_t frame_count = animate::Get_Animation_Frame_Count(anim);

                    if (frame_count > 1 || full_draw != 0 || is_scanning_valid) {
                        Draw_Colony_Bldg(col, row);
                    }
                }
            }
        }
    }

    void __cdecl Draw_Colony_Screen() {
        video::Set_Page_Off();

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            full_draw = 1;
        }

        if (full_draw != 0) {
            graphics::Fill(0, 0, 639 /* 0x27f */, 479 /* 0x1df */, 0);

            s_animation_header* background_anim = C_Anims(1);
            animate::Draw(0, 0, background_anim);

            s_animation_header* base_anim = C_Anims(0);
            animate::Draw(0, 0, base_anim);

            COLDRAW::Draw_Road_List();

            if (valid_colony != 0) {
                Draw_Colony_Info_Background();
            }

            if (drawing_display == 0 || drawing_display == 2) {
                if (debug_flag != 0) {
                    char* msg = ESTRINGS::E_Strings(274 /* ESTR_DRAW_SYSTEM */);
                    fonts::PSTRS(0, 0, msg);
                }
                Set_Colony_Font();
                COLSYSDI::Draw_Col_Sys_Disp(7, 24 /* 0x18 */);
            } else if (drawing_display == 1) {
                if (debug_flag != 0) {
                    char* msg = ESTRINGS::E_Strings(273 /* ESTR_DRAW_GALAXY */);
                    fonts::PSTRS(0, 0, msg);
                }

                int16_t width = MOX::galaxy_map_width;
                int16_t height = MOX::galaxy_map_height;
                s_animation_header* anim_c = C_Anims(12 /* 0xc */);
                s_animation_header* anim_b = C_Anims(11 /* 0xb */);

                MOVEBOX::Draw_Galaxy_Map_Box(anim_b, anim_c, 7, 24 /* 0x18 */, height, width, 11 /* 0xb */, 31 /* 0x1f */, 0, 0, 3, 0);
            }

            if (valid_colony != 0) {
                Draw_Colony_Bldgs();
                Draw_Colony_Satellites();
            }

            video::Copy_Off_To_Back();
            video::Set_Refresh_Stencil();
            full_draw = 0;
        } else {
            if (valid_colony != 0) {
                Draw_Colony_Bldgs();
                Draw_Colony_Satellites();
            }
        }

        if (drawing_display == 1) {
            Box_Galaxy_Map_Star(star_handle, 0);
            if (star_handle != sys_disp_star_handle) {
                Box_Galaxy_Map_Star(sys_disp_star_handle, 1);
            }
        }

        if (valid_colony != 0) {
            Draw_Colony_Info();
        } else {
            No_Op1();
        }

        FAKEBUTS::Draw_Fake_Buttons();
        fields::Draw_Visible_Fields();

        if (field_mode == 1) {
            int16_t mouse_y = mouse::Pointer_Y();
            int16_t mouse_x = mouse::Pointer_X();
            COLMOVE::Draw_Cluster(mouse_x, mouse_y);
        }

        palette::Cycle_Palette_Color(255 /* 0xff */, 19 /* 0x13 */, 16 /* 0x10 */, 9, 32 /* 0x20 */, 40 /* 0x28 */, 21 /* 0x15 */, 2);
    }

    void __cdecl Show_Colony_Screen() {
        MOX::FULL_DRAW_SCREEN_FLAG = 0;
        fields::Deactivate_Auto_Function();
        full_draw = 1;
        fields::Assign_Auto_Function(Draw_Colony_Screen, 1);
        ERICHELP::Set_Colony_Screen_Help_List();
    }

    void __cdecl Restore_Control() {
        Show_Colony_Screen();
    }

    void __cdecl Draw_Colony_Info() {
        Draw_Colony_Info_Military();
        Draw_Colony_Info_Officer();
        Draw_Colony_Info_Production();
        Draw_Colony_Info_Pop();
        Draw_Colony_Info_Morale();
        Draw_Info_Name_And_Pop();
        Draw_Info_Build();
        Draw_Sys_Disp_Scanned();
        Draw_Colony_Info_Scanned();
    }

    void __cdecl Draw_Planet_Summary(int16_t x, int16_t y, int16_t planet_idx) {
        char buffer[2000];

        fonts::Set_Font_Style_Shadow_Down(1, &font_color_array);

        graphics::Fill(x + 5, y + 2, x + 0xb0, y + 0xc3, 0xe9);

        s_animation_header* planet_anim = Mini_Planet_Anim(planet_idx);
        remap::Calculate_Picture_Remap_Colors(planet_anim);

        s_animation_header* ui_anim = C_Anims(0xe);
        animate::Draw(x, y, ui_anim);

        MAINPUPS::Build_Generic_Planet_Or_Colony_Description_String_Formatted(planet_idx, buffer, sizeof(buffer));

        BILL::Squeeze_Print_Formatted_Paragraph(x + 14, y + 45, 154, 140, buffer, 0);
    }

    int16_t __cdecl Bldg_Coords_To_Effective_Frame(int16_t x, int32_t y) {
        if ((y % 2) == 0) {
            return (int16_t)y * 6 + x;
        } else {
            return ((int16_t)y * 6 - x) + 5;
        }
    }

    void __cdecl Colony_Screen() {
        int16_t input_val;
        int16_t scanned_val;
        bool input_received = false;
        bool quit_flag = false;

        uint32_t saved_seed = game_random::Get_Random_Seed();
        game_random::Set_Random_Seed(colony_handle);
        video::Clip_Off();
        using_colony_screen_palette = 1;

        star_handle = MOX::screen_data;
        orbit_n = (int32_t)orbit_temp;
        int16_t planet_idx = HAROLD::Planet_Index(MOX::screen_data, orbit_temp);
        planet_handle = planet_idx;

        colony_handle = MOX::planet[planet_handle].colony_index;
        g_scanned_star = -1;
        g_scanned_planet = -1;
        sys_disp_star_handle = star_handle;

        if (colony_handle == -1) {
            valid_colony = 0;
            drawing_display = 2;
            planet_display_handle = planet_idx;
        } else {
            valid_colony = 1;
            drawing_display = 0;
            g_scanned_planet = -1;
        }

        fields::Clear_Mouse_Buffer();
        fields::Set_Input_Delay(3);
        ERICHELP::Set_Colony_Screen_Help_List();

        int16_t saved_player_num = MOX::PLAYER_NUM;

        if (!valid_colony) {
            MOX::current_screen = MOX::return_screen;
            MOX::PLAYER_NUM = saved_player_num;
            using_colony_screen_palette = 0;
            game_random::Set_Random_Seed(saved_seed);
            return;
        }

        MOX::PLAYER_NUM = (int16_t)MOX::colony[colony_handle].owner;

        Load_Colony_Screen_Seg();
        Load_Planet_Screen();

        Make_Bldg_Array_For_Colony(colony_handle, 0);

        if (MOX::return_screen != MOX::current_screen) {
            colony_saved_screen = MOX::return_screen;
        }

        fields::Clear_Fields();
        Set_Colony_Font();
        mouse::Set_Mouse_List(&COLGCBT::mouse_list_blank, 1);
        video::Set_Page_Off();
        video::Set_Refresh_Stencil();

        if (ERIC::Fast_Fade_Out_Needed_For_Colony_Palette()) {
            ERIC::Fast_Fade_Out();
        }

        Update_Colony_Palette();

        if (ERIC::Fast_Fade_Out_Needed_For_Colony_Palette()) {
            palstore::Darken_Palette(100);
            palette::Clear_Palette_Changes(0, 255);
        }

        palette::Refresh_Palette();
        Reset_Field_Arrays();
        Update_Fields();
        full_draw = 1;

        fields::Assign_Auto_Function(Draw_Colony_Screen, 1);
        graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
        Draw_Colony_Screen();
        ERIC::Fast_Fade_In();
        video::Set_Refresh_Stencil();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        Do_Informational_And_Decision_Popups();

        while (true) {
            timer::Mark_Time();
            g_count++;

            if (input_received) {
                Update_Fields();
                input_val = 0;
            }

            Draw_Colony_Screen();
            RUSS::Mox_Sync_Update();

            input_val = fields::Get_Input();
            scanned_val = fields::Scan_Input();
            input_received = (input_val != 0);

            if (!Evaluate_Input(&input_val, &scanned_val)) {
                quit_flag = true;
            }

            timer::Release_Time(1);

            if (quit_flag) break;

            if (valid_colony) {
                if (input_val == colony_fields[1]) break;
            } else {
                if (input_val == colony_fields[0] && input_val == colony_fields[1]) break;
            }
        }

        Reset_Field_Mode();
        fields::Clear_Fields();
        FAKEBUTS::Clear_Fake_Buttons();
        fields::Deactivate_Auto_Function();

        MOX::PLAYER_NUM = saved_player_num;
        using_colony_screen_palette = 0;

        if (!quit_flag) {
            Cleanup_Colony_Queue(colony_handle);
            if (MOX::return_screen == MOX::current_screen) {
                MOX::current_screen = colony_saved_screen;
                MOX::return_screen = colony_saved_screen;
            } else {
                MOX::current_screen = MOX::return_screen;
            }
        }

        fields::Deactivate_Help_List();
        game_random::Set_Random_Seed(saved_seed);
    }

    void __cdecl Load_Colony_Screen_Seg() {
        COLBLDG::Load_Build_Anims();
        RUSS::Mox_Update();
        COLXPORT::Load_Xport_Anims();
        RUSS::Mox_Update();
        Load_Ship_Bitm();
        RUSS::Mox_Update();
        Load_Bldg_Bitm();
        RUSS::Mox_Update();
        Load_Officer_Anim();
        RUSS::Mox_Update();
        COLDRAW::Load_Road_List_Anims();
        COLBLDG::Clear_Last_App_Desc();
        RUSS::Mox_Update();
        Load_Galaxy_Map_Anims();
        RUSS::Mox_Update();

        c_anims[19] = (s_animation_header*)farload::Far_Reload_Next(COLPUPS_LBX, 1, MOX::screen_seg);
        c_anims[20] = (s_animation_header*)farload::Far_Reload_Next(COLPUPS_LBX, 2, MOX::screen_seg);
        c_anims[21] = (s_animation_header*)farload::Far_Reload_Next(COLPUPS_LBX, 4, MOX::screen_seg);
        c_anims[22] = (s_animation_header*)farload::Far_Reload_Next(COLPUPS_LBX, 7, MOX::screen_seg);
        c_anims[23] = (s_animation_header*)farload::Far_Reload_Next(COLPUPS_LBX, 8, MOX::screen_seg);
        c_anims[24] = (s_animation_header*)farload::Far_Reload_Next(COLPUPS_LBX, 9, MOX::screen_seg);
        c_anims[32] = (s_animation_header*)farload::Far_Reload_Next(COLPUPS_LBX, 21, MOX::screen_seg);
    }

    void __cdecl Load_Officer_Anim() {
        int8_t officer_idx = MOX::star[COLONY::star_handle].officer_index[MOX::PLAYER_NUM];

        if (officer_idx != -1) {
            c_anims[30] = OFFICER::Load_Officer_Picture(
                (int32_t)MOX::PLAYER_NUM,
                (int16_t)officer_idx,
                MOX::screen_seg,
                0,
                1);
        }
    }

    void __cdecl Draw_Building_With_Bottom_Centered(int16_t x, int16_t y, int16_t type, int16_t col, int16_t row) {
        s_animation_header* bldg_anim = COLBCACH::Cache_Load_Bldg(col, row, type);

        int32_t cr_x = CR_To_XY(col, row, 0);
        int32_t cr_y = CR_To_XY(col, row, 1);

        bitmap::Create_Blank_Bitmap(120, 120, COLBLDG::bldg_bitm);

        int16_t offset_x = -(static_cast<int16_t>(cr_x) - 60);
        int16_t offset_y = -(static_cast<int16_t>(cr_y) - 120);

        bitmap::Add_Picture_To_Bitmap(offset_x, offset_y, bldg_anim, (s_animation_header*)COLBLDG::bldg_bitm);
        bitmap::Mask_Out_Colors(COLBLDG::bldg_bitm, 240, 255);

        int16_t min_x, min_y, actual_w, actual_h;
        bitmap::Get_Bitmap_Actual_Size((s_animation_header*)COLBLDG::bldg_bitm, &min_x, &min_y, &actual_w, &actual_h);

        int16_t draw_x = x - (actual_w / 2) - min_x;
        int16_t draw_y = y - actual_h - min_y;

        animate::Draw(draw_x, draw_y, (s_animation_header*)COLBLDG::bldg_bitm);
    }

    s_animation_header* __cdecl People_Anim(int16_t job_type, int16_t pop_state, int16_t race_idx) {
        if (pop_state == 2) {
            int32_t entry_num = ((int32_t)race_idx * 13) + ((int32_t)job_type * 2) + 1;
            return (s_animation_header*)buffer::Buffer_Reload(RACEICON_LBX, entry_num, MOX::global_cache_seg);
        }

        if (pop_state == 0) {
            int32_t entry_num = ((int32_t)race_idx * 13) + ((int32_t)job_type * 2);
            return (s_animation_header*)buffer::Buffer_Reload(RACEICON_LBX, entry_num, MOX::global_cache_seg);
        }

        if (pop_state == 3) {
            return (s_animation_header*)buffer::Buffer_Reload(RACEICON_LBX, 0xAA, MOX::global_cache_seg);
        }

        if (pop_state == 4) {
            return (s_animation_header*)buffer::Buffer_Reload(RACEICON_LBX, 0xA9, MOX::global_cache_seg);
        }

        return nullptr;
    }

    s_animation_header* __cdecl Satellite_Anim(int16_t colony_id) {
        int16_t pic_index = Satellite_Anim_Pic(colony_id);
        return Buffer_Reload_Colony(pic_index);
    }

    s_animation_header* __cdecl C_Anims(int16_t anim_id) {
        RUSS::Mox_Update();

        switch (anim_id) {
            case 0:
                return Cache_Load_Planet(
                    static_cast<int16_t>(static_cast<int8_t>(MOX::planet[MOX::colony[colony_handle].planet].climate_bg_type) +
                        static_cast<uint8_t>(MOX::colony[colony_handle].climate) * 3));
            case 1:
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x31, MOX::global_cache_seg);
            case 2:
            case 11: // 0x0B
            case 12: // 0x0C
            case 16: // 0x10
            case 17: // 0x11
            case 18: // 0x12
            case 19: // 0x13
            case 20: // 0x14
            case 21: // 0x15
            case 22: // 0x16
            case 23: // 0x17
            case 24: // 0x18
            case 25: // 0x19
            case 26: // 0x1A
            case 27: // 0x1B
            case 28: // 0x1C
            case 30: // 0x1E
            case 31: // 0x1F
            case 32: // 0x20
                return c_anims[anim_id];
            case 3:
                return (s_animation_header*)buffer::Buffer_Reload(COLPUPS_LBX, 5, MOX::global_cache_seg);
            case 4:
                return (s_animation_header*)buffer::Buffer_Reload(COLPUPS_LBX, 6, MOX::global_cache_seg);
            case 5:
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x10, MOX::global_cache_seg);
            case 6:
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x11, MOX::global_cache_seg);
            case 7:
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x12, MOX::global_cache_seg);
            case 8:
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x2B, MOX::global_cache_seg);
            case 9:
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x2C, MOX::global_cache_seg);
            case 10:
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x2D, MOX::global_cache_seg);
            case 13: // 0x0D
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x32, MOX::global_cache_seg);
            case 14: // 0x0E
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x33, MOX::global_cache_seg);
            case 15: // 0x0F
                return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, 0x34, MOX::global_cache_seg);
            case 33: // 0x21
                return (s_animation_header*)buffer::Buffer_Reload(COLPUPS_LBX, 0x16, MOX::global_cache_seg);
            case 34: // 0x22
                return (s_animation_header*)buffer::Buffer_Reload(COLONY_LBX, 0x11, MOX::global_cache_seg);
            case 35: // 0x23
                return (s_animation_header*)buffer::Buffer_Reload(COLONY_LBX, 0x12, MOX::global_cache_seg);
            default:
                return nullptr;
        }
    }

    void __cdecl Make_Bldg_Array_For_Colony(int16_t colony_idx, uint8_t is_ui_view) {
        uint32_t previous_seed = game_random::Get_Random_Seed();
        game_random::Set_Random_Seed(colony_idx);

        int16_t satellite_count = 0;
        Clear_Colony_Buidlings();
        Clear_Satellite_Array();

        int bldg_count = 0;
        for (int16_t bldg_id = 0; bldg_id < BUILDING_COUNT; ++bldg_id) {
            if (MOX::colony[colony_idx].buildings[bldg_id] != 0) {
                if (is_ui_view == 0 || bldg_count < 18) {
                    if (TECHDATA::buildings[bldg_id].group == 7 /* Satellite */) {
                        Insert_Satellite_Into_Array(bldg_id, &satellite_count);
                    } else {
                        Insert_Bldg_Into_Array(colony_idx, bldg_id, bldg_count, is_ui_view);
                        bldg_count++;
                    }
                }
            }
        }

        int housing_to_add = (static_cast<uint8_t>(MOX::colony[colony_idx].n_pops) / 3) + 1;
        for (int h = 0; h < housing_to_add; ++h) {
            if (bldg_count < 36) {
                if (is_ui_view == 0 || (is_ui_view == 1 && bldg_count < 18)) {
                    Insert_Bldg_Into_Array(colony_idx, COLONY_PRODUCTION_HOUSING, bldg_count, is_ui_view);
                    bldg_count++;
                }
            }
        }

        Sort_Bldg_Array_Columns();

        if (is_ui_view == 0) {
            BUILDING target_jitter_ids[4] = {
                BUILDING_ARTEMIS_SYSTEM_NET,
                BUILDING_DIMENSIONAL_PORTAL,
                BUILDING_STAR_BASE,
                BUILDING_STAR_FORTRESS};
            for (int k = 0; k < 8; ++k) {
                int16_t bldg_id = target_jitter_ids[k % 4];
                int16_t row, col;
                if (Get_Bldg_CR(bldg_id, &row, &col)) {
                    bool swapped = false;
                    for (int16_t si = 1; si >= -1 && !swapped; --si) {
                        for (int16_t di = 1; di >= -1 && !swapped; --di) {
                            int tr = row - si;
                            if (tr < 0) tr = 0;
                            if (tr > 5) tr = 5;
                            int tc = tr;
                            bool should_swap = false;
                            if (COLONY::colony_bldgs[tr].buildings[tc] == 0) {
                                if (game_random::Random(3) == 1) should_swap = true;
                            } else {
                                if (game_random::Random(3) == 1) {
                                    should_swap = true;
                                } else if (game_random::Random(3) == 1) {
                                    should_swap = true;
                                }
                            }
                            if (should_swap) {
                                int32_t temp_id = COLONY::colony_bldgs[tr].buildings[tc];
                                COLONY::colony_bldgs[tr].buildings[tc] = COLONY::colony_bldgs[row].buildings[col];
                                COLONY::colony_bldgs[row].buildings[col] = temp_id;
                                swapped = true;
                            }
                        }
                    }
                }
            }
            int16_t sp_row, sp_col;
            if (Get_Bldg_CR(BUILDING_CAPITOL, &sp_row, &sp_col)) {
                if (COLONY::colony_bldgs[1].buildings[1] != 0) {
                    int32_t temp_val = COLONY::colony_bldgs[1].buildings[1];
                    COLONY::colony_bldgs[1].buildings[1] = COLONY::colony_bldgs[sp_row].buildings[sp_col];
                    COLONY::colony_bldgs[sp_row].buildings[sp_col] = temp_val;
                }
            }
        }

        COLDRAW::Build_Road_List_Based_On_Bldg_List();

        if (is_ui_view == 0) {
            COLDRAW::Build_Veggie_List_Based_On_Bldg_List();
        } else {
            COLDRAW::Clear_Veggie_Draw_Flags();
        }

        game_random::Set_Random_Seed(previous_seed);
    }

    void __cdecl Insert_Bldg_Into_Array(int16_t num_houses_delta, int16_t bldg_id, int16_t bldg_type, int8_t use_back_to_front) {
        int16_t row = -1;
        int16_t col = -1;
        int16_t n_houses = COLONY::N_Houses_In_Bldg_Array();

        int total = (int)n_houses + (int)num_houses_delta;
        int16_t house_mod = (int16_t)(total % 4);

        if (bldg_type < 36 /* 0x24 */) {
            if (use_back_to_front == 1) {
                COLONY::Bldg_Back_To_Front(bldg_type, &row, &col);
            } else {
                int16_t empty_count = 0;
                for (int16_t i = 0; i < 6; ++i) {
                    for (int16_t j = 0; j < 6; ++j) {
                        if (COLONY::colony_bldgs[i].buildings[j] == 0) {
                            empty_count++;
                            if (game_random::Random((int)empty_count) == 1) {
                                row = i;
                                col = j;
                            }
                        }
                    }
                }

                if (row == -1 || (col == -1 && bldg_id != -3)) {
                    COLONY::Find_Replacement_Slot_For_Building(bldg_id, &row, &col);
                }
            }
        } else {
            COLONY::Find_Replacement_Slot_For_Building(bldg_id, &row, &col);
        }

        if (row == -1 || col == -1) {
            return;
        }

        if (bldg_id >= 0) {
            COLONY::colony_bldgs[row].buildings[col] = bldg_id;
        } else if (bldg_id == COLONY_PRODUCTION_HOUSING) {
            house_mod++;
            int switch_idx = (int)house_mod % 4;

            if (static_cast<uint32_t>(switch_idx) > 3) {
                COLONY::colony_bldgs[row].buildings[col] = BUILDING_DIMENSIONAL_PORTAL /* 0x0E */;
            } else {
                switch (switch_idx) {
                    case 0: COLONY::colony_bldgs[row].buildings[col] = BUILDING_ARTEMIS_SYSTEM_NET /* 3 */; break;
                    case 1: COLONY::colony_bldgs[row].buildings[col] = BUILDING_DIMENSIONAL_PORTAL /* 0x0E */; break;
                    case 2: COLONY::colony_bldgs[row].buildings[col] = BUILDING_STAR_BASE /* 0x28 */; break;
                    case 3: COLONY::colony_bldgs[row].buildings[col] = BUILDING_STAR_FORTRESS /* 0x29 */; break;
                }
            }
        } else if (bldg_id == -1) {
            COLONY::colony_bldgs[row].buildings[col] = BUILDING_BATTLESTATION /* 8 */;
        }
    }

    void Sort_Bldg_Array_Columns() {
        int16_t iteration = 0;
        int16_t swapped_flag;

        do {
            swapped_flag = 0;

            for (int16_t row = 0; row < 5; ++row) {
                for (int16_t col = 0; col < 5; ++col) {
                    int32_t& b1 = colony_bldgs[row].buildings[col];

                    int32_t& b_diag = colony_bldgs[row + 1].buildings[col + 1];
                    if (b1 != 0 && b_diag != 0 &&
                        TECHDATA::buildings[b_diag].group < TECHDATA::buildings[b1].group) {
                        swapped_flag = static_cast<int16_t>(b1);
                        int32_t swapped_building = b1;
                        b1 = b_diag;
                        b_diag = swapped_building;
                    }

                    int32_t& b_below = colony_bldgs[row + 1].buildings[col];
                    if (b1 != 0 && b_below != 0 &&
                        TECHDATA::buildings[b_below].group < TECHDATA::buildings[b1].group) {
                        swapped_flag = static_cast<int16_t>(b1);
                        int32_t swapped_building = b1;
                        b1 = b_below;
                        b_below = swapped_building;
                    }

                    int32_t& b_right = colony_bldgs[row].buildings[col + 1];
                    if (b1 != 0 && b_right != 0 &&
                        TECHDATA::buildings[b_right].group < TECHDATA::buildings[b1].group) {
                        swapped_flag = static_cast<int16_t>(b1);
                        int32_t swapped_building = b1;
                        b1 = b_right;
                        b_right = swapped_building;
                    }
                }
            }
        } while (swapped_flag != 0 && ++iteration < 6);
    }

    void __cdecl Tested_Colony_Buys_Outright(int16_t colony_idx) {
        s_colony* col = &MOX::colony[colony_idx];
        char buf[500];

        if (col->bought_outright != 0) {
            if (!BUILD_QUEUE::Unbuy_Product(col)) {
                GENDRAW::Help(ESTRINGS::E_Strings(ESTR_YOU_HAVE_ALREADY_BOUGHT_THIS_PRODUCT));
                COLONY::Show_Colony_Screen();
            }
            return;
        }

        int16_t star_idx = MOX::planet[col->planet].star_index;
        if ((MOX::star[star_idx].blockaded >> (MOX::PLAYER_NUM & 0x1f)) & 1) {
            GENDRAW::Help(ESTRINGS::E_Strings(ESTR_YOU_CANNOT_BUY_WHILE_THIS_STAR_IS_BLOCKADED));
            COLONY::Show_Colony_Screen();
            return;
        }

        if (col->producing[0] == COLONY_PRODUCTION_HOUSING || col->producing[0] == COLONY_PRODUCTION_TRADE_GOODS) {
            GENDRAW::Help(ESTRINGS::E_Strings(ESTR_YOU_CANNOT_BUY_HOUSING_OR_TRADE_GOODS_AS_THEY_ARE_SETTINGS_NOT_SPECIFIC_ITEMS));
            COLONY::Show_Colony_Screen();
            return;
        }

        int buy_cost = COLCALC::Colony_Cost_To_Buy_Producing_Product(col);
        if (buy_cost <= 0) {
            if (col->producing[0] != COLONY_PRODUCTION_NONE) {
                GENDRAW::Help(ESTRINGS::E_Strings(ESTR_YOU_CANNOT_SPEND_BCS_TO_GET_THIS_FASTER_THAN_IN_A_SINGLE_TURN));
            } else {
                GENDRAW::Help(ESTRINGS::E_Strings(ESTR_YOU_HAVE_NO_PRODUCT_SELECTED_TO_BUY));
            }
            COLONY::Show_Colony_Screen();
            return;
        }

        if (COLCALC::Colony_Can_Afford_To_Buy_Product(colony_idx)) {
            if (COLONY::Colony_Can_Buy_Product_0(colony_idx)) {
                int prod_remaining = (int)(int16_t)COLCALC::Colony_Producing_Product_Cost(col) - (int)col->production_spent;
                int actual_buy_cost = COLCALC::Colony_Cost_To_Buy_Producing_Product(col);

                snprintf(buf, sizeof(buf), ESTRINGS::E_Strings(ESTR_DO_YOU_WANT_TO_SPEND_D_BC_TO_BUY_D_PRODUCTION), actual_buy_cost, prod_remaining);

                if (GENDRAW::Do_Confirmation_Box(buf) != 0) {
                    COLCALC::Colony_Buy_Outright(colony_idx);
                }
                COLONY::Col_Calc_Wrapper(colony_idx);
            } else {
                GENDRAW::Help(ESTRINGS::E_Strings(ESTR_PURCHASING_FAILED_UNEXPECTEDLY_PLEASE_TELL_ERIC));
                COLONY::Show_Colony_Screen();
            }
        } else {
            int16_t short_by = (int16_t)((uint16_t)COLCALC::Colony_Cost_To_Buy_Producing_Product(col) -
                (uint16_t)MOX::player[col->owner].bc);
            snprintf(buf, sizeof(buf), ESTRINGS::E_Strings(ESTR_YOU_CANNOT_AFFORD_THIS_INCREASE_IN_PRODUCTION_YOU_ARE_SHORT_BY_D_BC), short_by);
            GENDRAW::Help(buf);
            COLONY::Show_Colony_Screen();
        }
    }

    void Draw_Info_Name_And_Pop() {
        char buffer[104];
        int16_t handle = colony_handle;

        if (MOX::colony[handle].occupation_policy == 0) {
            char* planet_name = HACCESS::Get_Planet_Name(planet_handle);
            char* fmt = ESTRINGS::E_Strings(ESTR_ANNIHILATING_S);
            snprintf(buffer, sizeof(buffer), fmt, planet_name);
            int16_t cycle_val = ERIC::Global_Cycler(30, 2);
            Set_Colony_Font_To_Red(4, (uint8_t)cycle_val);
        } else {
            char* planet_name = HACCESS::Get_Planet_Name(planet_handle);
            char* colony_type = Colony_Type(handle);
            char* fmt = ESTRINGS::E_Strings(ESTR_SCOLONY_OF_S);
            snprintf(buffer, sizeof(buffer), fmt, colony_type, planet_name);
            Set_Colony_Font_To(4);
        }

        fonts::Set_Font_Spacing(2);
        fonts::Print_Centered(320, 0, buffer);

        int16_t colony_planet = MOX::colony[handle].planet;
        int16_t star_idx = MOX::planet[colony_planet].star_index;

        int16_t status_msg_id = -1;
        if ((MOX::star[star_idx].blockaded >> (MOX::PLAYER_NUM & 0x1F)) & 1) {
            int16_t cycle_val = ERIC::Global_Cycler(30, 2);
            Set_Colony_Font_To_Red(4, (uint8_t)cycle_val);
            fonts::Set_Font_Spacing(2);
            status_msg_id = ESTR_BLOCKADED1;
        } else if (EVENTS::Event_Check_Plague(handle)) {
            int16_t cycle_val = ERIC::Global_Cycler(30, 2);
            Set_Colony_Font_To_Red(4, (uint8_t)cycle_val);
            fonts::Set_Font_Spacing(2);
            status_msg_id = ESTR_PLAGUE;
        } else if (EVENTS::Event_Check_Population_Boom(handle)) {
            Set_Colony_Font_To(4);
            fonts::Set_Font_Spacing(2);
            status_msg_id = ESTR_POP_BOOM;
        }

        if (status_msg_id != -1) {
            char* status_text = ESTRINGS::E_Strings(status_msg_id);
            fonts::Print(0, 0, status_text);
        }

        Set_Colony_Font_To(4);
        fonts::Set_Font_Spacing(3);

        int sum_roundoff = 0;
        int sum_growth = 0;
        for (int i = 0; i < 10; ++i) {
            sum_roundoff += MOX::colony[handle].pop_roundoff[i];
            sum_growth += MOX::colony[handle].pop_growth[i];
        }

        int16_t n_base_pop = static_cast<int16_t>(Sum_Colonists(handle, -1, -1, -1, -1, -1));
        int total_pop = n_base_pop + (sum_roundoff / 1000);
        int roundoff_rem = sum_roundoff % 1000;

        char* pop_fmt = ESTRINGS::E_Strings(ESTR_POP_D03D_K_DK);
        snprintf(buffer, sizeof(buffer), pop_fmt, total_pop, roundoff_rem, sum_growth);

        fonts::Set_Font_Style_Shadow_Down(2, &font_color_array);
        fonts::Print_Right(638, 3, buffer);
        Set_Colony_Font();
    }

    void __cdecl Draw_Current_Building2(int16_t x, int16_t y, int16_t building_id) {
        if (building_id == COLONY_PRODUCTION_HOUSING) {
            graphics::Set_Window(517, 17, 639, 158);
            video::Clip_On();
            COLONY::Draw_Building_With_Bottom_Centered(579, 106, 14, 5, 5);
            video::Clip_Off();
            return;
        }

        if (TECHDATA::buildings[building_id].group != 7) {
            graphics::Set_Window(517, 17, 639, 158);
            video::Clip_On();
            COLONY::Draw_Building_With_Bottom_Centered(579, 106, building_id, 5, 5);
            video::Clip_Off();
            return;
        }

        s_animation_header* animation = COLONY::Satellite_Anim(building_id);
        ERIC::Draw_Centered(x, (int16_t)(y - 70), animation);
    }

    void __cdecl Draw_Info_Build() {
        char name_buffer[100];
        s_colony& colony = MOX::colony[COLONY::colony_handle];
        int16_t producing_id = colony.producing[0];

        switch (producing_id) {
            case COLONY_PRODUCTION_FREIGHTERS:
                COLONY::Remap_Draw_Ship_Seg_N_Centered(0x242 /* 578 */, 0x4d /* 77 */, 0x30 /* 48 */);
                break;
            case COLONY_PRODUCTION_COLONY_SHIP:
                COLONY::Remap_Draw_Ship_Seg_N_Centered(0x242 /* 578 */, 0x39 /* 57 */, 0x2d /* 45 */);
                break;
            case COLONY_PRODUCTION_TRANSPORT_SHIP:
                COLONY::Remap_Draw_Ship_Seg_N_Centered(0x242 /* 578 */, 0x39 /* 57 */, 0x2f /* 47 */);
                break;
            case COLONY_PRODUCTION_REFIT:
            case COLONY_PRODUCTION_TRADE_GOODS:
            case COLONY_PRODUCTION_NONE:
            case 0:
                // Do nothing
                break;
            case COLONY_PRODUCTION_SPY: {
                uint16_t race = (uint16_t)(uint8_t)MOX::player[MOX::PLAYER_NUM].race;
                s_animation_header* anim = COLONY::Spy_Anim(race);
                ERIC::Draw_Centered(0x242 /* 578 */, 0x4d /* 77 */, anim);
                break;
            }
            case COLONY_PRODUCTION_SCIENTIST:
            case COLONY_PRODUCTION_WORKER:
            case COLONY_PRODUCTION_FARMER: {
                uint16_t race = (uint16_t)(uint8_t)MOX::player[MOX::PLAYER_NUM].race;
                s_animation_header* anim = COLONY::People_Anim(0, 4, race);
                ERIC::Draw_Centered(0x242 /* 578 */, 0x4d /* 77 */, anim);
                break;
            }
            case COLONY_PRODUCTION_HOUSING:
                COLONY::Draw_Current_Building(COLONY_PRODUCTION_HOUSING);
                break;
            default:
                COLONY::Set_Colony_Font();
                if (COLBLDG::Colony_Production_Is_Building(producing_id)) {
                    COLONY::Draw_Current_Building(producing_id);
                } else if (COLBLDG::Colony_Production_Is_Queued_Ship(producing_id)) {
                    COLONY::Draw_Current_Ship(COLBLDG::Colony_Production_Ship_Index(producing_id));
                } else {
                    char* err_msg = ESTRINGS::E_Strings(0xf7);
                    RUSS::PFMT(0, 0, err_msg, producing_id);
                    key::Wait_For_Key();
                }
                break;
        }

        COLONY::Draw_Current_Production_Graph();

        char* name_str = COLBLDG::Selection_Name(producing_id);
        strlcpy(name_buffer, name_str, sizeof(name_buffer));

        fonts::Set_Font_Style_Shadow_Down(1, &COLONY::font_color_array);
        BILL::Squeeze_Print_Paragraph(0x20a /* 522 */, 0x26 /* 38 */, 0x70 /* 112 */, 0x8d /* 141 */, name_buffer, 2);

        const int autobuild_preset = BUILD_QUEUE::autobuild_settings.enabled != 0
            ? BUILD_QUEUE::Decode_Autobuild_Preset(colony.auto_building)
            : -1;
        if (BUILD_QUEUE::Is_Legacy_Autobuild(colony.auto_building) || autobuild_preset >= 0) {
            fonts::Set_Font_Style_Shadow_Down(1, &COLONY::bright_blue_font_color_array);
            if (autobuild_preset >= 0) {
                char auto_msg[40];
                snprintf(auto_msg, sizeof(auto_msg), "Auto Build Queue %d", autobuild_preset);
                fonts::Print_Centered(0x242 /* 578 */, 0x1c /* 28 */, auto_msg);
            } else {
                fonts::Print_Centered(0x242 /* 578 */, 0x1c /* 28 */, ESTRINGS::E_Strings(0xbd));
            }
        }

        COLONY::Draw_Turn_Count();
    }

    void __cdecl Add_Screen_Fields() {
        colony_fields[10] = -1000;
        colony_fields[9] = -1000;

        colony_fields[2] = fields::Add_Hot_Key(ESTRINGS::E_Strings(0x87));
        colony_fields[6] = fields::Add_Hot_Key(ESTRINGS::E_Strings(0x8b));
        colony_fields[7] = fields::Add_Hot_Key(ESTRINGS::E_Strings(0x8c));

        colony_fields[19] = fields::Add_Multi_Hot_Key_Field(ESTRINGS::E_Strings(0xf5));
        colony_fields[20] = fields::Add_Multi_Hot_Key_Field(ESTRINGS::E_Strings(0x218));

        colony_fields[1] = fields::Add_Button_Field(0x22c, 0x1cb, ESTRINGS::E_Strings(0xc), COLONY::C_Anims(0x15), ESTRINGS::E_Strings(0xc), 0x28);
        colony_fields[5] = fields::Add_Hot_Key("'");
        _shortcut_previous_colony = fields::Add_Hot_Key(",");
        _shortcut_next_colony = fields::Add_Hot_Key(".");
        _shortcut_return = fields::Add_Hot_Key(" ");

        if (MOX::star[star_handle].officer_index[MOX::PLAYER_NUM] == -1) {
            colony_fields[17] = fields::Add_Button_Field(0x22c, 0x1ab, ESTRINGS::E_Strings(0xc), COLONY::C_Anims(0x20), ESTRINGS::E_Strings(0x158), 0x28);
        } else {
            colony_fields[17] = fields::Add_Hidden_Field(0x226, 0x165, 0x26f, 0x1bd, ESTRINGS::E_Strings(0x158), 0);
        }

        colony_fields[12] = fields::Add_Hidden_Field(0x136, 0x20, 0x1fe, 0x3d, ESTRINGS::E_Strings(0xc), 0);
        colony_fields[13] = fields::Add_Hidden_Field(0x80, 0x20, 0x12d, 0x3d, ESTRINGS::E_Strings(0xc), 0);
        colony_fields[14] = fields::Add_Hidden_Field(0x80, 0x3e, 0x12d, 0x5b, ESTRINGS::E_Strings(0xc), 0);
        colony_fields[15] = fields::Add_Hidden_Field(0x80, 0x5c, 0x12d, 0x79, ESTRINGS::E_Strings(0xc), 0);
        colony_fields[16] = fields::Add_Hidden_Field(0x80, 0x7a, 0x12d, 0x97, ESTRINGS::E_Strings(0xc), 0);

        if (valid_colony != 0) {
            colony_fields[4] = fields::Add_Button_Field(0x207, 0x7b, ESTRINGS::E_Strings(0xc), COLONY::C_Anims(0x13), ESTRINGS::E_Strings(0xd5), 0x28);
            colony_fields[18] = fields::Add_Hidden_Field(0x20d, 0x1a, 0x276, 0x25, ESTRINGS::E_Strings(0xa6), 0x29);

            if (Colony_Can_Buy_Product_0(colony_handle)) {
                colony_fields[3] = fields::Add_Button_Field(0x24e, 0x7b, ESTRINGS::E_Strings(0xc), COLONY::C_Anims(0x14), ESTRINGS::E_Strings(0xc1), 0x28);
            } else {
                s_animation_header* buy_anim = COLONY::C_Anims(0x14);
                int16_t h = animate::Get_Height(buy_anim);
                int16_t w = animate::Get_Width(COLONY::C_Anims(0x14));
                colony_fields[3] = fields::Add_Hidden_Field(0x24e, 0x7b, 0x24e + w, 0x7b + h, ESTRINGS::E_Strings(0xc1), 0x28);
            }
        } else {
            colony_fields[4] = -1000;
            colony_fields[3] = -1000;
        }

        Add_Stacking_Fields();
        Add_Satellite_Fields();
        Add_Bldg_Fields();

        colony_fields[0] = fields::Add_Hidden_Field(0, 0, 0x27f, 0x1df, ESTRINGS::E_Strings(0xc), 0);
    }

    int8_t __cdecl Evaluate_Screen_Input(int16_t* input_field) {
        int16_t field_val = *input_field;

        if (colony_fields[8] == field_val) {
            OCPOLPUP::Occupation_Policy_Popup();
            Show_Colony_Screen();
            Col_Calc_Wrapper(colony_handle);
            return 1;
        }

        if (colony_fields[17] == field_val) {
            MOX::temp_star_handle = star_handle;
            MOX::return_screen = MOX::current_screen;
            MOX::screen_mouse_list_type = 1;
            MOX::current_screen = SCREEN_OFFICERS;
            return 0;
        }

        if (colony_fields[9] == field_val) {
            if (drawing_display == 0) {
                drawing_display = 1;
                full_draw = 1;
                return 1;
            }
        }

        if (colony_fields[10] == field_val) {
            if (drawing_display == 1) {
                drawing_display = 0;
                full_draw = 1;
                g_scanned_planet = -1;
                return 1;
            }
        }

        if (colony_fields[11] == field_val) {
            int16_t transport_result = COLXPORT::Xport_Popup();
            Show_Colony_Screen();
            if (transport_result != -1) {
                COLMOVE::Send_Cluster(transport_result, -1);
            }
            Show_Colony_Screen();
            Col_Calc_Wrapper(colony_handle);
            return 1;
        }

        if (colony_fields[18] == field_val) {
            if (BUILD_QUEUE::autobuild_settings.enabled != 0) {
                if (MOX::colony[colony_handle].auto_building !=
                    BUILD_QUEUE::AUTOBUILD_DISABLED) {
                    MOX::colony[colony_handle].auto_building =
                        BUILD_QUEUE::AUTOBUILD_DISABLED;
                } else {
                    Prompt_For_Autobuild_Queue(colony_handle);
                }
            } else {
                MOX::colony[colony_handle].auto_building =
                    (MOX::colony[colony_handle].auto_building == 0);
            }
            Show_Colony_Screen();
            return 1;
        }

        if (colony_fields[3] == field_val) {
            Tested_Colony_Buys_Outright(colony_handle);
            Update_Fields();
            Show_Colony_Screen();
            *input_field = 0;
            fields::Clear_Mouse_Buffer();
            fields::Set_Input_Delay(3);
            return 1;
        }

        if (colony_fields[5] == field_val) {
            if (cheats != 0) {
                Do_Cheats();
                Show_Colony_Screen();
                return 1;
            }
            Col_Calc_Wrapper(colony_handle);
            return 1;
        }

        if (colony_fields[12] == field_val) {
            Show_Morale(colony_handle);
            Show_Colony_Screen();
            return 1;
        }

        if (colony_fields[13] == field_val) {
            Show_BC_Production(colony_handle);
            Show_Colony_Screen();
            return 1;
        }

        if (colony_fields[14] == field_val) {
            Show_Food_Production(colony_handle);
            Show_Colony_Screen();
            return 1;
        }

        if (colony_fields[15] == field_val) {
            COLCALC::Show_Industry_Production(colony_handle);
            Show_Colony_Screen();
            return 1;
        }

        if (colony_fields[16] == field_val) {
            COLCALC::Show_Research_Production(colony_handle);
            Show_Colony_Screen();
            return 1;
        }

        if (colony_fields[4] == field_val) {
            MOX::return_screen = MOX::current_screen;
            MOX::current_screen = 25 /* 0x19 */;
            return 0;
        }

        if (colony_fields[6] == field_val || _shortcut_previous_colony == field_val) {
            int16_t new_colony = Get_Next_Colony(colony_handle, 0);
            if (new_colony != colony_handle) {
                Set_Colony_Screen_To_New_Colony(new_colony);
                return 0;
            }
        } else if (colony_fields[20] == field_val) {
            if (capture::Check_Release_Version() != 0) {
                cheats = (cheats == 0);
                MAINSCR::Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 6);
                ERICNET::Mox_Play_Sound(51 /* 0x33 */);
                return 1;
            }
        } else if (colony_fields[19] == field_val) {
            s_colony* colony = &MOX::colony[colony_handle];
            colony->production_spent = static_cast<int16_t>(COLCALC::Colony_Producing_Product_Cost(colony));
            MAINSCR::Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 6);
            ERICNET::Mox_Play_Sound(51 /* 0x33 */);
            return 1;
        } else if (colony_fields[7] == field_val || _shortcut_next_colony == field_val) {
            int16_t new_colony = Get_Next_Colony(colony_handle, 1);
            if (new_colony != colony_handle) {
                Set_Colony_Screen_To_New_Colony(new_colony);
                return 0;
            }
        } else if (colony_fields[2] == field_val || _shortcut_return == field_val) {
            *input_field = colony_fields[1];
            return 1;
        } else if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            Show_Colony_Screen();
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        }

        return 1;
    }

    int32_t __cdecl Do_Colony_Info_Military_Stuff_For(int32_t military_index, int32_t start_x, int32_t drawn_count, int32_t mode) {
        int military_count;
        int16_t owner;
        int16_t local_n;
        char buffer[256];
        s_animation_header* anim;
        int16_t height;

        military_count = MOX::colony[colony_handle].military[military_index];
        owner = (int16_t)MOX::colony[colony_handle].owner;
        step_squish = 0;

        if (mode != 2) {
            Do_Colony_Info_Military_Stuff_For(military_index, start_x, drawn_count, 2);
        }

        Set_Colony_Font();

        if (step_squish == 0) {
            local_n = 0;
            if (military_count > 0) {
                do {
                    if (mode == 0) {
                        uint8_t race = MOX::player[owner].race;
                        s_animation_header* height_anim;

                        anim = Military_Anim((int16_t)military_index, (int16_t)race);
                        height_anim = Military_Anim((int16_t)military_index, (int16_t)race);
                        height = animate::Get_Height(height_anim);
                        animate::Draw((int16_t)(((int16_t)(local_n + start_x)) * (30 - step_squish)), (int16_t)(0x1df - height), anim);
                    }
                    local_n++;
                } while (local_n < military_count);
            }
        } else {
            step_squish = 0;
            local_n = 0;
            if (military_count > 0) {
                uint8_t race = MOX::player[owner].race;
                s_animation_header* height_anim;

                anim = Military_Anim((int16_t)military_index, (int16_t)race);
                height_anim = Military_Anim((int16_t)military_index, (int16_t)race);
                height = animate::Get_Height(height_anim);

                animate::Draw((int16_t)((int16_t)start_x * 30), (int16_t)(0x1df - height), anim);

                snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x7c /* "x %d" */), military_count);
                Set_Colony_Font_To(3);

                anim = Military_Anim((int16_t)military_index, (int16_t)race);
                height_anim = Military_Anim((int16_t)military_index, (int16_t)race);
                height = animate::Get_Height(height_anim);

                fonts::Print((int16_t)(((int16_t)start_x + 1) * (30 - step_squish)), (int16_t)(0x1df - height), buffer);
                local_n = 3;
            }
        }

        if (mode == 1) {
            if (local_n > 0) {
                int16_t step = 30 - step_squish;
                uint8_t race = MOX::player[owner].race;
                anim = Military_Anim((int16_t)military_index, (int16_t)race);
                height = animate::Get_Height(anim);

                int16_t x1 = (int16_t)start_x * step;
                int16_t y1 = (int16_t)(0x1df - height);
                int16_t x2 = (int16_t)(((int16_t)(local_n + start_x)) * step - 1);
                int16_t y2 = 0x1df;

                military_fields[military_index] = fields::Add_Hidden_Field(x1, y1, x2, y2, ESTRINGS::E_Strings(0xc), 0);
            } else {
                military_fields[military_index] = -1000;
            }
        } else if (mode == 2) {
            int other_index = (military_index + 1) % 2;
            int total_military = military_count + MOX::colony[colony_handle].military[other_index];

            if (total_military < 2) {
                total_military = 1;
            }

            int step = 260 / total_military;
            if (step < 1) {
                step = 1;
            }

            if (step < 30) {
                step_squish = static_cast<short>(30 - step);
            } else {
                step_squish = 0;
            }
        }

        return start_x + local_n;
    }

} // namespace COLONY
