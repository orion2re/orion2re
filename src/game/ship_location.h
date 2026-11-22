#pragma once

#include <cstdint>
#include "object_ids.h"
#include "game_state.h"
#include "unaligned.h"

namespace game {
    enum class e_ship_location : uint8_t {
        at_star,
        in_flight,
        in_wormhole,
    };

    enum class e_ship_target : uint8_t {
        star,
        antares,
        none,
    };

    struct s_ship_location {
        e_ship_location kind;
        // Current star in orbit; destination star while travelling.
        id_star star_id;
        e_ship_target target = e_ship_target::star;

        bool operator==(const s_ship_location&) const = default;
    };

    inline constexpr s_ship_location NO_SHIP_LOCATION{e_ship_location::at_star, NO_STAR, e_ship_target::none};

    // Packed ship records store the same three fields without native padding.
    struct s_stored_ship_location {
        e_ship_location kind{};
        e_ship_target target{};
        unaligned::field<id_star> star_id;

        s_ship_location Get() const { return {kind, star_id.get(), target}; }
        int16_t Target_Position(int16_t antares, const s_game_state& state = current_state) const {
            if (target == e_ship_target::antares) return antares;
            return target == e_ship_target::star ? state.Star_Position(star_id.get()) : -1;
        }
        int16_t Orbit_Position(int16_t antares, const s_game_state& state = current_state) const {
            return kind == e_ship_location::at_star ? Target_Position(antares, state) : -1;
        }
        void Set_Orbit_At_Position(int16_t position, int16_t antares, const s_game_state& state = current_state) {
            if (position >= 0 && position == antares) {
                Set({e_ship_location::at_star, NO_STAR, e_ship_target::antares});
            } else {
                const auto id = state.Star_Id_At_Position(position);
                Set(id == NO_STAR ? NO_SHIP_LOCATION : s_ship_location{e_ship_location::at_star, id});
            }
        }
        int16_t Legacy_Target(int16_t antares) const {
            if (target == e_ship_target::antares) return antares;
            const auto id = star_id.get().id;
            return target == e_ship_target::star && id <= INT16_MAX ? static_cast<int16_t>(id) : -1;
        }
        int16_t Legacy_Orbit(int16_t antares) const {
            return kind == e_ship_location::at_star ? Legacy_Target(antares) : -1;
        }
        void Set_Orbit(int16_t slot, int16_t antares) {
            if (slot < 0) Set(NO_SHIP_LOCATION);
            else if (slot == antares) Set({e_ship_location::at_star, NO_STAR, e_ship_target::antares});
            else Set({e_ship_location::at_star, id_star{static_cast<uint32_t>(slot)}});
        }
        bool operator==(const s_stored_ship_location&) const = default;
        void Set(const s_ship_location& location) {
            kind = location.kind;
            target = location.target;
            star_id = location.star_id;
        }
    };
    static_assert(alignof(s_stored_ship_location) == 1);
    static_assert(sizeof(s_stored_ship_location) == 6);
} // namespace game
