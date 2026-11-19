#include "pch.h"

namespace COLLAND {
    s_animation_header* l_anims;
    int32_t pod1l_sfx;
    int32_t pod2d_sfx;
    int32_t pod2l_sfx;
    int32_t pod1d_sfx;
    int16_t fields;

    void __cdecl Print_Planet_Data(::game::s_world_state* world_state, ::game::id_planet planet_idx) {
        char buffer[500];
        s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));

        COLONY::Set_Colony_Font_To(4);

        if (planet->colony_index == -1) {
            char* planet_name = HACCESS::Get_Planet_Name(legacy_world::Planet_Id(planet_idx));
            char* fmt = ESTRINGS::E_Strings(0x46 /* ESTR_S1 */);
            snprintf(buffer, sizeof(buffer), fmt, planet_name);
        } else {
            s_colony* colony = &MOX::colony[planet->colony_index];
            char* race_name = MOX::player[colony->owner].race_name;
            char* planet_name = HACCESS::Get_Planet_Name(legacy_world::Planet_Id(planet_idx));
            char* fmt = ESTRINGS::E_Strings(0x28 /* ESTR_S__S_COLONY */);
            snprintf(buffer, sizeof(buffer), fmt, planet_name, race_name);
        }
        fonts::Print(0x14, 0x28, buffer);

        char* climate_str;
        char* size_str = MOX::planet_size_string[planet->size];

        if (planet->colony_index == -1) {
            if (planet->planet_type != PLANET_TYPE_PLANET) {
                char* gas_giant_msg = ESTRINGS::E_Strings(0x132 /* ESTR_GAS_GIANT__UNINHABITABLE */);
                strlcpy(buffer, gas_giant_msg, sizeof(buffer));
                fonts::Print(0x14, 0x3c, buffer);
                return;
            }
            climate_str = MOX::planet_climate_string[planet->climate];
        } else {
            climate_str = MOX::planet_climate_string[(uint8_t)MOX::colony[planet->colony_index].climate];
        }

        const char* size_climate_fmt;
        if (MOX::settings.language == 4) {
            size_climate_fmt = "%s, %s";
        } else {
            size_climate_fmt = ESTRINGS::E_Strings(0x2d /* ESTR_S_S */);
        }

        snprintf(buffer, sizeof(buffer), size_climate_fmt, size_str, climate_str);
        BILL::_Print_Paragraph(0x14, 0x3c, 0x26b, buffer, 0);

        int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(planet, MOX::PLAYER_NUM);
        char* fmt_pop = ESTRINGS::E_Strings(0x1f);
        snprintf(buffer, sizeof(buffer), fmt_pop, (int)max_pop);
        fonts::Print(0x14, 0x50, buffer);

        char* mineral_str = MOX::mineral_class_string[planet->mineral_class];
        char* fmt_minerals = ESTRINGS::E_Strings(0x176);
        snprintf(buffer, sizeof(buffer), fmt_minerals, mineral_str);
        fonts::Print(0x14, 100, buffer);

        fonts::Print(0x14, 0x78, MOX::planet_gravity_string[planet->gravity_class]);

        if (planet->planet_special != SYSTEM_SPECIAL_NO_SPECIAL) {
            char* special_str = MOX::planet_special_string[planet->planet_special];
            char* fmt_special = ESTRINGS::E_Strings(0x46 /* ESTR_S1 */);
            snprintf(buffer, sizeof(buffer), fmt_special, special_str);
            BILL::_Print_Paragraph(0x14, 0x8c, 0x26b, buffer, 0);
        }
    }

    void __cdecl Load_Anims() {
        COLLAND::l_anims = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLONY_LBX, 6, MOX::screen_seg);
    }

    void Add_Fields() {
        ::fields::Clear_Fields();

        char* hotkey = ESTRINGS::E_Strings(12);

        fields = ::fields::Add_Hidden_Field(
            0,
            0,
            639,
            479,
            hotkey,
            0);
    }

    void __cdecl Draw_Colony_Landing_Screen() {
        char buffer[500];

        video::Set_Page_Off();

        if (COLONY::full_draw != 0) {
            graphics::Fill(0, 0, 0x27F, 0x1DF, 0);

            s_animation_header* anim1 = COLONY::C_Anims(&::game::world, 1);
            animate::Draw(0, 0, anim1);

            s_animation_header* anim0 = COLONY::C_Anims(&::game::world, 0);
            animate::Draw(0, 0, anim0);

            COLONY::Set_Colony_Font_To(4);

            char* planet_name = HACCESS::Get_Planet_Name(legacy_world::Planet_Id(COLONY::planet_handle));
            char* title_fmt = ESTRINGS::E_Strings(0xE5 /* ESTR_COLONY_BUILT_ON_S */);
            snprintf(buffer, sizeof(buffer), title_fmt, planet_name);
            fonts::Print_Centered(0x13F, 10, buffer);

            if (COLONY::full_draw != 0) {
                video::Copy_Off_To_Back();
                video::Set_Refresh_Stencil();
                COLONY::full_draw = 0;
            }
        }

        animate::Draw(0, 0, COLLAND::l_anims);

        int16_t current_frame = animate::Get_Current_Frame(COLLAND::l_anims);
        int16_t frame_count = animate::Get_Animation_Frame_Count(COLLAND::l_anims);

        if (((int32_t)frame_count - 2) <= (int32_t)current_frame) {
            COLLAND::Print_Planet_Data(&::game::world, legacy_world::Planet_Id(COLONY::planet_handle));
        }

        if (current_frame == 1) {
            COLLAND::pod1d_sfx = ERICNET::Mox_Play_Sound(0x3A);
            return;
        }

        if (current_frame == 2) {
            COLLAND::pod2d_sfx = ERICNET::Mox_Play_Sound(0x3A);
            return;
        }

        if (current_frame == 0x39) {
            sound::Fadeout_Sound(COLLAND::pod1d_sfx, 2);
            COLLAND::pod1l_sfx = ERICNET::Mox_Play_Sound(0x39);
            return;
        }

        if (current_frame == 0x3E) {
            sound::Fadeout_Sound(COLLAND::pod2d_sfx, 2);
            COLLAND::pod2l_sfx = ERICNET::Mox_Play_Sound(0x39);
        }
    }

    void __cdecl Colony_Landing_Screen(::game::s_world_state* world_state) {
        MOX2::action_func_t saved_auto_fn;
        int16_t input_result;

        COLONY::Set_Colony_Font();
        COLONY::Load_Colony_Screen_Seg();
        video::Clip_Off();
        COLONY::using_colony_screen_palette = 1;

        COLONY::star_handle = MOX::screen_data;
        COLONY::orbit_n = COLONY::orbit_temp;

        COLONY::planet_handle = HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);
        COLONY::colony_handle = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(COLONY::planet_handle))).colony_index;
        COLONY::valid_colony = 1;
        COLONY::drawing_display = 0;
        COLONY::sys_disp_star_handle = COLONY::star_handle;

        Load_Anims();
        COLONY::Load_Planet_Screen();

        if (ERIC::Fast_Fade_Out_Needed_For_Colony_Palette()) {
            ERIC::Fast_Fade_Out();
            graphics::Fill(0, 0, 639, 479, 0);
            COLONY::Update_Colony_Palette();
        }

        if (ERIC::Fast_Fade_Out_Needed_For_Colony_Palette()) {
            palstore::Darken_Palette(100);
            palette::Clear_Palette_Changes(0, 255);
        }

        palette::Refresh_Palette();
        COLONY::Reset_Field_Arrays();
        Add_Fields();
        COLONY::full_draw = 1;

        MOX2::Get_Auto_Function(&saved_auto_fn);
        ::fields::Assign_Auto_Function(Draw_Colony_Landing_Screen, 1);
        Draw_Colony_Landing_Screen();
        ERIC::Fast_Fade_In();
        video::Set_Refresh_Stencil();

        if (COLONY::Colony_Needs_Occupation_Policy(COLONY::colony_handle)) {
            OCPOLPUP::Occupation_Policy_Popup(world_state);
            COLONY::Col_Calc_Wrapper(COLONY::colony_handle);
        }

        do {
            timer::Mark_Time();
            Add_Fields();
            Draw_Colony_Landing_Screen();
            animate::Get_Animation_Frame_Count(COLLAND::l_anims);
            animate::Get_Current_Frame(COLLAND::l_anims);
            RUSS::Mox_Sync_Update();
            input_result = ::fields::Get_Input();
            ::fields::Scan_Input();
            timer::Release_Time(1);
        } while (input_result == 0);

        COLONY::Reset_Field_Mode();
        ::fields::Clear_Fields();
        ::fields::Deactivate_Auto_Function();

        sound::Fadeout_Sound(pod1d_sfx, 10);
        sound::Fadeout_Sound(pod2d_sfx, 10);
        sound::Fadeout_Sound(pod1l_sfx, 10);
        sound::Fadeout_Sound(pod2l_sfx, 10);

        ::game::id_planet planet_idx = MOX::colony[COLONY::colony_handle].planet;
        const auto star_idx = game::Planet(world_state, planet_idx).Star_Position(MOX::star);

        if (COLCALC::N_Colonies_And_Outposts_At_Star(world_state, star_idx) == 1 && MOX::settings.animations_on) {
            int16_t cy = namestar::Star_Name_Popup_Screen_Center_Y();
            int16_t cx = namestar::Star_Name_Popup_Screen_Center_X();
            namestar::Change_Star_Name_Popup(cx, cy, star_idx, 1);
        }

        MOX2::Put_Auto_Function(&saved_auto_fn);
        COLONY::using_colony_screen_palette = 0;
        MOX::current_screen = 1; /* SCR_MAIN_VIEW */
    }
} // namespace COLLAND