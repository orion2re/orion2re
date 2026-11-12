#include "pch.h"

#include "doctest/doctest.h"
#include "game/settings_layout.h"

namespace {

    using namespace SETTINGS_LAYOUT;

    // The screen's three category labels, in tab order.
    const int kLabel_Lengths[3] = {14, 8, 4}; // Game Internals, Gameplay, Mods

    bool Overlaps(const s_rect& a, const s_rect& b) {
        return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
            b.y < a.y + a.h;
    }

    bool Inside(const s_rect& inner, const s_rect& outer) {
        return inner.x >= outer.x && inner.y >= outer.y &&
            Rect_Right(inner) <= Rect_Right(outer) &&
            Rect_Bottom(inner) <= Rect_Bottom(outer);
    }

    void Check_Bands_In_Order(const s_settings_layout& layout) {
        // Top to bottom, no overlaps: header, list, description,
        // bottom bar (which holds the tabs, MORE and RETURN).
        // The bands TOUCH (the tie-together: one continuous
        // frame, no gaps between bands), so adjacency is the
        // invariant, not separation.
        CHECK(layout.header.y < layout.list.y);
        CHECK(layout.list.y < layout.description.y);
        CHECK(layout.description.y < layout.bottom_bar.y);
        CHECK_FALSE(Overlaps(layout.header, layout.list));
        CHECK_FALSE(Overlaps(layout.list, layout.description));
        CHECK_FALSE(Overlaps(layout.description, layout.bottom_bar));
        CHECK(Rect_Bottom(layout.header) == layout.list.y);
        CHECK(Rect_Bottom(layout.list) == layout.description.y);
        CHECK(Rect_Bottom(layout.description) == layout.bottom_bar.y);
        // The tabs sit in the bottom bar, left of MORE / RETURN.
        CHECK(Inside(layout.tab_bar, layout.bottom_bar));
        for (int tab = 0; tab < 3; ++tab) {
            CHECK(Inside(layout.tabs[tab], layout.tab_bar));
            CHECK_FALSE(Overlaps(layout.tabs[tab], layout.show_more));
            CHECK_FALSE(Overlaps(layout.tabs[tab], layout.done));
        }
        CHECK(layout.tab_bar.y == layout.show_more.y);
        CHECK(layout.tab_bar.h == layout.show_more.h);
        // The scrollbar strip runs the header band as well as the
        // list (its up arrow sits in the top corner with the
        // header plates, like the colony's), so it starts at the
        // header's top and ends at the list's bottom.
        CHECK(layout.scrollbar.y == layout.header.y);
        CHECK(layout.scrollbar.h ==
            layout.header.h + layout.list.h);
        CHECK(Rect_Bottom(layout.scrollbar) == Rect_Bottom(layout.list));
        CHECK(Rect_Right(layout.scrollbar) <=
            Rect_Right(layout.bottom_bar));
        CHECK(Inside(layout.show_more, layout.bottom_bar));
        CHECK(Inside(layout.done, layout.bottom_bar));
        CHECK_FALSE(Overlaps(layout.show_more, layout.done));
        CHECK(Rect_Right(layout.done) == Rect_Right(layout.bottom_bar));
        CHECK(layout.show_more.x < layout.done.x);
        // MORE and RETURN are the same size.
        CHECK(layout.show_more.w == layout.done.w);
        CHECK(layout.show_more.h == layout.done.h);
        CHECK(layout.show_more.y == layout.done.y);
        // The VALUE zone lines up between the header and the rows.
        for (int row = 0; row < layout.row_count; ++row) {
            const s_rect value = Row_Value_Rect(layout, row);
            CHECK(value.x == layout.header_value.x);
            CHECK(value.w == layout.header_value.w);
            CHECK(Inside(value, Row_Rect(layout, row)));
            CHECK(Inside(Row_Name_Rect(layout, row), Row_Rect(layout, row)));
            CHECK_FALSE(Overlaps(Row_Name_Rect(layout, row), value));
        }
    }

    TEST_CASE("layout at 640x480 pins the percentage allocation") {
        const s_settings_layout layout =
            Compute_Layout(640, 480, 10, kLabel_Lengths);
        CHECK(layout.width == 640);
        CHECK(layout.height == 480);
        CHECK(layout.row_count == 10);
        CHECK(layout.content.x == 13);
        CHECK(layout.content.y == 10);
        CHECK(layout.content.w == 614);
        CHECK(layout.content.h == 460);
        // Vertical bands (permille of 480, rounded): the header is
        // the first band; the list takes the old top tab bar's
        // space; the tabs live in the bottom bar. The bands touch
        // (the tie-together: the list absorbed the gaps), so the
        // list is taller and the bottom bar sits one pixel higher.
        // The reshape (Tom, 2026-11-11): the description shrank
        // to its content (the longest description is four lines
        // plus the status line) and the list took the freed
        // space, so the rows are taller and the value boxes fill
        // them like the sample's tall inset boxes.
        CHECK(layout.header.y == 10);
        CHECK(layout.header.h == 26);
        CHECK(layout.list.y == 36);
        CHECK(layout.list.h == 314);
        CHECK(layout.description.y == 350);
        CHECK(layout.description.h == 89);
        CHECK(layout.bottom_bar.y == 439);
        CHECK(layout.bottom_bar.h == 31);
        CHECK(Rect_Bottom(layout.bottom_bar) == 470);
        // Tabs tile the bottom bar's left width in proportion to
        // their label lengths, with no gap between them (one
        // continuous band, like the colony's SORT band) and
        // spanning the bar's full height, so their borders are
        // the bar's borders.
        CHECK(layout.tab_bar.x == 13);
        CHECK(layout.tab_bar.y == 439);
        CHECK(layout.tab_bar.w == 400);
        CHECK(layout.tab_bar.h == 31);
        CHECK(layout.tabs[0].x == 13);
        CHECK(layout.tabs[0].w == 189);
        CHECK(layout.tabs[1].x == 202);
        CHECK(layout.tabs[1].w == 126);
        CHECK(layout.tabs[2].x == 328);
        CHECK(layout.tabs[2].w == 85);
        CHECK(Rect_Right(layout.tabs[2]) == Rect_Right(layout.tab_bar));
        CHECK(layout.tabs[0].w > layout.tabs[1].w);
        CHECK(layout.tabs[1].w > layout.tabs[2].w);
        // VALUE zone at 77.8% of the width, in the header and rows.
        // It moved left with the (wider) scrollbar strip, so the
        // value boxes keep their exact width -- the strip's extra
        // width comes out of the name zone instead. The strip now
        // touches the list (no gap), so the value zone is 4 px
        // wider than before.
        CHECK(layout.header_value.x == 498);
        CHECK(layout.header_value.w == 116);
        CHECK(layout.header_setting.w == 479);
        // Scrollbar strip at the content's right edge: chunky, like
        // the colony's, so the arrows read as buttons. It runs the
        // header band as well as the list.
        CHECK(layout.scrollbar.x == 614);
        CHECK(layout.scrollbar.w == 13);
        CHECK(layout.scrollbar.y == 10);
        CHECK(layout.scrollbar.h == 340);
        CHECK(layout.list.w == 601);
        // MORE / RETURN at the bottom bar's right end, same size,
        // spanning the bar's full height. The width (99 at 640) fits
        // RETURN's label (~79 px at button cell 18, measured from the
        // screenshot) with ~8 px of margin inside the button's borders
        // on each side (Tom, 2026-11-11: RETURN was tight/clipped
        // against the frame's right edge at 85 px); the tabs give up
        // the width.
        CHECK(layout.show_more.x == 421);
        CHECK(layout.show_more.w == 99);
        CHECK(layout.done.x == 528);
        CHECK(layout.done.w == 99);
        CHECK(layout.show_more.h == 31);
        CHECK(layout.done.h == 31);
        CHECK(layout.show_more.y == 439);
        // Rows tile the list height exactly. The description
        // shrank to its content and the list took the freed
        // space, so the rows are taller and the bottom bar does
        // not move. The value control is 80% of the row (the
        // sample's tall inset boxes), vertically centred.
        CHECK(Row_Rect(layout, 0).y == 36);
        CHECK(Row_Rect(layout, 0).h == 31);
        CHECK(Row_Rect(layout, 9).y == 318);
        CHECK(Rect_Bottom(Row_Rect(layout, 9)) == 350);
        CHECK(Row_Value_Rect(layout, 0).y == 39);
        CHECK(Row_Value_Rect(layout, 0).h == 24);
        Check_Bands_In_Order(layout);
    }

    TEST_CASE("layout scales with the screen size") {
        const s_settings_layout small =
            Compute_Layout(640, 480, 10, kLabel_Lengths);
        const s_settings_layout big =
            Compute_Layout(1280, 960, 10, kLabel_Lengths);
        // Doubling the screen doubles every rect (within 2 px of
        // rounding): the layout is percentages, not fixed pixels.
        // The tolerance is 2, not 1: each band rounds independently,
        // so a doubled rect can drift by the rounding of every
        // band above it (the bottom bar's y drifts 2 px).
        const auto near_double = [](int16_t a, int16_t b) {
            return b >= 2 * a - 2 && b <= 2 * a + 2;
        };
        CHECK(near_double(small.tab_bar.y, big.tab_bar.y));
        CHECK(near_double(small.tab_bar.h, big.tab_bar.h));
        CHECK(near_double(small.header.y, big.header.y));
        CHECK(near_double(small.list.y, big.list.y));
        CHECK(near_double(small.list.h, big.list.h));
        CHECK(near_double(small.description.y, big.description.y));
        CHECK(near_double(small.bottom_bar.y, big.bottom_bar.y));
        CHECK(near_double(small.tabs[0].w, big.tabs[0].w));
        // The last tab absorbs the rounding remainder, so it can
        // drift 2 px on a doubled screen.
        CHECK(big.tabs[2].w >= 2 * small.tabs[2].w - 2);
        CHECK(big.tabs[2].w <= 2 * small.tabs[2].w + 2);
        CHECK(near_double(small.header_value.x, big.header_value.x));
        CHECK(near_double(small.scrollbar.w, big.scrollbar.w));
        CHECK(near_double(small.done.w, big.done.w));
        CHECK(near_double(Row_Rect(small, 0).h, Row_Rect(big, 0).h));
        Check_Bands_In_Order(big);

        const s_settings_layout wide =
            Compute_Layout(1920, 1440, 10, kLabel_Lengths);
        CHECK(wide.tab_bar.y == 1319);
        CHECK(wide.list.h == 943);
        Check_Bands_In_Order(wide);

        const s_settings_layout mid =
            Compute_Layout(800, 600, 10, kLabel_Lengths);
        CHECK(mid.tab_bar.y == 550);
        CHECK(mid.list.h == 393);
        Check_Bands_In_Order(mid);
    }

    TEST_CASE("a wrapped row spans two slots at full size") {
        const s_settings_layout layout =
            Compute_Layout(640, 480, 10, kLabel_Lengths);
        // Row 3 as a single slot, and as a two-slot span.
        const s_rect single = Row_Rect(layout, 3);
        const s_rect span = Row_Rect_For_Span(layout, 3, 2);
        CHECK(span.x == single.x);
        CHECK(span.y == single.y);
        CHECK(span.w == single.w);
        CHECK(span.h == single.h + Row_Rect(layout, 4).h);
        CHECK(Rect_Bottom(span) == Rect_Bottom(Row_Rect(layout, 4)));
        // A one-slot span is the row itself; the span clips to
        // the list at the end; lines clamp to at least 1.
        CHECK(Row_Rect_For_Span(layout, 3, 1).h == single.h);
        CHECK(Rect_Bottom(Row_Rect_For_Span(layout, 9, 2)) ==
            Rect_Bottom(Row_Rect(layout, 9)));
        CHECK(Row_Rect_For_Span(layout, 3, 0).h == single.h);
        // The name and value zones follow the merged row: the
        // name spans the merged width, the value control is 80%
        // of the merged height, vertically centred.
        const s_rect name = Row_Name_Rect_For_Row(layout, span);
        CHECK(name.y == span.y);
        CHECK(name.h == span.h);
        CHECK(Rect_Right(name) == Rect_Right(layout.header_setting));
        const s_rect value = Row_Value_Rect_For_Row(layout, span);
        CHECK(value.x == layout.header_value.x);
        CHECK(value.w == layout.header_value.w);
        CHECK(value.h == span.h * 80 / 100);
        CHECK(value.y == span.y + (span.h - value.h) / 2);
        CHECK(Inside(value, span));
        // The single-slot helpers agree with the span helpers.
        CHECK(Row_Name_Rect(layout, 3).h == single.h);
        CHECK(Row_Value_Rect(layout, 3).h == single.h * 80 / 100);
    }

    TEST_CASE("slot arithmetic over per-setting line counts") {
        // 11 settings; setting 2 wraps onto a second line.
        const int lines[11] = {1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1};
        // From the top, 9 settings fit in 10 slots (the wrapped
        // one takes two): 1+1+2+1+1+1+1+1+1 = 10.
        CHECK(Settings_On_Page(lines, 11, 0, 10) == 9);
        // From setting 2, 9 settings fit (2+1*8 = 10 slots).
        CHECK(Settings_On_Page(lines, 11, 2, 10) == 9);
        // From setting 3, all 8 remaining fit in 8 slots.
        CHECK(Settings_On_Page(lines, 11, 3, 10) == 8);
        // The last page starts at setting 2: settings 2..10 take
        // exactly 10 slots.
        CHECK(First_For_Last_Page(lines, 11, 10) == 2);
        // No wrap: the last page is the usual total - 10.
        const int flat[21] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        CHECK(First_For_Last_Page(flat, 21, 10) == 11);
        CHECK(Settings_On_Page(flat, 21, 0, 10) == 10);
        // Everything fits: the last page is the first page.
        CHECK(First_For_Last_Page(lines, 5, 10) == 0);
        CHECK(Settings_On_Page(lines, 5, 0, 10) == 5);
        // Lines before / on a page.
        CHECK(Lines_Before(lines, 11, 3) == 4);
        CHECK(Lines_Before(lines, 11, 0) == 0);
        CHECK(Lines_On_Page(lines, 11, 0, 10) == 10);
        CHECK(Lines_On_Page(lines, 11, 2, 10) == 10);
    }

    TEST_CASE("scrollbar thumb follows the scroll offset") {
        const s_settings_layout layout =
            Compute_Layout(640, 480, 10, kLabel_Lengths);
        // A list that fits spans the whole track (the colony's
        // rule): the thumb is the colony's narrow indicator bar,
        // inset from the strip's edges, travelling between the
        // arrow/track dividers.
        const s_rect none = Scrollbar_Thumb_Rect(layout, 10, 0, 10);
        CHECK(none.w == 6);
        CHECK(none.x == layout.scrollbar_track.x + 2);
        CHECK(none.y == layout.scrollbar_track.y + 2);
        CHECK(none.h == layout.scrollbar_track.h - 4);
        const s_rect none2 = Scrollbar_Thumb_Rect(layout, 5, 0, 10);
        CHECK(none2.w == 6);
        // 186 lines, 10 visible: the thumb is ~10/186 of the
        // track, at the top when scrolled to the first lines,
        // at the bottom at the end.
        const s_rect top = Scrollbar_Thumb_Rect(layout, 186, 0, 10);
        CHECK(top.w == 6);
        CHECK(top.h == 15);
        CHECK(top.y == layout.scrollbar_track.y + 2);
        const s_rect bottom = Scrollbar_Thumb_Rect(layout, 186, 176, 10);
        CHECK(bottom.h == 15);
        CHECK(Rect_Bottom(bottom) ==
            Rect_Bottom(layout.scrollbar_track) - 2);
        const s_rect middle = Scrollbar_Thumb_Rect(layout, 186, 88, 10);
        CHECK(middle.y > top.y);
        CHECK(middle.y < bottom.y);
        CHECK(Inside(middle, layout.scrollbar_track));
        // A very long list still gets a visible thumb.
        const s_rect tiny = Scrollbar_Thumb_Rect(layout, 1000, 500, 10);
        CHECK(tiny.h == 8);
        CHECK(Inside(tiny, layout.scrollbar_track));
    }

    TEST_CASE("scrollbar arrows cap the strip and the thumb travels the track between them") {
        const s_settings_layout layout =
            Compute_Layout(640, 480, 10, kLabel_Lengths);
        // The arrows are the strip's width and twice as tall
        // (the colony's ~1:2 arrow aspect: 10x20, 11x22).
        CHECK(layout.scrollbar_arrow_up.x == layout.scrollbar.x);
        CHECK(layout.scrollbar_arrow_up.y == layout.scrollbar.y);
        CHECK(layout.scrollbar_arrow_up.w == layout.scrollbar.w);
        CHECK(layout.scrollbar_arrow_up.h == 2 * layout.scrollbar.w);
        CHECK(layout.scrollbar_arrow_up.h == 26);
        CHECK(layout.scrollbar_arrow_down.x == layout.scrollbar.x);
        CHECK(layout.scrollbar_arrow_down.w == layout.scrollbar.w);
        CHECK(layout.scrollbar_arrow_down.h == 2 * layout.scrollbar.w);
        CHECK(Rect_Bottom(layout.scrollbar_arrow_down) ==
            Rect_Bottom(layout.scrollbar));
        // The track is the strip between the arrows.
        CHECK(layout.scrollbar_track.x == layout.scrollbar.x);
        CHECK(layout.scrollbar_track.w == layout.scrollbar.w);
        CHECK(layout.scrollbar_track.y ==
            layout.scrollbar.y + layout.scrollbar_arrow_up.h);
        CHECK(Rect_Bottom(layout.scrollbar_track) ==
            layout.scrollbar_arrow_down.y);
        CHECK(layout.scrollbar_track.h ==
            layout.scrollbar.h - 2 * layout.scrollbar_arrow_up.h);
        CHECK(layout.scrollbar_track.h == 288);
        // The thumb stays inside the track at both ends (inset
        // from the arrow/track dividers).
        const s_rect top = Scrollbar_Thumb_Rect(layout, 186, 0, 10);
        CHECK(top.y == layout.scrollbar_track.y + 2);
        CHECK(Inside(top, layout.scrollbar_track));
        const s_rect bottom = Scrollbar_Thumb_Rect(layout, 186, 176, 10);
        CHECK(Rect_Bottom(bottom) ==
            Rect_Bottom(layout.scrollbar_track) - 2);
        CHECK(Inside(bottom, layout.scrollbar_track));
        // The arrow zones do not overlap the track.
        CHECK_FALSE(Overlaps(layout.scrollbar_arrow_up,
            layout.scrollbar_track));
        CHECK_FALSE(Overlaps(layout.scrollbar_arrow_down,
            layout.scrollbar_track));
    }

    TEST_CASE("dropdown popup anchors to the control and stays in the frame") {
        const s_settings_layout layout =
            Compute_Layout(640, 480, 10, kLabel_Lengths);
        // content 13..627 x 10..470 at 640x480 (margins 20 permille).
        CHECK(layout.content.x == 13);
        CHECK(layout.content.y == 10);
        CHECK(Rect_Right(layout.content) == 627);
        // A popup anchored to the value column's right edge (the
        // closed control's right edge) sits with its right edge
        // there, its top at the row's top.
        const s_rect at_anchor = Popup_Rect(layout.content, 618, 100, 120, 90);
        CHECK(Rect_Right(at_anchor) == 618);
        CHECK(at_anchor.y == 100);
        CHECK(at_anchor.w == 120);
        CHECK(at_anchor.h == 90);
        CHECK(Inside(at_anchor, layout.content));
        // Anchored past the frame's right edge: the popup shifts
        // left so its right edge is the frame's, never past it.
        const s_rect past_right = Popup_Rect(layout.content, 700, 100, 120, 90);
        CHECK(Rect_Right(past_right) == 627);
        CHECK(Inside(past_right, layout.content));
        // Anchored near the bottom: the popup shifts up so its
        // bottom is the frame's, never past it.
        const s_rect past_bottom = Popup_Rect(layout.content, 618, 450, 120, 90);
        CHECK(Rect_Bottom(past_bottom) == 470);
        CHECK(Inside(past_bottom, layout.content));
        // Anchored above the frame: the popup pins to the top.
        const s_rect above = Popup_Rect(layout.content, 618, -50, 120, 90);
        CHECK(above.y == layout.content.y);
        CHECK(Inside(above, layout.content));
        // A popup wider than the content pins to the content's
        // left edge instead of running off the right one.
        const s_rect too_wide = Popup_Rect(layout.content, 618, 100, 900, 90);
        CHECK(too_wide.x == layout.content.x);
        // The same placement scales: at 1920x1440 the anchored
        // popup still sits inside the frame.
        const s_settings_layout wide =
            Compute_Layout(1920, 1440, 10, kLabel_Lengths);
        const s_rect wide_popup = Popup_Rect(wide.content,
            Rect_Right(wide.header_value), 300, 360, 270);
        CHECK(Rect_Right(wide_popup) == Rect_Right(wide.header_value));
        CHECK(Inside(wide_popup, wide.content));
    }

    TEST_CASE("degenerate sizes lay out nothing") {
        const s_settings_layout zero =
            Compute_Layout(0, 0, 10, kLabel_Lengths);
        CHECK(zero.width == 0);
        CHECK(zero.tab_bar.w == 0);
        CHECK(zero.list.h == 0);
        const s_settings_layout no_rows =
            Compute_Layout(640, 480, 0, kLabel_Lengths);
        CHECK(no_rows.width == 0);
    }

} // namespace
