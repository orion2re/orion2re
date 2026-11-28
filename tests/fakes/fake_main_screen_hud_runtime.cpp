#include "pch.h"

#include "fake_main_screen_hud_runtime.h"

namespace {
    auto& player = game::current_state.players[0];
    int16_t fitted_style;
    int16_t spacing;
    s_colors current_colors;
    s_colors fitted_colors;
}

namespace TEST_MAIN_SCREEN_HUD {
    std::vector<s_draw> draws;
    std::vector<s_fit> fits;

    void Reset() {
        player = {};
        player.bc = 500;
        player.surplus_bc = 120;
        player.current_research_field = 1;
        player.research_produced = 25;
        MOX::settings = {};
        MOX::local_player = game::id_player{0};
        MOX::red_color_pulse_value = 0;
        draws.clear();
        fits.clear();
    }
}

namespace MOX {

    game::id_player local_player{0};
    s_settings settings;
    int32_t stardate;
    uint8_t red_color_pulse_value;
}

namespace fonts {
    int16_t current_font_style;

    void __cdecl Set_Font_Style(int32_t style, const s_colors* colors) {
        current_font_style = (int16_t)style;
        current_colors = *colors;
    }

    void __cdecl Set_Font_Style_Outline(int16_t style, s_colors* colors) {
        Set_Font_Style(style, colors);
    }

    void __cdecl Set_Outline_Color(uint16_t, uint16_t) {
    }

    void __cdecl Set_Font_Spacing(int16_t value) {
        spacing = value;
    }

    int16_t __cdecl Get_Font_Height() {
        return current_font_style == 2 ? 12 : 10;
    }

    int16_t __cdecl Get_String_Width(const char* text) {
        return (int16_t)(strlen(text) * 6);
    }

    void __cdecl Print_Centered(int16_t x, int16_t y, const char* text) {
        TEST_MAIN_SCREEN_HUD::draws.push_back({x, y, text, current_font_style, spacing, current_colors.colors[0]});
    }

    void __cdecl Print(int16_t, int16_t, const char*) {
    }

    void __cdecl Print_Right(int16_t, int16_t, const char*) {
    }
}

namespace MISC {
    void __cdecl Set_Fitted_Font_Style(char* text, s_colors* colors, int16_t initial_style, int16_t initial_spacing, int16_t variant, int16_t max_width, int16_t min_style) {
        TEST_MAIN_SCREEN_HUD::fits.push_back({text, initial_style, initial_spacing, variant, max_width, min_style});
        fitted_style = fonts::Get_String_Width(text) <= max_width ? initial_style : min_style;
        spacing = initial_spacing;
        fonts::Set_Font_Style(fitted_style, colors);
    }

    int16_t __cdecl Get_Font_Style() {
        return fitted_style;
    }

    int16_t __cdecl Get_Font_Spacing() {
        return spacing;
    }

    void __cdecl Get_Mox_Font_Colors(int16_t, uint8_t primary, uint8_t secondary, s_colors* colors) {
        *colors = {};
        colors->colors[0] = primary;
        colors->colors[1] = secondary;
    }

    s_colors* __cdecl Font_Colors2(int16_t style, uint8_t primary, uint8_t secondary) {
        Get_Mox_Font_Colors(style, primary, secondary, &fitted_colors);
        return &fitted_colors;
    }

    void __cdecl Increase_Font_Color_Range(int16_t) {
    }

    char* __cdecl Get_Comma_String(int32_t value) {
        static char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d", value);
        return buffer;
    }
}

namespace HAROLD {
    void __cdecl Get_Monetary_Unit_String(char* buffer) {
        strcpy(buffer, MOX::settings.language == 1 ? "MC" : "BC");
    }

    const char* __cdecl H_Message(int16_t, int, const s_save_game_description*) {
        return "%d turns";
    }
}

namespace EVENTS {
    int16_t __cdecl Event_Check_Warp_Beast() {
        return 0;
    }

    int16_t __cdecl Event_Check_Hyperspace_Flux() {
        return 0;
    }
}

namespace ESTRINGS {
    char* __cdecl E_Strings(int16_t) {
        static char text[] = "";
        return text;
    }
}

namespace line {
    void __cdecl Line(int16_t, int16_t, int16_t, int16_t, uint8_t) {
    }
}

namespace COLCALC {
    int16_t __cdecl Player_N_Turns_Until_Research_Complete(s_player*) {
        return 3;
    }

    int32_t __cdecl Chance_For_Research_Breakthrough(s_player*) {
        return 20;
    }
}
