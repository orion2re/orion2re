#pragma once

#include "rpc_service.h"

namespace rpc {
    struct s_tactical_forwarder {
        s_service* service = nullptr;
        uint64_t subscription = 0;
        int16_t viewer = -1;
        char battle_id[33] = {};
    };
    bool Subscribe_Tactical_Events(s_tactical_forwarder* forwarder, s_service* service);
    void Unsubscribe_Tactical_Events(s_tactical_forwarder* forwarder);
} // namespace rpc
