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
    bool Publish_Event(s_service* service, uint8_t kind, std::string_view battle_id, std::string_view unit_id = {}, std::string detail = {});
    void Revoke_Event_Delivery(s_service* service);
    void Invalidate_Read_Delivery(s_service* service);
    void Set_Decision_Generation(s_service* service, uint64_t generation);
    using t_execute_read = std::string (*)(const s_read_request*, void*, bool* dependent);
    // Game thread only. Unsafe points leave observed reads queued until deadline.
    void Execute_Reads(s_service* service, bool safe, bool observed, t_execute_read execute, void* context);
    bool Take_Operation(s_service* service, s_operation_request* request, bool report_only = false);
    void Complete_Operation(s_service* service, const s_operation_request* request, int error = 0, std::string boarding = {});
    void Close_Boarding_Report(s_service* service);
    void Open_Boarding_Report(s_service* service, std::string report_id);
} // namespace rpc
