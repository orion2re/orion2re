#include "pch.h"

namespace NETSTART {
    int8_t __cdecl Net_Pick_Position() {
        uint8_t connected_players[MAX_PLAYERS] = {};
        int16_t selection;
        int32_t message_buffer;

        const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        for (int16_t i = 0; i < player_count; ++i) {
            connected_players[i] = (MOX::player[i].network_player_id != -1);
        }

        if (MULTPLAY::Choose_Network_Plyrs_Screen(&selection, connected_players, false, true) == -1) {
            message_buffer = -1;
            NETMOX::Mox_Send_Message(NETMOX::net.host_election_id, NET_PACKET_PICK_YOUR_POSITION_RESULT, &message_buffer, sizeof(message_buffer));
            return 0;
        }

        fields::Clear_Fields();
        fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);

        NETMOX::net.network_data_ptr = nullptr;
        message_buffer = (int32_t)selection;
        NETMOX::Mox_Send_Message(NETMOX::net.host_election_id, NET_PACKET_PICK_YOUR_POSITION_RESULT, &message_buffer, sizeof(message_buffer));

        fields::Assign_Auto_Function(MULTPLAY::Draw_SendGet_Net_Info_Screen, 2);
        MULTPLAY::Reload_Getting_Data_Info();

        strlcpy(MOX::temp_string, NETTURN::waiting_message_fmt, sizeof(MOX::temp_string));

        while (NETMOX::net.network_data_ptr == nullptr) {
            fields::Get_Input();
            video::Set_Page_Off();
            MULTPLAY::Draw_SendGet_Net_Info_Screen();
            RUSS::Mox_Sync_Update();
        }

        NETMOX::net.sync_state = NET_SYNC_STATE_WAITING_FOR_GAME_DATA;
        NETMOX::Decode_Broadcast_Game_Data_Differential(NETMOX::net.network_data_ptr, NETMOX::net.total_bytes_received, 1);
        memset(NETMOX::net.player_phase_ready_sent_flags, 0, sizeof(NETMOX::net.player_phase_ready_sent_flags));
        NETMOX::net.network_data_ptr = nullptr;
        allocate::Pop_Block(MOX::screen_seg);

        MOX::temp_string[0] = '\0';
        MOX::PLAYER_NUM = -1;
        NETMOX::net.num_connected_players = 0;

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                NETMOX::net.num_connected_players++;
                if (MOX::player[i].network_player_id == NETMOX::net.net_host_id) {
                    MOX::PLAYER_NUM = (int16_t)i;
                }
            }
        }

        NETMOX::net.sync_state = NET_SYNC_STATE_READY;
        MOX2::Init_Map_Pictures_Default_();
        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
        MAPGEN::Init_Main_Screen(MOX::settings.galaxy_size);
        HAROLD::Harold_Init();
        MOX2::Init_Map_Defaults();
        MAINSCR2::main_screen_fade_in_flag = 1;
        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        return 1;
    }
} // namespace NETSTART
