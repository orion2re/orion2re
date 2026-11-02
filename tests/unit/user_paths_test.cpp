#include "pch.h"

#include "doctest/doctest.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "game/user_paths.h"

namespace test_user_paths_detail {
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
        SDL_SetEnvironmentVariable(SDL_GetEnvironment(), name, value.c_str(), true);
    }

    void Unset_Variable_(const char* name) {
        SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), name);
    }

    // Owns a temporary home directory and restores the environment afterwards.
    class Scoped_User_Environment {
      public:
        Scoped_User_Environment() {
            const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
            root_path_ = fs::temp_directory_path() / ("orion2re-user-paths-" + std::to_string(unique_id));
            fs::create_directories(root_path_ / "home");
            fs::create_directories(root_path_ / "install");

            for (const char* name : managed_variables) {
                const char* value = SDL_getenv(name);
                saved_.push_back({name, value != nullptr, value != nullptr ? value : ""});
                Unset_Variable_(name);
            }
            Set_Variable_("HOME", (root_path_ / "home").string());
            Set_Variable_("XDG_DATA_DIRS", (root_path_ / "system").string());
            Set_Variable_("ORION2RE_XDG", "1");
        }

        ~Scoped_User_Environment() {
            user_paths::Shutdown();
            std::error_code permissions_error;
            fs::permissions(root_path_ / "install", fs::perms::owner_all, fs::perm_options::add, permissions_error);
            for (const s_saved_variable& variable : saved_) {
                if (variable.present) {
                    Set_Variable_(variable.name, variable.value);
                } else {
                    Unset_Variable_(variable.name);
                }
            }
            std::error_code error;
            fs::remove_all(root_path_, error);
        }

        [[nodiscard]] const fs::path& root_path() const {
            return root_path_;
        }

        [[nodiscard]] std::string Path(const fs::path& relative_path) const {
            return (root_path_ / relative_path).string();
        }

        [[nodiscard]] std::string Install_Directory() const {
            return (root_path_ / "install").string() + "/";
        }

        void Write_File(const fs::path& relative_path, const std::string& contents) const {
            const fs::path full_path = root_path_ / relative_path;
            fs::create_directories(full_path.parent_path());
            std::ofstream output(full_path, std::ios::binary);
            DOCTEST_REQUIRE(output.good());
            output.write(contents.data(), (std::streamsize)contents.size());
            DOCTEST_REQUIRE(output.good());
        }

        bool Init() const {
            return user_paths::Init(Install_Directory().c_str(), true);
        }

      private:
        struct s_saved_variable {
            const char* name;
            bool present;
            std::string value;
        };

        fs::path root_path_;
        std::vector<s_saved_variable> saved_;
    };

    std::string Redirect_(const char* path, bool writing) {
        char output[1024] = {};
        if (!user_paths::Redirect_Legacy_Path(path, writing, output, sizeof(output))) {
            return {};
        }
        return output;
    }

    // Case-insensitive file systems report the requested spelling rather than
    // the spelling on disk.
    bool Same_Path_(const std::string& actual, const std::string& expected) {
        return strcasecmp(actual.c_str(), expected.c_str()) == 0;
    }

        std::string Read_File_(FILE* file) {
        std::string contents;
        char buffer[64];
        size_t count;
        while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0) {
            contents.append(buffer, count);
        }
        return contents;
    }
} // namespace test_user_paths_detail

DOCTEST_TEST_SUITE("user paths") {
    DOCTEST_TEST_CASE("user paths stay disabled on platforms without XDG") {
        test_user_paths_detail::Scoped_User_Environment environment;

        DOCTEST_CHECK_FALSE(user_paths::Init(environment.Install_Directory().c_str(), false));
        DOCTEST_CHECK_FALSE(user_paths::Is_Enabled());
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_DATA)).empty());
        DOCTEST_CHECK(user_paths::Get_Game_Asset_Directories().empty());
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("SAVE1.GAM", true).empty());
    }

    DOCTEST_TEST_CASE("user paths default to the home directory") {
        test_user_paths_detail::Scoped_User_Environment environment;

        DOCTEST_REQUIRE(environment.Init());
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_CONFIG)) == environment.Path("home/.config/orion2re/"));
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_DATA)) == environment.Path("home/.local/share/orion2re/"));
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_STATE)) == environment.Path("home/.local/state/orion2re/"));
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_CACHE)) == environment.Path("home/.cache/orion2re/"));

        for (int directory = 0; directory < user_paths::USER_DIRECTORY_COUNT; ++directory) {
            const char* path = user_paths::Get_Directory((user_paths::e_user_directory)directory);
            DOCTEST_CHECK(test_user_paths_detail::fs::is_directory(path));
#if !defined(_WIN32)
            struct stat info;
            DOCTEST_REQUIRE(stat(path, &info) == 0);
            DOCTEST_CHECK((info.st_mode & 0777) == 0700);
#endif
        }
    }

    DOCTEST_TEST_CASE("user paths honor absolute XDG variables and ignore relative ones") {
        test_user_paths_detail::Scoped_User_Environment environment;
        test_user_paths_detail::Set_Variable_("XDG_CONFIG_HOME", environment.Path("xdg-config"));
        test_user_paths_detail::Set_Variable_("XDG_DATA_HOME", environment.Path("xdg-data") + "/");
        test_user_paths_detail::Set_Variable_("XDG_STATE_HOME", "relative/state");
        test_user_paths_detail::Set_Variable_("XDG_CACHE_HOME", "");

        DOCTEST_REQUIRE(environment.Init());
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_CONFIG)) == environment.Path("xdg-config/orion2re/"));
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_DATA)) == environment.Path("xdg-data/orion2re/"));
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_STATE)) == environment.Path("home/.local/state/orion2re/"));
        DOCTEST_CHECK(std::string(user_paths::Get_Directory(user_paths::USER_DIRECTORY_CACHE)) == environment.Path("home/.cache/orion2re/"));
    }

    DOCTEST_TEST_CASE("user paths search XDG data directories for the original game") {
        test_user_paths_detail::Scoped_User_Environment environment;
        test_user_paths_detail::Set_Variable_("XDG_DATA_DIRS", environment.Path("first") + ":relative::" + environment.Path("second") + "/");

        DOCTEST_REQUIRE(environment.Init());
        const std::vector<std::string>& directories = user_paths::Get_Game_Asset_Directories();
        DOCTEST_REQUIRE(directories.size() == 3);
        DOCTEST_CHECK(directories[0] == environment.Path("home/.local/share/orion2re/game/"));
        DOCTEST_CHECK(directories[1] == environment.Path("first/orion2re/game/"));
        DOCTEST_CHECK(directories[2] == environment.Path("second/orion2re/game/"));
    }

    DOCTEST_TEST_CASE("portable mode is the default for a writable installation") {
        test_user_paths_detail::Scoped_User_Environment environment;
        test_user_paths_detail::Unset_Variable_("ORION2RE_XDG");

        DOCTEST_CHECK_FALSE(environment.Init());
        DOCTEST_CHECK_FALSE(user_paths::Is_Enabled());
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("SAVE1.GAM", true).empty());
        DOCTEST_CHECK_FALSE(test_user_paths_detail::fs::exists(environment.Path("home/.local/share/orion2re")));
    }

    DOCTEST_TEST_CASE("XDG mode is selected by marker file or environment variable") {
        test_user_paths_detail::Scoped_User_Environment environment;

        DOCTEST_CHECK(environment.Init());

        test_user_paths_detail::Set_Variable_("ORION2RE_XDG", "");
        DOCTEST_CHECK_FALSE(environment.Init());

        test_user_paths_detail::Unset_Variable_("ORION2RE_XDG");
        environment.Write_File(test_user_paths_detail::fs::path("install") / user_paths::XDG_MARKER_NAME, "");
        DOCTEST_CHECK(environment.Init());

        test_user_paths_detail::Set_Variable_("ORION2RE_XDG", "0");
        DOCTEST_CHECK_FALSE(environment.Init());
        DOCTEST_CHECK_FALSE(user_paths::Is_Enabled());
    }

#if !defined(_WIN32)
    DOCTEST_TEST_CASE("XDG mode is used when the installation is not writable") {
        test_user_paths_detail::Scoped_User_Environment environment;
        test_user_paths_detail::Unset_Variable_("ORION2RE_XDG");
        test_user_paths_detail::fs::permissions(environment.Path("install"),
            test_user_paths_detail::fs::perms::owner_write | test_user_paths_detail::fs::perms::group_write | test_user_paths_detail::fs::perms::others_write,
            test_user_paths_detail::fs::perm_options::remove);

        // Privileged users can write regardless of permission bits.
        if (access(environment.Path("install").c_str(), W_OK) == 0) {
            return;
        }
        DOCTEST_CHECK(environment.Init());

        test_user_paths_detail::Set_Variable_("ORION2RE_XDG", "0");
        DOCTEST_CHECK_FALSE(environment.Init());
    }
#endif

    DOCTEST_TEST_CASE("legacy file names redirect to their user directories") {
        test_user_paths_detail::Scoped_User_Environment environment;
        DOCTEST_REQUIRE(environment.Init());
        const std::string data = user_paths::Get_Directory(user_paths::USER_DIRECTORY_DATA);
        const std::string config = user_paths::Get_Directory(user_paths::USER_DIRECTORY_CONFIG);
        const std::string cache = user_paths::Get_Directory(user_paths::USER_DIRECTORY_CACHE);

        DOCTEST_CHECK(test_user_paths_detail::Redirect_("SAVE10.GAM", true) == data + "SAVE10.GAM");
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("save?.gam", false) == data + "save?.gam");
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("HOF.M2", false) == data + "HOF.M2");
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("ORION2RE.RAC", true) == data + "ORION2RE.RAC");
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("orion2re.set", true) == config + "orion2re.set");
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("language.ini", true) == config + "language.ini");
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("TEMP.TMP", true) == cache + "TEMP.TMP");

        DOCTEST_CHECK(test_user_paths_detail::Redirect_("ORION2.LBX", false).empty());
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("saves/SAVE1.GAM", false).empty());
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("C:SAVE1.GAM", false).empty());
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("", false).empty());

        char small[4];
        DOCTEST_CHECK_FALSE(user_paths::Redirect_Legacy_Path("SAVE1.GAM", false, small, sizeof(small)));
    }

    DOCTEST_TEST_CASE("installation files are read from next to the original game") {
        test_user_paths_detail::Scoped_User_Environment environment;
        DOCTEST_REQUIRE(environment.Init());

        DOCTEST_CHECK(test_user_paths_detail::Redirect_("ORIONCD.INI", false).empty());

        environment.Write_File("home/.local/share/orion2re/game/ORIONCD.INI", "game");
        DOCTEST_CHECK(test_user_paths_detail::Same_Path_(test_user_paths_detail::Redirect_("orioncd.ini", false), environment.Path("home/.local/share/orion2re/game/ORIONCD.INI")));
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("orioncd.ini", true).empty());

        environment.Write_File("install/orioncd.ini", "install");
        DOCTEST_CHECK(test_user_paths_detail::Same_Path_(test_user_paths_detail::Redirect_("ORIONCD.INI", false), environment.Path("install/orioncd.ini")));
    }

    DOCTEST_TEST_CASE("installed language setting is read until the user writes one") {
        test_user_paths_detail::Scoped_User_Environment environment;
        DOCTEST_REQUIRE(environment.Init());
        const std::string config = user_paths::Get_Directory(user_paths::USER_DIRECTORY_CONFIG);

        DOCTEST_CHECK(test_user_paths_detail::Redirect_("language.ini", false) == config + "language.ini");

        environment.Write_File("system/orion2re/game/LANGUAGE.INI", "1");
        DOCTEST_CHECK(test_user_paths_detail::Same_Path_(test_user_paths_detail::Redirect_("language.ini", false), environment.Path("system/orion2re/game/LANGUAGE.INI")));
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("language.ini", true) == config + "language.ini");

        environment.Write_File("home/.config/orion2re/language.ini", "2");
        DOCTEST_CHECK(test_user_paths_detail::Redirect_("language.ini", false) == config + "language.ini");
    }

    DOCTEST_TEST_CASE("legacy file helpers write and find saves in the user data directory") {
        test_user_paths_detail::Scoped_User_Environment environment;
        DOCTEST_REQUIRE(environment.Init());
        const std::string data = user_paths::Get_Directory(user_paths::USER_DIRECTORY_DATA);

        FILE* file = fopen_case("SAVE3.GAM", "wb");
        DOCTEST_REQUIRE(file != nullptr);
        fputs("save", file);
        fclose(file);
        DOCTEST_CHECK(test_user_paths_detail::fs::is_regular_file(data + "SAVE3.GAM"));

        DOCTEST_CHECK(File_Exists("SAVE3.GAM"));
        file = fopen_case("save3.gam", "rb");
        DOCTEST_REQUIRE(file != nullptr);
        DOCTEST_CHECK(test_user_paths_detail::Read_File_(file) == "save");
        fclose(file);

        char found[64] = {};
        DOCTEST_CHECK(dos::DIR("save?.gam", found, sizeof(found)));
        DOCTEST_CHECK(std::string(found) == "SAVE3.GAM");
        DOCTEST_CHECK(dos::Time_Stamp("SAVE3.GAM") != nullptr);

        dos::Delete_File("SAVE3.GAM");
        DOCTEST_CHECK_FALSE(File_Exists("SAVE3.GAM"));
        DOCTEST_CHECK_FALSE(dos::DIR("save?.gam", found, sizeof(found)));
    }
}
