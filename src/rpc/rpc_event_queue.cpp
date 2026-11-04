#include "rpc_event_queue.h"

#include <algorithm>
#include <limits>

namespace rpc {
    namespace {
        void Clear_Pending(s_event_queue* queue) {
            auto* batch = &queue->pending;
            queue->reserved_response_bytes -= batch->response_bytes;
            for (auto& item : batch->items) item = {};
            batch->count = 0;
            batch->response_bytes = 0;
        }
    } // namespace
    bool Push_Event(s_event_queue* queue, uint8_t kind, std::string_view battle_id, std::string_view unit_id) {
        if (queue == nullptr || (kind != EVENT_COMBAT_OPENED && kind != EVENT_COMBAT_CLOSED && kind != EVENT_COMBAT_INVALIDATED && kind != EVENT_COMBAT_SELECTED && kind != EVENT_COMBAT_ATTENTION && kind != EVENT_COMBAT_ATTENTION_CLEARED) || battle_id.size() != 32 ||
            (!unit_id.empty() && unit_id.size() != 32) ||
            !std::all_of(battle_id.begin(), battle_id.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) return false;
        std::lock_guard lock(queue->mutex);
        auto& batch = queue->pending;
        if (batch.position.stopped) return false;
        if (batch.position.sequence == std::numeric_limits<uint64_t>::max()) {
            batch.position.stopped = true;
            Clear_Pending(queue);
            return false;
        }
        const auto sequence = ++batch.position.sequence;
        // Only adjacent complete hints coalesce; never cross another delivery.
        if (kind == EVENT_COMBAT_INVALIDATED && batch.count != 0) {
            auto& last = batch.items[batch.count - 1];
            if (last.kind == kind && std::string_view(last.battle_id.data()) == battle_id) {
                last.sequence = sequence;
                return true;
            }
        }
        if (batch.count == MAX_PENDING_EVENTS) {
            batch.overflow_position = sequence;
            Clear_Pending(queue);
            return false;
        }
        auto& item = batch.items[batch.count++];
        item = {};
        item.sequence = sequence;
        item.kind = kind;
        std::copy(battle_id.begin(), battle_id.end(), item.battle_id.begin());
        std::copy(unit_id.begin(), unit_id.end(), item.unit_id.begin());
        if (kind == EVENT_COMBAT_SELECTED) item.read_epoch = batch.position.read_epoch;
        return true;
    }

    bool Push_Response(s_event_queue* queue, uint64_t connection, uint64_t epoch, uint64_t read_epoch, std::string response, uint64_t selection, std::string encoded_id) {
        std::lock_guard lock(queue->mutex);
        auto& batch = queue->pending;
        if (batch.position.stopped || batch.position.epoch != epoch) return false;
        if (batch.position.sequence == std::numeric_limits<uint64_t>::max()) {
            batch.position.stopped = true;
            return false;
        }
        const auto sequence = ++batch.position.sequence;
        if (batch.count == MAX_PENDING_EVENTS || response.size() > MAX_PENDING_RESPONSE_BYTES - queue->reserved_response_bytes) {
            Clear_Pending(queue);
            batch.overflow_position = sequence;
            return false;
        }
        batch.response_bytes += response.size();
        queue->reserved_response_bytes += response.size();
        auto& item = batch.items[batch.count++];
        item = {};
        item.sequence = sequence;
        item.connection = connection;
        item.selection = selection;
        item.encoded_id = std::move(encoded_id);
        item.read_epoch = read_epoch;
        item.response = std::move(response);
        return true;
    }

    void Invalidate_Read_Output(s_event_queue* queue) {
        std::lock_guard lock(queue->mutex);
        auto& position = queue->pending.position;
        if (position.read_epoch == std::numeric_limits<uint64_t>::max()) {
            position.stopped = true;
        } else {
            ++position.read_epoch;
        }
    }

    s_event_position Event_Position(s_event_queue* queue) {
        std::lock_guard lock(queue->mutex);
        return queue->pending.position;
    }

    s_event_batch Take_Events(s_event_queue* queue) {
        std::lock_guard lock(queue->mutex);
        auto batch = std::move(queue->pending);
        queue->pending = {};
        queue->pending.position = batch.position;
        queue->pending.count = 0;
        queue->pending.overflow_position = 0;
        return batch;
    }

    void Release_Event_Batch(s_event_queue* queue, const s_event_batch* batch) {
        std::lock_guard lock(queue->mutex);
        queue->reserved_response_bytes -= batch->response_bytes;
    }

    void Revoke_Events(s_event_queue* queue) {
        std::lock_guard lock(queue->mutex);
        auto& batch = queue->pending;
        Clear_Pending(queue);
        batch.overflow_position = 0;
        if (batch.position.epoch == std::numeric_limits<uint64_t>::max()) {
            batch.position.stopped = true;
        } else {
            ++batch.position.epoch;
        }
    }

    void Stop_Events(s_event_queue* queue) {
        std::lock_guard lock(queue->mutex);
        Clear_Pending(queue);
        queue->pending.position.stopped = true;
    }

    bool Event_Delivery_Expired(const s_session* session, const s_event_batch* batch) {
        return batch->position.stopped || (session->authenticated && session->auth_epoch != batch->position.epoch) ||
            (session->events != 0 && session->events_after < batch->overflow_position);
    }
} // namespace rpc
