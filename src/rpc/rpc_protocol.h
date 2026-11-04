#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace rpc {
    constexpr size_t MAX_INPUT_BYTES = 65536;
    constexpr size_t MAX_OUTPUT_BYTES = 65536;
    constexpr size_t MAX_OUTPUT_MESSAGES = 16;
    constexpr size_t MAX_CONNECTIONS = 4;
    constexpr unsigned AUTHENTICATION_DEADLINE_MS = 5000;

    enum e_event : uint8_t {
        EVENT_COMBAT_OPENED = 1,
        EVENT_COMBAT_CLOSED = 2,
    };
    struct s_session {
        bool authenticated = false;
        bool controller = false;
        bool close_after_output = false;
        uint8_t events = 0;
        uint64_t events_after = 0;
        uint64_t auth_epoch = 0;
    };

    // Transport-only methods. No game state may be read on the service thread.
    bool Process_Request(std::string_view input, std::string_view key, bool control_available, s_session* session, std::string* response, bool* selection_updated = nullptr);
} // namespace rpc
