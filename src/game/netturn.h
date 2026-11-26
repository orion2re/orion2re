#pragma once
#include <cstdint>

struct s_player;

#pragma pack(push, 1)
struct s_net_election_msg {
    int32_t rank;
    int8_t hit_next_turn;
};
#pragma pack(pop)

namespace NETTURN {
    /** Advances the networked end-of-turn flow for the local player. */
    void __cdecl Net_Next_Turn();

    /** Runs the host-side turn. Returns false if the host saved and left. */
    bool __cdecl Host_Next_Turn();

    /** Shows queued player-drop prompts between network operations. Returns true if the host saved and left. */
    bool Confirm_Player_Drops();

    /** Runs the client-side order submission and waits for host results. */
    void __cdecl Client_Next_Turn();

    /**
     * Loads packed net-turn UI strings from a contiguous string block.
     * @param string_block Pointer to the first packed string.
     * @return Pointer to the first unread byte after the consumed strings.
     */
    char* __cdecl Load_Netturn_Strings(char* string_block);

    extern char* waiting_message_fmt;

    /** Returns whether any AI-player sync mask still requires host broadcast data. */
    int8_t __cdecl Must_Broadcast_AI_Player_Data();

    /** Returns the lowest active player slot that can serve as the next-turn host, or -1 if none qualify. */
    int32_t __cdecl Determine_Lowest_Active_Host_Player(const s_player* players, int32_t num_players);

    /** Resets host-election and per-turn network sync state for the next turn. */
    void __cdecl Net_Init_Host_Election();
} // namespace NETTURN