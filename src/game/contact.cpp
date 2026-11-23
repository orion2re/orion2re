#include "pch.h"

namespace CONTACT {

    void __cdecl Compute_Contacts() {
        uint8_t old_contacts[MAX_PLAYERS][MAX_PLAYERS];

        for (int i = 0; i < (int)game::current_state.player_count; ++i) {
            s_player* plr = &game::current_state.players[i];

            memcpy(old_contacts[i], plr->contact, sizeof(old_contacts[i]));
            memset(plr->contact, 0, sizeof(plr->contact));
            memset(plr->within_range, 0, sizeof(plr->within_range));

            plr->contact[i] = 1;
            plr->within_range[i] = 1;
            plr->just_established_contact.reset();
        }

        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            s_colony* colony_a = &roster_colony;
            if (colony_a->planet_id == game::NO_PLANET) {
                continue;
            }

            int8_t owner_a = legacy_world::Encode_Player_Slot(colony_a->owner);
            s_star_data* star_a = &game::current_state.stars.at(game::current_state.planets.at(colony_a->planet_id).star_id);

            for (auto it = game::current_state.colonies.begin(); it->first < roster_id; ++it) {
                auto* colony_b = &it->second;
                if (colony_b->planet_id == game::NO_PLANET) {
                    continue;
                }

                int8_t owner_b = legacy_world::Encode_Player_Slot(colony_b->owner);
                s_star_data* star_b = &game::current_state.stars.at(game::current_state.planets.at(colony_b->planet_id).star_id);
                bool contact_made = false;

                // Check if player A can see colony B
                if (SHIPMOVE::Star_In_Extended_Range_Of_Player(game::current_state, *star_b, legacy_world::Decode_Player_Slot(owner_a))) {
                    contact_made = true;
                    if (SHIPMOVE::Star_In_Normal_Range_Of_Player(game::current_state, *star_b, legacy_world::Decode_Player_Slot(owner_a))) {
                        game::current_state.players[owner_a].within_range[owner_b] = 1;
                    }
                }

                // Check if player B can see colony A
                if (SHIPMOVE::Star_In_Extended_Range_Of_Player(game::current_state, *star_a, legacy_world::Decode_Player_Slot(owner_b))) {
                    contact_made = true;
                    if (SHIPMOVE::Star_In_Normal_Range_Of_Player(game::current_state, *star_a, legacy_world::Decode_Player_Slot(owner_b))) {
                        game::current_state.players[owner_b].within_range[owner_a] = 1;
                    }
                }

                if (contact_made) {
                    game::current_state.players[owner_a].contact[owner_b] = 1;
                    game::current_state.players[owner_b].contact[owner_a] = 1;
                }
            }
        }

        for (int i = 0; i < (int)game::current_state.player_count; ++i) {
            bool changed = false;
            s_player* plr = &game::current_state.players[i];

            for (int j = 0; j < (int)game::current_state.player_count; ++j) {
                if (old_contacts[i][j] != plr->contact[j]) {
                    changed = true;
                    if (old_contacts[i][j] == 0) {
                        plr->just_established_contact.set_slot(j);
                        plr->n_times_established_contact[j]++;
                    }
                    break;
                }
            }

            if (changed) {
                REPORT::something_interesting_happened[i] = 1;
            }
        }

        for (int i = 0; i < (int)game::current_state.player_count; ++i) {
            s_player* plr = &game::current_state.players[i];
            for (int j = 0; j < (int)game::current_state.player_count; ++j) {
                if (plr->contact[j] == 0) {
                    plr->trade_treaty[j] = 0;
                    plr->research_treaty[j] = 0;
                    spy::Bring_Home_Spies_Vs(legacy_world::Decode_Player_Slot((int16_t)i), legacy_world::Decode_Player_Slot((int16_t)j));
                    spy::Bring_Home_Spies_Vs(legacy_world::Decode_Player_Slot((int16_t)j), legacy_world::Decode_Player_Slot((int16_t)i));
                }
            }
        }
    }

} // namespace CONTACT
