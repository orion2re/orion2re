#include "pch.h"

namespace SHIPS {
    void Kill_Ship(s_ship_data& ship) {
        if (ship.owner < 0 || ship.owner > 15) {
            char message[200];
            snprintf(message, sizeof(message), "Invalid ship owner: ship key %lu, owner %d, stardate %ld",
                static_cast<unsigned long>(ship.id.id), static_cast<int>(ship.owner), static_cast<long>(MOX::stardate));
            exitm::Exit_With_Message(message);
        }

        auto& state = game::current_state;
        for (int16_t i = 0; i < MAX_LEADERS; ++i) {
            const auto& leader = state.leaders[i];
            if (leader.type == LEADER_TYPE_SHIP && leader.eta > 0 && game::Find_Leader_Ship(state, leader) == &ship) {
                OFFICER::Move_Officer_To_Pool(i);
                break;
            }
        }

        // Keep the retired record for reports and preserve the 1.31 state fields.
        ship.position.Set(game::NO_SHIP_LOCATION);
        ship.status = 5;
    }

    bool Kill_Ship(game::id_ship key) {
        auto* ship = game::Find_Ship(game::current_state, key);
        if (!ship || ship->status == 5) return false;
        Kill_Ship(*ship);
        return true;
    }

}
