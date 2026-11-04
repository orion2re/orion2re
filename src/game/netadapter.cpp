#include "pch.h"

#include "game/netadapter.h"

#include "network/router_service.h"
#include "network/router_session_browser.h"

namespace netadapter {
    namespace {
        constexpr uint32_t k_recent_window_seconds = 10;

        struct s_traffic_bucket {
            uint64_t second = 0;
            s_traffic_counters sent;
            s_traffic_counters received;
        };

        struct s_peer_traffic_state {
            uint32_t network_id = 0;
            s_traffic_counters sent;
            s_traffic_counters received;
            std::array<s_traffic_bucket, k_recent_window_seconds> buckets;
        };

        struct s_provisional_member {
            uint32_t network_id = 0;
            uint64_t deadline_ms = 0;
            bool removal_requested = false;
            std::string removal_reason;
        };

        struct s_wire_game_packet_header {
            uint16_t message_type = 0;
            uint32_t payload_size = 0;
        };

        void Write_Uint16(network::router::Bit_Stream* stream, uint16_t value) {
            stream->Write_Uint16(value);
        }

        void Write_Uint32(network::router::Bit_Stream* stream, uint32_t value) {
            stream->Write_Uint32(value);
        }

        bool Decode_Game_Packet(const network::router::s_router_message* message, s_game_packet* packet) {
            if (message == nullptr || packet == nullptr) {
                return false;
            }

            network::router::Bit_Stream reader = message->payload;
            uint16_t message_type = 0;
            uint32_t payload_size = 0;
            if (!reader.Read_Uint16(&message_type) || !reader.Read_Uint32(&payload_size)) {
                return false;
            }
            if (reader.Size() - reader.Tell() != payload_size) {
                return false;
            }

            packet->sender_network_id = message->header.sender_network_id;
            packet->target_network_id = message->header.target_network_id;
            packet->message_type = message_type;
            packet->payload.resize(payload_size);
            if (payload_size != 0 && !reader.Read_Bytes(packet->payload.data(), payload_size)) {
                return false;
            }
            return reader.At_End();
        }

        void Copy_Config_To_Client(const s_config* config, network::router::s_router_client_config* client_config) {
            network::router::Router_Copy_String(client_config->host, sizeof(client_config->host), config != nullptr ? config->host : "127.0.0.1");
            client_config->tcp_port = config != nullptr ? config->tcp_port : 47800;
        }

    } // namespace

    struct s_state {
        s_config config;
        network::router::s_router_client client;
        network::router::s_router_session_browser browser;
        network::router::s_router_service embedded_router_service;
        SDL_Thread* embedded_router_thread = nullptr;
        SDL_AtomicInt embedded_router_result;
        std::deque<s_game_packet> packets;
        std::deque<s_event> events;
        uint32_t next_request_id = 1;
        uint32_t session_list_revision = 0;
        e_connect_failure_phase connect_failure_phase = CONNECT_FAILURE_NONE;
        uint64_t traffic_started_ms = 0;
        s_traffic_counters traffic_sent;
        s_traffic_counters traffic_received;
        std::array<s_traffic_bucket, k_recent_window_seconds> traffic_buckets;
        std::vector<s_peer_traffic_state> peer_traffic;
        std::vector<uint32_t> accepted_members;
        std::vector<s_provisional_member> provisional_members;
    };

    namespace {

        uint64_t Saturating_Add(uint64_t left, uint64_t right) {
            return right > std::numeric_limits<uint64_t>::max() - left
                ? std::numeric_limits<uint64_t>::max()
                : left + right;
        }

        void Add_Counters(s_traffic_counters* counters, uint64_t bytes) {
            counters->payload_bytes = Saturating_Add(counters->payload_bytes, bytes);
            counters->packets = Saturating_Add(counters->packets, 1);
        }

        s_peer_traffic_state* Peer_Traffic(s_state* state, uint32_t network_id) {
            for (s_peer_traffic_state& peer : state->peer_traffic) {
                if (peer.network_id == network_id) {
                    return &peer;
                }
            }
            state->peer_traffic.push_back({});
            state->peer_traffic.back().network_id = network_id;
            return &state->peer_traffic.back();
        }

        s_traffic_bucket* Current_Bucket(
            std::array<s_traffic_bucket, k_recent_window_seconds>* buckets,
            uint64_t now_ms) {
            const uint64_t second = now_ms / 1000;
            s_traffic_bucket& bucket = (*buckets)[second % buckets->size()];
            if (bucket.second != second) {
                bucket = {};
                bucket.second = second;
            }
            return &bucket;
        }

        void Account_Traffic(s_state* state, uint32_t network_id, uint64_t bytes, bool sent) {
            const uint64_t now_ms = SDL_GetTicks();
            s_peer_traffic_state* peer = Peer_Traffic(state, network_id);
            s_traffic_bucket* peer_bucket = Current_Bucket(&peer->buckets, now_ms);
            if (sent) {
                Add_Counters(&peer->sent, bytes);
                Add_Counters(&peer_bucket->sent, bytes);
            } else {
                Add_Counters(&peer->received, bytes);
                Add_Counters(&peer_bucket->received, bytes);
            }
        }

        void Account_Aggregate_Traffic(s_state* state, uint64_t bytes, bool sent) {
            s_traffic_bucket* bucket = Current_Bucket(
                &state->traffic_buckets, SDL_GetTicks());
            if (sent) {
                Add_Counters(&state->traffic_sent, bytes);
                Add_Counters(&bucket->sent, bytes);
            } else {
                Add_Counters(&state->traffic_received, bytes);
                Add_Counters(&bucket->received, bytes);
            }
        }

        s_traffic_counters Recent_Counters(
            const std::array<s_traffic_bucket, k_recent_window_seconds>& buckets,
            uint64_t now_ms,
            bool sent) {
            s_traffic_counters counters;
            const uint64_t current_second = now_ms / 1000;
            for (const s_traffic_bucket& bucket : buckets) {
                if (bucket.second > current_second ||
                    current_second - bucket.second >= k_recent_window_seconds) {
                    continue;
                }
                const s_traffic_counters& source = sent ? bucket.sent : bucket.received;
                counters.payload_bytes = Saturating_Add(
                    counters.payload_bytes, source.payload_bytes);
                counters.packets = Saturating_Add(counters.packets, source.packets);
            }
            return counters;
        }

        double Rate_Kbps(uint64_t bytes, uint64_t elapsed_ms) {
            return elapsed_ms == 0 ? 0.0
                                   : (double)bytes * 8.0 / (double)elapsed_ms;
        }

        void Reset_Traffic(s_state* state) {
            state->traffic_started_ms = SDL_GetTicks();
            state->traffic_sent = {};
            state->traffic_received = {};
            state->traffic_buckets = {};
            state->peer_traffic.clear();
        }

        bool Is_Accepted_Member(
            const s_state* state,
            const network::router::s_router_member_info& member) {
            return member.owner ||
                std::find(state->accepted_members.begin(),
                    state->accepted_members.end(),
                    member.network_id) !=
                state->accepted_members.end();
        }

        void Remove_Provisional_Member(s_state* state, uint32_t network_id) {
            state->provisional_members.erase(
                std::remove_if(state->provisional_members.begin(),
                    state->provisional_members.end(),
                    [network_id](const s_provisional_member& member) {
                        return member.network_id == network_id;
                    }),
                state->provisional_members.end());
        }

        s_provisional_member* Find_Provisional_Member(
            s_state* state, uint32_t network_id) {
            const auto member = std::find_if(
                state->provisional_members.begin(),
                state->provisional_members.end(),
                [network_id](const s_provisional_member& value) {
                    return value.network_id == network_id;
                });
            return member == state->provisional_members.end()
                ? nullptr
                : &*member;
        }

        bool Remove_Member(
            s_state* state, uint32_t network_id, const char* reason) {
            s_provisional_member* member =
                Find_Provisional_Member(state, network_id);
            if (member == nullptr) {
                return false;
            }
            if (reason != nullptr && reason[0] != '\0') {
                member->removal_reason.assign(reason,
                    strnlen(reason, ROUTER_MEMBER_REMOVAL_REASON_SIZE - 1));
            }
            const bool sent = network::router::Router_Client_Remove_Member(
                &state->client,
                network_id,
                member->removal_reason.c_str(),
                state->next_request_id++);
            if (sent) {
                member->removal_requested = true;
                member->deadline_ms = SDL_GetTicks() +
                    state->config.admission_timeout_ms;
            } else {
                member->deadline_ms = SDL_GetTicks() + 1000;
            }
            return sent;
        }

        void Expire_Provisional_Members(s_state* state) {
            const uint64_t now = SDL_GetTicks();
            std::vector<uint32_t> expired;
            for (const s_provisional_member& member :
                state->provisional_members) {
                if (now >= member.deadline_ms) {
                    expired.push_back(member.network_id);
                }
            }
            for (uint32_t network_id : expired) {
                s_provisional_member* member =
                    Find_Provisional_Member(state, network_id);
                if (member != nullptr && !member->removal_requested) {
                    SDL_Log("Network member %u admission timed out",
                        (unsigned)network_id);
                    member->removal_reason =
                        "Timed out verifying game content with the host.";
                }
                Remove_Member(state, network_id, nullptr);
            }
        }

        void Reset_Browser_From_Config(s_state* state) {
            network::router::s_router_session_browser_config browser_config;
            browser_config.mode = state->config.bootstrap_mode == BOOTSTRAP_LAN_DISCOVERY ? network::router::ROUTER_SESSION_BROWSER_LAN_DISCOVERY : network::router::ROUTER_SESSION_BROWSER_DIRECT_ENDPOINT;
            network::router::Router_Copy_String(browser_config.host, sizeof(browser_config.host), state->config.host);
            browser_config.tcp_port = state->config.tcp_port;
            browser_config.discovery_port = state->config.discovery_port;
            browser_config.discovery_timeout_ms = state->config.discovery_timeout_ms;
            network::router::Router_Copy_String(browser_config.client_name, sizeof(browser_config.client_name), state->config.client_name);
            network::router::Router_Session_Browser_Init(&state->browser, &browser_config);
        }

        void Reset_Client_From_Config(s_state* state) {
            network::router::s_router_client_config client_config;
            Copy_Config_To_Client(&state->config, &client_config);
            network::router::Router_Client_Init(&state->client, &client_config);
        }

        bool Connect_Client(s_state* state) {
            if (!network::router::Router_Client_Connect(&state->client)) {
                state->connect_failure_phase = CONNECT_FAILURE_TCP_CONNECT;
                return false;
            }
            if (!network::router::Router_Client_Send_Hello(
                    &state->client,
                    state->config.client_name,
                    ROUTER_VERSION,
                    state->next_request_id++)) {
                state->connect_failure_phase = CONNECT_FAILURE_HELLO_SEND;
                return false;
            }
            return true;
        }

        bool Connect_Client_To_Endpoint(s_state* state, const network::router::s_router_endpoint* endpoint) {
            if (state == nullptr || endpoint == nullptr) {
                return false;
            }

            network::router::Router_Client_Disconnect(&state->client);
            Reset_Traffic(state);
            network::router::Router_Copy_String(state->config.host, sizeof(state->config.host), endpoint->host);
            state->config.tcp_port = endpoint->tcp_port;
            Reset_Client_From_Config(state);
            return Connect_Client(state);
        }

        int Embedded_Router_Thread(void* data) {
            s_state* state = static_cast<s_state*>(data);
            int result = network::router::Router_Service_Run(&state->embedded_router_service);
            SDL_SetAtomicInt(&state->embedded_router_result, result);
            return result;
        }

        void Stop_Embedded_Router(s_state* state) {
            if (state == nullptr || state->embedded_router_thread == nullptr) {
                return;
            }

            network::router::Router_Service_Request_Stop(&state->embedded_router_service);
            SDL_WaitThread(state->embedded_router_thread, nullptr);
            state->embedded_router_thread = nullptr;
            if (SDL_GetAtomicInt(&state->embedded_router_result) != 0) {
                SDL_Log("Embedded router stopped with result %d", SDL_GetAtomicInt(&state->embedded_router_result));
            }
        }

        bool Start_Embedded_Router(s_state* state) {
            if (state == nullptr) {
                return false;
            }
            if (state->embedded_router_thread != nullptr) {
                return true;
            }

            network::router::s_router_config service_config;
            network::router::Router_Copy_String(service_config.listen_host, sizeof(service_config.listen_host), "0.0.0.0");
            service_config.tcp_port = state->config.tcp_port;
            service_config.discovery_port = state->config.discovery_port;
            service_config.enable_discovery = true;
            service_config.enable_signal_handlers = false;
            service_config.verbose_logging = false;
            network::router::Router_Copy_String(service_config.router_id, sizeof(service_config.router_id), "embedded-router");
            network::router::Router_Copy_String(service_config.router_name, sizeof(service_config.router_name), "Orion2 Embedded Router");

            network::router::Router_Service_Init(&state->embedded_router_service, &service_config);
            SDL_SetAtomicInt(&state->embedded_router_result, 0);
            state->embedded_router_thread = SDL_CreateThread(Embedded_Router_Thread, "orion2-router", state);
            if (state->embedded_router_thread == nullptr) {
                SDL_Log("Embedded router thread creation failed: %s", SDL_GetError());
                return false;
            }

            uint32_t start_timeout_ms = state->config.discovery_timeout_ms;
            if (start_timeout_ms < 1000) {
                start_timeout_ms = 1000;
            }

            uint64_t start_time = SDL_GetTicks();
            while ((uint32_t)(SDL_GetTicks() - start_time) < start_timeout_ms) {
                if (network::router::Router_Service_Is_Running(&state->embedded_router_service) &&
                    network::router::Router_Service_Bound_Tcp_Port(&state->embedded_router_service) != 0) {
                    state->config.tcp_port = network::router::Router_Service_Bound_Tcp_Port(&state->embedded_router_service);
                    network::router::Router_Copy_String(state->config.host, sizeof(state->config.host), "127.0.0.1");
                    SDL_Log("Embedded router started on 127.0.0.1:%u", static_cast<unsigned>(state->config.tcp_port));
                    Reset_Browser_From_Config(state);
                    return true;
                }
                if (!network::router::Router_Service_Is_Running(&state->embedded_router_service) && SDL_GetAtomicInt(&state->embedded_router_result) != 0) {
                    break;
                }
                SDL_Delay(1);
            }

            SDL_Log("Embedded router failed to start, result=%d", SDL_GetAtomicInt(&state->embedded_router_result));
            Stop_Embedded_Router(state);
            return false;
        }

    } // namespace

    s_adapter::s_adapter()
        : state(std::make_unique<s_state>()) {
    }

    s_adapter::~s_adapter() {
        Shutdown(this);
    }

    void Init(s_adapter* adapter, const s_config* config) {
        if (adapter == nullptr) {
            return;
        }
        if (adapter->state == nullptr) {
            adapter->state = std::make_unique<s_state>();
        }
        Shutdown(adapter);
        adapter->state->config = config != nullptr ? *config : s_config{};
        adapter->state->packets.clear();
        adapter->state->events.clear();
        adapter->state->next_request_id = 1;
        adapter->state->session_list_revision = 0;
        adapter->state->connect_failure_phase = CONNECT_FAILURE_NONE;
        adapter->state->accepted_members.clear();
        adapter->state->provisional_members.clear();
        Reset_Traffic(adapter->state.get());
        SDL_SetAtomicInt(&adapter->state->embedded_router_result, 0);

        Reset_Client_From_Config(adapter->state.get());
        Reset_Browser_From_Config(adapter->state.get());
    }

    bool Connect(s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return false;
        }
        adapter->state->connect_failure_phase = CONNECT_FAILURE_NONE;

        if (adapter->state->config.bootstrap_mode == BOOTSTRAP_EMBEDDED_LAN_HOST) {
            if (!Start_Embedded_Router(adapter->state.get())) {
                adapter->state->connect_failure_phase = CONNECT_FAILURE_TCP_CONNECT;
                return false;
            }
            Reset_Client_From_Config(adapter->state.get());
        } else if (adapter->state->config.bootstrap_mode == BOOTSTRAP_LAN_DISCOVERY) {
            return true;
        }

        return Connect_Client(adapter->state.get());
    }

    void Shutdown(s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return;
        }
        network::router::Router_Client_Disconnect(&adapter->state->client);
        network::router::Router_Session_Browser_Shutdown(&adapter->state->browser);
        Stop_Embedded_Router(adapter->state.get());
        adapter->state->packets.clear();
        adapter->state->events.clear();
        adapter->state->accepted_members.clear();
        adapter->state->provisional_members.clear();
        Reset_Traffic(adapter->state.get());
    }

    void Pump(s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return;
        }

        network::router::Router_Client_Pump(&adapter->state->client);
        network::router::Router_Session_Browser_Pump(&adapter->state->browser);
        adapter->state->session_list_revision = network::router::Router_Session_Browser_Revision(&adapter->state->browser);

        network::router::s_router_client_event client_event;
        while (network::router::Router_Client_Pop_Event(&adapter->state->client, &client_event)) {
            switch (client_event.kind) {
                case network::router::ROUTER_CLIENT_EVENT_GAME_DATA: {
                    s_game_packet packet;
                    if (Decode_Game_Packet(&client_event.message, &packet)) {
                        Account_Aggregate_Traffic(
                            adapter->state.get(), packet.payload.size(), false);
                        Account_Traffic(adapter->state.get(),
                            packet.sender_network_id,
                            packet.payload.size(),
                            false);
                        adapter->state->packets.push_back(std::move(packet));
                    } else {
                        s_event event;
                        event.kind = EVENT_ERROR;
                        event.error.code = network::router::ROUTER_ERROR_INVALID_MESSAGE;
                        network::router::Router_Copy_String(event.error.message, sizeof(event.error.message), "invalid game packet envelope");
                        adapter->state->events.push_back(std::move(event));
                    }
                    break;
                }
                case network::router::ROUTER_CLIENT_EVENT_MEMBER_JOINED: {
                    if (Session_Owner_Network_Id(adapter) ==
                        Local_Network_Id(adapter)) {
                        Remove_Provisional_Member(adapter->state.get(),
                            client_event.member_event.member.network_id);
                        adapter->state->provisional_members.push_back(
                            {client_event.member_event.member.network_id,
                                SDL_GetTicks() +
                                    adapter->state->config.admission_timeout_ms,
                                false,
                                {}});
                    }
                    s_event event;
                    event.kind = EVENT_MEMBER_JOINED;
                    event.network_id = client_event.member_event.member.network_id;
                    adapter->state->events.push_back(std::move(event));
                    break;
                }
                case network::router::ROUTER_CLIENT_EVENT_MEMBER_LEFT: {
                    Remove_Provisional_Member(adapter->state.get(),
                        client_event.member_event.member.network_id);
                    const bool accepted_member = Is_Accepted_Member(
                        adapter->state.get(), client_event.member_event.member);
                    adapter->state->accepted_members.erase(
                        std::remove(adapter->state->accepted_members.begin(),
                            adapter->state->accepted_members.end(),
                            client_event.member_event.member.network_id),
                        adapter->state->accepted_members.end());
                    s_event event;
                    event.kind = EVENT_MEMBER_LEFT;
                    event.network_id = client_event.member_event.member.network_id;
                    event.accepted_member = accepted_member;
                    adapter->state->events.push_back(std::move(event));
                    break;
                }
                case network::router::ROUTER_CLIENT_EVENT_LEFT_SESSION: {
                    adapter->state->accepted_members.clear();
                    adapter->state->provisional_members.clear();
                    s_event event;
                    event.kind = EVENT_SESSION_LEFT;
                    event.reason = client_event.reason;
                    adapter->state->events.push_back(std::move(event));
                    break;
                }
                case network::router::ROUTER_CLIENT_EVENT_SESSION_CLOSED: {
                    adapter->state->accepted_members.clear();
                    adapter->state->provisional_members.clear();
                    s_event event;
                    event.kind = EVENT_SESSION_CLOSED;
                    adapter->state->events.push_back(std::move(event));
                    break;
                }
                case network::router::ROUTER_CLIENT_EVENT_DISCONNECTED: {
                    adapter->state->accepted_members.clear();
                    adapter->state->provisional_members.clear();
                    s_event event;
                    event.kind = EVENT_DISCONNECTED;
                    adapter->state->events.push_back(std::move(event));
                    break;
                }
                case network::router::ROUTER_CLIENT_EVENT_ERROR:
                case network::router::ROUTER_CLIENT_EVENT_PROTOCOL_ERROR: {
                    s_event event;
                    event.kind = EVENT_ERROR;
                    event.error = client_event.error;
                    event.header = client_event.message.header;
                    adapter->state->events.push_back(std::move(event));
                    break;
                }
                default:
                    break;
            }
        }
        Expire_Provisional_Members(adapter->state.get());
    }

    bool Is_Connected(const s_adapter* adapter) {
        return adapter != nullptr && adapter->state != nullptr && network::router::Router_Client_Is_Connected(&adapter->state->client);
    }

    int Last_Error(const s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return 0;
        }
        return network::router::Router_Client_Last_Error(&adapter->state->client);
    }

    const char* Last_Error_Message(const s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return "no adapter";
        }
        return network::router::Router_Client_Last_Error_Message(&adapter->state->client);
    }

    uint32_t Local_Network_Id(const s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return 0;
        }
        return network::router::Router_Client_Local_Network_Id(&adapter->state->client);
    }

    uint32_t Session_Id(const s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return 0;
        }
        return network::router::Router_Client_Session_Id(&adapter->state->client);
    }

    bool Has_Pending_Client_Events(const s_adapter* adapter) {
        return adapter != nullptr &&
            adapter->state != nullptr &&
            network::router::Router_Client_Event_Count(&adapter->state->client) != 0;
    }

    bool Get_Traffic_Stats(const s_adapter* adapter, s_traffic_stats* stats) {
        if (adapter == nullptr || adapter->state == nullptr || stats == nullptr) {
            return false;
        }
        const uint64_t now_ms = SDL_GetTicks();
        const uint64_t elapsed_ms = now_ms >= adapter->state->traffic_started_ms
            ? now_ms - adapter->state->traffic_started_ms
            : 0;
        const uint64_t recent_elapsed_ms = std::min<uint64_t>(
            elapsed_ms, k_recent_window_seconds * 1000);
        const s_traffic_counters recent_sent = Recent_Counters(
            adapter->state->traffic_buckets, now_ms, true);
        const s_traffic_counters recent_received = Recent_Counters(
            adapter->state->traffic_buckets, now_ms, false);

        *stats = {};
        stats->started_ms = adapter->state->traffic_started_ms;
        stats->sampled_ms = now_ms;
        stats->recent_window_ms = k_recent_window_seconds * 1000;
        stats->sent = adapter->state->traffic_sent;
        stats->received = adapter->state->traffic_received;
        stats->average_sent_kbps = Rate_Kbps(stats->sent.payload_bytes, elapsed_ms);
        stats->average_received_kbps = Rate_Kbps(stats->received.payload_bytes, elapsed_ms);
        stats->recent_sent_kbps = Rate_Kbps(recent_sent.payload_bytes, recent_elapsed_ms);
        stats->recent_received_kbps = Rate_Kbps(recent_received.payload_bytes, recent_elapsed_ms);
        stats->transport = network::router::Router_Client_Stats(&adapter->state->client);
        stats->peers.reserve(adapter->state->peer_traffic.size());
        for (const s_peer_traffic_state& peer : adapter->state->peer_traffic) {
            s_peer_traffic_stats result;
            const s_traffic_counters peer_recent_sent = Recent_Counters(
                peer.buckets, now_ms, true);
            const s_traffic_counters peer_recent_received = Recent_Counters(
                peer.buckets, now_ms, false);
            result.network_id = peer.network_id;
            result.sent = peer.sent;
            result.received = peer.received;
            result.average_sent_kbps = Rate_Kbps(peer.sent.payload_bytes, elapsed_ms);
            result.average_received_kbps = Rate_Kbps(peer.received.payload_bytes, elapsed_ms);
            result.recent_sent_kbps = Rate_Kbps(
                peer_recent_sent.payload_bytes, recent_elapsed_ms);
            result.recent_received_kbps = Rate_Kbps(
                peer_recent_received.payload_bytes, recent_elapsed_ms);
            stats->peers.push_back(result);
        }
        return true;
    }

    e_connect_failure_phase Connect_Failure_Phase(const s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return CONNECT_FAILURE_NONE;
        }
        return adapter->state->connect_failure_phase;
    }

    bool Refresh_Game_List(s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return false;
        }
        return network::router::Router_Session_Browser_Refresh(&adapter->state->browser);
    }

    uint32_t Game_List_Revision(const s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return 0;
        }
        return adapter->state->session_list_revision;
    }

    int32_t Game_Count(const s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return 0;
        }
        return (int32_t)network::router::Router_Session_Browser_Sessions(&adapter->state->browser).size();
    }

    const char* Game_Name(const s_adapter* adapter, uint32_t session_index) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return nullptr;
        }
        const std::vector<network::router::s_router_session_record>& sessions = network::router::Router_Session_Browser_Sessions(&adapter->state->browser);
        if (session_index >= sessions.size()) {
            return nullptr;
        }
        return sessions[session_index].session.session_name;
    }

    uint32_t Game_Session_Id(const s_adapter* adapter, uint32_t session_index) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return 0;
        }
        const std::vector<network::router::s_router_session_record>& sessions = network::router::Router_Session_Browser_Sessions(&adapter->state->browser);
        if (session_index >= sessions.size()) {
            return 0;
        }
        return sessions[session_index].session.session_id;
    }

    bool Create_Game(s_adapter* adapter, const char* name, int32_t max_players, const char* player_name) {
        if (adapter == nullptr || adapter->state == nullptr || max_players <= 0) {
            return false;
        }

        network::router::s_router_session_registration_payload registration;
        const char* session_name = (name != nullptr && name[0] != '\0') ? name : "Game";
        network::router::Router_Copy_String(registration.session_name, sizeof(registration.session_name), session_name);
        network::router::Router_Copy_String(registration.host_name, sizeof(registration.host_name), player_name != nullptr ? player_name : adapter->state->config.client_name);
        network::router::Router_Copy_String(registration.build_string, sizeof(registration.build_string), adapter->state->config.build_string);
        registration.max_players = (uint16_t)max_players;
        return network::router::Router_Client_Register_Session(&adapter->state->client, &registration, adapter->state->next_request_id++);
    }

    bool Join_Game(s_adapter* adapter, uint32_t session_id, const char* player_name) {
        (void)player_name;
        if (adapter == nullptr || adapter->state == nullptr || session_id == 0) {
            return false;
        }

        const std::vector<network::router::s_router_session_record>& sessions = network::router::Router_Session_Browser_Sessions(&adapter->state->browser);
        const network::router::s_router_session_record* record = nullptr;
        for (const network::router::s_router_session_record& candidate : sessions) {
            if (candidate.session.session_id == session_id) {
                record = &candidate;
                break;
            }
        }
        if (record == nullptr) {
            return false;
        }

        if (!record->session.open) {
            return false;
        }

        if (!network::router::Router_Client_Is_Connected(&adapter->state->client) ||
            std::strcmp(record->router.host, adapter->state->config.host) != 0 ||
            record->router.tcp_port != adapter->state->config.tcp_port) {
            if (!Connect_Client_To_Endpoint(adapter->state.get(), &record->router)) {
                return false;
            }
        }

        return network::router::Router_Client_Join_Session(
            &adapter->state->client,
            session_id,
            adapter->state->config.build_string,
            adapter->state->next_request_id++);
    }

    bool Leave_Game(s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr ||
            Session_Id(adapter) == 0) {
            return false;
        }
        adapter->state->accepted_members.clear();
        adapter->state->provisional_members.clear();
        return network::router::Router_Client_Leave_Session(
            &adapter->state->client, adapter->state->next_request_id++);
    }

    bool Set_New_Players_Enabled(s_adapter* adapter, bool enabled) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return false;
        }
        const bool sent = network::router::Router_Client_Set_Session_Open(
            &adapter->state->client,
            Session_Id(adapter),
            enabled,
            adapter->state->next_request_id++);
        if (!enabled) {
            std::vector<uint32_t> provisional;
            for (const s_provisional_member& member :
                adapter->state->provisional_members) {
                provisional.push_back(member.network_id);
            }
            for (uint32_t network_id : provisional) {
                Remove_Member(adapter->state.get(),
                    network_id,
                    "The host has already started the game.");
            }
        }
        return sent;
    }

    bool Accept_Member(s_adapter* adapter, uint32_t network_id) {
        if (adapter == nullptr || adapter->state == nullptr || network_id == 0) {
            return false;
        }
        const auto& members =
            network::router::Router_Client_Members(&adapter->state->client);
        const auto member = std::find_if(members.begin(),
            members.end(),
            [network_id](const network::router::s_router_member_info& value) {
                return value.network_id == network_id;
            });
        if (member == members.end()) {
            return false;
        }
        if (std::find(adapter->state->accepted_members.begin(),
                adapter->state->accepted_members.end(),
                network_id) == adapter->state->accepted_members.end()) {
            adapter->state->accepted_members.push_back(network_id);
        }
        Remove_Provisional_Member(adapter->state.get(), network_id);
        return true;
    }

    bool Reject_Member(
        s_adapter* adapter, uint32_t network_id, const char* reason) {
        if (adapter == nullptr || adapter->state == nullptr ||
            network_id == 0 ||
            Session_Owner_Network_Id(adapter) != Local_Network_Id(adapter)) {
            return false;
        }
        return Remove_Member(adapter->state.get(), network_id, reason);
    }

    bool Is_Member_Provisional(
        const s_adapter* adapter, uint32_t network_id) {
        if (adapter == nullptr || adapter->state == nullptr ||
            network_id == 0) {
            return false;
        }
        return std::any_of(adapter->state->provisional_members.begin(),
            adapter->state->provisional_members.end(),
            [network_id](const s_provisional_member& member) {
                return member.network_id == network_id;
            });
    }

    bool Is_Member_Accepted(const s_adapter* adapter, uint32_t network_id) {
        if (adapter == nullptr || adapter->state == nullptr || network_id == 0) {
            return false;
        }
        for (const network::router::s_router_member_info& member :
            network::router::Router_Client_Members(&adapter->state->client)) {
            if (member.network_id == network_id) {
                return Is_Accepted_Member(adapter->state.get(), member);
            }
        }
        return false;
    }

    int32_t Get_Users(const s_adapter* adapter, uint32_t* users, int32_t max_users) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return 0;
        }
        const std::vector<network::router::s_router_member_info>& members = network::router::Router_Client_Members(&adapter->state->client);
        int32_t accepted_count = 0;
        for (const network::router::s_router_member_info& member : members) {
            if (!Is_Accepted_Member(adapter->state.get(), member)) {
                continue;
            }
            if (users != nullptr && accepted_count < max_users) {
                users[accepted_count] = member.network_id;
            }
            ++accepted_count;
        }
        return accepted_count;
    }

    uint32_t Session_Owner_Network_Id(const s_adapter* adapter) {
        if (adapter == nullptr || adapter->state == nullptr) {
            return 0;
        }
        for (const network::router::s_router_member_info& member :
            network::router::Router_Client_Members(&adapter->state->client)) {
            if (member.owner) {
                return member.network_id;
            }
        }
        return 0;
    }

    bool Send_Game_Message(s_adapter* adapter, uint32_t target_network_id, uint16_t message_type, const void* payload, uint32_t payload_size) {
        if (adapter == nullptr || adapter->state == nullptr || (payload == nullptr && payload_size != 0)) {
            return false;
        }

        network::router::Bit_Stream envelope;
        Write_Uint16(&envelope, message_type);
        Write_Uint32(&envelope, payload_size);
        envelope.Write_Bytes((const uint8_t*)payload, payload_size);

        bool sent = false;
        bool delivered = false;
        if (target_network_id == NETADAPTER_BROADCAST_EXCLUDING_SELF || target_network_id == NETADAPTER_BROADCAST_INCLUDING_SELF) {
            sent = true;
            const uint32_t local_network_id = Local_Network_Id(adapter);
            for (const network::router::s_router_member_info& member :
                network::router::Router_Client_Members(&adapter->state->client)) {
                if (!Is_Accepted_Member(adapter->state.get(), member) ||
                    (target_network_id == NETADAPTER_BROADCAST_EXCLUDING_SELF &&
                        member.network_id == local_network_id)) {
                    continue;
                }
                if (!network::router::Router_Client_Send_To(
                        &adapter->state->client,
                        member.network_id,
                        envelope.Data(),
                        envelope.Size(),
                        adapter->state->next_request_id++)) {
                    sent = false;
                    break;
                }
                Account_Traffic(adapter->state.get(),
                    member.network_id,
                    payload_size,
                    true);
                delivered = true;
            }
        } else {
            sent = network::router::Router_Client_Send_To(
                &adapter->state->client,
                target_network_id,
                envelope.Data(),
                envelope.Size(),
                adapter->state->next_request_id++);
            if (sent) {
                Account_Traffic(adapter->state.get(), target_network_id, payload_size, true);
                delivered = true;
            }
        }
        if (sent && delivered) {
            Account_Aggregate_Traffic(adapter->state.get(), payload_size, true);
        }
        return sent;
    }

    bool Poll_Game_Packet(s_adapter* adapter, s_game_packet* packet) {
        if (adapter == nullptr || adapter->state == nullptr || packet == nullptr || adapter->state->packets.empty()) {
            return false;
        }
        *packet = std::move(adapter->state->packets.front());
        adapter->state->packets.pop_front();
        return true;
    }

    bool Poll_Event(s_adapter* adapter, s_event* event) {
        if (adapter == nullptr || adapter->state == nullptr || event == nullptr || adapter->state->events.empty()) {
            return false;
        }
        *event = adapter->state->events.front();
        adapter->state->events.pop_front();
        return true;
    }

} // namespace netadapter