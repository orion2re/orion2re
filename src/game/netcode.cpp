#include "pch.h"

#include "game/game_wire_version.h"
#include "game/netcode_bootstrap.h"
#include "game/multplay.h"
#include "lua/lua_network.h"

namespace netcode {
    uint32_t thisHost;
    uint32_t netInitialized;
    int8_t tpSending;
    uint8_t inComms;
    recv_callback_fn recv_fn;
    int32_t connection_type;
    int32_t in_send_message;

    namespace {
        netadapter::s_adapter router_adapter;
        int32_t router_last_send_successful;
        uint32_t router_wait_session_list_revision;
        const char* initialization_error = "";
        std::string join_error;
        std::vector<uint8_t> admission_offer;
        bool admission_host = false;
        bool admission_open = false;
        enum e_admission_state {
            ADMISSION_IDLE,
            ADMISSION_WAITING,
            ADMISSION_ACCEPTED,
            ADMISSION_REJECTED,
        };
        e_admission_state admission_state = ADMISSION_IDLE;
        constexpr uint32_t ROUTER_CONNECT_ATTEMPTS = 3;
        constexpr uint32_t ROUTER_CONNECT_RETRY_DELAY_MS = 250;
        constexpr uint32_t ADMISSION_TIMEOUT_MS = 30'000;

        bool __cdecl Is_Router_Mode(uint32_t mode) {
            return mode == 0 || mode == 1;
        }

        bool Send_Admission_Decision(
            uint32_t network_id, bool accepted, const std::string& diagnostic) {
            uint8_t packet[1100] = {};
            uint32_t packet_size = 0;
            if (orion2re::lua::Write_Lua_Network_Setup_Decision(packet,
                    sizeof(packet),
                    accepted,
                    diagnostic,
                    &packet_size)) {
                return netadapter::Send_Game_Message(&router_adapter,
                    network_id,
                    NET_PACKET_LUA_SETUP_DECISION,
                    packet,
                    packet_size);
            }
            return false;
        }

        void Handle_Admission_Offer(const netadapter::s_game_packet& packet) {
            if (admission_host || admission_state != ADMISSION_WAITING ||
                packet.sender_network_id !=
                    netadapter::Session_Owner_Network_Id(&router_adapter)) {
                return;
            }
            orion2re::lua::s_lua_network_setup_offer offer;
            std::string diagnostic;
            if (!orion2re::lua::Read_Lua_Network_Setup_Offer(
                    packet.payload.data(),
                    static_cast<uint32_t>(packet.payload.size()),
                    &offer,
                    &diagnostic)) {
                join_error = diagnostic.empty()
                    ? "The host sent an invalid compatibility offer."
                    : diagnostic;
                admission_state = ADMISSION_REJECTED;
                return;
            }
            uint8_t result[1100] = {};
            uint32_t result_size = 0;
            if (!orion2re::lua::Process_Lua_Network_Setup_Offer(
                    packet.payload.data(),
                    static_cast<uint32_t>(packet.payload.size()),
                    offer.kind,
                    result,
                    sizeof(result),
                    &result_size) ||
                !netadapter::Send_Game_Message(&router_adapter,
                    packet.sender_network_id,
                    NET_PACKET_LUA_SETUP_RESULT,
                    result,
                    result_size)) {
                join_error = "Unable to verify game content with the host.";
                admission_state = ADMISSION_REJECTED;
            }
        }

        void Handle_Admission_Result(const netadapter::s_game_packet& packet) {
            if (!admission_host ||
                !netadapter::Is_Member_Provisional(
                    &router_adapter, packet.sender_network_id) ||
                netadapter::Is_Member_Accepted(
                    &router_adapter, packet.sender_network_id)) {
                return;
            }
            std::string diagnostic;
            const bool accepted =
                admission_open &&
                orion2re::lua::Validate_Lua_Network_Setup_Result(
                    packet.payload.data(),
                    static_cast<uint32_t>(packet.payload.size()),
                    &diagnostic);
            if (!admission_open) {
                diagnostic = "The host has already started the game.";
            }
            if (accepted) {
                if (Send_Admission_Decision(
                        packet.sender_network_id, true, {}) &&
                    netadapter::Accept_Member(
                        &router_adapter, packet.sender_network_id)) {
                    SDL_Log("Network member %u passed compatibility admission",
                        (unsigned)packet.sender_network_id);
                    return;
                }
                if (diagnostic.empty()) {
                    diagnostic = "Unable to complete compatibility admission.";
                }
            }
            if (!accepted || !diagnostic.empty()) {
                const bool decision_sent = Send_Admission_Decision(
                    packet.sender_network_id,
                    false,
                    diagnostic);
                if (decision_sent) {
                    netadapter::Reject_Member(
                        &router_adapter,
                        packet.sender_network_id,
                        diagnostic.c_str());
                }
                SDL_Log("Network member %u rejected: %s",
                    (unsigned)packet.sender_network_id,
                    diagnostic.c_str());
            }
        }

        void Handle_Admission_Decision(const netadapter::s_game_packet& packet) {
            if (admission_host || admission_state != ADMISSION_WAITING ||
                packet.sender_network_id !=
                    netadapter::Session_Owner_Network_Id(&router_adapter)) {
                return;
            }
            std::string diagnostic;
            if (orion2re::lua::Validate_Lua_Network_Setup_Result(
                    packet.payload.data(),
                    static_cast<uint32_t>(packet.payload.size()),
                    &diagnostic)) {
                netadapter::Accept_Member(&router_adapter,
                    netadapter::Local_Network_Id(&router_adapter));
                admission_state = ADMISSION_ACCEPTED;
                return;
            }
            join_error = diagnostic.empty()
                ? "The host rejected incompatible game content."
                : diagnostic;
            orion2re::lua::Discard_Pending_Lua_Network_Setup();
            admission_state = ADMISSION_REJECTED;
        }

        bool Build_Admission_Offer() {
            try {
                admission_offer.resize(
                    orion2re::lua::LUA_NETWORK_MAX_BYTES);
            } catch (...) {
                admission_offer.clear();
                join_error = "Unable to allocate the compatibility offer.";
                return false;
            }
            const auto kind = NETMOX::net.sync_state ==
                    NET_SYNC_STATE_LOAD_GAME_HOST_SETUP
                ? orion2re::lua::LUA_NETWORK_SETUP_LOAD_GAME
                : orion2re::lua::LUA_NETWORK_SETUP_NEW_GAME;
            uint32_t size = 0;
            std::string diagnostic;
            if (!orion2re::lua::Begin_Lua_Network_Setup_Offer(
                    admission_offer.data(),
                    static_cast<uint32_t>(admission_offer.size()),
                    kind,
                    &size,
                    &diagnostic)) {
                admission_offer.clear();
                join_error = diagnostic.empty()
                    ? "Unable to build the compatibility offer."
                    : diagnostic;
                return false;
            }
            admission_offer.resize(size);
            return true;
        }

        void __cdecl Pump_Router_Packets() {
            netadapter::Pump(&router_adapter);

            if (netInitialized != 0 &&
                (!netadapter::Is_Connected(&router_adapter) || netadapter::Session_Id(&router_adapter) == 0)) {
                SDL_Log("Lost router session: %s", netadapter::Last_Error_Message(&router_adapter));
                throw session_lost{};
            }

            netadapter::s_game_packet packet;
            while (netadapter::Poll_Game_Packet(&router_adapter, &packet)) {
                if (packet.message_type == NET_PACKET_LUA_SETUP_OFFER) {
                    Handle_Admission_Offer(packet);
                    continue;
                }
                if (packet.message_type == NET_PACKET_LUA_SETUP_RESULT) {
                    Handle_Admission_Result(packet);
                    continue;
                }
                if (packet.message_type == NET_PACKET_LUA_SETUP_DECISION) {
                    Handle_Admission_Decision(packet);
                    continue;
                }
                if (!netadapter::Is_Member_Accepted(
                        &router_adapter, packet.sender_network_id)) {
                    SDL_Log("Ignoring game packet %u from provisional member %u",
                        (unsigned)packet.message_type,
                        (unsigned)packet.sender_network_id);
                    continue;
                }
                if (recv_fn != nullptr) {
                    recv_fn(
                        (int16_t)packet.sender_network_id,
                        packet.message_type,
                        (uint32_t)packet.payload.size(),
                        packet.payload.empty() ? nullptr : packet.payload.data());
                }
            }

            netadapter::s_event event;
            while (netadapter::Poll_Event(&router_adapter, &event)) {
                if (event.kind == netadapter::EVENT_MEMBER_JOINED) {
                    if (admission_host && !admission_offer.empty()) {
                        netadapter::Send_Game_Message(&router_adapter,
                            event.network_id,
                            NET_PACKET_LUA_SETUP_OFFER,
                            admission_offer.data(),
                            static_cast<uint32_t>(admission_offer.size()));
                    }
                } else if (event.kind == netadapter::EVENT_MEMBER_LEFT) {
                    SDL_Log("Router member left: %u", (unsigned)event.network_id);
                    const auto player = NETMOX::Net_Id_To_Player_Id((int32_t)event.network_id);
                    if (player != game::NO_PLAYER) {
                        NETMOX::Handle_Network_Player_Disconnected(player);
                    } else if (event.accepted_member) {
                        orion2re::lua::Reset_Lua_Network_Session();
                    }
                } else if (event.kind == netadapter::EVENT_SESSION_LEFT) {
                    if (admission_state == ADMISSION_WAITING) {
                        join_error = event.reason.empty()
                            ? "The host ended compatibility admission."
                            : event.reason;
                        admission_state = ADMISSION_REJECTED;
                    }
                } else if (event.kind == netadapter::EVENT_SESSION_CLOSED) {
                    SDL_Log("Router session closed");
                    if (admission_state == ADMISSION_WAITING) {
                        join_error = "The host closed the network session.";
                        admission_state = ADMISSION_REJECTED;
                    }
                    orion2re::lua::Reset_Lua_Network_Session();
                } else if (event.kind == netadapter::EVENT_DISCONNECTED) {
                    SDL_Log("Router disconnected");
                    if (admission_state == ADMISSION_WAITING) {
                        join_error = "The router disconnected during admission.";
                        admission_state = ADMISSION_REJECTED;
                    }
                    orion2re::lua::Reset_Lua_Network_Session();
                } else if (event.kind == netadapter::EVENT_ERROR) {
                    if (admission_state == ADMISSION_WAITING) {
                        join_error = event.error.code ==
                                network::router::ROUTER_ERROR_BUILD_MISMATCH
                            ? "Cannot join: incompatible network version."
                            : event.error.message;
                        admission_state = ADMISSION_REJECTED;
                    }
                    SDL_Log(
                        "Router error: %s (code=%u sender=%u target=%u request=%u)",
                        event.error.message,
                        (unsigned)event.error.code,
                        (unsigned)event.header.sender_network_id,
                        (unsigned)event.header.target_network_id,
                        (unsigned)event.header.request_id);
                }
            }
        }

        void __cdecl Pump_Router_Until(bool (*predicate)(), uint32_t timeout_ms) {
            uint64_t start_time = SDL_GetTicks();
            while (!predicate()) {
                Pump_Router_Packets();
                if ((uint32_t)(SDL_GetTicks() - start_time) >= timeout_ms) {
                    break;
                }
                SDL_Delay(1);
            }
        }

        bool __cdecl Router_Local_Id_Ready() {
            return netadapter::Local_Network_Id(&router_adapter) != 0;
        }

        bool __cdecl Router_Session_List_Ready() {
            return netadapter::Game_List_Revision(&router_adapter) != router_wait_session_list_revision;
        }

        bool __cdecl Router_Session_Ready() {
            return netadapter::Session_Id(&router_adapter) != 0;
        }

        bool __cdecl Router_Join_Finished() {
            return Router_Session_Ready() ||
                admission_state == ADMISSION_REJECTED;
        }

        int32_t __cdecl Refresh_Router_Game_List(uint32_t timeout_ms) {
            if (!Is_Router_Mode((uint32_t)connection_type)) {
                return 0;
            }

            router_wait_session_list_revision = netadapter::Game_List_Revision(&router_adapter);
            if (!netadapter::Refresh_Game_List(&router_adapter)) {
                return netadapter::Game_Count(&router_adapter);
            }
            Pump_Router_Until(Router_Session_List_Ready, timeout_ms);
            return netadapter::Game_Count(&router_adapter);
        }

        bool __cdecl Init_Router_Config(netadapter::s_config* config, const char* player_name, uint32_t mode) {
            *config = netadapter::s_config{};
            e_router_endpoint_result endpoint_result =
                Configure_Router_Endpoint_For_Mode(mode, MULTPLAY::router_endpoint, config);
            if (mode == 1) {
                if (endpoint_result == ROUTER_ENDPOINT_MISSING) {
                    initialization_error =
                        "Online play requires a router. Add interface router = \"host[:port]\"; to data/orion2re.cfg.";
                    SDL_Log("%s", initialization_error);
                    return false;
                }
                if (endpoint_result == ROUTER_ENDPOINT_INVALID) {
                    initialization_error =
                        "Invalid online router. Use interface router = \"host[:port]\"; in data/orion2re.cfg.";
                    SDL_Log("%s", initialization_error);
                    return false;
                }
                SDL_Log("Router endpoint from interface config: %s:%u", config->host, (unsigned)config->tcp_port);
            }
            if (player_name != nullptr && player_name[0] != '\0') {
                network::router::Router_Copy_String(config->client_name, sizeof(config->client_name), player_name);
            }
            static_assert(sizeof(config->build_string) >= sizeof(NETPROTO::GAME_WIRE_VERSION));
            network::router::Router_Copy_String(config->build_string, sizeof(config->build_string), NETPROTO::GAME_WIRE_VERSION);
            return true;
        }

        const char* __cdecl Get_Router_Bootstrap_Mode_Name(netadapter::e_bootstrap_mode mode) {
            switch (mode) {
                case netadapter::BOOTSTRAP_LAN_DISCOVERY:
                    return "LAN discovery";
                case netadapter::BOOTSTRAP_EMBEDDED_LAN_HOST:
                    return "embedded LAN host";
                default:
                    return "direct endpoint";
            }
        }

        int32_t __cdecl Connect_Router_With_Retry(const netadapter::s_config* config) {
            for (uint32_t attempt = 1; attempt <= ROUTER_CONNECT_ATTEMPTS; ++attempt) {
                if (netadapter::Connect(&router_adapter)) {
                    return 1;
                }

                const char* failure_phase = "startup";
                switch (netadapter::Connect_Failure_Phase(&router_adapter)) {
                    case netadapter::CONNECT_FAILURE_TCP_CONNECT:
                        failure_phase = "TCP connect";
                        break;
                    case netadapter::CONNECT_FAILURE_HELLO_SEND:
                        failure_phase = "HELLO send";
                        break;
                    default:
                        break;
                }

                SDL_Log(
                    "Router %s attempt %u/%u failed for %s:%u, error=%d (%s)",
                    failure_phase,
                    (unsigned)attempt,
                    (unsigned)ROUTER_CONNECT_ATTEMPTS,
                    config->host,
                    (unsigned)config->tcp_port,
                    netadapter::Last_Error(&router_adapter),
                    netadapter::Last_Error_Message(&router_adapter));

                netadapter::Shutdown(&router_adapter);
                if (attempt != ROUTER_CONNECT_ATTEMPTS) {
                    SDL_Delay(ROUTER_CONNECT_RETRY_DELAY_MS);
                }
            }
            return 0;
        }
    } // namespace

    const char* __cdecl Net_Initialization_Error() {
        return initialization_error;
    }

    const char* __cdecl Net_Join_Error() {
        return join_error.c_str();
    }

    int32_t __cdecl Do_Net_Init(uint32_t mode, recv_callback_fn recv_fn_ptr, s_network* network) {
        RUSS::Mox_Update();
        int32_t result = Net_Init(mode, recv_fn_ptr, network);
        RUSS::Mox_Update();
        return result;
    }

    int32_t __cdecl Net_Init(uint32_t mode, recv_callback_fn recv_fn_ptr, s_network* network) {
        int32_t result = 0;
        connection_type = mode;
        initialization_error = "";
        join_error.clear();
        admission_offer.clear();
        admission_host = false;
        admission_open = false;
        admission_state = ADMISSION_IDLE;
        SDL_Log("Net_Init mode=%u", (unsigned)mode);

        if (Is_Router_Mode(mode)) {
            time_::_time = time_::time_;
            time_::_time2 = time_::time_;

            netadapter::s_config config;
            if (!Init_Router_Config(&config, nullptr, mode)) {
                connection_type = -1;
                return 0;
            }
            config.bootstrap_mode = Get_Router_Bootstrap_Mode(mode, network != nullptr ? network->sync_state : NET_SYNC_STATE_IDLE);
            SDL_Log(
                "Router bootstrap=%s endpoint=%s:%u sync_state=%u",
                Get_Router_Bootstrap_Mode_Name(config.bootstrap_mode),
                config.host,
                (unsigned)config.tcp_port,
                network != nullptr ? (unsigned)network->sync_state : (unsigned)NET_SYNC_STATE_IDLE);
            netadapter::Init(&router_adapter, &config);
            result = Connect_Router_With_Retry(&config);
            if (result != 0 && netadapter::Is_Connected(&router_adapter)) {
                Pump_Router_Until(Router_Local_Id_Ready, 5000);
                result = Router_Local_Id_Ready() ? 1 : 0;
                if (result == 0) {
                    SDL_Log("Router connected but WELCOME was not received within timeout");
                } else {
                    SDL_Log("Router connected as network id %u", (unsigned)netadapter::Local_Network_Id(&router_adapter));
                }
            }
            if (result != 0) {
                Refresh_Router_Game_List(1000);
            }
            if (result == 0) {
                netadapter::Shutdown(&router_adapter);
            }

            recv_fn = recv_fn_ptr;
        }

        if (mode == 2) {
            SDL_Log("Unsupported legacy networking mode");
            connection_type = -1;
            return 0;
        }

        if (mode == 3) {
            connection_type = -1;
            return 0;
        }

        return result;
    }

    int32_t __cdecl Net_Get_Game_Count() {
        if (Is_Router_Mode((uint32_t)connection_type)) {
            Pump_Router_Packets();
            return netadapter::Game_Count(&router_adapter);
        }
        return 0;
    }

    int32_t __cdecl Net_Refresh_Game_List() {
        return Refresh_Router_Game_List(1000);
    }

    int32_t __cdecl Get_Player_Count() {
        if (Is_Router_Mode((uint32_t)connection_type)) {
            Pump_Router_Packets();
            return netadapter::Get_Users(&router_adapter, nullptr, 0);
        }
        return 0;
    }

    char* __cdecl Net_Get_Game_Name(uint32_t session_idx) {
        if (Is_Router_Mode((uint32_t)connection_type)) {
            Pump_Router_Packets();
            return const_cast<char*>(netadapter::Game_Name(&router_adapter, session_idx));
        }
        return nullptr;
    }

    uint32_t __cdecl Net_Get_Game_Session_Id(uint32_t session_idx) {
        if (Is_Router_Mode((uint32_t)connection_type)) {
            Pump_Router_Packets();
            return netadapter::Game_Session_Id(&router_adapter, session_idx);
        }
        return 0;
    }

    void __cdecl Net_Uninit() {
        if (Is_Router_Mode((uint32_t)connection_type)) {
            netadapter::Shutdown(&router_adapter);
        }
        netInitialized = 0;
        thisHost = 0;
        inComms = 0;
        in_send_message = 0;
        tpSending = 0;
        router_last_send_successful = 0;
        orion2re::lua::Discard_Pending_Lua_Network_Setup();
        join_error.clear();
        admission_offer.clear();
        admission_host = false;
        admission_open = false;
        admission_state = ADMISSION_IDLE;
        recv_fn = nullptr;
        connection_type = -1;
    }

    int32_t __cdecl Net_Is_Sending() {
        return (int32_t)netcode::tpSending;
    }

    int32_t __cdecl Net_Join_Game(uint32_t session_id, const char* password, const char* player_name) {
        (void)password;
        join_error.clear();
        admission_host = false;
        admission_state = ADMISSION_WAITING;
        if (session_id == 0) {
            join_error = "Invalid network session.";
            admission_state = ADMISSION_REJECTED;
            return 0;
        }
        if (!netadapter::Join_Game(&router_adapter, session_id, player_name)) {
            join_error = "The selected game is unavailable or closed.";
            admission_state = ADMISSION_REJECTED;
            return 0;
        }

        Pump_Router_Until(Router_Join_Finished, 5000);
        if (!Router_Session_Ready()) {
            admission_state = ADMISSION_REJECTED;
            return 0;
        }

        const uint64_t started = SDL_GetTicks();
        while (admission_state == ADMISSION_WAITING &&
            SDL_GetTicks() - started < ADMISSION_TIMEOUT_MS) {
            Pump_Router_Packets();
            SDL_Delay(1);
        }
        if (admission_state != ADMISSION_ACCEPTED) {
            if (join_error.empty()) {
                join_error =
                    "Timed out verifying game content with the host.";
            }
            orion2re::lua::Discard_Pending_Lua_Network_Setup();
            netadapter::Leave_Game(&router_adapter);
            return 0;
        }

        thisHost = netadapter::Local_Network_Id(&router_adapter);

        netInitialized = 1;
        return 1;
    }

    int32_t __cdecl Net_Get_Users(int16_t* users) {
        if (Is_Router_Mode((uint32_t)connection_type)) {
            Pump_Router_Packets();
            uint32_t router_users[256]{};
            int32_t player_count = netadapter::Get_Users(&router_adapter, router_users, 256);
            if (users != nullptr && player_count > 0) {
                for (int32_t i = 0; i < player_count; ++i) {
                    users[i] = (int16_t)router_users[i];
                }
            }
            return player_count;
        }

        return 0;
    }

    void __cdecl Net_Accept_Member(int32_t network_id) {
        if (Is_Router_Mode((uint32_t)connection_type) && network_id > 0) {
            netadapter::Accept_Member(
                &router_adapter, static_cast<uint32_t>(network_id));
        }
    }

    void __cdecl Net_Disable_New_Players() {
        if (Is_Router_Mode((uint32_t)connection_type)) {
            admission_open = false;
            netadapter::Set_New_Players_Enabled(&router_adapter, false);
            Pump_Router_Packets();
            return;
        }
    }

    uint32_t __cdecl Net_Host_ID() {
        if (Is_Router_Mode((uint32_t)connection_type)) {
            return netadapter::Local_Network_Id(&router_adapter);
        }
        return thisHost;
    }

    uint32_t __cdecl Net_Session_Owner_ID() {
        if (Is_Router_Mode((uint32_t)connection_type)) {
            Pump_Router_Packets();
            return netadapter::Session_Owner_Network_Id(&router_adapter);
        }
        return 0;
    }

    void __cdecl Net_Update() {
        if (Is_Router_Mode((uint32_t)connection_type) && inComms == 0) {
            inComms = 1;
            if (netInitialized != 0) {
                Pump_Router_Packets();
            }
            inComms = 0;
        }
    }

    uint32_t __cdecl Net_Send_Message(int32_t target_id, int16_t msg_type, void* packet, uint32_t size) {
        uint8_t* data = (uint8_t*)packet;

        if (in_send_message == 0) {
            in_send_message = 1;
        } else {
            SDL_Log("REENTERING (Net_Send_Message)!!!!");
        }

        if (Is_Router_Mode((uint32_t)connection_type)) {
            router_last_send_successful = netadapter::Send_Game_Message(&router_adapter, (uint32_t)target_id, (uint16_t)msg_type, data, size) ? 1 : 0;
            const bool send_failed = router_last_send_successful == 0;
            Pump_Router_Packets();
            if (send_failed && netInitialized != 0) {
                SDL_Log("Router send failed: %s", netadapter::Last_Error_Message(&router_adapter));
                throw session_lost{};
            }
        }

        in_send_message = 0;
        return Net_Status() & 4;
    }

    uint32_t __cdecl Net_Status() {
        uint32_t status = 0;

        if (Is_Router_Mode((uint32_t)connection_type)) {
            if (router_last_send_successful != 0) {
                status |= 4;
            }
        }

        return status;
    }

    int32_t __cdecl Net_Get_Traffic_Stats(netadapter::s_traffic_stats* stats) {
        if (!Is_Router_Mode((uint32_t)connection_type)) {
            if (stats != nullptr) {
                *stats = {};
            }
            return 0;
        }
        return netadapter::Get_Traffic_Stats(&router_adapter, stats) ? 1 : 0;
    }

    int32_t __cdecl Net_Create_Game(const char* name, int32_t max_players, const char* password, const char* player_name) {
        (void)password;
        join_error.clear();
        admission_host = true;
        admission_open = true;
        admission_state = ADMISSION_ACCEPTED;
        if (!Build_Admission_Offer()) {
            admission_host = false;
            admission_state = ADMISSION_REJECTED;
            return 0;
        }
        if (!netadapter::Create_Game(&router_adapter, name, max_players, player_name)) {
            admission_offer.clear();
            admission_host = false;
            admission_open = false;
            admission_state = ADMISSION_REJECTED;
            return 0;
        }

        Pump_Router_Until(Router_Session_Ready, 5000);
        if (!Router_Session_Ready()) {
            admission_offer.clear();
            admission_host = false;
            admission_open = false;
            admission_state = ADMISSION_REJECTED;
            return 0;
        }

        thisHost = netadapter::Local_Network_Id(&router_adapter);

        netInitialized = 1;
        return 1;
    }
} // namespace netcode