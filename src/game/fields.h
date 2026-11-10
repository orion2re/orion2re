#pragma once

struct s_animation_header;
struct s_colors;

#pragma pack(push, 1)

typedef struct s_field s_field, *Ps_field;
typedef union s_field_detail s_field_detail, *Ps_field_detail;
typedef struct s_field_button s_field_button, *Ps_field_button;
typedef struct s_field_locked s_field_locked, *Ps_field_locked;
typedef struct s_field_grid s_field_grid, *Ps_field_grid;
typedef struct s_field_radio s_field_radio, *Ps_field_radio;
typedef struct s_field_scroll s_field_scroll, *Ps_field_scroll;
typedef struct s_field_cont_input s_field_cont_input, *Ps_field_cont_input;
typedef struct s_field_input_string s_field_input_string, *Ps_field_input_string;
typedef struct s_field_multi_button s_field_multi_button, *Ps_field_multi_button;
typedef struct s_field_string_list s_field_string_list, *Ps_field_string_list;
typedef struct s_field_circle s_field_circle, *Ps_field_circle;
typedef struct s_field_multi_hotkey s_field_multi_hotkey, *Ps_field_multi_hotkey;
typedef struct s_field_irregular_button s_field_irregular_button, *Ps_field_irregular_button;
typedef struct s_field_hidden s_field_hidden, *Ps_field_hidden;

struct s_field_grid {
    int16_t step_x;
    int16_t step_y;
    int16_t* grid_x_ptr;
    int16_t* grid_y_ptr;
};

struct s_field_circle {
    int16_t center_x;
    int16_t center_y;
    int32_t radius_squared;
};

struct s_field_scroll {
    int16_t return_value;
    int16_t set_value;
    int16_t range_min;
    int16_t range_max;
    int16_t is_vertical;
    int16_t* value;
};

struct s_field_multi_button {
    const char* help;
    struct s_animation_header* pic;
    int16_t draw_style;
    int16_t* selection;
    int16_t button_value;
};

struct s_field_button {
    const char* help;
    struct s_animation_header* pic;
    int16_t draw_style;
};

struct s_field_irregular_button {
    const char* help;
    struct s_animation_header* pic;
    int16_t draw_style;
    int16_t draw_x;
    int16_t draw_y;
};

struct s_field_locked {
    const char* help;
    struct s_animation_header* pic;
    int16_t draw_style;
    int16_t* selection;
};

struct s_field_input_string {
    struct s_colors* highlight_color;
    char* buffer;
    int16_t background_color;
    int16_t draw_style;
    int16_t input_mode;
    int16_t max_length;
    char* cursor_color_ramp;
};

struct s_field_string_list {
    struct s_colors* highlight_color;
    const char* help;
    int8_t* value;
    int16_t help_start;
    int16_t is_selectable;
    struct s_colors* gradient_colors;
    int16_t gradient_mode;
    uint8_t* gradient_palette;
    int32_t gradient_extra;
    struct s_animation_header* gradient_pic;
};

struct s_field_radio {
    const char* help;
    struct s_animation_header* pic;
    int16_t draw_style;
    int16_t* selection;
};

struct s_field_cont_input {
    struct s_colors* highlight_color;
    int16_t max_length;
    char* buffer;
    int16_t full_store_flag;
    struct s_animation_header* pic;
    int16_t background_color;
    int16_t font_shadow_style;
};

struct s_field_multi_hotkey {
    const char* help;
    int16_t help_start;
    int16_t help_len;
};

struct s_field_hidden {
    uint8_t unused;
};

union s_field_detail {
    struct s_field_button button;
    struct s_field_irregular_button irregular_button;
    struct s_field_grid grid;
    struct s_field_radio radio;
    struct s_field_locked locked;
    struct s_field_scroll scroll;
    struct s_field_input_string input;
    struct s_field_cont_input cont_input;
    struct s_field_multi_button multibtn;
    struct s_field_string_list strings;
    struct s_field_circle circle;
    struct s_field_multi_hotkey multi_hotkey;
    struct s_field_hidden hidden;
};

struct s_field {
    int16_t x;
    int16_t y;
    int16_t x_end;
    int16_t y_end;
    int16_t field_type;
    int16_t font_style;
    struct s_colors* font_color;
    union s_field_detail detail;
    char hotkey;
    int16_t sound;
};

#pragma pack(pop)

namespace fields {
    inline constexpr int32_t NON_ENTITY_FIELDS_COUNT = 250;
    inline constexpr int32_t MAX_ENTITY_FIELDS_COUNT =
        MAX_SHIPS > MAX_COLONIES
        ? (MAX_SHIPS > MAX_STARS ? MAX_SHIPS : MAX_STARS)
        : (MAX_COLONIES > MAX_STARS ? MAX_COLONIES : MAX_STARS);
    inline constexpr int32_t MAX_FIELDS_COUNT =
        NON_ENTITY_FIELDS_COUNT + MAX_ENTITY_FIELDS_COUNT;
    static_assert(MAX_FIELDS_COUNT <= INT16_MAX, "field indices do not fit in int16_t");

    extern MOX2::action_func_t update_function;
    extern uint16_t auto_active_flag;
    extern uint16_t list_field_on;
    extern int16_t auto_function_delay;
    extern uint16_t list_field_scan_flag;
    extern int16_t down_mouse_button;
    extern int16_t auto_input_variable;
    extern uint16_t input_field_active;
    extern int16_t active_input_field_number;
    extern int16_t cursor_count;
    extern int16_t cursor_position;
    extern int16_t cursor_on;
    extern int16_t help_list_active;
    extern int16_t help_list_count;
    extern int16_t multi_hotkey_active_field;
    extern uint16_t draw_fields_flag;
    extern char s__00551100[1];
    extern char* invalid_field_string_ptr;
    extern int32_t max_fields_count;
    extern uint8_t exit_the_list_field;
    extern s_field* fields;
    extern s_field* old_fields;
    extern int16_t pointer_offset;
    extern uint16_t old_input_field_active;
    extern int16_t old_active_input_field_number;
    extern int16_t old_down_mouse_button;
    extern int16_t old_fields_count;
    extern int16_t old_input_delay;
    extern int16_t old_auto_input_variable;
    extern int16_t fields_count;
    extern int16_t selected_count;
    extern int16_t mouse_saved_center_y;
    extern int16_t mouse_saved_center_x;
    extern uint16_t pressed_mouse_x_value;
    extern uint16_t pressed_mouse_y_value;
    extern int16_t pressed_button_flag;
    extern int16_t last_button_number;
    extern int16_t last_button_x;
    extern int16_t last_button_y;
    extern s_colors list_normal_color;
    extern s_colors list_highlight_colors;
    extern s_colors list_special_colors;
    extern int16_t input_delay;
    extern MOX2::action_func_t function;
    extern MOX2::action_func_t function2;
    extern s_help_box* help_struct_pointer;
    extern char continuous_string[256];
    extern uint8_t active_input_field_first;
    extern uint16_t field_outline_color;
    extern uint16_t help_field_outline_color;
    extern int16_t selected_button_text_x_offset;
    extern int16_t selected_button_text_y_offset;
    extern uint16_t mouse_cancel_disabled;
    extern uint16_t dynamic_field_refresh;
    extern uint16_t field_box_mode;
    extern uint16_t help_box_mode;
    extern int16_t mouse_auto_exit;
    extern uint32_t draw_override_flag;

    [[noreturn]] inline void __cdecl Field_Type_Mismatch(const char* helper_name, int16_t actual_type) {
        char message[96];
        snprintf(message, sizeof(message), "%s type mismatch (%d)", helper_name, (int)actual_type);
#if !defined(NDEBUG)
        SDL_TriggerBreakpoint();
#endif
        exitm::Exit_With_Message(message);
        abort();
    }

    inline s_field_button* __cdecl Field_Data_Button(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_BUTTON) {
            Field_Type_Mismatch("Field_Data_Button", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.button;
    }

    inline const s_field_button* __cdecl Field_Data_Button(const s_field* field) {
        return Field_Data_Button(const_cast<s_field*>(field));
    }

    inline s_field_irregular_button* __cdecl Field_Data_Irregular_Button(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_IRREGULAR_BUTTON) {
            Field_Type_Mismatch("Field_Data_Irregular_Button", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.irregular_button;
    }

    inline const s_field_irregular_button* __cdecl Field_Data_Irregular_Button(const s_field* field) {
        return Field_Data_Irregular_Button(const_cast<s_field*>(field));
    }

    inline s_field_radio* __cdecl Field_Data_Radio(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_RADIO) {
            Field_Type_Mismatch("Field_Data_Radio", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.radio;
    }

    inline const s_field_radio* __cdecl Field_Data_Radio(const s_field* field) {
        return Field_Data_Radio(const_cast<s_field*>(field));
    }

    inline s_field_locked* __cdecl Field_Data_Locked_Button(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_LOCKED) {
            Field_Type_Mismatch("Field_Data_Locked_Button", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.locked;
    }

    inline const s_field_locked* __cdecl Field_Data_Locked_Button(const s_field* field) {
        return Field_Data_Locked_Button(const_cast<s_field*>(field));
    }

    inline s_field_multi_button* __cdecl Field_Data_Multi_Button(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_MULTI_BUTTON) {
            Field_Type_Mismatch("Field_Data_Multi_Button", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.multibtn;
    }

    inline const s_field_multi_button* __cdecl Field_Data_Multi_Button(const s_field* field) {
        return Field_Data_Multi_Button(const_cast<s_field*>(field));
    }

    inline s_field_scroll* __cdecl Field_Data_Scroll(s_field* field) {
        if (field == nullptr || (field->field_type != FIELD_TYPE_SCROLL && field->field_type != FIELD_TYPE_SCROLL_RETURN)) {
            Field_Type_Mismatch("Field_Data_Scroll", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.scroll;
    }

    inline const s_field_scroll* __cdecl Field_Data_Scroll(const s_field* field) {
        return Field_Data_Scroll(const_cast<s_field*>(field));
    }

    inline s_field_input_string* __cdecl Field_Data_Legacy_Input(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_LEGACY_INPUT) {
            Field_Type_Mismatch("Field_Data_Legacy_Input", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.input;
    }

    inline const s_field_input_string* __cdecl Field_Data_Legacy_Input(const s_field* field) {
        return Field_Data_Legacy_Input(const_cast<s_field*>(field));
    }

    inline s_field_cont_input* __cdecl Field_Data_Continuous_Input(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_CONTINUOUS_INPUT) {
            Field_Type_Mismatch("Field_Data_Continuous_Input", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.cont_input;
    }

    inline const s_field_cont_input* __cdecl Field_Data_Continuous_Input(const s_field* field) {
        return Field_Data_Continuous_Input(const_cast<s_field*>(field));
    }

    inline s_field_string_list* __cdecl Field_Data_String_List(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_STRING_LIST) {
            Field_Type_Mismatch("Field_Data_String_List", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.strings;
    }

    inline const s_field_string_list* __cdecl Field_Data_String_List(const s_field* field) {
        return Field_Data_String_List(const_cast<s_field*>(field));
    }

    inline s_field_grid* __cdecl Field_Data_Grid(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_GRID) {
            Field_Type_Mismatch("Field_Data_Grid", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.grid;
    }

    inline const s_field_grid* __cdecl Field_Data_Grid(const s_field* field) {
        return Field_Data_Grid(const_cast<s_field*>(field));
    }

    inline s_field_circle* __cdecl Field_Data_Circle(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_CIRCLE) {
            Field_Type_Mismatch("Field_Data_Circle", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.circle;
    }

    inline const s_field_circle* __cdecl Field_Data_Circle(const s_field* field) {
        return Field_Data_Circle(const_cast<s_field*>(field));
    }

    inline s_field_multi_hotkey* __cdecl Field_Data_Multi_Hot_Key(s_field* field) {
        if (field == nullptr || field->field_type != FIELD_TYPE_MULTI_HOTKEY) {
            Field_Type_Mismatch("Field_Data_Multi_Hot_Key", field != nullptr ? field->field_type : -1);
        }

        return &field->detail.multi_hotkey;
    }

    inline const s_field_multi_hotkey* __cdecl Field_Data_Multi_Hot_Key(const s_field* field) {
        return Field_Data_Multi_Hot_Key(const_cast<s_field*>(field));
    }

    void __cdecl Assign_Update_Function(MOX2::action_func_t fn);
    void __cdecl Save_Auto_Function();
    void __cdecl Deactivate_Auto_Function();
    void __cdecl Activate_Auto_Function();
    void __cdecl Restore_Auto_Function();
    void __cdecl Clear_Fields();

    int16_t __cdecl Add_Hot_Key(const char* hotkey_str);

    void __cdecl Assign_Auto_Function(MOX2::action_func_t fn, int16_t delay);

    int16_t __cdecl Get_Input();

    void __cdecl Enable_Mouse_Wait();
    void __cdecl Disable_Mouse_Wait();

    int16_t __cdecl Interpret_Mouse_Input();

    int16_t __cdecl Scan_Input();

    int16_t __cdecl Add_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, const char* text, int8_t key);

    int16_t __cdecl Add_Clipped_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, const char* hotkey, int8_t sound);

    int16_t __cdecl Add_Hidden_Field(int16_t x, int16_t y, int16_t x_end, int16_t y_end, const char* hotkey, int8_t sound);

    int16_t __cdecl Add_Clipped_Hidden_Field(int16_t x, int16_t y, int16_t x_end, int16_t y_end, const char* hotkey, int8_t sound);

    // Type 13 irregular buttons have their own payload with explicit sprite draw
    // coordinates.
    int16_t __cdecl Add_Irregular_Button_Field(int16_t x, int16_t y, int16_t x_end, int16_t y_end, const char* help, s_animation_header* pic, int16_t draw_x, int16_t draw_y, const char* hotkey, int8_t sound);

    int16_t __cdecl Add_Multi_Hot_Key_Field(const char* help_text);
    int16_t __cdecl Check_Multi_Hot_Key_Fields(int16_t hotkey);

    int16_t __cdecl Auto_Input();

    uint16_t __cdecl Pressed_Mouse_X();

    uint16_t __cdecl Pressed_Mouse_Y();

    /* Adds a grid field that converts mouse position into grid coordinates.
     * grid_x: Receives the selected column.
     * grid_y: Receives the selected row.
     */
    int16_t __cdecl Add_Grid_Field(int16_t x, int16_t y, int16_t step_x, int16_t step_y, int16_t cols, int16_t rows, int16_t* grid_x, int16_t* grid_y, int8_t sound);

    /* Adds a scroll field that updates a bound value within the supplied range.
     * value: Pointer to the value controlled by the scroll field.
     * hotkey_str: Optional hotkey string; only the first character is used.
     * The related FIELD_TYPE_SCROLL_RETURN action reuses the same detail.scroll payload.
     */
    int16_t __cdecl Add_Scroll_Field(int16_t x, int16_t y, int16_t range_min, int16_t range_max, int16_t return_value, int16_t set_value, int16_t width, int16_t height, int16_t* value, const char* hotkey_str, int8_t sound);

    // Type 11 is the newer continuous editor used by screen text boxes.
    // It uses detail.cont_input and is distinct from the older type 4 inline editor.
    int16_t __cdecl Add_Continuous_String_Input_Field(int16_t x, int16_t y, int16_t width, int16_t height, char* value, int16_t max_length, int16_t background_color, s_animation_header* pic, int8_t sound, int16_t font_shadow_style);

    // Type 3 multi-buttons share one selection variable across several button fields.
    // The payload stores the shared selection pointer plus this field's selected value.
    int16_t __cdecl Add_Multi_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, int16_t* value, int16_t button_value, const char* hotkey, int8_t sound);

    // Type 14 clipped circle fields use detail.circle for hit testing.
    int16_t __cdecl Add_Clipped_Circle_Field(int16_t center_x, int16_t center_y, int16_t radius, const char* hotkey, int8_t sound);

    int16_t __cdecl Add_Radio_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, int16_t* variable, const char* hotkey, int8_t sound);

    void __cdecl Disable_Draw_Field(int16_t field_idx);
    void __cdecl Enable_Draw_Field(int16_t field_idx);

    void __cdecl Disable_Draw_All_Fields();
    void __cdecl Enable_Draw_All_Fields();

    void __cdecl Disable_Cancel();

    /* Truncates the field list above the supplied field and clears pending mouse input.
     * field_idx: Highest field index to keep.
     */
    void __cdecl Clear_Fields_Above(int16_t field_idx);

    int16_t __cdecl Get_Field_Counter();

    void __cdecl Enable_Mouse_Pressed_Field_Update();
    void __cdecl Disable_Mouse_Pressed_Field_Update();

    void __cdecl Deactivate_Help_List();

    /* Registers the active help-box array used for right-click help lookup.
     * help_data: Pointer to the help-box records.
     * count: Number of help-box records.
     */
    void __cdecl Set_Help_List(s_help_box* help_data, int16_t count);

    void __cdecl Invoke_Auto_Function();
    void __cdecl Invoke_Saved_Auto_Function();

    void __cdecl F11_Key();

    void __cdecl Push_Field_Down(int16_t field_idx, int16_t x, int16_t y);

    void __cdecl Copy_Continuous_String();

    int16_t __cdecl Interpret_Keyboard_Input(int16_t* field_idx);

    void __cdecl Increment_Scroll_Bar(int16_t field_idx);
    void __cdecl Decrement_Scroll_Bar(int16_t field_idx);

    /// Moves a scroll field's value by whole wheel notches. Positive steps raise
    /// the value (wheel up / right), negative steps lower it. One notch moves
    /// max(1, range / 10) units, clamped to [return_value, set_value].
    void __cdecl Step_Scroll_Bar(int16_t field_idx, int16_t steps);

    /// Moves the selection of the modal list containing the given string-list
    /// field by whole wheel notches, skipping rows that are not selectable.
    /// Positive steps move toward earlier items (wheel up), negative steps
    /// toward later items (wheel down). Returns nonzero when the selection moved.
    int16_t __cdecl Move_List_Selection(int16_t hovered_field_idx, int16_t steps);

    /// Consumes any pending mouse wheel delta and applies it to the field under
    /// the cursor (a scrollbar), the lone scrollbar on this screen, or the open
    /// modal list's selection. Returns the scrolled field index (> 0), 0 when no
    /// wheel delta is pending, or -1 when a delta was consumed without a scroll
    /// return (list selection moved, or no scrollable target on this screen).
    int16_t __cdecl Handle_Mouse_Wheel();

    /// Returns nonzero while a wheel move holds the modal list selection against
    /// the hover scan (the hover wins again once the mouse moves to another row).
    int16_t __cdecl Mouse_Wheel_List_Selection_Held();

    /// Clears the wheel-held list selection (called when the fields are torn down).
    void __cdecl Clear_Mouse_Wheel_List_Selection_Hold();

    // Type 4 is the older inline input field renderer/editor.
    // It uses detail.input, including input_mode and a cursor color ramp.
    void __cdecl Display_Input_Field(int16_t field_idx, char* input_buffer);

    void __cdecl Input_String(int16_t field_idx);

    void __cdecl Find_Bar_Position(int16_t field_idx);

    /* Checks whether the mouse is inside any registered help box and draws that help entry.
     * Returns 0 when a help entry was shown, or 1 when no help box matched.
     */
    int16_t __cdecl Check_Help_List();

    void __cdecl Clear_Multi_Hot_Key_Offsets();

    uint8_t __cdecl Get_Alt_Alpha_Key(int16_t keycode);

    void __cdecl Draw_Field(int16_t field_idx, int16_t action);

    void __cdecl Call_Auto_Function();

    int16_t __cdecl Scan_Field();

    void __cdecl Clear_Mouse_Buffer();

    void __cdecl Quick_Call_Auto_Function();

    void __cdecl Timed_Wait_For_Input(uint32_t timeout);

    void __cdecl Draw_Visible_Fields();

    void __cdecl Draw_All_Fields();
    void __cdecl Set_Input_Delay(int16_t delay);
    void __cdecl Set_Field_Outline_Color(uint16_t color);

    void __cdecl Draw_Visible_Fields_Back();

    int16_t __cdecl Add_Locked_Button_Field(int16_t x, int16_t y, const char* help, s_animation_header* pic, int16_t* selection, const char* hotkey, int8_t sound);

    // Type 10 string list rows store a per-row selectability flag at detail+0x2.
    int16_t __cdecl Add_String_List_Field(int16_t x, int16_t y, int16_t width, const char* help, int16_t is_selectable, int8_t* value, int16_t help_start, int16_t gradient_mode, uint8_t* gradient_palette, int32_t gradient_extra, s_animation_header* gradient_pic, int32_t unused_param_12, const char* hotkey_str, const char* unused_param_14);

    void __cdecl Mark_Fields();

    void __cdecl Clipped_Dot(int16_t x, int16_t y, uint8_t color);

    void __cdecl Exit_List_Field();

    void __cdecl Release_Fields();

    /* Builds a temporary selectable list from fixed-size records and returns the chosen row.
     * list_ptr: Pointer to the first list record.
     * item_size: Byte stride between records.
     * selected_index: In/out selected row index.
     */
    int16_t __cdecl Get_List_Field(int16_t x, int16_t y, int16_t width, const char* help, const char* list_ptr, int32_t item_size, int8_t* selected_index, uint8_t* item_flags, int16_t gradient_mode, uint8_t* gradient_palette, int32_t gradient_extra, s_animation_header* gradient_pic, int32_t unused_param_13, const char* unused_param_14);
} // namespace fields