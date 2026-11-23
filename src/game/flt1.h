#pragma once

#include <span>

#include "object_ids.h"

namespace FLT1 {
    void __cdecl Draw_Box_Around_Scanned_Ship(int16_t icon_idx);

    void __cdecl Draw_Fltscrn_Big_Ship_Icons(int16_t screen_id);

    /** Draws fleet relocation lines for the requested fleet-screen layout.
     * @param screen_id Fleet-screen layout identifier used to choose the map viewport.
     */
    void __cdecl Draw_Fltscrn_Relocation_Lines(int16_t screen_id);

    game::id_ship First_Selected_Ship();

    int16_t __cdecl F_Ship_Icon_From_Node_Ptr(int16_t node_idx);

    /** Restores the current fleet as the active galaxy-map summary. */
    void __cdecl Restore_Current_Fleet_Summary();

    /** Draws ship destination lines for the requested fleet-screen layout.
     * @param screen_id Fleet-screen layout identifier used to choose the map viewport.
     */
    void __cdecl Draw_Fltscrn_Ship_Destination_Lines(int16_t screen_id);

    void __cdecl Draw_Fltscrn_Small_Ship_Icons();

    void __cdecl Draw_Flashing_Small_Ship_Icon();

    void __cdecl Fill_FltScrn_Scroll_Bar();

    void __cdecl Fleet_Screen();

    /** Initializes the fleet-screen scroll bar state and thumb position.
     * @param scroll_bar Scroll bar state to initialize.
     */
    void __cdecl Initialize_Scroll_Bar(s_scroll_bar* scroll_bar);

    /** Scans the small ship icon fields for hover and click hits.
     * @param scanned_field_id Current scanned or hovered field id.
     * @param clicked_field_id Current clicked field id.
     * @param scanned_icon_idx Receives the hovered icon index, or -1.
     * @param clicked_icon_idx Receives the clicked icon index, or -1.
     */
    int16_t __cdecl Scan_Small_Ship_Icons(int16_t scanned_field_id, int16_t clicked_field_id, int16_t* scanned_icon_idx, int16_t* clicked_icon_idx);

    void __cdecl Allocate_Fltscrn_Next_Screen_Seg();

    void __cdecl Set_Small_Ship_Stack_Pointer(bool check_box);

    /** Loads a fleet-screen officer portrait from FLEET.LBX.
     * @param player_num Unused player id retained for signature compatibility.
     * @param leader_idx Leader record to load.
     * @param buffer Destination allocation segment or buffer.
     * @param reload_flag Nonzero to reload in place instead of appending.
     */
    void* __cdecl Load_Fltscrn_Officer_Picture(int16_t player_num, int16_t leader_idx, void* buffer, uint8_t reload_flag);

    void __cdecl Reload_Small_Ship_Icons();

    void __cdecl Load_Galaxy_Stars();

    void __cdecl Reload_Fleet_Screen();

    void __cdecl Draw_Fleet_Screen();

    void __cdecl Add_Fleet_Screen_Fields(int16_t* relocate_fleet_field_id, int16_t* scrap_button_field_id, int16_t* fleet_return_button_field_id, int16_t* up_scroll_button_field_id, int16_t* down_scroll_button_field_id, int16_t* next_fleet_button_field_id, int16_t* prev_fleet_button_field_id, int16_t* officers_button_field_id, int16_t* all_button_field_id, int16_t* big_icon_field_ids);

    bool __cdecl At_Least_One_Officer(int16_t player_num);

    /** Cancels the relocation order for a star and shows the confirmation message.
     * @param star_id Index of the star whose relocation order is cleared.
     */
    void __cdecl Cancel_Star_Relocation(int16_t star_id);

    /** Advances the two-step relocation source/destination selection flow.
     * @param star_from_ptr Tracks the currently selected source star index.
     * @param star_to_ptr Tracks the currently selected destination star index.
     * @param star_idx Newly selected star index.
     */
    void __cdecl Star_Relocation(int16_t* star_from_ptr, int16_t* star_to_ptr, int16_t star_idx);

    bool __cdecl Okay_To_Set_Relocate_Star(int16_t star_idx, uint8_t prompt_flag);

    uint8_t __cdecl Okay_To_Set_Relocate_Star2(int16_t star_idx);

    /** Checks whether a relocation destination star is valid for a source star.
     * @param src_star_idx Source star index.
     * @param dst_star_idx Destination star index.
     */
    uint8_t __cdecl Okay_To_Set_Relocate_To_Star(int16_t src_star_idx, int16_t dst_star_idx);

    void __cdecl Cant_Order_Ship_Message(const s_ship_data* ship);

    void __cdecl Set_Fltscrn_Big_Icons(int16_t select_flag);

    /** Scraps the selected ship keys in order, optionally prompting and awarding BC/tech. */
    bool Scrap_Ships(int16_t player_idx, std::span<const game::id_ship> keys, bool interactive);

    /** Attempts to award technology recovered from a scrapped ship.
     * @param ship Scrapped ship to inspect.
     * @param player_id Player receiving technology.
     * @param show_msg Nonzero to show the recovered-technology message.
     */
    void __cdecl Get_Technology_From_Scrapped_Ship(s_ship_data* ship, int16_t player_id, uint8_t show_msg);

    int16_t __cdecl N_Fltscrn_Ships_Selected();

    void __cdecl Next_Ship_Icon();

    void __cdecl Previous_Ship_Icon();

    int16_t Count_Selected_Ships();

    extern uint8_t relocate_button_mode;
    extern int16_t fleet_screen_bad_ship_id;
    extern int16_t combat_filter_button;
    extern int16_t support_filter_button;
    extern void* multi_button_support_seg;
    extern uint8_t merging_relocations;
    extern void* multi_button_combat_seg;
    extern s_mouse_list mouse_list_from[4];
    extern s_mouse_list mouse_list_to[4];
    extern s_mouse_list mouse_list_all_to[4];
    extern const char* PTR_s_FLEET_LBX;
} // namespace FLT1