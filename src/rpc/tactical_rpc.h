#pragma once

#include "rpc_service.h"
#include "game/tactical_view.h"
#include <array>
#include <vector>

namespace rpc {
    struct s_tactical_forwarder {
        s_service* service = nullptr;
        uint64_t subscription = 0;
        int16_t viewer = -1;
        char battle_id[33] = {};
        tactical_view::s_reader reader;
        struct s_reference {
            int16_t slot = -1;
            std::array<char, 33> token = {};
        };
        std::vector<s_reference> references;
        std::string selected;
        bool retiring = false;
    };
    bool Subscribe_Tactical_Events(s_tactical_forwarder* forwarder, s_service* service);
    void Unsubscribe_Tactical_Events(s_tactical_forwarder* forwarder);
} // namespace rpc
