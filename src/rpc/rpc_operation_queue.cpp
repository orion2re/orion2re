#include "rpc_operation_queue.h"
#include "rpc_read_queue.h"

namespace rpc {
    bool Enqueue_Operation(s_operation_queue* queue, const s_operation_request& request, int* error) {
        std::lock_guard lock(queue->mutex);
        if (error != nullptr) *error = -32006;
        if (queue->stopped) return false;
        if (Is_Input_Operation(request.method) && (queue->report_id.empty() || request.report_id != queue->report_id)) {
            if (error != nullptr) *error = -32004;
            return false;
        }
        if (queue->pending.method != OP_NONE) {
            if (request.method == OP_MANUAL || !Is_Input_Operation(request.method) || !queue->executing ||
                Is_Input_Operation(queue->pending.method) ||
                queue->pending.request.connection != request.request.connection ||
                queue->acknowledgement.method != OP_NONE) return false;
            queue->acknowledgement = request;
            queue->acknowledgement.child = true;
            return true;
        }
        queue->pending = request;
        return true;
    }
    void Set_Boarding_Report(s_operation_queue* queue, std::string report_id) {
        std::lock_guard lock(queue->mutex);
        queue->report_id = std::move(report_id);
    }
    bool Take_Operation(s_operation_queue* queue, s_operation_request* request, bool report_only) {
        std::lock_guard lock(queue->mutex);
        if (report_only && queue->acknowledgement.method != OP_NONE) {
            if (queue->stopped || queue->acknowledgement_executing || Monotonic_Ms() >= queue->acknowledgement.request.deadline_ms) return false;
            queue->acknowledgement_executing = true;
            *request = queue->acknowledgement;
            return true;
        }
        if (queue->stopped || queue->executing || queue->pending.method == OP_NONE) return false;
        if (report_only && !Is_Input_Operation(queue->pending.method)) return false;
        // Expiry and cancellation race this transition, never native execution.
        if (Monotonic_Ms() >= queue->pending.request.deadline_ms) return false;
        queue->executing = true;
        *request = queue->pending;
        return true;
    }
    void Finish_Operation(s_operation_queue* queue, bool child) {
        std::lock_guard lock(queue->mutex);
        if (child) {
            queue->acknowledgement = {};
            queue->acknowledgement_executing = false;
            return;
        }
        queue->pending = {};
        queue->executing = false;
    }
    bool Cancel_Operation(s_operation_queue* queue, uint64_t connection, uint64_t sequence, s_operation_request* cancelled, bool stop, bool expired_only) {
        std::lock_guard lock(queue->mutex);
        if (stop) queue->stopped = true;
        auto& child = queue->acknowledgement;
        if (!queue->acknowledgement_executing && child.method != OP_NONE &&
            (connection == 0 || child.request.connection == connection) &&
            (sequence == 0 || child.sequence == sequence) &&
            (!expired_only || child.request.deadline_ms <= Monotonic_Ms())) {
            *cancelled = std::move(child);
            child = {};
            return true;
        }
        if (queue->executing || queue->pending.method == OP_NONE ||
            (connection != 0 && queue->pending.request.connection != connection) ||
            (sequence != 0 && queue->pending.sequence != sequence) ||
            (expired_only && queue->pending.request.deadline_ms > Monotonic_Ms())) return false;
        *cancelled = std::move(queue->pending);
        queue->pending = {};
        return true;
    }
} // namespace rpc
