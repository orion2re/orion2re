#include "pch.h"

#include "game/arena.h"

namespace arena {
    namespace {
        struct s_chunk {
            s_chunk* next;
            size_t capacity;
            size_t used;
            alignas(std::max_align_t) unsigned char data[1];
        };

        [[noreturn]] void Fail(const char* message) {
            exitm::Exit_With_Message(message);
        }

        bool Is_Power_Of_Two(size_t value) {
            return value != 0 && (value & (value - 1)) == 0;
        }

        size_t Align_Up(size_t value, size_t alignment) {
            if (value > std::numeric_limits<size_t>::max() - (alignment - 1)) {
                Fail("Arena allocation size overflow");
            }
            return (value + alignment - 1) & ~(alignment - 1);
        }

        void Add_Size(size_t* value, size_t increment, const char* message) {
            if (*value > std::numeric_limits<size_t>::max() - increment) {
                Fail(message);
            }
            *value += increment;
        }
    } // namespace

    struct s_arena {
        s_chunk* first;
        s_chunk* last;
        size_t chunk_capacity;
        size_t chunk_count;
        size_t used_bytes;
        size_t reserved_bytes;
        uint32_t reference_count;
    };

    static s_chunk* Allocate_Chunk(size_t capacity) {
        constexpr size_t header_size = offsetof(s_chunk, data);
        if (capacity == 0 || capacity > std::numeric_limits<size_t>::max() - header_size) {
            Fail("Arena chunk size overflow");
        }
        s_chunk* chunk = static_cast<s_chunk*>(std::malloc(header_size + capacity));
        if (chunk == nullptr) {
            Fail("Unable to allocate arena chunk");
        }
        chunk->next = nullptr;
        chunk->capacity = capacity;
        chunk->used = 0;
        return chunk;
    }

    s_arena* Create(size_t chunk_capacity) {
        if (chunk_capacity == 0) {
            Fail("Arena chunk capacity must be positive");
        }
        s_arena* result = static_cast<s_arena*>(std::calloc(1, sizeof(s_arena)));
        if (result == nullptr) {
            Fail("Unable to allocate arena");
        }
        result->chunk_capacity = chunk_capacity;
        result->reference_count = 1;
        return result;
    }

    void* Allocate_Aligned(s_arena* target, size_t size, size_t alignment) {
        if (target == nullptr || size == 0) {
            Fail("Invalid arena allocation");
        }
        if (!Is_Power_Of_Two(alignment) || alignment > alignof(std::max_align_t)) {
            Fail("Invalid arena allocation alignment");
        }

        size_t offset = target->last != nullptr ? Align_Up(target->last->used, alignment) : 0;
        if (target->last == nullptr || offset > target->last->capacity ||
            size > target->last->capacity - offset) {
            if (size > std::numeric_limits<size_t>::max() - (alignment - 1)) {
                Fail("Arena allocation size overflow");
            }
            const size_t capacity = std::max(target->chunk_capacity,
                size + alignment - 1);
            s_chunk* chunk = Allocate_Chunk(capacity);
            if (target->last != nullptr) {
                target->last->next = chunk;
            } else {
                target->first = chunk;
            }
            target->last = chunk;
            Add_Size(&target->chunk_count, 1, "Arena chunk count overflow");
            Add_Size(&target->reserved_bytes, capacity, "Arena reserved byte count overflow");
            offset = 0;
        }

        void* result = target->last->data + offset;
        target->last->used = offset + size;
        Add_Size(&target->used_bytes, size, "Arena used byte count overflow");
        return result;
    }

    void* Allocate(s_arena* target, size_t size) {
        return Allocate_Aligned(target, size, alignof(std::max_align_t));
    }

    void* Allocate_Zeroed_Aligned(s_arena* target, size_t size, size_t alignment) {
        void* result = Allocate_Aligned(target, size, alignment);
        std::memset(result, 0, size);
        return result;
    }

    void* Allocate_Zeroed(s_arena* target, size_t size) {
        return Allocate_Zeroed_Aligned(target, size, alignof(std::max_align_t));
    }

    char* Copy_String_N(s_arena* target, const char* source, size_t length) {
        if (source == nullptr || length == std::numeric_limits<size_t>::max()) {
            Fail("Invalid arena string copy");
        }
        char* result = static_cast<char*>(Allocate_Aligned(target, length + 1, 1));
        if (length != 0) {
            std::memcpy(result, source, length);
        }
        result[length] = '\0';
        return result;
    }

    char* Copy_String(s_arena* target, const char* source) {
        if (source == nullptr) {
            Fail("Invalid arena string copy");
        }
        return Copy_String_N(target, source, std::strlen(source));
    }

    bool Contains(const s_arena* target, const void* pointer) {
        if (target == nullptr || pointer == nullptr) {
            return false;
        }
        const uintptr_t address = (uintptr_t)pointer;
        for (const s_chunk* chunk = target->first; chunk != nullptr; chunk = chunk->next) {
            const uintptr_t begin = (uintptr_t)chunk->data;
            if (address >= begin && address < begin + chunk->used) {
                return true;
            }
        }
        return false;
    }

    void Add_Ref(s_arena* target) {
        if (target == nullptr || target->reference_count == UINT32_MAX) {
            Fail("Invalid arena reference");
        }
        ++target->reference_count;
    }

    uint32_t Release(s_arena* target) {
        if (target == nullptr || target->reference_count == 0) {
            Fail("Invalid arena release");
        }
        const uint32_t remaining = --target->reference_count;
        if (remaining != 0) {
            return remaining;
        }
        s_chunk* chunk = target->first;
        while (chunk != nullptr) {
            s_chunk* next = chunk->next;
            std::free(chunk);
            chunk = next;
        }
        std::free(target);
        return 0;
    }

    s_stats Get_Stats(const s_arena* target) {
        if (target == nullptr) {
            Fail("Invalid arena statistics request");
        }
        return {
            target->chunk_count,
            target->used_bytes,
            target->reserved_bytes,
            target->reference_count,
        };
    }
} // namespace arena