#pragma once

#include "game_state.h"

namespace player_roster {
    inline void Install(game::s_game_state& state, uint32_t slot, const s_player& player) {
        auto& destination = state.players.at(slot);
        destination = player;
        destination.id = game::id_player{slot};
    }

    inline bool Contains(const game::s_game_state& state, game::id_player id) {
        return state.player_count >= 0 && state.player_count <= MAX_PLAYERS &&
            id.id < static_cast<uint32_t>(state.player_count) && state.players[id.id].id == id;
    }

    inline const s_player* Find(const game::s_game_state& state, game::id_player id) {
        return Contains(state, id) ? &state.players[id.id] : nullptr;
    }

    inline s_player* Find(game::s_game_state& state, game::id_player id) {
        return Contains(state, id) ? &state.players[id.id] : nullptr;
    }
} // namespace player_roster
