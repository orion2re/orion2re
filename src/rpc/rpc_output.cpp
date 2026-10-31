#include "rpc_output.h"

#include <utility>

#include "rpc_protocol.h"

namespace rpc {
    bool Queue_Output(s_output_queue* queue, std::string json, const std::string* state_key, size_t* global_bytes) {
        if (queue == nullptr || global_bytes == nullptr || json.empty()) return false;
        const std::string key = state_key != nullptr ? *state_key : std::string();
        size_t replaced_bytes = 0;
        size_t replaced_count = 0;
        for (auto item = queue->messages.begin(); item != queue->messages.end(); ++item) {
            if (!key.empty() && item->state_key == key && (item != queue->messages.begin() || queue->offset == 0)) {
                replaced_bytes += item->json.size();
                ++replaced_count;
            }
        }
        const s_protocol_limits* limits = Protocol_Limits();
        if (queue->messages.size() - replaced_count >= limits->output_messages_per_connection ||
            json.size() > limits->output_bytes_per_connection - (queue->bytes - replaced_bytes) ||
            json.size() > limits->output_bytes_global - (*global_bytes - replaced_bytes)) return false;
        for (auto item = queue->messages.begin(); item != queue->messages.end();) {
            if (!key.empty() && item->state_key == key && (item != queue->messages.begin() || queue->offset == 0))
                item = queue->messages.erase(item);
            else
                ++item;
        }
        queue->bytes = queue->bytes - replaced_bytes + json.size();
        *global_bytes = *global_bytes - replaced_bytes + json.size();
        queue->messages.push_back({std::move(json), key});
        return true;
    }

    bool Advance_Output(s_output_queue* queue, size_t count, size_t* global_bytes) {
        if (queue == nullptr || global_bytes == nullptr || queue->messages.empty() ||
            count > queue->messages.front().json.size() - queue->offset) return false;
        queue->offset += count;
        if (queue->offset == queue->messages.front().json.size()) {
            const size_t size = queue->messages.front().json.size();
            queue->bytes -= size;
            *global_bytes -= size;
            queue->messages.pop_front();
            queue->offset = 0;
        }
        return true;
    }
} // namespace rpc
