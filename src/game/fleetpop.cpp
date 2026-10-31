#include "pch.h"
#include "game/game_events.h"

namespace FLEETPOP {
    uint8_t n_added_buttons;

    void __cdecl Fleet_Movement_Scroll_Callback() {
        int16_t thumb_height = MOX::fltpop_scroll_bar.thumb_height_px;
        int16_t track_len = MOX::fltpop_scroll_bar.track_len_px;
        int16_t thumb_y = MOX::fltpop_scroll_bar.thumb_center_y;

        int16_t start_y = static_cast<int16_t>(
            MOX::fleet_box.y +
            MOX::moveable_box[2].content_offset_y +
            track_len -
            (thumb_height / 2) -
            thumb_y +
            0x43);

        int16_t end_y = static_cast<int16_t>(start_y + thumb_height);
        int16_t x = static_cast<int16_t>(MOX::fleet_box.x + MOX::moveable_box[2].content_offset_x + 0xC0);
        uint8_t color = 0x74;

        for (int16_t i = 0; i < 9; ++i) {
            line::Line(x, start_y, x, end_y, color);

            if ((i % 2) == 0) {
                --color;
            }

            ++x;
        }

        s_animation_header* scroll_anim =
            static_cast<s_animation_header*>(buffer::Buffer_Reload("BUFFER0.LBX", 0x39, MOX::global_cache_seg));
        animate::Draw(
            static_cast<int16_t>(MOX::fleet_box.x + MOX::moveable_box[2].content_offset_x + 0xBD),
            static_cast<int16_t>(MOX::fleet_box.y + MOX::moveable_box[2].content_offset_y + 0x42),
            scroll_anim);
    }

    void __cdecl Draw_Damage_Bars(int16_t x, int16_t y, int16_t width, int16_t y_offset, int16_t x_offset, int16_t ship_idx) {
        int16_t effective_width = width - 4;
        int16_t draw_x = x_offset + x;
        int16_t draw_y = y + y_offset - 5;

        int16_t max_hits = AIPOWER::Max_Ship_Hits(ship_idx);
        int16_t current_hits = AIPOWER::Current_Ship_Hits(ship_idx);

        int16_t green_width = (static_cast<int>(current_hits) * effective_width) / max_hits;
        if (green_width < 2) {
            green_width = 2;
        }

        int16_t red_width = effective_width - green_width;

        if (current_hits != max_hits) {
            if (red_width == 1) {
                red_width = 2;
                green_width = width - 6;
            }

            if (green_width > 0) {
                int16_t end_x = draw_x + green_width;
                int16_t bottom_y = draw_y + 4;

                line::Line(draw_x, draw_y, end_x, draw_y, 0x70);
                line::Line(draw_x, draw_y, draw_x, bottom_y, 0x70);
                line::Line(end_x, draw_y + 1, end_x, bottom_y, 0x72);
                line::Line(end_x, bottom_y, draw_x + 1, bottom_y, 0x72);
                graphics::Fill(draw_x + 1, draw_y + 1, end_x - 1, draw_y + 3, 0x71);
            }

            if (red_width > 1) {
                int16_t start_red_x = draw_x + green_width + 1;
                int16_t end_red_x = start_red_x + red_width;
                int16_t bottom_y = draw_y + 4;

                line::Line(start_red_x, draw_y, end_red_x, draw_y, 0x4B);
                line::Line(start_red_x, draw_y, start_red_x, bottom_y, 0x4B);
                line::Line(end_red_x, draw_y + 1, end_red_x, bottom_y, 0x4E);
                line::Line(end_red_x, bottom_y, start_red_x + 1, bottom_y, 0x4E);
                graphics::Fill(start_red_x + 1, draw_y + 1, end_red_x - 1, draw_y + 3, 0x4D);
            }
        }
    }

    int16_t __cdecl Fleet_Box_Height(int16_t fleet_count) {
        int16_t close_btn_height = animate::Get_Height(MOX::fleet_box_close_lg_button_seg);
        int16_t name_height;
        int16_t bottom_height;

        if (MOX::fleet_box.is_small_frame == 0) {
            MOX::fleet_box_name_lg_seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x33, MOX::global_cache_seg);
            MOX::fleet_box.name_seg = MOX::fleet_box_name_lg_seg;
            name_height = animate::Get_Height(MOX::fleet_box.name_seg);

            MOX::fleet_box.bottom_seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x37, MOX::global_cache_seg);
            MOX::fleet_box_bottom_lg_seg = MOX::fleet_box.bottom_seg;
        } else {
            MOX::fleet_box_name_sm_seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x34, MOX::global_cache_seg);
            MOX::fleet_box.name_seg = MOX::fleet_box_name_sm_seg;
            name_height = animate::Get_Height(MOX::fleet_box.name_seg);

            MOX::fleet_box.bottom_seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x38, MOX::global_cache_seg);
            MOX::fleet_box_bottom_sm_seg = MOX::fleet_box.bottom_seg;
        }

        bottom_height = animate::Get_Height(MOX::fleet_box.bottom_seg);

        int16_t grid_height;
        if (fleet_count > 9) {
            MOX::fleet_box_grid_lg_seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x35, MOX::global_cache_seg);
            grid_height = animate::Get_Height(MOX::fleet_box_grid_lg_seg);
        } else {
            int32_t rows = (static_cast<int32_t>(fleet_count) - 1) / 3 + 1;
            if (rows > 3) {
                rows = 3;
            }
            grid_height = FLEETPOP::Fleet_Box_Grid_Height(static_cast<int16_t>(rows));
        }

        return grid_height + close_btn_height + bottom_height + name_height;
    }

    void __cdecl Add_Fleet_Movement_Box_Fields(int16_t node_ptr, int16_t allow_input_fields) {
        int16_t first_icon_x = -1;
        int16_t first_icon_y = -1;
        int16_t stack_node = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);

        if (stack_node == MOX::first_node_ptr_in_stack_) {
            node_ptr = stack_node;
            int16_t num_icons = N_Fleet_Icons(node_ptr);
            Fleet_Box_Data(num_icons);

            int16_t box_x = MOX::moveable_box[2].x;
            int16_t box_y = MOX::moveable_box[2].y;
            int16_t box_width = Fleet_Box_Width(MOX::fleet_box.count);

            MOX::moveable_box[2].window_field_id = -1000;
            MOX::moveable_box[2].drag_field = fields::Add_Hidden_Field(
                box_x, box_y, box_width + box_x, box_y + 0x23, (char*)"", 0);

            int16_t end_idx = MOX::fleet_icon_buttons_added + MOX::first_fleet_movement_icon;
            if (MOX::fleet_box.count < end_idx) {
                end_idx = MOX::fleet_box.count;
            }

            for (int16_t i = MOX::first_fleet_movement_icon; i < end_idx; i++) {
                int16_t icon_x = MOX::fleet_movement_icon[i].x;
                int16_t icon_y = MOX::fleet_movement_icon[i].y;

                if (first_icon_x < 0) {
                    first_icon_x = icon_x;
                }
                if (first_icon_y < 0) {
                    first_icon_y = icon_y;
                }

                HACCESS::Fleet_Movement_Ship_Id(i);

                MOX::fleet_movement_icon[i].field = fields::Add_Hidden_Field(
                    icon_x, icon_y, icon_x + 0x35, icon_y + 0x35, (char*)"", 0x29);
            }

            node_ptr = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);
            int16_t ship_idx = MOX::ship_node[node_ptr].ship_idx;

            if (MOX::moveable_box_order[0] == 2 && allow_input_fields != 0 &&
                HACCESS::Ship_Owner(ship_idx) == MOX::PLAYER_NUM &&
                HACCESS::Player_Can_Order_Ship(MOX::PLAYER_NUM, &MOX::ship[ship_idx])) {
                int16_t btn_x = box_x + 16;
                if (MOX::fleet_box.is_small_frame == 1) {
                    btn_x += 1;
                }

                MOX::fleet_movement_all_button = fields::Add_Clipped_Button_Field(
                    btn_x, MOX::fleet_box.field9_0x10 + box_y, (char*)"", MOX::fleet_box_all_button_seg, (char*)"", 0x28);
            } else {
                MOX::fleet_movement_all_button = -1000;
            }

            if (MOX::fleet_box.count > 9) {
                int16_t up_btn = -1000;
                int16_t dn_btn = -1000;

                MOX::fleet_box_scroll_up_button = -1000;
                MOX::fleet_box_scroll_dn_button = -1000;

                if (allow_input_fields != 0) {
                    up_btn = fields::Add_Button_Field(
                        box_x + 0xbf, box_y + 0x2a, (char*)"", MOX::fleet_box_scroll_up_button_seg, (char*)"-", 0x28);
                    MOX::fleet_box_scroll_up_button = up_btn;

                    dn_btn = fields::Add_Button_Field(
                        box_x + 0xbf, box_y + 0xae, (char*)"", MOX::fleet_box_scroll_dn_button_seg, (char*)"+", 0x28);
                }

                MOX::fleet_box_scroll_dn_button = dn_btn;
                MOX::fltpop_scroll_bar.up_btn = up_btn;
                MOX::fltpop_scroll_bar.dn_btn = dn_btn;

                MOX::fleet_movement_bar_field = fields::Add_Scroll_Field(
                    box_x + 0xc0, box_y + 0x42, 0, 0x66, 0, 0x66, 10, 0x66, &MOX::fltpop_scroll_bar.thumb_center_y, (char*)"", 0);
            }

            MOX::fleet_box_outpost_button = -1000;
            MOX::fleet_box_colonize_button = -1000;
            MOX::fleet_box_engage_button = -1000;
            MOX::fleet_box_transport_button = -1000;
            MOX::fleet_box_attack_button = -1000;

            Add_Added_Buttons(box_x, box_y);
            MOVEBOX::Add_Whole_Window_Field(2);
        }
    }

    int16_t __cdecl Fleet_Box_Grid_Height(int16_t grid_type) {
        if (grid_type == 1) {
            return 0x38;
        }

        if (grid_type == 2) {
            return 0x70;
        }

        if (grid_type == 3) {
            MOX::fleet_box_grid_sm_seg4 = buffer::Buffer_Reload("BUFFER0.LBX", 0x36, MOX::global_cache_seg);
            return animate::Get_Height((s_animation_header*)MOX::fleet_box_grid_sm_seg4);
        }

        return 0;
    }

    void __cdecl Set_Fleet_Movement_Box_Dimensions(int16_t count) {
        int32_t rows = (static_cast<int32_t>(count) - 1) / 3 + 1;
        if (rows > 3) {
            rows = 3;
        }

        MOX::fleet_box_map_height = Fleet_Box_Grid_Height(static_cast<int16_t>(rows));
        MOX::moveable_box[2].height = Fleet_Box_Height(count);
    }

    int16_t __cdecl Fleet_Box_Width(int16_t unused_ship_count) {
        (void)unused_ship_count;
        return animate::Get_Width(MOX::fleet_box.name_seg);
    }

    void __cdecl Reload_Fleet_Box_Data(void) {
        if (HACCESS::Get_Stack_Start_From_Saved_Ship_Array() == 0) {
            MOVEBOX::Vanish_Fleet_Movement_Box();
            return;
        }

        int16_t node_ptr = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);
        int16_t num_icons = FLEETPOP::N_Fleet_Icons(node_ptr);
        FLEETPOP::Fleet_Box_Data(num_icons);
    }

    void __cdecl Set_Fleet_Movement_Icon_Selections(int16_t state, uint8_t auto_select_flag) {
        for (int16_t i = 0; i < MOX::fleet_box.count; ++i) {
            int16_t node_idx = MOX::fleet_movement_icon[i].ship_node_idx;
            s_ship_node& node = MOX::ship_node[node_idx];
            int16_t ship_idx = node.ship_idx;

            if (HACCESS::Ship_Owner(ship_idx) == MOX::PLAYER_NUM) {
                if (state == 0) {
                    node.selected = 0;
                } else if (state == 1) {
                    if (auto_select_flag != 0) {
                        if (FLEETPOP::Ship_Should_Be_Auto_Selected(ship_idx)) {
                            node.selected = 1;
                        }
                    } else {
                        if (MAINSCR::Ship_Can_Be_Selected(ship_idx)) {
                            node.selected = 1;
                        }
                    }
                }
            }
        }

        MOX::fleet_movement_icons_on = static_cast<uint8_t>(state);
    }

    bool __cdecl Some_Icon_Selected() {
        int16_t selected_ship_ids[MAX_SHIPS];
        const int16_t selected_ship_capacity =
            (int16_t)(sizeof(selected_ship_ids) / sizeof(selected_ship_ids[0]));

        if (MOVEBOX::Moveable_Box_Selected(2)) {
            int16_t count = HACCESS::Get_Fleet_Box_Selected_Ship_Ids(
                selected_ship_ids, selected_ship_capacity, 255);
            return count > 0;
        }

        return false;
    }

    void __cdecl Reload_Ship_Selection_Status() {
        int16_t node_ptr = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);
        int16_t stack_owner = HACCESS::Ship_Stack_Owner(node_ptr);
        const int16_t ship_node_count =
            (int16_t)(sizeof(MOX::ship_node) / sizeof(MOX::ship_node[0]));

        if (MOX::PLAYER_NUM == stack_owner) {
            for (int16_t i = 0; i < ship_node_count; ++i) {
                int16_t ship_idx = MOX::ship_node[i].ship_idx;
                int16_t bit_set = struct_::Test_Bit_Field(MOX::fleet_icon_selection_status, ship_idx);

                bool is_selectable = false;
                if (bit_set != 0) {
                    if (MAINSCR::Ship_Can_Be_Selected(ship_idx)) {
                        is_selectable = true;
                    }
                }

                MOX::ship_node[i].selected = is_selectable ? 1 : 0;
            }
        }
    }

    int16_t __cdecl First_Unselected_Icon() {
        int16_t first_unselected_index = 0;
        bool found = false;

        if (MOX::fleet_box.count > 0) {
            for (int16_t i = 0; i < MOX::fleet_box.count; ++i) {
                if (found) {
                    break;
                }

                if (!HACCESS::Fleet_Movement_Icon_Selected(i)) {
                    first_unselected_index = i;
                    found = true;
                }
            }
        }

        return first_unselected_index;
    }

    void __cdecl Save_Ship_Selection_Status() {
        int16_t node_ptr = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);
        int16_t owner = HACCESS::Ship_Stack_Owner(node_ptr);

        if (MOX::PLAYER_NUM == owner) {
            for (int16_t i = 0; i < MOX::fleet_box.count; ++i) {
                int16_t ship_node_idx = MOX::fleet_movement_icon[i].ship_node_idx;

                if (ship_node_idx != -1) {
                    s_ship_node& node = MOX::ship_node[ship_node_idx];

                    if (node.selected == 0) {
                        struct_::Clear_Bit_Field(MOX::fleet_icon_selection_status, node.ship_idx);
                    } else {
                        struct_::Set_Bit_Field(MOX::fleet_icon_selection_status, node.ship_idx);
                    }
                }
            }
        }
    }

    int16_t __cdecl Fleet_Buttons_Chosen() {
        int16_t selected_count = 0;
        int16_t i = 0;

        if (MOX::fleet_box.count > 0) {
            do {
                int16_t node_idx = MOX::fleet_movement_icon[i].ship_node_idx;

                if (MOX::ship_node[node_idx].selected != 0) {
                    selected_count++;
                }

                i++;
            } while (i < MOX::fleet_box.count);
        }

        return selected_count;
    }

    bool __cdecl Can_Select_Fleet_Box_Ship(int16_t icon_index) {
        int16_t ship_node_idx = MOX::fleet_movement_icon[icon_index].ship_node_idx;
        int16_t owner_id = HACCESS::Ship_Stack_Owner(ship_node_idx);

        return owner_id == MOX::PLAYER_NUM;
    }

    int16_t __cdecl Icon_Selection_Status(int16_t icon_field_id) {
        int16_t first_icon_idx = MOX::first_fleet_movement_icon;
        int16_t limit_idx = MOX::fleet_icon_buttons_added + first_icon_idx;

        if (limit_idx > MOX::fleet_box.count) {
            limit_idx = MOX::fleet_box.count;
        }

        for (int16_t i = first_icon_idx; i < limit_idx; ++i) {
            if (MOX::fleet_movement_icon[i].field == icon_field_id) {
                int16_t ship_node_idx = MOX::fleet_movement_icon[i].ship_node_idx;
                return static_cast<int16_t>(MOX::ship_node[ship_node_idx].selected);
            }
        }

        return 0;
    }

    void __cdecl Clear_Ship_Node_Selection_Flags() {
        MOX::fleet_movement_icons_on = 0;
        const int16_t ship_node_count =
            (int16_t)(sizeof(MOX::ship_node) / sizeof(MOX::ship_node[0]));

        for (int16_t i = 0; i < ship_node_count; ++i) {
            MOX::ship_node[i].selected = 0;
        }
    }

    void __cdecl Reload_Fleet_Box_Palette(int16_t player_idx, uint32_t unused_is_not_player) {
        (void)unused_is_not_player;
        KEN::Load_Player_Ship_Palette(player_idx);
        MOX::mainscr_palette = 2;
    }

    bool __cdecl Ship_Should_Be_Auto_Selected(int16_t ship_idx) {
        if (MAINSCR::Ship_Can_Be_Selected(ship_idx) && MOX::just_moved_stack == 0) {
            return true;
        }
        return false;
    }

    void __cdecl Add_Added_Buttons(int16_t x_offset, int16_t y_offset) {
        int16_t btn_height;
        int16_t x_pos;
        int16_t y_base;
        int16_t stack_y_offset;
        s_animation_header* close_seg;
        s_animation_header* engage_seg;
        s_animation_header* colonize_seg;
        s_animation_header* transport_seg;
        s_animation_header* outpost_seg;
        s_animation_header* attack_seg;

        btn_height = animate::Get_Height(MOX::fleet_box_close_lg_button_seg);

        if (MOX::fleet_box.count > 9) {
            attack_seg = MOX::fleet_box_att_xenons_lg_button_seg;
            colonize_seg = MOX::fleet_box_colonize_lg_button_seg;
            transport_seg = MOX::fleet_box_transport_lg_button_seg;
            outpost_seg = MOX::fleet_box_outpost_lg_button_seg;
            engage_seg = MOX::fleet_box_engage_lg_button_seg;
            close_seg = MOX::fleet_box_close_lg_button_seg;
        } else {
            attack_seg = MOX::fleet_box_att_xenons_sm_button_seg;
            colonize_seg = MOX::fleet_box_colonize_sm_button_seg;
            transport_seg = MOX::fleet_box_transport_sm_button_seg;
            outpost_seg = MOX::fleet_box_outpost_sm_button_seg;
            engage_seg = MOX::fleet_box_engage_sm_button_seg;
            close_seg = MOX::fleet_box_close_sm_button_seg;
        }

        x_pos = x_offset + 2;
        y_base = (MOX::fleet_box.field8_0xe - btn_height) + y_offset;

        MOX::moveable_box[2].close_btn = fields::Add_Clipped_Button_Field(
            x_pos,
            y_base,
            (char*)"",
            close_seg,
            (char*)"\x1b", /* fmtpara 0x1B starts a color-attribute control sequence; here it is reused as a non-printing field label. */
            40);

        stack_y_offset = btn_height;

        if (MOX::fleet_box.outpost_btn_active != 0) {
            MOX::fleet_box_outpost_button = fields::Add_Clipped_Button_Field(x_pos, y_base + btn_height, (char*)"", outpost_seg, (char*)"", 40);
            stack_y_offset = btn_height * 2;
        }

        if (MOX::fleet_box.colonize_btn_active != 0) {
            MOX::fleet_box_colonize_button = fields::Add_Clipped_Button_Field(x_pos, y_base + stack_y_offset, (char*)"", colonize_seg, (char*)"", 40);
            stack_y_offset += btn_height;
        }

        if (MOX::fleet_box.engage_btn_active != 0) {
            MOX::fleet_box_engage_button = fields::Add_Clipped_Button_Field(x_pos, y_base + stack_y_offset, (char*)"", engage_seg, (char*)"", 40);
            stack_y_offset += btn_height;
        }

        if (MOX::fleet_box.transport_btn_active != 0) {
            MOX::fleet_box_transport_button = fields::Add_Clipped_Button_Field(x_pos, y_base + stack_y_offset, (char*)"", transport_seg, (char*)"", 40);
            stack_y_offset += btn_height;
        }

        if (MOX::fleet_box.attack_btn_active != 0) {
            MOX::fleet_box_attack_button = fields::Add_Clipped_Button_Field(x_pos, y_base + stack_y_offset, (char*)"", attack_seg, (char*)"", 40);
        }
    }

    int16_t __cdecl N_Fleet_Icons(int16_t node_idx) {
        int16_t count = 1;
        const int16_t ship_node_count =
            (int16_t)(sizeof(MOX::ship_node) / sizeof(MOX::ship_node[0]));

        while (MOX::ship_node[node_idx].next_node != -1 && count < ship_node_count) {
            count++;
            HACCESS::Ship_Owner(MOX::ship_node[node_idx].ship_idx);
            node_idx = MOX::ship_node[node_idx].next_node;
        }

        return count;
    }

    bool __cdecl At_Least_One_Ship_Of_Type_Selected(int16_t ship_type) {
        int16_t selected_ship_ids[MAX_SHIPS];
        const int16_t selected_ship_capacity =
            (int16_t)(sizeof(selected_ship_ids) / sizeof(selected_ship_ids[0]));

        int16_t count = HACCESS::Get_Fleet_Box_Selected_Ship_Ids(
            selected_ship_ids, selected_ship_capacity, ship_type);

        return count > 0;
    }

    void __cdecl Draw_Fleet_Movement_Box() {
        s_colors font_colors;
        char title[120];
        char race_or_ship_name[80];
        int16_t frame_out_value;
        int16_t icon_button_x;
        int16_t icon_button_y;

        strlcpy(title, "llllllll", sizeof(title));

        for (int16_t i = 0; i < 8; ++i) {
            if (i == 0) {
                font_colors.colors[i] = 0x6E;
            } else {
                font_colors.colors[i] = static_cast<uint8_t>(i + 0x71);
            }
        }

        const int16_t content_offset_x = MOX::moveable_box[2].content_offset_x;
        const int16_t content_offset_y = MOX::moveable_box[2].content_offset_y;
        const int16_t head_node_ptr = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);

        bool rebuild_saved_ship_array;
        if (head_node_ptr == MOX::first_node_ptr_in_stack_) {
            rebuild_saved_ship_array = false;
        } else {
            const int16_t fleet_icon_count =
                FLEETPOP::N_Fleet_Icons(HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack));

            FLEETPOP::Fleet_Box_Data(fleet_icon_count);

            int32_t scroll_rows = (fleet_icon_count + 2) / 3;
            if (scroll_rows <= 1) {
                scroll_rows = 1;
            }

            FLEETPOP::Initialize_Scroll_Bar(&MOX::fltpop_scroll_bar, static_cast<int16_t>(scroll_rows));
            rebuild_saved_ship_array = true;
            HACCESS::Clear_Fleet_Box_Selected_Ship_Status();
        }

        const int16_t stack_owner = HACCESS::Ship_Stack_Owner(head_node_ptr);
        if (stack_owner != MOX::ship_icon_player_id) {
            MOX::ship_icon_player_id = stack_owner;
        }

        FLEETPOP::Buffer_Reload_Fleetpop_Frame();

        const int16_t fleet_box_x = MOX::fleet_box.x;
        const int16_t fleet_box_y = MOX::fleet_box.y;
        const int16_t fleet_box_width = MOX::fleet_box.width;

        if (MOX::draw_expanding_box == 0) {
            if (MOX::fleet_box.is_small_frame == 0) {
                FLEETPOP::Draw_Large_Fleet_Box_Frame(
                    MOX::fleet_box.x,
                    MOX::fleet_box.y,
                    content_offset_x,
                    content_offset_y,
                    MOX::fleet_box.count,
                    &frame_out_value);
            } else {
                FLEETPOP::Draw_Small_Fleet_Box_Frame(
                    MOX::fleet_box.x,
                    MOX::fleet_box.y,
                    content_offset_x,
                    content_offset_y,
                    MOX::fleet_box.count,
                    &frame_out_value);
            }

            const int16_t title_origin_x = static_cast<int16_t>(fleet_box_x + content_offset_x);
            const int16_t title_origin_y = static_cast<int16_t>(fleet_box_y + content_offset_y);

            FLEETPOP::Draw_Fleet_Pop_Added_Buttons(title_origin_x, title_origin_y);

            if (stack_owner < 9) {
                MOX::mox_font_colors_offset = 0x71;
                MISC::Get_Mox_Font_Colors(4, 'o', 's', &font_colors);
                HACCESS::Race_Name(stack_owner, race_or_ship_name, sizeof(race_or_ship_name));
                snprintf(title, sizeof(title), HAROLD::H_Message(0x66), race_or_ship_name);
                fonts::Set_Font_Style_Outline(4, &font_colors);
                fonts::Set_Outline_Color(2, 1);
                fonts::Set_Font_Spacing(1);
            } else {
                MISC::Get_Mox_Font_Colors(4, 'K', 'N', &font_colors);
                strlcpy(title, MOX::ship[MOX::ship_node[head_node_ptr].ship_idx].d.name, sizeof(title));
                strings::String_To_Upper(title);
                fonts::Set_Font_Style_Outline(4, &font_colors);
                fonts::Set_Outline_Color(2, 1);
            }

            fonts::Print_Centered(
                static_cast<int16_t>(title_origin_x + fleet_box_width / 2),
                static_cast<int16_t>(title_origin_y + 14),
                title);
        }

        if ((head_node_ptr != MOX::first_node_ptr_in_stack_) || (MOX::first_node_ptr_in_stack_ == -1)) {
            memset(MOX::fleet_movement_icon, 0, sizeof(MOX::fleet_movement_icon));
            MOX::fleet_icon_buttons_added = 0;
        }

        MOX::first_fleet_movement_icon =
            static_cast<int16_t>(MOX::fltpop_scroll_bar.first_visible_row * 3);

        bool stop_building_icons = false;
        int16_t icon_slot = 0;
        int16_t current_node_ptr = head_node_ptr;
        const int16_t fleet_movement_icon_count =
            (int16_t)(sizeof(MOX::fleet_movement_icon) / sizeof(MOX::fleet_movement_icon[0]));

        if (head_node_ptr != MOX::first_node_ptr_in_stack_) {
            while (true) {
                if ((current_node_ptr == -1) || (icon_slot >= fleet_movement_icon_count)) {
                    stop_building_icons = true;
                }

                if (MOX::fleet_icon_buttons_added < 9) {
                    ++MOX::fleet_icon_buttons_added;
                }

                if (icon_slot >= fleet_movement_icon_count) {
                    break;
                }

                MOX::fleet_movement_icon[icon_slot].ship_node_idx = current_node_ptr;
                MOX::fleet_movement_icon[icon_slot].field = -1000;
                ++icon_slot;

                current_node_ptr = MOX::ship_node[current_node_ptr].next_node;
                if (current_node_ptr == -1) {
                    stop_building_icons = true;
                }

                if (!stop_building_icons) {
                    continue;
                }

                break;
            }
        }

        if ((MOX::first_time != 0) &&
            (MOX::settings.auto_select_ships != 0) &&
            (MOX::PLAYER_NUM == stack_owner)) {
            FLEETPOP::Set_Fleet_Movement_Icon_Selections(1, 1);
        }

        if (rebuild_saved_ship_array) {
            HACCESS::Build_Saved_Ship_Array();
        }

        int16_t visible_icon_end =
            static_cast<int16_t>(MOX::fleet_icon_buttons_added + MOX::first_fleet_movement_icon);
        if (visible_icon_end >= MOX::fleet_box.count) {
            visible_icon_end = MOX::fleet_box.count;
        }

        int16_t visible_icon_idx = MOX::first_fleet_movement_icon;
        int16_t visible_button_slot = 0;

        if ((visible_icon_end > visible_icon_idx) && (MOX::draw_expanding_box == 0)) {
            do {
                const int16_t ship_node_idx =
                    MOX::fleet_movement_icon[visible_icon_idx].ship_node_idx;
                const int16_t ship_idx = MOX::ship_node[ship_node_idx].ship_idx;

                MOX::fleet_movement_box_ptr[visible_button_slot] = visible_icon_idx;

                FLEETPOP::Get_Fleet_Icon_Button_XY(visible_button_slot, &icon_button_x, &icon_button_y);

                icon_button_x = static_cast<int16_t>(icon_button_x + content_offset_x + fleet_box_x);
                icon_button_y = static_cast<int16_t>(icon_button_y + content_offset_y + fleet_box_y);

                MOX::fleet_movement_icon[visible_icon_idx].x = icon_button_x;
                MOX::fleet_movement_icon[visible_icon_idx].y = icon_button_y;

                int16_t ship_picture_y_offset = 2;
                if (MOX::ship[ship_idx].d.size == SHIP_SIZE_DOOM_STAR) {
                    ship_picture_y_offset = 0;
                }

                int16_t picture_ship_idx;
                if (HACCESS::Fleet_Movement_Icon_Selected(visible_icon_idx)) {
                    int16_t selection_x_offset = 0;
                    if (MOX::fleet_box.is_small_frame == 1) {
                        selection_x_offset = 1;
                    }

                    animate::Draw(
                        static_cast<int16_t>(icon_button_x + selection_x_offset),
                        icon_button_y,
                        MOX::fleet_box_selection_seg);

                    picture_ship_idx =
                        MOX::ship_node[MOX::fleet_movement_icon[visible_icon_idx].ship_node_idx].ship_idx;
                } else {
                    picture_ship_idx =
                        MOX::ship_node[MOX::fleet_movement_icon[visible_icon_idx].ship_node_idx].ship_idx;
                }

                animate::Draw(
                    static_cast<int16_t>(icon_button_x + 2),
                    static_cast<int16_t>(icon_button_y + ship_picture_y_offset),
                    KEN::Get_Ship_Id_Picture_Seg(picture_ship_idx));

                if (MOX::PLAYER_NUM == stack_owner) {
                    const int16_t fleet_ship_id =
                        HACCESS::Fleet_Movement_Ship_Id(visible_icon_idx);

                    if (HACCESS::Is_Combat_Ship(fleet_ship_id)) {
                        FLEETPOP::Draw_Damage_Bars(
                            static_cast<int16_t>(MOX::fleet_movement_icon[visible_icon_idx].x - 1),
                            MOX::fleet_movement_icon[visible_icon_idx].y,
                            0x35,
                            0x35,
                            4,
                            fleet_ship_id);
                    }
                }

                ++visible_button_slot;
                ++visible_icon_idx;
            } while (visible_icon_idx < visible_icon_end);
        }

        KEN::Load_Player_Ship_Palette(
            static_cast<int16_t>(
                static_cast<int8_t>(MOX::ship[MOX::ship_node[head_node_ptr].ship_idx].owner)));

        MOX::first_node_ptr_in_stack_ = head_node_ptr;

        if (MOX::draw_expanding_box == 0) {
            FLEETPOP::Print_Fleet_Box_Info();

            if (MOX::fleet_box.count > 9) {
                SCROLL::Draw_Generic_Vertical_Scroll_Bar(&MOX::fltpop_scroll_bar, Fleet_Movement_Scroll_Callback);
            }
        }

        MOX::first_time = 0;

        if (MOX::draw_expanding_box != 0) {
            const int16_t ship_icon_id =
                HACCESS::Ship_Icon_Id_From_Node_Ptr(MOX::first_node_ptr_in_stack_);

            MOX::draw_expanding_box = 0;

            MAINSCR::Draw_Exploding_Plasma_Box(
                static_cast<int16_t>(MOX::ship_icon[ship_icon_id].x + 3),
                static_cast<int16_t>(MOX::ship_icon[ship_icon_id].y + 3),
                MOX::moveable_box[2].x,
                MOX::moveable_box[2].y,
                MOX::moveable_box[2].width,
                MOX::moveable_box[2].height,
                2);

            FLEETPOP::Draw_Fleet_Movement_Box();
        }

        if (MOX::add_help_fields == 0) {
            MOX::add_help_fields = 0xFF;
            EVANHELP::Set_Main_Screen_Help_List();
        }
    }

    void __cdecl Set_Added_Button_Stuff() {
        int16_t added_buttons_height = 0;

        FLEETPOP::n_added_buttons = 1;

        if (FLEETPOP::Check_For_Colonization_Button()) {
            added_buttons_height = animate::Get_Height(MOX::fleet_box_colonize_lg_button_seg);
            ++FLEETPOP::n_added_buttons;
        }

        if (FLEETPOP::Check_For_Transport_Troops_Button()) {
            added_buttons_height = static_cast<int16_t>(
                static_cast<uint16_t>(added_buttons_height) +
                static_cast<uint16_t>(animate::Get_Height(MOX::fleet_box_transport_lg_button_seg)));
            ++FLEETPOP::n_added_buttons;
        }

        if (FLEETPOP::Check_For_Build_Outpost_Button()) {
            added_buttons_height = static_cast<int16_t>(
                static_cast<uint16_t>(added_buttons_height) +
                static_cast<uint16_t>(animate::Get_Height(MOX::fleet_box_outpost_lg_button_seg)));
            ++FLEETPOP::n_added_buttons;
        }

        if (FLEETPOP::Check_For_Engage_Enemy_Forces_Button()) {
            added_buttons_height = static_cast<int16_t>(
                static_cast<uint16_t>(added_buttons_height) +
                static_cast<uint16_t>(animate::Get_Height(MOX::fleet_box_engage_lg_button_seg)));
            ++FLEETPOP::n_added_buttons;
        }

        if (FLEETPOP::Check_For_Attack_Antares_Button()) {
            added_buttons_height = static_cast<int16_t>(
                static_cast<uint16_t>(added_buttons_height) +
                static_cast<uint16_t>(animate::Get_Height(MOX::fleet_box_att_xenons_lg_button_seg)));
            ++FLEETPOP::n_added_buttons;
        }

        MOX::fleet_box.height = static_cast<int16_t>(
            static_cast<uint16_t>(MOX::fleet_box.field8_0xe) +
            static_cast<uint16_t>(added_buttons_height));

        FLEETPOP::Set_Fleet_Box_Data_Dimensions();
    }

    void __cdecl Get_Fleet_Icon_Button_XY(int16_t fleet_icon_index, int16_t* out_x, int16_t* out_y) {
        *out_x = static_cast<int16_t>((fleet_icon_index % 3) * 58 + 14);
        *out_y = static_cast<int16_t>((fleet_icon_index / 3) * 56 + 38);
    }

    void __cdecl Draw_Small_Fleet_Box_Frame(
        int16_t x,
        int16_t y,
        int16_t x_offset,
        int16_t y_offset,
        int16_t fleet_count,
        int16_t* current_y) {
        int32_t grid_rows = (((int32_t)fleet_count - 1) / 3) + 1;
        if (grid_rows >= 3) {
            grid_rows = 3;
        }

        const int16_t left_x = static_cast<int16_t>(x + x_offset);
        const int16_t right_x = static_cast<int16_t>(left_x + MOX::fleet_box.width);
        const int16_t start_y = static_cast<int16_t>(y + y_offset);
        int16_t saved_window_status = 0;

        *current_y = start_y;

        MISC::Save_Window_Status(&saved_window_status);

        int32_t clipped_bottom = (int32_t)MOX::fleet_box.field8_0xe + (int32_t)y_offset + (int32_t)y;
        if (clipped_bottom >= 0x1A5) {
            clipped_bottom = 0x1A5;
        }

        graphics::Set_Window(left_x, start_y, right_x, static_cast<int16_t>(clipped_bottom));
        video::Clip_On();
        graphics::Fill(
            static_cast<int16_t>(left_x + 6),
            static_cast<int16_t>(start_y + 0x29),
            static_cast<int16_t>(right_x - 6),
            static_cast<int16_t>(start_y + MOX::fleet_box.field8_0xe + 0x1D),
            0);
        MISC::Restore_Window_Status(saved_window_status);

        *current_y = start_y;

        s_animation_header* top_frame =
            (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x34, MOX::global_cache_seg);
        animate::Draw(left_x, *current_y, top_frame);
        *current_y = static_cast<int16_t>(*current_y + animate::Get_Height(top_frame));

        (void)animate::Get_Height(top_frame);

        const int16_t grid_height =
            FLEETPOP::Fleet_Box_Grid_Height(static_cast<int16_t>(grid_rows));

        clipped_bottom = (int32_t)(*current_y) + (int32_t)grid_height;
        if (clipped_bottom >= 0x1A5) {
            clipped_bottom = 0x1A5;
        }

        MISC::Save_Window_Status(&saved_window_status);
        graphics::Set_Window(left_x, *current_y, right_x, static_cast<int16_t>(clipped_bottom));
        video::Clip_On();

        MOX::fleet_box_grid_sm_seg4 =
            buffer::Buffer_Reload("BUFFER0.LBX", 0x36, MOX::global_cache_seg);
        animate::Draw(left_x, *current_y, (s_animation_header*)MOX::fleet_box_grid_sm_seg4);

        MISC::Restore_Window_Status(saved_window_status);

        *current_y = static_cast<int16_t>(*current_y + grid_height);

        s_animation_header* bottom_frame =
            (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x38, MOX::global_cache_seg);
        animate::Draw(left_x, *current_y, bottom_frame);
        *current_y = static_cast<int16_t>(*current_y + animate::Get_Height(bottom_frame));
    }

    void __cdecl Fleet_Box_Data(int16_t ship_count) {
        game_events::Publish(game_events::FLEET_BOX_INFO_CLEARED);
        if (MOX::first_time != 0) {
            MOX::fleet_box_scroll_first_ship = 0;
            MOX::fleet_box.mouse_pressed = 0;
        }

        if (ship_count > 9) {
            MOX::fleet_box.is_small_frame = 0;

            MOX::fleet_box_bottom_lg_seg =
                (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x37, MOX::global_cache_seg);
            MOX::fleet_box.bottom_seg = MOX::fleet_box_bottom_lg_seg;

            MOX::fleet_box_name_lg_seg =
                (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x33, MOX::global_cache_seg);
            MOX::fleet_box.x = MOX::moveable_box[2].x;
            MOX::fleet_box.y = MOX::moveable_box[2].y;
            MOX::fleet_box.name_seg = MOX::fleet_box_name_lg_seg;

            MOX::fleet_box.width = Fleet_Box_Width(ship_count);
            MOX::fleet_box.height = Fleet_Box_Height(ship_count);

            MOX::fleet_box_grid_lg_seg =
                (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x35, MOX::global_cache_seg);
            MOX::fleet_box.map_height = animate::Get_Height(MOX::fleet_box_grid_lg_seg);

            MOX::fleet_box.field9_0x10 = static_cast<int16_t>(
                MOX::fleet_box.map_height + animate::Get_Height(MOX::fleet_box.name_seg) + 4);
        } else {
            MOX::fleet_box.is_small_frame = 1;

            MOX::fleet_box_bottom_sm_seg =
                (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x38, MOX::global_cache_seg);
            MOX::fleet_box.bottom_seg = MOX::fleet_box_bottom_sm_seg;

            MOX::fleet_box_name_sm_seg =
                (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x34, MOX::global_cache_seg);
            MOX::fleet_box.name_seg = MOX::fleet_box_name_sm_seg;

            Set_Fleet_Movement_Box_Dimensions(ship_count);

            MOX::fleet_box.x = MOX::moveable_box[2].x;
            MOX::fleet_box.y = MOX::moveable_box[2].y;
            MOX::fleet_box.width = Fleet_Box_Width(ship_count);
            MOX::fleet_box.height = Fleet_Box_Height(ship_count);
            MOX::fleet_box.map_height = MOX::fleet_box_map_height;

            MOX::fleet_box.field9_0x10 = static_cast<int16_t>(
                animate::Get_Height(MOX::fleet_box.name_seg) + MOX::fleet_box.map_height + 4);
        }

        MOX::fleet_box.count = ship_count;
        MOX::fleet_box.field8_0xe = MOX::fleet_box.height;

        if (MOX::fleet_box.mouse_pressed == 0) {
            Set_Added_Button_Stuff();
        }
    }

    void __cdecl Print_Fleet_Box_Info() {
        game_events::Publish(game_events::FLEET_BOX_INFO_CLEARED);
        int16_t info_value = -1;
        int16_t destination_star = -1;
        int16_t info_type = 9;
        bool use_primary_colors = true;
        char text[80];
        s_colors font_colors;
        s_colors fitted_colors;

        const int16_t ship_idx = HACCESS::Fleet_Movement_Ship_Id(0);
        const bool icon_selected = FLEETPOP::Some_Icon_Selected();

        int16_t max_width;
        int16_t text_y;

        if (MOX::fleet_box.count > 9) {
            max_width = 0x84;
            text_y = 0x00d6;
        } else {
            max_width = 0x70;
            text_y = static_cast<int16_t>(
                MOX::fleet_box.map_height +
                animate::Get_Height(MOX::fleet_box.name_seg) +
                10);
        }

        const int16_t text_x = static_cast<int16_t>(
            MOX::fleet_box.x +
            MOX::moveable_box[2].content_offset_x +
            (MOX::fleet_box.is_small_frame == 1 ? 0x80 : 0x8a));

        text_y = static_cast<int16_t>(
            MOX::moveable_box[2].content_offset_y +
            MOX::moveable_box[2].y +
            text_y);

        if (MOX::eta_star_id == -1) {
            if (MOX::fleet_ship_scanned != -1) {
                info_type = 0;
            } else if (!icon_selected) {
                if (MOX::fleet_ship_scanned == -1) {
                    if (MAINSCR::fleet_icon_chosen != -1) {
                        MOX::fleet_ship_scanned = MAINSCR::fleet_icon_chosen;
                    }

                    if (MOX::fleet_ship_scanned == -1) {
                        if (MOX::ship[ship_idx].status == 0) {
                            info_type = 1;
                            info_value = MOX::ship[ship_idx].location;
                        } else {
                            FLEETPOP::Get_Absolute_Location_Scan_Info(
                                ship_idx,
                                &info_value,
                                &destination_star,
                                &info_type);
                        }
                    }
                }
            } else {
                if (MOX::ship[ship_idx].status == 0) {
                    info_type = 1;
                    info_value = MOX::ship[ship_idx].location;
                } else {
                    FLEETPOP::Get_Absolute_Location_Scan_Info(
                        ship_idx,
                        &info_value,
                        &destination_star,
                        &info_type);
                }
            }
        } else if ((MOX::eta_star_id > -1) && icon_selected) {
            if (MOX::g_ship_move_info.moving != 0) {
                if (MOX::g_ship_move_info.turns_left == 0) {
                    info_type = 1;
                    info_value = MOX::eta_star_id;
                } else if (HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, MOX::eta_star_id)) {
                    info_type = 2;
                    info_value = static_cast<uint8_t>(MOX::g_ship_move_info.turns_left);
                    destination_star = MOX::eta_star_id;
                } else {
                    info_type = 3;
                    info_value = static_cast<uint8_t>(MOX::g_ship_move_info.turns_left);
                }
            } else if (MOX::g_ship_move_info.blackhole_blocks != 0) {
                info_type = 4;
            } else if (MOX::g_ship_move_info.immobile != 0) {
                info_type = 5;
            } else if (MOX::g_ship_move_info.out_of_range != 0) {
                if (HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, MOX::eta_star_id)) {
                    info_type = 6;
                    info_value = static_cast<uint8_t>(MOX::g_ship_move_info.parsecs);
                    destination_star = MOX::eta_star_id;
                } else {
                    info_type = 7;
                    info_value = static_cast<uint8_t>(MOX::g_ship_move_info.parsecs);
                }
            } else if (MOX::g_ship_move_info.hyperspace_flux != 0) {
                info_type = 8;
            }
        }

        if (MOX::eta_star_id == -1 && (info_type == 2 || info_type == 3)) {
            const game_events::s_fleet_box_info observation = {ship_idx, static_cast<uint8_t>(info_value), destination_star};
            game_events::Publish_Fleet_Box_Info(&observation);
        }

        switch (info_type) {
            case 0:
                FLEETPOP::Get_Fleetpop_Scanned_Ship_Info(text, sizeof(text));
                break;

            case 1:
                snprintf(text, sizeof(text), HAROLD::H_Message(0x67), MOX::star[info_value].name);
                break;

            case 2:
                if (MOX::NUM_STARS == destination_star) {
                    strlcpy(text, HAROLD::H_Message(0x68), sizeof(text));
                } else if (info_value == 1) {
                    snprintf(text, sizeof(text), HAROLD::H_Message(0x69), 1, MOX::star[destination_star].name);
                } else {
                    snprintf(text, sizeof(text), HAROLD::H_Message(0x6a), info_value, MOX::star[destination_star].name);
                }
                break;

            case 3:
                if (info_value == 1) {
                    snprintf(text, sizeof(text), HAROLD::H_Message(0x6b), info_value);
                } else {
                    snprintf(text, sizeof(text), HAROLD::H_Message(0x6c), info_value);
                }
                break;

            case 4:
                strlcpy(text, HAROLD::H_Message(0x6d), sizeof(text));
                use_primary_colors = false;
                break;

            case 5:
                strlcpy(text, HAROLD::H_Message(0x6e), sizeof(text));
                use_primary_colors = false;
                break;

            case 6:
                if (info_value == 1) {
                    snprintf(text, sizeof(text), HAROLD::H_Message(0x6f), 1, MOX::star[destination_star].name);
                } else {
                    snprintf(text, sizeof(text), HAROLD::H_Message(0x70), info_value, MOX::star[destination_star].name);
                }
                use_primary_colors = false;
                break;

            case 7:
                if (info_value == 0) {
                    strlcpy(text, HAROLD::H_Message(0x71), sizeof(text));
                } else {
                    snprintf(text, sizeof(text), HAROLD::H_Message(0xeb), info_value);
                }
                use_primary_colors = false;
                break;

            case 8:
                strlcpy(text, HAROLD::H_Message(0x73), sizeof(text));
                use_primary_colors = false;
                break;

            case 9:
            default:
                text[0] = '\0';
                break;
        }

        if (text[0] == '\0') {
            return;
        }

        MISC::Set_Fitted_Font_Style(text, &fitted_colors, 3, 2, 0, max_width, 1);

        if (use_primary_colors) {
            MOX::mox_font_colors_offset = 0x71;
            MISC::Get_Mox_Font_Colors(fonts::current_font_style, 0x70, 0x73, &font_colors);
        } else {
            MISC::Get_Mox_Font_Colors(fonts::current_font_style, 0x4b, 0x50, &font_colors);
        }

        MISC::Set_Font_Colors(&font_colors);

        if (fonts::current_font_style == 1) {
            text_y = static_cast<int16_t>(text_y + 2);
        }

        const int16_t string_width = fonts::Get_String_Width(text);
        if (string_width > max_width) {
            ERIC::Print_Paragraph_Centered(
                text_x - max_width / 2,
                text_y - 6,
                max_width,
                0x14,
                text);
            return;
        }

        fonts::Print_Centered(text_x, text_y, text);
    }

    void __cdecl Draw_Fleet_Pop_Added_Buttons(int16_t x, int16_t y) {
        s_animation_header* colonize_seg;
        s_animation_header* engage_seg;
        s_animation_header* transport_seg;
        s_animation_header* attack_seg;
        s_animation_header* outpost_seg;
        s_animation_header* close_seg;

        const int16_t button_height = animate::Get_Height(MOX::fleet_box_close_lg_button_seg);

        if (MOX::fleet_box.count > 9) {
            attack_seg = MOX::fleet_box_att_xenons_lg_button_seg;
            colonize_seg = MOX::fleet_box_colonize_lg_button_seg;
            transport_seg = MOX::fleet_box_transport_lg_button_seg;
            outpost_seg = MOX::fleet_box_outpost_lg_button_seg;
            close_seg = MOX::fleet_box_close_lg_button_seg;
            engage_seg = MOX::fleet_box_engage_lg_button_seg;
        } else {
            attack_seg = MOX::fleet_box_att_xenons_sm_button_seg;
            colonize_seg = MOX::fleet_box_colonize_sm_button_seg;
            transport_seg = MOX::fleet_box_transport_sm_button_seg;
            outpost_seg = MOX::fleet_box_outpost_sm_button_seg;
            close_seg = MOX::fleet_box_close_sm_button_seg;
            engage_seg = MOX::fleet_box_engage_sm_button_seg;
        }

        const int16_t base_y = static_cast<int16_t>(MOX::fleet_box.field8_0xe - button_height + y);

        animate::Reset_Animation_Frame(close_seg);
        x = static_cast<int16_t>(x + 2);
        animate::Draw(x, base_y, close_seg);

        int16_t next_offset = button_height;

        if (MOX::fleet_box.outpost_btn_active != 0) {
            animate::Reset_Animation_Frame(outpost_seg);
            animate::Draw(x, static_cast<int16_t>(base_y + button_height), outpost_seg);
            next_offset = static_cast<int16_t>(button_height * 2);
        }

        if (MOX::fleet_box.colonize_btn_active != 0) {
            animate::Reset_Animation_Frame(colonize_seg);
            animate::Draw(x, static_cast<int16_t>(base_y + next_offset), colonize_seg);
            next_offset = static_cast<int16_t>(next_offset + button_height);
        }

        if (MOX::fleet_box.engage_btn_active != 0) {
            animate::Reset_Animation_Frame(engage_seg);
            animate::Draw(x, static_cast<int16_t>(base_y + next_offset), engage_seg);
            next_offset = static_cast<int16_t>(next_offset + button_height);
        }

        if (MOX::fleet_box.transport_btn_active != 0) {
            animate::Reset_Animation_Frame(transport_seg);
            animate::Draw(x, static_cast<int16_t>(base_y + next_offset), transport_seg);
            next_offset = static_cast<int16_t>(next_offset + button_height);
        }

        if (MOX::fleet_box.attack_btn_active != 0) {
            animate::Reset_Animation_Frame(attack_seg);
            animate::Draw(x, static_cast<int16_t>(base_y + next_offset), attack_seg);
        }
    }

    void __cdecl Draw_Large_Fleet_Box_Frame(
        int16_t x,
        int16_t y,
        int16_t x_offset,
        int16_t y_offset,
        uint32_t unused_fleet_count,
        int16_t* current_y) {
        int16_t window_status;
        int16_t fleet_box_inner_height;
        s_animation_header* segment;
        int16_t frame_left;
        int32_t window_bottom;

        frame_left = static_cast<int16_t>(x + x_offset);
        fleet_box_inner_height = MOX::fleet_box.field8_0xe;
        *current_y = static_cast<int16_t>(y_offset + y);

        MISC::Save_Window_Status(&window_status);

        window_bottom =
            static_cast<int32_t>(y) +
            static_cast<int32_t>(fleet_box_inner_height) +
            static_cast<int32_t>(y_offset);
        if (window_bottom >= 0x1A5) {
            window_bottom = 0x1A5;
        }

        graphics::Set_Window(
            frame_left,
            *current_y,
            static_cast<int16_t>(x + MOX::fleet_box.width + x_offset),
            static_cast<int16_t>(window_bottom));
        video::Clip_On();

        graphics::Fill(
            static_cast<int16_t>(frame_left + 6),
            static_cast<int16_t>(*current_y + 0x29),
            static_cast<int16_t>(frame_left + MOX::fleet_box.width - 6),
            static_cast<int16_t>(*current_y + fleet_box_inner_height + 0x1D),
            0);

        MISC::Restore_Window_Status(window_status);

        segment = (s_animation_header*)buffer::Buffer_Reload(
            "BUFFER0.LBX",
            0x33,
            MOX::global_cache_seg);
        animate::Draw(frame_left, *current_y, segment);
        *current_y = static_cast<int16_t>(*current_y + animate::Get_Height(segment));

        MOX::fleet_box_grid_lg_seg = (s_animation_header*)buffer::Buffer_Reload(
            "BUFFER0.LBX",
            0x35,
            MOX::global_cache_seg);
        animate::Draw(frame_left, *current_y, MOX::fleet_box_grid_lg_seg);
        *current_y = static_cast<int16_t>(*current_y + animate::Get_Height(MOX::fleet_box_grid_lg_seg));

        segment = (s_animation_header*)buffer::Buffer_Reload(
            "BUFFER0.LBX",
            0x37,
            MOX::global_cache_seg);
        animate::Draw(frame_left, *current_y, segment);
        *current_y = static_cast<int16_t>(*current_y + animate::Get_Height(segment));

        (void)unused_fleet_count;
    }

    void __cdecl Initialize_Scroll_Bar(s_scroll_bar* scroll_bar, int16_t total_rows) {
        scroll_bar->total_rows = total_rows;
        scroll_bar->visible_rows = 3;
        scroll_bar->track_len_px = 102;
        scroll_bar->first_visible_row = 0;

        scroll_bar->thumb_height_px = SCROLL::Get_Vertical_Scroll_Bar_Height(scroll_bar);
        scroll_bar->last_thumb_center_y = -1;
        scroll_bar->type = 2;
        scroll_bar->row_step = 1;
        scroll_bar->thumb_center_y =
            static_cast<int16_t>(scroll_bar->track_len_px - (scroll_bar->thumb_height_px / 2));

        MOX::scroll_bar = scroll_bar;
    }

    void __cdecl Buffer_Reload_Fleetpop_Frame() {
        s_animation_header* seg;

        if (MOX::fleet_box.is_small_frame == 0) {
            seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x37, MOX::global_cache_seg);
            MOX::fleet_box_bottom_lg_seg = seg;
            MOX::fleet_box.bottom_seg = seg;

            seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x33, MOX::global_cache_seg);
            MOX::fleet_box_name_lg_seg = seg;
            MOX::fleet_box.name_seg = seg;
            return;
        }

        seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x38, MOX::global_cache_seg);
        MOX::fleet_box_bottom_sm_seg = seg;
        MOX::fleet_box.bottom_seg = seg;

        seg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x34, MOX::global_cache_seg);
        MOX::fleet_box_name_sm_seg = seg;
        MOX::fleet_box.name_seg = seg;
    }

    void __cdecl Get_Fleetpop_Scanned_Ship_Info(char* out_text, uint32_t out_text_size) {
        if (out_text == nullptr || out_text_size == 0) {
            return;
        }

        out_text[0] = '\0';

        if (MOX::fleet_ship_scanned <= -1) {
            return;
        }

        const int16_t ship_idx = HACCESS::Fleet_Movement_Ship_Id(MOX::fleet_ship_scanned);
        const int16_t ship_owner = HACCESS::Ship_Owner(ship_idx);
        s_ship_data* const ship = &MOX::ship[ship_idx];
        const int16_t hull_size = static_cast<int8_t>(ship->d.size);

        if (MOX::PLAYER_NUM == ship_owner) {
            strlcpy(out_text, ship->d.name, out_text_size);
            return;
        }

        if (ship_owner <= 8) {
            if (HACCESS::Is_Combat_Ship(ship_idx)) {
                strlcpy(out_text, TECHDATA::hull_data[hull_size].name, out_text_size);
                return;
            }
        }

        strlcpy(out_text, ship->d.name, out_text_size);
    }

    bool __cdecl Check_For_Colonization_Button() {
        const uint8_t colonize_button_flag =
            FLEETPOP::Set_Colonize_Button_Flag(MOX::fleet_box.count);

        MOX::fleet_box.colonize_btn_active =
            static_cast<int8_t>(colonize_button_flag);

        return colonize_button_flag != 0;
    }

    bool __cdecl Check_For_Transport_Troops_Button(void) {
        const uint8_t transport_button_state =
            FLEETPOP::Set_Transport_Troops_Flag(MOX::fleet_box.count);

        MOX::fleet_box.transport_btn_active =
            static_cast<int8_t>(transport_button_state);

        return transport_button_state != 0;
    }

    bool __cdecl Check_For_Build_Outpost_Button() {
        const uint8_t outpost_button_flag =
            FLEETPOP::Set_Build_Outpost_Button_Flag(MOX::fleet_box.count);

        MOX::fleet_box.outpost_btn_active =
            static_cast<int8_t>(outpost_button_flag);

        return outpost_button_flag != 0;
    }

    bool __cdecl Check_For_Engage_Enemy_Forces_Button(void) {
        const uint8_t engage_btn_active =
            FLEETPOP::Set_Engage_Enemy_Forces_Flag(MOX::fleet_box.count);

        MOX::fleet_box.engage_btn_active =
            static_cast<int8_t>(engage_btn_active);

        return engage_btn_active != 0;
    }

    bool __cdecl Check_For_Attack_Antares_Button() {
        const uint8_t attack_btn_active = FLEETPOP::Set_Attack_Antares_Button_Flag();
        MOX::fleet_box.attack_btn_active = static_cast<int8_t>(attack_btn_active);
        return attack_btn_active != 0;
    }

    void __cdecl Set_Fleet_Box_Data_Dimensions(void) {
        MOX::moveable_box[2].width = MOX::fleet_box.width;
        MOX::moveable_box[2].height = MOX::fleet_box.height;

        if (MOX::fleet_box.width + MOX::fleet_box.x > 0x20f) {
            MOX::fleet_box.x = static_cast<int16_t>(0x20f - MOX::fleet_box.width);
            MOX::moveable_box[2].x = MOX::fleet_box.x;
        }

        if (MOX::fleet_box.y + MOX::fleet_box.height > 0x1a5) {
            MOX::fleet_box.y = static_cast<int16_t>(0x1a5 - MOX::fleet_box.height);
            MOX::moveable_box[2].y = MOX::fleet_box.y;
        }
    }

    void __cdecl Get_Absolute_Location_Scan_Info(
        int16_t ship_idx,
        int16_t* value,
        int16_t* star_idx,
        int16_t* info_type) {
        s_ship_data& ship = MOX::ship[ship_idx];
        int16_t absolute_location = HAROLD::Absolute_Location(ship.location);
        uint16_t turns_left = static_cast<uint16_t>(static_cast<uint8_t>(ship.turns_left));

        if (absolute_location != MOX::NUM_STARS) {
            if (!HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, absolute_location)) {
                *value = static_cast<int16_t>(turns_left);
                *info_type = 3;
                return;
            }
        }

        *value = static_cast<int16_t>(turns_left);
        *star_idx = absolute_location;
        *info_type = 2;
    }

    uint8_t __cdecl Set_Build_Outpost_Button_Flag(int16_t ship_count) {
        if (MOX::fleet_box_ship_stack == -1) {
            return 0;
        }

        int16_t current_node = MOX::ship_stack_start[MOX::fleet_box_ship_stack];
        int16_t ship_idx = MOX::ship_node[current_node].ship_idx;
        int16_t ship_owner = HACCESS::Ship_Owner(ship_idx);
        uint8_t can_build_outpost = 0;

        const int16_t location = MOX::ship[ship_idx].location;
        const bool valid_star_location =
            location >= 0 &&
            location < SHIP_LOCATION_MOVING_OFFSET &&
            location < MOX::NUM_STARS;

        if (MOX::PLAYER_NUM != ship_owner) {
            return can_build_outpost;
        }

        if (!valid_star_location) {
            return can_build_outpost;
        }

        if (MOX::star[location].colonize_player != MOX::PLAYER_NUM) {
            return can_build_outpost;
        }

        int16_t processed_count = 0;
        if (ship_count <= 0) {
            return can_build_outpost;
        }

        while (processed_count < ship_count) {
            if (can_build_outpost != 0) {
                break;
            }

            if (current_node <= -1) {
                break;
            }

            ship_idx = MOX::ship_node[current_node].ship_idx;
            if (MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_OUTPOST) {
                for (int16_t planet_slot = 0; planet_slot < 5; ++planet_slot) {
                    if (FLEETPOP::Can_Build_Outpost(MOX::star[location].planet_index[planet_slot]) != 0) {
                        can_build_outpost = 1;
                        break;
                    }
                }
            }

            current_node = MOX::ship_node[current_node].next_node;
            ++processed_count;
        }

        return can_build_outpost;
    }

    int8_t __cdecl Set_Transport_Troops_Flag(int16_t ship_count) {
        if (MOX::fleet_box_ship_stack == -1) {
            return 0;
        }

        int16_t current_node = MOX::ship_stack_start[MOX::fleet_box_ship_stack];
        int16_t first_ship_idx = MOX::ship_node[current_node].ship_idx;
        int16_t first_ship_owner = HACCESS::Ship_Owner(first_ship_idx);
        int8_t transport_troops_flag = 0;

        int16_t star_idx = MOX::ship[first_ship_idx].location;
        bool is_valid_star_location = false;
        if (star_idx > -1) {
            is_valid_star_location = (star_idx < SHIP_LOCATION_MOVING_OFFSET);
        }

        bool needs_xy_to_star = false;
        if (star_idx > SHIP_LOCATION_WORMHOLE_OFFSET) {
            needs_xy_to_star = (star_idx < SHIP_LOCATION_LIMIT);
        }

        if (needs_xy_to_star) {
            star_idx = HAROLD::XY_To_Star_Id(MOX::ship[first_ship_idx].x, MOX::ship[first_ship_idx].y);
        }

        if ((MOX::PLAYER_NUM == first_ship_owner) && is_valid_star_location) {
            int16_t processed_count = 0;
            if (ship_count > 0) {
                while (processed_count < ship_count) {
                    if (transport_troops_flag != 0) {
                        break;
                    }

                    if (current_node < 0) {
                        break;
                    }

                    const int16_t ship_idx = MOX::ship_node[current_node].ship_idx;
                    (void)HACCESS::Ship_Owner(ship_idx);

                    if (MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_TRANSPORT) {
                        for (int16_t planet_slot = 0; planet_slot < 5; ++planet_slot) {
                            const int16_t planet_idx = MOX::star[star_idx].planet_index[planet_slot];
                            if (planet_idx != -1) {
                                const int16_t colony_idx = MOX::planet[planet_idx].colony_index;
                                if (colony_idx != -1) {
                                    if (static_cast<int16_t>(MOX::colony[colony_idx].owner) == MOX::PLAYER_NUM) {
                                        transport_troops_flag = 1;
                                    }
                                }
                            }
                        }
                    }

                    current_node = MOX::ship_node[current_node].next_node;
                    ++processed_count;
                }
            }
        }

        return transport_troops_flag;
    }

    bool __cdecl Set_Attack_Antares_Button_Flag(void) {
        bool found_matching_ship = false;
        bool has_dimensional_portal = false;

        if (MOX::fleet_box_ship_stack != -1) {
            const int16_t selected_ship_idx =
                MOX::ship_node[MOX::ship_stack_start[MOX::fleet_box_ship_stack]].ship_idx;
            const int16_t star_idx =
                HAROLD::XY_To_Star_Id(MOX::ship[selected_ship_idx].x, MOX::ship[selected_ship_idx].y);

            if (star_idx != -1) {
                int16_t ship_slot = 0;

                if (MOX::NUM_SHIPS > 0) {
                    do {
                        if (MOX::ship[ship_slot].status < 3) {
                            // BUG: selected_ship_idx is used here instead of ship_slot.
                            if (HACCESS::Is_Combat_Ship(selected_ship_idx)) {
                                if (HAROLD::Absolute_Location(MOX::ship[selected_ship_idx].location) !=
                                    MOX::NUM_STARS) {
                                    if (MOX::ship[ship_slot].x == MOX::ship[selected_ship_idx].x) {
                                        if (MOX::ship[ship_slot].y == MOX::ship[selected_ship_idx].y) {
                                            found_matching_ship = true;
                                        }
                                    }
                                }
                            }
                        }

                        ++ship_slot;
                    } while (ship_slot < MOX::NUM_SHIPS && !found_matching_ship);
                }

                if (found_matching_ship) {
                    has_dimensional_portal =
                        HAROLD::Player_Has_Dimensional_Portal(
                            &MOX::player[MOX::ship[selected_ship_idx].owner],
                            &MOX::star[star_idx]);
                }
            }
        }

        return has_dimensional_portal;
    }

    int8_t __cdecl Set_Colonize_Button_Flag(int16_t ship_count) {
        if (MOX::fleet_box_ship_stack == -1) {
            return 0;
        }

        int16_t current_node_index = MOX::ship_stack_start[MOX::fleet_box_ship_stack];
        int16_t ship_idx = MOX::ship_node[current_node_index].ship_idx;
        int16_t ship_owner = HACCESS::Ship_Owner(ship_idx);
        int8_t colonize_button_flag = 0;
        bool found_colonizable_planet = false;

        int16_t star_index = MOX::ship[ship_idx].location;
        const bool is_at_star = (star_index > -1) && (star_index < SHIP_LOCATION_MOVING_OFFSET);
        const bool is_in_transit_to_star = (star_index > SHIP_LOCATION_WORMHOLE_OFFSET) && (star_index < SHIP_LOCATION_LIMIT);

        if (is_in_transit_to_star) {
            star_index -= SHIP_LOCATION_WORMHOLE_OFFSET;
        }

        if ((ship_owner == MOX::PLAYER_NUM) &&
            (is_at_star || is_in_transit_to_star) &&
            (MOX::star[star_index].colonize_player == MOX::PLAYER_NUM) &&
            (ship_count > 0)) {
            int16_t processed_count = 0;

            while (processed_count < ship_count) {
                if (colonize_button_flag != 0) {
                    break;
                }

                if (current_node_index < 0) {
                    break;
                }

                ship_idx = MOX::ship_node[current_node_index].ship_idx;
                HACCESS::Ship_Owner(ship_idx);

                if (MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_COLONY) {
                    int16_t planet_slot = 0;

                    if (!found_colonizable_planet) {
                        while (planet_slot < 5) {
                            if (found_colonizable_planet) {
                                break;
                            }

                            if (FLEETPOP::Planet_Is_Colonizable(MOX::star[star_index].planet_index[planet_slot]) != 0) {
                                found_colonizable_planet = true;
                                colonize_button_flag = 1;
                            }

                            ++planet_slot;
                        }
                    }
                }

                current_node_index = MOX::ship_node[current_node_index].next_node;
                ++processed_count;
            }
        }

        return colonize_button_flag;
    }

    uint8_t __cdecl Can_Build_Outpost(int16_t planet_idx) {
        if (planet_idx <= -1) {
            return 0;
        }

        if (MOX::planet[planet_idx].colony_index != -1) {
            return 0;
        }

        if (HAROLD::Planet_Has_Outpost(planet_idx) != 0) {
            return 0;
        }

        return 1;
    }

    int8_t __cdecl Planet_Is_Colonizable(int16_t planet_idx) {
        int8_t is_colonizable = 0;

        if (planet_idx > -1) {
            s_planet_data* planet = &MOX::planet[planet_idx];
            if (planet->planet_type == PLANET_TYPE_PLANET) {
                int16_t colony_idx = planet->colony_index;
                if (colony_idx == -1 || MOX::colony[colony_idx].outpost_flag != 0) {
                    is_colonizable = 1;
                }
            }
        }

        return is_colonizable;
    }

    int8_t __cdecl Set_Engage_Enemy_Forces_Flag(int16_t ship_count) {
        bool has_owned_combat_ship = false;

        if (MOX::fleet_box_ship_stack == -1) {
            return 0;
        }

        int8_t engage_enemy_forces = 0;
        int16_t stack_node_idx = MOX::ship_stack_start[MOX::fleet_box_ship_stack];
        int16_t primary_ship_idx = MOX::ship_node[stack_node_idx].ship_idx;
        int16_t primary_owner = HACCESS::Ship_Owner(primary_ship_idx);
        int16_t star_idx = MOX::ship[primary_ship_idx].location;

        const bool is_at_star = (star_idx > -1) && (star_idx < SHIP_LOCATION_MOVING_OFFSET);
        const bool is_at_xy_star = (star_idx > SHIP_LOCATION_WORMHOLE_OFFSET) && (star_idx < SHIP_LOCATION_LIMIT);

        if (is_at_xy_star) {
            star_idx = HAROLD::XY_To_Star_Id(MOX::ship[primary_ship_idx].x, MOX::ship[primary_ship_idx].y);
        }

        if ((MOX::PLAYER_NUM == primary_owner) && (is_at_star || is_at_xy_star)) {
            for (int16_t i = 0; i < ship_count; ++i) {
                if (has_owned_combat_ship) {
                    break;
                }

                if (stack_node_idx < 0) {
                    break;
                }

                const int16_t ship_idx = MOX::ship_node[stack_node_idx].ship_idx;
                if ((MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_COMBAT) &&
                    (MOX::ship[ship_idx].status < 3) &&
                    (HACCESS::Ship_Owner(ship_idx) == MOX::PLAYER_NUM)) {
                    has_owned_combat_ship = true;
                }

                stack_node_idx = MOX::ship_node[stack_node_idx].next_node;
            }

            if (has_owned_combat_ship) {
                if (((MOX::star[star_idx].ignore_combat_ships >>
                         (static_cast<uint8_t>(MOX::PLAYER_NUM) & 0x1f)) &
                        1) != 0) {
                    engage_enemy_forces = 1;
                }
            }
        }

        return engage_enemy_forces;
    }

} // namespace FLEETPOP