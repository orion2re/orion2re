#pragma once

#include "object_ids.h"
#include "unaligned.h"

namespace game {
    enum class e_star_destination : uint8_t { star, antares, none };

    struct s_star_destination {
        e_star_destination kind = e_star_destination::none;
        unaligned::field<id_star> star_id = NO_STAR;

        bool operator==(const s_star_destination&) const = default;
    };
    static_assert(alignof(s_star_destination) == 1);
}
