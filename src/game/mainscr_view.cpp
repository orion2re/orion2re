#include "pch.h"

namespace MAINSCR {
    namespace {
        bool Is_Extended_Max_Map_View() {
            return MAP_SCALE::Is_Extended_Max_Map_View_(
                MOX::NUM_STARS,
                ORIGINAL_MAX_STARS,
                MOX::cur_map_scale,
                MOX::max_map_scale);
        }
    } // namespace

    s_mouse_list mouse_list_relocation[2] = {
        {2, 0, 0, 0, 639, 479},
        {18, 0, 22, 22, 526, 420},
    };

    bool Use_Classic_Maximum_Galaxy_Display() {
        return maximum_galaxy_display == MAXIMUM_GALAXY_DISPLAY_CLASSIC &&
            MOX::settings.galaxy_size == GALAXY_SIZE_MAXIMUM &&
            Is_Extended_Max_Map_View();
    }

    bool Use_Maximum_Galaxy_Overview() {
        return Is_Extended_Max_Map_View() && !Use_Classic_Maximum_Galaxy_Display();
    }

    int16_t __cdecl Get_New_Paralax_XYs(int16_t target_map_x, int16_t target_map_y, int16_t scale) {
        int16_t dx = (static_cast<int32_t>(target_map_x) - MOX::cur_map_x) / scale;
        int16_t dy = (static_cast<int32_t>(target_map_y) - MOX::cur_map_y) / scale;

        int16_t near_x = MOX::near_star_x;
        int16_t mid_x = MOX::mid_star_x;
        int16_t far_x = MOX::far_star_x;

        if (target_map_x != MOX::cur_map_x) {
            // sign_x logic calculates -abs(dx)
            int16_t sign_x = (dx > 0) ? -1 : (dx < 0 ? 1 : 0);
            int16_t step_x = sign_x * dx;

            far_x -= step_x;
            mid_x -= 2 * step_x;
            near_x -= 4 * step_x;
        }

        int16_t near_y = MOX::near_star_y;
        int16_t mid_y = MOX::mid_star_y;
        int16_t far_y = MOX::far_star_y;

        if (target_map_y != MOX::cur_map_y) {
            // sign_y logic calculates -abs(dy)
            int16_t sign_y = (dy > 0) ? -1 : (dy < 0 ? 1 : 0);
            int16_t step_y = sign_y * dy;

            far_y -= step_y;
            mid_y -= 2 * step_y;
            near_y -= 4 * step_y;
        }

        // Apply screen wrap (640x480)
        if (near_x < 0) near_x += 640;
        if (near_y < 0) near_y += 480;
        if (mid_x < 0) mid_x += 640;
        if (mid_y < 0) mid_y += 480;
        if (far_x < 0) far_x += 640;
        if (far_y < 0) far_y += 480;

        MOX::near_star_x = near_x % 640;
        MOX::near_star_y = near_y % 480;
        MOX::mid_star_x = mid_x % 640;
        MOX::mid_star_y = mid_y % 480;
        MOX::far_star_x = far_x % 640;

        int16_t wrap_count_y = far_y / 480;
        MOX::far_star_y = far_y % 480;

        return wrap_count_y;
    }

    void __cdecl Center_Map(int16_t target_x, int16_t target_y) {
        int16_t old_x = MOX::cur_map_x;
        int16_t old_y = MOX::cur_map_y;

        // Calculate top-left corner based on centered target
        MOX::cur_map_x = MAP_SCALE::Clamp_Map_Offset_(target_x - HAROLD::Get_Up_Scaled_Value(253), MOX::MAP_MAX_X, HAROLD::Get_Up_Scaled_Value(506));
        MOX::cur_map_y = MAP_SCALE::Clamp_Map_Offset_(target_y - HAROLD::Get_Up_Scaled_Value(200), MOX::MAP_MAX_Y, HAROLD::Get_Up_Scaled_Value(400));

        // If viewport position changed, update parallax offsets
        if (old_x != MOX::cur_map_x || old_y != MOX::cur_map_y) {
            MAINSCR::Get_New_Paralax_XYs((int16_t)old_x, (int16_t)old_y, 1);
        }

        MOX::draw_main_screen_to_back = 1;
    }
} // namespace MAINSCR
