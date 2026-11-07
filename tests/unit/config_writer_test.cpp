#include "pch.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "config/config_writer.h"
#include "doctest/doctest.h"

namespace {

    bool Rewrite_(const char* source, const std::vector<std::string>& enabled,
        std::string* rewritten, config::s_cfg_diagnostic* diagnostic) {
        return config::Rewrite_Enabled_Mods("fixture.cfg", source, enabled, rewritten,
            diagnostic);
    }

    std::string Read_File(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(
            (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    }

    void Write_File(const std::filesystem::path& path, const std::string& content) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    struct Temp_Dir {
        std::filesystem::path path;
        Temp_Dir() {
            std::random_device device;
            std::ostringstream name;
            name << "orion2re-config-writer-" << std::hex << device() << device();
            path = std::filesystem::temp_directory_path() / name.str();
            std::filesystem::create_directories(path);
        }
        ~Temp_Dir() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };

    const char* kHandEditedConfig =
        "# Orion2 Redone configuration. Edit this file to customize your game.\n"
        "# Uncomment any of these examples to enable them:\n"
        "# interface scaler = hq3x;\n"
        "# enable v150i;\n"
        "\n"
        "include \"extra.cfg\";\n"
        "\n"
        "interface window_width = 640;\n"
        "interface window_height = 480;\n"
        "\n"
        "# Enabled mods, highest priority first.\n"
        "enable v150;\n"
        "enable ice;\n"
        "\n"
        "interface show_exit_message = false;\n";

    const char* kIncludedConfig =
        "# Extra settings pulled in by the main config.\n"
        "interface fullscreen = false;\n";

} // namespace

DOCTEST_TEST_SUITE("config writer") {

DOCTEST_TEST_CASE("round-trip changes only the enable lines") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    const std::filesystem::path included_path = dir.path / "extra.cfg";
    Write_File(included_path, kIncludedConfig);
    Write_File(config_path, kHandEditedConfig);

    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(config::Write_Enabled_Mods(
        config_path.string(), {"ice", "v150", "ice-x"}, &diagnostic));

    // The enable block is reordered (ice first), gains ice-x, and keeps the
    // original `enable v150;` line's spelling; everything else is identical.
    DOCTEST_CHECK(Read_File(config_path) ==
        "# Orion2 Redone configuration. Edit this file to customize your game.\n"
        "# Uncomment any of these examples to enable them:\n"
        "# interface scaler = hq3x;\n"
        "# enable v150i;\n"
        "\n"
        "include \"extra.cfg\";\n"
        "\n"
        "interface window_width = 640;\n"
        "interface window_height = 480;\n"
        "\n"
        "# Enabled mods, highest priority first.\n"
        "enable ice;\n"
        "enable v150;\n"
        "enable ice-x;\n"
        "\n"
        "interface show_exit_message = false;\n");
    DOCTEST_CHECK(Read_File(included_path) == kIncludedConfig);
}

DOCTEST_TEST_CASE("a symlinked config is written through, keeping the link") {
    Temp_Dir dir;
    const std::filesystem::path target_dir = dir.path / "real";
    std::filesystem::create_directories(target_dir);
    const std::filesystem::path target_path = target_dir / "orion2re.cfg";
    const std::filesystem::path link_path = dir.path / "orion2re.cfg";
    Write_File(target_path, "enable a;\n");
    std::error_code error;
    std::filesystem::create_symlink(target_path, link_path, error);
    DOCTEST_REQUIRE(!error);

    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(config::Write_Enabled_Mods(link_path.string(), {"b"}, &diagnostic));
    DOCTEST_CHECK(std::filesystem::is_symlink(link_path));
    DOCTEST_CHECK(Read_File(target_path) == "enable b;\n");
    DOCTEST_CHECK(Read_File(link_path) == "enable b;\n");

    // A dangling link cannot be resolved and is refused.
    const std::filesystem::path dangling_path = dir.path / "dangling.cfg";
    std::filesystem::create_symlink(dir.path / "missing.cfg", dangling_path, error);
    DOCTEST_REQUIRE(!error);
    DOCTEST_CHECK_FALSE(
        config::Write_Enabled_Mods(dangling_path.string(), {"b"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
}

DOCTEST_TEST_CASE("a symlinked config resolves includes beside the link, not the target") {
    Temp_Dir dir;
    const std::filesystem::path target_dir = dir.path / "real";
    const std::filesystem::path link_dir = dir.path / "link";
    std::filesystem::create_directories(target_dir);
    std::filesystem::create_directories(link_dir);
    const std::filesystem::path target_path = target_dir / "orion2re.cfg";
    const std::filesystem::path link_path = link_dir / "orion2re.cfg";
    // The config includes "extra.cfg". The loader resolves it lexically
    // beside the link, so an extra.cfg beside the link is the one that
    // takes effect -- even though the config file itself lives beside
    // the target.
    Write_File(target_path, "include \"extra.cfg\";\nenable a;\n");
    std::error_code error;
    std::filesystem::create_symlink(target_path, link_path, error);
    DOCTEST_REQUIRE(!error);

    // An extra.cfg beside the link that enables a mod must be refused:
    // the loader would see the included mod, so the written set would
    // not match the effective set.
    Write_File(link_dir / "extra.cfg", "enable sneaky;\n");
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(
        config::Write_Enabled_Mods(link_path.string(), {"b"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(Read_File(target_path) == "include \"extra.cfg\";\nenable a;\n");

    // A harmless extra.cfg beside the link (a setting, no enable) is
    // resolved and read, so the write succeeds -- proving the writer
    // looked beside the link. An extra.cfg beside the target only is
    // invisible to the loader and must not be consulted.
    Write_File(link_dir / "extra.cfg", "interface fullscreen = false;\n");
    Write_File(target_dir / "extra.cfg", "enable sneaky;\n");
    DOCTEST_REQUIRE(config::Write_Enabled_Mods(link_path.string(), {"b"}, &diagnostic));
    DOCTEST_CHECK(Read_File(target_path) == "include \"extra.cfg\";\nenable b;\n");
}

DOCTEST_TEST_CASE("desired order is the written order") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_(
        "enable a;\nenable b;\nenable c;\n", {"c", "a"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "enable c;\nenable a;\n");
}

DOCTEST_TEST_CASE("removing a mod drops its line without leaving a blank line") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_(
        "interface window_width = 640;\nenable v150;\nenable ice;\n"
        "interface show_exit_message = false;\n",
        {"ice"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten ==
        "interface window_width = 640;\nenable ice;\n"
        "interface show_exit_message = false;\n");
}

DOCTEST_TEST_CASE("an empty desired set removes all enable lines") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_(
        "# Mods.\nenable v150;\nenable ice;\n\ninterface fullscreen = false;\n", {},
        &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "# Mods.\n\ninterface fullscreen = false;\n");
}

DOCTEST_TEST_CASE("duplicate enable lines collapse to one") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_("enable ice;\nenable ice;\n", {"ice"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "enable ice;\n");
}

DOCTEST_TEST_CASE("enable lines sharing a line are edited, not the rest of the line") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_("enable a; enable b;\n", {"b"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "enable b; \n");
}

DOCTEST_TEST_CASE("removing every enable on a shared line drops the whole line") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_("enable a; enable b;\n", {}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "");
    DOCTEST_REQUIRE(Rewrite_("enable a; enable b;\ninterface x = 1;\n", {}, &rewritten,
        &diagnostic));
    DOCTEST_CHECK(rewritten == "interface x = 1;\n");
}

DOCTEST_TEST_CASE("appending to a file with no enable lines goes at the end") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    Write_File(config_path,
        "# No mods enabled yet.\n"
        "interface window_width = 640;\n");
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(config::Write_Enabled_Mods(
        config_path.string(), {"ice", "v150"}, &diagnostic));
    DOCTEST_CHECK(Read_File(config_path) ==
        "# No mods enabled yet.\n"
        "interface window_width = 640;\n"
        "enable ice;\n"
        "enable v150;\n");
}

DOCTEST_TEST_CASE("appending to an empty file and to a file without a trailing newline") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_("", {"ice"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "enable ice;\n");
    DOCTEST_REQUIRE(Rewrite_(
        "interface window_width = 640;", {"ice"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "interface window_width = 640;\nenable ice;\n");
}

DOCTEST_TEST_CASE("appended lines match the file's CRLF line endings") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_(
        "interface window_width = 640;\r\n", {"ice"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "interface window_width = 640;\r\nenable ice;\r\n");
}

DOCTEST_TEST_CASE("removing enable lines keeps CRLF endings and drops shared CRLF lines") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_(
        "enable a;\r\nenable b;\r\n", {"b"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "enable b;\r\n");
    DOCTEST_REQUIRE(Rewrite_("enable a; enable b;\r\n", {}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "");
}

DOCTEST_TEST_CASE("appended lines match the first line's endings, not a later line's") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_(
        "interface x = 1;\nenable a;\r\n", {"a", "b"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "interface x = 1;\nenable a;\r\nenable b;\n");
}

DOCTEST_TEST_CASE("a matching desired set does not rewrite the file") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    Write_File(config_path, "enable   ice;\n");
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(config::Write_Enabled_Mods(config_path.string(), {"ice"}, &diagnostic));
    // The hand-edited spacing survives: no rewrite happened at all.
    DOCTEST_CHECK(Read_File(config_path) == "enable   ice;\n");
}

DOCTEST_TEST_CASE("refuse: unparseable enable line leaves the file untouched") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    const std::string original =
        "interface window_width = 640;\n"
        "enable 123;\n"
        "enable ice;\n";
    Write_File(config_path, original);
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(config::Write_Enabled_Mods(config_path.string(), {"ice"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(diagnostic.location.line == 2);
    DOCTEST_CHECK(Read_File(config_path) == original);
}

DOCTEST_TEST_CASE("refuse: enable line with a comment inside it leaves the file untouched") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    const std::string original =
        "enable ice # trailing comment\n"
        ";\n";
    Write_File(config_path, original);
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(config::Write_Enabled_Mods(config_path.string(), {"ice"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(diagnostic.location.line == 1);
    DOCTEST_CHECK(Read_File(config_path) == original);
}

DOCTEST_TEST_CASE("refuse: unresolvable include leaves the file untouched") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    const std::string original =
        "include \"missing.cfg\";\n"
        "enable ice;\n";
    Write_File(config_path, original);
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(config::Write_Enabled_Mods(config_path.string(), {"ice"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(diagnostic.location.line == 1);
    DOCTEST_CHECK(Read_File(config_path) == original);
}

DOCTEST_TEST_CASE("refuse: included file that enables a mod leaves the file untouched") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    const std::filesystem::path included_path = dir.path / "extra.cfg";
    const std::string original =
        "include \"extra.cfg\";\n"
        "enable ice;\n";
    Write_File(included_path, "enable v150;\n");
    Write_File(config_path, original);
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(config::Write_Enabled_Mods(config_path.string(), {"ice"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(Read_File(config_path) == original);
    DOCTEST_CHECK(Read_File(included_path) == "enable v150;\n");
}

DOCTEST_TEST_CASE("refuse: include cycles leave the file untouched") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    const std::filesystem::path other_path = dir.path / "other.cfg";
    const std::string original =
        "include \"other.cfg\";\n"
        "enable ice;\n";
    Write_File(other_path, "include \"orion2re.cfg\";\ninterface x = 1;\n");
    Write_File(config_path, original);
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(config::Write_Enabled_Mods(config_path.string(), {"ice"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(Read_File(config_path) == original);

    // A file that includes itself is a cycle too.
    const std::filesystem::path self_path = dir.path / "self.cfg";
    const std::string self_original = "include \"self.cfg\";\nenable ice;\n";
    Write_File(self_path, self_original);
    DOCTEST_CHECK_FALSE(config::Write_Enabled_Mods(self_path.string(), {"ice"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(Read_File(self_path) == self_original);
}

DOCTEST_TEST_CASE("includes in subdirectories resolve relative to the including file") {
    Temp_Dir dir;
    const std::filesystem::path sub_dir = dir.path / "sub";
    std::filesystem::create_directories(sub_dir);
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    const std::filesystem::path extra_path = sub_dir / "extra.cfg";
    const std::filesystem::path nested_path = sub_dir / "nested.cfg";
    Write_File(nested_path, "interface x = 1;\n");
    Write_File(extra_path, "include \"nested.cfg\";\ninterface y = 2;\n");
    Write_File(config_path,
        "include \"sub/extra.cfg\";\n"
        "enable a;\n");
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(config::Write_Enabled_Mods(config_path.string(), {"b"}, &diagnostic));
    DOCTEST_CHECK(Read_File(config_path) ==
        "include \"sub/extra.cfg\";\n"
        "enable b;\n");

    // An enable in a subdirectory include is still refused.
    Write_File(nested_path, "enable v150;\n");
    Write_File(config_path,
        "include \"sub/extra.cfg\";\n"
        "enable a;\n");
    DOCTEST_CHECK_FALSE(config::Write_Enabled_Mods(config_path.string(), {"b"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(Read_File(config_path) ==
        "include \"sub/extra.cfg\";\n"
        "enable a;\n");
}

DOCTEST_TEST_CASE("include depth mirrors the loader: 63 nested includes pass, 64 refuse") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    auto write_chain = [&](size_t nested_count) {
        // The entry file includes f1, each fi includes f(i+1), and the
        // last file in the chain has no include.
        for (size_t index = 1; index <= nested_count; ++index) {
            const std::filesystem::path file_path =
                dir.path / ("f" + std::to_string(index) + ".cfg");
            if (index < nested_count) {
                Write_File(file_path,
                    "include \"f" + std::to_string(index + 1) + ".cfg\";\n");
            } else {
                Write_File(file_path, "interface x = 1;\n");
            }
        }
        Write_File(config_path, "include \"f1.cfg\";\nenable a;\n");
    };
    config::s_cfg_diagnostic diagnostic;
    write_chain(63);
    DOCTEST_REQUIRE(config::Write_Enabled_Mods(config_path.string(), {"a"}, &diagnostic));
    write_chain(64);
    DOCTEST_CHECK_FALSE(config::Write_Enabled_Mods(config_path.string(), {"a"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
}

DOCTEST_TEST_CASE("refuse: duplicate or invalid desired mod ID leaves the file untouched") {
    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    const std::string original = "enable ice;\n";
    Write_File(config_path, original);
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(
        config::Write_Enabled_Mods(config_path.string(), {"ice", "ice"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK_FALSE(
        config::Write_Enabled_Mods(config_path.string(), {"not an id"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(Read_File(config_path) == original);
}

DOCTEST_TEST_CASE("refuse: stop with no live enable lines cannot take appended lines") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(
        Rewrite_("interface window_width = 640;\nstop;\n", {"ice"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(diagnostic.location.line == 2);
}

DOCTEST_TEST_CASE("refuse: stop sharing the last enable line cannot take appended lines") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(
        Rewrite_("enable a; stop;\n", {"a", "b"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(diagnostic.location.line == 1);
    // No append needed: the same layout with an unchanged set still works.
    DOCTEST_REQUIRE(Rewrite_("enable a; stop;\n", {"a"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "enable a; stop;\n");

    Temp_Dir dir;
    const std::filesystem::path config_path = dir.path / "orion2re.cfg";
    const std::string original = "enable a; stop;\n";
    Write_File(config_path, original);
    DOCTEST_CHECK_FALSE(config::Write_Enabled_Mods(
        config_path.string(), {"a", "b"}, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
    DOCTEST_CHECK(diagnostic.location.line == 1);
    DOCTEST_CHECK(Read_File(config_path) == original);
}

DOCTEST_TEST_CASE("enable lines after stop are left alone") {
    std::string rewritten;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Rewrite_(
        "enable v150;\nstop;\nenable ice;\n", {"ice"}, &rewritten, &diagnostic));
    DOCTEST_CHECK(rewritten == "enable ice;\nstop;\nenable ice;\n");
}

} // DOCTEST_TEST_SUITE
