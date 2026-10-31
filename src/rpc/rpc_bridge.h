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
    };

    struct s_bridge_ticket {
        uint64_t client;
        uint64_t ticket;
        bool started;
    };

    struct s_bridge {
        std::mutex mutex;
        uint64_t epoch = 1;
        uint64_t next_ticket = 0;
        bool stopped = false;
        uint64_t clients[4] = {};
        std::vector<s_bridge_ticket> tickets;
        std::deque<s_work> pending;
        std::deque<s_publication> ready;
        size_t ready_bytes = 0;
    };

    bool Register_Client(s_bridge* bridge, uint64_t client, uint64_t* epoch);
    bool Client_Is_Current(s_bridge* bridge, uint64_t client, uint64_t epoch);
    bool Has_Clients(s_bridge* bridge);
    void Cancel_Client(s_bridge* bridge, uint64_t client);
    void Revoke_Bridge(s_bridge* bridge);
    void Stop_Bridge(s_bridge* bridge);
    int32_t Submit_Work(s_bridge* bridge, uint64_t client, uint64_t epoch, const s_request* request, uint64_t deadline_ms);
    bool Take_Work(s_bridge* bridge, uint64_t now_ms, s_work* work);
    void Expire_Work(s_bridge* bridge, uint64_t now_ms);
    bool Complete_Work(s_bridge* bridge, const s_work* work, const std::string* json);
    bool Take_Publication(s_bridge* bridge, s_publication* publication);
} // namespace rpc
