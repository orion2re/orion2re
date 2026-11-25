#include "pch.h"

namespace RACERPRT {

    s_colors report_normal_color = {{0x14, 0xB9, 0xB8, 0xB6, 0xB8, 0xB9, 0xB9, 0xBA}};
    uint8_t race_report_jim_hack;
    s_racescrn_icon_layout spy_group_data = {24, 216, 187, 239, 24, nullptr};

    void __cdecl Race_Report_Screen(game::id_player player) {
        int32_t paragraph_integer;
        char spy_status_text[40];
        s_animation_header* screen_anims[10];
        s_racescrn_mouse_group spy_icon_group;
        char war_heading_text[40];
        char war_race_list[128];
        char alliance_race_list[128];
        char personality_objective_text[128];
        char treaty_summary_text[256];

        auto& subject = game::current_state.players.at(player.id);

        if (subject.objectives == PLAYER_OBJECTIVE_HUMAN) {
            strlcpy(personality_objective_text, MOX::personality_labels[(uint8_t)subject.personality], sizeof(personality_objective_text));
        } else {
            /* fmtpara: '\r' starts a new paragraph between the personality and objective lines. */
            snprintf(personality_objective_text, sizeof(personality_objective_text), "%s\r%s", MOX::personality_labels[(uint8_t)subject.personality], MOX::objective_labels[subject.objectives]);
        }

        FMTPARA::Set_Paragraph_Integer_Pointer(0, &paragraph_integer);
        fields::Clear_Fields();
        BILLHELP::Report_Help();
        fields::Enable_Draw_All_Fields();
        RUSS::Mox_Update();

        s_animation_header* race_portrait_anim = (s_animation_header*)farload::Far_Reload("races.lbx", subject.race + 0x20, (void*)MOX::screen_seg);
        s_animation_header* spy_icon_sprite = (s_animation_header*)farload::Far_Reload_Next("races.lbx", subject.race + 0x2E, (void*)MOX::screen_seg);
        RUSS::Mox_Update();

        for (uint16_t i = 0; i < 10; ++i) {
            screen_anims[i] = (s_animation_header*)farload::Far_Reload_Next("racerprt.lbx", i, (void*)MOX::screen_seg);
            RUSS::Mox_Update();
        }

        int16_t spy_count = BILL::Get_Their_Spy_Number(player, MOX::local_player);
        RACESCRN::Setup_Icon_Group_Data(&spy_icon_group, &RACERPRT::spy_group_data, spy_count, spy_icon_sprite, nullptr);

        int16_t num_wars = 0;
        alliance_race_list[0] = '\0';
        int16_t num_alliances = 0;
        war_race_list[0] = '\0';

        if (game::current_state.player_count > 0) {
            for (int16_t other_player_idx = 0; other_player_idx < game::current_state.player_count; ++other_player_idx) {
                if (game::current_state.players[other_player_idx].id != player) {
                    if (subject.treaty[other_player_idx] == 2) {
                        strlcat(alliance_race_list, game::current_state.players[other_player_idx].race_name, sizeof(alliance_race_list));
                        strlcat(alliance_race_list, string::s_Newline, sizeof(alliance_race_list));
                        num_alliances++;
                    } else if (subject.treaty[other_player_idx] >= 4) {
                        strlcat(war_race_list, game::current_state.players[other_player_idx].race_name, sizeof(war_race_list));
                        strlcat(war_race_list, string::s_Newline, sizeof(war_race_list));
                        num_wars++;
                    }
                }
            }
        }

        if (num_alliances == 0) {
            JIM::Get_Text_Message("billtext.lbx", 0x1D, alliance_race_list, 0x28);
            strlcat(alliance_race_list, string::s_Newline, sizeof(alliance_race_list));
        }
        if (num_wars == 0) {
            JIM::Get_Text_Message("billtext.lbx", 0x1E, war_race_list, 0x28);
            strlcat(war_race_list, string::s_Newline, sizeof(war_race_list));
        }

        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x1F, spy_status_text, 0x28);
        JIM::Get_Text_Message("billtext.lbx", 0x20, war_heading_text, 0x28);
        RUSS::Mox_Update();

        /* fmtpara: '\n' inserts paragraph breaks between the treaty headings and their race-name lists. */
        snprintf(treaty_summary_text, sizeof(treaty_summary_text), "%s\n%s\n%s\n%s", spy_status_text, alliance_race_list, war_heading_text, war_race_list);

        INFO::g_exit_field = fields::Add_Button_Field(0x216, 0x1B1, "", screen_anims[1], "\x1B", '(');

        MOX::first_time = 1;

        while (true) {
            if (MOX::FULL_DRAW_SCREEN_FLAG == 1) {
                fields::Clear_Fields_Above(INFO::g_exit_field);
            }
            MOX::FULL_DRAW_SCREEN_FLAG = 0;

            video::Set_Page_Off();
            graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
            graphics::Tri_Interlaced_Fill(0xD4, 0x17, 0x26C, 0x1C9, 0xB2, 0xB3, 0xB3);
            animate::Draw(0, 0, screen_anims[0]);

            if (MOX::first_time != 0) {
                BILL::Turn_Off_Palettes(0, 0xFF);
            }

            animate::Draw(0x46, 0x1C, race_portrait_anim);
            graphics::Box(0x46, 0x1C, 0x4C, 0x58, 1, RACESCRN::banner_color_high.colors[(uint8_t)subject.color]);
            RUSS::Mox_Update();
            graphics::Box(0x47, 0x1D, 0x4A, 0x56, 1, RACESCRN::banner_color_low.colors[(uint8_t)subject.color]);

            fonts::Set_Font_Style(3, &RACERPRT::report_normal_color);
            BILL::Squeeze_Paragraph_Centered(0x2A, 0x87, 0x80, 0x1A, subject.name, 2);

            fonts::Set_Font_Style(4, &RACERPRT::report_normal_color);
            BILL::Squeeze_Paragraph_Centered(0x18, 0xA7, 0xA4, 0x2A, personality_objective_text, 2);

            if (spy_count == 0) {
                fonts::Set_Font_Style(3, &RACERPRT::report_normal_color);
                JIM::Get_Text_Message("billtext.lbx", 0x1C, spy_status_text, 0x28);
            } else {
                RACESCRN::Draw_Icon_Group(&spy_icon_group);
                fonts::Set_Font_Style(2, &RACERPRT::report_normal_color);
                if (spy_count <= 1) {
                    JIM::Get_Text_Message("billtext.lbx", 0x4B, spy_status_text, 0x28);
                } else {
                    JIM::Get_Text_Message("billtext.lbx", 0x4A, spy_status_text, 0x28);
                    JIM::Decode_Text_Stat(spy_status_text, spy_count, 0x28);
                }
            }

            BILL::Squeeze_Centered_Print(0x18, 0xDE, spy_status_text, 0xA4);
            RUSS::Mox_Update();

            fonts::Set_Font_Style(4, &RACERPRT::report_normal_color);
            BILL::Squeeze_Formatted_Paragraph_Centered(0x18, 0x123, 0xA4, 0xAA, treaty_summary_text, 2);

            video::Copy_Off_To_Back();
            ERIC::Fast_Fade_In();
            MOX::first_time = 0;
            RACERPRT::race_report_jim_hack = 1;

            allocate::Mark_Block((void*)MOX::screen_seg);
            int16_t action = INFO::_Tech_Review_Subscreen(screen_anims + 2, INFO::g_exit_field + 1, player);
            allocate::Release_Block((void*)MOX::screen_seg);

            RUSS::Mox_Update();
            RACERPRT::race_report_jim_hack = 0;

            if (action == INFO::g_exit_field) {
                ERIC::Fast_Fade_Out();
                return;
            }
        }
    }

} // namespace RACERPRT
