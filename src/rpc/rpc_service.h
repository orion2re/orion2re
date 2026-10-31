#pragma once

#include <cstdint>
#include <string>

#include "rpc_bridge.h"

namespace rpc {
    struct s_service_config {
        uint16_t port = 17362;
        std::string key;
        s_bridge* bridge = nullptr;
    };

    struct s_service {
        // Owner-thread handle. Do not copy it or stop it from the service thread.
        void* state = nullptr;
    };

    bool Start_Service(s_service* service, const s_service_config* config, std::string* diagnostic);
    void Stop_Service(s_service* service);
    uint16_t Service_Port(const s_service* service);
} // namespace rpc
