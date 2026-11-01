#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "rpc_protocol.h"

namespace rpc {
    struct s_work {
        uint64_t client = 0;
        uint64_t epoch = 0;
        uint64_t ticket = 0;
        uint64_t deadline_ms = 0;
        s_request request;
    };

    struct s_publication {
        uint64_t client = 0;
        uint64_t epoch = 0;
        uint64_t ticket = 0;
        std::string json;
        std::string state_key; // Nonempty only for replaceable state invalidations.
        uint64_t context_epoch = 0;
        std::string revoked_json; // A response cannot silently disappear on context loss.
    };

    struct s_bridge_ticket {
        uint64_t client;
        uint64_t ticket;
        bool started;
    };

    struct s_client_identity {
        uint64_t client;
        uint64_t epoch;
    };

    struct s_context_delivery {
        uint64_t client;
        uint64_t ticket;
    };

    struct s_bridge {
        std::mutex mutex;
        uint64_t epoch = 1;
        uint64_t context_epoch = 1;
        uint64_t next_ticket = 0;
        bool stopped = false;
        uint64_t clients[4] = {};
        bool authorized[4] = {};
        std::vector<s_bridge_ticket> tickets;
        std::deque<s_work> pending;
        std::deque<s_publication> ready;
        size_t ready_bytes = 0;
        std::vector<s_context_delivery> deliveries;
    };

    bool Register_Client(s_bridge* bridge, uint64_t client, uint64_t* epoch);
    bool Client_Is_Current(s_bridge* bridge, uint64_t client, uint64_t epoch);
    bool Authorize_Client(s_bridge* bridge, uint64_t client, uint64_t epoch);
    void Get_Authorized_Clients(s_bridge* bridge, std::vector<s_client_identity>* clients);
    bool Has_Clients(s_bridge* bridge);
    bool Has_Pending_Work(s_bridge* bridge);
    void Cancel_Client(s_bridge* bridge, uint64_t client);
    void Revoke_Bridge(s_bridge* bridge);
    uint64_t Bridge_Context_Epoch(s_bridge* bridge);
    void Revoke_Bridge_Context(s_bridge* bridge);
    bool Context_Response_Pending(s_bridge* bridge, uint64_t client, uint64_t ticket);
    void Acknowledge_Response(s_bridge* bridge, uint64_t client, uint64_t ticket);
    void Stop_Bridge(s_bridge* bridge);
    int32_t Submit_Work(s_bridge* bridge, uint64_t client, uint64_t epoch, const s_request* request, uint64_t deadline_ms);
    bool Take_Work(s_bridge* bridge, uint64_t now_ms, s_work* work);
    void Expire_Work(s_bridge* bridge, uint64_t now_ms);
    bool Complete_Work(s_bridge* bridge, const s_work* work, const std::string* json, uint64_t context_epoch = 0);
    bool Publish_Event(s_bridge* bridge, uint64_t client, uint64_t epoch, const std::string* json, const std::string* state_key = nullptr, uint64_t context_epoch = 0);
    bool Take_Publication(s_bridge* bridge, s_publication* publication);
} // namespace rpc
