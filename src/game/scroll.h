#pragma once

namespace SCROLL {
    /** Handles Home/End commands, keeping the visible row and scroll thumb in sync. */
    bool __cdecl Handle_List_Navigation(s_scroll_bar* scroll_bar, int16_t input);

    /**
     * Draws a vertical scroll bar and updates its state from mouse input.
     * @param scroll_bar Scroll bar state to update and render.
     * @param draw_callback Callback that performs the bar's drawing.
     * @return True when the first visible row changed.
     */
    bool __cdecl Draw_Generic_Vertical_Scroll_Bar(s_scroll_bar* scroll_bar, void(__cdecl* draw_callback)());

    /**
     * Advances the scroll bar by one row-step toward later rows.
     * @param scroll_bar Scroll bar state to update.
     */
    void __cdecl Scroll_Down_One_Item(s_scroll_bar* scroll_bar);
    /**
     * Moves the scroll bar by one row-step toward earlier rows.
     * @param scroll_bar Scroll bar state to update.
     */
    void __cdecl Scroll_Up_One_Item(s_scroll_bar* scroll_bar);

    /**
     * Computes the scroll thumb height from the visible and total row counts.
     * @param scroll_bar Scroll bar state to inspect.
     * @return Thumb height in pixels.
     */
    int16_t __cdecl Get_Vertical_Scroll_Bar_Height(s_scroll_bar* scroll_bar);

    /**
     * Recomputes derived scroll-bar state from the current thumb position.
     * @param scroll_bar Scroll bar state to update.
     */
    void __cdecl Update_Vertical_Scroll_Bar_State(s_scroll_bar* scroll_bar);

    /**
     * Clamps the thumb center so the thumb remains within the track.
     * @param scroll_bar Scroll bar state to clamp in place.
     * @return Nonzero when the thumb position changed.
     */
    int8_t __cdecl Set_Vertical_Scroll_Bar_Maxima(s_scroll_bar* scroll_bar);

    /**
     * Computes the first visible row index from the current thumb position.
     * @param scroll_bar Scroll bar state to inspect.
     * @return First visible row index.
     */
    int16_t __cdecl Get_Vertical_Scroll_Bar_First_Item(s_scroll_bar* scroll_bar);
} // namespace SCROLL