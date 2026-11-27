#pragma once

#include "consts.h"
#include "settler_data.h"
#include "planning_context.h"

struct s_colony;
struct s_player;
struct s_settler_settle_result;

namespace game {
    struct s_game_state;
}

namespace SETTLER {
    void Pop_Tries_To_Settle(const game::s_game_state& state, game::id_colony source, int16_t pop,
        game::id_colony destination, s_settler_settle_result* result, const game::s_planning_context& planning);
    extern bool allow_last_population_transfer;
    extern uint16_t max_pop_freighted;


    /**
     * Returns how many freighters the player can scrap without stranding settlers already in transit.
     * @param player_idx Player index to inspect.
     */
    int32_t __cdecl N_Freighters_Player_Can_Scrap(game::id_player player_idx);

    /**
     * Checks whether the colony can accept one more pop of the specified race.
     * @param colony Colony being checked.
     * @param race_idx Race index of the incoming pop.
     * @param include_freighters Nonzero to count settlers already heading to the colony.
     */
    bool __cdecl Room_For_Another(const s_colony* colony, int16_t race_idx, int8_t include_freighters);
    bool Room_For_Another(const game::s_game_state& state, const s_colony* colony, int16_t race_idx, int8_t include_freighters);

    /**
     * Adds an encoded settler record to the colony if capacity allows.
     * @param settler The settler joining the colony.
     * @param colony Destination colony receiving the pop.
     */
    int8_t __cdecl Add_Settler_To_Colony(const s_settler_info& settler, s_colony* colony);

    /**
     * Advances all in-transit settlers by one turn and resolves arrivals.
     */
    void __cdecl Move_Settlers();

    /**
     * Validates whether the selected pop can settle the destination colony and records the result flags.
     * @param state State containing the source and destination planets.
     * @param source_id Colony providing the pop.
     * @param selected_pop_idx Pop index selected in the source colony.
     * @param destination_id Destination colony to receive the pop.
     * @param result Output structure filled with settlement validation flags.
     */
    void __cdecl Pop_Tries_To_Settle(
        const game::s_game_state& state,
        game::id_colony source_id,
        int16_t selected_pop_idx,
        game::id_colony destination_id,
        s_settler_settle_result* result);

    /**
     * Computes the number of turns needed to move a settler between two stars.
     * @param player Player whose ship speed determines travel time.
     * @param source_star Source star index.
     * @param dest_star Destination star index.
     */

    /**
     * Moves one pop from the source colony to the destination colony or dispatches it into transit.
     * @param state State containing the source and destination planets.
     * @param source_id Colony losing the pop.
     * @param pop_index Pop index to move from the source colony.
     * @param destination_id Destination colony receiving the pop.
     */
    // Updates the active game, including its legacy player freight storage.
    void __cdecl Settle_Pop(game::id_colony source_id,
        int16_t pop_index, game::id_colony destination_id);
} // namespace SETTLER
