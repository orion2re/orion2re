#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace rpc {
    constexpr size_t MAX_INPUT_BYTES = 65536;
    constexpr size_t MAX_OUTPUT_BYTES = 65536;
    constexpr size_t MAX_OUTPUT_MESSAGES = 16;
    constexpr size_t MAX_CONNECTIONS = 4;
    constexpr unsigned AUTHENTICATION_DEADLINE_MS = 5000;

    struct s_session {
        bool authenticated = false;
        bool controller = false;
        bool close_after_output = false;
    };

    // Transport-only methods. No game state may be read on the service thread.
    bool Process_Request(std::string_view input, std::string_view key, bool control_available, s_session* session, std::string* response);
} // namespace rpc
