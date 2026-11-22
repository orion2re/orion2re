#pragma once

#include <array>
#include <map>
#include <set>
#include <type_traits>
#include <vector>

#include "colony_serialization.h"
#include "ship_serialization.h"

namespace combat_ship_sync {
    inline constexpr size_t COLONY_FIELDS_BYTES = SERIAL::COLONY_FIELDS_BYTES + 8 * sizeof(uint32_t);

    // The combat batch can omit queued ships and remap survivors to local rows.
    inline void Write_Colony_Fields(SERIAL::Writer_& writer, const game::s_game_state& state, const s_colony& colony) {
        auto fields = colony;
        std::array<game::id_ship, 8> keys;
        const auto project = [&](game::colony_product& product) {
            auto key = game::Queued_Ship_Key(product);
            const auto row = game::Legacy_Queued_Ship_Row(product);
            if (row >= 0) {
                if (const auto* ship = game::Find_Legacy_Ship_At_Row(state, row)) key = ship->id;
            }
            if (key != game::NO_SHIP || row >= 0) product = COLONY_PRODUCTION_NONE;
            return key;
        };
        static_assert(std::extent_v<decltype(s_colony::producing)> == 7);
        for (size_t i = 0; i < 7; ++i) keys[i] = project(fields.producing[i]);
        keys.back() = project(fields.just_produced);
        SERIAL::Write_Colony_Fields(writer, fields);
        for (const auto key : keys) writer.Write_Uint32(key.id);
    }

    inline bool Read_Colony_With_Id(SERIAL::Reader_& reader, s_colony& colony, game::id_colony id) {
        if (!reader.Require(COLONY_FIELDS_BYTES)) return false;
        s_colony decoded{};
        SERIAL::Read_Colony_With_Id(reader, decoded, id);
        const auto bind = [&](game::colony_product& product) {
            const game::id_ship key{reader.Read_Uint32()};
            if (game::Legacy_Queued_Ship_Row(product) >= 0) return false;
            if (key != game::NO_SHIP) {
                if (product != COLONY_PRODUCTION_NONE) return false;
                product = game::Queued_Ship_Product(key);
            }
            return true;
        };
        for (auto& product : decoded.producing) {
            if (!bind(product)) return false;
        }
        if (!bind(decoded.just_produced) || reader.Failed()) return false;
        colony = decoded;
        return true;
    }

    enum class e_batch_kind { participants, survivors };
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

    inline bool Read_Batch(SERIAL::Reader_& reader, const game::s_game_state& state,
        e_batch_kind kind, s_batch& result) {
        const int16_t sender_extent = reader.Read_Int16();
        if (reader.Failed() || sender_extent < 0 || sender_extent > MAX_SHIPS ||
            state.ship_count < 0 || state.ship_count > MAX_SHIPS ||
            state.ship_count > std::ssize(state.ship_records)) return false;

        s_batch batch;
        batch.updated_rows.resize(MAX_SHIPS);
        const bool participants = kind == e_batch_kind::participants;
        batch.extent = participants ? sender_extent : state.ship_count;
        batch.next_key = state.next_ship_id;
        std::map<game::id_ship, int16_t> rows;
        for (int16_t row = 0; !participants && row < state.ship_count; ++row) {
            const auto key = state.ship_records[row].id;
            if (key != game::NO_SHIP && !rows.emplace(key, row).second) return false;
        }

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
            if (key != game::NO_SHIP && !received_keys.insert(key).second) return false;
            if (key != game::NO_SHIP) batch.next_key = std::max(batch.next_key, key.id + 1);
            if (participants) {
                update.local_row = sender_row;
            } else if (key == game::NO_SHIP) {
                if (sender_row < state.ship_count && state.ship_records[sender_row].id != game::NO_SHIP) return false;
                update.local_row = sender_row;
            } else {
                const auto existing = rows.find(key);
                if (existing != rows.end()) update.local_row = existing->second;
            }
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

    inline void Apply_Batch(game::s_game_state& state, const s_batch& batch, bool participants_only) {
        if (participants_only) state.ship_records.assign(state.ship_records.size(), s_ship_data{});
        if (std::ssize(state.ship_records) < batch.extent) state.ship_records.resize(batch.extent);
        for (int row = 0; row < batch.extent; ++row) {
            if (!batch.updated_rows[row] && (participants_only || row >= state.ship_count)) {
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

    inline std::vector<int16_t> Casualty_Rows(const game::s_game_state& state, const s_batch& batch,
        int combat_star_row, int attacker, int defender) {
        std::vector<int16_t> casualties;
        for (int16_t row = 0; row < state.ship_count; ++row) {
            const auto& ship = state.ship_records[row];
            if (!batch.updated_rows[row] && (ship.owner == attacker || ship.owner == defender) &&
                ship.status != 5 && game::Ship_Orbit_Row(state, ship.position) == combat_star_row) {
                casualties.push_back(row);
            }
        }
        return casualties;
    }
}
