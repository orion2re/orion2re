#pragma once

#include <cstddef>
#include <cstdint>

#include "object_ids.h"

namespace SERIAL {
    inline constexpr size_t NARROW_SOURCE_SETTLER_BYTES = 13;
    inline constexpr size_t SETTLER_BYTES = 11;
}

struct s_settler_info {
    game::id_colony source_colony{};
    game::id_planet destination{};
    uint8_t race = 0;
    uint8_t eta = 0;
    uint8_t profession = 0;

    bool operator==(const s_settler_info&) const = default;
};
