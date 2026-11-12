#include "pch.h"

namespace CMBTAI {
    namespace {
        int16_t Range_To_XY_With_Offset(
            int16_t ship_idx,
            int16_t target_x_px,
            int16_t target_y_px,
            int16_t offset_x_px,
            int16_t offset_y_px) {
            const int16_t ship_x_px =
                (int16_t)(MOX::combat_data[ship_idx].x * 20 + offset_x_px);
            const int16_t ship_y_px =
                (int16_t)(MOX::combat_data[ship_idx].y * 20 + offset_y_px);
            const int16_t distance_squares = special::Range(
                                                 ship_x_px, ship_y_px, target_x_px, target_y_px) /
                20;
            return (distance_squares + 2) / 3;
        }
    } // namespace

    int16_t __cdecl Range_To_Ship_Squares(int16_t ship1_idx, int16_t ship2_idx) {
        int16_t w1, h1, w2, h2;
        int32_t is_large1 = 0;
        int32_t is_huge1 = 0;
        int32_t is_large2 = 0;
        int32_t is_huge2 = 0;

        COMBINIT::Ship_Center_Offsets(ship1_idx, &w1, &h1);
        int16_t x1 = MOX::combat_data[ship1_idx].x;
        int16_t y1 = MOX::combat_data[ship1_idx].y;

        if (w1 > 10) {
            is_large1 = 1;
        }
        if (w1 > 20) {
            is_huge1 = 1;
            x1++;
            y1++;
        }

        COMBINIT::Ship_Center_Offsets(ship2_idx, &w2, &h2);
        int16_t x2 = MOX::combat_data[ship2_idx].x;
        int16_t y2 = MOX::combat_data[ship2_idx].y;

        if (w2 > 10) {
            is_large2 = 1;
        }
        if (w2 > 20) {
            is_huge2 = 1;
            x2++;
            y2++;
        }

        int16_t x1_adj, y1_adj;
        if (x2 < x1) {
            x1_adj = x1 - is_huge1;
            if (x1_adj <= x2) {
                x1_adj = x2;
            }
        } else {
            x1_adj = x1 + is_large1;
            if (x1_adj >= x2) {
                x1_adj = x2;
            }
        }

        if (y2 < y1) {
            y1_adj = y1 - is_huge1;
            if (y1_adj <= y2) {
                y1_adj = y2;
            }
        } else {
            y1_adj = y1 + is_large1;
            if (y1_adj >= y2) {
                y1_adj = y2;
            }
        }

        int16_t x2_adj, y2_adj;
        if (x2 > x1_adj) {
            x2_adj = x2 - is_huge2;
            if (x2_adj <= x1_adj) {
                x2_adj = x1_adj;
            }
        } else {
            x2_adj = x2 + is_large2;
            if (x2_adj >= x1_adj) {
                x2_adj = x1_adj;
            }
        }

        if (y2 > y1_adj) {
            y2_adj = y2 - is_huge2;
            if (y2_adj <= y1_adj) {
                y2_adj = y1_adj;
            }
        } else {
            y2_adj = y2 + is_large2;
            if (y2_adj >= y1_adj) {
                y2_adj = y1_adj;
            }
        }

        return special::Range(x1_adj, y1_adj, x2_adj, y2_adj);
    }

    int16_t __cdecl Range_To_Ship(int16_t ship1_idx, int16_t ship2_idx) {
        return (Range_To_Ship_Squares(ship1_idx, ship2_idx) + 2) / 3;
    }

    int16_t __cdecl Range_To_Missile(int16_t unit_idx, int16_t missile_idx) {
        s_missile* missile = &MOX::missile[missile_idx];
        return CMBTAI::Range_To_XY(unit_idx, missile->x, missile->y);
    }

    int16_t __cdecl Range_To_XY(int16_t ship_idx, int16_t target_x_px, int16_t target_y_px) {
        int16_t center_x_offset;
        int16_t center_y_offset;
        COMBINIT::Ship_Center_Offsets(
            ship_idx, &center_x_offset, &center_y_offset);
        return Range_To_XY_With_Offset(
            ship_idx,
            target_x_px,
            target_y_px,
            center_x_offset,
            center_y_offset);
    }

    int16_t __cdecl Range_To_XY_From_Corner(
        int16_t ship_idx, int16_t target_x_px, int16_t target_y_px) {
        return Range_To_XY_With_Offset(
            ship_idx, target_x_px, target_y_px, 0, 0);
    }
} // namespace CMBTAI
