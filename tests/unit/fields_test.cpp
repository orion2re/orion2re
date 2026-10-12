#include "pch.h"

#include "doctest/doctest.h"

#include <cstring>

#include "fakes/fake_test_support.h"

namespace {
    struct Test_Animation_Header {
        alignas(::s_animation_header) unsigned char storage[sizeof(::s_animation_header)]{};

        ::s_animation_header* operator&() {
            return reinterpret_cast<::s_animation_header*>(storage);
        }

        const ::s_animation_header* operator&() const {
            return reinterpret_cast<const ::s_animation_header*>(storage);
        }
    };

    int g_test_auto_function_calls = 0;
    int g_test_auto_script_mode = 0;
    int g_test_auto_script_step = 0;
    int16_t g_test_auto_script_button = 0;
    int16_t g_test_auto_script_x = 0;
    int16_t g_test_auto_script_y = 0;

    void Queue_Test_Mouse_Buffer_Click_() {
        test::fakes::g_mouse_buffer_flag = 1;
        test::fakes::g_mouse_buffer_flag2 = 1;
        test::fakes::g_mouse_buffer_button = g_test_auto_script_button;
        test::fakes::g_mouse_buffer_x = g_test_auto_script_x;
        test::fakes::g_mouse_buffer_y = g_test_auto_script_y;
        test::fakes::g_mouse_pointer_x = g_test_auto_script_x;
        test::fakes::g_mouse_pointer_y = g_test_auto_script_y;
    }

    void Test_Auto_Function_Increment_() {
        ++g_test_auto_function_calls;
    }

    void Test_Auto_Function_Queue_Keyboard_Input_() {
        ++g_test_auto_function_calls;
        test::fakes::g_keyboard_status = 1;
    }

    void Test_Auto_Function_List_Script_() {
        ++g_test_auto_function_calls;

        switch (g_test_auto_script_mode) {
            case 1:
                fields::Exit_List_Field();
                break;
            case 2:
                if (g_test_auto_script_step == 0) {
                    Queue_Test_Mouse_Buffer_Click_();
                } else if (g_test_auto_script_step == 1) {
                    fields::Exit_List_Field();
                }
                ++g_test_auto_script_step;
                break;
            case 3:
                if (g_test_auto_script_step == 0) {
                    Queue_Test_Mouse_Buffer_Click_();
                } else if (g_test_auto_script_step == 1) {
                    test::fakes::g_keyboard_status = 1;
                    test::fakes::g_read_key_value = 0x011B;
                }
                ++g_test_auto_script_step;
                break;
            default:
                break;
        }
    }

    struct Fields_Test_Reset {
        Fields_Test_Reset() {
            test::fakes::Reset_All_();
            g_test_auto_function_calls = 0;
            g_test_auto_script_mode = 0;
            g_test_auto_script_step = 0;
            g_test_auto_script_button = 0;
            g_test_auto_script_x = 0;
            g_test_auto_script_y = 0;
        }

        ~Fields_Test_Reset() {
            test::fakes::Reset_All_();
            g_test_auto_function_calls = 0;
            g_test_auto_script_mode = 0;
            g_test_auto_script_step = 0;
            g_test_auto_script_button = 0;
            g_test_auto_script_x = 0;
            g_test_auto_script_y = 0;
        }
    };
}

#define s_animation_header Test_Animation_Header

DOCTEST_TEST_SUITE("fields cpp") {

DOCTEST_TEST_CASE("timed wait supports the complete unsigned word duration domain") {
    const uint32_t durations[] = {1, 32767, 32768, 65535};

    for (uint32_t duration : durations) {
        Fields_Test_Reset reset;
        fields::Assign_Auto_Function(Test_Auto_Function_Increment_, 1);

        fields::Timed_Wait_For_Input(duration);

        DOCTEST_CHECK(g_test_auto_function_calls == duration);
    }
}

DOCTEST_TEST_CASE("zero timed wait retains manual input dismissal") {
    Fields_Test_Reset reset;
    fields::Assign_Auto_Function(Test_Auto_Function_Queue_Keyboard_Input_, 1);

    fields::Timed_Wait_For_Input(0);

    DOCTEST_CHECK(g_test_auto_function_calls == 2);
}

DOCTEST_TEST_CASE("add button field initializes semantic payload and bounds") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    test::fakes::g_anim_width = 21;
    test::fakes::g_anim_height = 9;
    test::fakes::g_anim_full_store_flag = 1;
    test::fakes::g_font_style = 7;

    const int16_t field_idx = fields::Add_Button_Field(10, 20, "Launch", &picture, "q", 5);

    DOCTEST_REQUIRE(field_idx == 1);
    DOCTEST_REQUIRE(fields::fields_count == 2);

    const s_field& field = fields::fields[field_idx];
    DOCTEST_CHECK(field.field_type == FIELD_TYPE_BUTTON);
    DOCTEST_CHECK(field.x == 10);
    DOCTEST_CHECK(field.y == 20);
    DOCTEST_CHECK(field.x_end == 30);
    DOCTEST_CHECK(field.y_end == 28);
    DOCTEST_CHECK(field.sound == 5);
    DOCTEST_CHECK(field.hotkey == 'q');
    DOCTEST_CHECK(field.font_style == 7);

    const s_field_button* button = fields::Field_Data_Button(&field);
    DOCTEST_REQUIRE(button != nullptr);
    DOCTEST_CHECK(std::strcmp(button->help, "Launch") == 0);
    DOCTEST_CHECK(button->pic == &picture);
    DOCTEST_CHECK(button->draw_style == 1);
}

DOCTEST_TEST_CASE("add irregular button field stores explicit draw coordinates") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    test::fakes::g_anim_full_store_flag = 2;

    const int16_t field_idx = fields::Add_Irregular_Button_Field(1, 2, 30, 40, "Odd", &picture, 11, 17, "a", 9);

    const s_field& field = fields::fields[field_idx];
    const s_field_irregular_button* payload = fields::Field_Data_Irregular_Button(&field);

    DOCTEST_CHECK(field.field_type == FIELD_TYPE_IRREGULAR_BUTTON);
    DOCTEST_CHECK(field.hotkey == 'a');
    DOCTEST_REQUIRE(payload != nullptr);
    DOCTEST_CHECK(payload->draw_x == 11);
    DOCTEST_CHECK(payload->draw_y == 17);
    DOCTEST_CHECK(payload->draw_style == 2);
    DOCTEST_CHECK(std::strcmp(payload->help, "Odd") == 0);
}

DOCTEST_TEST_CASE("add scroll field stores range value orientation and hotkey") {
    Fields_Test_Reset reset;
    int16_t value = 4;

    const int16_t field_idx = fields::Add_Scroll_Field(100, 50, 2, 12, 3, 10, 40, 8, &value, "m", 6);

    const s_field& field = fields::fields[field_idx];
    const s_field_scroll* scroll = fields::Field_Data_Scroll(&field);

    DOCTEST_CHECK(field.field_type == FIELD_TYPE_SCROLL);
    DOCTEST_CHECK(field.hotkey == 'm');
    DOCTEST_REQUIRE(scroll != nullptr);
    DOCTEST_CHECK(scroll->range_min == 2);
    DOCTEST_CHECK(scroll->range_max == 12);
    DOCTEST_CHECK(scroll->return_value == 3);
    DOCTEST_CHECK(scroll->set_value == 10);
    DOCTEST_CHECK(scroll->value == &value);
    DOCTEST_CHECK(scroll->is_vertical == 0);
}

DOCTEST_TEST_CASE("add grid field computes bounds and clears hotkey") {
    Fields_Test_Reset reset;
    int16_t grid_x = -1;
    int16_t grid_y = -1;

    const int16_t field_idx = fields::Add_Grid_Field(5, 6, 7, 8, 3, 4, &grid_x, &grid_y, 2);

    const s_field& field = fields::fields[field_idx];
    const s_field_grid* grid = fields::Field_Data_Grid(&field);

    DOCTEST_CHECK(field.field_type == FIELD_TYPE_GRID);
    DOCTEST_CHECK(field.x_end == 25);
    DOCTEST_CHECK(field.y_end == 37);
    DOCTEST_CHECK(field.hotkey == '\0');
    DOCTEST_REQUIRE(grid != nullptr);
    DOCTEST_CHECK(grid->step_x == 7);
    DOCTEST_CHECK(grid->step_y == 8);
    DOCTEST_CHECK(grid->grid_x_ptr == &grid_x);
    DOCTEST_CHECK(grid->grid_y_ptr == &grid_y);
}

DOCTEST_TEST_CASE("draw field for irregular button uses explicit draw origin") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    const int16_t field_idx = fields::Add_Irregular_Button_Field(20, 30, 60, 70, "Odd", &picture, 111, 222, "x", 4);

    fields::Draw_Field(field_idx, 0);

    DOCTEST_REQUIRE(test::fakes::g_draw_calls.size() == 1);
    DOCTEST_CHECK(test::fakes::g_draw_calls[0].x == 111);
    DOCTEST_CHECK(test::fakes::g_draw_calls[0].y == 222);
    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 1);
    DOCTEST_CHECK(std::strcmp(test::fakes::g_print_centered_calls[0].text, "Odd") == 0);
}

DOCTEST_TEST_CASE("draw field for multi button updates shared selection based on action") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 0;

    const int16_t field_idx = fields::Add_Multi_Button_Field(3, 4, "Pick", &picture, &selection, 9, "p", 1);

    DOCTEST_CHECK(fields::fields[field_idx].hotkey == 'p');
    fields::Draw_Field(field_idx, 1);
    DOCTEST_CHECK(selection == 9);

    fields::Draw_Field(field_idx, 0);
    DOCTEST_CHECK(selection == -1000);
}

DOCTEST_TEST_CASE("scan field returns matching field under pointer") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    fields::Add_Button_Field(10, 20, "A", &picture, "a", 0);
    fields::Add_Button_Field(100, 120, "B", &picture, "b", 0);

    test::fakes::g_mouse_pointer_x = 105;
    test::fakes::g_mouse_pointer_y = 123;

    DOCTEST_CHECK(fields::Scan_Field() == 2);
}

DOCTEST_TEST_CASE("scan input updates grid coordinates from mouse position") {
    Fields_Test_Reset reset;
    int16_t grid_x = -1;
    int16_t grid_y = -1;

    const int16_t field_idx = fields::Add_Grid_Field(10, 10, 5, 4, 6, 6, &grid_x, &grid_y, 0);
    test::fakes::g_mouse_pointer_x = 27;
    test::fakes::g_mouse_pointer_y = 19;

    const int16_t result = fields::Scan_Input();

    DOCTEST_CHECK(result == field_idx);
    DOCTEST_CHECK(grid_x == 3);
    DOCTEST_CHECK(grid_y == 2);
}

DOCTEST_TEST_CASE("increment and decrement scroll bar clamp within configured bounds") {
    Fields_Test_Reset reset;
    int16_t value = 10;

    const int16_t field_idx = fields::Add_Scroll_Field(0, 0, 0, 100, 10, 90, 20, 60, &value, "s", 0);

    fields::Increment_Scroll_Bar(field_idx);
    DOCTEST_CHECK(value == 15);

    value = 10;
    fields::Decrement_Scroll_Bar(field_idx);
    DOCTEST_CHECK(value == 10);
}

DOCTEST_TEST_CASE("find bar position uses horizontal scroll geometry and clamps to return set range") {
    Fields_Test_Reset reset;
    int16_t value = 0;

    const int16_t field_idx = fields::Add_Scroll_Field(100, 50, 0, 100, 20, 80, 40, 8, &value, "h", 0);
    test::fakes::g_mouse_pointer_x = 120;
    test::fakes::g_mouse_pointer_y = 52;

    fields::Find_Bar_Position(field_idx);

    DOCTEST_CHECK(value == 50);
}

DOCTEST_TEST_CASE("check multi hot key fields advances progress and matches complete sequence") {
    Fields_Test_Reset reset;

    const int16_t field_idx = fields::Add_Multi_Hot_Key_Field("AB");

    DOCTEST_CHECK(fields::Check_Multi_Hot_Key_Fields('A') == -1);
    DOCTEST_CHECK(fields::Field_Data_Multi_Hot_Key(&fields::fields[field_idx])->help_start == 1);
    DOCTEST_CHECK(fields::Check_Multi_Hot_Key_Fields('B') == field_idx);
}

DOCTEST_TEST_CASE("clear multi hot key offsets resets every active sequence") {
    Fields_Test_Reset reset;

    const int16_t first = fields::Add_Multi_Hot_Key_Field("AB");
    const int16_t second = fields::Add_Multi_Hot_Key_Field("CD");

    fields::Field_Data_Multi_Hot_Key(&fields::fields[first])->help_start = 1;
    fields::Field_Data_Multi_Hot_Key(&fields::fields[second])->help_start = 2;

    fields::Clear_Multi_Hot_Key_Offsets();

    DOCTEST_CHECK(fields::Field_Data_Multi_Hot_Key(&fields::fields[first])->help_start == 0);
    DOCTEST_CHECK(fields::Field_Data_Multi_Hot_Key(&fields::fields[second])->help_start == 0);
}

DOCTEST_TEST_CASE("check help list invokes textbox entry when pointer overlaps help box") {
    Fields_Test_Reset reset;
    s_help_box help_boxes[1]{};
    help_boxes[0].x1 = 10;
    help_boxes[0].y1 = 20;
    help_boxes[0].x2 = 30;
    help_boxes[0].y2 = 40;
    help_boxes[0].help_id = 77;

    fields::Set_Help_List(help_boxes, 1);
    test::fakes::g_mouse_pointer_x = 15;
    test::fakes::g_mouse_pointer_y = 25;

    DOCTEST_CHECK(fields::Check_Help_List() == 0);
    DOCTEST_CHECK(test::fakes::g_last_help_id == 77);
}

DOCTEST_TEST_CASE("add radio button field stores payload and lowercase hotkey") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 0;

    test::fakes::g_anim_width = 13;
    test::fakes::g_anim_height = 11;
    test::fakes::g_anim_full_store_flag = 1;

    const int16_t field_idx = fields::Add_Radio_Button_Field(8, 9, "Radio", &picture, &selection, "r", 7);

    const s_field& field = fields::fields[field_idx];
    const s_field_radio* radio = fields::Field_Data_Radio(&field);

    DOCTEST_REQUIRE(field_idx == 1);
    DOCTEST_CHECK(field.field_type == FIELD_TYPE_RADIO);
    DOCTEST_CHECK(field.x_end == 20);
    DOCTEST_CHECK(field.y_end == 19);
    DOCTEST_CHECK(field.hotkey == 'r');
    DOCTEST_REQUIRE(radio != nullptr);
    DOCTEST_CHECK(std::strcmp(radio->help, "Radio") == 0);
    DOCTEST_CHECK(radio->pic == &picture);
    DOCTEST_CHECK(radio->draw_style == 1);
    DOCTEST_CHECK(radio->selection == &selection);
}

DOCTEST_TEST_CASE("add locked button field stores payload and lowercase hotkey") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 0;

    test::fakes::g_anim_width = 14;
    test::fakes::g_anim_height = 6;
    test::fakes::g_anim_full_store_flag = 2;

    const int16_t field_idx = fields::Add_Locked_Button_Field(30, 40, "Lock", &picture, &selection, "l", 8);

    const s_field& field = fields::fields[field_idx];
    const s_field_locked* locked = fields::Field_Data_Locked_Button(&field);

    DOCTEST_REQUIRE(field_idx == 1);
    DOCTEST_CHECK(field.field_type == FIELD_TYPE_LOCKED);
    DOCTEST_CHECK(field.x_end == 43);
    DOCTEST_CHECK(field.y_end == 45);
    DOCTEST_CHECK(field.hotkey == 'l');
    DOCTEST_REQUIRE(locked != nullptr);
    DOCTEST_CHECK(std::strcmp(locked->help, "Lock") == 0);
    DOCTEST_CHECK(locked->pic == &picture);
    DOCTEST_CHECK(locked->draw_style == 2);
    DOCTEST_CHECK(locked->selection == &selection);
}

DOCTEST_TEST_CASE("add continuous input field stores copied colors and clamps max length") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char buffer[32] = "name";

    for (int i = 0; i < 8; ++i) {
        test::fakes::g_font_color.colors[i] = static_cast<uint8_t>(i + 10);
    }

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(50, 60, 70, 12, buffer, 999, 4, &picture, 3, 1);

    const s_field& field = fields::fields[field_idx];
    const s_field_cont_input* cont_input = fields::Field_Data_Continuous_Input(&field);

    DOCTEST_CHECK(field.field_type == FIELD_TYPE_CONTINUOUS_INPUT);
    DOCTEST_CHECK(field.x_end == 120);
    DOCTEST_CHECK(field.y_end == 72);
    DOCTEST_CHECK(field.font_color == &fields::list_normal_color);
    DOCTEST_CHECK(field.hotkey == 0);
    DOCTEST_REQUIRE(cont_input != nullptr);
    DOCTEST_CHECK(cont_input->highlight_color == &fields::list_highlight_colors);
    DOCTEST_CHECK(cont_input->buffer == buffer);
    DOCTEST_CHECK(cont_input->max_length == 256);
    DOCTEST_CHECK(cont_input->pic == &picture);
    DOCTEST_CHECK(cont_input->background_color == 4);
    DOCTEST_CHECK(cont_input->font_shadow_style == 1);

    for (int i = 0; i < 8; ++i) {
        DOCTEST_CHECK(fields::list_normal_color.colors[i] == test::fakes::g_font_color.colors[i]);
        DOCTEST_CHECK(fields::list_highlight_colors.colors[i] == test::fakes::g_font_color.colors[i]);
    }
}

DOCTEST_TEST_CASE("add string list field stores row metadata and hotkey") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int8_t selected_value = 2;
    uint8_t gradient_palette[] = {1, 2, 3};

    test::fakes::g_font_height = 9;
    test::fakes::g_font_style = 6;

    const int16_t field_idx = fields::Add_String_List_Field(10, 20, 30, "Row", 1, &selected_value, 4, 7, gradient_palette, 42, &picture, 0, "z", "");

    const s_field& field = fields::fields[field_idx];
    const s_field_string_list* string_list = fields::Field_Data_String_List(&field);

    DOCTEST_CHECK(field.field_type == FIELD_TYPE_STRING_LIST);
    DOCTEST_CHECK(field.x == 10);
    DOCTEST_CHECK(field.y == 19);
    DOCTEST_CHECK(field.x_end == 40);
    DOCTEST_CHECK(field.y_end == 30);
    DOCTEST_CHECK(field.hotkey == 'z');
    DOCTEST_CHECK(field.font_style == 6);
    DOCTEST_CHECK(field.font_color == &fields::list_normal_color);
    DOCTEST_REQUIRE(string_list != nullptr);
    DOCTEST_CHECK(std::strcmp(string_list->help, "Row") == 0);
    DOCTEST_CHECK(string_list->value == &selected_value);
    DOCTEST_CHECK(string_list->help_start == 4);
    DOCTEST_CHECK(string_list->is_selectable == 1);
    DOCTEST_CHECK(string_list->gradient_mode == 7);
    DOCTEST_CHECK(string_list->gradient_palette == gradient_palette);
    DOCTEST_CHECK(string_list->gradient_extra == 42);
    DOCTEST_CHECK(string_list->gradient_pic == &picture);
}

DOCTEST_TEST_CASE("draw field radio action path uses selected text offsets when selection is clear") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 0;

    test::fakes::g_anim_full_store_flag = 1;

    const int16_t field_idx = fields::Add_Radio_Button_Field(20, 30, "Radio", &picture, &selection, "r", 0);

    fields::Draw_Field(field_idx, 1);

    DOCTEST_REQUIRE(test::fakes::g_draw_calls.size() == 1);
    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 1);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].x == 30);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].y == 30);
}

DOCTEST_TEST_CASE("draw field locked selected path uses selected text offsets") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 1;

    test::fakes::g_anim_full_store_flag = 1;

    const int16_t field_idx = fields::Add_Locked_Button_Field(40, 50, "Lock", &picture, &selection, "l", 0);

    fields::Draw_Field(field_idx, 0);

    DOCTEST_REQUIRE(test::fakes::g_draw_calls.size() == 1);
    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 1);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].x == 50);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].y == 50);
}

DOCTEST_TEST_CASE("get alt alpha key maps known and unknown codes") {
    Fields_Test_Reset reset;

    DOCTEST_CHECK(fields::Get_Alt_Alpha_Key(0x1000) == 'Q');
    DOCTEST_CHECK(fields::Get_Alt_Alpha_Key(0x7900) == '1');
    DOCTEST_CHECK(fields::Get_Alt_Alpha_Key(0x9999) == 0xFF);
}

DOCTEST_TEST_CASE("add clipped circle field stores clipped bounds and circle payload") {
    Fields_Test_Reset reset;

    video::screen_window_x1 = 40;
    video::screen_window_y1 = 50;
    video::screen_window_x2 = 60;
    video::screen_window_y2 = 70;

    const int16_t field_idx = fields::Add_Clipped_Circle_Field(50, 60, 15, "c", 3);

    const s_field& field = fields::fields[field_idx];
    const s_field_circle* circle = fields::Field_Data_Circle(&field);

    DOCTEST_CHECK(field.field_type == FIELD_TYPE_CIRCLE);
    DOCTEST_CHECK(field.x == 40);
    DOCTEST_CHECK(field.y == 50);
    DOCTEST_CHECK(field.x_end == 60);
    DOCTEST_CHECK(field.y_end == 70);
    DOCTEST_CHECK(field.hotkey == 'c');
    DOCTEST_REQUIRE(circle != nullptr);
    DOCTEST_CHECK(circle->center_x == 50);
    DOCTEST_CHECK(circle->center_y == 60);
    DOCTEST_CHECK(circle->radius_squared == 225);
}

DOCTEST_TEST_CASE("interpret mouse input selects circle only when click is inside radius") {
    Fields_Test_Reset reset;

    const int16_t field_idx = fields::Add_Clipped_Circle_Field(50, 60, 10, "c", 3);

    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 1;
    test::fakes::g_mouse_buffer_x = 56;
    test::fakes::g_mouse_buffer_y = 64;

    const int16_t result = fields::Interpret_Mouse_Input();

    DOCTEST_CHECK(result == field_idx);
    DOCTEST_CHECK(fields::Pressed_Mouse_X() == 56);
    DOCTEST_CHECK(fields::Pressed_Mouse_Y() == 64);
    DOCTEST_CHECK(fields::last_button_number == 1);
    DOCTEST_CHECK(fields::last_button_x == 56);
    DOCTEST_CHECK(fields::last_button_y == 64);
}

DOCTEST_TEST_CASE("interpret mouse input ignores points outside circle radius even inside clipped bounds") {
    Fields_Test_Reset reset;

    fields::Add_Clipped_Circle_Field(50, 60, 10, "c", 3);

    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 1;
    test::fakes::g_mouse_buffer_x = 60;
    test::fakes::g_mouse_buffer_y = 69;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == 0);
}

DOCTEST_TEST_CASE("get input plays sound for selected field") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    const int16_t field_idx = fields::Add_Button_Field(10, 20, "Go", &picture, "g", 12);

    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 1;
    test::fakes::g_mouse_buffer_x = 12;
    test::fakes::g_mouse_buffer_y = 22;

    DOCTEST_CHECK(fields::Get_Input() == field_idx);
    DOCTEST_CHECK(test::fakes::g_last_played_sound == 12);
}

DOCTEST_TEST_CASE("get input returns F9 distance command without indexing the field table") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    fields::Add_Button_Field(10, 20, "Go", &picture, "g", 12);
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x4300;

    DOCTEST_CHECK(fields::Get_Input() == -1009);
    DOCTEST_CHECK(test::fakes::g_last_played_sound == -1);
}

DOCTEST_TEST_CASE("add clipped hidden field clips bounds and preserves shortcut conventions") {
    Fields_Test_Reset reset;

    video::screen_window_x1 = 100;
    video::screen_window_y1 = 200;
    video::screen_window_x2 = 150;
    video::screen_window_y2 = 260;

    const int16_t field_idx = fields::Add_Clipped_Hidden_Field(
        90, 190, 170, 270, "H", 4);

    const s_field& field = fields::fields[field_idx];
    DOCTEST_CHECK(field.field_type == FIELD_TYPE_HIDDEN);
    DOCTEST_CHECK(field.x == 100);
    DOCTEST_CHECK(field.y == 200);
    DOCTEST_CHECK(field.x_end == 150);
    DOCTEST_CHECK(field.y_end == 260);
    DOCTEST_CHECK(field.hotkey == 'h');

    const int16_t shifted_idx = fields::Add_Clipped_Hidden_Field(
        100, 200, 110, 210, "Q!", 0);
    const int16_t null_idx = fields::Add_Clipped_Hidden_Field(
        100, 200, 110, 210, nullptr, 0);
    DOCTEST_CHECK(fields::fields[shifted_idx].hotkey == 'Q');
    DOCTEST_CHECK(fields::fields[null_idx].hotkey == '\0');
}

DOCTEST_TEST_CASE("add clipped hidden field returns sentinel when rectangle is fully outside clip window") {
    Fields_Test_Reset reset;

    video::screen_window_x1 = 100;
    video::screen_window_y1 = 200;
    video::screen_window_x2 = 150;
    video::screen_window_y2 = 260;

    DOCTEST_CHECK(fields::Add_Clipped_Hidden_Field(0, 0, 10, 10, "h", 4) == -10000);
}

DOCTEST_TEST_CASE("add hot key creates hidden field with lowercase hotkey") {
    Fields_Test_Reset reset;

    const int16_t field_idx = fields::Add_Hot_Key("k");

    const s_field& field = fields::fields[field_idx];
    DOCTEST_CHECK(field.field_type == FIELD_TYPE_HIDDEN);
    DOCTEST_CHECK(field.x == 5000);
    DOCTEST_CHECK(field.y == 5000);
    DOCTEST_CHECK(field.hotkey == 'k');
}

DOCTEST_TEST_CASE("add hot key accepts null as no shortcut") {
    Fields_Test_Reset reset;

    const int16_t field_idx = fields::Add_Hot_Key(nullptr);

    DOCTEST_CHECK(fields::fields[field_idx].hotkey == '\0');
}

DOCTEST_TEST_CASE("add hidden field stores explicit bounds sound and lowercase hotkey") {
    Fields_Test_Reset reset;

    const int16_t field_idx = fields::Add_Hidden_Field(1, 2, 30, 40, "h", 7);

    DOCTEST_REQUIRE(field_idx == 1);
    DOCTEST_CHECK(fields::fields_count == 2);

    const s_field& field = fields::fields[field_idx];
    DOCTEST_CHECK(field.field_type == FIELD_TYPE_HIDDEN);
    DOCTEST_CHECK(field.x == 1);
    DOCTEST_CHECK(field.y == 2);
    DOCTEST_CHECK(field.x_end == 30);
    DOCTEST_CHECK(field.y_end == 40);
    DOCTEST_CHECK(field.sound == 7);
    DOCTEST_CHECK(field.hotkey == 'h');
}

DOCTEST_TEST_CASE("add hidden field lowercases one-character alphabetic hotkeys") {
    Fields_Test_Reset reset;

    const int16_t upper_idx = fields::Add_Hidden_Field(10, 20, 30, 40, "Q", 1);
    const int16_t digit_idx = fields::Add_Hidden_Field(11, 21, 31, 41, "7", 2);

    DOCTEST_CHECK(fields::fields[upper_idx].hotkey == 'q');
    DOCTEST_CHECK(fields::fields[digit_idx].hotkey == '7');
    DOCTEST_CHECK(fields::fields[digit_idx].sound == 2);
    DOCTEST_CHECK(fields::fields_count == 3);
}

DOCTEST_TEST_CASE("add hidden field appends sequentially after existing fields") {
    Fields_Test_Reset reset;

    fields::Add_Hidden_Field(1, 1, 2, 2, "a", 0);
    const int16_t second_idx = fields::Add_Hidden_Field(3, 4, 5, 6, "b", 9);

    DOCTEST_CHECK(second_idx == 2);
    DOCTEST_CHECK(fields::fields_count == 3);
    DOCTEST_CHECK(fields::fields[second_idx].x == 3);
    DOCTEST_CHECK(fields::fields[second_idx].y == 4);
    DOCTEST_CHECK(fields::fields[second_idx].x_end == 5);
    DOCTEST_CHECK(fields::fields[second_idx].y_end == 6);
    DOCTEST_CHECK(fields::fields[second_idx].hotkey == 'b');
    DOCTEST_CHECK(fields::fields[second_idx].sound == 9);
}

DOCTEST_TEST_CASE("scan input ignores non-selectable string list rows") {
    Fields_Test_Reset reset;
    int8_t selected_value = 0;

    fields::Add_String_List_Field(10, 20, 30, "Row", 0, &selected_value, 4, 0, nullptr, 0, nullptr, 0, "s", "");

    test::fakes::g_mouse_pointer_x = 15;
    test::fakes::g_mouse_pointer_y = 25;

    DOCTEST_CHECK(fields::Scan_Input() == 0);
}

DOCTEST_TEST_CASE("scan input accepts selectable string list rows") {
    Fields_Test_Reset reset;
    int8_t selected_value = 0;

    const int16_t field_idx = fields::Add_String_List_Field(10, 20, 30, "Row", 1, &selected_value, 4, 0, nullptr, 0, nullptr, 0, "s", "");

    test::fakes::g_mouse_pointer_x = 15;
    test::fakes::g_mouse_pointer_y = 25;

    DOCTEST_CHECK(fields::Scan_Input() == field_idx);
}

DOCTEST_TEST_CASE("copy continuous string trims trailing cursor marker before copying") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char buffer[32] = "old";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 40, 10, buffer, 12, 0, &picture, 0, 0);

    strlcpy(fields::continuous_string, "alpha_", sizeof(fields::continuous_string));
    fields::active_input_field_number = field_idx;

    fields::Copy_Continuous_String();

    DOCTEST_CHECK(std::strcmp(buffer, "alpha") == 0);
}

DOCTEST_TEST_CASE("interpret mouse input activates continuous input field and copies source buffer") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char buffer[32] = "captain";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 40, 10, buffer, 12, 0, &picture, 0, 0);

    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 1;
    test::fakes::g_mouse_buffer_x = 12;
    test::fakes::g_mouse_buffer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(fields::input_field_active == 1);
    DOCTEST_CHECK(fields::active_input_field_number == field_idx);
    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "captain") == 0);
}

DOCTEST_TEST_CASE("interpret mouse input switches active continuous input field") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char first_buffer[32] = "alpha";
    char second_buffer[32] = "beta";

    fields::Add_Continuous_String_Input_Field(10, 20, 40, 10, first_buffer, 12, 0, &picture, 0, 0);
    const int16_t second_idx = fields::Add_Continuous_String_Input_Field(60, 20, 40, 10, second_buffer, 12, 0, &picture, 0, 0);

    fields::input_field_active = 1;
    fields::active_input_field_number = 1;
    strlcpy(fields::continuous_string, "staged", sizeof(fields::continuous_string));

    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 1;
    test::fakes::g_mouse_buffer_x = 62;
    test::fakes::g_mouse_buffer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == second_idx);
    DOCTEST_CHECK(fields::input_field_active == 1);
    DOCTEST_CHECK(fields::active_input_field_number == second_idx);
    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "beta") == 0);
}

DOCTEST_TEST_CASE("add clipped button field returns sentinel when clipped sprite would fall outside view") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    animate::clipping_flag = 1;
    test::fakes::g_anim_width = 20;
    test::fakes::g_anim_height = 10;
    video::screen_window_x1 = 100;
    video::screen_window_y1 = 100;
    video::screen_window_x2 = 200;
    video::screen_window_y2 = 200;

    DOCTEST_CHECK(fields::Add_Clipped_Button_Field(90, 120, "Btn", &picture, "b", 1) == -1000);
}

DOCTEST_TEST_CASE("add clipped button field stores payload and bounds when clipping is disabled") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};

    animate::clipping_flag = 0;
    test::fakes::g_anim_width = 20;
    test::fakes::g_anim_height = 10;
    test::fakes::g_anim_full_store_flag = 2;
    test::fakes::g_font_style = 6;

    const int16_t field_idx = fields::Add_Clipped_Button_Field(90, 120, "Btn", &picture, "b", 1);

    DOCTEST_REQUIRE(field_idx == 1);
    DOCTEST_CHECK(fields::fields_count == 2);

    const s_field& field = fields::fields[field_idx];
    const s_field_button* button = fields::Field_Data_Button(&field);

    DOCTEST_CHECK(field.field_type == FIELD_TYPE_BUTTON);
    DOCTEST_CHECK(field.x == 90);
    DOCTEST_CHECK(field.y == 120);
    DOCTEST_CHECK(field.x_end == 109);
    DOCTEST_CHECK(field.y_end == 129);
    DOCTEST_CHECK(field.sound == 1);
    DOCTEST_CHECK(field.hotkey == 'b');
    DOCTEST_CHECK(field.font_style == 6);
    DOCTEST_REQUIRE(button != nullptr);
    DOCTEST_CHECK(std::strcmp(button->help, "Btn") == 0);
    DOCTEST_CHECK(button->pic == &picture);
    DOCTEST_CHECK(button->draw_style == 2);
}

DOCTEST_TEST_CASE("add clipped button field accepts sprite fully inside clipping window") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};

    animate::clipping_flag = 1;
    test::fakes::g_anim_width = 20;
    test::fakes::g_anim_height = 10;
    test::fakes::g_anim_full_store_flag = 1;
    video::screen_window_x1 = 100;
    video::screen_window_y1 = 100;
    video::screen_window_x2 = 200;
    video::screen_window_y2 = 200;

    const int16_t field_idx = fields::Add_Clipped_Button_Field(100, 100, "Clip", &picture, "c", 4);

    DOCTEST_REQUIRE(field_idx == 1);
    DOCTEST_CHECK(fields::fields_count == 2);
    DOCTEST_CHECK(fields::fields[field_idx].x == 100);
    DOCTEST_CHECK(fields::fields[field_idx].y == 100);
    DOCTEST_CHECK(fields::fields[field_idx].x_end == 119);
    DOCTEST_CHECK(fields::fields[field_idx].y_end == 109);
    DOCTEST_CHECK(fields::fields[field_idx].hotkey == 'c');
}

DOCTEST_TEST_CASE("add clipped button field rejects sprite touching right or bottom edge beyond allowed range") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};

    animate::clipping_flag = 1;
    test::fakes::g_anim_width = 20;
    test::fakes::g_anim_height = 10;
    video::screen_window_x1 = 100;
    video::screen_window_y1 = 100;
    video::screen_window_x2 = 119;
    video::screen_window_y2 = 109;

    DOCTEST_CHECK(fields::Add_Clipped_Button_Field(100, 100, "Clip", &picture, "c", 4) == -1000);
    DOCTEST_CHECK(fields::fields_count == 1);
}

DOCTEST_TEST_CASE("add clipped circle field returns sentinel when fully outside clip window") {
    Fields_Test_Reset reset;

    video::screen_window_x1 = 100;
    video::screen_window_y1 = 100;
    video::screen_window_x2 = 200;
    video::screen_window_y2 = 200;

    DOCTEST_CHECK(fields::Add_Clipped_Circle_Field(20, 20, 5, "c", 1) == -10000);
}

DOCTEST_TEST_CASE("clear fields above truncates field count and clears active mouse down state") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    fields::Add_Button_Field(10, 10, "A", &picture, "a", 0);
    fields::Add_Button_Field(30, 10, "B", &picture, "b", 0);
    fields::down_mouse_button = 2;

    fields::Clear_Fields_Above(1);

    DOCTEST_CHECK(fields::fields_count == 2);
    DOCTEST_CHECK(fields::down_mouse_button == -1);
}

DOCTEST_TEST_CASE("scan input on scroll return sets bound value and returns configured result") {
    Fields_Test_Reset reset;
    int16_t value = 1;

    const int16_t field_idx = fields::Add_Scroll_Field(10, 20, 0, 100, 7, 33, 20, 6, &value, "s", 0);
    fields::fields[field_idx].field_type = FIELD_TYPE_SCROLL_RETURN;
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Scan_Input() == 7);
    DOCTEST_CHECK(value == 33);
}

DOCTEST_TEST_CASE("interpret keyboard input maps function key to negative command index") {
    Fields_Test_Reset reset;
    int16_t field_idx = 0;

    test::fakes::g_read_key_value = 0x3B00;

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&field_idx) == 0x3B00);
    DOCTEST_CHECK(field_idx == -1001);
}

DOCTEST_TEST_CASE("interpret keyboard input resolves lowercase hotkey without changing case") {
    Fields_Test_Reset reset;

    const int16_t field_idx = fields::Add_Hot_Key("k");
    int16_t resolved_field = 0;
    test::fakes::g_read_key_value = 'k';

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == 'k');
    DOCTEST_CHECK(resolved_field == field_idx);
}

DOCTEST_TEST_CASE("interpret keyboard input suppresses hotkey search while continuous input is active") {
    Fields_Test_Reset reset;

    fields::Add_Hot_Key("k");
    fields::active_input_field_number = 1;
    int16_t resolved_field = 0;
    test::fakes::g_read_key_value = 'k';

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == 'k');
    DOCTEST_CHECK(resolved_field == 0);
}

DOCTEST_TEST_CASE("interpret keyboard input completes multi hotkey sequence") {
    Fields_Test_Reset reset;

    const int16_t field_idx = fields::Add_Multi_Hot_Key_Field("AB");
    fields::Check_Multi_Hot_Key_Fields('A');
    int16_t resolved_field = 0;

    fields::multi_hotkey_active_field = 1;
    test::fakes::g_read_key_value = 0x3000;

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == -1);
    DOCTEST_CHECK(resolved_field == field_idx);
}

DOCTEST_TEST_CASE("interpret keyboard input maps additional command keys to negative command indices") {
    Fields_Test_Reset reset;
    int16_t field_idx = 0;

    test::fakes::g_read_key_value = 0x3C00;
    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&field_idx) == 0x3C00);
    DOCTEST_CHECK(field_idx == -1002);

    test::fakes::g_read_key_value = 0x4400;
    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&field_idx) == 0x4400);
    DOCTEST_CHECK(field_idx == -1010);

    test::fakes::g_read_key_value = 0x6F00;
    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&field_idx) == 0x6F00);
    DOCTEST_CHECK(field_idx == -1108);

    test::fakes::g_read_key_value = 0x7100;
    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&field_idx) == 0x7100);
    DOCTEST_CHECK(field_idx == -1110);

    test::fakes::g_read_key_value = 0x01F0;
    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&field_idx) == 0x01F0);
    DOCTEST_CHECK(field_idx == -1500);
}

DOCTEST_TEST_CASE("interpret keyboard input resolves explicitly preserved uppercase hotkey") {
    Fields_Test_Reset reset;

    const int16_t field_idx = fields::Add_Hot_Key("K!");
    int16_t resolved_field = 0;
    test::fakes::g_read_key_value = 'K';

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == 'K');
    DOCTEST_CHECK(resolved_field == field_idx);
}

DOCTEST_TEST_CASE("interpret keyboard input does not match uppercase input to lowercase shortcut") {
    Fields_Test_Reset reset;

    fields::Add_Hot_Key("K");
    int16_t resolved_field = 0;
    test::fakes::g_read_key_value = 'K';

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == 'K');
    DOCTEST_CHECK(resolved_field == 0);
}

DOCTEST_TEST_CASE("interpret keyboard input leaves unmatched printable lowercase key unchanged") {
    Fields_Test_Reset reset;

    int16_t resolved_field = 0;
    test::fakes::g_read_key_value = 'x';

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == 'x');
    DOCTEST_CHECK(resolved_field == 0);
}

DOCTEST_TEST_CASE("interpret keyboard input falls back to original key value when low byte is zero and no field matches") {
    Fields_Test_Reset reset;

    int16_t resolved_field = 0;
    test::fakes::g_read_key_value = 0x8500;

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == static_cast<int16_t>(0x8500));
    DOCTEST_CHECK(resolved_field == 0);
}

DOCTEST_TEST_CASE("interpret keyboard input advances but does not complete multi hotkey sequence") {
    Fields_Test_Reset reset;

    fields::Add_Multi_Hot_Key_Field("AB");
    int16_t resolved_field = 0;

    fields::multi_hotkey_active_field = 1;
    test::fakes::g_read_key_value = 0x1E00;

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == 0x1E00);
    DOCTEST_CHECK(resolved_field == 0);
    DOCTEST_CHECK(fields::Field_Data_Multi_Hot_Key(&fields::fields[1])->help_start == 1);
}

DOCTEST_TEST_CASE("interpret keyboard input leaves multi hotkey unresolved when alt mapping is unknown") {
    Fields_Test_Reset reset;

    fields::Add_Multi_Hot_Key_Field("AB");
    int16_t resolved_field = 0;

    fields::multi_hotkey_active_field = 1;
    test::fakes::g_read_key_value = 0x9999;

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == static_cast<int16_t>(0x9999));
    DOCTEST_CHECK(resolved_field == 0);
    DOCTEST_CHECK(fields::Field_Data_Multi_Hot_Key(&fields::fields[1])->help_start == 0);
}

DOCTEST_TEST_CASE("interpret keyboard input suppresses hotkey resolution whenever active input field number is set") {
    Fields_Test_Reset reset;

    const int16_t field_idx = fields::Add_Hot_Key("k");
    (void)field_idx;
    fields::active_input_field_number = 5;
    int16_t resolved_field = 0;
    test::fakes::g_read_key_value = 'K';

    DOCTEST_CHECK(fields::Interpret_Keyboard_Input(&resolved_field) == 'K');
    DOCTEST_CHECK(resolved_field == 0);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard hotkey toggles radio selection") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 0;

    const int16_t field_idx = fields::Add_Radio_Button_Field(10, 20, "R", &picture, &selection, "r", 0);
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 'r';
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(selection == 1);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard hotkey sets locked button selection") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 0;

    const int16_t field_idx = fields::Add_Locked_Button_Field(10, 20, "L", &picture, &selection, "l", 0);
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 'l';
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(selection == 1);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard hotkey returns scroll return result") {
    Fields_Test_Reset reset;
    int16_t value = 0;

    const int16_t field_idx = fields::Add_Scroll_Field(10, 20, 0, 100, 9, 22, 20, 6, &value, "s", 0);
    fields::fields[field_idx].field_type = FIELD_TYPE_SCROLL_RETURN;
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 's';
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == 9);
}

DOCTEST_TEST_CASE("interpret mouse input returns escape cancel when keyboard escape is pressed") {
    Fields_Test_Reset reset;

    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x011B;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == -1);
}

DOCTEST_TEST_CASE("interpret mouse input enter confirms active continuous input field") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char buffer[32] = "before";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 40, 10, buffer, 12, 0, &picture, 0, 0);
    fields::input_field_active = 1;
    fields::active_input_field_number = field_idx;
    strlcpy(fields::continuous_string, "after_", sizeof(fields::continuous_string));
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x1C0D;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(std::strcmp(buffer, "after") == 0);
    DOCTEST_CHECK(fields::input_field_active == 0);
    DOCTEST_CHECK(fields::active_input_field_number == -1);
}

DOCTEST_TEST_CASE("interpret mouse input plus key increments scroll field under pointer") {
    Fields_Test_Reset reset;
    int16_t value = 10;

    const int16_t field_idx = fields::Add_Scroll_Field(10, 20, 0, 100, 10, 90, 20, 6, &value, "s", 0);
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x0D2B;
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(value == 15);
}

DOCTEST_TEST_CASE("interpret mouse input right click on help box shows help and returns zero") {
    Fields_Test_Reset reset;
    s_help_box help_boxes[1]{};
    help_boxes[0].x1 = 10;
    help_boxes[0].y1 = 20;
    help_boxes[0].x2 = 30;
    help_boxes[0].y2 = 40;
    help_boxes[0].help_id = 55;

    fields::Set_Help_List(help_boxes, 1);
    test::fakes::g_mouse_pointer_x = 15;
    test::fakes::g_mouse_pointer_y = 25;
    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 2;
    test::fakes::g_mouse_buffer_x = 15;
    test::fakes::g_mouse_buffer_y = 25;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == 0);
    DOCTEST_CHECK(test::fakes::g_last_help_id == 55);
}

DOCTEST_TEST_CASE("interpret mouse input right click returns cancel when no help box matches") {
    Fields_Test_Reset reset;

    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 2;
    test::fakes::g_mouse_buffer_x = 15;
    test::fakes::g_mouse_buffer_y = 25;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == -1);
}

DOCTEST_TEST_CASE("draw field grid action clamps computed coordinates into valid range") {
    Fields_Test_Reset reset;
    int16_t grid_x = 0;
    int16_t grid_y = 0;

    const int16_t field_idx = fields::Add_Grid_Field(10, 10, 5, 4, 3, 4, &grid_x, &grid_y, 0);
    test::fakes::g_mouse_pointer_x = 40;
    test::fakes::g_mouse_pointer_y = 40;

    fields::Draw_Field(field_idx, 1);

    DOCTEST_CHECK(grid_x == 2);
    DOCTEST_CHECK(grid_y == 3);
}

DOCTEST_TEST_CASE("disable and enable draw field roundtrip through encoded hidden types") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 0;

    const int16_t button_idx = fields::Add_Button_Field(10, 10, "B", &picture, "b", 0);
    const int16_t radio_idx = fields::Add_Radio_Button_Field(20, 10, "R", &picture, &selection, "r", 0);
    const int16_t locked_idx = fields::Add_Locked_Button_Field(30, 10, "L", &picture, &selection, "l", 0);
    const int16_t multi_idx = fields::Add_Multi_Button_Field(40, 10, "M", &picture, &selection, 3, "m", 0);

    fields::Disable_Draw_Field(button_idx);
    fields::Disable_Draw_Field(radio_idx);
    fields::Disable_Draw_Field(locked_idx);
    fields::Disable_Draw_Field(multi_idx);

    DOCTEST_CHECK(fields::fields[button_idx].field_type == -1001);
    DOCTEST_CHECK(fields::fields[radio_idx].field_type == -1);
    DOCTEST_CHECK(fields::fields[locked_idx].field_type == -2);
    DOCTEST_CHECK(fields::fields[multi_idx].field_type == -3);

    fields::Enable_Draw_Field(button_idx);
    fields::Enable_Draw_Field(radio_idx);
    fields::Enable_Draw_Field(locked_idx);
    fields::Enable_Draw_Field(multi_idx);

    DOCTEST_CHECK(fields::fields[button_idx].field_type == FIELD_TYPE_BUTTON);
    DOCTEST_CHECK(fields::fields[radio_idx].field_type == FIELD_TYPE_RADIO);
    DOCTEST_CHECK(fields::fields[locked_idx].field_type == FIELD_TYPE_LOCKED);
    DOCTEST_CHECK(fields::fields[multi_idx].field_type == FIELD_TYPE_MULTI_BUTTON);
}

DOCTEST_TEST_CASE("draw visible fields clamps scroll value and renders string list text") {
    Fields_Test_Reset reset;
    int16_t scroll_value = 95;
    int8_t selected_value = 4;

    const int16_t scroll_idx = fields::Add_Scroll_Field(10, 10, 0, 100, 10, 90, 20, 6, &scroll_value, "s", 0);
    const int16_t string_idx = fields::Add_String_List_Field(50, 20, 30, "Row", 1, &selected_value, 4, 17, nullptr, 0, nullptr, 0, "r", "");
    (void)scroll_idx;
    (void)string_idx;

    fields::Draw_Visible_Fields();

    DOCTEST_CHECK(scroll_value == 90);
    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(std::strcmp(test::fakes::g_print_calls.back().text, "Row") == 0);
}

DOCTEST_TEST_CASE("draw visible fields renders selected button-like field variants with offset paths") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};
    int16_t radio_selection = 1;
    int16_t locked_selection = 1;
    int16_t multi_selection = 7;

    test::fakes::g_anim_full_store_flag = 1;

    fields::Add_Button_Field(10, 10, "Btn", &picture, "b", 0);
    fields::Add_Radio_Button_Field(40, 10, "Rad", &picture, &radio_selection, "r", 0);
    fields::Add_Locked_Button_Field(70, 10, "Lock", &picture, &locked_selection, "l", 0);
    fields::Add_Multi_Button_Field(100, 10, "Multi", &picture, &multi_selection, 7, "m", 0);

    fields::Draw_Visible_Fields();

    DOCTEST_CHECK(test::fakes::g_draw_calls.size() == 4);
    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 4);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].text_copy == "Btn");
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[1].text_copy == "Rad");
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[2].text_copy == "Lock");
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[3].text_copy == "Multi");
}

DOCTEST_TEST_CASE("draw visible fields uses pressed-state draw path for button-like fields") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};

    const int16_t field_idx = fields::Add_Button_Field(10, 10, "Btn", &picture, "b", 0);
    fields::down_mouse_button = field_idx;

    fields::Draw_Visible_Fields();

    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 1);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].x == 20);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].y == 10);
}

DOCTEST_TEST_CASE("draw visible fields renders continuous input and appends cursor marker when blink state changes to on") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};
    char buffer[32] = "Alpha";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 60, 16, buffer, 12, 3, &picture, 0, 0);
    fields::input_field_active = 1;
    fields::active_input_field_number = field_idx;
    strlcpy(fields::continuous_string, "Alpha", sizeof(fields::continuous_string));
    fields::cursor_on = 0;
    fields::cursor_position = 2;

    fields::Draw_Visible_Fields();

    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "Alpha_") == 0);
    DOCTEST_REQUIRE(!test::fakes::g_fill_calls.empty());
    DOCTEST_REQUIRE(!test::fakes::g_draw_calls.empty());
    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(test::fakes::g_print_calls.back().text_copy == "Alpha_");
}

DOCTEST_TEST_CASE("draw visible fields removes continuous input cursor marker when blink toggles off") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};
    char buffer[32] = "Alpha";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 60, 16, buffer, 12, 0, &picture, 0, 0);
    fields::input_field_active = 1;
    fields::active_input_field_number = field_idx;
    strlcpy(fields::continuous_string, "Alpha_", sizeof(fields::continuous_string));
    fields::cursor_on = 1;
    fields::cursor_position = 1;

    fields::Draw_Visible_Fields();

    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "Alpha") == 0);
    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(test::fakes::g_print_calls.back().text_copy == "Alpha");
}

DOCTEST_TEST_CASE("draw visible fields draws field and help outlines when overlay modes are enabled") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};
    s_help_box help_boxes[1]{};
    help_boxes[0].x1 = 1;
    help_boxes[0].y1 = 2;
    help_boxes[0].x2 = 3;
    help_boxes[0].y2 = 4;
    help_boxes[0].help_id = 9;

    fields::Add_Button_Field(10, 20, "Btn", &picture, "b", 0);
    fields::Set_Help_List(help_boxes, 1);
    fields::field_box_mode = 1;
    fields::help_box_mode = 1;
    fields::field_outline_color = 12;
    fields::help_field_outline_color = 34;

    fields::Draw_Visible_Fields();

    DOCTEST_REQUIRE(test::fakes::g_line_calls.size() == 8);
    DOCTEST_CHECK(test::fakes::g_line_calls[0].color == 12);
    DOCTEST_CHECK(test::fakes::g_line_calls[4].color == 34);
}

DOCTEST_TEST_CASE("draw visible fields back clamps scroll value and renders string list text") {
    Fields_Test_Reset reset;
    int16_t scroll_value = 5;
    int8_t selected_value = 9;

    const int16_t scroll_idx = fields::Add_Scroll_Field(10, 10, 0, 100, 10, 90, 20, 6, &scroll_value, "s", 0);
    const int16_t string_idx = fields::Add_String_List_Field(50, 20, 30, "BackRow", 0, &selected_value, 4, 0, nullptr, 0, nullptr, 0, "r", "");
    (void)scroll_idx;
    (void)string_idx;

    fields::Draw_Visible_Fields_Back();

    DOCTEST_CHECK(scroll_value == 10);
    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(std::strcmp(test::fakes::g_print_calls.back().text, "BackRow") == 0);
}

DOCTEST_TEST_CASE("draw visible fields back renders selected button-like field variants with offset paths") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};
    int16_t radio_selection = 1;
    int16_t locked_selection = 1;
    int16_t multi_selection = 7;

    test::fakes::g_anim_full_store_flag = 1;

    fields::Add_Button_Field(10, 10, "Btn", &picture, "b", 0);
    fields::Add_Radio_Button_Field(40, 10, "Rad", &picture, &radio_selection, "r", 0);
    fields::Add_Locked_Button_Field(70, 10, "Lock", &picture, &locked_selection, "l", 0);
    fields::Add_Multi_Button_Field(100, 10, "Multi", &picture, &multi_selection, 7, "m", 0);

    fields::Draw_Visible_Fields_Back();

    DOCTEST_CHECK(test::fakes::g_draw_calls.size() == 4);
    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 4);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].text_copy == "Btn");
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[1].text_copy == "Rad");
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[2].text_copy == "Lock");
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[3].text_copy == "Multi");
}

DOCTEST_TEST_CASE("draw visible fields back uses pressed-state draw path for button-like fields") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};

    const int16_t field_idx = fields::Add_Button_Field(10, 10, "Btn", &picture, "b", 0);
    fields::down_mouse_button = field_idx;

    fields::Draw_Visible_Fields_Back();

    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 1);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].x == 20);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].y == 10);
}

DOCTEST_TEST_CASE("draw visible fields back renders active continuous input with cursor marker") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};
    char buffer[32] = "Alpha";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 60, 16, buffer, 12, 3, &picture, 0, 0);
    fields::input_field_active = 1;
    fields::active_input_field_number = field_idx;
    strlcpy(fields::continuous_string, "Alpha", sizeof(fields::continuous_string));
    fields::cursor_on = 0;

    fields::Draw_Visible_Fields_Back();

    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "Alpha_") == 0);
    DOCTEST_REQUIRE(!test::fakes::g_fill_calls.empty());
    DOCTEST_REQUIRE(!test::fakes::g_draw_calls.empty());
    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(test::fakes::g_print_calls.back().text_copy == "Alpha_");
}

DOCTEST_TEST_CASE("draw visible fields back renders inactive continuous input from stored buffer") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};
    char buffer[32] = "Stored";

    fields::Add_Continuous_String_Input_Field(10, 20, 60, 16, buffer, 12, 4, &picture, 0, 0);

    fields::Draw_Visible_Fields_Back();

    DOCTEST_REQUIRE(!test::fakes::g_fill_calls.empty());
    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(test::fakes::g_print_calls.back().text_copy == "Stored");
}

DOCTEST_TEST_CASE("draw visible fields back draws field and help outlines when overlay modes are enabled") {
    Fields_Test_Reset reset;
    Test_Animation_Header picture{};
    s_help_box help_boxes[1]{};
    help_boxes[0].x1 = 1;
    help_boxes[0].y1 = 2;
    help_boxes[0].x2 = 3;
    help_boxes[0].y2 = 4;
    help_boxes[0].help_id = 9;

    fields::Add_Button_Field(10, 20, "Btn", &picture, "b", 0);
    fields::Set_Help_List(help_boxes, 1);
    fields::field_box_mode = 1;
    fields::help_box_mode = 1;
    fields::field_outline_color = 12;
    fields::help_field_outline_color = 34;

    fields::Draw_Visible_Fields_Back();

    DOCTEST_REQUIRE(test::fakes::g_line_calls.size() == 8);
    DOCTEST_CHECK(test::fakes::g_line_calls[0].color == 12);
    DOCTEST_CHECK(test::fakes::g_line_calls[4].color == 34);
}

DOCTEST_TEST_CASE("interpret mouse input held left button toggles radio on release path") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 0;

    const int16_t field_idx = fields::Add_Radio_Button_Field(10, 20, "R", &picture, &selection, "r", 0);
    test::fakes::g_mouse_button_sequence = {1, 1, 0};
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(selection == 1);
}

DOCTEST_TEST_CASE("interpret mouse input held right button returns negative scroll result when cancel disabled") {
    Fields_Test_Reset reset;
    int16_t value = 0;

    const int16_t field_idx = fields::Add_Scroll_Field(10, 20, 0, 100, 9, 22, 20, 6, &value, "s", 0);
    fields::fields[field_idx].field_type = FIELD_TYPE_SCROLL_RETURN;
    fields::mouse_cancel_disabled = 1;
    test::fakes::g_mouse_button_sequence = {2, 2, 0};
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == -9);
}

DOCTEST_TEST_CASE("draw field button action path uses selected text offsets") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    const int16_t field_idx = fields::Add_Button_Field(10, 20, "Btn", &picture, "b", 0);

    fields::Draw_Field(field_idx, 1);

    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 1);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].x == 20);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].y == 20);
}

DOCTEST_TEST_CASE("draw field radio default path uses centered text without selected offset") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 1;

    const int16_t field_idx = fields::Add_Radio_Button_Field(20, 30, "Radio", &picture, &selection, "r", 0);

    fields::Draw_Field(field_idx, 0);

    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 1);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].x == 29);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].y == 31);
}

DOCTEST_TEST_CASE("draw field locked default path uses centered text without selected offset") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 0;

    const int16_t field_idx = fields::Add_Locked_Button_Field(40, 50, "Lock", &picture, &selection, "l", 0);

    fields::Draw_Field(field_idx, 0);

    DOCTEST_REQUIRE(test::fakes::g_print_centered_calls.size() == 1);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].x == 49);
    DOCTEST_CHECK(test::fakes::g_print_centered_calls[0].y == 51);
}

DOCTEST_TEST_CASE("display input field draws underline cursor for input mode zero") {
    Fields_Test_Reset reset;
    char buffer[32] = "ABC";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 50;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    fields::cursor_count = 1;
    fields::cursor_position = 1;

    fields::Display_Input_Field(1, buffer);

    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(test::fakes::g_print_calls.back().text_copy == "ABC");
}

DOCTEST_TEST_CASE("display input field blinks underline cursor for input mode one") {
    Fields_Test_Reset reset;
    char buffer[32] = "ABC";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 50;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 1;
    field.detail.input.cursor_color_ramp = cursor_colors;

    fields::cursor_count = 1;
    fields::cursor_position = 3;
    fields::cursor_on = 0;

    fields::Display_Input_Field(1, buffer);

    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(test::fakes::g_print_calls.back().text_copy == "ABC");
    DOCTEST_CHECK(fields::cursor_on == 1);
}

DOCTEST_TEST_CASE("display input field mode zero draws lower cursor ramp when cursor position exceeds font height") {
    Fields_Test_Reset reset;
    char buffer[32] = "ABC";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 50;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    fields::cursor_count = 3;
    fields::cursor_position = 4;
    test::fakes::g_font_height = 4;

    fields::Display_Input_Field(1, buffer);

    DOCTEST_REQUIRE(test::fakes::g_line_calls.size() == 2);
    DOCTEST_CHECK(test::fakes::g_line_calls.front().y1 == 21);
    DOCTEST_CHECK(test::fakes::g_line_calls.back().y1 == 20);
}

DOCTEST_TEST_CASE("display input field mode zero skips cursor lines when cursor position is zero") {
    Fields_Test_Reset reset;
    char buffer[32] = "ABC";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 50;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    fields::cursor_count = 1;
    fields::cursor_position = 0;

    fields::Display_Input_Field(1, buffer);

    DOCTEST_CHECK(test::fakes::g_line_calls.empty());
    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(test::fakes::g_print_calls.back().text_copy == "ABC");
}

DOCTEST_TEST_CASE("display input field fills background when background color is nonzero") {
    Fields_Test_Reset reset;
    char buffer[32] = "ABC";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 50;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 9;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 1;
    field.detail.input.cursor_color_ramp = cursor_colors;

    fields::cursor_count = 1;
    fields::cursor_position = 1;

    fields::Display_Input_Field(1, buffer);

    DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 1);
    DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 9);
    DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 20);
    DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 50);
    DOCTEST_CHECK(test::fakes::g_fill_calls[0].y2 == 30);
    DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 9);
}

DOCTEST_TEST_CASE("display input field mode one toggles cursor off on next blink boundary") {
    Fields_Test_Reset reset;
    char buffer[32] = "ABC";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 50;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 1;
    field.detail.input.cursor_color_ramp = cursor_colors;

    fields::cursor_count = 1;
    fields::cursor_position = 6;
    fields::cursor_on = 1;

    fields::Display_Input_Field(1, buffer);

    DOCTEST_CHECK(fields::cursor_on == 0);
    DOCTEST_CHECK(test::fakes::g_line_calls.empty());
}

DOCTEST_TEST_CASE("display input field with unsupported input mode still prints text") {
    Fields_Test_Reset reset;
    char buffer[32] = "ABC";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 50;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 2;
    field.detail.input.cursor_color_ramp = cursor_colors;

    fields::Display_Input_Field(1, buffer);

    DOCTEST_CHECK(test::fakes::g_line_calls.empty());
    DOCTEST_REQUIRE(!test::fakes::g_print_calls.empty());
    DOCTEST_CHECK(test::fakes::g_print_calls.back().text_copy == "ABC");
    DOCTEST_CHECK(test::fakes::g_mark_time_calls == 1);
    DOCTEST_REQUIRE(!test::fakes::g_release_time_calls.empty());
    DOCTEST_CHECK(test::fakes::g_release_time_calls.back() == fields::auto_function_delay);
}

DOCTEST_TEST_CASE("input string accepts typed character and commits on enter") {
    Fields_Test_Reset reset;
    char buffer[32] = "AB";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 80;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    test::fakes::g_scan_key_sequence = {'C', 0x1C0D};
    test::fakes::g_string_width = 8;

    fields::Input_String(1);

    DOCTEST_CHECK(std::strcmp(buffer, "ABC") == 0);
    DOCTEST_CHECK(fields::down_mouse_button == -1);
}

DOCTEST_TEST_CASE("input string escape keeps original buffer") {
    Fields_Test_Reset reset;
    char buffer[32] = "AB";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 80;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    test::fakes::g_scan_key_sequence = {0x011B};

    fields::Input_String(1);

    DOCTEST_CHECK(std::strcmp(buffer, "AB") == 0);
}

DOCTEST_TEST_CASE("input string first backspace clears original buffer before typing replacement") {
    Fields_Test_Reset reset;
    char buffer[32] = "AB";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 80;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    test::fakes::g_scan_key_sequence = {0x0E08, 'Z', 0x1C0D};
    test::fakes::g_string_width = 8;

    fields::Input_String(1);

    DOCTEST_CHECK(std::strcmp(buffer, "Z") == 0);
}

DOCTEST_TEST_CASE("input string left arrow and typed character insert within existing text") {
    Fields_Test_Reset reset;
    char buffer[32] = "AB";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 80;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    test::fakes::g_scan_key_sequence = {0x4BE0, 'Z', 0x1C0D};
    test::fakes::g_string_width = 8;

    fields::Input_String(1);

    DOCTEST_CHECK(std::strcmp(buffer, "AZB") == 0);
}

DOCTEST_TEST_CASE("input string delete removes character at cursor position") {
    Fields_Test_Reset reset;
    char buffer[32] = "ABC";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 80;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    test::fakes::g_scan_key_sequence = {0x4BE0, 0x53E0, 0x1C0D};
    test::fakes::g_string_width = 8;

    fields::Input_String(1);

    DOCTEST_CHECK(std::strcmp(buffer, "AB") == 0);
}

DOCTEST_TEST_CASE("input string backspace after moving left removes previous character") {
    Fields_Test_Reset reset;
    char buffer[32] = "ABC";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 80;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    test::fakes::g_scan_key_sequence = {0x4BE0, 0x0E08, 0x1C0D};
    test::fakes::g_string_width = 8;

    fields::Input_String(1);

    DOCTEST_CHECK(std::strcmp(buffer, "AC") == 0);
}

DOCTEST_TEST_CASE("input string respects max length when appending characters") {
    Fields_Test_Reset reset;
    char buffer[32] = "AB";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 80;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 2;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    test::fakes::g_scan_key_sequence = {'C', 0x1C0D};
    test::fakes::g_string_width = 8;

    fields::Input_String(1);

    DOCTEST_CHECK(std::strcmp(buffer, "AB") == 0);
}

DOCTEST_TEST_CASE("input string clears oversized initial buffer when field width is too small") {
    Fields_Test_Reset reset;
    char buffer[32] = "AB";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 12;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    test::fakes::g_scan_key_sequence = {0x1C0D};
    test::fakes::g_string_width = 8;

    fields::Input_String(1);

    DOCTEST_CHECK(std::strcmp(buffer, "") == 0);
}

DOCTEST_TEST_CASE("input string exits on pending mouse input and waits for mouse release") {
    Fields_Test_Reset reset;
    char buffer[32] = "AB";
    char cursor_colors[] = "12345678";

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    field.x = 10;
    field.y = 20;
    field.x_end = 80;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    test::fakes::g_scan_key_sequence = {0};
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_button_sequence = {1, 0};

    fields::Input_String(1);

    DOCTEST_CHECK(std::strcmp(buffer, "AB") == 0);
    DOCTEST_CHECK(fields::down_mouse_button == -1);
}

DOCTEST_TEST_CASE("get list field updates selection through scan input and exits cleanly") {
    Fields_Test_Reset reset;

    struct Row {
        char text[8];
    } rows[3] = {{"One"}, {"Two"}, {""}};

    int8_t selected_index = 0;
    fields::list_field_scan_flag = 1;
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 43;
    fields::Assign_Auto_Function(fields::Exit_List_Field, 1);

    const int16_t result = fields::Get_List_Field(10, 10, 40, "Help", reinterpret_cast<const char*>(rows), sizeof(Row), &selected_index, nullptr, 0, nullptr, 0, nullptr, 0, "");

    DOCTEST_CHECK(result == -1);
    DOCTEST_CHECK(selected_index == 0);
    DOCTEST_CHECK(fields::list_field_on == 0);
}

DOCTEST_TEST_CASE("get list field returns cancel on negative input") {
    Fields_Test_Reset reset;

    struct Row {
        char text[8];
    } rows[2] = {{"One"}, {""}};

    int8_t selected_index = 0;
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x011B;

    const int16_t result = fields::Get_List_Field(10, 10, 40, "Help", reinterpret_cast<const char*>(rows), sizeof(Row), &selected_index, nullptr, 0, nullptr, 0, nullptr, 0, "");

    DOCTEST_CHECK(result == -1);
}

DOCTEST_TEST_CASE("get list field ignores help row input and can still be canceled") {
    Fields_Test_Reset reset;

    struct Row {
        char text[8];
    } rows[2] = {{"One"}, {""}};

    int8_t selected_index = 0;
    g_test_auto_script_mode = 3;
    g_test_auto_script_button = 1;
    g_test_auto_script_x = 12;
    g_test_auto_script_y = 9;
    fields::Assign_Auto_Function(Test_Auto_Function_List_Script_, 1);

    const int16_t result = fields::Get_List_Field(10, 10, 40, "Help", reinterpret_cast<const char*>(rows), sizeof(Row), &selected_index, nullptr, 0, nullptr, 0, nullptr, 0, "");

    DOCTEST_CHECK(result == -1);
    DOCTEST_CHECK(selected_index == 1);
    DOCTEST_CHECK(fields::list_field_on == 0);
}

DOCTEST_TEST_CASE("get list field returns clicked selectable row index") {
    Fields_Test_Reset reset;

    struct Row {
        char text[8];
    } rows[3] = {{"One"}, {"Two"}, {""}};

    int8_t selected_index = 0;
    g_test_auto_script_mode = 2;
    g_test_auto_script_button = 1;
    g_test_auto_script_x = 12;
    g_test_auto_script_y = 35;
    fields::Assign_Auto_Function(Test_Auto_Function_List_Script_, 1);

    const int16_t result = fields::Get_List_Field(10, 10, 40, "Help", reinterpret_cast<const char*>(rows), sizeof(Row), &selected_index, nullptr, 0, nullptr, 0, nullptr, 0, "");

    DOCTEST_CHECK(result == 1);
    DOCTEST_CHECK(selected_index == 1);
    DOCTEST_CHECK(fields::list_field_on == 0);
}

DOCTEST_TEST_CASE("get list field ignores click on disabled row and exits later") {
    Fields_Test_Reset reset;

    struct Row {
        char text[8];
    } rows[2] = {{"One"}, {""}};

    uint8_t item_flags[1] = {0};
    int8_t selected_index = 0;
    g_test_auto_script_mode = 3;
    g_test_auto_script_button = 1;
    g_test_auto_script_x = 12;
    g_test_auto_script_y = 25;
    fields::Assign_Auto_Function(Test_Auto_Function_List_Script_, 1);

    const int16_t result = fields::Get_List_Field(10, 10, 40, "Help", reinterpret_cast<const char*>(rows), sizeof(Row), &selected_index, item_flags, 0, nullptr, 0, nullptr, 0, "");

    DOCTEST_CHECK(result == -1);
    DOCTEST_CHECK(selected_index == 0);
    DOCTEST_CHECK(fields::list_field_on == 0);
}

DOCTEST_TEST_CASE("get list field scan updates selected row when enabled") {
    Fields_Test_Reset reset;

    struct Row {
        char text[8];
    } rows[3] = {{"One"}, {"Two"}, {""}};

    int8_t selected_index = 0;
    fields::list_field_scan_flag = 1;
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 35;
    g_test_auto_script_mode = 1;
    fields::Assign_Auto_Function(Test_Auto_Function_List_Script_, 1);

    const int16_t result = fields::Get_List_Field(10, 10, 40, "Help", reinterpret_cast<const char*>(rows), sizeof(Row), &selected_index, nullptr, 0, nullptr, 0, nullptr, 0, "");

    DOCTEST_CHECK(result == -1);
    DOCTEST_CHECK(selected_index == 1);
    DOCTEST_CHECK(fields::list_field_scan_flag == 1);
}

DOCTEST_TEST_CASE("get list field scan does not update selected row when disabled") {
    Fields_Test_Reset reset;

    struct Row {
        char text[8];
    } rows[3] = {{"One"}, {"Two"}, {""}};

    int8_t selected_index = 0;
    fields::list_field_scan_flag = 0;
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 35;
    g_test_auto_script_mode = 1;
    fields::Assign_Auto_Function(Test_Auto_Function_List_Script_, 1);

    const int16_t result = fields::Get_List_Field(10, 10, 40, "Help", reinterpret_cast<const char*>(rows), sizeof(Row), &selected_index, nullptr, 0, nullptr, 0, nullptr, 0, "");

    DOCTEST_CHECK(result == -1);
    DOCTEST_CHECK(selected_index == 0);
    DOCTEST_CHECK(fields::list_field_scan_flag == 1);
}

DOCTEST_TEST_CASE("get list field sets selection to minus one when no rows are selectable") {
    Fields_Test_Reset reset;

    struct Row {
        char text[8];
    } rows[3] = {{"One"}, {"Two"}, {""}};

    uint8_t item_flags[2] = {0, 0};
    int8_t selected_index = 1;
    g_test_auto_script_mode = 1;
    fields::Assign_Auto_Function(Test_Auto_Function_List_Script_, 1);

    const int16_t result = fields::Get_List_Field(10, 10, 40, "Help", reinterpret_cast<const char*>(rows), sizeof(Row), &selected_index, item_flags, 0, nullptr, 0, nullptr, 0, "");

    DOCTEST_CHECK(result == -1);
    DOCTEST_CHECK(selected_index == 1);
    DOCTEST_CHECK(fields::list_field_on == 0);
}

DOCTEST_TEST_CASE("draw field scroll action updates bar position from pointer geometry") {
    Fields_Test_Reset reset;
    int16_t value = 0;

    const int16_t field_idx = fields::Add_Scroll_Field(100, 50, 0, 100, 20, 80, 40, 8, &value, "h", 0);
    test::fakes::g_mouse_pointer_x = 132;
    test::fakes::g_mouse_pointer_y = 52;

    fields::Draw_Field(field_idx, 1);

    DOCTEST_CHECK(value == 80);
}

DOCTEST_TEST_CASE("draw field scroll default path leaves value unchanged") {
    Fields_Test_Reset reset;
    int16_t value = 37;

    const int16_t field_idx = fields::Add_Scroll_Field(100, 50, 0, 100, 20, 80, 40, 8, &value, "h", 0);

    fields::Draw_Field(field_idx, 0);

    DOCTEST_CHECK(value == 37);
}

DOCTEST_TEST_CASE("draw field legacy input delegates to display input field") {
    Fields_Test_Reset reset;

    fields::fields_count = 2;
    s_field& field = fields::fields[1];
    std::memset(&field, 0, sizeof(field));
    char buffer[32] = "Text";
    char cursor_colors[] = "12345678";

    field.x = 10;
    field.y = 20;
    field.x_end = 50;
    field.y_end = 30;
    field.field_type = FIELD_TYPE_LEGACY_INPUT;
    field.font_style = 1;
    field.font_color = &test::fakes::g_font_color;
    field.detail.input.buffer = buffer;
    field.detail.input.max_length = 16;
    field.detail.input.background_color = 0;
    field.detail.input.draw_style = 0;
    field.detail.input.input_mode = 0;
    field.detail.input.cursor_color_ramp = cursor_colors;

    fields::Draw_Field(1, 0);

    DOCTEST_CHECK(test::fakes::g_print_calls.empty());
}

DOCTEST_TEST_CASE("interpret mouse input backtick toggles field box mode") {
    Fields_Test_Reset reset;

    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x00C0;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == 0);
    DOCTEST_CHECK(fields::field_box_mode == 1);
}

DOCTEST_TEST_CASE("interpret mouse input tilded help shortcut toggles help box mode") {
    Fields_Test_Reset reset;

    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x297E;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == 0);
    DOCTEST_CHECK(fields::help_box_mode == 1);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard hotkey returns hidden field without auto function") {
    Fields_Test_Reset reset;

    fields::Assign_Auto_Function(Test_Auto_Function_Increment_, 1);
    const int16_t field_idx = fields::Add_Hot_Key("h");
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 'h';

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(g_test_auto_function_calls == 0);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard hotkey returns button field and runs auto function") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    fields::Assign_Auto_Function(Test_Auto_Function_Increment_, 1);
    const int16_t field_idx = fields::Add_Button_Field(10, 20, "Go", &picture, "g", 0);
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 'g';
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(g_test_auto_function_calls == 1);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard hotkey toggles radio selection off when already selected") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 1;

    const int16_t field_idx = fields::Add_Radio_Button_Field(10, 20, "R", &picture, &selection, "r", 0);
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 'r';
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(selection == 0);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard hotkey ignores plain scroll fields") {
    Fields_Test_Reset reset;
    int16_t value = 30;

    fields::Add_Scroll_Field(10, 20, 0, 100, 9, 22, 20, 6, &value, "s", 0);
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 's';
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == 0);
    DOCTEST_CHECK(value == 30);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard enter on selected radio toggles it and returns field") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    int16_t selection = 1;

    const int16_t field_idx = fields::Add_Radio_Button_Field(10, 20, "R", &picture, &selection, "r", 0);
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x1C0D;
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(selection == 0);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard enter returns selected list row when list mode is active") {
    Fields_Test_Reset reset;
    int8_t selected_value = 5;

    fields::list_field_on = 1;
    const int16_t field_idx = fields::Add_String_List_Field(10, 20, 30, "Row", 1, &selected_value, 5, 0, nullptr, 0, nullptr, 0, "s", "");
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x1C0D;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard plus and minus adjust scroll under pointer") {
    Fields_Test_Reset reset;
    int16_t value = 15;

    const int16_t field_idx = fields::Add_Scroll_Field(10, 20, 0, 100, 10, 90, 20, 6, &value, "s", 0);
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x0D2B;
    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(value == 20);

    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x0C2D;
    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(value == 15);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard escape returns cancel before continuous input editing branch") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char buffer[32] = "stored";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 40, 10, buffer, 12, 0, &picture, 0, 0);
    fields::input_field_active = 1;
    fields::active_input_field_number = field_idx;
    strlcpy(fields::continuous_string, "editing_", sizeof(fields::continuous_string));
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x011B;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == -field_idx);
    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "editing_") == 0);
    DOCTEST_CHECK(fields::input_field_active == 1);
    DOCTEST_CHECK(fields::active_input_field_number == field_idx);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard delete clears active continuous input field") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char buffer[32] = "stored";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 40, 10, buffer, 12, 0, &picture, 0, 0);
    fields::input_field_active = 1;
    fields::active_input_field_number = field_idx;
    fields::active_input_field_first = 1;
    strlcpy(fields::continuous_string, "stored", sizeof(fields::continuous_string));
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x0E7F;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == 0);
    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "") == 0);
    DOCTEST_CHECK(fields::active_input_field_first == 0);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard backspace truncates active continuous input field") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char buffer[32] = "stored";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 40, 10, buffer, 12, 0, &picture, 0, 0);
    fields::input_field_active = 1;
    fields::active_input_field_number = field_idx;
    fields::active_input_field_first = 0;
    strlcpy(fields::continuous_string, "AB_", sizeof(fields::continuous_string));
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 0x0E08;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == 0);
    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "A") == 0);
}

DOCTEST_TEST_CASE("interpret mouse input keyboard printable character appends to active continuous input field") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char buffer[32] = "stored";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 60, 10, buffer, 12, 0, &picture, 0, 0);
    fields::input_field_active = 1;
    fields::active_input_field_number = field_idx;
    fields::active_input_field_first = 1;
    strlcpy(fields::continuous_string, "AB", sizeof(fields::continuous_string));
    test::fakes::g_keyboard_status = 1;
    test::fakes::g_read_key_value = 'C';
    test::fakes::g_string_width = 1;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == 0);
    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "ABC") == 0);
    DOCTEST_CHECK(fields::active_input_field_first == 0);
}

DOCTEST_TEST_CASE("interpret mouse input buffered click activates continuous input field") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char buffer[32] = "captain";

    const int16_t field_idx = fields::Add_Continuous_String_Input_Field(10, 20, 40, 10, buffer, 12, 0, &picture, 0, 0);
    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 1;
    test::fakes::g_mouse_buffer_x = 12;
    test::fakes::g_mouse_buffer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(fields::input_field_active == 1);
    DOCTEST_CHECK(fields::active_input_field_number == field_idx);
    DOCTEST_CHECK(std::strcmp(fields::continuous_string, "captain") == 0);
}

DOCTEST_TEST_CASE("interpret mouse input buffered click on non-input commits active continuous input field") {
    Fields_Test_Reset reset;
    s_animation_header picture{};
    char input_buffer[32] = "old";

    const int16_t input_idx = fields::Add_Continuous_String_Input_Field(10, 20, 40, 10, input_buffer, 12, 0, &picture, 0, 0);
    const int16_t button_idx = fields::Add_Button_Field(80, 20, "Go", &picture, "g", 0);
    fields::input_field_active = 1;
    fields::active_input_field_number = input_idx;
    strlcpy(fields::continuous_string, "new_", sizeof(fields::continuous_string));
    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 1;
    test::fakes::g_mouse_buffer_x = 82;
    test::fakes::g_mouse_buffer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == button_idx);
    DOCTEST_CHECK(std::strcmp(input_buffer, "new") == 0);
    DOCTEST_CHECK(fields::input_field_active == 0);
    DOCTEST_CHECK(fields::active_input_field_number == -1);
}

DOCTEST_TEST_CASE("interpret mouse input buffered right click on field returns negative field index") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    const int16_t field_idx = fields::Add_Button_Field(10, 20, "Go", &picture, "g", 0);
    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 2;
    test::fakes::g_mouse_buffer_x = 12;
    test::fakes::g_mouse_buffer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == -field_idx);
}

DOCTEST_TEST_CASE("interpret mouse input buffered right click on scroll return yields cancel when cancel enabled") {
    Fields_Test_Reset reset;
    int16_t value = 0;

    const int16_t field_idx = fields::Add_Scroll_Field(10, 20, 0, 100, 9, 22, 20, 6, &value, "s", 0);
    fields::fields[field_idx].field_type = FIELD_TYPE_SCROLL_RETURN;
    test::fakes::g_mouse_button = 0;
    test::fakes::g_mouse_buffer_flag = 1;
    test::fakes::g_mouse_buffer_flag2 = 1;
    test::fakes::g_mouse_buffer_button = 2;
    test::fakes::g_mouse_buffer_x = 12;
    test::fakes::g_mouse_buffer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == -1);
}

DOCTEST_TEST_CASE("interpret mouse input held left button over button returns selected field index") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    const int16_t field_idx = fields::Add_Button_Field(10, 20, "Go", &picture, "g", 0);
    test::fakes::g_mouse_button_sequence = {1, 1, 0};
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == field_idx);
    DOCTEST_CHECK(fields::last_button_number == 1);
    DOCTEST_CHECK(fields::last_button_x == 12);
    DOCTEST_CHECK(fields::last_button_y == 22);
}

DOCTEST_TEST_CASE("interpret mouse input held right button over button returns negative selected field index") {
    Fields_Test_Reset reset;
    s_animation_header picture{};

    const int16_t field_idx = fields::Add_Button_Field(10, 20, "Go", &picture, "g", 0);
    test::fakes::g_mouse_button_sequence = {2, 2, 0};
    test::fakes::g_mouse_pointer_x = 12;
    test::fakes::g_mouse_pointer_y = 22;

    DOCTEST_CHECK(fields::Interpret_Mouse_Input() == -field_idx);
}

DOCTEST_TEST_CASE("get input throws after exit is requested") {
    Fields_Test_Reset reset;

    platform::Request_Exit();

    DOCTEST_CHECK_THROWS_AS(fields::Get_Input(), platform::process_exit_requested);
}

DOCTEST_TEST_CASE("fatal exit with message defers dialog display to the main-thread shutdown path") {
    Fields_Test_Reset reset;
    const std::string message = std::string(512, 'F') + " fatal diagnostic suffix";

    DOCTEST_CHECK_THROWS_AS(exitm::Exit_With_Message(message.c_str()), platform::process_exit_requested);
    DOCTEST_CHECK(platform::Exit_Requested());
    DOCTEST_CHECK(std::strcmp(platform::Exit_Message(), message.c_str()) == 0);
    DOCTEST_CHECK(platform::Exit_Status() == EXIT_FAILURE);
    DOCTEST_CHECK(test::fakes::g_last_message_box_text.empty());

    exitm::Present_Exit_Message(nullptr);

    DOCTEST_CHECK(test::fakes::g_last_message_box_text == message);
    DOCTEST_CHECK(test::fakes::g_last_message_box_is_error);
}

DOCTEST_TEST_CASE("normal exit preserves successful status and defers its message") {
    Fields_Test_Reset reset;
    const std::string message = std::string(512, 'N') + " normal diagnostic suffix";

    DOCTEST_CHECK_THROWS_AS(exitm::Exit_Normally(message.c_str()), platform::process_exit_requested);
    DOCTEST_CHECK(platform::Exit_Requested());
    DOCTEST_CHECK(std::strcmp(platform::Exit_Message(), message.c_str()) == 0);
    DOCTEST_CHECK(platform::Exit_Status() == EXIT_SUCCESS);
    DOCTEST_CHECK(test::fakes::g_last_message_box_text.empty());

    exitm::Present_Exit_Message(nullptr);

    DOCTEST_CHECK(test::fakes::g_last_message_box_text == message);
    DOCTEST_CHECK_FALSE(test::fakes::g_last_message_box_is_error);
}

DOCTEST_TEST_CASE("normal exit without a message remains noninteractive") {
    Fields_Test_Reset reset;

    platform::Request_Exit();
    exitm::Present_Exit_Message(nullptr);

    DOCTEST_CHECK(platform::Exit_Status() == EXIT_SUCCESS);
    DOCTEST_CHECK(test::fakes::g_last_message_box_text.empty());
}

}

#undef s_animation_header