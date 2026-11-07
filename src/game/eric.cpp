#include "pch.h"

namespace ERIC {
    uint8_t fade_percent;
    s_fade_speed fade_speed = {25, 25};
    uint8_t hacky_need_to_fade_to_colony_palette_flag;
    uint16_t saved_sfx_l;
    uint16_t saved_music_l;
    MOX2::action_func_t cinematic_saved_autofunction;

    uint16_t __cdecl Get_Fade_Percent() {
        return fade_percent;
    }

    void __cdecl Eric_Defaults() {
    }

    void __cdecl Eric_Init() {
        COLONY::Init_Colonies();
        EVENTS::Init_Events();
    }

    void __cdecl Set_Fade_Percent(int16_t percent) {
        fade_percent = (uint8_t)percent;

        if (percent == 0) {
            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        }
    }

    void __cdecl Set_Fade_Speed_Percent(uint8_t percentage) {
        fade_speed.fade_out = percentage;
    }

    int16_t __cdecl Get_Sint_Index_From_List(int16_t value, const void* list, uint16_t count) {
        if (count == 0) {
            return -1;
        }

        // Callers pass arrays from packed records, so copy each element out
        // rather than reading through a possibly misaligned int16_t pointer.
        const uint8_t* bytes = (const uint8_t*)list;
        for (uint16_t i = 0; i < count; ++i) {
            const int16_t element = unaligned::Load<int16_t>(bytes + i * sizeof(int16_t));
            if (element == value) {
                return (int16_t)i;
            }
        }

        return -1;
    }

    void __cdecl Fast_Fade_In() {
        if (fade_percent != 0) {
            video::Set_Refresh_Full();

            int16_t fade_level = (int16_t)fade_percent;

            while (fade_level >= 0) {
                palstore::Darken_Palette((int32_t)fade_level);
                fade_percent = (uint8_t)fade_level;
                video::Submit_Palette();
                RUSS::Mox_Update();
                fade_level -= fade_speed.fade_in;
            }
        }

        video::Set_Refresh_Stencil();
    }

    void Fast_Fade_Out() {
        bool faded = false;
        capture::Check_Release_Version();

        if (fade_percent == 0) {
            video::Set_Refresh_Full();

            uint16_t current_fade = fade_percent;
            if (current_fade < 101) {
                faded = true;
                do {
                    palstore::Darken_Palette(current_fade);
                    fade_percent = (uint8_t)current_fade;
                    video::Submit_Palette();
                    RUSS::Mox_Update();
                    current_fade += fade_speed.fade_out;
                } while (current_fade < 101);
            }

            if (faded) {
                video::Set_Page_Back();
                graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
                video::Copy_Back_To_Off();
                palette::Clear_Palette_Changes(0, 0xFF);
                RUSS::Mox_Sync_Update();
            }

            video::Set_Refresh_Stencil();
        }
    }

    int16_t __cdecl Global_Cycler(int16_t divisor, int16_t modulus) {
        uint64_t current_time = SDL_GetTicks();
        uint64_t cycle_index = current_time / (uint16_t)divisor;
        return (int16_t)(cycle_index % (uint16_t)modulus);
    }

    bool __cdecl Fast_Fade_Out_Needed_For_Colony_Palette() {
        if (hacky_need_to_fade_to_colony_palette_flag != 0) {
            return true;
        }

        uint8_t previous_screen = (uint8_t)MOX::previous_screen;

        switch (MOX::current_screen) {
            case SCREEN_COLONY:
                if (previous_screen == SCREEN_QUEUE_POPUP ||
                    previous_screen == SCREEN_COLONY_LANDING) {
                    return false;
                }
                return true;

            case SCREEN_COLONY_SUMMARY:
                return previous_screen != SCREEN_QUEUE_POPUP;

            case SCREEN_QUEUE_POPUP:
                if (previous_screen == SCREEN_COLONY ||
                    previous_screen == SCREEN_COLONY_LANDING ||
                    previous_screen == SCREEN_COLONY_SUMMARY) {
                    return false;
                }
                return true;

            case SCREEN_COLONY_LANDING:
                return true;

            default:
                return true;
        }
    }

    void __cdecl Draw_Centered(int16_t x, int16_t y, s_animation_header* animation) {
        int16_t width = animate::Get_Width(animation);
        int16_t height = animate::Get_Height(animation);

        int16_t x_centered = x - (width / 2);
        int16_t y_centered = y - (height / 2);

        animate::Draw(x_centered, y_centered, animation);
    }

    void __cdecl Draw_Bottom_Centered(int16_t x, int16_t y, s_animation_header* animation) {
        int16_t width = animate::Get_Width(animation);
        int16_t height = animate::Get_Height(animation);
        animate::Draw(x - (width / 2), y - height, animation);
    }

    void __cdecl Draw_Bottom_Centered_No_Glass(int16_t x, int16_t y, s_animation_header* animation) {
        int16_t width = animate::Get_Width(animation);
        int16_t height = animate::Get_Height(animation);
        animate::Draw_No_Glass(x - (width / 2), y - height, animation);
    }

    void __cdecl Print_Paragraph_Centered(int16_t x, int16_t y, int16_t width, int16_t height, const char* text) {
        int16_t text_height = BILL::_Get_Paragraph_Max_Height(width, text);

        int16_t y_centered = (int16_t)((int32_t)y +
            ((uint16_t)height >> 1) -
            ((int32_t)text_height / 2));

        BILL::_Print_Paragraph(x, y_centered, width, text, 2);
    }

    void __cdecl Print_Paragraph_Centered_Vertically(int16_t x, int16_t y, int16_t width, uint16_t box_height, char* text, int16_t color) {
        int16_t max_height = BILL::_Get_Paragraph_Max_Height(width, text);
        BILL::_Print_Paragraph(x, y + (box_height >> 1) - (max_height / 2), width, text, color);
    }

    const char* __cdecl Red_If_Negative_Fmt_String(int16_t value) {
        if (value >= 0) {
            return ESTRINGS::E_Strings(12);
        }
        /* fmtpara: ESC + '2' emits a text-attribute control sequence for subsequent characters. */
        return "\0332";
    }

    void __cdecl Play_Dimensional_Portal_Flic() {
        Do_Cinematic_Popup_Setup();
        SMACK::Play_Cinematic(COLONY::DIMTVFIN_LBX, 0, 0);
        Do_Cinematic_Popup_Cleanup();
    }

    void __cdecl Play_Planetary_Destruction_Flic() {
        Do_Cinematic_Popup_Setup();
        SMACK::Play_Cinematic(COLONY::PLNTDFIN_LBX, 0, 0);
        Do_Cinematic_Popup_Cleanup();
    }

    void __cdecl Do_Cinematic_Popup_Cleanup() {
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        fields::Deactivate_Auto_Function();
        allocate::Pop_Block(MOX::screen_seg);

        if (ERIC::saved_sfx_l != 0 || ERIC::saved_music_l != 0) {
            MOX::settings.sound_fx_level = (int8_t)ERIC::saved_sfx_l;
            MOX::settings.music_level = (int8_t)ERIC::saved_music_l;
        }

        // The movie's saved background includes the popup frame. Dismiss it
        // before a caller can fade the published image with the game palette.
        video::Set_Page_Off();
        MOX::FULL_DRAW_SCREEN_FLAG = 1;
        MOX::dont_draw_main_screen_to_back = 0;
        MAINSCR::Draw_Mini_Main_Screen();
        palette::Refresh_Palette();
        video::Publish_Off_Page();
        MOX2::Put_Auto_Function(&ERIC::cinematic_saved_autofunction);
    }

    bool __cdecl Sint_Is_In_List(int16_t value, int16_t* list, uint16_t count) {
        if (count == 0) {
            return false;
        }

        const uint8_t* bytes = (const uint8_t*)list;
        for (uint16_t i = 0; i < count; ++i) {
            if (unaligned::Load<int16_t>(bytes + i * sizeof(int16_t)) == value) {
                return true;
            }
        }

        return false;
    }

    void __cdecl Pulsing_Box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t box_color, int32_t reserved_arg, int16_t cycle_delay) {
        graphics::Box(x1, y1, x2, y2, box_color, 0xff);
        palette::Cycle_Palette_Color(0xff, 0x13, 0x10, 0x09, 0x20, 0x28, 0x15, cycle_delay);
    }

    void __cdecl Play_Loose_Flic() {
        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        SMACK::Play_Cinematic(ESTRINGS::E_Strings(0x16B), 0, 1);
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
    }

    void __cdecl Play_Normal_Win_Flic() {
        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        SMACK::Play_Cinematic(ESTRINGS::E_Strings(0x136), 0x400, 1);
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
    }

    void __cdecl Play_All_Flics() {
        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        SMACK::Play_Cinematic(ESTRINGS::E_Strings(0xb7), 0x400, 1);
        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        SMACK::Play_Cinematic(ESTRINGS::E_Strings(0x136), 0, 1);
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
    }

    void __cdecl Play_Take_Orion_Flic() {
        Do_Cinematic_Popup_Setup();
        SMACK::Play_Cinematic(ESTRINGS::E_Strings(0x194), 0, 0);
        Do_Cinematic_Popup_Cleanup();
    }

    void __cdecl Set_Refresh_Mode(int16_t mode) {
        if (mode == 0) {
            video::Set_Refresh_Full();
        } else if (mode == 2) {
            video::Set_Refresh_Differential();
        } else if (mode == 3) {
            video::Set_Refresh_Stencil();
        }
    }

    void __cdecl Do_Cinematic_Popup_Setup() {
        MOX2::Get_Auto_Function(&cinematic_saved_autofunction);
        fields::Deactivate_Auto_Function();

        if (MOX::settings.sound_fx_on == 0 && MOX::settings.music_on == 0) {
            saved_sfx_l = 0;
            saved_music_l = 0;
        } else {
            saved_sfx_l = MOX::settings.sound_fx_level;
            saved_music_l = MOX::settings.music_level;
            if (MOX::settings.music_level <= MOX::settings.sound_fx_level) {
                MOX::settings.music_level = MOX::settings.sound_fx_level;
            }
        }

        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        Cinematic_Autofunction();
        fields::Assign_Auto_Function(Cinematic_Autofunction, 1);
        allocate::Push_Block(MOX::screen_seg);
        video::Copy_Off_To_Back();
    }

    void __cdecl Play_Antarans_Attack_Flic() {
        Do_Cinematic_Popup_Setup();
        SMACK::Play_Cinematic(ESTRINGS::E_Strings(0xAE), 0, 0);
        Do_Cinematic_Popup_Cleanup();
    }

    void __cdecl Play_Diplomatic_Win_Flic() {
        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        SMACK::Play_Cinematic(ESTRINGS::E_Strings(0x22C), 0, 1);

        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        SMACK::Play_Cinematic(ESTRINGS::E_Strings(0x136), 0, 1);

        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
    }

    void __cdecl Play_Amoeba_Flic() {
        Do_Cinematic_Popup_Setup();
        SMACK::Play_Cinematic(COLONY::AMEBAFIN_LBX, 0, 0);
        Do_Cinematic_Popup_Cleanup();
    }

    int8_t __cdecl Player_Has_Ship_Size_Or_Larger_At_Star(int16_t player_idx, int16_t minimum_size, int16_t star_idx) {
        for (int16_t i = 0; i < MAX_SHIPS; ++i) {
            if (MOX::ship[i].location == star_idx &&
                MOX::ship[i].status == 0 &&
                MOX::ship[i].owner == player_idx &&
                MOX::ship[i].d.size >= minimum_size) {
                return 1;
            }
        }
        return 0;
    }

    int8_t __cdecl Player_Can_Mind_Control_Colony(int16_t player_idx, int16_t colony_idx) {
        if (MOX::player[player_idx].traits[TRAIT_TELEPATHIC] != 0) {
            if (Player_Has_Ship_Size_Or_Larger_At_Star(player_idx, mind_control_ship_class, MOX::planet[MOX::colony[colony_idx].planet].star_index)) {
                if (!Player_Has_Leader_With_General_Skill_At_Star(MOX::colony[colony_idx].owner, 48, MOX::planet[MOX::colony[colony_idx].planet].star_index)) {
                    if (!Player_Has_Leader_With_General_Skill_At_Star(MOX::colony[colony_idx].owner, 49, MOX::planet[MOX::colony[colony_idx].planet].star_index)) {
                        if (MOX::player[MOX::colony[colony_idx].owner].traits[TRAIT_TELEPATHIC] == 0) {
                            return 1;
                        }
                    }
                }
            }
        }
        return 0;
    }

    int8_t __cdecl Player_Can_Mind_Control_Colony_With_Help(int16_t player_idx, int16_t colony_idx) {
        if (MOX::player[player_idx].traits[TRAIT_TELEPATHIC] == 0) {
            GENDRAW::Help(ESTRINGS::E_Strings(0x233));
            return 0;
        }

        int16_t planet_idx = MOX::colony[colony_idx].planet;
        int16_t star_idx = MOX::planet[planet_idx].star_index;

        if (Player_Has_Ship_Size_Or_Larger_At_Star(player_idx, mind_control_ship_class, star_idx) == 0) {
            GENDRAW::Help(ESTRINGS::E_Strings(0x24C));
            return 0;
        }

        int16_t owner_idx = MOX::colony[colony_idx].owner;

        if (Player_Has_Leader_With_General_Skill_At_Star(owner_idx, 48, star_idx) != 0 ||
            Player_Has_Leader_With_General_Skill_At_Star(owner_idx, 49, star_idx) != 0) {
            GENDRAW::Help(ESTRINGS::E_Strings(0x200));
            return 0;
        }

        if (MOX::player[owner_idx].traits[TRAIT_TELEPATHIC] != 0) {
            GENDRAW::Help(ESTRINGS::E_Strings(0x201));
            return 0;
        }

        return 1;
    }

    void __cdecl Force_Fade_To_Colony_Palette(uint8_t flag) {
        hacky_need_to_fade_to_colony_palette_flag = flag;
    }

    int16_t __cdecl Player_Capitol_World(int16_t player_idx) {
        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            if (MOX::colony[i].buildings[BUILDING_CAPITOL] != 0 && MOX::colony[i].owner == player_idx) {
                return i;
            }
        }
        return Pick_Random_Colony(player_idx);
    }

    int16_t __cdecl Pick_Random_Colony(int16_t owner_idx) {
        int16_t count = 0;
        int16_t chosen_colony = -1;

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            if (MOX::colony[i].owner == owner_idx) {
                count++;
                if (game_random::Random(count) == 1) {
                    chosen_colony = i;
                }
            }
        }

        return chosen_colony;
    }

    void __cdecl Cinematic_Autofunction() {
        video::Set_Page_Off();

        s_animation_header* anim = (s_animation_header*)farload::Far_Reload_Next(
            COLONY::MAINPUPS_LBX, 0x53, MOX::screen_seg);
        int16_t width = animate::Get_Width(anim);
        int16_t x = ((640 - width) / 2) + 3;

        anim = (s_animation_header*)farload::Far_Reload_Next(
            COLONY::MAINPUPS_LBX, 0x53, MOX::screen_seg);
        int16_t height = animate::Get_Height(anim);
        int16_t y = ((480 - height) / 2) + 1;

        MOX::FULL_DRAW_SCREEN_FLAG = 1;
        MOX::dont_draw_main_screen_to_back = 1;

        MAINSCR::Draw_Mini_Main_Screen();

        anim = (s_animation_header*)farload::Far_Reload_Next(
            COLONY::MAINPUPS_LBX, 0x53, MOX::screen_seg);
        animate::Draw(x, y, anim);

        palette::Clear_Palette_Changes(0, 255);
    }

    void __cdecl String_Builder2(char* dest, int32_t dest_size, int16_t build_mode, const char* format, ...) {
        char local_buf[1000];
        char existing_buf[1000];
        va_list args;

        if (dest_size <= 0 || build_mode < 0) {
            return;
        }

        if (build_mode >= 0) {
            va_start(args, format);
            int formatted_length = vsnprintf(local_buf, sizeof(local_buf), format, args);
            va_end(args);

            if (formatted_length < 0 || (size_t)formatted_length >= sizeof(local_buf)) {
                exitm::Exit_With_Message(ESTRINGS::E_Strings(408 /* ESTR_PFMT__STRING_BUILDER */));
                return;
            }

            if (build_mode == 0) {
                strlcpy(dest, local_buf, (size_t)dest_size);
            } else {
                strlcpy(existing_buf, dest, sizeof(existing_buf));

                int combined_length = snprintf(dest, (size_t)dest_size, ESTRINGS::E_Strings(71), existing_buf, 13, local_buf);
                if (combined_length < 0 || combined_length >= dest_size) {
                    exitm::Exit_With_Message(ESTRINGS::E_Strings(408 /* ESTR_PFMT__STRING_BUILDER */));
                }
            }
        }
    }

    int8_t __cdecl Player_Has_Leader_With_General_Skill_At_Star(int16_t player_idx, int16_t skill_idx, int16_t star_idx) {
        for (int16_t i = 0; i < MAX_LEADERS; ++i) {
            if (MOX::leaders[i].player_index == player_idx &&
                MOX::leaders[i].status == 1 &&
                MOX::leaders[i].eta == 0) {
                if ((MOX::leaders[i].type == LEADER_TYPE_SHIP && MOX::ship[MOX::leaders[i].location].location == star_idx) ||
                    (MOX::leaders[i].type == LEADER_TYPE_COLONY && MOX::leaders[i].location == star_idx)) {
                    if (OFFICER::Officer_Has_General_Skill(player_idx, i, skill_idx)) {
                        return 1;
                    }
                }
            }
        }
        return 0;
    }
} // namespace ERIC