#include "pch.h"

#include "build_queue.h"

namespace COLSUM {
    int16_t x_fields[14];
    s_animation_header* anims[21];
    uint8_t draw_planet_sum;

    int16_t first;
    int16_t slider_bar_position;

    int16_t list_col_prod_fields[10];
    int16_t list_col[10];
    s_colsum_col_pop_ref list_col_pop_ref[30];
    int16_t list_col_coord_vars[32];
    int16_t list_col_pop_count[32];
    int16_t list_buy_fields[10];
    int16_t list_fields[10];
    int16_t list_buy_data[10];
    int16_t list_col_job_fields[30];
    int16_t colony_summary_saved_screen;
    int16_t colony_build_queue_hotkeys[BUILD_QUEUE::PRESET_COUNT];
    int16_t empire_build_queue_hotkeys[BUILD_QUEUE::PRESET_COUNT];
    int16_t open_colony_hotkey;
    int16_t return_hotkey;
    int16_t stored_production_hotkey;

    uint8_t colsum_strings_been_loaded;

    int8_t event_good_lookup[36] = {
        1,
        1,
        0,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
    };

    char* wee_event_strings[36];

    const char s_0_0055110c[3] = "\0320";
    const char s_1_00551110[3] = "\0321";

    const char s__005510f0[2] = " ";
    const char s_s_005510f4[2] = "s";
    const char s_f_005510f8[2] = "f";
    const char s_p_005510fc[2] = "p";

    void __cdecl Check_Galaxy_Map_Fields(int16_t* clicked_field, int16_t* hovered_field) {
        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            int16_t best_planet = Pick_Best_Planet_To_Goto_At_Star(star_idx);

            if (*clicked_field == MOX::galaxy_map_star_field[star_idx] && best_planet != -1) {
                for (int16_t colony_list_idx = 0; colony_list_idx < MOX::NUM_COLONIES; ++colony_list_idx) {
                    int16_t col_idx = COLXPORT::g_colony_list_ptr[colony_list_idx];

                    if (col_idx != -1 && MOX::colony[col_idx].planet == best_planet) {
                        first = colony_list_idx;

                        while (COLXPORT::g_colony_list_ptr[first + 9] == -1) {
                            Decrement_First(10);
                        }

                        int16_t n_colonies = COLXPORT::N_Colonies(MOX::PLAYER_NUM);
                        slider_bar_position = n_colonies - first - 10;

                        Update_First(10, &first, &slider_bar_position);
                        Update_Col_List();
                        COLONY::full_draw = 1;
                    }
                }
            }

            if (*hovered_field == MOX::galaxy_map_star_field[star_idx]) {
                best_planet = Pick_Best_Planet_To_Goto_At_Star(star_idx);
                MOX::galaxy_map_scanned_star = star_idx;

                if (best_planet != -1) {
                    COLONY::g_colony_n = MOX::planet[best_planet].colony_index;
                }
            }
        }
    }

    void __cdecl Draw_Scan_Info() {
        Draw_Colony_Scan_Info();
        COLONY::Set_Colony_Font_To(1);

        if (MOX::galaxy_map_scanned_star != -1) {
            const char* star_name = reinterpret_cast<const char*>(&MOX::star[MOX::galaxy_map_scanned_star]);
            fonts::Print_Centered(444, 431, star_name);
        }
    }

    void __cdecl Colony_Summary_Screen() {
        MOX2::action_func_t saved_auto_fn = nullptr;
        int16_t input_val = 0;
        int16_t scan_val = 0;
        int16_t colony_list_buffer[MAX_COLONIES];

        COLSUM::draw_planet_sum = 0;
        COLXPORT::g_colony_list_ptr = colony_list_buffer;

        if (MOX::return_screen != MOX::current_screen &&
            COLBLDG::return_to_build_popup_from_custom_design == 0 &&
            MOX::return_screen != SCREEN_QUEUE_POPUP) {
            COLSUM::colony_summary_saved_screen = MOX::return_screen;
        }

        COLSUM::Load_Col_Sum_Anims();
        COLSUM::Load_Colsum_Strings();
        COLONY::Allocate_Mini_Planet_Cache();
        COLXPORT::Build_Global_Colony_List();
        COLSUM::Sort_Col_List();
        COLSUM::Update_Col_List();

        if (COLSUM::list_col[9] == -1) {
            COLSUM::Reset_List_Col_Array();
            COLSUM::Update_Col_List();
        }

        fonts::Set_Font_Style_Shadow_Down(1, &COLONY::font_color_array);
        MOX2::Get_Auto_Function(&saved_auto_fn);
        mouse::Set_Mouse_List(&COLGCBT::mouse_list_blank, 1);
        fields::Deactivate_Auto_Function();

        if (ERIC::Fast_Fade_Out_Needed_For_Colony_Palette()) {
            ERIC::Fast_Fade_Out();
            graphics::Fill(0, 0, 639, 479, 0);
        }

        fonts::Load_Palette(1, 0, 255);
        animate::Draw_Palette(COLSUM::anims[0]);
        remap::Update_Glass_Remap_Colors();

        if (ERIC::Fast_Fade_Out_Needed_For_Colony_Palette()) {
            palstore::Darken_Palette(100);
            palette::Clear_Palette_Changes(0, 255);
        }

        palette::Refresh_Palette();

        COLONY::g_colony_n = COLSUM::list_col[0];
        COLONY::using_colony_screen_palette = 1;
        MOX::galaxy_map_scanned_star = MOX::planet[MOX::colony[COLONY::g_colony_n].planet].star_index;
        COLONY::full_draw = 1;

        ERICHELP::Set_Colony_Summary_Screen_Help_List();
        fields::Assign_Auto_Function(COLSUM::Draw_Colony_Summary_Screen, 1);
        COLSUM::Draw_Colony_Summary_Screen();
        palette::Clear_Palette_Changes(0, 255);
        video::Set_Refresh_Stencil();
        fields::Draw_Visible_Fields();
        RUSS::Mox_Sync_Update();
        COLONY::full_draw = 0;
        ERIC::Fast_Fade_In();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        video::Set_Refresh_Stencil();
        COLSUM::Update_Col_List();

        while (true) {
            timer::Mark_Time();
            input_val = fields::Get_Input();
            scan_val = fields::Scan_Input();

            if (!COLSUM::Evaluate_Input(&input_val, &scan_val)) {
                FAKEBUTS::Clear_Fake_Buttons();
                COLONY::Clear_Galaxy_Map_Fields();
                fields::Clear_Fields();
                fields::Deactivate_Auto_Function();
                COLONY::using_colony_screen_palette = 0;
                return;
            }

            COLSUM::Draw_Colony_Summary_Screen();
            if (COLONY::full_draw != 0) {
                COLSUM::Update_Col_List();
                COLONY::full_draw = 0;
            }
            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);

            if (input_val == COLSUM::x_fields[3]) {
                break;
            }
        }

        FAKEBUTS::Clear_Fake_Buttons();
        COLONY::Clear_Galaxy_Map_Fields();
        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
        fields::Deactivate_Help_List();
        MOX2::Put_Auto_Function(&saved_auto_fn);
        COLONY::using_colony_screen_palette = 0;
    }

    void __cdecl Update_First(int16_t visible_rows, int16_t* first_visible_index, int16_t* slider_position) {
        if (COLXPORT::N_Colonies(MOX::PLAYER_NUM) < visible_rows) {
            *slider_position = -1;
            *first_visible_index = 0;
            return;
        }

        if ((int32_t)COLXPORT::N_Colonies(MOX::PLAYER_NUM) - *slider_position - *first_visible_index != visible_rows) {
            COLONY::full_draw = 1;
            *first_visible_index = COLXPORT::N_Colonies(MOX::PLAYER_NUM) - *slider_position - visible_rows;
            COLONY::scanned_pop = -1;
        }
    }

    void __cdecl Decrement_First(int16_t num_items) {
        int16_t colonies_count = COLXPORT::N_Colonies(MOX::PLAYER_NUM);

        if (colonies_count >= num_items) {
            int32_t first_val = static_cast<int32_t>(first) - 1;
            if (first_val < 1) {
                first_val = 0;
            }
            first = static_cast<int16_t>(first_val);

            int16_t updated_colonies_count = COLXPORT::N_Colonies(MOX::PLAYER_NUM);
            slider_bar_position = (updated_colonies_count - first) - num_items;
            COLONY::full_draw = 1;
        }
    }

    void __cdecl Increment_First(int16_t count) {
        int16_t num_colonies = COLXPORT::N_Colonies(MOX::PLAYER_NUM);

        if (num_colonies >= count) {
            first++;
            num_colonies = COLXPORT::N_Colonies(MOX::PLAYER_NUM);
            slider_bar_position = num_colonies - first - count;
            COLONY::full_draw = 1;
        }
    }

    void __cdecl Clear_List_Col_Array() {
        memset(COLSUM::list_col, 0xff, sizeof(COLSUM::list_col));
    }

    void __cdecl Reset_List_Col_Array() {
        first = 0;
        Clear_List_Col_Array();
        Update_Col_List();
    }

    void __cdecl Init_Fields() {
        fields::Clear_Fields();
        FAKEBUTS::Clear_Fake_Buttons();
        COLONY::Clear_Galaxy_Map_Fields();
        Add_Fields();
    }

    void __cdecl Add_Fields() {
        int16_t num_colonies = COLXPORT::N_Colonies(MOX::PLAYER_NUM);

        static const int16_t buy_btn_y_coords[] = {35, 65, 96, 128, 159, 190, 221, 251, 283, 314};

        colony_build_queue_hotkeys[0] = fields::Add_Hot_Key("q");
        empire_build_queue_hotkeys[0] = fields::Add_Hot_Key("Q!");
        static const char shifted_digits[] = "!@#$%^&*()";
        for (int index = 0; index < 10; ++index) {
            const char digit_hotkey[2] = {
                index == 9 ? '0' : static_cast<char>('1' + index),
                '\0',
            };
            const char hotkey[2] = {shifted_digits[index], '\0'};
            colony_build_queue_hotkeys[index + 1] =
                fields::Add_Hot_Key(digit_hotkey);
            empire_build_queue_hotkeys[index + 1] = fields::Add_Hot_Key(hotkey);
        }
        open_colony_hotkey = fields::Add_Hot_Key("c");
        return_hotkey = fields::Add_Hot_Key(" ");
        stored_production_hotkey = fields::Add_Hot_Key("t");

        x_fields[1] = fields::Add_Button_Field(619, 15, ESTRINGS::E_Strings(12), anims[2], ESTRINGS::E_Strings(137), 40);
        x_fields[2] = fields::Add_Button_Field(619, 316, ESTRINGS::E_Strings(12), anims[3], ESTRINGS::E_Strings(11), 40);
        x_fields[3] = fields::Add_Button_Field(531, 445, ESTRINGS::E_Strings(12), anims[1], ESTRINGS::E_Strings(40), 40);
        x_fields[4] = fields::Add_Hot_Key(ESTRINGS::E_Strings(135));
        x_fields[5] = fields::Add_Multi_Button_Field(89, 446, ESTRINGS::E_Strings(12), anims[4], &g_sort_index, 0, ESTRINGS::E_Strings(384), 41);
        x_fields[6] = fields::Add_Multi_Button_Field(140, 446, ESTRINGS::E_Strings(12), anims[5], &g_sort_index, 1, s_p_005510fc, 41);
        x_fields[7] = fields::Add_Multi_Button_Field(219, 446, ESTRINGS::E_Strings(12), anims[6], &g_sort_index, 2, s_f_005510f8, 41);
        x_fields[8] = fields::Add_Multi_Button_Field(262, 446, ESTRINGS::E_Strings(12), anims[7], &g_sort_index, 3, ESTRINGS::E_Strings(327), 41);
        x_fields[9] = fields::Add_Multi_Button_Field(326, 446, ESTRINGS::E_Strings(12), anims[8], &g_sort_index, 4, s_s_005510f4, 41);
        x_fields[10] = fields::Add_Multi_Button_Field(393, 446, ESTRINGS::E_Strings(12), anims[9], &g_sort_index, 5, ESTRINGS::E_Strings(436), 41);
        x_fields[11] = fields::Add_Multi_Button_Field(480, 446, ESTRINGS::E_Strings(12), anims[10], &g_sort_index, 6, ESTRINGS::E_Strings(193), 41);

        if (num_colonies < 10) {
            x_fields[12] = -1000;
        } else {
            x_fields[12] = fields::Add_Scroll_Field(621, 40, 0, num_colonies, 0, num_colonies - 10, 5, 271, &slider_bar_position, ESTRINGS::E_Strings(12), 0);
        }

        x_fields[13] = fields::Add_Hot_Key(ESTRINGS::E_Strings(596));

        int16_t y_row_end = 65;
        for (int16_t i = 0; i < 10; ++i) {
            if (list_col[i] == -1) {
                list_fields[i] = -1000;
                list_col_prod_fields[i] = -1000;
                list_buy_fields[i] = -1000;
            } else {
                int16_t y1 = y_row_end - 30;
                list_fields[i] = fields::Add_Hidden_Field(12, y1, 101, y_row_end, ESTRINGS::E_Strings(12), 0);
                list_col_prod_fields[i] = fields::Add_Hidden_Field(512, y1, 597, y_row_end, ESTRINGS::E_Strings(12), 0);

                int16_t col_idx = list_col[i];
                if (COLONY::Colony_Can_Buy_Product_0(col_idx) == 0 && MOX::colony[col_idx].bought_outright == 0) {
                    short bx1 = 599;
                    short by1 = buy_btn_y_coords[i];
                    short w = animate::Get_Width(anims[i + 11]);
                    short h = animate::Get_Height(anims[i + 11]);
                    list_buy_fields[i] = fields::Add_Hidden_Field(bx1, by1, bx1 + w, by1 + h, ESTRINGS::E_Strings(12), 0);
                } else {
                    list_buy_fields[i] = fields::Add_Locked_Button_Field(599, buy_btn_y_coords[i], ESTRINGS::E_Strings(12), anims[i + 11], &list_buy_data[i], ESTRINGS::E_Strings(12), 41);
                }
            }
            y_row_end += 31;
            Add_Fields_Pop_For(i);
        }

        x_fields[0] = fields::Add_Hidden_Field(0, 0, 639, 479, s__005510f0, 0);
    }

    void __cdecl Add_Fields_Pop_For(int16_t colony_idx) {
        int16_t col_id = list_col[colony_idx];
        for (int16_t i = 0; i < 3; ++i) {
            int32_t idx = colony_idx * 3 + i;
            if (col_id == -1) {
                list_col_job_fields[idx] = -1000;
            } else {
                int16_t val1, val2;
                if (i == 0) {
                    val2 = 101;
                    val1 = 236;
                } else if (i == 1) {
                    val2 = 236;
                    val1 = 378;
                } else {
                    val2 = 378;
                    val1 = 512;
                }

                COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(
                    col_id,
                    i,
                    1,
                    val2,
                    (int16_t)(colony_idx * 31 + 34),
                    (int16_t)(val1 - 10),
                    0,
                    list_col_pop_ref[idx].refs,
                    &list_col_pop_count[idx],
                    &list_col_coord_vars[idx],
                    &list_col_job_fields[idx]);
            }
        }
    }

    void __cdecl Update_Col_List() {
        for (int i = 0; i < 10; ++i) {
            int16_t colony_idx = COLXPORT::g_colony_list_ptr[COLSUM::first + i];
            COLSUM::list_col[i] = colony_idx;

            if (colony_idx == -1) {
                COLSUM::list_buy_data[i] = 0;
            } else {
                COLSUM::list_buy_data[i] = MOX::colony[colony_idx].bought_outright;
            }
        }

        COLSUM::Init_Fields();
    }

    void __cdecl Load_Col_Sum_Anims() {
        COLBLDG::Load_Build_Anims();

        for (int i = 0; i < 21; ++i) {
            anims[i] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLSUM_LBX, i, MOX::screen_seg);
        }

        COLONY::Load_Galaxy_Map_Anims();
    }

    void __cdecl Draw_Galaxy_Map() {
        MOVEBOX::Draw_Galaxy_Map_Box(nullptr, 0, 0x17c, 0x15d, 0x80, 0x5b, 0, 0, 0, 0, 3, 0);
    }

    void __cdecl Draw_Colony_Summary_Screen() {
        COLONY::g_count++;
        video::Set_Page_Off();
        graphics::Set_Window(0, 0, 639, 479);
        video::Clip_On();
        Update_First(10, &first, &slider_bar_position);

        if (COLONY::full_draw != 0) {
            fonts::Set_Font_Style_Shadow_Down(1, &COLONY::font_color_array);
            graphics::Fill(0, 0, 639, 479, 0);
            animate::Draw(0, 0, anims[0]);
            Clear_List_Col_Array();
            Update_Col_List();
            Draw_Empire_Info();
        }

        Draw_Galaxy_Map();
        for (int16_t i = 0; i < 10; ++i) {
            if (list_col[i] != -1) {
                Draw_Colony_Summary_For_Colony(i);
            }
        }

        if (COLONY::full_draw != 0) {
            if (fields::draw_fields_flag == 0) {
                fields::Enable_Draw_All_Fields();
                fields::Draw_Visible_Fields();
                fields::Disable_Draw_All_Fields();
            } else {
                fields::Draw_Visible_Fields();
            }
            video::Copy_Off_To_Back();
        }

        Draw_Scan_Info();

        if (COLMOVE::cluster_colony_n != -1 && COLONY::g_colony_n != -1) {
            s_colors colors = {};
            colors.colors[0] = 0xE8;
            colors.colors[1] = 0xE9;
            colors.colors[2] = 0xEA;
            colors.colors[3] = 0xEB;
            colors.colors[4] = 0xEA;
            colors.colors[5] = 0xE9;
            int16_t star1 = MOX::planet[MOX::colony[COLMOVE::cluster_colony_n].planet].star_index;
            int16_t star2 = MOX::planet[MOX::colony[COLONY::g_colony_n].planet].star_index;
            Colsum_Connect_Galaxy_Map_Stars(star1, star2, &colors);
        }

        if (draw_planet_sum == 1) {
            int16_t index = HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);
            COLONY::Draw_Planet_Summary(229, 135, index);
        }

        Draw_Bar_Indicator();
        FAKEBUTS::Draw_Fake_Buttons();
        fields::Draw_Visible_Fields();

        int16_t pointer_y = mouse::Pointer_Y();
        int16_t pointer_x = mouse::Pointer_X();
        COLMOVE::Draw_Cluster(pointer_x, pointer_y);

        video::Clip_Off();
    }

    void __cdecl Draw_Colony_Summary() {
        fields::Deactivate_Auto_Function();

        // Sets up the auto-redraw function for the colony summary screen
        fields::Assign_Auto_Function(Draw_Colony_Summary_Screen, 1);

        COLONY::full_draw = 1;
        video::Set_Page_Off();

        Draw_Colony_Summary_Screen();

        video::Copy_Off_To_Back();
        video::Set_Refresh_Stencil();

        // Updates context-sensitive help for the current screen
        ERICHELP::Set_Colony_Summary_Screen_Help_List();
    }

    bool __cdecl Event_Good(int16_t event_id) {
        return static_cast<bool>(event_good_lookup[event_id]);
    }

    void __cdecl Draw_Colony_Summary_For_Colony(int16_t list_idx) {
        char buffer[500];
        char sub_buffer1[500];
        char sub_buffer2[500];

        int16_t colony_idx = list_col[list_idx];
        s_colony* colony = &MOX::colony[colony_idx];
        int16_t planet_idx = colony->planet;

        fonts::Set_Outline_Color(0x50, 0x50);

        if (colony_idx == COLONY::g_colony_n) {
            Draw_Info_Pop_For(list_idx);
        }

        int16_t event_id = EVENTS::Colony_Has_Event(colony_idx);
        COLONY::Set_Colony_Font_To_Blue(2, colony_idx == COLONY::g_colony_n);

        uint16_t paragraph_type;
        if (event_id == -1) {
            s_planet_data* planet = &MOX::planet[planet_idx];
            int16_t star_idx = planet->star_index;
            s_star_data* star = &MOX::star[star_idx];

            if ((star->blockaded >> (MOX::PLAYER_NUM & 0x1F) & 1) == 0) {
                strlcpy(sub_buffer1, ESTRINGS::E_Strings(0x0C), sizeof(sub_buffer1));
                strlcpy(sub_buffer2, ESTRINGS::E_Strings(0x0C), sizeof(sub_buffer2));
            } else {
                /* Fmtpara: "\x1B2" switches the following text to an alternate inline color attribute. */
                snprintf(sub_buffer1, sizeof(sub_buffer1), ESTRINGS::E_Strings(0x46), "\x1B"
                                                                                       "2");
                strlcpy(sub_buffer2, ESTRINGS::E_Strings(0x86), sizeof(sub_buffer2));
            }

            snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x51), sub_buffer1, HACCESS::Get_Planet_Name_No_Prime(planet_idx), sub_buffer2);
            paragraph_type = 0x59;
        } else {
            /* Fmtpara: "\x1B1" and "\x1B2" switch the event label to specific inline color attributes. */
            const char* event_prefix = Event_Good(event_id) ? "\x1B"
                                                               "1"
                                                             : "\x1B"
                                                               "2";
            snprintf(sub_buffer1, sizeof(sub_buffer1), ESTRINGS::E_Strings(0x46), event_prefix);
            snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x55), HACCESS::Get_Planet_Name_No_Prime(planet_idx), sub_buffer1, wee_event_strings[event_id]);
            paragraph_type = 0x57;
        }

        int16_t y_pos = list_idx * 0x1F + 0x26;
        BILL::Squeeze_Formatted_Paragraph_Centered(0x0C, y_pos, paragraph_type, 0x17, buffer, 0);

        if (COLONY::full_draw != 0) {
            fonts::Set_Font_Style_Shadow_Down(0, &COLONY::font_color_array);
            Draw_Info_Pop_For(list_idx);

            int16_t turn_count = COLONY::Calculate_Current_Production_Turn_Count(colony_idx);
            COLONY::Set_Colony_Font_To(1);

            int16_t item_producing = colony->producing[0];
            const int autobuild_preset = BUILD_QUEUE::autobuild_settings.enabled != 0
                ? BUILD_QUEUE::Decode_Autobuild_Preset(colony->auto_building)
                : -1;
            const bool autobuild_active =
                BUILD_QUEUE::Is_Legacy_Autobuild(colony->auto_building) ||
                autobuild_preset >= 0;
            if (item_producing != COLONY_PRODUCTION_NONE) {
                /* Fmtpara: these inline "\x1B?" sequences apply specific color attributes to the auto-build text. */
                if (turn_count < 1 || turn_count > 9999) {
                    if (!autobuild_active) {
                        snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x46), COLBLDG::Selection_Name(item_producing));
                    } else {
                        snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x5D), "\x1B"
                                                                                     "1",
                            "\x1B"
                            "0",
                            COLBLDG::Selection_Name(item_producing));
                    }
                } else {
                    if (!autobuild_active) {
                        snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x27), COLBLDG::Selection_Name(item_producing), (int)turn_count);
                    } else {
                        snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x5C), "\x1B"
                                                                                     "1",
                            "\x1B"
                            "0",
                            COLBLDG::Selection_Name(item_producing),
                            (int)turn_count);
                    }
                }

                if (autobuild_preset >= 0) {
                    strlcat(buffer, " [Q", sizeof(buffer));
                    char preset_text[8];
                    snprintf(preset_text, sizeof(preset_text), "%d]", autobuild_preset);
                    strlcat(buffer, preset_text, sizeof(buffer));
                }
                COLONY::Set_Colony_Font_To_Blue(2, 0);
                BILL::Squeeze_Print_Formatted_Paragraph(0x200, y_pos, 0x55, 0x16, buffer, 0);
                fonts::Set_Font_Style_Shadow_Down(1, &COLONY::font_color_array);
                return;
            }

            if (autobuild_active) {
                if (autobuild_preset >= 0) {
                    snprintf(buffer, sizeof(buffer), "Auto Build Queue %d", autobuild_preset);
                } else {
                    snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x5D), "\x1B"
                                                                                 "1",
                        "\x1B"
                        "0",
                        COLBLDG::Selection_Name(COLONY_PRODUCTION_NONE));
                }
                BILL::Squeeze_Print_Formatted_Paragraph(0x200, y_pos, 0x55, 0x16, buffer, 0);
                fonts::Set_Font_Style_Shadow_Down(1, &COLONY::font_color_array);
            }
        }
    }

    void __cdecl Load_Colsum_Strings() {
        if (colsum_strings_been_loaded != 0) {
            return;
        }

        colsum_strings_been_loaded = 1;

        wee_event_strings[0] = ESTRINGS::E_Strings(0x104);
        wee_event_strings[1] = ESTRINGS::E_Strings(0xdd);
        wee_event_strings[2] = ESTRINGS::E_Strings(0xf0);
        wee_event_strings[3] = ESTRINGS::E_Strings(0x227);
        wee_event_strings[4] = ESTRINGS::E_Strings(0xcc);
        wee_event_strings[5] = ESTRINGS::E_Strings(0x171);
        wee_event_strings[6] = ESTRINGS::E_Strings(0x10e);
        wee_event_strings[7] = ESTRINGS::E_Strings(0x117);
        wee_event_strings[8] = ESTRINGS::E_Strings(0xa7);
        wee_event_strings[9] = ESTRINGS::E_Strings(0x127);
        wee_event_strings[10] = ESTRINGS::E_Strings(0xa7);
        wee_event_strings[11] = ESTRINGS::E_Strings(0x102);
        wee_event_strings[12] = ESTRINGS::E_Strings(0x103);
        wee_event_strings[13] = ESTRINGS::E_Strings(0x17d);
        wee_event_strings[14] = ESTRINGS::E_Strings(0x19e);
        wee_event_strings[15] = ESTRINGS::E_Strings(0x19d);
        wee_event_strings[16] = ESTRINGS::E_Strings(0x19f);
        wee_event_strings[17] = ESTRINGS::E_Strings(0xce);
        wee_event_strings[18] = ESTRINGS::E_Strings(0x1d2);
        wee_event_strings[19] = ESTRINGS::E_Strings(0xad);
        wee_event_strings[20] = ESTRINGS::E_Strings(0xf6);
        wee_event_strings[21] = ESTRINGS::E_Strings(0x110);
        wee_event_strings[22] = ESTRINGS::E_Strings(0x119);
        wee_event_strings[23] = ESTRINGS::E_Strings(0x144);
        wee_event_strings[24] = ESTRINGS::E_Strings(0x18e);
        wee_event_strings[25] = ESTRINGS::E_Strings(0xb6);
        wee_event_strings[26] = ESTRINGS::E_Strings(0xc6);
        wee_event_strings[27] = ESTRINGS::E_Strings(0x12e);
        wee_event_strings[28] = ESTRINGS::E_Strings(0x22e);
        wee_event_strings[29] = ESTRINGS::E_Strings(0x1b6);
        wee_event_strings[30] = ESTRINGS::E_Strings(0x1a1);
        wee_event_strings[31] = ESTRINGS::E_Strings(0x135);
        wee_event_strings[32] = ESTRINGS::E_Strings(0x193);
        wee_event_strings[33] = ESTRINGS::E_Strings(0xff);
        wee_event_strings[34] = ESTRINGS::E_Strings(0x1f4);
        wee_event_strings[35] = ESTRINGS::E_Strings(0x186);
    }

    void __cdecl Draw_Info_Pop_For(int16_t colony_idx) {
        const int16_t list_col_value = COLSUM::list_col[colony_idx];
        const int32_t colony_pop_base = static_cast<int32_t>(colony_idx) * 3;
        const int16_t row_coord = static_cast<int16_t>(static_cast<int32_t>(colony_idx) * 0x1f + 0x26);

        for (int16_t pop_slot = 0; pop_slot < 3; ++pop_slot) {
            int16_t coord_c;
            if (pop_slot == -1) {
                coord_c = 0x0065;
            } else if (pop_slot == 0) {
                coord_c = 0x00ec;
            } else if (static_cast<uint16_t>(pop_slot - 1) < 1u) {
                coord_c = 0x017a;
            } else {
                coord_c = 0x0200;
            }

            int16_t coord_a;
            if (pop_slot == 0) {
                coord_a = 0x0065;
            } else if (pop_slot == 1) {
                coord_a = 0x00ec;
            } else if (static_cast<uint16_t>(pop_slot - 2) < 1u) {
                coord_a = 0x017a;
            } else {
                coord_a = 0x0200;
            }

            coord_c = static_cast<int16_t>(coord_c - 10);

            const int32_t pop_ref_index = colony_pop_base + pop_slot;

            COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(
                list_col_value,
                pop_slot,
                0,
                coord_a,
                row_coord,
                coord_c,
                0,
                COLSUM::list_col_pop_ref[pop_ref_index].refs,
                COLSUM::list_col_pop_count + pop_ref_index,
                COLSUM::list_col_coord_vars + pop_ref_index,
                COLSUM::list_col_job_fields + pop_ref_index);
        }
    }

    void __cdecl Colsum_Connect_Galaxy_Map_Stars(int16_t star_idx1, int16_t star_idx2, s_colors* pattern) {
        int16_t x1, y1, x2, y2;

        MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx1, 380, 349, 128, 91, 0, 0, 0, &x1, &y1);
        MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx2, 380, 349, 128, 91, 0, 0, 0, &x2, &y2);

        SHIPS::Draw_Directional_Multi_Colored_Line(
            x1 + 2,
            y1 + 2,
            x2 + 2,
            y2 + 2,
            pattern,
            COLONY::g_count % 6);
    }

    void __cdecl Draw_Bar_Indicator() {
        int16_t num_colonies = COLXPORT::N_Colonies(MOX::PLAYER_NUM);
        COLSUM::Update_First(10, &COLSUM::first, &COLSUM::slider_bar_position);

        if (num_colonies >= 10) {
            int32_t y1 = (271 * (int32_t)COLSUM::first) / (int32_t)num_colonies + 40;
            int32_t y2 = (271 * ((int32_t)COLSUM::first + 10)) / (int32_t)num_colonies + 40;

            graphics::Set_Window(0, 0, 639, 479);
            video::Clip_On();

            // Draw the indicator bar body
            graphics::Fill(621, (int16_t)y1, 626, (int16_t)y2, 229);

            // Draw the indicator bar borders (shaded edges)
            line::Line(621, (int16_t)y1, 626, (int16_t)y1, 230);
            line::Line(621, (int16_t)y1, 621, (int16_t)y2, 230);
            line::Line(626, (int16_t)y1, 626, (int16_t)y2, 228);
            line::Line(621, (int16_t)y2, 626, (int16_t)y2, 228);

            // Draw corner markers for the scroll track area
            graphics::Dot(621, 40, 80);
            graphics::Dot(626, 40, 80);
            graphics::Dot(621, 311, 80);
            graphics::Dot(626, 311, 80);
        }

        video::Clip_Off();
    }

    bool __cdecl Evaluate_Input(int16_t* field_ptr, int16_t* scan_val) {
        int16_t field = *field_ptr;

        for (int preset = 0; preset < BUILD_QUEUE::PRESET_COUNT; ++preset) {
            if (field == colony_build_queue_hotkeys[preset] &&
                COLONY::g_colony_n != -1) {
                BUILD_QUEUE::Apply_Preset_To_Colony(COLONY::g_colony_n, preset);
                Update_Col_List();
                COLONY::full_draw = 1;
                *field_ptr = 0;
                return true;
            }
            if (field == empire_build_queue_hotkeys[preset]) {
                BUILD_QUEUE::Apply_Preset_To_Empire(MOX::PLAYER_NUM, preset);
                Update_Col_List();
                COLONY::full_draw = 1;
                *field_ptr = 0;
                return true;
            }
        }

        if (field == x_fields[1]) {
            Decrement_First(10);
            return true;
        }

        if (field == x_fields[2]) {
            if (COLXPORT::g_colony_list_ptr[first + 10] != -1) {
                Increment_First(10);
                return true;
            }
        }

        if (field == x_fields[3] || field == x_fields[4] ||
            field == return_hotkey) {
            if (COLMOVE::cluster_colony_n != -1) {
                COLMOVE::Clear_Cluster();
                COLONY::Reset_Mouse_Picture();
            }

            if (MOX::return_screen == MOX::current_screen && COLBLDG::return_to_build_popup_from_custom_design != 0) {
                MOX::current_screen = SCREEN_QUEUE_POPUP;
            } else {
                MOX::current_screen = colony_summary_saved_screen;
                MOX::return_screen = colony_summary_saved_screen;
            }

            if (*field_ptr == x_fields[4] || *field_ptr == return_hotkey) {
                *field_ptr = x_fields[3];
            }
            return true;
        }

        if (field == open_colony_hotkey && COLONY::g_colony_n != -1) {
            const int16_t planet_idx = MOX::colony[COLONY::g_colony_n].planet;
            MOX::screen_data = MOX::planet[planet_idx].star_index;
            COLONY::orbit_temp = static_cast<int16_t>(MOX::planet[planet_idx].orbit);
            MOX::return_screen = MOX::current_screen;
            MOX::current_screen = SCREEN_COLONY;
            *field_ptr = x_fields[3];
            return true;
        }

        if (field == x_fields[13]) {
            COLONY::full_draw = 1;
            return true;
        }

        // The stored-production sort has a hotkey but no visible header.
        if (field == x_fields[5] || field == x_fields[6] || field == x_fields[7] ||
            field == x_fields[8] || field == x_fields[9] || field == x_fields[10] ||
            field == x_fields[11] || field == stored_production_hotkey) {
            if (field == stored_production_hotkey) {
                g_sort_index = 7;
            }
            Sort_Col_List();
            Clear_List_Col_Array();
            first = 0;
            Decrement_First(10);
            Update_First(10, &first, &slider_bar_position);
            Update_Col_List();
            COLONY::full_draw = 1;
            return true;
        }

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            Draw_Colony_Summary();
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            return true;
        }

        bool result = Evaluate_Col_List_Input(field_ptr, scan_val);
        Check_Galaxy_Map_Fields(field_ptr, scan_val);

        return result;
    }

    void __cdecl Evaluate_Colony_Pop_Input(int16_t* input_field_1, int16_t* input_field_2, int16_t colony_idx_in_list) {
        if (colony_idx_in_list == 0) {
            COLONY::scanned_pop = -1;
        }

        for (int16_t job_idx = 0; job_idx < 3; ++job_idx) {
            int field_idx = colony_idx_in_list * 3 + job_idx;
            int16_t job_field_id = list_col_job_fields[field_idx];

            if (*input_field_1 == job_field_id) {
                if (COLMOVE::cluster_colony_n == -1) {
                    int16_t selected_pop = Get_Selected_Pop(colony_idx_in_list, job_idx);
                    if (selected_pop != -1) {
                        COLMOVE::Get_Cluster(list_col[colony_idx_in_list], selected_pop);
                    }
                    COLONY::full_draw = 1;
                } else {
                    COLMOVE::Send_Cluster(list_col[colony_idx_in_list], job_idx);
                    Draw_Colony_Summary();
                }
            }

            if (*input_field_2 == job_field_id) {
                COLONY::scanned_pop = Get_Scanned_Pop(colony_idx_in_list, job_idx);
            }
        }

        int16_t active_field_2 = *input_field_2;
        if (list_fields[colony_idx_in_list] == active_field_2 ||
            list_col_prod_fields[colony_idx_in_list] == active_field_2 ||
            list_buy_fields[colony_idx_in_list] == active_field_2) {
            int16_t colony_id = list_col[colony_idx_in_list];
            COLONY::g_colony_n = colony_id;

            // Accessing planet index from colony structure (size 361) and star index from planet structure (size 17)
            int16_t planet_idx = MOX::colony[colony_id].planet;
            MOX::galaxy_map_scanned_star = MOX::planet[planet_idx].star_index;
        }
    }

    bool __cdecl Evaluate_Col_List_Input(int16_t* p_input_field, int16_t* p_input_field_2) {
        if (COLSUM::draw_planet_sum == 1) {
            if (*p_input_field == 0) {
                return true;
            }
            COLSUM::draw_planet_sum = 0;
            if (*p_input_field > 0) {
                *p_input_field = 0;
            }
        }

        for (int i = 0; i < 10; ++i) {
            int16_t current_input = *p_input_field;

            if (COLSUM::list_fields[i] == current_input) {
                if (COLMOVE::cluster_colony_n != -1) {
                    COLMOVE::Send_Cluster(COLSUM::list_col[i], -1);
                    COLSUM::Draw_Colony_Summary();
                } else {
                    int16_t colony_idx = COLSUM::list_col[i];
                    int16_t planet_idx = MOX::colony[colony_idx].planet;

                    MOX::screen_data = MOX::planet[planet_idx].star_index;
                    COLONY::orbit_temp = (int16_t)MOX::planet[planet_idx].orbit;

                    MOX::return_screen = MOX::current_screen;
                    MOX::current_screen = SCREEN_COLONY;
                    *p_input_field = x_fields[3];
                }
            } else if (list_col_prod_fields[i] == current_input) {
                fields::Deactivate_Auto_Function();
                FAKEBUTS::Clear_Fake_Buttons();
                COLONY::Clear_Galaxy_Map_Fields();
                fields::Clear_Fields();

                int16_t colony_idx = COLSUM::list_col[i];
                int16_t planet_idx = MOX::colony[colony_idx].planet;

                MOX::screen_data = MOX::planet[planet_idx].star_index;
                COLONY::orbit_temp = (int16_t)MOX::planet[planet_idx].orbit;

                int16_t target_planet = HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);
                MOX::planet[target_planet].colony_index = colony_idx;

                if (COLMOVE::cluster_colony_n != -1) {
                    COLMOVE::Clear_Cluster();
                    COLONY::Reset_Mouse_Picture();
                }

                MOX::return_screen = MOX::current_screen;
                MOX::current_screen = SCREEN_QUEUE_POPUP;
                return false;
            } else if (COLSUM::list_fields[i] + current_input == 0) {
                int16_t colony_idx = COLSUM::list_col[i];
                int16_t planet_idx = MOX::colony[colony_idx].planet;

                MOX::screen_data = MOX::planet[planet_idx].star_index;
                COLONY::orbit_temp = (int16_t)MOX::planet[planet_idx].orbit;
                COLSUM::draw_planet_sum = 1;
            } else if (list_buy_fields[i] == current_input) {
                COLONY::Tested_Colony_Buys_Outright(COLSUM::list_col[i]);
                COLSUM::Draw_Colony_Summary();
            }

            COLSUM::Evaluate_Colony_Pop_Input(p_input_field, p_input_field_2, i);
        }

        return true;
    }

    int16_t __cdecl Get_Scanned_Pop(int16_t colony_list_row, int16_t job_type) {
        int16_t colony_idx = list_col[colony_list_row];
        int16_t val_ebx;
        int16_t val_ebp;

        if (job_type == -1) {
            val_ebx = 101 /* 0x65 */;
        } else if (job_type == 0) {
            val_ebx = 236 /* 0xec */;
        } else {
            val_ebx = (job_type == 1) ? 378 /* 0x17a */ : 512 /* 0x200 */;
        }

        if (job_type == 0) {
            val_ebp = 101 /* 0x65 */;
        } else if (job_type == 1) {
            val_ebp = 236 /* 0xec */;
        } else {
            val_ebp = (job_type == 2) ? 378 /* 0x17a */ : 512 /* 0x200 */;
        }

        int32_t list_idx = (int32_t)colony_list_row * 3 + (int32_t)job_type;

        int16_t result = COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(
            colony_idx,
            job_type,
            4,
            val_ebp,
            (int16_t)(colony_list_row * 31 + 38),
            (int16_t)(val_ebx - 10),
            0,
            list_col_pop_ref[list_idx].refs,
            &list_col_pop_count[list_idx],
            &list_col_coord_vars[list_idx],
            &list_col_job_fields[list_idx]);

        COLONY::g_colony_n = colony_idx;
        MOX::galaxy_map_scanned_star = MOX::planet[MOX::colony[colony_idx].planet].star_index;

        return result;
    }

    int16_t __cdecl Get_Selected_Pop(int16_t colony_idx, int16_t job_idx) {
        int32_t ebx_val;
        if (job_idx == -1) {
            ebx_val = 101;
        } else if (job_idx == 0) {
            ebx_val = 236;
        } else {
            ebx_val = (job_idx == 1) ? 378 : 512;
        }

        int32_t ebp_val;
        if (job_idx == 0) {
            ebp_val = 101;
        } else if (job_idx == 1) {
            ebp_val = 236;
        } else {
            ebp_val = (job_idx == 2) ? 378 : 512;
        }

        int32_t index = (int32_t)colony_idx * 3 + (int32_t)job_idx;

        return COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(
            list_col[colony_idx],
            job_idx,
            3,
            (int16_t)ebp_val,
            (int16_t)(colony_idx * 31 + 38),
            (int16_t)(ebx_val - 10),
            0,
            list_col_pop_ref[index].refs,
            &list_col_pop_count[index],
            &list_col_coord_vars[index],
            &list_col_job_fields[index]);
    }

    int16_t __cdecl Pick_Best_Planet_To_Goto_At_Star(int16_t star_idx) {
        int16_t planet_idx = MOX::star[star_idx].last_planet_selected[MOX::PLAYER_NUM];

        bool needs_search = false;
        if (planet_idx == -1) {
            needs_search = true;
        } else {
            int16_t colony_idx = MOX::planet[planet_idx].colony_index;
            if (colony_idx == -1) {
                needs_search = true;
            } else {
                if (MOX::colony[colony_idx].owner != 0) {
                    needs_search = true;
                }
            }
        }

        if (needs_search) {
            for (int16_t i = 0; i < 5; ++i) {
                int16_t p_idx = MOX::star[star_idx].planet_index[i];
                if (p_idx != -1) {
                    int16_t c_idx = MOX::planet[p_idx].colony_index;
                    if (c_idx != -1) {
                        if (MOX::colony[c_idx].owner == MOX::PLAYER_NUM) {
                            planet_idx = p_idx;
                        }
                    }
                }
            }
        }

        return planet_idx;
    }

} // namespace COLSUM