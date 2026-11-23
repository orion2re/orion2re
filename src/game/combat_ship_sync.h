#pragma once

#include <set>
#include <vector>

#include "combat_fleets.h"
#include "ship_serialization.h"

namespace combat_ship_sync {
    struct s_update {
        int16_t sender_row;
        int16_t local_row = -1;
        s_ship_data ship;
    };

    struct s_batch {
        std::vector<s_update> updates;
        std::vector<bool> updated_rows;
        int16_t extent = 0;
        uint32_t next_key = 0;
    };

    inline bool Read_Batch(SERIAL::Reader_& reader, const game::s_game_state& state, s_batch& result) {
        const int16_t sender_extent = reader.Read_Int16();
        if (reader.Failed() || sender_extent < 0 || sender_extent > MAX_SHIPS ||
            state.ship_count < 0 || state.ship_count > MAX_SHIPS ||
            state.ship_count > std::ssize(state.ship_records)) return false;

        s_batch batch;
        batch.updated_rows.resize(MAX_SHIPS);
        batch.extent = state.ship_count;
        batch.next_key = state.next_ship_id;

        int16_t previous = -1;
        std::set<game::id_ship> received_keys;
        for (;;) {
            const auto sender_row = reader.Read_Int16();
            if (reader.Failed()) return false;
            if (sender_row == -1) break;
            if (sender_row <= previous || sender_row >= sender_extent) return false;
            previous = sender_row;

            s_update update{.sender_row = sender_row, .ship = {}};
            if (!SERIAL::Read_Ship_Packet(reader, update.ship, state) || update.ship.status == 5) return false;
            const auto key = update.ship.id;
            if (key == game::NO_SHIP || !received_keys.insert(key).second) return false;
            batch.next_key = std::max(batch.next_key, key.id + 1);
            update.local_row = game::Ship_Row(state, key);
            if (update.local_row >= 0) {
                if (batch.updated_rows[update.local_row]) return false;
                batch.updated_rows[update.local_row] = true;
            }
            batch.updates.push_back(update);
        }

        const auto free_row = [&](int row) {
            return !batch.updated_rows[row] && (row >= state.ship_count || state.ship_records[row].status == 5);
        };
        int16_t candidate = 0;
        for (auto& update : batch.updates) {
            if (update.local_row < 0) {
                if (free_row(update.sender_row)) {
                    update.local_row = update.sender_row;
                } else {
                    while (candidate < MAX_SHIPS && !free_row(candidate)) ++candidate;
                    if (candidate == MAX_SHIPS) return false;
                    update.local_row = candidate++;
                }
                batch.updated_rows[update.local_row] = true;
            }
            batch.extent = std::max<int16_t>(batch.extent, update.local_row + 1);
        }
        result = std::move(batch);
        return true;
    }

    inline void Apply_Batch(game::s_game_state& state, const s_batch& batch) {
        if (std::ssize(state.ship_records) < batch.extent) state.ship_records.resize(batch.extent);
        for (int row = state.ship_count; row < batch.extent; ++row) {
            if (!batch.updated_rows[row]) {
                state.ship_records.Replace_Row(row, {});
                state.ship_records[row].status = 5;
                state.ship_records[row].position.Set(game::NO_SHIP_LOCATION);
                state.ship_records[row].officer_index = -1;
            }
        }
        for (const auto& update : batch.updates) state.ship_records.Replace_Row(update.local_row, update.ship);
        state.ship_count = batch.extent;
        state.next_ship_id = batch.next_key;
    }

    inline game::s_combat_ship_order Participant_Order(const s_batch& batch, game::s_star_destination destination) {
        game::s_combat_ship_order order{.destination = destination};
        order.keys.reserve(batch.updates.size());
        for (const auto& update : batch.updates) {
            order.keys.push_back(update.ship.id);
            order.ranks.emplace(update.ship.id, update.sender_row);
        }
        return order;
    }

    inline std::vector<game::id_ship> Casualty_Keys(const game::s_game_state& state, const s_batch& batch,
        int combat_star_row, int attacker, int defender) {
        std::vector<game::id_ship> casualties;
        for (int16_t row = 0; row < state.ship_count; ++row) {
            const auto& ship = state.ship_records[row];
            if (ship.id != game::NO_SHIP && !batch.updated_rows[row] && (ship.owner == attacker || ship.owner == defender) &&
                ship.status != 5 && game::Ship_Orbit_Row(state, ship.position) == combat_star_row) {
                casualties.push_back(ship.id);
            }
        }
        return casualties;
    }
}
