#include "pch.h"
#include "game_events.h"
#include <cstring>
#include <cstdio>

namespace PLNTSUM {
    constexpr int PLANET_CLIMATE_EXTENT = PLANET_CLIMATE_COUNT;
    constexpr int PLANET_SIZE_EXTENT = PLANET_SIZE_COUNT;
    constexpr int PLANET_PICTURE_COUNT = PLANET_CLIMATE_EXTENT * PLANET_SIZE_EXTENT;

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

    void __cdecl Summary_Screen_Print_Data(void) {
        int16_t monster_race = -1;
        int first_visible = static_cast<int>(MOX::scroll_bar->first_visible_row);
        int end_index = first_visible + static_cast<int>(MOX::scroll_bar->total_rows);

        if (first_visible + 8 <= end_index) {
            end_index = first_visible + 8;
        }

        for (int16_t scroll_row = 0; scroll_row < (end_index - first_visible); ++scroll_row) {
            int16_t summary_idx = static_cast<int16_t>(first_visible + scroll_row);
            int16_t y_base = PLNTSUM::plntscrn_delta_y[scroll_row] + 0x27;
            s_plntsum_planet* summary = &PLNTSUM::planet_summary[summary_idx];

            if (summary->explored != -1) {
                s_planet_data* planet = &MOX::planet[summary->explored];
                int16_t star_idx = planet->star_index;
                int16_t mineral_class = summary->mineral_class;
                int16_t climate_idx = summary->climate;
                int16_t gravity_class = static_cast<int16_t>(planet->gravity_class);

                char color_choice = 0;
                if (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1) {
                    if (scroll_row == (int16_t)PLNTSUM::box_field_scanned) {
                        color_choice = (PLNTSUM::can_reach_scanned_planet != 0) ? 1 : 2;
                    }
                } else if ((int16_t)PLNTSUM::box_field_scanned == scroll_row) {
                    color_choice = 1;
                }

                s_colors font_colors;
                Colony_Owner_Colors(planet->colony_index, &font_colors, color_choice);

                graphics::Set_Window(0x12, y_base, 0x68, PLNTSUM::plntscrn_delta_y[scroll_row + 1] + y_base);
                video::Clip_On();
                fonts::Set_Font_Style(1, &font_colors);

                int16_t special_id = 0;
                char system_special = MOX::star[star_idx].system_special;
                if (system_special > SYSTEM_SPECIAL_NO_SPECIAL && system_special != SYSTEM_SPECIAL_ORION_STAR) {
                    special_id = static_cast<int16_t>(system_special);
                }
                if (planet->planet_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
                    special_id = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
                }

                if (special_id > SYSTEM_SPECIAL_NO_SPECIAL && HACCESS::Special_Is_Planet_Special(special_id)) {
                    char* special_str = nullptr;
                    char special_buf[80];

                    if (MOX::star[star_idx].system_special == SYSTEM_SPECIAL_SPACE_MONSTER) {
                        int16_t ship_idx = HAROLD::First_Ship_Orbiting_Star(&MOX::star[star_idx]);
                        if (ship_idx != -1) {
                            special_str = MOX::ship[ship_idx].d.name;
                        } else if (special_id == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
                            strlcpy(special_buf, HAROLD::H_Message(140), sizeof(special_buf));
                            special_str = special_buf;
                        } else {
                            special_str = MOX::planet_special_string[special_id];
                        }
                    } else if (planet->planet_special == special_id) {
                        special_str = MOX::planet_special_string[special_id];
                    }

                    if (special_str) {
                        fonts::Print_Centered(0x3d, y_base, special_str);
                    }
                }

                fonts::Set_Font_Style(2, &font_colors);
                int16_t font_h = fonts::Get_Font_Height();
                int16_t y_offset = (25 - font_h) - 4;

                char planet_name_buf[32];
                strlcpy(planet_name_buf, HACCESS::Get_Planet_Name_No_Prime(summary->explored), sizeof(planet_name_buf));
                MISC::Set_Fitted_Font_Style(planet_name_buf, &font_colors, 2, 2, 0, 0x56, 1);
                int16_t fitted_name_h = fonts::Get_Font_Height();
                fonts::Print_Centered(0x3d, y_base + (25 - fitted_name_h) * 2, planet_name_buf);

                char owner_label[100];
                bool has_owner_label = false;

                if (planet->colony_index != -1) {
                    char* race_name = MOX::player[MOX::colony[planet->colony_index].owner].race_name;
                    snprintf(owner_label, sizeof(owner_label), "(%s)", race_name);
                    has_owner_label = true;
                } else if (HAROLD::Star_Guarded_By_Monster(star_idx, &monster_race)) {
                    char race_buf[20];
                    HACCESS::Race_Name(monster_race, race_buf, sizeof(race_buf));
                    snprintf(owner_label, sizeof(owner_label), "(%s)", race_buf);
                    has_owner_label = true;
                }

                if (has_owner_label) {
                    fonts::Set_Font_Style(1, &font_colors);
                    fonts::Print_Centered(0x3d, (y_base - fonts::Get_Font_Height()) + 0x30, owner_label);
                }

                video::Clip_Off();
                graphics::Reset_Window();

                int16_t y_climate = y_base + y_offset;
                char* climate_str = MOX::planet_climate_string[climate_idx];
                MISC::Set_Fitted_Font_Style(climate_str, &font_colors, 2, 2, -1, 0x41, 1);
                int16_t climate_h = fonts::Get_Font_Height();
                fonts::Print_Centered(0x8c, y_climate, climate_str);

                char work_buf[100];
                char food_text[16];
                fonts::Set_Font_Style(1, &font_colors);
                COLCALC::Format_Food2(
                    (uint16_t)summary->food_per_farmer,
                    food_text,
                    sizeof(food_text));
                snprintf(work_buf, sizeof(work_buf), "%s Food", food_text);
                fonts::Print_Centered(0x8c, y_climate + climate_h + 4, work_buf);

                char* gravity_str = MOX::planet_gravity_string[gravity_class];
                fonts::Set_Font_Style(2, &font_colors);
                int16_t gravity_h = fonts::Get_Font_Height();
                int16_t penalty = Productivity_Penalty(MOX::PLAYER_NUM, gravity_class);
                int16_t y_gravity;

                if (penalty == 0) {
                    y_gravity = y_climate + (gravity_h / 2);
                } else {
                    fonts::Print_Centered(0xd9, y_climate, gravity_str);
                    fonts::Set_Font_Style(1, &font_colors);
                    snprintf(work_buf, sizeof(work_buf), HAROLD::H_Message(0x142), (int)penalty);
                    y_gravity = y_climate + gravity_h + 4;
                }
                fonts::Print_Centered(0xd9, y_gravity, (penalty == 0) ? gravity_str : work_buf);

                fonts::Set_Font_Style(2, &font_colors);
                int16_t mineral_h = fonts::Get_Font_Height();
                fonts::Print_Centered(0x137, y_climate, MOX::mineral_class_string[mineral_class]);
                fonts::Set_Font_Style(1, &font_colors);
                snprintf(work_buf, sizeof(work_buf), HAROLD::H_Message(0x152), (int)summary->minerals);
                fonts::Print_Centered(0x137, y_climate + mineral_h + 4, work_buf);

                fonts::Set_Font_Style(2, &font_colors);
                int16_t size_h = fonts::Get_Font_Height();
                fonts::Print_Centered(0x182, y_climate, MOX::planet_size_string[planet->size]);
                fonts::Set_Font_Style(1, &font_colors);
                snprintf(work_buf, sizeof(work_buf), HAROLD::H_Message(0x143), (int)summary->max_pop);
                fonts::Print_Centered(0x182, y_climate + size_h + 4, work_buf);
                game_events::Observe_Settlement_Text(summary->explored, "planets-summary", work_buf, false, true);

                if (summary->colony_eta >= 0) {
                    PLNTSUM::Draw_Colony_ETA(summary_idx, scroll_row);
                }
                if (summary->outpost_eta >= 0) {
                    PLNTSUM::Draw_Outpost_ETA(summary_idx, scroll_row);
                }
            }
        }
    }

    void __cdecl Send_Nearest_Ship(int16_t planet_idx) {
        char msg_buffer[120];
        int16_t ship_list[2];
        s_ship_move_info move_info;
        int16_t nearest_ship_idx;
        int16_t summary_idx;
        int16_t target_star_idx;
        int16_t ship_origin_star_idx;
        bool should_continue = true;
        bool omniscient;
        uint8_t ship_type;
        char race_name_buffer[32];

        ship_list[0] = -1;
        ship_list[1] = -1;

        target_star_idx = MOX::planet[planet_idx].star_index;
        omniscient = HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);
        summary_idx = Planet_Summary_Id(planet_idx);
        nearest_ship_idx = Nearest_Ship_Id(summary_idx);

        ship_type = MOX::ship[nearest_ship_idx].d.ship_type;
        ship_origin_star_idx = HAROLD::Absolute_Location(MOX::ship[nearest_ship_idx].location);

        if (((ship_origin_star_idx != target_star_idx) && (ship_type == SHIP_TYPE_COLONY) && (PLNTSUM::planet_summary[summary_idx].colony_eta > 0)) ||
            ((ship_type == SHIP_TYPE_OUTPOST) && (PLNTSUM::planet_summary[summary_idx].outpost_eta > 0))) {
            snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x134 /* HESTR_134_YOU_ALREADY_HAVE_ONE_C */), &MOX::star[target_star_idx]);
            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
            should_continue = (GENDRAW::Confirmation_Box(msg_buffer) != 0);
            PLNTSUM::screen->refresh_fields = true;
            if (!should_continue) {
                return;
            }
        }

        if (nearest_ship_idx <= -1 || MOX::ship[nearest_ship_idx].location < SHIP_LOCATION_MOVING_OFFSET) {
            if (nearest_ship_idx == -1) {
                should_continue = false;
            }
        } else {
            bool visited_origin = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, ship_origin_star_idx);
            bool visited_dest = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, target_star_idx);

            if (ship_origin_star_idx == target_star_idx) {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x138 /* HESTR_138_S_ALREADY_HAS_A_COURSE */), &MOX::ship[nearest_ship_idx], &MOX::star[target_star_idx]);
                GENDRAW::Message_Box(msg_buffer);
                PLNTSUM::screen->refresh_fields = true;
                return;
            }

            if (omniscient || (visited_dest && visited_origin)) {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x139 /* HESTR_139_S_HAS_SET_A_COURSE_FOR */), &MOX::ship[nearest_ship_idx], &MOX::star[ship_origin_star_idx], &MOX::star[target_star_idx]);
            } else if (visited_dest) {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x13A /* HESTR_13A_S_HAS_SET_A_COURSE_SHO */), &MOX::ship[nearest_ship_idx], &MOX::star[target_star_idx]);
            } else if (visited_origin) {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x139 /* HESTR_139_S_HAS_SET_A_COURSE_FOR */), &MOX::ship[nearest_ship_idx], &MOX::star[ship_origin_star_idx]);
            } else {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x13A /* HESTR_13A_S_HAS_SET_A_COURSE_SHO */), &MOX::ship[nearest_ship_idx]);
            }

            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
            should_continue = (GENDRAW::Confirmation_Box(msg_buffer) != 0);
            PLNTSUM::screen->refresh_fields = true;
        }

        if (should_continue) {
            int16_t best_ship_idx;
            if (PLNTSUM::send_choice == 1) {
                best_ship_idx = PLNTSUM::planet_summary[summary_idx].best_outpost_ship_idx;
            } else {
                best_ship_idx = PLNTSUM::planet_summary[summary_idx].best_colony_ship_idx;
            }

            ship_list[0] = best_ship_idx;
            ship_list[1] = -1;

            SHIPMOVE::Ships_Try_To_Move_To(ship_list, target_star_idx, &move_info);

            if (move_info.moving == 0) {
                if (move_info.hyperspace_flux != 0) {
                    GENDRAW::Warning_Box(HAROLD::H_Message(0x13F /* HESTR_13F_SHIP_MOVEMENT_IS_NOT_A */));
                    PLNTSUM::screen->refresh_fields = true;
                }
            } else {
                bool user_confirmed_move = true;
                int16_t monster_race_summary_id;
                if (HAROLD::Star_Guarded_By_Monster(target_star_idx, &monster_race_summary_id)) {
                    HACCESS::Race_Name(monster_race_summary_id, race_name_buffer, sizeof(race_name_buffer));
                    mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

                    int16_t msg_id = (PLNTSUM::send_choice == 0) ? 0x13C /* HESTR_13C_S_IS_GUARDED_BY_A_S_DO */ : 0x13B /* HESTR_13B_S_IS_GUARDED_BY_A_S_DO */;
                    snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(msg_id), &MOX::star[target_star_idx], race_name_buffer);
                    user_confirmed_move = (GENDRAW::Confirmation_Box(msg_buffer) != 0);
                }

                if (user_confirmed_move) {
                    SHIPMOVE::Make_Ships_Move_To(ship_list, target_star_idx, &move_info);

                    int16_t confirm_msg_id = (move_info.turns_left == 1) ? 0x13D : 0x13E;

                    snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(confirm_msg_id), &MOX::ship[best_ship_idx], &MOX::star[target_star_idx], (uint32_t)(uint8_t)move_info.turns_left);
                    GENDRAW::Message_Box(msg_buffer);
                    PLNTSUM::screen->refresh_fields = true;
                    PLNTSUM::last_ship_moved = best_ship_idx;
                    PLNTSUM::Update_Ship_Etas();
                    PLNTSUM::Set_Auto_Colonize_Flag(&MOX::ship[best_ship_idx], planet_idx);
                }
            }
        }
    }
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

        // Calculate coordinates based on hardcoded offsets and row scaling
        int16_t x1 = 100 /* 0x64 */ - icon_w;
        int16_t y1 = (row_index * 55 /* 0x37 */) - icon_h + 82 /* 0x52 */;

        int16_t x2 = x1 + icon_w;
        int16_t y2 = y1 + icon_h;

        // Register the field and store its handle
        *field_id = fields::Add_Hidden_Field(x1, y1, x2, y2, "", 0);
    }

    void __cdecl Add_Outpost_Ship_Icon_Field(int16_t* field_id, int16_t row_index) {
        s_animation_header* pic = screen->pics[13];
        int16_t width = animate::Get_Width(pic);
        int16_t height = animate::Get_Height(pic);

        // Calculate top-left Y based on row index and animation height
        int16_t y = (row_index * 55 - height) + 84;

        *field_id = fields::Add_Hidden_Field(
            102,
            y,
            width + 102,
            y + height,
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

    int16_t __cdecl Productivity_Penalty(int16_t player_idx, int16_t planet_gravity) {
        s_player* plr = &MOX::player[player_idx];

        int8_t low_g_trait = plr->traits[TRAIT_LOW_G_WORLD];
        int8_t heavy_g_trait = plr->traits[TRAIT_HEAVY_G_WORLD];

        // A race is considered "Normal Gravity" if it has neither Low-G nor Heavy-G traits.
        bool is_normal_g_race = (low_g_trait == 0 && heavy_g_trait == 0);

        // 50% Penalty Conditions:
        // - Low-G race on Heavy-G planet (2)
        // - Normal-G race on Heavy-G planet (2)
        if ((low_g_trait != 0 && planet_gravity == PLANET_GRAVITY_HEAVY_G) ||
            (is_normal_g_race && planet_gravity == PLANET_GRAVITY_HEAVY_G)) {
            return 50;
        }

        // 25% Penalty Conditions:
        // - Low-G race on Normal-G planet (1)
        // - Normal-G or Heavy-G race on Low-G planet (0)
        if ((low_g_trait != 0 && planet_gravity == PLANET_GRAVITY_NORMAL_G) ||
            (low_g_trait == 0 && planet_gravity == PLANET_GRAVITY_LOW_G)) {
            return 25;
        }

        return 0;
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
            screen->buttons[8] = fields::Add_Hidden_Field(386, 408, 609, 413, "C", 41);
        } else {
            screen->buttons[8] = -1000;
        }

        if (Send_Outpost_Button_Selectable()) {
            screen->buttons[9] = fields::Add_Hidden_Field(413, 437, 610, 442, "O", 41);
        } else {
            screen->buttons[9] = -1000;
        }

        for (int i = 0; i < 8; ++i) {
            screen->colony_icons[i] = -1000;
            screen->output_icons[i] = -1000;
        }

        for (int i = 0; i < 8; ++i) {
            int row = MOX::scroll_bar->first_visible_row + i;
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
    }

    void __cdecl Allocate_Space_For_Summary_Screen() {
        planet_summary = (s_plntsum_planet*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*planet_summary) * MAX_PLANETS);
        memset(planet_summary, 0xFF, sizeof(*planet_summary) * MAX_PLANETS);

        screen = (s_plntsum_screen*)allocate::Allocate_Next_Block_Aligned(
            MOX::screen_seg, sizeof(*screen), alignof(s_plntsum_screen));
        memset(screen, 0, sizeof(*screen));

        ship_summary = (s_plntsum_ship*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*ship_summary) * MAX_SHIPS);
        memset(ship_summary, 0xFF, sizeof(*ship_summary) * MAX_SHIPS);

        MOX::scroll_bar = (s_scroll_bar*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*MOX::scroll_bar));
        memset(MOX::scroll_bar, 0, sizeof(*MOX::scroll_bar));
    }

    void __cdecl Load_Planet_Summary_Pictures(void) {
        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        screen->pics[0] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 0, MOX::screen_seg);
        screen->pics[1] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 1, MOX::screen_seg);
        screen->pics[2] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 2, MOX::screen_seg);
        screen->pics[3] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 3, MOX::screen_seg);

        RUSS::Mox_Update();

        screen->pics[4] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 4, MOX::screen_seg);
        screen->pics[5] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 5, MOX::screen_seg);
        screen->pics[6] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 6, MOX::screen_seg);
        screen->pics[7] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 7, MOX::screen_seg);

        RUSS::Mox_Update();

        screen->pics[8] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 8, MOX::screen_seg);

        send_colony_ship_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 9, MOX::screen_seg);
        send_outpost_ship_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 10, MOX::screen_seg);

        screen->pics[9] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 11, MOX::screen_seg);
        screen->pics[10] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 12, MOX::screen_seg);
        screen->pics[12] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 13, MOX::screen_seg);

        RUSS::Mox_Update();

        screen->pics[11] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 14, MOX::screen_seg);

        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        for (int i = 0; i < 11; ++i) {
            MOX::planet_summary_galaxy_star_seg[i] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 15 + i, MOX::screen_seg);
            RUSS::Mox_Update();
        }

        allocate::Get_Remaining_Block_Space(MOX::screen_seg);
        Load_Player_Icon();
        palette::Set_Palette_Changes(0, 255);
        reduce::Clear_Reduced_Palette_Groups();
        allocate::Get_Remaining_Block_Space(MOX::screen_seg);
    }

    void __cdecl Determine_Button_Status() {
        Build_Ship_Summary();

        draw_colony_button_status = 0;
        draw_outpost_button_status = 0;

        if (MOX::scroll_bar->total_rows > 0 && EVENTS::Event_Check_Hyperspace_Flux() == 0) {
            if (Send_Outpost_Button_Selectable()) {
                if (Send_Colony_Button_Selectable()) {
                    draw_colony_button_status = 2;
                    send_choice = 0;

                    if (!Send_Outpost_Button_Selectable()) {
                        return;
                    }

                    draw_outpost_button_status = 1;
                    return;
                }
            }

            if (Send_Colony_Button_Selectable()) {
                if (!Send_Outpost_Button_Selectable()) {
                    draw_colony_button_status = 2;
                    send_choice = 0;
                    return;
                }
            }

            if (!Send_Colony_Button_Selectable() && Send_Outpost_Button_Selectable()) {
                draw_outpost_button_status = 2;
                send_choice = 1;
            }
        }
    }

    void __cdecl Check_For_Colony_And_Outpost_Ships() {
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
                            // The original binary sets the colony-found flag here as well,
                            // and the early-exit check depends on preserving that behavior.
                            found_colony_ship = 1;
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
        game_events::Invalidate_Settlement_View();
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

    void __cdecl Scan_Summary_Box_Fields(int16_t input_field_id, int16_t clicked_field_id, int16_t* last_input_field_id_ptr) {
        int first_row = (int)MOX::scroll_bar->first_visible_row;
        int total_rows = (int)MOX::scroll_bar->total_rows;
        int max_visible = first_row + 8;
        int end_idx = (first_row + total_rows < max_visible) ? (first_row + total_rows) : max_visible;

        PLNTSUM::can_reach_scanned_planet = false;
        PLNTSUM::box_field_scanned = -1;

        int count = end_idx - first_row;
        if (count > 0) {
            for (int16_t i = 0; i < (int16_t)count; ++i) {
                int current_row = first_row + i;
                int16_t explored_idx = PLNTSUM::planet_summary[current_row].explored;

                if (explored_idx != -1) {
                    int16_t star_idx = MOX::planet[explored_idx].star_index;

                    // Handle interaction: Click on fields/icons
                    if (PLNTSUM::screen->fields[i] == clicked_field_id) {
                        if (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1) {
                            int16_t planet_id = PLNTSUM::Planet_Summary_Id(explored_idx);
                            int16_t ship_id = PLNTSUM::Nearest_Ship_Id(planet_id);
                            if (ship_id > -1) {
                                PLNTSUM::Send_Nearest_Ship(explored_idx);
                                PLNTSUM::Determine_Button_Status();
                                PLNTSUM::Reset_Ship_To_Star_Distances();
                            }
                        }
                    } else if (PLNTSUM::screen->colony_icons[i] == clicked_field_id) {
                        PLNTSUM::Undo_Send_Order(explored_idx, SHIP_TYPE_COLONY);
                        PLNTSUM::Determine_Button_Status();
                        PLNTSUM::Reset_Ship_To_Star_Distances();
                    } else if (PLNTSUM::screen->output_icons[i] == clicked_field_id) {
                        PLNTSUM::Undo_Send_Order(explored_idx, SHIP_TYPE_OUTPOST);
                        PLNTSUM::Determine_Button_Status();
                        PLNTSUM::Reset_Ship_To_Star_Distances();
                    }
                    // Handle interaction: Hover/Input on fields
                    else if (PLNTSUM::screen->fields[i] == input_field_id && current_row < total_rows) {
                        PLNTSUM::box_field_scanned = (int8_t)i;
                        if (explored_idx > -1) {
                            int16_t planet_id = PLNTSUM::Planet_Summary_Id(explored_idx);
                            int16_t ship_id = PLNTSUM::Nearest_Ship_Id(planet_id);
                            if (ship_id != -1 && (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1)) {
                                PLNTSUM::can_reach_scanned_planet = true;
                            }
                        }

                        if (*last_input_field_id_ptr != input_field_id) {
                            MOX::scanned_field = star_idx + 1000 /* 0x3E8 */;
                            int16_t target_id = -1;
                            if (PLNTSUM::send_choice == 0) {
                                target_id = PLNTSUM::planet_summary[current_row].best_colony_ship_idx;
                            } else if (PLNTSUM::send_choice == 1) {
                                target_id = PLNTSUM::planet_summary[current_row].best_outpost_ship_idx;
                            }

                            if (target_id != -1) {
                                int16_t targets[2];
                                targets[0] = target_id;
                                targets[1] = -1;
                                SHIPMOVE::Ships_Try_To_Move_To(targets, star_idx, &MOX::g_ship_move_info);
                                MOX::scanned_field = star_idx;
                            }
                            *last_input_field_id_ptr = input_field_id;
                        }
                    } else if (PLNTSUM::screen->colony_icons[i] == input_field_id || PLNTSUM::screen->output_icons[i] == input_field_id) {
                        PLNTSUM::can_reach_scanned_planet = true;
                    }
                }
            }
        }
    }

    void __cdecl Load_Player_Icon() {
        int entry_num = (int)(short)((uint8_t)MOX::player[MOX::PLAYER_NUM].color + 0x4C);

        s_animation_header* player_icon = (s_animation_header*)farload::Far_Reload_Next(
            PLNTSUM::PTR_s_PLNTSUM_LBX_00556AC0,
            entry_num,
            MOX::screen_seg);

        PLNTSUM::screen->pics[13] = player_icon;
    }

    void __cdecl Wrapper_For_Planet_Summary_Galaxy_Map() {
        short star_x, star_y;
        uint32_t color = 0xE2;

        graphics::Set_Window(443, 17, 623, 133);
        video::Clip_On();

        if (MOX::scanned_field != -1) {
            int16_t star_idx = MOX::scanned_field;
            if (star_idx < 1000) {
                Draw_Nearest_Ship_Directional_Line(star_idx);
            } else if (star_idx < MOX::NUM_STARS + 1000) {
                Draw_Nearest_Ship_Directional_Line(star_idx - 1000);
            }

            if (star_idx >= 1000 && star_idx < MOX::NUM_STARS + 1000) {
                star_idx -= 1000;
            }

            MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx, 443, 17, 180, 116, 0, 0, 2, &star_x, &star_y);

            int8_t owner = MOX::star[star_idx].owner;
            if (owner > -1) {
                color = (uint32_t)(MOX::player[owner].color * 6 + 212);
            }

            graphics::Box(star_x - 1, star_y - 1, 9, 9, 1, (uint8_t)color);
        }

        for (int16_t i = 0; i < PLNTSUM::n_ships_in_summary; ++i) {
            bool found_colony = Summary_Found_Colony_Ship(i);
            bool found_outpost = Summary_Found_Outpost_Ship(i);

            if (found_colony || found_outpost) {
                short marker_x, marker_y;
                Get_Colony_Outpost_Marker_XY(i, &marker_x, &marker_y);
                Print_Colony_Outpost_Marker(marker_x, marker_y, i, found_colony, found_outpost);
            }
        }

        video::Clip_Off();
        graphics::Reset_Window();

        MOVEBOX::Draw_Galaxy_Map_Box(nullptr, nullptr, 443, 17, 180, 116, 0, 0, 0, 0, 2, -1);

        PLNTSUM::planet_summary->scan_highlight_pulse += (short)(PLNTSUM::planet_summary->scan_highlight_pulse_step * 2);

        int8_t step = PLNTSUM::planet_summary->scan_highlight_pulse_step;
        if (step > 0) {
            if (PLNTSUM::planet_summary->scan_highlight_pulse >= 3) {
                PLNTSUM::planet_summary->scan_highlight_pulse_step = -step;
            }
        } else if (step < 0) {
            if (PLNTSUM::planet_summary->scan_highlight_pulse <= 0) {
                PLNTSUM::planet_summary->scan_highlight_pulse_step = -step;
            }
        }
    }

    int16_t __cdecl Nearest_Ship_Id(int16_t index) {
        if (PLNTSUM::send_choice == 0) {
            return PLNTSUM::planet_summary[index].best_colony_ship_idx;
        }

        if (PLNTSUM::send_choice == 1) {
            return PLNTSUM::planet_summary[index].best_outpost_ship_idx;
        }

        return -1;
    }

    void __cdecl Draw_Nearest_Ship_Directional_Line(int16_t star_idx) {
        int16_t summary_idx;
        s_colors colors;
        uint8_t colony_selectable;
        uint8_t outpost_selectable;
        bool colony_active = false;
        bool outpost_active = false;
        int16_t ship_idx;
        int16_t marker_x, marker_y;
        int16_t star_x, star_y;
        int16_t dist;

        summary_idx = Summary_Planet_Index(star_idx);

        colors.colors[0] = 0xE2;
        colors.colors[1] = 0xE3;
        colors.colors[2] = 0xE4;
        colors.colors[3] = 0xE5;
        colors.colors[4] = 0xE4;
        colors.colors[5] = 0xE3;
        colors.colors[6] = 0xE3;
        colors.colors[7] = 0xE2;

        colony_selectable = (uint8_t)Send_Colony_Button_Selectable();
        outpost_selectable = (uint8_t)Send_Outpost_Button_Selectable();

        if (summary_idx == -1) {
            return;
        }

        if (!colony_selectable && !outpost_selectable) {
            return;
        }

        if (PLNTSUM::draw_colony_button_status == 1) {
            colony_active = true;
        } else if (PLNTSUM::draw_outpost_button_status == 1) {
            outpost_active = true;
        } else if (!colony_selectable && outpost_selectable) {
            outpost_active = true;
        } else {
            colony_active = true;
        }

        if (colony_active) {
            ship_idx = PLNTSUM::planet_summary[summary_idx].best_colony_ship_idx;
        } else {
            ship_idx = PLNTSUM::planet_summary[summary_idx].best_outpost_ship_idx;
        }

        if (ship_idx == -1) {
            return;
        }

        bool ship_status_is_one = (MOX::ship[ship_idx].status == 1);

        Get_Colony_Outpost_Marker_XY(PLNTSUM::planet_summary[summary_idx].ship_summary_idx, &marker_x, &marker_y);

        if (colony_active && ship_idx > -1 && PLNTSUM::can_reach_star[star_idx] != 0 && !ship_status_is_one) {
            MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx, 0x1BB, 0x11, 0xB4, 0x74, 0, 0, 0, &star_x, &star_y);
            dist = special::Range(marker_x, marker_y, star_x + 4, star_y + 3);
            if (dist > 10) {
                SHIPS::Draw_Directional_Multi_Colored_Line(marker_x + 7, marker_y + 2, star_x + 2, star_y + 2, &colors, MOX::multi_colored_line_start);

                int32_t next_idx = (int32_t)MOX::multi_colored_line_start + 1;
                int32_t abs_val = (next_idx >= 0) ? next_idx : -next_idx;
                int32_t result = abs_val % 8;
                MOX::multi_colored_line_start = (int16_t)((next_idx >= 0) ? result : -result);
            }
        }

        if (outpost_active && ship_idx > -1 && PLNTSUM::can_reach_star[star_idx] != 0 && !ship_status_is_one) {
            MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx, 0x1BB, 0x11, 0xB4, 0x74, 0, 0, 0, &star_x, &star_y);
            dist = special::Range(marker_x, marker_y, star_x + 4, star_y + 3);
            if (dist > 10) {
                SHIPS::Draw_Directional_Multi_Colored_Line(marker_x + 8, marker_y + 2, star_x + 2, star_y + 2, &colors, MOX::multi_colored_line_start);

                int32_t next_idx = (int32_t)MOX::multi_colored_line_start + 1;
                int32_t abs_val = (next_idx >= 0) ? next_idx : -next_idx;
                int32_t result = abs_val % 8;
                MOX::multi_colored_line_start = (int16_t)((next_idx >= 0) ? result : -result);
            }
        }
    }

    void __cdecl Undo_Send_Order(int16_t planet_idx, int16_t order_type) {
        int16_t ship_idx = -1;
        int16_t ships[2];
        bool found = false;
        char buffer1[200];
        char buffer2[200];
        char eta_str[40];

        ships[0] = -1;
        ships[1] = -1;

        for (int16_t i = 0; i < MOX::scroll_bar->total_rows; ++i) {
            if (found) break;

            if (PLNTSUM::planet_summary[i].explored == planet_idx) {
                if (order_type == SHIP_TYPE_COLONY) {
                    ship_idx = PLNTSUM::planet_summary[i].best_colony_ship_idx;
                } else {
                    ship_idx = PLNTSUM::planet_summary[i].best_outpost_ship_idx;
                }

                if (!MAINSCR::Ship_Can_Be_Selected(ship_idx)) {
                    int16_t msg_id = (order_type == SHIP_TYPE_COLONY) ? 0x149 /* HESTR_149_YOU_ARE_UNABLE_TO_UNDO */ : 0x14A /* HESTR_14A_YOU_ARE_UNABLE_TO_UNDO */;
                    const char* msg = HAROLD::H_Message(msg_id);
                    strlcpy(buffer1, msg, sizeof(buffer1));
                    char* planet_name = HACCESS::Get_Planet_Name_No_Prime(planet_idx);
                    snprintf(buffer2, sizeof(buffer2), buffer1, planet_name);
                    GENDRAW::Warning_Box(buffer2);
                }
                found = true;
            }
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
                PLNTSUM::Update_Ship_Etas();
                SHIPS::Clear_Auto_Colonization_Flag(&MOX::ship[ship_idx]);
            }
            PLNTSUM::screen->refresh_fields = true;
        }
    }

    void __cdecl Print_Colony_Outpost_Marker(int16_t x, int16_t y, int16_t ship_summary_idx, int8_t colony, int8_t outpost) {
        (void)ship_summary_idx;
        s_colors inactive_colors;
        s_colors active_colors;

        inactive_colors.colors[0] = 0;
        inactive_colors.colors[1] = ':';
        inactive_colors.colors[2] = ':';
        inactive_colors.colors[3] = 'C';
        inactive_colors.colors[4] = 'B';
        inactive_colors.colors[5] = 'A';
        inactive_colors.colors[6] = '?';
        inactive_colors.colors[7] = '?';
        active_colors.colors[0] = 0;

        for (int16_t i = 1; i < 8; ++i) {
            int16_t color_idx = PLNTSUM::planet_summary->scan_highlight_pulse + i;
            if (color_idx < 1) {
                color_idx = 1;
            }
            if (color_idx > 7) {
                color_idx = 7;
            }
            active_colors.colors[i] = inactive_colors.colors[color_idx];
        }

        if (outpost != 0 && colony != 0) {
            int16_t cur_x = x + 3;
            int16_t cur_y = y - 1;

            if (MOX::scanned_field > -1 && MOX::scanned_field < 1000) {
                if (PLNTSUM::send_choice == 1) {
                    fonts::Set_Font_Style(0, &active_colors);
                    int16_t bottom = fonts::Get_Font_Height() + cur_y;
                    int16_t right = fonts::Get_String_Width("oc") + cur_x;
                    graphics::Fill(cur_x, cur_y, right, bottom, 1);
                    fonts::Print(cur_x, cur_y, "o");
                    fonts::Set_Font_Style(0, &inactive_colors);
                    int16_t text_x = fonts::Get_String_Width("o") + cur_x;
                    fonts::Print(text_x, cur_y, "c");
                    return;
                }

                if (PLNTSUM::send_choice == 0) {
                    fonts::Set_Font_Style(0, &inactive_colors);
                    int16_t bottom = fonts::Get_Font_Height() + cur_y;
                    int16_t right = fonts::Get_String_Width("oc") + cur_x;
                    graphics::Fill(cur_x, cur_y, right, bottom, 1);
                    fonts::Print(cur_x, cur_y, "o");
                    fonts::Set_Font_Style(0, &active_colors);
                    int16_t text_x = fonts::Get_String_Width("o") + cur_x;
                    fonts::Print(text_x, cur_y, "c");
                    return;
                }
            }

            fonts::Set_Font_Style(0, &inactive_colors);
            fonts::Print(cur_x, cur_y, "oc");
        } else if (outpost != 0) {
            int16_t cur_x = x + 6;
            int16_t cur_y = y - 1;

            if (MOX::scanned_field > -1 && MOX::scanned_field < 1000) {
                fonts::Set_Font_Style(0, &active_colors);
                int16_t bottom = fonts::Get_Font_Height() + cur_y;
                int16_t right = fonts::Get_String_Width("o") + cur_x;
                graphics::Fill(cur_x, cur_y, right, bottom, 1);
            } else {
                fonts::Set_Font_Style(0, &inactive_colors);
            }

            fonts::Print(cur_x, cur_y, "o");
        } else if (colony != 0) {
            int16_t cur_x = x + 6;
            int16_t cur_y = y - 1;

            if (MOX::scanned_field > -1 && MOX::scanned_field < 1000) {
                fonts::Set_Font_Style(0, &active_colors);
                int16_t bottom = fonts::Get_Font_Height() + cur_y;
                int16_t right = fonts::Get_String_Width("c") + cur_x;
                graphics::Fill(cur_x, cur_y, right, bottom, 1);
            } else {
                fonts::Set_Font_Style(0, &inactive_colors);
            }

            fonts::Print(cur_x, cur_y, "c");
        }
    }

    void __cdecl Get_Colony_Outpost_Marker_XY(int16_t summary_idx, int16_t* out_x, int16_t* out_y) {
        s_plntsum_ship* summary = &PLNTSUM::ship_summary[summary_idx];
        int16_t ship_idx = summary->outpost_ship_idx;

        if (ship_idx <= summary->colony_ship_idx) {
            ship_idx = summary->colony_ship_idx;
        }

        s_ship_data* ship = &MOX::ship[ship_idx];
        int16_t location = ship->location;

        bool is_space_location = (location >= SHIP_LOCATION_MOVING_OFFSET && location < SHIP_LOCATION_WORMHOLE_OFFSET);
        bool is_travel_location = (location >= SHIP_LOCATION_WORMHOLE_OFFSET && location < SHIP_LOCATION_LIMIT);

        if (is_space_location) {
            MOVEBOX::Get_Galaxy_Map_Space_XY(summary->ship_icon_id, 443, 17, 180, 116, out_x, out_y);
            *out_x += 2;
            return;
        }

        int16_t star_idx = location;
        if (is_travel_location) {
            star_idx = HAROLD::XY_To_Star_Id(ship->x, ship->y);
        }

        MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx, 443, 17, 180, 116, 0, 0, 2, out_x, out_y);
        *out_x += 2;
    }

    void Draw_Rotating_Planets() {
        int16_t first_row = MOX::scroll_bar->first_visible_row;
        int16_t total_rows = MOX::scroll_bar->total_rows;
        int32_t upper_bound = first_row + total_rows;

        // Draw at most 8 rows starting from the scrollbar position
        if (first_row + 8 < upper_bound) {
            upper_bound = first_row + 8;
        }

        int32_t count = (int16_t)upper_bound - first_row;
        if (count > 0) {
            for (int16_t i = 0; i < (int16_t)count; ++i) {
                int16_t explored_planet_idx = PLNTSUM::planet_summary[first_row + i].explored;

                if (explored_planet_idx != -1) {
                    s_planet_data* planet = &MOX::planet[explored_planet_idx];

                    // Planet animation index is determined by climate type and planet size
                    int16_t anim_idx = (int16_t)planet->climate * PLANET_SIZE_COUNT + (int16_t)planet->size;
                    s_animation_header* anim = PLNTSUM::p_rotating_planet[anim_idx];

                    int16_t width = animate::Get_Width(anim);
                    int16_t height = animate::Get_Height(anim);

                    // Center the planet image within the designated list cell area (86x25)
                    int16_t x = (86 /* 0x56 */ - width) / 2 + 18 /* 0x12 */;
                    int16_t y = PLNTSUM::plntscrn_delta_y[i] + (25 /* 0x19 */ - height) / 2 + 45 /* 0x2d */;

                    animate::Draw(x, y, anim);
                }
            }
        }
    }

    void __cdecl Fill_Summary_Screen_Scroll_Bar(void) {
        int16_t y_base;
        uint8_t color = 150 /* 0x96 */;
        int16_t x = 423 /* 0x1a7 */;

        graphics::Set_Window(421 /* 0x1a5 */, 37 /* 0x25 */, 431 /* 0x1af */, 444 /* 0x1bc */);
        video::Clip_On();

        // Calculate visual offset for the scroll thumb based on scrollbar position
        y_base = MOX::scroll_bar->track_len_px - (MOX::scroll_bar->thumb_height_px / 2) - MOX::scroll_bar->thumb_center_y;

        for (int16_t i = 0; i < 10; ++i) {
            int16_t y1 = y_base + 38 /* 0x26 */;
            int16_t y2 = MOX::scroll_bar->thumb_height_px + y_base + 37 /* 0x25 */;

            line::Line(x, y1, x, y2, color);

            // Assembly implementation of parity check: alternates color decrement
            if ((i % 2) == 0) {
                color--;
            }
            x++;
        }

        video::Clip_Off();
        graphics::Reset_Window();

        // Redraw the scroll background frame or overlay
        animate::Draw(421 /* 0x1a5 */, 37 /* 0x25 */, PLNTSUM::screen->pics[12]);

        PLNTSUM::recalc_first_planet = false;
    }

    void __cdecl Load_Planet_Pictures() {
        for (int16_t i = 0; i < PLANET_PICTURE_COUNT; ++i) {
            s_animation_header* segment = Load_Planet_Segment(i);
            p_rotating_planet[i] = segment;
            RUSS::Mox_Update();
        }
    }

    void __cdecl Draw_Mini_Planet_Summary_Screen() {
        game_events::s_settlement_frame settlements;
        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            video::Set_Page_Back();
            graphics::Fill(0, 0, 639, 479, 0);
            animate::Draw(0, 0, PLNTSUM::screen->pics[0]);
            video::Copy_Back_To_Off();
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            video::Set_Page_Off();
        }

        graphics::Fill(0, 0, 639, 479, 0);
        animate::Draw(0, 0, PLNTSUM::screen->pics[0]);
        graphics::Reset_Window();
        video::Clip_Off();

        int16_t input_id = fields::Auto_Input();
        if ((int16_t)PLNTSUM::plntsum_scroll_bar == input_id) {
            PLNTSUM::recalc_first_planet = true;
        }

        PLNTSUM::Draw_Rotating_Planets();
        PLNTSUM::Wrapper_For_Planet_Summary_Galaxy_Map();
        SCROLL::Draw_Generic_Vertical_Scroll_Bar(MOX::scroll_bar, PLNTSUM::Fill_Summary_Screen_Scroll_Bar);
        PLNTSUM::Summary_Screen_Print_Data();

        if (MOX::scanned_field > -1) {
            PLNTSUM::Summary_Screen_Print_Scanned();
        }

        for (int16_t i = 0; i < (int16_t)PLNTSUM::n_ships_in_summary; ++i) {
            if (PLNTSUM::fade_flag != 0) {
                break;
            }

            int16_t ship_icon_id = PLNTSUM::ship_summary[i].ship_icon_id;
            int16_t node_idx = MOX::ship_icon[ship_icon_id].node_idx;
            int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
            s_ship_data* ship_ptr = &MOX::ship[ship_idx];

            if (ship_ptr->location >= SHIP_LOCATION_MOVING_OFFSET) {
                uint8_t type = ship_ptr->d.ship_type;
                if (type == SHIP_TYPE_COLONY || type == SHIP_TYPE_OUTPOST) {
                    PLNTSUM::Draw_Nearest_Ship_Directional_Line(ship_ptr->location);
                }
            }
        }

        animate::Set_Animation_Frame(PLNTSUM::send_colony_ship_button_seg, (int)PLNTSUM::draw_colony_button_status);
        animate::Draw(454, 386, PLNTSUM::send_colony_ship_button_seg);

        animate::Set_Animation_Frame(PLNTSUM::send_outpost_ship_button_seg, (int)PLNTSUM::draw_outpost_button_status);
        animate::Draw(454, 413, PLNTSUM::send_outpost_ship_button_seg);

        PLNTSUM::skip_draw_planets = 0;
    }

    void __cdecl Summary_Screen_Print_Scanned() {
        char msg_buffer[200];
        char suffix_buffer[100];
        char star_name_buffer[100];
        int8_t status = 0; // 0: neutral, 1: red, 2: green
        int16_t scanned_field = MOX::scanned_field;
        int16_t planet_idx = Summary_Planet_Index(scanned_field);

        if (PLNTSUM::planet_summary[planet_idx].blocked == 1) {
            snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(324 /* HESTR_144_BLACK_HOLE_BLOCKS_TRAV */), MOX::star[scanned_field].name);
            fonts::Set_Font_Style(1, &PLNTSUM::red_colors);
            int16_t y = (fonts::Get_Font_Height() / 2) + 143 /* 0x8F */;
            fonts::Print_Centered(533 /* 0x215 */, y, msg_buffer);
            return;
        }

        if (scanned_field < 1000 || scanned_field >= (MOX::NUM_STARS + 1000)) {
            if (PLNTSUM::send_choice != 0 && PLNTSUM::send_choice != 1) {
                return;
            }

            bool visited = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, scanned_field) || HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);

            if (MOX::g_ship_move_info.moving != 0) {
                status = 2;
                if (visited) {
                    if (MOX::g_ship_move_info.turns_left == 1) {
                        snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(105 /* HESTR_069_D_TURN_TO_S */), (int)(uint8_t)MOX::g_ship_move_info.turns_left, MOX::star[scanned_field].name);
                    } else {
                        snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(106 /* HESTR_06A_D_TURNS_TO_S */), (int)(uint8_t)MOX::g_ship_move_info.turns_left, MOX::star[scanned_field].name);
                    }
                } else {
                    snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(332 /* HESTR_14C_ETA_D_TURNS */), (int)(uint8_t)MOX::g_ship_move_info.turns_left);
                }
            } else {
                status = 1;
                strlcpy(msg_buffer, HAROLD::H_Message(328 /* HESTR_148_OUT_OF_RANGE */), sizeof(msg_buffer));
            }
        } else {
            int16_t star_id = scanned_field - 1000;
            bool visited = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_id) || HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);

            if (MOX::star[star_id].spectral_class == STAR_CLASS_BLACK_HOLE) {
                strlcpy(msg_buffer, HAROLD::H_Message(325 /* HESTR_145_BLACK_HOLE */), sizeof(msg_buffer));
                status = 1;
            } else if (!visited) {
                if (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1) {
                    if (PLNTSUM::can_reach_star[star_id] != 0) {
                        int16_t eta = Get_ETA_To_Star(star_id);
                        if (eta > 0) {
                            if (eta == 1) {
                                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(331 /* HESTR_14B_ETA_D_TURN */), (int)eta);
                            } else {
                                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(332 /* HESTR_14C_ETA_D_TURNS */), (int)eta);
                            }
                        }
                        status = 2;
                    } else {
                        status = 1;
                        strlcpy(msg_buffer, HAROLD::H_Message(327 /* HESTR_147_STAR_SYSTEM_UNREACHABL */), sizeof(msg_buffer));
                    }
                } else {
                    strlcpy(msg_buffer, HAROLD::H_Message(148 /* HESTR_094_UNEXPLORED */), sizeof(msg_buffer));
                    status = 0;
                }
            } else {
                strlcpy(star_name_buffer, MOX::star[star_id].name, sizeof(star_name_buffer));
                suffix_buffer[0] = '\0';
                if (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1) {
                    if (PLNTSUM::can_reach_star[star_id] == 0) {
                        strlcpy(suffix_buffer, HAROLD::H_Message(326 /* HESTR_146_CANNOT_REACH */), sizeof(suffix_buffer));
                        status = 1;
                    } else {
                        int16_t eta = Get_ETA_To_Star(star_id);
                        if (eta > 0) {
                            if (eta == 1) {
                                snprintf(suffix_buffer, sizeof(suffix_buffer), HAROLD::H_Message(336 /* HESTR_150_D_TURN_TO */), (int)eta);
                            } else {
                                snprintf(suffix_buffer, sizeof(suffix_buffer), HAROLD::H_Message(337 /* HESTR_151_D_TURNS_TO */), (int)eta);
                            }
                        }
                        status = 2;
                    }
                }
                snprintf(msg_buffer, sizeof(msg_buffer), "%s%s", suffix_buffer, star_name_buffer);
            }
        }

        s_colors* p_colors;
        if (status == 1) {
            p_colors = &PLNTSUM::red_colors;
        } else if (status == 2) {
            p_colors = &PLNTSUM::green_colors;
        } else {
            p_colors = &PLNTSUM::hi_neutral_colors;
        }

        fonts::Set_Font_Style(2, p_colors);
        int16_t y = ((fonts::Get_Font_Height() + 2) / 2) + 143 /* 0x8F */;
        fonts::Print_Centered(533 /* 0x215 */, y, msg_buffer);
    }

    void Planet_Summary_Screen() {
        game_events::s_settlement_view_scope settlement_view(MOX::PLAYER_NUM);
        int16_t exit_flag = 0;
        int16_t last_scanned_status = -1;
        char warning_msg[100];

        MAINSCR::Allocate_First_Screen_Seg(8);
        fade_flag = 1;
        fields::Set_Field_Outline_Color(215 /* 0xd7 */);
        EVANHELP::Set_PlntSum_Screen_Help_List();
        n_ships_in_summary = 0;
        send_choice = -1;
        COLONY::using_colony_screen_palette = 0;
        MOX::scanned_field = -1;
        planet_summary_refresh_marker = 0;
        last_scanned_status = -1;
        recalc_first_planet = false;
        MOX::multi_colored_line_start = 0;
        MOX::galaxy_star_anim_count = 0;
        last_ship_moved = -1;
        planet_summary_reserved_counter = 0;
        planet_summary_state_marker = 0;
        skip_draw_planets = 0;
        can_reach_scanned_planet = false;
        draw_colony_button_status = 0;
        DESBOX::incr_value = 1;
        draw_outpost_button_status = 0;

        video::Set_Refresh_Full();
        Check_For_Colony_And_Outpost_Ships();

        if (found_colony_ship != 0) {
            draw_colony_button_status = 1;
        }
        if (found_outpost_ship != 0) {
            draw_outpost_button_status = 1;
        }

        fields::Assign_Auto_Function(Draw_Mini_Planet_Summary_Screen, 1);
        fields::Clear_Fields();
        MISC::Fade_To_Black();
        fonts::Load_Palette(6, 0, 254 /* 0xfe */);

        Allocate_Space_For_Summary_Screen();
        Load_Planet_Summary_Pictures();
        Add_Plntsum_Fields();

        planet_summary->scan_highlight_pulse = 0;
        planet_summary->scan_highlight_pulse_step = 1;

        Set_Planet_Explored_Flags();
        Filter_Explored_Planets(&MOX::scroll_bar->total_rows);

        MOX::scroll_bar->thumb_center_y = 407 /* 0x197 */;
        MOX::scroll_bar->visible_rows = 8;
        MOX::scroll_bar->track_len_px = 407 /* 0x197 */;
        MOX::scroll_bar->thumb_height_px = SCROLL::Get_Vertical_Scroll_Bar_Height(MOX::scroll_bar);
        MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px - (MOX::scroll_bar->thumb_height_px / 2);
        MOX::scroll_bar->last_thumb_center_y = -1;
        MOX::scroll_bar->type = 2;
        MOX::scroll_bar->row_step = 1;

        p_rotating_planet = (s_animation_header**)allocate::Allocate_Next_Block_Aligned(
            MOX::screen_seg, sizeof(*p_rotating_planet) * PLANET_PICTURE_COUNT, alignof(s_animation_header*));
        Load_Planet_Pictures();
        Determine_Button_Status();

        do {
            settlement_view.Initialized();
            int16_t prev_sort_choice = sort_choice;
            int16_t input_id = fields::Get_Input();
            int16_t scanned_id = fields::Scan_Input();
            screen->scanned_input = scanned_id;
            int16_t current_scanned = screen->scanned_input;

            if (screen->esc_button == input_id) {
                MOX::current_screen = SCREEN_MAIN;
                exit_flag = 1;
            }

            if (sort_choice != prev_sort_choice) {
                if (input_id == screen->buttons[0] || input_id == screen->buttons[1] || input_id == screen->buttons[2]) {
                    Sort_Planet_Summary();
                    MOX::scanned_field = -1;
                }
            }

            if (input_id == screen->buttons[3] || input_id == screen->buttons[4] || input_id == screen->buttons[5] ||
                input_id == screen->buttons[6] || input_id == screen->buttons[7]) {
                Filter_Explored_Planets(&MOX::scroll_bar->total_rows);
                last_scanned_status = -1;
                MOX::scroll_bar->first_visible_row = 0;
                if (MOX::scroll_bar->total_rows > 0) {
                    MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px;
                }

                if (Send_Colony_Button_Selectable() || Send_Outpost_Button_Selectable()) {
                    Build_Ship_Summary();
                }
            }

            if (input_id == screen->up_button && MOX::scroll_bar->total_rows > 8) {
                SCROLL::Scroll_Up_One_Item(MOX::scroll_bar);
                last_scanned_status = -1;
            }

            if (input_id == screen->dn_button && MOX::scroll_bar->total_rows > 8) {
                SCROLL::Scroll_Down_One_Item(MOX::scroll_bar);
                last_scanned_status = -1;
            }

            if (input_id == plntsum_scroll_bar) {
                recalc_first_planet = true;
                last_scanned_status = -1;
            }

            uint8_t handled_star = 0;
            for (int16_t i = 0; i < MOX::NUM_STARS && !handled_star; ++i) {
                if (screen->planet_icons[i] == input_id && MOX::scroll_bar->total_rows > 8) {
                    int16_t row = First_Planet_In_Summary_List(i);
                    if (row <= -1) {
                        s_star_data* p_star = &MOX::star[i];
                        const char* msg_fmt = HAROLD::H_Message(0x14f /* HESTR_14F_NO_PLANETS_IN_THE_S_ST */);
                        snprintf(warning_msg, sizeof(warning_msg), msg_fmt, p_star);
                        GENDRAW::Warning_Box(warning_msg);
                    } else {
                        MOX::scroll_bar->first_visible_row = row;
                        MOX::scroll_bar->thumb_center_y = ((-407 / (int)MOX::scroll_bar->total_rows) - 1) * MOX::scroll_bar->first_visible_row + 407;
                    }
                    handled_star = 1;
                }
                if (screen->planet_icons[i] == current_scanned) {
                    MOX::scanned_field = i + 1000;
                    handled_star = 1;
                }
            }

            Scan_Summary_Box_Fields(current_scanned, input_id, &last_scanned_status);

            if (input_id == screen->buttons[8] && draw_colony_button_status != 0) {
                if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
                    const char* flux_msg = HAROLD::H_Message(0x13f);
                    GENDRAW::Warning_Box(flux_msg);
                    draw_colony_button_status = 1;
                    send_choice = -1;
                } else {
                    if (draw_colony_button_status == 2) {
                        draw_colony_button_status = 1;
                        send_choice = -1;
                    } else {
                        draw_colony_button_status = 2;
                        draw_outpost_button_status = 1;
                        send_choice = 0;
                    }
                }
            }

            if (input_id == screen->buttons[9] && draw_outpost_button_status != 0) {
                if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
                    const char* flux_msg = HAROLD::H_Message(0x13f);
                    GENDRAW::Warning_Box(flux_msg);
                    draw_outpost_button_status = 1;
                    send_choice = -1;
                } else {
                    if (draw_outpost_button_status == 2) {
                        draw_outpost_button_status = 1;
                        send_choice = -1;
                    } else {
                        draw_outpost_button_status = 2;
                        draw_colony_button_status = 1;
                        send_choice = 1;
                    }
                }
            }

            if (screen->refresh_fields) {
                fields::Clear_Fields();
                Add_Plntsum_Fields();
                screen->refresh_fields = false;
            }

            if (exit_flag == 0) {
                settlement_view.Initialized();
                if (fade_flag != 0) {
                    video::Set_Page_Off();
                    Draw_Mini_Planet_Summary_Screen();
                    video::Copy_Off_To_Back();
                    ERIC::Fast_Fade_In();
                    fade_flag = 0;
                } else {
                    video::Set_Page_Off();
                    Draw_Mini_Planet_Summary_Screen();
                    RUSS::Mox_Sync_Update();
                }
            }

        } while (exit_flag == 0);

        settlement_view.Close();
        fields::Clear_Fields();
        fields::Disable_Draw_All_Fields();
        video::Set_Refresh_Stencil();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        MISC::Fade_To_Black();
        fields::Enable_Draw_All_Fields();
        MOX::scroll_bar = nullptr;
        if (MOVEBOX::Moveable_Box_Selected(2) != 0 && last_ship_moved != -1) {
            MOX::first_node_ptr_in_stack_ = -1;
        }
        MAINSCR::Draw_Mini_Main_Screen();
        fields::Set_Field_Outline_Color(10);
    }

    s_animation_header* __cdecl Load_Planet_Segment(int16_t planet_anim_idx) {
        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        s_animation_header* segment = (s_animation_header*)farload::Far_Reload_Next(
            PTR_s_PLNTSUM_LBX_00556AC0,
            (int32_t)(int16_t)(planet_anim_idx + 0x1A),
            MOX::screen_seg);

        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        return segment;
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

    void __cdecl Colony_Owner_Colors(int16_t colony_idx, s_colors* colors, int8_t highlight_type) {
        uint8_t glow_color;

        if (colony_idx != -1) {
            Player_Colors(MOX::colony[colony_idx].owner, colors, highlight_type, &glow_color);
        } else {
            Player_Colors(-1, colors, highlight_type, &glow_color);
        }
    }

    void __cdecl Player_Colors(int16_t player_idx, s_colors* colors_out, int8_t type, uint8_t* base_color_out) {
        if (player_idx == -1) {
            if (type == 1) {
                MOX::mox_font_colors_offset = 0x42;
                s_colors* c = MISC::Font_Colors2(4, 0x40, 0xFF);
                *colors_out = *c;
                *base_color_out = colors_out->colors[1];
            } else if (type == 2) {
                MOX::mox_font_colors_offset = 0x40;
                s_colors* c = MISC::Font_Colors2(4, 0x00, 0x8A);
                *colors_out = *c;
                *base_color_out = colors_out->colors[1];
            } else {
                MOX::mox_font_colors_offset = 0x43;
                s_colors* c = MISC::Font_Colors2(4, 0x40, 0x41);
                *colors_out = *c;
                *base_color_out = colors_out->colors[1];
            }
            return;
        }

        uint8_t base_color = static_cast<uint8_t>(MOX::player[player_idx].color * 6) - 0x31;
        *base_color_out = base_color;

        if (type == 0) {
            MOX::mox_font_colors_offset = base_color + 2;
            s_colors* c = MISC::Font_Colors2(4, base_color, base_color + 4);
            *colors_out = *c;
        } else if (type == 1) {
            MOX::mox_font_colors_offset = base_color + 3;
            s_colors* c = MISC::Font_Colors2(4, base_color, base_color + 5);
            *colors_out = *c;
        } else if (type == 2) {
            MOX::mox_font_colors_offset = base_color + 1;
            s_colors* c = MISC::Font_Colors2(4, base_color, base_color + 3);
            *colors_out = *c;
        }
    }

} // namespace PLNTSUM