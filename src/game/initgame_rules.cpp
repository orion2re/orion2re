#include "pch.h"

namespace INITGAME {

    int16_t Normalize_New_Game_Player_Count(
        int16_t requested_player_count,
        int16_t fallback_player_count) {
        if ((requested_player_count >= 2 && requested_player_count <= MAX_PLAYERS) ||
            requested_player_count == NEW_GAME_RANDOM_PLAYER_COUNT) {
            return requested_player_count;
        }
        if (fallback_player_count >= 2 && fallback_player_count <= MAX_PLAYERS) {
            return fallback_player_count;
        }
        return NEW_GAME_DEFAULT_PLAYER_COUNT;
    }

    int16_t Resolve_New_Game_Player_Count(
        int16_t requested_player_count,
        const s_player* players) {
        if (requested_player_count != NEW_GAME_RANDOM_PLAYER_COUNT) {
            return requested_player_count;
        }

        int16_t minimum_player_count = MAX_PLAYERS;
        for (int16_t player_idx = 1; player_idx < MAX_PLAYERS - 1; ++player_idx) {
            if (players[player_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                minimum_player_count = player_idx == 1 ? 2 : player_idx;
                break;
            }
        }

        return minimum_player_count +
            (int16_t)game_random::Random(MAX_PLAYERS - minimum_player_count + 1) - 1;
    }

} // namespace INITGAME