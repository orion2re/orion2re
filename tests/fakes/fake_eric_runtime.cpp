#include "pch.h"

namespace test_eric_runtime {
    s_player players[2];
    s_colony colonies[1];
    s_planet_data planets[1];
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

    void Saved_Auto_Function_() {}

    void Reset_Cinematic_() {
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
    void __cdecl Deactivate_Auto_Function() {
        test_eric_runtime::auto_function = nullptr;
    }
} // namespace fields

namespace MOX2 {
    void __cdecl Put_Auto_Function(action_func_t* slot) {
        test_eric_runtime::auto_function = *slot;
    }
} // namespace MOX2

namespace allocate {
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
    void __cdecl Fill(int16_t, int16_t, int16_t, int16_t, uint8_t color) {
        memset(test_eric_runtime::off_selected ? test_eric_runtime::cinematic_off : test_eric_runtime::cinematic_back, color, 3);
    }
} // namespace graphics