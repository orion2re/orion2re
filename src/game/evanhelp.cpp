#include "pch.h"

namespace EVANHELP {
    s_help_box main_screen_help_list[25] = {
        {284, 542, 24, 616, 41},
        {285, 237, 0, 320, 20},
        {286, 541, 45, 617, 119},
        {288, 541, 121, 617, 193},
        {290, 541, 195, 617, 268},
        {291, 541, 270, 617, 342},
        {292, 541, 344, 617, 418},
        {293, 13, 428, 79, 473},
        {294, 91, 428, 155, 473},
        {295, 166, 428, 231, 473},
        {300, 236, 425, 307, 479},
        {297, 311, 428, 381, 475},
        {298, 385, 428, 454, 475},
        {299, 461, 428, 527, 475},
        {296, 542, 426, 618, 479},
    };

    s_help_box static_design_screen_help_list[13] = {
        {375, 17, 15, 227, 44},
        {376, 231, 53, 427, 150},
        {377, 432, 53, 623, 149},
        {378, 118, 55, 226, 149},
        {379, 16, 55, 114, 118},
        {382, 18, 120, 111, 147},
        {383, 17, 154, 622, 282},
        {385, 16, 304, 623, 429},
        {387, 371, 440, 443, 466},
        {388, 459, 440, 531, 466},
        {389, 545, 440, 617, 466},
        {390, 18, 441, 140, 466},
        {391, 157, 441, 357, 464},
    };

    s_help_box design_screen_help_list[13];

    s_help_box main_game_screen_help_list[12] = {
        {415, 145, 0, 639, 26},
        {415, 0, 0, 143, 479},
        {415, 145, 403, 420, 479},
        {415, 422, 28, 639, 479},
        {416, 181, 69, 276, 97},
        {417, 290, 69, 382, 97},
        {418, 182, 112, 275, 143},
        {419, 289, 113, 382, 144},
        {420, 196, 188, 370, 236},
        {421, 196, 238, 369, 290},
        {422, 181, 332, 276, 363},
        {423, 293, 332, 385, 362},
    };

    s_help_box main_menu_screen_help_list[MAIN_MENU_BTN_QUIT + 1] = {
        {645, 415, 160, 567, 181},
        {646, 415, 183, 567, 204},
        {647, 415, 205, 567, 226},
        {648, 415, 228, 567, 249},
        {649, 415, 250, 567, 271},
        // Quit follows the centred 7-button stack; Settings itself
        // has no help entry, like the Credits link.
        {650, 415, 296, 567, 317},
    };

    s_help_box multi_player_setup_screen_help_list[10] = {
        {651, 135, 161, 294, 189},
        {652, 135, 192, 294, 221},
        {653, 135, 225, 294, 253},
        {654, 135, 255, 294, 286},
        {655, 253, 358, 385, 384},
        {705, 189, 292, 439, 318},
        {656, 345, 161, 504, 189},
        {657, 345, 192, 504, 221},
        {658, 345, 225, 504, 253},
        {659, 345, 256, 504, 286},
    };

    s_help_box load_game_screen_help_list[7] = {
        {415, 145, 0, 639, 26},
        {415, 0, 0, 143, 479},
        {415, 145, 403, 420, 479},
        {415, 422, 28, 639, 479},
        {424, 163, 43, 401, 357},
        {426, 178, 360, 254, 389},
        {425, 310, 359, 385, 388},
    };

    s_help_box options_game_screen_help_list[18] = {
        {415, 145, 0, 639, 26},
        {415, 0, 0, 143, 479},
        {415, 145, 403, 420, 479},
        {415, 422, 28, 639, 479},
        {429, 169, 67, 398, 80},
        {430, 169, 84, 397, 96},
        {431, 170, 100, 396, 114},
        {432, 170, 117, 396, 131},
        {433, 170, 134, 396, 148},
        {434, 170, 151, 396, 165},
        {435, 170, 168, 396, 182},
        {436, 170, 185, 396, 199},
        {437, 170, 202, 395, 216},
        {438, 170, 219, 395, 233},
        {439, 170, 236, 395, 250},
        {440, 170, 253, 395, 267},
        {441, 170, 270, 395, 284},
        {442, 236, 358, 319, 387},
    };

    s_help_box save_game_screen_help_list[7] = {
        {415, 145, 0, 639, 26},
        {415, 0, 0, 143, 479},
        {415, 145, 403, 420, 479},
        {415, 422, 28, 639, 479},
        {427, 155, 37, 402, 357},
        {428, 178, 360, 252, 387},
        {425, 310, 359, 386, 388},
    };

    s_help_box plntsum_screen_help_list[15] = {
        {337, 17, 37, 415, 469},
        {343, 441, 15, 623, 164},
        {345, 423, 14, 434, 469},
        {348, 442, 202, 498, 223},
        {349, 502, 202, 564, 223},
        {350, 568, 202, 623, 223},
        {351, 442, 267, 623, 287},
        {352, 442, 291, 623, 310},
        {353, 442, 314, 623, 333},
        {354, 442, 337, 623, 356},
        {355, 442, 360, 623, 378},
        {356, 468, 390, 605, 406},
        {357, 468, 418, 605, 436},
        {358, 452, 440, 611, 464},
        {338, 16, 12, 414, 32},
    };

    s_help_box input_box_help_list[1] = {
        {675, 0, 0, 639, 479},
    };

    s_help_box static_diplomacy_request_help_list[4] = {
        {678, 62, 90, 168, 113},
        {679, 188, 90, 294, 113},
        {680, 314, 90, 419, 113},
        {677, 0, 0, 639, 479},
    };

    s_help_box static_diplomacy_request_help_list2[4];

    s_help_box* prev_help_struct_pointer;
    int16_t prev_help_list_active;
    int16_t prev_help_list_count;

    s_help_box static_fleet_screen_help_list[32] = {
        {361, 605, 59, 619, 349},
        {363, 66, 248, 267, 268},
        {364, 19, 249, 50, 267},
        {365, 283, 249, 313, 267},
        {366, 13, 280, 319, 465},
        {368, 348, 380, 421, 407},
        {369, 441, 380, 529, 407},
        {370, 549, 380, 621, 407},
        {371, 342, 430, 414, 456},
        {372, 425, 435, 485, 453},
        {373, 487, 435, 546, 453},
        {374, 456, 430, 628, 456},
    };

    s_help_box fleet_screen_help_list[32];

    s_help_box static_coloff_screen_help_list[16] = {
        {313, 9, 11, 148, 31},
        {314, 150, 11, 295, 31},
        {315, 305, 16, 624, 196},
        {316, 91, 34, 299, 140},
        {316, 91, 143, 299, 250},
        {316, 91, 252, 299, 359},
        {316, 91, 361, 300, 472},
        {318, 364, 203, 566, 226},
        {319, 327, 204, 362, 225},
        {320, 569, 204, 602, 225},
        {321, 306, 234, 623, 403},
        {322, 363, 410, 567, 434},
        {323, 306, 440, 378, 465},
        {324, 388, 440, 460, 465},
        {325, 469, 440, 541, 465},
        {326, 554, 441, 623, 464},
    };

    s_help_box coloff_screen_help_list[16];

    s_help_box static_shipoff_screen_help_list[33] = {
        {313, 9, 11, 148, 31},
        {314, 150, 11, 295, 31},
        {328, 91, 34, 299, 140},
        {328, 91, 143, 299, 250},
        {328, 91, 252, 299, 359},
        {328, 91, 361, 300, 472},
        {329, 364, 203, 566, 226},
        {330, 327, 204, 362, 225},
        {331, 569, 204, 602, 225},
        {332, 306, 234, 623, 403},
        {322, 363, 410, 567, 434},
        {323, 306, 440, 378, 465},
        {324, 388, 440, 460, 465},
        {325, 469, 440, 541, 465},
        {334, 613, 21, 623, 41},
        {335, 613, 45, 624, 167},
        {336, 613, 170, 623, 190},
        {326, 554, 441, 623, 464},
    };

    s_help_box shipoff_screen_help_list[33];

    s_help_box scrap_freighters_help_list[1] = {
        {660, 0, 0, 639, 479},
    };

    s_help_box weapon_replacement_screen_help_list[12];

    s_help_box static_hotseat_setup_screen_help[4] = {
        {701, 1, 1, 408, 245},
        {702, 126, 100, 279, 125},
        {703, 126, 137, 278, 160},
        {704, 141, 201, 269, 224},
    };

    s_help_box hotseat_setup_screen_help_list[4];

    void __cdecl Set_Main_Screen_Help_List() {
        int16_t help_list_count = 15;

        if (MOX::game_type == 2) {
            EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 303, 73, 0, 156, 19);
            EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 302, 418, 0, 519, 19);
        } else if (MOX::game_type == 1) {
            EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 303, 73, 0, 156, 19);
        }

        if (MOVEBOX::Moveable_Box_Selected(2) != 0) {
            int16_t button_height = animate::Get_Height(MOX::fleet_box_close_lg_button_seg);
            int16_t button_left = MOX::fleet_box.x + 2;
            int16_t button_top = MOX::fleet_box.y - button_height + MOX::fleet_box.field8_0xe;
            int16_t button_right = MOX::fleet_box.width + button_left;

            EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 310, button_left, button_top - 35, button_right, button_top);
            EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 304, button_left, button_top, button_right, button_top + button_height);

            int16_t stacked_height = button_height;

            if (MOX::fleet_box.outpost_btn_active != 0) {
                EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 309, button_left, button_top, button_right, button_top + stacked_height + button_height);
                stacked_height += button_height;
            }

            if (MOX::fleet_box.colonize_btn_active != 0) {
                EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 306, button_left, button_top, button_right, button_top + stacked_height + button_height);
                stacked_height += button_height;
            }

            if (MOX::fleet_box.engage_btn_active != 0) {
                EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 307, button_left, button_top, button_right, button_top + stacked_height + button_height);
                stacked_height += button_height;
            }

            if (MOX::fleet_box.transport_btn_active != 0) {
                EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 308, button_left, button_top, button_right, button_top + stacked_height + button_height);
                stacked_height += button_height;
            }

            if (MOX::fleet_box.attack_btn_active != 0) {
                EVANHELP::Append_Help_List(EVANHELP::main_screen_help_list, &help_list_count, 25, 305, button_left, button_top, button_right, button_top + stacked_height + button_height);
            }
        }

        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(EVANHELP::main_screen_help_list, help_list_count);
    }

    void __cdecl Set_Main_Game_Screen_Help_List() {
        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(EVANHELP::main_game_screen_help_list, 0xc);
    }

    void __cdecl Set_Multi_Player_Setup_Screen_Help_List() {
        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(multi_player_setup_screen_help_list, 10);
    }

    void Set_Load_Game_Screen_Help_List() {
        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(load_game_screen_help_list, 7);
    }

    void __cdecl Set_Options_Game_Screen_Help_List() {
        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(options_game_screen_help_list, 18);
    }

    void __cdecl Set_Save_Game_Screen_Help_List() {
        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(save_game_screen_help_list, 7);
    }

    void __cdecl Set_PlntSum_Screen_Help_List() {
        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(plntsum_screen_help_list, 15);
    }

    void __cdecl Set_Officer_Screen_Help() {
        int16_t displayed_skill_counts[4] = {0};

        OFFICER::Get_N_Skills_Per_Officer_Displayed(displayed_skill_counts);

        if (MOX::officer_scrn_type == 1) {
            Set_ColOff_Screen_Help_List(displayed_skill_counts);
        } else {
            Set_ShipOff_Screen_Help_List(displayed_skill_counts, MOX::n_fltscrn_big_icons_added);
        }
    }

    void __cdecl Set_Design_Screen_Help_List(int16_t lines_1, int16_t lines_2) {
        memcpy(design_screen_help_list, static_design_screen_help_list, sizeof(design_screen_help_list));

        if (lines_1 > 0) {
            int32_t new_y1 = design_screen_help_list[6].y1 + lines_1 * 14 + 15;
            if (new_y1 >= design_screen_help_list[6].y2) {
                new_y1 = design_screen_help_list[6].y2;
            }
            design_screen_help_list[6].y1 = (int16_t)new_y1;
        }

        if (lines_2 > 0) {
            int32_t new_y1 = design_screen_help_list[7].y1 + lines_2 * 14 + 15;
            if (new_y1 >= design_screen_help_list[7].y2) {
                new_y1 = design_screen_help_list[7].y2;
            }
            design_screen_help_list[7].y1 = (int16_t)new_y1;
        }

        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(design_screen_help_list, 13);
    }

    void __cdecl Get_Application_Description(char* dest, int32_t dest_size, int16_t item_index) {
        if (dest_size <= 0) {
            return;
        }

        char help_lbx_name[20];

        TEXTBOX::Get_Help_Lbx_Name(help_lbx_name, sizeof(help_lbx_name));
        allocate::Push_Block(MOX::screen_seg);

        void* buffer = allocate::Allocate_Next_Block(MOX::screen_seg, 2000);
        allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(s_help_record));

        s_help_record* data = (s_help_record*)farload::Far_Reload_Next_Data(help_lbx_name, 0, buffer, item_index, 1, sizeof(s_help_record));

        strlcpy(dest, data->body, (size_t)dest_size);

        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Save_Help_List() {
        prev_help_struct_pointer = fields::help_struct_pointer;
        prev_help_list_count = fields::help_list_count;
        prev_help_list_active = fields::help_list_active;
    }

    void __cdecl Set_Input_Box_Help_List() {
        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(input_box_help_list, 1);
    }

    void __cdecl Restore_Help_List() {
        fields::help_struct_pointer = prev_help_struct_pointer;
        fields::help_list_count = prev_help_list_count;
        fields::help_list_active = prev_help_list_active;

        if (prev_help_list_active != 0) {
            fields::Set_Help_List(prev_help_struct_pointer, prev_help_list_count);
        } else {
            fields::Deactivate_Help_List();
        }
    }

    void __cdecl Set_Fleet_Screen_Help_List(int16_t start_index) {
        int16_t help_list_count = 12;

        memcpy(EVANHELP::fleet_screen_help_list, EVANHELP::static_fleet_screen_help_list, sizeof(EVANHELP::fleet_screen_help_list));

        if (start_index == 0) {
            EVANHELP::Append_Help_List(
                EVANHELP::fleet_screen_help_list,
                &help_list_count,
                0x20,
                0x168,
                0x158,
                0x35,
                0x24f,
                0x161);
        } else {
            for (int16_t i = start_index; i < 20; ++i) {
                int32_t slot_offset_x = (i % 4) * 0x40;
                int32_t slot_offset_y = (i / 4) * 0x40;

                EVANHELP::Append_Help_List(
                    EVANHELP::fleet_screen_help_list,
                    &help_list_count,
                    0x20,
                    0x168,
                    slot_offset_x + 0x156,
                    slot_offset_y + 0x34,
                    slot_offset_x + 0x196,
                    slot_offset_y + 0x72);
            }
        }

        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(EVANHELP::fleet_screen_help_list, help_list_count);
    }

    void __cdecl Set_Scrap_Freighters_Help_List() {
        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(scrap_freighters_help_list, 1);
    }

    void __cdecl Set_Hotseat_Setup_Screen_Help_List(int16_t x_offset, int16_t y_offset) {
        memcpy(hotseat_setup_screen_help_list, static_hotseat_setup_screen_help, sizeof(static_hotseat_setup_screen_help));
        Offset_Help_Field_XYs(hotseat_setup_screen_help_list, 4, x_offset, y_offset);
        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(hotseat_setup_screen_help_list, 4);
    }

    void __cdecl Set_Weapon_Replacement_Screen_Help_List(int16_t total_rows, int16_t x, int16_t y, uint8_t needs_racks) {
        int16_t help_count = 0;

        int16_t height = animate::Get_Height(MOX::weapons_mid_box_seg);
        if (total_rows < 7) {
            height = 0x66;
        }

        int16_t base_y = height + animate::Get_Height(MOX::weapons_top_box_seg) + y;

        memset(weapon_replacement_screen_help_list, 0, sizeof(weapon_replacement_screen_help_list));

        int16_t y1 = base_y + 10;

        int16_t h = animate::Get_Height(MOX::beam_filter_button_seg) + y1;
        int16_t w = animate::Get_Width(MOX::beam_filter_button_seg) + x + 0x65;
        Append_Help_List(weapon_replacement_screen_help_list, &help_count, 12, 0x188, x + 0x65, y1, w, h);

        h = animate::Get_Height(MOX::missile_filter_button_seg) + y1;
        w = animate::Get_Width(MOX::missile_filter_button_seg) + x + 0xcb;
        Append_Help_List(weapon_replacement_screen_help_list, &help_count, 12, 0x189, x + 0xcb, y1, w, h);

        h = animate::Get_Height(MOX::bomb_filter_button_seg) + y1;
        w = animate::Get_Width(MOX::bomb_filter_button_seg) + x + 0x146;
        Append_Help_List(weapon_replacement_screen_help_list, &help_count, 12, 0x18a, x + 0x146, y1, w, h);

        h = animate::Get_Height(MOX::special_filter_button_seg) + y1;
        w = animate::Get_Width(MOX::special_filter_button_seg) + x + 0x1ad;
        Append_Help_List(weapon_replacement_screen_help_list, &help_count, 12, 0x18b, x + 0x1ad, y1, w, h);

        int16_t btn_x = x + 0x21a;

        int16_t cancel_y = base_y + 0x52;
        h = animate::Get_Height(MOX::weapons_cancel_button_seg) + cancel_y;
        w = animate::Get_Width(MOX::weapons_cancel_button_seg) + btn_x;
        Append_Help_List(weapon_replacement_screen_help_list, &help_count, 12, 0x18c, btn_x, cancel_y, w, h);

        int16_t accept_y = base_y + 0x82;
        h = animate::Get_Height(MOX::weapons_accept_button_seg) + accept_y;
        w = animate::Get_Width(MOX::weapons_accept_button_seg) + btn_x;
        Append_Help_List(weapon_replacement_screen_help_list, &help_count, 12, 0x18d, btn_x, accept_y, w, h);

        int16_t conditional_id = (needs_racks == 0) ? 0x18e : 0x18f;
        Append_Help_List(weapon_replacement_screen_help_list, &help_count, 12, conditional_id, x + 0x18, base_y + 0x45, x + 0xd2, base_y + 0xaa);

        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(weapon_replacement_screen_help_list, help_count);
    }

    void __cdecl Offset_Help_Field_XYs(s_help_box* help_boxes, int16_t count, int16_t offset_x, int16_t offset_y) {
        for (int16_t i = 0; i < count; ++i) {
            help_boxes[i].x1 += offset_x;
            help_boxes[i].y1 += offset_y;
            help_boxes[i].x2 += offset_x;
            help_boxes[i].y2 += offset_y;
        }
    }

    void __cdecl Set_ColOff_Screen_Help_List(int16_t* displayed_skill_counts) {
        memcpy(coloff_screen_help_list, static_coloff_screen_help_list, sizeof(static_coloff_screen_help_list));

        for (int16_t i = 0; i < 4; ++i) {
            if (displayed_skill_counts[i] > 0) {
                coloff_screen_help_list[i + 3].y1 += displayed_skill_counts[i] * 17 + 16;
            }
        }

        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(coloff_screen_help_list, 16);
    }

    void __cdecl Set_ShipOff_Screen_Help_List(int16_t* displayed_skill_counts, int16_t start_index) {
        int16_t count = 18;

        memcpy(shipoff_screen_help_list, static_shipoff_screen_help_list, sizeof(shipoff_screen_help_list));

        for (int16_t i = 0; i < 4; i++) {
            if (displayed_skill_counts[i] > 0) {
                shipoff_screen_help_list[i + 2].y1 += displayed_skill_counts[i] * 17 + 16;
            }
        }

        if (start_index == 0) {
            Append_Help_List(shipoff_screen_help_list, &count, 33, 333, 301, 16, 610, 198);
        } else {
            for (int16_t i = start_index; i < 15; i++) {
                int16_t x1 = (i % 5) * 63 + 301;
                int16_t y1 = (i / 5) * 62 + 16;
                Append_Help_List(shipoff_screen_help_list, &count, 33, 333, x1, y1, x1 + 63, y1 + 62);
            }
        }

        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(shipoff_screen_help_list, count);
    }

    void __cdecl Set_Diplomacy_Request_Help_List(int16_t x_offset, int16_t y_offset) {
        memcpy(
            EVANHELP::static_diplomacy_request_help_list2,
            EVANHELP::static_diplomacy_request_help_list,
            sizeof(EVANHELP::static_diplomacy_request_help_list));

        EVANHELP::Offset_Help_Field_XYs(
            EVANHELP::static_diplomacy_request_help_list2,
            4,
            x_offset,
            y_offset);

        MAINMENU::Set_Main_Menu_Screen_Help_List_Aux(
            EVANHELP::static_diplomacy_request_help_list2,
            4);
    }

    void __cdecl Append_Help_List(s_help_box* help_list, int16_t* list_count, int32_t, int16_t help_id, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        help_list[*list_count].help_id = help_id;
        help_list[*list_count].x1 = x1;
        help_list[*list_count].y1 = y1;
        help_list[*list_count].x2 = x2;
        help_list[*list_count].y2 = y2;
        ++*list_count;
    }
} // namespace EVANHELP