#pragma once

namespace COLXPORT {
    extern uint8_t cheezy_hack_col_count_state;

    extern int16_t cheezy_hack_col_count[MAX_PLAYERS];

    /**
     * @brief Returns the number of non-outpost colonies owned by a player.
     * @param player_idx Player index to count colonies for.
     */
    int16_t __cdecl N_Colonies(int16_t player_idx);

    extern game::id_colony* g_colony_list_ptr;

    extern MOX2::action_func_t saved_autofunction;

    extern game::id_colony list_col[6];

    extern s_colsum_col_pop_ref list_col_pop_ref[6];

    extern int16_t list_col_coord_vars[6];

    extern int16_t list_col_pop_count[6];

    extern int16_t list_col_job_fields[6];

    extern int16_t list_fields[6];

    extern int16_t x_fields_array[4];

    /**
     * @brief Rebuilds the current player's global transport-colony list.
     */
    void __cdecl Build_Global_Colony_List();

    /**
     * @brief Resets the visible colony-entry list to empty values.
     */
    void __cdecl Clear_List_Col_Array();

    /**
     * @brief Draws the transport popup and its current colony page.
     */
    void __cdecl Draw_Xport_Popup();

    /**
     * @brief Draws one colony summary entry in the transport popup.
     * @param colony_idx Colony index to display.
     * @param entry_idx Visible list entry index.
     */
    void __cdecl Draw_Colony_Summary_For_Colony(game::id_colony colony_idx, int16_t entry_idx);

    /**
     * @brief Draws the transport-popup scroll bar indicator.
     */
    void __cdecl Draw_Bar_Indicator();

    /**
     * @brief Draws the population/job summary for one visible colony entry.
     * @param entry_index Visible list entry index.
     */
    void __cdecl Draw_Info_Pop_For(int16_t entry_index);

    /**
     * @brief Rebuilds the transport popup input fields for the current page.
     */
    void __cdecl Add_Fields();

    extern s_animation_header* anims[4];

    /**
     * @brief Loads the transport popup button animations.
     */
    void __cdecl Load_Xport_Anims();

    /**
     * @brief Runs the transport popup until a colony is selected or the popup is closed.
     * @return Selected colony index, or -1 if no colony was chosen.
     */
    game::id_colony __cdecl Xport_Popup();
} // namespace COLXPORT
