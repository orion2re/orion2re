#include "pch.h"

#include "doctest/doctest.h"

#include "config/config_loader.h"
#include "game/savegame.h"
#include "../fixtures/game_state.h"

#include <fstream>

namespace {
    struct Config_Scope_ {
        ~Config_Scope_() {
            config::Shutdown_Config_Loader();
            vfs::Shutdown();
        }
    };
} // namespace

DOCTEST_TEST_CASE("every released savegame version loads through the game runtime") {
    Config_Scope_ config_scope;
    // The v0 Doom Star design migration normally gets this name from the game assets.
    char doom_star_name[] = "Doom Star";
    test::s_saved_value hull_name(TECHDATA::hull_data[HULL_DOOM_STAR].name);
    TECHDATA::hull_data[HULL_DOOM_STAR].name = doom_star_name;
    DOCTEST_REQUIRE(vfs::Init(SAVEGAME_TEST_DATA_DIR));
    config::s_effective_config effective;
    config::s_cfg_diagnostic diagnostic;
    bool loaded = false;
    DOCTEST_INFO(diagnostic.message);
    DOCTEST_REQUIRE(config::Load_Startup_Config(
        "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));

    for (uint16_t version = 0; version <= SAVEGAME::CURRENT_VERSION; ++version) {
        const std::string name = "savegame_v" + std::to_string(version) + ".sav";
        DOCTEST_INFO(name);
        std::ifstream file(SAVEGAME_TEST_DATA_DIR "/" + name, std::ios::binary | std::ios::ate);
        DOCTEST_REQUIRE(file.is_open());
        const std::streamoff size = file.tellg();
        DOCTEST_REQUIRE(size > 0);
        DOCTEST_REQUIRE(size <= SAVEGAME::BUFFER_CAPACITY);
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.seekg(0);
        DOCTEST_REQUIRE(file.read(reinterpret_cast<char*>(bytes.data()), size));

        test::s_game_state state;
        s_nebula nebulas[MAX_NEBULAS] = {};
        test::s_saved_value saved_nebula(MOX::nebula);
        test::s_saved_value saved_nebula_count(MOX::NUM_NEBULAS);
        MOX::nebula = nebulas;
        MOX::NUM_NEBULAS = 0;

        SERIAL::Reader_ reader(bytes.data(), static_cast<uint32_t>(size), "invalid save fixture", true);
        SAVEGAME::s_metadata metadata = {};
        s_settings settings = {};
        uint32_t seed = 0;
        std::string error;
        DOCTEST_INFO(error);
        DOCTEST_REQUIRE(SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed, &error));
        DOCTEST_CHECK_FALSE(reader.Failed());
        DOCTEST_CHECK(reader.Remaining() == 0);
        DOCTEST_CHECK((metadata.header ==
            (version == 0 ? SAVEGAME::LEGACY_HEADER : SAVEGAME::VERSIONED_HEADER)));
        DOCTEST_CHECK((metadata.version == version));
        constexpr char description[sizeof(metadata.description)] = "Exhaustive save state";
        DOCTEST_CHECK(memcmp(metadata.description, description, sizeof(description)) == 0);
        DOCTEST_CHECK((metadata.stardate == 35123));
        DOCTEST_CHECK(seed == 0xCAFEBABE);
        DOCTEST_CHECK(MOX::NUM_COLONIES == 249);
        DOCTEST_CHECK(MOX::planet_count == 359);
        DOCTEST_CHECK(MOX::NUM_STARS == 71);
        DOCTEST_CHECK(MOX::NUM_SHIPS == 499);
        DOCTEST_CHECK(MOX::cur_map_x == -144);

        // Assert the migrated values, not just successful parsing. Copy packed fields
        // by value rather than having doctest bind a potentially misaligned reference.
        DOCTEST_CHECK((state.planets[0].food_per_farmer == 54));
        DOCTEST_CHECK((MOX::antarans.offensive_titan_built == (version >= 3 ? 1 : 0)));
        DOCTEST_CHECK((MOX::antarans.offensive_limit_exhausted_mask == (version >= 3 ? 4 : 0)));
        DOCTEST_CHECK((state.players[0].settlers[ORIGINAL_MAX_FREIGHTED_SETTLERS].val ==
            (version >= 3 ? 0x0000000200030004ull : 0)));
        DOCTEST_CHECK((state.ships[0].prior_jump_gate_destination == (version >= 4 ? 3 : -1)));
        DOCTEST_CHECK((state.ships[0].prior_jump_gate_stardate == (version >= 4 ? 35122 : 0)));
        DOCTEST_CHECK((state.colonies[0].buildings_scrapped_this_turn == (version >= 7 ? 3 : 1)));
        DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message() ==
            (version >= 3 ? "Loaded config: base_game" : ""));
    }
}
