#include "pch.h"

#include <string>
#include <vector>

#include "config/settings_value.h"
#include "doctest/doctest.h"

namespace {

    const config::s_settings_row* Find_Row_(
        const std::vector<config::s_settings_row>& rows, const std::string& path) {
        for (const config::s_settings_row& row : rows) {
            if (row.path == path) {
                return &row;
            }
        }
        return nullptr;
    }

    bool Parse_(const std::string& path, const std::string& text,
        std::string* config_text, std::string* error) {
        config::t_variable_id variable_id = config::CONFIG_NO_VARIABLE;
        if (!config::Find_Variable(path.data(), path.size(), &variable_id)) {
            return false;
        }
        config::s_typed_value value;
        return config::Parse_Leaf_Edit_Text(variable_id, text, &value, config_text,
            error);
    }

    std::string Format_(const std::string& path, config::s_typed_value value) {
        config::t_variable_id variable_id = config::CONFIG_NO_VARIABLE;
        if (!config::Find_Variable(path.data(), path.size(), &variable_id)) {
            return "<unknown>";
        }
        return config::Format_Leaf_Value(variable_id, value);
    }

} // namespace

DOCTEST_TEST_SUITE("settings values") {

DOCTEST_TEST_CASE("every spec setting has rows, in spec order") {
    const std::vector<config::s_settings_row> rows = config::Build_Settings_Rows();

    // 49 spec parameters: 41 interface + 8 synchronized_interface.
    DOCTEST_REQUIRE(config::settings_metadata_count == 49);
    DOCTEST_CHECK(rows.size() == 208);

    // Metadata order is the row order: the first row is the first spec
    // parameter and the last row belongs to the last one.
    DOCTEST_REQUIRE(!rows.empty());
    DOCTEST_CHECK(rows.front().path == "interface.router");
    DOCTEST_CHECK(rows.back().path == "interface.show_exit_message");

    // Every metadata path contributes at least one row, and every row
    // carries its metadata title and group.
    for (size_t index = 0; index < config::settings_metadata_count; ++index) {
        const config::s_settings_metadata& metadata =
            config::settings_metadata[index];
        bool found = false;
        for (const config::s_settings_row& row : rows) {
            if (row.path == metadata.path ||
                row.path.compare(0, std::string(metadata.path).size() + 1,
                    std::string(metadata.path) + ".") == 0) {
                found = true;
                DOCTEST_CHECK(row.title.rfind(metadata.title, 0) == 0);
                DOCTEST_CHECK(row.group == metadata.group);
                DOCTEST_CHECK(row.description == metadata.description);
            }
        }
        DOCTEST_CHECK_MESSAGE(found, "no rows for ", metadata.path);
    }
}

DOCTEST_TEST_CASE("the restart-required set is the six spec settings") {
    std::vector<std::string> settings;
    for (size_t index = 0; index < config::settings_metadata_count; ++index) {
        const config::s_settings_metadata& metadata =
            config::settings_metadata[index];
        if (metadata.restart_required) {
            settings.push_back(metadata.path);
        }
    }
    DOCTEST_CHECK(settings ==
        std::vector<std::string>({
            "interface.window_width",
            "interface.window_height",
            "interface.fullscreen",
            "interface.debugger_enabled",
            "interface.debugger_font_size",
            "interface.debugger_ui_scale",
        }));
    // Every flagged row belongs to one of those six settings.
    const std::vector<config::s_settings_row> rows = config::Build_Settings_Rows();
    for (const config::s_settings_row& row : rows) {
        if (!row.restart_required) {
            continue;
        }
        bool owned = false;
        for (const std::string& setting : settings) {
            if (row.path == setting ||
                row.path.compare(0, setting.size() + 1, setting + ".") == 0) {
                owned = true;
            }
        }
        DOCTEST_CHECK_MESSAGE(owned, "flagged row with no restart setting: ",
            row.path);
    }
}

DOCTEST_TEST_CASE("widget kinds come from the leaf type") {
    const std::vector<config::s_settings_row> rows = config::Build_Settings_Rows();
    const config::s_settings_row* width = Find_Row_(rows, "interface.window_width");
    const config::s_settings_row* fullscreen =
        Find_Row_(rows, "interface.fullscreen");
    const config::s_settings_row* scaler = Find_Row_(rows, "interface.scaler");
    const config::s_settings_row* router = Find_Row_(rows, "interface.router");
    const config::s_settings_row* cloak_normal =
        Find_Row_(rows, "interface.animation.cloak_steps.normal");
    const config::s_settings_row* cloak_fast =
        Find_Row_(rows, "interface.animation.cloak_steps.fast");
    const config::s_settings_row* format_color =
        Find_Row_(rows, "interface.build_list_format.0.color");
    const config::s_settings_row* colors_base =
        Find_Row_(rows, "interface.build_list_colors.0.0.base");
    DOCTEST_REQUIRE(width != nullptr);
    DOCTEST_REQUIRE(fullscreen != nullptr);
    DOCTEST_REQUIRE(scaler != nullptr);
    DOCTEST_REQUIRE(router != nullptr);
    DOCTEST_REQUIRE(cloak_normal != nullptr);
    DOCTEST_REQUIRE(cloak_fast != nullptr);
    DOCTEST_REQUIRE(format_color != nullptr);
    DOCTEST_REQUIRE(colors_base != nullptr);
    DOCTEST_CHECK(width->widget == config::SETTINGS_WIDGET_EDIT);
    DOCTEST_CHECK(fullscreen->widget == config::SETTINGS_WIDGET_TOGGLE);
    DOCTEST_CHECK(scaler->widget == config::SETTINGS_WIDGET_DROPDOWN);
    DOCTEST_CHECK(router->widget == config::SETTINGS_WIDGET_EDIT);
    DOCTEST_CHECK(cloak_normal->widget == config::SETTINGS_WIDGET_EDIT);
    DOCTEST_CHECK(cloak_fast->widget == config::SETTINGS_WIDGET_EDIT);
    DOCTEST_CHECK(format_color->widget == config::SETTINGS_WIDGET_DROPDOWN);
    DOCTEST_CHECK(colors_base->widget == config::SETTINGS_WIDGET_EDIT);
    DOCTEST_CHECK(cloak_normal->title ==
        "Interface animation settings - cloak_steps.normal");
}

DOCTEST_TEST_CASE("row strings stay within the game font's byte range") {
    // The settings screen draws row strings with the game's
    // byte-oriented font, which has no UTF-8 decoding and no em-dash
    // glyph: a non-ASCII byte is drawn from a negative glyph index and
    // garbles the row (and aborts the sanitizer build). Every rendered
    // row string must be plain ASCII.
    const std::vector<config::s_settings_row> rows = config::Build_Settings_Rows();
    for (const config::s_settings_row& row : rows) {
        for (const std::string* text : {&row.title, &row.description, &row.info}) {
            for (const unsigned char character : *text) {
                DOCTEST_CHECK_MESSAGE(character < 0x80,
                    "non-ASCII byte in row '", row.path, "': ", row.title);
            }
        }
    }
}

DOCTEST_TEST_CASE("unrenderable settings become info rows, not dropped rows") {
    const std::vector<config::s_settings_row> rows = config::Build_Settings_Rows();
    size_t info_rows = 0;
    for (const config::s_settings_row& row : rows) {
        if (row.widget == config::SETTINGS_WIDGET_INFO) {
            ++info_rows;
            DOCTEST_CHECK(row.variable_id == config::CONFIG_NO_VARIABLE);
            DOCTEST_CHECK(!row.info.empty());
            DOCTEST_CHECK(row.info.find("orion2re.cfg") != std::string::npos);
        }
    }
    // Four Tech Review ordering tables plus the per-colony build queues.
    DOCTEST_CHECK(info_rows == 5);
    const config::s_settings_row* tech_review =
        Find_Row_(rows, "interface.tech_review_achievements");
    const config::s_settings_row* build_queue =
        Find_Row_(rows, "colony.build_queue");
    DOCTEST_REQUIRE(tech_review != nullptr);
    DOCTEST_REQUIRE(build_queue != nullptr);
    DOCTEST_CHECK(tech_review->widget == config::SETTINGS_WIDGET_INFO);
    DOCTEST_CHECK(build_queue->widget == config::SETTINGS_WIDGET_INFO);
    // No leaf rows leak out of the excluded tables.
    for (const config::s_settings_row& row : rows) {
        if (row.path.rfind("interface.tech_review_", 0) == 0) {
            DOCTEST_CHECK(row.widget == config::SETTINGS_WIDGET_INFO);
        }
        if (row.path.rfind("colony.build_queue", 0) == 0) {
            DOCTEST_CHECK(row.widget == config::SETTINGS_WIDGET_INFO);
        }
    }
}

DOCTEST_TEST_CASE("leaf counts match the spec shapes") {
    const std::vector<config::s_settings_row> rows = config::Build_Settings_Rows();
    auto count_prefix = [&rows](const std::string& prefix) {
        size_t count = 0;
        for (const config::s_settings_row& row : rows) {
            if (row.path == prefix ||
                row.path.compare(0, prefix.size() + 1, prefix + ".") == 0) {
                ++count;
            }
        }
        return count;
    };
    DOCTEST_CHECK(count_prefix("interface.animation") == 2);
    DOCTEST_CHECK(count_prefix("interface.build_list_colors") == 60);
    DOCTEST_CHECK(count_prefix("interface.build_list_format") == 100);
    DOCTEST_CHECK(count_prefix("interface.tech_review_achievements") == 1);
    DOCTEST_CHECK(count_prefix("colony.build_queue") == 1);
}

DOCTEST_TEST_CASE("formatting shows enum names, booleans and decimals") {
    config::s_typed_value value;
    value.type = config::CONFIG_VALUE_BOOL;
    value.bool_value = true;
    DOCTEST_CHECK(Format_("interface.fullscreen", value) == "true");
    value.bool_value = false;
    DOCTEST_CHECK(Format_("interface.fullscreen", value) == "false");

    value.type = config::CONFIG_VALUE_INT32;
    value.integer_value = 1280;
    DOCTEST_CHECK(Format_("interface.window_width", value) == "1280");

    value.type = config::CONFIG_VALUE_UINT8;
    value.integer_value = 0;
    DOCTEST_CHECK(Format_("interface.scaler", value) == "pixelart");
    value.integer_value = 3;
    DOCTEST_CHECK(Format_("interface.scaler", value) == "hq3x");

    value.type = config::CONFIG_VALUE_STRING;
    value.string_value = "example.com:47800";
    DOCTEST_CHECK(Format_("interface.router", value) == "example.com:47800");
    config::t_variable_id router_id = config::CONFIG_NO_VARIABLE;
    DOCTEST_REQUIRE(config::Find_Variable("interface.router",
        strlen("interface.router"), &router_id));
    DOCTEST_CHECK(config::Format_Leaf_Value_For_Config(router_id, value) ==
        "\"example.com:47800\"");
    value.string_value = "a\"b\\c";
    DOCTEST_CHECK(config::Format_Leaf_Value_For_Config(router_id, value) ==
        "\"a\\\"b\\\\c\"");
}

DOCTEST_TEST_CASE("edit text is validated against the leaf type") {
    std::string config_text;
    std::string error;
    DOCTEST_CHECK(Parse_("interface.window_width", "1280", &config_text, &error));
    DOCTEST_CHECK(config_text == "1280");
    DOCTEST_CHECK_FALSE(Parse_("interface.window_width", "0", &config_text, &error));
    DOCTEST_CHECK(!error.empty());
    DOCTEST_CHECK_FALSE(
        Parse_("interface.window_width", "16385", &config_text, &error));
    DOCTEST_CHECK_FALSE(Parse_("interface.window_width", "abc", &config_text, &error));
    DOCTEST_CHECK_FALSE(
        Parse_("interface.window_width", "12.5", &config_text, &error));
    DOCTEST_CHECK_FALSE(Parse_("interface.window_width", "", &config_text, &error));
    DOCTEST_CHECK_FALSE(Parse_("interface.window_width", "12 80", &config_text, &error));
    // Surrounding whitespace is trimmed for numbers, enum names and
    // booleans, but kept for strings.
    DOCTEST_CHECK(Parse_("interface.window_width", " 1280 ", &config_text, &error));
    DOCTEST_CHECK(config_text == "1280");

    DOCTEST_CHECK(Parse_("interface.fullscreen", "true", &config_text, &error));
    DOCTEST_CHECK_FALSE(Parse_("interface.fullscreen", "yes", &config_text, &error));
    DOCTEST_CHECK(Parse_("interface.fullscreen", " false ", &config_text, &error));
    DOCTEST_CHECK(config_text == "false");

    DOCTEST_CHECK(Parse_("interface.scaler", "hq3x", &config_text, &error));
    DOCTEST_CHECK(config_text == "hq3x");
    DOCTEST_CHECK_FALSE(Parse_("interface.scaler", "hq9x", &config_text, &error));
    DOCTEST_CHECK(Parse_("interface.scaler", " hq3x ", &config_text, &error));
    DOCTEST_CHECK(config_text == "hq3x");

    DOCTEST_CHECK(Parse_("interface.scroll_interval", "255", &config_text, &error));
    DOCTEST_CHECK_FALSE(
        Parse_("interface.scroll_interval", "-1", &config_text, &error));
    DOCTEST_CHECK_FALSE(
        Parse_("interface.scroll_interval", "256", &config_text, &error));

    DOCTEST_CHECK(Parse_("interface.router", "example.com", &config_text, &error));
    DOCTEST_CHECK(config_text == "\"example.com\"");
    DOCTEST_CHECK(Parse_("interface.router", " example.com ", &config_text, &error));
    DOCTEST_CHECK(config_text == "\" example.com \"");
    DOCTEST_CHECK_FALSE(Parse_("interface.router", std::string(70, 'a'),
        &config_text, &error));
    DOCTEST_CHECK(!error.empty());
}

DOCTEST_TEST_CASE("splitting a dotted path gives the writer tokens") {
    DOCTEST_CHECK(config::Split_Setting_Path("interface.window_width") ==
        std::vector<std::string>({"interface", "window_width"}));
    DOCTEST_CHECK(config::Split_Setting_Path(
        "interface.animation.cloak_steps.normal") ==
        std::vector<std::string>(
            {"interface", "animation", "cloak_steps", "normal"}));
    DOCTEST_CHECK(config::Split_Setting_Path(
        "interface.build_list_colors.default.normal.base") ==
        std::vector<std::string>({"interface", "build_list_colors", "default",
            "normal", "base"}));
}

DOCTEST_TEST_CASE("the current window width reads back from the native target") {
    const std::vector<config::s_settings_row> rows = config::Build_Settings_Rows();
    const config::s_settings_row* width = Find_Row_(rows, "interface.window_width");
    DOCTEST_REQUIRE(width != nullptr);
    config::s_typed_value value;
    DOCTEST_REQUIRE(config::Get_Settings_Row_Value(*width, &value));
    DOCTEST_CHECK(config::Format_Leaf_Value(width->variable_id, value) == "640");
}

} // DOCTEST_TEST_SUITE
