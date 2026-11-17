#include "pch.h"

namespace CONTACT {

    void __cdecl Compute_Contacts(::game::s_world_state* world_state) {
        uint8_t old_contacts[MAX_PLAYERS][MAX_PLAYERS];

        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            s_player* plr = &MOX::player[i];

            memcpy(old_contacts[i], plr->contact, sizeof(old_contacts[i]));
            memset(plr->contact, 0, sizeof(plr->contact));
            memset(plr->within_range, 0, sizeof(plr->within_range));

            plr->contact[i] = 1;
            plr->within_range[i] = 1;
            plr->just_established_contact = 0;
        }

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony_a = &MOX::colony[i];
            if (colony_a->planet == ::game::NO_PLANET) {
                continue;
            }

            int8_t owner_a = colony_a->owner;
            int16_t star_a_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(colony_a->planet))).star_index;
            s_star_data* star_a = &MOX::star[star_a_idx];

            for (int16_t j = 0; j < i; ++j) {
                s_colony* colony_b = &MOX::colony[j];
                if (colony_b->planet == ::game::NO_PLANET) {
                    continue;
                }

                int8_t owner_b = colony_b->owner;
                int16_t star_b_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(colony_b->planet))).star_index;
                s_star_data* star_b = &MOX::star[star_b_idx];
                bool contact_made = false;

                // Check if player A can see colony B
                if (SHIPMOVE::Star_In_Extended_Range_Of_Player(star_b, (int16_t)owner_a)) {
                    contact_made = true;
                    if (SHIPMOVE::Star_In_Normal_Range_Of_Player(star_b, (int16_t)owner_a)) {
                        MOX::player[owner_a].within_range[owner_b] = 1;
                    }
                }

                // Check if player B can see colony A
                if (SHIPMOVE::Star_In_Extended_Range_Of_Player(star_a, (int16_t)owner_b)) {
                    contact_made = true;
                    if (SHIPMOVE::Star_In_Normal_Range_Of_Player(star_a, (int16_t)owner_b)) {
                        MOX::player[owner_b].within_range[owner_a] = 1;
                    }
                }

                if (contact_made) {
                    MOX::player[owner_a].contact[owner_b] = 1;
                    MOX::player[owner_b].contact[owner_a] = 1;
                }
            }
        }

        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            bool changed = false;
            s_player* plr = &MOX::player[i];

            for (int j = 0; j < (int)MOX::NUM_PLAYERS; ++j) {
                if (old_contacts[i][j] != plr->contact[j]) {
                    changed = true;
                    if (old_contacts[i][j] == 0) {
                        plr->just_established_contact |= (1 << j);
                        plr->n_times_established_contact[j]++;
                    }
                    break;
                }
            }

            if (changed) {
                REPORT::something_interesting_happened[i] = 1;
            }
        }

        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            s_player* plr = &MOX::player[i];
            for (int j = 0; j < (int)MOX::NUM_PLAYERS; ++j) {
                if (plr->contact[j] == 0) {
                    plr->trade_treaty[j] = 0;
                    plr->research_treaty[j] = 0;
                    spy::Bring_Home_Spies_Vs((int16_t)i, (int16_t)j);
                    spy::Bring_Home_Spies_Vs((int16_t)j, (int16_t)i);
                }
            }
        }
    }

} // namespace CONTACT