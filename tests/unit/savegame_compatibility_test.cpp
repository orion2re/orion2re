#include "pch.h"

#include "doctest/doctest.h"

#include "config/config_loader.h"
#include "game/colony_roster_serialization.h"
#include "game/savegame.h"
#include "game/star_serialization.h"
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

    constexpr uint16_t last_fixture_version = 7;
    for (uint16_t version = 0; version <= last_fixture_version; ++version) {
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
        DOCTEST_CHECK(game::current_state.next_colony_id == 249);
        const auto& game_state = game::current_state;
        DOCTEST_CHECK(game_state.planets.size() == 359);
        DOCTEST_REQUIRE(game_state.planet_order.size() == 359);
        DOCTEST_CHECK(game_state.planet_order.front() == game::id_planet{0});
        DOCTEST_CHECK(game_state.planet_order.back() == game::id_planet{358});
        DOCTEST_CHECK(game_state.next_planet_id == 359);
        DOCTEST_CHECK(std::ssize(game::current_state.stars) == 71);
        DOCTEST_CHECK(game::current_state.ship_slots.size() == 499);
        DOCTEST_CHECK(MOX::cur_map_x == -144);

        // Assert the migrated values, not just successful parsing. Copy packed fields
        // by value rather than having doctest bind a potentially misaligned reference.
        DOCTEST_REQUIRE(game_state.planets.contains(game::id_planet{0}));
        DOCTEST_CHECK(game_state.planets.at(game::id_planet{0}).food_per_farmer == 54);
        DOCTEST_CHECK((MOX::antarans.offensive_titan_built == (version >= 3 ? 1 : 0)));
        DOCTEST_CHECK((MOX::antarans.offensive_limit_exhausted_mask == (version >= 3 ? 4 : 0)));
        const auto& settler = state.players[0].settlers[ORIGINAL_MAX_FREIGHTED_SETTLERS];
        DOCTEST_CHECK(settler.source_colony == game::id_colony{version >= 3 ? 4u : 0u});
        DOCTEST_CHECK(settler.destination == game::id_planet{version >= 3 ? 3u : 0u});
        DOCTEST_CHECK(settler.race == (version >= 3 ? 2 : 0));
        DOCTEST_CHECK(settler.eta == 0);
        DOCTEST_CHECK(settler.profession == 0);
        DOCTEST_CHECK(state.ships[0].prior_jump_gate_destination == (version >= 4 ? game::id_star{3} : game::NO_STAR));
        DOCTEST_CHECK((state.ships[0].prior_jump_gate_stardate == (version >= 4 ? 35122 : 0)));
        DOCTEST_CHECK_FALSE(game_state.colonies.contains(game::id_colony{0}));
        // The synthetic legacy records contain dangling and out-of-range links.
        DOCTEST_CHECK_FALSE(game_state.colonies.contains(game::id_colony{3}));
        DOCTEST_CHECK(game_state.colonies.at(game::id_colony{60}).buildings_scrapped_this_turn == 1);
        DOCTEST_CHECK(game_state.planets.at(game::id_planet{292}).colony_id == game::id_colony{60});
        DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message() ==
            (version >= 3 ? "Loaded config: base_game" : ""));

        const auto saved_extent = version == 0 ? size_t{ORIGINAL_MAX_SHIPS} : game_state.ship_slots.size();
        const game::owner_slot antaran{NONPLAYER_SHIP_TYPE_ANTARAN};
        int quota = 0;
        for (const auto& count : MOX::antarans.offensive_ships) quota += count;
        quota = quota > 20 ? 20 : std::max(quota, 0) + 2;
        size_t first = MAX_SHIPS;
        int saved_reservations = 0;
        int total_reservations = 0;
        for (size_t slot = 0; slot < MAX_SHIPS; ++slot) {
            if (ship_slots::Available(game_state, slot) && ship_slots::Owner(game_state, slot) == antaran) {
                first = std::min(first, slot);
                ++total_reservations;
                if (slot < saved_extent) ++saved_reservations;
            }
        }
        DOCTEST_REQUIRE(total_reservations == std::max(quota, saved_reservations));
        DOCTEST_REQUIRE(first < MAX_SHIPS);
        if (saved_reservations < quota) DOCTEST_CHECK(ship_slots::Owner(game_state, saved_extent) == antaran);
        const auto& star = game_state.stars.at(game_state.star_order.front());
        const auto spawned = SHIPMOVE::Create_Nonplayer_Ship(antaran, nullptr, star.x, star.y);
        DOCTEST_REQUIRE(spawned != game::NO_SHIP);
        DOCTEST_CHECK(game_state.ships.at(spawned).owner == antaran);
        DOCTEST_CHECK(game_state.ship_slots[first] == spawned);

        if (version == 7) {
            constexpr size_t colony_count_offset = sizeof(uint32_t) + sizeof(metadata.description) +
                sizeof(int32_t) + sizeof(int8_t) + sizeof(uint16_t) + sizeof(s_settings) + sizeof(uint32_t);
            SERIAL::Reader_ layout(bytes.data(), bytes.size(), "invalid v7 colony fixture", true);
            layout.Skip(colony_count_offset);
            const auto colony_count = layout.Read_Uint16();
            const auto colony_records = layout.Length();
            layout.Skip(colony_count * SERIAL::LEGACY_COLONY_BYTES);
            const auto planet_count = layout.Read_Uint16();
            layout.Skip(planet_count * SERIAL::NARROW_COLONY_PLANET_BYTES);
            const auto star_count = layout.Read_Uint16();
            const auto star_records = layout.Length();
            constexpr size_t star_bytes = SERIAL::NARROW_LINK_STAR_BYTES - 10;
            constexpr size_t priority_offset = SERIAL::NARROW_WFI_OFFSET - 3;
            DOCTEST_REQUIRE(layout.Require(star_count * star_bytes));
            DOCTEST_REQUIRE(colony_count == 249);
            DOCTEST_REQUIRE(game_state.player_count >= 2);

            // Preserve one claim inside the saved extent, for a different player.
            for (size_t i = 0; i < colony_count; ++i) bytes[colony_records + i * SERIAL::LEGACY_COLONY_BYTES + 1] = 0xff;
            bytes[colony_records] = 0xff; // no owner
            bytes[colony_records + 1] = 1; // allocated to player 1
            bytes[colony_records + 2] = bytes[colony_records + 3] = 0xff; // no planet
            for (size_t i = 0; i < star_count; ++i) bytes[star_records + i * star_bytes + priority_offset] = 0xff;

            game::id_planet target = game::NO_PLANET;
            size_t demand = 0;
            for (const auto& [key, system] : game_state.stars) {
                for (const auto& link : system.orbit_planet_ids) {
                    const auto site = game_state.planets.find(link);
                    if (site != game_state.planets.end() && site->second.colony_id == game::NO_COLONY) {
                        ++demand;
                        if (site->second.star_id == key) target = link;
                    }
                }
                if (target != game::NO_PLANET) {
                    DOCTEST_REQUIRE(key.id < star_count);
                    bytes[star_records + key.id * star_bytes + priority_offset] = 0;
                    break;
                }
                demand = 0;
            }
            DOCTEST_REQUIRE(target != game::NO_PLANET);

            SERIAL::Reader_ modified(bytes.data(), bytes.size(), "invalid v7 reservation fixture", true);
            DOCTEST_REQUIRE(SAVEGAME::Read_Game(&modified, &metadata, &settings, &seed, &error));
            DOCTEST_CHECK(game_state.colony_reservations[0].empty());
            DOCTEST_CHECK(game_state.colony_reservations[1] == std::vector{game::id_colony{249}});

            SERIAL::Reader_ for_play(bytes.data(), bytes.size(), "invalid v7 reservation fixture", true);
            DOCTEST_REQUIRE(FILEDEF::Read_Game_For_Play(&for_play, &metadata, &settings, &seed, &error));
            DOCTEST_CHECK(game_state.next_colony_id == colony_count);
            DOCTEST_CHECK(game_state.colony_reservations[1] == std::vector{game::id_colony{249}});
            DOCTEST_REQUIRE(game_state.colony_reservations[0].size() == demand);
            const auto first_claim = game_state.colony_reservations[0].front();
            DOCTEST_CHECK(first_claim == game::id_colony{250});
            DOCTEST_REQUIRE(COLONIZE::Make_New_Outpost(target, game::id_player{0}));
            DOCTEST_CHECK(game_state.planets.at(target).colony_id == first_claim);
            DOCTEST_CHECK(game_state.colony_reservations[0].size() == demand - 1);
        }
    }
}
