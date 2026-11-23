#pragma once

struct s_colors;
struct s_animation_header;
struct s_mouse_list;

namespace council {
    struct s_meeting_schedule {
        uint8_t first_meeting_delay;
        uint16_t meeting_interval;
    };

    extern s_meeting_schedule meeting_schedule;

    extern const char lbx_00559a68[12];
    extern const char LBX_00559a74[12];

    extern s_colors council_normal_colors;
    extern s_colors council_highlight_colors;
    extern game::id_player council_nominee_[2];
    extern int16_t council_total_votes;
    extern int16_t council_nominee_vote_total[2];
    extern game::id_player council_members[MAX_PLAYERS];
    extern uint8_t council_state;
    extern char* council_vote_message;
    extern s_animation_header* council_dark_race_seg[MAX_PLAYERS];
    extern game::id_player council_voter;
    extern char* council_no_votes_message;
    extern int16_t council_vote_cast;
    extern int16_t council_member_votes[MAX_PLAYERS];
    extern s_animation_header* council_light_race_seg[MAX_PLAYERS];
    extern char* council_message;
    extern uint8_t player_voted_winner_message_id[20];
    extern char* council_votes_message;
    extern int16_t council_nominee_votes[2];
    extern void* council_mouse_bitmap;
    extern game::id_player winning_player;
    extern s_animation_header* council_nominees_seg[2];
    extern uint8_t council_total_members;
    extern char* council_votes_total_message;
    extern uint8_t council_vote_for;

    /* Loads a random post-election reaction line into the council message buffer.
       unused_callback_arg: Unused callback argument preserved for the original interface. */
    void __cdecl Get_Player_Voted_Winner_Message(int16_t unused_callback_arg);

    /* Draws the post-election winner-reaction screen. */
    void __cdecl Draw_Council_Player_Voted_Winner();

    extern s_mouse_list council_blank_mouse;

    /* Plays the winner-reaction sequence for the specified player.
       player_idx: Player index whose reaction animation is shown.
       unused_callback_arg: Unused callback argument preserved for the original interface. */
    void Drive_Council_Player_Voted_Winner(game::id_player player, int16_t unused_callback_arg);

    /* Triggers the council meeting when the current game state satisfies the meeting conditions. */
    void Check_For_Council_Meeting();

    /* Advances the council state machine to the next high-level state. */
    void __cdecl Next_Council_State();

    /* Runs the full council screen sequence from setup through teardown. */
    void __cdecl Main_Council_Screen();

    /* Draws the main council screen and its current message and vote state. */
    void __cdecl Draw_Main_Council_Screen();

    /* Processes input for the main council screen until the current step is dismissed. */
    void __cdecl Control_Main_Council_Screen();

    /* Prompts the human player to cast a council vote. */
    void Council_Player_Vote();

    /* Returns the number of council votes controlled by the specified player.
       player_idx: Player index whose vote count is computed. */
    int16_t Get_Num_Votes(game::id_player player);

    /* Applies the result of a single council vote and updates diplomacy and state.
       vote_choice: Vote selection code for abstaining or choosing one of the nominees.
       player_idx: Player index that cast the vote. */
    void Council_Vote_Result(int8_t vote_choice, game::id_player player);

    /* Returns whether the voter supports the candidate under the current diplomatic state.
       candidate_idx: Player index of the candidate being evaluated.
       voter_idx: Player index of the empire casting the vote.
       opponent_idx: Player index of the opposing nominee. */
    bool Vote_Check(game::id_player candidate, game::id_player voter, game::id_player opponent);

    /* Rebuilds council assets, nominees, voter ordering, and localized text for the screen. */
    void __cdecl Reload_Main_Council_Screen();

    /* Loads the localized council vote-count text snippets. */
    void __cdecl Load_Council_Text_Messages();

    /* Refreshes the current council message text for the active council state. */
    void __cdecl Get_Council_Message();

    /* Resolves all AI and player council votes for the current meeting. */
    void __cdecl Council_Votes();

    /* Evaluates the final vote totals and advances to the result state. */
    void __cdecl Council_Finished_Voting();

    /* Prompts the player to accept or reject the council ruling when applicable. */
    void Council_Accept_Ruling();

    /* Plays the post-election winner-reaction sequence for the remaining council members. */
    void Council_Player_Voted_Winner();
} // namespace council