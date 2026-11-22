#pragma once

#include <cstddef>
#include <cstring>

namespace NETPROTO {
    // Bump for incompatible game packets, independently of save and router versions.
    inline constexpr char GAME_WIRE_VERSION[] = "orion2re-keyed/1";

    template <std::size_t N>
    void Write_Setup_Version(char (&field)[N]) {
        static_assert(N >= sizeof(GAME_WIRE_VERSION));
        std::memset(field, 0, N);
        std::memcpy(field, GAME_WIRE_VERSION, sizeof(GAME_WIRE_VERSION));
    }

    template <std::size_t N>
    bool Valid_Setup_Version(const char (&field)[N]) {
        static_assert(N >= sizeof(GAME_WIRE_VERSION));
        return std::memcmp(field, GAME_WIRE_VERSION, sizeof(GAME_WIRE_VERSION)) == 0;
    }
}
