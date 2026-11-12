#include "pch.h"

namespace aileader {
    void __cdecl Reassign_Fleet_Officer(int32_t player_index, int32_t leader_index) {
        int16_t original_location = MOX::leaders[leader_index].location;
        s_ship_data* original_ship = &MOX::ship[original_location];
        s_ship_data* best_ship = original_ship;

        for (int32_t ship_index = (int32_t)MOX::NUM_SHIPS - 1; ship_index >= 0; --ship_index) {
            s_ship_data* ship = &MOX::ship[ship_index];
            if (ship->owner == player_index && ship->status <= 2 && ship->d.ship_type == SHIP_TYPE_COMBAT && ship->officer_index == -1) {
                if (OFFICER::Ships_In_Same_Place((int16_t)ship_index, original_location)) {
                    if ((best_ship == original_ship && original_ship->status == 7 /* Scrapped */) ||
                        (ship->d.size >= best_ship->d.size &&
                            ship->d.shield_type >= best_ship->d.shield_type &&
                            ship->d.armor_type >= best_ship->d.armor_type &&
                            ship->d.computer_type >= best_ship->d.computer_type &&
                            ship->crew_experience > best_ship->crew_experience)) {
                        best_ship = ship;
                    }
                }
            }
        }

        if (best_ship != original_ship) {
            OFFICER::Ai_Assign_Leader_To_Ship(&MOX::player[player_index], &MOX::leaders[leader_index], best_ship);
        }
    }
} // namespace aileader
