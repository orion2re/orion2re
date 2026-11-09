#include "pch.h"

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
    MOX::planet_count = 5;
    MOX::NUM_COLONIES = 3;
    MOX::NUM_SHIPS = 1;
    MOX::NUM_NEBULAS = 1;
    for (int16_t system_id = 0; system_id < MOX::NUM_STARS;
         ++system_id) {
        MOX::star[system_id].spectral_class = STAR_CLASS_G;
        MOX::star[system_id].system_special =
            SYSTEM_SPECIAL_NO_SPECIAL;
        MOX::star[system_id].wormhole_star_id = -1;
        memset(MOX::star[system_id].planet_index,
            0xff,
            sizeof(MOX::star[system_id].planet_index));
    }
    MOX::star[0].planet_index[0] = 0;
    MOX::star[0].planet_index[2] = 1;
    MOX::star[1].planet_index[4] = 2;
    MOX::star[1].planet_index[1] = 3;
    MOX::star[2].planet_index[3] = 4;
    MOX::star[0].wormhole_star_id = 2;
    MOX::star[2].wormhole_star_id = 0;

    MOX::planet[0].star_index = 0;
    MOX::planet[0].orbit = 0;
    MOX::planet[0].planet_type = PLANET_TYPE_PLANET;
    MOX::planet[0].colony_index = 0;
    MOX::planet[0].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    MOX::planet[1].star_index = 0;
    MOX::planet[1].orbit = 2;
    MOX::planet[1].planet_type = PLANET_TYPE_GAS_GIANT;
    MOX::planet[1].size = PLANET_SIZE_LARGE;
    MOX::planet[1].gravity_class = PLANET_GRAVITY_HEAVY_G;
    MOX::planet[1].climate = PLANET_CLIMATE_BARREN;
    MOX::planet[1].mineral_class = MINERAL_RESOURCE_RICH;
    MOX::planet[1].colony_index = -1;
    MOX::planet[1].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    MOX::planet[2].star_index = 1;
    MOX::planet[2].orbit = 4;
    MOX::planet[2].planet_type = PLANET_TYPE_PLANET;
    MOX::planet[2].colony_index = 1;
    MOX::planet[2].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    MOX::planet[3].star_index = 1;
    MOX::planet[3].orbit = 1;
    MOX::planet[3].planet_type = PLANET_TYPE_ASTEROID;
    MOX::planet[3].size = PLANET_SIZE_SMALL;
    MOX::planet[3].gravity_class = PLANET_GRAVITY_LOW_G;
    MOX::planet[3].climate = PLANET_CLIMATE_TOXIC;
    MOX::planet[3].mineral_class = MINERAL_RESOURCE_ULTRA_POOR;
    MOX::planet[3].colony_index = -1;
    MOX::planet[3].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    MOX::planet[4].star_index = 2;
    MOX::planet[4].orbit = 3;
    MOX::planet[4].planet_type = PLANET_TYPE_PLANET;
    MOX::planet[4].colony_index = 2;
    MOX::planet[4].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    MOX::player[0].home_planet_id = 0;
    MOX::player[1].home_planet_id = 2;
    MOX::colony[0].planet = 0;
    MOX::colony[0].owner = 0;
    MOX::colony[1].planet = 2;
    MOX::colony[1].owner = 1;
    MOX::colony[2].planet = 4;
    MOX::colony[2].owner = 0;
    MOX::ship[0].location = 2;
    MOX::leaders[0].status = 5;
    MOX::leaders[0].location = 2;
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
    const int16_t source_orbit = MOX::planet[source_home].orbit;
    const int16_t source_extra = source_player == 0 ? 1 : 3;
    const int16_t source_extra_orbit = MOX::planet[source_extra].orbit;
    const s_planet_data expected_extra = MOX::planet[source_extra];
    const s_planet_data unrelated_planet = MOX::planet[4];
    const s_colony unrelated_colony = MOX::colony[2];
    const s_ship_data unrelated_ship = MOX::ship[0];
    const s_leader_data unrelated_leader = MOX::leaders[0];
    const s_nebula unrelated_nebula = MOX::nebula[0];

    bool invoked = false;
    std::string diagnostic;
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Galaxy_Generator(
        &invoked, &diagnostic));

    CHECK(invoked);
    CHECK(diagnostic.empty());
    CHECK(game_random::Get_Random_Seed() == expected_seed);
    CHECK((MOX::player[source_player].home_planet_id == source_home));
    CHECK((MOX::player[destination_player].home_planet_id ==
        destination_home));
    CHECK(MOX::planet[source_home].star_index == source_system);
    CHECK(MOX::planet[source_home].orbit == source_orbit);
    CHECK(MOX::planet[destination_home].star_index == destination_system);
    CHECK(MOX::planet[destination_home].orbit == source_orbit);
    const int16_t copied_planet =
        MOX::star[destination_system].planet_index[source_extra_orbit];
    REQUIRE(copied_planet >= 0);
    CHECK(MOX::planet[copied_planet].star_index == destination_system);
    CHECK(MOX::planet[copied_planet].orbit == source_extra_orbit);
    CHECK(MOX::planet[copied_planet].planet_type ==
        expected_extra.planet_type);
    CHECK(MOX::planet[copied_planet].size == expected_extra.size);
    CHECK(MOX::planet[copied_planet].gravity_class ==
        expected_extra.gravity_class);
    CHECK(MOX::planet[copied_planet].climate == expected_extra.climate);
    CHECK(MOX::planet[copied_planet].mineral_class ==
        expected_extra.mineral_class);
    CHECK(MOX::planet[copied_planet].colony_index == -1);
    CHECK(MOX::planet[source_extra].star_index == source_system);
    CHECK(MOX::colony[0].planet == 0);
    CHECK(MOX::colony[1].planet == 2);
    CHECK(memcmp(&MOX::planet[4],
              &unrelated_planet,
              sizeof(unrelated_planet)) == 0);
    CHECK(memcmp(&MOX::colony[2],
              &unrelated_colony,
              sizeof(unrelated_colony)) == 0);
    CHECK(memcmp(&MOX::ship[0],
              &unrelated_ship,
              sizeof(unrelated_ship)) == 0);
    CHECK(memcmp(&MOX::leaders[0],
              &unrelated_leader,
              sizeof(unrelated_leader)) == 0);
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
    MOX::planet_count = 10;
    MOX::NUM_COLONIES = 4;
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
        memset(system.planet_index, 0xff, sizeof(system.planet_index));
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
    for (int16_t planet_id = 0; planet_id < MOX::planet_count;
         ++planet_id) {
        s_planet_data& planet = MOX::planet[planet_id];
        planet.star_index = planet_systems[planet_id];
        planet.orbit = planet_orbits[planet_id];
        planet.planet_type = planet_id % 3;
        planet.size = planet_id % PLANET_SIZE_COUNT;
        planet.gravity_class = planet_id % PLANET_GRAVITY_COUNT;
        planet.climate = planet_id % PLANET_CLIMATE_COUNT;
        planet.mineral_class = planet_id % MINERAL_RESOURCE_COUNT;
        planet.planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
        planet.colony_index = -1;
        MOX::star[planet.star_index].planet_index[planet.orbit] =
            planet_id;
    }
    MOX::planet[0].colony_index = 0;
    MOX::planet[2].colony_index = 1;
    MOX::planet[1].colony_index = 2;
    MOX::planet[3].colony_index = 3;
    MOX::player[0].home_planet_id = 0;
    MOX::player[1].home_planet_id = 2;
    MOX::star[0].owner = 0;
    MOX::star[4].owner = 1;
    for (int16_t colony_id = 0; colony_id < MOX::NUM_COLONIES;
         ++colony_id) {
        MOX::colony[colony_id].officer = -1;
    }
    MOX::colony[0].planet = 0;
    MOX::colony[0].owner = 0;
    MOX::colony[1].planet = 2;
    MOX::colony[1].owner = 1;
    MOX::colony[2].planet = 1;
    MOX::colony[2].owner = 0;
    MOX::colony[3].planet = 3;
    MOX::colony[3].owner = 1;

    for (int16_t leader_id = 0; leader_id < MAX_LEADERS; ++leader_id) {
        MOX::leaders[leader_id].status = -1;
        MOX::leaders[leader_id].location = -1;
        MOX::leaders[leader_id].player_index = -1;
    }
    strcpy(MOX::leaders[0].name, "Source Hero");
    MOX::leaders[0].status = 5;
    MOX::leaders[0].location = 5;
    MOX::leaders[1].status = 1;
    MOX::leaders[1].location = 4;
    MOX::leaders[1].player_index = 1;
    MOX::colony[3].officer = 1;
    MOX::star[4].officer_index[1] = 1;
    MOX::leaders[2].status = 1;
    MOX::leaders[2].location = 0;
    MOX::leaders[2].player_index = 0;
    MOX::colony[2].officer = 2;
    MOX::star[0].officer_index[0] = 2;
    MOX::leaders[3].status = 1;
    MOX::leaders[3].location = 1;
    MOX::leaders[3].player_index = 0;

    MOX::ship[0].owner = NONPLAYER_SHIP_TYPE_GUARDIAN;
    MOX::ship[0].status = 1;
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

    const s_planet_data source_home_planet = MOX::planet[2];
    const s_planet_data source_extra_planet = MOX::planet[3];
    const s_colony destination_home_colony = MOX::colony[0];
    const s_colony source_home_colony = MOX::colony[1];
    const s_colony source_extra_colony = MOX::colony[3];
    const s_ship_data advanced_start_ship = MOX::ship[1];
    const s_leader_data source_colony_officer = MOX::leaders[1];
    const s_leader_data ship_officer = MOX::leaders[3];

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
    CHECK(MOX::NUM_COLONIES == 3);
    CHECK((MOX::player[0].home_planet_id == 0));
    CHECK((MOX::player[1].home_planet_id == 2));
    CHECK(MOX::planet[0].star_index == 0);
    CHECK(MOX::planet[0].orbit == source_home_planet.orbit);
    CHECK(MOX::planet[0].colony_index == 0);
    CHECK(memcmp(&MOX::colony[0],
              &destination_home_colony,
              sizeof(destination_home_colony)) == 0);
    CHECK(memcmp(&MOX::planet[2],
              &source_home_planet,
              sizeof(source_home_planet)) == 0);
    CHECK(memcmp(&MOX::colony[1],
              &source_home_colony,
              sizeof(source_home_colony)) == 0);
    CHECK(MOX::planet[3].colony_index == 2);
    CHECK(memcmp(&MOX::colony[2],
              &source_extra_colony,
              sizeof(source_extra_colony)) == 0);
    CHECK(memcmp(&MOX::leaders[1],
              &source_colony_officer,
              sizeof(source_colony_officer)) == 0);
    CHECK(MOX::star[4].officer_index[1] == 1);
    CHECK(MOX::leaders[2].status == 0);
    CHECK((MOX::leaders[2].location == -1));
    CHECK((MOX::star[0].officer_index[0] == -1));
    CHECK(memcmp(&MOX::ship[1],
              &advanced_start_ship,
              sizeof(advanced_start_ship)) == 0);
    CHECK(memcmp(&MOX::leaders[3],
              &ship_officer,
              sizeof(ship_officer)) == 0);

    const int16_t copied_home_extra = MOX::star[0].planet_index[1];
    REQUIRE(copied_home_extra >= 0);
    CHECK((MOX::star[0].planet_index[0] == -1));
    CHECK((MOX::star[0].planet_index[2] == -1));
    CHECK((MOX::star[0].planet_index[3] == 0));
    CHECK((MOX::star[0].planet_index[4] == -1));
    CHECK(MOX::planet[copied_home_extra].planet_type ==
        source_extra_planet.planet_type);
    CHECK(MOX::planet[copied_home_extra].size == source_extra_planet.size);
    CHECK(MOX::planet[copied_home_extra].colony_index == -1);
    CHECK((MOX::star[4].planet_index[1] == 3));
    CHECK((MOX::star[4].planet_index[3] == 2));
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
    for (int16_t leader_id = 0; leader_id < MAX_LEADERS; ++leader_id) {
        if (MOX::leaders[leader_id].status != 5) {
            continue;
        }
        ++marooned_leaders;
        copied_leader_found |=
            MOX::leaders[leader_id].location == copied_marooned_system;
    }
    CHECK(marooned_leaders == 2);
    CHECK(copied_leader_found);
    CHECK(MOX::NUM_SHIPS == 3);
    CHECK(MOX::ship[2].owner == NONPLAYER_SHIP_TYPE_GUARDIAN);
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
    MOX::planet_count = 10;
    MOX::NUM_COLONIES = 3;
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
        memset(system.planet_index, 0xff, sizeof(system.planet_index));
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
    for (int16_t planet_id = 0; planet_id < MOX::planet_count;
         ++planet_id) {
        s_planet_data& planet = MOX::planet[planet_id];
        planet.star_index = planet_systems[planet_id];
        planet.orbit = planet_orbits[planet_id];
        planet.planet_type = planet_id % 3;
        planet.size = planet_id % PLANET_SIZE_COUNT;
        planet.gravity_class = planet_id % PLANET_GRAVITY_COUNT;
        planet.climate = planet_id % PLANET_CLIMATE_COUNT;
        planet.mineral_class = planet_id % MINERAL_RESOURCE_COUNT;
        planet.planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
        planet.colony_index = -1;
        MOX::star[planet.star_index].planet_index[planet.orbit] =
            planet_id;
    }
    MOX::planet[0].colony_index = 0;
    MOX::planet[2].colony_index = 1;
    MOX::planet[6].colony_index = 2;
    MOX::player[0].home_planet_id = 0;
    MOX::player[1].home_planet_id = 2;
    MOX::star[0].owner = 0;
    MOX::star[4].owner = 1;
    MOX::colony[0].planet = 0;
    MOX::colony[0].owner = 0;
    MOX::colony[0].officer = -1;
    MOX::colony[1].planet = 2;
    MOX::colony[1].owner = 1;
    MOX::colony[1].officer = -1;
    MOX::colony[2].planet = 6;
    MOX::colony[2].owner = 0;
    MOX::colony[2].officer = -1;

    for (int16_t leader_id = 0; leader_id < MAX_LEADERS; ++leader_id) {
        MOX::leaders[leader_id].status = -1;
        MOX::leaders[leader_id].location = -1;
        MOX::leaders[leader_id].player_index = -1;
    }
    strcpy(MOX::leaders[0].name, "Source Hero");
    MOX::leaders[0].status = 5;
    MOX::leaders[0].location = 5;
    MOX::ship[0].owner = NONPLAYER_SHIP_TYPE_GUARDIAN;
    MOX::ship[0].status = 1;
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

    const s_planet_data home_planet_0 = MOX::planet[0];
    const s_planet_data home_extra_0 = MOX::planet[1];
    const s_planet_data home_planet_1 = MOX::planet[2];
    const s_planet_data home_extra_1 = MOX::planet[3];
    const s_colony home_colony_0 = MOX::colony[0];
    const s_colony home_colony_1 = MOX::colony[1];
    const s_ship_data advanced_start_ship = MOX::ship[1];
    const int8_t home_size_0 = MOX::star[0].size;
    const int8_t home_picture_0 = MOX::star[0].pict_type;
    const int8_t home_size_1 = MOX::star[4].size;
    const int8_t home_picture_1 = MOX::star[4].pict_type;
    const s_planet_data source_planet_0 = MOX::planet[4];
    const s_planet_data source_planet_1 = MOX::planet[5];

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
    CHECK(MOX::NUM_COLONIES == 2);
    CHECK((MOX::player[0].home_planet_id == 0));
    CHECK((MOX::player[1].home_planet_id == 2));
    CHECK(memcmp(&MOX::planet[0],
              &home_planet_0,
              sizeof(home_planet_0)) == 0);
    CHECK(memcmp(&MOX::planet[1],
              &home_extra_0,
              sizeof(home_extra_0)) == 0);
    CHECK(memcmp(&MOX::planet[2],
              &home_planet_1,
              sizeof(home_planet_1)) == 0);
    CHECK(memcmp(&MOX::planet[3],
              &home_extra_1,
              sizeof(home_extra_1)) == 0);
    CHECK(memcmp(&MOX::colony[0],
              &home_colony_0,
              sizeof(home_colony_0)) == 0);
    CHECK(memcmp(&MOX::colony[1],
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
    CHECK((MOX::star[0].planet_index[0] == 0));
    CHECK((MOX::star[0].planet_index[1] == -1));
    CHECK((MOX::star[0].planet_index[2] == 1));
    CHECK((MOX::star[0].planet_index[3] == -1));
    CHECK((MOX::star[0].planet_index[4] == -1));
    CHECK((MOX::star[4].planet_index[0] == -1));
    CHECK((MOX::star[4].planet_index[3] == 2));
    CHECK((MOX::star[4].planet_index[1] == 3));
    CHECK((MOX::star[4].planet_index[2] == -1));
    CHECK((MOX::star[4].planet_index[4] == -1));

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

    const int16_t copied_planet_0 =
        MOX::star[copied_marooned_system].planet_index[1];
    const int16_t copied_planet_1 =
        MOX::star[copied_monster_system].planet_index[4];
    REQUIRE(copied_planet_0 >= 0);
    REQUIRE(copied_planet_1 >= 0);
    CHECK(MOX::planet[copied_planet_0].planet_type ==
        source_planet_0.planet_type);
    CHECK(MOX::planet[copied_planet_0].size == source_planet_0.size);
    CHECK(MOX::planet[copied_planet_0].colony_index == -1);
    CHECK(MOX::planet[copied_planet_1].planet_type ==
        source_planet_1.planet_type);
    CHECK(MOX::planet[copied_planet_1].size == source_planet_1.size);
    CHECK(MOX::planet[copied_planet_1].colony_index == -1);

    int marooned_leaders = 0;
    bool copied_leader_found = false;
    for (int16_t leader_id = 0; leader_id < MAX_LEADERS; ++leader_id) {
        if (MOX::leaders[leader_id].status != 5) {
            continue;
        }
        ++marooned_leaders;
        copied_leader_found |=
            MOX::leaders[leader_id].location == copied_marooned_system;
    }
    CHECK(marooned_leaders == 2);
    CHECK(copied_leader_found);
    CHECK(MOX::NUM_SHIPS == 3);
    CHECK(MOX::ship[2].owner == NONPLAYER_SHIP_TYPE_GUARDIAN);
    CHECK(MOX::ship[2].status == 1);
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