#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "config/config_runtime.h"
#include "config/generated/settings_metadata.h"

namespace config {

    // Widget kinds for the settings screen, decided from the leaf's
    // runtime type descriptor — never per setting:
    // - booleans toggle;
    // - enums with a small value set open a selection list (dropdown);
    // - integers and strings take an edit field.
    // A leaf with any other type (a mask, bitmap, ratio, or an enum too
    // large for a selection list) has no widget and renders as an info
    // row instead; it is never silently dropped.
    enum e_settings_widget {
        SETTINGS_WIDGET_TOGGLE,
        SETTINGS_WIDGET_DROPDOWN,
        SETTINGS_WIDGET_EDIT,
        SETTINGS_WIDGET_INFO,
    };

    struct s_settings_row {
        // CONFIG_NO_VARIABLE for info rows, which name no variable.
        t_variable_id variable_id = CONFIG_NO_VARIABLE;
        // Dotted config path, for example "interface.window_width".
        std::string path;
        // Metadata title, with the leaf suffix appended for multi-leaf
        // settings (for example "Interface animation settings - cloak_steps normal").
        std::string title;
        std::string description;
        // "interface" or "synchronized_interface".
        std::string group;
        // True when the spec remarks say the change takes effect the next
        // time the game starts.
        bool restart_required = false;
        e_settings_widget widget = SETTINGS_WIDGET_INFO;
        // For info rows: why the setting has no editor here.
        std::string info;
    };

    // The Settings screen's three categories. Game internals are
    // engine, display, debug and infrastructure settings; gameplay
    // settings are user-facing in-game behaviour; mods is the mod
    // picker plus the two mod-adjacent settings (the main-menu mod
    // list and the Lua source-hash mismatch allowance for saves).
    // Every metadata path maps to exactly one category; a row's
    // category is its metadata setting's category.
    enum e_settings_category {
        SETTINGS_CATEGORY_GAME_INTERNALS,
        SETTINGS_CATEGORY_GAMEPLAY,
        SETTINGS_CATEGORY_MODS,
    };

    // Category of a settings row, from its dotted path: the path
    // equals its metadata setting's path or extends it (a leaf below
    // it, for example "interface.animation.cloak_steps.normal"). An
    // unrecognized path falls back to game internals; the unit test
    // pins all 49 metadata paths so the fallback is never reached for
    // a shipped setting.
    e_settings_category Settings_Category_For_Path(const std::string& path);

    // Primary settings are the ones a human commonly changes: toggles
    // and frequently-changed values. The rest are advanced or
    // ordering-table settings, shown only with Show all. A row's
    // visibility is its metadata setting's visibility, so multi-leaf
    // settings (animation leaves, build-list leaves) move as one.
    // An unrecognized path is primary; the unit test pins all 49
    // metadata paths so the fallback is never reached for a shipped
    // setting.
    bool Settings_Is_Primary_For_Path(const std::string& path);

    // All rows for the settings screen, in spec declaration order: one
    // row per editable leaf, plus one info row per setting that has no
    // editor (per-colony state, or an ordering table too large to edit
    // row by row). The titles and descriptions come from the generated
    // settings metadata; the widget kinds come from the runtime type
    // descriptors.
    std::vector<s_settings_row> Build_Settings_Rows();

    // Widget for one leaf type, or SETTINGS_WIDGET_INFO when the type
    // has no widget in the field system.
    e_settings_widget Widget_For_Leaf_Type(const s_type_descriptor* type);

    // Largest enum value set shown as a selection list. Above this the
    // list is unusable, so the setting renders as an info row instead.
    // The only enum above it among the settings is technology_application
    // (212 values, used by the Tech Review ordering tables).
    inline constexpr size_t SETTINGS_MAX_DROPDOWN_VALUES = 32;

    // Display text for a leaf value: enum name, "true"/"false", decimal,
    // or the raw string bytes (game encoding, as the field system edits).
    std::string Format_Leaf_Value(t_variable_id variable_id,
        const s_typed_value& value);

    // Config-file text for a leaf value: the same, with strings quoted
    // and escaped.
    std::string Format_Leaf_Value_For_Config(t_variable_id variable_id,
        const s_typed_value& value);

    // Validates edit-field text for a leaf and converts it to a typed
    // value plus its config-file text. On failure returns false and
    // fills `error` with the reason; `value` and `config_text` are
    // untouched. Integers are range-checked against the type descriptor
    // (and the storage range); strings against the maximum length;
    // enum names must match a symbol (numeric input only where the type
    // allows it).
    bool Parse_Leaf_Edit_Text(t_variable_id variable_id, const std::string& text,
        s_typed_value* value, std::string* config_text, std::string* error);

    // Splits a dotted config path into the tokens the settings writer
    // takes ("interface.animation.cloak_steps.normal" becomes
    // {"interface", "animation", "cloak_steps", "normal"}).
    std::vector<std::string> Split_Setting_Path(const std::string& path);

    // Current value of a row's leaf, or false when unavailable.
    bool Get_Settings_Row_Value(const s_settings_row& row, s_typed_value* value);

    // The window size is one choice, not two: the aspect ratio is
    // pinned to 4:3, so window_width and window_height are set together
    // from a single Resolution dropdown. The row is synthetic -- its
    // path names no config leaf, and its variable id is
    // CONFIG_NO_VARIABLE -- so it reads and writes through the two
    // settings below instead of the single-leaf path.
    inline constexpr const char* kResolutionRowPath = "interface.resolution";

    struct s_resolution_option {
        int width = 0;
        int height = 0;
        // "640x480".
        std::string label;
    };

    // Known 4:3 resolutions for the Resolution dropdown, up to near-4K,
    // plus the current pair (in size order) when it is not one of them,
    // so the dropdown always reflects the running window instead of
    // silently snapping an odd size to a listed one.
    std::vector<s_resolution_option> Resolution_Options(
        int current_width, int current_height);

    // Current window size from the active config, or false when either
    // setting is unreadable.
    bool Get_Current_Resolution(int* width, int* height);

    // "640x480" for a width/height pair.
    std::string Format_Resolution(int width, int height);

} // namespace config
