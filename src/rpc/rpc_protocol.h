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
    };
    struct s_session {
        bool authenticated = false;
        bool controller = false;
        bool close_after_output = false;
        uint8_t events = 0;
        uint64_t events_after = 0;
        uint64_t auth_epoch = 0;
    };
    enum e_read_method {
        READ_NONE,
        READ_VIEW,
        READ_UNIT,
    };
    struct s_read_request {
        e_read_method method = READ_NONE;
        std::string encoded_id;
        std::string battle_id;
        std::string unit_id;
        uint64_t connection = 0, ticket = 0, epoch = 0;
        uint64_t selection = 0;
        uint64_t deadline_ms = 0;
    };

    std::string Error_Response(std::string_view id, int code, const char* message);
    // Validation only; typed game requests leave through read, never execute here.
    bool Process_Request(std::string_view input, std::string_view key, bool control_available, s_session* session, std::string* response, bool* selection_updated = nullptr, s_read_request* read = nullptr);
} // namespace rpc
