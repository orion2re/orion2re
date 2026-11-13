#include "pch.h"

#include "doctest/doctest.h"
#include "game/settings_layout.h"

namespace {

    using namespace SETTINGS_LAYOUT;

    // The content rect the dropdown window is clamped into
    // (the 640x480 screen's content: 20 permille margins).
    const s_rect kContent{13, 10, 614, 460};

    TEST_CASE("dropdown window encloses the list with a margin") {
        // Title + 3 items, 20 px rows, at (100, 50), 200 wide.
        const s_dropdown_window w = Dropdown_Window(kContent,
            100, 50, 200, 80, 20, 4);
        // The margin is the 5 px bitmap border plus 3 px of padding.
        CHECK(w.margin == 8);
        CHECK(w.margin == kDropdown_Border + 3);
        CHECK(w.step == 20);
        CHECK(w.rows == 4);
        // The window is the list plus the margin on all sides.
        CHECK(w.window.x == 92);
        CHECK(w.window.y == 42);
        CHECK(w.window.w == 216);
        CHECK(w.window.h == 96);
        // The list itself sits inside the window.
        CHECK(w.window.x <= 100);
        CHECK(w.window.y <= 50);
        CHECK(Rect_Right(w.window) >= 300);
        CHECK(Rect_Bottom(w.window) >= 130);
    }

    TEST_CASE("dropdown window clamps inside the content rect") {
        // A list hard against the content's right and bottom edges.
        const s_dropdown_window w = Dropdown_Window(kContent,
            500, 400, 120, 60, 20, 3);
        CHECK(w.window.x >= kContent.x);
        CHECK(w.window.y >= kContent.y);
        CHECK(Rect_Right(w.window) <= Rect_Right(kContent));
        CHECK(Rect_Bottom(w.window) <= Rect_Bottom(kContent));
        // A list wider than the content pins to the content.
        const s_dropdown_window wide = Dropdown_Window(kContent,
            0, 0, 700, 500, 20, 3);
        CHECK(wide.window.x == kContent.x);
        CHECK(wide.window.y == kContent.y);
        CHECK(Rect_Right(wide.window) <= Rect_Right(kContent));
        CHECK(Rect_Bottom(wide.window) <= Rect_Bottom(kContent));
    }

    TEST_CASE("dropdown separators sit on the row boundaries") {
        const s_dropdown_window w = Dropdown_Window(kContent,
            100, 50, 200, 80, 20, 4);
        // Three separators for four rows (title + 3 items).
        // Window y (42) + margin (8) + row index * step.
        CHECK(Dropdown_Separator_Y(w, 1) == 42 + 8 + 20);
        CHECK(Dropdown_Separator_Y(w, 2) == 42 + 8 + 40);
        CHECK(Dropdown_Separator_Y(w, 3) == 42 + 8 + 60);
        // Each separator is strictly inside the window, between
        // the first and last row.
        for (int k = 1; k < 4; ++k) {
            const int16_t sy = Dropdown_Separator_Y(w, k);
            CHECK(sy > w.window.y);
            CHECK(sy < Rect_Bottom(w.window));
        }
        // Out-of-range indices clamp to the first/last separator.
        CHECK(Dropdown_Separator_Y(w, 0) ==
            Dropdown_Separator_Y(w, 1));
        CHECK(Dropdown_Separator_Y(w, 99) ==
            Dropdown_Separator_Y(w, 3));
    }

    TEST_CASE("dropdown border is the screen frame's border, thinner") {
        // The bitmap border (5 px) is thinner than the screen
        // frame's ~8 px border; the margin is the border plus
        // 3 px of padding.
        CHECK(kDropdown_Border == 5);
        const s_dropdown_window w = Dropdown_Window(kContent,
            100, 50, 200, 80, 20, 4);
        CHECK(w.margin == kDropdown_Border + 3);
    }

    TEST_CASE("dropdown triangle sits at the button's right end") {
        const s_rect button{508, 100, 104, 26};
        const s_rect tri = Dropdown_Triangle_Rect(button);
        CHECK(tri.w == 7);
        CHECK(tri.h == 4);
        // Right edge 4 px inside the button's right edge.
        CHECK(Rect_Right(tri) == 508 + 104 - 4);
        CHECK(tri.x == 508 + 104 - 4 - 7);
        // Vertically centred on the button.
        CHECK(tri.y == 100 + (26 - 4) / 2);
        CHECK(Rect_Bottom(tri) == 100 + (26 + 4) / 2);
        // Inside the button.
        CHECK(tri.x >= button.x);
        CHECK(tri.y >= button.y);
        CHECK(Rect_Right(tri) <= Rect_Right(button));
        CHECK(Rect_Bottom(tri) <= Rect_Bottom(button));
    }

    TEST_CASE("dropdown value text clears the triangle zone") {
        // A 104 px button (the dropdown-closed frame): the centred
        // text's right edge must clear the triangle zone's left
        // edge (104 - 14). The zone is 14 px wide, plus 1 px.
        const int16_t max_w = Dropdown_Value_Text_Width(104);
        CHECK(max_w == 104 - 2 * 14 - 2);
        // The widest dropdown value, "overview" (74 px), fits.
        CHECK(max_w >= 74);
        // The text's right edge, centred, clears the zone:
        // (104 + max_w) / 2 <= zone left edge - 1 (104 - 14 - 1).
        CHECK((104 + max_w) / 2 <= 104 - 14 - 1);
        // Narrower buttons still leave room for a short value.
        CHECK(Dropdown_Value_Text_Width(85) >= 10);
        CHECK(Dropdown_Value_Text_Width(20) >= 1);
    }

} // namespace
