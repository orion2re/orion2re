#pragma once
#include <cstdint>
#include "object_ids.h"

struct s_player;

namespace TRADE {
    /**
     * Starts a bilateral trade agreement between two players.
     * @param player Player starting the agreement.
     * @param other Player on the other side of the agreement.
     */
    void __cdecl Start_Trade_Agreement(s_player* player, game::id_player other);

    /**
     * Starts a bilateral research agreement between two players.
     * @param player Player starting the agreement.
     * @param other Player on the other side of the agreement.
     */
    void __cdecl Start_Research_Agreement(s_player* player, game::id_player other);

    /**
     * Advances one player's stored trade-agreement progress with another player.
     * @param player Player whose agreement state is updated.
     * @param other Player on the other side of the agreement.
     */
    void __cdecl Process_Trade_Agreement(s_player* player, game::id_player other);

    /**
     * Advances one player's stored research-agreement progress with another player.
     * @param player Player whose agreement state is updated.
     * @param other_player_idx Index of the other player in the agreement.
     */
    void __cdecl Process_Research_Agreement(s_player* player, game::id_player other);

    /** Recomputes each player's trade and research population counts from colony data. */
    void __cdecl Compute_N_Trade_And_Research_Pops();

    /** Advances all active trade and research agreements, then refreshes population totals. */
    void __cdecl Process_Trade_And_Research_Agreements();

    /**
     * Returns the unmodified trade-agreement goal shared by two players.
     * @param player One player in the agreement.
     * @param other_player_idx Index of the other player.
     */
    int16_t __cdecl Base_Trade_Agreement_Goal(s_player* player, game::id_player other);

    /**
     * Returns the trade-agreement goal after applying trait and leader modifiers.
     * @param player Player whose modifiers are applied.
     * @param other_player_idx Index of the other player.
     */
    int16_t __cdecl Trade_Agreement_Goal(s_player* player, game::id_player other);

    /**
     * Returns half of the smaller research-population count between two players.
     * @param player One player in the comparison.
     * @param other_player_idx Index of the other player.
     */
    int32_t __cdecl Get_Combined_Research_Points(s_player* player, game::id_player other);

    /**
     * Returns the research-agreement goal after applying government modifiers.
     * @param player Player whose modifiers are applied.
     * @param other_player_idx Index of the other player.
     */
    int32_t __cdecl Research_Agreement_Goal(s_player* player, game::id_player other);
} // namespace TRADE