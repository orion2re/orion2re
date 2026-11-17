#pragma once

namespace REPORT {
    extern uint8_t no_outpost_prompt;
    extern uint8_t initial_tech_prompt;
    extern bool sort_serious_turn_summary_first;
    extern int16_t n_reports;
    extern s_report reports[MAX_REPORTS];
    extern uint8_t done_player_reports;
    extern uint8_t something_interesting_happened[MAX_PLAYERS];
    extern uint8_t done_colonize_check[BITMAP(MAX_STARS)];
    extern uint8_t displayed_msgs;
    extern uint8_t displayed_gnn;

    /** Resets per-client report display state for a new turn. */
    void __cdecl Client_Initialize_Reports();

    /** Clears report storage and per-turn report state. */
    void __cdecl Initialize_Reports();
    /** Appends a report when there is remaining report capacity. */
    void __cdecl Add_Report(s_report* report);

    extern char* your_spy_destroys_fmt;
    extern char* your_spy_steals_fmt;
    extern char* you_framed_fmt;
    extern char* colonize_preselected_planet_fmt;
    extern char* enemy_spy_destroys_fmt;
    extern char* really_trash_colony_base_fmt;
    extern char* enemy_spy_steals_fmt;
    extern char* outpost_preselected_planet_fmt;

    /** Loads report format strings from a packed NUL-separated string block. */
    char* __cdecl Load_Report_Strings(char* data);

    /**
     * Returns the player's colony base at the requested star, if one exists.
     * @param player_idx Player to search for.
     * @param star_idx Star to inspect.
     */
    s_colony* __cdecl Player_Colony_Base_At_Star(::game::s_world_state* world_state, int16_t player_idx, int16_t star_idx);
    /**
     * Returns the first matching ship type at a star for the requested owner.
     * @param owner Ship owner to match.
     * @param ship_type Ship type to match.
     * @param location Star location to search.
     * @param include_just_built Nonzero to include ships flagged as newly built.
     */
    s_ship_data* __cdecl Player_Ship_Type_At_Star(int16_t owner, int16_t ship_type, int16_t location, int8_t include_just_built);
    /**
     * Checks whether the player has a turn-summary message worth showing.
     * @param player_idx Player whose messages are being checked.
     * @param show_summary Nonzero to open the summary screen immediately.
     */
    int8_t __cdecl Has_Msg(int16_t player_idx, int8_t show_summary);
    /** Returns whether a message triggers a serious-only turn summary. */
    bool Is_Serious_Msg(const s_MSG& msg);
    /** Produces display indices, keeping queue order within each severity group. */
    void Order_Turn_Summary_Messages(uint8_t* indices, int16_t first_idx, int16_t count, bool serious_first);
    /**
     * Checks whether the Antaran notification flow should run this turn.
     * @param unused_player_idx Unused caller-supplied player index.
     * @param show_notification Nonzero to execute the notification UI flow.
     */
    int8_t __cdecl Antarans(int16_t unused_player_idx, int8_t show_notification);

    /**
     * Processes pending colony or outpost placement prompts for a player.
     * @param player_idx Player whose colonization state is being checked.
     * @param show_dialog Nonzero to show the placement UI.
     */
    int8_t __cdecl Colonization(::game::s_world_state* world_state, int16_t player_idx, int8_t show_dialog);

    /**
     * Checks for queued reports for a player and optionally displays them.
     * @param player_idx Player whose reports are being checked.
     * @param show_messages True to display queued report UI.
     */
    int8_t __cdecl Has_Report(int16_t player_idx, bool show_messages);

    /**
     * Formats a report record into a displayable text buffer.
     * @param buffer Destination text buffer.
     * @param buffer_size Destination text buffer size in bytes.
     * @param report Report to format.
     */
    void __cdecl sprintf_report(char* buffer, uint32_t buffer_size, s_report* report);

    /**
     * Runs the report pipeline for a player and optionally opens report UI.
     * @param player_idx Player whose report state is being processed.
     * @param show_ui Nonzero to execute UI side effects.
     */
    int8_t __cdecl Display_Report_Aux(int16_t player_idx, uint8_t show_ui);

    /**
     * Displays the next report step for the requested player.
     * @param player_idx Player whose report flow should advance.
     */
    uint8_t __cdecl Display_Report(int16_t player_idx);

    /** Applies relocation orders to ships completed this turn. */
    void __cdecl Redirect_Newly_Built_Ships(int16_t player_idx);

    /** Checks whether the player must stop on a report before ending the turn. */
    uint8_t __cdecl Must_Display_Reports(int16_t player_idx);
    /**
     * Checks for a pending GNN event and optionally marks it displayed.
     * @param player_idx Player/bit index in the displayed-GNN mask.
     * @param mark_as_displayed True to mark the event as shown.
     */
    int8_t __cdecl Has_GNN(uint8_t player_idx, bool mark_as_displayed);
    /**
     * Opens the initial tech selection flow when the player has no active field.
     * @param player_idx Player whose research state is being checked.
     * @param select_tech Nonzero to open tech selection immediately.
     */
    int8_t __cdecl Set_Initial_Tech(int16_t player_idx, int8_t select_tech);
    /** Checks for an officer-for-hire offer and optionally resolves it. */
    int8_t __cdecl Has_Officer_For_Hire(int16_t player_idx, int8_t hire_flag);
    /** Checks for a research breakthrough and optionally opens tech selection. */
    int8_t __cdecl Has_Research_Breakthrough(int16_t player_idx, int8_t do_select);
    /** Checks whether the player must choose an evolutionary upgrade. */
    int8_t __cdecl Has_Evolutionary_Upgrade(int16_t player_idx, int8_t do_setup);
    /** Returns whether a star currently holds a player-specific special. */
    int8_t __cdecl Star_Has_Special_For_Player(s_star_data* star, int player_idx);

    /**
     * Maps a star special to the discovery popup art for the requested player.
     * @param star Star whose special is being evaluated.
     * @param player_idx Player receiving the popup.
     */
    int8_t __cdecl Draw_Special(s_star_data* star, int16_t player_idx);

    /**
     * Checks for newly visited systems and optionally shows discovery popups.
     * @param player_idx Player whose exploration state is being checked.
     * @param show_popup Nonzero to show the discovery popup.
     */
    int8_t __cdecl Explored_New_Star(int16_t player_idx, int8_t show_popup);
    /** Checks for diplomacy messages addressed to a player and optionally shows them. */
    int8_t __cdecl Has_Diplomacy_Messages(int16_t receiver_idx, bool show_messages);
    /** Checks for colonies that still need an occupation policy choice. */
    int8_t __cdecl Need_To_Set_Occupation_Policy(::game::s_world_state* world_state, int16_t player_idx, int8_t show_screen);
    /** Finds an empty colony queue and optionally jumps to that colony screen. */
    int8_t __cdecl Empty_Colony_Queue(::game::s_world_state* world_state, int16_t player_idx, int8_t do_jump);
} // namespace REPORT