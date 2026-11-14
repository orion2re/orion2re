#pragma once

#include <cstdint>

namespace SETTINGS_LAYOUT {

    // A rectangle: top-left corner (x, y), size (w, h). Right and
    // bottom edges are exclusive (x + w, y + h).
    struct s_rect {
        int16_t x = 0;
        int16_t y = 0;
        int16_t w = 0;
        int16_t h = 0;
    };

    // The settings screen's geometry, allocated as fractions of the
    // screen size -- never as fixed 640x480 pixel coordinates -- so
    // the same layout scales to any game resolution. Vertical
    // allocation (fractions of the height, top to bottom):
    //
    //   top margin      0.020
    //   header row      0.055  (SETTING / VALUE column labels --
    //                            the colony's height, its plates
    //                            20 px at 640x480, measured from
    //                            the colony reference)
    //   list            0.655  (10 row slots, tiled exactly; a
    //                            row whose label wraps takes two
    //                            slots, so it renders at full size)
    //   description     0.186  (selected row's description + status:
    //                            sized to its content -- the longest
    //                            description (v120) is four lines
    //                            plus the status line -- so no big
    //                            empty area opens below the text)
    //   bottom bar      0.064  (tabs + MORE + RETURN)
    //   bottom margin   0.020
    //
    //   The bands touch (no gaps): the tie-together pass
    //   (Tom, 2026-11-11) integrates the elements' borders
    //   into one continuous frame, close to how the rest of
    //   the interfaces are done.
    //
    // Horizontal allocation (fractions of the width): the content
    // spans 0.020..0.980. The bottom bar holds the "Options"
    // background label at the far left, then the three category
    // tabs tiled adjacently right after it (the colony sort-tab
    // frames: 78 px for Internals/Gameplay, 42 px for Mods at
    // 640x480 -- the tab rects ARE the frame rects, with NO gap
    // between tabs, one continuous band like the colony's SORT
    // band), and MORE and RETURN at the right end, the SAME width
    // (0.133, sized for the longer label) with a 0.012 gap between
    // them. The button rects span the bar's full height (the hit
    // area); the drawn buttons sit below the bar's top divider, so
    // their top borders meet the divider's bottom line and the
    // divider runs the full width above them (the sample's
    // continuous divider), and their bottoms meet the outer frame.
    // The list's rows span the content width minus the scrollbar
    // strip (0.020 wide, at the content's right edge, touching
    // the list with no gap) -- the strip is chunky, like the
    // colony's, so the arrows read as buttons. The strip runs the
    // header band as well as the list, so its up arrow sits in the
    // top corner with the header plates, like the colony's. The
    // strip holds an up arrow at its top and a down arrow at its
    // bottom (each a framed button twice the strip's width tall);
    // the thumb travels the track between them. The thumb is the
    // colony's indicator bar: 6 px wide in the colony's 13 px
    // strip, and it spans the whole track when the list fits
    // (the colony's rule -- the arrows stay visible and do
    // nothing on a short list). The VALUE zone starts at 0.820
    // of the width, in the header and in every row alike -- the
    // zone is 89 px at 640x480, so the 85 px colony RETURN frame
    // the dropdown/edit values sit on nearly fills it, like the
    // sample's wide value boxes filling their column; the freed
    // width goes to the name zone.
    //
    // The layout is pure: it takes the screen size and the tab
    // label lengths and returns rectangles. No video, font or
    // global state, so unit tests pin it at several resolutions.
    struct s_settings_layout {
        int16_t width = 0;
        int16_t height = 0;
        int row_count = 0;
        // The content rect: the outer box the screen's elements
        // live in (content margins on all four sides).
        s_rect content;
        // The tab strip: the rect spanning the three tabs, inside
        // the bottom bar.
        s_rect tab_bar;
        s_rect tabs[3];
        s_rect header;
        s_rect header_setting;
        s_rect header_value;
        s_rect list;
        s_rect scrollbar;
        // The scrollbar's arrow zones (up at the strip's top,
        // down at the bottom) and the track between them, where
        // the thumb travels. The arrows are the strip's width
        // and twice as tall (the colony's arrows are ~1:2,
        // 10x20 and 11x22); the track is the strip between
        // them, so the thumb never runs under an arrow. Each
        // arrow zone is a dark-navy button with a light-grey
        // triangle (the colony's arrows), not a flat glyph.
        s_rect scrollbar_arrow_up;
        s_rect scrollbar_arrow_down;
        s_rect scrollbar_track;
        s_rect description;
        s_rect bottom_bar;
        s_rect show_more;
        s_rect done; // RETURN
    };

    // Computes the layout for a screen of the given size.
    // `row_count` is the number of visible list rows (the screen's
    // ROWS_PER_PAGE); `tab_label_lengths` holds the character
    // length of each of the 3 category labels, in tab order
    // (accepted for the call sites; the tab widths are the colony
    // sort-tab frame widths, not the label lengths).
    s_settings_layout Compute_Layout(int32_t width, int32_t height,
        int row_count, const int tab_label_lengths[3]);

    // The rect of visible row `index` (0-based, clamped into
    // range). Rows tile the list area exactly; the last row
    // absorbs the rounding remainder.
    s_rect Row_Rect(const s_settings_layout& layout, int index);

    // The rect spanning slots [first_slot, first_slot + lines):
    // a row whose label wraps takes two slots, so the label
    // renders at full size instead of shrinking into one slot.
    // `lines` is clamped to at least 1 and the span is clipped
    // to the list area.
    s_rect Row_Rect_For_Span(const s_settings_layout& layout,
        int first_slot, int lines);

    // The setting-name zone inside a row: the row minus the VALUE
    // zone and the gap between them.
    s_rect Row_Name_Rect(const s_settings_layout& layout, int index);

    // The setting-name / value-control zones for an arbitrary row
    // rect (a single slot, or a multi-slot span for a wrapped row).
    s_rect Row_Name_Rect_For_Row(const s_settings_layout& layout,
        const s_rect& row);
    s_rect Row_Value_Rect_For_Row(const s_settings_layout& layout,
        const s_rect& row);

    // The row's plate: the row minus its bottom divider (the
    // background's 8px chiseled band). The plate is the face the
    // row's elements sit on; the elements centre on it, not on
    // the full row slot.
    s_rect Row_Plate_Rect_For_Row(const s_settings_layout& layout,
        const s_rect& row);
    s_rect Row_Plate_Rect(const s_settings_layout& layout, int index);

    // The value-control zone inside a row: the row's VALUE zone,
    // vertically centred on the row's plate, 80% of the plate
    // height -- the sample's tall inset value boxes, filling most
    // of the row. Dropdowns, toggles and edit fields sit in this
    // rect (step 4 renders them; step 3 only allocates it).
    s_rect Row_Value_Rect(const s_settings_layout& layout, int index);

    // Slot arithmetic over per-setting line counts (each entry is
    // 1, or 2 for a setting whose label wraps onto a second line):
    // how many settings from `first` fit in `page_lines` slots
    // (at least 1, so a page never shows nothing); the smallest
    // first index whose settings to the end fit in `page_lines`
    // slots (0 when the whole list fits); the slots the settings
    // before `first` take; and the slots the page from `first`
    // uses.
    int Settings_On_Page(const int* line_counts, int total, int first,
        int page_lines);
    int First_For_Last_Page(const int* line_counts, int total,
        int page_lines);
    int Lines_Before(const int* line_counts, int total, int first);
    int Lines_On_Page(const int* line_counts, int total, int first,
        int page_lines);

    // The scrollbar thumb for a list of `total_lines` lines whose
    // first visible line is `first_line` and whose page shows
    // `visible_lines` lines. When the whole list fits there is no
    // thumb (the colony's rule -- colsum.cpp Draw_Bar_Indicator
    // draws only when the list scrolls); otherwise a rect inside
    // the scrollbar track, its height proportional to the visible
    // fraction and its position proportional to the scroll offset
    // (top of the track = first lines). Empty (w == 0) when the
    // list fits or is empty. The thumb is the colony's indicator
    // bar: 6 px wide in the colony's 13 px strip, inset 2 px from
    // the strip's left edge, and it travels between the arrow/
    // track dividers (inset 2 px at each end of the track), so it
    // never paints over one.
    s_rect Scrollbar_Thumb_Rect(const s_settings_layout& layout,
        int total_lines, int first_line, int visible_lines);

    // The popup rect for a dropdown list of size `popup_w` x
    // `popup_h`: anchored with its right edge at `anchor_right`
    // (the closed control's right edge) and its top at `anchor_y`
    // (the row's top), clamped inside `content` so the popup --
    // and every label in it -- stays inside the frame. A popup
    // wider or taller than the content is pinned to the content's
    // top-left corner instead of running off an edge.
    s_rect Popup_Rect(const s_rect& content, int16_t anchor_right,
        int16_t anchor_y, int16_t popup_w, int16_t popup_h);

    // The expanded dropdown window: the field system's list
    // picker (Get_List_Field) draws bare text rows over the
    // screen, with no window around them. The settings screen
    // draws this window behind the list while the picker runs,
    // so the open dropdown reads as a window: an opaque fill
    // (the rows behind must not show through), a bitmap border
    // around it (the screen frame's border, thinner), and a
    // separator between every two rows. The window encloses the
    // picker's list (title + items) with a margin for the border
    // and padding, clamped inside the content rect. `rows` is the
    // title plus the item count, `step` the row height (font
    // height + vertical spacing).
    //
    // The border thickness: a BITMAP like the one drawn on the
    // background all around the screen (the outer frame: riveted,
    // textured, chiseled), just thinner -- 5 px against the screen
    // frame's ~8 px (Tom, 2026-11-13). The border strips
    // (dropdown-border-h/v, built by
    // tools/sprites/build_dropdown_border.py) are derived from the
    // frame's own pixels, so the border reads as the same border
    // family, only thinner. The window's margin is this border
    // plus 3 px of padding.
    constexpr int16_t kDropdown_Border = 5;
    struct s_dropdown_window {
        s_rect window;
        int16_t step = 0;
        int16_t rows = 0;
        int16_t margin = 0;
    };
    s_dropdown_window Dropdown_Window(const s_rect& content,
        int16_t list_x, int16_t list_y, int16_t list_w,
        int16_t list_h, int16_t step, int16_t rows);

    // The y of the separator between row `index` - 1 and row
    // `index` (1 <= index < rows): the row boundary, so the
    // separator sits in the gap between the two rows' text.
    int16_t Dropdown_Separator_Y(const s_dropdown_window& w,
        int index);

    // The collapsed dropdown's triangle: a small down-pointing
    // triangle on the value button, at its right end, vertically
    // centred, so the closed dropdown reads as a dropdown at a
    // glance. 7 px wide, 4 tall (45-degree sides), its right edge
    // 4 px inside the button's right edge.
    s_rect Dropdown_Triangle_Rect(const s_rect& button);

    // The widest value text a dropdown button of width
    // `button_w` can show without running under its triangle
    // zone: the centred text's right edge must clear the zone's
    // left edge, so the text is limited to the button width
    // minus twice the zone width and twice 1 px.
    int16_t Dropdown_Value_Text_Width(int16_t button_w);

    // Exclusive right / bottom edges.
    int16_t Rect_Right(const s_rect& rect);
    int16_t Rect_Bottom(const s_rect& rect);

} // namespace SETTINGS_LAYOUT
