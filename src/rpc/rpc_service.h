#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "rpc_protocol.h"

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
    // Producer may run on the game thread. Unregister/join producers before Stop.
    bool Publish_Event(s_service* service, uint8_t kind, std::string_view battle_id, std::string_view unit_id = {});
    void Revoke_Event_Delivery(s_service* service);
    void Invalidate_Read_Delivery(s_service* service);
    using t_execute_read = std::string (*)(const s_read_request*, void*, bool* dependent);
    // Game thread only. Unsafe points leave observed reads queued until deadline.
    void Execute_Reads(s_service* service, bool safe, bool observed, t_execute_read execute, void* context);
} // namespace rpc
