#pragma once

namespace mouse {
    /// Updates the active cursor shape from the current mouse hit-test list.
    void __cdecl Check_Mouse_Shape();

    /// Buffers the current mouse position and button state when a button is pressed.
    void __cdecl Check_Mouse_Buffer();

    /// Processes a mouse input callback and updates buffered cursor state.
    /// @param mouse_x Current pointer X coordinate.
    /// @param mouse_y Current pointer Y coordinate.
    /// @param mouse_buttons Current mouse button bitmask.
    void __cdecl User_Mouse_Handler(int16_t mouse_x, int16_t mouse_y, uint16_t mouse_buttons);

    struct s_wheel_event {
        int16_t mouse_x;
        int16_t mouse_y;
        float wheel_x;
        float wheel_y;
    };

    /// Queues wheel input at its event position; positive deltas mean right/up.
    void __cdecl User_Mouse_Wheel_Handler(int16_t mouse_x, int16_t mouse_y, float wheel_x, float wheel_y);

    int16_t __cdecl Consume_Mouse_Wheel_Event(s_wheel_event* event);

    void __cdecl Clear_Mouse_Wheel_Delta();

    /// Returns and clears the primary mouse-buffer flag.
    int16_t __cdecl Mouse_Buffer();

    /// Returns and clears the secondary mouse-buffer flag.
    uint16_t __cdecl Mouse_Buffer2();

    /**
     * Returns the current mouse button state, allowing for button overrides.
     * Usually, the return value represents a bitmask or sum of button states:
     * 1 = Left, 2 = Right, 3 = Both.
     */
    int16_t __cdecl Mouse_Button();

    /// Installs the active mouse hit-test list and refreshes the cursor shape.
    /// @param mouse_list Pointer to the mouse region list.
    /// @param list_count Number of entries in the mouse region list.
    void __cdecl Set_Mouse_List(s_mouse_list* mouse_list, int16_t list_count);

    /// Returns the current mouse X coordinate.
    int16_t Pointer_X();

    /// Returns the current mouse Y coordinate.
    int16_t Pointer_Y();

    /// Clears cached mouse-over selection indices.
    void __cdecl Clear_Mouse_XY();

    /// Returns the frame offset from the active mouse-list entry.
    /// Caller must ensure a valid mouse-list entry is currently selected.
    int __cdecl Get_Pointer_Offset();

    /// Returns the icon index from the active mouse-list entry.
    /// Caller must ensure a valid mouse-list entry is currently selected.
    int __cdecl Get_Pointer_Image_Number();

    /// Saves the current mouse-enabled state and disables the mouse.
    void __cdecl Save_Mouse_State();

    /// Restores the previously saved mouse-enabled state.
    void __cdecl Restore_Mouse_State();

    /// Resets runtime mouse state, buffers, and active-list tracking.
    void __cdecl Reset_Mouse_Runtime();

    /// Returns nonzero when the logical game-thread mouse is enabled.
    uint16_t __cdecl Mouse_Enabled();

    /// Copies the active mouse hit-test list into caller-owned storage.
    /// Returns the number of copied entries.
    int16_t __cdecl Copy_Current_Mouse_List(s_mouse_list* out_mouse_list, int16_t max_count);

    /// Returns the buffered mouse button state.
    int16_t __cdecl Mouse_Buffer_Button();

    /// Returns the buffered mouse X coordinate.
    int16_t __cdecl Mouse_Buffer_X();

    /// Returns the buffered mouse Y coordinate.
    int16_t __cdecl Mouse_Buffer_Y();

    /// Disables the mouse.
    void __cdecl Disable_Mouse();

    /// Enables the mouse.
    void __cdecl Enable_Mouse();
} // namespace mouse