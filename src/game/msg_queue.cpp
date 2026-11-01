#include "pch.h"

namespace MSG_QUEUE {
    int16_t Lowest_Priority_Index(const s_MSG* messages, int16_t count) {
        if (messages == nullptr || count <= 0) {
            return -1;
        }

        int16_t lowest_priority_idx = 0;
        for (int16_t idx = 1; idx < count; ++idx) {
            if (messages[idx].priority < messages[lowest_priority_idx].priority) {
                lowest_priority_idx = idx;
            }
        }

        return lowest_priority_idx;
    }

    int16_t Insertion_Index(const s_MSG* messages, int16_t capacity, int16_t count, uint8_t priority) {
        if (messages == nullptr || capacity <= 0) return -1;
        if (count < capacity) return count;
        const int16_t lowest = Lowest_Priority_Index(messages, capacity);
        return lowest >= 0 && priority > messages[lowest].priority ? lowest : -1;
    }

    void Add(s_MSG* messages, int16_t capacity, uint8_t* count, s_MSG message) {
        if (messages == nullptr || count == nullptr || capacity <= 0) {
            return;
        }

        if (*count > capacity) {
            *count = static_cast<uint8_t>(capacity);
        }

        const int16_t index = Insertion_Index(messages, capacity, *count, message.priority);
        if (index < 0) return;
        messages[index] = message;
        if (*count < capacity) ++*count;
    }
} // namespace MSG_QUEUE