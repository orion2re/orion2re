#pragma once

#include <cstdint>
#include "consts.h"
#include "object_ids.h"
#include "orion2_consts.h"

namespace factions {
    // Adapters for integer owner-slot APIs, not a separate faction roster.
    inline constexpr game::owner_slot From_Legacy(int slot) { return {slot}; }
    inline constexpr int Legacy_Code(game::owner_slot owner) { return owner.code; }
    inline constexpr bool Valid_Design_Owner(game::owner_slot owner) {
        return owner.code >= 0 && owner.code <= NONPLAYER_SHIP_TYPE_HYDRA;
    }
    inline constexpr game::id_player Player(game::owner_slot owner) {
        return owner.code >= 0 && owner.code < MAX_PLAYERS
            ? game::id_player{static_cast<uint32_t>(owner.code)} : game::NO_PLAYER;
    }
    inline constexpr game::owner_slot From_Player(game::id_player player) {
        return player.id < MAX_PLAYERS ? game::owner_slot{static_cast<int32_t>(player.id)} : game::NO_OWNER;
    }
} // namespace factions
