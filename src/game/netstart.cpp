#include "pch.h"

#include "config/config_loader.h"
#include "config/config_snapshot.h"
#include "game/netstart_roster.h"
#include "game/serialization.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_network.h"

namespace NETSTART {
    char* dialing_failed_error;
    char* really_begin_game_with_unused_slots_msg;
    char* really_start_with_only_1_msg;
    char* no_more_player_slots_error;
    char* game_full_string;
    char* answering_failed_error;
    char* windows_string;

    char s_myname_005597dc[7] = "myname";

    namespace {
        bool Network_Lua_Is_Available() {
            std::string diagnostic;
            if (orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
                    orion2re::lua::Get_Active_Mod_Runtimes(),
                    &diagnostic)) {
                return true;
            }
            GENDRAW::Message_Box(diagnostic.c_str());
            MOX::current_screen = SCREEN_MULTI_PLAYER;
            return false;
        }

        const char* Network_Initialization_Error() {
            const char* diagnostic = netcode::Net_Initialization_Error();
            return diagnostic[0] != '\0' ? diagnostic : NETSTRAT::comm_failure_string;
        }

        bool Dispatch_Initialized() {
            std::string diagnostic;
            if (orion2re::lua::Dispatch_Active_Initialized_Event(
                    &diagnostic)) {
                return true;
            }
            GENDRAW::Message_Box(diagnostic.c_str());
            return false;
        }

        void Send_Configured_Setup_Message(uint16_t packet_id,
            const void* prefix,
            uint32_t prefix_size) {
            std::string lua_diagnostic;
            if (!orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
                    orion2re::lua::Get_Active_Mod_Runtimes(),
                    &lua_diagnostic)) {
                exitm::Exit_With_Message(lua_diagnostic.c_str());
            }
            const config::s_active_config* active_config =
                config::Get_Active_Config();
            if (active_config == nullptr) {
                exitm::Exit_With_Message("network configuration is not initialized");
            }
            const config::s_prepared_config_snapshot* snapshot = nullptr;
            config::s_cfg_diagnostic config_diagnostic;
            if (!config::Get_Prepared_Active_Config_Snapshot(
                    &config::synchronized_config_snapshot,
                    &snapshot,
                    &config_diagnostic)) {
                exitm::Exit_With_Message("unable to read network configuration");
            }
            const uint32_t snapshot_size =
                static_cast<uint32_t>(snapshot->bytes.size());
            uint8_t commit[16] = {};
            uint32_t commit_size = 0;
            if (!orion2re::lua::Write_Lua_Network_Setup_Commit(
                    commit, sizeof(commit), &commit_size)) {
                exitm::Exit_With_Message(
                    "Lua network setup commit is unavailable");
            }
            if (snapshot_size == 0 ||
                prefix_size > UINT32_MAX - commit_size ||
                prefix_size + commit_size > UINT32_MAX - snapshot_size) {
                exitm::Exit_With_Message("network configuration packet too large");
            }
            const uint32_t packet_size =
                prefix_size + commit_size + snapshot_size;
            allocate::Push_Block(MOX::screen_seg);
            void* packet = allocate::Allocate_Next_Block(
                MOX::screen_seg, packet_size);
            SERIAL::Writer_ writer(
                packet, packet_size, "network configuration packet overflow");
            writer.Write_Bytes(prefix, prefix_size);
            if (commit_size != 0) {
                writer.Write_Bytes(commit, commit_size);
            }
            config::Write_Prepared_Config_Snapshot(&writer, snapshot);
            NETMOX::Mox_Send_Message(
                (int16_t)netadapter::NETADAPTER_BROADCAST_EXCLUDING_SELF,
                packet_id,
                packet,
                writer.Length());
            allocate::Pop_Block(MOX::screen_seg);
        }

        void Ensure_Net_Game_Name() {
            if (NETMOX::net.cur_game_name[0] != '\0') {
                return;
            }

            if (MOX::settings.net_game_name[0] == '\0') {
                memcpy(MOX::settings.net_game_name, "Moo II ", sizeof("Moo II "));
                MOX::settings.net_game_name[6] = (char)('a' + ((int32_t)clock() % 26));
            }

            strlcpy(NETMOX::net.cur_game_name, MOX::settings.net_game_name, sizeof(NETMOX::net.cur_game_name));
        }
    } // namespace

    int8_t __cdecl Net_Select_Race() {
        uint8_t fade_flag;
        s_player* local_player = &MOX::player[MOX::PLAYER_NUM];

        RACESEL::Race_Selection_Screen(local_player);

        local_player->objectives = PLAYER_OBJECTIVE_HUMAN;
        MULTPLAY::Reload_Generating_Map_Info();

        MISC::Fade_Into_Screen(MULTPLAY::Draw_Generic_Net_Info_Screen, &fade_flag, 11);
        MULTPLAY::Draw_Generic_Net_Info_Screen();
        RUSS::Mox_Sync_Update();

        NETMOX::net.network_data_ptr = nullptr;
        NETMOX::net.sync_state = NET_SYNC_STATE_WAITING_FOR_GAME_DATA;

        NETMOX::Mox_Send_Message(NETMOX::net.host_election_id, NET_PACKET_PLAYER_SETUP, local_player, sizeof(*local_player));

        while (NETMOX::net.network_data_ptr == nullptr) {
            fields::Get_Input();
            MULTPLAY::Draw_Generic_Net_Info_Screen();
            RUSS::Mox_Sync_Update();
        }

        NETMOX::Decode_Broadcast_Game_Data_Differential(NETMOX::net.network_data_ptr, NETMOX::net.total_bytes_received, 1);
        memset(NETMOX::net.player_phase_ready_sent_flags, 0, sizeof(NETMOX::net.player_phase_ready_sent_flags));
        NETMOX::net.network_data_ptr = nullptr;
        allocate::Pop_Block(MOX::screen_seg);
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

    char* __cdecl Load_Netstart_Strings(char* buffer) {
        char* current = buffer;

        windows_string = current;
        current += strlen(current) + 1;

        NETSTRAT::comm_failure_string = current;
        current += strlen(current) + 1;

        game_full_string = current;
        current += strlen(current) + 1;

        current += strlen(current) + 1;

        answering_failed_error = current;
        current += strlen(current) + 1;

        no_more_player_slots_error = current;
        current += strlen(current) + 1;

        dialing_failed_error = current;
        current += strlen(current) + 1;

        really_begin_game_with_unused_slots_msg = current;
        current += strlen(current) + 1;

        really_start_with_only_1_msg = current;
        current += strlen(current) + 1;

        return current;
    }

    uint8_t __cdecl Join_Net_Screen() {
        if (!Network_Lua_Is_Available()) {
            return 0;
        }
        uint8_t result = 0;
        uint8_t fade_status;
        const char* myname_str = s_myname_005597dc;

        NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_JOINING;

        NETTURN::Net_Init_Host_Election();
        MULTPLAY::Reload_Initializing_Net_Info();
        fields::Assign_Auto_Function(MULTPLAY::Draw_Generic_Net_Info_Screen, 1);
        MISC::Fade_Into_Screen(MULTPLAY::Draw_Generic_Net_Info_Screen, &fade_status, 11);
        MULTPLAY::Draw_Generic_Net_Info_Screen();
        RUSS::Mox_Sync_Update();
        RUSS::Mox_Update();

        if (!netcode::Do_Net_Init(NETMOX::net.mode, NETMOX::Receive_Message, &NETMOX::net)) {
            RUSS::Mox_Update();
            GENDRAW::Message_Box(Network_Initialization_Error());
            MOX::current_screen = SCREEN_MULTI_PLAYER;
            NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
            return 0;
        }

        if (NETMOX::net.mode == 0 || NETMOX::net.mode == 1) {
            MULTPLAY::Choose_Multi_Network_Game_Screen();
            if (NETMOX::net.cur_session_id == 0) {
                MOX::current_screen = SCREEN_MULTI_PLAYER;
                NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
                return 0;
            }

            if (!netcode::Net_Join_Game(NETMOX::net.cur_session_id, "", myname_str)) {
                MULTPLAY::Reload_Initializing_Net_Info();
                const char* diagnostic = netcode::Net_Join_Error();
                GENDRAW::Message_Box(diagnostic[0] != '\0'
                        ? diagnostic
                        : NETSTART::game_full_string);
                NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
                MOX::current_screen = SCREEN_MULTI_PLAYER;
                NETSTART::Mox_Net_Uninit();
                return 0;
            }
        } else {
            GENDRAW::Message_Box(NETSTRAT::comm_failure_string);
            MOX::current_screen = SCREEN_MULTI_PLAYER;
            NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
            NETSTART::Mox_Net_Uninit();
            return 0;
        }

        NETMOX::net.is_network_active = 1;
        NETMOX::net.net_host_id = netcode::Net_Host_ID();
        MULTPLAY::Reload_Join_Net_Screen();
        fields::Clear_Fields();
        fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);
        fields::Assign_Auto_Function(MULTPLAY::Draw_Generic_Net_Info_Screen, 2);

        memset(RACESEL::color_flag, 0, sizeof(RACESEL::color_flag));
        memset(RACESEL::race_flag, 0, sizeof(RACESEL::race_flag));

        while (NETMOX::net.sync_state == NET_SYNC_STATE_CLIENT_JOINING) {
            fields::Get_Input();
            MULTPLAY::Draw_Generic_Net_Info_Screen();
            RUSS::Mox_Sync_Update();
        }

        if (NETMOX::net.sync_state == NET_SYNC_STATE_CLIENT_NO_AVAILABLE_SLOT) {
            NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
            MOX::current_screen = SCREEN_MULTI_PLAYER;
            NETSTART::Mox_Net_Uninit();
            GENDRAW::Message_Box(NETSTART::no_more_player_slots_error);
            return 0;
        }

        if (NETMOX::net.sync_state == NET_SYNC_STATE_CLIENT_SELECT_RACE) {
            NETMOX::Mox_Send_Message(NETMOX::net.host_election_id, NET_PACKET_NETWORK_SYNC_STATUS, &MOX::nebula_screen_seg, sizeof(MOX::nebula_screen_seg));
            result = (uint8_t)NETSTART::Net_Select_Race();
        } else if (NETMOX::net.sync_state == NET_SYNC_STATE_CLIENT_PICK_POSITION) {
            NETMOX::Mox_Send_Message(NETMOX::net.host_election_id, NET_PACKET_NETWORK_SYNC_STATUS, &MOX::nebula_screen_seg, sizeof(MOX::nebula_screen_seg));
            result = (uint8_t)NETSTART::Net_Pick_Position();
            if (!result) {
                NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
                MOX::current_screen = SCREEN_MULTI_PLAYER;
                NETSTART::Mox_Net_Uninit();
            }
        }

        return result;
    }

    uint8_t __cdecl Start_Net_Screen() {
        if (!Network_Lua_Is_Available()) {
            return 0;
        }
        NETMOX::net.sync_state = NET_SYNC_STATE_HOST_STARTUP;
        NETTURN::Net_Init_Host_Election();
        MULTPLAY::Reload_Initializing_Net_Info();
        fields::Assign_Auto_Function(MULTPLAY::Draw_Generic_Net_Info_Screen, 1);

        uint8_t unused;
        MISC::Fade_Into_Screen(MULTPLAY::Draw_Generic_Net_Info_Screen, &unused, 11);
        MULTPLAY::Draw_Generic_Net_Info_Screen();
        RUSS::Mox_Sync_Update();

        if (netcode::Do_Net_Init(NETMOX::net.mode, NETMOX::Receive_Message, &NETMOX::net) == 0) {
            NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
            GENDRAW::Message_Box(Network_Initialization_Error());
            MOX::current_screen = SCREEN_MULTI_PLAYER;
            return 0;
        }

        if (NETMOX::net.mode == 0 || NETMOX::net.mode == 1) {
            if (Russ_Net_Create_Game(NETMOX::net.cur_game_name, MAX_PLAYERS, "", s_myname_005597dc) == 0) {
                GENDRAW::Message_Box(NETSTRAT::comm_failure_string);
                NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
                MOX::current_screen = SCREEN_MULTI_PLAYER;
                Mox_Net_Uninit();
                return 0;
            }
        }

        NETMOX::net.is_network_active = 1;
        NETMOX::net.net_host_id = netcode::Net_Host_ID();
        NETMOX::net.host_election_id = NETMOX::net.net_host_id;

        if (MOX::nebula_screen_seg == 0) {
            NETMOX::net.network_sync_complete_flag = 0;
        } else {
            NETMOX::net.network_sync_complete_flag = 1;
        }

        if (NETMOX::net.mode == 0 || NETMOX::net.mode == 1) {
            MULTPLAY::Reload_Waiting_For_Joiners_Screen();
            fields::Clear_Fields();
            MULTPLAY::Add_Waiting_For_Joiners_Field();
            fields::Assign_Auto_Function(MULTPLAY::Draw_Wait_For_New_Joiners_Screen, 2);
            MULTPLAY::Draw_Wait_For_New_Joiners_Screen();
            RUSS::Mox_Sync_Update();

            while (true) {
                int32_t input = fields::Get_Input();
                MULTPLAY::Draw_Wait_For_New_Joiners_Screen();
                RUSS::Mox_Sync_Update();

                if (input != 0) {
                    if (netcode::Net_Get_Users(nullptr) == 1) {
                        input = GENDRAW::Confirmation_Box(really_start_with_only_1_msg);
                    }
                    if (input != 0) {
                        break;
                    }
                }

                if (netcode::Net_Get_Users(nullptr) >= MOX::settings.number_of_players) {
                    break;
                }
            }

            netcode::Net_Disable_New_Players();
        }

        int16_t users[256];
        NETMOX::net.num_connected_players = static_cast<int8_t>(netcode::Net_Get_Users(users));

        if (NETMOX::net.num_connected_players > MOX::settings.number_of_players) {
            NETMOX::net.num_connected_players = MOX::settings.number_of_players;
        }

        Dump_Users();

        s_net_pack_start_game msg;
        NETMOX::net.sync_state = NET_SYNC_STATE_HOST_ASSIGN_PLAYERS;
        MOX::PLAYER_NUM = Assign_New_Game_Network_Roster_(MOX::player,
            &msg,
            users,
            NETMOX::net.num_connected_players,
            NETMOX::net.net_host_id);

        Dump_Users();

        MOX::NUM_PLAYERS = MOX::settings.number_of_players;

        msg.net_host_id = NETMOX::net.net_host_id;
        msg.num_connected_players = NETMOX::net.num_connected_players;
        msg.random_events = MOX::settings.random_events;
        msg.game_difficulty = MOX::settings.game_difficulty;
        msg.number_of_players = MOX::settings.number_of_players;
        msg.galaxy_size = MOX::settings.galaxy_size;
        msg.galaxy_age = MOX::settings.galaxy_age;
        msg.strategic_combat_flag = MOX::settings.strategic_combat_flag;
        msg.starting_civilization_level = MOX::settings.starting_civilization_level;
        msg.xenons_exist = MOX::settings.xenons_exist;

        strlcpy(msg.version_string, GAME_BUILD_DATE, sizeof(msg.version_string));

        memset(RACESEL::color_flag, 0, sizeof(RACESEL::color_flag));
        memset(NETMOX::net.player_connected_flags, 0, sizeof(NETMOX::net.player_connected_flags));
        memset(RACESEL::race_flag, 0, sizeof(RACESEL::race_flag));
        memset(NETMOX::net.player_color_selected_flags, 0, sizeof(NETMOX::net.player_color_selected_flags));

        NETMOX::net.sync_state = NET_SYNC_STATE_HOST_WAITING_FOR_PLAYER_SETUP;

        if (MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < MOX::NUM_PLAYERS; i++) {
                MOX::player[i].color = 8;
            }
        }

        RACESEL::Race_Selection_Screen(&MOX::player[MOX::PLAYER_NUM]);

        NETMOX::net.player_color_selected_flags[MOX::PLAYER_NUM] = 1;
        MOX::player[MOX::PLAYER_NUM].objectives = PLAYER_OBJECTIVE_HUMAN;

        RUSS::Mox_Update();

        Send_Configured_Setup_Message(NET_PACKET_START_GAME, &msg, sizeof(msg));

        MULTPLAY::Reload_Wait_For_Race_Info();
        MISC::Fade_Into_Screen(MULTPLAY::Draw_Generic_Net_Info_Screen, &unused, 11);

        NETMOX::net.player_connected_flags[MOX::PLAYER_NUM] = 1;

        if (NETMOX::net.num_connected_players != 0) {
            while (true) {
                MULTPLAY::Draw_Wait_For_Race_Info_Screen();

                int32_t ready_count = 0;
                if (MOX::NUM_PLAYERS > 0) {
                    for (int i = 0; i < MOX::NUM_PLAYERS; i++) {
                        if (NETMOX::net.player_connected_flags[i] != 0) {
                            ready_count++;
                        }
                    }
                }

                fields::Get_Input();
                RUSS::Mox_Sync_Update();

                if (NETMOX::net.num_connected_players <= ready_count) {
                    break;
                }
            }
        }

        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();

        if (!INITGAME::Init_New_Game(
                (int16_t)MOX::settings.game_difficulty,
                (int16_t)MOX::settings.galaxy_size,
                (int16_t)MOX::settings.galaxy_age,
                NEWGAME::Player_Count_For_Initialization(),
                (int16_t)MOX::settings.strategic_combat_flag,
                (int16_t)MOX::settings.starting_civilization_level)) {
            NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
            MOX::current_screen = SCREEN_MULTI_PLAYER;
            Mox_Net_Uninit();
            return 0;
        }
        if (MOX::NUM_PLAYERS > NETMOX::net.num_connected_players) {
            for (int i = NETMOX::net.num_connected_players; i < MOX::NUM_PLAYERS; i++) {
                MOX::player[i].network_player_id = -1;
            }
        }

        uint8_t color_used[8] = {0};

        if (MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < MOX::NUM_PLAYERS; i++) {
                if (MOX::player[i].color != 8) {
                    color_used[MOX::player[i].color] = 1;
                }
            }
        }

        if (MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < MOX::NUM_PLAYERS; i++) {
                if (MOX::player[i].color == 8) {
                    int8_t selected_color = -1;
                    int32_t num_available = 0;

                    for (int j = 0; j < 8; j++) {
                        if (color_used[j] == 0) {
                            num_available++;
                            if (game_random::Random(num_available) == 1) {
                                selected_color = static_cast<int8_t>(j);
                            }
                        }
                    }

                    MOX::player[i].color = selected_color;
                    color_used[selected_color] = 1;
                }
            }
        }

        MULTPLAY::Reload_Sending_Data_Info();
        video::Set_Page_Off();
        MULTPLAY::Draw_SendGet_Net_Info_Screen();
        fields::Assign_Auto_Function(MULTPLAY::Draw_SendGet_Net_Info_Screen, 2);
        RUSS::Mox_Sync_Update();

        if (NETMOX::net.network_sync_complete_flag == 0) {
            MOX::NUM_NEBULAS = 0;
        }

        if (MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < MOX::NUM_PLAYERS; i++) {
                NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_READY;
                NETMOX::net.player_net_info[i].diplomacy_target_id = -1;
            }
        }

        NETMOX::net.host_election_winner_id = NETMOX::net.net_host_id;

        if (orion2re::lua::Network_Lua_Enabled() &&
            !Dispatch_Initialized()) {
            Mox_Net_Uninit();
            return 0;
        }
        NETMOX::Broadcast_Game_Data_Differential(1);

        NETMOX::net.player_race_selected_flags[MOX::PLAYER_NUM] = 1;
        memset(NETMOX::net.player_phase_ready_sent_flags, 0, sizeof(NETMOX::net.player_phase_ready_sent_flags));
        NETMOX::net.sync_state = NET_SYNC_STATE_READY;

        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();

        MAPGEN::Init_Main_Screen(MOX::settings.galaxy_size);
        HAROLD::Harold_Init();
        MOX2::Init_Map_Defaults();

        MAINSCR2::main_screen_fade_in_flag = 1;
        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        return 1;
    }

    void __cdecl Mox_Net_Uninit() {
        orion2re::lua::Reset_Lua_Network_Session();
        netcode::Net_Uninit();
        NETMOX::Clear_Leader_Deassignments();

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            MOX::player[i].network_player_id = -1;
        }

        NETMOX::net.is_network_active = 0;
    }

    int8_t __cdecl Request_Race(uint8_t race_idx) {
        while (NETMOX::net.race_selection_changed != 0) {
            RUSS::Mox_Update();
        }

        MOX::player[MOX::PLAYER_NUM].race = 14;

        if (RACESEL::race_flag[race_idx] != 0) {
            return 0;
        }

        NETMOX::Mox_Send_Message(NETMOX::net.host_election_id, NET_PACKET_REQUEST_PLAYER_RACE, &race_idx, sizeof(race_idx));

        while (RACESEL::race_flag[race_idx] == 0) {
            RUSS::Mox_Update();
        }

        return MOX::player[MOX::PLAYER_NUM].race == race_idx;
    }

    uint8_t __cdecl Load_Net_Screen() {
        if (!Network_Lua_Is_Available()) {
            return 0;
        }
        uint8_t fade_flag = 0;
        uint8_t status[MAX_PLAYERS] = {0};
        s_load_net_screen_packet payload = {};
        int16_t user_list[256];

        NETMOX::net.sync_state = NET_SYNC_STATE_LOAD_GAME_HOST_SETUP;
        NETTURN::Net_Init_Host_Election();
        MULTPLAY::Reload_Initializing_Net_Info();

        fields::Assign_Auto_Function(MULTPLAY::Draw_Generic_Net_Info_Screen, 1);
        MISC::Fade_Into_Screen(MULTPLAY::Draw_Generic_Net_Info_Screen, &fade_flag, 11);
        MULTPLAY::Draw_Generic_Net_Info_Screen();
        RUSS::Mox_Sync_Update();

        if (netcode::Do_Net_Init(NETMOX::net.mode, NETMOX::Receive_Message, &NETMOX::net) == 0) {
            GENDRAW::Message_Box(Network_Initialization_Error());
            MOX::current_screen = SCREEN_MULTI_PLAYER;
            NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
            return 0;
        }

        if (NETMOX::net.mode == 0 || NETMOX::net.mode == 1) {
            if (Russ_Net_Create_Game(NETMOX::net.cur_game_name, MAX_PLAYERS, "", s_myname_005597dc) == 0) {
                GENDRAW::Message_Box(NETSTRAT::comm_failure_string);
                NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
                MOX::current_screen = SCREEN_MULTI_PLAYER;
                Mox_Net_Uninit();
                return 0;
            }
        }

        fields::Clear_Fields();
        fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);

        fields::Assign_Auto_Function(SCORE::nullsub, 2);

        NETMOX::net.is_network_active = 1;
        NETMOX::net.net_host_id = netcode::Net_Host_ID();
        NETMOX::net.host_election_id = NETMOX::net.net_host_id;

        if (MOX::nebula_screen_seg != 0) {
            NETMOX::net.network_sync_complete_flag = 1;
        } else {
            NETMOX::net.network_sync_complete_flag = 0;
        }

        int16_t choose_result = MULTPLAY::Choose_Network_Plyrs_Screen(&payload.begin_state, status, 0, 1);

        fields::Clear_Fields();
        fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);

        if (choose_result == -1) {
            NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
            MOX::current_screen = SCREEN_MULTI_PLAYER;
            Mox_Net_Uninit();
            return 0;
        }

        int16_t player_0 = payload.begin_state;
        status[player_0] = 1;
        MOX::player[player_0].network_player_id = NETMOX::net.net_host_id;
        NETMOX::net.joining_net_id = -1;
        MOX::PLAYER_NUM = player_0;

        payload.begin_state = 0;

        fields::Assign_Auto_Function(MULTPLAY::Draw_Wait_For_New_Joiners_Screen, 2);

        MULTPLAY::Reload_Waiting_For_Joiners_Screen();
        fields::Clear_Fields();
        MULTPLAY::Add_Waiting_For_Joiners_Field();

        NETMOX::net.num_connected_players = 0;
        if (MOX::NUM_PLAYERS > 0) {
            for (int32_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    NETMOX::net.num_connected_players++;
                }
            }
        }

        int32_t connected_count = 1;

        while (true) {
            if (NETMOX::net.joining_net_id == -1) {
                int32_t num_users = netcode::Net_Get_Users(user_list);
                if (num_users > 0) {
                    for (int32_t i = num_users - 1; i >= 0; i--) {
                        if (NETMOX::net.joining_net_id != -1) {
                            break;
                        }
                        if (NETMOX::Net_Id_To_Player_Id(user_list[i]) == -1) {
                            connected_count++;
                            NETMOX::net.joining_net_id = user_list[i];
                            NETMOX::Send_Pick_Your_Position(user_list[i]);
                        }
                    }
                }
            }

            MULTPLAY::Draw_Wait_For_New_Joiners_Screen();
            RUSS::Mox_Sync_Update();

            if (NETMOX::net.joining_net_id == -1) {
                if (NETMOX::net.num_connected_players == connected_count ||
                    (connected_count == 2 && NETMOX::net.mode == 1)) {
                    payload.begin_state = 1;
                } else {
                    if (netcode::Net_Get_Users(nullptr) == connected_count) {
                        if (fields::Get_Input() == MOX::mp_screen->field_id_begin) {
                            if (NETMOX::net.num_connected_players > connected_count) {
                                payload.begin_state = (int16_t)(uint8_t)GENDRAW::Confirmation_Box(really_begin_game_with_unused_slots_msg);
                            } else {
                                payload.begin_state = 1;
                            }
                        }
                    }
                }
            }

            RUSS::Mox_Update();
            MULTPLAY::Draw_Wait_For_New_Joiners_Screen();
            RUSS::Mox_Sync_Update();

            if (payload.begin_state != 0) {
                break;
            }
        }

        if (NETMOX::net.mode == 0 || NETMOX::net.mode == 1) {
            netcode::Net_Disable_New_Players();
        }

        fields::Assign_Auto_Function(SCORE::nullsub, 2);

        NETMOX::net.num_connected_players = 0;
        if (MOX::NUM_PLAYERS > 0) {
            for (int32_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                if (MOX::player[i].network_player_id == -1) {
                    if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        RUSS::Make_Player_Into_AI(i);
                    }
                } else {
                    MOX::player[i].objectives = PLAYER_OBJECTIVE_HUMAN;
                    NETMOX::net.num_connected_players++;
                }
            }
        }

        if (MOX::NUM_PLAYERS > 0) {
            for (int32_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                COLCALC::Do_Player_Colony_Calculations(i);
            }
        }

        if (NETMOX::net.network_sync_complete_flag == 0) {
            MOX::NUM_NEBULAS = 0;
            if (MOX::NUM_STARS > 0) {
                for (int16_t i = 0; i < MOX::NUM_STARS; i++) {
                    MOX::star[i].in_nebula = 0;
                }
            }
        }

        MOX::temp_string[0] = '\0';
        MULTPLAY::Reload_Sending_Data_Info();
        video::Set_Page_Off();
        MULTPLAY::Draw_SendGet_Net_Info_Screen();

        fields::Assign_Auto_Function(MULTPLAY::Draw_SendGet_Net_Info_Screen, 2);
        RUSS::Mox_Sync_Update();

        if (MOX::NUM_PLAYERS > 0) {
            for (int32_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_READY;
                NETMOX::net.player_net_info[i].diplomacy_target_id = -1;
            }
        }

        payload.number_of_players = (int8_t)MOX::NUM_PLAYERS;
        if (MOX::NUM_PLAYERS > 0) {
            for (int32_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                payload.network_ids[i] = MOX::player[i].network_player_id;
            }
        }

        strlcpy(payload.version_string, GAME_BUILD_DATE, sizeof(payload.version_string));
        Send_Configured_Setup_Message(
            NET_PACKET_LOAD_GAME_START, &payload, sizeof(payload));

        NETMOX::net.host_election_winner_id = NETMOX::net.net_host_id;
        if (orion2re::lua::Network_Lua_Enabled() &&
            !orion2re::lua::Has_Queued_Lua_Network_Load_State()) {
            GENDRAW::Message_Box(
                "Loaded Lua save state is unavailable for network transfer.");
            Mox_Net_Uninit();
            return 0;
        }
        NETMOX::Broadcast_Game_Data_Differential(1);
        NETMOX::net.player_race_selected_flags[MOX::PLAYER_NUM] = 1;

        memset(NETMOX::net.player_phase_ready_sent_flags, 0, sizeof(NETMOX::net.player_phase_ready_sent_flags));
        NETMOX::net.sync_state = NET_SYNC_STATE_READY;

        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
        MAPGEN::Init_Main_Screen(MOX::settings.galaxy_size);
        HAROLD::Harold_Init();
        MAINSCR2::main_screen_fade_in_flag = 1;
        MOX2::Init_Map_Defaults();
        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        return 1;
    }

    int32_t __cdecl Russ_Net_Create_Game(const char* game_name, int32_t max_players, const char* str1, const char* str2) {
        RUSS::Mox_Update();
        if ((NETMOX::net.mode == 0 || NETMOX::net.mode == 1) && (game_name == nullptr || game_name[0] == '\0')) {
            Ensure_Net_Game_Name();
            game_name = NETMOX::net.cur_game_name;
        }
        int32_t result = netcode::Net_Create_Game(game_name, max_players, str1, str2);
        RUSS::Mox_Update();
        return result;
    }

    int __cdecl Dump_Users() {
        int16_t buffer[20];
        return netcode::Net_Get_Users(buffer);
    }

    int8_t __cdecl Request_Banner_Color(uint8_t requested_color) {
        if (RACESEL::color_flag[requested_color] != 0) {
            return 0;
        }

        NETMOX::Mox_Send_Message(NETMOX::net.host_election_id, NET_PACKET_REQUEST_PLAYER_COLOR, &requested_color, sizeof(requested_color));

        while (RACESEL::color_flag[requested_color] == 0) {
            int32_t unused_zero = 0;
            (void)unused_zero;
            RUSS::Mox_Update();
        }

        return static_cast<int8_t>(
            static_cast<uint8_t>(MOX::player[MOX::PLAYER_NUM].color) == requested_color);
    }

} // namespace NETSTART