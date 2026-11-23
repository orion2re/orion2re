#include "pch.h"

#include "legacy_ids.h"
#include "ship_ids.h"

namespace maintain {
    bool __cdecl Ship_Is_At_Colony(s_ship_data* ship) {
        int16_t star_id;
        int8_t status = ship->status;
        uint8_t owner = (uint8_t)factions::Legacy_Code(ship->owner);

        if (status == 0 || status == 4) {
            star_id = game::Ship_Orbit_Row(game::current_state, ship->position, -1);
        } else if (status == 2) {
            int16_t x = ship->x;
            int16_t y = ship->y;
            star_id = HAROLD::XY_To_Star_Id(x, y);
        } else {
            return false;
        }

        const auto star = game::current_state.stars.find(game::Star_Key_At_Row(game::current_state, star_id));
        if (star != game::current_state.stars.end() && (star->second.has_colony.test_slot(owner)) != 0) {
            return true;
        }

        return false;
    }

    int16_t __cdecl Ship_Scrap_Value(s_ship_data* ship) {
        if (Ship_Is_At_Colony(ship)) {
            return (int16_t)(ship->d.cost * ship_scrap_refund_percent / 100);
        }
        return 0;
    }

    /**
     * Iteratively scraps the cheapest ships matching criteria until player maintenance deficit is resolved.
     */
    void __cdecl Kill_Ships(s_player* player, bool at_colony_only, bool restricted_mission, int16_t ship_type) {
        int16_t player_idx = (int16_t)(player->id.id);
        bool no_more_ships_found = false;

        while ((player->bc + player->surplus_bc < 0) && !no_more_ships_found) {
            s_ship_data* cheapest_ship = nullptr;
            int16_t min_worth = 0x7FFF;

            for (int16_t i = 0; i < game::current_state.ship_count; ++i) {
                s_ship_data* ship = &game::current_state.ship_records[i];

                if (factions::Legacy_Code(ship->owner) == player_idx &&
                    (int16_t)ship->d.ship_type == ship_type &&
                    ship->status < 3 /* Active */) {
                    if (at_colony_only && !Ship_Is_At_Colony(ship)) {
                        continue;
                    }

                    if (restricted_mission) {
                        if (!(ship->mission == 0 || ship->mission == 3 || ship->mission == 7)) {
                            continue;
                        }
                    }

                    int16_t worth = (*maintain::ship_worths)[i];
                    if (worth < min_worth) {
                        min_worth = worth;
                        cheapest_ship = ship;
                    }
                }
            }

            if (cheapest_ship == nullptr) {
                no_more_ships_found = true;
            } else {
                int16_t scrap_cost = maintain::Ship_Scrap_Value(cheapest_ship);
                player->bc += scrap_cost;

                if (player->objectives != PLAYER_OBJECTIVE_HUMAN) {
                    player->bc += MOX::settings.game_difficulty;
                }

                cheapest_ship->status = 7; /* Scrapped */

                COLCALC::Do_Player_Colony_Post_Production(legacy_world::Decode_Player_Slot(player_idx));

                const auto ship_index = game::current_state.ship_records.Row_Of(*cheapest_ship);

                s_MSG msg;
                memset(&msg, 0, sizeof(msg));
                msg.msg_type = MSG_TYPE_SCRAP_SHIP;
                msg.u.scrap_ship.destination = {};
                if (cheapest_ship->position.kind == game::e_ship_location::at_star &&
                    cheapest_ship->position.target == game::e_ship_target::star)
                    msg.u.scrap_ship.destination = {game::e_star_destination::star, cheapest_ship->position.star_id.get()};
                game::Set_Ship_Notice(msg.u.scrap_ship, ship_index, cheapest_ship->id);
                msg.u.scrap_ship.bc = scrap_cost;

                MSG_::Add_Msg(msg, player_idx);

                if (cheapest_ship->officer_index != -1) {
                    int16_t officer_idx = cheapest_ship->officer_index;
                    auto& world = game::current_state;
                    aileader::Reassign_Fleet_Officer(&world, legacy_world::Player_Id(player_idx), officer_idx);

                    if (cheapest_ship->officer_index == officer_idx) {
                        OFFICER::Move_Officer_To_Pool(officer_idx);
                    }
                    cheapest_ship->officer_index = -1;
                }

                MAINSCR::Update_Ship_Icons_In_Between_Turns();
            }
        }
    }
} // namespace maintain
