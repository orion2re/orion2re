#pragma once

#include <deque>
#include <string>

namespace rpc {
    struct s_output_message {
        std::string json;
        std::string state_key;
    };

    // Service-thread-owned. The front message is immutable once offset is nonzero.
    struct s_output_queue {
        std::deque<s_output_message> messages;
        size_t bytes = 0;
        size_t offset = 0;
    };

    bool Queue_Output(s_output_queue* queue, std::string json, const std::string* state_key, size_t* global_bytes);
    bool Advance_Output(s_output_queue* queue, size_t count, size_t* global_bytes);
} // namespace rpc
