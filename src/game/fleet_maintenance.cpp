#include "pch.h"

#include "legacy_world.h"

namespace maintain {
    bool __cdecl Ship_Is_At_Colony(s_ship_data* ship) {
        int16_t star_id;
        int8_t status = ship->status;
        uint8_t owner = (uint8_t)ship->owner;

        if (status == 0 || status == 4) {
            star_id = ship->location;
        } else if (status == 2) {
            int16_t x = ship->x;
            int16_t y = ship->y;
            star_id = HAROLD::XY_To_Star_Id(x, y);
        } else {
            return false;
        }

        if (((MOX::star[star_id].has_colony >> owner) & 1) != 0) {
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
        int16_t player_idx = (int16_t)(player - MOX::player);
        bool no_more_ships_found = false;

        while ((player->bc + player->surplus_bc < 0) && !no_more_ships_found) {
            s_ship_data* cheapest_ship = nullptr;
            int16_t min_worth = 0x7FFF;

            for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                s_ship_data* ship = &MOX::ship[i];

                if (ship->owner == player_idx &&
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

                COLCALC::Do_Player_Colony_Post_Production(player_idx);

                int16_t ship_index = (int16_t)(cheapest_ship - MOX::ship);

                s_MSG msg;
                memset(&msg, 0, sizeof(msg));
                msg.msg_type = MSG_TYPE_SCRAP_SHIP;
                msg.u.scrap_ship.where = cheapest_ship->location;
                msg.u.scrap_ship.ship_idx = ship_index;
                msg.u.scrap_ship.bc = scrap_cost;

                MSG_::Add_Msg(msg, player_idx);

                if (cheapest_ship->officer_index != -1) {
                    int16_t officer_idx = cheapest_ship->officer_index;
                    const game::s_view world{
                        .players = MOX::player,
                        .ships = MOX::ship,
                        .leaders = MOX::leaders,
                        .ship_count = &MOX::NUM_SHIPS,
                    };
                    aileader::Reassign_Fleet_Officer(&world, legacy_world::Player_Id(player_idx), legacy_world::Leader_Id(officer_idx));

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
