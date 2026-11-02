#include "pch.h"
#include "colony_read.h"

namespace COLXPORT {
    uint8_t cheezy_hack_col_count_state;

    int16_t cheezy_hack_col_count[MAX_PLAYERS];

    int16_t* g_colony_list_ptr;

    MOX2::action_func_t saved_autofunction;

    int16_t list_col[6];

    s_colsum_col_pop_ref list_col_pop_ref[6];

    int16_t list_col_coord_vars[6];

    int16_t list_col_pop_count[6];

    int16_t list_col_job_fields[6];

    int16_t list_fields[6];

    int16_t x_fields_array[4];

    s_animation_header* anims[4];

    void __cdecl Draw_Xport_Popup() {
        COLONY::Set_Colony_Font();

        if (COLONY::full_draw != 0) {
            COLMOVE::g_hacky_dont_draw_cluster_flag = 1;
            MOX2::Execute_Auto_Function(&COLXPORT::saved_autofunction);
            COLMOVE::g_hacky_dont_draw_cluster_flag = 0;
            COLONY::full_draw = 1;

            COLXPORT::anims[0] = (s_animation_header*)buffer::Buffer_Reload(COLONY::COLPUPS_LBX, 0x10, MOX::global_cache_seg);
            animate::Draw(0, 0, COLXPORT::anims[0]);
            video::Copy_Off_To_Back();

            COLONY::full_draw = 0;
        }

        COLXPORT::Clear_List_Col_Array();
        COLSUM::Update_First(6, &COLSUM::first, &COLSUM::slider_bar_position);

        for (int16_t i = 0; i < 6; ++i) {
            int16_t colony_idx = COLXPORT::g_colony_list_ptr[i + COLSUM::first];
            if (colony_idx != -1) {
                COLXPORT::Draw_Colony_Summary_For_Colony(colony_idx, i);
            }
        }

        COLXPORT::Draw_Bar_Indicator();

        int16_t mouse_y = mouse::Pointer_Y();
        int16_t mouse_x = mouse::Pointer_X();
        COLMOVE::Draw_Cluster(mouse_x, mouse_y);
    }

    /**
 * @brief Returns the number of non-outpost colonies belonging to a specific player.
 * 
 * If a "cheezy hack" state is active, it returns a cached value.
 * Otherwise, it iterates through the global colony list.
 */
    int16_t __cdecl N_Colonies(int16_t player_idx) {
        if (cheezy_hack_col_count_state != 0) {
            return cheezy_hack_col_count[(int16_t)player_idx];
        }

        int16_t colony_count = 0;
        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];

            if (colony_read::Own_Full_Colony(colony, player_idx)) {
                colony_count++;
            }
        }

        return colony_count;
    }

    /**
 * @brief Rebuilds the global list of colonies belonging to the current player.
 *
 * Scans through all existing colonies, filtering for those owned by the current player
 * and not marked as outposts. The resulting indices are stored in a global list,
 * which is then padded with -1 up to its capacity of 250.
 */
    void __cdecl Build_Global_Colony_List() {
        int16_t count = 0;

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            if (colony_read::Own_Full_Colony(&MOX::colony[i], MOX::PLAYER_NUM)) {
                g_colony_list_ptr[count] = i;
                count++;
            }
        }

        for (; count < MAX_COLONIES; ++count) {
            g_colony_list_ptr[count] = -1;
        }
    }

    void __cdecl Load_Xport_Anims() {
        anims[3] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLPUPS_LBX, 0x11, MOX::screen_seg);
        anims[1] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLPUPS_LBX, 0x13, MOX::screen_seg);
        anims[2] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLPUPS_LBX, 0x14, MOX::screen_seg);
    }

    int16_t __cdecl Xport_Popup() {
        int16_t colony_list_buffer[MAX_COLONIES];
        int16_t input_id;
        int16_t selected_colony = -1;

        for (int32_t i = 0; i < 15; ++i) {
            colony_list_buffer[i] = -1;
        }

        memset(&colony_list_buffer[15], 0, (MAX_COLONIES - 15) * sizeof(colony_list_buffer[0]));

        g_colony_list_ptr = colony_list_buffer;
        COLONY::full_draw = 1;

        Build_Global_Colony_List();
        fonts::Set_Font_Style_Shadow_Down(1, &COLONY::font_color_array);
        MOX2::Get_Auto_Function(&saved_autofunction);
        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(Draw_Xport_Popup, 1);
        ERICHELP::Set_Transport_Popup_Help_List();
        Draw_Xport_Popup();
        video::Set_Refresh_Stencil();
        Add_Fields();
        RUSS::Mox_Sync_Update();

        do {
            input_id = fields::Get_Input();

            if (input_id == x_fields_array[2]) {
                COLSUM::Decrement_First(6);
            } else if (input_id == x_fields_array[3]) {
                if (g_colony_list_ptr[COLSUM::first + 6] != -1) {
                    COLSUM::Increment_First(6);
                }
            } else {
                for (int32_t i = 0; i < 6; ++i) {
                    if (list_fields[i] == input_id) {
                        selected_colony = list_col[i];
                    }
                }
            }

            if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                MOX::FULL_DRAW_SCREEN_FLAG = 0;
                COLONY::full_draw = 1;
                ERICHELP::Set_Transport_Popup_Help_List();
            }

            Draw_Xport_Popup();
            Add_Fields();
            RUSS::Mox_Sync_Update();

        } while (input_id != x_fields_array[1] && selected_colony == -1);

        fields::Clear_Fields();
        fields::Deactivate_Help_List();
        fields::Deactivate_Auto_Function();
        MOX2::Put_Auto_Function(&saved_autofunction);

        return selected_colony;
    }

    void __cdecl Add_Fields() {
        int16_t num_colonies;
        int16_t y_offset;
        char* empty_string;

        fields::Clear_Fields();

        num_colonies = COLXPORT::N_Colonies(MOX::PLAYER_NUM);

        if (num_colonies < 6) {
            COLXPORT::x_fields_array[0] = -1000;
        } else {
            empty_string = ESTRINGS::E_Strings(12);
            COLXPORT::x_fields_array[0] = fields::Add_Scroll_Field(
                449,
                75,
                0,
                num_colonies,
                0,
                num_colonies - 6,
                14,
                164,
                &COLSUM::slider_bar_position,
                empty_string,
                0);
        }

        COLXPORT::x_fields_array[2] = fields::Add_Button_Field(
            448,
            52,
            ESTRINGS::E_Strings(12),
            anims[1],
            ESTRINGS::E_Strings(12),
            40);

        COLXPORT::x_fields_array[3] = fields::Add_Button_Field(
            447,
            243,
            ESTRINGS::E_Strings(12),
            anims[2],
            ESTRINGS::E_Strings(12),
            40);

        COLXPORT::x_fields_array[1] = fields::Add_Button_Field(
            352,
            285,
            ESTRINGS::E_Strings(12),
            anims[3],
            ESTRINGS::E_Strings(12),
            40);

        y_offset = 83;
        for (int32_t i = 0; i < 6; ++i) {
            if (COLXPORT::list_col[i] != -1) {
                char* scroll_str = ESTRINGS::E_Strings(12);
                COLXPORT::list_fields[i] = fields::Add_Hidden_Field(
                    15,
                    y_offset - 31,
                    441,
                    y_offset,
                    scroll_str,
                    0);
            }
            y_offset += 38;
        }
    }

    void __cdecl Draw_Colony_Summary_For_Colony(int16_t colony_idx, int16_t entry_idx) {
        COLXPORT::list_col[entry_idx] = colony_idx;

        int16_t planet_idx = MOX::colony[colony_idx].planet;
        int16_t star_idx = MOX::planet[planet_idx].star_index;

        if (((MOX::star[star_idx].blockaded >> MOX::PLAYER_NUM) & 1) != 0 || EVENTS::Event_Check_Plague(colony_idx) != 0) {
            COLONY::Set_Colony_Font_To_Red(4, 0);
        } else {
            COLONY::Set_Colony_Font_To(4);
        }

        char* planet_name = HACCESS::Get_Planet_Name(planet_idx);
        int16_t y_pos = entry_idx * 38 + 54;
        BILL::Squeeze_Print_Paragraph(15, y_pos, 130, 28, planet_name, 0);

        COLXPORT::Draw_Info_Pop_For(entry_idx);
    }

    void __cdecl Clear_List_Col_Array() {
        memset(list_col, 0xff, sizeof(list_col));
    }

    void __cdecl Draw_Bar_Indicator() {
        int16_t n_colonies = COLXPORT::N_Colonies(MOX::PLAYER_NUM);
        COLSUM::Update_First(6, &COLSUM::first, &COLSUM::slider_bar_position);

        if (n_colonies >= 6) {
            int16_t y1 = (int16_t)((COLSUM::first * 164) / n_colonies) + 75 /* 0x4b */;
            int16_t y2 = (int16_t)(((COLSUM::first + 6) * 164) / n_colonies) + 75 /* 0x4b */;

            graphics::Fill(449 /* 0x1c1 */, y1, 457 /* 0x1c9 */, y2, 229 /* 0xe5 */);

            line::Line(449 /* 0x1c1 */, y1, 457 /* 0x1c9 */, y1, 230 /* 0xe6 */);
            line::Line(449 /* 0x1c1 */, y1, 449 /* 0x1c1 */, y2, 230 /* 0xe6 */);
            line::Line(457 /* 0x1c9 */, y1, 457 /* 0x1c9 */, y2, 228 /* 0xe4 */);
            line::Line(449 /* 0x1c1 */, y2, 457 /* 0x1c9 */, y2, 228 /* 0xe4 */);

            graphics::Dot(449 /* 0x1c1 */, 75 /* 0x4b */, 80 /* 0x50 / 'P' */);
            graphics::Dot(457 /* 0x1c9 */, 75 /* 0x4b */, 80 /* 0x50 / 'P' */);
            graphics::Dot(449 /* 0x1c1 */, 239 /* 0xef */, 80 /* 0x50 / 'P' */);
            graphics::Dot(457 /* 0x1c9 */, 239 /* 0xef */, 80 /* 0x50 / 'P' */);
        } else {
            graphics::Fill(449 /* 0x1c1 */, 75 /* 0x4b */, 457 /* 0x1c9 */, 239 /* 0xef */, 80 /* 0x50 / 'P' */);
        }
    }

    void __cdecl Draw_Info_Pop_For(int16_t entry_index) {
        const int32_t index = (int32_t)entry_index;
        int16_t y = (int16_t)(entry_index * 0x26);
        y = (int16_t)(y + 0x36);

        COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(
            COLXPORT::list_col[index],
            -1,
            0,
            0x94,
            y,
            0x1B9,
            0,
            COLXPORT::list_col_pop_ref[index].refs,
            &COLXPORT::list_col_pop_count[index],
            &COLXPORT::list_col_coord_vars[index],
            &COLXPORT::list_col_job_fields[index]);
    }

} // namespace COLXPORT