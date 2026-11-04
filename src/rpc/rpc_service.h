#pragma once

#include <cstdint>
#include <string>

namespace rpc {
    struct s_service_config {
        uint16_t port = 17362;
        std::string key;
    };

    struct s_service {
        void* state = nullptr;
    };

    // Call lifecycle functions from one owner thread. Stop wakes and joins the
    // service independently of any future game poll. Port zero is for tests.
    bool Start_Service(s_service* service, const s_service_config* config, std::string* diagnostic);
    void Stop_Service(s_service* service);
    uint16_t Service_Port(const s_service* service);
} // namespace rpc
