#include "pch.h"

namespace fields {
    namespace {
        s_field g_fields_storage[MAX_FIELDS_COUNT];
        constexpr std::uintptr_t kInvalidStringMarker = static_cast<std::uintptr_t>(-1);

        char Shortcut_Character(const char* shortcut) {
            if (shortcut == nullptr || shortcut[0] == '\0') {
                return '\0';
            }

            char hotkey = shortcut[0];
            if (shortcut[1] == '\0' && hotkey >= 'A' && hotkey <= 'Z') {
                hotkey += 'a' - 'A';
            }
            return hotkey;
        }
    } // namespace

    MOX2::action_func_t update_function;
    uint16_t auto_active_flag;
    uint16_t list_field_on;
    int16_t auto_function_delay = 2;
    uint16_t list_field_scan_flag = 1;
    int16_t down_mouse_button = -1;
    int16_t auto_input_variable;
    uint16_t input_field_active;
    int16_t active_input_field_number = -1;
    int16_t cursor_count;
    int16_t cursor_position;
    int16_t cursor_on;
    int16_t help_list_active;
    int16_t help_list_count;
    int16_t multi_hotkey_active_field;
    uint16_t draw_fields_flag = 1;
    char s__00551100[1] = "";
    char* invalid_field_string_ptr = reinterpret_cast<char*>(kInvalidStringMarker);
    int32_t max_fields_count = MAX_FIELDS_COUNT;
    uint8_t exit_the_list_field;
    s_field* fields = g_fields_storage;
    s_field* old_fields = g_fields_storage;
    int16_t pointer_offset;
    uint16_t old_input_field_active;
    int16_t old_active_input_field_number;
    int16_t old_down_mouse_button;
    int16_t old_fields_count;
    int16_t old_input_delay;
    int16_t old_auto_input_variable;
    int16_t fields_count;
    int16_t selected_count;
    int16_t mouse_saved_center_y;
    int16_t mouse_saved_center_x;
    uint16_t pressed_mouse_x_value;
    uint16_t pressed_mouse_y_value;
    int16_t pressed_button_flag;
    int16_t last_button_number;
    int16_t last_button_x;
    int16_t last_button_y;
    s_colors list_normal_color;
    s_colors list_highlight_colors;
    s_colors list_special_colors;
    int16_t input_delay;
    MOX2::action_func_t function;
    MOX2::action_func_t function2;
    s_help_box* help_struct_pointer;
    char continuous_string[256];
    uint8_t active_input_field_first;
    uint16_t field_outline_color = 14;
    uint16_t help_field_outline_color = 180;
    int16_t selected_button_text_x_offset = 1;
    int16_t selected_button_text_y_offset = -1;
    uint16_t mouse_cancel_disabled;
    uint16_t dynamic_field_refresh = 1;
    uint16_t field_box_mode;
    uint16_t help_box_mode;
    int16_t mouse_auto_exit;
    uint32_t draw_override_flag;

    void __cdecl Assign_Update_Function(MOX2::action_func_t fn) {
        update_function = fn;
    }

    void __cdecl Assign_Auto_Function(MOX2::action_func_t fn, int16_t delay) {
        auto_active_flag = 1;
        function = fn;

        if (delay > 0 && delay < 10) {
            auto_function_delay = delay;
        } else {
            auto_function_delay = 2;
        }
    }

    void __cdecl Deactivate_Auto_Function() {
        auto_active_flag = 0;
    }

    void __cdecl Activate_Auto_Function() {
        auto_active_flag = 1;
    }

    int16_t __cdecl Auto_Input() {
        return auto_input_variable;
    }

    void __cdecl Set_Help_List(s_help_box* help_data, int16_t count) {
        help_list_count = count;
        help_struct_pointer = help_data;
        help_list_active = 1;
    }

    void __cdecl Disable_Draw_All_Fields() {
        draw_fields_flag = 0;
    }

    void __cdecl Enable_Draw_All_Fields() {
        draw_fields_flag = 1;
    }

    void __cdecl Disable_Cancel() {
        mouse_cancel_disabled = 1;
    }

    void __cdecl Deactivate_Help_List() {
        help_list_active = 0;
        help_struct_pointer = nullptr;
        help_list_count = 0;
    }

    void __cdecl Invoke_Auto_Function() {
        platform::Check_Exit();

        if (auto_active_flag != 0) {
            function();
        }
    }

    void __cdecl Invoke_Saved_Auto_Function() {
        function2();
    }

    void __cdecl Save_Auto_Function() {
        function2 = function;
    }

    void __cdecl Restore_Auto_Function() {
        auto_active_flag = 1;
        function = function2;
    }

    void __cdecl Set_Input_Delay(int16_t delay) {
        input_delay = delay;
        mouse::Mouse_Buffer();
        mouse::Mouse_Buffer2();
    }

    void __cdecl Set_Field_Outline_Color(uint16_t color) {
        field_outline_color = color;
    }

    void __cdecl Clear_Mouse_Buffer() {
        mouse::Mouse_Buffer();
        mouse::Mouse_Buffer2();
    }

    int16_t __cdecl Get_Input() {
        platform::Check_Exit();

        if (input_delay > 0) {
            input_delay--;
            return 0;
        }

        if (input_delay < 0) {
            input_delay = 0;
        }

        if (fields_count < 2) {
            return 0;
        }

        int16_t input_result = Interpret_Mouse_Input();
        video::Set_Page_Off();

        if (input_result != 0) {
            int16_t field_idx = (input_result < 0) ? -input_result : input_result;
            if (field_idx > 0 && field_idx < fields_count) {
                ERICNET::Mox_Play_Sound(fields[field_idx].sound);
            }
        }

        return input_result;
    }

    void __cdecl Clear_Fields() {
        fields_count = 1;
        input_delay = 0;
        down_mouse_button = -1;
        auto_input_variable = 0;
        input_field_active = 0;
        active_input_field_number = -1;
        multi_hotkey_active_field = 0;

        mouse::Mouse_Buffer();
        mouse::Mouse_Buffer2();
    }

    void __cdecl Clear_Fields_Above(int16_t field_idx) {
        down_mouse_button = -1;
        fields_count = field_idx + 1;
        mouse::Mouse_Buffer();
        mouse::Mouse_Buffer2();
    }

    int16_t __cdecl Get_Field_Counter() {
        return fields_count;
    }

    void __cdecl Enable_Mouse_Pressed_Field_Update() {
        dynamic_field_refresh = 1;
        mouse::Mouse_Buffer();
        mouse::Mouse_Buffer2();
    }

    void __cdecl Disable_Mouse_Pressed_Field_Update() {
        dynamic_field_refresh = 0;
    }

    int16_t __cdecl Add_Hot_Key(const char* hotkey_str) {
        if ((int32_t)fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        s_field& field = fields[fields_count];

        field.x = 5000;
        field.y = 5000;
        field.x_end = 5000;
        field.y_end = 5000;
        field.field_type = FIELD_TYPE_HIDDEN;

        field.hotkey = Shortcut_Character(hotkey_str);

        int16_t result_index = fields_count;
        fields_count++;
        return result_index;
    }

    void __cdecl Quick_Call_Auto_Function() {
        platform::Check_Exit();

        if (auto_active_flag == 0) {
            platform::Toggle_Pages_Stub();
            return;
        }

        timer::Mark_Time();
        video::Set_Page_Off();
        Invoke_Auto_Function();
        platform::Toggle_Pages_Stub();
        timer::Release_Time(1);

        if (update_function != nullptr) {
            update_function();
        }
    }

    void __cdecl Call_Auto_Function() {
        platform::Check_Exit();

        if (auto_active_flag == 0) {
            platform::Toggle_Pages_Stub();
            return;
        }

        timer::Mark_Time();
        video::Set_Page_Off();
        Invoke_Auto_Function();
        platform::Toggle_Pages_Stub();
        timer::Release_Time(auto_function_delay);

        if (update_function != nullptr) {
            update_function();
        }
    }

    int16_t __cdecl Add_Hidden_Field(int16_t x, int16_t y, int16_t x_end, int16_t y_end, const char* hotkey, int8_t sound) {
        if (fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        const int16_t field_index = fields_count;
        s_field* const field = &fields[field_index];

        field->x = x;
        field->y = y;
        field->x_end = x_end;
        field->y_end = y_end;
        field->field_type = FIELD_TYPE_HIDDEN;
        field->sound = sound;
        field->hotkey = Shortcut_Character(hotkey);

        ++fields_count;
        return field_index;
    }

    int16_t __cdecl Add_Multi_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, int16_t* value, int16_t button_value, const char* hotkey, int8_t sound) {
        if ((int32_t)fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        s_field* field = &fields[fields_count];

        struct_::Clear_Structure(field, sizeof(*field));

        field->x = x;
        field->y = y;
        field->x_end = field->x + animate::Get_Width(pic) - 1;
        field->y_end = field->y + animate::Get_Height(pic) - 1;
        field->font_style = fonts::Get_Current_Font_Style();
        field->font_color = fonts::Get_Current_Font_Color();
        field->sound = sound;
        field->hotkey = Shortcut_Character(hotkey);

        field->field_type = FIELD_TYPE_MULTI_BUTTON;
        s_field_multi_button* multi_button = Field_Data_Multi_Button(field);
        multi_button->help = help;
        multi_button->pic = pic;
        multi_button->draw_style = animate::Get_Full_Store_Flag(pic);
        multi_button->selection = value;
        multi_button->button_value = button_value;

        int16_t current_index = fields_count;
        fields_count++;

        return current_index;
    }

    int16_t __cdecl Add_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, const char* hotkey, int8_t sound) {
        if (fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        s_field* field = &fields[fields_count];

        struct_::Clear_Structure(field, sizeof(*field));

        field->x = x;
        field->y = y;
        field->x_end = field->x + animate::Get_Width(pic) - 1;
        field->y_end = field->y + animate::Get_Height(pic) - 1;
        field->field_type = FIELD_TYPE_BUTTON;
        field->font_style = fonts::Get_Current_Font_Style();
        field->font_color = fonts::Get_Current_Font_Color();
        field->sound = sound;
        field->hotkey = Shortcut_Character(hotkey);
        s_field_button* button = Field_Data_Button(field);
        button->help = help;
        button->pic = pic;
        button->draw_style = animate::Get_Full_Store_Flag(pic);

        int16_t current_id = fields_count;
        fields_count++;

        return current_id;
    }

    int16_t __cdecl Add_Radio_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, int16_t* variable, const char* hotkey, int8_t sound) {
        if ((int32_t)fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        int16_t current_field = fields_count;
        s_field* field = &fields[current_field];

        struct_::Clear_Structure(field, sizeof(*field));

        field->x = x;
        field->y = y;
        field->x_end = field->x + animate::Get_Width(pic) - 1;
        field->y_end = field->y + animate::Get_Height(pic) - 1;
        field->font_style = fonts::Get_Current_Font_Style();
        field->font_color = fonts::Get_Current_Font_Color();
        field->sound = sound;
        field->hotkey = Shortcut_Character(hotkey);

        fields[current_field].field_type = FIELD_TYPE_RADIO;
        s_field_radio* radio = Field_Data_Radio(&fields[current_field]);
        radio->help = help;
        radio->pic = pic;
        radio->draw_style = animate::Get_Full_Store_Flag(pic);
        radio->selection = variable;

        fields_count++;

        return current_field;
    }

    int16_t __cdecl Add_Clipped_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, const char* hotkey, int8_t sound) {
        if (animate::clipping_flag == 1) {
            int16_t width = animate::Get_Width(pic);
            int16_t height = animate::Get_Height(pic);

            if (x < video::screen_window_x1 ||
                (x + width) > video::screen_window_x2 ||
                y < video::screen_window_y1 ||
                (y + height) > video::screen_window_y2) {
                return -1000;
            }
        }

        if (fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        s_field* field = &fields[fields_count];

        struct_::Clear_Structure(field, sizeof(*field));

        field->x = x;
        field->y = y;
        field->x_end = field->x + animate::Get_Width(pic) - 1;
        field->y_end = field->y + animate::Get_Height(pic) - 1;
        field->field_type = FIELD_TYPE_BUTTON;
        field->font_style = fonts::Get_Current_Font_Style();
        field->font_color = fonts::Get_Current_Font_Color();
        field->sound = sound;
        field->hotkey = Shortcut_Character(hotkey);
        s_field_button* button = Field_Data_Button(field);
        button->help = help;
        button->pic = pic;
        button->draw_style = animate::Get_Full_Store_Flag(pic);

        int16_t current_index = fields_count;
        fields_count++;

        return current_index;
    }

    int16_t __cdecl Add_Irregular_Button_Field(int16_t x, int16_t y, int16_t x_end, int16_t y_end, const char* help, s_animation_header* pic, int16_t draw_x, int16_t draw_y, const char* hotkey, int8_t sound) {
        char buffer[80];

        if (max_fields_count < fields_count) {
            snprintf(buffer, sizeof(buffer), "Maximum number of button fields exceeded (%d)", (int32_t)max_fields_count);
            exitm::Exit_With_Message(buffer);
        }

        s_field& field = fields[fields_count];

        struct_::Clear_Structure(&field, sizeof(field));

        field.x = x;
        field.y = y;
        field.x_end = x_end;
        field.y_end = y_end;

        field.font_style = fonts::Get_Current_Font_Style();
        field.font_color = fonts::Get_Current_Font_Color();

        field.sound = (int16_t)sound;
        field.field_type = FIELD_TYPE_IRREGULAR_BUTTON;
        s_field_irregular_button* irregular_button = Field_Data_Irregular_Button(&field);
        irregular_button->help = help;
        irregular_button->pic = pic;
        irregular_button->draw_style = animate::Get_Full_Store_Flag(pic);

        field.hotkey = Shortcut_Character(hotkey);

        irregular_button->draw_x = draw_x;
        irregular_button->draw_y = draw_y;

        int16_t current_index = fields_count;
        fields_count++;

        return current_index;
    }

    int16_t __cdecl Add_Grid_Field(int16_t x, int16_t y, int16_t step_x, int16_t step_y, int16_t cols, int16_t rows, int16_t* grid_x, int16_t* grid_y, int8_t sound) {
        if (fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        s_field& field = fields[fields_count];

        field.x = x;
        field.y = y;
        field.x_end = x + (cols * step_x) - 1;
        field.y_end = y + (rows * step_y) - 1;
        field.sound = static_cast<int16_t>(sound);
        field.field_type = FIELD_TYPE_GRID;

        s_field_grid* grid = Field_Data_Grid(&field);
        grid->step_x = step_x;
        grid->step_y = step_y;
        grid->grid_x_ptr = grid_x;
        grid->grid_y_ptr = grid_y;

        field.hotkey = '\0';

        int16_t current_index = fields_count;
        fields_count++;

        return current_index;
    }

    int16_t __cdecl Add_Continuous_String_Input_Field(
        int16_t x, int16_t y, int16_t width, int16_t height, char* value, int16_t max_length, int16_t background_color, s_animation_header* pic, int8_t sound, int16_t font_shadow_style) {
        if (fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        s_colors* norm_color = fonts::Get_Current_Font_Color();
        s_colors* high_color = fonts::Get_Current_Font_Color();

        for (int i = 0; i < 8; ++i) {
            list_normal_color.colors[i] = norm_color->colors[i];
            list_highlight_colors.colors[i] = high_color->colors[i];
        }

        int16_t current = fields_count;

        fields[current].x = x;
        fields[current].y = y;
        fields[current].font_style = fonts::Get_Current_Font_Style();
        fields[current].font_color = &list_normal_color;
        fields[current].field_type = FIELD_TYPE_CONTINUOUS_INPUT;
        s_field_cont_input* cont_input = Field_Data_Continuous_Input(&fields[current]);
        cont_input->highlight_color = &list_highlight_colors;

        fields[current].x_end = x + width;
        fields[current].y_end = y + height;

        if (max_length > (int16_t)sizeof(continuous_string)) {
            max_length = (int16_t)sizeof(continuous_string);
        }

        cont_input->max_length = max_length;
        fields[current].hotkey = 0;
        fields[current].sound = sound;
        cont_input->buffer = value;
        cont_input->full_store_flag = 0;
        cont_input->pic = pic;
        cont_input->background_color = background_color;
        cont_input->font_shadow_style = font_shadow_style;

        fields_count++;

        return current;
    }

    int16_t __cdecl Add_Scroll_Field(int16_t x, int16_t y, int16_t range_min, int16_t range_max, int16_t return_value, int16_t set_value, int16_t width, int16_t height, int16_t* value, const char* hotkey_str, int8_t sound) {
        if ((int32_t)fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        fields[fields_count].x = x;
        fields[fields_count].y = y;
        fields[fields_count].x_end = fields[fields_count].x + width;
        fields[fields_count].y_end = fields[fields_count].y + height;
        fields[fields_count].sound = sound;
        fields[fields_count].field_type = FIELD_TYPE_SCROLL;

        s_field_scroll* scroll = Field_Data_Scroll(&fields[fields_count]);
        scroll->return_value = return_value;
        scroll->set_value = set_value;
        scroll->range_min = range_min;
        scroll->range_max = range_max;

        if (height < width) {
            scroll->is_vertical = 0;
        } else {
            scroll->is_vertical = 1;
        }

        scroll->value = value;
        fields[fields_count].hotkey = Shortcut_Character(hotkey_str);

        int16_t current_count = fields_count;
        fields_count++;

        return current_count;
    }

    int16_t __cdecl Add_Multi_Hot_Key_Field(const char* help_text) {
        if (fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        int16_t current_idx = fields_count;
        s_field* field = &fields[current_idx];

        field->x = -1;
        field->y = -1;
        field->x_end = -1;
        field->y_end = -1;
        field->hotkey = '\0';
        field->field_type = FIELD_TYPE_MULTI_HOTKEY;

        s_field_multi_hotkey* multi_hotkey = Field_Data_Multi_Hot_Key(field);
        multi_hotkey->help = help_text;
        multi_hotkey->help_start = 0;
        multi_hotkey->help_len = (short)strlen(help_text);

        multi_hotkey_active_field = 1;
        fields_count++;

        return fields_count - 1;
    }

    int16_t __cdecl Scan_Input() {
        platform::Check_Exit();

        platform::Pump_Game_Input_Queue();
        mouse::Pointer_X();
        mouse::Pointer_Y();
        mouse::Check_Mouse_Shape();

        pointer_offset = mouse::Get_Pointer_Offset();

        int16_t field_id = Scan_Field();
        s_field* field = &fields[field_id];

        int16_t type = field->field_type;

        if (type == FIELD_TYPE_SCROLL_RETURN) {
            s_field_scroll* scroll = Field_Data_Scroll(field);
            *scroll->value = scroll->set_value;
            return scroll->return_value;
        }

        if (type == FIELD_TYPE_STRING_LIST) {
            s_field_string_list* string_list = Field_Data_String_List(field);
            if (string_list->is_selectable == 0) {
                return 0;
            }
        }

        if (type == FIELD_TYPE_GRID) {
            if (down_mouse_button == -1 || fields[down_mouse_button].field_type != FIELD_TYPE_GRID) {
                s_field_grid* grid = Field_Data_Grid(field);
                *grid->grid_x_ptr = (int16_t)((mouse::Pointer_X() - field->x) / grid->step_x);
                *grid->grid_y_ptr = (int16_t)((mouse::Pointer_Y() - field->y) / grid->step_y);
            }
        }

        return field_id;
    }

    int16_t __cdecl Scan_Field() {
        platform::Check_Exit();

        platform::Pump_Game_Input_Queue();
        int16_t mouse_x = static_cast<int16_t>(mouse::Pointer_X());
        int16_t mouse_y = static_cast<int16_t>(mouse::Pointer_Y());
        mouse::Check_Mouse_Shape();
        pointer_offset = mouse::Get_Pointer_Offset();

        if (fields_count > 1) {
            int32_t adj_x = static_cast<int32_t>(mouse_x) + pointer_offset;
            int32_t adj_y = static_cast<int32_t>(mouse_y) + pointer_offset;

            for (int16_t i = 1; i < fields_count; ++i) {
                s_field* field = &fields[i];

                if (field->x <= adj_x && adj_x <= field->x_end &&
                    field->y <= adj_y && adj_y <= field->y_end) {
                    return i;
                }
            }
        }

        return 0;
    }

    int16_t __cdecl Add_Locked_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, int16_t* selection, const char* hotkey, int8_t sound) {
        if ((int32_t)fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        s_field& current_field = fields[fields_count];

        struct_::Clear_Structure(&current_field, sizeof(current_field));

        current_field.x = x;
        current_field.y = y;
        current_field.x_end = current_field.x + animate::Get_Width(pic) - 1;
        current_field.y_end = current_field.y + animate::Get_Height(pic) - 1;
        current_field.field_type = FIELD_TYPE_LOCKED;
        current_field.font_style = fonts::Get_Current_Font_Style();
        current_field.font_color = fonts::Get_Current_Font_Color();
        current_field.sound = sound;
        current_field.hotkey = Shortcut_Character(hotkey);
        s_field_locked* locked = Field_Data_Locked_Button(&current_field);
        locked->help = help;
        locked->pic = pic;
        locked->draw_style = animate::Get_Full_Store_Flag(pic);
        locked->selection = selection;

        int16_t current_index = fields_count;
        fields_count++;

        return current_index;
    }

    void __cdecl Release_Fields() {
        fields_count = old_fields_count;
        input_delay = old_input_delay;
        down_mouse_button = old_down_mouse_button;

        max_fields_count = MAX_FIELDS_COUNT;

        auto_input_variable = old_auto_input_variable;
        input_field_active = old_input_field_active;
        fields = old_fields;
        active_input_field_number = old_active_input_field_number;
    }

    int16_t __cdecl Add_String_List_Field(
        int16_t x,
        int16_t y,
        int16_t width,
        const char* help,
        int16_t is_selectable,
        int8_t* value,
        int16_t help_start,
        int16_t gradient_mode,
        uint8_t* gradient_palette,
        int32_t gradient_extra,
        s_animation_header* gradient_pic,
        int32_t unused_param_12,
        const char* hotkey_str,
        const char* unused_param_14) {
        int16_t index = fields_count;

        fields[index].x = x;
        fields[index].y = y - 1;
        fields[index].x_end = x + width;

        fields[index].font_style = fonts::Get_Current_Font_Style();
        fields[index].font_color = &list_normal_color;

        fields[index].field_type = FIELD_TYPE_STRING_LIST;
        s_field_string_list* string_list = Field_Data_String_List(&fields[index]);
        string_list->highlight_color = &list_highlight_colors;
        string_list->gradient_colors = &list_special_colors;

        fields[index].y_end = y + fonts::Get_Font_Height() + 1;
        string_list->help_start = help_start;
        fields[index].hotkey = Shortcut_Character(hotkey_str);

        string_list->is_selectable = is_selectable;

        string_list->help = help;
        string_list->value = value;
        string_list->gradient_mode = gradient_mode;
        string_list->gradient_palette = gradient_palette;
        string_list->gradient_extra = gradient_extra;
        string_list->gradient_pic = gradient_pic;
        (void)unused_param_12;
        (void)unused_param_14;

        fields_count++;
        return fields_count - 1;
    }

    void __cdecl Enable_Mouse_Wait() {
        mouse_auto_exit = 0;
    }

    void __cdecl Disable_Mouse_Wait() {
        mouse_auto_exit = 1;
    }

    void __cdecl Timed_Wait_For_Input(uint32_t timeout) {
        bool has_input = false;

        if (timeout == 0) {
            do {
                platform::Check_Exit();
                if (key::Keyboard_Status()) {
                    has_input = true;
                }
                if (mouse::Mouse_Buffer() == 1) {
                    has_input = true;
                }
                Quick_Call_Auto_Function();
            } while (!has_input);
        } else {
            uint32_t ticks = 0;
            do {
                platform::Check_Exit();
                if (key::Keyboard_Status()) {
                    has_input = true;
                }
                if (mouse::Mouse_Buffer() == 1) {
                    has_input = true;
                }

                ticks++;
                Quick_Call_Auto_Function();

                if (ticks == timeout) {
                    has_input = true;
                }
            } while (!has_input);
        }
    }

    void __cdecl Draw_All_Fields() {
        draw_override_flag = 1;
        Draw_Visible_Fields();
        draw_override_flag = 0;
    }

    void __cdecl Mark_Fields() {
        old_fields_count = fields_count;
        old_input_delay = input_delay;
        old_down_mouse_button = down_mouse_button;
        old_auto_input_variable = auto_input_variable;
        old_input_field_active = input_field_active;
        max_fields_count -= fields_count;
        old_active_input_field_number = active_input_field_number;

        fields += fields_count;

        Clear_Fields();
    }

    void __cdecl Clipped_Dot(int16_t x, int16_t y, uint8_t color) {
        if (x >= video::screen_window_x1 && x <= video::screen_window_x2) {
            if (y >= video::screen_window_y1 && y <= video::screen_window_y2) {
                graphics::Dot(x, y, color);
            }
        }
    }

    void __cdecl Exit_List_Field() {
        exit_the_list_field = 1;
    }

    int16_t __cdecl Add_Clipped_Circle_Field(int16_t center_x, int16_t center_y, int16_t radius, const char* hotkey, int8_t sound) {
        int32_t max_x = center_x + radius;
        int32_t min_x = center_x - radius;
        int32_t min_y = center_y - radius;
        int32_t max_y = center_y + radius;

        if (max_fields_count <= fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        if (max_x < video::screen_window_x1 ||
            video::screen_window_x2 < min_x ||
            max_y < video::screen_window_y1 ||
            video::screen_window_y2 < min_y) {
            return -10000;
        }

        if (video::screen_window_x1 > min_x) {
            fields[fields_count].x = video::screen_window_x1;
        } else {
            fields[fields_count].x = static_cast<int16_t>(min_x);
        }

        if (video::screen_window_x2 < max_x) {
            fields[fields_count].x_end = video::screen_window_x2;
        } else {
            fields[fields_count].x_end = static_cast<int16_t>(max_x);
        }

        if (video::screen_window_y1 > min_y) {
            fields[fields_count].y = video::screen_window_y1;
        } else {
            fields[fields_count].y = static_cast<int16_t>(min_y);
        }

        if (video::screen_window_y2 < max_y) {
            fields[fields_count].y_end = video::screen_window_y2;
        } else {
            fields[fields_count].y_end = static_cast<int16_t>(max_y);
        }

        fields[fields_count].field_type = FIELD_TYPE_CIRCLE;
        s_field_circle* circle = Field_Data_Circle(&fields[fields_count]);
        circle->center_x = center_x;
        circle->center_y = center_y;
        circle->radius_squared = radius * radius;
        fields[fields_count].sound = sound;
        fields[fields_count].hotkey = Shortcut_Character(hotkey);

        int16_t result = fields_count;
        fields_count++;

        return result;
    }

    int16_t __cdecl Interpret_Mouse_Input() {
        platform::Check_Exit();

        int16_t field_idx = 0;
        down_mouse_button = -1;
        auto_input_variable = 0;

        int16_t ptr_x = mouse::Pointer_X();
        int16_t ptr_y = mouse::Pointer_Y();
        pointer_offset = mouse::Get_Pointer_Offset();

        if (key::Keyboard_Status() != 0) {
            int16_t key_code = Interpret_Keyboard_Input(&field_idx);
            if (key_code == 0 || field_idx < 0 || key_code == -1) {
                if (multi_hotkey_active_field != 0) Clear_Multi_Hot_Key_Offsets();
                return field_idx;
            }

            if (key_code == (int16_t)0x8500) {
                if (capture::Check_Release_Version() != 1) {
                    F11_Key();
                }
            }

            if (key_code == (int16_t)0x8600) {
                mouse::Save_Mouse_State();
                capture::Screen_Flic_Capture();
                int16_t cur_y = mouse::Pointer_Y();
                int16_t cur_x = mouse::Pointer_X();
                platform::Force_Cursor_Pos(cur_x, cur_y);
                mouse::Restore_Mouse_State();
            }

            if (key_code == 0x011B) {
                if (mouse_cancel_disabled == 0) {
                    if (multi_hotkey_active_field != 0) Clear_Multi_Hot_Key_Offsets();
                    return -1;
                }
            }

            if (key_code == 0x00C0) {
                if (capture::Check_Release_Version() == 0) {
                    if (field_box_mode == 0) {
                        field_box_mode = 1;
                    } else {
                        field_box_mode = 0;
                    }
                }
            }

            if (key_code == 0x297E) {
                if (capture::Check_Release_Version() == 0) {
                    if (help_box_mode == 0) {
                        help_box_mode = 1;
                    } else {
                        help_box_mode = 0;
                    }
                }
            }

            if (field_idx != 0 && (int16_t)fields[field_idx].hotkey == (int16_t)key_code) {
                if (fields[field_idx].field_type == FIELD_TYPE_SCROLL) {
                    return 0;
                }

                Push_Field_Down(field_idx, ptr_x, ptr_y);
                int16_t field_type = fields[field_idx].field_type;

                if (field_type == FIELD_TYPE_RADIO) {
                    s_field_radio* radio = Field_Data_Radio(&fields[field_idx]);
                    if (*radio->selection == 0) {
                        *radio->selection = 1;
                    } else {
                        *radio->selection = 0;
                    }
                } else if (field_type == FIELD_TYPE_LOCKED) {
                    s_field_locked* locked = Field_Data_Locked_Button(&fields[field_idx]);
                    if (*locked->selection == 0) {
                        *locked->selection = 1;
                    }
                } else if (field_type == FIELD_TYPE_SCROLL_RETURN) {
                    down_mouse_button = -1;
                    return Field_Data_Scroll(&fields[field_idx])->return_value;
                }

                if (field_type != FIELD_TYPE_HIDDEN && field_type != FIELD_TYPE_MULTI_HOTKEY) {
                    Quick_Call_Auto_Function();
                }
                down_mouse_button = -1;
                return field_idx;
            }

            if (key_code == 0x1C0D || key_code == (int16_t)0xE00D) {
                if (input_field_active != 0) {
                    int16_t ret = active_input_field_number;
                    Copy_Continuous_String();
                    if (mouse_auto_exit == 0) {
                        Quick_Call_Auto_Function();
                    }
                    input_field_active = 0;
                    down_mouse_button = -1;
                    active_input_field_number = -1;
                    return ret;
                }

                int16_t scanned_field = Scan_Field();
                if (scanned_field == 0) {
                    return 0;
                }

                if (scanned_field > 0) {
                    if (fields[scanned_field].field_type == FIELD_TYPE_CONTINUOUS_INPUT) {
                        s_field_cont_input* cont_input = Field_Data_Continuous_Input(&fields[scanned_field]);
                        if (input_field_active == 0) {
                            if (active_input_field_number == scanned_field) {
                                int16_t len = 0;
                                if (continuous_string[0] != '\0') {
                                    while (len < cont_input->max_length && continuous_string[len] != '\0') {
                                        len++;
                                    }
                                }
                                if (len != 0 && continuous_string[len - 1] == '_') {
                                    len--;
                                }
                                continuous_string[len] = '\0';
                                strlcpy(cont_input->buffer, continuous_string, (size_t)cont_input->max_length + 1);
                                active_input_field_number = -1;
                                cursor_position = 0;
                            } else {
                                Copy_Continuous_String();
                                active_input_field_number = scanned_field;
                                cursor_position = 0;
                                cursor_on = 0;
                            }
                        } else {
                            Copy_Continuous_String();
                            input_field_active = 1;
                            cursor_position = 0;
                            cursor_on = 0;
                            active_input_field_number = scanned_field;
                        }
                    } else if (input_field_active != 0) {
                        int16_t len = 0;
                        s_field_cont_input* active_cont_input = Field_Data_Continuous_Input(&fields[active_input_field_number]);
                        if (continuous_string[0] != '\0') {
                            while (len < active_cont_input->max_length && continuous_string[len] != '\0') {
                                len++;
                            }
                        }
                        if (len != 0 && continuous_string[len - 1] == '_') {
                            len--;
                        }
                        continuous_string[len] = '\0';
                        strlcpy(active_cont_input->buffer, continuous_string, (size_t)active_cont_input->max_length + 1);
                        cursor_position = 0;
                        input_field_active = 0;
                        active_input_field_number = -1;
                    }
                }

                if (scanned_field != 0) {
                    if (fields[scanned_field].field_type != FIELD_TYPE_SCROLL && fields[scanned_field].field_type != FIELD_TYPE_CONTINUOUS_INPUT) {
                        Push_Field_Down(scanned_field, ptr_x, ptr_y);
                    }

                    int16_t field_type = fields[scanned_field].field_type;
                    if (field_type == FIELD_TYPE_RADIO) {
                        s_field_radio* radio = Field_Data_Radio(&fields[scanned_field]);
                        if (*radio->selection == 0) {
                            *radio->selection = 1;
                        } else {
                            *radio->selection = 0;
                        }
                    } else if (field_type == FIELD_TYPE_LOCKED) {
                        s_field_locked* locked = Field_Data_Locked_Button(&fields[scanned_field]);
                        if (*locked->selection == 0) {
                            *locked->selection = 1;
                        }
                    } else if (field_type == FIELD_TYPE_SCROLL_RETURN) {
                        down_mouse_button = -1;
                        return Field_Data_Scroll(&fields[scanned_field])->return_value;
                    }

                    if (mouse_auto_exit == 0) {
                        Quick_Call_Auto_Function();
                    }
                    down_mouse_button = -1;
                    return scanned_field;
                }

                if (list_field_on != 0) {
                    for (int16_t i = 1; i < fields_count; ++i) {
                        if (fields[i].field_type == FIELD_TYPE_STRING_LIST) {
                            s_field_string_list* string_list = Field_Data_String_List(&fields[i]);
                            if (*string_list->value == string_list->help_start && string_list->is_selectable != 0) {
                                down_mouse_button = -1;
                                return i;
                            }
                        }
                    }
                }
            } else if (key_code == 0x0D2B || key_code == 0x0C2D) {
                int16_t scanned_field = Scan_Field();
                if (scanned_field != 0 && fields[scanned_field].field_type == FIELD_TYPE_SCROLL) {
                    if (key_code == 0x0D2B) {
                        Increment_Scroll_Bar(scanned_field);
                    }
                    if (key_code == 0x0C2D) {
                        Decrement_Scroll_Bar(scanned_field);
                    }
                    down_mouse_button = -1;
                    return scanned_field;
                }
            }

            if (input_field_active > 0) {
                s_field_cont_input* active_cont_input = Field_Data_Continuous_Input(&fields[active_input_field_number]);
                if (key_code == 0x011B) {
                    strlcpy(continuous_string, active_cont_input->buffer, sizeof(continuous_string));
                    int16_t act_num = active_input_field_number;
                    active_input_field_number = -1;
                    input_field_active = 0;
                    cursor_position = 0;
                    cursor_on = 0;
                    return -act_num;
                }
                if (key_code == 0x0E7F) {
                    continuous_string[0] = '\0';
                    active_input_field_first = 0;
                } else if (key_code == 0x0E08) {
                    if (active_input_field_first == 0) {
                        int16_t len = 0;
                        if (continuous_string[0] != '\0') {
                            while (len < active_cont_input->max_length - 1 && continuous_string[len] != '\0' && continuous_string[len] != '_') {
                                len++;
                            }
                        }
                        if (len > 0) {
                            cursor_position = 0;
                            continuous_string[len - 1] = '\0';
                        }
                    } else {
                        continuous_string[0] = '\0';
                        active_input_field_first = 0;
                    }
                } else {
                    active_input_field_first = 0;
                    int16_t chr = (int16_t)((int32_t)key_code % 0x100);
                    if (chr >= 0x20 && chr <= 0x7E && chr != 0x5F) {
                        if (active_input_field_number >= 0 && active_input_field_number < fields_count) {
                            int16_t font_shadow = active_cont_input->font_shadow_style;
                            if (font_shadow == -1) {
                                fonts::Set_Font_Style_Shadow_Down(fields[active_input_field_number].font_style, fields[active_input_field_number].font_color);
                            } else if (font_shadow == 0) {
                                fonts::Set_Font_Style(fields[active_input_field_number].font_style, fields[active_input_field_number].font_color);
                            } else if (font_shadow == 1) {
                                fonts::Set_Font_Style_Shadow_Up(fields[active_input_field_number].font_style, fields[active_input_field_number].font_color);
                            } else {
                                fonts::Set_Font_Style(fields[active_input_field_number].font_style, fields[active_input_field_number].font_color);
                            }
                        }
                        int16_t len = 0;
                        while (continuous_string[len] != '_' && continuous_string[len] != '\0' && len < active_cont_input->max_length - 1) {
                            len++;
                        }
                        if (len < active_cont_input->max_length - 1 && fonts::Get_String_Width(continuous_string) + fields[active_input_field_number].x <= fields[active_input_field_number].x_end) {
                            continuous_string[len] = (char)chr;
                            continuous_string[len + 1] = '\0';
                        } else {
                            continuous_string[len] = '\0';
                        }
                        cursor_on = 0;
                        cursor_position = 0;
                    }
                }
            }

            down_mouse_button = -1;
            return 0;
        }

        int16_t mouse_btn = mouse::Mouse_Button();
        if (mouse_btn == 0) {
            if (mouse::Mouse_Buffer() == 0) {
                pressed_button_flag = 0;
                return 0;
            }

            int16_t buf_btn = mouse::Mouse_Buffer_Button();
            if (buf_btn == 2) {
                if (help_list_active != 0 && Check_Help_List() == 0) {
                    mouse::Mouse_Buffer();
                    mouse::Mouse_Buffer2();
                    return 0;
                }
                if (mouse_cancel_disabled == 0) {
                    mouse::Mouse_Buffer();
                    mouse::Mouse_Buffer2();
                    if (multi_hotkey_active_field != 0) Clear_Multi_Hot_Key_Offsets();
                    return -1;
                }
            }

            int16_t buf_x = mouse::Mouse_Buffer_X();
            int16_t buf_y = mouse::Mouse_Buffer_Y();
            if (pressed_button_flag == 0) {
                pressed_button_flag = 1;
                pressed_mouse_x_value = buf_x;
                pressed_mouse_y_value = buf_y;
            }

            field_idx = 0;
            pointer_offset = mouse::Get_Pointer_Offset();

            for (int16_t i = 1; i < fields_count; ++i) {
                int32_t x = (int32_t)buf_x + pointer_offset;
                int32_t y = (int32_t)buf_y + pointer_offset;

                if (fields[i].x <= x && x <= fields[i].x_end && fields[i].y <= y && y <= fields[i].y_end) {
                    if (fields[i].field_type != FIELD_TYPE_CIRCLE) {
                        field_idx = i;
                        break;
                    }

                    s_field_circle* circle = Field_Data_Circle(&fields[i]);
                    int dx = buf_x - circle->center_x;
                    int dy = buf_y - circle->center_y;
                    if (dx * dx + dy * dy <= circle->radius_squared) {
                        field_idx = i;
                        break;
                    }
                }
            }

            if (field_idx != 0) {
                if (fields[field_idx].field_type != FIELD_TYPE_CONTINUOUS_INPUT) {
                    auto_input_variable = field_idx;
                    Push_Field_Down(field_idx, buf_x, buf_y);
                    Quick_Call_Auto_Function();
                }

                switch (fields[field_idx].field_type) {
                    case FIELD_TYPE_RADIO:
                        if (*Field_Data_Radio(&fields[field_idx])->selection == 0) {
                            *Field_Data_Radio(&fields[field_idx])->selection = 1;
                        } else {
                            *Field_Data_Radio(&fields[field_idx])->selection = 0;
                        }
                        break;
                    case FIELD_TYPE_LOCKED:
                        if (*Field_Data_Locked_Button(&fields[field_idx])->selection == 0) {
                            *Field_Data_Locked_Button(&fields[field_idx])->selection = 1;
                        }
                        break;
                    case FIELD_TYPE_LEGACY_INPUT:
                        Push_Field_Down(field_idx, buf_x, buf_y);
                        break;
                    case FIELD_TYPE_SCROLL_RETURN:
                        down_mouse_button = -1;
                        return Field_Data_Scroll(&fields[field_idx])->return_value;
                    case FIELD_TYPE_CONTINUOUS_INPUT: {
                        s_field_cont_input* cont_input = Field_Data_Continuous_Input(&fields[field_idx]);
                        if (input_field_active == 0) {
                            strlcpy(continuous_string, cont_input->buffer, sizeof(continuous_string));
                            input_field_active = 1;
                            cursor_on = 0;
                            cursor_position = 0;
                            active_input_field_number = field_idx;
                        } else if (field_idx != active_input_field_number) {
                            strlcpy(continuous_string, cont_input->buffer, sizeof(continuous_string));
                            cursor_on = 0;
                            cursor_position = 0;
                            active_input_field_number = field_idx;
                        }
                        break;
                    }
                    default:
                        if (input_field_active != 0) {
                            int16_t len = 0;
                            s_field_cont_input* active_cont_input = Field_Data_Continuous_Input(&fields[active_input_field_number]);
                            if (continuous_string[0] != '\0') {
                                while (len < active_cont_input->max_length && continuous_string[len] != '\0') {
                                    len++;
                                }
                            }
                            if (len != 0 && continuous_string[len - 1] == '_') {
                                len--;
                            }
                            continuous_string[len] = '\0';
                            strlcpy(active_cont_input->buffer, continuous_string, (size_t)active_cont_input->max_length + 1);
                            input_field_active = 0;
                            active_input_field_number = -1;
                        }
                        break;
                }
            }

            down_mouse_button = -1;
            if (field_idx != 0) {
                mouse::Mouse_Buffer2();
                last_button_number = buf_btn;
                last_button_x = buf_x;
                last_button_y = buf_y;
            }

            auto_input_variable = 0;
            if (buf_btn != 2) {
                return field_idx;
            } else {
                return -field_idx;
            }
        } else {
            if (mouse_btn == 2) {
                if (help_list_active != 0 && Check_Help_List() == 0) {
                    mouse::Mouse_Buffer();
                    mouse::Mouse_Buffer2();
                    return 0;
                }
                if (mouse_cancel_disabled == 0) {
                    while (mouse::Mouse_Button() == 2) {
                        Quick_Call_Auto_Function();
                    }
                    mouse::Mouse_Buffer();
                    mouse::Mouse_Buffer2();
                    return -1;
                }
            }

            ptr_x = mouse::Pointer_X();
            ptr_y = mouse::Pointer_Y();
            if (pressed_button_flag == 0) {
                pressed_button_flag = 1;
                pressed_mouse_x_value = ptr_x;
                pressed_mouse_y_value = ptr_y;
            }

            while (mouse::Mouse_Button() != 0) {
                ptr_x = mouse::Pointer_X();
                ptr_y = mouse::Pointer_Y();
                pointer_offset = mouse::Get_Pointer_Offset();

                field_idx = 0;
                if (dynamic_field_refresh != 0) {
                    field_idx = Scan_Field();
                    down_mouse_button = -1;
                }

                if (field_idx == 0) {
                    if (down_mouse_button != -1) {
                        if (fields[down_mouse_button].field_type == FIELD_TYPE_SCROLL) {
                            Invoke_Auto_Function();
                        }
                        int16_t down_type = fields[down_mouse_button].field_type;
                        if (down_type != FIELD_TYPE_MULTI_BUTTON && down_type != FIELD_TYPE_STRING_LIST && down_type != FIELD_TYPE_CONTINUOUS_INPUT) {
                            mouse::Save_Mouse_State();
                            Draw_Field(down_mouse_button, 0);
                            platform::Force_Cursor_Pos(ptr_x, ptr_y);
                            mouse::Restore_Mouse_State();
                        }
                        down_mouse_button = -1;
                    }
                    break;
                }

                if (down_mouse_button != field_idx && fields[field_idx].field_type != FIELD_TYPE_LEGACY_INPUT && fields[field_idx].field_type != FIELD_TYPE_CONTINUOUS_INPUT) {
                    bool skip_push = false;
                    if (down_mouse_button != -1) {
                        if (fields[field_idx].field_type == FIELD_TYPE_GRID && fields[down_mouse_button].field_type == FIELD_TYPE_GRID) {
                            skip_push = true;
                        } else if (fields[down_mouse_button].field_type == FIELD_TYPE_SCROLL) {
                            Invoke_Auto_Function();
                        }
                    }
                    if (!skip_push) {
                        Push_Field_Down(field_idx, ptr_x, ptr_y);
                    }
                }

                if (fields[field_idx].field_type == FIELD_TYPE_CONTINUOUS_INPUT) {
                    s_field_cont_input* cont_input = Field_Data_Continuous_Input(&fields[field_idx]);
                    if (input_field_active == 0 || field_idx != active_input_field_number) {
                        strlcpy(continuous_string, cont_input->buffer, sizeof(continuous_string));
                        input_field_active = 1;
                        active_input_field_first = 1;
                        cursor_position = 0;
                        cursor_on = 0;
                        active_input_field_number = field_idx;
                    }
                } else if (input_field_active != 0) {
                    int16_t len = 0;
                    s_field_cont_input* active_cont_input = Field_Data_Continuous_Input(&fields[active_input_field_number]);
                    if (continuous_string[0] != '\0') {
                        while (len < active_cont_input->max_length && continuous_string[len] != '\0') {
                            len++;
                        }
                    }
                    if (len != 0 && continuous_string[len - 1] == '_') {
                        len--;
                    }
                    continuous_string[len] = '\0';
                    strlcpy(active_cont_input->buffer, continuous_string, (size_t)active_cont_input->max_length + 1);
                    active_input_field_number = -1;
                    input_field_active = 0;
                }

                auto_input_variable = field_idx;
                if (mouse_auto_exit != 0) break;

                if (mouse::Mouse_Button() != 0) {
                    Call_Auto_Function();
                }
                mouse::Mouse_Buffer();
                mouse::Mouse_Buffer2();
            }

            if (fields[auto_input_variable].field_type == FIELD_TYPE_SCROLL) {
                Invoke_Auto_Function();
            }
            auto_input_variable = 0;

            if (field_idx != 0) {
                mouse::Mouse_Buffer();
                mouse::Mouse_Buffer2();
                last_button_number = mouse_btn;
                last_button_y = ptr_y;
                last_button_x = ptr_x;

                switch (fields[field_idx].field_type) {
                    case FIELD_TYPE_RADIO:
                        if (*Field_Data_Radio(&fields[field_idx])->selection == 0) {
                            *Field_Data_Radio(&fields[field_idx])->selection = 1;
                        } else {
                            *Field_Data_Radio(&fields[field_idx])->selection = 0;
                        }
                        break;
                    case FIELD_TYPE_LOCKED:
                        if (*Field_Data_Locked_Button(&fields[field_idx])->selection == 0) {
                            *Field_Data_Locked_Button(&fields[field_idx])->selection = 1;
                        }
                        break;
                    case FIELD_TYPE_LEGACY_INPUT:
                        Push_Field_Down(field_idx, ptr_x, ptr_y);
                        break;
                    case FIELD_TYPE_SCROLL_RETURN:
                        down_mouse_button = -1;
                        if (mouse_btn != 2) {
                            return Field_Data_Scroll(&fields[field_idx])->return_value;
                        }
                        if (mouse_cancel_disabled != 0) {
                            return -Field_Data_Scroll(&fields[field_idx])->return_value;
                        }
                        return -1;
                }
            }

            down_mouse_button = -1;
            if (mouse_btn == 1) {
                if (multi_hotkey_active_field != 0) Clear_Multi_Hot_Key_Offsets();
                return field_idx;
            }
            if (mouse_btn == 2) {
                if (multi_hotkey_active_field != 0) Clear_Multi_Hot_Key_Offsets();
                return -field_idx;
            }

            return 0;
        }
    }

    void __cdecl Disable_Draw_Field(int16_t field_idx) {
        switch (fields[field_idx].field_type) {
            case FIELD_TYPE_BUTTON:
                fields[field_idx].field_type = -1001;
                break;
            case FIELD_TYPE_RADIO:
                fields[field_idx].field_type = -1;
                break;
            case FIELD_TYPE_LOCKED:
                fields[field_idx].field_type = -2;
                break;
            case FIELD_TYPE_MULTI_BUTTON:
                fields[field_idx].field_type = -3;
                break;
            default:
                break;
        }
    }

    void __cdecl Enable_Draw_Field(int16_t field_idx) {
        int16_t field_type = fields[field_idx].field_type;

        if (field_type > -3) {
            if (field_type == -2) {
                fields[field_idx].field_type = FIELD_TYPE_LOCKED;
            } else if (field_type == -1) {
                fields[field_idx].field_type = FIELD_TYPE_RADIO;
            }
        } else if (field_type == -3) {
            fields[field_idx].field_type = FIELD_TYPE_MULTI_BUTTON;
        } else if (field_type == -1001) {
            fields[field_idx].field_type = FIELD_TYPE_BUTTON;
        }
    }

    int16_t __cdecl Get_List_Field(
        int16_t x, int16_t y, int16_t width, const char* help, const char* list_ptr, int32_t item_size, int8_t* selected_index, uint8_t* item_flags, int16_t gradient_mode, uint8_t* gradient_palette, int32_t gradient_extra, s_animation_header* gradient_pic, int32_t unused_param_13, const char* unused_param_14) {
        list_field_on = 1;
        Set_Input_Delay(1);
        Clear_Fields();
        exit_the_list_field = 0;

        int16_t font_height = fonts::Get_Font_Height();
        int16_t font_vert_spacing = fonts::Get_Font_Vertical_Spacing();

        int first_valid_index = -1;
        bool first_valid_found = false;
        int16_t index = 0;
        bool eof = false;

        int16_t cur_y = y;
        int16_t y_step = font_height + font_vert_spacing;

        do {
            if (*list_ptr == '\0') {
                eof = true;
            } else {
                if (!first_valid_found && (item_flags == nullptr || item_flags[index] != 0)) {
                    first_valid_found = true;
                    first_valid_index = index;
                }

                cur_y += y_step;

                int16_t flag = 1;
                if (item_flags != nullptr) {
                    flag = item_flags[index];
                }

                Add_String_List_Field(
                    x, cur_y, width, list_ptr, flag, selected_index, index, gradient_mode, gradient_palette, gradient_extra, gradient_pic, unused_param_13, s__00551100, unused_param_14);

                index++;
            }
            list_ptr += item_size;
        } while (!eof);

        int16_t cur_font_style = fonts::Get_Current_Font_Style();
        s_colors* cur_font_color = fonts::Get_Current_Font_Color();
        s_colors* cur_highlight = fonts::Get_Current_Highlight_Color();
        s_colors* cur_special = fonts::Get_Current_Special_Color();

        fonts::Set_Font_Style(cur_font_style, cur_font_color);

        for (int i = 0; i < 8; i++) {
            list_normal_color.colors[i] = cur_font_color->colors[i];
            list_special_colors.colors[i] = cur_special->colors[i];
            list_highlight_colors.colors[i] = cur_highlight->colors[i];
        }

        int8_t help_selection_placeholder = 0;
        int16_t help_field_idx = Add_String_List_Field(
            x, y, width, help, 1, &help_selection_placeholder, 1, 0, nullptr, 0, nullptr, 0, s__00551100, unused_param_14);

        int8_t cur_selection = *selected_index;
        if (cur_selection < 0 || cur_selection >= index || cur_selection < first_valid_index) {
            if (first_valid_index < 0 || first_valid_index >= index) {
                *selected_index = -1;
            } else {
                *selected_index = static_cast<int8_t>(Field_Data_String_List(&fields[first_valid_index + 1])->help_start);
            }
        }

        int16_t input;
        bool exit_flag;
        do {
            platform::Check_Exit();
            timer::Mark_Time();
            input = Get_Input();

            exit_flag = false;
            if (input != 0) {
                exit_flag = true;
                if (input > 0 && item_flags != nullptr && item_flags[input - 1] == 0) {
                    exit_flag = false;
                }
                if (input == help_field_idx) {
                    exit_flag = false;
                }
            }

            if (exit_flag) {
                break;
            }

            if (exit_the_list_field == 1) {
                exit_flag = true;
            }

            video::Set_Page_Off();
            Invoke_Auto_Function();

            if (list_field_scan_flag != 0) {
                int16_t scan_idx = Scan_Input();
                if (scan_idx > 0) {
                    *selected_index = static_cast<int8_t>(Field_Data_String_List(&fields[scan_idx])->help_start);
                }
            }

            palette::Refresh_Palette();
            platform::Toggle_Pages_Stub();
            timer::Release_Time(auto_function_delay);
        } while (!exit_flag);

        Clear_Fields();
        list_field_on = 0;
        list_field_scan_flag = 1;
        mouse::Mouse_Buffer();
        mouse::Mouse_Buffer2();

        if (input < 0) {
            return -1;
        }
        return input - 1;
    }

    void __cdecl F11_Key() {
        s_memorystatus memory_status;
        char buffer[80];

        Clear_Fields();

        Query_Memory_Status(&memory_status);

        snprintf(buffer, sizeof(buffer), "Avail Mem = %dk   Total Mem = %dk", (uint32_t)(memory_status.dwAvailPhys >> 10), (uint32_t)(memory_status.dwTotalPhys >> 10));

        video::Clear_Display_And_Release_Palette();

        Show_Message_Box(platform::g_main_window, buffer, "Orion2re", true);

        platform::Prepare_Exit();
        platform::Check_Exit();
    }

    void __cdecl Find_Bar_Position(int16_t field_idx) {
        s_field* field = &fields[field_idx];
        s_field_scroll* scroll = Field_Data_Scroll(field);
        int16_t new_value;

        if (scroll->is_vertical == 0) {
            int16_t width = field->x_end - field->x;
            int16_t mouse_x = mouse::Pointer_X() + pointer_offset;

            if (mouse_x >= field->x_end) {
                new_value = scroll->range_max;
            } else if (mouse_x <= field->x) {
                new_value = scroll->range_min;
            } else {
                int delta = mouse_x - field->x;
                int range = scroll->range_max - scroll->range_min;
                new_value = (int16_t)((delta * range) / width) + scroll->range_min;
            }
        } else {
            int16_t height = field->y_end - field->y;
            int16_t mouse_y = mouse::Pointer_Y() + pointer_offset;

            if (mouse_y >= field->y_end) {
                new_value = scroll->range_min;
            } else if (mouse_y <= field->y) {
                new_value = scroll->range_max;
            } else {
                int delta = field->y_end - mouse_y;
                int range = scroll->range_max - scroll->range_min;
                new_value = (int16_t)((delta * range) / height);
            }
        }

        if (new_value < scroll->return_value) {
            new_value = scroll->return_value;
        }

        if (new_value > scroll->set_value) {
            new_value = scroll->set_value;
        }

        *scroll->value = new_value;
    }

    int16_t __cdecl Add_Clipped_Hidden_Field(int16_t x, int16_t y, int16_t x_end, int16_t y_end, const char* hotkey, int8_t sound) {
        if (fields_count >= max_fields_count) {
            exitm::Exit_With_Message("You exceeded the max number of fields (150)");
        }

        if (x_end < video::screen_window_x1 || x > video::screen_window_x2 ||
            y_end < video::screen_window_y1 || y > video::screen_window_y2) {
            return -10000;
        }

        if (x < video::screen_window_x1) {
            x = video::screen_window_x1;
        }

        if (x_end > video::screen_window_x2) {
            x_end = video::screen_window_x2;
        }

        if (y < video::screen_window_y1) {
            y = video::screen_window_y1;
        }

        if (y_end > video::screen_window_y2) {
            y_end = video::screen_window_y2;
        }

        fields[fields_count].x = x;
        fields[fields_count].x_end = x_end;
        fields[fields_count].y = y;
        fields[fields_count].y_end = y_end;
        fields[fields_count].field_type = FIELD_TYPE_HIDDEN;
        fields[fields_count].sound = sound;
        fields[fields_count].hotkey = Shortcut_Character(hotkey);

        int16_t result = fields_count;
        fields_count++;

        return result;
    }

    void __cdecl Draw_Visible_Fields_Back() {
        int16_t x1 = video::screen_window_x1;
        int16_t x2 = video::screen_window_x2;
        int16_t y1 = video::screen_window_y1;
        int16_t y2 = video::screen_window_y2;

        graphics::Reset_Window();
        video::Set_Page_Back();

        if (draw_fields_flag != 0 && fields_count > 1) {
            for (int16_t i = 1; i < fields_count; ++i) {
                s_field* f = &fields[i];

                if (i == down_mouse_button && f->field_type != FIELD_TYPE_LEGACY_INPUT && f->field_type != FIELD_TYPE_GRID && f->field_type != FIELD_TYPE_STRING_LIST) {
                    Draw_Field(i, 1);
                    continue;
                }

                switch (f->field_type) {
                    case FIELD_TYPE_BUTTON: {
                        s_field_button* button = Field_Data_Button(f);
                        animate::Reset_Animation_Frame(button->pic);
                        animate::Draw(f->x, f->y, button->pic);
                        if (button->help != nullptr && *button->help != '\0') {
                            fonts::Set_Font_Style(f->font_style, f->font_color);
                            int16_t h = fonts::Get_Font_Height() - 1;
                            fonts::Print_Centered(
                                f->x + (f->x_end - f->x) / 2,
                                f->y + (f->y_end - f->y) / 2 - h / 2,
                                button->help);
                        }
                    } break;
                    case FIELD_TYPE_RADIO: {
                        s_field_radio* radio = Field_Data_Radio(f);
                        if (*radio->selection == 0) {
                            animate::Reset_Animation_Frame(radio->pic);
                            animate::Draw(f->x, f->y, radio->pic);
                            if (radio->help != nullptr && *radio->help != '\0') {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                                int16_t h = fonts::Get_Font_Height() - 1;
                                fonts::Print_Centered(
                                    f->x + (f->x_end - f->x) / 2,
                                    f->y + (f->y_end - f->y) / 2 - h / 2,
                                    radio->help);
                            }
                        } else {
                            if (radio->draw_style != 0) {
                                animate::Set_Animation_Frame(radio->pic, 1);
                            } else {
                                animate::Reset_Animation_Frame(radio->pic);
                                animate::Draw(f->x, f->y, radio->pic);
                            }
                            animate::Draw(f->x, f->y, radio->pic);
                            if (radio->help != nullptr && *radio->help != '\0') {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                                int16_t h = fonts::Get_Font_Height() - 1;
                                fonts::Print_Centered(
                                    f->x + (f->x_end - f->x) / 2 + selected_button_text_x_offset,
                                    f->y + (f->y_end - f->y) / 2 - h / 2 + selected_button_text_y_offset,
                                    radio->help);
                            }
                        }
                    } break;
                    case FIELD_TYPE_LOCKED: {
                        s_field_locked* locked = Field_Data_Locked_Button(f);
                        if (*locked->selection == 0) {
                            animate::Reset_Animation_Frame(locked->pic);
                            animate::Draw(f->x, f->y, locked->pic);
                            if (locked->help != nullptr && *locked->help != '\0') {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                                int16_t h = fonts::Get_Font_Height() - 1;
                                fonts::Print_Centered(
                                    f->x + (f->x_end - f->x) / 2,
                                    f->y + (f->y_end - f->y) / 2 - h / 2,
                                    locked->help);
                            }
                        } else {
                            if (locked->draw_style != 0) {
                                animate::Set_Animation_Frame(locked->pic, 1);
                            } else {
                                animate::Reset_Animation_Frame(locked->pic);
                                animate::Draw(f->x, f->y, locked->pic);
                            }
                            animate::Draw(f->x, f->y, locked->pic);
                            if (locked->help != nullptr && *locked->help != '\0') {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                                int16_t h = fonts::Get_Font_Height() - 1;
                                fonts::Print_Centered(
                                    f->x + (f->x_end - f->x) / 2 + selected_button_text_x_offset,
                                    f->y + (f->y_end - f->y) / 2 - h / 2 + selected_button_text_y_offset,
                                    locked->help);
                            }
                        }
                    } break;
                    case FIELD_TYPE_MULTI_BUTTON: {
                        s_field_multi_button* multi_button = Field_Data_Multi_Button(f);
                        if (*multi_button->selection == multi_button->button_value) {
                            if (multi_button->draw_style != 0) {
                                animate::Set_Animation_Frame(multi_button->pic, 1);
                            } else {
                                animate::Reset_Animation_Frame(multi_button->pic);
                                animate::Draw(f->x, f->y, multi_button->pic);
                            }
                            animate::Draw(f->x, f->y, multi_button->pic);
                            if (multi_button->help != nullptr && *multi_button->help != '\0') {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                                int16_t h = fonts::Get_Font_Height() - 1;
                                fonts::Print_Centered(
                                    f->x + (f->x_end - f->x) / 2 + selected_button_text_x_offset,
                                    f->y + (f->y_end - f->y) / 2 - h / 2 + selected_button_text_y_offset,
                                    multi_button->help);
                            }
                        } else {
                            animate::Reset_Animation_Frame(multi_button->pic);
                            animate::Draw(f->x, f->y, multi_button->pic);
                            if (multi_button->help != nullptr && *multi_button->help != '\0') {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                                int16_t h = fonts::Get_Font_Height() - 1;
                                fonts::Print_Centered(
                                    f->x + (f->x_end - f->x) / 2,
                                    f->y + (f->y_end - f->y) / 2 - h / 2,
                                    multi_button->help);
                            }
                        }
                    } break;
                    case FIELD_TYPE_LEGACY_INPUT:
                        if (down_mouse_button != i) {
                            s_field_input_string* legacy_input = Field_Data_Legacy_Input(f);
                            fonts::Set_Font_Style(f->font_style, f->font_color);
                            fonts::Set_Font_Highlight_Colors(legacy_input->highlight_color);
                            if (legacy_input->background_color != 0) {
                                graphics::Fill(f->x - 1, f->y, f->x_end, f->y_end, static_cast<uint8_t>(legacy_input->background_color));
                            }
                            if (legacy_input->draw_style == 0) {
                                fonts::Print(f->x, f->y, legacy_input->buffer);
                            } else {
                                fonts::Print_Right(f->x_end, f->y, legacy_input->buffer);
                            }
                        }
                        break;
                    case FIELD_TYPE_SCROLL: {
                        s_field_scroll* scroll = Field_Data_Scroll(f);
                        int16_t return_val = scroll->return_value;
                        int16_t set_val = scroll->set_value;
                        int16_t val = *scroll->value;

                        if (val < return_val) {
                            val = return_val;
                        }
                        if (val > set_val) {
                            val = set_val;
                        }
                        *scroll->value = val;
                        break;
                    }
                    case FIELD_TYPE_STRING_LIST: {
                        s_field_string_list* string_list = Field_Data_String_List(f);
                        fonts::Set_Font_Style(f->font_style, f->font_color);
                        int16_t spacing = fonts::Get_Font_Vertical_Spacing() / 2;
                        if (spacing == 0) {
                            spacing = 1;
                        }
                        int16_t font_height = fonts::Get_Font_Height();

                        if (*string_list->value == string_list->help_start) {
                            graphics::Gradient_Fill(
                                f->x - 1,
                                f->y - spacing + 1,
                                f->x_end,
                                f->y + font_height + 1,
                                string_list->gradient_mode,
                                string_list->gradient_palette,
                                string_list->gradient_extra,
                                string_list->gradient_pic,
                                string_list->gradient_colors);
                            if (string_list->gradient_mode == 17) {
                                fonts::Set_Font_Style(f->font_style, string_list->highlight_color);
                            }
                            fonts::Print(f->x, f->y + 1, string_list->help);
                        } else if (string_list->is_selectable == 0) {
                            fonts::Set_Font_Style(f->font_style, string_list->gradient_colors);
                            fonts::Print(f->x, f->y + 1, string_list->help);
                        } else {
                            fonts::Print(f->x, f->y + 1, string_list->help);
                        }
                        break;
                    }
                    case FIELD_TYPE_CONTINUOUS_INPUT: {
                        s_field_cont_input* cont_input = Field_Data_Continuous_Input(f);
                        int16_t shadow_style = cont_input->font_shadow_style;
                        if (active_input_field_number == i) {
                            if (shadow_style == -1) {
                                fonts::Set_Font_Style_Shadow_Down(f->font_style, f->font_color);
                            } else if (shadow_style == 0) {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                            } else if (shadow_style == 1) {
                                fonts::Set_Font_Style_Shadow_Up(f->font_style, f->font_color);
                            } else {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                            }

                            fonts::Set_Font_Highlight_Colors(cont_input->highlight_color);
                            fonts::Set_Highlight_Colors_On();

                            if (cont_input->background_color != 0) {
                                graphics::Fill(f->x - 1, f->y, f->x_end, f->y_end, static_cast<uint8_t>(cont_input->background_color));
                            }

                            int16_t pos = cursor_position / 2;
                            bool blink = (pos != cursor_on);

                            cursor_on = pos;
                            cursor_position++;
                            if (cursor_position > 3) {
                                cursor_position = 0;
                            }

                            if (blink) {
                                if (pos == 0) {
                                    int16_t len = 0;
                                    if (continuous_string[0] != '_') {
                                        while (len < cont_input->max_length && continuous_string[len] != '_') {
                                            len++;
                                        }
                                    }
                                    continuous_string[len] = '\0';
                                } else {
                                    int16_t len = 0;
                                    if (continuous_string[0] != '\0') {
                                        while (len < cont_input->max_length - 1 && continuous_string[len] != '\0') {
                                            len++;
                                        }
                                    }
                                    if (len < cont_input->max_length - 1) {
                                        continuous_string[len] = '_';
                                        continuous_string[len + 1] = '\0';
                                    }
                                }
                            }

                            if (cont_input->pic != nullptr) {
                                animate::Draw(f->x, f->y, cont_input->pic);
                            }

                            int16_t height = f->y_end - f->y;
                            int16_t font_height = fonts::Get_Font_Height();
                            if (height > font_height) {
                                fonts::Print(f->x + 10, f->y + (height / 2) - (font_height / 2), continuous_string);
                            } else {
                                fonts::Print(f->x + 10, f->y, continuous_string);
                            }
                            fonts::Set_Normal_Colors_On();

                        } else {
                            if (shadow_style == -1) {
                                fonts::Set_Font_Style_Shadow_Down(f->font_style, f->font_color);
                            } else if (shadow_style == 0) {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                            } else if (shadow_style == 1) {
                                fonts::Set_Font_Style_Shadow_Up(f->font_style, f->font_color);
                            } else {
                                fonts::Set_Font_Style(f->font_style, f->font_color);
                            }

                            if (cont_input->background_color != 0) {
                                graphics::Fill(f->x - 1, f->y, f->x_end, f->y_end, static_cast<uint8_t>(cont_input->background_color));
                            }

                            int16_t height = f->y_end - f->y;
                            int16_t font_height = fonts::Get_Font_Height();
                            if (height > font_height) {
                                fonts::Print(f->x + 10, f->y + (height / 2) - (font_height / 2), cont_input->buffer);
                            } else {
                                fonts::Print(f->x + 10, f->y, cont_input->buffer);
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }

                if (field_box_mode != 0 && f->field_type != FIELD_TYPE_MULTI_HOTKEY) {
                    video::Clip_On();
                    line::Line(f->x, f->y, f->x_end, f->y, static_cast<uint8_t>(field_outline_color));
                    line::Line(f->x, f->y, f->x, f->y_end, static_cast<uint8_t>(field_outline_color));
                    line::Line(f->x, f->y_end, f->x_end, f->y_end, static_cast<uint8_t>(field_outline_color));
                    line::Line(f->x_end, f->y, f->x_end, f->y_end, static_cast<uint8_t>(field_outline_color));
                    video::Clip_Off();
                }
            }
        }

        if (help_box_mode != 0 && help_list_count > 0) {
            for (int16_t i = 0; i < help_list_count; ++i) {
                const s_help_box& help_box = help_struct_pointer[i];
                if (help_box.help_id != -1) {
                    video::Clip_On();
                    line::Line(help_box.x1, help_box.y1, help_box.x2, help_box.y1, static_cast<uint8_t>(help_field_outline_color));
                    line::Line(help_box.x1, help_box.y1, help_box.x1, help_box.y2, static_cast<uint8_t>(help_field_outline_color));
                    line::Line(help_box.x1, help_box.y2, help_box.x2, help_box.y2, static_cast<uint8_t>(help_field_outline_color));
                    line::Line(help_box.x2, help_box.y1, help_box.x2, help_box.y2, static_cast<uint8_t>(help_field_outline_color));
                    video::Clip_Off();
                }
            }
        }

        graphics::Set_Window(x1, y1, x2, y2);
    }

    void __cdecl Push_Field_Down(int16_t field_idx, int16_t x, int16_t y) {
        if (x >= 0 && x < 640 && y >= 0 && y < 480) {
            mouse::Save_Mouse_State();

            if (down_mouse_button != field_idx) {
                if (down_mouse_button == -1) {
                    down_mouse_button = field_idx;
                    Draw_Field(field_idx, 1);

                    if (fields[field_idx].field_type == FIELD_TYPE_LEGACY_INPUT) {
                        x = mouse::Pointer_X();
                        y = mouse::Pointer_Y();
                    }

                    platform::Force_Cursor_Pos(x, y);
                    mouse::Restore_Mouse_State();
                    return;
                }

                int16_t down_idx = down_mouse_button;
                int16_t type_down = fields[down_idx].field_type;

                if ((type_down != FIELD_TYPE_MULTI_BUTTON || fields[field_idx].field_type == FIELD_TYPE_MULTI_BUTTON) &&
                    (type_down != FIELD_TYPE_STRING_LIST || (fields[field_idx].field_type == FIELD_TYPE_STRING_LIST && Field_Data_String_List(&fields[field_idx])->is_selectable != 0))) {
                    Draw_Field(down_idx, 0);
                }

                down_mouse_button = field_idx;
                Draw_Field(field_idx, 1);

                if (fields[field_idx].field_type == FIELD_TYPE_LEGACY_INPUT) {
                    x = mouse::Pointer_X();
                    y = mouse::Pointer_Y();
                }

                platform::Force_Cursor_Pos(x, y);
            }

            mouse::Restore_Mouse_State();
        }
    }

    uint16_t __cdecl Pressed_Mouse_X() {
        return pressed_mouse_x_value;
    }

    uint16_t __cdecl Pressed_Mouse_Y() {
        return pressed_mouse_y_value;
    }

    void __cdecl Copy_Continuous_String() {
        int16_t len = 0;
        s_field_cont_input* cont_input = Field_Data_Continuous_Input(&fields[active_input_field_number]);

        if (continuous_string[0] != '\0') {
            int16_t max_len = cont_input->max_length;
            do {
                if (len >= max_len) {
                    break;
                }
                len++;
            } while (continuous_string[len] != '\0');
        }

        if (len != 0 && continuous_string[len - 1] == '_') {
            len--;
        }

        continuous_string[len] = '\0';

        strlcpy(cont_input->buffer, continuous_string, (size_t)cont_input->max_length + 1);
    }

    void __cdecl Draw_Visible_Fields() {
        int16_t old_x1 = video::screen_window_x1;
        int16_t old_x2 = video::screen_window_x2;
        int16_t old_y1 = video::screen_window_y1;
        int16_t old_y2 = video::screen_window_y2;

        graphics::Reset_Window();
        video::Set_Page_Off();

        if (draw_fields_flag != 0 && fields_count > 1) {
            for (int i = 1; i < fields_count; i++) {
                if (down_mouse_button == i && fields[i].field_type != FIELD_TYPE_LEGACY_INPUT && fields[i].field_type != FIELD_TYPE_GRID && fields[i].field_type != FIELD_TYPE_STRING_LIST) {
                    Draw_Field(i, 1);
                    continue;
                }

                switch (fields[i].field_type) {
                    case FIELD_TYPE_BUTTON: {
                        s_field_button* button = Field_Data_Button(&fields[i]);
                        animate::Reset_Animation_Frame(button->pic);
                        animate::Draw(fields[i].x, fields[i].y, button->pic);
                        if (button->help != nullptr && button->help[0] != '\0') {
                            fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                            int font_height = fonts::Get_Font_Height();
                            int cx = fields[i].x + (fields[i].x_end - fields[i].x) / 2;
                            int cy = fields[i].y + ((fields[i].y_end - fields[i].y) / 2) - ((font_height - 1) / 2);
                            fonts::Print_Centered(cx, cy, button->help);
                        }
                        break;
                    }
                    case FIELD_TYPE_RADIO: {
                        s_field_radio* radio = Field_Data_Radio(&fields[i]);
                        if (*radio->selection == 0) {
                            animate::Reset_Animation_Frame(radio->pic);
                            animate::Draw(fields[i].x, fields[i].y, radio->pic);
                            if (radio->help != nullptr && radio->help[0] != '\0') {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                                int font_height = fonts::Get_Font_Height();
                                int cx = fields[i].x + (fields[i].x_end - fields[i].x) / 2;
                                int cy = fields[i].y + ((fields[i].y_end - fields[i].y) / 2) - ((font_height - 1) / 2);
                                fonts::Print_Centered(cx, cy, radio->help);
                            }
                        } else {
                            if (radio->draw_style != 0) {
                                animate::Set_Animation_Frame(radio->pic, 1);
                            } else {
                                animate::Reset_Animation_Frame(radio->pic);
                                animate::Draw(fields[i].x, fields[i].y, radio->pic);
                            }
                            animate::Draw(fields[i].x, fields[i].y, radio->pic);
                            if (radio->help != nullptr && radio->help[0] != '\0') {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                                int font_height = fonts::Get_Font_Height();
                                int cx = fields[i].x + (fields[i].x_end - fields[i].x) / 2 + selected_button_text_x_offset;
                                int cy = fields[i].y + ((fields[i].y_end - fields[i].y) / 2) - ((font_height - 1) / 2) + selected_button_text_y_offset;
                                fonts::Print_Centered(cx, cy, radio->help);
                            }
                        }
                    } break;
                    case FIELD_TYPE_LOCKED: {
                        s_field_locked* locked = Field_Data_Locked_Button(&fields[i]);
                        if (*locked->selection == 0) {
                            animate::Reset_Animation_Frame(locked->pic);
                            animate::Draw(fields[i].x, fields[i].y, locked->pic);
                            if (locked->help != nullptr && locked->help[0] != '\0') {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                                int font_height = fonts::Get_Font_Height();
                                int cx = fields[i].x + (fields[i].x_end - fields[i].x) / 2;
                                int cy = fields[i].y + ((fields[i].y_end - fields[i].y) / 2) - ((font_height - 1) / 2);
                                fonts::Print_Centered(cx, cy, locked->help);
                            }
                        } else {
                            if (locked->draw_style != 0) {
                                animate::Set_Animation_Frame(locked->pic, 1);
                            } else {
                                animate::Reset_Animation_Frame(locked->pic);
                                animate::Draw(fields[i].x, fields[i].y, locked->pic);
                            }
                            animate::Draw(fields[i].x, fields[i].y, locked->pic);
                            if (locked->help != nullptr && locked->help[0] != '\0') {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                                int font_height = fonts::Get_Font_Height();
                                int cx = fields[i].x + (fields[i].x_end - fields[i].x) / 2 + selected_button_text_x_offset;
                                int cy = fields[i].y + ((fields[i].y_end - fields[i].y) / 2) - ((font_height - 1) / 2) + selected_button_text_y_offset;
                                fonts::Print_Centered(cx, cy, locked->help);
                            }
                        }
                        break;
                    }
                    case FIELD_TYPE_MULTI_BUTTON: {
                        s_field_multi_button* multi_button = Field_Data_Multi_Button(&fields[i]);
                        if (*multi_button->selection == multi_button->button_value) {
                            if (multi_button->draw_style != 0) {
                                animate::Set_Animation_Frame(multi_button->pic, 1);
                            } else {
                                animate::Reset_Animation_Frame(multi_button->pic);
                                animate::Draw(fields[i].x, fields[i].y, multi_button->pic);
                            }
                            animate::Draw(fields[i].x, fields[i].y, multi_button->pic);
                            if (multi_button->help != nullptr && multi_button->help[0] != '\0') {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                                int font_height = fonts::Get_Font_Height();
                                int cx = fields[i].x + (fields[i].x_end - fields[i].x) / 2 + selected_button_text_x_offset;
                                int cy = fields[i].y + ((fields[i].y_end - fields[i].y) / 2) - ((font_height - 1) / 2) + selected_button_text_y_offset;
                                fonts::Print_Centered(cx, cy, multi_button->help);
                            }
                        } else {
                            animate::Reset_Animation_Frame(multi_button->pic);
                            animate::Draw(fields[i].x, fields[i].y, multi_button->pic);
                            if (multi_button->help != nullptr && multi_button->help[0] != '\0') {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                                int font_height = fonts::Get_Font_Height();
                                int cx = fields[i].x + (fields[i].x_end - fields[i].x) / 2;
                                int cy = fields[i].y + ((fields[i].y_end - fields[i].y) / 2) - ((font_height - 1) / 2);
                                fonts::Print_Centered(cx, cy, multi_button->help);
                            }
                        }
                        break;
                    }
                    case FIELD_TYPE_LEGACY_INPUT: {
                        s_field_input_string* legacy_input = Field_Data_Legacy_Input(&fields[i]);
                        if (down_mouse_button != i) {
                            fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                            fonts::Set_Font_Highlight_Colors(legacy_input->highlight_color);
                            if (legacy_input->background_color != 0) {
                                graphics::Fill(fields[i].x - 1, fields[i].y, fields[i].x_end, fields[i].y_end, (uint8_t)legacy_input->background_color);
                            }
                            if (legacy_input->draw_style == 0) {
                                fonts::Print(fields[i].x, fields[i].y, legacy_input->buffer);
                            } else {
                                fonts::Print_Right(fields[i].x_end, fields[i].y, legacy_input->buffer);
                            }
                        }
                        break;
                    }
                    case FIELD_TYPE_SCROLL: {
                        s_field_scroll* scroll = Field_Data_Scroll(&fields[i]);
                        int16_t ret_val = scroll->return_value;
                        int16_t max_val = scroll->set_value;
                        int16_t cur_val = *scroll->value;
                        if (cur_val < ret_val) {
                            cur_val = ret_val;
                        }
                        if (max_val < cur_val) {
                            cur_val = max_val;
                        }
                        *scroll->value = cur_val;
                        break;
                    }
                    case FIELD_TYPE_STRING_LIST: {
                        s_field_string_list* string_list = Field_Data_String_List(&fields[i]);
                        fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                        int spacing = fonts::Get_Font_Vertical_Spacing() / 2;
                        if (spacing == 0) {
                            spacing = 1;
                        }
                        int font_height = fonts::Get_Font_Height();
                        if (*string_list->value == string_list->help_start) {
                            graphics::Gradient_Fill(
                                fields[i].x - 1,
                                (fields[i].y - spacing) + 1,
                                fields[i].x_end,
                                fields[i].y + font_height + 1,
                                string_list->gradient_mode,
                                string_list->gradient_palette,
                                string_list->gradient_extra,
                                string_list->gradient_pic,
                                string_list->gradient_colors);
                            if (string_list->gradient_mode == 17) {
                                fonts::Set_Font_Style(fields[i].font_style, string_list->highlight_color);
                            }
                            fonts::Print(fields[i].x, fields[i].y + 1, string_list->help);
                        } else if (string_list->is_selectable == 0) {
                            fonts::Set_Font_Style(fields[i].font_style, string_list->gradient_colors);
                            fonts::Print(fields[i].x, fields[i].y + 1, string_list->help);
                        } else {
                            fonts::Print(fields[i].x, fields[i].y + 1, string_list->help);
                        }
                        break;
                    }
                    case FIELD_TYPE_CONTINUOUS_INPUT: {
                        s_field_cont_input* cont_input = Field_Data_Continuous_Input(&fields[i]);
                        int shadow_style = cont_input->font_shadow_style;
                        if (i != active_input_field_number) {
                            if (shadow_style == -1) {
                                fonts::Set_Font_Style_Shadow_Down(fields[i].font_style, fields[i].font_color);
                            } else if (shadow_style == 0) {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                            } else if (shadow_style == 1) {
                                fonts::Set_Font_Style_Shadow_Up(fields[i].font_style, fields[i].font_color);
                            } else {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                            }

                            if (cont_input->background_color != 0) {
                                graphics::Fill(fields[i].x - 1, fields[i].y, fields[i].x_end, fields[i].y_end, (uint8_t)cont_input->background_color);
                            }

                            int font_height = fonts::Get_Font_Height();
                            if (font_height < fields[i].y_end - fields[i].y) {
                                int y_centered = fields[i].y + ((fields[i].y_end - fields[i].y) / 2) - (font_height / 2);
                                fonts::Print(fields[i].x + 10, y_centered, cont_input->buffer);
                            } else {
                                fonts::Print(fields[i].x + 10, fields[i].y, cont_input->buffer);
                            }
                        } else {
                            if (shadow_style == -1) {
                                fonts::Set_Font_Style_Shadow_Down(fields[i].font_style, fields[i].font_color);
                            } else if (shadow_style == 0) {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                            } else if (shadow_style == 1) {
                                fonts::Set_Font_Style_Shadow_Up(fields[i].font_style, fields[i].font_color);
                            } else {
                                fonts::Set_Font_Style(fields[i].font_style, fields[i].font_color);
                            }

                            fonts::Set_Font_Highlight_Colors(cont_input->highlight_color);
                            fonts::Set_Highlight_Colors_On();

                            if (cont_input->background_color != 0) {
                                graphics::Fill(fields[i].x - 1, fields[i].y, fields[i].x_end, fields[i].y_end, (uint8_t)cont_input->background_color);
                            }

                            int cur_pos = cursor_position / 2;
                            bool cursor_changed = cur_pos != cursor_on;
                            cursor_on = (int16_t)cur_pos;
                            cursor_position++;
                            if (cursor_position > 3) {
                                cursor_position = 0;
                            }

                            if (cursor_changed) {
                                if (cur_pos == 0) {
                                    int len = 0;
                                    while (len < cont_input->max_length - 1 && continuous_string[len] != '_') {
                                        len++;
                                    }
                                    continuous_string[len] = '\0';
                                } else {
                                    int len = 0;
                                    while (len < cont_input->max_length - 2 && continuous_string[len] != '\0') {
                                        len++;
                                    }
                                    if (len < cont_input->max_length - 1) {
                                        strlcat(continuous_string, "_", sizeof(continuous_string));
                                    }
                                }
                            }

                            if (cont_input->pic != nullptr) {
                                animate::Draw(fields[i].x, fields[i].y, cont_input->pic);
                            }

                            int font_height = fonts::Get_Font_Height();
                            if (font_height < fields[i].y_end - fields[i].y) {
                                int y_centered = fields[i].y + ((fields[i].y_end - fields[i].y) / 2) - (font_height / 2);
                                fonts::Print(fields[i].x + 10, y_centered, continuous_string);
                            } else {
                                fonts::Print(fields[i].x + 10, fields[i].y, continuous_string);
                            }
                            fonts::Set_Normal_Colors_On();
                        }
                        break;
                    }
                    default:
                        break;
                }

                if (field_box_mode != 0 && fields[i].field_type != FIELD_TYPE_MULTI_HOTKEY) {
                    video::Clip_On();
                    line::Line(fields[i].x, fields[i].y, fields[i].x_end, fields[i].y, (uint8_t)field_outline_color);
                    line::Line(fields[i].x, fields[i].y, fields[i].x, fields[i].y_end, (uint8_t)field_outline_color);
                    line::Line(fields[i].x, fields[i].y_end, fields[i].x_end, fields[i].y_end, (uint8_t)field_outline_color);
                    line::Line(fields[i].x_end, fields[i].y, fields[i].x_end, fields[i].y_end, (uint8_t)field_outline_color);
                    video::Clip_Off();
                }
            }
        }

        if (help_box_mode != 0 && help_list_count > 0) {
            for (int16_t i = 0; i < help_list_count; ++i) {
                const s_help_box& help_box = help_struct_pointer[i];
                if (help_box.help_id != -1) {
                    video::Clip_On();
                    line::Line(help_box.x1, help_box.y1, help_box.x2, help_box.y1, (uint8_t)help_field_outline_color);
                    line::Line(help_box.x1, help_box.y1, help_box.x1, help_box.y2, (uint8_t)help_field_outline_color);
                    line::Line(help_box.x1, help_box.y2, help_box.x2, help_box.y2, (uint8_t)help_field_outline_color);
                    line::Line(help_box.x2, help_box.y1, help_box.x2, help_box.y2, (uint8_t)help_field_outline_color);
                    video::Clip_Off();
                }
            }
        }

        graphics::Set_Window(old_x1, old_y1, old_x2, old_y2);
    }

    int16_t __cdecl Check_Multi_Hot_Key_Fields(int16_t hotkey) {
        int16_t result = -1;

        for (int16_t i = 0; i < fields_count; ++i) {
            if (result != -1) {
                break;
            }

            if (fields[i].field_type == FIELD_TYPE_MULTI_HOTKEY) {
                s_field_multi_hotkey* multi_hotkey = Field_Data_Multi_Hot_Key(&fields[i]);
                int16_t help_start = multi_hotkey->help_start;

                if (multi_hotkey->help[help_start] == hotkey) {
                    multi_hotkey->help_start++;

                    if (multi_hotkey->help_start == multi_hotkey->help_len) {
                        result = i;
                    }
                }
            }
        }

        return result;
    }

    void __cdecl Clear_Multi_Hot_Key_Offsets() {
        for (int16_t i = 0; i < fields_count; ++i) {
            if (fields[i].field_type == FIELD_TYPE_MULTI_HOTKEY) {
                Field_Data_Multi_Hot_Key(&fields[i])->help_start = 0;
            }
        }
    }

    int16_t __cdecl Interpret_Keyboard_Input(int16_t* field_idx) {
        int16_t key_val = (int16_t)key::Read_Key();

        if (fields_count <= selected_count) {
            selected_count = 0;
        }

        int16_t out_cmd = 0;
        if (key_val <= 0x3B00) {
            if (key_val == 0x3B00) {
                out_cmd = -1001;
            } else if (key_val == 0x01F0) {
                out_cmd = -1500;
            }
        } else if (key_val <= 0x3D00) {
            if (key_val == 0x3D00) {
                out_cmd = -1003;
            } else if (key_val == 0x3C00) {
                out_cmd = -1002;
            }
        } else if (key_val <= 0x3F00) {
            if (key_val == 0x3F00) {
                out_cmd = -1005;
            } else if (key_val == 0x3E00) {
                out_cmd = -1004;
            }
        } else if (key_val <= 0x4100) {
            if (key_val == 0x4100) {
                out_cmd = -1007;
            } else if (key_val == 0x4000) {
                out_cmd = -1006;
            }
        } else if (key_val <= 0x4300) {
            if (key_val == 0x4300) {
                out_cmd = -1009;
            } else if (key_val == 0x4200) {
                out_cmd = -1008;
            }
        } else if (key_val <= 0x6800) {
            if (key_val == 0x6800) {
                out_cmd = -1101;
            } else if (key_val == 0x4400) {
                out_cmd = -1010;
            }
        } else if (key_val <= 0x6A00) {
            if (key_val == 0x6A00) {
                out_cmd = -1103;
            } else if (key_val == 0x6900) {
                out_cmd = -1102;
            }
        } else if (key_val <= 0x6C00) {
            if (key_val == 0x6C00) {
                out_cmd = -1105;
            } else if (key_val == 0x6B00) {
                out_cmd = -1104;
            }
        } else if (key_val <= 0x6E00) {
            if (key_val == 0x6E00) {
                out_cmd = -1107;
            } else if (key_val == 0x6D00) {
                out_cmd = -1106;
            }
        } else if (key_val == 0x6F00) {
            out_cmd = -1108;
        } else if (key_val == 0x7000) {
            out_cmd = -1109;
        } else if (key_val == 0x7100) {
            out_cmd = -1110;
        }

        if (out_cmd < 0) {
            *field_idx = out_cmd;
            return key_val;
        }

        if (multi_hotkey_active_field != 0) {
            uint8_t alt_key = Get_Alt_Alpha_Key(key_val);
            int16_t multi_hotkey_res = Check_Multi_Hot_Key_Fields(alt_key);
            if (multi_hotkey_res > -1) {
                *field_idx = multi_hotkey_res;
                return -1;
            }
        }

        int32_t eax_val = key_val;
        int32_t edx_val = (eax_val < 0) ? -1 : 0;
        eax_val = eax_val ^ edx_val;
        eax_val = eax_val - edx_val;
        eax_val = eax_val & 0xFF;
        eax_val = eax_val ^ edx_val;
        eax_val = eax_val - edx_val;

        int16_t hotkey_char = (int16_t)eax_val;

        if (hotkey_char == 0) {
            hotkey_char = key_val;
        }

        if (fields_count > fields_count) {
            *field_idx = fields_count;
            if (fields[fields_count].x < 640 && fields[fields_count].y < 480) {
                mouse::Save_Mouse_State();
                mouse_saved_center_x = fields[fields_count].x + (fields[fields_count].x_end - fields[fields_count].x) / 2;
                mouse_saved_center_y = fields[fields_count].y + (fields[fields_count].y_end - fields[fields_count].y) / 2;
                mouse::Restore_Mouse_State();
            }
        }

        for (int16_t i = 0; i < fields_count; i++) {
            if (active_input_field_number == -1 && fields[i].hotkey == hotkey_char) {
                *field_idx = i;
                break;
            }
        }

        if (fields[*field_idx].hotkey != hotkey_char) {
            hotkey_char = key_val;
        }

        return hotkey_char;
    }

    void __cdecl Increment_Scroll_Bar(int16_t field_idx) {
        s_field* field = &fields[field_idx];
        s_field_scroll* scroll = Field_Data_Scroll(field);
        int16_t range_min = scroll->range_min;
        int16_t* value_ptr = scroll->value;
        int32_t range_diff = scroll->range_max - range_min;

        int32_t percent = (((int32_t)*value_ptr - (int32_t)range_min) * 100) / range_diff + 5;

        int16_t new_val;
        if (percent <= 100) {
            new_val = (int16_t)(range_min + ((percent * range_diff) / 100));
        } else {
            new_val = scroll->set_value;
        }

        if (new_val > scroll->set_value) {
            new_val = scroll->set_value;
        }

        *value_ptr = new_val;
    }

    void __cdecl Decrement_Scroll_Bar(int16_t field_idx) {
        s_field_scroll* scroll = Field_Data_Scroll(&fields[field_idx]);
        int16_t range_min = scroll->range_min;
        int16_t* value_ptr = scroll->value;
        int16_t range = scroll->range_max - range_min;

        int16_t pct = (int16_t)((((*value_ptr - range_min) * 100) / range) - 5);

        int16_t new_value;
        if (pct < 0) {
            new_value = scroll->return_value;
        } else {
            new_value = range_min + (int16_t)(((int32_t)pct * range) / 100);
        }

        if (new_value < scroll->return_value) {
            new_value = scroll->return_value;
        }

        *value_ptr = new_value;
    }

    void __cdecl Draw_Field(int16_t field_idx, int16_t action) {
        if (draw_fields_flag == 0) {
            return;
        }

        s_field* field = &fields[field_idx];

        switch (field->field_type) {
            case FIELD_TYPE_BUTTON:
            case FIELD_TYPE_IRREGULAR_BUTTON: {
                int16_t x_pos;
                int16_t y_pos;
                const char* help_text;
                s_animation_header* pic;
                int16_t draw_style;

                if (field->field_type == FIELD_TYPE_IRREGULAR_BUTTON) {
                    const s_field_irregular_button* irregular_button = Field_Data_Irregular_Button(field);
                    x_pos = irregular_button->draw_x;
                    y_pos = irregular_button->draw_y;
                    help_text = irregular_button->help;
                    pic = irregular_button->pic;
                    draw_style = irregular_button->draw_style;
                } else {
                    const s_field_button* button = Field_Data_Button(field);
                    x_pos = field->x;
                    y_pos = field->y;
                    help_text = button->help;
                    pic = button->pic;
                    draw_style = button->draw_style;
                }

                if (action == 0) {
                    animate::Reset_Animation_Frame(pic);
                    animate::Draw(x_pos, y_pos, pic);

                    fonts::Set_Font_Style(field->font_style, field->font_color);
                    int16_t font_height = fonts::Get_Font_Height();
                    fonts::Print_Centered(
                        field->x + (field->x_end - field->x) / 2,
                        field->y + (field->y_end - field->y) / 2 - (font_height - 1) / 2,
                        help_text);
                } else {
                    if (draw_style == 0) {
                        animate::Reset_Animation_Frame(pic);
                        animate::Draw(x_pos, y_pos, pic);
                    } else {
                        animate::Set_Animation_Frame(pic, 1);
                    }
                    animate::Draw(x_pos, y_pos, pic);

                    fonts::Set_Font_Style(field->font_style, field->font_color);
                    int16_t font_height = fonts::Get_Font_Height();
                    fonts::Print_Centered(
                        field->x + (field->x_end - field->x) / 2 + selected_button_text_x_offset,
                        field->y + (field->y_end - field->y) / 2 - (font_height - 1) / 2 + selected_button_text_y_offset,
                        help_text);
                }
                break;
            }
            case FIELD_TYPE_RADIO: {
                const s_field_radio* radio = Field_Data_Radio(field);
                if (action != 0 && *radio->selection != 1) {
                    if (radio->draw_style == 0) {
                        animate::Reset_Animation_Frame(radio->pic);
                        animate::Draw(field->x, field->y, radio->pic);
                    } else {
                        animate::Set_Animation_Frame(radio->pic, 1);
                    }
                    animate::Draw(field->x, field->y, radio->pic);

                    fonts::Set_Font_Style(field->font_style, field->font_color);
                    int16_t font_height = fonts::Get_Font_Height();
                    fonts::Print_Centered(
                        field->x + (field->x_end - field->x) / 2 + selected_button_text_x_offset,
                        field->y + (field->y_end - field->y) / 2 - (font_height - 1) / 2 + selected_button_text_y_offset,
                        radio->help);
                } else {
                    animate::Reset_Animation_Frame(radio->pic);
                    animate::Draw(field->x, field->y, radio->pic);

                    fonts::Set_Font_Style(field->font_style, field->font_color);
                    int16_t font_height = fonts::Get_Font_Height();
                    fonts::Print_Centered(
                        field->x + (field->x_end - field->x) / 2,
                        field->y + (field->y_end - field->y) / 2 - (font_height - 1) / 2,
                        radio->help);
                }
                break;
            }
            case FIELD_TYPE_LOCKED: {
                const s_field_locked* locked = Field_Data_Locked_Button(field);
                if (*locked->selection == 0 && action != 1) {
                    animate::Reset_Animation_Frame(locked->pic);
                    animate::Draw(field->x, field->y, locked->pic);

                    fonts::Set_Font_Style(field->font_style, field->font_color);
                    int16_t font_height = fonts::Get_Font_Height();
                    fonts::Print_Centered(
                        field->x + (field->x_end - field->x) / 2,
                        field->y + (field->y_end - field->y) / 2 - (font_height - 1) / 2,
                        locked->help);
                } else {
                    if (locked->draw_style == 0) {
                        animate::Reset_Animation_Frame(locked->pic);
                        animate::Draw(field->x, field->y, locked->pic);
                    } else {
                        animate::Set_Animation_Frame(locked->pic, 1);
                    }
                    animate::Draw(field->x, field->y, locked->pic);

                    fonts::Set_Font_Style(field->font_style, field->font_color);
                    int16_t font_height = fonts::Get_Font_Height();
                    fonts::Print_Centered(
                        field->x + (field->x_end - field->x) / 2 + selected_button_text_x_offset,
                        field->y + (field->y_end - field->y) / 2 - (font_height - 1) / 2 + selected_button_text_y_offset,
                        locked->help);
                }
                break;
            }
            case FIELD_TYPE_MULTI_BUTTON: {
                s_field_multi_button* multi_button = Field_Data_Multi_Button(field);
                if (action == 0) {
                    *multi_button->selection = -1000;
                } else {
                    *multi_button->selection = multi_button->button_value;
                }

                if (*multi_button->selection != multi_button->button_value) {
                    animate::Reset_Animation_Frame(multi_button->pic);
                    animate::Draw(field->x, field->y, multi_button->pic);

                    fonts::Set_Font_Style(field->font_style, field->font_color);
                    int16_t font_height = fonts::Get_Font_Height();
                    fonts::Print_Centered(
                        field->x + (field->x_end - field->x) / 2,
                        field->y + (field->y_end - field->y) / 2 - (font_height - 1) / 2,
                        multi_button->help);
                } else {
                    if (multi_button->draw_style == 0) {
                        animate::Reset_Animation_Frame(multi_button->pic);
                        animate::Draw(field->x, field->y, multi_button->pic);
                    } else {
                        animate::Set_Animation_Frame(multi_button->pic, 1);
                    }
                    animate::Draw(field->x, field->y, multi_button->pic);

                    fonts::Set_Font_Style(field->font_style, field->font_color);
                    int16_t font_height = fonts::Get_Font_Height();
                    fonts::Print_Centered(
                        field->x + (field->x_end - field->x) / 2 + selected_button_text_x_offset,
                        field->y + (field->y_end - field->y) / 2 - (font_height - 1) / 2 + selected_button_text_y_offset,
                        multi_button->help);
                }
                break;
            }
            case FIELD_TYPE_LEGACY_INPUT: {
                if (action == 1) {
                    Input_String(field_idx);
                }
                break;
            }
            case FIELD_TYPE_SCROLL: {
                if (action == 1) {
                    Find_Bar_Position(field_idx);
                }
                break;
            }
            case FIELD_TYPE_GRID: {
                s_field_grid* grid = Field_Data_Grid(field);
                pointer_offset = mouse::Get_Pointer_Offset();
                if (action == 1) {
                    *grid->grid_x_ptr = (mouse::Pointer_X() - field->x + pointer_offset) / grid->step_x;
                    *grid->grid_y_ptr = (mouse::Pointer_Y() - field->y + pointer_offset) / grid->step_y;

                    int16_t max_x = (field->x_end - field->x) / grid->step_x;
                    if (max_x < *grid->grid_x_ptr) {
                        *grid->grid_x_ptr = max_x;
                    }

                    int16_t max_y = (field->y_end - field->y) / grid->step_y;
                    if (max_y < *grid->grid_y_ptr) {
                        *grid->grid_y_ptr = max_y;
                    }

                    if (*grid->grid_x_ptr < 0) {
                        *grid->grid_x_ptr = 0;
                    }

                    if (*grid->grid_y_ptr < 0) {
                        *grid->grid_y_ptr = 0;
                    }
                }
                break;
            }
            default: {
                break;
            }
        }
    }

    uint8_t __cdecl Get_Alt_Alpha_Key(int16_t keycode) {
        switch (keycode) {
            case 0x1000: return 0x51; /* 'Q' */
            case 0x1100: return 0x57; /* 'W' */
            case 0x1200: return 0x45; /* 'E' */
            case 0x1300: return 0x52; /* 'R' */
            case 0x1400: return 0x54; /* 'T' */
            case 0x1500: return 0x59; /* 'Y' */
            case 0x1600: return 0x55; /* 'U' */
            case 0x1700: return 0x49; /* 'I' */
            case 0x1800: return 0x4F; /* 'O' */
            case 0x1900: return 0x50; /* 'P' */
            case 0x1E00: return 0x41; /* 'A' */
            case 0x1F00: return 0x53; /* 'S' */
            case 0x2000: return 0x44; /* 'D' */
            case 0x2100: return 0x46; /* 'F' */
            case 0x2200: return 0x47; /* 'G' */
            case 0x2300: return 0x48; /* 'H' */
            case 0x2400: return 0x4A; /* 'J' */
            case 0x2500: return 0x4B; /* 'K' */
            case 0x2600: return 0x4C; /* 'L' */
            case 0x2C00: return 0x5A; /* 'Z' */
            case 0x2D00: return 0x58; /* 'X' */
            case 0x2E00: return 0x43; /* 'C' */
            case 0x2F00: return 0x56; /* 'V' */
            case 0x3000: return 0x42; /* 'B' */
            case 0x3100: return 0x4E; /* 'N' */
            case 0x3200: return 0x4D; /* 'M' */
            case 0x7800: return 0x30; /* '0' */
            case 0x7900: return 0x31; /* '1' */
            case 0x7A00: return 0x32; /* '2' */
            case 0x7B00: return 0x33; /* '3' */
            case 0x7C00: return 0x34; /* '4' */
            case 0x7D00: return 0x35; /* '5' */
            case 0x7E00: return 0x36; /* '6' */
            case 0x7F00: return 0x37; /* '7' */
            default: return 0xFF;
        }
    }

    int16_t __cdecl Check_Help_List() {
        int16_t mouse_x = static_cast<int16_t>(mouse::Pointer_X());
        int16_t mouse_y = static_cast<int16_t>(mouse::Pointer_Y());

        if (help_list_count == 0) {
            return 1;
        }

        for (int16_t i = 0; i < help_list_count; ++i) {
            const s_help_box& entry = help_struct_pointer[i];

            if (entry.help_id != -1 &&
                entry.x1 <= mouse_x && mouse_x <= entry.x2 &&
                entry.y1 <= mouse_y && mouse_y <= entry.y2) {
                TEXTBOX::Draw_Help_Entry(entry.help_id);
                return 0;
            }
        }

        return 1;
    }

    void __cdecl Input_String(int16_t field_idx) {
        platform::Check_Exit();

        int16_t len = 0;
        cursor_count = 0;
        cursor_position = 0;

        s_field* field = &fields[field_idx];
        s_field_input_string* legacy_input = Field_Data_Legacy_Input(field);
        int16_t max_length = legacy_input->max_length;
        int16_t max_width = field->x_end - field->x;
        bool is_editing_started = false;

        mouse::Save_Mouse_State();
        fonts::Set_Font_Style(field->font_style, field->font_color);

        char buffer[64];
        strlcpy(buffer, legacy_input->buffer, sizeof(buffer));

        len = (int16_t)strlen(buffer);

        while (max_width < fonts::Get_String_Width(buffer) && len > 0) {
            buffer[0] = '\0';
            fonts::Get_String_Width(buffer);
            len = 0;
        }

        cursor_count = len;
        if (max_length <= len) {
            cursor_count = max_length;
        }

        strlcpy(legacy_input->buffer, buffer, (size_t)legacy_input->max_length + 1);
        int16_t font_height = fonts::Get_Font_Height();
        Display_Input_Field(field_idx, buffer);

        int16_t done = 0;
        while (done == 0) {
            platform::Check_Exit();
            int16_t key_code = 0;
            while (done == 0) {
                platform::Check_Exit();
                key_code = key::Scan_Key();
                if (mouse::Mouse_Button() != 0 || mouse::Mouse_Buffer() != 0) {
                    done = 1;
                    break;
                }

                cursor_position++;
                if (cursor_position > font_height * 2 - 1) {
                    cursor_position = 0;
                }

                Display_Input_Field(field_idx, buffer);
                if (key_code != 0) {
                    break;
                }
            }

            if (done != 0) {
                break;
            }

            if (key_code == 0x1C0D) { // Enter
                break;
            } else if (key_code == 0x0E08) { // Backspace
                if (!is_editing_started) {
                    buffer[0] = '\0';
                    is_editing_started = true;
                    len = 0;
                    cursor_count = 0;
                    cursor_position = 0;
                } else if (len > 0) {
                    if (cursor_count < len) {
                        if (cursor_count > 0) {
                            for (int16_t i = cursor_count; i < len; i++) {
                                buffer[i - 1] = buffer[i];
                            }
                            len--;
                            cursor_count--;
                            buffer[len] = '\0';
                            cursor_position = 0;
                        }
                    } else {
                        cursor_position = 0;
                        len--;
                        cursor_count--;
                        buffer[len] = '\0';
                    }
                }
            } else if (key_code == 0x4BE0) { // Left Arrow
                is_editing_started = true;
                if (cursor_count > 0) {
                    cursor_position = 0;
                    cursor_count--;
                }
            } else if (key_code == 0x4DE0) { // Right Arrow
                if (cursor_count < max_length && cursor_count < len) {
                    cursor_position = 0;
                    cursor_count++;
                    if (cursor_count >= len) {
                        buffer[len] = ' ';
                        buffer[len + 1] = '\0';
                        if (cursor_count >= max_length || fonts::Get_String_Width(buffer) > max_width) {
                            cursor_count--;
                        }
                        buffer[len] = '\0';
                    }
                }
            } else if (key_code == 0x53E0) { // Delete
                if (len > 0 && cursor_count < len) {
                    for (int16_t i = cursor_count; i < len; i++) {
                        buffer[i] = buffer[i + 1];
                    }
                    len--;
                    cursor_position = 0;
                    buffer[len] = '\0';
                }
            } else {
                bool is_valid = false;
                if (key_code == 0x011B) { // Escape
                    done = 2;
                } else {
                    // Extract lower 8-bits while preserving absolute original sign logic
                    int16_t sign = key_code >> 15;
                    key_code = ((((key_code ^ sign) - sign) & 0xFF) ^ sign) - sign;

                    if ((key_code > 0x40 && key_code < 0x5D) ||
                        (key_code > 0x2D && key_code < 0x3B) ||
                        key_code == 0x20 ||
                        key_code == 0x2D ||
                        (key_code > 0x60 && key_code < 0x7B)) {
                        is_valid = true;
                    }

                    if (is_valid) {
                        is_editing_started = true;
                        char ch = static_cast<char>(key_code);
                        buffer[len] = ch;
                        buffer[len + 1] = '\0';
                        if (len < max_length && fonts::Get_String_Width(buffer) <= max_width) {
                            buffer[len] = '\0';
                            if (cursor_count < len) {
                                for (int16_t i = len; i > cursor_count; i--) {
                                    buffer[i] = buffer[i - 1];
                                }
                                len++;
                                cursor_position = 0;
                                buffer[cursor_count++] = ch;
                                buffer[len] = '\0';
                            } else {
                                buffer[len] = ch;
                                len++;
                                buffer[len] = ' ';
                                buffer[len + 1] = '\0';
                                if (len < max_length && fonts::Get_String_Width(buffer) <= max_width) {
                                    cursor_count++;
                                }
                                cursor_position = 0;
                                buffer[len] = '\0';
                            }
                        } else {
                            buffer[len] = '\0';
                        }
                    }
                }
            }

            Display_Input_Field(field_idx, buffer);
            if (key_code == 0x1C0D) {
                break;
            }
        }

        strlcpy(legacy_input->buffer, buffer, (size_t)legacy_input->max_length + 1);

        if (done != 0) {
            while (mouse::Mouse_Button() != 0) {
                platform::Check_Exit();
                // Wait for mouse release
            }
        }

        mouse::Restore_Mouse_State();
        cursor_count = 0;
        cursor_position = 0;
        down_mouse_button = -1;
    }

    void __cdecl Display_Input_Field(int16_t field_idx, char* input_buffer) {
        platform::Check_Exit();

        char cursor_color_ramp[16];
        char display_buffer[64];
        s_field_input_string* legacy_input = Field_Data_Legacy_Input(&fields[field_idx]);

        timer::Mark_Time();
        video::Set_Page_Off();

        strlcpy(cursor_color_ramp, legacy_input->cursor_color_ramp, sizeof(cursor_color_ramp));
        strlcpy(display_buffer, input_buffer, sizeof(display_buffer));

        Invoke_Auto_Function();

        fonts::Set_Font_Style(fields[field_idx].font_style, fields[field_idx].font_color);
        short font_height = fonts::Get_Font_Height() - 1;

        if (legacy_input->background_color != 0) {
            graphics::Fill(
                fields[field_idx].x - 1,
                fields[field_idx].y,
                fields[field_idx].x_end,
                fields[field_idx].y_end,
                static_cast<uint8_t>(legacy_input->background_color));
        }

        char char_str[2];
        char_str[1] = '\0';

        short input_mode = legacy_input->input_mode;

        if (input_mode == 0) {
            char_str[0] = display_buffer[cursor_count];
            display_buffer[cursor_count] = '\0';
            short prefix_width = fonts::Get_String_Width(display_buffer);
            short cursor_x = fields[field_idx].x + prefix_width;

            display_buffer[cursor_count] = char_str[0];
            if (char_str[0] == '\0') {
                char_str[0] = ' ';
            }
            short char_width = fonts::Get_String_Width(char_str);

            if (cursor_position <= 0 || font_height < cursor_position) {
                if (cursor_position != 0) {
                    short lines = font_height * 2 - cursor_position;
                    if (lines > 0) {
                        short color_idx = 0;
                        do {
                            short line_y = fields[field_idx].y + lines - 1;
                            line::Line(cursor_x, line_y, cursor_x + char_width + 1, line_y, (uint8_t)cursor_color_ramp[font_height - color_idx - 1]);
                            color_idx++;
                            lines--;
                        } while (lines != 0);
                    }
                }
            } else {
                short line_y_base = fields[field_idx].y + font_height;
                short lines = cursor_position;
                short color_idx = 0;
                do {
                    short line_y = line_y_base - lines + 1;
                    line::Line(cursor_x, line_y, cursor_x + char_width + 1, line_y, (uint8_t)cursor_color_ramp[color_idx]);
                    color_idx++;
                    lines--;
                } while (lines != 0);
            }
        } else if (input_mode == 1) {
            char_str[0] = display_buffer[cursor_count];
            display_buffer[cursor_count] = '\0';
            short prefix_width = fonts::Get_String_Width(display_buffer);
            short cursor_x = fields[field_idx].x + prefix_width;

            display_buffer[cursor_count] = char_str[0];
            if (char_str[0] == '\0') {
                char_str[0] = ' ';
            }
            short char_width = fonts::Get_String_Width(char_str);

            if ((cursor_position % 3) == 0) {
                if (cursor_on == 0) {
                    cursor_on = 1;
                } else if (cursor_on == 1) {
                    cursor_on = 0;
                }
            }

            if (cursor_on != 0) {
                short line_y = fields[field_idx].y + font_height + 1;
                line::Line(cursor_x, line_y, cursor_x + char_width + 1, line_y, (uint8_t)cursor_color_ramp[0]);
            }
        }

        fonts::Set_Font_Style(fields[field_idx].font_style, fields[field_idx].font_color);
        fonts::Print(fields[field_idx].x, fields[field_idx].y, display_buffer);
        palette::Refresh_Palette();
        platform::Toggle_Pages_Stub();
        timer::Release_Time(auto_function_delay);
    }
} // namespace fields