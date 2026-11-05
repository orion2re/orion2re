#pragma once

#include "rpc_service.h"
#include "rpc_combat_control.h"
#include "rpc_tactical_view.h"
#include "game/cmbtdrw1.h"
#include <array>
#include <vector>

namespace rpc {
    struct s_tactical_forwarder {
        s_service* service = nullptr;
        uint64_t subscription = 0;
        int16_t viewer = -1;
        char battle_id[33] = {};
        s_reader reader;
        bool (*options)(s_options*) = nullptr;
        bool (*authorize)(int16_t, bool) = nullptr;
        COMBAT1::e_result (*validate)(const COMBAT1::s_operation*) = nullptr;
        bool (*missile)(int16_t, s_missile*) = nullptr;
        bool (*missiles)(std::vector<s_missile>*) = nullptr;
        bool (*authorize_missile)(int16_t) = nullptr;
        const CMBTDRW1::s_tactical_input* (*input)() = nullptr;
        bool (*scan)(int16_t, CMBTDRW1::s_combat_detail*) = nullptr;
        bool (*help)(int16_t, std::string*) = nullptr;
        bool (*condition)(int16_t, CMBTDRW1::s_damage_bars*) = nullptr;
        uint64_t decision_generation = 0;
        std::string decision_id;
        std::string manual_id;
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
        bool input_retired = false;
    };
    bool Subscribe_Tactical_Events(s_tactical_forwarder* forwarder, s_service* service);
    void Unsubscribe_Tactical_Events(s_tactical_forwarder* forwarder);
} // namespace rpc
