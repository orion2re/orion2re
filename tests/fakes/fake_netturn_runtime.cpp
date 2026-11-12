#include "pch.h"
#include <stdexcept>

#include "config/config_snapshot.h"
#include "game/allocate.h"
#include "game/chat.h"
#include "game/colcalc.h"
#include "game/combfind.h"
#include "game/eric.h"
#include "game/ericnet.h"
#include "game/fields.h"
#include "game/mainscr.h"
#include "game/mox.h"
#include "game/multplay.h"
#include "game/netcode.h"
#include "game/netmox.h"
#include "game/netstart.h"
#include "game/nextturn.h"
#include "game/report.h"
#include "game/russ.h"
#include "game/sound.h"

namespace CHAT {
    s_net_chat* chat_info = nullptr;
}

#ifdef TEST_REAL_MAIN_RECEIVE
namespace exitm {
    [[noreturn]] void __cdecl Exit_With_Message(const char* message) {
        throw std::runtime_error(message);
    }
    [[noreturn]] void __cdecl Exit_With_message2(const char* message) {
        Exit_With_Message(message);
    }
}
#endif

namespace MOX {
    int16_t return_screen = 0;
    int8_t previous_screen = 0;
    int16_t current_screen = 0;
    s_animation_header* screen_seg = nullptr;
    int8_t game_type = 0;
    char temp_string[512] = {};
    int16_t PLAYER_NUM = 0;
    int16_t combat_turn = 0;
    s_player* player = nullptr;
    int16_t NUM_PLAYERS = 0;
    int16_t NUM_STARS = 0;
    s_star_data* star = nullptr;
    int32_t stardate = 0;
    s_settings settings = {};
    s_antaran antarans = {};
    s_nebula* nebula = nullptr;
    uint8_t NUM_NEBULAS = 0;
    int16_t NUM_COLONIES = 0;
    int16_t planet_count = 0;
    s_planet_data* planet = nullptr;
    int16_t NUM_SHIPS = 0;
    s_ship_data* ship = nullptr;
    s_leader_data* leaders = nullptr;
    s_colony* colony = nullptr;
    int16_t bill_savegame[6] = {};
} // namespace MOX

namespace NETSTART {
    void __cdecl Mox_Net_Uninit() {}
} // namespace NETSTART

namespace CMBTFIR2 {
    void __cdecl Save_Artemis_Random_Seed() {}
} // namespace CMBTFIR2

namespace MULTPLAY {
    void (*g_chat_box_input_loop_hook)() = nullptr;

    void __cdecl Load_Net_Next_Turn_Screen(int8_t) {}
    void __cdecl Draw_Net_Next_Turn_Screen() {}
    void __cdecl Add_Net_Next_Turn_Fields() {}
    void __cdecl Chat_Box_Input_Loop() {
        if (g_chat_box_input_loop_hook != nullptr) {
            g_chat_box_input_loop_hook();
        }
    }
} // namespace MULTPLAY

namespace fields {
    void __cdecl Assign_Auto_Function(MOX2::action_func_t, int16_t) {}
} // namespace fields

namespace ERICNET {
    int __cdecl Mox_Play_Sound(int16_t) {
        return 0;
    }
} // namespace ERICNET

namespace ERIC {
    void __cdecl Play_Loose_Flic() {}
} // namespace ERIC

namespace RUSS {
    void __cdecl Mox_Update() {
        sound::Sound_Update();
        NETMOX::Mox_Net_Update();
    }
    void __cdecl Make_Player_Into_AI(int) {}
} // namespace RUSS

namespace TEST_NETMOX_UI {
    int ship_icon_refresh_count;
    int32_t last_refresh_sync_state;
}

namespace MAINSCR {
#ifndef TEST_REAL_MAIN_RECEIVE
    void __cdecl Main_Receive_Message(
        uint32_t, int16_t, uint16_t, uint32_t, void*) {}

    void __cdecl Update_Ship_Icons_In_Between_Turns() {
        ++TEST_NETMOX_UI::ship_icon_refresh_count;
        TEST_NETMOX_UI::last_refresh_sync_state = NETMOX::net.sync_state;
    }
#endif
} // namespace MAINSCR

namespace COLCALC {
    void __cdecl No_Op1() {}
    void __cdecl Compute_Star_Colony_Stuff(s_star_data*) {}
} // namespace COLCALC

namespace NEXTTURN {
    void __cdecl Next_Turn() {}
} // namespace NEXTTURN

namespace COMBFIND {
    s_combfind_star_has_player* current_star_has_player = nullptr;
    uint8_t* current_star_has_nonplayer = nullptr;
    s_combfind_star_has_player (*star_has_player)[MAX_STARS + 1][MAX_PLAYERS] = nullptr;
    uint8_t (*star_has_nonplayer)[MAX_STARS + 1] = nullptr;

    void __cdecl Client_Combat_Loop() {}
    void __cdecl Do_Net_Combat_Update(bool) {}
    void __cdecl Copy_Target_Info(s_combfind_target*) {}
    int16_t __cdecl Target_Colony_To_Planet(int16_t colony_idx) {
        return colony_idx;
    }
    void __cdecl Decode_Star_Player_Presence(NETPROTO::Packet_Reader_*, int16_t) {}
    void __cdecl Send_Star_Player_Presence(NETPROTO::Packet_Writer_*, int16_t) {}
} // namespace COMBFIND

namespace EVENTS {
    s_event_data event_data[36] = {};
    uint32_t last_event_year = 0;
    int16_t event_delay_count = 0;
    void __cdecl Event_Monster_Update(int16_t, int16_t, int16_t) {}
} // namespace EVENTS

namespace REPORT {
    int16_t n_reports = 0;
    s_report reports[MAX_REPORTS] = {};
    uint8_t something_interesting_happened[MAX_PLAYERS] = {};

    void __cdecl Client_Initialize_Reports() {}
} // namespace REPORT

namespace MSG_ {
    uint8_t first_msg[MAX_PLAYERS] = {};
    uint8_t n_msgs[MAX_PLAYERS] = {};
    s_MSG msgs[MAX_MESSAGES] = {};
    uint8_t max_msgs[MAX_PLAYERS] = {};
} // namespace MSG_

namespace diplodef {
    int16_t COUNCIL_FLAG = 0;
    int16_t next_council_meeting_turn = 0;
} // namespace diplodef

namespace diplomac {
    void __cdecl Change_Relations(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t) {}
} // namespace diplomac

namespace GENDRAW {
    void __cdecl Message_Box(char const*) {}
} // namespace GENDRAW

namespace DIP_SCRN {
    void __cdecl Diplomacy_Receive_Message(uint32_t, uint16_t, uint32_t, s_dipscrn_net_message*) {}
    void __cdecl Diplomacy_Screen(int32_t) {}
} // namespace DIP_SCRN

namespace CMBTMOV1 {
    void __cdecl Tactical_Combat_Receive_Message(int, uint16_t, int, s_net_tactical_combat_message*) {}
} // namespace CMBTMOV1

namespace netcode {
    void (*g_net_update_hook)() = nullptr;
    void (*g_net_send_hook)(int32_t, int16_t, void*, uint32_t) = nullptr;

    uint32_t __cdecl Net_Send_Message(int32_t recipient, int16_t packet_id, void* packet, uint32_t packet_len) {
        if (g_net_send_hook != nullptr) {
            g_net_send_hook(recipient, packet_id, packet, packet_len);
        }
        return 1;
    }
    void __cdecl Net_Update() {
        if (g_net_update_hook != nullptr) {
            g_net_update_hook();
        }
    }
    int32_t __cdecl Net_Is_Sending() {
        return 0;
    }
    void __cdecl Net_Accept_Member(int32_t) {}
} // namespace netcode

namespace sound {
    void __cdecl Sound_Update() {}
} // namespace sound

namespace SHIPS {
    void __cdecl Kill_Ship(int16_t) {}
    void __cdecl Build_Ship_Icons() {}
    void __cdecl Set_Ship_Icon_XYs() {}
} // namespace SHIPS

namespace HAROLD {
    int16_t __cdecl Star_Owner(int16_t, int16_t) {
        return -1;
    }

    bool __cdecl Star_Is_Outpost_Star(int16_t) {
        return false;
    }
} // namespace HAROLD

namespace CHAT {
    void __cdecl Receive_Chat_Msg(uint8_t, const char*) {}
} // namespace CHAT

namespace RACESEL {
    uint8_t race_flag[14] = {};
    uint8_t color_flag[MAX_PLAYERS] = {};
} // namespace RACESEL

namespace diplomac {
    void __cdecl Declare_War(int16_t, int16_t) {}
} // namespace diplomac

namespace BOMB {
    void __cdecl Destroy_Colony(s_colony*, int32_t) {}
} // namespace BOMB

namespace RUSS {
    int16_t __cdecl Get_Weighted_Choice_Char(uint8_t*, int16_t) {
        return 0;
    }
} // namespace RUSS

namespace config {
    const s_config_snapshot_descriptor synchronized_config_snapshot = {};

    bool Apply_Config_Snapshot(
        SERIAL::Reader_*, const s_config_snapshot_descriptor*) {
        return false;
    }
} // namespace config

namespace netcode {
    uint32_t __cdecl Net_Session_Owner_ID() {
        return UINT32_MAX;
    }
} // namespace netcode