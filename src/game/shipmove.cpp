#include "pch.h"
#include "fleet_movement.h"
#include "legacy_world.h"

#include <unordered_set>

using game::current_state;
#include "legacy_location.h"
#include "legacy_ids.h"
#include "legacy_ship_location_codec.h"

#include "lua/lua_game_ship_identity.h"

namespace SHIPS {
    void __cdecl Clear_Auto_Colonization_Flag(s_ship_data* ship) {
        ship->colonization_target = game::NO_PLANET;
    }
} // namespace SHIPS

namespace SHIPMOVE {
    uint8_t jump_gate_speed_bonus = 3;
    uint16_t warp_interdictor_range = 8100;
    bool jump_gate_pass_nebula = false;

    namespace {
        constexpr uint8_t JUMP_GATE_ROUTE_MASK = 1;
        using t_fleet = std::span<s_ship_data* const>;

        game::s_star_destination Movement_Target(const game::s_ship_location& location) {
            using enum game::e_ship_target;
            if (location.target == star) return {game::e_star_destination::star, location.star_id};
            if (location.target == antares) return {game::e_star_destination::antares, game::NO_STAR};
            return {};
        }

        const s_star_data* Find_Destination(const game::s_star_destination& destination) {
            if (destination.kind == game::e_star_destination::antares && destination.star_id.get() == game::NO_STAR) {
                return &current_state.antares_destination;
            }
            if (destination.kind != game::e_star_destination::star) return nullptr;
            const auto entry = current_state.stars.find(destination.star_id.get());
            return entry == current_state.stars.end() ? nullptr : &entry->second;
        }

        std::vector<s_ship_data*> Resolve_Fleet(std::span<const game::id_ship> keys) {
            if (keys.size() > MAX_SHIPS) return {};
            const game::s_view world{.state = &current_state};
            std::vector<s_ship_data*> ships;
            ships.reserve(keys.size());
            for (const auto key : keys) {
                auto* ship = game::Find_Ship(world, key);
                if (!ship || ship->status == 5) return {};
                ships.push_back(ship);
            }
            return ships;
        }

        bool Ship_Uses_Jump_Gate(const s_ship_data* ship) {
            return (ship->special_device_damage_flags[0] & JUMP_GATE_ROUTE_MASK) != 0;
        }

        bool Ship_Used_Jump_Gate_To(const s_ship_data* ship, const game::s_star_destination& destination) {
            return destination.kind == game::e_star_destination::star && ship->prior_jump_gate_stardate == MOX::stardate &&
                ship->prior_jump_gate_destination != game::NO_STAR &&
                ship->prior_jump_gate_destination == destination.star_id.get();
        }

        void Set_Ship_Uses_Jump_Gate(s_ship_data* ship, bool uses_jump_gate) {
            if (uses_jump_gate) {
                ship->special_device_damage_flags[0] |= JUMP_GATE_ROUTE_MASK;
            } else {
                ship->special_device_damage_flags[0] &= (uint8_t)~JUMP_GATE_ROUTE_MASK;
            }
        }

        bool Ship_Is_In_Same_Moving_Fleet(const s_ship_data* first, const s_ship_data* ship) {
            return SHIPMOVE::Ship_Is_Moving(ship) &&
                ship->owner == first->owner &&
                ship->status == first->status &&
                ship->position.Get() == first->position.Get() &&
                ship->x == first->x &&
                ship->y == first->y;
        }

        std::vector<s_ship_data*> Moving_Fleet(const s_ship_data* first) {
            if (!first || !SHIPMOVE::Ship_Is_Moving(first)) return {};
            std::vector<s_ship_data*> ships;
            for (int row = 0; row < current_state.ship_count && ships.size() < MAX_SHIPS - 1; ++row) {
                auto* ship = &current_state.ship_records[row];
                if (Ship_Is_In_Same_Moving_Fleet(first, ship)) {
                    ships.push_back(ship);
                }
            }
            return ships;
        }

        struct s_move_position {
            game::s_ship_location location;
            int16_t x;
            int16_t y;
        };

        struct s_move_origin {
            s_move_position position;
        };

        game::s_ship_location Movement_Location(const s_ship_data& ship) {
            return ship.position.Get();
        }

        int16_t Destination_At_Location(const game::s_ship_location& location) {
            return static_cast<int16_t>(legacy_world::Destination_Slot(location, game::ANTARES_POSITION));
        }

        s_move_position Movement_Position(const s_ship_data& ship, const s_move_origin* origin) {
            if (origin != nullptr) return origin->position;
            return {Movement_Location(ship), ship.x, ship.y};
        }

        bool Same_Moving_Fleet_At_Positions(const s_ship_data* first, const s_move_position* first_position, const s_ship_data* ship, const s_move_position* position) {
            return Ship_Is_Moving(ship) && ship->owner == first->owner && ship->status == first->status &&
                position->location == first_position->location && position->x == first_position->x && position->y == first_position->y;
        }

        bool Moving_Fleet_List_Is_Complete(t_fleet ships, const s_move_origin* origin = nullptr) {
            if (ships.empty()) return false;
            const auto* first = ships.front();
            const auto first_position = Movement_Position(*first, origin);
            for (const auto* ship : ships) {
                const auto position = Movement_Position(*ship, origin);
                if (!Same_Moving_Fleet_At_Positions(first, &first_position, ship, &position)) {
                    return false;
                }
            }
            const std::unordered_set<const s_ship_data*> members(ships.begin(), ships.end());
            return !game::Any_Ship(current_state, [&](const s_ship_data& ship) {
                const auto position = Movement_Position(ship, nullptr);
                return Same_Moving_Fleet_At_Positions(first, &first_position, &ship, &position) && !members.contains(&ship);
            });
        }

        bool Fleet_Uses_Jump_Gate(t_fleet ships, const game::s_star_destination& destination, const s_move_origin* origin) {
            bool uses_prior_destination = false;
            for (const auto* ship : ships) {
                const auto position = Movement_Position(*ship, origin);
                if (ship->status != 1) {
                    return false;
                }
                if (position.location.kind == game::e_ship_location::in_flight &&
                    Movement_Target(position.location) == destination &&
                    Ship_Uses_Jump_Gate(ship)) {
                    continue;
                }
                if (!Ship_Used_Jump_Gate_To(ship, destination)) {
                    return false;
                }
                uses_prior_destination = true;
            }
            return !ships.empty() && (!uses_prior_destination || Moving_Fleet_List_Is_Complete(ships, origin));
        }

        int Fleet_Delay(const s_ship_data& ship, const s_ship_move_info& move_info) {
            return static_cast<uint8_t>(move_info.turns_left) - static_cast<uint8_t>(ship.turns_left);
        }

        bool Needs_Fleet_Confirmation(const s_ship_data* ship, const s_ship_move_info& move_info) {
            return ship && SHIPMOVE::Ship_Is_Moving(ship) && (!move_info.moving || Fleet_Delay(*ship, move_info) > 0);
        }

        bool Confirm_Fleet_Change(const s_ship_data* ship, const s_ship_move_info* move_info) {
            if (!Needs_Fleet_Confirmation(ship, *move_info)) return true;

            if (move_info->moving == 0) {
                return GENDRAW::Confirmation_Box(
                           "The fleet on course to pass the black hole will have no navigator. "
                           "If this is not remedied the fleet will be lost at the end of the turn. Continue?") != 0;
            }

            const int delay = Fleet_Delay(*ship, *move_info);
            char message[200];
            snprintf(
                message,
                sizeof(message),
                "The fleet without navigator will be delayed by %d turn%s. Continue?",
                delay,
                delay == 1 ? "" : "s");
            return GENDRAW::Confirmation_Box(message) != 0;
        }

        std::vector<s_ship_data> Capture_Fleet(t_fleet ships) {
            std::vector<s_ship_data> records;
            records.reserve(ships.size());
            for (const auto* ship : ships) {
                records.push_back(*ship);
            }
            return records;
        }

        bool Rebind_Fleet(std::span<const s_ship_data> records, std::vector<s_ship_data*>& ships) {
            ships.clear();
            for (const auto& record : records) {
                auto* ship = game::Find_Ship(current_state, record.id);
                if (!ship || *ship != record) return false;
                ships.push_back(ship);
            }
            return true;
        }

    } // namespace

    /**
     * Reserves available ship slots for Antarans and players based on current production demand.
     */
    void __cdecl Allocate_New_Ship_Slots() {
        int antaran_max_slots = 0;
        for (int i = 0; i < 9; ++i) {
            antaran_max_slots += MOX::antarans.offensive_ships[i];
        }

        if (antaran_max_slots > 20) {
            antaran_max_slots = 20;
        } else {
            antaran_max_slots += 2;
        }

        int colony_production_slots[MAX_PLAYERS] = {0};
        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            s_colony* col = &roster_colony;
            if (col->owner != -1 && col->outpost_flag == 0) {
                for (int j = 0; j < 7; ++j) {
                    if (!COLBLDG::Colony_Production_Is_Queued_Ship(col->producing[j])) {
                        int owner = col->owner;
                        colony_production_slots[owner]++;
                    }
                }
            }
        }

        int player_max_slots = colony_production_slots[MOX::PLAYER_NUM];
        if (MOX::game_type == 0 && player_max_slots > 20) {
            player_max_slots = 20;
        }

        int16_t unassigned_indices[MAX_SHIPS];
        int unassigned_count = 0;
        int antaran_assigned = 0;
        int player_assigned = 0;

        for (int16_t i = 0; i < MAX_SHIPS; ++i) {
            if (i >= game::current_state.ship_count || game::current_state.ship_records[i].status == 5) {
                if (antaran_assigned < antaran_max_slots) {
                    game::current_state.ship_records[i].owner = (int8_t)MAX_PLAYERS;
                    antaran_assigned++;
                } else if (player_assigned < player_max_slots) {
                    game::current_state.ship_records[i].owner = (int8_t)MOX::PLAYER_NUM;
                    player_assigned++;
                } else {
                    unassigned_indices[unassigned_count++] = i;
                }
            }
        }

        int ships_assigned_per_player[MAX_PLAYERS] = {0};
        ships_assigned_per_player[MOX::PLAYER_NUM] = player_assigned;

        int current_player = (int)MOX::stardate % (int)MOX::NUM_PLAYERS;
        int assigned_from_unassigned = 0;
        int fail_count = 0;

        if (unassigned_count > 0) {
            while (assigned_from_unassigned < unassigned_count) {
                if (fail_count >= (int)MOX::NUM_PLAYERS) break;

                if (ships_assigned_per_player[current_player] < colony_production_slots[current_player]) {
                    int16_t ship_idx = unassigned_indices[assigned_from_unassigned++];
                    game::current_state.ship_records[ship_idx].owner = (int8_t)current_player;
                    ships_assigned_per_player[current_player]++;
                    fail_count = 0;
                } else {
                    fail_count++;
                }

                current_player++;
                if (current_player == (int)MOX::NUM_PLAYERS) {
                    current_player = 0;
                }
            }
        }

        while (assigned_from_unassigned < unassigned_count) {
            if (MOX::player[current_player].eliminated == 0) {
                int16_t ship_idx = unassigned_indices[assigned_from_unassigned++];
                game::current_state.ship_records[ship_idx].owner = (int8_t)current_player;
                ships_assigned_per_player[current_player]++;
            }

            current_player++;
            if (current_player == (int)MOX::NUM_PLAYERS) {
                current_player = 0;
            }
        }
    }

    /**
     * Iterates through all players to refresh their ship ranges,
     * best warp drive technology, and current FTL speed.
     */
    void __cdecl Update_All_Ship_Ranges() {
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            s_player* current_player = &MOX::player[i];

            Update_Player_Ship_Range(current_player);

            current_player->best_ftl_drive = (char)INITSHIP::Best_Warp_Drive(i);
            current_player->ship_speed = (uint8_t)INITSHIP::Calc_Player_FTL_Speed(i);
        }
    }

    /**
     * Updates the movement range of the player's ships based on their researched fuel tech tiers.
     * Iterates backwards through fuel tiers 5 down to 1, checking if the required tech is researched.
     */
    void __cdecl Update_Player_Ship_Range(s_player* player) {
        for (int i = FUEL_COUNT - 1; i > FUEL_NO_FUEL_CELLS; --i) {
            int16_t tech_idx = TECHDATA::fuel[i].tech_app_id;

            // Check if the tech application for this fuel tier is fully researched
            if (player->tech_applications[tech_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                player->ship_range = TECHDATA::fuel[i].ship_range;
                return;
            }
        }

        // If no fuel upgrades (tiers 1-5) are researched, range is set to 0.
        player->ship_range = 0;
    }

    /**
     * Checks if any star colonized by a specific player is within a given range of the target star.
     * Considers black hole blockages.
     * @param star The target star to check from.
     * @param player_idx Index of the player to check colonies for.
     * @param dist The maximum range (in parsecs, where 1 parsec is roughly 30 pixels).
     */
    static bool Star_In_Range_Of_Player_Aux1(const game::s_game_state& state, const s_star_data* star, int16_t player_idx, int16_t dist) {
        const auto star_position = game::Star_Row(state, star->id);
        // dist is scaled by 30 pixels per unit (30^2 = 900)
        int32_t dist_sq_limit = (int32_t)dist * dist * 900;

        for (const auto& [key, record] : state.stars) {
            const auto* current_star = &record;

            // Check if player_idx bit is set in has_colony bitmask
            if ((current_star->has_colony >> (player_idx & 0x1F)) & 1) {
                // Check if current_star can 'see' the target star through black hole blockages
                if (star_position < 0 || !struct_::Read_Bit_Field(current_star->black_hole_blocks, star_position)) {
                    int32_t dx = (int32_t)star->x - current_star->x;
                    int32_t dy = (int32_t)star->y - current_star->y;

                    if ((dx * dx + dy * dy) <= dist_sq_limit) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    /**
     * Checks if a star is in range of a player, considering colonies, direct range, and wormholes.
     * @param star The target star to check.
     * @param player_idx Index of the player.
     * @param dist Maximum range in parsecs.
     * @return True if the star is in range.
     */
    static bool Star_In_Range_Of_Player_Aux2(const game::s_game_state& state, const s_star_data* star, int16_t player_idx, int16_t dist) {
        // Check if player has a colony in this star system
        if ((star->has_colony >> ((uint8_t)player_idx & 0x1F)) & 1) {
            return true;
        }

        // Check range normally
        if (Star_In_Range_Of_Player_Aux1(state, star, player_idx, dist)) {
            return true;
        }

        // Check range via wormhole if the destination star is visited
        const auto exit = state.stars.find(star->wormhole_star_id);
        if (exit != state.stars.end()) {
            const auto* wormhole_star = &exit->second;

            if ((wormhole_star->visited >> ((uint8_t)player_idx & 0x1F)) & 1) {
                if (Star_In_Range_Of_Player_Aux1(state, wormhole_star, player_idx, dist)) {
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * Checks if a star is within the normal ship range of a player.
     * @param star The target star.
     * @param player_idx Index of the player.
     * @return True if in range.
     */
    bool __cdecl Star_In_Normal_Range_Of_Player(s_star_data* star, int16_t player_idx) {
        int16_t range = MOX::player[player_idx].ship_range;
        return Star_In_Range_Of_Player(star, player_idx, range);
    }

    /**
     * Checks if a star system is within the extended ship range of a player.
     * The extended range is calculated as 150% of the player's standard ship range.
     */
    bool __cdecl Star_In_Extended_Range_Of_Player(s_star_data* star, int16_t player_idx) {
        int16_t extended_range = (int16_t)((MOX::player[player_idx].ship_range * 3 + 1) / 2);

        return Star_In_Range_Of_Player(star, player_idx, extended_range);
    }

    /**
     * Checks if a star is within the extended range of a player, considering alliances.
     * @param star The target star.
     * @param player_idx Index of the player.
     * @param dist Maximum range in parsecs.
     * @return True if in range for the player or any ally.
     */
    bool __cdecl Star_In_Range_Of_Player(s_star_data* star, int16_t player_idx, int16_t dist) {
        const game::s_view world{.players = MOX::player, .state = &current_state, .player_count = &MOX::NUM_PLAYERS};
        return Star_In_Range_Of_Player(world, *star, player_idx, dist);
    }

    bool Star_In_Range_Of_Player(const game::s_view& world, const s_star_data& star, int16_t player_idx, int16_t dist) {
        if (Star_In_Range_Of_Player_Aux2(*world.state, &star, player_idx, dist)) {
            return true;
        }

        for (int i = 0; i < world.Player_Count(); ++i) {
            // Check if player is an ally (treaty type 2) and the star is in their range
            if (player_idx != i && world.players[player_idx].treaty[i] == 2) {
                if (Star_In_Range_Of_Player_Aux2(*world.state, &star, (int16_t)i, dist)) {
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * Returns whether the ship is currently travelling between stars.
     * @param ship Ship record to inspect.
     */
    bool __cdecl Ship_Is_Moving(const s_ship_data* ship) {
        if (ship->status >= 1 && ship->status <= 2) {
            return true;
        }
        return false;
    }

    /**
     * Determines if a ship can receive orders, based on its status and player's communications technology.
     * @param ship The ship to check.
     * @return True if the ship can be ordered.
     */
    bool __cdecl Can_Order_Ship(s_ship_data* ship) {
        int16_t owner = (int8_t)ship->owner;

        if (owner >= MOX::NUM_PLAYERS) {
            return false;
        }

        if (ship->status == 0 || ship->status == 2) {
            return true;
        }

        if (ship->status != 1) {
            return false;
        }

        s_player* player = &MOX::player[owner];

        // Check for Hyperspace Communications
        if (player->tech_applications[TECH_APP_HYPERSPACE_COMMUNICATIONS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return true;
        }

        // Check if player has simpler comms tech and is near a relay
        if (player->tech_applications[TECH_APP_TACHYON_COMMUNICATIONS] == TECH_RESEARCH_STATUS_RESEARCHED ||
            player->tech_applications[TECH_APP_SUB_SPACE_COMMUNICATIONS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            uint8_t player_mask = (uint8_t)((uint32_t)1 << ((uint8_t)ship->owner & 0x1F));

            for (const auto& [star_id, record] : current_state.stars) {
                const auto* star = &record;
                int dist = GEO::Parsecs_Between_Points(star->x, star->y, ship->x, ship->y);

                if (dist <= ship_communication_range.sub_space) {
                    if (star->has_subspace & player_mask) {
                        return true;
                    }
                    if (dist <= ship_communication_range.tachyon) {
                        if (star->has_tachyon & player_mask) {
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    /**
     * Returns the ship's travel range, including the extended-range special when present.
     * @param ship Ship record to inspect.
     */
    int16_t __cdecl Ship_Range(s_ship_data* ship) {
        if (ship->owner >= MAX_PLAYERS) {
            return 0xFF;
        }

        int16_t range = MOX::player[ship->owner].ship_range;

        if (struct_::Test_Bit_Field(ship->d.special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS)) {
            range = (range * 3 + 1) / 2;
        }

        return range;
    }

    /**
     * Returns the destination star index encoded in the ship's current location field.
     * @param ship Ship record to inspect.
     */
    int16_t __cdecl Ship_Destination_Star(s_ship_data* ship) {
        return Destination_At_Location(Movement_Location(*ship));
    }

    /**
     * Creates a non-player ship in the first available slot and returns its index.
     * @param owner Owner slot to assign.
     * @param design Optional design to copy into the new ship.
     * @param x Initial x-coordinate.
     * @param y Initial y-coordinate.
     */
    game::id_ship Create_Nonplayer_Ship(int8_t owner, const s_ship_design* design, int16_t x, int16_t y) {
        int16_t i;

        // Search for an available ship slot
        for (i = 0; i < MAX_SHIPS; ++i) {
            if (game::current_state.ship_records[i].owner == MAX_PLAYERS) {
                // Slot is free if status is 5 or it's beyond the current total count
                if (game::current_state.ship_records[i].status == 5 || i >= game::current_state.ship_count) {
                    break;
                }
            }
        }

        if (i == MAX_SHIPS) {
            return game::NO_SHIP;
        }
        const auto id = legacy_world::Allocate_Live_Ship_Id(current_state, MOX::game_type, MOX::PLAYER_NUM);
        if (id == game::NO_SHIP || !orion2re::lua::game::Advance_Ship_Generation(i)) {
            return game::NO_SHIP;
        }

        s_ship_data ship{};
        ship.id = id;
        if (design) ship.d = *design;
        ship.owner = owner;
        ship.d.previous_owner = static_cast<uint8_t>(owner);
        ship.position.Set(legacy_world::Location_At_Position(
            game::e_ship_location::at_star, HAROLD::XY_To_Star_Id(x, y), game::ANTARES_POSITION));
        ship.x = x;
        ship.y = y;
        ship.officer_index = -1;
        ship.d.date_of_design = static_cast<uint16_t>(MOX::stardate);
        current_state.ship_records.Replace_Row(i, ship);
        if (i >= game::current_state.ship_count) {
            game::current_state.ship_count = i + 1;
        }
        return id;
    }

    /**
     * Advances each moving ship by one turn and finalizes any arrivals.
     */
    void __cdecl Move_All_Ships_Toward_Stars() {
        int8_t unused_navigator_flag;
        int8_t unused_direction;

        if (MOX::game_type >= 2 && MOX::game_type <= 3) {
            for (auto& [star_id, star] : current_state.stars) {
                star.just_visited = 0;
            }
        }

        for (int16_t i = 0; i < game::current_state.ship_count; ++i) {
            s_ship_data* ship = &game::current_state.ship_records[i];

            if (Ship_Is_Moving(ship)) {
                auto location = Movement_Location(*ship);
                const auto star_idx = Destination_At_Location(location);
                if (location.kind == game::e_ship_location::at_star || star_idx < 0 ||
                    (location.target == game::e_ship_target::star && !current_state.stars.contains(location.star_id))) continue;
                if (location.kind == game::e_ship_location::in_wormhole) {
                    location.kind = game::e_ship_location::in_flight;
                    ship->position.Set(location);
                }
                const auto& destination = game::Destination_Record_At_Row(current_state, star_idx);
                int16_t star_x = destination.x;
                int16_t star_y = destination.y;

                if (ship->travelling_speed == UINT8_MAX) {
                    ship->x = star_x;
                    ship->y = star_y;
                } else {
                    // ship->x/y are packed fields, so move through aligned locals.
                    int16_t new_x = ship->x;
                    int16_t new_y = ship->y;
                    GEO::Move_Player_1_Turn_To_Star(
                        ship->owner,
                        ship->x,
                        ship->y,
                        star_idx,
                        (uint16_t)(uint8_t)ship->travelling_speed,
                        &new_x,
                        &new_y,
                        (ship->group_has_navigator == 0 &&
                            (!jump_gate_pass_nebula || !Ship_Uses_Jump_Gate(ship)))
                            ? &unused_navigator_flag
                            : nullptr,
                        &unused_direction);
                    ship->x = new_x;
                    ship->y = new_y;
                }

                ship->turns_left--;

                if (ship->x == star_x && ship->y == star_y) {
                    Make_Ship_Arrive_At_Star(ship, star_idx);
                } else {
                    ship->status = 1;
                }
            } else {
                ship->mission = 0;
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();
    }

    static bool Apply_Resolved_Move(t_fleet ships, game::s_star_destination target, const s_ship_move_info* move_info) {
        if (ships.empty() || !move_info || !Find_Destination(target)) return false;
        int8_t move_status = 0;
        auto kind = game::e_ship_location::at_star;
        if (move_info->turns_left != 0) {
            const auto first = Movement_Location(*ships.front());
            const bool already_in_flight = first.kind == game::e_ship_location::in_flight;
            kind = already_in_flight ? game::e_ship_location::in_flight : game::e_ship_location::in_wormhole;
            move_status = already_in_flight ? 1 : 2;
        }
        const bool normal_star = target.kind == game::e_star_destination::star;
        const game::s_ship_location destination{kind, target.star_id.get(),
            normal_star ? game::e_ship_target::star : game::e_ship_target::antares};

        const bool complete_moving_fleet = Moving_Fleet_List_Is_Complete(ships);
        for (auto* ship : ships) {

            if (Ship_Used_Jump_Gate_To(ship, target) &&
                !complete_moving_fleet) {
                for (int16_t other_ship_idx = 0;
                     other_ship_idx < game::current_state.ship_count;
                     ++other_ship_idx) {
                    s_ship_data* other_ship = &game::current_state.ship_records[other_ship_idx];
                    if (Ship_Is_In_Same_Moving_Fleet(ship, other_ship) &&
                        Ship_Used_Jump_Gate_To(other_ship, target)) {
                        other_ship->prior_jump_gate_destination = game::NO_STAR;
                        other_ship->prior_jump_gate_stardate = 0;
                    }
                }
            }

            if (ship->status == 1 &&
                Ship_Uses_Jump_Gate(ship) &&
                Movement_Target(Movement_Location(*ship)) != target) {
                ship->prior_jump_gate_destination = ship->position.target == game::e_ship_target::star
                    ? ship->position.star_id.get() : game::NO_STAR;
                ship->prior_jump_gate_stardate = MOX::stardate;
            }
            if (Ship_Used_Jump_Gate_To(ship, target)) {
                ship->prior_jump_gate_destination = game::NO_STAR;
                ship->prior_jump_gate_stardate = 0;
            }

            ship->position.Set(destination);
            ship->turns_left = move_info->turns_left;
            ship->group_has_navigator = move_info->has_navigator;
            ship->travelling_speed = move_info->traveling_speed;
            Set_Ship_Uses_Jump_Gate(ship, move_info->has_jumpgate_route != 0);
            ship->status = move_status;

            if (ship->d.ship_type == SHIP_TYPE_COLONY || ship->d.ship_type == SHIP_TYPE_OUTPOST) {
                const auto planet = current_state.planets.find(ship->colonization_target);
                if (!normal_star || planet == current_state.planets.end() || planet->second.star_id != target.star_id.get()) {
                    SHIPS::Clear_Auto_Colonization_Flag(ship);
                }
            }

        }
        return true;
    }

    bool Order_Fleet_Move(std::span<const game::id_ship> ships, game::s_star_destination destination, const s_ship_move_info* result) {
        return Apply_Resolved_Move(Resolve_Fleet(ships), destination, result);
    }

    static void Apply_Movement_Result(t_fleet ships, game::s_star_destination destination, const s_ship_move_info& result) {
        if (result.moving) Apply_Resolved_Move(ships, destination, &result);
        else for (auto* ship : ships) ship->status = 8;
    }

    static e_fleet_evaluation Evaluate_Ship_Movement(t_fleet ships, game::s_star_destination destination,
        s_ship_move_info* move_info, const s_move_origin* origin);

    static bool Apply_Resolved_Player_Order(t_fleet selected, game::s_star_destination destination, s_ship_move_info* move_info) {
        if (selected.empty() || !move_info || !Find_Destination(destination)) return false;

        const auto first_before = *selected.front();
        // Retain the old extent limit and the selection's first-occurrence order.
        constexpr size_t capacity = MAX_SHIPS - 1;
        std::vector<s_ship_data*> target_list;
        std::unordered_set<const s_ship_data*> target_members;
        for (auto* ship : selected.first(std::min(selected.size(), capacity))) {
            if (target_members.insert(ship).second) target_list.push_back(ship);
        }

        std::vector<s_ship_data*> source_list;
        if (Ship_Is_Moving(&first_before)) {
            for (int row = 0; row < current_state.ship_count && source_list.size() < capacity; ++row) {
                auto* ship = &current_state.ship_records[row];
                if (ship->id != game::NO_SHIP && Ship_Is_In_Same_Moving_Fleet(&first_before, ship) && !target_members.contains(ship)) {
                    source_list.push_back(ship);
                }
            }
        }

        const s_ship_data* target_baseline = nullptr;
        for (int row = 0; row < current_state.ship_count && target_list.size() < capacity; ++row) {
            auto* ship = &current_state.ship_records[row];
            if (ship->id != game::NO_SHIP && !target_members.contains(ship) &&
                Ship_Is_Moving(ship) &&
                ship->owner == first_before.owner &&
                ship->status == first_before.status &&
                ship->x == first_before.x &&
                ship->y == first_before.y &&
                Movement_Target(Movement_Location(*ship)) == destination) {
                if (!target_baseline) target_baseline = ship;
                target_members.insert(ship);
                target_list.push_back(ship);
            }
        }
        std::erase_if(source_list, [&](auto* ship) { return target_members.contains(ship); });

        s_ship_move_info target_info{};
        if (Evaluate_Ship_Movement(target_list, destination, &target_info, nullptr) != e_fleet_evaluation::can_move) return false;

        s_ship_move_info source_info{};
        game::s_star_destination source_destination;
        if (!source_list.empty()) {
            source_destination = Movement_Target(Movement_Location(*source_list.front()));
            Evaluate_Ship_Movement(source_list, source_destination, &source_info, nullptr);
        }

        const auto* source_baseline = source_list.empty() ? nullptr : source_list.front();
        if (Needs_Fleet_Confirmation(target_baseline, target_info) || Needs_Fleet_Confirmation(source_baseline, source_info)) {
            // Confirmation pumps network updates. Don't use the resolved pointers afterward.
            const auto target_before = Capture_Fleet(target_list);
            const auto source_before = Capture_Fleet(source_list);
            const auto date = MOX::stardate;
            const auto source_copy = source_baseline ? std::optional<s_ship_data>{*source_baseline} : std::nullopt;
            if (!Confirm_Fleet_Change(target_baseline, &target_info) ||
                !Confirm_Fleet_Change(source_copy ? &*source_copy : nullptr, &source_info) ||
                MOX::stardate != date || !Find_Destination(destination) ||
                (source_info.moving && !Find_Destination(source_destination)) ||
                !Rebind_Fleet(target_before, target_list) || !Rebind_Fleet(source_before, source_list)) return false;
        }

        Apply_Resolved_Move(target_list, destination, &target_info);
        Apply_Movement_Result(source_list, source_destination, source_info);
        *move_info = target_info;
        return true;
    }

    bool Apply_Player_Movement_Order(std::span<const game::id_ship> keys, game::s_star_destination destination, s_ship_move_info* result) {
        return Apply_Resolved_Player_Order(Resolve_Fleet(keys), destination, result);
    }

    /**
     * Clears the just_built flag on AI-controlled ships.
     */
    void __cdecl Unmark_Just_Built_AI_Ships() {
        int32_t num_ships = game::current_state.ship_count;
        auto& ship_array = current_state.ship_records;

        for (int16_t i = 0; i < num_ships; ++i) {
            s_ship_data& ship = ship_array[i];

            // AI players (owner 0-7) who aren't the primary human player
            // have their just_built flag cleared.
            if (ship.owner < MAX_PLAYERS && MOX::player[(int32_t)ship.owner].objectives != PLAYER_OBJECTIVE_HUMAN) {
                ship.just_built = 0;
            }
        }
    }

    bool Stop_Ship_Transit(s_ship_data* ship, std::optional<uint32_t> current_star) {
        if (current_star && *current_star > MAX_STARS) return false;
        const auto location = current_star
            ? legacy_world::Location_At_Position(game::e_ship_location::at_star, static_cast<int32_t>(*current_star), game::ANTARES_POSITION)
            : game::NO_SHIP_LOCATION;
        ship->position.Set(location);
        ship->status = 0;
        return true;
    }

    /** Finalizes arrival, including coordinates, visibility and mission effects. */
    void __cdecl Make_Ship_Arrive_At_Star(s_ship_data* ship, int16_t star_idx) {
        if (star_idx < 0 || !Stop_Ship_Transit(ship, (uint32_t)star_idx)) return;
        Set_Ship_Uses_Jump_Gate(ship, false);
        ship->prior_jump_gate_destination = game::NO_STAR;
        ship->prior_jump_gate_stardate = 0;
        auto& destination = game::Destination_Record_At_Row(current_state, star_idx);
        ship->x = destination.x;
        ship->y = destination.y;
        ship->mission = 0;

        destination.ignore_combat_ships = 0;

        int8_t owner = ship->owner;

        if (owner < MAX_PLAYERS) {
            // Check for Orion discovery
            if (destination.system_special == SYSTEM_SPECIAL_ORION_STAR && destination.visited == 0) {
                EVENTS::Event_Orion_Discovered((int16_t)owner);
            }

            // Update colony ship arrival flags
            if (ship->d.ship_type == SHIP_TYPE_COLONY || ship->d.ship_type == SHIP_TYPE_OUTPOST) {
                destination.ignore_colony_ships &= ~(1 << owner);
            }

            // Mark system as visited by player
            if (!((destination.visited >> owner) & 1)) {
                destination.visited |= (1 << owner);
                destination.just_visited |= (1 << owner);
            }
        } else if (owner == 13 /* Monster/Antaran */) {
            ship->d.date_of_design = 0;
        }

        if (owner < MAX_PLAYERS) {
            REPORT::something_interesting_happened[owner] = 1;

            if (MOX::player[owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
                destination.not_used3 = 0;
            }
        }
    }

    static int32_t Phase_Shifter_Speed(int32_t base_speed) {
        const auto [numerator, denominator] = phase_shifter_speed_multiplier;
        if (base_speed <= 0 || numerator == denominator) {
            return base_speed;
        }
        if (numerator <= 0) {
            return 0;
        }
        // UINT8_MAX is reserved for instantaneous travel routes.
        return std::clamp(base_speed * numerator / denominator, 1, UINT8_MAX - 1);
    }

    int32_t Effective_FTL_Speed(int16_t owner, int16_t drive, bool has_phase_shifter) {
        if (drive <= DRIVE_NO_DRIVE || drive >= DRIVE_COUNT) {
            return 0;
        }
        const int32_t base_speed = (owner >= 0 && owner < MAX_PLAYERS)
            ? INITSHIP::Get_FTL_Speed(owner, drive)
            : TECHDATA::drives[drive].warp_speed;
        return has_phase_shifter ? Phase_Shifter_Speed(base_speed) : base_speed;
    }

    /**
     * Evaluates whether the listed ships can move to the target star and fills out move_info.
     * @param ship_list Sentinel-terminated ship index list.
     * @param target_star_idx Destination star index.
     * @param move_info Output movement analysis record.
     */
    static e_fleet_evaluation Evaluate_Ship_Movement(t_fleet ships, game::s_star_destination destination,
        s_ship_move_info* move_info, const s_move_origin* origin) {
        if (!move_info) return e_fleet_evaluation::invalid_input;
        *move_info = {};
        const auto* target_star = Find_Destination(destination);
        if (ships.empty() || !target_star) return e_fleet_evaluation::invalid_input;
        const bool to_antares = destination.kind == game::e_star_destination::antares;
        const auto target_star_idx = game::Destination_Row(current_state, destination);
        int16_t start_x = 0;
        int16_t start_y = 0;
        int32_t navigator_bonus = 0;
        int32_t min_range = 30000;
        int8_t have_start_position = 0;
        bool any_phase_shifter = false;
        bool all_phase_shifters = true;
        int8_t player_idx;
        int32_t ship_speed;

        const auto* first = ships.front();
        const auto first_position = Movement_Position(*first, origin);
        player_idx = first->owner;

        if (player_idx < MAX_PLAYERS) {
            const s_ship_data* first_ship = first;
            if (first_ship->status == 1) {
                uint8_t departure_speed = first_ship->travelling_speed;
                departure_speed -= (uint8_t)first_ship->group_has_navigator;
                if (Ship_Uses_Jump_Gate(first_ship)) {
                    departure_speed -= jump_gate_speed_bonus;
                }
                ship_speed = departure_speed;
            } else {
                ship_speed = (uint8_t)MOX::player[player_idx].ship_speed;
            }
        } else {
            ship_speed = 1;
        }

        if (!to_antares && player_idx < MAX_PLAYERS) {
            if (MOX::player[player_idx].traits[TRAIT_TRANS_DIMENSIONAL] == 0) {
                if (EVENTS::Event_Check_Hyperspace_Flux() != 0 &&
                    first->status != 1) {
                    move_info->hyperspace_flux = 1;
                    return e_fleet_evaluation::blocked;
                }
            }
        }

        for (auto* ship : ships) {
            int16_t range = SHIPMOVE::Ship_Range(ship);
            bool has_phase_shifter = struct_::Test_Bit_Field(ship->d.special_device_flags, SPECIAL_PHASE_SHIFTER) != 0;
            any_phase_shifter |= has_phase_shifter;
            all_phase_shifters &= has_phase_shifter;

            if (ship->officer_index != -1) {
                auto* leader = &current_state.leaders.at(ship->officer_index);
                if (leader->eta == 0) {
                    int32_t bonus = 0;
                    if ((leader->special_skills & LEADER_SHIP_SKILL_NAVIGATOR2) != 0) {
                        bonus = (OFFICER::Owned_Officer_Level(ship->officer_index) / 3) + 1;
                    } else if ((leader->special_skills & LEADER_SHIP_SKILL_NAVIGATOR) != 0) {
                        bonus = (OFFICER::Owned_Officer_Level(ship->officer_index) / 4) + 1;
                    }
                    if (navigator_bonus < bonus) {
                        navigator_bonus = bonus;
                    }
                }
            }

            if (have_start_position == 0) {
                have_start_position = 1;
                start_x = first_position.x;
                start_y = first_position.y;
            }

            if (range < min_range) {
                min_range = range;
            }
        }

        // In-flight player fleets already include the multiplier in their departure speed.
        bool phase_speed_modified = any_phase_shifter &&
            phase_shifter_speed_multiplier.numerator != phase_shifter_speed_multiplier.denominator;
        bool scale_phase_speed = phase_speed_modified &&
            (first->status != 1 || player_idx >= MAX_PLAYERS);
        if (scale_phase_speed) {
            const int32_t phase_speed = Phase_Shifter_Speed(ship_speed);
            ship_speed = all_phase_shifters ? phase_speed : std::min(ship_speed, phase_speed);
        }

        int16_t start_star_id = HAROLD::XY_To_Star_Id(start_x, start_y);
        auto* start_star = start_star_id >= 0
            ? &game::Destination_Record_At_Row(current_state, start_star_id) : nullptr;
        bool continuing_existing_route = first->status == 1 &&
            first_position.location.kind == game::e_ship_location::in_flight &&
            Movement_Target(first_position.location) == destination;
        bool continuing_jump_gate_route = Fleet_Uses_Jump_Gate(ships, destination, origin);

        // Dimensional Portal logic for Antares
        if (to_antares && start_star) {
            if ((start_star->has_dimensional_portal >> (player_idx & 0x1F)) & 1) {
                move_info->turns_left = 1;
                move_info->moving = 1;
                move_info->traveling_speed = UINT8_MAX;
                return e_fleet_evaluation::can_move;
            }
        }

        if (player_idx < MAX_PLAYERS) {
            const game::s_view world{.players = MOX::player, .state = &current_state};
            if (!SHIPMOVE::Star_In_Range_Of_Player(world, *target_star, (int16_t)player_idx, (int16_t)min_range)) {
                const auto closest_star = to_antares || target_star_idx < 0 ? -1
                    : COMBFIND::Closest_Other_Colony_Star((int16_t)player_idx, target_star_idx);
                if (closest_star != -1) {
                    move_info->parsecs = (uint8_t)GEO::Parsecs_Between_Stars(closest_star, target_star_idx);
                }
                move_info->out_of_range = 1;
            }

            if (start_star) {
                if (!to_antares && start_star->wormhole_star_id == destination.star_id.get()) {
                    move_info->uses_wormhole = 1;
                }

                if (((start_star->has_stargate >> (player_idx & 0x1F)) & 1) &&
                    ((target_star->has_stargate >> (player_idx & 0x1F)) & 1)) {
                    move_info->has_stargate_route = 1;
                }

                if (((start_star->has_jumpgate >> (player_idx & 0x1F)) & 1) &&
                    ((target_star->has_jumpgate >> (player_idx & 0x1F)) & 1)) {
                    move_info->has_jumpgate_route = 1;
                }

                if (!to_antares && target_star_idx >= 0 && struct_::Test_Bit_Field(start_star->black_hole_blocks, target_star_idx)) {
                    if (navigator_bonus == 0 && move_info->uses_wormhole == 0) {
                        if (move_info->has_stargate_route == 0 && move_info->has_jumpgate_route == 0) {
                            move_info->blackhole_blocks = 1;
                        }
                    }
                }
            } else {
                if (continuing_jump_gate_route) {
                    move_info->has_jumpgate_route = 1;
                }
                if (GEO::Black_Hole_Blocks_Points(start_x, start_y, target_star->x, target_star->y)) {
                    move_info->blackhole_blocks = 1;
                }
            }
        }

        int32_t base_speed = ship_speed + navigator_bonus;
        if (move_info->has_stargate_route == 0 && move_info->uses_wormhole == 0) {
            if (ship_speed <= 0) {
                move_info->immobile = 1;
            } else {
                int16_t cur_x = start_x;
                int16_t cur_y = start_y;
                uint8_t turn_count = 0;
                if (move_info->has_jumpgate_route != 0) {
                    base_speed += jump_gate_speed_bonus;
                }
                if (phase_speed_modified) {
                    // UINT8_MAX is reserved for instantaneous travel routes.
                    base_speed = std::min(base_speed, int32_t{UINT8_MAX - 1});
                }

                while (cur_x != target_star->x || cur_y != target_star->y) {
                    int8_t unused_direction;
                    int8_t unused_navigator_flag;
                    turn_count++;
                    GEO::Move_Player_1_Turn_To_Star(
                        (uint8_t)player_idx,
                        cur_x,
                        cur_y,
                        *target_star,
                        (int16_t)base_speed,
                        &cur_x,
                        &cur_y,
                        (navigator_bonus == 0 &&
                            (!jump_gate_pass_nebula || move_info->has_jumpgate_route == 0))
                            ? &unused_navigator_flag
                            : nullptr,
                        &unused_direction);
                }
                move_info->turns_left = turn_count;
            }
        } else {
            move_info->turns_left = 0;
            if (game::Destination_At_Row(current_state, start_star_id) != destination) {
                move_info->turns_left = 1;
            }
        }

        move_info->has_navigator = (int8_t)navigator_bonus;

        // Final validation
        if ((move_info->blackhole_blocks != 0 &&
                move_info->has_jumpgate_route == 0 &&
                move_info->has_stargate_route == 0 &&
                navigator_bonus <= 0) ||
            (move_info->out_of_range != 0 && !continuing_existing_route) ||
            (ship_speed == 0 &&
                !continuing_existing_route &&
                (target_star->x != start_x || target_star->y != start_y))) {
            move_info->turns_left = -1;
            move_info->moving = 0;
            return e_fleet_evaluation::blocked;
        }

        if (move_info->has_stargate_route == 0 && move_info->uses_wormhole == 0) {
            move_info->moving = 1;
            move_info->traveling_speed = (uint8_t)base_speed;
        } else {
            move_info->traveling_speed = UINT8_MAX;
            move_info->moving = 1;
        }
        return e_fleet_evaluation::can_move;
    }

    e_fleet_evaluation Evaluate_Fleet_Move(std::span<const game::id_ship> keys, game::s_star_destination destination,
        s_ship_move_info* result, std::optional<game::id_star> origin) {
        std::optional<s_move_origin> start;
        if (origin) {
            const auto star = current_state.stars.find(*origin);
            if (star == current_state.stars.end()) {
                if (result) *result = {};
                return e_fleet_evaluation::invalid_input;
            }
            start = s_move_origin{{{game::e_ship_location::at_star, *origin}, star->second.x, star->second.y}};
        }
        return Evaluate_Ship_Movement(Resolve_Fleet(keys), destination, result, start ? &*start : nullptr);
    }

    /**
     * Recomputes ETAs for moving ships that do not belong to the specified player.
     * @param player_id Player index to exclude from the update.
     */
    void __cdecl Update_Ship_ETAs(int16_t player_id) {
        int num_ships = game::current_state.ship_count;
        auto& ships = current_state.ship_records;

        for (int16_t i = 0; i < num_ships; ++i) {
            s_ship_data* ship = &ships[i];

            // Only process ships that are active/moving and do not belong to player_id
            if ((ship->status == 1 || ship->status == 2) && (int16_t)ship->owner != (int16_t)player_id) {
                const auto dest_star_idx = SHIPMOVE::Ship_Destination_Star(ship);
                if (dest_star_idx < 0) continue;
                const auto& star = game::Destination_Record_At_Row(current_state, dest_star_idx);

                int16_t target_x = star.x;
                int16_t target_y = star.y;

                int16_t current_x = ship->x;
                int16_t current_y = ship->y;
                uint8_t turn_count = 0;

                int8_t unused_navigator_flag;
                int8_t unused_direction;

                // Simulate movement turn by turn until destination is reached
                while (current_x != target_x || current_y != target_y) {
                    turn_count++;

                    int8_t* navigator_ptr = nullptr;
                    if (ship->group_has_navigator == 0 &&
                        (!jump_gate_pass_nebula || !Ship_Uses_Jump_Gate(ship))) {
                        navigator_ptr = &unused_navigator_flag;
                    }

                    GEO::Move_Player_1_Turn_To_Star(
                        ship->owner,
                        current_x,
                        current_y,
                        star,
                        (int16_t)(uint8_t)ship->travelling_speed,
                        &current_x,
                        &current_y,
                        navigator_ptr,
                        &unused_direction);
                }

                ship->turns_left = turn_count;
            }
        }
    }

    bool Confirm_Officer_Change(int16_t leader_idx, s_ship_data* target, int8_t target_eta) {
        if (leader_idx < 0 || leader_idx >= MAX_LEADERS ||
            current_state.leaders.at(leader_idx).type != LEADER_TYPE_SHIP) {
            return true;
        }

        auto* leader = &current_state.leaders.at(leader_idx);
        auto* source = game::Find_Leader_Ship(current_state, *leader);
        const auto source_officer_idx = source ? source->officer_index : -1;
        const auto target_officer_idx = target ? target->officer_index : -1;

        int8_t old_eta = leader->eta;
        if (source) source->officer_index = -1;
        if (target) {
            target->officer_index = leader_idx;
            leader->eta = target_eta;
        } else {
            leader->eta = 0;
        }

        const auto source_list = Moving_Fleet(source);
        auto target_list = Moving_Fleet(target);
        s_ship_move_info source_info{};
        s_ship_move_info target_info{};
        if (!source_list.empty()) {
            const auto destination = Movement_Target(Movement_Location(*source_list.front()));
            Evaluate_Ship_Movement(source_list, destination, &source_info, nullptr);
        }
        if (!target_list.empty()) {
            if (std::ranges::find(source_list, target_list.front()) != source_list.end()) {
                target_list.clear();
            } else {
                const auto destination = Movement_Target(Movement_Location(*target_list.front()));
                Evaluate_Ship_Movement(target_list, destination, &target_info, nullptr);
            }
        }

        if (source) source->officer_index = source_officer_idx;
        if (target) target->officer_index = target_officer_idx;
        leader->eta = old_eta;

        const auto target_before_confirmation = target_list.empty()
            ? std::optional<s_ship_data>{} : std::optional<s_ship_data>{*target_list.front()};
        return Confirm_Fleet_Change(source_list.empty() ? nullptr : source_list.front(), &source_info) &&
            Confirm_Fleet_Change(target_before_confirmation ? &*target_before_confirmation : nullptr, &target_info);
    }

    static void Recalculate_Fleet(t_fleet ships) {
        if (ships.empty()) return;
        const auto destination = Movement_Target(Movement_Location(*ships.front()));
        s_ship_move_info result{};
        Evaluate_Ship_Movement(ships, destination, &result, nullptr);
        Apply_Movement_Result(ships, destination, result);
    }

    void Recalculate_Moving_Fleet(const s_ship_data* ship) {
        Recalculate_Fleet(Moving_Fleet(ship));
    }

    void __cdecl Recalculate_All_Moving_Fleets() {
        std::unordered_set<const s_ship_data*> processed;
        // Departures consume RNG in roster order.
        for (int row = 0; row < current_state.ship_count; ++row) {
            const auto* ship = &std::as_const(current_state.ship_records)[row];
            if (processed.contains(ship)) continue;
            const auto fleet = Moving_Fleet(ship);
            processed.insert(fleet.begin(), fleet.end());
            Recalculate_Fleet(fleet);
        }
    }

    /**
     * Clears ignore-combat flags from stars where no opposing force remains.
     */
    void __cdecl Reset_Needless_Ignore_Combat_Ships_Flags() {
        uint16_t presence_mask[MAX_STARS]; // Tracks which players have assets at each star
        memset(presence_mask, 0, sizeof(presence_mask));

        // Mark player presence via ships
        if (game::current_state.ship_count > 0) {
            for (int16_t i = 0; i < game::current_state.ship_count; ++i) {
                s_ship_data* ship = &game::current_state.ship_records[i];
                // Only active ships with a valid owner count
                if (ship->owner != -1 && ship->status == 0) {
                    const auto slot = Destination_At_Location(ship->position.Get());
                    if (slot >= 0 && slot < std::ssize(current_state.stars)) presence_mask[slot] |= (1 << ship->owner);
                }
            }
        }

        // Mark player presence via colonies
        if (!game::current_state.colonies.empty()) {
            for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
                s_colony* colony = &roster_colony;
                if (colony->owner != -1) {
                    int16_t star_index = game::Star_Row(current_state, current_state.planets.at(colony->planet_id).star_id);
                    if (star_index >= 0) presence_mask[star_index] |= (1 << colony->owner);
                }
            }
        }

        // Review stars and clear ignore_combat_ships flags where no longer applicable
        if (std::ssize(current_state.stars) > 0) {
            for (int16_t i = 0; i < std::ssize(current_state.star_order); ++i) {
                const auto star_id = current_state.star_order[i];
                s_star_data* star = &game::current_state.stars.at(star_id);

                if (star->ignore_combat_ships != 0) {
                    for (int player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                        // If the flag is set for this player at this star
                        if ((star->ignore_combat_ships >> player_idx) & 1) {
                            // Clear the flag if no other player has presence at this star
                            if ((presence_mask[i] & ~(1 << player_idx)) == 0) {
                                star->ignore_combat_ships &= ~(1 << player_idx);
                            }
                        }
                    }
                }
            }
        }
    }

    /**
     * Returns whether the player has an orbiting ship at a star colonized only by another player.
     * @param player_idx Player whose ships are checked.
     * @param colony_bit_idx Colony-owner bit to test for.
     */
    bool __cdecl Player_Has_Ships_Pestering_Other_Player(int player_idx, uint8_t colony_bit_idx) {
        return game::Any_Ship(current_state, [&](const s_ship_data& ship) {
            if (ship.owner != player_idx || ship.status != 0 || ship.position.target != game::e_ship_target::star) return false;
            const auto star = current_state.stars.find(ship.position.star_id.get());
            return star != current_state.stars.end() && game::Has_Player_Colony(star->second, colony_bit_idx) &&
                !game::Has_Player_Colony(star->second, player_idx);
        });
    }

    /**
     * Returns the number of turns the player needs to travel between two stars.
     * @param player_idx Player whose travel rules are used.
     * @param start_star_idx Starting star index.
     * @param end_star_idx Destination star index.
     */
    int16_t __cdecl Player_N_Turns_From_Star_To_Star(int16_t player_idx, int16_t start_star_idx, int16_t end_star_idx) {
        const game::s_view live{.players = MOX::player, .state = &game::current_state};
        return Player_N_Turns_From_Star_To_Star(live, player_idx,
            game::Star_Key_At_Row(*live.state, start_star_idx), game::Star_Key_At_Row(*live.state, end_star_idx));
    }

    int16_t Player_N_Turns_From_Star_To_Star(const game::s_view& world, int16_t player_idx,
        game::id_star source, game::id_star destination) {
        const auto from = world.state->stars.find(source);
        const auto to = world.state->stars.find(destination);
        const auto end_star_idx = game::Star_Row(*world.state, destination);
        if (from == world.state->stars.end() || to == world.state->stars.end() || end_star_idx < 0) return -1;
        if (source == destination) return 0;
        const auto* start_star = &from->second;
        const auto* target_star = &to->second;
        uint8_t player_bit = (uint8_t)player_idx & 0x1F;

        // Check stargate connectivity (player bitmask)
        if (((start_star->has_stargate >> player_bit) & 1) &&
            ((target_star->has_stargate >> player_bit) & 1)) {
            return 1;
        }

        // Check wormhole link
        if (start_star->wormhole_star_id == destination) {
            return 1;
        }

        int16_t speed = world.players[player_idx].ship_speed;
        bool is_jumpgate_used = false;

        // Check jumpgate connectivity (player bitmask)
        if (((start_star->has_jumpgate >> player_bit) & 1) &&
            ((target_star->has_jumpgate >> player_bit) & 1)) {
            speed += jump_gate_speed_bonus;
            is_jumpgate_used = true;
        }

        if (!is_jumpgate_used) {
            // Black holes block travel unless jumpgates are used
            if (struct_::Read_Bit_Field(start_star->black_hole_blocks, end_star_idx)) {
                return -1;
            }
        }

        if (speed == 0) {
            return -1;
        }

        int16_t current_x = start_star->x;
        int16_t current_y = start_star->y;
        int16_t target_x = target_star->x;
        int16_t target_y = target_star->y;
        int16_t n_turns = 0;

        int8_t unused_navigator_flag;
        int8_t unused_direction;

        while (current_x != target_x || current_y != target_y) {
            n_turns++;
            GEO::Move_Player_1_Turn_To_Star(
                (uint8_t)player_idx,
                current_x,
                current_y,
                *target_star,
                (int16_t)speed,
                &current_x,
                &current_y,
                (jump_gate_pass_nebula && is_jumpgate_used) ? nullptr : &unused_navigator_flag,
                &unused_direction);
        }

        return n_turns;
    }

    /**
     * Resolves the Hyperspace Beast event for one player's moving ships.
     * @param player_idx Owning player to search, or -1 for no eligible ship.
     */
    void __cdecl Do_Hyperspace_Beast(int32_t player_idx) {
        s_MSG msg;
        msg.msg_type = MSG_TYPE_HYPERSPACE_BEAST;

        s_ship_data* selected_ship = nullptr;
        int16_t ship_index = -1;

        if (player_idx != -1) {
            int32_t candidate_count = 0;
            for (int16_t i = 0; i < game::current_state.ship_count; ++i) {
                s_ship_data* ship = &game::current_state.ship_records[i];
                if (ship->owner == player_idx && ship->status == 1) {
                    candidate_count++;
                    if (game_random::Random(candidate_count) == 1) {
                        selected_ship = ship;
                        ship_index = i;
                    }
                }
            }
        }

        if (selected_ship == nullptr) {
            msg.u.hyperspace_beast.ship_idx = -1;
        } else {
            int16_t officer_index = selected_ship->officer_index;

            game::Set_Ship_Notice(msg.u.hyperspace_beast, ship_index, selected_ship->id);
            msg.u.hyperspace_beast.officer_idx = (int8_t)officer_index;
            msg.u.hyperspace_beast.destination = {static_cast<game::e_star_destination>(selected_ship->position.target), selected_ship->position.star_id.get()};

            if (officer_index != -1) {
                OFFICER::Move_Officer_To_Pool(officer_index);

                auto* leader = &current_state.leaders.at(officer_index);
                if (leader->eta > 0) {
                    msg.u.hyperspace_beast.leader_status = 1;
                } else {
                    if (MOX::player[(uint8_t)selected_ship->owner].tech_applications[TECH_APP_SURVIVAL_PODS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                        s_ship_data* replacement_ship = nullptr;
                        int16_t replacement_row = -1;
                        bool found_similar = false;

                        for (int16_t i = 0; i < game::current_state.ship_count; ++i) {
                            s_ship_data* ship = &game::current_state.ship_records[i];
                            if (ship->owner == player_idx &&
                                ship->status == selected_ship->status &&
                                ship->x == selected_ship->x &&
                                ship->y == selected_ship->y &&
                                ship != selected_ship &&
                                ship->position.Get() == selected_ship->position.Get() &&
                                ship->travelling_speed == selected_ship->travelling_speed) {
                                found_similar = true;

                                if (ship->officer_index == -1) {
                                    if (replacement_ship == nullptr ||
                                        replacement_ship->d.size < ship->d.size) {
                                        if (ship->d.ship_type == SHIP_TYPE_COMBAT) {
                                            replacement_ship = ship;
                                            replacement_row = i;
                                        }
                                    }
                                }
                            }
                        }

                        if (replacement_ship != nullptr) {
                            replacement_ship->officer_index = officer_index;
                            game::Set_Leader_Ship(*leader, *replacement_ship, replacement_row);
                            msg.u.hyperspace_beast.leader_status = 2;
                            leader->status = 1;
                        } else {
                            msg.u.hyperspace_beast.leader_status = 1;
                            if (!found_similar) {
                                msg.u.hyperspace_beast.leader_status = 0;
                            }
                        }
                    } else {
                        msg.u.hyperspace_beast.leader_status = 0;
                    }

                    if (msg.u.hyperspace_beast.leader_status == 0) {
                        OFFICER::Kill_Officer(officer_index);
                    }
                }
            }

            MAINSCR::Update_Ship_Icons_In_Between_Turns();
            selected_ship->status = 8;
        }

        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            msg.u.hyperspace_beast.fate = (player_idx == i) ? 1 : 0;
            MSG_::Add_Msg(msg, i);
        }
    }

    /**
     * Resolves the wormhole event by selecting one moving fleet and snapping matching ships to arrival.
     * @param player_idx Player whose ships are eligible.
     * @param wormhole Output event result record.
     */
    void __cdecl Do_Wormhole_Event(int32_t player_idx, s_wormhole_data* wormhole) {
        *wormhole = {};

        int32_t candidate_count = 0;
        s_ship_data* selected_ship = nullptr;
        int16_t selected_star = -1;

        for (int16_t i = 0; i < game::current_state.ship_count; ++i) {
            s_ship_data* ship = &game::current_state.ship_records[i];
            if (ship->owner == player_idx && ship->status == 1 && ship->turns_left != 0) {
                const auto location = Movement_Location(*ship);
                if (location.kind != game::e_ship_location::in_flight) continue;
                const auto star_idx = Destination_At_Location(location);
                if (star_idx < 0) continue;
                candidate_count++;
                if (game_random::Random(candidate_count) == 1) {
                    selected_ship = ship;
                    selected_star = star_idx;
                }
            }
        }

        if (selected_ship != nullptr) {
            const auto location = Movement_Location(*selected_ship);
            int32_t x = selected_ship->x;
            int32_t y = selected_ship->y;
            wormhole->success = 1;
            wormhole->destination = game::Destination_At_Row(current_state, selected_star);

            for (int16_t i = 0; i < game::current_state.ship_count; ++i) {
                s_ship_data* ship = &game::current_state.ship_records[i];
                if (ship->owner == player_idx &&
                    ship->status == 1 &&
                    ship->x == x &&
                    ship->y == y &&
                    Movement_Location(*ship) == location &&
                    ship->travelling_speed == selected_ship->travelling_speed) {
                    Make_Ship_Arrive_At_Star(ship, selected_star);
                    wormhole->ships_moved++;
                }
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();
    }
} // namespace SHIPMOVE
