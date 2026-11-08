#pragma once

namespace TEST_PLNTSUM {
    struct s_field {
        int16_t x;
        int16_t y;
        int16_t x_end;
        int16_t y_end;
        char hotkey;
    };

    extern bool in_range[MAX_STARS];
    extern s_field fields[512];
    extern int16_t field_count;
    extern int clear_count;
    extern int16_t mouse_button;
    extern int16_t hyperspace_flux;
    extern bool confirm_recall;
    extern bool can_select_ship;
    extern int confirmation_count;
    extern int warning_count;
    extern int16_t moved_ship;
    extern int16_t moved_to_star;
    extern int16_t draw_x;
    extern int16_t draw_y;

    void Reset_();
} // namespace TEST_PLNTSUM
