#pragma once

#include <cstddef>
#include <cstdint>

#include "object_ids.h"
#include "unaligned.h"

namespace SERIAL {
    inline constexpr size_t NARROW_SOURCE_SETTLER_BYTES = 13;
    inline constexpr size_t SETTLER_BYTES = 15;
}

struct s_settler_info {
    // Player records are still packed; these fields do not require aligned storage.
    unaligned::field<game::id_colony> source_colony;
    unaligned::field<game::id_planet> destination;
    uint8_t race = 0;
    uint8_t eta = 0;
    uint8_t profession = 0;
    unaligned::field<uint32_t> legacy_flags;

    bool operator==(const s_settler_info&) const = default;
};
