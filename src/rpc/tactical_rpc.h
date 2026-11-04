#pragma once

#include "rpc_service.h"
#include "game/tactical_view.h"
#include "game/tactical_control.h"
#include <array>
#include <vector>

namespace rpc {
    struct s_tactical_forwarder {
        s_service* service = nullptr;
        uint64_t subscription = 0;
        int16_t viewer = -1;
        char battle_id[33] = {};
        tactical_view::s_reader reader;
        bool (*options)(tactical_control::s_options*) = nullptr;
        bool (*authorize)(int16_t, bool) = nullptr;
        bool (*missile)(int16_t, tactical_view::s_missile*) = nullptr;
        bool (*missiles)(std::vector<tactical_view::s_missile>*) = nullptr;
        bool (*authorize_missile)(int16_t) = nullptr;
        uint64_t decision_generation = 0;
        std::string decision_id;
        s_operation_request executing;
        s_operation_request acknowledgement;
        std::string report_id;
        uint64_t report_generation = 0;
        struct s_reference {
            int16_t slot = -1;
            std::array<char, 33> token = {};
        };
        std::vector<s_reference> references;
        std::vector<s_reference> missile_references;
        std::string selected;
        bool retiring = false;
    };
    bool Subscribe_Tactical_Events(s_tactical_forwarder* forwarder, s_service* service);
    void Unsubscribe_Tactical_Events(s_tactical_forwarder* forwarder);
} // namespace rpc
