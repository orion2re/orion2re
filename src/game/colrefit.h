#pragma once

#include "object_ids.h"

namespace COLREFIT {
#pragma pack(push, 1)
    struct s_scroll_state {
        int32_t data[6];
    };
#pragma pack(pop)

    extern int16_t first_ship;

    extern int16_t slider_bar_position;

    extern int16_t fields[12];

    extern int16_t esc;

    extern int16_t scanned_ship_index;

    extern s_animation_header* anims[4];

    extern int16_t scanned_ship_d_index;
    extern int16_t lscanned_field;

    /** Updates the first visible ship index to match the scrollbar position.
     * @param first_ship_ptr Receives the aligned first visible ship index.
     * @param slider_pos_ptr Points to the current scrollbar position.
     */
    void __cdecl Update_First_Ship(int16_t* first_ship_ptr, int16_t* slider_pos_ptr);

    extern int16_t* g_ship_field_list_ptr;

    extern MOX2::action_func_t saved_autofunction;

    extern int16_t n_ships_in_list;

    extern s_scroll_state* g_scroll_ptr;

    /** Builds the list of refittable orbiting ships for the current colony. */
    void __cdecl Build_Ship_List();

    /** Loads the first-stage refit popup animations into the screen segment. */
    void __cdecl Load_Refit_Ship_Anims();

    /** Draws the first-stage ship-refit popup. */
    void __cdecl Draw_Refit_Popup();

    /** Draws the scroll-bar thumb for the current ship page. */
    void __cdecl Draw_Bar_Indicator();

    /** Draws the currently visible ship thumbnails in the refit list. */
    void __cdecl Draw_Ship_List();

    /** Draws one ship thumbnail into a popup slot.
     * @param key Ship to display.
     * @param slot_idx Visible popup slot index from 0 to 14.
     */
    void Draw_Ship2(game::id_ship key, int16_t slot_idx);

    /** Draws the detailed text summary for the scanned ship.
     * @param key Ship whose details should be shown.
     */
    void Scan_Ship_Info(game::id_ship key);

    /** Adds hidden input fields for the visible ship thumbnails. */
    void __cdecl Add_Ship_List_Fields();

    /** Adds the first-stage refit popup input fields. */
    void __cdecl Add_Fields();

    /** Adds the second-stage ship-design selection fields. */
    void __cdecl Add2_Fields();

    /** Handles first-stage refit popup input.
     * @param selected_field_id Points to the activated field id.
     * @param hovered_field_id Points to the currently hovered field id.
     */
    game::id_ship Evaluate_Input(int16_t* selected_field_id, int16_t* hovered_field_id);

    /** Loads the second-stage refit popup animations into the screen segment. */
    void __cdecl Load_Refit2_Ship_Anims();

    /** Draws the ship-design choices for the second-stage popup. */
    void __cdecl Draw_Ship_D_List();

    /** Prints the formatted details for a ship design.
     * @param design Ship design to describe.
     */
    void __cdecl Show_Ship_Design(s_ship_design* design);

    /** Draws the second-stage ship-design refit popup. */
    void __cdecl Draw_Refit2_Popup();

    /** Rebuilds the second-stage popup field list. */
    void __cdecl Do_Add2_Fields();

    /** Handles second-stage refit popup input.
     * @param selected_field_id Points to the activated field id.
     * @param hovered_field_id Points to the currently hovered field id.
     */
    int16_t Evaluate2_Input(game::id_ship key, int16_t* selected_field_id, int16_t* hovered_field_id);

    /** Runs the first-stage colony refit popup and returns the chosen ship. */
    game::id_ship Colony_Refit_Popup();

    /** Runs the second-stage colony refit popup for a ship.
     * @param key Ship being considered for refit.
     */
    int16_t Colony_Refit2_Popup(game::id_ship key);
} // namespace COLREFIT