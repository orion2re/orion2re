#include "pch.h"

namespace MULTPLAY {
    int16_t hotseat_player_hotkeys[MAX_PLAYERS];

    const char s__00551100[1] = "";
    const char s__00551120[2] = "\x1B";

    void __cdecl Add_MP_Setup_Screen_Fields(void) {
        s_animation_header* anim;
        int16_t x, y;

        fields::Clear_Fields();

        x = MOX::mp_screen->screen_x;
        y = MOX::mp_screen->screen_y;

        // LAN/router hidden field.
        anim = MOX::mp_screen->anim_network;
        MOX::mp_screen->field_id_network = fields::Add_Hidden_Field(
            x + 0x3B,
            y + 0x5B,
            x + 0x3B + animate::Get_Width(anim),
            y + 0x5B + animate::Get_Height(anim),
            "N",
            0x28 /* 40 */
        );

        // Online hidden field.
        anim = MOX::mp_screen->anim_online;
        MOX::mp_screen->field_id_online = fields::Add_Hidden_Field(
            x + 0x3B,
            y + 0x7A,
            x + 0x3B + animate::Get_Width(anim),
            y + 0x7A + animate::Get_Height(anim),
            "M",
            0x28 /* 40 */
        );

        // Hotseat hidden field.
        anim = MOX::mp_screen->anim_hotseat;
        MOX::mp_screen->field_id_hotseat = fields::Add_Hidden_Field(
            x + 0x3B,
            y + 0x9B,
            x + 0x3B + animate::Get_Width(anim),
            y + 0x9B + animate::Get_Height(anim),
            "H",
            0x28 /* 40 */
        );

        MOX::mp_screen->setup_screen_state = 0;

        // Cancel button
        MOX::mp_screen->field_id_cancel = fields::Add_Button_Field(
            x + 0xB0,
            y + 0x11E,
            s__00551100,
            MOX::mp_screen->anim_btn_cancel,
            s__00551120,
            0x28 /* 40 */
        );

        // Check if any multi-player method is enabled
        if (MOX::mp_screen->flag_network_disabled != 1 &&
            MOX::mp_screen->flag_online_disabled != 1 &&
            MOX::mp_screen->flag_hotseat_disabled != 1) {
            MOX::mp_screen->field_id_start_game = -1000;
            MOX::mp_screen->field_id_load_game = -1000;
            MOX::mp_screen->field_id_join_game = -1000;
            MOX::mp_screen->field_id_setup = -1000;
            Set_Multi_Player_Game_Type();
            return;
        }

        // Start Game button
        MOX::mp_screen->field_id_start_game = fields::Add_Button_Field(
            x + 0x10D,
            y + 0x5B,
            s__00551100,
            MOX::mp_screen->anim_btn_start_game,
            "S",
            0x28 /* 40 */
        );

        // Load Game button
        MOX::mp_screen->field_id_load_game = fields::Add_Button_Field(
            x + 0x10D,
            y + 0x7A,
            s__00551100,
            MOX::mp_screen->anim_btn_load_game,
            "L",
            0x28 /* 40 */
        );

        // Join Game button
        if (MOX::mp_screen->flag_hotseat_disabled == 1) {
            MOX::mp_screen->field_id_join_game = -1000;
        } else {
            MOX::mp_screen->field_id_join_game = fields::Add_Button_Field(
                x + 0x10D,
                y + 0x9B,
                s__00551100,
                MOX::mp_screen->anim_btn_join_game,
                "J",
                0x28 /* 40 */
            );
        }

        // Router configuration is supplied by the local interface config.
        MOX::mp_screen->field_id_setup = -1000;

        Set_Multi_Player_Game_Type();
    }

    int8_t __cdecl Hotseat_Player_Status(int16_t player_idx) {
        int8_t status = 0;

        if (MOX::player[player_idx].objectives == PLAYER_OBJECTIVE_HUMAN && MOX::player[player_idx].eliminated == 0) {
            status = (int8_t)((MOX::player[player_idx].player_done_flags == 0) + 1);
        }

        return status;
    }

    void __cdecl Add_Banner_Field(int16_t player_idx, int16_t vertical_idx) {
        int16_t x1 = MOX::mp_screen->screen_x + 40;
        int16_t y1 = MOX::mp_screen->screen_y + 65 + vertical_idx * 36;

        uint8_t race = static_cast<uint8_t>(MOX::player[player_idx].race);
        uint8_t color = static_cast<uint8_t>(MOX::player[player_idx].color);

        int16_t lbx_entry = color + (race * 8) + 46;

        s_animation_header* banner_anim = (s_animation_header*)buffer::Buffer_Reload(s_MULTIGM_LBX_00558d04, lbx_entry, MOX::global_cache_seg);

        int16_t width = animate::Get_Width(banner_anim);
        int16_t height = animate::Get_Height(banner_anim);

        int16_t field_id = fields::Add_Hidden_Field(x1, y1, x1 + width, y1 + height, s__00551100, 41);

        MOX::mp_screen->game_list_field_ids[player_idx] = field_id;
    }

    void __cdecl Hotseat_Add_Next_Player_Fields(void) {
        int16_t banner_idx = 0;
        int16_t y_offset = 0;

        for (int16_t player_idx = 0; player_idx < MAX_PLAYERS; ++player_idx) {
            hotseat_player_hotkeys[player_idx] = -1000;
        }

        if (MOX::NUM_PLAYERS > 0) {
            for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                if (MOX::player[player_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    if (MOX::player[player_idx].eliminated != 0) {
                        MOX::mp_screen->player_field_ids[player_idx] = -1000;
                        MOX::mp_screen->game_list_field_ids[player_idx] = -1000;
                    } else {
                        const int16_t left = static_cast<int16_t>(MOX::mp_screen->screen_x + 0x6a);
                        const int16_t top = static_cast<int16_t>(MOX::mp_screen->screen_y + y_offset + 0x40);
                        const int16_t right = static_cast<int16_t>(MOX::mp_screen->screen_x + 0x1b3);
                        const int16_t bottom = static_cast<int16_t>(MOX::mp_screen->screen_y + y_offset + 0x5d);

                        if ((MOX::mp_screen->player_button_status[player_idx] == 1) ||
                            (MOX::mp_screen->player_button_status[player_idx] == 2)) {
                            y_offset = static_cast<int16_t>(y_offset + 0x24);

                            MOX::mp_screen->player_field_ids[player_idx] =
                                fields::Add_Hidden_Field(left, top, right, bottom, s__00551100, 0x29);

                            MULTPLAY::Add_Banner_Field(player_idx, banner_idx);
                            if (MOX::mp_screen->player_button_status[player_idx] == 2) {
                                const char hotkey[2] = {
                                    (char)('1' + player_idx),
                                    '\0',
                                };
                                hotseat_player_hotkeys[player_idx] =
                                    fields::Add_Hot_Key(hotkey);
                            }
                            ++banner_idx;
                        }
                    }
                }
            }
        }
    }

    void __cdecl Set_Multi_Player_Game_Type(void) {
        const uint8_t multi_player_game_type = MOX::settings.multi_player_game_type;

        if (multi_player_game_type == 2 || multi_player_game_type == 3) {
            MOX::settings.multi_player_game_type = 2;
            MOX::game_type = 3;
            MOX::mp_screen->flag_network_disabled = 1;
            MOX::mp_screen->flag_online_disabled = 0;
            MOX::mp_screen->flag_hotseat_disabled = 0;
            NETMOX::net.mode = 1;
            return;
        }

        if (multi_player_game_type == 4) {
            MOX::game_type = 1;
            MOX::mp_screen->flag_network_disabled = 0;
            MOX::mp_screen->flag_online_disabled = 0;
            MOX::mp_screen->flag_hotseat_disabled = 1;
            return;
        }

        MOX::game_type = 2;
        MOX::mp_screen->flag_network_disabled = 0;
        MOX::mp_screen->flag_online_disabled = 1;
        MOX::mp_screen->flag_hotseat_disabled = 0;
        NETMOX::net.mode = 0;
    }

    void __cdecl Hotseat_Set_Next_Player_Status(void) {
        int16_t ready_player_count = 0;

        for (int16_t player_idx = 0; player_idx < MAX_PLAYERS; ++player_idx) {
            if (player_idx < MOX::NUM_PLAYERS) {
                const int8_t player_status = MULTPLAY::Hotseat_Player_Status(player_idx);
                MOX::mp_screen->player_button_status[player_idx] = player_status;

                if ((MOX::player[player_idx].objectives == PLAYER_OBJECTIVE_HUMAN) &&
                    (MOX::player[player_idx].eliminated == 0)) {
                    MOX::mp_screen->forced_player_idx = player_idx;
                    ++MOX::mp_screen->force_player_selection;
                }

                if ((MOX::mp_screen->player_button_status[player_idx] == 1) &&
                    (MOX::player[player_idx].eliminated == 0)) {
                    ++ready_player_count;
                }
            }
        }

        if (MOX::mp_screen->force_player_selection == ready_player_count) {
            fields::Deactivate_Help_List();
            fields::Disable_Cancel();
            MOX::mp_screen->exit_screen_flag = 1;
            return;
        }

        MOX::mp_screen->exit_screen_flag = 0;
    }
} // namespace MULTPLAY
