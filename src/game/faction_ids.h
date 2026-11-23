#pragma once

#include <cstdint>
#include "consts.h"
#include "object_ids.h"

namespace factions {
    inline constexpr game::id_faction From_Legacy(int code) { return {code}; }
    inline constexpr int Legacy_Code(game::id_faction faction) { return faction.code; }
    inline constexpr game::id_player Player(game::id_faction faction) {
        return faction.code >= 0 && faction.code < MAX_PLAYERS
            ? game::id_player{static_cast<uint32_t>(faction.code)} : game::NO_PLAYER;
    }
    inline constexpr game::id_faction From_Player(game::id_player player) {
        return player.id < MAX_PLAYERS ? game::id_faction{static_cast<int32_t>(player.id)} : game::id_faction{-1};
    }
} // namespace factions
