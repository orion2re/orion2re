#pragma once

#include "game_state.h"
#include "legacy_ids.h"
#include "faction_ids.h"
#include "ship_slots.h"

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

    inline int Ship_Slot(const s_game_state& state, id_ship id) {
        return ship_slots::Slot(state, id);
    }

    [[deprecated("Use Ship_Slot for positional access or Find_Ship for record access")]]
    inline int Ship_Row(const s_game_state& state, id_ship id) {
        return Ship_Slot(state, id);
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
            ship.officer_id == leader.id && leader.id != NO_LEADER) {
            Set_Leader_Ship(leader, ship, -1);
        }
    }

    inline void Import_Leader_Ship_Assignments(s_game_state& state) {
        for (auto& leader : state.leaders) {
            const auto row = leader.location;
            if (ship_slots::Occupied(state, row)) {
                Import_Leader_Ship_Assignment(leader, ship_slots::At(std::as_const(state), row));
            }
        }
    }

    inline id_ship Ship_Key_At_Slot(const s_game_state& state, int slot) {
        if (slot < 0 || slot >= std::ssize(state.ship_slots)) return NO_SHIP;
        return state.ship_slots[slot];
    }

    [[deprecated("Use Ship_Key_At_Slot at positional boundaries")]]
    inline id_ship Ship_Key_At_Row(const s_game_state& state, int slot) { return Ship_Key_At_Slot(state, slot); }

    inline const s_ship_data* Find_Legacy_Ship_At_Row(const s_game_state& state, int row) {
        if (!ship_slots::Occupied(state, row)) return nullptr;
        return &ship_slots::At(state, row);
    }

    template<typename Notice>
    inline const s_ship_data* Find_Ship_For_Notice(const s_game_state& state, const Notice& notice) {
        if (Is_Keyed_Ship_Notice(notice.ship_idx)) return [&] { const auto entry = state.ships.find(notice.ship_id.get()); return entry == state.ships.end() ? nullptr : &entry->second; }();
        return Find_Legacy_Ship_At_Row(state, notice.ship_idx);
    }

    inline const s_ship_data* Find_Leader_Ship(const s_game_state& state, const s_leader_data& leader) {
        if (leader.ship_location != NO_SHIP) return [&] { const auto entry = state.ships.find(leader.ship_location); return entry == state.ships.end() ? nullptr : &entry->second; }();
        return nullptr; // Legacy assignments are bound during import, never to a later occupant.
    }

    inline s_ship_data* Find_Leader_Ship(s_game_state& state, const s_leader_data& leader) {
        if (leader.ship_location != NO_SHIP) return [&] { const auto entry = state.ships.find(leader.ship_location); return entry == state.ships.end() ? nullptr : &entry->second; }();
        return nullptr;
    }

    template<class Predicate>
    inline bool Any_Ship(const s_game_state& state, Predicate matches) {
        return std::ranges::any_of(state.ships, [&](const auto& entry) { return matches(entry.second); });
    }

    inline const s_ship_data* Find_Queued_Ship(const s_game_state& state, colony_product product) {
        const auto key = Queued_Ship_Key(product);
        const auto entry = state.ships.find(key);
        return entry == state.ships.end() ? nullptr : &entry->second;
    }

    inline s_ship_data* Find_Queued_Ship(s_game_state& state, colony_product product) {
        const auto key = Queued_Ship_Key(product);
        const auto entry = state.ships.find(key);
        return entry == state.ships.end() ? nullptr : &entry->second;
    }

    inline int Queued_Ship_Row(const s_game_state& state, colony_product product) {
        const auto key = Queued_Ship_Key(product);
        return Ship_Slot(state, key);
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
        if (Is_Keyed_Ship_Notice(code)) return [&] { const auto entry = state.ships.find(key); return entry == state.ships.end() ? nullptr : &entry->second; }();
        return Find_Legacy_Ship_At_Row(state, Legacy_Queued_Ship_Row(code));
    }

    inline void Import_Colony_Queue(const s_game_state& state, s_colony& colony) {
        for (auto& product : colony.producing) {
            const int row = Legacy_Queued_Ship_Row(product);
            if (row >= 0) product = Queued_Ship_Product(Ship_Key_At_Slot(state, row));
        }
        // A completion is history, not an active order. Its hull may already be gone.
        const auto completed = Ship_Key_At_Slot(state, Legacy_Queued_Ship_Row(colony.just_produced));
        if (completed != NO_SHIP) colony.just_produced = Queued_Ship_Product(completed);
    }
}

namespace ship_slots {
    inline game::id_ship Allocate_Key(game::s_game_state& state, std::optional<game::id_player> peer) {
        // Leave implicit historical row keys alone.
        state.next_ship_id = std::max(state.next_ship_id, static_cast<uint32_t>(state.ship_slots.size()));
        return game::Allocate_Ship_Id(state, peer ? std::optional<unsigned>{peer->id} : std::nullopt);
    }
}
