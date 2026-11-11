#pragma once

#include <cstdint>

namespace npcdiplo {
    int32_t Tech_Trade_Value(int16_t player_idx, int16_t other_player_idx, int16_t tech_app_idx);
    extern s_npcdiplo_tech human_tech[TECH_APP_COUNT];

    extern s_npcdiplo_tech target_tech[TECH_APP_COUNT];
    extern uint16_t ai_surrender_delay;

    /// Computes the reaction score for a proposed technology exchange.
    /// @param player_idx Player evaluating the exchange.
    /// @param target_idx Other player involved in the exchange.
    /// @param reaction_bonus_flag Non-zero to use the fixed proposal-bonus path.
    int16_t __cdecl Get_Tech_Exchange_Reaction(int16_t player_idx, int16_t target_idx, int16_t reaction_bonus_flag);

    /// Compares technology entries by ascending value for qsort.
    int __cdecl compare_tech(const void* a, const void* b);

    /// Computes the expected value threshold for a technology exchange.
    /// @param player_idx Player evaluating the incoming offer.
    /// @param target_idx Other player involved in the exchange.
    int32_t __cdecl Get_Tech_Exchange_Expectation(int16_t player_idx, int16_t target_idx);

    /// Compares technology entries by descending value for qsort.
    int __cdecl reverse_compare_tech(const void* a, const void* b);

    /// Clears the cached last-proposal turn for both players in a diplomacy pair.
    /// @param player_id_1 First player in the pair.
    /// @param player_id_2 Second player in the pair.
    void __cdecl Clear_Last_Proposal_Turn(int16_t player_id_1, int16_t player_id_2);

    /// Updates the shared proposal-rejection state after a proposal is accepted or rejected.
    /// @param player_idx Player whose personality drives the adjustment.
    /// @param other_player_idx Other player in the proposal pair.
    /// @param accepted Non-zero when the proposal was accepted.
    void __cdecl NPC_Proposal_Rejection_Accept(int16_t player_idx, int16_t other_player_idx, int8_t accepted);

    /// Checks whether an NPC should send a diplomatic proposal to another player.
    /// @param npc_idx NPC evaluating the proposal chance.
    /// @param player_idx Other player being evaluated.
    bool __cdecl NPC_Good_Proposal_Check(int16_t npc_idx, int16_t player_idx);

    /// Prepares a technology-exchange proposal when the target has suitable trade candidates.
    /// @param player_idx Proposing player.
    /// @param target_player_idx Target player for the exchange.
    void __cdecl NPC_Tech_Exchange_Check(int16_t player_idx, int16_t target_player_idx);

    /// Runs the main NPC diplomacy turn pipeline.
    void __cdecl NPC_Diplomacy();

    /// Applies periodic relation penalties for empires that out-expand their rivals.
    void __cdecl NPC_Diplomacy_Buildup_Penalties();

    /// Updates ongoing diplomatic threat levels between empires.
    void __cdecl NPC_Threats();

    /// Runs AI-only treaty, trade, research, and tribute negotiations.
    void __cdecl NPC_To_NPC_Treaty_Negotiations();

    /// Forces a player to honor an alliance against eligible enemies.
    /// @param player_idx Player being checked for alliance obligations.
    /// @param ally_idx Allied player whose wars may be inherited.
    void __cdecl Honor_Alliances(int16_t player_idx, int16_t ally_idx);

    /// Breaks treaty combinations that conflict with existing inter-empire treaty states.
    void __cdecl Eliminate_NPC_Treaty_Conflicts();

    /// Placeholder hook with no current behavior.
    void __cdecl NoOp3();

    /// Evaluates and executes NPC declarations of war.
    void __cdecl NPC_Declarations_Of_War();

    /// Evaluates peace and surrender outcomes for ongoing NPC wars.
    void __cdecl NPC_Peace_Negotiations();

    /// Checks whether an NPC should surrender and selects a surrender target when possible.
    /// @param player_idx Player being evaluated for surrender.
    int16_t __cdecl NPC_Surrender_Check(int16_t player_idx);

    /// Builds the list of acceptable technology exchange pairs for two players.
    /// @param player_a Player offering technologies.
    /// @param player_b Player receiving technologies.
    /// @param out_count Receives the number of generated exchange entries.
    /// @param out_tech_index Receives candidate target technology indices.
    /// @param out_human_tech_index Receives matching offered technology indices.
    void __cdecl Get_Exchange_Tech_List(int16_t player_a, int16_t player_b, int16_t* out_count, uint8_t* out_tech_index, uint8_t* out_human_tech_index);

    /// Performs direct NPC-to-NPC technology trades.
    void __cdecl NPC_Tech_Exchange();

    /// Applies treaty jealousy penalties on higher difficulties.
    void __cdecl NPC_Treaty_Hatred();

    /// Builds a filtered list of technologies one player can gain from another.
    /// @param player_idx Player evaluating the technology values.
    /// @param other_player_idx Player who currently owns the technologies.
    /// @param min_tech_value Minimum accepted technology value.
    /// @param max_tech_value Maximum accepted technology value.
    /// @param tech_list_indices Receives matching technology IDs.
    /// @param tech_list_values Receives matching technology values.
    /// @param tech_count Receives the number of matching entries.
    /// @param sort_flag Non-zero to sort the result by descending value.
    void __cdecl Get_Differential_Tech_List(
        int16_t player_idx,
        int16_t other_player_idx,
        uint32_t min_tech_value,
        uint32_t max_tech_value,
        uint8_t* tech_list_indices,
        int32_t* tech_list_values,
        int16_t* tech_count,
        int16_t sort_flag);

    /// Rewrites a diplomacy message into a rejection when proposal fatigue is too high.
    /// @param player_idx Player evaluating the proposal.
    /// @param target_idx Other player in the exchange.
    /// @param msg_type Message type to inspect and overwrite when rejected.
    void __cdecl NPC_Message_Rejection_Check(int16_t player_idx, int16_t target_idx, int16_t* msg_type);
} // namespace npcdiplo