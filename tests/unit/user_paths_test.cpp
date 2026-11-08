#include "pch.h"

#include "doctest/doctest.h"
#include "game/user_paths.h"
#include "startup_directory.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
    namespace fs = std::filesystem;
    const char* const managed_variables[] = {
        "HOME",
        "XDG_CONFIG_HOME",
        "XDG_DATA_HOME",
        "XDG_STATE_HOME",
        "XDG_CACHE_HOME",
        "XDG_DATA_DIRS",
        "ORION2RE_XDG",
    };

    void Set_Variable_(const char* name, const std::string& value) {
        DOCTEST_REQUIRE(SDL_SetEnvironmentVariable(SDL_GetEnvironment(), name, value.c_str(), true));
    }

    void Unset_Variable_(const char* name) {
        SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), name);
    }

    struct s_user_environment {
        struct s_saved_variable {
            const char* name;
            bool present;
            std::string value;
        };
        fs::path root;
        std::vector<s_saved_variable> saved;

        s_user_environment() {
            root = fs::temp_directory_path() / ("orion2re-user-paths-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            fs::create_directories(root / "home");
            fs::create_directories(root / "install");
            for (const char* name : managed_variables) {
                const char* value = SDL_getenv(name);
                saved.push_back({name, value != nullptr, value != nullptr ? value : ""});
                Unset_Variable_(name);
            }
            Set_Variable_("HOME", Path("home"));
            Set_Variable_("XDG_DATA_DIRS", Path("system"));
            Set_Variable_("ORION2RE_XDG", "1");
        }

        ~s_user_environment() {
            user_paths::Shutdown();
            for (const s_saved_variable& variable : saved) {
                if (variable.present) {
                    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), variable.name, variable.value.c_str(), true);
                } else {
                    Unset_Variable_(variable.name);
                }
            }
            std::error_code error;
            fs::permissions(root / "install", fs::perms::owner_all, fs::perm_options::add, error);
            fs::remove_all(root, error);
        }

        std::string Path(const char* relative) const {
            return (root / relative).string();
        }

        void Write_File(const char* relative, const char* contents) const {
            const fs::path full_path = root / relative;
            fs::create_directories(full_path.parent_path());
            std::ofstream output(full_path, std::ios::binary);
            DOCTEST_REQUIRE(output.good());
            output << contents;
            DOCTEST_REQUIRE(output.good());
        }

        bool Init() const {
            return user_paths::Init((Path("install") + "/").c_str(), true);
        }
    };

    std::string Redirect_(const char* path, bool writing = false) {
        char output[1024] = {};
        return user_paths::Redirect_Legacy_Path(path, writing, output, sizeof(output)) ? output : "";
    }
} // namespace

DOCTEST_TEST_SUITE("user paths") {
    DOCTEST_TEST_CASE("non-XDG platforms and writable portable installations stay isolated") {
        s_user_environment environment;
        DOCTEST_CHECK_FALSE(user_paths::Init((environment.Path("install") + "/").c_str(), false));
        DOCTEST_CHECK_FALSE(user_paths::Is_Enabled());
        DOCTEST_CHECK(Redirect_("SAVE1.GAM", true).empty());

        Unset_Variable_("ORION2RE_XDG");
        DOCTEST_CHECK_FALSE(environment.Init());
        DOCTEST_CHECK_FALSE(fs::exists(environment.Path("home/.local/share/orion2re")));
        DOCTEST_CHECK(user_paths::Get_Game_Asset_Directories().empty());
    }

    DOCTEST_TEST_CASE("portable mode falls back to XDG game-asset directories for reading") {
        s_user_environment environment;
        Unset_Variable_("ORION2RE_XDG");
        DOCTEST_CHECK_FALSE(environment.Init());
        DOCTEST_CHECK_FALSE(user_paths::Is_Enabled());
        DOCTEST_CHECK(user_paths::Get_Game_Asset_Directories().empty());
        DOCTEST_CHECK_FALSE(fs::exists(environment.Path("home/.local/share/orion2re")));

        Set_Variable_("XDG_DATA_HOME", environment.Path("data") + "/");
        Set_Variable_("XDG_DATA_DIRS", environment.Path("first") + ":relative::" + environment.Path("second") + "/");
        const std::vector<std::string> paths = user_paths::Get_Fallback_Game_Asset_Directories(true);
        DOCTEST_REQUIRE(paths.size() == 4);
        DOCTEST_CHECK(paths[0] == environment.Path("data/orion2re/game/"));
        DOCTEST_CHECK(paths[1] == environment.Path("data/orion2re/"));
        DOCTEST_CHECK(paths[2] == environment.Path("first/orion2re/game/"));
        DOCTEST_CHECK(paths[3] == environment.Path("second/orion2re/game/"));
        // The fallback only reads: it creates no XDG directories and
        // redirects no legacy paths.
        DOCTEST_CHECK_FALSE(fs::exists(environment.Path("home/.local/share/orion2re")));
        DOCTEST_CHECK_FALSE(fs::exists(environment.Path("data/orion2re")));
        DOCTEST_CHECK(Redirect_("SAVE1.GAM", true).empty());
        DOCTEST_CHECK(user_paths::Get_Fallback_Game_Asset_Directories(false).empty());
    }

    DOCTEST_TEST_CASE("portable mode redirects read-only install files from the fallback directories") {
        s_user_environment environment;
        Unset_Variable_("ORION2RE_XDG");
        DOCTEST_CHECK_FALSE(environment.Init());
        DOCTEST_CHECK_FALSE(user_paths::Is_Enabled());
        environment.Write_File("home/.local/share/orion2re/game/orioncd.ini", "alternate path");
        DOCTEST_CHECK(Redirect_("ORIONCD.INI") == environment.Path("home/.local/share/orion2re/game/orioncd.ini"));
        // Writes and user files never take the fallback.
        DOCTEST_CHECK(Redirect_("ORIONCD.INI", true).empty());
        DOCTEST_CHECK(Redirect_("SAVE1.GAM").empty());
        DOCTEST_CHECK(Redirect_("SAVE1.GAM", true).empty());
    }

#if !defined(_WIN32)
    DOCTEST_TEST_CASE("XDG mode uses home defaults and private directories") {
        s_user_environment environment;
        DOCTEST_REQUIRE(environment.Init());
        const char* defaults[] = {"home/.config/orion2re/", "home/.local/share/orion2re/", "home/.local/state/orion2re/", "home/.cache/orion2re/"};
        for (int index = 0; index < user_paths::USER_DIRECTORY_COUNT; ++index) {
            const char* path = user_paths::Get_Directory(static_cast<user_paths::e_user_directory>(index));
            DOCTEST_CHECK(std::string(path) == environment.Path(defaults[index]));
            DOCTEST_CHECK(fs::is_directory(path));
#if !defined(_WIN32)
            struct stat info;
            DOCTEST_REQUIRE(stat(path, &info) == 0);
            DOCTEST_CHECK((info.st_mode & 0777) == 0700);
#endif
        }
    }

    DOCTEST_TEST_CASE("XDG paths accept absolute overrides and reject relative or empty values") {
        s_user_environment environment;
        Set_Variable_("XDG_CONFIG_HOME", environment.Path("config"));
        Set_Variable_("XDG_DATA_HOME", environment.Path("data") + "/");
        Set_Variable_("XDG_STATE_HOME", "relative/state");
        Set_Variable_("XDG_CACHE_HOME", "");
        Set_Variable_("XDG_DATA_DIRS", environment.Path("first") + ":relative::" + environment.Path("second") + "/");
        DOCTEST_REQUIRE(environment.Init());
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_CONFIG)) == environment.Path("config/orion2re/"));
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_DATA)) == environment.Path("data/orion2re/"));
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_STATE)) == environment.Path("home/.local/state/orion2re/"));
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_CACHE)) == environment.Path("home/.cache/orion2re/"));
        const auto& paths = user_paths::Get_Game_Asset_Directories();
        DOCTEST_REQUIRE(paths.size() == 4);
        DOCTEST_CHECK(paths[0] == environment.Path("data/orion2re/game/"));
        DOCTEST_CHECK(paths[1] == environment.Path("data/orion2re/"));
        DOCTEST_CHECK(paths[2] == environment.Path("first/orion2re/game/"));
        DOCTEST_CHECK(paths[3] == environment.Path("second/orion2re/game/"));
    }

    DOCTEST_TEST_CASE("environment override takes precedence over marker and clears all XDG state") {
        s_user_environment environment;
        DOCTEST_REQUIRE(environment.Init());
        Set_Variable_("ORION2RE_XDG", "");
        DOCTEST_CHECK_FALSE(environment.Init());
        environment.Write_File("install/xdg", "");
        DOCTEST_CHECK(environment.Init());
        Set_Variable_("ORION2RE_XDG", "0");
        DOCTEST_CHECK_FALSE(environment.Init());
        DOCTEST_CHECK_FALSE(user_paths::Is_Enabled());
        DOCTEST_CHECK(user_paths::Get_Game_Asset_Directories().empty());
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_DATA)).empty());
        DOCTEST_CHECK(Redirect_("SAVE1.GAM", true).empty());
        DOCTEST_CHECK(Redirect_("ORIONCD.INI").empty());
    }

#if !defined(_WIN32)
    DOCTEST_TEST_CASE("read-only executable directory selects XDG unless portable mode is forced") {
        s_user_environment environment;
        Unset_Variable_("ORION2RE_XDG");
        fs::permissions(environment.root / "install", fs::perms::owner_read | fs::perms::owner_exec);
        if (access(environment.Path("install").c_str(), W_OK) == 0) return;
        DOCTEST_CHECK(environment.Init());
        Set_Variable_("ORION2RE_XDG", "0");
        DOCTEST_CHECK_FALSE(environment.Init());
    }
#endif

    DOCTEST_TEST_CASE("legacy names map to the right directories and explicit paths are untouched") {
        s_user_environment environment;
        DOCTEST_REQUIRE(environment.Init());
        DOCTEST_CHECK(Redirect_("SAVE10.GAM", true) == environment.Path("home/.local/share/orion2re/SAVE10.GAM"));
        DOCTEST_CHECK(Redirect_("save?.gam") == environment.Path("home/.local/share/orion2re/save?.gam"));
        DOCTEST_CHECK(Redirect_("HOF.M2") == environment.Path("home/.local/share/orion2re/HOF.M2"));
        DOCTEST_CHECK(Redirect_("ORION2RE.RAC", true) == environment.Path("home/.local/share/orion2re/ORION2RE.RAC"));
        DOCTEST_CHECK(Redirect_("ORION2RE.SET", true) == environment.Path("home/.config/orion2re/ORION2RE.SET"));
        DOCTEST_CHECK(Redirect_("TEMP.TMP", true) == environment.Path("home/.cache/orion2re/TEMP.TMP"));
        DOCTEST_CHECK(Redirect_("ORION2.LBX").empty());
        DOCTEST_CHECK(Redirect_("saves/SAVE1.GAM", true).empty());
        DOCTEST_CHECK(Redirect_("C:SAVE1.GAM", true).empty());
        DOCTEST_CHECK(Redirect_("./language.ini", true).empty());

        char small[4] = {};
        DOCTEST_CHECK(user_paths::Redirect_Legacy_Path("SAVE1.GAM", true, small, sizeof(small)));
        DOCTEST_CHECK(small[0] == '\0');
        DOCTEST_CHECK(errno == ENAMETOOLONG);
    }

    DOCTEST_TEST_CASE("installation INI files use executable installed and XDG fallback directories") {
        s_user_environment environment;
        environment.Write_File("home/.local/share/orion2re/game/ORIONCD.INI", "game");
        environment.Write_File("home/.local/share/orion2re/ORIONCD.INI", "root");
        environment.Write_File("system/orion2re/game/LANGUAGE.INI", "1");
        DOCTEST_REQUIRE(environment.Init());
        DOCTEST_CHECK(strcasecmp(Redirect_("orioncd.ini").c_str(), environment.Path("home/.local/share/orion2re/game/ORIONCD.INI").c_str()) == 0);
        DOCTEST_CHECK(strcasecmp(Redirect_("language.ini").c_str(), environment.Path("system/orion2re/game/LANGUAGE.INI").c_str()) == 0);
        DOCTEST_CHECK(Redirect_("language.ini", true) == environment.Path("home/.config/orion2re/language.ini"));
        environment.Write_File("home/.config/orion2re/language.ini", "2");
        DOCTEST_CHECK(Redirect_("language.ini") == environment.Path("home/.config/orion2re/language.ini"));

        environment.Write_File("installed/ORIONCD.INI", "installed");
        DOCTEST_REQUIRE(user_paths::Init((environment.Path("install") + "/").c_str(), true, environment.Path("installed").c_str()));
        DOCTEST_CHECK(strcasecmp(Redirect_("orioncd.ini").c_str(), environment.Path("installed/ORIONCD.INI").c_str()) == 0);
        environment.Write_File("install/orioncd.ini", "executable");
        DOCTEST_CHECK(strcasecmp(Redirect_("ORIONCD.INI").c_str(), environment.Path("install/orioncd.ini").c_str()) == 0);
        DOCTEST_CHECK(Redirect_("ORIONCD.INI", true).empty());
    }

    DOCTEST_TEST_CASE("legacy helpers write list reopen timestamp and delete XDG saves") {
        s_user_environment environment;
        DOCTEST_REQUIRE(environment.Init());
        FILE* file = fopen_case("SAVE3.GAM", "wb");
        DOCTEST_REQUIRE(file != nullptr);
        fputs("save", file);
        fclose(file);
        DOCTEST_CHECK(File_Exists("save3.gam"));
        DOCTEST_CHECK(fs::is_regular_file(environment.Path("home/.local/share/orion2re/SAVE3.GAM")));
        file = fopen_case("save3.gam", "rb");
        DOCTEST_REQUIRE(file != nullptr);
        char contents[5] = {};
        DOCTEST_CHECK(fread(contents, 1, 4, file) == 4);
        DOCTEST_CHECK(std::string(contents) == "save");
        fclose(file);
        char found[64] = {};
        DOCTEST_REQUIRE(dos::DIR("save?.gam", found, sizeof(found)));
        DOCTEST_CHECK(std::string(found) == "SAVE3.GAM");
        DOCTEST_CHECK(dos::Time_Stamp(found) != nullptr);
        dos::Delete_File(found);
        DOCTEST_CHECK_FALSE(File_Exists("SAVE3.GAM"));
        DOCTEST_CHECK_FALSE(dos::DIR("save?.gam", found, sizeof(found)));
    }

    DOCTEST_TEST_CASE("runtime data never selects an XDG runtime tree regardless of mode") {
        s_user_environment environment;
        environment.Write_File("home/.local/share/orion2re/data/base.zip", "user runtime marker");
        environment.Write_File("installed/data/base.zip", "installed runtime marker");
        const std::string installed = environment.Path("installed");
        for (const char* mode : {"0", "1"}) {
            Set_Variable_("ORION2RE_XDG", mode);
            environment.Init();
            std::string directory;
            DOCTEST_REQUIRE(startup::Resolve_Data_Directory(environment.Path("install"), installed.c_str(), &directory));
            DOCTEST_CHECK(directory == installed);
            DOCTEST_REQUIRE(startup::Resolve_Data_Directory(environment.Path("install"), nullptr, &directory));
            DOCTEST_CHECK(directory == environment.Path("install"));
        }
    }
#endif
}
