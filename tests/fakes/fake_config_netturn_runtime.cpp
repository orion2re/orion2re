#include "pch.h"

#include <stdexcept>

#include "game/chat.h"
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

namespace TEST_CONFIG_NETTURN {
    void (*chat_box_input_loop_hook)();
    void (*next_turn_hook)();
    int next_turn_calls;

    void Reset_() {
        chat_box_input_loop_hook = nullptr;
        next_turn_hook = nullptr;
        next_turn_calls = 0;
    }
} // namespace TEST_CONFIG_NETTURN

namespace CHAT {
    s_net_chat* chat_info;
} // namespace CHAT

namespace MOX {
    int16_t return_screen;
    int8_t previous_screen;
    int16_t current_screen;
    s_animation_header* screen_seg;
    int8_t game_type;
    char temp_string[512];
    game::id_player local_player{0};

    int32_t stardate;
    uint16_t FULL_DRAW_SCREEN_FLAG;
    s_save_game_description save_game_description[10];
} // namespace MOX

namespace NETMOX {
    s_network net;

    void __cdecl Net_Debug_Log(const char*, ...) {}
    void __cdecl Clear_Leader_Deassignments() {}
    int32_t __cdecl Mox_Send_Message(
        int16_t, uint16_t, void*, uint32_t) {
        return 1;
    }
    bool __cdecl Decode_Game_Orders(const void*, uint32_t, game::id_player) { return true; }
    void __cdecl Broadcast_Game_Data_Differential(uint8_t) {}
    int8_t __cdecl Decode_Broadcast_Game_Data_Differential(
        void*, size_t, uint8_t) {
        return 1;
    }
    void __cdecl Send_Game_Orders() {}
    void __cdecl Human_Surrenders(game::id_player) {}
} // namespace NETMOX

namespace FILEDEF {
    bool Write_Autosave(const void*, size_t) { return true; }
}

namespace SAVEGAME {
    bool Write_Game(SERIAL::Writer_* writer, const char*, uint32_t, std::string*) {
        writer->Write_Uint8(0);
        return true;
    }
}

namespace game_random {
    uint32_t __cdecl Get_Random_Seed() { return 0; }
}

namespace platform {
    int Show_Yes_No_Message(const char*, const char*) { return 1; }
}

namespace GENDRAW {
    int16_t __cdecl Confirmation_Box(const char*) { return 1; }
}

namespace CMBTFIR2 {
    void __cdecl Save_Artemis_Random_Seed() {}
} // namespace CMBTFIR2

namespace MULTPLAY {
    void __cdecl Load_Net_Next_Turn_Screen(int8_t) {}
    void __cdecl Draw_Net_Next_Turn_Screen() {}
    void __cdecl Add_Net_Next_Turn_Fields() {}
    void __cdecl Chat_Box_Input_Loop() {
        if (TEST_CONFIG_NETTURN::chat_box_input_loop_hook != nullptr) {
            TEST_CONFIG_NETTURN::chat_box_input_loop_hook();
        }
    }
} // namespace MULTPLAY

namespace fields {
    void __cdecl Assign_Auto_Function(MOX2::action_func_t, int16_t) {}
    void __cdecl Deactivate_Auto_Function() {}
} // namespace fields

namespace NEXTTURN {
    void __cdecl Next_Turn() {
        ++TEST_CONFIG_NETTURN::next_turn_calls;
        if (TEST_CONFIG_NETTURN::next_turn_hook != nullptr) {
            TEST_CONFIG_NETTURN::next_turn_hook();
        }
    }
} // namespace NEXTTURN

namespace NETSTART {
    void __cdecl Mox_Net_Uninit() {}
} // namespace NETSTART

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {}
} // namespace MAINSCR

namespace COLCALC {
    void __cdecl No_Op1() {}
} // namespace COLCALC

namespace COMBFIND {
    s_combfind_star_has_player* current_star_has_player;
    uint8_t* current_star_has_nonplayer;

    void __cdecl Client_Combat_Loop() {}
} // namespace COMBFIND

namespace ERICNET {
    int __cdecl Mox_Play_Sound(int16_t) {
        return 0;
    }
} // namespace ERICNET

namespace ERIC {
    void __cdecl Play_Loose_Flic() {}
} // namespace ERIC

namespace RUSS {
    void __cdecl Mox_Update() {}
} // namespace RUSS

namespace REPORT {
    uint8_t something_interesting_happened[MAX_PLAYERS];
} // namespace REPORT

namespace allocate {
    void __cdecl Pop_Block(void*) {}
} // namespace allocate

namespace netcode {
    void Net_End_Game_After_Save(game::id_player) {}
    int32_t __cdecl Net_Is_Sending() {
        return 0;
    }
} // namespace netcode

namespace exitm {
    [[noreturn]] void __cdecl Exit_With_Message(const char* message) {
        throw std::runtime_error(message != nullptr ? message : "");
    }
} // namespace exitm
