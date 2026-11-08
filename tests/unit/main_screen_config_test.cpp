#include "pch.h"

#include "config/cfg_parser.h"
#include "config/config_decode.h"
#include "config/config_runtime.h"
#include "doctest/doctest.h"
#include "game/mainscr.h"

DOCTEST_TEST_CASE("main-screen display settings are optional local interface settings") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(MAINSCR::main_screen_tax_rate);
    DOCTEST_CHECK(MAINSCR::maximum_galaxy_display == MAINSCR::MAXIMUM_GALAXY_DISPLAY_OVERVIEW);

    const char source[] =
        "interface main_screen_tax_rate = true;"
        "interface maximum_galaxy_display = classic;";
    config::s_cfg_document document;
    config::s_cfg_diagnostic diagnostic;
    config::s_config_write_set writes;
    DOCTEST_REQUIRE(config::Parse_Cfg("main-screen-test", source, strlen(source), &document, &diagnostic));
    for (const auto& statement : document.statements) {
        config::s_config_write_set statement_writes;
        DOCTEST_REQUIRE(config::Decode_Assignment(&statement, &statement_writes, &diagnostic));
        writes.writes.insert(writes.writes.end(), statement_writes.writes.begin(), statement_writes.writes.end());
    }
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(MAINSCR::main_screen_tax_rate);
    DOCTEST_CHECK(MAINSCR::maximum_galaxy_display == MAINSCR::MAXIMUM_GALAXY_DISPLAY_CLASSIC);
    for (const auto& write : writes.writes) {
        const auto* group = config::Get_Config_Group(config::Get_Variable_Group(write.variable_id));
        DOCTEST_REQUIRE(group != nullptr);
        DOCTEST_CHECK(std::string(group->id) == "interface");
        DOCTEST_CHECK_FALSE(group->save);
        DOCTEST_CHECK_FALSE(group->synchronize);
        DOCTEST_CHECK_FALSE(config::Snapshot_Contains_Variable(
            &config::saved_config_snapshot, write.variable_id));
        DOCTEST_CHECK_FALSE(config::Snapshot_Contains_Variable(
            &config::synchronized_config_snapshot, write.variable_id));
    }

    const char overview_source[] = "interface maximum_galaxy_display = overview;";
    DOCTEST_REQUIRE(config::Parse_Cfg("main-screen-test", overview_source, strlen(overview_source), &document, &diagnostic));
    DOCTEST_REQUIRE(document.statements.size() == 1);
    writes.writes.clear();
    DOCTEST_REQUIRE(config::Decode_Assignment(&document.statements[0], &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(MAINSCR::maximum_galaxy_display == MAINSCR::MAXIMUM_GALAXY_DISPLAY_OVERVIEW);

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(MAINSCR::main_screen_tax_rate);
    DOCTEST_CHECK(MAINSCR::maximum_galaxy_display == MAINSCR::MAXIMUM_GALAXY_DISPLAY_OVERVIEW);
}

DOCTEST_TEST_CASE("maximum galaxy display rejects values other than named modes") {
    for (const char* value : {"true", "false", "0", "1", "automatic"}) {
        DOCTEST_CAPTURE(value);
        const std::string source = std::string("interface maximum_galaxy_display = ") + value + ";";
        config::s_cfg_document document;
        config::s_cfg_diagnostic diagnostic;
        config::s_config_write_set writes;
        DOCTEST_REQUIRE(config::Parse_Cfg("main-screen-test", source.data(), source.size(), &document, &diagnostic));
        DOCTEST_REQUIRE(document.statements.size() == 1);
        DOCTEST_CHECK_FALSE(config::Decode_Assignment(&document.statements[0], &writes, &diagnostic));
    }
}