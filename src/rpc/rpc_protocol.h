#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace rpc {
    constexpr size_t MAX_INPUT_BYTES = 65536;
    constexpr size_t MAX_RESPONSE_BYTES = 16 * 1024 * 1024;
    constexpr size_t MAX_OUTPUT_BYTES = 32 * 1024 * 1024;
    constexpr size_t MAX_OUTPUT_MESSAGES = 16;
    constexpr size_t MAX_CONNECTIONS = 4;
    constexpr unsigned AUTHENTICATION_DEADLINE_MS = 5000;
    constexpr unsigned READ_DEADLINE_MS = 2000;

    enum e_event : uint8_t {
        EVENT_COMBAT_OPENED = 1,
        EVENT_COMBAT_CLOSED = 2,
        EVENT_COMBAT_INVALIDATED = 4,
        EVENT_COMBAT_SELECTED = 8,
        EVENT_COMBAT_ATTENTION = 16,
        EVENT_COMBAT_ATTENTION_CLEARED = 32,
        EVENT_COMBAT_BOARDING = 64,
        EVENT_COMBAT_RETREAT = 128,
    };
    struct s_session {
        bool authenticated = false;
        bool controller = false;
        bool close_after_output = false;
        uint8_t events = 0;
        uint64_t events_after = 0;
        uint64_t auth_epoch = 0;
        uint64_t operation_sequence = 0;
    };
    enum e_read_method {
        READ_NONE,
        READ_VIEW,
        READ_UNIT,
        READ_OPTIONS,
        READ_MISSILES,
        READ_MISSILE,
        READ_HITS,
    };

    struct s_read_request {
        e_read_method method = READ_NONE;
        std::string encoded_id;
        std::string battle_id;
        std::string unit_id;
        std::string missile_id;
        uint64_t connection = 0, ticket = 0, epoch = 0;
        uint64_t selection = 0;
        uint64_t deadline_ms = 0;
    };
    enum e_operation_method {
        OP_NONE,
        OP_SELECT,
        OP_MOVE,
        OP_FACE,
        OP_FIRE,
        OP_DONE,
        OP_CANCEL,
        OP_FIRE_SELECTION,
        OP_BOARD,
        OP_BOARDING_ACK,
        OP_RETREAT,
        OP_WAIT,
    };
    struct s_operation_request {
        s_read_request request;
        e_operation_method method = OP_NONE;
        uint64_t sequence = 0;
        std::string actor_id, target_id, decision_id;
        int16_t x = 0, y = 0;
        bool missile_target = false;
        uint8_t bank_mask = 0;
        int8_t bank_states[8] = {};
        int16_t boarding_mode = 0, marines = 0;
        std::string report_id;
        bool child = false;
    };

    std::string Error_Response(std::string_view id, int code, const char* message);
    // Validation only; typed game requests leave through read, never execute here.
    bool Process_Request(std::string_view input, std::string_view key, bool control_available, s_session* session, std::string* response, bool* selection_updated = nullptr, s_read_request* read = nullptr, s_operation_request* operation = nullptr);
} // namespace rpc
