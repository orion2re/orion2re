#include "pch.h"

#include "doctest/doctest.h"

#include <cstring>
#include <utility>

// Field payloads are packed, so their members may be misaligned. Assertions
// on them use double parentheses so doctest compares the values directly
// instead of binding references to the packed members.

DOCTEST_TEST_SUITE("fields helpers") {

DOCTEST_TEST_CASE("field data button returns typed payload for mutable and const access") {
    s_field field{};
    field.field_type = FIELD_TYPE_BUTTON;
    field.detail.button.help = "ok";
    field.detail.button.draw_style = 3;

    s_field_button* payload = fields::Field_Data_Button(&field);
    const s_field_button* const_payload = fields::Field_Data_Button(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.button);
    DOCTEST_REQUIRE(const_payload == &field.detail.button);
    DOCTEST_REQUIRE(std::strcmp(payload->help, "ok") == 0);
    DOCTEST_REQUIRE((const_payload->draw_style == 3));
}

DOCTEST_TEST_CASE("field data irregular button returns irregular payload") {
    s_field field{};
    field.field_type = FIELD_TYPE_IRREGULAR_BUTTON;
    field.detail.irregular_button.help = "odd";
    field.detail.irregular_button.draw_x = 11;
    field.detail.irregular_button.draw_y = 17;

    s_field_irregular_button* payload = fields::Field_Data_Irregular_Button(&field);
    const s_field_irregular_button* const_payload = fields::Field_Data_Irregular_Button(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.irregular_button);
    DOCTEST_REQUIRE(const_payload == &field.detail.irregular_button);
    DOCTEST_REQUIRE(std::strcmp(payload->help, "odd") == 0);
    DOCTEST_REQUIRE((const_payload->draw_x == 11));
    DOCTEST_REQUIRE((const_payload->draw_y == 17));
}

DOCTEST_TEST_CASE("field data radio returns radio payload") {
    s_field field{};
    int16_t selection = 4;

    field.field_type = FIELD_TYPE_RADIO;
    field.detail.radio.selection = &selection;
    field.detail.radio.draw_style = 2;

    s_field_radio* payload = fields::Field_Data_Radio(&field);
    const s_field_radio* const_payload = fields::Field_Data_Radio(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.radio);
    DOCTEST_REQUIRE(const_payload == &field.detail.radio);
    DOCTEST_REQUIRE((payload->selection == &selection));
    DOCTEST_REQUIRE((const_payload->draw_style == 2));
}

DOCTEST_TEST_CASE("field data locked button returns locked payload") {
    s_field field{};
    int16_t selection = 9;

    field.field_type = FIELD_TYPE_LOCKED;
    field.detail.locked.help = "lock";
    field.detail.locked.selection = &selection;

    s_field_locked* payload = fields::Field_Data_Locked_Button(&field);
    const s_field_locked* const_payload = fields::Field_Data_Locked_Button(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.locked);
    DOCTEST_REQUIRE(const_payload == &field.detail.locked);
    DOCTEST_REQUIRE(std::strcmp(payload->help, "lock") == 0);
    DOCTEST_REQUIRE((const_payload->selection == &selection));
}

DOCTEST_TEST_CASE("field data multi button returns multi button payload") {
    s_field field{};
    int16_t selection = 1;

    field.field_type = FIELD_TYPE_MULTI_BUTTON;
    field.detail.multibtn.selection = &selection;
    field.detail.multibtn.button_value = 6;

    s_field_multi_button* payload = fields::Field_Data_Multi_Button(&field);
    const s_field_multi_button* const_payload = fields::Field_Data_Multi_Button(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.multibtn);
    DOCTEST_REQUIRE(const_payload == &field.detail.multibtn);
    DOCTEST_REQUIRE((payload->selection == &selection));
    DOCTEST_REQUIRE((const_payload->button_value == 6));
}

DOCTEST_TEST_CASE("field data scroll accepts scroll type") {
    s_field field{};
    int16_t value = 12;

    field.field_type = FIELD_TYPE_SCROLL;
    field.detail.scroll.value = &value;
    field.detail.scroll.range_min = 3;
    field.detail.scroll.range_max = 15;

    s_field_scroll* payload = fields::Field_Data_Scroll(&field);
    const s_field_scroll* const_payload = fields::Field_Data_Scroll(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.scroll);
    DOCTEST_REQUIRE(const_payload == &field.detail.scroll);
    DOCTEST_REQUIRE((payload->value == &value));
    DOCTEST_REQUIRE((const_payload->range_min == 3));
    DOCTEST_REQUIRE((const_payload->range_max == 15));
}

DOCTEST_TEST_CASE("field data scroll accepts scroll return type") {
    s_field field{};
    field.field_type = FIELD_TYPE_SCROLL_RETURN;
    field.detail.scroll.return_value = 7;
    field.detail.scroll.set_value = 5;

    s_field_scroll* payload = fields::Field_Data_Scroll(&field);
    const s_field_scroll* const_payload = fields::Field_Data_Scroll(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.scroll);
    DOCTEST_REQUIRE(const_payload == &field.detail.scroll);
    DOCTEST_REQUIRE((payload->return_value == 7));
    DOCTEST_REQUIRE((const_payload->set_value == 5));
}

DOCTEST_TEST_CASE("field data legacy input returns legacy input payload pointer") {
    s_field field{};
    field.field_type = FIELD_TYPE_LEGACY_INPUT;

    char buffer[16] = "legacy";
    char cursor_colors[] = "rgb";
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.input_mode = 2;
    field.detail.input.cursor_color_ramp = cursor_colors;

    s_field_input_string* payload = fields::Field_Data_Legacy_Input(&field);
    const s_field_input_string* const_payload = fields::Field_Data_Legacy_Input(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.input);
    DOCTEST_REQUIRE(const_payload == &field.detail.input);
    DOCTEST_REQUIRE((payload->buffer == buffer));
    DOCTEST_REQUIRE((const_payload->max_length == 16));
    DOCTEST_REQUIRE((const_payload->input_mode == 2));
    DOCTEST_REQUIRE((const_payload->cursor_color_ramp == cursor_colors));
}

DOCTEST_TEST_CASE("field data continuous input returns cont input payload pointer") {
    s_field field{};
    field.field_type = FIELD_TYPE_CONTINUOUS_INPUT;

    char buffer[8] = "abc";
    field.detail.cont_input.buffer = buffer;
    field.detail.cont_input.max_length = 8;
    field.detail.cont_input.full_store_flag = 1;
    field.detail.cont_input.font_shadow_style = 4;

    s_field_cont_input* payload = fields::Field_Data_Continuous_Input(&field);
    const s_field_cont_input* const_payload = fields::Field_Data_Continuous_Input(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.cont_input);
    DOCTEST_REQUIRE(const_payload == &field.detail.cont_input);
    DOCTEST_REQUIRE((payload->buffer == buffer));
    DOCTEST_REQUIRE((payload->max_length == 8));
    DOCTEST_REQUIRE((const_payload->full_store_flag == 1));
    DOCTEST_REQUIRE((const_payload->font_shadow_style == 4));
}

DOCTEST_TEST_CASE("field data string list returns string list payload pointer") {
    s_field field{};
    field.field_type = FIELD_TYPE_STRING_LIST;

    int8_t selected_value = 2;
    uint8_t gradient_palette[] = {1, 2, 3};
    field.detail.strings.help = "row";
    field.detail.strings.value = &selected_value;
    field.detail.strings.help_start = 19;
    field.detail.strings.is_selectable = 1;
    field.detail.strings.gradient_mode = 7;
    field.detail.strings.gradient_palette = gradient_palette;
    field.detail.strings.gradient_extra = 42;

    s_field_string_list* payload = fields::Field_Data_String_List(&field);
    const s_field_string_list* const_payload = fields::Field_Data_String_List(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.strings);
    DOCTEST_REQUIRE(const_payload == &field.detail.strings);
    DOCTEST_REQUIRE(std::strcmp(payload->help, "row") == 0);
    DOCTEST_REQUIRE((payload->value == &selected_value));
    DOCTEST_REQUIRE((const_payload->help_start == 19));
    DOCTEST_REQUIRE((const_payload->is_selectable == 1));
    DOCTEST_REQUIRE((const_payload->gradient_mode == 7));
    DOCTEST_REQUIRE((const_payload->gradient_palette == gradient_palette));
    DOCTEST_REQUIRE((const_payload->gradient_extra == 42));
}

DOCTEST_TEST_CASE("field data grid returns grid payload pointer") {
    s_field field{};
    int16_t grid_x = 5;
    int16_t grid_y = 8;

    field.field_type = FIELD_TYPE_GRID;
    field.detail.grid.step_x = 12;
    field.detail.grid.step_y = 14;
    field.detail.grid.grid_x_ptr = &grid_x;
    field.detail.grid.grid_y_ptr = &grid_y;

    s_field_grid* payload = fields::Field_Data_Grid(&field);
    const s_field_grid* const_payload = fields::Field_Data_Grid(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.grid);
    DOCTEST_REQUIRE(const_payload == &field.detail.grid);
    DOCTEST_REQUIRE((payload->step_x == 12));
    DOCTEST_REQUIRE((payload->step_y == 14));
    DOCTEST_REQUIRE((const_payload->grid_x_ptr == &grid_x));
    DOCTEST_REQUIRE((const_payload->grid_y_ptr == &grid_y));
}

DOCTEST_TEST_CASE("field data circle returns circle payload pointer") {
    s_field field{};
    field.field_type = FIELD_TYPE_CIRCLE;
    field.detail.circle.center_x = 21;
    field.detail.circle.center_y = 34;
    field.detail.circle.radius_squared = 144;

    s_field_circle* payload = fields::Field_Data_Circle(&field);
    const s_field_circle* const_payload = fields::Field_Data_Circle(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.circle);
    DOCTEST_REQUIRE(const_payload == &field.detail.circle);
    DOCTEST_REQUIRE((payload->center_x == 21));
    DOCTEST_REQUIRE((payload->center_y == 34));
    DOCTEST_REQUIRE((const_payload->radius_squared == 144));
}

DOCTEST_TEST_CASE("field data multi hot key returns multi hot key payload pointer") {
    s_field field{};
    field.field_type = FIELD_TYPE_MULTI_HOTKEY;
    field.detail.multi_hotkey.help = "ctrl-h";
    field.detail.multi_hotkey.help_start = 3;
    field.detail.multi_hotkey.help_len = 6;

    s_field_multi_hotkey* payload = fields::Field_Data_Multi_Hot_Key(&field);
    const s_field_multi_hotkey* const_payload = fields::Field_Data_Multi_Hot_Key(&std::as_const(field));

    DOCTEST_REQUIRE(payload == &field.detail.multi_hotkey);
    DOCTEST_REQUIRE(const_payload == &field.detail.multi_hotkey);
    DOCTEST_REQUIRE(std::strcmp(payload->help, "ctrl-h") == 0);
    DOCTEST_REQUIRE((const_payload->help_start == 3));
    DOCTEST_REQUIRE((const_payload->help_len == 6));
}

DOCTEST_TEST_CASE("list selection step moves one item down and up") {
    const int16_t items[] = {0, 1, 2};
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 0, 1) == 1));
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 1, 1) == 2));
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 2, -1) == 1));
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 1, -1) == 0));
}

DOCTEST_TEST_CASE("list selection step clamps at the ends") {
    const int16_t items[] = {0, 1, 2};
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 2, 1) == 2));
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 0, -1) == 0));
    // A single item never moves.
    const int16_t single[] = {0};
    DOCTEST_REQUIRE((fields::List_Selection_Step(single, 1, 0, 1) == 0));
    DOCTEST_REQUIRE((fields::List_Selection_Step(single, 1, 0, -1) == 0));
}

DOCTEST_TEST_CASE("list selection step follows the item order, not arithmetic") {
    // Gaps in the ids (disabled items already excluded by the
    // caller): stepping follows the array, so Down from 0 lands
    // on 2, not on the missing 1.
    const int16_t items[] = {0, 2, 5};
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 0, 1) == 2));
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 2, 1) == 5));
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 5, -1) == 2));
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, 2, -1) == 0));
}

DOCTEST_TEST_CASE("list selection step with no current selection does not move") {
    const int16_t items[] = {0, 1, 2};
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, -1, 1) == -1));
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 3, -1, -1) == -1));
    // An empty list does not move either.
    DOCTEST_REQUIRE((fields::List_Selection_Step(items, 0, 0, 1) == 0));
}

}