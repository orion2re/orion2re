#include "pch.h"

#include "doctest/doctest.h"
#include "game/config_refresh.h"
#include "game/loader.h"
#include "config/config_loader.h"
#include "game/savegame.h"
#include "game/ship_read.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include "miniz.h"

namespace {
    namespace fs = std::filesystem;

    struct Fixture {
        fs::path root = fs::temp_directory_path() /
            ("orion-resource-reload-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::vector<s_leader_data> leaders{MAX_LEADERS};
        std::vector<s_player> players{MAX_PLAYERS};
        std::vector<s_ship_data> ships{MAX_SHIPS};

        Fixture() {
            platform::Reset_Exit();
            fs::create_directories(root);
            MOX::settings = {};
            MOX::leaders = leaders.data();
            MOX::player = players.data();
            MOX::ship = ships.data();
            MOX::NUM_PLAYERS = 1;
            MOX::NUM_SHIPS = 1;
            MOX::global_screen_seg = allocate::Allocate_Space(allocate::Global_Screen_Space());
            MOX::screen_seg = (s_animation_header*)MOX::global_screen_seg;
            MOX::global_cache_seg = buffer::Allocate_Buffer_Space(0x174508);
            Write_Resources("data/", 'B', 0);
            Write_Resources("data/mods/visual/data/", 'M', 7);
            Write_Resources("data/mods/alternate/data/", 'A', 3);
            Write("data/mods/visual/mod.json5",
                R"({id:'visual',name:'Visual',version:'1.0.0',requires:{engine:'>=2.0.0'},config:'config/main.cfg'})");
            Write("data/mods/alternate/mod.json5",
                R"({id:'alternate',name:'Alternate',version:'1.0.0',requires:{engine:'>=2.0.0'}})");
            Write("data/mods/visual/config/main.cfg", "interface print_research_value = percentage1;\n");
            Write("data/orion2re.cfg", "interface print_research_value = classic;\n");
            DOCTEST_REQUIRE(vfs::Init(root.string().c_str()));
        }

        ~Fixture() {
            CONFIG_REFRESH::Resources_Unloaded();
            config::Shutdown_Config_Loader();
            LOADER::Free_Pictures();
            buffer::Free_Buffer_Space();
            free((allocate::s_alloc_header*)MOX::global_screen_seg - 1);
            MOX::screen_seg = nullptr;
            MOX::global_screen_seg = nullptr;
            MOX::leaders = nullptr;
            MOX::player = nullptr;
            MOX::ship = nullptr;
            vfs::Shutdown();
            std::error_code error;
            fs::remove_all(root, error);
        }

        void Write(const std::string& path, const std::string& bytes) {
            const auto full = root / path;
            fs::create_directories(full.parent_path());
            std::ofstream out(full, std::ios::binary);
            out.write(bytes.data(), (std::streamsize)bytes.size());
            DOCTEST_REQUIRE(out.good());
        }

        static std::string Record(const std::string& bytes, uint16_t count = 1, uint16_t width = 0) {
            if (width == 0) width = (uint16_t)bytes.size();
            std::string result((char*)&count, sizeof(count));
            result.append((char*)&width, sizeof(width));
            result += bytes;
            return result;
        }

        static std::string Table(size_t size, int count, char label) {
            std::string result;
            for (int i = 0; i < count; ++i) {
                result += std::string(1, label) + std::to_string(i);
                result.push_back('\0');
            }
            result.resize(size, '\0');
            return Record(result);
        }

        void Write_Resources(const std::string& prefix, char label, uint32_t extra) {
            Write(prefix + "ESTRINGS.LBX/000", Table(21000, 812, label));
            Write(prefix + "HESTRNGS.LBX/000", Table(0x34D1, 397, label));
            Write(prefix + "RSTRING0.LBX/000", Table(6500, 182, label));
            for (int message : {3, 4, 5, 6, 31, 32, 33, 34, 35}) {
                for (int language = 0; language < 6; ++language) {
                    char entry[4];
                    snprintf(entry, sizeof(entry), "%03d", message * 6 + language);
                    std::string text = std::string(1, label) + std::to_string(message);
                    text.resize(100, '\0');
                    Write(prefix + "KENTEXT.LBX/" + entry, Record(text));
                }
            }
            for (int language = 0; language < 6; ++language) {
                char entry[4];
                snprintf(entry, sizeof(entry), "%03d", 24 * 6 + language);
                std::string text = std::string(1, label) + " and";
                text.resize(100, '\0');
                Write(prefix + "KENTEXT1.LBX/" + entry, Record(text));
            }
            for (int message = 0x21; message <= 0x3C; ++message) {
                if (message > 0x31 && message < 0x39) continue;
                for (int language = 0; language < 6; ++language) {
                    char entry[4];
                    snprintf(entry, sizeof(entry), "%03d", message * 6 + language);
                    std::string text = std::string(1, label) + std::to_string(message);
                    text.resize(100, '\0');
                    Write(prefix + "BILLTEXT.LBX/" + entry, Record(text));
                }
            }
            std::string titles(MAX_LEADERS * 59, '\0');
            for (int i = 0; i < MAX_LEADERS; ++i) titles[i * 59 + 15] = label;
            Write(prefix + "HERODATA.LBX/000", Record(titles, MAX_LEADERS, 59));
            std::string font(64 + sizeof(s_font) + 32, '\0');
            s_font header{};
            header.font_heights[0] = label == 'M' ? 13 : 8;
            memcpy(font.data() + 64, &header, sizeof(header));
            Write(prefix + "FONTS.LBX/000", font);
            for (int i = 1; i < 14; ++i) {
                char entry[4];
                snprintf(entry, sizeof(entry), "%03d", i);
                Write(prefix + "FONTS.LBX/" + entry, std::string(1600, label));
            }
            auto picture = [&](const char* file, int entry) {
                char number[4];
                snprintf(number, sizeof(number), "%03d", entry);
                std::string bytes(sizeof(s_animation_header) + (entry % 8) + extra, label);
                Write(prefix + file + "/" + number, bytes);
            };
            for (int i = 1; i <= 12; ++i) picture("BUFFER0.LBX", i);
            picture("BUFFER0.LBX", 0x52);
            for (int i = 0x3A; i <= 0x48; ++i) picture("BUFFER0.LBX", i);
            for (int i = 0xF1; i < 0xF1 + 24; ++i) picture("BUFFER0.LBX", i);
            for (int i = 1; i <= 11; ++i) picture("GSTAR.LBX", i);
            for (int i = 0; i < MAX_PLAYERS; ++i) picture("BUFFER0.LBX", 0x109 + i);
        }

        void Mount(bool mod) {
            Mount(mod ? std::vector<std::string>{"visual"} : std::vector<std::string>{});
        }

        void Mount(const std::vector<std::string>& mods) {
            auto* transaction = vfs::Begin_Mount_Transaction();
            DOCTEST_REQUIRE(transaction != nullptr);
            std::string diagnostic;
            for (const auto& mod : mods) {
                DOCTEST_REQUIRE(vfs::Stage_Package_Mount(transaction, mod.c_str(), &diagnostic));
            }
            DOCTEST_REQUIRE(vfs::Commit_Mount_Transaction(transaction));
        }

        void Package_As_Zip() {
            mz_zip_archive archive{};
            const auto directory = root / "data/mods/visual";
            const auto path = root / "data/mods/visual.zip";
            DOCTEST_REQUIRE(mz_zip_writer_init_file(&archive, path.string().c_str(), 0));
            for (const auto& item : fs::recursive_directory_iterator(directory)) {
                if (!item.is_regular_file()) continue;
                std::ifstream input(item.path(), std::ios::binary);
                const std::string bytes((std::istreambuf_iterator<char>(input)), {});
                const std::string name = fs::relative(item.path(), directory).generic_string();
                DOCTEST_REQUIRE(mz_zip_writer_add_mem(&archive, name.c_str(), bytes.data(), bytes.size(), MZ_BEST_COMPRESSION));
            }
            DOCTEST_REQUIRE(mz_zip_writer_finalize_archive(&archive));
            DOCTEST_REQUIRE(mz_zip_writer_end(&archive));
            fs::remove_all(directory);
        }
    };
} // namespace

DOCTEST_TEST_CASE("reload requests before resource initialization are ignored") {
    CONFIG_REFRESH::Resources_Unloaded();
    CONFIG_REFRESH::Request_Resource_Reload();
    DOCTEST_CHECK_FALSE(CONFIG_REFRESH::Resource_Reload_Pending());
    CONFIG_REFRESH::Resources_Loaded();
    CONFIG_REFRESH::Request_Resource_Reload();
    DOCTEST_CHECK(CONFIG_REFRESH::Resource_Reload_Pending());
    CONFIG_REFRESH::Resources_Unloaded();
    DOCTEST_CHECK_FALSE(CONFIG_REFRESH::Resource_Reload_Pending());
    CONFIG_REFRESH::Reload_Resources_Between_Screens();
}

DOCTEST_TEST_CASE("real resources switch repeatedly without consuming screen storage or renaming saves") {
    Fixture fixture;
    DOCTEST_SUBCASE("directory package") {}
    DOCTEST_SUBCASE("ZIP package") {
        fixture.Package_As_Zip();
    }
    CONFIG_REFRESH::Resources_Loaded();
    strcpy(MOX::player[0].race_name, "Saved race");
    strcpy(MOX::ship[0].d.name, "Saved ship");
    MOX::ship[0].d.ship_type = SHIP_TYPE_COLONY;
    void* screen = MOX::screen_seg;
    const uint32_t capacity = allocate::Get_Max_Block_Space(screen);
    const uint32_t base_size = LOADER::Persistent_Picture_Space();

    for (int repeat = 0; repeat < 40; ++repeat) {
        const bool mod = repeat % 2 != 0;
        fixture.Mount(mod);
        const char expected = mod ? 'M' : 'B';
        const uint32_t measured = LOADER::Persistent_Picture_Space();
        DOCTEST_CHECK(measured >= base_size);
        COLSUM::colsum_strings_been_loaded = 1;
        CONFIG_REFRESH::Request_Resource_Reload();
        CONFIG_REFRESH::Request_Resource_Reload();
        DOCTEST_REQUIRE(CONFIG_REFRESH::Resource_Reload_Pending());
        CONFIG_REFRESH::Reload_Resources_Between_Screens();
        DOCTEST_CHECK_FALSE(CONFIG_REFRESH::Resource_Reload_Pending());
        DOCTEST_CHECK(MOX::screen_seg == screen);
        DOCTEST_CHECK(allocate::Get_Max_Block_Space(screen) == capacity);
        DOCTEST_CHECK(allocate::Get_Remaining_Block_Space(screen) == (int32_t)capacity - 16);
        DOCTEST_CHECK(ESTRINGS::E_Strings(126)[0] == expected);
        DOCTEST_CHECK(MOX::race_names[0] == ESTRINGS::E_Strings(0x285));
        DOCTEST_CHECK(MOX::h_strings[0] == expected);
        DOCTEST_CHECK(RUSS::russ_strings[0] == expected);
        DOCTEST_CHECK(MSG_::artifacts_and[0] == expected);
        DOCTEST_CHECK(RACESCRN::relation_text[0][0] == expected);
        DOCTEST_CHECK(RACESCRN::relation_text[16][0] == expected);
        DOCTEST_CHECK(RACESCRN::research_treaty_buffer[0] == expected);
        DOCTEST_CHECK(RACESCRN::trade_treaty_buffer[0] == expected);
        DOCTEST_CHECK(RACESCRN::giving_treaty_buffer[0] == expected);
        DOCTEST_CHECK(RACESCRN::receiving_treaty_buffer[0] == expected);
        DOCTEST_CHECK(COLSUM::colsum_strings_been_loaded == 0);
        DOCTEST_CHECK(fonts::font_header.font_heights[0] == (mod ? 13 : 8));
        fonts::Load_Palette(5, 0, 255);
        DOCTEST_CHECK(video::current_palette[0].r == (uint8_t)expected);
        DOCTEST_CHECK(video::mouse_palette[0] == (uint8_t)expected);
        DOCTEST_CHECK(((char*)MOX::main_game_button_seg)[0] == expected);
        DOCTEST_CHECK(allocate::Get_Max_Block_Space(MOX::main_game_button_seg) == measured);
        DOCTEST_CHECK(allocate::Get_Remaining_Block_Space(MOX::main_game_button_seg) == 0);
        DOCTEST_CHECK(((char*)MOX::star_colors_seg[10])[0] == expected);
        DOCTEST_CHECK(((char*)MOX::fleet_box_close_sm_button_seg)[0] == expected);
        DOCTEST_CHECK(strcmp(MOX::player[0].race_name, "Saved race") == 0);
        DOCTEST_CHECK(strcmp(MOX::ship[0].d.name, "Saved ship") == 0);
        DOCTEST_CHECK(MOX::leaders[0].title[0] == expected);
        DOCTEST_CHECK(MOX::star_name_loaded_string == nullptr);
        DOCTEST_CHECK(MOX::sys_disp == nullptr);
        DOCTEST_CHECK(allocate::Get_Max_Block_Space(SYS::sys_data_buffer) == 0xDC);
        DOCTEST_CHECK(allocate::Get_Max_Block_Space(COLONY::swap_bitm) == 0x3A4);
        DOCTEST_CHECK(allocate::Get_Max_Block_Space(COLONY::mouse_bitm) == 0x3A4);
        memset(screen, 0xCD, capacity);
        DOCTEST_CHECK(((char*)MOX::main_game_button_seg)[0] == expected);
        void* art = MOX::main_game_button_seg;
        CONFIG_REFRESH::Reload_Resources_Between_Screens();
        DOCTEST_CHECK(MOX::main_game_button_seg == art);
        DOCTEST_CHECK(((uint8_t*)screen)[0] == 0xCD);
    }
}

DOCTEST_TEST_CASE("persistent packed string tables grow beyond disk size and rebuild pointers on reload") {
    Fixture fixture;
    for (size_t length : {size_t(70000), size_t(90000), size_t(3)}) {
        const std::string replacement(length, 'X');
        const std::string sidecar = "{1:'" + replacement + "'}";
        fixture.Write("data/estrings.json5", sidecar);
        fixture.Write("data/hestrngs.json5", sidecar);
        fixture.Write("data/rstring0.json5", sidecar);
        strings::Load_Strings();
        DOCTEST_CHECK(std::string(ESTRINGS::E_Strings(0)) == replacement);
        DOCTEST_CHECK(std::string(ESTRINGS::E_Strings(1)) == "B1");
        DOCTEST_CHECK(MOX::race_names[0] == ESTRINGS::E_Strings(0x285));
        DOCTEST_CHECK(std::string(HAROLD::H_Message(0)) == replacement);
        DOCTEST_CHECK(MOX::h_string_ix[1] == length + 1);
        DOCTEST_CHECK(std::string(HAROLD::H_Message(1)) == "B1");
        DOCTEST_CHECK(std::string(RUSS::russ_strings.data()) == replacement);
        DOCTEST_CHECK(std::string(RUSS::russ_strings.data() + length + 1) == "B1");
    }
}

DOCTEST_TEST_CASE("all mod mount and priority transitions reload strings and artwork") {
    Fixture fixture;
    CONFIG_REFRESH::Resources_Loaded();
    const struct {
        std::vector<std::string> mods;
        char expected;
    } cases[] = {
        {{}, 'B'},
        {{"visual"}, 'M'},
        {{"alternate"}, 'A'},
        {{"visual", "alternate"}, 'M'},
        {{"alternate", "visual"}, 'A'},
    };
    for (const auto& source : cases) {
        fixture.Mount(source.mods);
        CONFIG_REFRESH::Request_Resource_Reload();
        CONFIG_REFRESH::Reload_Resources_Between_Screens();
        for (const auto& target : cases) {
            fixture.Mount(target.mods);
            CONFIG_REFRESH::Request_Resource_Reload();
            CONFIG_REFRESH::Reload_Resources_Between_Screens();
            DOCTEST_CHECK(ESTRINGS::E_Strings(126)[0] == target.expected);
            DOCTEST_CHECK(((char*)MOX::main_game_button_seg)[0] == target.expected);
            DOCTEST_CHECK(MOX::leaders[0].title[0] == target.expected);
            DOCTEST_CHECK_FALSE(CONFIG_REFRESH::Resource_Reload_Pending());
        }
    }
}

DOCTEST_TEST_CASE("persistent arena sizing handles alignment growth and refuses oversized entries") {
    Fixture fixture;
    const uint32_t initial = LOADER::Persistent_Picture_Space();
    LOADER::Load_Main_Screen_Pictures();
    void* old = MOX::main_game_button_seg;
    const auto path = fixture.root / "data/BUFFER0.LBX/001";
    for (uint32_t extra = 0; extra < 16; ++extra) {
        const uint32_t size = sizeof(s_animation_header) + 1 + extra;
        fixture.Write("data/BUFFER0.LBX/001", std::string(size, 'G'));
        DOCTEST_CHECK(LOADER::Persistent_Picture_Space() == initial - allocate::Nested_No_Header_Pool_Size(sizeof(s_animation_header) + 1) + allocate::Nested_No_Header_Pool_Size(size));
        LOADER::Load_Main_Screen_Pictures();
        DOCTEST_CHECK(((char*)MOX::main_game_button_seg)[size - 1] == 'G');
    }
    old = MOX::main_game_button_seg;
    fixture.Write("data/BUFFER0.LBX/001", "");
    DOCTEST_CHECK_THROWS(LOADER::Load_Main_Screen_Pictures());
    DOCTEST_CHECK(MOX::main_game_button_seg == old);
    fixture.Write("data/BUFFER0.LBX/001", "x");
    fs::resize_file(path, (uintmax_t)INT32_MAX - 32);
    DOCTEST_CHECK_THROWS(LOADER::Persistent_Picture_Space());
    DOCTEST_CHECK(MOX::main_game_button_seg == old);
}

DOCTEST_TEST_CASE("saved mod configuration and canonical launch restoration refresh real resources") {
    Fixture fixture;
    DOCTEST_SUBCASE("directory package") {}
    DOCTEST_SUBCASE("ZIP package") {
        fixture.Package_As_Zip();
    }
    config::s_effective_config effective;
    config::s_cfg_diagnostic diagnostic;
    bool loaded = false;
    const bool started = config::Load_Startup_Config("orion2re.cfg", "base_game", &effective, &loaded, &diagnostic);
    DOCTEST_INFO(diagnostic.message);
    DOCTEST_REQUIRE(started);
    DOCTEST_CHECK(loaded);
    CONFIG_REFRESH::Resources_Loaded();
    CONFIG_REFRESH::Request_Resource_Reload();
    CONFIG_REFRESH::Reload_Resources_Between_Screens();
    DOCTEST_CHECK(ESTRINGS::E_Strings(126)[0] == 'B');
    config::s_config_state saved;
    DOCTEST_REQUIRE(config::Initialize_Tracked_Defaults(&saved));
    const std::vector<config::s_mod_requirement> required = {{"visual", "1.0.0"}};

    for (int repeat = 0; repeat < 5; ++repeat) {
        DOCTEST_INFO(diagnostic.message);
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(required,
            &config::saved_config_snapshot,
            &saved,
            &diagnostic));
        DOCTEST_REQUIRE(CONFIG_REFRESH::Resource_Reload_Pending());
        DOCTEST_CHECK(ESTRINGS::E_Strings(126)[0] == 'B');
        CONFIG_REFRESH::Reload_Resources_Between_Screens();
        DOCTEST_CHECK(COLSUM::print_research_value == COLSUM::RESEARCH_VALUE_PERCENTAGE_1);
        DOCTEST_CHECK(ESTRINGS::E_Strings(126)[0] == 'M');
        DOCTEST_CHECK(((char*)MOX::main_game_button_seg)[0] == 'M');

        DOCTEST_REQUIRE(config::Restore_Launch_Config(&diagnostic));
        DOCTEST_REQUIRE(CONFIG_REFRESH::Resource_Reload_Pending());
        CONFIG_REFRESH::Reload_Resources_Between_Screens();
        DOCTEST_CHECK(COLSUM::print_research_value == COLSUM::RESEARCH_VALUE_CLASSIC);
        DOCTEST_CHECK(ESTRINGS::E_Strings(126)[0] == 'B');
        DOCTEST_CHECK(((char*)MOX::main_game_button_seg)[0] == 'B');
        DOCTEST_CHECK(fonts::font_header.font_heights[0] == 8);
    }

    const std::vector<config::s_mod_requirement> unavailable = {{"missing", "1.0.0"}};
    DOCTEST_CHECK_FALSE(config::Reapply_Config_Snapshot(unavailable,
        &config::saved_config_snapshot,
        &saved,
        &diagnostic));
    DOCTEST_CHECK_FALSE(CONFIG_REFRESH::Resource_Reload_Pending());
    DOCTEST_CHECK(ESTRINGS::E_Strings(126)[0] == 'B');
}

DOCTEST_TEST_CASE("successful file save load requests resource reload even when mods are unchanged") {
    Fixture fixture;
    std::vector<s_colony> colonies(MAX_COLONIES);
    std::vector<s_planet_data> planets(MAX_PLANETS);
    std::vector<s_star_data> stars(MAX_STARS);
    std::vector<s_nebula> nebulas(MAX_NEBULAS);
    MOX::colony = colonies.data();
    MOX::planet = planets.data();
    MOX::star = stars.data();
    MOX::nebula = nebulas.data();
    MOX::global_data_seg = allocate::Allocate_Space(0x2800 + MAX_SHIPS - ORIGINAL_MAX_SHIPS);
    MOX::NUM_COLONIES = MOX::planet_count = MOX::NUM_STARS = MOX::NUM_SHIPS = 0;
    MOX::NUM_NEBULAS = 0;
    MOX::NUM_PLAYERS = 1;
    MOX::PLAYER_NUM = 0;
    MOX::game_type = 0;
    MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    MOX::player[0].surrender_to = -1;
    config::s_effective_config effective;
    config::s_cfg_diagnostic diagnostic;
    bool loaded = false;
    DOCTEST_REQUIRE(config::Load_Startup_Config("orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
    CONFIG_REFRESH::Resources_Loaded();
    CONFIG_REFRESH::Request_Resource_Reload();
    CONFIG_REFRESH::Reload_Resources_Between_Screens();
    std::vector<uint8_t> bytes(SAVEGAME::BUFFER_CAPACITY);
    SERIAL::Writer_ writer(bytes.data(), (uint32_t)bytes.size(), "test save overflow");
    std::string error;
    const char description[sizeof(SAVEGAME::s_metadata::description)] = "resource lifecycle";
    const bool written = SAVEGAME::Write_Game(&writer, description, 123, &error);
    DOCTEST_INFO(error);
    DOCTEST_REQUIRE(written);
    fixture.Write("SAVE1.GAM", std::string((char*)bytes.data(), writer.Length()));
    const auto prior = fs::current_path();
    struct Restore_Directory {
        fs::path path;
        ~Restore_Directory() {
            fs::current_path(path);
        }
    } restore{prior};
    fs::current_path(fixture.root);
    DOCTEST_REQUIRE(FILEDEF::Load_Game(0) == 1);
    DOCTEST_CHECK(CONFIG_REFRESH::Resource_Reload_Pending());
    CONFIG_REFRESH::Reload_Resources_Between_Screens();
    DOCTEST_CHECK_FALSE(CONFIG_REFRESH::Resource_Reload_Pending());
    DOCTEST_CHECK(ESTRINGS::E_Strings(126)[0] == 'B');
    free((allocate::s_alloc_header*)MOX::global_data_seg - 1);
    MOX::global_data_seg = nullptr;
    MOX::colony = nullptr;
    MOX::planet = nullptr;
    MOX::star = nullptr;
    MOX::nebula = nullptr;
}

DOCTEST_TEST_CASE("original filename placeholder is replaced by the mod percentage research format") {
    Fixture fixture;
    auto research_table = [](char label, const std::string& format) {
        std::string data;
        for (int i = 0; i < 812; ++i) {
            data += i == 126 ? format : std::string(1, label) + std::to_string(i);
            data.push_back('\0');
        }
        data.resize(21000, '\0');
        return Fixture::Record(data);
    };
    fixture.Write("data/ESTRINGS.LBX/000", research_table('B', "????.lbx"));
    fixture.Write("data/mods/visual/data/ESTRINGS.LBX/000",
        research_table('M', "%sResearch: %s%d%%"));
    CONFIG_REFRESH::Resources_Loaded();
    for (bool mod : {false, true, false, true}) {
        fixture.Mount(mod);
        CONFIG_REFRESH::Request_Resource_Reload();
        CONFIG_REFRESH::Reload_Resources_Between_Screens();
        DOCTEST_CHECK(strcmp(ESTRINGS::E_Strings(126),
                          mod ? "%sResearch: %s%d%%" : "????.lbx") == 0);
        COLSUM::Load_Colsum_Strings();
        DOCTEST_CHECK(COLSUM::wee_event_strings[0] == ESTRINGS::E_Strings(0x104));
        DOCTEST_CHECK(COLSUM::wee_event_strings[35] == ESTRINGS::E_Strings(0x186));
        DOCTEST_CHECK(COLSUM::wee_event_strings[0][0] == (mod ? 'M' : 'B'));
    }
}