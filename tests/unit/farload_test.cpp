#include "pch.h"

#include "doctest/doctest.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "game/buffer.h"
#include "game/farload.h"
#include "game/string_overrides.h"
#include "game/vfs.h"

#include "miniz.h"

namespace test_farload_buffer_detail {
    namespace fs = std::filesystem;

    class Scoped_Test_Directory {
      public:
        Scoped_Test_Directory() {
            string_overrides::Reset_Cache();
            const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
            root_path_ = fs::temp_directory_path() / ("orion2re-farload-" + std::to_string(unique_id));
            fs::create_directories(root_path_);
        }

        ~Scoped_Test_Directory() {
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

        void Write_Zip_File(const fs::path& relative_path, const std::vector<std::pair<std::string, std::string>>& entries) const {
            const fs::path full_path = root_path_ / relative_path;
            mz_zip_archive archive{};

            fs::create_directories(full_path.parent_path());
            mz_zip_zero_struct(&archive);
            DOCTEST_REQUIRE(mz_zip_writer_init_file(&archive, full_path.string().c_str(), 0) == MZ_TRUE);

            for (const auto& entry : entries) {
                DOCTEST_REQUIRE(mz_zip_writer_add_mem(&archive, entry.first.c_str(), entry.second.data(), entry.second.size(), MZ_BEST_COMPRESSION) == MZ_TRUE);
            }

            DOCTEST_REQUIRE(mz_zip_writer_finalize_archive(&archive) == MZ_TRUE);
            DOCTEST_REQUIRE(mz_zip_writer_end(&archive) == MZ_TRUE);
        }

      private:
        fs::path root_path_;
    };

    struct Scoped_Buffer_Pool {
        std::vector<uint8_t> storage;
        uint8_t* payload;

        explicit Scoped_Buffer_Pool(uint32_t payload_size)
            : storage(sizeof(allocate::s_alloc_header) + payload_size), payload(storage.data() + sizeof(allocate::s_alloc_header)) {
            auto* header = (allocate::s_alloc_header*)storage.data();
            header->current = 0;
            header->size = payload_size;
            header->reserved2 = 0;
        }
    };

    std::string Read_File_Contents(const fs::path& full_path) {
        std::ifstream input(full_path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    }

    std::string Records(const std::vector<std::string>& strings, uint16_t size) {
        const uint16_t count = (uint16_t)strings.size();
        std::string result((const char*)&count, sizeof(count));
        result.append((const char*)&size, sizeof(size));
        for (const auto& text : strings) {
            DOCTEST_REQUIRE(text.size() <= size);
            result += text;
            result.append(size - text.size(), '\0');
        }
        return result;
    }

    void Mount_Packages(const std::vector<std::string>& packages) {
        auto* transaction = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(transaction != nullptr);
        std::string diagnostic;
        for (const auto& package : packages) {
            DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, package.c_str(), &diagnostic));
        }
        DOCTEST_REQUIRE(vfs::Commit_Mount_Transaction(transaction));
    }
} // namespace test_farload_buffer_detail

DOCTEST_TEST_SUITE("farload buffer vfs") {
    DOCTEST_TEST_CASE("base translations match scrap prompt argument contracts without enabling a mod") {
        using namespace test_farload_buffer_detail;
        const std::vector<std::pair<std::string, std::string>> sidecars = {
            {"hfstrngs.json5", Read_File_Contents(fs::path(STRING_OVERRIDE_BASE_DATA_DIR) / "hfstrngs.json5")},
            {"hsstrngs.json5", Read_File_Contents(fs::path(STRING_OVERRIDE_BASE_DATA_DIR) / "hsstrngs.json5")},
            {"histrngs.json5", Read_File_Contents(fs::path(STRING_OVERRIDE_BASE_DATA_DIR) / "histrngs.json5")},
        };
        for (const auto& sidecar : sidecars) {
            DOCTEST_REQUIRE_FALSE(sidecar.second.empty());
        }

        struct s_case {
            const char* filename;
            std::vector<int> corrected_messages;
        };
        const s_case cases[] = {
            {"HFSTRNGS.LBX", {128, 129}},
            {"HSSTRNGS.LBX", {122, 123, 124, 125, 126, 128, 129}},
            {"HISTRNGS.LBX", {126}},
            {"HESTRNGS.LBX", {}},
            {"HGSTRNGS.LBX", {}},
        };
        for (const s_case& test_case : cases) {
            DOCTEST_CAPTURE(test_case.filename);
            Scoped_Test_Directory test_dir;
            std::vector<std::string> original;
            std::string packed;
            for (int index = 0; index < 397; ++index) {
                std::string text = "unchanged message " + std::to_string(index);
                if (std::find(test_case.corrected_messages.begin(), test_case.corrected_messages.end(), index) != test_case.corrected_messages.end()) {
                    text = index >= 128 ? "value %d BC%s"
                        : index == 126  ? "name %s name %s extra %s"
                                        : "name %s extra %s";
                }
                original.push_back(text);
                packed += text;
                packed.push_back('\0');
            }
            test_dir.Write_Lbx_File(test_case.filename, {Records({packed}, (uint16_t)packed.size())});
            test_dir.Write_Zip_File("data/base.zip", sidecars);
            DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()));

            std::vector<char> table(packed.size());
            farload::Farload_Data_Static(test_case.filename, 0, table.data(), 0, 1, (int32_t)table.size());
            string_overrides::Apply_Table(test_case.filename, &table, 397);

            size_t offset = 0;
            for (int index = 0; index < 397; ++index) {
                DOCTEST_CAPTURE(index);
                const char* text = table.data() + offset;
                offset += strlen(text) + 1;
                if (std::find(test_case.corrected_messages.begin(), test_case.corrected_messages.end(), index) == test_case.corrected_messages.end()) {
                    DOCTEST_CHECK(text == original[index]);
                    continue;
                }

                std::string signature;
                for (const char* p = text; *p != '\0'; ++p) {
                    if (*p == '%') {
                        ++p;
                        DOCTEST_REQUIRE((*p == 's' || *p == 'd'));
                        signature += *p;
                    }
                }
                const char* expected = "s";
                if (index >= 128) {
                    expected = "d";
                } else if (index == 126) {
                    expected = "ss";
                }
                DOCTEST_REQUIRE(signature == expected);

                char output[300];
                if (index >= 128) {
                    for (int value : {1, 25}) {
                        int length = snprintf(output, sizeof(output), text, value);
                        DOCTEST_REQUIRE(length > 0);
                        DOCTEST_REQUIRE(length < (int)sizeof(output));
                        DOCTEST_CHECK(std::string(output).find(std::to_string(value) + " BC") != std::string::npos);
                    }
                } else if (index == 126) {
                    int length = snprintf(output, sizeof(output), text, "Orion", "Captain");
                    DOCTEST_REQUIRE(length > 0);
                    DOCTEST_REQUIRE(length < (int)sizeof(output));
                    const std::string formatted(output);
                    DOCTEST_REQUIRE(formatted.find("Orion") != std::string::npos);
                    DOCTEST_REQUIRE(formatted.find("Captain") != std::string::npos);
                    DOCTEST_CHECK(formatted.find("Orion") < formatted.find("Captain"));
                } else {
                    int length = snprintf(output, sizeof(output), text, "Alkari");
                    DOCTEST_REQUIRE(length > 0);
                    DOCTEST_REQUIRE(length < (int)sizeof(output));
                    DOCTEST_CHECK(std::string(output).find("Alkari") != std::string::npos);
                }
            }
        }
    }

    DOCTEST_TEST_CASE("JSON5 overrides use caller supplied logical IDs and preserve missing strings") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("KENTEXT.LBX", {Records({"original"}, 100), Records({"unchanged"}, 100)});
        test_dir.Write_File("data/overrides/kentext.json5", "{/* 2: ignored */ 1: 'caf\\u00e9 // 2: text',}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        char output[100] = {};
        string_overrides::Load_Message("KENTEXT.LBX", 0, 0, 100, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "caf\xe9 // 2: text");
        string_overrides::Load_Message("kentext.lbx", 1, 0, 100, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "caf\xe9 // 2: text");
        string_overrides::Load_Message("kentext.lbx", 1, 0, 100, 2, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "unchanged");
        farload::Farload_Data_Static("KENTEXT.LBX", 0, output, 0, 1, 100);
        DOCTEST_CHECK(std::string(output) == "original");
    }

    DOCTEST_TEST_CASE("JSON5 escapes preserve every nonzero byte in messages and packed tables") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        const char* hex = "0123456789abcdef";
        std::string escaped;
        std::string expected;
        for (int byte = 1; byte <= 255; ++byte) {
            escaped += "\\u00";
            escaped += hex[byte >> 4];
            escaped += hex[byte & 15];
            expected += (char)byte;
        }
        test_dir.Write_File("data/custom.json5", "{1:'" + escaped + "'}");
        test_dir.Write_Lbx_File("custom.lbx", {Records({"original"}, 256)});
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        char output[256] = {};
        string_overrides::Load_Message("custom.lbx", 0, 0, 256, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output, 255) == expected);
        DOCTEST_CHECK(output[255] == '\0');
        std::vector<char> table = {'a', '\0', 'b', '\0'};
        string_overrides::Apply_Table("custom.lbx", &table, 2);
        expected.append("\0b\0", 3);
        DOCTEST_CHECK(std::string(table.data(), table.size()) == expected);
    }

    DOCTEST_TEST_CASE("literal UTF8 and JSON5 byte escapes have the same numeric values") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_File("data/custom.json5",
            "{1:'\xc2\x80\xc2\x90\xc3\xa9\xc3\xbf', 2:'\\u0080\\u0090\\u00e9\\u00ff',"
            "3:'\\x80\\x90\\xe9\\xff', 4:'\\u0090poca de Galaxia', 5:'\\n\\u0007\\u0018A0 #['}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        char output[32] = {};
        for (size_t id = 1; id <= 3; ++id) {
            string_overrides::Apply_String("custom.lbx", id, output, 5);
            DOCTEST_CHECK(std::string(output) == "\x80\x90\xe9\xff");
        }
        string_overrides::Apply_String("custom.lbx", 4, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "\x90poca de Galaxia");
        string_overrides::Apply_String("custom.lbx", 5, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "\n\a\x18"
                                             "A0 #[");
    }

    DOCTEST_TEST_CASE("non-byte strings fail atomically without using cached overrides") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        for (const auto& source : {
                 "{1:'\\u0000'}", "{1:'\\x00'}", "{1:'\\u0100'}", "{1:'\\u0131'}", "{1:'\\u20ac'}", "{1:'\\ud83d\\ude00'}", "{1:'\\ud800'}", "{1:'\xc4\x80'}", "{1:'\xc0\x80'}", "{1:'\xc2'}", "{1:'\xc3X'}", "{1:'\x90'}"}) {
            test_dir.Write_File("data/custom.json5", "{1:'\\u0080'}");
            char output[16] = {};
            string_overrides::Apply_String("custom.lbx", 1, output, sizeof(output));
            DOCTEST_CHECK(std::string(output) == "\x80");
            test_dir.Write_File("data/custom.json5", source);
            for (int repeat = 0; repeat < 2; ++repeat) {
                strlcpy(output, "original", sizeof(output));
                std::string diagnostic;
                DOCTEST_CHECK_FALSE(string_overrides::Try_Apply_String("custom.lbx", 1, output, sizeof(output), &diagnostic));
                DOCTEST_CHECK(diagnostic.find("custom.json5:") == 0);
                DOCTEST_CHECK(std::string(output) == "original");
                std::vector<char> table = {'a', '\0'};
                const auto original = table;
                DOCTEST_CHECK_FALSE(string_overrides::Try_Apply_Table("custom.lbx", &table, 1, 1, false, &diagnostic));
                DOCTEST_CHECK(table == original);
            }
        }
    }

    DOCTEST_TEST_CASE("packed overrides resize strings and retain positional empty strings") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        std::string packed("old\0\0third\0", 11);
        packed.append(809, '\0');
        test_dir.Write_Lbx_File("estrings.lbx", {Records({packed}, 21000)});
        test_dir.Write_File("data/estrings.json5", "{1: 'longer replacement', '3': '', 812: 'last'}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        std::vector<char> output(21000);
        farload::Farload_Data_Static("estrings.lbx", 0, output.data(), 0, 1, (int32_t)output.size());
        string_overrides::Apply_Table("estrings.lbx", &output, 812);
        DOCTEST_CHECK(std::string(output.data()) == "longer replacement");
        const char* cursor = output.data() + strlen(output.data()) + 1;
        DOCTEST_CHECK(*cursor == '\0');
        ++cursor;
        DOCTEST_CHECK(*cursor == '\0');
        for (int i = 2; i < 811; ++i) cursor += strlen(cursor) + 1;
        DOCTEST_CHECK(std::string(cursor) == "last");
    }

    DOCTEST_TEST_CASE("record overrides use explicit group offsets") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("skildesc.lbx", {Records({"first", "second"}, 38), Records({"description"}, 255)});
        test_dir.Write_File("data/skildesc.json5", "{2: 'changed', 3: 'new description'}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        char output[255] = {};
        farload::Farload_Data_Static("skildesc.lbx", 0, output, 1, 1, 38);
        string_overrides::Apply_String("skildesc.lbx", 2, output, 38);
        DOCTEST_CHECK(std::string(output) == "changed");
        farload::Farload_Data_Static("skildesc.lbx", 1, output, 0, 1, 255);
        string_overrides::Apply_String("skildesc.lbx", 3, output, 255);
        DOCTEST_CHECK(std::string(output) == "new description");
    }

    DOCTEST_TEST_CASE("raw custom tables receive overrides before pointer parsing") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        std::string packed;
        for (int i = 0; i < 64; ++i) packed.append("name\0\0", 6);
        test_dir.Write_Lbx_File("racestuf.lbx", {packed});
        test_dir.Write_File("data/racestuf.json5", "{1: 'new name', 64: 'last'}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        auto* output = string_overrides::Load_Raw_Table("racestuf.lbx", 0, nullptr, 0, 64, 1, true);
        DOCTEST_CHECK(std::string(output) == "new name");
        const char* cursor = output;
        for (int i = 0; i < 63; ++i) cursor += strlen(cursor) + 1;
        DOCTEST_CHECK(std::string(cursor) == "last");
        std::free(output);
    }

    DOCTEST_TEST_CASE("invalid sidecars and oversized strings fail without overflowing") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("shipname.lbx", {Records({"old"}, 16)});
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        for (const auto& source : {"{0:'bad'}", "{1:42}", "{1:'a', '1':'b'}", "{1:'\\u0000'}", "[]", "{", "{1:'1234567890123456'}"}) {
            test_dir.Write_File("data/shipname.json5", source);
            string_overrides::Reset_Cache();
            char output[17] = {};
            output[16] = 'X';
            std::string diagnostic;
            DOCTEST_CHECK_FALSE(string_overrides::Try_Apply_String("shipname.lbx", 1, output, 16, &diagnostic));
            DOCTEST_CHECK(diagnostic.find("shipname.json5:") == 0);
            DOCTEST_CHECK(output[16] == 'X');
        }
    }

    DOCTEST_TEST_CASE("sidecars follow VFS precedence changes without cache resets") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_File("data/mods/text/data/custom.json5", "{1: 'package'}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        char output[32] = "original";
        string_overrides::Apply_String("custom.lbx", 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "original");
        auto* transaction = vfs::Begin_Mount_Transaction();
        std::string diagnostic;
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "text", &diagnostic));
        DOCTEST_REQUIRE(vfs::Commit_Mount_Transaction(transaction));
        string_overrides::Apply_String("custom.lbx", 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "package");
        test_dir.Write_File("data/overrides/custom.json5", "{1: 'local'}");
        string_overrides::Apply_String("custom.lbx", 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "local");
        string_overrides::Reset_Cache();
        DOCTEST_CHECK(std::string(output) == "local");
        string_overrides::Apply_String("custom.lbx", 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "local");

        std::error_code error;
        DOCTEST_REQUIRE(fs::remove(test_dir.root_path() / "data/overrides/custom.json5", error));
        DOCTEST_REQUIRE_FALSE(error);
        string_overrides::Apply_String("custom.lbx", 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "package");
        transaction = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(vfs::Commit_Mount_Transaction(transaction));
        strlcpy(output, "original", sizeof(output));
        string_overrides::Apply_String("custom.lbx", 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "original");
    }

    DOCTEST_TEST_CASE("JSON5 sidecars merge per string across directory and ZIP packages") {
        using namespace test_farload_buffer_detail;
        for (bool upper_zip : {false, true}) {
            for (bool lower_zip : {false, true}) {
                DOCTEST_CAPTURE(upper_zip);
                DOCTEST_CAPTURE(lower_zip);
                Scoped_Test_Directory test_dir;
                const std::vector<std::string> originals = {"base one", "base two", "base three", "base four"};
                std::string packed;
                for (const auto& text : originals) {
                    packed += text;
                    packed.push_back('\0');
                }
                test_dir.Write_Lbx_File("custom.lbx", {Records(originals, 32), packed});
                const auto package = [&](const char* name, bool zip, const char* json) {
                    const std::string path = std::string("data/mods/") + name;
                    if (zip) {
                        test_dir.Write_Zip_File(path + ".zip", {{"data/custom.json5", json}});
                    } else {
                        test_dir.Write_File(path + "/data/custom.json5", json);
                    }
                };
                package("upper", upper_zip, "{1:'upper',3:''}");
                package("lower", lower_zip, "{1:'lower',2:'lower only',3:'lower third'}");
                DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()));
                const auto message = [&](size_t id) {
                    char output[32] = {};
                    string_overrides::Load_Message("custom.lbx", 0, (uint32_t)id - 1, 32, id, output, sizeof(output));
                    return std::string(output);
                };
                Mount_Packages({"upper", "lower"});
                DOCTEST_CHECK(message(1) == "upper");
                DOCTEST_CHECK(message(2) == "lower only");
                DOCTEST_CHECK(message(3).empty());
                DOCTEST_CHECK(message(4) == "base four");
                std::vector<char> table(packed.begin(), packed.end());
                string_overrides::Apply_Table("custom.lbx", &table, 4);
                const std::string expected("upper\0lower only\0\0base four\0", 28);
                DOCTEST_CHECK(std::string(table.data(), table.size()) == expected);
                char* raw = string_overrides::Load_Raw_Table("custom.lbx", 1, nullptr, 0, 4);
                DOCTEST_REQUIRE(raw != nullptr);
                DOCTEST_CHECK(std::string(raw) == "upper");
                DOCTEST_CHECK(std::string(raw + strlen(raw) + 1) == "lower only");
                std::free(raw);
                Mount_Packages({"lower", "upper"});
                DOCTEST_CHECK(message(1) == "lower");
                DOCTEST_CHECK(message(2) == "lower only");
                DOCTEST_CHECK(message(3) == "lower third");
                Mount_Packages({"upper"});
                DOCTEST_CHECK(message(1) == "upper");
                DOCTEST_CHECK(message(2) == "base two");
                DOCTEST_CHECK(message(3).empty());
                auto* transaction = vfs::Begin_Mount_Transaction();
                std::string diagnostic;
                DOCTEST_REQUIRE(transaction != nullptr);
                DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "lower", &diagnostic));
                DOCTEST_CHECK(message(1) == "lower");
                DOCTEST_CHECK(message(2) == "lower only");
                DOCTEST_REQUIRE(vfs::Begin_Mount_Transaction_Commit(transaction));
                DOCTEST_CHECK(message(2) == "lower only");
                vfs::Rollback_Mount_Transaction_Commit(transaction);
                vfs::Discard_Mount_Transaction(transaction);
                DOCTEST_CHECK(message(1) == "upper");
                DOCTEST_CHECK(message(2) == "base two");
            }
        }
    }

    DOCTEST_TEST_CASE("JSON5 strings merge through every VFS source without exposing private package paths") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("fallback/custom.lbx", {Records(std::vector<std::string>(7, "original"), 32)});
        test_dir.Write_File("data/overrides/custom.json5", "{1:'loose'}");
        test_dir.Write_Zip_File("data/mods/upper.zip", {{"data/custom.json5", "{1:'package conflict',2:'upper'}"}});
        test_dir.Write_File("data/mods/lower/data/custom.json5", "{2:'lower conflict',3:'lower'}");
        test_dir.Write_Zip_File("data/base.zip", {{"custom.json5", "{4:'base ZIP'}"}});
        test_dir.Write_File("data/custom.json5", "{5:'data'}");
        test_dir.Write_File("custom.json5", "{6:'raw'}");
        test_dir.Write_File("fallback/custom.json5", "{7:'fallback'}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()));
        DOCTEST_REQUIRE(vfs::Set_Raw_Fallback_Path((test_dir.root_path() / "fallback").string().c_str()));
        Mount_Packages({"upper", "lower"});
        const char* expected[] = {"loose", "upper", "lower", "base ZIP", "data", "raw", "fallback"};
        for (size_t id = 1; id <= std::size(expected); ++id) {
            DOCTEST_CAPTURE(id);
            char output[32] = {};
            string_overrides::Load_Message("custom.lbx", 0, (uint32_t)id - 1, 32, id, output, sizeof(output));
            DOCTEST_CHECK(std::string(output) == expected[id - 1]);
        }
        char output[32] = "original";
        string_overrides::Apply_String("mods/upper/data/custom.lbx", 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "package conflict");
        strlcpy(output, "original", sizeof(output));
        string_overrides::Apply_String("mods/upper/data/custom.lbx", 3, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "original");
    }

    DOCTEST_TEST_CASE("merged sidecars stop at a whole LBX and include JSON from that same layer") {
        using namespace test_farload_buffer_detail;
        for (bool middle_zip : {false, true}) {
            DOCTEST_CAPTURE(middle_zip);
            Scoped_Test_Directory test_dir;
            test_dir.Write_Lbx_File("custom.lbx", {Records({"base one", "base two", "base three"}, 32)});
            const fs::path middle_path = "data/mods/middle/data/custom.lbx";
            test_dir.Write_Lbx_File(middle_path, {Records({"middle one", "middle two", "middle three"}, 32)});
            if (middle_zip) {
                const auto lbx = Read_File_Contents(test_dir.root_path() / middle_path);
                fs::remove_all(test_dir.root_path() / "data/mods/middle");
                test_dir.Write_Zip_File("data/mods/middle.zip",
                    {{"data/custom.lbx", lbx}, {"data/custom.json5", "{3:'middle JSON'}"}});
            } else {
                test_dir.Write_File("data/mods/middle/data/custom.json5", "{3:'middle JSON'}");
            }
            test_dir.Write_Zip_File("data/mods/upper.zip", {{"data/custom.json5", "{1:'upper'}"}});
            test_dir.Write_File("data/mods/lower/data/custom.json5", "{2:42}");
            DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()));
            const auto message = [&](size_t id) {
                char output[32] = {};
                string_overrides::Load_Message("custom.lbx", 0, (uint32_t)id - 1, 32, id, output, sizeof(output));
                return std::string(output);
            };
            Mount_Packages({"upper", "middle", "lower"});
            DOCTEST_CHECK(message(1) == "upper");
            DOCTEST_CHECK(message(2) == "middle two");
            DOCTEST_CHECK(message(3) == "middle JSON");
            Mount_Packages({"middle", "lower"});
            DOCTEST_CHECK(message(1) == "middle one");
            DOCTEST_CHECK(message(2) == "middle two");
            DOCTEST_CHECK(message(3) == "middle JSON");
            Mount_Packages({"upper", "lower", "middle"});
            char output[32] = "unchanged";
            std::string diagnostic;
            DOCTEST_CHECK_FALSE(string_overrides::Try_Apply_String("custom.lbx", 1, output, sizeof(output), &diagnostic));
            DOCTEST_CHECK(std::string(output) == "unchanged");
            Mount_Packages({"upper", "middle", "lower"});
            DOCTEST_CHECK(message(2) == "middle two");
            DOCTEST_CHECK(message(3) == "middle JSON");
        }
    }

    DOCTEST_TEST_CASE("merged sidecar caches track lower-layer edits errors removal and recreation") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("custom.lbx", {Records({"base one", "base two"}, 32)});
        test_dir.Write_File("data/mods/upper/data/custom.json5", "{1:'upper'}");
        const fs::path lower_path = "data/mods/lower/data/custom.json5";
        test_dir.Write_File(lower_path, "{2:'first'}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()));
        Mount_Packages({"upper", "lower"});
        const auto message = [&]() {
            char output[32] = {};
            string_overrides::Load_Message("custom.lbx", 0, 1, 32, 2, output, sizeof(output));
            return std::string(output);
        };
        DOCTEST_CHECK(message() == "first");
        const auto timestamp = fs::last_write_time(test_dir.root_path() / lower_path);
        test_dir.Write_File(lower_path, "{2:'other'}");
        fs::last_write_time(test_dir.root_path() / lower_path, timestamp);
        DOCTEST_CHECK(message() == "other");
        test_dir.Write_File(lower_path, "{2:42}");
        char output[32] = "unchanged";
        std::string diagnostic;
        for (int repeat = 0; repeat < 2; ++repeat) {
            DOCTEST_CHECK_FALSE(string_overrides::Try_Apply_String("custom.lbx", 1, output, sizeof(output), &diagnostic));
            DOCTEST_CHECK(std::string(output) == "unchanged");
            std::vector<char> table = {'a', '\0'};
            const auto original = table;
            DOCTEST_CHECK_FALSE(string_overrides::Try_Apply_Table("custom.lbx", &table, 1, 1, false, &diagnostic));
            DOCTEST_CHECK(table == original);
        }
        fs::remove(test_dir.root_path() / lower_path);
        DOCTEST_CHECK(message() == "base two");
        test_dir.Write_File(lower_path, "{2:'again'}");
        DOCTEST_CHECK(message() == "again");
    }

    DOCTEST_TEST_CASE("higher package LBX files override lower JSON5 sidecars across mount changes") {
        using namespace test_farload_buffer_detail;
        for (bool lbx_zip : {false, true}) {
            for (bool json_zip : {false, true}) {
                DOCTEST_CAPTURE(lbx_zip);
                DOCTEST_CAPTURE(json_zip);
                Scoped_Test_Directory test_dir;
                test_dir.Write_Lbx_File("TECHDESC.LBX", {"", Records({"base description"}, 100)});
                const std::string json = "{1:'+20 to marine defense'}";
                if (json_zip) {
                    test_dir.Write_Zip_File("data/mods/v150.zip", {{"data/techdesc.json5", json}});
                } else {
                    test_dir.Write_File("data/mods/v150/data/techdesc.json5", json);
                }
                const fs::path lbx_path = "data/mods/vdc/data/TECHDESC.LBX";
                test_dir.Write_Lbx_File(lbx_path, {"", Records({"+30% to marine defense"}, 100)});
                if (lbx_zip) {
                    const std::string lbx = Read_File_Contents(test_dir.root_path() / lbx_path);
                    fs::remove_all(test_dir.root_path() / "data/mods/vdc");
                    test_dir.Write_Zip_File("data/mods/vdc.zip", {{"data/TECHDESC.LBX", lbx}});
                }
                DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()));
                const auto mount = [&](const std::vector<std::string>& packages) {
                    auto* transaction = vfs::Begin_Mount_Transaction();
                    DOCTEST_REQUIRE(transaction != nullptr);
                    std::string diagnostic;
                    for (const auto& package : packages) {
                        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, package.c_str(), &diagnostic));
                    }
                    DOCTEST_REQUIRE(vfs::Commit_Mount_Transaction(transaction));
                };
                const auto description = [&]() {
                    char output[100] = {};
                    farload::Farload_Data_Static("TECHDESC.LBX", 1, output, 0, 1, 100);
                    string_overrides::Apply_String("TECHDESC.LBX", 1, output, sizeof(output));
                    return std::string(output);
                };
                mount({"v150"});
                DOCTEST_CHECK(description() == "+20 to marine defense");
                mount({"vdc", "v150"});
                DOCTEST_CHECK(description() == "+30% to marine defense");
                mount({"v150", "vdc"});
                DOCTEST_CHECK(description() == "+20 to marine defense");
                for (bool rollback : {false, true}) {
                    auto* transaction = vfs::Begin_Mount_Transaction();
                    DOCTEST_REQUIRE(transaction != nullptr);
                    std::string diagnostic;
                    DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "vdc", &diagnostic));
                    DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "v150", &diagnostic));
                    DOCTEST_CHECK(description() == "+30% to marine defense");
                    if (rollback) {
                        DOCTEST_REQUIRE(vfs::Begin_Mount_Transaction_Commit(transaction));
                        DOCTEST_CHECK(description() == "+30% to marine defense");
                        vfs::Rollback_Mount_Transaction_Commit(transaction);
                    }
                    vfs::Discard_Mount_Transaction(transaction);
                    DOCTEST_CHECK(description() == "+20 to marine defense");
                }
                mount({"vdc", "v150"});
                DOCTEST_CHECK(description() == "+30% to marine defense");
                mount({"v150"});
                DOCTEST_CHECK(description() == "+20 to marine defense");
            }
        }
    }

    DOCTEST_TEST_CASE("whole LBX precedence applies to loose overrides base ZIPs data and raw fallback") {
        using namespace test_farload_buffer_detail;
        struct s_case {
            const char* lbx_path;
            const char* json_path;
            bool base_zip;
        };
        const s_case cases[] = {
            {"data/overrides/custom.lbx", "data/custom.json5", false},
            {"data/custom.lbx", "custom.json5", false},
            {"custom.lbx", "fallback/custom.json5", false},
            {"custom.lbx", "data/custom.json5", true},
        };
        for (const auto& value : cases) {
            DOCTEST_CAPTURE(value.lbx_path);
            DOCTEST_CAPTURE(value.json_path);
            Scoped_Test_Directory test_dir;
            test_dir.Write_Lbx_File(value.lbx_path, {Records({"higher LBX"}, 32)});
            test_dir.Write_File(value.json_path, "{1:'lower JSON'}");
            if (value.base_zip) {
                const auto lbx = Read_File_Contents(test_dir.root_path() / value.lbx_path);
                test_dir.Write_Zip_File("data/base.zip", {{"custom.lbx", lbx}});
            }
            DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()));
            DOCTEST_REQUIRE(vfs::Set_Raw_Fallback_Path((test_dir.root_path() / "fallback").string().c_str()));
            char output[32] = {};
            string_overrides::Load_Message("custom.lbx", 0, 0, 32, 1, output, sizeof(output));
            DOCTEST_CHECK(std::string(output) == "higher LBX");
        }
    }

    DOCTEST_TEST_CASE("shadowed JSON is ignored by every string loader while same-layer JSON still applies") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("data/overrides/custom.lbx",
            {std::string("a\0b\0", 4), Records({"first", "second"}, 32)});
        test_dir.Write_File("data/custom.json5", "{1:42}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()));
        std::string diagnostic;
        char output[32] = "first";
        DOCTEST_REQUIRE(string_overrides::Try_Apply_String("custom.lbx", 1, output, sizeof(output), &diagnostic));
        DOCTEST_CHECK(std::string(output) == "first");
        std::vector<char> table = {'a', '\0', 'b', '\0'};
        DOCTEST_REQUIRE(string_overrides::Try_Apply_Table("custom.lbx", &table, 2, 1, false, &diagnostic));
        DOCTEST_CHECK((table == std::vector<char>{'a', '\0', 'b', '\0'}));
        char* raw = string_overrides::Load_Raw_Table("custom.lbx", 0, nullptr, 0, 2);
        DOCTEST_REQUIRE(raw != nullptr);
        DOCTEST_CHECK(std::string(raw) == "a");
        std::free(raw);
        string_overrides::Load_Message("custom.lbx", 1, 0, 32, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "first");

        test_dir.Write_File("data/overrides/custom.json5", "{1:'same layer'}");
        string_overrides::Load_Message("custom.lbx", 1, 0, 32, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "same layer");
        string_overrides::Load_Message("custom.lbx", 1, 1, 32, 2, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "second");
        fs::remove(test_dir.root_path() / "data/overrides/custom.json5");
        string_overrides::Load_Message("custom.lbx", 1, 0, 32, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "first");
        fs::remove(test_dir.root_path() / "data/overrides/custom.lbx");
        DOCTEST_CHECK_FALSE(string_overrides::Try_Apply_String("custom.lbx", 1, output, sizeof(output), &diagnostic));
        DOCTEST_CHECK(diagnostic.find("expected positive decimal string IDs and string values") != std::string::npos);
    }

    DOCTEST_TEST_CASE("sidecar edits removal and recreation are detected by content") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("custom.lbx", {Records({"original"}, 32)});
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        char output[32] = {};
        string_overrides::Load_Message("custom.lbx", 0, 0, 32, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "original");
        test_dir.Write_File("data/custom.json5", "{1:'first'}");
        string_overrides::Load_Message("custom.lbx", 0, 0, 32, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "first");
        std::error_code error;
        const auto path = test_dir.root_path() / "data/custom.json5";
        const auto timestamp = fs::last_write_time(path, error);
        DOCTEST_REQUIRE_FALSE(error);
        test_dir.Write_File("data/custom.json5", "{1:'other'}");
        fs::last_write_time(path, timestamp, error);
        DOCTEST_REQUIRE_FALSE(error);
        for (int i = 0; i < 2; ++i) {
            string_overrides::Load_Message("custom.lbx", 0, 0, 32, 1, output, sizeof(output));
            DOCTEST_CHECK(std::string(output) == "other");
        }
        DOCTEST_REQUIRE(fs::remove(path, error));
        DOCTEST_REQUIRE_FALSE(error);
        string_overrides::Load_Message("custom.lbx", 0, 0, 32, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "original");
        test_dir.Write_File("data/custom.json5", "{1:'again'}");
        string_overrides::Load_Message("custom.lbx", 0, 0, 32, 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "again");
    }

    DOCTEST_TEST_CASE("invalid changed sidecars never serve stale overrides") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_File("data/custom.json5", "{1:'valid'}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        char output[32] = "original";
        string_overrides::Apply_String("custom.lbx", 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "valid");
        for (const auto& source : {"{1:'partial', 2:42}", ""}) {
            test_dir.Write_File("data/custom.json5", source);
            for (int i = 0; i < 2; ++i) {
                strlcpy(output, "original", sizeof(output));
                std::string diagnostic;
                DOCTEST_CHECK_FALSE(string_overrides::Try_Apply_String("custom.lbx", 1, output, sizeof(output), &diagnostic));
                DOCTEST_CHECK(diagnostic.find("custom.json5:") == 0);
                DOCTEST_CHECK(std::string(output) == "original");
            }
        }
        test_dir.Write_File("data/custom.json5", "{1:'fixed'}");
        string_overrides::Apply_String("custom.lbx", 1, output, sizeof(output));
        DOCTEST_CHECK(std::string(output) == "fixed");
    }

    DOCTEST_TEST_CASE("sidecar cache follows ZIP contents after VFS reinitialization") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Zip_File("data/base.zip", {{"custom.json5", "{1:'first'}"}});
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        std::vector<char> table = {'o', '\0'};
        string_overrides::Apply_Table("custom.lbx", &table, 1);
        DOCTEST_CHECK(std::string(table.data()) == "first");
        vfs::Shutdown();
        test_dir.Write_Zip_File("data/base.zip", {{"custom.json5", "{1:'other'}"}});
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        table = {'o', '\0'};
        string_overrides::Apply_Table("custom.lbx", &table, 1);
        DOCTEST_CHECK(std::string(table.data()) == "other");
    }

    DOCTEST_TEST_CASE("table failures are atomic and paths retain their directory") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_File("data/text/custom.json5", "{1:'replacement'}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        std::vector<char> table = {'a', '\0', 'b'};
        const auto original = table;
        std::string diagnostic;
        DOCTEST_CHECK_FALSE(string_overrides::Try_Apply_Table("TEXT/CUSTOM.LBX", &table, 2, 1, false, &diagnostic));
        DOCTEST_CHECK(diagnostic.find("unterminated") != std::string::npos);
        DOCTEST_CHECK(table == original);
        table.push_back('\0');
        string_overrides::Apply_Table("custom.lbx", &table, 2);
        DOCTEST_CHECK(std::string(table.data()) == "a");
    }

    DOCTEST_TEST_CASE("raw tables allocate their final size without overwriting neighboring pool blocks") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("custom.lbx", {std::string("a\0b\0", 4)});
        const std::string replacement(70000, 'L');
        test_dir.Write_File("data/custom.json5", "{1:'" + replacement + "'}");
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        Scoped_Buffer_Pool pool(80000);
        auto* before = (char*)allocate::Allocate_Next_Block_No_Header(pool.payload, 16);
        memset(before, 'B', 16);
        char* table = string_overrides::Load_Raw_Table("custom.lbx", 0, pool.payload, 2, 2);
        DOCTEST_CHECK(std::string(table) == replacement);
        DOCTEST_CHECK(std::string(table + replacement.size() + 1) == "b");
        auto* after = (char*)allocate::Allocate_Next_Block_No_Header(pool.payload, 16);
        DOCTEST_REQUIRE(after != nullptr);
        memset(after, 'A', 16);
        DOCTEST_CHECK(std::string(before, 16) == std::string(16, 'B'));
        DOCTEST_CHECK(std::string(table) == replacement);
        DOCTEST_CHECK(after >= table + replacement.size() + 3);
    }

    DOCTEST_TEST_CASE("dedicated table pools grow and can reload shorter strings") {
        using namespace test_farload_buffer_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("custom.lbx", {std::string("a\0b\0", 4)});
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        void* pool = allocate::Allocate_Space(4);
        for (size_t length : {size_t(40000), size_t(80000), size_t(2)}) {
            const std::string replacement(length, 'X');
            test_dir.Write_File("data/custom.json5", "{1:'" + replacement + "'}");
            char* table = string_overrides::Reload_Raw_Table("custom.lbx", 0, &pool, 2);
            DOCTEST_CHECK(std::string(table) == replacement);
            DOCTEST_CHECK(std::string(table + length + 1) == "b");
            DOCTEST_CHECK(allocate::Get_Max_Block_Space(pool) >= length + 3);
        }
        free((allocate::s_alloc_header*)pool - 1);
    }

    DOCTEST_TEST_CASE("cache invalidation changes resource provenance without moving the arena") {
        test_farload_buffer_detail::Scoped_Test_Directory test_dir;
        test_dir.Write_File("data/TEST.LBX/000", std::string(16, 'B'));
        test_dir.Write_File("data/mods/visual/data/TEST.LBX/000", std::string(31, 'M'));
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        void* pool = buffer::Allocate_Buffer_Space(sizeof(s_file_buffer_entry) * 50 + 1024);
        void* metadata = buffer::file_buffer;
        auto* header = (allocate::s_alloc_header*)pool - 1;
        for (int repeat = 0; repeat < 30; ++repeat) {
            std::string diagnostic;
            auto* transaction = vfs::Begin_Mount_Transaction();
            DOCTEST_REQUIRE(transaction != nullptr);
            if (repeat % 2 != 0) {
                DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, "visual", &diagnostic));
            }
            DOCTEST_REQUIRE(vfs::Commit_Mount_Transaction(transaction));
            buffer::Reset_Buffer_Cache();
            DOCTEST_CHECK(buffer::buffer_space_seg == pool);
            DOCTEST_CHECK(buffer::file_buffer == metadata);
            DOCTEST_CHECK(header->current == 0);
            DOCTEST_CHECK(buffer::file_buffer_count == 0);
            DOCTEST_CHECK(buffer::global_load_count == 0);
            const size_t size = repeat % 2 != 0 ? 31 : 16;
            const char expected = repeat % 2 != 0 ? 'M' : 'B';
            void* loaded = buffer::Buffer_Reload("TEST.LBX", 0, pool);
            DOCTEST_CHECK(std::string((char*)loaded, size) == std::string(size, expected));
            DOCTEST_CHECK(header->current == allocate::Nested_No_Header_Pool_Size((uint32_t)size));
            DOCTEST_CHECK(buffer::Buffer_Reload("test.lbx", 0, pool) == loaded);
            const auto cursor = header->current;
            buffer::Flush_Buffer_Space(nullptr);
            DOCTEST_CHECK(header->current == cursor);
            DOCTEST_CHECK(buffer::file_buffer[0].segments[0].is_loaded == 1);
        }
        buffer::Free_Buffer_Space();
        buffer::Reset_Buffer_Cache();
        DOCTEST_CHECK(buffer::buffer_space_seg == nullptr);
    }

    DOCTEST_TEST_CASE("farload library loads an LBX entry through vfs per-entry override") {
        test_farload_buffer_detail::Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("TEST.LBX", {"raw-zero", "raw-one"});
        test_dir.Write_File(test_farload_buffer_detail::fs::path("data") / "overrides" / "TEST.LBX" / "001", "override-one");

        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        char pool[32] = {};
        void* loaded = farload::Farload_Library("TEST.LBX", 1, pool, 0);
        DOCTEST_REQUIRE(loaded != nullptr);
        DOCTEST_CHECK(std::string((const char*)loaded, 12) == "override-one");
        std::free(loaded);
    }

    DOCTEST_TEST_CASE("farload library data reads structured records from whole-LBX zip replacement") {
        test_farload_buffer_detail::Scoped_Test_Directory test_dir;

        std::string structured_entry;
        uint16_t total_count = 3;
        uint16_t element_size = 2;
        structured_entry.append((const char*)&total_count, sizeof(total_count));
        structured_entry.append((const char*)&element_size, sizeof(element_size));
        structured_entry.push_back('A');
        structured_entry.push_back('1');
        structured_entry.push_back('B');
        structured_entry.push_back('2');
        structured_entry.push_back('C');
        structured_entry.push_back('3');

        test_dir.Write_Lbx_File("ZIPSRC.LBX", {structured_entry});
        std::string zip_lbx = test_farload_buffer_detail::Read_File_Contents(test_dir.root_path() / "ZIPSRC.LBX");
        test_dir.Write_Zip_File(test_farload_buffer_detail::fs::path("data") / "base.zip", {
                                                                                               {"TESTDATA.LBX", zip_lbx},
                                                                                           });

        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        char output[4] = {};
        void* loaded = farload::Farload_Library_Data("TESTDATA.LBX", 0, output, 3, 1, 2, 2);
        DOCTEST_REQUIRE(loaded == output);
        DOCTEST_CHECK(std::string(output, 4) == "B2C3");
        DOCTEST_CHECK(farload::Get_Farload_Data_Record_Count("TESTDATA.LBX", 0) == 3);
    }

    DOCTEST_TEST_CASE("buffer farload library uses vfs lbx entry resolution") {
        test_farload_buffer_detail::Scoped_Test_Directory test_dir;
        test_dir.Write_Lbx_File("BUFFER.LBX", {"raw-entry"});

        test_dir.Write_Lbx_File("BUFFERZIP.LBX", {"zip-entry"});
        std::string zip_lbx = test_farload_buffer_detail::Read_File_Contents(test_dir.root_path() / "BUFFERZIP.LBX");
        test_dir.Write_Zip_File(test_farload_buffer_detail::fs::path("data") / "base.zip", {
                                                                                               {"BUFFER.LBX", zip_lbx},
                                                                                           });

        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        test_farload_buffer_detail::Scoped_Buffer_Pool pool(64);
        void* loaded = buffer::Farload_Buffer_Library("BUFFER.LBX", 0, pool.payload);
        DOCTEST_REQUIRE(loaded != nullptr);
        DOCTEST_CHECK(std::string((const char*)loaded, 9) == "zip-entry");
    }
}