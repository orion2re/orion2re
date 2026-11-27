#include "pch.h"

namespace SHIPS {
    void Kill_Ship(s_ship_data& ship) {
        if (factions::Legacy_Code(ship.owner) < 0 || factions::Legacy_Code(ship.owner) > 15) {
            char message[200];
            snprintf(message, sizeof(message), "Invalid ship owner: ship key %lu, owner %d, stardate %ld",
                static_cast<unsigned long>(ship.id.id), static_cast<int>(factions::Legacy_Code(ship.owner)), static_cast<long>(MOX::stardate));
            exitm::Exit_With_Message(message);
        }

        auto& state = game::current_state;
        for (int16_t i = 0; i < MAX_LEADERS; ++i) {
            auto& leader = state.leaders[i];
            if (leader.type == LEADER_TYPE_SHIP && game::Find_Leader_Ship(state, leader) == &ship) {
                if (leader.status >= 0) OFFICER::Move_Officer_To_Pool(leader_slots::Decode(i));
                else leader.Clear_Location();
            }
        }
        ship.officer_id = game::NO_LEADER;

        // Keep the retired record for reports and preserve the 1.31 state fields.
        ship.position.Set(game::NO_SHIP_LOCATION);
        ship.status = 5;
    }

    bool Kill_Ship(game::id_ship key) {
        const auto ship_entry = game::current_state.ships.find(key);
        auto* ship = ship_entry == game::current_state.ships.end() ? nullptr : &ship_entry->second;
        if (!ship || ship->status == 5) return false;
        Kill_Ship(*ship);
        return true;
    }

}
