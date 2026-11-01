#include "rpc_output.h"

#include <utility>

#include "rpc_protocol.h"

namespace rpc {
    bool Queue_Output(s_output_queue* queue, std::string json, const std::string* state_key, size_t* global_bytes, uint64_t context_epoch, std::string revoked_json, uint64_t ticket) {
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
        queue->messages.push_back({std::move(json), key, context_epoch, std::move(revoked_json), ticket});
        return true;
    }

    bool Revoke_Context_Output(s_output_queue* queue, uint64_t current_epoch, size_t* global_bytes) {
        if (queue == nullptr || global_bytes == nullptr) return false;
        for (auto item = queue->messages.begin(); item != queue->messages.end();) {
            if (item->context_epoch == 0 || item->context_epoch == current_epoch) {
                ++item;
                continue;
            }
            if (item == queue->messages.begin() && queue->offset != 0) return false;
            queue->bytes -= item->json.size();
            *global_bytes -= item->json.size();
            if (item->revoked_json.empty()) {
                item = queue->messages.erase(item);
            } else {
                item->json = std::move(item->revoked_json);
                item->context_epoch = 0;
                item->state_key.clear();
                queue->bytes += item->json.size();
                *global_bytes += item->json.size();
                ++item;
            }
        }
        return queue->bytes <= Protocol_Limits()->output_bytes_per_connection &&
            *global_bytes <= Protocol_Limits()->output_bytes_global;
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
