#pragma once

#include "network/router_client.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace netadapter {

    constexpr uint32_t NETADAPTER_BROADCAST_EXCLUDING_SELF = ROUTER_RESERVED_BROADCAST_EXCLUDING_SELF_ID;
    constexpr uint32_t NETADAPTER_BROADCAST_INCLUDING_SELF = ROUTER_RESERVED_BROADCAST_INCLUDING_SELF_ID;

    struct s_game_packet {
        uint32_t sender_network_id = 0;
        uint32_t target_network_id = 0;
        uint16_t message_type = 0;
        std::vector<uint8_t> payload;
    };

    enum e_event_kind {
        EVENT_MEMBER_JOINED,
        EVENT_MEMBER_LEFT,
        EVENT_SESSION_LEFT,
        EVENT_SESSION_CLOSED,
        EVENT_DISCONNECTED,
        EVENT_ERROR,
    };

    struct s_event {
        e_event_kind kind = EVENT_ERROR;
        uint32_t network_id = 0;
        bool accepted_member = false;
        std::string reason;
        network::router::s_router_error_payload error;
        network::router::s_router_message_header header;
    };

    enum e_bootstrap_mode {
        BOOTSTRAP_DIRECT_ENDPOINT,
        BOOTSTRAP_LAN_DISCOVERY,
        BOOTSTRAP_EMBEDDED_LAN_HOST,
    };

    struct s_config {
        char host[ROUTER_ADDRESS_SIZE] = "127.0.0.1";
        uint16_t tcp_port = 47800;
        uint16_t discovery_port = 47800;
        uint32_t discovery_timeout_ms = 750;
        e_bootstrap_mode bootstrap_mode = BOOTSTRAP_DIRECT_ENDPOINT;
        char client_name[ROUTER_PLAYER_NAME_SIZE] = "Player";
        char build_string[ROUTER_BUILD_STRING_SIZE] = "orion2re";
        uint32_t admission_timeout_ms = 30'000;
    };

    enum e_connect_failure_phase {
        CONNECT_FAILURE_NONE,
        CONNECT_FAILURE_TCP_CONNECT,
        CONNECT_FAILURE_HELLO_SEND,
    };

    struct s_state;

    struct s_adapter {
        s_adapter();
        ~s_adapter();

        s_adapter(const s_adapter&) = delete;
        s_adapter& operator=(const s_adapter&) = delete;

        std::unique_ptr<s_state> state;
    };

    struct s_traffic_counters {
        uint64_t payload_bytes = 0;
        uint64_t packets = 0;
    };

    struct s_peer_traffic_stats {
        uint32_t network_id = 0;
        s_traffic_counters sent;
        s_traffic_counters received;
        double average_sent_kbps = 0.0;
        double average_received_kbps = 0.0;
        double recent_sent_kbps = 0.0;
        double recent_received_kbps = 0.0;
    };

    struct s_traffic_stats {
        uint64_t started_ms = 0;
        uint64_t sampled_ms = 0;
        uint32_t recent_window_ms = 10000;
        s_traffic_counters sent;
        s_traffic_counters received;
        double average_sent_kbps = 0.0;
        double average_received_kbps = 0.0;
        double recent_sent_kbps = 0.0;
        double recent_received_kbps = 0.0;
        std::vector<s_peer_traffic_stats> peers;
        network::router::s_router_client_stats transport;
    };

    void Init(s_adapter* adapter, const s_config* config);
    bool Connect(s_adapter* adapter);
    void Shutdown(s_adapter* adapter);
    void Pump(s_adapter* adapter);

    bool Is_Connected(const s_adapter* adapter);
    int Last_Error(const s_adapter* adapter);
    const char* Last_Error_Message(const s_adapter* adapter);
    uint32_t Local_Network_Id(const s_adapter* adapter);
    uint32_t Session_Owner_Network_Id(const s_adapter* adapter);
    uint32_t Session_Id(const s_adapter* adapter);
    bool Has_Pending_Client_Events(const s_adapter* adapter);
    bool Get_Traffic_Stats(const s_adapter* adapter, s_traffic_stats* stats);
    e_connect_failure_phase Connect_Failure_Phase(const s_adapter* adapter);

    bool Refresh_Game_List(s_adapter* adapter);
    uint32_t Game_List_Revision(const s_adapter* adapter);
    int32_t Game_Count(const s_adapter* adapter);
    const char* Game_Name(const s_adapter* adapter, uint32_t session_index);
    uint32_t Game_Session_Id(const s_adapter* adapter, uint32_t session_index);

    bool Create_Game(s_adapter* adapter, const char* name, int32_t max_players, const char* player_name);
    bool Join_Game(s_adapter* adapter, uint32_t session_id, const char* player_name);
    bool Leave_Game(s_adapter* adapter);
    bool Set_New_Players_Enabled(s_adapter* adapter, bool enabled);
    bool Accept_Member(s_adapter* adapter, uint32_t network_id);
    bool Reject_Member(s_adapter* adapter, uint32_t network_id, const char* reason);
    bool Is_Member_Provisional(const s_adapter* adapter, uint32_t network_id);
    bool Is_Session_Member(const s_adapter* adapter, uint32_t network_id);
    bool Is_Member_Accepted(const s_adapter* adapter, uint32_t network_id);
    int32_t Get_Users(const s_adapter* adapter, uint32_t* users, int32_t max_users);

    bool Send_Game_Message(s_adapter* adapter, uint32_t target_network_id, uint16_t message_type, const void* payload, uint32_t payload_size);
    bool Poll_Game_Packet(s_adapter* adapter, s_game_packet* packet);
    bool Poll_Event(s_adapter* adapter, s_event* event);

} // namespace netadapter