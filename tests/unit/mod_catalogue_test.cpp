#include "pch.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "config/mod_catalogue.h"
#include "doctest/doctest.h"
#include "miniz.h"

namespace {

    namespace fs = std::filesystem;

    // One scratch tree per test case, removed on destruction.
    struct s_catalogue_fixture {
        fs::path root;

        s_catalogue_fixture() {
            root = fs::temp_directory_path() / "orion2re-mod-catalogue-test";
            fs::remove_all(root);
            fs::create_directories(root);
        }

        ~s_catalogue_fixture() {
            std::error_code ec;
            fs::remove_all(root, ec);
        }

        fs::path Write_File(const fs::path& relative, const std::string& content) {
            const fs::path path = root / relative;
            fs::create_directories(path.parent_path());
            std::ofstream output(path, std::ios::binary);
            output << content;
            return path;
        }

        fs::path Write_Mod(const std::string& mods_dir, const std::string& id,
            const std::string& manifest, bool with_config = true) {
            const fs::path directory = root / mods_dir / id;
            fs::create_directories(directory);
            Write_File(fs::path(mods_dir) / id / "mod.json5", manifest);
            if (with_config) {
                Write_File(fs::path(mods_dir) / id / "config" / "main.cfg",
                    "interface scaler = pixelart;\n");
            }
            return directory;
        }

        void Write_Zip(const std::string& mods_dir, const std::string& id,
            const std::vector<std::pair<std::string, std::string>>& entries) {
            const fs::path full_path = root / mods_dir / (id + ".zip");
            fs::create_directories(full_path.parent_path());
            mz_zip_archive archive{};
            mz_zip_zero_struct(&archive);
            DOCTEST_REQUIRE(mz_zip_writer_init_file(&archive,
                full_path.string().c_str(), 0) == MZ_TRUE);
            for (const auto& entry : entries) {
                DOCTEST_REQUIRE(mz_zip_writer_add_mem(&archive,
                    entry.first.c_str(), entry.second.data(), entry.second.size(),
                    MZ_BEST_COMPRESSION) == MZ_TRUE);
            }
            DOCTEST_REQUIRE(mz_zip_writer_finalize_archive(&archive) == MZ_TRUE);
            DOCTEST_REQUIRE(mz_zip_writer_end(&archive) == MZ_TRUE);
        }

        void Write_Zip_Mod(const std::string& mods_dir, const std::string& id,
            const std::string& manifest, bool with_config = true) {
            std::vector<std::pair<std::string, std::string>> entries = {
                { "mod.json5", manifest },
            };
            if (with_config) {
                entries.push_back({ "config/main.cfg",
                    "interface scaler = pixelart;\n" });
            }
            Write_Zip(mods_dir, id, entries);
        }

        static std::string Manifest(const std::string& id,
            const std::string& version = "\"1.2.3\"",
            const std::string& extra = "") {
            return "{\n  id: \"" + id + "\",\n  name: \"" + id + " mod\",\n"
                "  description: \"Test mod " + id + ".\",\n"
                "  version: " + version + ",\n"
                "  requires: { engine: \">=2.0.0 <3.0.0\" },\n"
                "  config: \"config/main.cfg\",\n" + extra + "}\n";
        }
    };

} // namespace

DOCTEST_TEST_SUITE("mod catalogue") {

DOCTEST_TEST_CASE("lists a valid bundled mod with its fields and enabled state") {
    s_catalogue_fixture files;
    files.Write_Mod("bundled", "alpha", s_catalogue_fixture::Manifest("alpha"));
    files.Write_Mod("bundled", "beta", s_catalogue_fixture::Manifest("beta"));

    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue(
            { (files.root / "bundled").string() }, "", { "beta" });

    DOCTEST_REQUIRE(catalogue.size() == 2);
    // Sorted by id.
    DOCTEST_CHECK(catalogue[0].id == "alpha");
    DOCTEST_CHECK(catalogue[0].name == "alpha mod");
    DOCTEST_CHECK(catalogue[0].description == "Test mod alpha.");
    DOCTEST_CHECK(catalogue[0].version_text == "1.2.3");
    DOCTEST_CHECK(catalogue[0].engine_range == ">=2.0.0 <3.0.0");
    DOCTEST_CHECK(catalogue[0].source == config::MOD_SOURCE_BUNDLED);
    DOCTEST_CHECK_FALSE(catalogue[0].broken);
    DOCTEST_CHECK_FALSE(catalogue[0].enabled);
    DOCTEST_CHECK(catalogue[1].id == "beta");
    DOCTEST_CHECK(catalogue[1].enabled);
}

DOCTEST_TEST_CASE("a user mod with the same id replaces the bundled one") {
    s_catalogue_fixture files;
    files.Write_Mod("bundled", "alpha", s_catalogue_fixture::Manifest("alpha"));
    files.Write_Mod("user", "alpha",
        s_catalogue_fixture::Manifest("alpha", "\"9.9.9\""));

    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue({ (files.root / "bundled").string() },
            (files.root / "user").string(), {});

    DOCTEST_REQUIRE(catalogue.size() == 1);
    DOCTEST_CHECK(catalogue[0].id == "alpha");
    DOCTEST_CHECK(catalogue[0].version_text == "9.9.9");
    DOCTEST_CHECK(catalogue[0].source == config::MOD_SOURCE_USER);
    DOCTEST_CHECK_FALSE(catalogue[0].broken);
}

DOCTEST_TEST_CASE("a config-only mod with no data directory is valid") {
    s_catalogue_fixture files;
    files.Write_Mod("bundled", "alpha", s_catalogue_fixture::Manifest("alpha"));

    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue(
            { (files.root / "bundled").string() }, "", {});

    DOCTEST_REQUIRE(catalogue.size() == 1);
    DOCTEST_CHECK_FALSE(catalogue[0].broken);
}

DOCTEST_TEST_CASE("broken mods are listed unavailable with the diagnostic reason") {
    s_catalogue_fixture files;
    // No manifest at all.
    fs::create_directories(files.root / "bundled" / "missing");
    // Float version: a bare JSON number, not a string.
    files.Write_Mod("bundled", "floaty",
        s_catalogue_fixture::Manifest("floaty", "26.11"));
    // Not a semantic version.
    files.Write_Mod("bundled", "badver",
        s_catalogue_fixture::Manifest("badver", "\"banana\""));
    // Manifest id does not match the package directory.
    files.Write_Mod("bundled", "other",
        s_catalogue_fixture::Manifest("alpha"));
    // Engine range this engine does not satisfy.
    files.Write_Mod("bundled", "future",
        "{ id: \"future\", name: \"Future\", description: \"Too new.\",\n"
        "  version: \"1.0.0\",\n"
        "  requires: { engine: \">=99.0.0\" },\n"
        "  config: \"config/main.cfg\" }\n");
    // Manifest is valid but its config file is missing.
    files.Write_Mod("bundled", "noconfig",
        s_catalogue_fixture::Manifest("noconfig"), false);

    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue(
            { (files.root / "bundled").string() }, "", { "missing", "floaty" });

    DOCTEST_REQUIRE(catalogue.size() == 6);
    for (const config::s_mod_catalogue_entry& entry : catalogue) {
        DOCTEST_CHECK(entry.broken);
        DOCTEST_CHECK_FALSE(entry.reason.empty());
        // A broken mod can never be enabled, even when the enabled
        // set names it.
        DOCTEST_CHECK_FALSE(entry.enabled);
    }
    for (const config::s_mod_catalogue_entry& entry : catalogue) {
        if (entry.id == "missing") {
            DOCTEST_CHECK(entry.reason == "mod manifest not found");
        }
        if (entry.id == "noconfig") {
            DOCTEST_CHECK(entry.reason ==
                "referenced config file 'config/main.cfg' is missing");
        }
        if (entry.id == "other") {
            DOCTEST_CHECK(entry.reason.find("does not match package") !=
                std::string::npos);
        }
    }
}

DOCTEST_TEST_CASE("dependencies and conflicts come from the manifest") {
    s_catalogue_fixture files;
    files.Write_Mod("bundled", "alpha",
        s_catalogue_fixture::Manifest("alpha", "\"1.2.3\"",
            "  dependencies: [ { id: \"beta\", version: \"2.0.0\" } ],\n"
            "  conflicts: [\"gamma\"],\n"));

    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue(
            { (files.root / "bundled").string() }, "", {});

    DOCTEST_REQUIRE(catalogue.size() == 1);
    DOCTEST_REQUIRE(catalogue[0].dependencies.size() == 1);
    DOCTEST_CHECK(catalogue[0].dependencies[0].id == "beta");
    DOCTEST_CHECK(catalogue[0].dependencies[0].version == "2.0.0");
    DOCTEST_CHECK(catalogue[0].conflicts ==
        std::vector<std::string>({ "gamma" }));
}

DOCTEST_TEST_CASE("a ZIP-form mod is listed like a directory mod") {
    s_catalogue_fixture files;
    files.Write_Zip_Mod("bundled", "alpha", s_catalogue_fixture::Manifest("alpha"));

    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue(
            { (files.root / "bundled").string() }, "", { "alpha" });

    DOCTEST_REQUIRE(catalogue.size() == 1);
    DOCTEST_CHECK(catalogue[0].id == "alpha");
    DOCTEST_CHECK(catalogue[0].name == "alpha mod");
    DOCTEST_CHECK(catalogue[0].version_text == "1.2.3");
    DOCTEST_CHECK(catalogue[0].source == config::MOD_SOURCE_BUNDLED);
    DOCTEST_CHECK_FALSE(catalogue[0].broken);
    DOCTEST_CHECK(catalogue[0].enabled);
}

DOCTEST_TEST_CASE("a user ZIP replaces a bundled directory mod") {
    s_catalogue_fixture files;
    files.Write_Mod("bundled", "alpha", s_catalogue_fixture::Manifest("alpha"));
    files.Write_Zip_Mod("user", "alpha",
        s_catalogue_fixture::Manifest("alpha", "\"9.9.9\""));

    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue({ (files.root / "bundled").string() },
            (files.root / "user").string(), {});

    DOCTEST_REQUIRE(catalogue.size() == 1);
    DOCTEST_CHECK(catalogue[0].version_text == "9.9.9");
    DOCTEST_CHECK(catalogue[0].source == config::MOD_SOURCE_USER);
    DOCTEST_CHECK_FALSE(catalogue[0].broken);
}

DOCTEST_TEST_CASE("broken ZIP mods are listed unavailable with the reason") {
    s_catalogue_fixture files;
    // No manifest in the ZIP.
    files.Write_Zip("bundled", "nomanifest",
        { { "config/main.cfg", "interface scaler = pixelart;\n" } });
    // Manifest is valid but its config file is missing from the ZIP.
    files.Write_Zip_Mod("bundled", "noconfig",
        s_catalogue_fixture::Manifest("noconfig"), false);
    // Both forms in one root: the loader refuses the package.
    files.Write_Mod("bundled", "both", s_catalogue_fixture::Manifest("both"));
    files.Write_Zip_Mod("bundled", "both", s_catalogue_fixture::Manifest("both"));

    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue(
            { (files.root / "bundled").string() }, "", {});

    DOCTEST_REQUIRE(catalogue.size() == 3);
    for (const config::s_mod_catalogue_entry& entry : catalogue) {
        DOCTEST_CHECK(entry.broken);
        DOCTEST_CHECK_FALSE(entry.reason.empty());
        DOCTEST_CHECK_FALSE(entry.enabled);
    }
    for (const config::s_mod_catalogue_entry& entry : catalogue) {
        if (entry.id == "nomanifest") {
            DOCTEST_CHECK(entry.reason == "mod manifest not found");
        }
        if (entry.id == "noconfig") {
            DOCTEST_CHECK(entry.reason ==
                "referenced config file 'config/main.cfg' is missing");
        }
        if (entry.id == "both") {
            DOCTEST_CHECK(entry.reason ==
                "package 'both' has both directory and ZIP forms");
        }
    }
}

DOCTEST_TEST_CASE("missing roots give an empty catalogue") {
    s_catalogue_fixture files;
    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue(
            { (files.root / "no-such-dir").string() }, "", {});
    DOCTEST_CHECK(catalogue.empty());
}

DOCTEST_TEST_CASE("reading enable lines stops at stop and keeps file order") {
    s_catalogue_fixture files;
    const fs::path config = files.Write_File("orion2re.cfg",
        "# comment\n"
        "enable v150;\n"
        "interface scaler = pixelart;\n"
        "enable ice-x;\n"
        "# enable vdc-gm3;\n"
        "stop;\n"
        "enable melee;\n");

    std::vector<std::string> enabled;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(config::Read_Enabled_Mods(config.string(), &enabled, &diagnostic));
    DOCTEST_CHECK(enabled == std::vector<std::string>({ "v150", "ice-x" }));
}

DOCTEST_TEST_CASE("reading enable lines fails on a missing or unparseable file") {
    s_catalogue_fixture files;
    std::vector<std::string> enabled;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(config::Read_Enabled_Mods(
        (files.root / "no-such.cfg").string(), &enabled, &diagnostic));
    const fs::path broken = files.Write_File("broken.cfg", "enable ;\n");
    DOCTEST_CHECK_FALSE(config::Read_Enabled_Mods(
        broken.string(), &enabled, &diagnostic));
    DOCTEST_CHECK(!diagnostic.message.empty());
}

DOCTEST_TEST_CASE("disabling a mod removes its dependents and its orphaned dependencies") {
    s_catalogue_fixture files;
    const auto depends = [](const std::string& id) {
        return "  dependencies: [ { id: \"" + id + "\", version: \"1.2.3\" } ],\n";
    };
    // Chain alpha -> beta -> gamma, an independent delta, and a
    // diamond arm zeta -> gamma.
    files.Write_Mod("bundled", "alpha",
        s_catalogue_fixture::Manifest("alpha", "\"1.2.3\"", depends("beta")));
    files.Write_Mod("bundled", "beta",
        s_catalogue_fixture::Manifest("beta", "\"1.2.3\"", depends("gamma")));
    files.Write_Mod("bundled", "gamma",
        s_catalogue_fixture::Manifest("gamma"));
    files.Write_Mod("bundled", "delta",
        s_catalogue_fixture::Manifest("delta"));
    files.Write_Mod("bundled", "zeta",
        s_catalogue_fixture::Manifest("zeta", "\"1.2.3\"", depends("gamma")));

    const std::vector<std::string> full = { "alpha", "beta", "gamma", "delta" };
    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue(
            { (files.root / "bundled").string() }, "", full);
    DOCTEST_REQUIRE(catalogue.size() == 5);

    // Disabling the leaf takes the whole chain with it: beta is only
    // on because alpha needs it, gamma only because beta needs it.
    DOCTEST_CHECK(config::Prune_Disabled_Mod_Set(catalogue, full, "alpha") ==
        std::vector<std::string>({ "delta" }));
    // Disabling the middle takes the dependent above and the orphan below.
    DOCTEST_CHECK(config::Prune_Disabled_Mod_Set(catalogue, full, "beta") ==
        std::vector<std::string>({ "delta" }));
    // Disabling the root takes every dependent with it.
    DOCTEST_CHECK(config::Prune_Disabled_Mod_Set(catalogue, full, "gamma") ==
        std::vector<std::string>({ "delta" }));
    // An independent mod goes alone.
    DOCTEST_CHECK(config::Prune_Disabled_Mod_Set(catalogue, full, "delta") ==
        std::vector<std::string>({ "alpha", "beta", "gamma" }));
    // A dependency still needed by a remaining mod is kept: zeta still
    // needs gamma after alpha goes, and beta goes with alpha.
    DOCTEST_CHECK(config::Prune_Disabled_Mod_Set(catalogue,
        { "alpha", "beta", "gamma", "zeta" }, "alpha") ==
        std::vector<std::string>({ "gamma", "zeta" }));
    // Disabling an id that is not enabled changes nothing.
    DOCTEST_CHECK(config::Prune_Disabled_Mod_Set(catalogue, full, "zeta") == full);
    DOCTEST_CHECK(config::Prune_Disabled_Mod_Set(catalogue, full, "no-such-mod") == full);
}

DOCTEST_TEST_CASE("disabling a mod removes an explicitly enabled dependency nothing else needs") {
    s_catalogue_fixture files;
    files.Write_Mod("bundled", "beta",
        s_catalogue_fixture::Manifest("beta", "\"1.2.3\"",
            "  dependencies: [ { id: \"gamma\", version: \"1.2.3\" } ],\n"));
    files.Write_Mod("bundled", "gamma",
        s_catalogue_fixture::Manifest("gamma"));

    const std::vector<config::s_mod_catalogue_entry> catalogue =
        config::Build_Mod_Catalogue(
            { (files.root / "bundled").string() }, "", { "beta", "gamma" });

    // The enabled set holds the resolved set only: it cannot tell an
    // explicit enable of gamma from one pulled in by beta, so gamma
    // goes with beta when nothing remaining needs it.
    DOCTEST_CHECK(config::Prune_Disabled_Mod_Set(catalogue,
        { "beta", "gamma" }, "beta") == std::vector<std::string>({}));
}

} // DOCTEST_TEST_SUITE
