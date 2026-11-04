#include "rpc_event_queue.h"

#include <algorithm>
#include <limits>

namespace rpc {
    bool Push_Event(s_event_queue* queue, uint8_t kind, std::string_view battle_id) {
        if (queue == nullptr || (kind != EVENT_COMBAT_OPENED && kind != EVENT_COMBAT_CLOSED) || battle_id.size() != 32 ||
            !std::all_of(battle_id.begin(), battle_id.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) return false;
        std::lock_guard lock(queue->mutex);
        auto& batch = queue->pending;
        if (batch.position.stopped) return false;
        if (batch.position.sequence == std::numeric_limits<uint64_t>::max()) {
            batch.position.stopped = true;
            batch.count = 0;
            return false;
        }
        const auto sequence = ++batch.position.sequence;
        if (batch.count == MAX_PENDING_EVENTS) {
            batch.overflow_position = sequence;
            batch.count = 0;
            return false;
        }
        auto& item = batch.items[batch.count++];
        item = {};
        item.sequence = sequence;
        item.kind = kind;
        std::copy(battle_id.begin(), battle_id.end(), item.battle_id.begin());
        return true;
    }

    s_event_position Event_Position(s_event_queue* queue) {
        std::lock_guard lock(queue->mutex);
        return queue->pending.position;
    }

    s_event_batch Take_Events(s_event_queue* queue) {
        std::lock_guard lock(queue->mutex);
        auto batch = queue->pending;
        queue->pending.count = 0;
        queue->pending.overflow_position = 0;
        return batch;
    }

    void Revoke_Events(s_event_queue* queue) {
        std::lock_guard lock(queue->mutex);
        auto& batch = queue->pending;
        batch.count = 0;
        batch.overflow_position = 0;
        if (batch.position.epoch == std::numeric_limits<uint64_t>::max()) {
            batch.position.stopped = true;
        } else {
            ++batch.position.epoch;
        }
    }

    void Stop_Events(s_event_queue* queue) {
        std::lock_guard lock(queue->mutex);
        queue->pending.count = 0;
        queue->pending.position.stopped = true;
    }

    bool Event_Delivery_Expired(const s_session* session, const s_event_batch* batch) {
        return batch->position.stopped || (session->authenticated && session->auth_epoch != batch->position.epoch) ||
            (session->events != 0 && session->events_after < batch->overflow_position);
    }
} // namespace rpc
