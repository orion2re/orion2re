#include "pch.h"
#include <stdexcept>
#include "doctest/doctest.h"

namespace test_eric_runtime {
    s_player players[2];
    s_colony colonies[4];
    s_planet_data planets[4];
    s_ship_data ships[MAX_SHIPS];
    s_leader_data leaders[MAX_LEADERS];
    int16_t last_help_string;
    char help_string[1];
    int32_t fade_levels[101];
    int16_t fade_level_count;
    uint8_t cinematic_off[3];
    uint8_t cinematic_back[3];
    uint8_t cinematic_published[3];
    bool off_selected;
    bool block_popped;
    bool redraw_ready;
    bool palette_refreshed;
    bool check_cinematic_fade;
    bool popup_visible_during_fade;
    MOX2::action_func_t auto_function;
    s_animation_header popup_picture;
    int service_order;
    int colony_init_order;
    int events_init_order;
    int draw_count;
    int16_t draw_x, draw_y;
    bool draw_no_glass;
    int16_t paragraph_x, paragraph_y, paragraph_width, paragraph_alignment;
    int16_t measured_width;
    std::string paragraph_text;
    int16_t box_args[6];
    int16_t cycle_args[8];
    int box_order, cycle_order;
    int16_t refresh_mode;
    int resource_load_count;
    bool block_pushed;
    int16_t assignment_delay;
    int movie_count;
    std::string movie_name;
    uint32_t movie_flags;
    char movie_fade;

    void Reset_Utilities_() {
        service_order = colony_init_order = events_init_order = 0;
        draw_count = resource_load_count = movie_count = 0;
        draw_x = draw_y = measured_width = 0;
        draw_no_glass = false;
        paragraph_x = paragraph_y = paragraph_width = paragraph_alignment = 0;
        paragraph_text.clear();
        memset(box_args, 0, sizeof(box_args));
        memset(cycle_args, 0, sizeof(cycle_args));
        box_order = cycle_order = 0;
        refresh_mode = -1;
        block_pushed = false;
        assignment_delay = -1;
        movie_name.clear();
        movie_flags = 0;
        movie_fade = 0;
        popup_picture = {};
        popup_picture.width = 31;
        popup_picture.height = 18;
    }

    void Saved_Auto_Function_() {}

    void Reset_Cinematic_() {
        Reset_Utilities_();
        ERIC::fade_speed = {25, 25};
        fade_level_count = 0;
        ERIC::fade_percent = 0;
        ERIC::saved_sfx_l = 35;
        ERIC::saved_music_l = 20;
        ERIC::cinematic_saved_autofunction = Saved_Auto_Function_;
        MOX::settings.sound_fx_level = 55;
        MOX::settings.music_level = 55;
        MOX::FULL_DRAW_SCREEN_FLAG = 1;
        MOX::dont_draw_main_screen_to_back = 0;
        // Movie cleanup has restored its saved background, including the frame.
        const uint8_t framed_background[] = {7, 9, 7};
        memcpy(cinematic_off, framed_background, sizeof(cinematic_off));
        memcpy(cinematic_back, framed_background, sizeof(cinematic_back));
        memcpy(cinematic_published, framed_background, sizeof(cinematic_published));
        off_selected = false;
        block_popped = false;
        redraw_ready = false;
        palette_refreshed = false;
        check_cinematic_fade = true;
        popup_visible_during_fade = false;
        auto_function = Saved_Auto_Function_;
    }

    bool Cinematic_Dismissed_() {
        const uint8_t background[] = {7, 7, 7};
        return redraw_ready && palette_refreshed &&
            auto_function == ERIC::cinematic_saved_autofunction &&
            memcmp(cinematic_off, background, sizeof(background)) == 0 &&
            memcmp(cinematic_back, background, sizeof(background)) == 0 &&
            memcmp(cinematic_published, background, sizeof(background)) == 0;
    }

    bool Popup_Visible_During_Fade_() {
        return popup_visible_during_fade;
    }

    void Reset_() {
        Reset_Utilities_();
        MOX::NUM_COLONIES = 1;
        memset(players, 0, sizeof(players));
        memset(colonies, 0, sizeof(colonies));
        memset(planets, 0, sizeof(planets));
        memset(ships, 0, sizeof(ships));
        memset(leaders, 0, sizeof(leaders));

        players[0].traits[TRAIT_TELEPATHIC] = 1;
        colonies[0].owner = 1;
        colonies[0].planet = 0;
        planets[0].star_index = 3;

        ships[0].owner = 0;
        ships[0].status = 0;
        ships[0].location = 3;
        ships[0].d.size = SHIP_SIZE_LARGE;

        leaders[0].player_index = 1;
        leaders[0].status = 1;
        leaders[0].eta = 0;
        leaders[0].type = LEADER_TYPE_COLONY;
        leaders[0].location = 3;

        last_help_string = -1;
    }

    void Reset_Fade_() {
        ERIC::fade_percent = 100;
        ERIC::fade_speed = {25, 25};
        fade_level_count = 0;
        check_cinematic_fade = false;
    }

    int16_t Fade_Level_Count_() {
        return fade_level_count;
    }

    int32_t Fade_Level_(int16_t index) {
        return fade_levels[index];
    }

    s_leader_data* Defender_Leader_() {
        return &leaders[0];
    }

    int16_t Last_Help_String_() {
        return last_help_string;
    }
}

namespace MOX {
    int16_t NUM_COLONIES;
    int16_t current_screen;
    int8_t previous_screen;
    s_mouse_list mouse_list_none;
    s_settings settings;
    s_animation_header* screen_seg;
    s_mouse_list mouse_list_arrow;
    uint16_t FULL_DRAW_SCREEN_FLAG;
    uint8_t dont_draw_main_screen_to_back;
    s_player* player = test_eric_runtime::players;
    s_colony* colony = test_eric_runtime::colonies;
    s_planet_data* planet = test_eric_runtime::planets;
    s_ship_data* ship = test_eric_runtime::ships;
    s_leader_data* leaders = test_eric_runtime::leaders;
}

namespace OFFICER {
    bool __cdecl Officer_Has_General_Skill(int16_t, int16_t leader_idx, int16_t skill_idx) {
        uint32_t skill_mask = 0;
        if (skill_idx == 0) {
            skill_mask = LEADER_GENERAL_SKILL_ASSASSIN;
        } else if (skill_idx == 48) {
            skill_mask = LEADER_GENERAL_SKILL_TELEPATH;
        } else if (skill_idx == 49) {
            skill_mask = LEADER_GENERAL_SKILL_TELEPATH2;
        }
        return (MOX::leaders[leader_idx].general_skills & skill_mask) != 0;
    }
}

namespace ESTRINGS {
    char* __cdecl E_Strings(int16_t string_id) {
        test_eric_runtime::last_help_string = string_id;
        static char combined[] = "%s%c%s";
        static char overflow[] = "String builder overflow";
        if (string_id == 71) return combined;
        if (string_id == 408) return overflow;
        return test_eric_runtime::help_string;
    }
}

namespace GENDRAW {
    void __cdecl Help(const char*) {
    }
}

namespace palstore {
    void __cdecl Darken_Palette(int32_t fade_level) {
        test_eric_runtime::fade_levels[test_eric_runtime::fade_level_count++] =
            fade_level;
    }
}

namespace video {
    void __cdecl Set_Refresh_Full() {
        test_eric_runtime::refresh_mode = 0;
    }

    void __cdecl Submit_Palette() {
        if (test_eric_runtime::check_cinematic_fade) {
            for (uint8_t pixel : test_eric_runtime::cinematic_published) {
                if (pixel != 7) {
                    test_eric_runtime::popup_visible_during_fade = true;
                }
            }
        }
    }

    void __cdecl Set_Refresh_Stencil() {
        test_eric_runtime::refresh_mode = 3;
    }

    void __cdecl Set_Refresh_Differential() {
        test_eric_runtime::refresh_mode = 2;
    }

    void __cdecl Copy_Off_To_Back() {
        memcpy(test_eric_runtime::cinematic_back, test_eric_runtime::cinematic_off, 3);
    }

    void __cdecl Set_Page_Off() {
        test_eric_runtime::off_selected = true;
    }

    void __cdecl Set_Page_Back() {
        test_eric_runtime::off_selected = false;
    }

    void __cdecl Copy_Back_To_Off() {
        memcpy(test_eric_runtime::cinematic_off, test_eric_runtime::cinematic_back, 3);
    }

    void __cdecl Publish_Off_Page() {
        memcpy(test_eric_runtime::cinematic_published, test_eric_runtime::cinematic_off, 3);
    }
}

namespace RUSS {
    void __cdecl Mox_Update() {
    }

    void __cdecl Mox_Sync_Update() {
        video::Publish_Off_Page();
    }
}

namespace fields {
    void __cdecl Assign_Auto_Function(MOX2::action_func_t function, int16_t delay) {
        test_eric_runtime::auto_function = function;
        test_eric_runtime::assignment_delay = delay;
    }
    void __cdecl Deactivate_Auto_Function() {
        test_eric_runtime::auto_function = nullptr;
    }
} // namespace fields

namespace MOX2 {
    void __cdecl Get_Auto_Function(action_func_t* slot) {
        *slot = test_eric_runtime::auto_function;
    }
    void __cdecl Put_Auto_Function(action_func_t* slot) {
        test_eric_runtime::auto_function = *slot;
    }
} // namespace MOX2

namespace allocate {
    void __cdecl Push_Block(void*) {
        test_eric_runtime::block_pushed = true;
        test_eric_runtime::block_popped = false;
    }
    void __cdecl Pop_Block(void*) {
        test_eric_runtime::block_popped = true;
    }
} // namespace allocate

namespace mouse {
    void __cdecl Set_Mouse_List(s_mouse_list*, int16_t) {}
} // namespace mouse

namespace MAINSCR {
    void Draw_Mini_Main_Screen() {
        test_eric_runtime::redraw_ready = test_eric_runtime::off_selected &&
            test_eric_runtime::block_popped && test_eric_runtime::auto_function == nullptr &&
            MOX::FULL_DRAW_SCREEN_FLAG != 0 && MOX::dont_draw_main_screen_to_back == 0;
        memset(test_eric_runtime::cinematic_off, 7, 3);
        if (MOX::dont_draw_main_screen_to_back == 0) {
            memcpy(test_eric_runtime::cinematic_back, test_eric_runtime::cinematic_off, 3);
        }
        MOX::FULL_DRAW_SCREEN_FLAG = 0;
    }
} // namespace MAINSCR

namespace palette {
    void __cdecl Cycle_Palette_Color(int16_t index, int16_t r1, int16_t g1, int16_t b1,
        int16_t r2, int16_t g2, int16_t b2, int16_t delay) {
        const int16_t args[] = {index, r1, g1, b1, r2, g2, b2, delay};
        memcpy(test_eric_runtime::cycle_args, args, sizeof(args));
        test_eric_runtime::cycle_order = ++test_eric_runtime::service_order;
    }
    void __cdecl Refresh_Palette() {
        test_eric_runtime::palette_refreshed = true;
    }

    void __cdecl Clear_Palette_Changes(int16_t, int16_t) {}
} // namespace palette

namespace capture {
    uint16_t __cdecl Check_Release_Version() {
        return 0;
    }
} // namespace capture

namespace graphics {
    void __cdecl Box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color, uint8_t fill) {
        const int16_t args[] = {x1, y1, x2, y2, color, fill};
        memcpy(test_eric_runtime::box_args, args, sizeof(args));
        test_eric_runtime::box_order = ++test_eric_runtime::service_order;
    }
    void __cdecl Fill(int16_t, int16_t, int16_t, int16_t, uint8_t color) {
        memset(test_eric_runtime::off_selected ? test_eric_runtime::cinematic_off : test_eric_runtime::cinematic_back, color, 3);
    }
} // namespace graphics

namespace COLONY {
    const char AMEBAFIN_LBX[13] = "AMEBAFIN.LBX";
    const char DIMTVFIN_LBX[13] = "DIMTVFIN.LBX";
    const char PLNTDFIN_LBX[13] = "PLNTDFIN.LBX";
    const char MAINPUPS_LBX[13] = "MAINPUPS.LBX";
    void __cdecl Init_Colonies() {
        test_eric_runtime::colony_init_order = ++test_eric_runtime::service_order;
    }
}

namespace EVENTS {
    void __cdecl Init_Events() {
        test_eric_runtime::events_init_order = ++test_eric_runtime::service_order;
    }
}

namespace animate {
    void __cdecl Draw(int16_t x, int16_t y, s_animation_header* picture) {
        ++test_eric_runtime::draw_count;
        test_eric_runtime::draw_x = x;
        test_eric_runtime::draw_y = y;
        test_eric_runtime::draw_no_glass = false;
        if (picture == &test_eric_runtime::popup_picture) test_eric_runtime::cinematic_off[1] = 9;
    }
    void __cdecl Draw_No_Glass(int16_t x, int16_t y, s_animation_header* picture) {
        Draw(x, y, picture);
        test_eric_runtime::draw_no_glass = true;
    }
}

namespace farload {
    void* __cdecl Far_Reload_Next(const char* name, int32_t entry, void*) {
        DOCTEST_CHECK(strcmp(name, COLONY::MAINPUPS_LBX) == 0);
        DOCTEST_CHECK(entry == 0x53);
        ++test_eric_runtime::resource_load_count;
        return &test_eric_runtime::popup_picture;
    }
}

namespace BILL {
    int16_t __cdecl _Get_Paragraph_Max_Height(int16_t width, const char*) {
        test_eric_runtime::measured_width = width;
        return 20;
    }
    void __cdecl _Print_Paragraph(int16_t x, int16_t y, int16_t width, const char* text, int16_t alignment) {
        test_eric_runtime::paragraph_x = x;
        test_eric_runtime::paragraph_y = y;
        test_eric_runtime::paragraph_width = width;
        test_eric_runtime::paragraph_alignment = alignment;
        test_eric_runtime::paragraph_text = text;
    }
}

namespace SMACK {
    void __cdecl Play_Cinematic(const char* name, uint32_t flags, char fade) {
        ++test_eric_runtime::movie_count;
        test_eric_runtime::movie_name = name;
        test_eric_runtime::movie_flags = flags;
        test_eric_runtime::movie_fade = fade;
    }
}

namespace exitm {
    [[noreturn]] void __cdecl Exit_With_Message(const char* message) {
        throw std::runtime_error(message);
    }
}