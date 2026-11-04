#include "rpc_read_queue.h"

#include <chrono>
#include <limits>

namespace rpc {
    uint64_t Monotonic_Ms() {
        return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    bool Enqueue_Read(s_read_queue* queue, s_read_request request) {
        std::lock_guard lock(queue->mutex);
        if (queue->stopped || queue->next_ticket == std::numeric_limits<uint64_t>::max()) return false;
        for (auto& slot : queue->slots) {
            if (slot.request.ticket != 0) continue;
            request.ticket = ++queue->next_ticket;
            slot = {std::move(request), false};
            return true;
        }
        return false;
    }

    bool Take_Read(s_read_queue* queue, s_read_request* request) {
        std::lock_guard lock(queue->mutex);
        s_read_slot* first = nullptr;
        for (auto& slot : queue->slots) {
            if (slot.request.ticket != 0 && !slot.executing && (first == nullptr || slot.request.ticket < first->request.ticket)) first = &slot;
        }
        if (first == nullptr || queue->stopped) return false;
        first->executing = true;
        *request = first->request;
        return true;
    }

    bool Finish_Read(s_read_queue* queue, uint64_t ticket) {
        std::lock_guard lock(queue->mutex);
        for (auto& slot : queue->slots) {
            if (slot.request.ticket != ticket || ticket == 0) continue;
            slot = {};
            return true;
        }
        return false;
    }

    void Cancel_Reads(s_read_queue* queue, uint64_t connection, bool stop, std::vector<s_read_request>* cancelled) {
        std::lock_guard lock(queue->mutex);
        for (auto& slot : queue->slots) {
            if (slot.request.ticket == 0 || (connection != 0 && slot.request.connection != connection)) continue;
            if (cancelled != nullptr) cancelled->push_back(std::move(slot.request));
            slot = {};
        }
        if (stop) queue->stopped = true;
    }

    bool Expire_Read(s_read_queue* queue, s_read_request* request) {
        std::lock_guard lock(queue->mutex);
        const auto now = Monotonic_Ms();
        s_read_slot* first = nullptr;
        for (auto& slot : queue->slots) {
            if (slot.request.ticket == 0 || slot.request.deadline_ms > now) continue;
            if (first == nullptr || slot.request.ticket < first->request.ticket) first = &slot;
        }
        if (first == nullptr) return false;
        *request = std::move(first->request);
        *first = {};
        return true;
    }
} // namespace rpc
