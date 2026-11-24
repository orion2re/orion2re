#pragma once

#include "star_surrender.h"

namespace SERIAL {
    inline constexpr game::star_surrender_order Decode_Star_Surrender(int8_t encoded) {
        using order = game::star_surrender_order;
        if (encoded == -1) return order::None();
        if (encoded >= 0 && encoded < MAX_PLAYERS) return order::To(game::id_player{static_cast<uint32_t>(encoded)});
        if (encoded >= MAX_PLAYERS && encoded < 2 * MAX_PLAYERS) {
            return order::Proposal(game::id_player{static_cast<uint32_t>(encoded - MAX_PLAYERS)});
        }
        return {game::NO_PLAYER, order::kind::unrecognized, encoded};
    }

    inline constexpr int8_t Encode_Star_Surrender(game::star_surrender_order order) {
        using kind = game::star_surrender_order::kind;
        switch (order.state) {
            case kind::none: return -1;
            case kind::ordered: return static_cast<int8_t>(order.recipient.id);
            case kind::proposed: return static_cast<int8_t>(MAX_PLAYERS + order.recipient.id);
            case kind::unrecognized: return order.unrecognized_byte;
        }
        return -1;
    }
}
