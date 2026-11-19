#pragma once

#include <cstdint>
#include "object_ids.h"

namespace game {
    enum class e_ship_location : uint8_t {
        at_star,
        in_flight,
        in_wormhole,
    };

    enum class e_ship_destination : uint8_t {
        star = 0, // Preserve the destination of a zero-filled legacy ship record.
        none,
        antares,
    };

    struct s_ship_destination {
        constexpr s_ship_destination() = default;

        static constexpr s_ship_destination None() { return {}; }
        static constexpr s_ship_destination Star(id_star id) {
            return id == NO_STAR ? None() : s_ship_destination(e_ship_destination::star, id);
        }
        static constexpr s_ship_destination Antares() {
            return s_ship_destination(e_ship_destination::antares, NO_STAR);
        }

        constexpr e_ship_destination Kind() const { return kind_; }
        constexpr id_star Star_Id_Or_None() const { return star_; }
        bool operator==(const s_ship_destination&) const = default;

    private:
        constexpr s_ship_destination(e_ship_destination kind, id_star star) : star_(star), kind_(kind) {}

        id_star star_ = NO_STAR;
        e_ship_destination kind_ = e_ship_destination::none;
    };

    struct s_ship_location {
        e_ship_location kind = e_ship_location::at_star;
        // Current star in orbit; destination star while travelling.
        s_ship_destination destination;

        constexpr s_ship_location() = default;
        constexpr s_ship_location(e_ship_location mode, s_ship_destination target) : kind(mode), destination(target) {}
        constexpr s_ship_location(e_ship_location mode, id_star star) : s_ship_location(mode, s_ship_destination::Star(star)) {}

        bool operator==(const s_ship_location&) const = default;
    };
} // namespace game
