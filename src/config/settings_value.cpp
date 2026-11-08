#include "config/settings_value.h"

#include <algorithm>
#include <charconv>
#include <limits>

namespace config {
    namespace {

        bool Is_Primitive_Integer(e_value_type type) {
            return type == CONFIG_VALUE_INT8 || type == CONFIG_VALUE_UINT8 ||
                type == CONFIG_VALUE_INT16 || type == CONFIG_VALUE_UINT16 ||
                type == CONFIG_VALUE_INT32 || type == CONFIG_VALUE_UINT32;
        }

        void Storage_Range(e_value_type type, int64_t* minimum, int64_t* maximum) {
            switch (type) {
                case CONFIG_VALUE_INT8:
                    *minimum = std::numeric_limits<int8_t>::min();
                    *maximum = std::numeric_limits<int8_t>::max();
                    return;
                case CONFIG_VALUE_UINT8:
                    *minimum = 0;
                    *maximum = std::numeric_limits<uint8_t>::max();
                    return;
                case CONFIG_VALUE_INT16:
                    *minimum = std::numeric_limits<int16_t>::min();
                    *maximum = std::numeric_limits<int16_t>::max();
                    return;
                case CONFIG_VALUE_UINT16:
                    *minimum = 0;
                    *maximum = std::numeric_limits<uint16_t>::max();
                    return;
                case CONFIG_VALUE_INT32:
                    *minimum = std::numeric_limits<int32_t>::min();
                    *maximum = std::numeric_limits<int32_t>::max();
                    return;
                case CONFIG_VALUE_UINT32:
                    *minimum = 0;
                    *maximum = std::numeric_limits<uint32_t>::max();
                    return;
                default:
                    *minimum = 0;
                    *maximum = 0;
                    return;
            }
        }

        const s_symbol_descriptor* Find_Enum_Symbol(
            const s_type_descriptor* type, const std::string& name) {
            for (size_t index = 0; index < type->value_count; ++index) {
                if (name == type->values[index].name) {
                    return &type->values[index];
                }
            }
            return nullptr;
        }

        const s_symbol_descriptor* Find_Enum_Value(
            const s_type_descriptor* type, int64_t value) {
            for (size_t index = 0; index < type->value_count; ++index) {
                if (type->values[index].value == value) {
                    return &type->values[index];
                }
            }
            return nullptr;
        }

        // Quotes a string for the config file, escaping the characters
        // the parser treats specially inside a string.
        std::string Quote_Config_String(const std::string& value) {
            std::string quoted = "\"";
            for (char character : value) {
                switch (character) {
                    case '"': quoted += "\\\""; break;
                    case '\\': quoted += "\\\\"; break;
                    case '\b': quoted += "\\b"; break;
                    case '\f': quoted += "\\f"; break;
                    case '\n': quoted += "\\n"; break;
                    case '\r': quoted += "\\r"; break;
                    case '\t': quoted += "\\t"; break;
                    default: quoted += character; break;
                }
            }
            quoted += '"';
            return quoted;
        }

        bool Parse_Integer(const std::string& text, int64_t* value) {
            if (text.empty()) {
                return false;
            }
            const char* begin = text.data();
            const char* end = begin + text.size();
            const std::from_chars_result result = std::from_chars(begin, end, *value);
            return result.ec == std::errc() && result.ptr == end;
        }

        // Surrounding spaces and tabs carry no meaning for a number, an
        // enum name or a boolean, and the edit field can leave them behind;
        // strings keep their exact text instead.
        std::string Trim_Whitespace(const std::string& text) {
            size_t begin = 0;
            while (begin < text.size() &&
                (text[begin] == ' ' || text[begin] == '\t')) {
                ++begin;
            }
            size_t end = text.size();
            while (end > begin &&
                (text[end - 1] == ' ' || text[end - 1] == '\t')) {
                --end;
            }
            return text.substr(begin, end - begin);
        }

    } // namespace

    e_settings_widget Widget_For_Leaf_Type(const s_type_descriptor* type) {
        if (type == nullptr) {
            return SETTINGS_WIDGET_INFO;
        }
        switch (type->scalar_kind) {
            case CONFIG_SCALAR_ENUM:
                return type->value_count <= SETTINGS_MAX_DROPDOWN_VALUES
                    ? SETTINGS_WIDGET_DROPDOWN
                    : SETTINGS_WIDGET_INFO;
            case CONFIG_SCALAR_PRIMITIVE:
                break;
            default:
                return SETTINGS_WIDGET_INFO;
        }
        switch (type->value_type) {
            case CONFIG_VALUE_BOOL:
                return SETTINGS_WIDGET_TOGGLE;
            case CONFIG_VALUE_STRING:
                return SETTINGS_WIDGET_EDIT;
            default:
                return Is_Primitive_Integer(type->value_type)
                    ? SETTINGS_WIDGET_EDIT
                    : SETTINGS_WIDGET_INFO;
        }
    }

    std::string Format_Leaf_Value(t_variable_id variable_id,
        const s_typed_value& value) {
        const s_type_descriptor* type = Get_Variable_Type(variable_id);
        switch (value.type) {
            case CONFIG_VALUE_BOOL:
                return value.bool_value ? "true" : "false";
            case CONFIG_VALUE_STRING:
                return value.string_value;
            default:
                break;
        }
        if (!Is_Primitive_Integer(value.type)) {
            return std::to_string(value.integer_value);
        }
        if (type != nullptr && type->scalar_kind == CONFIG_SCALAR_ENUM) {
            const s_symbol_descriptor* symbol =
                Find_Enum_Value(type, value.integer_value);
            if (symbol != nullptr) {
                return symbol->name;
            }
        }
        return std::to_string(value.integer_value);
    }

    std::string Format_Leaf_Value_For_Config(t_variable_id variable_id,
        const s_typed_value& value) {
        if (value.type == CONFIG_VALUE_STRING) {
            return Quote_Config_String(value.string_value);
        }
        return Format_Leaf_Value(variable_id, value);
    }

    bool Parse_Leaf_Edit_Text(t_variable_id variable_id, const std::string& text,
        s_typed_value* value, std::string* config_text, std::string* error) {
        if (value == nullptr || config_text == nullptr || error == nullptr) {
            return false;
        }
        error->clear();
        const s_type_descriptor* type = Get_Variable_Type(variable_id);
        if (type == nullptr) {
            *error = "unknown setting";
            return false;
        }
        const std::string trimmed = Trim_Whitespace(text);
        if (type->scalar_kind == CONFIG_SCALAR_ENUM) {
            const s_symbol_descriptor* symbol = Find_Enum_Symbol(type, trimmed);
            if (symbol != nullptr) {
                value->type = type->value_type;
                value->integer_value = symbol->value;
                *config_text = trimmed;
                return true;
            }
            if (type->allow_numeric) {
                int64_t numeric = 0;
                if (Parse_Integer(trimmed, &numeric)) {
                    const s_symbol_descriptor* known =
                        Find_Enum_Value(type, numeric);
                    if (known != nullptr || !type->reject_unknown_numeric) {
                        value->type = type->value_type;
                        value->integer_value = numeric;
                        *config_text = trimmed;
                        return true;
                    }
                }
            }
            *error = "'" + text + "' is not one of this setting's values";
            return false;
        }
        if (type->scalar_kind != CONFIG_SCALAR_PRIMITIVE) {
            *error = "this setting cannot be edited as text";
            return false;
        }
        if (type->value_type == CONFIG_VALUE_BOOL) {
            if (trimmed == "true" || trimmed == "false") {
                value->type = CONFIG_VALUE_BOOL;
                value->bool_value = trimmed == "true";
                *config_text = trimmed;
                return true;
            }
            *error = "'" + text + "' is not 'true' or 'false'";
            return false;
        }
        if (type->value_type == CONFIG_VALUE_STRING) {
            if (type->has_max_length && text.size() > type->max_length) {
                *error = "value is longer than the maximum of " +
                    std::to_string(type->max_length) + " characters";
                return false;
            }
            value->type = CONFIG_VALUE_STRING;
            value->string_value = text;
            *config_text = Quote_Config_String(text);
            return true;
        }
        if (Is_Primitive_Integer(type->value_type)) {
            int64_t numeric = 0;
            if (!Parse_Integer(trimmed, &numeric)) {
                *error = "'" + text + "' is not a whole number";
                return false;
            }
            int64_t minimum = 0;
            int64_t maximum = 0;
            Storage_Range(type->value_type, &minimum, &maximum);
            if (type->has_min && type->min > minimum) {
                minimum = type->min;
            }
            if (type->has_max && type->max < maximum) {
                maximum = type->max;
            }
            if (numeric < minimum || numeric > maximum) {
                *error = "value must be between " + std::to_string(minimum) +
                    " and " + std::to_string(maximum);
                return false;
            }
            value->type = type->value_type;
            value->integer_value = numeric;
            *config_text = trimmed;
            return true;
        }
        *error = "this setting cannot be edited as text";
        return false;
    }

    std::vector<std::string> Split_Setting_Path(const std::string& path) {
        std::vector<std::string> segments;
        size_t begin = 0;
        while (begin <= path.size()) {
            const size_t end = path.find('.', begin);
            segments.push_back(path.substr(begin,
                end == std::string::npos ? std::string::npos : end - begin));
            if (end == std::string::npos) {
                break;
            }
            begin = end + 1;
        }
        return segments;
    }

    bool Get_Settings_Row_Value(const s_settings_row& row, s_typed_value* value) {
        if (row.variable_id == CONFIG_NO_VARIABLE || value == nullptr) {
            return false;
        }
        return generated::Get_Current_Value(row.variable_id, value);
    }

    namespace {

        struct s_settings_category_entry {
            const char* path;
            e_settings_category category;
        };

        // One entry per metadata setting (49), in metadata order.
        // Game internals: engine, display, debug and infrastructure.
        // Gameplay: user-facing in-game behaviour. Mods: the
        // mod-adjacent settings; the picker itself is not a setting.
        // Ambiguous calls, with reasons:
        // - interface.scroll_interval is combat camera pacing the
        //   player feels while fighting, so gameplay, not internals.
        // - interface.animation is presentation-only pacing (not the
        //   original dialog's Animations toggle, which lives in
        //   MOX::settings and is not a config setting), so internals.
        // - interface.print_research_value, build_list_colors and
        //   build_list_format are in-game displays, but colony and
        //   empire screens the player reads while playing, so gameplay.
        // - interface.loaded_config_prompt is a prompt shown when a
        //   save is loaded, so gameplay, not internals.
        // - interface.show_exit_message is a quit-time presentation
        //   toggle, so internals.
        // - interface.allow_lua_source_hash_mismatch only matters
        //   when mods are loaded, so mods.
        // - interface.main_menu_mod_list shows loaded mods, so mods.
        // - The Tech Review ordering tables and the colony build
        //   queues are info rows with no editor, but they configure
        //   in-game ordering, so gameplay.
        // - The original GAME SETTINGS dialog's options (End Of Turn
        //   Summary, Auto Save, Animations, ...) are not config
        //   settings at all: they live in MOX::settings
        //   (ORION2RE.SET / saves), so no row here covers them.
        inline const s_settings_category_entry settings_categories[] = {
            { "interface.router", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.scroll_interval", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.scaler", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.window_width", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.window_height", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.fullscreen", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.vsync_enabled", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.net_debug_enabled", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.debugger_enabled", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.debugger_font_size", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.debugger_ui_scale", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.allow_lua_source_hash_mismatch", SETTINGS_CATEGORY_MODS },
            { "interface.screen_fade_speed", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.animation", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.history_graph_smoothing", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.sort_serious_turn_summary_first", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.wormhole_link_color", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.tech_review_achievements", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.tech_review_colony", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.tech_review_weapons", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.tech_review_equipment", SETTINGS_CATEGORY_GAMEPLAY },
            { "colony.build_queue", SETTINGS_CATEGORY_GAMEPLAY },
            { "design.missile_default_x2_rack", SETTINGS_CATEGORY_GAMEPLAY },
            { "design.design_button_computer", SETTINGS_CATEGORY_GAMEPLAY },
            { "design.design_button_shield", SETTINGS_CATEGORY_GAMEPLAY },
            { "design.clear_button_computer", SETTINGS_CATEGORY_GAMEPLAY },
            { "design.clear_button_shield", SETTINGS_CATEGORY_GAMEPLAY },
            { "leaders.leader_dies_with_no", SETTINGS_CATEGORY_GAMEPLAY },
            { "research.finds_presented_by_scientist", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.print_research_value", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.warn_blockers", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.build_list_colors", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.build_list_format", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.no_outpost_prompt", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.initial_tech_prompt", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.rebel_display", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.loaded_config_prompt", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.main_menu_mod_list", SETTINGS_CATEGORY_MODS },
            { "interface.keep_end_of_turn_wait_in_multiplayer", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.allow_future_technology_browsing", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.confirm_battle_message_timeout", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.center_on_next_ship", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.death_spore_list_damage", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.bio_terminator_list_damage", SETTINGS_CATEGORY_GAMEPLAY },
            { "interface.main_map_overview_markers", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.maximum_galaxy_display", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.main_screen_tax_rate", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.maximum_galaxy_zoom_mode", SETTINGS_CATEGORY_GAME_INTERNALS },
            { "interface.show_exit_message", SETTINGS_CATEGORY_GAME_INTERNALS },
        };

    } // namespace

    e_settings_category Settings_Category_For_Path(const std::string& path) {
        for (const s_settings_category_entry& entry : settings_categories) {
            const std::string prefix = std::string(entry.path) + ".";
            if (path == entry.path || path.compare(0, prefix.size(), prefix) == 0) {
                return entry.category;
            }
        }
        return SETTINGS_CATEGORY_GAME_INTERNALS;
    }

    std::vector<s_settings_row> Build_Settings_Rows() {
        std::vector<s_settings_row> rows;
        for (size_t metadata_index = 0; metadata_index < settings_metadata_count;
            ++metadata_index) {
            const s_settings_metadata& metadata = settings_metadata[metadata_index];
            const std::string path = metadata.path;
            const std::string prefix = path + ".";

            std::vector<t_variable_id> variables;
            for (t_variable_id variable_id = 0; variable_id < variable_count;
                ++variable_id) {
                std::string name;
                if (!Get_Variable_Name(variable_id, &name)) {
                    continue;
                }
                if (name == path || name.compare(0, prefix.size(), prefix) == 0) {
                    variables.push_back(variable_id);
                }
            }

            s_settings_row info_row;
            info_row.variable_id = CONFIG_NO_VARIABLE;
            info_row.path = path;
            info_row.title = metadata.title;
            info_row.description = metadata.description;
            info_row.group = metadata.group;
            info_row.restart_required = metadata.restart_required;
            info_row.widget = SETTINGS_WIDGET_INFO;

            if (variables.empty()) {
                info_row.info =
                    "This setting is not present in the running build. Edit it in orion2re.cfg.";
                rows.push_back(std::move(info_row));
                continue;
            }

            // Per-colony state (the colony build queues) is not a global
            // setting: one row per colony would not be a settings screen.
            const bool per_colony =
                path.compare(0, 7, "colony.") == 0;
            bool renderable = !per_colony;
            bool oversized_enum = false;
            for (t_variable_id variable_id : variables) {
                const s_type_descriptor* type = Get_Variable_Type(variable_id);
                if (Widget_For_Leaf_Type(type) == SETTINGS_WIDGET_INFO) {
                    renderable = false;
                    if (type != nullptr &&
                        type->scalar_kind == CONFIG_SCALAR_ENUM &&
                        type->value_count > SETTINGS_MAX_DROPDOWN_VALUES) {
                        oversized_enum = true;
                    }
                }
            }
            if (!renderable) {
                if (per_colony) {
                    info_row.info =
                        "Per-colony state, not a global setting. Edit it in orion2re.cfg.";
                } else if (oversized_enum) {
                    info_row.info =
                        "Ordering table with hundreds of entries. Edit it in orion2re.cfg.";
                } else {
                    info_row.info =
                        "This setting has no editor on this screen. Edit it in orion2re.cfg.";
                }
                rows.push_back(std::move(info_row));
                continue;
            }

            const bool multi_leaf = variables.size() > 1;
            for (t_variable_id variable_id : variables) {
                std::string name;
                Get_Variable_Name(variable_id, &name);
                s_settings_row row;
                row.variable_id = variable_id;
                row.path = name;
                row.title = metadata.title;
                if (multi_leaf) {
                    // ASCII hyphen: the game font is byte-oriented with
                    // no em-dash glyph, so a UTF-8 separator garbles
                    // the row (and aborts the sanitizer build).
                    row.title += " - ";
                    row.title += name.substr(prefix.size());
                }
                row.description = metadata.description;
                row.group = metadata.group;
                row.restart_required = metadata.restart_required;
                row.widget = Widget_For_Leaf_Type(Get_Variable_Type(variable_id));
                rows.push_back(std::move(row));
            }
        }
        return rows;
    }

} // namespace config
