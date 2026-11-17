#pragma once

struct s_animation_header;
struct s_invasion_side_info;

namespace NEWPUP {
    extern MOX2::action_func_t saved_autofunction;

    extern s_animation_header* anims;
    extern uint8_t rebel_display;

    /* Prints one formatted summary line in the troop popup.
     * label: summary label text.
     * value: numeric value shown beside the label.
     * row: in/out summary row index.
     * unused_side_info: unused retained invasion-side parameter.
     */
    void __cdecl Print_Data(char* label, int16_t value, int16_t* row, s_invasion_side_info* unused_side_info);

    /* Prints the best equipment-related combat bonuses for one side.
     * side_info: invasion-side combat data to summarize.
     * row: in/out summary row index.
     */
    void __cdecl Print_Best_Summary(s_invasion_side_info* side_info, int16_t* row);

    /* Prints racial, gravity, difficulty, and officer combat modifiers.
     * side_info: invasion-side combat data to summarize.
     * row: in/out summary row index.
     */
    void __cdecl Print_Race_Summary(s_invasion_side_info* side_info, int16_t* row);

    /* Prints the troop columns for one invasion side.
     * x_base: left edge used to place the troop columns.
     * unused_y_base: unused retained vertical-placement parameter.
     * invasion_side: invasion-side combat data to summarize.
     */
    void __cdecl Print_Troop_Totals(int16_t x_base, int16_t unused_y_base, s_invasion_side_info* invasion_side);

    /* Redraws the troop popup when a full refresh is pending. */
    void __cdecl Draw_Troop_Popup(::game::s_world_state* world_state);

    /* Rebuilds the popup input fields after a frame update. */
    void __cdecl Reset_Fields();

    /* Opens the troop popup, runs its input loop, and restores UI state on exit. */
    void __cdecl Troop_Popup();
} // namespace NEWPUP