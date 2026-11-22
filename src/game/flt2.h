#pragma once
#include <cstdint>

namespace FLT2 {
    /// Adds hidden input fields for the currently built small fleet-screen icons.
    void __cdecl Add_Fltscrn_Small_Icon_Fields();

    /// Gets the animation dimensions for a small fleet-screen icon.
    /// @param icon_idx Small-icon entry to inspect.
    /// @param width_out Receives the animation width.
    /// @param height_out Receives the animation height.
    void __cdecl Get_Small_Icon_Animation_Size(int16_t icon_idx, int16_t* width_out, int16_t* height_out);

    /// Refreshes the big-icon field list and optional scroll field for the fleet screen.
    /// @param button_id_ptr Receives the scroll field id, or -1000 when no scroll field is added.
    /// @param x Scroll field x position.
    /// @param y Scroll field y position.
    /// @param w Scroll field width.
    /// @param h Scroll field height.
    void __cdecl Fleet_Screen_Big_Icon_Fields(int16_t* button_id_ptr, int16_t x, int16_t y, int16_t w, int16_t h);

    /// Moves the specified big icon to the front of the current display list.
    /// @param icon_idx Big-icon index to move.
    void __cdecl Move_Ship_To_Front_Of_List(int16_t icon_idx);

    /// Adds hidden fields for the currently visible big fleet-screen icons.
    /// @param base_index First icon index in the visible page.
    void __cdecl Add_Fltscrn_Big_Icon_Fields(int16_t base_index);

    /// Initializes the small-ship stack pointer state used by the fleet screen.
    void __cdecl Set_Initial_Stack_Pointer();

    /// Clears the selected flag on every current big fleet-screen icon.
    void __cdecl Clear_All_Selection_Status();

    /// Rebuilds the big-icon list for a ship stack and preserves prior selections when possible.
    /// @param stack_idx Ship-stack index to rebuild from.
    /// @param first_visible_row First visible row requested by the caller.
    void __cdecl Build_Fltscrn_Big_Icon_Fields(int16_t stack_idx, int16_t first_visible_row = 0);

    /// Returns whether a ship passes the current fleet-screen filter.
    /// @param ship_idx Ship index to test.
    bool __cdecl Ok_To_Add_Ship(int16_t ship_idx);

    /// Updates the cached owner for the currently selected fleet-screen stack.
    void __cdecl Set_Fleet_Screen_Stack_Owner();

    /// Converts a big-icon index into on-screen coordinates.
    /// @param icon_idx Big-icon index to convert.
    /// @param out_x Receives the screen x coordinate.
    /// @param out_y Receives the screen y coordinate.
    void __cdecl Get_Fltscrn_Big_Icon_XY(int16_t icon_idx, int16_t* out_x, int16_t* out_y);

    /// Scans the visible big-icon fields for the supplied button ids.
    int16_t __cdecl Scan_Fltscrn_Big_Icons(int16_t btn_id1, int16_t btn_id2, int16_t* out_idx1, int16_t* out_idx2);

    /// Attempts to move the currently selected ships to the target star.
    /// @param target_star Destination star index.
    void __cdecl Fltscrn_Move_Ships(int16_t target_star);

    /// Returns the current fleet-screen ship filter mode.
    int16_t __cdecl Get_Filter_Button_Status();

    /// Prints the status text for the currently scanned star.
    void __cdecl Print_Fltscrn_Scanned_Star_Name();

    /// Builds a list of selected ships from the current big-icon selection state.
    /// @param ship_list Receives the selected ship indices.

    extern s_colors normal_colors;
    extern s_colors neutral_colors;
    extern s_colors red_colors;

    /// Returns the localized crew-quality description for a ship.
    /// @param ship_idx Ship index to describe.
    char* __cdecl Crew_Description_String(int16_t ship_idx);

    /// Draws the assigned captain portrait for a ship.
    /// @param ship_idx Ship index whose captain should be shown.
    uint8_t __cdecl Draw_Captains_Picture(int16_t ship_idx);

    /// Prints the detailed text panel for a scanned ship.
    /// @param ship_idx Ship index to describe.
    void __cdecl Print_Scanned_Ship_Data(int16_t ship_idx);

    /// Counts the ships in a stack that pass the current fleet-screen filter.
    /// @param ship_stack_head_node_idx First node in the ship stack to inspect.
    int16_t __cdecl Fltscrn_Ships_To_Be_Displayed(int16_t ship_stack_head_node_idx);

    /// Loads the owning player's ship palette and refreshes the fleet screen.
    /// @param player_idx Player index whose ship palette should be loaded.
    void __cdecl Fix_Palette(int16_t player_idx);
} // namespace FLT2