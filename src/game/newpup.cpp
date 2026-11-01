#include "pch.h"
#include "game_events.h"

namespace NEWPUP {
    MOX2::action_func_t saved_autofunction;

    s_animation_header* anims;
    uint8_t rebel_display;
    static bool Draw_Troop_Popup_With_Read(leader_read::s_leader* leader);

    void __cdecl Troop_Popup() {
        game_events::s_leader_view_scope leader_view(leader_read::TROOP_SUMMARY);
        COLONY::full_draw = 1;
        fonts::Set_Font_Style_Shadow_Down(1, &COLONY::font_color_array);

        MOX2::Get_Auto_Function(&NEWPUP::saved_autofunction);
        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(NEWPUP::Draw_Troop_Popup, 1);

        ERICHELP::Set_Transport_Popup_Help_List();

        leader_read::s_leader leader;
        bool redrawn = Draw_Troop_Popup_With_Read(&leader);
        video::Set_Refresh_Stencil();
        NEWPUP::Reset_Fields();
        game_events::Ready_Leader_View(leader_read::TROOP_SUMMARY);
        if (redrawn) game_events::Publish_Leader_View(leader_read::TROOP_SUMMARY, &leader, leader.slot >= 0 ? 1 : 0);
        RUSS::Mox_Sync_Update();

        int16_t input;
        do {
            input = fields::Get_Input();
            redrawn = Draw_Troop_Popup_With_Read(&leader);
            NEWPUP::Reset_Fields();
            game_events::Ready_Leader_View(leader_read::TROOP_SUMMARY);
            if (redrawn) game_events::Publish_Leader_View(leader_read::TROOP_SUMMARY, &leader, leader.slot >= 0 ? 1 : 0);
            RUSS::Mox_Sync_Update();
        } while (input == 0);

        leader_view.Close();
        fields::Clear_Fields();
        fields::Deactivate_Help_List();
        fields::Deactivate_Auto_Function();
        MOX2::Put_Auto_Function(&NEWPUP::saved_autofunction);
    }

    void __cdecl Draw_Troop_Popup() {
        leader_read::s_leader leader;
        if (Draw_Troop_Popup_With_Read(&leader)) game_events::Publish_Leader_View(leader_read::TROOP_SUMMARY, &leader, leader.slot >= 0 ? 1 : 0);
    }

    static bool Draw_Troop_Popup_With_Read(leader_read::s_leader* leader) {
        int16_t summary_row;
        s_invasion_side_info invasion_info;
        int16_t planet_index;

        COLONY::Set_Colony_Font();

        if (COLONY::full_draw != 0) {
            COLONY::full_draw = 1;
            MOX2::Execute_Auto_Function(&NEWPUP::saved_autofunction);

            s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::COLGCBT_LBX, 0x15, MOX::global_cache_seg);
            graphics::Darken_Fill(3, 0x14A, (int16_t)(animate::Get_Width(anim) + 1), 0x1D9);

            NEWPUP::anims = (s_animation_header*)buffer::Buffer_Reload(COLONY::COLGCBT_LBX, 0x15, MOX::global_cache_seg);
            animate::Draw(2, 0x149, NEWPUP::anims);

            summary_row = 0;
            planet_index = HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);

            invasion::Compute_Colony_Ground_Combat_Info(
                &MOX::colony[MOX::planet[planet_index].colony_index],
                &invasion_info,
                rebel_display != 0);

            Print_Best_Summary(&invasion_info, &summary_row);
            Print_Race_Summary(&invasion_info, &summary_row);

            ++summary_row;
            Print_Troop_Totals(2, summary_row * 11 + 0x153, &invasion_info);

            video::Copy_Off_To_Back();
            leader_read::Read_Ground(MOX::PLAYER_NUM, &invasion_info, leader_read::TROOP_SUMMARY, leader);
            COLONY::full_draw = 0;
            return true;
        }
        return false;
    }

    void __cdecl Print_Data(char* label, int16_t value, int16_t* row, s_invasion_side_info* unused_side_info) {
        (void)unused_side_info;

        char text_buffer[500];
        s_animation_header* anim;

        /* fmtpara: \x1A0 left-aligns the label segment and \x1A1 right-aligns the value segment. */
        snprintf(
            text_buffer,
            sizeof(text_buffer),
            ESTRINGS::E_Strings(0x4B),
            "\x1A"
            "0",
            label,
            "\x1A"
            "1",
            value);

        COLONY::Set_Colony_Font_To(2);

        anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::COLGCBT_LBX, 0x15, MOX::global_cache_seg);

        FMTPARA::Print_Formatted_Paragraph(11, *row * 11 + 0x153, animate::Get_Width(anim) - 0x12, text_buffer, 0);

        ++(*row);
    }

    void __cdecl Print_Best_Summary(s_invasion_side_info* side_info, int16_t* row) {
        if (side_info->combat_modifiers.antigrav_bonus != 0) {
            Print_Data(
                TECHDATA::technology_applications[TECH_APP_ANTI_GRAV_HARNESS].name,
                (uint8_t)side_info->combat_modifiers.antigrav_bonus,
                row,
                side_info);
        }

        if (side_info->combat_modifiers.best_armor_tech_app_id != 0) {
            uint8_t tech_app_id = side_info->combat_modifiers.best_armor_tech_app_id;

            Print_Data(
                TECHDATA::technology_applications[tech_app_id].name,
                (uint8_t)side_info->combat_modifiers.troop_bonus,
                row,
                side_info);
        }

        if (side_info->combat_modifiers.personal_shield_tech_app_id != 0) {
            uint8_t tech_app_id = side_info->combat_modifiers.personal_shield_tech_app_id;

            Print_Data(
                TECHDATA::technology_applications[tech_app_id].name,
                (uint8_t)side_info->combat_modifiers.personal_shield_bonus,
                row,
                side_info);
        }

        if (side_info->combat_modifiers.rifle_tech_app_id != 0) {
            uint8_t tech_app_id = side_info->combat_modifiers.rifle_tech_app_id;

            Print_Data(
                TECHDATA::technology_applications[tech_app_id].name,
                (uint8_t)side_info->combat_modifiers.rifle_bonus,
                row,
                side_info);
        }
    }

    void __cdecl Print_Race_Summary(s_invasion_side_info* side_info, int16_t* row) {
        char officer_name[52];

        if (side_info->combat_modifiers.ground_combat_bonus != 0) {
            Print_Data(
                ESTRINGS::E_Strings(0x1B5),
                (int16_t)side_info->combat_modifiers.ground_combat_bonus,
                row,
                side_info);
        }

        if (side_info->combat_modifiers.low_g_penalty != 0) {
            Print_Data(
                ESTRINGS::E_Strings(0x16C),
                (int16_t)side_info->combat_modifiers.low_g_penalty,
                row,
                side_info);
        }

        const int16_t subterranean_bonus = (int16_t)(uint8_t)side_info->combat_modifiers.subterranean_bonus;
        if (subterranean_bonus != 0) {
            Print_Data(
                ESTRINGS::E_Strings(0x1F1),
                subterranean_bonus,
                row,
                side_info);
        }

        if (side_info->combat_modifiers.difficulty_bonus != 0 &&
            capture::Check_Release_Version() != 0) {
            Print_Data(
                ESTRINGS::E_Strings(0x007B),
                (int16_t)side_info->combat_modifiers.difficulty_bonus,
                row,
                side_info);
        }

        if (side_info->combat_modifiers.officer_idx > 0) {
            leader_read::s_leader leader;
            if (leader_read::Read_Ground(MOX::PLAYER_NUM, side_info, leader_read::TROOP_SUMMARY, &leader)) {
                Print_Data(leader.name, leader.bonus, row, side_info);
                return;
            }
            OFFICER::Officer_Name(
                officer_name,
                sizeof(officer_name),
                side_info->player_id,
                (int16_t)side_info->combat_modifiers.officer_idx);

            Print_Data(
                officer_name,
                side_info->combat_modifiers.officer_bonus,
                row,
                side_info);
        }
    }

    void __cdecl Reset_Fields() {
        fields::Clear_Fields();
        COLXPORT::N_Colonies(MOX::PLAYER_NUM);
        fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "", 0);
    }

    void __cdecl Print_Troop_Totals(int16_t x_base, int16_t unused_y_base, s_invasion_side_info* invasion_side) {
        (void)unused_y_base;

        int16_t troop_type = 0;
        int16_t displayed_column_base = 0;
        char text_buffer[500];

        do {
            const int16_t player_id = invasion_side->player_id;
            const uint16_t race = (uint16_t)(uint8_t)MOX::player[player_id].race;
            s_animation_header* troop_anim =
                COLONY::Player_Troop_Anim(player_id, troop_type, race);
            const int16_t displayed_column = (int16_t)(displayed_column_base + 1);
            const int16_t row_index = 2;

            COLONY::Set_Colony_Font_To_Player(1, (uint8_t)player_id);

            snprintf(
                text_buffer,
                sizeof(text_buffer),
                ESTRINGS::E_Strings(0x0d),
                (int32_t)invasion_side->unit_strengths[troop_type]);

            {
                const int32_t width = (int32_t)animate::Get_Width(
                    (s_animation_header*)buffer::Buffer_Reload(COLONY::COLGCBT_LBX, 0x15, MOX::global_cache_seg));
                const int16_t x = (int16_t)((((width + ((width >> 31) & 3)) >> 2) * displayed_column) + x_base);
                fonts::Print_Centered(x, 0x1d1, text_buffer);
            }

            {
                const uint8_t hit_points = (uint8_t)invasion_side->unit_hps[troop_type];

                snprintf(
                    text_buffer,
                    sizeof(text_buffer),
                    ESTRINGS::E_Strings((hit_points > 1) ? 0x17 : 0x16),
                    (uint32_t)hit_points);

                const int32_t width = (int32_t)animate::Get_Width(
                    (s_animation_header*)buffer::Buffer_Reload(COLONY::COLGCBT_LBX, 0x15, MOX::global_cache_seg));
                const int16_t x = (int16_t)((((width + ((width >> 31) & 3)) >> 2) * displayed_column) + x_base);
                fonts::Print_Centered(x, 0x1c8, text_buffer);
            }

            {
                const int32_t unit_count = (int32_t)invasion_side->unit_counts[troop_type];
                char* troop_name = COLONY::Player_Troop_Name(player_id, troop_type, race);

                snprintf(
                    text_buffer,
                    sizeof(text_buffer),
                    ESTRINGS::E_Strings(0x0e),
                    unit_count,
                    troop_name);

                const int32_t width = (int32_t)animate::Get_Width(
                    (s_animation_header*)buffer::Buffer_Reload(COLONY::COLGCBT_LBX, 0x15, MOX::global_cache_seg));
                const int16_t x = (int16_t)((((width + ((width >> 31) & 3)) >> 2) * displayed_column) + x_base);
                fonts::Print_Centered(x, 0x1d1 - row_index * 9, text_buffer);
            }

            {
                const int32_t width = (int32_t)animate::Get_Width(
                    (s_animation_header*)buffer::Buffer_Reload(COLONY::COLGCBT_LBX, 0x15, MOX::global_cache_seg));
                const int16_t x = (int16_t)((((width + ((width >> 31) & 3)) >> 2) * displayed_column) + x_base);
                ERIC::Draw_Bottom_Centered(x, 0x1d9 - (row_index + 1) * 9, troop_anim);
            }

            {
                const int32_t unit_count = (int32_t)invasion_side->unit_counts[troop_type];
                snprintf(text_buffer, sizeof(text_buffer), ESTRINGS::E_Strings(0x7c), unit_count);
            }

            ++troop_type;
            displayed_column_base = displayed_column;
        } while (troop_type < 3 + rebel_display);
    }
} // namespace NEWPUP