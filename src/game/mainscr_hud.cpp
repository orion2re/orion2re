#include "pch.h"

namespace MAINSCR {
    /**
     * @brief Returns a 3-character unit abbreviation string for research points based on current language.
     *
     * @param out Buffer of at least 3 bytes to store the result (including null terminator).
     */
    void __cdecl Get_Research_Unit_String(char* out_buffer) {
        const char* src;

        switch (MOX::settings.language) {
            case 1:
                src = "FP";
                break;
            case 2:
                src = "PR";
                break;
            case 3:
                src = "PI";
                break;
            case 4:
                src = "PR";
                break;
            default:
                src = "RP";
                break;
        }

        out_buffer[0] = src[0];
        out_buffer[1] = src[1];
        out_buffer[2] = src[2];
    }

    void __cdecl Set_Post_Fitted_Font_Style(uint8_t c1, uint8_t c2) {
        int16_t style = MISC::Get_Font_Style();
        int16_t spacing = MISC::Get_Font_Spacing();
        s_colors colors;

        MISC::Get_Mox_Font_Colors(style, (char)c1, (char)c2, &colors);

        fonts::Set_Outline_Color(2, 1);
        fonts::Set_Font_Style_Outline(style, &colors);
        fonts::Set_Font_Spacing(spacing);
    }

    /**
     * Draws the main-screen HUD summaries for stardate, economy, command points, food, freighters, and research.
     */
    void __cdecl Print_Main_Screen_Data() {
        char sep = (MOX::settings.language == 1 || MOX::settings.language == 2) ? ',' : '.';
        s_colors colors1, colors2;
        char buffer[128];
        char monetary_unit[16];
        char research_unit[16];

        memset(&colors1, 0, sizeof(colors1));
        memset(&colors2, 0, sizeof(colors2));

        fonts::Set_Font_Style(0, &colors1);
        fonts::Get_Font_Height();
        snprintf(buffer, sizeof(buffer), "%d%c%d", (int)(MOX::stardate / 10), (int)sep, (int)(MOX::stardate % 10));
        MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, -1, 0x43, 1);
        int16_t stardate_font_style = MISC::Get_Font_Style();
        s_colors* p_stardate_colors = MISC::Font_Colors2(stardate_font_style, 14, 23);
        memcpy(&colors2, p_stardate_colors, sizeof(colors2));
        fonts::Set_Outline_Color(2, 1);
        fonts::Set_Font_Style_Outline(stardate_font_style, &colors2);
        fonts::Print_Centered(580, 29, buffer);

        if (EVENTS::Event_Check_Warp_Beast()) {
            MISC::Get_Mox_Font_Colors(3, 'I', 'L', &colors1);
            fonts::Set_Font_Style(3, &colors1);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 5) / 9);
            char beast_buffer[128];
            strlcpy(beast_buffer, ESTRINGS::E_Strings(ESTR_BEAST), sizeof(beast_buffer));
            int16_t beast_right = (int16_t)(25 + fonts::Get_String_Width(beast_buffer));

            line::Line(beast_right, 21, 25, 21, colors1.colors[0]);
            line::Line(beast_right, 22, 25, 22, colors1.colors[1]);
            fonts::Print(25, 24, beast_buffer);

            int16_t fh = fonts::Get_Font_Height();
            line::Line(beast_right, fh + 25, 25, fh + 25, colors1.colors[1]);
            line::Line(beast_right, fh + 26, 25, fh + 26, colors1.colors[0]);
        }

        if (EVENTS::Event_Check_Hyperspace_Flux()) {
            MISC::Get_Mox_Font_Colors(3, 'I', 'L', &colors1);
            fonts::Set_Font_Style(3, &colors1);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 5) / 9);
            const char* flux_msg = HAROLD::H_Message(0xe0);
            char flux_buffer[128];
            strlcpy(flux_buffer, flux_msg, sizeof(flux_buffer));
            int16_t flux_width = fonts::Get_String_Width(flux_buffer);
            int16_t flux_x = 526 - flux_width;

            line::Line(flux_x, 21, 526, 21, colors1.colors[0]);
            line::Line(flux_x, 22, 526, 22, colors1.colors[1]);
            fonts::Print_Right(526, 24, flux_buffer);

            int16_t fh = fonts::Get_Font_Height();
            line::Line(flux_x, fh + 25, 526, fh + 25, colors1.colors[1]);
            line::Line(flux_x, fh + 26, 526, fh + 26, colors1.colors[0]);
        }

        fonts::Get_Font_Height();
        HAROLD::Get_Monetary_Unit_String(monetary_unit);
        s_player* plr = &MOX::player[MOX::PLAYER_NUM];

        if (main_screen_tax_rate && plr->tax_rate != 0) {
            snprintf(buffer, sizeof(buffer), "%d%%", (int)plr->tax_rate);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, (int16_t)(96 - fonts::Get_Font_Height()), buffer);
        }

        char* surplus_bc_comma = MISC::Get_Comma_String(plr->surplus_bc);
        const char* bc_sign = (plr->surplus_bc < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%s %s", bc_sign, surplus_bc_comma, monetary_unit);
        MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
        int16_t bc_style = MISC::Get_Font_Style();
        MISC::Get_Font_Spacing();
        MISC::Get_Mox_Font_Colors(bc_style, 75, 77, &colors1);
        MISC::Get_Mox_Font_Colors(bc_style, 14, 23, &colors2);

        char* bc_comma = MISC::Get_Comma_String(plr->bc);
        snprintf(buffer, sizeof(buffer), "%s %s", bc_comma, monetary_unit);

        if (plr->bc < 0) {
            MISC::Set_Fitted_Font_Style(buffer, &colors1, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style('K', 'M');
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
            fonts::Set_Outline_Color(0x49, 0x49);
        } else {
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            fonts::Set_Outline_Color(2, 1);
        }
        fonts::Print_Centered(579, 96, buffer);

        surplus_bc_comma = MISC::Get_Comma_String(plr->surplus_bc);
        bc_sign = (plr->surplus_bc < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%s %s", bc_sign, surplus_bc_comma, monetary_unit);
        MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
        bc_style = MISC::Get_Font_Style();
        int16_t bc_spacing = MISC::Get_Font_Spacing();
        MISC::Get_Mox_Font_Colors(bc_style, 75, 77, &colors1);
        MISC::Get_Mox_Font_Colors(bc_style, 14, 23, &colors2);

        if (plr->surplus_bc < 0) {
            fonts::Set_Outline_Color(0x49, 0x49);
            fonts::Set_Font_Style_Outline(bc_style, &colors1);
            fonts::Set_Font_Spacing(bc_spacing);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
        } else {
            fonts::Set_Font_Style_Outline(bc_style, &colors2);
            fonts::Set_Outline_Color(2, 1);
            fonts::Set_Font_Spacing(bc_spacing);
        }
        fonts::Print_Centered(579, fonts::Get_Font_Height() + 96, buffer);

        int16_t cp_surplus = plr->command_points - plr->command_points_used;
        const char* cp_sign = (cp_surplus < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%d (%d)", cp_sign, (int)cp_surplus, (int)plr->command_points);
        MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 0);
        int16_t current_style = (int16_t)fonts::current_font_style;
        int16_t current_spacing = MISC::Get_Font_Spacing();
        MISC::Get_Mox_Font_Colors(current_style, 'K', 'M', &colors1);
        MISC::Get_Mox_Font_Colors(current_style, 14, 23, &colors2);

        char cp_left[64];
        snprintf(cp_left, sizeof(cp_left), "%s%d", cp_sign, (int)cp_surplus);
        int16_t cp_left_w = fonts::Get_String_Width(cp_left);
        char cp_right[64];
        snprintf(cp_right, sizeof(cp_right), "(%d)", (int)plr->command_points);
        int16_t cp_total_w = (int16_t)(fonts::Get_String_Width(cp_right) + cp_left_w + 4);

        if (cp_surplus < 0) {
            fonts::Set_Font_Style_Outline(current_style, &colors1);
            fonts::Set_Outline_Color(0x49, 0x49);
            fonts::Set_Font_Spacing(current_spacing);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
        } else {
            fonts::Set_Font_Style_Outline(current_style, &colors2);
            fonts::Set_Outline_Color(2, 1);
            fonts::Set_Font_Spacing(current_spacing);
        }
        int16_t cp_half_w = (int16_t)(cp_total_w / 2);
        fonts::Print(581 - cp_half_w, 181, cp_left);
        fonts::Set_Font_Style(current_style, &colors2);
        fonts::Set_Font_Spacing(current_spacing);
        fonts::Print(581 + cp_left_w + 4 - cp_half_w, 181, cp_right);

        fonts::Get_Font_Height();
        char* food_comma = MISC::Get_Comma_String(plr->surplus_food);
        const char* food_sign = (plr->surplus_food < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%s", food_sign, food_comma);
        if (plr->surplus_food < 0) {
            MISC::Set_Fitted_Font_Style(buffer, &colors1, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style('K', 'M');
            fonts::Set_Outline_Color(0x49, 0x49);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
        } else {
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
        }
        fonts::Print_Centered(579, 255, buffer);

        const char* fr_sign = (plr->surplus_freighters < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%d (%d)", fr_sign, (int)plr->surplus_freighters, (int)plr->n_freighters);
        MISC::Set_Fitted_Font_Style(buffer, &colors1, 2, 2, 4, 0x43, 1);
        int16_t fr_style = MISC::Get_Font_Style();
        MISC::Get_Mox_Font_Colors(fr_style, 'K', 'M', &colors1);
        MISC::Get_Mox_Font_Colors(fr_style, 14, 23, &colors2);
        int16_t fr_spacing = MISC::Get_Font_Spacing();
        int16_t fr_total_w = fonts::Get_String_Width(buffer);

        snprintf(buffer, sizeof(buffer), "%s%d", fr_sign, (int)plr->surplus_freighters);
        if (plr->surplus_freighters < 0) {
            fonts::Set_Font_Style_Outline(fr_style, &colors1);
            fonts::Set_Font_Spacing(fr_spacing);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
            fonts::Set_Outline_Color(0x49, 0x49);
        } else {
            fonts::Set_Font_Style_Outline(fr_style, &colors2);
            fonts::Set_Font_Spacing(fr_spacing);
            fonts::Set_Outline_Color(2, 1);
        }
        int16_t fr_half_w = (int16_t)((fr_total_w + 4) / 2);
        fonts::Print(581 - fr_half_w, 330, buffer);

        fonts::Set_Outline_Color(2, 1);
        int16_t fr_left_w = fonts::Get_String_Width(buffer);
        snprintf(buffer, sizeof(buffer), "(%d)", (int)plr->n_freighters);
        fonts::Set_Font_Style_Outline(fr_style, &colors2);
        fonts::Set_Font_Spacing(fr_spacing);
        fonts::Print(fr_left_w - fr_half_w + 583, 330, buffer);

        Get_Research_Unit_String(research_unit);
        if (plr->research_breakthrough != 0) {
            fonts::Set_Outline_Color(2, 1);
            fonts::Set_Font_Style_Outline(0, &colors2);
            fonts::Print_Centered(579, 403, HAROLD::H_Message(0x183));
            return;
        }
        if (plr->current_research_field == 0) {
            fonts::Set_Outline_Color(2, 1);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Print_Centered(579, 403, HAROLD::H_Message(0x188));
            return;
        }

        int16_t turns = COLCALC::Player_N_Turns_Until_Research_Complete(plr);
        if (turns > 0) {
            int32_t chance = COLCALC::Chance_For_Research_Breakthrough(plr);
            if (chance > 0) {
                snprintf(buffer, sizeof(buffer), "%d%%", (int)chance);
                MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
                Set_Post_Fitted_Font_Style(14, 23);
                fonts::Set_Outline_Color(2, 1);
                fonts::Print_Centered(579, (int16_t)(403 - 2 * fonts::Get_Font_Height()), buffer);
            }
            const char* turns_msg = HAROLD::H_Message(turns == 1 ? 0xe1 : 0xe2);
            snprintf(buffer, sizeof(buffer), turns_msg, (int)turns);
            char at_buffer[sizeof(buffer) + 1];
            snprintf(at_buffer, sizeof(at_buffer), "@%s", buffer);
            MISC::Set_Fitted_Font_Style(at_buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            int16_t fh = fonts::Get_Font_Height();
            int16_t turns_y = (int16_t)(403 - fh);
            fonts::Set_Outline_Color(2, 1);
            int16_t msg_w = fonts::Get_String_Width(buffer);
            int16_t start_x = (int16_t)((62 - msg_w) / 2 + 548);
            fonts::Print(start_x, turns_y + 3, "@");

            const char* turns_msg2 = HAROLD::H_Message(turns == 1 ? 0xe1 : 0xe2);
            snprintf(buffer, sizeof(buffer), turns_msg2, (int)turns);
            fonts::Print((int16_t)(fonts::Get_String_Width("@") + start_x), turns_y, buffer);

            char* res_comma = MISC::Get_Comma_String(plr->research_produced);
            snprintf(buffer, sizeof(buffer), "%s %s", res_comma, research_unit);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, turns_y + fonts::Get_Font_Height(), buffer);
        } else if (turns < 0) {
            snprintf(buffer, sizeof(buffer), "0 %s", research_unit);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, 403, buffer);
        } else {
            int32_t chance = COLCALC::Chance_For_Research_Breakthrough(plr);
            snprintf(buffer, sizeof(buffer), "%d%%", (int)chance);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, 391, buffer);

            snprintf(buffer, sizeof(buffer), "%d %s", (int)plr->research_produced, research_unit);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, (int16_t)(391 + fonts::Get_Font_Height()), buffer);
        }
    }
} // namespace MAINSCR
