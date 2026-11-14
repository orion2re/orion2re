#include "pch.h"

namespace SCROLL {
    bool __cdecl Handle_List_Navigation(s_scroll_bar* scroll_bar, int16_t input) {
        if (input != fields::INPUT_HOME && input != fields::INPUT_END) {
            return false;
        }

        int16_t last_page = std::max(0, scroll_bar->total_rows - scroll_bar->visible_rows);
        scroll_bar->first_visible_row = input == fields::INPUT_HOME ? 0 : last_page;
        scroll_bar->thumb_height_px = Get_Vertical_Scroll_Bar_Height(scroll_bar);
        int16_t half_height = scroll_bar->thumb_height_px / 2;
        scroll_bar->thumb_center_y = scroll_bar->first_visible_row == 0
            ? scroll_bar->track_len_px - half_height
            : half_height;
        scroll_bar->last_thumb_center_y = scroll_bar->thumb_center_y;
        scroll_bar->thumb_center_y_copy = scroll_bar->thumb_center_y;
        return true;
    }

    int8_t __cdecl Set_Vertical_Scroll_Bar_Maxima(s_scroll_bar* scroll_bar) {
        int16_t half_height = scroll_bar->thumb_height_px / 2;

        if (scroll_bar->thumb_center_y + half_height > scroll_bar->track_len_px) {
            scroll_bar->thumb_center_y = scroll_bar->track_len_px - half_height;
            return 1;
        }

        if (scroll_bar->thumb_center_y - half_height < 0) {
            scroll_bar->thumb_center_y = half_height;
            return 1;
        }

        return 0;
    }

    int16_t __cdecl Get_Vertical_Scroll_Bar_Height(s_scroll_bar* scroll_bar) {
        scroll_bar->thumb_y_snapshot = scroll_bar->thumb_center_y;

        if (scroll_bar->total_rows < scroll_bar->visible_rows) {
            return scroll_bar->track_len_px;
        }

        int32_t track_len_px = (int32_t)scroll_bar->track_len_px;
        int32_t numerator = (track_len_px + 1) * (int32_t)scroll_bar->visible_rows - 1;

        return (int16_t)(numerator / (int32_t)scroll_bar->total_rows);
    }

    void __cdecl Scroll_Down_One_Item(s_scroll_bar* scroll_bar) {
        if (scroll_bar->total_rows > scroll_bar->visible_rows) {
            uint8_t step = scroll_bar->row_step;
            scroll_bar->first_visible_row = (int16_t)((uint16_t)scroll_bar->first_visible_row + step);

            if ((scroll_bar->total_rows - scroll_bar->visible_rows) < scroll_bar->first_visible_row) {
                scroll_bar->first_visible_row = scroll_bar->total_rows - scroll_bar->visible_rows;
                scroll_bar->thumb_center_y = scroll_bar->thumb_height_px / 2;
            } else {
                scroll_bar->thumb_center_y -= (scroll_bar->track_len_px / scroll_bar->total_rows) * step;
            }

            scroll_bar->last_thumb_center_y = scroll_bar->thumb_center_y;
            scroll_bar->thumb_center_y_copy = scroll_bar->thumb_center_y;
        }
    }

    void __cdecl Scroll_Up_One_Item(s_scroll_bar* scroll_bar) {
        if (scroll_bar->visible_rows < scroll_bar->total_rows) {
            scroll_bar->first_visible_row -= (int16_t)scroll_bar->row_step;

            if (scroll_bar->first_visible_row <= 0) {
                scroll_bar->first_visible_row = 0;
                scroll_bar->thumb_center_y = scroll_bar->track_len_px + (scroll_bar->thumb_height_px / 2);
            } else {
                scroll_bar->thumb_center_y += (scroll_bar->track_len_px / scroll_bar->total_rows) * (int16_t)scroll_bar->row_step;
            }

            scroll_bar->last_thumb_center_y = scroll_bar->thumb_center_y;
            scroll_bar->thumb_center_y_copy = scroll_bar->thumb_center_y;
        }
    }

    int16_t __cdecl Get_Vertical_Scroll_Bar_First_Item(s_scroll_bar* scroll_bar) {
        int16_t result = 0;
        int16_t total_rows = scroll_bar->total_rows;

        if (total_rows > 0) {
            int32_t track_len_px = scroll_bar->track_len_px;

            int32_t diff = track_len_px - (int16_t)(scroll_bar->thumb_height_px / 2 + scroll_bar->thumb_center_y);
            int32_t step = ((uint8_t)scroll_bar->row_step * track_len_px * 10) / total_rows;

            result = (diff * 10) / (int16_t)step;

            if (scroll_bar->visible_rows + result > total_rows) {
                result = total_rows - scroll_bar->visible_rows;
            } else if (total_rows > scroll_bar->visible_rows) {
                int16_t remaining_track_px = MOX::scroll_bar->track_len_px -
                    MOX::scroll_bar->thumb_height_px / 2 -
                    MOX::scroll_bar->thumb_center_y;

                if (remaining_track_px + scroll_bar->thumb_height_px >= track_len_px) {
                    result = total_rows - scroll_bar->visible_rows;
                }
            }

            if (result <= 0) {
                result = 0;
            }
        }

        return result;
    }

    void __cdecl Update_Vertical_Scroll_Bar_State(s_scroll_bar* scroll_bar) {
        scroll_bar->thumb_height_px = Get_Vertical_Scroll_Bar_Height(scroll_bar);
        Set_Vertical_Scroll_Bar_Maxima(scroll_bar);
        scroll_bar->first_visible_row = Get_Vertical_Scroll_Bar_First_Item(scroll_bar);
    }

    bool __cdecl Draw_Generic_Vertical_Scroll_Bar(s_scroll_bar* scroll_bar, void(__cdecl* draw_callback)()) {
        int16_t mouse_btn = mouse::Mouse_Button();
        int16_t orig_first_visible_row = scroll_bar->first_visible_row;

        scroll_bar->mouse_button_down = 0;
        scroll_bar->thumb_height_px = Get_Vertical_Scroll_Bar_Height(scroll_bar);

        if (scroll_bar->last_thumb_center_y == -1) {
            scroll_bar->last_thumb_center_y = scroll_bar->thumb_center_y;
            scroll_bar->thumb_center_y_copy = scroll_bar->thumb_center_y;
        } else if (scroll_bar->last_thumb_center_y != scroll_bar->thumb_center_y) {
            int32_t diff = abs(scroll_bar->thumb_center_y - scroll_bar->thumb_center_y_copy);
            if (diff > (scroll_bar->thumb_height_px / 2)) {
                Update_Vertical_Scroll_Bar_State(scroll_bar);
                scroll_bar->last_thumb_center_y = scroll_bar->thumb_center_y;
                scroll_bar->thumb_center_y_copy = scroll_bar->thumb_center_y;
            }
        }

        if (Set_Vertical_Scroll_Bar_Maxima(scroll_bar)) {
            scroll_bar->first_visible_row = Get_Vertical_Scroll_Bar_First_Item(scroll_bar);
            scroll_bar->last_thumb_center_y = scroll_bar->thumb_center_y;
            scroll_bar->thumb_center_y_copy = scroll_bar->thumb_center_y;
        }

        draw_callback();

        if (mouse_btn == 1) {
            scroll_bar->mouse_button_down++;
        }

        return scroll_bar->first_visible_row != orig_first_visible_row;
    }
} // namespace SCROLL