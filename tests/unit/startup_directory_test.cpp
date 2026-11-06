#include "pch.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "doctest/doctest.h"
#include "game/vfs.h"
#include "startup_directory.h"

namespace {
    namespace fs = std::filesystem;

    struct s_startup_directory_fixture {
        fs::path original_directory = fs::current_path();
        fs::path executable_directory;
        fs::path game_files;
        fs::path launch_directory;
        fs::path installed_directory;
        fs::path user_directory;
        std::string relative_directory;

        s_startup_directory_fixture() {
            const char* base_path = SDL_GetBasePath();
            DOCTEST_REQUIRE(base_path != nullptr);
            executable_directory = fs::path(std::u8string(base_path, base_path + strlen(base_path)));
            relative_directory = "orion2re-startup-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            game_files = executable_directory / relative_directory;
            launch_directory = fs::temp_directory_path() / (relative_directory + "-launcher");
            installed_directory = fs::temp_directory_path() / (relative_directory + "-installed");
            user_directory = fs::temp_directory_path() / (relative_directory + "-user");
            fs::create_directories(game_files);
            fs::create_directories(launch_directory / relative_directory);
            fs::create_directories(installed_directory);
            fs::create_directories(user_directory);
        }

        ~s_startup_directory_fixture() {
            vfs::Shutdown();
            std::error_code error;
            fs::current_path(original_directory, error);
            fs::remove_all(game_files, error);
            fs::remove_all(launch_directory, error);
            fs::remove_all(installed_directory, error);
            fs::remove_all(user_directory, error);
        }
    };

    void Write_File_(const fs::path& path, const char* contents) {
        std::ofstream output(path, std::ios::binary);
        DOCTEST_REQUIRE(output.good());
        output << contents;
        DOCTEST_REQUIRE(output.good());
    }

    std::string Read_Vfs_File_(const std::string& path) {
        s_vfs_file* file = vfs::fopen(path.c_str(), "rb");
        if (file == nullptr) return {};
        const int32_t size = vfs::fsize(file);
        std::string contents(size > 0 ? static_cast<size_t>(size) : 0, '\0');
        if (!contents.empty()) {
            vfs::fread(contents.data(), 1, contents.size(), file);
        }
        vfs::fclose(file);
        return contents;
    }
} // namespace

DOCTEST_TEST_CASE("startup uses the executable directory instead of the launcher's working directory") {
    s_startup_directory_fixture fixture;
    Write_File_(fixture.game_files / "asset.dat", "executable data");
    Write_File_(fixture.game_files / "config.cfg", "executable config");
    Write_File_(fixture.launch_directory / fixture.relative_directory / "asset.dat", "launcher data");
    Write_File_(fixture.launch_directory / fixture.relative_directory / "config.cfg", "launcher config");
    fs::current_path(fixture.launch_directory);

    std::string directory;
    std::string error = "previous error";
    DOCTEST_REQUIRE(startup::Set_Game_Directory(&directory, &error));
    DOCTEST_CHECK(error.empty());
    DOCTEST_CHECK(fs::equivalent(fs::path(std::u8string(directory.begin(), directory.end())), fixture.executable_directory));
    DOCTEST_CHECK(fs::equivalent(fs::current_path(), fixture.executable_directory));

    DOCTEST_REQUIRE(vfs::Init(directory.c_str()) == 1);
    DOCTEST_CHECK(Read_Vfs_File_(fixture.relative_directory + "/asset.dat") == "executable data");
    DOCTEST_CHECK(Read_Vfs_File_(fixture.relative_directory + "/config.cfg") == "executable config");

    // Legacy relative file I/O must use the same directory as the VFS.
    std::ifstream input(fs::path(fixture.relative_directory) / "asset.dat");
    std::string contents;
    std::getline(input, contents);
    DOCTEST_CHECK(contents == "executable data");
    Write_File_(fs::path(fixture.relative_directory) / "settings.ini", "saved settings");
    DOCTEST_CHECK(fs::exists(fixture.game_files / "settings.ini"));
    DOCTEST_CHECK_FALSE(fs::exists(fixture.launch_directory / fixture.relative_directory / "settings.ini"));
}

DOCTEST_TEST_CASE("data directory prefers packaged data beside the executable") {
    s_startup_directory_fixture fixture;
    fs::create_directories(fixture.game_files / "data");
    fs::create_directories(fixture.installed_directory / "data");
    fs::create_directories(fixture.user_directory / "data");
    Write_File_(fixture.game_files / "data" / "base.zip", "executable base data");
    Write_File_(fixture.installed_directory / "data" / "base.zip", "installed base data");
    Write_File_(fixture.user_directory / "data" / "base.zip", "user base data");

    std::string directory;
    DOCTEST_REQUIRE(startup::Resolve_Data_Directory(fixture.game_files.string(), fixture.user_directory.string().c_str(), fixture.installed_directory.string().c_str(), &directory));
    DOCTEST_CHECK(fs::equivalent(fs::path(directory), fixture.game_files));
}

DOCTEST_TEST_CASE("data directory prefers the user data directory over the installed data directory") {
    s_startup_directory_fixture fixture;
    fs::create_directories(fixture.user_directory / "data");
    fs::create_directories(fixture.installed_directory / "data");
    Write_File_(fixture.user_directory / "data" / "base.zip", "user base data");
    Write_File_(fixture.installed_directory / "data" / "base.zip", "installed base data");

    std::string directory;
    DOCTEST_REQUIRE(startup::Resolve_Data_Directory(fixture.game_files.string(), fixture.user_directory.string().c_str(), fixture.installed_directory.string().c_str(), &directory));
    DOCTEST_CHECK(fs::equivalent(fs::path(directory), fixture.user_directory));
}

DOCTEST_TEST_CASE("data directory falls back to the installed data directory when the user directory has no packaged data") {
    s_startup_directory_fixture fixture;
    fs::create_directories(fixture.user_directory / "data");
    fs::create_directories(fixture.installed_directory / "data");
    Write_File_(fixture.installed_directory / "data" / "base.zip", "installed base data");

    std::string directory;
    DOCTEST_REQUIRE(startup::Resolve_Data_Directory(fixture.game_files.string(), fixture.user_directory.string().c_str(), fixture.installed_directory.string().c_str(), &directory));
    DOCTEST_CHECK(fs::equivalent(fs::path(directory), fixture.installed_directory));

    // An empty user directory is skipped the same way as a missing one.
    DOCTEST_REQUIRE(startup::Resolve_Data_Directory(fixture.game_files.string(), "", fixture.installed_directory.string().c_str(), &directory));
    DOCTEST_CHECK(fs::equivalent(fs::path(directory), fixture.installed_directory));
}

DOCTEST_TEST_CASE("data directory falls back to the installed data directory") {
    s_startup_directory_fixture fixture;
    fs::create_directories(fixture.installed_directory / "data");
    Write_File_(fixture.installed_directory / "data" / "base.zip", "installed base data");

    std::string directory;
    DOCTEST_REQUIRE(startup::Resolve_Data_Directory(fixture.game_files.string(), nullptr, fixture.installed_directory.string().c_str(), &directory));
    DOCTEST_CHECK(fs::equivalent(fs::path(directory), fixture.installed_directory));
}

DOCTEST_TEST_CASE("data directory keeps the game directory when no packaged data exists") {
    s_startup_directory_fixture fixture;

    std::string directory;
    DOCTEST_REQUIRE(startup::Resolve_Data_Directory(fixture.game_files.string(), fixture.user_directory.string().c_str(), fixture.installed_directory.string().c_str(), &directory));
    DOCTEST_CHECK(fs::equivalent(fs::path(directory), fixture.game_files));

    DOCTEST_REQUIRE(startup::Resolve_Data_Directory(fixture.game_files.string(), fixture.user_directory.string().c_str(), nullptr, &directory));
    DOCTEST_CHECK(fs::equivalent(fs::path(directory), fixture.game_files));

    DOCTEST_REQUIRE(startup::Resolve_Data_Directory(fixture.game_files.string(), nullptr, nullptr, &directory));
    DOCTEST_CHECK(fs::equivalent(fs::path(directory), fixture.game_files));
}
