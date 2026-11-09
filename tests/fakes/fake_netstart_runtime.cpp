#include "pch.h"

namespace {
    s_player players_[MAX_PLAYERS];
    uint8_t game_data_;
    int32_t selected_position_ = -1;
    int32_t state_when_position_sent_ = -1;
    int32_t state_when_game_data_decoded_ = -1;
    int16_t selection_result_;
    int32_t messages_sent_;
} // namespace

namespace MOX {
    s_animation_header* screen_seg;
    char temp_string[512];
    int16_t PLAYER_NUM;
    s_player* player;
    s_settings settings;
    int16_t NUM_PLAYERS;
} // namespace MOX

namespace NETMOX {
    s_network net;

    int32_t __cdecl Mox_Send_Message(
        int16_t, uint16_t msg_type, void* packet, uint32_t packet_len) {
        if (msg_type == NET_PACKET_PICK_YOUR_POSITION_RESULT &&
            packet != nullptr && packet_len == sizeof(selected_position_)) {
            ++messages_sent_;
            memcpy(&selected_position_, packet, sizeof(selected_position_));
            state_when_position_sent_ = net.sync_state;
            net.network_data_ptr = &game_data_;
            net.total_bytes_received = sizeof(game_data_);
        }
        return 1;
    }

    int8_t __cdecl Decode_Broadcast_Game_Data_Differential(
        void*, size_t, uint8_t) {
        state_when_game_data_decoded_ = net.sync_state;
        return 1;
    }

} // namespace NETMOX

namespace TEST_NETSTART {
    void Reset_() {
        memset(players_, 0, sizeof(players_));
        game_data_ = 0;
        selected_position_ = -1;
        state_when_position_sent_ = -1;
        state_when_game_data_decoded_ = -1;
        selection_result_ = 0;
        messages_sent_ = 0;
        MOX::screen_seg = nullptr;
        MOX::temp_string[0] = '\0';
        MOX::PLAYER_NUM = 0;
        MOX::player = players_;
        MOX::settings = {};
        MOX::NUM_PLAYERS = 1;
        players_[0].network_player_id = 17;
        players_[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        NETMOX::net = {};
        NETMOX::net.host_election_id = 17;
        NETMOX::net.net_host_id = 17;
        NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_PICK_POSITION;
    }

    int32_t Selected_Position_() {
        return selected_position_;
    }

    int32_t State_When_Position_Sent_() {
        return state_when_position_sent_;
    }

    int32_t State_When_Game_Data_Decoded_() {
        return state_when_game_data_decoded_;
    }

    void Cancel_Selection_() {
        selection_result_ = -1;
    }

    int32_t Messages_Sent_() {
        return messages_sent_;
    }

} // namespace TEST_NETSTART

namespace MULTPLAY {
    int16_t __cdecl Choose_Network_Plyrs_Screen(
        int16_t* selection, uint8_t*, int8_t, int8_t) {
        *selection = 0;
        return selection_result_;
    }

    void __cdecl Reload_Getting_Data_Info() {}
    void __cdecl Draw_SendGet_Net_Info_Screen() {}
} // namespace MULTPLAY

namespace fields {
    void __cdecl Clear_Fields() {}
    int16_t __cdecl Add_Hidden_Field(
        int16_t, int16_t, int16_t, int16_t, const char*, int8_t) {
        return 0;
    }
    void __cdecl Assign_Auto_Function(MOX2::action_func_t, int16_t) {}
    int16_t __cdecl Get_Input() {
        return 0;
    }
    void __cdecl Deactivate_Auto_Function() {}
} // namespace fields

namespace video {
    void __cdecl Set_Page_Off() {}
} // namespace video

namespace RUSS {
    void __cdecl Mox_Sync_Update() {}
} // namespace RUSS

namespace allocate {
    void __cdecl Pop_Block(void*) {}
} // namespace allocate

namespace MOX2 {
    void __cdecl Init_Map_Pictures_Default_() {}
    void __cdecl Init_Map_Defaults() {}
} // namespace MOX2

namespace MAPGEN {
    void __cdecl Init_Main_Screen(uint8_t) {}
} // namespace MAPGEN

namespace HAROLD {
    uint8_t* __cdecl Harold_Init() {
        return nullptr;
    }
} // namespace HAROLD

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {}
} // namespace MAINSCR

namespace MAINSCR2 {
    uint8_t main_screen_fade_in_flag;
}

namespace NETTURN {
    char* waiting_message_fmt = const_cast<char*>("");
}
