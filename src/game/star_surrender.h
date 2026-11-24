#pragma once

#include "consts.h"
#include "object_ids.h"

namespace game {
    struct star_surrender_order {
        enum class kind : uint8_t { ordered, none, proposed, unrecognized };

        // Zero-initialized star records historically carry an order to player 0.
        id_player recipient{0};
        kind state = kind::ordered;
        int8_t unrecognized_byte = 0;

        static constexpr star_surrender_order None() { return {NO_PLAYER, kind::none}; }
        static constexpr star_surrender_order To(id_player player) { return {player, kind::ordered}; }
        static constexpr star_surrender_order Proposal(id_player player) { return {player, kind::proposed}; }

        constexpr bool Is_None() const { return state == kind::none; }
        constexpr id_player Ordered_Recipient() const { return state == kind::ordered ? recipient : NO_PLAYER; }
        constexpr id_player Proposed_Recipient() const { return state == kind::proposed ? recipient : NO_PLAYER; }
        constexpr bool Needs_Review() const {
            // The AI also used to clear unrecognized positive proposal bytes.
            return state == kind::proposed || (state == kind::unrecognized && unrecognized_byte >= MAX_PLAYERS);
        }

        bool operator==(const star_surrender_order&) const = default;
    };
}
