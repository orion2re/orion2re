#pragma once

typedef struct s_net_snapshot_cache s_net_snapshot_cache, *Ps_net_snapshot_cache;

struct s_net_snapshot_cache {
    uint8_t* buffer;
    uint8_t* current_ptr;
    uint32_t buffer_size;
};

namespace NETMOX {
    constexpr uint32_t NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES = 8 * 1024 * 1024;

    extern s_network net;

    extern uint8_t sending_msg;

    extern char* lost_net_user_error;

    extern char* only_1_host_allowed_error;

    extern char* net_snapshot_diff_buffer_error;

    extern char* net_snapshot_cache_error;

    extern s_net_snapshot_cache net_snapshot_cache;

    extern char* incompatible_versions_error;

    extern char* ambassador_left_before_you_arrived_fmt;

    extern char* net_send_error;

    extern uint8_t in_receive_message;

    extern uint8_t is_updating;

    extern uint8_t next_update_player_counter;

    game::id_player __cdecl Human_Requesting_Diplomacy();

    void __cdecl Accept_Diplomacy_Request(game::id_player player);

    void __cdecl Refuse_Diplomacy_Request(game::id_player player);

    void __cdecl Tell_Diplomacy_Requester_To_Wait(game::id_player player);

    game::player_bitmap Humans_Requesting_Diplomacy();

    void __cdecl Russ_Change_Relations(int16_t change, game::id_player attacker, game::id_player defender, int16_t reason, int16_t star_idx, int16_t incident_value);

    constexpr uint16_t NET_MSG_PLAYER_DISCONNECTED = 0x45;

    void __cdecl Send_Drop_Out_Info(game::id_player player, uint16_t msg_type);

    void __cdecl Host_Send_Player_Disconnected_Info(game::id_player player);

    void __cdecl Handle_Network_Player_Disconnected(game::id_player player);
    bool Is_Disconnect_Recovery_Host(game::id_player dropped_player, int32_t sender_network_id);

    void __cdecl Russ_Event_Monster_Update(game::owner_slot monster, game::id_planet target, game::owner_slot victor = game::NO_OWNER);

    game::id_player __cdecl Net_Id_To_Player_Id(int32_t network_id);

    void __cdecl Send_Pick_Your_Position(int16_t network_player_id);

    char* __cdecl Load_Netmox_Strings(char* strings);

    void __cdecl Update();

    void __cdecl Mox_Net_Update();

    inline bool net_debug_enabled = false;

    bool __cdecl Net_Debug_Enabled();

    void __cdecl Net_Debug_Log(const char* format, ...);

    void __cdecl Send_You_Are_Target_Info(
        game::id_player target,
        game::owner_slot other,
        game::owner_slot defender,
        game::id_colony colony_id,
        game::s_star_destination destination);

    void __cdecl Send_Pick_Target_Info(game::id_player player, game::s_star_destination destination, s_combfind_target* target_info);

    void Ditch_Leader(game::s_game_state& state, game::id_leader key);

    void __cdecl Mark_Leader_Deassigned(int16_t leader_idx);

    void __cdecl Clear_Leader_Deassignments();

    int32_t __cdecl Mox_Send_Message(int16_t recipient_net_id, uint16_t msg_type, void* packet, uint32_t packet_len);

    bool __cdecl Decode_Game_Orders(const void* data, uint32_t len, game::id_player expected_player);

    uint32_t __cdecl Encode_Broadcast_Game_Data_Snapshot(void* buffer, uint32_t len);

    void __cdecl Broadcast_Game_Data_Differential(uint8_t use_diff);

    int8_t __cdecl Decode_Broadcast_Game_Data_Differential(void* buffer, size_t size, uint8_t flag);

    s_net_snapshot_cache* __cdecl Open_Net_Snapshot_Cache(uint32_t size);

    int __cdecl fseek_net_snapshot_cache(s_net_snapshot_cache* cache, int offset, int origin);

    size_t __cdecl fread_net_snapshot_cache(void* ptr, size_t size, size_t count, s_net_snapshot_cache* stream);

    size_t __cdecl fwrite_net_snapshot_cache(const void* buffer, size_t size, size_t count, s_net_snapshot_cache* stream);

    int __cdecl Decode_Difference_Buffer(s_net_snapshot_cache* cache, uint8_t* out_buffer, int max_out_len, uint8_t* diff_buffer, int diff_len);

    int32_t __cdecl Make_Difference_Buffer(s_net_snapshot_cache* cache, uint8_t* buffer, int32_t buffer_size, uint8_t* diff_buffer, int32_t diff_buffer_max_size);

    int32_t __cdecl fgetc_net_snapshot_cache(s_net_snapshot_cache* cache);

    void __cdecl Write_Net_Snapshot_Cache(s_net_snapshot_cache* cache, const void* buffer, size_t size);

    void __cdecl Send_Game_Orders();

    void __cdecl Receive_Message(int16_t net_id, uint16_t packet_id, uint32_t len, uint8_t* packet);

    void __cdecl Net_Request_AI_Diplomacy(game::id_player player);

    int8_t __cdecl Net_May_Begin_AI_Diplomacy();

    bool __cdecl No_AI_Diplomacy_Allowed();

    void __cdecl Net_Cancel_AI_Diplomacy_Request();

    bool __cdecl Net_Cancel_AI_Diplomacy_Confirmed();

    void __cdecl Net_End_AI_Diplomacy();

    void __cdecl Net_Request_Human_Diplomacy(game::id_player player);

    int8_t __cdecl Net_May_Begin_Human_Diplomacy();

    int8_t __cdecl Human_Hit_Next_Turn(game::id_player player);

    bool __cdecl Allowed_To_Cancel_Human_Diplomacy();

    int8_t __cdecl Human_Diplomat_Says_Wait();

    bool __cdecl Net_Human_Rejected_Diplomacy();

    void __cdecl Net_End_Human_Diplomacy();

    void __cdecl Net_Cancel_Human_Diplomacy_Request();

    game::id_player __cdecl Net_Diplomacy_With();

    void __cdecl Human_Abdicates(game::id_player player);

    void __cdecl Human_Surrenders(game::id_player player);

    game::id_player __cdecl Get_Next_AI_Talker(game::id_player target);

    void __cdecl Decode_Pick_Target_Info(void* packet_data, uint32_t len);

    void __cdecl Decode_You_Are_Target_Info(void* packet_data, uint32_t len);

    void __cdecl Decode_Battle_Results(void* packet_data, uint32_t len);

    void __cdecl Decode_Pick_Your_Position(s_net_pack_pick_your_position* packet, uint32_t len);

    void __cdecl Decode_Dragon_Info(const void* payload, uint32_t len);

    void __cdecl Send_Dragon_Info(game::id_colony colony_id);

    inline constexpr size_t TARGET_LIST_BYTES = 5 * sizeof(uint32_t) + 17 * sizeof(int16_t);
    inline constexpr size_t COMBAT_DESTINATION_BYTES = sizeof(uint8_t) + sizeof(uint32_t);
    inline constexpr size_t CHANGE_RELATIONS_BYTES = 5 * sizeof(int16_t) + COMBAT_DESTINATION_BYTES;

    void __cdecl Decode_Drop_Out_Info(game::id_player player, s_net_pack_drop_out_info* drop_out_info);

    void __cdecl Decode_Player_Disconnected_Info(void* data, uint32_t len);

    void __cdecl Send_Battle_Results(game::s_star_destination destination, game::owner_slot attacker, game::owner_slot target, game::owner_slot defender, game::id_planet combat_planet_id, int16_t combat_result);

#pragma pack(push, 1)
    struct s_packet_dragon_info {
        unaligned::field<game::id_colony> colony_id;
    };

    struct s_packet_0x12 {
        uint8_t destination_kind;
        unaligned::field<game::id_star> star_id;
        int16_t other_player_idx;
        int16_t defender_id;
        unaligned::field<game::id_colony> colony_id;
    };

    struct s_NET_MSG_SYNC_PLAYER {
        int32_t player1_idx;
        s_net_player_wire player1;
        s_net_star_surrender_wire surrender_to;
        int32_t player2_idx;
        s_net_player_wire player2;
    };

    struct s_NET_MSG_RANK {
        int32_t rank;
        int8_t hit_next_turn;
    };

    struct s_NET_MSG_COLOR {
        int8_t color;
        int8_t player_idx;
    };

    struct s_NET_MSG_RACE {
        uint8_t race;
        int8_t player_idx;
    };
#pragma pack(pop)
} // namespace NETMOX
