#include "pch.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "config/mod_manifest.h"
#include "game/random.h"
#include "game/vfs.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_registry.h"

namespace TEST_LUA_GAME {
    void Reset_();
}

namespace {
    namespace fs = std::filesystem;

    constexpr uint8_t SHIP_STATUS_AT_STAR = 0;

    struct s_package_files {
        fs::path root;
    };

    std::string Load_Source_Package_File_(const std::string& package_id,
        const fs::path& relative_path) {
        std::ifstream file(fs::path(LUA_SOURCE_PACKAGE_DIR) / package_id /
                relative_path,
            std::ios::binary);
        DOCTEST_REQUIRE(file.good());
        return {std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
    }

    void Initialize_Package_Files_(s_package_files* files,
        const std::string& package_id) {
        static unsigned int next_id;
        files->root = fs::temp_directory_path() /
            ("orion2re-lua-galaxy-migration-" +
                std::to_string(next_id++));
        fs::create_directories(
            files->root / "data" / "mods" / package_id);
        const std::string root = files->root.string();
        DOCTEST_REQUIRE(vfs::Init(root.c_str()) == 1);
        s_vfs_mount_transaction* transaction =
            vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(transaction != nullptr);
        std::string diagnostic;
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(
            transaction, package_id.c_str(), &diagnostic));
        DOCTEST_REQUIRE(vfs::Order_Package_Mounts(
            transaction, {package_id}, &diagnostic));
    }

    void Write_Package_File_(const s_package_files* files,
        const std::string& package_id,
        const fs::path& relative_path,
        const std::string& source) {
        const fs::path path = files->root / "data" / "mods" / package_id /
            relative_path;
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(source.data(), static_cast<std::streamsize>(source.size()));
        DOCTEST_REQUIRE(file.good());
    }

    void Shutdown_Package_Files_(s_package_files* files) {
        vfs::Shutdown();
        std::error_code error;
        fs::remove_all(files->root, error);
    }

    config::s_mod_manifest Install_Source_Runtime_(
        const s_package_files* files,
        const std::string& package_id) {
        Write_Package_File_(files,
            package_id,
            "main.lua",
            Load_Source_Package_File_(package_id, "main.lua"));
        config::s_mod_manifest package;
        package.id = package_id;
        package.version_text = "1.50.26";
        package.has_lua_runtime = true;
        package.lua_runtime.api = 1;
        package.lua_runtime.storage_version = 1;
        return package;
    }
} // namespace

TEST_CASE("home-system mirror package copies the selected native orbit layout") {
    s_package_files files;
    Initialize_Package_Files_(&files, "mirror_hw");
    const std::vector<config::s_mod_manifest> packages = {
        Install_Source_Runtime_(&files, "mirror_hw"),
    };
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &registry,
        &runtimes,
        &config_diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    TEST_LUA_GAME::Reset_();
    MOX::NUM_PLAYERS = 2;
    MOX::NUM_STARS = 3;
    test_planets::Resize_Dense(5);
    MOX::NUM_SHIPS = 1;
    MOX::NUM_NEBULAS = 1;
    for (int16_t system_id = 0; system_id < MOX::NUM_STARS;
         ++system_id) {
        MOX::star[system_id].spectral_class = STAR_CLASS_G;
        MOX::star[system_id].system_special =
            SYSTEM_SPECIAL_NO_SPECIAL;
        MOX::star[system_id].wormhole_star_id = -1;
        for (auto& link : MOX::star[system_id].orbit_planet_ids) link = game::NO_PLANET;
    }
    MOX::star[0].orbit_planet_ids[0] = game::id_planet{0};
    MOX::star[0].orbit_planet_ids[2] = game::id_planet{1};
    MOX::star[1].orbit_planet_ids[4] = game::id_planet{2};
    MOX::star[1].orbit_planet_ids[1] = game::id_planet{3};
    MOX::star[2].orbit_planet_ids[3] = game::id_planet{4};
    MOX::star[0].wormhole_star_id = 2;
    MOX::star[2].wormhole_star_id = 0;

    test_planets::Record(0).star_index = 0;
    test_planets::Record(0).orbit = 0;
    test_planets::Record(0).planet_type = PLANET_TYPE_PLANET;
    test_planets::Record(0).colony_id = game::id_colony{0};
    test_planets::Record(0).planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    test_planets::Record(1).star_index = 0;
    test_planets::Record(1).orbit = 2;
    test_planets::Record(1).planet_type = PLANET_TYPE_GAS_GIANT;
    test_planets::Record(1).size = PLANET_SIZE_LARGE;
    test_planets::Record(1).gravity_class = PLANET_GRAVITY_HEAVY_G;
    test_planets::Record(1).climate = PLANET_CLIMATE_BARREN;
    test_planets::Record(1).mineral_class = MINERAL_RESOURCE_RICH;
    test_planets::Record(1).colony_id = game::NO_COLONY;
    test_planets::Record(1).planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    test_planets::Record(2).star_index = 1;
    test_planets::Record(2).orbit = 4;
    test_planets::Record(2).planet_type = PLANET_TYPE_PLANET;
    test_planets::Record(2).colony_id = game::id_colony{1};
    test_planets::Record(2).planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    test_planets::Record(3).star_index = 1;
    test_planets::Record(3).orbit = 1;
    test_planets::Record(3).planet_type = PLANET_TYPE_ASTEROID;
    test_planets::Record(3).size = PLANET_SIZE_SMALL;
    test_planets::Record(3).gravity_class = PLANET_GRAVITY_LOW_G;
    test_planets::Record(3).climate = PLANET_CLIMATE_TOXIC;
    test_planets::Record(3).mineral_class = MINERAL_RESOURCE_ULTRA_POOR;
    test_planets::Record(3).colony_id = game::NO_COLONY;
    test_planets::Record(3).planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    test_planets::Record(4).star_index = 2;
    test_planets::Record(4).orbit = 3;
    test_planets::Record(4).planet_type = PLANET_TYPE_PLANET;
    test_planets::Record(4).colony_id = game::id_colony{2};
    test_planets::Record(4).planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    MOX::player[0].home_planet_id = game::id_planet{0};
    MOX::player[1].home_planet_id = game::id_planet{2};
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_colonies::Record(0).owner = 0;
    test_colonies::Record(1).planet_id = game::id_planet{2};
    test_colonies::Record(1).owner = 1;
    test_colonies::Record(2).planet_id = game::id_planet{4};
    test_colonies::Record(2).owner = 0;
    MOX::ship[0].location = 2;
    game::current_state.leaders[0].status = 5;
    game::current_state.leaders[0].location = 2;
    MOX::nebula[0] = {70, 80, 1};

    constexpr uint32_t RNG_SEED = 0x12345678;
    game_random::Set_Random_Seed(RNG_SEED);
    const int16_t source_player =
        static_cast<int16_t>(game_random::Random(2) - 1);
    const uint32_t expected_seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(RNG_SEED);
    const int16_t destination_player = 1 - source_player;
    const int16_t source_home = source_player == 0 ? 0 : 2;
    const int16_t destination_home = destination_player == 0 ? 0 : 2;
    const int16_t source_system = source_player;
    const int16_t destination_system = destination_player;
    const int16_t source_orbit = test_planets::Record(source_home).orbit;
    const int16_t source_extra = source_player == 0 ? 1 : 3;
    const int16_t source_extra_orbit = test_planets::Record(source_extra).orbit;
    const s_planet_data expected_extra = test_planets::Record(source_extra);
    const s_planet_data unrelated_planet = test_planets::Record(4);
    const s_colony unrelated_colony = test_colonies::Record(2);
    const s_ship_data unrelated_ship = MOX::ship[0];
    const auto unrelated_leader = game::current_state.leaders[0];
    const s_nebula unrelated_nebula = MOX::nebula[0];

    bool invoked = false;
    std::string diagnostic;
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Galaxy_Generator(
        &invoked, &diagnostic));

    CHECK(invoked);
    CHECK(diagnostic.empty());
    CHECK(game_random::Get_Random_Seed() == expected_seed);
    CHECK(MOX::player[source_player].home_planet_id.get() == game::id_planet{static_cast<uint32_t>(source_home)});
    CHECK(MOX::player[destination_player].home_planet_id.get() == game::id_planet{static_cast<uint32_t>(destination_home)});
    CHECK(test_planets::Record(source_home).star_index == source_system);
    CHECK(test_planets::Record(source_home).orbit == source_orbit);
    CHECK(test_planets::Record(destination_home).star_index == destination_system);
    CHECK(test_planets::Record(destination_home).orbit == source_orbit);
    const auto copied_planet = MOX::star[destination_system].orbit_planet_ids[source_extra_orbit].get().id;
    REQUIRE(game::current_state.planets.contains(game::id_planet{copied_planet}));
    CHECK(test_planets::Record(copied_planet).star_index == destination_system);
    CHECK(test_planets::Record(copied_planet).orbit == source_extra_orbit);
    CHECK(test_planets::Record(copied_planet).planet_type ==
        expected_extra.planet_type);
    CHECK(test_planets::Record(copied_planet).size == expected_extra.size);
    CHECK(test_planets::Record(copied_planet).gravity_class ==
        expected_extra.gravity_class);
    CHECK(test_planets::Record(copied_planet).climate == expected_extra.climate);
    CHECK(test_planets::Record(copied_planet).mineral_class ==
        expected_extra.mineral_class);
    CHECK(test_planets::Record(copied_planet).colony_id == game::NO_COLONY);
    CHECK(test_planets::Record(source_extra).star_index == source_system);
    CHECK(SERIAL::Encode_Legacy_Planet_Reference(test_colonies::Record(0).planet_id) == 0);
    CHECK(SERIAL::Encode_Legacy_Planet_Reference(test_colonies::Record(1).planet_id) == 2);
    CHECK(memcmp(&test_planets::Record(4),
              &unrelated_planet,
              sizeof(unrelated_planet)) == 0);
    CHECK(memcmp(&test_colonies::Record(2),
              &unrelated_colony,
              sizeof(unrelated_colony)) == 0);
    CHECK(memcmp(&MOX::ship[0],
              &unrelated_ship,
              sizeof(unrelated_ship)) == 0);
    CHECK(game::current_state.leaders[0] == unrelated_leader);
    CHECK(memcmp(&MOX::nebula[0],
              &unrelated_nebula,
              sizeof(unrelated_nebula)) == 0);
    CHECK(MOX::star[0].wormhole_star_id == 2);
    CHECK(MOX::star[2].wormhole_star_id == 0);

    TEST_LUA_GAME::Reset_();
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Package_Files_(&files);
}

TEST_CASE("full mirror package rebuilds destination home systems") {
    s_package_files files;
    Initialize_Package_Files_(&files, "mirror");
    const std::vector<config::s_mod_manifest> packages = {
        Install_Source_Runtime_(&files, "mirror"),
    };
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &registry,
        &runtimes,
        &config_diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    TEST_LUA_GAME::Reset_();
    MOX::NUM_PLAYERS = 2;
    MOX::NUM_STARS = 8;
    test_planets::Resize_Dense(10);
    MOX::NUM_SHIPS = 2;
    MOX::NUM_NEBULAS = 2;
    for (int16_t system_id = 0; system_id < MOX::NUM_STARS;
         ++system_id) {
        s_star_data& system = MOX::star[system_id];
        system.spectral_class = STAR_CLASS_G;
        system.system_special = SYSTEM_SPECIAL_NO_SPECIAL;
        system.owner = -1;
        system.wormhole_star_id = -1;
        system.visited = 0xff;
        for (auto& link : system.orbit_planet_ids) link = game::NO_PLANET;
        memset(system.officer_index, 0xff, sizeof(system.officer_index));
    }
    const int16_t coordinates[8][2] = {
        {100, 100},
        {130, 105},
        {70, 95},
        {200, 250},
        {400, 300},
        {420, 260},
        {382, 262},
        {250, 50},
    };
    for (int16_t system_id = 0; system_id < MOX::NUM_STARS;
         ++system_id) {
        MOX::star[system_id].x = coordinates[system_id][0];
        MOX::star[system_id].y = coordinates[system_id][1];
        MOX::star[system_id].size = system_id + 1;
        MOX::star[system_id].pict_type = system_id + 2;
    }
    MOX::star[4].spectral_class = STAR_CLASS_F;
    MOX::star[5].system_special = SYSTEM_SPECIAL_MAROONED_HERO;
    MOX::star[6].system_special = SYSTEM_SPECIAL_SPACE_MONSTER;
    MOX::star[5].wormhole_star_id = 6;
    MOX::star[6].wormhole_star_id = 5;

    const int16_t planet_systems[10] = {0, 0, 4, 4, 5, 6, 1, 2, 3, 7};
    const int8_t planet_orbits[10] = {0, 2, 3, 1, 1, 4, 0, 1, 2, 3};
    for (int16_t planet_id = 0; planet_id < static_cast<int16_t>(game::current_state.planets.size());
         ++planet_id) {
        s_planet_data& planet = test_planets::Record(planet_id);
        planet.star_index = planet_systems[planet_id];
        planet.orbit = planet_orbits[planet_id];
        planet.planet_type = planet_id % 3;
        planet.size = planet_id % PLANET_SIZE_COUNT;
        planet.gravity_class = planet_id % PLANET_GRAVITY_COUNT;
        planet.climate = planet_id % PLANET_CLIMATE_COUNT;
        planet.mineral_class = planet_id % MINERAL_RESOURCE_COUNT;
        planet.planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
        planet.colony_id = game::NO_COLONY;
        MOX::star[planet.star_index].orbit_planet_ids[planet.orbit] = game::id_planet{static_cast<uint32_t>(planet_id)};
    }
    test_planets::Record(0).colony_id = game::id_colony{0};
    test_planets::Record(2).colony_id = game::id_colony{1};
    test_planets::Record(1).colony_id = game::id_colony{2};
    test_planets::Record(3).colony_id = game::id_colony{3};
    MOX::player[0].home_planet_id = game::id_planet{0};
    MOX::player[1].home_planet_id = game::id_planet{2};
    MOX::star[0].owner = 0;
    MOX::star[4].owner = 1;
    for (uint32_t colony_id = 0; colony_id < 4;
         ++colony_id) {
        test_colonies::Record(colony_id).officer = -1;
    }
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_colonies::Record(0).owner = 0;
    test_colonies::Record(1).planet_id = game::id_planet{2};
    test_colonies::Record(1).owner = 1;
    test_colonies::Record(2).planet_id = game::id_planet{1};
    test_colonies::Record(2).owner = 0;
    test_colonies::Record(3).planet_id = game::id_planet{3};
    test_colonies::Record(3).owner = 1;

    auto& leaders = game::current_state.leaders;
    for (auto& leader : leaders) {
        leader.status = -1;
        leader.location = -1;
        leader.player_index = -1;
    }
    strcpy(leaders[0].name, "Source Hero");
    leaders[0].status = 5;
    leaders[0].location = 5;
    leaders[1].status = 1;
    leaders[1].location = 4;
    leaders[1].player_index = 1;
    test_colonies::Record(3).officer = 1;
    MOX::star[4].officer_index[1] = 1;
    leaders[2].status = 1;
    leaders[2].location = 0;
    leaders[2].player_index = 0;
    test_colonies::Record(2).officer = 2;
    MOX::star[0].officer_index[0] = 2;
    leaders[3].status = 1;
    leaders[3].location = 1;
    leaders[3].player_index = 0;

    MOX::ship[0].owner = NONPLAYER_SHIP_TYPE_GUARDIAN;
    MOX::ship[0].status = SHIP_STATUS_AT_STAR;
    MOX::ship[0].location = 6;
    MOX::ship[0].x = MOX::star[6].x;
    MOX::ship[0].y = MOX::star[6].y;
    MOX::ship[0].officer_index = -1;
    MOX::ship[1].owner = 0;
    MOX::ship[1].status = 1;
    MOX::ship[1].location = 0;
    MOX::ship[1].x = MOX::star[0].x;
    MOX::ship[1].y = MOX::star[0].y;
    MOX::ship[1].d.cost = 321;
    MOX::ship[1].officer_index = 3;
    MOX::nebula[0] = {70, 80, 1};
    MOX::nebula[1] = {90, 100, 2};

    const s_planet_data source_home_planet = test_planets::Record(2);
    const s_planet_data source_extra_planet = test_planets::Record(3);
    const s_colony destination_home_colony = test_colonies::Record(0);
    const s_colony source_home_colony = test_colonies::Record(1);
    const s_colony source_extra_colony = test_colonies::Record(3);
    const s_ship_data advanced_start_ship = MOX::ship[1];
    const auto source_colony_officer = leaders[1];
    const auto ship_officer = leaders[3];

    constexpr uint32_t RNG_SEED = 0x12345678;
    game_random::Set_Random_Seed(RNG_SEED);
    game_random::Random(1);
    game_random::Random(MAX_LEADERS - 4);
    const int16_t nebula_x =
        static_cast<int16_t>(game_random::Random(226) + 90);
    const int16_t nebula_y =
        static_cast<int16_t>(game_random::Random(120) + 90);
    const uint32_t expected_seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(RNG_SEED);

    bool invoked = false;
    std::string diagnostic;
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Galaxy_Generator(
        &invoked, &diagnostic));

    CHECK(invoked);
    CHECK(diagnostic.empty());
    CHECK(game_random::Get_Random_Seed() == expected_seed);
    CHECK(game::current_state.next_colony_id == 4);
    CHECK(MOX::player[0].home_planet_id.get() == game::id_planet{0});
    CHECK(MOX::player[1].home_planet_id.get() == game::id_planet{2});
    CHECK(test_planets::Record(0).star_index == 0);
    CHECK(test_planets::Record(0).orbit == source_home_planet.orbit);
    CHECK(test_planets::Record(0).colony_id == game::id_colony{0});
    CHECK(memcmp(&test_colonies::Record(0),
              &destination_home_colony,
              sizeof(destination_home_colony)) == 0);
    CHECK(memcmp(&test_planets::Record(2),
              &source_home_planet,
              sizeof(source_home_planet)) == 0);
    CHECK(memcmp(&test_colonies::Record(1),
              &source_home_colony,
              sizeof(source_home_colony)) == 0);
    CHECK(test_planets::Record(3).colony_id == game::id_colony{3});
    CHECK_FALSE(game::current_state.colonies.contains(game::id_colony{2}));
    CHECK(test_colonies::Record(3) == source_extra_colony);
    CHECK(leaders[1] == source_colony_officer);
    CHECK(MOX::star[4].officer_index[1] == 1);
    CHECK(leaders[2].status == 0);
    CHECK(leaders[2].location == -1);
    CHECK((MOX::star[0].officer_index[0] == -1));
    CHECK(memcmp(&MOX::ship[1],
              &advanced_start_ship,
              sizeof(advanced_start_ship)) == 0);
    CHECK(leaders[3] == ship_officer);

    const auto copied_home_extra = MOX::star[0].orbit_planet_ids[1].get().id;
    REQUIRE(game::current_state.planets.contains(game::id_planet{copied_home_extra}));
    CHECK(MOX::star[0].orbit_planet_ids[0].get() == game::NO_PLANET);
    CHECK(MOX::star[0].orbit_planet_ids[2].get() == game::NO_PLANET);
    CHECK(MOX::star[0].orbit_planet_ids[3].get() == game::id_planet{0});
    CHECK(MOX::star[0].orbit_planet_ids[4].get() == game::NO_PLANET);
    CHECK(test_planets::Record(copied_home_extra).planet_type ==
        source_extra_planet.planet_type);
    CHECK(test_planets::Record(copied_home_extra).size == source_extra_planet.size);
    CHECK(test_planets::Record(copied_home_extra).colony_id == game::NO_COLONY);
    CHECK(MOX::star[4].orbit_planet_ids[1].get() == game::id_planet{3});
    CHECK(MOX::star[4].orbit_planet_ids[3].get() == game::id_planet{2});
    CHECK((MOX::star[0].x == 356));
    CHECK((MOX::star[0].y == 218));
    CHECK(MOX::star[0].spectral_class == STAR_CLASS_F);
    CHECK(MOX::star[0].visited == 1);
    CHECK((MOX::star[4].x == 150));
    CHECK((MOX::star[4].y == 182));
    CHECK(MOX::star[4].visited == 2);

    int16_t copied_marooned_system = -1;
    int16_t copied_monster_system = -1;
    for (int16_t system_id = 0; system_id < MOX::NUM_STARS;
         ++system_id) {
        const s_star_data& system = MOX::star[system_id];
        if (system.x == 338 && system.y == 182) {
            copied_marooned_system = system_id;
        } else if (system.x == 373 && system.y == 184) {
            copied_monster_system = system_id;
        }
    }
    REQUIRE(copied_marooned_system >= 0);
    REQUIRE(copied_monster_system >= 0);
    CHECK(MOX::star[copied_marooned_system].system_special ==
        SYSTEM_SPECIAL_MAROONED_HERO);
    CHECK(MOX::star[copied_monster_system].system_special ==
        SYSTEM_SPECIAL_SPACE_MONSTER);
    CHECK(MOX::star[copied_marooned_system].visited == 0);
    CHECK(MOX::star[copied_marooned_system].wormhole_star_id ==
        copied_monster_system);
    CHECK(MOX::star[copied_monster_system].wormhole_star_id ==
        copied_marooned_system);
    CHECK(MOX::star[5].wormhole_star_id == 6);
    CHECK(MOX::star[6].wormhole_star_id == 5);

    int marooned_leaders = 0;
    bool copied_leader_found = false;
    for (const auto& leader : leaders) {
        if (leader.status != 5) {
            continue;
        }
        ++marooned_leaders;
        copied_leader_found |=
            leader.location == copied_marooned_system;
    }
    CHECK(marooned_leaders == 2);
    CHECK(copied_leader_found);
    CHECK(MOX::NUM_SHIPS == 3);
    CHECK(MOX::ship[2].owner == NONPLAYER_SHIP_TYPE_GUARDIAN);
    CHECK(MOX::ship[2].status == SHIP_STATUS_AT_STAR);
    CHECK((MOX::ship[2].location == copied_monster_system));
    CHECK((MOX::ship[2].x == MOX::star[copied_monster_system].x));
    CHECK((MOX::ship[2].y == MOX::star[copied_monster_system].y));
    CHECK(MOX::nebula[0].x == nebula_x - 40);
    CHECK(MOX::nebula[0].y == nebula_y - 40);
    CHECK((MOX::nebula[1].x == 366 - nebula_x));
    CHECK((MOX::nebula[1].y == 260 - nebula_y));

    TEST_LUA_GAME::Reset_();
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Package_Files_(&files);
}

TEST_CASE("resetting generated star names prevents reuse of the previous galaxy cache") {
    TEST_LUA_GAME::Reset_();
    char name[15] = {};
    REQUIRE(MAPGEN::Copy_Generated_Star_Name(0, name, sizeof(name)));
    CHECK(std::string(name) == "Star 0");

    MAPGEN::Reset_Generated_Star_Names();
    CHECK_FALSE(MAPGEN::Copy_Generated_Star_Name(0, name, sizeof(name)));
    CHECK(std::string(name) == "Star 0");

    TEST_LUA_GAME::Reset_();
    CHECK(MAPGEN::Copy_Generated_Star_Name(0, name, sizeof(name)));
}

TEST_CASE("full mirror package preserves native home systems") {
    s_package_files files;
    Initialize_Package_Files_(&files, "mirror_exhw");
    const std::vector<config::s_mod_manifest> packages = {
        Install_Source_Runtime_(&files, "mirror_exhw"),
    };
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &registry,
        &runtimes,
        &config_diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    TEST_LUA_GAME::Reset_();
    MOX::NUM_PLAYERS = 2;
    MOX::NUM_STARS = 8;
    test_planets::Resize_Dense(10);
    MOX::NUM_SHIPS = 2;
    MOX::NUM_NEBULAS = 2;
    char vocabulary[829][15] = {};
    uint16_t indices[829];
    for (int index = 0; index < 829; ++index) {
        snprintf(vocabulary[index], sizeof(vocabulary[index]), "Star %d", index);
        indices[index] = static_cast<uint16_t>((index + 11) % 829);
    }
    MAPGEN::name_seg = vocabulary;
    MAPGEN::name_index = indices;
    MAPGEN::Prepare_Generated_Star_Names();
    MAPGEN::name_seg = nullptr;
    MAPGEN::name_index = nullptr;
    char extended_name[15] = {};
    REQUIRE(MAPGEN::Copy_Generated_Star_Name(829, extended_name, sizeof(extended_name)));
    CHECK(std::string(extended_name) == "Star 11 I");
    for (int16_t system_id = 0; system_id < MOX::NUM_STARS;
         ++system_id) {
        s_star_data& system = MOX::star[system_id];
        system.spectral_class = STAR_CLASS_G;
        system.system_special = SYSTEM_SPECIAL_NO_SPECIAL;
        system.owner = -1;
        system.wormhole_star_id = -1;
        system.visited = 0xff;
        for (auto& link : system.orbit_planet_ids) link = game::NO_PLANET;
    }
    const int16_t coordinates[8][2] = {
        {100, 100},
        {130, 105},
        {70, 95},
        {200, 250},
        {400, 300},
        {420, 260},
        {382, 262},
        {250, 50},
    };
    for (int16_t system_id = 0; system_id < MOX::NUM_STARS;
         ++system_id) {
        MOX::star[system_id].x = coordinates[system_id][0];
        MOX::star[system_id].y = coordinates[system_id][1];
        MOX::star[system_id].size = system_id + 1;
        MOX::star[system_id].pict_type = system_id + 2;
    }
    MOX::star[4].spectral_class = STAR_CLASS_F;
    MOX::star[5].system_special = SYSTEM_SPECIAL_MAROONED_HERO;
    MOX::star[6].system_special = SYSTEM_SPECIAL_SPACE_MONSTER;
    MOX::star[5].wormhole_star_id = 6;
    MOX::star[6].wormhole_star_id = 5;

    const int16_t planet_systems[10] = {0, 0, 4, 4, 5, 6, 1, 2, 3, 7};
    const int8_t planet_orbits[10] = {0, 2, 3, 1, 1, 4, 0, 1, 2, 3};
    for (int16_t planet_id = 0; planet_id < static_cast<int16_t>(game::current_state.planets.size());
         ++planet_id) {
        s_planet_data& planet = test_planets::Record(planet_id);
        planet.star_index = planet_systems[planet_id];
        planet.orbit = planet_orbits[planet_id];
        planet.planet_type = planet_id % 3;
        planet.size = planet_id % PLANET_SIZE_COUNT;
        planet.gravity_class = planet_id % PLANET_GRAVITY_COUNT;
        planet.climate = planet_id % PLANET_CLIMATE_COUNT;
        planet.mineral_class = planet_id % MINERAL_RESOURCE_COUNT;
        planet.planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
        planet.colony_id = game::NO_COLONY;
        MOX::star[planet.star_index].orbit_planet_ids[planet.orbit] = game::id_planet{static_cast<uint32_t>(planet_id)};
    }
    test_planets::Record(0).colony_id = game::id_colony{0};
    test_planets::Record(2).colony_id = game::id_colony{1};
    test_planets::Record(6).colony_id = game::id_colony{2};
    MOX::player[0].home_planet_id = game::id_planet{0};
    MOX::player[1].home_planet_id = game::id_planet{2};
    MOX::star[0].owner = 0;
    MOX::star[4].owner = 1;
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_colonies::Record(0).owner = 0;
    test_colonies::Record(0).officer = -1;
    test_colonies::Record(1).planet_id = game::id_planet{2};
    test_colonies::Record(1).owner = 1;
    test_colonies::Record(1).officer = -1;
    test_colonies::Record(2).planet_id = game::id_planet{6};
    test_colonies::Record(2).owner = 0;
    test_colonies::Record(2).officer = -1;

    auto& leaders = game::current_state.leaders;
    for (auto& leader : leaders) {
        leader.status = -1;
        leader.location = -1;
        leader.player_index = -1;
    }
    strcpy(leaders[0].name, "Source Hero");
    leaders[0].status = 5;
    leaders[0].location = 5;
    MOX::ship[0].owner = NONPLAYER_SHIP_TYPE_GUARDIAN;
    MOX::ship[0].status = SHIP_STATUS_AT_STAR;
    MOX::ship[0].location = 6;
    MOX::ship[0].x = MOX::star[6].x;
    MOX::ship[0].y = MOX::star[6].y;
    MOX::ship[1].owner = 0;
    MOX::ship[1].status = 1;
    MOX::ship[1].location = 0;
    MOX::ship[1].x = MOX::star[0].x;
    MOX::ship[1].y = MOX::star[0].y;
    MOX::ship[1].d.cost = 321;
    MOX::nebula[0] = {70, 80, 1};
    MOX::nebula[1] = {90, 100, 2};

    const s_planet_data home_planet_0 = test_planets::Record(0);
    const s_planet_data home_extra_0 = test_planets::Record(1);
    const s_planet_data home_planet_1 = test_planets::Record(2);
    const s_planet_data home_extra_1 = test_planets::Record(3);
    const s_colony home_colony_0 = test_colonies::Record(0);
    const s_colony home_colony_1 = test_colonies::Record(1);
    const s_ship_data advanced_start_ship = MOX::ship[1];
    const int8_t home_size_0 = MOX::star[0].size;
    const int8_t home_picture_0 = MOX::star[0].pict_type;
    const int8_t home_size_1 = MOX::star[4].size;
    const int8_t home_picture_1 = MOX::star[4].pict_type;
    const s_planet_data source_planet_0 = test_planets::Record(4);
    const s_planet_data source_planet_1 = test_planets::Record(5);

    constexpr uint32_t RNG_SEED = 0x12345678;
    game_random::Set_Random_Seed(RNG_SEED);
    game_random::Random(1);
    game_random::Random(MAX_LEADERS - 1);
    const int16_t nebula_x =
        static_cast<int16_t>(game_random::Random(226) + 90);
    const int16_t nebula_y =
        static_cast<int16_t>(game_random::Random(120) + 90);
    const uint32_t expected_seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(RNG_SEED);

    bool invoked = false;
    std::string diagnostic;
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Galaxy_Generator(
        &invoked, &diagnostic));

    CHECK(invoked);
    CHECK(diagnostic.empty());
    CHECK(game_random::Get_Random_Seed() == expected_seed);
    CHECK(game::current_state.colonies.size() == 2);
    CHECK(MOX::player[0].home_planet_id.get() == game::id_planet{0});
    CHECK(MOX::player[1].home_planet_id.get() == game::id_planet{2});
    CHECK(memcmp(&test_planets::Record(0),
              &home_planet_0,
              sizeof(home_planet_0)) == 0);
    CHECK(memcmp(&test_planets::Record(1),
              &home_extra_0,
              sizeof(home_extra_0)) == 0);
    CHECK(memcmp(&test_planets::Record(2),
              &home_planet_1,
              sizeof(home_planet_1)) == 0);
    CHECK(memcmp(&test_planets::Record(3),
              &home_extra_1,
              sizeof(home_extra_1)) == 0);
    CHECK(memcmp(&test_colonies::Record(0),
              &home_colony_0,
              sizeof(home_colony_0)) == 0);
    CHECK(memcmp(&test_colonies::Record(1),
              &home_colony_1,
              sizeof(home_colony_1)) == 0);
    CHECK(memcmp(&MOX::ship[1],
              &advanced_start_ship,
              sizeof(advanced_start_ship)) == 0);

    CHECK((MOX::star[4].x == 150));
    CHECK((MOX::star[4].y == 182));
    CHECK(MOX::star[4].size == home_size_1);
    CHECK(MOX::star[4].pict_type == home_picture_1);
    CHECK(MOX::star[4].owner == 1);
    CHECK(MOX::star[4].system_special == SYSTEM_SPECIAL_NO_SPECIAL);
    CHECK(MOX::star[4].visited == 2);
    CHECK((MOX::star[0].x == 356));
    CHECK((MOX::star[0].y == 218));
    CHECK(MOX::star[0].spectral_class == STAR_CLASS_F);
    CHECK(MOX::star[0].size == home_size_0);
    CHECK(MOX::star[0].pict_type == home_picture_0);
    CHECK(MOX::star[0].owner == 0);
    CHECK(MOX::star[0].system_special == SYSTEM_SPECIAL_NO_SPECIAL);
    CHECK(MOX::star[0].visited == 1);
    CHECK(MOX::star[0].orbit_planet_ids[0].get() == game::id_planet{0});
    CHECK(MOX::star[0].orbit_planet_ids[1].get() == game::NO_PLANET);
    CHECK(MOX::star[0].orbit_planet_ids[2].get() == game::id_planet{1});
    CHECK(MOX::star[0].orbit_planet_ids[3].get() == game::NO_PLANET);
    CHECK(MOX::star[0].orbit_planet_ids[4].get() == game::NO_PLANET);
    CHECK(MOX::star[4].orbit_planet_ids[0].get() == game::NO_PLANET);
    CHECK(MOX::star[4].orbit_planet_ids[3].get() == game::id_planet{2});
    CHECK(MOX::star[4].orbit_planet_ids[1].get() == game::id_planet{3});
    CHECK(MOX::star[4].orbit_planet_ids[2].get() == game::NO_PLANET);
    CHECK(MOX::star[4].orbit_planet_ids[4].get() == game::NO_PLANET);

    int16_t copied_marooned_system = -1;
    int16_t copied_monster_system = -1;
    for (int16_t system_id = 0; system_id < MOX::NUM_STARS;
         ++system_id) {
        const s_star_data& system = MOX::star[system_id];
        if (system.spectral_class < STAR_CLASS_B ||
            system.spectral_class > STAR_CLASS_BLACK_HOLE) {
            continue;
        }
        if (system.x == 338 && system.y == 182) {
            copied_marooned_system = system_id;
        } else if (system.x == 373 && system.y == 184) {
            copied_monster_system = system_id;
        }
    }
    REQUIRE(copied_marooned_system >= 0);
    REQUIRE(copied_monster_system >= 0);
    CHECK(std::string(MOX::star[copied_marooned_system].name) ==
        "Star " + std::to_string((copied_marooned_system + 11) % 829));
    CHECK(std::string(MOX::star[copied_monster_system].name) ==
        "Star " + std::to_string((copied_monster_system + 11) % 829));
    CHECK(MOX::star[copied_marooned_system].system_special ==
        SYSTEM_SPECIAL_MAROONED_HERO);
    CHECK(MOX::star[copied_monster_system].system_special ==
        SYSTEM_SPECIAL_SPACE_MONSTER);
    CHECK(MOX::star[copied_marooned_system].wormhole_star_id ==
        copied_monster_system);
    CHECK(MOX::star[copied_monster_system].wormhole_star_id ==
        copied_marooned_system);
    CHECK(MOX::star[5].wormhole_star_id == 6);
    CHECK(MOX::star[6].wormhole_star_id == 5);

    const auto copied_planet_0 = MOX::star[copied_marooned_system].orbit_planet_ids[1].get().id;
    const auto copied_planet_1 = MOX::star[copied_monster_system].orbit_planet_ids[4].get().id;
    REQUIRE(game::current_state.planets.contains(game::id_planet{copied_planet_0}));
    REQUIRE(game::current_state.planets.contains(game::id_planet{copied_planet_1}));
    CHECK(test_planets::Record(copied_planet_0).planet_type ==
        source_planet_0.planet_type);
    CHECK(test_planets::Record(copied_planet_0).size == source_planet_0.size);
    CHECK(test_planets::Record(copied_planet_0).colony_id == game::NO_COLONY);
    CHECK(test_planets::Record(copied_planet_1).planet_type ==
        source_planet_1.planet_type);
    CHECK(test_planets::Record(copied_planet_1).size == source_planet_1.size);
    CHECK(test_planets::Record(copied_planet_1).colony_id == game::NO_COLONY);

    int marooned_leaders = 0;
    bool copied_leader_found = false;
    for (const auto& leader : leaders) {
        if (leader.status != 5) {
            continue;
        }
        ++marooned_leaders;
        copied_leader_found |=
            leader.location == copied_marooned_system;
    }
    CHECK(marooned_leaders == 2);
    CHECK(copied_leader_found);
    CHECK(MOX::NUM_SHIPS == 3);
    CHECK(MOX::ship[2].owner == NONPLAYER_SHIP_TYPE_GUARDIAN);
    CHECK(MOX::ship[2].status == SHIP_STATUS_AT_STAR);
    CHECK((MOX::ship[2].location == copied_monster_system));
    CHECK((MOX::ship[2].x == MOX::star[copied_monster_system].x));
    CHECK((MOX::ship[2].y == MOX::star[copied_monster_system].y));
    CHECK(MOX::nebula[0].x == nebula_x - 40);
    CHECK(MOX::nebula[0].y == nebula_y - 40);
    CHECK(MOX::nebula[0].type == 1);
    CHECK((MOX::nebula[1].x == 366 - nebula_x));
    CHECK((MOX::nebula[1].y == 260 - nebula_y));
    CHECK(MOX::nebula[1].type == 2);

    TEST_LUA_GAME::Reset_();
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Package_Files_(&files);
}
