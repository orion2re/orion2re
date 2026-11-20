#pragma once

#include <cstring>
#include <type_traits>

namespace unaligned {
    // Native-endian object access, not wire-format decoding. The caller must
    // provide sizeof(T) readable/writable bytes; no alignment is required.
    template <typename T>
    inline T Load(const void* address) {
        static_assert(std::is_trivially_copyable_v<T>);
        T value;
        std::memcpy(&value, address, sizeof(value));
        return value;
    }

    template <typename T>
    inline void Store(void* address, T value) {
        static_assert(std::is_trivially_copyable_v<T>);
        std::memcpy(address, &value, sizeof(value));
    }

    template <typename T>
    struct field {
        field() = default;
        field(T value) { Store(bytes, value); }
        T get() const { return Load<T>(bytes); }
        bool operator==(const field&) const = default;

      private:
        unsigned char bytes[sizeof(T)]{};
    };
} // namespace unaligned