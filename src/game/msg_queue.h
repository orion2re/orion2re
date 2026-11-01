#pragma once

#include <cstdint>

struct s_MSG;

namespace MSG_QUEUE {
    /** Returns the index of the first lowest-priority message in the bounded span. */
    int16_t Lowest_Priority_Index(const s_MSG* messages, int16_t count);
    int16_t Insertion_Index(const s_MSG* messages, int16_t capacity, int16_t count, uint8_t priority);

    /** Appends to a bounded message span or replaces its first lowest-priority entry. */
    void Add(s_MSG* messages, int16_t capacity, uint8_t* count, s_MSG message);
} // namespace MSG_QUEUE