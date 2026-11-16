#include "pch.h"

#include "command_line.h"
#include "startup_options.h"
#include "doctest/doctest.h"

namespace {
    bool Parse_(const std::vector<std::string>& arguments, char prefix, startup::s_startup_options* options, std::string* error) {
        std::vector<const char*> argv = {"orion2re"};
        for (const std::string& argument : arguments) {
            argv.push_back(argument.c_str());
        }
        return startup::Parse_Startup_Options(static_cast<int>(argv.size()), argv.data(), options, error, prefix);
    }
} // namespace

DOCTEST_TEST_CASE("startup defaults do not select a config or save") {
    startup::s_startup_options options;
    std::string error;
    const char* argv[] = {"orion2re"};
    DOCTEST_REQUIRE(startup::Parse_Startup_Options(1, argv, &options, &error));
    DOCTEST_CHECK(options.config_path == "orion2re.cfg");
    DOCTEST_CHECK_FALSE(options.config_selected);
    DOCTEST_CHECK_FALSE(options.skip_intro);
    DOCTEST_CHECK(options.load_save == 0);
    DOCTEST_CHECK(options.game_directory.empty());
    DOCTEST_CHECK(error.empty());
#if defined(_WIN32)
    DOCTEST_CHECK(command_line::OPTION_PREFIX == '/');
#else
    DOCTEST_CHECK(command_line::OPTION_PREFIX == '-');
#endif
}

DOCTEST_TEST_CASE("startup switches accept native prefixes and preserve path arguments") {
    for (char prefix : {'/', '-'}) {
        const std::string p(1, prefix);
        startup::s_startup_options options;
        std::string error;
        DOCTEST_REQUIRE(Parse_({p + "config", "configs/with space.cfg", p + "skip-intro", p + "load-save", "10"}, prefix, &options, &error));
        DOCTEST_CHECK(options.config_path == "configs/with space.cfg");
        DOCTEST_CHECK(options.config_selected);
        DOCTEST_CHECK(options.skip_intro);
        DOCTEST_CHECK(options.load_save == 10);

        DOCTEST_REQUIRE(Parse_({p + "load-save=1", p + "config=configs/a=b.cfg"}, prefix, &options, &error));
        DOCTEST_CHECK(options.config_path == "configs/a=b.cfg");
        DOCTEST_CHECK(options.load_save == 1);
        DOCTEST_CHECK_FALSE(options.skip_intro);

        DOCTEST_REQUIRE(Parse_({}, prefix, &options, &error));
        DOCTEST_CHECK_FALSE(options.config_selected);
        DOCTEST_CHECK(options.load_save == 0);
    }
}

DOCTEST_TEST_CASE("startup rejects malformed unknown and repeated options") {
    for (char prefix : {'/', '-'}) {
        const std::string p(1, prefix);
        const std::vector<std::vector<std::string>> cases = {
            {p + "config"},
            {p + "config="},
            {p + "config", ""},
            {p + "config", p + "skip-intro"},
            {p + "load-save"},
            {p + "load-save", p + "skip-intro"},
            {p + "game-dir"},
            {p + "game-dir="},
            {p + "game-dir=a", p + "game-dir=b"},
            {p + "skip-intro=true"},
            {p + "skip-intro="},
            {p + "skip-intro", p + "skip-intro"},
            {p + "config=a.cfg", p + "config", "b.cfg"},
            {p + "load-save=1", p + "load-save=2"},
            {p + "profile", "classic"},
            {p + "no-exit-message"},
            {p + "skip-into"},
            {p + "config-extra=a.cfg"},
            {p},
            {"--skip-intro"},
            {""},
            {"positional.cfg"},
            {std::string(1, prefix == '/' ? '-' : '/') + "skip-intro"},
        };
        for (const auto& arguments : cases) {
            DOCTEST_CAPTURE(arguments);
            startup::s_startup_options options;
            std::string error;
            DOCTEST_CHECK_FALSE(Parse_(arguments, prefix, &options, &error));
            DOCTEST_CHECK_FALSE(error.empty());
        }
    }
}

DOCTEST_TEST_CASE("game-dir accepts platform prefixes and preserves directory paths") {
    for (char prefix : {'/', '-'}) {
        startup::s_startup_options options;
        std::string error;
        const std::string p(1, prefix);
        DOCTEST_REQUIRE(Parse_({p + "game-dir", "some directory/moo2"}, prefix, &options, &error));
        DOCTEST_CHECK(options.game_directory == "some directory/moo2");
        DOCTEST_REQUIRE(Parse_({p + "game-dir=/absolute/path"}, prefix, &options, &error));
        DOCTEST_CHECK(options.game_directory == "/absolute/path");
    }
}

DOCTEST_TEST_CASE("load-save requires a whole save number from one through ten") {
    for (char prefix : {'/', '-'}) {
        const std::string p(1, prefix);
        for (const char* value : {"0", "11", "-1", "+1", " 1", "1 ", "1x", "1.0", "999999999999999999999999", ""}) {
            DOCTEST_CAPTURE(value);
            startup::s_startup_options options;
            std::string error;
            DOCTEST_CHECK_FALSE(Parse_({p + "load-save=" + value}, prefix, &options, &error));
            DOCTEST_CHECK_FALSE(error.empty());
        }
        for (int slot = 1; slot <= 10; ++slot) {
            startup::s_startup_options options;
            std::string error;
            DOCTEST_REQUIRE(Parse_({p + "load-save=" + std::to_string(slot)}, prefix, &options, &error));
            DOCTEST_CHECK(options.load_save == slot);
        }
    }
}

DOCTEST_TEST_CASE("generic command-line definitions are independent of startup switches") {
    const command_line::s_option_definition definitions[] = {
        {"verbose", false},
        {"output", true},
    };
    const char* argv[] = {"tool", "/output", "a path", "/verbose"};
    std::vector<command_line::s_option> options;
    std::string error = "old error";
    DOCTEST_REQUIRE(command_line::Parse_Options(4, argv, '/', definitions, std::size(definitions), &options, &error));
    DOCTEST_REQUIRE(options.size() == 2);
    DOCTEST_CHECK(options[0].definition_index == 1);
    DOCTEST_CHECK(options[0].value == "a path");
    DOCTEST_CHECK(options[1].definition_index == 0);
    DOCTEST_CHECK(options[1].value.empty());
    DOCTEST_CHECK(error.empty());

    DOCTEST_REQUIRE(command_line::Parse_Options(1, argv, '/', definitions, std::size(definitions), &options, &error));
    DOCTEST_CHECK(options.empty());
    const char* invalid[] = {"tool", nullptr};
    DOCTEST_CHECK_FALSE(command_line::Parse_Options(2, invalid, '/', definitions, std::size(definitions), &options, &error));
    DOCTEST_CHECK_FALSE(error.empty());
    DOCTEST_CHECK_FALSE(command_line::Parse_Options(1, nullptr, '/', definitions, std::size(definitions), &options, &error));
}

DOCTEST_TEST_CASE("oracle switches are debug-only and validate conflicting modes and turn limits") {
    startup::s_startup_options options;
    std::string error;
#if defined(ORION2RE_DEBUG_BUILD)
    DOCTEST_REQUIRE(Parse_({"-oracle-record=.oracle/baseline", "-oracle-turns=12"}, '-', &options, &error));
    DOCTEST_CHECK(options.oracle_record == ".oracle/baseline");
    DOCTEST_CHECK(options.oracle_turns == 12);
    DOCTEST_REQUIRE(Parse_({"/oracle-replay=some path"}, '/', &options, &error));
    DOCTEST_CHECK(options.oracle_replay == "some path");
    for (const auto& arguments : std::vector<std::vector<std::string>>{
             {"-oracle-record=a", "-oracle-replay=b"}, {"-oracle-record=a", "-load-save=1"},
             {"-oracle-turns=1"}, {"-oracle-replay=a", "-oracle-turns=1"},
             {"-oracle-record=a", "-oracle-turns=0"}, {"-oracle-record=a", "-oracle-turns=100001"},
             {"-oracle-record=a", "-oracle-turns=1x"}, {"-oracle-record=a", "-oracle-turns=-1"}}) {
        DOCTEST_CHECK_FALSE(Parse_(arguments, '-', &options, &error));
        DOCTEST_CHECK_FALSE(error.empty());
    }
#else
    DOCTEST_CHECK_FALSE(Parse_({"-oracle-record=a"}, '-', &options, &error));
    DOCTEST_CHECK_FALSE(Parse_({"-oracle-replay=a"}, '-', &options, &error));
    DOCTEST_CHECK_FALSE(Parse_({"-oracle-turns=1"}, '-', &options, &error));
#endif
}
