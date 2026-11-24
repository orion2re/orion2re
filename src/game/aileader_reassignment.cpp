#include "pch.h"

#include "legacy_ids.h"
#include "ship_ids.h"

namespace aileader {
    void Reassign_Fleet_Officer(game::s_game_state* world, game::id_player player_id, int16_t leader_index) {
        const uint32_t player_index = legacy_world::Slot(player_id);
        auto& state = *world;
        auto* original_ship = game::Find_Leader_Ship(state, state.leaders.at(leader_index));
        if (!original_ship) return;
        s_ship_data* best_ship = original_ship;

        for (int32_t ship_index = static_cast<int16_t>(world->ship_slots.size()) - 1; ship_index >= 0; --ship_index) {
            if (world->ship_slots[ship_index] == game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(*world, ship_index);
            if (factions::Legacy_Code(ship->owner) == player_index && ship->status <= 2 && ship->d.ship_type == SHIP_TYPE_COMBAT && ship->officer_index == -1) {
                if (ship->x == original_ship->x && ship->y == original_ship->y) {
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
            OFFICER::Ai_Assign_Leader_To_Ship(state, &world->players[player_index], leader_index, best_ship);
        }
    }
} // namespace aileader
