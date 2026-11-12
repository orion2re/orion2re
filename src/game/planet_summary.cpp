#include "pch.h"

namespace PLNTSUM {
    s_colors hi_neutral_colors = {{0x40, 0xFF, 0x43, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    s_colors red_colors = {{0xCF, 0xD3, 0xD2, 0xD3, 0xD3, 0xD3, 0xD3, 0xD3}};
    s_colors green_colors = {{0xDB, 0xDE, 0xDD, 0xDE, 0xDE, 0xDE, 0xDE, 0xDE}};

    int16_t plntscrn_delta_y[9] = {0, 55, 109, 164, 219, 274, 329, 383, 479};
    int16_t sort_choice;
    int16_t filter_out_adverse_gravity_planets;
    int16_t filter_out_hostile_environment_planets;
    int16_t filter_out_mineral_scarcity_planets;
    int16_t filter_out_enemy_controlled_planets;
    int16_t filter_out_planets_out_of_range = 1;

    s_plntsum_screen* screen;
    s_plntsum_planet* planet_summary;
    s_plntsum_ship* ship_summary;
    int16_t n_ships_in_summary;
    int8_t can_reach_star[MAX_STARS];
    uint8_t explored_bitmap[BITMAP(MAX_PLANETS)];

    uint8_t found_outpost_ship;
    uint8_t draw_colony_button_status;
    int16_t send_choice;
    uint8_t found_colony_ship;
    uint8_t draw_outpost_button_status;
    int16_t plntsum_scroll_bar;
    uint32_t planet_summary_reserved_counter;
    uint8_t planet_summary_refresh_marker;

    int16_t last_ship_moved;
    const char* PTR_s_PLNTSUM_LBX_00556AC0 = "PLNTSUM.LBX";
    s_animation_header* send_colony_ship_button_seg;
    s_animation_header* send_outpost_ship_button_seg;

    bool can_reach_scanned_planet;
    int8_t box_field_scanned;
    bool recalc_first_planet;
    uint8_t fade_flag;
    uint8_t planet_summary_state_marker;
    uint8_t skip_draw_planets;

    s_animation_header** p_rotating_planet;

    void __cdecl Add_Planet_Summary_Bar_Field() {
        plntsum_scroll_bar = fields::Add_Scroll_Field(
            0x1a2, // 418
            0x25,  // 37
            0,
            0x197, // 407
            0,
            0x197, // 407
            0xf,   // 15
            0x197, // 407
            (short*)MOX::scroll_bar,
            "",
            0);
    }

    bool __cdecl Summary_Found_Colony_Ship(int16_t ship_summary_idx) {
        if (ship_summary[ship_summary_idx].colony_ship_idx != -1 && ship_summary[ship_summary_idx].colony_available) {
            return true;
        }
        return false;
    }

    bool __cdecl Summary_Found_Outpost_Ship(int16_t ship_summary_idx) {
        if (ship_summary[ship_summary_idx].outpost_ship_idx != -1 && ship_summary[ship_summary_idx].output_available) {
            return true;
        }
        return false;
    }

    bool __cdecl Send_Colony_Button_Selectable() {
        return found_colony_ship != 0;
    }

    bool __cdecl Send_Outpost_Button_Selectable() {
        return found_outpost_ship != 0;
    }

    bool __cdecl Planet_Is_Outpost_Planet(int16_t planet_idx) {
        int16_t colony_idx = MOX::planet[planet_idx].colony_index;

        if (colony_idx > -1) {
            return MOX::colony[colony_idx].outpost_flag != 0;
        }

        return false;
    }

    bool __cdecl Planet_Has_Players_Colony(int16_t planet_idx) {
        int16_t owner_id = -1;
        int16_t colony_index = MOX::planet[planet_idx].colony_index;

        if (colony_index != -1) {
            owner_id = (int16_t)MOX::colony[colony_index].owner;
        }

        return owner_id == MOX::PLAYER_NUM;
    }

    bool __cdecl Planet_Has_Been_Visited(int16_t planet_index) {
        bool visited = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, MOX::planet[planet_index].star_index);
        bool omniscient = HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);

        if (omniscient || visited) {
            return true;
        }

        return false;
    }

    bool __cdecl Planet_In_Range_Of_A_Colony(int16_t planet_idx) {
        int16_t star_idx = MOX::planet[planet_idx].star_index;
        s_star_data* star = &MOX::star[star_idx];

        return SHIPMOVE::Star_In_Extended_Range_Of_Player(star, MOX::PLAYER_NUM);
    }

    bool __cdecl Planet_Has_Adverse_Gravity(int16_t planet_idx, int16_t player_idx) {
        int16_t gravity_class = static_cast<int16_t>(MOX::planet[planet_idx].gravity_class);

        // Productivity_Penalty returns a positive value if there is an adverse effect
        int16_t penalty = Productivity_Penalty(player_idx, gravity_class);

        return penalty > 0;
    }

    bool __cdecl Planet_Is_Mineral_Scarce(int16_t planet_idx) {
        if (MOX::planet[planet_idx].mineral_class == MINERAL_RESOURCE_POOR ||
            MOX::planet[planet_idx].mineral_class == MINERAL_RESOURCE_ULTRA_POOR) {
            return true;
        }
        return false;
    }

    bool __cdecl Planet_Has_Hostile_Environment(int16_t planet_idx) {
        return MOX::planet[planet_idx].food_per_farmer <= 0;
    }

    bool __cdecl Planet_Is_Enemy_Controlled(int16_t planet_idx, int16_t) {
        bool result = false;
        int16_t star_idx = MOX::planet[planet_idx].star_index;

        for (int16_t i = 0; i < 5; ++i) {
            if (result) {
                return result;
            }
            int16_t p_idx = MOX::star[star_idx].planet_index[i];
            if (p_idx > -1) {
                int16_t colony_idx = MOX::planet[p_idx].colony_index;
                if (colony_idx > -1) {
                    if (MOX::colony[colony_idx].owner != MOX::PLAYER_NUM) {
                        result = true;
                    }
                }
            }
        }

        if (result) {
            return result;
        }

        for (int16_t i = 0; i < (int16_t)MOX::ship_stack_count; ++i) {
            int16_t node_idx = MOX::ship_stack_start[i];
            bool done_traversing = false;
            while (!done_traversing) {
                if (node_idx == -1) {
                    done_traversing = true;
                } else {
                    int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
                    if (MOX::ship[ship_idx].location == star_idx) {
                        if (HACCESS::Ship_Owner(ship_idx) != MOX::PLAYER_NUM) {
                            done_traversing = true;
                            result = true;
                        }
                    }
                    node_idx = MOX::ship_node[node_idx].next_node;
                }
            }
        }

        return result;
    }

    int16_t __cdecl Colony_Ship_Auto_Colonization(s_ship_data* ship) {
        if ((ship->d.ship_type != SHIP_TYPE_COLONY && ship->d.ship_type != SHIP_TYPE_OUTPOST) ||
            ship->status >= 3) {
            return PLANET_INDEX_NONE;
        }

        if (ship->d.ship_weapon[1].type == 1) {
            int16_t planet_idx = ship->d.ship_weapon[0].type;
            if (planet_idx >= 0 && planet_idx < MAX_PLANETS) {
                return planet_idx;
            }
        }
        return PLANET_INDEX_NONE;
    }

    void __cdecl Set_Auto_Colonize_Flag(s_ship_data* ship, int16_t flag) {
        ship->d.ship_weapon[1].type = 1;
        ship->d.ship_weapon[0].type = flag;
    }

    bool __cdecl Ship_Is_Not_Colonizing(s_ship_data* ship) {
        int16_t location = ship->location;

        bool is_at_planet = (location >= 0 && location < SHIP_LOCATION_MOVING_OFFSET);
        bool is_in_warp_range = (location >= SHIP_LOCATION_WORMHOLE_OFFSET && location < SHIP_LOCATION_LIMIT);

        int16_t auto_col_status = Colony_Ship_Auto_Colonization(ship);

        if (is_at_planet) {
            return true;
        }

        if (is_in_warp_range && auto_col_status == PLANET_INDEX_NONE) {
            return true;
        }

        return false;
    }

    void __cdecl Update_Ship_Etas() {
        for (int16_t i = 0; i < MOX::scroll_bar->total_rows; ++i) {
            planet_summary[i].colony_eta = -1;
            planet_summary[i].outpost_eta = -1;
        }

        for (int16_t ship_idx = 0; ship_idx < (int16_t)MOX::NUM_SHIPS; ++ship_idx) {
            int16_t star_id = HAROLD::Absolute_Location(MOX::ship[ship_idx].location);
            int16_t owner = HACCESS::Ship_Owner(ship_idx);

            if (owner == MOX::PLAYER_NUM && star_id > -1 && MOX::ship[ship_idx].status < 3) {
                uint8_t ship_type = MOX::ship[ship_idx].d.ship_type;
                if (ship_type == SHIP_TYPE_COLONY || ship_type == SHIP_TYPE_OUTPOST) {
                    int16_t turns = HAROLD::N_Turns_To_Star(ship_idx, star_id);
                    int16_t planet_idx = Colony_Ship_Auto_Colonization(&MOX::ship[ship_idx]);

                    if (planet_idx == PLANET_INDEX_NONE || MOX::planet[planet_idx].colony_index != -1) {
                        continue;
                    }

                    int16_t summary_idx = -1;
                    if (planet_idx > -1) {
                        summary_idx = Planet_Summary_List_Id(planet_idx);
                    }

                    if (summary_idx > -1) {
                        if (ship_type == SHIP_TYPE_COLONY) {
                            planet_summary[summary_idx].colony_eta = (int8_t)turns;
                        } else {
                            planet_summary[summary_idx].outpost_eta = (int8_t)turns;
                        }
                    }
                }
            }
        }
    }

    int16_t __cdecl First_Planet_In_Summary_List(int16_t star_index) {
        int16_t result = -1;
        int16_t i = 0;

        if (MOX::scroll_bar->total_rows > 0) {
            do {
                if (result != -1) {
                    break;
                }

                int16_t planet_index = planet_summary[i].explored;

                if (MOX::planet[planet_index].star_index == star_index) {
                    result = i;
                }

                i++;
            } while (i < MOX::scroll_bar->total_rows);
        }

        return result;
    }

    int16_t __cdecl Summary_Planet_Index(int16_t star_index) {
        int16_t result = -1;

        for (int16_t i = 0; i < MOX::scroll_bar->total_rows; i++) {
            if (result != -1) {
                break;
            }

            int16_t planet_idx = PLNTSUM::planet_summary[i].explored;

            if (MOX::planet[planet_idx].star_index == star_index) {
                result = i;
            }
        }

        return result;
    }

    void __cdecl Add_Colony_Ship_Icon_Field(int16_t* field_id, int16_t row_index) {
        // pics[13] corresponds to the colony ship icon animation header
        s_animation_header* ship_icon = PLNTSUM::screen->pics[13];

        int16_t icon_w = animate::Get_Width(ship_icon);
        int16_t icon_h = animate::Get_Height(ship_icon);

        int16_t x1 = 102 - icon_w;
        int16_t y1 = plntscrn_delta_y[row_index] - icon_h + 84;

        int16_t x2 = x1 + icon_w - 1;
        int16_t y2 = y1 + icon_h - 1;

        // Register the field and store its handle
        *field_id = fields::Add_Hidden_Field(x1, y1, x2, y2, "", 0);
    }

    void __cdecl Add_Outpost_Ship_Icon_Field(int16_t* field_id, int16_t row_index) {
        s_animation_header* pic = screen->pics[13];
        int16_t width = animate::Get_Width(pic);
        int16_t height = animate::Get_Height(pic);

        int16_t y = plntscrn_delta_y[row_index] - height + 84;

        *field_id = fields::Add_Hidden_Field(
            18,
            y,
            18 + width - 1,
            y + height - 1,
            "",
            0);
    }

    void __cdecl Reset_Explored_Planets() {
        memset(explored_bitmap, 0, sizeof(explored_bitmap));
    }

    void __cdecl Reset_Ship_To_Star_Distances() {
        int choice_mode;

        if (PLNTSUM::send_choice == 0) {
            choice_mode = SHIP_TYPE_COLONY;
        } else {
            choice_mode = SHIP_TYPE_OUTPOST;
        }

        for (short i = 0; i < MOX::scroll_bar->total_rows; ++i) {
            if (choice_mode == SHIP_TYPE_COLONY) {
                PLNTSUM::planet_summary[i].best_colony_eta = 9999;
            } else if (choice_mode == SHIP_TYPE_OUTPOST) {
                PLNTSUM::planet_summary[i].best_outpost_eta = 9999;
            }
        }

        for (int16_t i = 0; i < PLNTSUM::n_ships_in_summary; ++i) {
            if (choice_mode == SHIP_TYPE_COLONY) {
                int16_t ship_idx = PLNTSUM::ship_summary[i].colony_ship_idx;
                if (ship_idx > -1 && ship_idx < MOX::NUM_SHIPS) {
                    PLNTSUM::Set_Colony_Ship_To_Star_Distances(i, ship_idx);
                }
            } else if (choice_mode == SHIP_TYPE_OUTPOST) {
                int16_t ship_idx = PLNTSUM::ship_summary[i].outpost_ship_idx;
                if (ship_idx > -1 && ship_idx < MOX::NUM_SHIPS) {
                    PLNTSUM::Set_Outpost_Ship_To_Star_Distances(i, ship_idx);
                }
            }
        }
    }

    void __cdecl Set_Planet_Explored_Flags() {
        Reset_Explored_Planets();

        bool has_omniscience_trait =
            MOX::player[MOX::PLAYER_NUM].traits[TRAIT_OMNISCIENCE] != 0;

        for (int16_t i = 0; i < (int16_t)MOX::planet_count; ++i) {
            int16_t star_idx = MOX::planet[i].star_index;
            if (has_omniscience_trait ||
                HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx)) {
                struct_::Set_Bit_Field(PLNTSUM::explored_bitmap, i);
            }
        }
    }

    void __cdecl Add_Plntsum_Fields() {
        screen->buttons[0] = fields::Add_Multi_Button_Field(441, 200, "", screen->pics[1], &sort_choice, 0, "", 41);
        screen->buttons[1] = fields::Add_Multi_Button_Field(501, 200, "", screen->pics[2], &sort_choice, 1, "", 41);
        screen->buttons[2] = fields::Add_Multi_Button_Field(567, 200, "", screen->pics[3], &sort_choice, 2, "", 41);

        screen->buttons[3] = fields::Add_Radio_Button_Field(441, 266, "", screen->pics[4], &filter_out_enemy_controlled_planets, "1", 41);
        screen->buttons[4] = fields::Add_Radio_Button_Field(441, 289, "", screen->pics[5], &filter_out_adverse_gravity_planets, "2", 41);
        screen->buttons[5] = fields::Add_Radio_Button_Field(441, 312, "", screen->pics[6], &filter_out_hostile_environment_planets, "3", 41);
        screen->buttons[6] = fields::Add_Radio_Button_Field(441, 335, "", screen->pics[7], &filter_out_mineral_scarcity_planets, "4", 41);
        screen->buttons[7] = fields::Add_Radio_Button_Field(441, 358, "", screen->pics[8], &filter_out_planets_out_of_range, "5", 41);

        if (Send_Colony_Button_Selectable()) {
            screen->buttons[8] = fields::Add_Hidden_Field(454, 386, 609, 408, "C", 41);
        } else {
            screen->buttons[8] = -1000;
        }

        if (Send_Outpost_Button_Selectable()) {
            screen->buttons[9] = fields::Add_Hidden_Field(454, 413, 610, 437, "O", 41);
        } else {
            screen->buttons[9] = -1000;
        }

        for (int i = 0; i < 8; ++i) {
            screen->colony_icons[i] = -1000;
            screen->output_icons[i] = -1000;
        }

        for (int i = 0; i < 8; ++i) {
            int row = MOX::scroll_bar->first_visible_row + i;
            if (row >= MOX::scroll_bar->total_rows) {
                break;
            }
            if (planet_summary[row].colony_eta >= 0) {
                Add_Colony_Ship_Icon_Field(&screen->colony_icons[i], i);
                // Preserve the original byte store from the field handle into global_var.
                memcpy(&MOX::global_var, &screen->colony_icons[i], sizeof(MOX::global_var));
            }
            if (planet_summary[row].outpost_eta >= 0) {
                Add_Outpost_Ship_Icon_Field(&screen->output_icons[i], i);
            }
        }

        for (int i = 0; i < 8; ++i) {
            int16_t y1 = plntscrn_delta_y[i] + 36;
            screen->fields[i] = fields::Add_Hidden_Field(18, y1, 414, y1 + 50, "", 41);
        }

        screen->up_button = fields::Add_Button_Field(422, 16, "", screen->pics[9], "-", 40);
        screen->dn_button = fields::Add_Button_Field(421, 448, "", screen->pics[10], "+", 40);

        Add_PlntSum_Galaxy_Map_Fields(screen->planet_icons);
        Add_Planet_Summary_Bar_Field();

        screen->esc_button = fields::Add_Button_Field(
            454,
            440,
            "",
            screen->pics[11],
            /* Preserves the raw 0x1B control byte token used by the original field wiring. */
            "\x1B",
            40);

        MOX::scroll_bar->up_btn = screen->up_button;
        MOX::scroll_bar->dn_btn = screen->dn_button;
        screen->fields_first_row = MOX::scroll_bar->first_visible_row;
        screen->fields_total_rows = MOX::scroll_bar->total_rows;
        screen->fields_sort_choice = sort_choice;
        screen->refresh_fields = false;
    }

    void __cdecl Refresh_Plntsum_Fields() {
        if (screen->fields_first_row != MOX::scroll_bar->first_visible_row ||
            screen->fields_total_rows != MOX::scroll_bar->total_rows ||
            screen->fields_sort_choice != sort_choice) {
            screen->refresh_fields = true;
        }

        // Rebuilding fields during a scroll-bar drag would discard its active field.
        if (screen->refresh_fields && mouse::Mouse_Button() == 0) {
            fields::Clear_Fields();
            Add_Plntsum_Fields();
        }
    }

    void __cdecl Handle_Send_Button(int16_t input_id) {
        int16_t choice;
        uint8_t* status;
        uint8_t* other_status;
        if (input_id == screen->buttons[8] && draw_colony_button_status != 0) {
            choice = 0;
            status = &draw_colony_button_status;
            other_status = &draw_outpost_button_status;
        } else if (input_id == screen->buttons[9] && draw_outpost_button_status != 0) {
            choice = 1;
            status = &draw_outpost_button_status;
            other_status = &draw_colony_button_status;
        } else {
            return;
        }

        if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
            GENDRAW::Warning_Box(HAROLD::H_Message(0x13f));
            *status = 1;
            if (*other_status != 0) {
                *other_status = 1;
            }
            send_choice = -1;
        } else if (*status == 2) {
            *status = 1;
            send_choice = -1;
        } else {
            *status = 2;
            if (*other_status != 0) {
                *other_status = 1;
            }
            send_choice = choice;
        }
    }
    void __cdecl Determine_Button_Status() {
        Build_Ship_Summary();

        draw_colony_button_status = 0;
        draw_outpost_button_status = 0;

        if (MOX::scroll_bar->total_rows > 0 && EVENTS::Event_Check_Hyperspace_Flux() == 0) {
            if (Send_Colony_Button_Selectable()) {
                draw_colony_button_status = 1;
            }
            if (Send_Outpost_Button_Selectable()) {
                draw_outpost_button_status = 1;
            }
        }
        if (send_choice == 0 && draw_colony_button_status != 0) {
            draw_colony_button_status = 2;
        } else if (send_choice == 1 && draw_outpost_button_status != 0) {
            draw_outpost_button_status = 2;
        } else {
            send_choice = -1;
        }
    }

    void __cdecl Check_For_Colony_And_Outpost_Ships() {
        found_colony_ship = 0;
        found_outpost_ship = 0;
        bool stop_searching = false;

        if (MOX::ship_stack_count <= 0) {
            return;
        }

        for (int16_t stack_idx = 0; stack_idx < (int16_t)MOX::ship_stack_count; ++stack_idx) {
            if (stop_searching) {
                break;
            }

            int16_t node_idx = MOX::ship_stack_start[stack_idx];
            bool stop_group = false;

            while (true) {
                int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
                s_ship_data* ship = &MOX::ship[ship_idx];

                bool can_order = HACCESS::Player_Can_Order_Ship(MOX::PLAYER_NUM, ship);

                // Colony ship check (SHIP_TYPE_COLONY = 1)
                if (ship->d.ship_type == SHIP_TYPE_COLONY) {
                    if (can_order && ship->status != 1) {
                        if (Ship_Is_Not_Colonizing(ship)) {
                            draw_colony_button_status = 1;
                            found_colony_ship = 1;
                        }
                    }
                }

                // Outpost ship check (SHIP_TYPE_OUTPOST = 4)
                if (ship->d.ship_type == SHIP_TYPE_OUTPOST) {
                    if (can_order && ship->status != 1) {
                        if (Ship_Is_Not_Colonizing(ship)) {
                            draw_outpost_button_status = 1;
                            found_outpost_ship = 1;
                        }
                    }
                }

                // Terminate if we have found a ship of interest and an external condition is met
                if (found_colony_ship != 0 && found_outpost_ship != 0) {
                    stop_group = true;
                    stop_searching = true;
                }

                if (stop_group) {
                    break;
                }

                node_idx = MOX::ship_node[node_idx].next_node;
                if (node_idx == -1) {
                    stop_group = true;
                    break;
                }
            }
        }
    }

    void Build_Ship_Summary() {
        n_ships_in_summary = 0;
        memset(ship_summary, 0xFF, sizeof(*ship_summary) * MAX_SHIPS);
        memset(can_reach_star, 0, sizeof(can_reach_star));

        found_colony_ship = 0;
        found_outpost_ship = 0;

        if (MOX::ship_stack_count > 0) {
            for (int16_t stack_idx = 0; stack_idx < MOX::ship_stack_count; ++stack_idx) {
                int16_t node_idx = MOX::ship_stack_start[stack_idx];
                int16_t first_node_ship_idx = MOX::ship_node[node_idx].ship_idx;

                if (HACCESS::Player_Can_Order_Ship(MOX::PLAYER_NUM, &MOX::ship[first_node_ship_idx]) && node_idx != -1) {
                    bool stop_stack = false;
                    do {
                        int16_t icon_id = HACCESS::Ship_Icon_Id_From_Node_Ptr(node_idx);
                        ship_summary[n_ships_in_summary].ship_icon_id = icon_id;

                        int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
                        int16_t owner = HACCESS::Ship_Owner(ship_idx);
                        s_ship_data* ship = &MOX::ship[ship_idx];

                        if (ship->d.ship_type == SHIP_TYPE_COLONY && owner == MOX::PLAYER_NUM && ship->status < 3) {
                            ship_summary[n_ships_in_summary].colony_ship_idx = ship_idx;
                            if (Ship_Is_Not_Colonizing(ship)) {
                                found_colony_ship = 1;
                                Set_Colony_Ship_To_Star_Distances(n_ships_in_summary, ship_idx);
                                ship_summary[n_ships_in_summary].colony_available = true;
                            } else {
                                ship_summary[n_ships_in_summary].colony_available = false;
                            }
                        } else if (ship->d.ship_type == SHIP_TYPE_OUTPOST && owner == MOX::PLAYER_NUM && ship->status < 3) {
                            ship_summary[n_ships_in_summary].outpost_ship_idx = ship_idx;
                            if (Ship_Is_Not_Colonizing(ship)) {
                                found_outpost_ship = 1;
                                Set_Outpost_Ship_To_Star_Distances(n_ships_in_summary, ship_idx);
                                ship_summary[n_ships_in_summary].output_available = true;
                            } else {
                                ship_summary[n_ships_in_summary].output_available = false;
                            }
                        }

                        n_ships_in_summary++;

                        if (found_colony_ship != 0 && found_outpost_ship != 0) {
                            stop_stack = true;
                        }

                        if (!stop_stack) {
                            node_idx = MOX::ship_node[node_idx].next_node;
                            if (node_idx == -1) {
                                stop_stack = true;
                            }
                        }
                    } while (!stop_stack);
                }
            }
        }

        Update_Ship_Etas();
    }

    void __cdecl Filter_Explored_Planets(int16_t* summary_count) {
        int16_t original_count = *summary_count;
        allocate::Push_Block(MOX::screen_seg);

        s_plntsum_planet* temp_buffer =
            (s_plntsum_planet*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*temp_buffer) * MAX_PLANETS);

        memset(temp_buffer, 0xFF, sizeof(*temp_buffer) * MAX_PLANETS);

        *summary_count = 0;

        for (int16_t i = 0; i < (int16_t)MOX::planet_count; ++i) {
            RUSS::Mox_Update();

            bool passes_filter = true;

            // Initial requirements: must be a planet, not owned by player, not an outpost
            if (MOX::planet[i].planet_type != PLANET_TYPE_PLANET) {
                passes_filter = false;
            } else if (Planet_Has_Players_Colony(i)) {
                passes_filter = false;
            } else if (Planet_Is_Outpost_Planet(i)) {
                passes_filter = false;
            }

            // Visitation and Exploration requirements
            if (passes_filter) {
                if (!Planet_Has_Been_Visited(i)) {
                    passes_filter = false;
                } else if (PLNTSUM::filter_out_enemy_controlled_planets != 0 &&
                    !struct_::Test_Bit_Field(PLNTSUM::explored_bitmap, i)) {
                    passes_filter = false;
                }
            }

            // Optional environmental/range filters
            if (passes_filter) {
                if (PLNTSUM::filter_out_planets_out_of_range != 0 && !Planet_In_Range_Of_A_Colony(i)) {
                    passes_filter = false;
                }
                if (passes_filter && PLNTSUM::filter_out_adverse_gravity_planets != 0 && Planet_Has_Adverse_Gravity(i, MOX::PLAYER_NUM)) {
                    passes_filter = false;
                }
                if (passes_filter && PLNTSUM::filter_out_enemy_controlled_planets != 0 && Planet_Is_Enemy_Controlled(i, MOX::PLAYER_NUM)) {
                    passes_filter = false;
                }
                if (passes_filter && PLNTSUM::filter_out_hostile_environment_planets != 0 && Planet_Has_Hostile_Environment(i)) {
                    passes_filter = false;
                }
                if (passes_filter && PLNTSUM::filter_out_mineral_scarcity_planets != 0 && Planet_Is_Mineral_Scarce(i)) {
                    passes_filter = false;
                }
            }

            // If all conditions met, record planet summary
            if (passes_filter) {
                s_plntsum_planet* p_sum = &temp_buffer[*summary_count];
                p_sum->max_pop = COLCALC::Planet_Max_Population_For_Player(&MOX::planet[i], MOX::PLAYER_NUM);
                p_sum->food_per_farmer = (int16_t)MOX::planet[i].food_per_farmer;
                p_sum->minerals = (int16_t)COLCALC::minerals_per_mine[MOX::planet[i].mineral_class];
                p_sum->mineral_class = (int16_t)MOX::planet[i].mineral_class;
                p_sum->climate = (int16_t)MOX::planet[i].climate;
                p_sum->explored = i;

                (*summary_count)++;
            }
        }

        // If the count of visible planets changed, update the global summary
        if (*summary_count != original_count) {
            memcpy(PLNTSUM::planet_summary, temp_buffer, sizeof(*temp_buffer) * MAX_PLANETS);
            PLNTSUM::Sort_Planet_Summary();
            MOX::scanned_field = -1;
        }

        allocate::Pop_Block(MOX::screen_seg);
    }

    int16_t __cdecl Planet_Summary_List_Id(int16_t planet_idx) {
        int16_t result = -1;
        int16_t total_rows = MOX::scroll_bar->total_rows;

        if (total_rows > 0) {
            for (int16_t i = 0; i < total_rows; ++i) {
                if (result != -1) {
                    break;
                }

                if (PLNTSUM::planet_summary[i].explored == planet_idx) {
                    result = i;
                }
            }
        }

        return result;
    }

    void __cdecl Set_Colony_Ship_To_Star_Distances(int16_t ship_summary_idx, int16_t ship_idx) {
        bool in_stasis_1000 = false;
        bool in_stasis_500 = false;
        int16_t moving_ships[2];
        s_ship_move_info move_info;

        int16_t location = MOX::ship[ship_idx].location;

        if (location >= SHIP_LOCATION_WORMHOLE_OFFSET && location < SHIP_LOCATION_LIMIT) {
            in_stasis_1000 = true;
        } else {
            in_stasis_1000 = false;
        }

        if (location >= SHIP_LOCATION_MOVING_OFFSET && location < SHIP_LOCATION_WORMHOLE_OFFSET) {
            in_stasis_500 = true;
        } else {
            in_stasis_500 = false;
        }

        moving_ships[0] = ship_summary[ship_summary_idx].colony_ship_idx;
        moving_ships[1] = -1;

        for (int16_t star_idx = 0; star_idx < MAX_STARS; ++star_idx) {
            int16_t summary_planet_idx = Summary_Planet_Index(star_idx);

            if (summary_planet_idx != -1) {
                SHIPMOVE::Ships_Try_To_Move_To(moving_ships, star_idx, &move_info);

                if (move_info.moving != 0) {
                    int16_t turns = (uint8_t)move_info.turns_left;
                    can_reach_star[star_idx] = 1;

                    if (in_stasis_1000) {
                        turns += 1000;
                    } else if (in_stasis_500) {
                        turns += 2000;
                    }

                    if (planet_summary[summary_planet_idx].best_colony_eta == -1 ||
                        turns < planet_summary[summary_planet_idx].best_colony_eta) {
                        for (int16_t p = 0; p < 5; ++p) {
                            int16_t planet_idx = MOX::star[star_idx].planet_index[p];
                            if (planet_idx != -1) {
                                int16_t summary_id = Planet_Summary_Id(planet_idx);
                                if (summary_id != -1) {
                                    planet_summary[summary_id].best_colony_eta = turns;
                                    planet_summary[summary_id].best_colony_ship_idx = ship_idx;
                                    planet_summary[summary_id].blocked = 0;
                                    planet_summary[summary_id].ship_summary_idx = ship_summary_idx;
                                }
                            }
                        }
                    }
                } else {
                    if (move_info.blackhole_blocks != 0) {
                        if (planet_summary[summary_planet_idx].best_colony_eta == -1) {
                            planet_summary[summary_planet_idx].blocked = 1;
                        }
                    }
                }
            }
        }
    }

    void __cdecl Set_Outpost_Ship_To_Star_Distances(int16_t ship_summary_idx, int16_t ship_idx) {
        bool in_stasis_1000 = false;
        bool in_stasis_500 = false;
        int16_t moving_ships[2];
        s_ship_move_info move_info;

        int16_t location = MOX::ship[ship_idx].location;

        if (location >= SHIP_LOCATION_WORMHOLE_OFFSET && location < SHIP_LOCATION_LIMIT) {
            in_stasis_1000 = true;
        } else if (location >= SHIP_LOCATION_MOVING_OFFSET && location < SHIP_LOCATION_WORMHOLE_OFFSET) {
            in_stasis_500 = true;
        }

        moving_ships[0] = ship_summary[ship_summary_idx].outpost_ship_idx;
        moving_ships[1] = -1;

        for (int16_t star_idx = 0; star_idx < MAX_STARS; ++star_idx) {
            int16_t summary_planet_idx = Summary_Planet_Index(star_idx);

            if (summary_planet_idx != -1) {
                SHIPMOVE::Ships_Try_To_Move_To(moving_ships, star_idx, &move_info);

                if (move_info.moving != 0) {
                    can_reach_star[star_idx] = 1;
                    int16_t turns = (uint8_t)move_info.turns_left;

                    if (in_stasis_1000) {
                        turns += 1000;
                    } else if (in_stasis_500) {
                        turns += 2000;
                    }

                    if (planet_summary[summary_planet_idx].best_outpost_eta == -1 ||
                        turns < planet_summary[summary_planet_idx].best_outpost_eta) {
                        for (int16_t p = 0; p < 5; ++p) {
                            int16_t planet_idx = MOX::star[star_idx].planet_index[p];
                            if (planet_idx != -1) {
                                int16_t summary_id = Planet_Summary_Id(planet_idx);
                                if (summary_id != -1) {
                                    planet_summary[summary_id].best_outpost_eta = turns;
                                    planet_summary[summary_id].best_outpost_ship_idx = ship_idx;
                                    planet_summary[summary_id].blocked = 0;
                                    planet_summary[summary_id].ship_summary_idx = ship_summary_idx;
                                }
                            }
                        }
                    }
                } else {
                    if (move_info.blackhole_blocks != 0 && planet_summary[summary_planet_idx].best_outpost_eta == -1) {
                        planet_summary[summary_planet_idx].blocked = 1;
                    }
                }
            }
        }
    }

    int16_t __cdecl Planet_Summary_Id(int16_t explored_id) {
        int16_t result_index = -1;
        int16_t total_rows = MOX::scroll_bar->total_rows;

        for (int16_t i = 0; i < total_rows; ++i) {
            // If we have already found a matching index, exit the loop.
            // Note: This logic follows the assembly check at the start of the loop block.
            if (result_index != -1) {
                break;
            }

            if (PLNTSUM::planet_summary[i].explored == explored_id) {
                result_index = i;
            }
        }

        return result_index;
    }

    int __cdecl qsort_by_climate(const void* a, const void* b) {
        const s_plntsum_planet* lhs = (const s_plntsum_planet*)a;
        const s_plntsum_planet* rhs = (const s_plntsum_planet*)b;
        return (int)rhs->climate - (int)lhs->climate;
    }

    int __cdecl qsort_by_minerals(const void* a, const void* b) {
        const s_plntsum_planet* lhs = (const s_plntsum_planet*)a;
        const s_plntsum_planet* rhs = (const s_plntsum_planet*)b;
        return (int)rhs->minerals - (int)lhs->minerals;
    }

    int __cdecl qsort_by_max_population(const void* a, const void* b) {
        const s_plntsum_planet* lhs = (const s_plntsum_planet*)a;
        const s_plntsum_planet* rhs = (const s_plntsum_planet*)b;
        return (int)rhs->max_pop - (int)lhs->max_pop;
    }

    void __cdecl Sort_Planet_Summary() {
        switch (sort_choice) {
            case 0:
                qsort(planet_summary, (uint32_t)MOX::scroll_bar->total_rows, sizeof(*planet_summary), qsort_by_climate);
                break;
            case 1:
                qsort(planet_summary, (uint32_t)MOX::scroll_bar->total_rows, sizeof(*planet_summary), qsort_by_minerals);
                break;
            case 2:
                qsort(planet_summary, (uint32_t)MOX::scroll_bar->total_rows, sizeof(*planet_summary), qsort_by_max_population);
                break;
            default:
                break;
        }
    }

    void __cdecl Add_PlntSum_Galaxy_Map_Fields(int16_t* fields_array) {
        for (int16_t i = 0; i < MAX_STARS; ++i) {
            fields_array[i] = -1000;
        }

        if (MOX::NUM_STARS > 0) {
            for (int16_t j = 0; j < MOX::NUM_STARS; ++j) {
                short x, y;
                MOVEBOX::Get_Galaxy_Map_Star_XY(j, 443, 17, 180, 116, 0, 0, 0, &x, &y);
                fields_array[j] = fields::Add_Hidden_Field(x, y, x + 5, y + 5, "", 41);
            }
        }
    }

    static int16_t Ship_Sent_To_Planet(int16_t planet_idx, int16_t order_type) {
        // Update_Ship_Etas uses the last matching ship for each planet's icon.
        for (int16_t ship_idx = MOX::NUM_SHIPS - 1; ship_idx >= 0; --ship_idx) {
            s_ship_data* ship = &MOX::ship[ship_idx];
            if (ship->d.ship_type == order_type && HACCESS::Ship_Owner(ship_idx) == MOX::PLAYER_NUM &&
                HAROLD::Absolute_Location(ship->location) > -1 &&
                Colony_Ship_Auto_Colonization(ship) == planet_idx) {
                return ship_idx;
            }
        }
        return -1;
    }

    void __cdecl Undo_Send_Order(int16_t planet_idx, int16_t order_type) {
        int16_t ship_idx = Ship_Sent_To_Planet(planet_idx, order_type);
        int16_t ships[2];
        char buffer1[200];
        char buffer2[200];
        char eta_str[40];

        ships[0] = -1;
        ships[1] = -1;

        if (ship_idx == -1) {
            return;
        }
        if (!MAINSCR::Ship_Can_Be_Selected(ship_idx)) {
            int16_t msg_id = (order_type == SHIP_TYPE_COLONY) ? 0x149 /* HESTR_149_YOU_ARE_UNABLE_TO_UNDO */ : 0x14A /* HESTR_14A_YOU_ARE_UNABLE_TO_UNDO */;
            const char* msg = HAROLD::H_Message(msg_id);
            strlcpy(buffer1, msg, sizeof(buffer1));
            char* planet_name = HACCESS::Get_Planet_Name_No_Prime(planet_idx);
            snprintf(buffer2, sizeof(buffer2), buffer1, planet_name);
            GENDRAW::Warning_Box(buffer2);
            PLNTSUM::screen->refresh_fields = true;
            return;
        }

        if (ship_idx != -1) {
            int16_t star_id = HAROLD::XY_To_Star_Id(MOX::ship[ship_idx].x, MOX::ship[ship_idx].y);

            if (star_id == -1) {
                ships[0] = ship_idx;
                star_id = HACCESS::Nearest_Star_To_XY(ships);

                if (star_id == -1) {
                    star_id = MOX::planet[MOX::player[MOX::PLAYER_NUM].home_planet_id].star_index;
                }
            }

            if (star_id == -1) {
                return;
            }

            ships[0] = ship_idx;
            SHIPMOVE::Ships_Try_To_Move_To(ships, star_id, &MOX::g_ship_move_info);

            int16_t eta_msg_id = (MOX::g_ship_move_info.turns_left == 1) ? 0x14B /* HESTR_14B_ETA_D_TURN */ : 0x14C /* HESTR_14C_ETA_D_TURNS */;
            const char* eta_raw = HAROLD::H_Message(eta_msg_id);
            snprintf(eta_str, sizeof(eta_str), eta_raw, (uint32_t)(uint8_t)MOX::g_ship_move_info.turns_left);

            int16_t heading_msg_id = (order_type == SHIP_TYPE_COLONY) ? 0x14D /* HESTR_14D_COLONY_SHIP_HEADING_TO */ : 0x14E /* HESTR_14E_OUTPOST_SHIP_HEADING_T */;
            const char* heading_raw = HAROLD::H_Message(heading_msg_id);
            strlcpy(buffer1, heading_raw, sizeof(buffer1));

            const char* eta_param = (MOX::g_ship_move_info.turns_left == 0) ? "" : eta_str;
            char* planet_name = HACCESS::Get_Planet_Name_No_Prime(planet_idx);

            snprintf(buffer2, sizeof(buffer2), buffer1, planet_name, &MOX::star[star_id], eta_param);

            if (GENDRAW::Do_Confirmation_Box(buffer2)) {
                SHIPMOVE::Make_Ships_Move_To(ships, star_id, &MOX::g_ship_move_info);
                SHIPS::Clear_Auto_Colonization_Flag(&MOX::ship[ship_idx]);
                PLNTSUM::Update_Ship_Etas();
            }
            PLNTSUM::screen->refresh_fields = true;
        }
    }

    void __cdecl Draw_Colony_ETA(int16_t unused_planet_sum_idx, int16_t scroll_idx) {
        (void)unused_planet_sum_idx;
        int16_t width = animate::Get_Width(screen->pics[13]);
        int16_t height = animate::Get_Height(screen->pics[13]);

        int16_t x = 0x66 - width;
        int16_t y = (plntscrn_delta_y[scroll_idx] - height) + 0x54;

        animate::Draw(x, y, screen->pics[13]);
    }

    int16_t __cdecl Get_ETA_To_Star(int16_t star_idx) {
        int16_t best_ship = -1;

        if (MOX::scroll_bar->total_rows > 0) {
            for (int16_t i = 0; i < MOX::scroll_bar->total_rows; ++i) {
                if (best_ship != -1) {
                    break;
                }

                int16_t planet_idx = planet_summary[i].explored;
                if (MOX::planet[planet_idx].star_index == star_idx) {
                    best_ship = planet_summary[i].best_colony_ship_idx;
                    if (best_ship <= planet_summary[i].best_outpost_ship_idx) {
                        best_ship = planet_summary[i].best_outpost_ship_idx;
                    }
                }
            }
        }

        if (best_ship != -1) {
            return HAROLD::N_Turns_To_Star(best_ship, star_idx);
        }

        return 0;
    }

    void __cdecl Draw_Outpost_ETA(int16_t unused_planet_sum_idx, int16_t scroll_idx) {
        (void)unused_planet_sum_idx;
        int16_t height = animate::Get_Height(screen->pics[13]);
        int16_t y = plntscrn_delta_y[scroll_idx] - height + 0x54;
        animate::Draw(0x12, y, screen->pics[13]);
    }
} // namespace PLNTSUM
