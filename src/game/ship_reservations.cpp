#include "pch.h"

namespace ship_slots {
    void Allocate_Reservations(game::s_game_state& state, game::id_player local, int game_type,
        uint32_t stardate, const s_antaran& antarans, size_t saved_extent) {
        if (state.player_count <= 0 || state.player_count > MAX_PLAYERS || saved_extent > MAX_SHIPS) return;
        int antaran_slots = 0;
        for (int i = 0; i < 9; ++i) antaran_slots += antarans.offensive_ships[i];
        antaran_slots = antaran_slots > 20 ? 20 : std::max(antaran_slots, 0) + 2;

        int demand[MAX_PLAYERS] = {};
        for (const auto& [id, colony] : state.colonies) {
            if (colony.owner.id >= MAX_PLAYERS || colony.outpost_flag != 0) continue;
            for (const auto product : colony.producing) {
                if (!COLBLDG::Colony_Production_Is_Queued_Ship(product)) ++demand[colony.owner.id];
            }
        }
        const bool has_local = local.id < state.player_count;
        const int local_slots = !has_local ? 0 : game_type == 0 ? std::min(demand[local.id], 20) : demand[local.id];

        int16_t unassigned[MAX_SHIPS];
        int count = 0;
        int assigned[MAX_PLAYERS] = {};
        constexpr game::owner_slot antaran{NONPLAYER_SHIP_TYPE_ANTARAN};
        for (size_t slot = 0; slot < saved_extent; ++slot) {
            if (!Available(state, slot)) continue;
            const auto owner = Owner(state, slot);
            if (owner == antaran) --antaran_slots;
            else if (const auto player = factions::Player(owner); player.id < state.player_count) ++assigned[player.id];
        }
        for (auto slot = static_cast<int16_t>(saved_extent); slot < MAX_SHIPS; ++slot) {
            if (!Available(state, slot)) continue;
            if (antaran_slots > 0) {
                Reserve(state, slot, antaran);
                --antaran_slots;
            } else if (has_local && assigned[local.id] < local_slots) {
                Reserve(state, slot, factions::From_Player(local));
                ++assigned[local.id];
            } else {
                unassigned[count++] = slot;
            }
        }

        int player = stardate % state.player_count;
        int next = 0;
        for (int skipped = 0; next < count && skipped < state.player_count;) {
            if (assigned[player] < demand[player]) {
                Reserve(state, unassigned[next++], factions::From_Player(state.players[player].id));
                ++assigned[player];
                skipped = 0;
            } else {
                ++skipped;
            }
            player = (player + 1) % state.player_count;
        }
        for (int skipped = 0; next < count && skipped < state.player_count;) {
            if (state.players[player].eliminated == 0) {
                Reserve(state, unassigned[next++], factions::From_Player(state.players[player].id));
                skipped = 0;
            } else {
                ++skipped;
            }
            player = (player + 1) % state.player_count;
        }
    }
}
