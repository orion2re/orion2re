#include "pch.h"

#include <algorithm>
#include <cstdint>

#include "game/settings_layout.h"

namespace SETTINGS_LAYOUT {

    namespace {

        // A fraction of a total, in permille (parts per thousand),
        // rounded to the nearest pixel. All allocation fractions
        // are permille constants, so the layout is exact integer
        // arithmetic -- no floating point, no 640x480 assumptions.
        int16_t Scale(int32_t total, int32_t permille) {
            return (int16_t)((total * permille + 500) / 1000);
        }

        // Vertical allocation, permille of the height.
        // The bands TOUCH (no gaps): the tie-together pass
        // (Tom, 2026-11-11) integrates the elements' borders
        // into one continuous frame, close to how the rest of
        // the interfaces are done -- not separate floating
        // boxes with gaps. The list absorbs the freed space.
        // The bottom-bar button rects span the bar's full height
        // (the hit area); the drawn buttons sit below the bar's
        // top divider, so their borders meet the divider above
        // and the outer frame below (the colony's SORT band fills
        // its bar the same way); the tabs tile their strip with
        // no gap between them, one continuous band.
        constexpr int32_t kMargin_Y = 20;
        constexpr int32_t kGap_Y = 0;
        // The header is the colony's height (26 px at 640x480, its
        // plates 20 px -- measured from the colony reference): Tom's
        // "header too large" was the sample's 29 px header, not
        // this screen's, so the header does not move.
        constexpr int32_t kHeader_H = 55;
        // The description is sized to its content (the longest
        // description, v120, is four lines plus the status line),
        // not to five lines: the panel's empty area below the text
        // was the wasted space Tom called out. The list takes the
        // freed space, so the rows are taller and the value boxes
        // fill them like the sample's tall inset boxes.
        constexpr int32_t kList_H = 655;
        constexpr int32_t kDescription_H = 186;
        constexpr int32_t kBottom_Bar_H = 64;

        // Horizontal allocation, permille of the width.
        constexpr int32_t kMargin_X = 20;
        constexpr int32_t kScrollbar_W = 20;
        // The scrollbar strip touches the list (no gap): its
        // left frame edge is the divider between the list and
        // the strip, one continuous line. The strip runs the
        // header band too, so its up arrow sits in the top
        // corner with the header plates, like the colony's
        // (whose up arrow is at the header's height).
        constexpr int32_t kScrollbar_Gap = 0;
        // The VALUE zone starts at 79.1% of the width: the zone
        // is 108 px at 640x480, so the 104 px widened value-box
        // frame the dropdown/edit values sit on nearly fills it
        // (2 px of margin each side), like the sample's wide value
        // boxes filling their column. The widest value, "overview"
        // (74 px at the value font), fits with room to spare --
        // the 85 px colony RETURN frame truncated it to "overvi..."
        // (Tom, 2026-11-13). The freed width goes to the name zone,
        // so long labels truncate less.
        constexpr int32_t kValue_X = 791;
        constexpr int32_t kValue_Gap = 10;
        constexpr int32_t kBottom_Gap = 12;
        // MORE and RETURN are the SAME width: one button width,
        // sized for the longer label (RETURN) plus a clear margin
        // on both sides -- RETURN's label is ~79 px at 640, so
        // 99 px leaves ~8 px of margin inside the button's
        // borders (Tom, 2026-11-11: RETURN was tight/clipped
        // against the frame's right edge at 85 px). The tabs
        // give up the width.
        constexpr int32_t kBottom_Button_W = 155;
        // MORE / RETURN / tab button height, permille of the height:
        // the full bottom-bar height (the hit area), so the buttons'
        // rects are the bar's rects (they meet the divider above
        // and the outer frame below, and the corner buttons tie
        // into the mechanical frame the elements' borders form).
        // The drawn buttons sit 2 px below the divider -- see
        // Draw_Elements -- so the divider stays visible above them.
        constexpr int32_t kButton_H = 64;

        // The "Options" background label reserves the bottom
        // bar's left ~55 px (86 permille of the width); the
        // tabs start after it, so no tab overlaps the label.
        constexpr int32_t kOptions_Reserve = 86;
        // The tabs ARE the colony sort-tab frames, stretched
        // wider by repeating a flat middle column (the frames are
        // uniform across their middles, so the stretch is
        // pixel-exact colonies art -- tools/stretch_sprite.py):
        // 160 px for Internals (sort-population-wide), 104 px for
        // Gameplay (sort-population-mid), 80 px for Mods
        // (sort-food-wide) at 640x480, as permille of the width,
        // so the tiling scales with the resolution. The tab rects
        // are the frame rects, so the fields built from them match
        // the visible frames. The widths are sized to the labels
        // (Tom's sample: the tabs fill the bar to MORE): "Game
        // Internals" is 85 px, "Gameplay" 55 px, "Mods" 29 px at
        // the tab font, so the full names fit (Tom, 2026-11-13).
        constexpr int32_t kTab_Internals_W = 250;
        constexpr int32_t kTab_Gameplay_W = 162;
        constexpr int32_t kTab_Mods_W = 125;

        // The value control's height, as a fraction of the row
        // height (percent): the control is vertically centred in
        // its row. 80%: the sample's value boxes are tall inset
        // slots filling most of the row, not small centred boxes
        // (62% left empty space above and below them).
        constexpr int32_t kValue_Control_H_Percent = 80;

        // The row divider's height, permille of the screen height:
        // the background's row dividers are the colony's 8px
        // chiseled bands (8 px at 640x480), so a row's plate --
        // the face the elements sit on -- is the row minus this
        // divider at its bottom. The elements centre on the plate,
        // not on the full row slot: the slot's centre is the
        // divider's height below the plate's centre.
        constexpr int32_t kRow_Divider_H = 17;

        // The scrollbar thumb's minimum height, in pixels.
        constexpr int16_t kThumb_Min_H = 8;

    } // namespace

    s_settings_layout Compute_Layout(int32_t width, int32_t height,
        int row_count, const int tab_label_lengths[3]) {
        s_settings_layout layout;
        if (width <= 0 || height <= 0 || row_count <= 0) {
            return layout;
        }
        layout.width = (int16_t)width;
        layout.height = (int16_t)height;
        layout.row_count = row_count;

        // Vertical bands, stacked from the top margin. Each band's
        // position is the sum of the bands above it, so the bands
        // tile the height exactly (top margin + bands + bottom
        // margin = 1000 permille). The tabs live in the bottom bar,
        // not at the top: the header is the first band, and the
        // list takes the space the old top tab bar used.
        const int16_t margin_y = Scale(height, kMargin_Y);
        const int16_t gap_y = Scale(height, kGap_Y);
        const int16_t header_h = Scale(height, kHeader_H);
        const int16_t list_h = Scale(height, kList_H);
        const int16_t description_h = Scale(height, kDescription_H);
        const int16_t bottom_bar_h = Scale(height, kBottom_Bar_H);
        int16_t y = margin_y;
        const int16_t header_y = y;
        y = (int16_t)(y + header_h + gap_y);
        const int16_t list_y = y;
        y = (int16_t)(y + list_h + gap_y);
        const int16_t description_y = y;
        y = (int16_t)(y + description_h + gap_y);
        const int16_t bottom_bar_y = y;
        const int16_t bottom_bar_bottom = (int16_t)(bottom_bar_y + bottom_bar_h);

        // Horizontal content: the same margin on both sides.
        const int16_t margin_x = Scale(width, kMargin_X);
        const int16_t content_x = margin_x;
        const int16_t content_right = (int16_t)(width - margin_x);
        const int16_t content_w = (int16_t)(content_right - content_x);

        layout.content = s_rect{content_x, header_y, content_w,
            (int16_t)(bottom_bar_bottom - header_y)};
        layout.bottom_bar = s_rect{content_x, bottom_bar_y,
            content_w, bottom_bar_h};

        // MORE and RETURN sit at the bottom bar's right end, the
        // same width, spanning the bar's full height (their
        // borders are the bar's borders). The tabs share the
        // bar, on the left, at the same height.
        const int16_t button_w = Scale(width, kBottom_Button_W);
        const int16_t bottom_gap = Scale(width, kBottom_Gap);
        const int16_t button_h = Scale(height, kButton_H);
        const int16_t button_y = bottom_bar_y;
        const int16_t done_x = (int16_t)(content_right - button_w);
        const int16_t more_x = (int16_t)(done_x - bottom_gap - button_w);
        layout.show_more = s_rect{more_x, button_y, button_w, button_h};
        layout.done = s_rect{done_x, button_y, button_w, button_h};

        // Tabs: the colony sort-tab frames, tiled adjacently
        // right after the "Options" label, with NO gap between
        // them: one continuous band like the colony's SORT band,
        // their shared borders one line. The tab rects ARE the
        // frame rects (the fields built from them match the
        // visible frames exactly); the strip fills the bar to
        // MORE, like Tom's sample (the 78/78/42 px tabs left a
        // bare 155 px gap, Tom, 2026-11-13).
        // The label lengths no longer size the tabs: the frames
        // do. (tab_label_lengths stays in the signature so the
        // layout stays a pure function of the screen and the tab
        // labels, and the call sites do not change.)
        (void)tab_label_lengths;
        const int16_t tab_x0 =
            (int16_t)(content_x + Scale(width, kOptions_Reserve));
        const int16_t tab_widths[3] = {Scale(width, kTab_Internals_W),
            Scale(width, kTab_Gameplay_W), Scale(width, kTab_Mods_W)};
        int16_t tab_x = tab_x0;
        for (int tab = 0; tab < 3; ++tab) {
            layout.tabs[tab] = s_rect{tab_x, button_y,
                tab_widths[tab], button_h};
            tab_x = (int16_t)(tab_x + tab_widths[tab]);
        }
        layout.tab_bar = s_rect{tab_x0, button_y,
            (int16_t)(tab_x - tab_x0), button_h};

        // The scrollbar strip sits at the content's right edge,
        // running the header band as well as the list: the list
        // rows span the content width minus the strip and the
        // gap before it, and the strip's up arrow sits in the
        // top corner with the header plates, like the colony's.
        const int16_t scrollbar_w = Scale(width, kScrollbar_W);
        const int16_t scrollbar_gap = Scale(width, kScrollbar_Gap);
        const int16_t scrollbar_x = (int16_t)(content_right - scrollbar_w);
        const int16_t list_right = (int16_t)(scrollbar_x - scrollbar_gap);
        layout.list = s_rect{content_x, list_y,
            (int16_t)(list_right - content_x), list_h};
        layout.scrollbar = s_rect{scrollbar_x, header_y, scrollbar_w,
            (int16_t)(header_h + list_h)};
        // The arrow zones cap the strip (each twice the strip's
        // width tall, the colony's ~1:2 arrow aspect); the track
        // is the strip between them, where the thumb travels.
        // The up arrow fills the header band's height in the
        // strip column at 640x480 (26 px); at other resolutions
        // its aspect is kept and the strip's divider runs the
        // full height regardless.
        const int16_t arrow_h = (int16_t)(2 * scrollbar_w);
        layout.scrollbar_arrow_up =
            s_rect{scrollbar_x, header_y, scrollbar_w, arrow_h};
        layout.scrollbar_arrow_down = s_rect{scrollbar_x,
            (int16_t)(list_y + list_h - arrow_h), scrollbar_w, arrow_h};
        const int16_t track_h =
            (int16_t)std::max(0, header_h + list_h - 2 * arrow_h);
        layout.scrollbar_track = s_rect{scrollbar_x,
            (int16_t)(header_y + arrow_h), scrollbar_w, track_h};

        // The VALUE zone starts at the same x in the header and
        // in every row, and runs to the list's right edge.
        const int16_t value_x = Scale(width, kValue_X);
        const int16_t value_gap = Scale(width, kValue_Gap);
        const int16_t value_w = (int16_t)(list_right - value_x);
        layout.header = s_rect{content_x, header_y, content_w, header_h};
        layout.header_setting = s_rect{content_x, header_y,
            (int16_t)(value_x - value_gap - content_x), header_h};
        layout.header_value = s_rect{value_x, header_y, value_w, header_h};

        layout.description = s_rect{content_x, description_y,
            content_w, description_h};
        return layout;
    }

    s_rect Row_Rect(const s_settings_layout& layout, int index) {
        if (index < 0) {
            index = 0;
        }
        if (index >= layout.row_count) {
            index = layout.row_count - 1;
        }
        // Tile the list height exactly: each row's edges are its
        // share of the list height, so the rows meet with no gaps
        // and the last row absorbs the rounding remainder.
        const int32_t list_h = layout.list.h;
        const int32_t y0 = layout.list.y + list_h * index / layout.row_count;
        const int32_t y1 =
            layout.list.y + list_h * (index + 1) / layout.row_count;
        return s_rect{layout.list.x, (int16_t)y0, layout.list.w,
            (int16_t)(y1 - y0)};
    }

    s_rect Row_Rect_For_Span(const s_settings_layout& layout,
        int first_slot, int lines) {
        if (lines < 1) {
            lines = 1;
        }
        if (first_slot < 0) {
            first_slot = 0;
        }
        if (first_slot >= layout.row_count) {
            first_slot = layout.row_count - 1;
        }
        if (first_slot + lines > layout.row_count) {
            lines = layout.row_count - first_slot;
        }
        const s_rect first = Row_Rect(layout, first_slot);
        const s_rect last = Row_Rect(layout, first_slot + lines - 1);
        return s_rect{first.x, first.y, first.w,
            (int16_t)(Rect_Bottom(last) - first.y)};
    }

    s_rect Row_Plate_Rect_For_Row(const s_settings_layout& layout,
        const s_rect& row) {
        // The plate is the row minus its bottom divider: the face
        // the background draws flat dark, with the chiseled divider
        // band below it. The divider height scales with the screen.
        const int16_t divider_h = Scale(layout.height, kRow_Divider_H);
        const int16_t plate_h =
            (int16_t)std::max(1, row.h - divider_h);
        return s_rect{row.x, row.y, row.w, plate_h};
    }

    s_rect Row_Plate_Rect(const s_settings_layout& layout, int index) {
        return Row_Plate_Rect_For_Row(layout, Row_Rect(layout, index));
    }

    s_rect Row_Name_Rect_For_Row(const s_settings_layout& layout,
        const s_rect& row) {
        // The name zone ends where the header's SETTING zone ends
        // (the gap before the VALUE zone); the row starts at the
        // list's left edge, like the header.
        return s_rect{row.x, row.y,
            (int16_t)(Rect_Right(layout.header_setting) - row.x), row.h};
    }

    s_rect Row_Name_Rect(const s_settings_layout& layout, int index) {
        return Row_Name_Rect_For_Row(layout, Row_Rect(layout, index));
    }

    s_rect Row_Value_Rect_For_Row(const s_settings_layout& layout,
        const s_rect& row) {
        // The value control centres on the plate (the row minus
        // its bottom divider), not on the full row slot -- the
        // plate is what the eye reads as the row.
        const s_rect plate = Row_Plate_Rect_For_Row(layout, row);
        const int16_t value_h =
            (int16_t)(plate.h * kValue_Control_H_Percent / 100);
        const int16_t value_y =
            (int16_t)(plate.y + (plate.h - value_h) / 2);
        return s_rect{layout.header_value.x, value_y,
            layout.header_value.w, value_h};
    }

    s_rect Row_Value_Rect(const s_settings_layout& layout, int index) {
        return Row_Value_Rect_For_Row(layout, Row_Rect(layout, index));
    }

    int Settings_On_Page(const int* line_counts, int total, int first,
        int page_lines) {
        if (line_counts == nullptr || total <= 0 || page_lines <= 0) {
            return 0;
        }
        if (first < 0) {
            first = 0;
        }
        if (first >= total) {
            return 0;
        }
        int used = 0;
        int count = 0;
        for (int i = first; i < total; ++i) {
            const int lines = line_counts[i] < 1 ? 1 : line_counts[i];
            if (count > 0 && used + lines > page_lines) {
                break;
            }
            used += lines;
            ++count;
        }
        return count;
    }

    int First_For_Last_Page(const int* line_counts, int total,
        int page_lines) {
        if (line_counts == nullptr || total <= 0 || page_lines <= 0) {
            return 0;
        }
        // Walk back from the end, taking settings while they fit
        // in the page: the first one taken is the last page's
        // first setting. The whole list fits when the walk
        // reaches the first setting.
        int used = 0;
        int first = total;
        for (int i = total - 1; i >= 0; --i) {
            const int lines = line_counts[i] < 1 ? 1 : line_counts[i];
            if (used + lines > page_lines) {
                break;
            }
            used += lines;
            first = i;
        }
        return first;
    }

    int Lines_Before(const int* line_counts, int total, int first) {
        if (line_counts == nullptr || total <= 0 || first <= 0) {
            return 0;
        }
        if (first > total) {
            first = total;
        }
        int sum = 0;
        for (int i = 0; i < first; ++i) {
            sum += line_counts[i] < 1 ? 1 : line_counts[i];
        }
        return sum;
    }

    int Lines_On_Page(const int* line_counts, int total, int first,
        int page_lines) {
        const int count =
            Settings_On_Page(line_counts, total, first, page_lines);
        int sum = 0;
        for (int i = first; i < first + count; ++i) {
            sum += line_counts[i] < 1 ? 1 : line_counts[i];
        }
        return sum;
    }

    s_rect Scrollbar_Thumb_Rect(const s_settings_layout& layout,
        int total_lines, int first_line, int visible_lines) {
        const s_rect track = layout.scrollbar_track;
        if (total_lines <= 0 || visible_lines <= 0) {
            return s_rect{track.x, track.y, 0, 0};
        }
        // The colony's thumb is 6 px wide in its 13 px strip
        // (colsum.cpp Draw_Bar_Indicator: Fill(621..626)), inset
        // 2 px from the strip's left edge -- narrower than the
        // strip, so the strip's own borders stay visible beside
        // it. The width scales with the strip.
        const int16_t thumb_w =
            (int16_t)std::max(4, track.w * 6 / 13);
        const int16_t thumb_x =
            (int16_t)(track.x + track.w * 2 / 13);
        // The thumb travels the track between the arrow/track
        // dividers: inset 2 px at each end, so it never paints
        // over a divider row.
        const int16_t span_y = (int16_t)(track.y + 2);
        const int16_t span_h =
            (int16_t)std::max(0, track.h - 4);
        if (total_lines <= visible_lines) {
            // The colony draws no thumb when the list fits
            // (colsum.cpp Draw_Bar_Indicator draws only when
            // num_colonies >= 10): no scrolling, no indicator.
            return s_rect{track.x, track.y, 0, 0};
        }
        const int max_first = total_lines - visible_lines;
        if (first_line < 0) {
            first_line = 0;
        }
        if (first_line > max_first) {
            first_line = max_first;
        }
        int16_t thumb_h =
            (int16_t)(span_h * visible_lines / total_lines);
        if (thumb_h < kThumb_Min_H) {
            thumb_h = kThumb_Min_H;
        }
        if (thumb_h > span_h) {
            thumb_h = span_h;
        }
        const int16_t travel = (int16_t)(span_h - thumb_h);
        const int16_t thumb_y = max_first > 0
            ? (int16_t)(span_y + travel * first_line / max_first)
            : span_y;
        return s_rect{thumb_x, thumb_y, thumb_w, thumb_h};
    }

    s_rect Popup_Rect(const s_rect& content, int16_t anchor_right,
        int16_t anchor_y, int16_t popup_w, int16_t popup_h) {
        if (popup_w < 1) {
            popup_w = 1;
        }
        if (popup_h < 1) {
            popup_h = 1;
        }
        int16_t x = (int16_t)(anchor_right - popup_w);
        if (x < content.x) {
            x = content.x;
        }
        if (x + popup_w > Rect_Right(content)) {
            x = (int16_t)(Rect_Right(content) - popup_w);
            if (x < content.x) {
                x = content.x;
            }
        }
        int16_t y = anchor_y;
        if (y < content.y) {
            y = content.y;
        }
        if (y + popup_h > Rect_Bottom(content)) {
            y = (int16_t)(Rect_Bottom(content) - popup_h);
            if (y < content.y) {
                y = content.y;
            }
        }
        return s_rect{x, y, popup_w, popup_h};
    }

    int16_t Rect_Right(const s_rect& rect) {
        return (int16_t)(rect.x + rect.w);
    }

    int16_t Rect_Bottom(const s_rect& rect) {
        return (int16_t)(rect.y + rect.h);
    }

} // namespace SETTINGS_LAYOUT
