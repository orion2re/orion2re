#include "pch.h"

#include "doctest/doctest.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "game/vfs.h"

#include "miniz.h"

namespace test_vfs_detail {
    namespace fs = std::filesystem;

    std::string Read_Vfs_File_(const char* path) {
        s_vfs_file* file = vfs::fopen(path, "rb");
        if (file == nullptr) return {};
        const int32_t size = vfs::fsize(file);
        std::string result(size > 0 ? static_cast<size_t>(size) : 0, '\0');
        if (!result.empty()) {
            vfs::fread(result.data(), 1, result.size(), file);
        }
        vfs::fclose(file);
        return result;
    }

    class Scoped_Vfs_Test_Directory {
      public:
        Scoped_Vfs_Test_Directory() {
            const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
            root_path_ = fs::temp_directory_path() / ("orion2re-vfs-" + std::to_string(unique_id));
            fs::create_directories(root_path_);
        }

        ~Scoped_Vfs_Test_Directory() {
            vfs::Shutdown();
            std::error_code error;
            fs::remove_all(root_path_, error);
        }

        [[nodiscard]] const fs::path& root_path() const {
            return root_path_;
        }

        void Write_File(const fs::path& relative_path, const std::string& contents) const {
            const fs::path full_path = root_path_ / relative_path;
            fs::create_directories(full_path.parent_path());

            std::ofstream output(full_path, std::ios::binary);
            DOCTEST_REQUIRE(output.good());
            output.write(contents.data(), (std::streamsize)contents.size());
            DOCTEST_REQUIRE(output.good());
        }

        void Write_Zip_File(const fs::path& relative_path, const std::vector<std::pair<std::string, std::string>>& entries) const {
            const fs::path full_path = root_path_ / relative_path;
            mz_zip_archive archive{};

            fs::create_directories(full_path.parent_path());

            mz_zip_zero_struct(&archive);
            DOCTEST_REQUIRE(mz_zip_writer_init_file(&archive, full_path.string().c_str(), 0) == MZ_TRUE);

            for (const auto& entry : entries) {
                DOCTEST_REQUIRE(mz_zip_writer_add_mem(
                                    &archive,
                                    entry.first.c_str(),
                                    entry.second.data(),
                                    entry.second.size(),
                                    MZ_BEST_COMPRESSION) == MZ_TRUE);
            }

            DOCTEST_REQUIRE(mz_zip_writer_finalize_archive(&archive) == MZ_TRUE);
            DOCTEST_REQUIRE(mz_zip_writer_end(&archive) == MZ_TRUE);
        }

        void Write_Lbx_File(const fs::path& relative_path, const std::vector<std::string>& entries) const {
            const fs::path full_path = root_path_ / relative_path;
            s_lbx_disk_header header{};
            uint32_t offsets[510] = {};
            uint32_t next_offset = sizeof(header) + sizeof(offsets);

            fs::create_directories(full_path.parent_path());

            header.entry_count = (uint16_t)entries.size();
            header.magic = 0xFEAD;
            header.header_data = 0;
            offsets[0] = next_offset;

            for (size_t i = 0; i < entries.size(); ++i) {
                next_offset += (uint32_t)entries[i].size();
                offsets[i + 1] = next_offset;
            }

            std::ofstream output(full_path, std::ios::binary);
            DOCTEST_REQUIRE(output.good());
            output.write((const char*)&header, sizeof(header));
            output.write((const char*)offsets, sizeof(offsets));
            for (const std::string& entry : entries) {
                output.write(entry.data(), (std::streamsize)entry.size());
            }
            DOCTEST_REQUIRE(output.good());
        }

      private:
        fs::path root_path_;
    };
} // namespace test_vfs_detail

DOCTEST_TEST_SUITE("vfs cpp") {
    DOCTEST_TEST_CASE("vfs requires initialization before assets can be opened") {
        vfs::Shutdown();

        DOCTEST_CHECK(vfs::File_Exists("sample.dat") == 0);
        DOCTEST_CHECK(vfs::fopen("sample.dat", "rb") == nullptr);
    }

    DOCTEST_TEST_CASE("vfs raw filesystem backend opens and reads files from the configured base path") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File("assets.bin", "abcdef");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(vfs::File_Exists("assets.bin") == 1);

        s_vfs_file* file = vfs::fopen("assets.bin", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 8> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 3, file) == 3);
        DOCTEST_CHECK(std::string(buffer.data(), 3) == "abc");
        DOCTEST_CHECK(vfs::ftell(file) == 3);

        DOCTEST_CHECK(vfs::fseek(file, -2, SEEK_END) == 0);
        DOCTEST_CHECK(vfs::ftell(file) == 4);
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 2, file) == 2);
        DOCTEST_CHECK(std::string(buffer.data(), 2) == "ef");

        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs reads loose shared data ahead of original raw assets") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File("shared.dat", "original");
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "shared.dat", "shared");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        s_vfs_file* file = vfs::fopen("shared.dat", "rb");
        DOCTEST_REQUIRE(file != nullptr);
        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 6, file) == 6);
        DOCTEST_CHECK(std::string(buffer.data(), 6) == "shared");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("staged directory package projects data and keeps private files namespaced") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "mods" / "sample" /
                "mod.json5",
            "manifest");
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "mods" / "sample" /
                "data" / "music" / "theme.dat",
            "package-theme");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        s_vfs_mount_transaction* transaction = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(transaction != nullptr);
        std::string diagnostic;
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "sample", &diagnostic) == 1);
        DOCTEST_CHECK(vfs::File_Exists("mods/sample/mod.json5") == 1);
        DOCTEST_CHECK(vfs::File_Exists("mod.json5") == 0);
        DOCTEST_CHECK(vfs::File_Exists("music/theme.dat") == 1);

        vfs::Discard_Mount_Transaction(transaction);
        DOCTEST_CHECK(vfs::File_Exists("mods/sample/mod.json5") == 0);
        DOCTEST_CHECK(vfs::File_Exists("music/theme.dat") == 0);
    }

    DOCTEST_TEST_CASE("private package paths cannot be supplied by non-package VFS sources") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "overrides" / "mods" /
                "sample" / "mod.json5",
            "override");
        test_dir.Write_File(test_vfs_detail::fs::path("mods") / "sample" / "mod.json5",
            "raw");
        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "base.zip", {
                                                                                    {"mods/sample/mod.json5", "shared-zip"},
                                                                                });

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(vfs::File_Exists("mods/sample/mod.json5") == 0);
        DOCTEST_CHECK(vfs::fopen("mods/sample/mod.json5", "rb") == nullptr);
    }

    DOCTEST_TEST_CASE("committed directory package order gives the first mount precedence") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "mods" / "first" /
                "data" / "priority.dat",
            "first");
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "mods" / "second" /
                "data" / "priority.dat",
            "second");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        s_vfs_mount_transaction* transaction = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(transaction != nullptr);
        std::string diagnostic;
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "first", &diagnostic) == 1);
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "second", &diagnostic) == 1);
        DOCTEST_REQUIRE(vfs::Commit_Mount_Transaction(transaction) == 1);

        s_vfs_file* file = vfs::fopen("priority.dat", "rb");
        DOCTEST_REQUIRE(file != nullptr);
        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 5, file) == 5);
        DOCTEST_CHECK(std::string(buffer.data(), 5) == "first");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("mount transaction commit can be rolled back before finalization") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "mods" / "first" /
                "data" / "active.dat",
            "first");
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "mods" / "second" /
                "data" / "active.dat",
            "second");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        std::string diagnostic;
        s_vfs_mount_transaction* first = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(first != nullptr);
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(first, "first", &diagnostic) == 1);
        DOCTEST_REQUIRE(vfs::Commit_Mount_Transaction(first) == 1);

        s_vfs_mount_transaction* second = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(second != nullptr);
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(second, "second", &diagnostic) == 1);
        DOCTEST_REQUIRE(vfs::Begin_Mount_Transaction_Commit(second));
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("active.dat") == "second");
        vfs::Rollback_Mount_Transaction_Commit(second);
        vfs::Discard_Mount_Transaction(second);
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("active.dat") == "first");
    }

    DOCTEST_TEST_CASE("staged ZIP package exposes private root and public data projection") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "mods" / "archive.zip", {
                                                                                                {"mod.json5", "manifest"},
                                                                                                {"config/main.cfg", "audio music_volume = 60;"},
                                                                                                {"data/voices/alert.dat", "archive-alert"},
                                                                                            });

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        s_vfs_mount_transaction* transaction = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(transaction != nullptr);
        std::string diagnostic;
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "archive", &diagnostic) == 1);
        DOCTEST_CHECK(vfs::File_Exists("mods/archive/mod.json5") == 1);
        DOCTEST_CHECK(vfs::File_Exists("mods/archive/config/main.cfg") == 1);
        DOCTEST_CHECK(vfs::File_Exists("voices/alert.dat") == 1);
        DOCTEST_CHECK(vfs::File_Exists("config/main.cfg") == 0);
        vfs::Discard_Mount_Transaction(transaction);
    }

    DOCTEST_TEST_CASE("package mount rejects ambiguous directory and ZIP forms without changing lookup") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "mods" / "duplicate" /
                "mod.json5",
            "directory");
        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "mods" /
                "duplicate.zip",
            {{"mod.json5", "archive"}});

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        s_vfs_mount_transaction* transaction = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(transaction != nullptr);
        std::string diagnostic;
        DOCTEST_CHECK(vfs::Stage_Package_Mount(transaction, "duplicate", &diagnostic) == 0);
        DOCTEST_CHECK(diagnostic == "package 'duplicate' has both directory and ZIP forms");
        vfs::Discard_Mount_Transaction(transaction);
        DOCTEST_CHECK(vfs::File_Exists("mods/duplicate/mod.json5") == 0);
    }

    DOCTEST_TEST_CASE("vfs gives data overrides precedence over raw files") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File("voices.dat", "raw");
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "overrides" / "voices.dat", "override");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        s_vfs_file* file = vfs::fopen("voices.dat", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 8, file) == 8);
        DOCTEST_CHECK(std::string(buffer.data(), 8) == "override");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs normalizes relative asset paths across nested directories") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("nested") / "sample.bin", "12345");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(vfs::File_Exists("nested\\sample.bin") == 1);

        s_vfs_file* file = vfs::fopen("./nested/sample.bin", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 8> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 2, 3, file) == 2);
        DOCTEST_CHECK(std::string(buffer.data(), 4) == "1234");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs resolves raw file names case-insensitively") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File("FONTS.LBX", "font-data");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(vfs::File_Exists("fonts.lbx") == 1);

        s_vfs_file* file = vfs::fopen("fonts.lbx", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 8, file) == 8);
        DOCTEST_CHECK(std::string(buffer.data(), 8) == "font-dat");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs can fall back to a secondary raw asset path without affecting local override lookup") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("cdroot") / "diplomat.lbx", "cd-asset");
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "overrides" / "diplomat.lbx", "override");

        const std::string base_path = test_dir.root_path().string();
        const std::string fallback_path = (test_dir.root_path() / "cdroot").string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_REQUIRE(vfs::Set_Raw_Fallback_Path(fallback_path.c_str()) == 1);

        char resolved_path[1024] = {};
        DOCTEST_CHECK(vfs::Resolve_Read_Path_No_Zip("diplomat.lbx", resolved_path, sizeof(resolved_path)) == 1);

        s_vfs_file* file = vfs::fopen("diplomat.lbx", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 8, file) == 8);
        DOCTEST_CHECK(std::string(buffer.data(), 8) == "override");
        DOCTEST_CHECK(vfs::fclose(file) == 0);

        test_dir.Write_File("raw-only.dat", "local-raw");
        DOCTEST_CHECK(vfs::File_Exists("raw-only.dat") == 1);
    }

    DOCTEST_TEST_CASE("vfs resolves raw assets from the secondary fallback path when no local asset exists") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("cdroot") / "simtex.lbx", "cinematic");

        const std::string base_path = test_dir.root_path().string();
        const std::string fallback_path = (test_dir.root_path() / "cdroot").string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_REQUIRE(vfs::Set_Raw_Fallback_Path(fallback_path.c_str()) == 1);

        char resolved_path[1024] = {};
        DOCTEST_CHECK(vfs::Resolve_Read_Path_No_Zip("simtex.lbx", resolved_path, sizeof(resolved_path)) == 1);
        DOCTEST_CHECK(vfs::File_Exists("simtex.lbx") == 1);

        s_vfs_file* file = vfs::fopen("simtex.lbx", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 9, file) == 9);
        DOCTEST_CHECK(std::string(buffer.data(), 9) == "cinematic");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs rejects write capable modes and out of range seeks") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File("sample.dat", "hello");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        DOCTEST_CHECK(vfs::fopen("sample.dat", "wb") == nullptr);
        DOCTEST_CHECK(vfs::fopen("sample.dat", "ab") == nullptr);
        DOCTEST_CHECK(vfs::fopen("sample.dat", "r+") == nullptr);
        DOCTEST_CHECK(vfs::fopen("sample.dat", "rb+") == nullptr);

        s_vfs_file* file = vfs::fopen("sample.dat", "rb");
        DOCTEST_REQUIRE(file != nullptr);
        DOCTEST_CHECK(vfs::fseek(file, -1, SEEK_SET) == -1);
        DOCTEST_CHECK(vfs::fseek(file, 6, SEEK_SET) == -1);
        DOCTEST_CHECK(vfs::ftell(file) == 0);
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs opens zip-backed assets and supports backward seek by recreating the stream") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "base.zip", {
                                                                                    {"music/theme.dat", "zip-theme"},
                                                                                });

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(vfs::File_Exists("music/theme.dat") == 1);

        s_vfs_file* file = vfs::fopen("music/theme.dat", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 4, file) == 4);
        DOCTEST_CHECK(std::string(buffer.data(), 4) == "zip-");
        DOCTEST_CHECK(vfs::fseek(file, 0, SEEK_SET) == 0);
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 9, file) == 9);
        DOCTEST_CHECK(std::string(buffer.data(), 9) == "zip-theme");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs loads only data base zip and ignores other shared zip files") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File("intro.dat", "raw-intro");
        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "ignored.zip", {
                                                                                       {"intro.dat", "ignored"},
                                                                                       {"ignored-only.dat", "ignored"},
                                                                                   });
        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "base.zip", {
                                                                                    {"intro.dat", "base"},
                                                                                });

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(vfs::File_Exists("ignored-only.dat") == 0);

        s_vfs_file* file = vfs::fopen("intro.dat", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 4, file) == 4);
        DOCTEST_CHECK(std::string(buffer.data(), 4) == "base");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs loose overrides still win over zip-backed assets") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "base.zip", {
                                                                                    {"voices/alert.dat", "zip-alert"},
                                                                                });
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "overrides" / "voices" / "alert.dat", "loose-alert");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        s_vfs_file* file = vfs::fopen("voices/alert.dat", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 11, file) == 11);
        DOCTEST_CHECK(std::string(buffer.data(), 11) == "loose-alert");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs resolves zip entry paths case-insensitively") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "base.zip", {
                                                                                    {"MUSIC/THEME.DAT", "zip-theme"},
                                                                                });

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(vfs::File_Exists("music/theme.dat") == 1);

        s_vfs_file* file = vfs::fopen("music/theme.dat", "rb");
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 9, file) == 9);
        DOCTEST_CHECK(std::string(buffer.data(), 9) == "zip-theme");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs fopen_lbx returns an entry-relative stream from a raw lbx") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Lbx_File("TEST.LBX", {"zero", "entry-one"});

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        s_vfs_file* file = vfs::fopen_lbx("test.lbx", 1);
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 5, file) == 5);
        DOCTEST_CHECK(std::string(buffer.data(), 5) == "entry");
        DOCTEST_CHECK(vfs::ftell(file) == 5);
        DOCTEST_CHECK(vfs::fseek(file, 0, SEEK_SET) == 0);
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 9, file) == 9);
        DOCTEST_CHECK(std::string(buffer.data(), 9) == "entry-one");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs fopen_lbx rejects entries outside the raw archive directory") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Lbx_File("TEST.LBX", {"zero", "one"});

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        DOCTEST_CHECK(vfs::fopen_lbx("TEST.LBX", -1) == nullptr);
        DOCTEST_CHECK(vfs::fopen_lbx("TEST.LBX", 2) == nullptr);
    }

    DOCTEST_TEST_CASE("vfs fopen_lbx loads the documented COLBLDG0 loose entry override") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Lbx_File("COLBLDG0.LBX", {"raw-entry"});
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "overrides" /
                "COLBLDG0.LBX" / "000",
            "entry-override");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        s_vfs_file* file = vfs::fopen_lbx("colbldg0.lbx", 0);
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 14, file) == 14);
        DOCTEST_CHECK(std::string(buffer.data(), 14) == "entry-override");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs fopen_lbx passes through entries without a loose override") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        std::vector<std::string> entries(71, "unused");
        entries[57] = "raw-entry-57";
        entries[70] = "raw-entry-70";
        test_dir.Write_Lbx_File("MAINPUPS.LBX", entries);
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "overrides" /
                "MAINPUPS.LBX" / "070",
            "override-entry-70");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        s_vfs_file* file = vfs::fopen_lbx("MAINPUPS.LBX", 57);
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 12, file) == 12);
        DOCTEST_CHECK(std::string(buffer.data(), 12) == "raw-entry-57");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs fopen_lbx prefers per-entry loose override over whole lbx replacement from zip") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Lbx_File("TEST.LBX", {"raw-zero", "raw-one"});

        test_dir.Write_Lbx_File("WHOLEZIP.LBX", {"whole-zero", "whole-one"});
        const test_vfs_detail::fs::path whole_zip_lbx_path = test_dir.root_path() / "WHOLEZIP.LBX";
        std::ifstream whole_zip_lbx(whole_zip_lbx_path, std::ios::binary);
        std::string whole_zip_lbx_contents((std::istreambuf_iterator<char>(whole_zip_lbx)), std::istreambuf_iterator<char>());
        DOCTEST_REQUIRE(!whole_zip_lbx_contents.empty());

        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "base.zip", {
                                                                                    {"TEST.LBX", whole_zip_lbx_contents},
                                                                                });
        test_dir.Write_File(test_vfs_detail::fs::path("data") / "overrides" / "TEST.LBX" / "001", "entry-override");

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        s_vfs_file* file = vfs::fopen_lbx("TEST.LBX", 1);
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 14, file) == 14);
        DOCTEST_CHECK(std::string(buffer.data(), 14) == "entry-override");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs fopen_lbx falls back to whole lbx replacement from zip") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Lbx_File("TEST.LBX", {"raw-zero", "raw-one"});
        test_dir.Write_Lbx_File("ZIPTEST.LBX", {"zip-zero", "zip-one"});

        const test_vfs_detail::fs::path zip_lbx_path = test_dir.root_path() / "ZIPTEST.LBX";
        std::ifstream zip_lbx(zip_lbx_path, std::ios::binary);
        std::string zip_lbx_contents((std::istreambuf_iterator<char>(zip_lbx)), std::istreambuf_iterator<char>());
        DOCTEST_REQUIRE(!zip_lbx_contents.empty());

        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "base.zip", {
                                                                                    {"TEST.LBX", zip_lbx_contents},
                                                                                });

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        s_vfs_file* file = vfs::fopen_lbx("TEST.LBX", 0);
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 16> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 8, file) == 8);
        DOCTEST_CHECK(std::string(buffer.data(), 8) == "zip-zero");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs fopen_lbx prefers zip per-entry override over whole lbx zip entry") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_Lbx_File("ZIPTEST2.LBX", {"whole-zero", "whole-one"});

        const test_vfs_detail::fs::path zip_lbx_path = test_dir.root_path() / "ZIPTEST2.LBX";
        std::ifstream zip_lbx(zip_lbx_path, std::ios::binary);
        std::string zip_lbx_contents((std::istreambuf_iterator<char>(zip_lbx)), std::istreambuf_iterator<char>());
        DOCTEST_REQUIRE(!zip_lbx_contents.empty());

        test_dir.Write_Zip_File(test_vfs_detail::fs::path("data") / "base.zip", {
                                                                                    {"TEST2.LBX", zip_lbx_contents},
                                                                                    {"TEST2.LBX/001", "zip-entry-override"},
                                                                                });

        const std::string base_path = test_dir.root_path().string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);

        s_vfs_file* file = vfs::fopen_lbx("TEST2.LBX", 1);
        DOCTEST_REQUIRE(file != nullptr);

        std::array<char, 32> buffer{};
        DOCTEST_CHECK(vfs::fread(buffer.data(), 1, 18, file) == 18);
        DOCTEST_CHECK(std::string(buffer.data(), 18) == "zip-entry-override");
        DOCTEST_CHECK(vfs::fclose(file) == 0);
    }

    DOCTEST_TEST_CASE("vfs resolves configuration files from the user config path first") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("install") / "data" / "orion2re.cfg", "install");
        test_dir.Write_File(test_vfs_detail::fs::path("install") / "data" / "shared.cfg", "install-shared");
        test_dir.Write_File(test_vfs_detail::fs::path("install") / "data" / "music" / "theme.dat", "install-theme");
        test_dir.Write_File(test_vfs_detail::fs::path("config") / "orion2re.cfg", "user");
        test_dir.Write_File(test_vfs_detail::fs::path("config") / "music" / "theme.dat", "user-theme");

        const std::string base_path = (test_dir.root_path() / "install").string();
        const std::string config_path = (test_dir.root_path() / "config").string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("orion2re.cfg") == "install");

        DOCTEST_REQUIRE(vfs::Set_User_Config_Path(config_path.c_str()) == 1);
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("orion2re.cfg") == "user");
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("shared.cfg") == "install-shared");
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("music/theme.dat") == "install-theme");

        char resolved_path[1024] = {};
        DOCTEST_REQUIRE(vfs::Resolve_Read_Path_No_Zip("orion2re.cfg", resolved_path, sizeof(resolved_path)) == 1);
        DOCTEST_CHECK(std::string(resolved_path).find("config") != std::string::npos);

        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("orion2re.cfg") == "install");
    }

    DOCTEST_TEST_CASE("vfs mounts a user package in place of an installed package with the same ID") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("install") / "data" / "mods" / "sample" / "data" / "priority.dat", "install");
        test_dir.Write_File(test_vfs_detail::fs::path("install") / "data" / "mods" / "other" / "data" / "other.dat", "install-other");
        test_dir.Write_Zip_File(test_vfs_detail::fs::path("user") / "mods" / "sample.zip", {
                                                                                               {"data/priority.dat", "user"},
                                                                                           });

        const std::string base_path = (test_dir.root_path() / "install").string();
        const std::string mods_path = (test_dir.root_path() / "user" / "mods").string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_REQUIRE(vfs::Set_User_Mods_Path(mods_path.c_str()) == 1);
        s_vfs_mount_transaction* transaction = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(transaction != nullptr);
        std::string diagnostic;
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "sample", &diagnostic) == 1);
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "other", &diagnostic) == 1);
        DOCTEST_REQUIRE(vfs::Commit_Mount_Transaction(transaction) == 1);

        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("priority.dat") == "user");
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("other.dat") == "install-other");
    }

    DOCTEST_TEST_CASE("vfs searches added raw paths after the base path and before the fallback path") {
        test_vfs_detail::Scoped_Vfs_Test_Directory test_dir;
        test_dir.Write_File(test_vfs_detail::fs::path("install") / "local.lbx", "install");
        test_dir.Write_File(test_vfs_detail::fs::path("game") / "local.lbx", "game-local");
        test_dir.Write_File(test_vfs_detail::fs::path("game") / "diplomat.lbx", "game");
        test_dir.Write_File(test_vfs_detail::fs::path("cdroot") / "diplomat.lbx", "cd");
        test_dir.Write_File(test_vfs_detail::fs::path("cdroot") / "cd-only.lbx", "cd-only");

        const std::string base_path = (test_dir.root_path() / "install").string();
        const std::string game_path = (test_dir.root_path() / "game").string();
        const std::string fallback_path = (test_dir.root_path() / "cdroot").string();
        DOCTEST_REQUIRE(vfs::Init(base_path.c_str()) == 1);
        DOCTEST_CHECK(vfs::Add_Raw_Search_Path("") == 0);
        DOCTEST_REQUIRE(vfs::Add_Raw_Search_Path(game_path.c_str()) == 1);
        DOCTEST_REQUIRE(vfs::Set_Raw_Fallback_Path(fallback_path.c_str()) == 1);

        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("local.lbx") == "install");
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("DIPLOMAT.LBX") == "game");
        DOCTEST_CHECK(test_vfs_detail::Read_Vfs_File_("cd-only.lbx") == "cd-only");
    }
}