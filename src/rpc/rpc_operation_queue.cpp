#include "rpc_operation_queue.h"
#include "rpc_read_queue.h"

namespace rpc {
    bool Enqueue_Operation(s_operation_queue* queue, const s_operation_request& request) {
        std::lock_guard lock(queue->mutex);
        if (queue->stopped || queue->pending.method != OP_NONE) return false;
        queue->pending = request;
        return true;
    }
    bool Take_Operation(s_operation_queue* queue, s_operation_request* request) {
        std::lock_guard lock(queue->mutex);
        if (queue->stopped || queue->executing || queue->pending.method == OP_NONE) return false;
        // Expiry and cancellation race this transition, never native execution.
        if (Monotonic_Ms() >= queue->pending.request.deadline_ms) return false;
        queue->executing = true;
        *request = queue->pending;
        return true;
    }
    void Finish_Operation(s_operation_queue* queue) {
        std::lock_guard lock(queue->mutex);
        queue->pending = {};
        queue->executing = false;
    }
    bool Cancel_Operation(s_operation_queue* queue, uint64_t connection, uint64_t sequence, s_operation_request* cancelled, bool stop, bool expired_only) {
        std::lock_guard lock(queue->mutex);
        if (stop) queue->stopped = true;
        if (queue->executing || queue->pending.method == OP_NONE ||
            (connection != 0 && queue->pending.request.connection != connection) ||
            (sequence != 0 && queue->pending.sequence != sequence) ||
            (expired_only && queue->pending.request.deadline_ms > Monotonic_Ms())) return false;
        *cancelled = std::move(queue->pending);
        queue->pending = {};
        return true;
    }
} // namespace rpc
