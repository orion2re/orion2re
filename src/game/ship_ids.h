#pragma once

#include "game_state.h"
#include "legacy_ids.h"
#include "faction_ids.h"

namespace game {
    inline bool Is_Keyed_Ship_Notice(int row) {
        return row >= INT16_MIN && row < INT16_MIN + MAX_SHIPS;
    }

    template<typename Notice>
    inline void Set_Ship_Notice(Notice& notice, int16_t row, id_ship key) {
        notice.ship_id = key;
        notice.ship_idx = key == NO_SHIP ? row : static_cast<int16_t>(INT16_MIN + row);
    }

    // Network peers allocate in distinct lanes. An allocator advances its
    // high-water mark when it receives a record from another peer.
    inline id_ship Allocate_Ship_Id(s_game_state& state, std::optional<unsigned> peer = {}) {
        uint64_t next = state.next_ship_id;
        if (peer) {
            if (*peer >= MAX_PLAYERS) return NO_SHIP;
            next += (*peer + MAX_PLAYERS - next % MAX_PLAYERS) % MAX_PLAYERS;
        }
        if (next >= NO_SHIP.id) return NO_SHIP;
        state.next_ship_id = static_cast<uint32_t>(next + 1);
        return id_ship{static_cast<uint32_t>(next)};
    }

    [[deprecated("Legacy ship row projection; use Find_Ship for record access")]]
    inline int Ship_Row(const s_game_state& state, id_ship id) {
        const int row = state.ship_records.Row(id);
        return row < state.ship_count ? row : -1;
    }

    inline void Set_Leader_Ship(s_leader_data& leader, const s_ship_data& ship, int16_t legacy_row) {
        leader.Clear_Location();
        leader.ship_location = ship.id;
        if (leader.ship_location == NO_SHIP) leader.location = legacy_row;
    }

    inline void Import_Leader_Ship_Assignment(s_leader_data& leader, const s_ship_data& ship) {
        if (leader.type == LEADER_TYPE_SHIP && leader.status == 1 &&
            leader.ship_location == NO_SHIP && leader.star_location == NO_STAR && leader.location >= 0 &&
            ship.id != NO_SHIP && ship.status != 5 && leader.employer != NO_PLAYER && leader.employer == factions::Player(ship.owner) &&
            ship.officer_index >= 0 && static_cast<uint32_t>(ship.officer_index) == leader.id.id) {
            Set_Leader_Ship(leader, ship, -1);
        }
    }

    inline void Import_Leader_Ship_Assignments(s_game_state& state) {
        const auto& roster = std::as_const(state.ship_records);
        for (auto& leader : state.leaders) {
            const auto row = leader.location;
            if (row >= 0 && row < state.ship_count && row < std::ssize(roster)) {
                Import_Leader_Ship_Assignment(leader, roster[row]);
            }
        }
    }

    [[deprecated("Legacy ship row projection; retain the ship key instead of a row")]]
    inline id_ship Ship_Key_At_Row(const s_game_state& state, int row) {
        if (row < 0 || row >= state.ship_count || row >= std::ssize(state.ship_records)) return NO_SHIP;
        return state.ship_records[row].id;
    }

    inline const s_ship_data* Find_Legacy_Ship_At_Row(const s_game_state& state, int row) {
        if (row < 0 || row >= state.ship_count || row >= std::ssize(state.ship_records)) return nullptr;
        return &state.ship_records[row];
    }

    inline s_ship_data* Find_Ship(s_game_state& state, id_ship id) {
        return state.ship_records.Find(id, state.ship_count);
    }

    inline const s_ship_data* Find_Ship(const s_game_state& state, id_ship id) {
        return state.ship_records.Find(id, state.ship_count);
    }

    template<typename Notice>
    inline const s_ship_data* Find_Ship_For_Notice(const s_game_state& state, const Notice& notice) {
        if (Is_Keyed_Ship_Notice(notice.ship_idx)) return Find_Ship(state, notice.ship_id.get());
        return Find_Legacy_Ship_At_Row(state, notice.ship_idx);
    }

    inline const s_ship_data* Find_Leader_Ship(const s_game_state& state, const s_leader_data& leader) {
        if (leader.ship_location != NO_SHIP) return Find_Ship(state, leader.ship_location);
        if (leader.star_location != NO_STAR) return nullptr;
        const auto* ship = Find_Legacy_Ship_At_Row(state, leader.location);
        // Keyed historical assignments are bound at import. An unresolved row
        // must not become a reference to a later keyed occupant.
        return ship && ship->id == NO_SHIP ? ship : nullptr;
    }

    inline s_ship_data* Find_Leader_Ship(s_game_state& state, const s_leader_data& leader) {
        if (leader.ship_location != NO_SHIP) return Find_Ship(state, leader.ship_location);
        return Find_Leader_Ship(std::as_const(state), leader) ? &state.ship_records[leader.location] : nullptr;
    }

    template<class Predicate>
    inline bool Any_Ship(const s_game_state& state, Predicate matches) {
        for (const auto& [id, entry] : state.ship_records.Keyed_Records()) {
            if (entry.row < state.ship_count && matches(entry.ship)) return true;
        }
        for (const auto& [row, ship] : state.ship_records.Unkeyed_Rows()) {
            if (row >= static_cast<size_t>(std::max<int>(0, state.ship_count))) break;
            if (matches(ship)) return true;
        }
        return false;
    }

    inline const s_ship_data* Find_Queued_Ship(const s_game_state& state, colony_product product) {
        const auto key = Queued_Ship_Key(product);
        return key != NO_SHIP ? Find_Ship(state, key)
            : Find_Legacy_Ship_At_Row(state, Legacy_Queued_Ship_Row(product));
    }

    inline s_ship_data* Find_Queued_Ship(s_game_state& state, colony_product product) {
        const auto key = Queued_Ship_Key(product);
        if (key != NO_SHIP) return Find_Ship(state, key);
        const auto row = Legacy_Queued_Ship_Row(product);
        return Find_Legacy_Ship_At_Row(std::as_const(state), row) ? &state.ship_records[row] : nullptr;
    }

    inline int Queued_Ship_Row(const s_game_state& state, colony_product product) {
        const auto key = Queued_Ship_Key(product);
        if (key != NO_SHIP) return Ship_Row(state, key);
        const auto row = Legacy_Queued_Ship_Row(product);
        return row >= 0 && row < state.ship_count && row < std::ssize(state.ship_records) ? row : -1;
    }

    inline int16_t Legacy_Colony_Product(const s_game_state& state, colony_product product) {
        if (Queued_Ship_Key(product) == NO_SHIP) return production::Code_Or(product);
        const auto row = Queued_Ship_Row(state, product);
        return row < 0 ? COLONY_PRODUCTION_NONE : COLONY_PRODUCTION_QUEUED_SHIP_BASE - row;
    }

    inline int16_t Production_Notice_Code(const s_game_state& state, colony_product product) {
        if (const auto key = Queued_Ship_Key(product); key != NO_SHIP) {
            const auto row = Ship_Row(state, key);
            if (row >= 0) return static_cast<int16_t>(INT16_MIN + row);
        }
        return Legacy_Colony_Product(state, product);
    }

    inline int16_t Legacy_Production_Notice_Code(int16_t code) {
        return Is_Keyed_Ship_Notice(code) ? COLONY_PRODUCTION_QUEUED_SHIP_BASE - (code - INT16_MIN) : code;
    }

    inline const s_ship_data* Find_Production_Notice_Ship(const s_game_state& state, int16_t code, id_ship key) {
        if (Is_Keyed_Ship_Notice(code)) return Find_Ship(state, key);
        return Find_Legacy_Ship_At_Row(state, Legacy_Queued_Ship_Row(code));
    }

    inline void Import_Colony_Queue(const s_game_state& state, s_colony& colony) {
        const auto import = [&](colony_product& product) {
            const int row = Legacy_Queued_Ship_Row(product);
            if (row >= 0 && row < state.ship_count && row < std::ssize(state.ship_records)) {
                const auto key = state.ship_records[row].id;
                if (key != NO_SHIP) product = Queued_Ship_Product(key);
            }
        };
        for (auto& product : colony.producing) import(product);
        import(colony.just_produced);
    }
}
