#pragma once

#include "game_state.h"
#include "unaligned.h"

namespace game {
    enum class e_star_destination : uint8_t { star, antares, none };

    struct s_star_destination {
        e_star_destination kind = e_star_destination::none;
        unaligned::field<id_star> star_id = NO_STAR;

        static s_star_destination At_Position(int16_t position, const s_game_state& state = current_state) {
            if (position == ANTARES_POSITION) return {e_star_destination::antares, NO_STAR};
            const auto key = state.Star_Id_At_Position(position);
            return {key == NO_STAR ? e_star_destination::none : e_star_destination::star, key};
        }

        int16_t Position(const s_game_state& state = current_state) const {
            if (kind == e_star_destination::antares) return ANTARES_POSITION;
            return kind == e_star_destination::star ? state.Star_Position(star_id.get()) : -1;
        }

        bool operator==(const s_star_destination&) const = default;
    };
    static_assert(alignof(s_star_destination) == 1);
}
