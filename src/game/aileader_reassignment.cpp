#include "pch.h"

#include "legacy_world.h"

namespace aileader {
    void Reassign_Fleet_Officer(const game::s_view* world, game::id_player player_id, game::id_leader leader_id) {
        const uint32_t player_index = legacy_world::Slot(player_id);
        const uint32_t leader_index = legacy_world::Slot(leader_id);
        int16_t original_location = world->leaders[leader_index].location;
        s_ship_data* original_ship = &world->ships[original_location];
        s_ship_data* best_ship = original_ship;

        for (int32_t ship_index = world->Ship_Count() - 1; ship_index >= 0; --ship_index) {
            s_ship_data* ship = &world->ships[ship_index];
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
            OFFICER::Ai_Assign_Leader_To_Ship(&world->players[player_index], &world->leaders[leader_index], best_ship);
        }
    }
} // namespace aileader
