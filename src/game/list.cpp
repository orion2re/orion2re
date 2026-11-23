#include "pch.h"

namespace LIST {

    void __cdecl Set_List_Page_Pointer(s_list_state* state) {
        uint8_t target_page = state->cur_page;
        state->cur_page = 1;

        if (target_page == 0) {
            target_page = 1;
        }

        s_list_item* items_start = state->items;
        s_list_item* current = items_start;
        s_list_item* items_end = &items_start[state->num_items];

        while (current < items_end) {
            if (target_page <= 1) {
                state->selected_item = current;
                return;
            }

            ++current;

            if (current >= items_end) {
                break;
            }

            if (current->stop_flag != 0) {
                --target_page;
                ++state->cur_page;
            }
        }

        state->selected_item = items_start;
    }

    void __cdecl Display_List_Text(s_list_state* state, s_list_item* item) {
        if (item == nullptr || item == (s_list_item*)-7) {
            return;
        }

        s_colors* current_color = state->group_colors[item->group_color_index];
        fonts::Set_Font_Style(state->group_font, current_color);
        BILL::Squeeze_Print(item->x, item->y, item->group_name, state->item_width);

        if (state->draw_app_cb != nullptr) {
            fonts::Set_Font_Style(state->item_font, current_color);

            if (item->app_count != 0) {
                for (uint16_t i = 0; i < (uint16_t)item->app_count; ++i) {
                    s_colors* app_color = state->group_colors[item->tech_app_status[i]];

                    if (app_color != current_color) {
                        current_color = app_color;
                        fonts::Set_Font_Style(state->item_font, current_color);
                    }

                    fonts::Set_Font_Style(state->item_font, current_color);

                    if (RACERPRT::race_report_jim_hack == 1) {
                        uint8_t app_id = item->tech_app_ids[i];
                        if (game::current_state.players.at(MOX::local_player.id).tech_applications[app_id] != TECH_RESEARCH_STATUS_RESEARCHED) {
                            fonts::Set_Font_Style(state->item_font, state->disabled_color);
                        }
                    }

                    if (state->draw_app_cb != nullptr) {
                        draw_app_fn draw_fn = (draw_app_fn)state->draw_app_cb;
                        draw_fn(item->app_base_x, item->app_y_offsets[i] + item->y, item->tech_app_ids[i]);
                    }
                }
            }
        }
    }

    void __cdecl Display_List_Page(s_list_state* state) {
        Set_List_Page_Pointer(state);

        s_list_item* item = state->selected_item;
        s_list_item* end = &state->items[state->num_items];

        while (item != end) {
            Display_List_Text(state, item);

            ++item;

            if (item >= end || item->stop_flag != 0) {
                break;
            }
        }
    }

    int16_t __cdecl Add_Fields_To_List_Page(s_list_state* state) {
        s_list_item* item = state->selected_item;
        s_list_item* end_item = &state->items[state->num_items];

        while (item < end_item) {
            if (item->app_count != 0) {
                for (uint16_t i = 0; i < item->app_count; ++i) {
                    int16_t field_id = fields::Add_Hidden_Field(
                        item->x,
                        state->app_ymin_offsets[i] + item->y,
                        item->app_x_max,
                        state->app_ymax_offsets[i] + item->y,
                        "",
                        0);

                    item->app_ids[i] = (uint8_t)field_id;
                }
            }

            if (item + 1 >= end_item || item[1].stop_flag != 0) {
                break;
            }
            ++item;
        }

        return fields::Get_Field_Counter() - 1;
    }

    void __cdecl Set_List_Up_Down_Field_Drawing(s_list_state* state, int16_t up_field_id, int16_t down_field_id) {
        if (state->cur_page < state->max_page) {
            fields::Enable_Draw_Field(down_field_id);
        } else {
            fields::Disable_Draw_Field(down_field_id);
        }

        if (state->cur_page > 1) {
            fields::Enable_Draw_Field(up_field_id);
        } else {
            fields::Disable_Draw_Field(up_field_id);
        }
    }

    int8_t __cdecl Next_List_Page(s_list_state* state) {
        if (state->cur_page < state->max_page) {
            state->cur_page++;
            LIST::Set_List_Page_Pointer(state);
            return 1;
        }

        return 0;
    }

    int8_t __cdecl Previous_List_Page(s_list_state* state) {
        if (state->cur_page > 1) {
            state->cur_page--;
            LIST::Set_List_Page_Pointer(state);
            return 1;
        }

        return 0;
    }

    s_list_item* __cdecl Set_Selected_List_Entry(s_list_state* state, int16_t target_field_id) {
        s_list_item* item = state->selected_item;
        s_list_item* end_item = &state->items[state->num_items];
        s_list_item* found_item = nullptr;

        while (item < end_item) {
            item->is_selectable = 0;

            if (found_item == nullptr) {
                for (uint8_t i = 0; i < item->app_count; ++i) {
                    if ((int16_t)(int8_t)item->app_ids[i] == target_field_id) {
                        item->is_selectable = 1;
                        item->current_app_index = i;
                        found_item = item;
                    }
                }
            }

            if (item + 1 >= end_item || item[1].stop_flag != 0) {
                break;
            }
            ++item;
        }

        return found_item;
    }

    s_list_item* __cdecl Get_Selected_List_Entry(s_list_state* state) {
        s_list_item* item = state->selected_item;
        s_list_item* end = &state->items[(uint8_t)state->num_items];

        while (item < end) {
            if (item->is_selectable != 0) {
                return item;
            }

            if (item + 1 >= end || (item + 1)->stop_flag != 0) {
                break;
            }

            ++item;
        }

        return nullptr;
    }

    void __cdecl Init_List_Data(s_list_state* state) {
        s_list_item* item = state->items;
        int16_t current_y = state->y;
        int16_t y_max = state->y_max;

        state->selected_item = state->items;
        state->max_page = 1;
        state->cur_page = 1;

        s_list_item* end_item = &state->items[state->num_items];

        do {
            item->x = state->x;
            item->app_base_x = state->x + (int16_t)state->app_x_offset;
            item->app_x_max = (state->item_width - (int16_t)state->app_x_offset) + state->x;
            item->y = current_y;
            item->is_selectable = 0;

            uint8_t i = 0;
            if (item->app_count != 0) {
                do {
                    item->app_y_offsets[i] = state->app_y_table[i];
                    i++;
                } while (i < item->app_count);
            }

            int16_t next_y_step = state->app_y_table[i] + (int16_t)state->item_spacing;
            current_y += next_y_step;

            if (current_y > y_max) {
                // Item overflows current page, move it to the start of the next page
                item->stop_flag = 1;
                state->max_page++;
                item->y = state->y;
                current_y = state->y + next_y_step;
            } else {
                item->stop_flag = 0;
            }

            item++;
        } while (item < end_item);

        // The first item always starts a page
        state->items[0].stop_flag = 1;
    }

    void __cdecl Display_Selected_List_Text(s_list_state* state, s_list_item* item) {
        if (item == nullptr || item == (s_list_item*)-7) {
            return;
        }

        fonts::Set_Font_Style(state->group_font, state->active_color);
        BILL::Squeeze_Print(item->x, item->y, item->group_name, state->item_width);

        if (state->draw_app_cb != nullptr) {
            fonts::Set_Font_Style(state->item_font, state->active_color);

            draw_app_fn draw_cb = (draw_app_fn)state->draw_app_cb;

            int16_t current_app_y = item->app_y_offsets[item->current_app_index] + item->y;
            draw_cb(item->app_base_x, current_app_y, item->tech_app_ids[item->current_app_index]);

            if (state->marker_color != 0) {
                int16_t marker_x = item->x + 2;
                int16_t base_x_minus_2 = item->app_base_x - 2;
                int16_t base_x_minus_5 = item->app_base_x - 5;
                int16_t marker_y_bottom = current_app_y + 6;
                int16_t marker_y_top = item->app_y_offsets[0] + item->y - 3;

                line::Line(marker_x, marker_y_top, marker_x, marker_y_bottom, state->marker_color);
                line::Line(marker_x, marker_y_bottom, base_x_minus_2, marker_y_bottom, state->marker_color);
                line::Line(base_x_minus_5, marker_y_bottom - 3, base_x_minus_2, marker_y_bottom, state->marker_color);
                line::Line(base_x_minus_5, marker_y_bottom + 3, base_x_minus_2, marker_y_bottom, state->marker_color);
            }
        }
    }

    int8_t __cdecl Goto_Page_With_Entry(s_list_state* state, s_list_item* target_item) {
        uint8_t page = 0;
        uint16_t item_index = 0;
        s_list_item* last_page_item;
        s_list_item* item = state->items;
        uint8_t item_count = state->num_items;

        if (item_count != 0) {
            do {
                if (item->stop_flag != 0) {
                    ++page;
                    last_page_item = item;
                }

                if (item == target_item) {
                    state->cur_page = page;
                    state->selected_item = last_page_item;
                    return 1;
                }

                ++item_index;
                ++item;
            } while (item_index < item_count);
        }

        return 0;
    }

    s_list_item* __cdecl Select_And_Page_By_Head_ID(s_list_state* state, uint8_t head_id) {
        s_list_item* items = state->items;
        s_list_item* selected_item = items;
        uint16_t item_index = 0;
        uint8_t item_count = state->num_items;

        do {
            if (items[item_index].group_id == head_id) {
                selected_item = items + item_index;
                break;
            }
            ++item_index;
        } while (item_count > item_index);

        selected_item->is_selectable = 1;
        LIST::Goto_Page_With_Entry(state, selected_item);
        return selected_item;
    }

    void __cdecl Display_Selected_List_Text_For_Tech_List(s_list_state* list_state, s_list_item* item) {
        if (item != nullptr && item != (s_list_item*)-7) {
            fonts::Set_Font_Style(list_state->group_font, list_state->active_color);
            BILL::Squeeze_Print(item->x, item->y, item->group_name, list_state->item_width);

            if (list_state->draw_app_cb != nullptr) {
                fonts::Set_Font_Style(list_state->item_font, list_state->active_color);

                draw_app_fn draw_cb = (draw_app_fn)list_state->draw_app_cb;

                uint8_t app_idx = item->current_app_index;
                draw_cb(item->app_base_x,
                    item->app_y_offsets[app_idx] + item->y,
                    item->tech_app_ids[app_idx]);
            }
        }
    }

} // namespace LIST
