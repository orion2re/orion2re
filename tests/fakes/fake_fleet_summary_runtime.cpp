#include "pch.h"

namespace {
    int16_t s_window_left;
    int16_t s_window_right;
    int16_t s_line_count;
    int16_t s_animation_draw_count;
    int16_t s_animation_x;
    int16_t s_best_fuel;
    uint32_t s_candidate_count;
    uint32_t s_random_percentage;
    int16_t s_granted_techs[SCRAP_TECH_ACQUIRED_CAPACITY];
    int16_t s_grant_count;
    s_player s_players[MAX_PLAYERS];
}

namespace MOX {
    int16_t ship_icon_count;
    int16_t ship_stack_count;
    int16_t ship_stack_start[MAX_SHIPS];
    s_ship_icon ship_icon[SHIP_ICON_COUNT];
    int16_t small_ship_stack_ptr;
    int16_t scanned_small_ship;
    game::id_star galaxy_map_scanned_star = game::NO_STAR;
    int16_t current_screen;
    s_scroll_bar* scroll_bar;
    s_animation_header* fleet_scroll_anim_seg;
    s_player* player = s_players;
    char temp_string[512];
    uint8_t draw_fleet_screen_to_back;
}

namespace TECHDATA {
    s_tech_armor_data armor[ARMOR_COUNT];
    s_tech_fuel_data fuel[FUEL_COUNT];
    s_tech_app_data technology_applications[TECH_APP_COUNT];
    s_tech_drive_data drives[DRIVE_COUNT];
    s_tech_shield_data shields[SHIELD_COUNT];
    s_tech_computer_data computers[COMPUTER_COUNT];
    s_tech_specials_data specials[SPECIAL_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];
}

namespace string {
    const char s_Newline[] = "\n";
}

namespace TEST_FLEET_SUMMARY {
    void Reset_Scroll_Draw_() {
        s_window_left = 0;
        s_window_right = 0;
        s_line_count = 0;
        s_animation_draw_count = 0;
        s_animation_x = 0;
    }

    void Reset_Tech_Plunder_() {
        memset(s_players, 0, sizeof(s_players));
        memset(MOX::temp_string, 0, sizeof(MOX::temp_string));
        memset(TECHDATA::armor, 0, sizeof(TECHDATA::armor));
        memset(TECHDATA::fuel, 0, sizeof(TECHDATA::fuel));
        memset(TECHDATA::technology_applications, 0, sizeof(TECHDATA::technology_applications));
        memset(TECHDATA::drives, 0, sizeof(TECHDATA::drives));
        memset(TECHDATA::shields, 0, sizeof(TECHDATA::shields));
        memset(TECHDATA::computers, 0, sizeof(TECHDATA::computers));
        memset(TECHDATA::specials, 0, sizeof(TECHDATA::specials));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        memset(s_granted_techs, 0, sizeof(s_granted_techs));
        for (int16_t i = 0; i < TECH_APP_COUNT; ++i) {
            TECHDATA::technology_applications[i].name = (char*)"Tech";
        }
        s_best_fuel = -1;
        s_candidate_count = 0;
        s_random_percentage = 100;
        s_grant_count = 0;
        MOX::draw_fleet_screen_to_back = 0;
    }

    void Set_Best_Fuel_(int16_t fuel_id) {
        s_best_fuel = fuel_id;
    }

    void Set_Random_Percentage_(uint32_t value) {
        s_random_percentage = value;
    }

    int16_t Window_Left_() {
        return s_window_left;
    }

    int16_t Window_Right_() {
        return s_window_right;
    }

    int16_t Line_Count_() {
        return s_line_count;
    }

    int16_t Animation_Draw_Count_() {
        return s_animation_draw_count;
    }

    int16_t Animation_X_() {
        return s_animation_x;
    }

    uint32_t Candidate_Count_() {
        return s_candidate_count;
    }

    int16_t Grant_Count_() {
        return s_grant_count;
    }

    int16_t Granted_Tech_(int16_t index) {
        return s_granted_techs[index];
    }
}

namespace INITSHIP {
    int16_t __cdecl Best_Ship_Fuel(int16_t) {
        return s_best_fuel;
    }
}

namespace HACCESS {
    bool __cdecl Player_Has_Tech(int16_t, int16_t) {
        return false;
    }
}

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bytes = (uint8_t*)bit_field;
        return (bytes[bit_index / 8] & (1 << (bit_index % 8))) != 0;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t max_val) {
        if (max_val == 100) {
            return s_random_percentage;
        }
        s_candidate_count = max_val;
        return 1;
    }
}

namespace MISC {
    void __cdecl Get_LBX_Message(int16_t, char* buffer) {
        strlcpy(buffer, "%s", 200);
    }
}

namespace COLCALC {
    void __cdecl Player_Gets_Tech_App(s_player*, int16_t tech_app_id) {
        s_granted_techs[s_grant_count++] = tech_app_id;
    }
}

namespace HAROLD {
    int16_t __cdecl User_Box(uint8_t*, const char*, int16_t) {
        return 1;
    }
}

namespace graphics {
    void __cdecl Set_Window(int16_t x1, int16_t, int16_t x2, int16_t) {
        s_window_left = x1;
        s_window_right = x2;
    }

    void Reset_Window() {
    }
}

namespace video {
    void __cdecl Clip_On() {
    }

    void __cdecl Clip_Off() {
    }
}

namespace line {
    void __cdecl Line(int16_t, int16_t, int16_t, int16_t, uint8_t) {
        ++s_line_count;
    }
}

namespace animate {
    void __cdecl Draw(int16_t x, int16_t, s_animation_header*) {
        ++s_animation_draw_count;
        s_animation_x = x;
    }
}