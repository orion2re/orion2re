#include "pch.h"

namespace FLT {

    void __cdecl Set_Fltscrn_Small_Ship_Icon_XYs(int16_t map_x, int16_t map_y, int16_t map_width, int16_t map_height) {
        int16_t zoom_level;
        int16_t star_x[MAX_STARS];
        int16_t star_y[MAX_STARS];
        int16_t icon_w, icon_h;
        int16_t actual_star_idx;
        int16_t icon_x, icon_y;

        zoom_level = HAROLD::Map_Scale_To_Zoom_Level();
        MOX::cur_map_scale = MOX::max_map_scale;

        for (int16_t i = 0; i < std::ssize(game::current_state.stars); ++i) {
            MOVEBOX::Get_Galaxy_Map_Star_XY(i, map_x, map_y, map_width, map_height, 0, 0, 1, &star_x[i], &star_y[i]);
        }

        for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
            actual_star_idx = MOX::ship_icon[i].star_idx;
            FLT2::Get_Small_Icon_Animation_Size(i, &icon_w, &icon_h);

            int16_t stack_slot = MOX::ship_icon[i].stack_slot;
            if (stack_slot == 0) {
                icon_x = star_x[actual_star_idx] + 4;
                icon_y = star_y[actual_star_idx] - (icon_h / 2) - 3;
            } else if (stack_slot == 1) {
                int16_t stack_x = HACCESS::Ship_Stack_X(MOX::ship_icon[i].node_idx);
                int16_t stack_y = HACCESS::Ship_Stack_Y(MOX::ship_icon[i].node_idx);
                actual_star_idx = HAROLD::XY_To_Star_Id(stack_x, stack_y);
                icon_x = star_x[actual_star_idx] - (icon_w / 2);
                icon_y = star_y[actual_star_idx] - (icon_h / 2) - 3;
            } else if (stack_slot == 2) {
                icon_x = star_x[actual_star_idx] - icon_w;
                icon_y = star_y[actual_star_idx] - 5;
            } else if (stack_slot < 5) {
                icon_x = star_x[actual_star_idx] - icon_w;
                icon_y = star_y[actual_star_idx] - 5 + icon_h;
            } else {
                int16_t ship_idx = MOX::ship_node[MOX::ship_icon[i].node_idx].ship_idx;
                int16_t scaled_x = HAROLD::Get_Scaled_Value(game::current_state.ship_records[ship_idx].x);
                int16_t scaled_y = HAROLD::Get_Scaled_Value(game::current_state.ship_records[ship_idx].y);

                SHIPS::Get_Ship_Icon_Dimensions(zoom_level, &MOX::ship_icon_width[zoom_level], &MOX::ship_icon_height[zoom_level]);

                int32_t x_denom = 506000 / map_width;
                icon_x = (int16_t)((scaled_x * 1000) / x_denom) - (icon_w / 2) + map_x;

                int32_t y_denom = 400000 / map_height;
                icon_y = (int16_t)((scaled_y * 1000) / y_denom) - (icon_h / 2) + map_y;
            }

            MOX::ship_icon[i].x = icon_x;
            MOX::ship_icon[i].y = icon_y;
        }
    }

} // namespace FLT
