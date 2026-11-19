#include "pch.h"

namespace COLREFIT {

    int16_t first_ship;
    int16_t slider_bar_position;
    int16_t fields[12];
    int16_t esc;
    int16_t scanned_ship_index = -1;
    s_animation_header* anims[4];
    int16_t scanned_ship_d_index = -1;
    int16_t lscanned_field = -1000;
    int16_t* g_ship_field_list_ptr;
    MOX2::action_func_t saved_autofunction;
    int16_t* g_ship_list_ptr;
    int16_t n_ships_in_list;
    int16_t g_ship_n;
    s_scroll_state* g_scroll_ptr;

    void __cdecl Update_First_Ship(int16_t* first_ship_ptr, int16_t* slider_pos_ptr) {
        int16_t rounded_ship_count = ((n_ships_in_list + 5) / 5) * 5;

        if (rounded_ship_count < 15) {
            *slider_pos_ptr = -1;
            *first_ship_ptr = 0;
            return;
        }

        if ((rounded_ship_count - *slider_pos_ptr) - *first_ship_ptr != 15) {
            COLONY::full_draw = 1;
            rounded_ship_count = (rounded_ship_count - *slider_pos_ptr) - 15;
            *first_ship_ptr = rounded_ship_count;
            *first_ship_ptr = (rounded_ship_count / 5) * 5;
            slider_bar_position = (((n_ships_in_list + 5) / 5) - 3) * 5 - first_ship;
        }
    }

    void __cdecl Build_Ship_List() {
        int16_t n_ships = 0;

        for (int16_t ship_idx = 0; ship_idx < MAX_SHIPS; ++ship_idx) {
            if (MOX::ship[ship_idx].owner == MOX::PLAYER_NUM &&
                MOX::ship[ship_idx].status < 3 &&
                MOX::ship[ship_idx].Legacy_Location(MOX::star) == MOX::screen_data &&
                MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_COMBAT &&
                n_ships < MAX_SHIPS) {
                g_ship_list_ptr[n_ships] = ship_idx;
                n_ships++;
            }
        }

        n_ships_in_list = n_ships;

        while (n_ships < MAX_SHIPS) {
            g_ship_list_ptr[n_ships] = -1;
            n_ships++;
        }
    }

    int16_t __cdecl Colony_Refit_Popup() {
        int16_t hovered_field_id;
        int16_t selected_field_id;
        s_scroll_state scroll_data = {0};
        int16_t ship_field_list[MAX_SHIPS];
        int16_t ship_list[MAX_SHIPS];
        int16_t popup_result = 0;

        selected_field_id = 0;

        g_scroll_ptr = &scroll_data;
        g_ship_list_ptr = ship_list;
        g_ship_field_list_ptr = ship_field_list;

        Build_Ship_List();

        first_ship = 0;
        slider_bar_position = (((n_ships_in_list + 5) / 5) - 3) * 5;

        allocate::Push_Block(MOX::screen_seg);
        COLONY::Set_Colony_Font();

        esc = 0;

        Load_Refit_Ship_Anims();

        MOX2::Get_Auto_Function(&saved_autofunction);
        ::fields::Deactivate_Auto_Function();
        ::fields::Assign_Auto_Function(Draw_Refit_Popup, 1);

        ERICHELP::Set_Refit_Ship_Popup_Help_List();
        Add_Fields();

        graphics::Set_Window(0, 0, 0x27f, 0x1df);
        video::Clip_On();
        Draw_Refit_Popup();
        video::Set_Refresh_Stencil();

        COLONY::full_draw = 1;

        do {
            selected_field_id = ::fields::Get_Input();
            hovered_field_id = ::fields::Scan_Input();

            Draw_Refit_Popup();

            popup_result = Evaluate_Input(&::game::world, &selected_field_id, &hovered_field_id);

            RUSS::Mox_Sync_Update();
        } while (esc == 0);

        ::fields::Clear_Fields();
        ::fields::Deactivate_Help_List();
        ::fields::Deactivate_Auto_Function();

        MOX2::Put_Auto_Function(&saved_autofunction);

        video::Copy_Back_To_Off();

        allocate::Pop_Block(MOX::screen_seg);
        video::Clip_Off();

        return popup_result;
    }

    int16_t __cdecl Colony_Refit2_Popup(int16_t ship_idx) {
        int16_t match_count = 0;
        int16_t selected_field_id = 0;
        int16_t hovered_field_id;
        s_scroll_state scroll_data = {0};
        int16_t ship_field_list[MAX_SHIPS];
        int16_t ship_list[MAX_SHIPS];

        g_ship_n = ship_idx;
        g_scroll_ptr = &scroll_data;
        g_ship_list_ptr = ship_list;
        g_ship_field_list_ptr = ship_field_list;

        for (int16_t design_idx = 0; design_idx < SHIP_SIZE_COUNT; ++design_idx) {
            if (MOX::player[MOX::PLAYER_NUM].ship_designs[design_idx].size == MOX::ship[ship_idx].d.size) {
                match_count++;
            }
        }

        if (match_count == 0) {
            return -2;
        }

        allocate::Push_Block(MOX::screen_seg);
        COLONY::Set_Colony_Font();
        esc = 0;

        Load_Refit2_Ship_Anims();

        MOX2::Get_Auto_Function(&saved_autofunction);
        ::fields::Deactivate_Auto_Function();
        ::fields::Assign_Auto_Function(Draw_Refit2_Popup, 1);
        Do_Add2_Fields();

        graphics::Set_Window(0, 0, 0x27f, 0x1df);
        video::Clip_On();
        Draw_Refit2_Popup();
        video::Set_Refresh_Stencil();
        COLONY::full_draw = 1;

        int16_t popup_result = 0;
        do {
            selected_field_id = ::fields::Get_Input();
            hovered_field_id = ::fields::Scan_Input();
            Draw_Refit2_Popup();
            popup_result = Evaluate2_Input(&selected_field_id, &hovered_field_id);
            RUSS::Mox_Sync_Update();
        } while (esc == 0);

        ::fields::Clear_Fields();
        ::fields::Deactivate_Help_List();
        ::fields::Deactivate_Auto_Function();
        MOX2::Put_Auto_Function(&saved_autofunction);

        video::Copy_Back_To_Off();
        allocate::Pop_Block(MOX::screen_seg);
        video::Clip_Off();

        return popup_result;
    }

    int16_t __cdecl Evaluate_Input(::game::s_world_state* world_state, int16_t* selected_field_id, int16_t* hovered_field_id) {
        char text[500];

        COLREFIT::scanned_ship_index = -1;

        for (int16_t i = 0; i < 15; i++) {
            int16_t ship_idx = COLREFIT::g_ship_list_ptr[COLREFIT::first_ship + i];

            if (COLREFIT::g_ship_field_list_ptr[i] == *selected_field_id && ship_idx != -1) {
                if (MOX::ship[ship_idx].d.picture_num == 44) {
                    GENDRAW::Help(ESTRINGS::E_Strings(0x23F));
                    COLONY::full_draw = 1;
                } else {
                    auto planet_idx = HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);
                    if (COLCALC::Colony_Can_Build_Product(world_state, &MOX::colony[::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index], COLBLDG::Colony_Production_From_Ship_Index(ship_idx))) {
                        if (MOX::ship[ship_idx].d.previous_owner == MOX::PLAYER_NUM) {
                            if (MOX::ship[ship_idx].officer_index == -1) {
                                COLREFIT::esc = 1;
                                return ship_idx;
                            } else {
                                char* leader_name = OFFICER::Leader_Name(&game::world, MOX::ship[ship_idx].officer_index, 0);
                                char* format_str = ESTRINGS::E_Strings(0x212);
                                snprintf(text, sizeof(text), format_str, leader_name);

                                if (GENDRAW::Do_Confirmation_Box(text) != 0) {
                                    OFFICER::Move_Officer_To_Pool(&game::world, MOX::ship[ship_idx].officer_index);
                                    MOX::ship[ship_idx].officer_index = -1;
                                    COLREFIT::esc = 1;
                                    return ship_idx;
                                } else {
                                    COLREFIT::esc = 1;
                                    return ship_idx;
                                }
                            }
                        } else {
                            GENDRAW::Help(ESTRINGS::E_Strings(0x23D));
                            COLONY::full_draw = 1;
                        }
                    } else {
                        GENDRAW::Help(ESTRINGS::E_Strings(0x23E));
                        COLONY::full_draw = 1;
                    }
                }
            }
        }

        for (int16_t i = 0; i < 15; i++) {
            int16_t ship_idx = COLREFIT::g_ship_list_ptr[COLREFIT::first_ship + i];
            if (COLREFIT::g_ship_field_list_ptr[i] == *hovered_field_id && ship_idx != -1) {
                ERIC::Pulsing_Box((i % 5) * 60 + 161, (i / 5) * 61 + 49, 55, 56, 1, 1, 2);
                COLREFIT::scanned_ship_index = COLREFIT::first_ship + i;
            }
        }

        int32_t next_page_ship_index = COLREFIT::first_ship + 15;
        if (*selected_field_id == fields[1] && next_page_ship_index < MAX_SHIPS && COLREFIT::g_ship_list_ptr[next_page_ship_index] != -1) {
            int32_t next_first = COLREFIT::first_ship + 5;
            if (next_first >= MAX_SHIPS) {
                next_first = MAX_SHIPS;
            }
            COLREFIT::first_ship = (int16_t)next_first;
            COLONY::full_draw = 1;
            COLREFIT::slider_bar_position = (((COLREFIT::n_ships_in_list + 5) / 5) - 3) * 5 - COLREFIT::first_ship;
            COLREFIT::Update_First_Ship(&COLREFIT::first_ship, &COLREFIT::slider_bar_position);
        }

        if (*selected_field_id == fields[2]) {
            int32_t next_first = COLREFIT::first_ship - 5;
            if (next_first <= 0) {
                next_first = 0;
            }
            COLREFIT::first_ship = (int16_t)next_first;
            COLREFIT::slider_bar_position = (((COLREFIT::n_ships_in_list + 5) / 5) - 3) * 5 - COLREFIT::first_ship;
            COLREFIT::Update_First_Ship(&COLREFIT::first_ship, &COLREFIT::slider_bar_position);
            COLONY::full_draw = 1;
        }

        if (*selected_field_id == fields[3]) {
            COLREFIT::esc = 1;
        }

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            COLONY::full_draw = 1;
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        }

        return -1;
    }

    void __cdecl Draw_Refit2_Popup() {
        if (COLONY::full_draw == 0) {
            Draw_Ship_D_List();
        } else {
            COLONY::Set_Colony_Font();
            video::Set_Page_Off();
            MOX2::Execute_Auto_Function(&saved_autofunction);
            COLONY::full_draw = 1;
            animate::Draw(0x93, 6, anims[0]);
            Draw_Ship_D_List();
            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;
        }

        if (scanned_ship_d_index != -1) {
            Show_Ship_Design(&MOX::player[MOX::PLAYER_NUM].ship_designs[scanned_ship_d_index]);
        }

        if (lscanned_field == fields[11]) {
            COLONY::Set_Colony_Font_To_Reg_Blue(4, 1);
            char* str = ESTRINGS::E_Strings(0x322);
            BILL::Squeeze_Formatted_Paragraph_Centered(0xA8, 0xF8, 0x131, 0xA9, str, 2);
        }
    }

    void __cdecl Draw_Refit_Popup() {
        COLONY::full_draw = 1;
        if (COLONY::full_draw == 0) {
            Draw_Bar_Indicator();
            if (scanned_ship_index != -1) {
                Scan_Ship_Info(g_ship_list_ptr[scanned_ship_index]);
            }
        } else {
            COLONY::Set_Colony_Font();
            video::Set_Page_Off();
            MOX2::Execute_Auto_Function(&saved_autofunction);
            COLONY::full_draw = 1;
            animate::Draw(0x93, 6, anims[0]);
            Draw_Bar_Indicator();
            Draw_Ship_List();
            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;

            if (scanned_ship_index != -1) {
                Scan_Ship_Info(g_ship_list_ptr[scanned_ship_index]);
            }
        }
    }

    void __cdecl Load_Refit_Ship_Anims() {
        anims[0] = (s_animation_header*)farload::Far_Reload_Next("refitpup.LBX", 0, MOX::screen_seg);
        anims[2] = (s_animation_header*)farload::Far_Reload_Next("refitpup.LBX", 1, MOX::screen_seg);
        anims[1] = (s_animation_header*)farload::Far_Reload_Next("refitpup.LBX", 2, MOX::screen_seg);
        anims[3] = (s_animation_header*)farload::Far_Reload_Next("refitpup.LBX", 3, MOX::screen_seg);
    }

    void __cdecl Load_Refit2_Ship_Anims() {
        anims[0] = (s_animation_header*)farload::Far_Reload_Next("refitpup.LBX", 4, MOX::screen_seg);
        anims[2] = (s_animation_header*)farload::Far_Reload_Next("refitpup.LBX", 1, MOX::screen_seg);
        anims[1] = (s_animation_header*)farload::Far_Reload_Next("refitpup.LBX", 2, MOX::screen_seg);
        anims[3] = (s_animation_header*)farload::Far_Reload_Next("refitpup.LBX", 3, MOX::screen_seg);
    }

    void __cdecl Show_Ship_Design(s_ship_design* design) {
        char buffer[10000];
        STRBUILD::s_builder builder = STRBUILD::Init(buffer, sizeof(buffer));

        COLONY::Set_Colony_Font_To(1);
        COLBLDG::Build_Ship_Design_Info_Formatted_Paragraph(design, &builder);
        BILL::Squeeze_Print_Formatted_Paragraph(168, 248, 305, 169, buffer, 0);
    }

    void __cdecl Do_Add2_Fields() {
        ::fields::Clear_Fields();
        Add2_Fields();
    }

    void __cdecl Add_Fields() {
        ::fields::Clear_Fields();
        Add_Ship_List_Fields();

        fields[1] = ::fields::Add_Button_Field(
            0x1D5, 0x197, ESTRINGS::E_Strings(0xC), anims[1], ESTRINGS::E_Strings(0x89), '(');

        fields[2] = ::fields::Add_Button_Field(
            0x1D4, 0x2E, ESTRINGS::E_Strings(0xC), anims[2], ESTRINGS::E_Strings(0xB), '(');

        fields[3] = ::fields::Add_Button_Field(
            0x112, 0x1B8, ESTRINGS::E_Strings(0xC), anims[3], ESTRINGS::E_Strings(0xC), '(');

        int16_t list_pages = (n_ships_in_list + 5) / 5;

        fields[4] = ::fields::Add_Scroll_Field(
            0x1D4, 0x45, 0, list_pages * 5, 0, (list_pages - 3) * 5, 0xB, 0x150, &slider_bar_position, ESTRINGS::E_Strings(0xC), 0);

        fields[0] = ::fields::Add_Hidden_Field(
            0, 0, 0x27F, 0x1DF, ESTRINGS::E_Strings(0xC), 0);
    }

    void __cdecl Add_Ship_List_Fields() {
        for (int16_t slot_idx = 0; slot_idx < 15; ++slot_idx) {
            if (g_ship_list_ptr[slot_idx] == -1) {
                g_ship_field_list_ptr[slot_idx] = -1000;
            } else {
                int32_t y_offset = (slot_idx / 5) * 61;
                int32_t x_offset = (slot_idx % 5) * 60;
                char* hotkey = ESTRINGS::E_Strings(12);

                g_ship_field_list_ptr[slot_idx] = ::fields::Add_Hidden_Field(
                    x_offset + 161,
                    y_offset + 49,
                    x_offset + 216,
                    y_offset + 105,
                    hotkey,
                    0);
            }
        }
    }

    void __cdecl Draw_Bar_Indicator() {
        Update_First_Ship(&first_ship, &slider_bar_position);

        if (n_ships_in_list >= 15) {
            int32_t y1 = (first_ship * 335) / n_ships_in_list + 69;
            int32_t y2 = ((first_ship + 15) * 335) / (((n_ships_in_list + 5) / 5) * 5) + 69;

            graphics::Fill(0x1d4, y1, 0x1de, y2, 0xe5);

            line::Line(0x1d4, y1, 0x1de, y1, 0xe6);
            line::Line(0x1d4, y1, 0x1d4, y2, 0xe6);
            line::Line(0x1de, y1, 0x1de, y2, 0xe4);
            line::Line(0x1d4, y2, 0x1de, y2, 0xe4);

            graphics::Dot(0x1d4, 0x45, 0x50);
            graphics::Dot(0x1de, 0x45, 0x50);
            graphics::Dot(0x1d4, 0x194, 0x50);
            graphics::Dot(0x1de, 0x194, 0x50);
        }
    }

    void __cdecl Draw_Ship2(::game::s_world_state* world_state, int16_t ship_idx, int16_t slot_idx) {
        int16_t y_offset = (slot_idx / 5) * 61;
        int16_t x_offset = (slot_idx % 5) * 60;

        COLONY::Remap_Draw_Ship_Centered(x_offset + 188, y_offset + 77, ship_idx);

        auto planet_idx = HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);
        int16_t colony_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index;

        if (!COLCALC::Colony_Can_Build_Product(world_state, &MOX::colony[colony_idx], COLBLDG::Colony_Production_From_Ship_Index(ship_idx))) {
            COLONY::Set_Colony_Font_To_Red(1, 1);
            char* text = ESTRINGS::E_Strings(0x18B);
            BILL::Squeeze_Paragraph_Centered(x_offset + 161, y_offset + 49, 55, 56, text, 2);
            return;
        }

        if (MOX::ship[ship_idx].d.previous_owner != MOX::PLAYER_NUM) {
            COLONY::Set_Colony_Font_To_Red(1, 1);
            char* text = ESTRINGS::E_Strings(0xD8);
            BILL::Squeeze_Paragraph_Centered(x_offset + 161, y_offset + 49, 55, 56, text, 2);
            return;
        }

        if (MOX::ship[ship_idx].d.picture_num == 44) {
            COLONY::Set_Colony_Font_To_Red(1, 1);
            char* text = ESTRINGS::E_Strings(0x16A);
            BILL::Squeeze_Paragraph_Centered(x_offset + 161, y_offset + 49, 55, 56, text, 2);
        }
    }

    void __cdecl Scan_Ship_Info(int16_t ship_idx) {
        char buffer[10000];
        STRBUILD::s_builder builder = STRBUILD::Init(buffer, sizeof(buffer));

        COLONY::Set_Colony_Font_To(1);
        COLBLDG::Build_Ship_Info_Formatted_Paragraph(&MOX::ship[ship_idx], &builder);
        BILL::Squeeze_Print_Formatted_Paragraph(168, 248, 283, 169, buffer, 0);
    }

    void __cdecl Draw_Ship_List() {
        COLONY::Set_Colony_Font_To(1);

        for (int16_t i = 0; i < 15; ++i) {
            int16_t ship_idx = g_ship_list_ptr[first_ship + i];
            if (ship_idx != -1 && COLONY::full_draw != 0) {
                Draw_Ship2(&::game::world, ship_idx, i);
            }
        }
    }

    void __cdecl Draw_Ship_D_List() {
        char buffer[476];
        int16_t player_num = MOX::PLAYER_NUM;

        for (int16_t design_idx = 0; design_idx < SHIP_SIZE_COUNT; ++design_idx) {
            int32_t font_color = 0;

            if (MOX::player[player_num].ship_designs[design_idx].size == MOX::ship[g_ship_n].d.size) {
                if (fields[5 + design_idx] == lscanned_field) {
                    font_color = 1;
                }
                font_color++;
            }

            COLONY::Set_Colony_Font_To_Reg_Blue(4, font_color);

            char* empty_str = ESTRINGS::E_Strings(12);
            char* selection_name = COLBLDG::Selection_Name(COLBLDG::Colony_Production_From_Ship_Design_Index(design_idx));

            if (strcmp(selection_name, empty_str) != 0) {
                int32_t y = ((179 * (design_idx + 1)) / 8) + 49;

                char* name_to_print = COLBLDG::Selection_Name(COLBLDG::Colony_Production_From_Ship_Design_Index(design_idx));
                fonts::Print(183, (int16_t)y, name_to_print);

                char* hull_name = TECHDATA::hull_data[MOX::player[player_num].ship_designs[design_idx].size].name;
                fonts::Print_Right(457, (int16_t)y, hull_name);

                fonts::Print_Centered(320, (int16_t)y, ":");
            }
        }

        int32_t bottom_font_color = 1;
        if (lscanned_field == fields[11]) {
            bottom_font_color = 2;
        }

        COLONY::Set_Colony_Font_To_Reg_Blue(4, bottom_font_color);

        char* refit_str = ESTRINGS::E_Strings(800);
        int32_t bottom_y = ((179 * 7) / 8) + 49;
        fonts::Print_Centered(320, (int16_t)bottom_y, refit_str);

        char* ship_format = ESTRINGS::E_Strings(801);
        snprintf(buffer, sizeof(buffer), ship_format, MOX::ship[g_ship_n].d.name);

        COLONY::Set_Colony_Font_To_Reg_Blue(4, 1);
        fonts::Print_Centered(320, 20, buffer);
    }

    int16_t __cdecl Evaluate2_Input(int16_t* selected_field_id, int16_t* hovered_field_id) {
        char buffer[500];

        scanned_ship_d_index = -1;
        lscanned_field = -1000;

        for (int16_t design_idx = 0; design_idx < SHIP_SIZE_COUNT; design_idx++) {
            if (fields[design_idx + 5] == *hovered_field_id) {
                lscanned_field = *hovered_field_id;
                scanned_ship_d_index = design_idx;
            }
            if (*selected_field_id == fields[design_idx + 5]) {
                if (MOX::player[MOX::PLAYER_NUM].ship_designs[design_idx].size == MOX::ship[g_ship_n].d.size) {
                    COLBLDG::Set_Pending_Refit_Design(
                        g_ship_n, &MOX::player[MOX::PLAYER_NUM].ship_designs[design_idx]);
                    MOX::temp_star_handle = SHIP_SIZE_DOOM_STAR;
                    esc = 1;
                    return -3;
                }
                GENDRAW::Help(ESTRINGS::E_Strings(0x323));
                esc = 0;
            }
            if (*selected_field_id + fields[design_idx + 5] == 0) {
                snprintf(
                    buffer,
                    sizeof(buffer),
                    ESTRINGS::E_Strings(0x324),
                    &MOX::ship[g_ship_n],
                    &MOX::player[MOX::PLAYER_NUM].ship_designs[design_idx]);
                TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x325), buffer);
            }
        }

        if (fields[11] == *hovered_field_id) {
            lscanned_field = *hovered_field_id;
        }
        if (*selected_field_id == fields[11]) {
            esc = 1;
            return -2;
        }
        if (*selected_field_id + fields[11] == 0) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x326), ESTRINGS::E_Strings(0x327));
        }
        if (*selected_field_id == fields[3]) {
            esc = 1;
        }
        if (*selected_field_id + fields[0] == 0) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x328), ESTRINGS::E_Strings(0x329));
        }
        if (*selected_field_id + fields[3] == 0) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x32a), ESTRINGS::E_Strings(0x32b));
        }
        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            COLONY::full_draw = 1;
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        }

        return -1;
    }

    void __cdecl Add2_Fields() {
        fields[3] = ::fields::Add_Button_Field(
            0x112, 0x1B8, ESTRINGS::E_Strings(0xC), anims[3], ESTRINGS::E_Strings(0xC), 0x28);

        for (int16_t design_idx = 0; design_idx < SHIP_SIZE_COUNT; ++design_idx) {
            if (strcmp(ESTRINGS::E_Strings(0xC), COLBLDG::Selection_Name(COLBLDG::Colony_Production_From_Ship_Design_Index(design_idx))) == 0) {
                fields[5 + design_idx] = -1000;
            } else {
                fields[5 + design_idx] = ::fields::Add_Hidden_Field(
                    0xA1,
                    ((design_idx + 1) * 179) / 8 + 49,
                    0x1DE,
                    ((design_idx + 2) * 179) / 8 + 48,
                    ESTRINGS::E_Strings(0xC),
                    0x29);
            }
        }

        fields[11] = ::fields::Add_Hidden_Field(
            0xA1,
            ((6 + 1) * 179) / 8 + 49,
            0x1DE,
            ((6 + 2) * 179) / 8 + 48,
            ESTRINGS::E_Strings(0xC),
            0x29);

        fields[0] = ::fields::Add_Hidden_Field(
            0, 0, 0x27F, 0x1DF, ESTRINGS::E_Strings(0xC), 0);
    }

} // namespace COLREFIT