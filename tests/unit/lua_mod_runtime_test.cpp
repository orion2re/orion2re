#include "lua/lua_mod_runtime.h"

#include <doctest/doctest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "config/mod_manifest.h"
#include "game/consts.h"
#include "game/random.h"
#include "game/vfs.h"
#include "lua/lua_game.h"
#include "lua/lua_game_internal.h"
#include "lua/lua_game_traits.h"
#include "lua/lua_game_ship_identity.h"
#include "lua/lua_network.h"
#include "lua/lua_registry.h"
#include "lua/lua_save.h"
#include "lua/lua_storage.h"
#include "game/serialization.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace TEST_LUA_GAME {
    void Reset_();
}

namespace config {
    void Reset_Lua_Reconstruction_Fake();
    void Set_Lua_Reconstruction_Result(bool result);
    int Lua_Reconstruction_Calls();
} // namespace config

namespace {
    namespace fs = std::filesystem;

    struct s_module_files {
        fs::path root;
    };

    void Initialize_Package_Files_(s_module_files* files,
        const std::vector<std::string>& package_ids);
    void Write_Package_File_(const s_module_files* files,
        const std::string& package_id,
        const fs::path& relative_path,
        const std::string& source);
    std::string Load_Lua_Fixture_(const fs::path& relative_path);
    std::string Load_Lua_Source_(const fs::path& relative_path);
    config::s_mod_manifest Runtime_Package_(const std::string& id);
    void Shutdown_Module_Files_(s_module_files* files);
} // namespace

TEST_CASE("Lua galaxy generator is exclusive versioned and uses ordinary game objects") {
    s_module_files files;
    Initialize_Package_Files_(
        &files, {"asset_library", "world_builder", "rules_observer"});
    Write_Package_File_(&files,
        "asset_library",
        "main.lua",
        "events.on('game.initialized', function() end)\n");
    Write_Package_File_(&files,
        "world_builder",
        "main.lua",
        "services.register('galaxy_generator', 1, function(...)\n"
        "  if select('#', ...) ~= 0 then error('service received arguments') end\n"
        "  local systems = game.systems:all()\n"
        "  systems[1].x = 77\n"
        "  systems[1].y = game.rng:integer(50, 90)\n"
        "  storage.calls = (storage.calls or 0) + 1\n"
        "  if storage.calls ~= 1 then error('service invoked more than once') end\n"
        "end)\n");
    Write_Package_File_(&files,
        "rules_observer",
        "main.lua",
        "rules.modify('population.growth', function(context, value) return value end)\n");
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("asset_library"),
        Runtime_Package_("world_builder"),
        Runtime_Package_("rules_observer"),
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
    const auto* runtime = orion2re::lua::Find_Mod_Runtime(
        runtimes, "world_builder");
    REQUIRE(runtime != nullptr);
    REQUIRE(orion2re::lua::Get_Mod_Service_Count(runtime) == 1);
    orion2re::lua::s_lua_service_info service;
    REQUIRE(orion2re::lua::Get_Mod_Service_Info(runtime, 0, &service));
    CHECK(service.name == "galaxy_generator");
    CHECK(service.version == 1);
    CHECK(service.registration_source == "mods/world_builder/main.lua");
    REQUIRE(registry.runtime_packages.size() == 3);
    const auto& world_builder_identity = registry.runtime_packages[1];
    REQUIRE(world_builder_identity.package_id == "world_builder");
    REQUIRE(world_builder_identity.services.size() == 1);
    CHECK(world_builder_identity.services[0].name == "galaxy_generator");
    CHECK(world_builder_identity.services[0].version == 1);

    orion2re::lua::Commit_Mod_Runtimes(&runtimes);
    TEST_LUA_GAME::Reset_();
    constexpr uint32_t RNG_SEED = 0x12345678;
    game_random::Set_Random_Seed(RNG_SEED);
    const int16_t expected_y = static_cast<int16_t>(
        game_random::Random(40) + 50);
    const uint32_t expected_seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(RNG_SEED);
    bool invoked = false;
    std::string diagnostic;
    REQUIRE(orion2re::lua::Dispatch_Active_Galaxy_Generator(
        &invoked, &diagnostic));
    CHECK(invoked);
    CHECK(diagnostic.empty());
    CHECK((MOX::star[0].x == 77));
    CHECK((MOX::star[0].y == expected_y));
    CHECK(game_random::Get_Random_Seed() == expected_seed);

    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua save restoration does not dispatch galaxy generation") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"load_observer"});
    Write_Package_File_(&files,
        "load_observer",
        "main.lua",
        Load_Lua_Fixture_("galaxy/load_non_dispatch.lua"));
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("load_observer"),
    };
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry saved_registry;
    orion2re::lua::s_lua_mod_runtime_set* saved_runtimes = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &saved_registry,
        &saved_runtimes,
        &config_diagnostic));
    saved_registry.packages_high_to_low_priority.push_back(
        {"load_observer", "1.0.0"});
    orion2re::lua::Commit_Registry(&saved_registry);
    orion2re::lua::Commit_Mod_Runtimes(&saved_runtimes);
    TEST_LUA_GAME::Reset_();

    orion2re::lua::s_lua_save_snapshot snapshot;
    std::string diagnostic;
    REQUIRE(orion2re::lua::Capture_Lua_Save_Snapshot(
        &snapshot, &diagnostic));
    orion2re::lua::Reset_Mod_Runtimes();
    orion2re::lua::Reset_Registry();

    orion2re::lua::s_lua_registry restored_registry;
    orion2re::lua::s_lua_mod_runtime_set* restored_runtimes = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &restored_registry,
        &restored_runtimes,
        &config_diagnostic));
    restored_registry.packages_high_to_low_priority.push_back(
        {"load_observer", "1.0.0"});
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Restore_Lua_Save_Snapshot(&snapshot,
        &restored_registry,
        restored_runtimes,
        &diagnostic));
    CHECK(diagnostic.empty());

    orion2re::lua::Destroy_Mod_Runtime_Set(restored_runtimes);
    TEST_LUA_GAME::Reset_();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua structural galaxy operations require service capability") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"capability_probe"});
    Write_Package_File_(&files,
        "capability_probe",
        "main.lua",
        Load_Lua_Fixture_("galaxy/capability_restriction.lua"));
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("capability_probe"),
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
    MOX::NUM_NEBULAS = 1;
    MOX::nebula[0] = {10, 20, 1};

    std::string diagnostic;
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Initialized_Event(&diagnostic));
    CHECK(diagnostic.empty());

    TEST_LUA_GAME::Reset_();
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua galaxy candidate handles expire after publication and rejection") {
    for (const int outcome : {0, 1, 2}) {
        const bool callback_fails = outcome == 1;
        const bool owner_rejects = outcome == 2;
        CAPTURE(callback_fails);
        CAPTURE(owner_rejects);
        s_module_files files;
        Initialize_Package_Files_(&files, {"identity_probe"});
        std::string source =
            Load_Lua_Fixture_("galaxy/candidate_handle_expiry.lua");
        if (callback_fails) {
            source += "candidate_should_fail = true\n";
        }
        Write_Package_File_(&files,
            "identity_probe",
            "main.lua",
            source);
        const std::vector<config::s_mod_manifest> packages = {
            Runtime_Package_("identity_probe"),
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
        MOX::leaders[0].status = 5;
        MOX::leaders[0].location = 0;

        const uint64_t live_topology_generation =
            orion2re::lua::game::internal::Topology_Generation();
        uint64_t live_ship_generations[MAX_SHIPS];
        REQUIRE(orion2re::lua::game::Capture_Ship_Generation_State(
            live_ship_generations, MAX_SHIPS));
        uint64_t candidate_ship_generations[MAX_SHIPS];
        std::memcpy(candidate_ship_generations,
            live_ship_generations,
            sizeof(candidate_ship_generations));
        for (uint64_t& generation : candidate_ship_generations) {
            generation = generation == 0 ? 1 : generation + 1;
        }
        orion2re::lua::game::internal::Advance_Topology_Generation();
        bool invoked = false;
        std::string diagnostic;
        bool succeeded = false;
        {
            orion2re::lua::game::s_ship_generation_scope scope(
                candidate_ship_generations);
            succeeded = orion2re::lua::Dispatch_Active_Galaxy_Generator(
                &invoked, &diagnostic);
        }
        CHECK(invoked);
        CHECK(succeeded != callback_fails);
        if (callback_fails) {
            CHECK(diagnostic.find("requested candidate failure") !=
                std::string::npos);
            MOX::NUM_NEBULAS = 0;
            orion2re::lua::game::internal::Restore_Topology_Generation(
                live_topology_generation);
        } else if (owner_rejects) {
            MOX::NUM_NEBULAS = 0;
            orion2re::lua::game::internal::Restore_Topology_Generation(
                live_topology_generation);
        } else {
            for (uint64_t& generation : candidate_ship_generations) {
                ++generation;
            }
            REQUIRE(orion2re::lua::game::Restore_Ship_Generation_State(
                candidate_ship_generations, MAX_SHIPS));
            orion2re::lua::game::internal::Restore_Topology_Generation(
                live_topology_generation);
            orion2re::lua::game::internal::Advance_Topology_Generation();
            orion2re::lua::game::internal::Advance_Topology_Generation();
        }

        auto* runtime = const_cast<orion2re::lua::s_lua_mod_runtime*>(
            orion2re::lua::Find_Mod_Runtime(
                orion2re::lua::Get_Active_Mod_Runtimes(),
                "identity_probe"));
        REQUIRE(runtime != nullptr);
        orion2re::lua::s_lua_result result;
        CAPTURE(result.diagnostic);
        REQUIRE(orion2re::lua::Dispatch_Mod_Event(runtime,
            "game.initialized",
            nullptr,
            nullptr,
            &result));

        TEST_LUA_GAME::Reset_();
        orion2re::lua::Reset_Mod_Runtimes();
        Shutdown_Module_Files_(&files);
    }
}

TEST_CASE("Lua galaxy collections enforce native capacity limits") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"capacity_probe"});
    Write_Package_File_(&files,
        "capacity_probe",
        "main.lua",
        Load_Lua_Fixture_("galaxy/capacity_rejection.lua"));
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("capacity_probe"),
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
    MOX::NUM_STARS = MAX_STARS;
    MOX::planet_count = MAX_PLANETS;
    for (int16_t planet_id = 3; planet_id < MAX_PLANETS; ++planet_id) {
        MOX::planet[planet_id].star_index = 0;
    }
    MOX::NUM_SHIPS = MAX_SHIPS;
    MOX::NUM_NEBULAS = MAX_NEBULAS;

    bool invoked = false;
    std::string diagnostic;
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Galaxy_Generator(
        &invoked, &diagnostic));
    CHECK(invoked);
    CHECK(diagnostic.empty());

    TEST_LUA_GAME::Reset_();
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua galaxy collections expose complete stable domain objects") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"world_builder"});
    Write_Package_File_(&files,
        "world_builder",
        "main.lua",
        Load_Lua_Fixture_("galaxy/stable_domain_objects.lua"));
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("world_builder"),
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
    MOX::NUM_COLONIES = 2;
    MOX::planet[0].colony_index = 0;
    MOX::colony[0].planet = 0;
    MOX::colony[0].owner = 0;
    MOX::planet[1].planet_type = PLANET_TYPE_PLANET;
    MOX::planet[1].colony_index = 1;
    MOX::colony[1].planet = 1;
    MOX::colony[1].owner = 1;
    MOX::star[0].size = 4;
    MOX::star[0].pict_type = 5;
    MOX::star[0].wormhole_star_id = -1;
    MOX::star[1].wormhole_star_id = -1;
    MOX::ship[0].owner = 0;
    MOX::ship[0].status = 1;
    MOX::ship[0].location = 0;
    MOX::ship[0].x = 10;
    MOX::ship[0].y = 20;
    std::strcpy(MOX::leaders[0].name, "Pathfinder");
    MOX::leaders[0].status = 5;
    MOX::leaders[0].location = 0;
    MOX::leaders[1].status = -1;
    MOX::leaders[1].location = -1;

    bool invoked = false;
    std::string diagnostic;
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Galaxy_Generator(
        &invoked, &diagnostic));
    CHECK(invoked);
    CHECK(diagnostic.empty());

    TEST_LUA_GAME::Reset_();
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua galaxy mutations reject invalid domains and references") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"validation_probe"});
    Write_Package_File_(&files,
        "validation_probe",
        "main.lua",
        Load_Lua_Fixture_("galaxy/validation_rejection.lua"));
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("validation_probe"),
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
    MOX::NUM_COLONIES = 1;
    MOX::planet[0].colony_index = 0;
    MOX::colony[0].planet = 0;
    MOX::colony[0].owner = 0;
    MOX::leaders[0].status = 0;
    MOX::leaders[1].status = 0;

    bool invoked = false;
    std::string diagnostic;
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Galaxy_Generator(
        &invoked, &diagnostic));
    CHECK(invoked);
    CHECK(diagnostic.empty());

    TEST_LUA_GAME::Reset_();
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua galaxy generator registration and provider conflicts fail activation") {
    struct s_failure {
        std::vector<std::string> package_ids;
        std::vector<std::string> sources;
        const char* message;
    };
    const std::vector<s_failure> failures = {
        {{"provider"},
            {"services.register('unknown_service', 1, function() end)"},
            "unsupported Lua service"},
        {{"provider"},
            {"services.register('galaxy_generator', 2, function() end)"},
            "unsupported Lua service"},
        {{"provider"},
            {"services.register('galaxy_generator', 1, function() end)\n"
             "services.register('galaxy_generator', 1, function() end)"},
            "duplicate Lua service registration"},
        {{"first", "second"},
            {"services.register('galaxy_generator', 1, function() end)",
                "services.register('galaxy_generator', 1, function() end)"},
            "multiple providers"},
    };
    for (const s_failure& failure : failures) {
        CAPTURE(failure.message);
        s_module_files files;
        Initialize_Package_Files_(&files, failure.package_ids);
        std::vector<config::s_mod_manifest> packages;
        for (size_t index = 0; index < failure.package_ids.size(); ++index) {
            Write_Package_File_(&files,
                failure.package_ids[index],
                "main.lua",
                failure.sources[index]);
            packages.push_back(
                Runtime_Package_(failure.package_ids[index]));
        }
        orion2re::lua::s_lua_mod_limits limits;
        orion2re::lua::s_lua_registry registry;
        orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
        config::s_cfg_diagnostic diagnostic;
        CHECK_FALSE(orion2re::lua::Stage_Mod_Runtimes(&packages,
            &limits,
            &registry,
            &runtimes,
            &diagnostic));
        CHECK(diagnostic.message.find(failure.message) !=
            std::string::npos);
        orion2re::lua::Destroy_Mod_Runtime_Set(runtimes);
        Shutdown_Module_Files_(&files);
    }
}

TEST_CASE("Lua galaxy structural operations preserve stable candidate handles") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"provider"});
    Write_Package_File_(&files,
        "provider",
        "main.lua",
        Load_Lua_Fixture_("galaxy/stable_structural_operations.lua"));
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("provider"),
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
    MOX::NUM_PLAYERS = 1;
    bool invoked = false;
    std::string diagnostic;

    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Galaxy_Generator(
        &invoked, &diagnostic));

    CHECK(invoked);
    CHECK(diagnostic.empty());
    CHECK((MOX::star[1].x == 70));
    CHECK((MOX::star[1].y == 80));
    CHECK((MOX::star[0].wormhole_star_id == 1));
    CHECK((MOX::star[1].wormhole_star_id == 0));

    TEST_LUA_GAME::Reset_();
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

namespace {

    void Initialize_Package_Files_(s_module_files* files,
        const std::vector<std::string>& package_ids) {
        static unsigned int next_id;
        files->root = fs::temp_directory_path() /
            ("orion2re-lua-mod-runtime-" +
                std::to_string(next_id++));
        for (const std::string& package_id : package_ids) {
            fs::create_directories(
                files->root / "data" / "mods" / package_id);
        }
        const std::string root = files->root.string();
        DOCTEST_REQUIRE(vfs::Init(root.c_str()) == 1);

        s_vfs_mount_transaction* transaction = vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(transaction != nullptr);
        std::string diagnostic;
        for (const std::string& package_id : package_ids) {
            DOCTEST_REQUIRE(vfs::Stage_Package_Mount(
                                transaction, package_id.c_str(), &diagnostic) == 1);
        }
        DOCTEST_REQUIRE(vfs::Order_Package_Mounts(
                            transaction, package_ids, &diagnostic) == 1);
    }

    void Initialize_Module_Files_(s_module_files* files) {
        Initialize_Package_Files_(files, {"example"});
    }

    void Write_Package_File_(const s_module_files* files,
        const std::string& package_id,
        const fs::path& relative_path,
        const std::string& source) {
        const fs::path path = files->root / "data" / "mods" / package_id /
            relative_path;
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(source.data(), (std::streamsize)source.size());
        DOCTEST_REQUIRE(file.good());
    }

    std::string Load_Lua_Fixture_(const fs::path& relative_path) {
        std::ifstream file(
            fs::path(LUA_MIGRATION_FIXTURE_DIR) / relative_path,
            std::ios::binary);
        DOCTEST_REQUIRE(file.good());
        return {std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
    }

    std::string Load_Lua_Source_(const fs::path& relative_path) {
        std::ifstream file(
            fs::path(LUA_SOURCE_PACKAGE_DIR) / relative_path,
            std::ios::binary);
        DOCTEST_REQUIRE(file.good());
        return {std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
    }

    void Write_Module_(const s_module_files* files,
        const fs::path& relative_path,
        const std::string& source) {
        Write_Package_File_(files, "example", relative_path, source);
    }

    config::s_mod_manifest Runtime_Package_(const std::string& id) {
        config::s_mod_manifest package;
        package.id = id;
        package.version_text = "1.0.0";
        package.has_lua_runtime = true;
        package.lua_runtime.api = 1;
        package.lua_runtime.storage_version = 1;
        return package;
    }

    void Shutdown_Module_Files_(s_module_files* files) {
        vfs::Shutdown();
        std::error_code error;
        fs::remove_all(files->root, error);
    }

    struct s_event_observation {
        lua_Integer amount = 0;
        lua_Integer transient = 0;
        lua_Integer stored = 0;
        int reports = 0;
    };

    int Record_(lua_State* lua_state) {
        auto* observation = (s_event_observation*)lua_touserdata(
            lua_state, lua_upvalueindex(1));
        observation->transient = luaL_checkinteger(lua_state, 1);
        observation->stored = luaL_checkinteger(lua_state, 2);
        ++observation->reports;
        return 0;
    }

    int Build_Event_(lua_State* lua_state, void* user_data) {
        auto* observation = (s_event_observation*)user_data;
        lua_newtable(lua_state);
        lua_pushinteger(lua_state, observation->amount);
        lua_setfield(lua_state, -2, "amount");
        lua_pushlightuserdata(lua_state, observation);
        lua_pushcclosure(lua_state, Record_, 1);
        lua_setfield(lua_state, -2, "report");
        return 1;
    }

    orion2re::lua::s_lua_mod_runtime* Create_(const char* source,
        orion2re::lua::s_lua_result* result,
        const orion2re::lua::s_lua_mod_limits* limits = nullptr) {
        const orion2re::lua::s_lua_event_descriptor events[] = {
            {"test.tick", 1},
            {"planet.generated", 1, orion2re::lua::LUA_GAME_STATE_PLANET_MUTABLE},
        };
        const orion2re::lua::s_lua_rule_descriptor rules[] = {
            {"population.growth", 1},
            {"combat.beam_damage", 1},
        };
        const orion2re::lua::s_lua_mod_limits default_limits;
        orion2re::lua::s_lua_mod_runtime* runtime = nullptr;
        if (!orion2re::lua::Create_Mod_Runtime(
                "example", "1.0.0", source, std::strlen(source), "mods/example/main.lua", std::strlen(source), events, 2, rules, 2, limits != nullptr ? limits : &default_limits, &runtime, result)) {
            return nullptr;
        }
        return runtime;
    }

    void Push_Storage_Graph_(lua_State* lua_state, bool reverse) {
        lua_newtable(lua_state);
        const int root = lua_gettop(lua_state);
        lua_newtable(lua_state);
        const int child = lua_gettop(lua_state);
        lua_pushinteger(lua_state, 17);
        lua_setfield(lua_state, child, "value");
        lua_pushvalue(lua_state, child);
        lua_setfield(lua_state, child, "self");

        const auto add_aliases = [&]() {
            lua_pushvalue(lua_state, child);
            lua_setfield(lua_state, root, "node");
            lua_pushvalue(lua_state, child);
            lua_setfield(lua_state, root, "alias");
        };
        const auto add_scalars = [&]() {
            lua_pushboolean(lua_state, 0);
            lua_pushliteral(lua_state, "false-key");
            lua_rawset(lua_state, root);
            lua_pushinteger(lua_state, -9);
            lua_pushliteral(lua_state, "negative-key");
            lua_rawset(lua_state, root);
            lua_pushliteral(lua_state, "count");
            lua_pushinteger(lua_state, -9223372036854775807LL - 1);
            lua_rawset(lua_state, root);
        };
        if (reverse) {
            add_scalars();
            add_aliases();
        } else {
            add_aliases();
            add_scalars();
        }
        lua_remove(lua_state, child);
    }

    orion2re::lua::e_lua_storage_reference_result Read_Test_Reference_(
        lua_State* lua_state,
        int index,
        orion2re::lua::s_lua_storage_reference* reference,
        std::string*) {
        if (lua_type(lua_state, index) != LUA_TUSERDATA) {
            return orion2re::lua::LUA_STORAGE_REFERENCE_UNSUPPORTED;
        }
        reference->kind = 3;
        reference->id = *static_cast<int64_t*>(
            lua_touserdata(lua_state, index));
        reference->generation = 9;
        return orion2re::lua::LUA_STORAGE_REFERENCE_OK;
    }

    bool Write_Test_Reference_(lua_State* lua_state,
        const orion2re::lua::s_lua_storage_reference* reference,
        std::string* diagnostic) {
        if (reference->kind != 3 || reference->generation != 9) {
            *diagnostic = "invalid test reference";
            return false;
        }
        lua_pushinteger(lua_state, reference->id);
        return true;
    }

    int Build_Test_Rule_Context_(lua_State* lua_state, void* user_data) {
        const auto* bonus = static_cast<const lua_Integer*>(user_data);
        lua_newtable(lua_state);
        lua_pushinteger(lua_state, *bonus);
        lua_setfield(lua_state, -2, "bonus");
        return 1;
    }

    struct s_reentrant_rule_context {
        orion2re::lua::s_lua_mod_runtime* runtime = nullptr;
        bool rejected = false;
        std::string diagnostic;
    };

    int Build_Reentrant_Rule_Context_(lua_State* lua_state,
        void* user_data) {
        auto* context = static_cast<s_reentrant_rule_context*>(user_data);
        lua_Integer bonus = 0;
        std::int64_t value = 1;
        orion2re::lua::s_lua_result result;
        context->rejected = !orion2re::lua::Dispatch_Mod_Rule(
            context->runtime,
            "population.growth",
            Build_Test_Rule_Context_,
            &bonus,
            INT16_MIN,
            INT16_MAX,
            &value,
            &result);
        context->diagnostic = result.diagnostic;
        lua_newtable(lua_state);
        lua_pushinteger(lua_state, 0);
        lua_setfield(lua_state, -2, "bonus");
        return 1;
    }

} // namespace

TEST_CASE("Lua storage serialization is canonical and preserves graph identity") {
    orion2re::lua::s_lua_limits runtime_limits;
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_runtime_state* first = nullptr;
    orion2re::lua::s_lua_runtime_state* second = nullptr;
    REQUIRE(orion2re::lua::Create_Runtime_State(
        &runtime_limits, &first, &result));
    REQUIRE(orion2re::lua::Create_Runtime_State(
        &runtime_limits, &second, &result));
    lua_State* first_state =
        orion2re::lua::Get_Runtime_State_Handle(first);
    lua_State* second_state =
        orion2re::lua::Get_Runtime_State_Handle(second);
    Push_Storage_Graph_(first_state, false);
    Push_Storage_Graph_(second_state, true);

    const orion2re::lua::s_lua_storage_limits limits;
    std::vector<uint8_t> first_bytes;
    std::vector<uint8_t> second_bytes;
    std::string diagnostic;
    REQUIRE(orion2re::lua::Serialize_Storage(first_state, -1, &limits, &first_bytes, &diagnostic));
    REQUIRE(orion2re::lua::Serialize_Storage(second_state, -1, &limits, &second_bytes, &diagnostic));
    CHECK(first_bytes == second_bytes);

    lua_settop(second_state, 0);
    REQUIRE(orion2re::lua::Deserialize_Storage(second_state,
        first_bytes.data(),
        first_bytes.size(),
        &limits,
        &diagnostic));
    REQUIRE(lua_istable(second_state, -1));
    lua_getfield(second_state, -1, "node");
    lua_getfield(second_state, -2, "alias");
    CHECK(lua_rawequal(second_state, -1, -2) != 0);
    lua_getfield(second_state, -2, "self");
    CHECK(lua_rawequal(second_state, -1, -3) != 0);
    lua_settop(second_state, 1);
    lua_getfield(second_state, 1, "count");
    CHECK(lua_tointeger(second_state, -1) ==
        (-9223372036854775807LL - 1));

    orion2re::lua::Destroy_Runtime_State(first);
    orion2re::lua::Destroy_Runtime_State(second);
}

TEST_CASE("Lua combat beam damage rule rejects negative damage") {
    const char source[] =
        "rules.modify('combat.beam_damage', function(context, value)\n"
        "    return -1\n"
        "end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = Create_(source, &result);
    REQUIRE(runtime != nullptr);
    lua_Integer bonus = 0;
    std::int64_t value = 5;
    CHECK_FALSE(orion2re::lua::Dispatch_Mod_Rule(runtime,
        "combat.beam_damage",
        Build_Test_Rule_Context_,
        &bonus,
        0,
        INT16_MAX,
        &value,
        &result));
    CHECK(result.diagnostic.find("outside the permitted range") !=
        std::string::npos);
    CHECK(value == 5);
    CHECK_FALSE(orion2re::lua::Rule_Modifier_Active());
    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("Lua storage serialization rejects unsupported values and limits") {
    orion2re::lua::s_lua_limits runtime_limits;
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_runtime_state* runtime = nullptr;
    REQUIRE(orion2re::lua::Create_Runtime_State(
        &runtime_limits, &runtime, &result));
    lua_State* lua_state =
        orion2re::lua::Get_Runtime_State_Handle(runtime);
    lua_newtable(lua_state);
    lua_pushcfunction(lua_state, Record_);
    lua_setfield(lua_state, -2, "function_value");

    orion2re::lua::s_lua_storage_limits limits;
    std::vector<uint8_t> bytes;
    std::string diagnostic;
    CHECK_FALSE(orion2re::lua::Serialize_Storage(lua_state, -1, &limits, &bytes, &diagnostic));
    CHECK(diagnostic.find("support only Boolean") != std::string::npos);

    lua_settop(lua_state, 0);
    lua_newtable(lua_state);
    lua_pushliteral(lua_state, "value");
    lua_setfield(lua_state, -2, "key");
    limits.bytes = 8;
    CHECK_FALSE(orion2re::lua::Serialize_Storage(lua_state, -1, &limits, &bytes, &diagnostic));
    CHECK(diagnostic.find("canonical byte limit") != std::string::npos);
    orion2re::lua::Destroy_Runtime_State(runtime);
}

TEST_CASE("Lua storage references use canonical tagged identity") {
    orion2re::lua::s_lua_limits runtime_limits;
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_runtime_state* runtime = nullptr;
    REQUIRE(orion2re::lua::Create_Runtime_State(
        &runtime_limits, &runtime, &result));
    lua_State* lua_state =
        orion2re::lua::Get_Runtime_State_Handle(runtime);
    const orion2re::lua::s_lua_storage_reference_adapter adapter = {
        Read_Test_Reference_,
        Write_Test_Reference_,
    };
    const orion2re::lua::s_lua_storage_limits limits;
    std::vector<uint8_t> first;
    std::vector<uint8_t> second;
    std::string diagnostic;

    lua_newtable(lua_state);
    *static_cast<int64_t*>(lua_newuserdatauv(
        lua_state, sizeof(int64_t), 0)) = 17;
    lua_setfield(lua_state, -2, "reference");
    REQUIRE(orion2re::lua::Serialize_Storage_With_References(
        lua_state, -1, &limits, &adapter, &first, &diagnostic));
    lua_settop(lua_state, 0);
    lua_newtable(lua_state);
    *static_cast<int64_t*>(lua_newuserdatauv(
        lua_state, sizeof(int64_t), 0)) = 17;
    lua_setfield(lua_state, -2, "reference");
    REQUIRE(orion2re::lua::Serialize_Storage_With_References(
        lua_state, -1, &limits, &adapter, &second, &diagnostic));
    CHECK(first == second);

    lua_settop(lua_state, 0);
    REQUIRE(orion2re::lua::Deserialize_Storage_With_References(
        lua_state,
        first.data(),
        first.size(),
        &limits,
        &adapter,
        &diagnostic));
    lua_getfield(lua_state, -1, "reference");
    CHECK(lua_tointeger(lua_state, -1) == 17);

    std::vector<uint8_t> invalid = first;
    REQUIRE(invalid.size() >= 18);
    invalid[invalid.size() - 17] = 0;
    lua_settop(lua_state, 0);
    CHECK_FALSE(orion2re::lua::Deserialize_Storage_With_References(
        lua_state,
        invalid.data(),
        invalid.size(),
        &limits,
        &adapter,
        &diagnostic));
    CHECK(diagnostic.find("engine reference") != std::string::npos);

    orion2re::lua::Destroy_Runtime_State(runtime);
}

TEST_CASE("Lua mod storage restores through migrations before game loaded") {
    const char saved_source[] =
        "events.on('test.tick', function(event)\n"
        "  storage.value = event.amount\n"
        "end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* saved_runtime =
        Create_(saved_source, &result);
    REQUIRE(saved_runtime != nullptr);
    s_event_observation saved_observation;
    saved_observation.amount = 4;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(saved_runtime,
        "test.tick",
        Build_Event_,
        &saved_observation,
        &result));
    std::vector<uint8_t> saved_storage;
    std::string diagnostic;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        saved_runtime, &saved_storage, &diagnostic));
    orion2re::lua::Destroy_Mod_Runtime(saved_runtime);

    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files, "main.lua", "local loaded = false\n"
                                      "events.on('game.loaded', function()\n"
                                      "  if storage.value ~= 7 or storage.version ~= 2 then\n"
                                      "    error('storage was not migrated before load')\n"
                                      "  end\n"
                                      "  loaded = true\n"
                                      "end)\n"
                                      "events.on('game.initialized', function(event)\n"
                                      "  event.report(loaded and storage.value or -1, storage.version)\n"
                                      "end)\n");
    Write_Module_(&files, "migrations/001-002.lua", "if migration.from ~= 1 or migration.to ~= 2 then\n"
                                                    "  error('wrong migration metadata')\n"
                                                    "end\n"
                                                    "if type(getmetatable(migration)) ~= 'string' then\n"
                                                    "  error('migration metadata table is exposed')\n"
                                                    "end\n"
                                                    "storage.value = storage.value + 3\n"
                                                    "storage.version = 2\n");
    config::s_mod_manifest package = Runtime_Package_("example");
    package.version_text = "2.0.0";
    package.lua_runtime.storage_version = 2;
    package.lua_runtime.migrations.push_back(
        {1, 2, "migrations/001-002.lua"});
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtime_set = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages, &limits, &registry, &runtime_set, &config_diagnostic));
    auto* runtime =
        orion2re::lua::Find_Mod_Runtime(runtime_set, "example");
    REQUIRE(runtime != nullptr);
    REQUIRE(orion2re::lua::Restore_Mod_Storage(runtime, 1, saved_storage.data(), saved_storage.size(), &diagnostic));
    REQUIRE(orion2re::lua::Dispatch_Loaded_Event(
        runtime_set, &diagnostic));

    s_event_observation observation;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(runtime,
        "game.initialized",
        Build_Event_,
        &observation,
        &result));
    CHECK(observation.transient == 7);
    CHECK(observation.stored == 2);

    orion2re::lua::Destroy_Mod_Runtime_Set(runtime_set);
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua save dispatches configuration changes after migrations") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files,
        "main.lua",
        "events.on('game.initialized', function() storage.value = 4 end)\n");
    config::s_mod_manifest saved_package = Runtime_Package_("example");
    std::vector<config::s_mod_manifest> saved_packages = {saved_package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry saved_registry;
    orion2re::lua::s_lua_mod_runtime_set* saved_runtimes = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&saved_packages,
        &limits,
        &saved_registry,
        &saved_runtimes,
        &config_diagnostic));
    saved_registry.packages_high_to_low_priority.push_back(
        {"example", "1.0.0"});
    auto* saved_runtime =
        orion2re::lua::Find_Mod_Runtime(saved_runtimes, "example");
    REQUIRE(saved_runtime != nullptr);
    orion2re::lua::s_lua_result result;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(saved_runtime,
        "game.initialized",
        nullptr,
        nullptr,
        &result));
    orion2re::lua::s_lua_save_snapshot snapshot;
    snapshot.registry = saved_registry;
    snapshot.storage.push_back({"example", 1, {}});
    std::string diagnostic;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(saved_runtime,
        &snapshot.storage[0].bytes,
        &diagnostic));
    orion2re::lua::Destroy_Mod_Runtime_Set(saved_runtimes);

    Write_Module_(&files,
        "main.lua",
        "local loaded = false\n"
        "events.on('game.configuration_changed', function(event)\n"
        "  if storage.value ~= 7 or storage.version ~= 2 then\n"
        "    error('storage migration did not run first')\n"
        "  end\n"
        "  if event.reason ~= 'package_version_changed' or\n"
        "      #event.changes ~= 1 or #event.changed_groups ~= 0 then\n"
        "    error('wrong change count or reason')\n"
        "  end\n"
        "  local change = event.changes[1]\n"
        "  if change.package_id ~= 'example' or\n"
        "      change.old_package_version ~= '1.0.0' or\n"
        "      change.new_package_version ~= '2.0.0' or\n"
        "      change.old_storage_version ~= 1 or\n"
        "      change.new_storage_version ~= 2 then\n"
        "    error('wrong configuration change identity')\n"
        "  end\n"
        "  if pcall(function() event.changed = true end) or\n"
        "      pcall(function() event.changes[1] = change end) or\n"
        "      pcall(function() change.package_id = 'other' end) then\n"
        "    error('configuration change payload is mutable')\n"
        "  end\n"
        "  storage.configured = true\n"
        "end)\n"
        "events.on('game.loaded', function()\n"
        "  if not storage.configured then\n"
        "    error('configuration change did not run before load')\n"
        "  end\n"
        "  loaded = true\n"
        "end)\n"
        "events.on('game.initialized', function(event)\n"
        "  event.report(loaded and storage.value or -1, storage.version)\n"
        "end)\n");
    Write_Module_(&files,
        "migrations/001-002.lua",
        "storage.value = storage.value + 3\n"
        "storage.version = 2\n");
    config::s_mod_manifest active_package = Runtime_Package_("example");
    active_package.version_text = "2.0.0";
    active_package.lua_runtime.storage_version = 2;
    active_package.lua_runtime.migrations.push_back(
        {1, 2, "migrations/001-002.lua"});
    std::vector<config::s_mod_manifest> active_packages = {active_package};
    orion2re::lua::s_lua_registry active_registry;
    orion2re::lua::s_lua_mod_runtime_set* active_runtimes = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&active_packages,
        &limits,
        &active_registry,
        &active_runtimes,
        &config_diagnostic));
    active_registry.packages_high_to_low_priority.push_back(
        {"example", "2.0.0"});
    orion2re::lua::s_lua_save_snapshot inconsistent_snapshot = snapshot;
    inconsistent_snapshot.registry.runtime_packages[0].package_version =
        "0.9.0";
    CHECK_FALSE(orion2re::lua::Restore_Lua_Save_Snapshot(
        &inconsistent_snapshot,
        &active_registry,
        active_runtimes,
        &diagnostic));
    CHECK(diagnostic ==
        "Lua package and runtime versions differ for package 'example'");
    orion2re::lua::s_lua_registry inconsistent_active_registry =
        active_registry;
    inconsistent_active_registry.runtime_packages[0].package_version =
        "2.1.0";
    CHECK_FALSE(orion2re::lua::Restore_Lua_Save_Snapshot(&snapshot,
        &inconsistent_active_registry,
        active_runtimes,
        &diagnostic));
    CHECK(diagnostic ==
        "Lua package and runtime versions differ for package 'example'");
    REQUIRE(orion2re::lua::Restore_Lua_Save_Snapshot(&snapshot,
        &active_registry,
        active_runtimes,
        &diagnostic));

    s_event_observation observation;
    auto* active_runtime =
        orion2re::lua::Find_Mod_Runtime(active_runtimes, "example");
    REQUIRE(active_runtime != nullptr);
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(active_runtime,
        "game.initialized",
        Build_Event_,
        &observation,
        &result));
    CHECK(observation.transient == 7);
    CHECK(observation.stored == 2);

    orion2re::lua::Destroy_Mod_Runtime_Set(active_runtimes);
    Shutdown_Module_Files_(&files);
}

TEST_CASE("game loaded rejects persistent storage changes") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files, "main.lua", "events.on('game.loaded', function() storage.changed = true end)\n"
                                      "events.on('game.initialized', function(event)\n"
                                      "  event.report(storage.changed and 1 or 0, 0)\n"
                                      "end)\n");
    config::s_mod_manifest package = Runtime_Package_("example");
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtime_set = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages, &limits, &registry, &runtime_set, &config_diagnostic));
    std::string diagnostic;
    CHECK_FALSE(orion2re::lua::Dispatch_Loaded_Event(
        runtime_set, &diagnostic));
    CHECK(diagnostic.find("modified persistent storage") !=
        std::string::npos);

    orion2re::lua::Destroy_Mod_Runtime_Set(runtime_set);
    Shutdown_Module_Files_(&files);
}

TEST_CASE("vanilla Lua save restoration accepts empty Lua identity") {
    const std::vector<config::s_mod_manifest> packages;
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry candidate_registry;
    orion2re::lua::s_lua_mod_runtime_set* candidate = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages,
        &limits,
        &candidate_registry,
        &candidate,
        &config_diagnostic));

    orion2re::lua::s_lua_save_snapshot snapshot;
    snapshot.registry.synchronized_config_hash = "stale saved identity";
    candidate_registry.synchronized_config_hash = "reconstructed identity";
    snapshot.registry.fingerprint = "stale fingerprint";
    candidate_registry.fingerprint = "current fingerprint";
    std::string diagnostic;
    CHECK(orion2re::lua::Restore_Lua_Save_Snapshot(
        &snapshot, &candidate_registry, candidate, &diagnostic));
    CHECK(diagnostic.empty());

    orion2re::lua::Destroy_Mod_Runtime_Set(candidate);
}

TEST_CASE("network compatibility accepts Lua for negotiated setup") {
    std::string diagnostic = "stale";
    CHECK_FALSE(orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
        nullptr, &diagnostic));
    CHECK(diagnostic.empty());

    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files,
        "main.lua",
        "services.register('galaxy_generator', 1, function() end)\n"
        "events.on('game.initialized', function() end)\n");
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("example"),
    };
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtime_set = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &registry,
        &runtime_set,
        &config_diagnostic));
    REQUIRE(registry.runtime_packages.size() == 1);
    REQUIRE(registry.runtime_packages[0].services.size() == 1);
    CHECK(registry.runtime_packages[0].services[0].name ==
        "galaxy_generator");
    CHECK(registry.runtime_packages[0].services[0].version == 1);

    CHECK(orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
        runtime_set, &diagnostic));
    CHECK(diagnostic.empty());
    CHECK_FALSE(orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
        runtime_set, nullptr));

    orion2re::lua::Destroy_Mod_Runtime_Set(runtime_set);
    Shutdown_Module_Files_(&files);

    const std::vector<config::s_mod_manifest> vanilla_packages;
    runtime_set = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&vanilla_packages,
        &limits,
        &registry,
        &runtime_set,
        &config_diagnostic));
    CHECK(orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
        runtime_set, &diagnostic));
    CHECK(diagnostic.empty());
    orion2re::lua::Destroy_Mod_Runtime_Set(runtime_set);
}

TEST_CASE("Lua network compatibility rejects hooks outside the MVP allowlist") {
    struct s_rejection {
        const char* source;
        const char* diagnostic;
    };
    const s_rejection rejections[] = {
        {"events.on('ship.destroyed', function() end)\n",
            "network-disabled event 'ship.destroyed'"},
        {"rules.modify('combat.beam_damage', function(context, value) return value end)\n",
            "network-disabled rule 'combat.beam_damage'"},
    };
    for (const s_rejection& rejection : rejections) {
        CAPTURE(rejection.source);
        s_module_files files;
        Initialize_Module_Files_(&files);
        Write_Module_(&files, "main.lua", rejection.source);
        const std::vector<config::s_mod_manifest> packages = {
            Runtime_Package_("example"),
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
        std::string diagnostic;
        CHECK_FALSE(orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
            runtimes, &diagnostic));
        CHECK(diagnostic.find(rejection.diagnostic) != std::string::npos);
        orion2re::lua::Destroy_Mod_Runtime_Set(runtimes);
        Shutdown_Module_Files_(&files);
    }
}

TEST_CASE("Lua network compatibility accepts authoritative population growth") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files,
        "main.lua",
        "rules.modify('population.growth', function(context, value) return value end)\n");
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("example"),
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
    std::string diagnostic;
    CHECK(orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
        runtimes, &diagnostic));
    CHECK(diagnostic.empty());
    orion2re::lua::Destroy_Mod_Runtime_Set(runtimes);
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua network setup records are bounded and exact") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files,
        "main.lua",
        "events.on('game.initialized', function() end)\n");
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("example"),
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
    registry.packages_high_to_low_priority.push_back(
        {"example", "1.0.0"});
    registry.synchronized_config_hash = "config";
    orion2re::lua::Commit_Registry(&registry);
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const uint8_t configuration[] = {1, 2, 3, 4};
    std::vector<uint8_t> packet(
        orion2re::lua::LUA_NETWORK_MAX_BYTES);
    uint32_t packet_size = 0;
    orion2re::lua::s_lua_network_digest expected_digest = {};
    std::string diagnostic;
    REQUIRE(orion2re::lua::Write_Lua_Network_Setup_Offer(
        packet.data(),
        static_cast<uint32_t>(packet.size()),
        orion2re::lua::LUA_NETWORK_SETUP_NEW_GAME,
        42,
        configuration,
        sizeof(configuration),
        &packet_size,
        &expected_digest,
        &diagnostic));
    REQUIRE(packet_size < packet.size());

    orion2re::lua::s_lua_network_setup_offer offer;
    REQUIRE(orion2re::lua::Read_Lua_Network_Setup_Offer(
        packet.data(),
        packet_size,
        &offer,
        &diagnostic));
    CHECK(offer.kind == orion2re::lua::LUA_NETWORK_SETUP_NEW_GAME);
    CHECK(offer.offer_id == 42);
    CHECK(offer.configuration_size == sizeof(configuration));
    CHECK(memcmp(offer.configuration,
              configuration,
              sizeof(configuration)) == 0);
    orion2re::lua::s_lua_network_digest local_digest = {};
    uint32_t mismatch_record_index = UINT32_MAX;
    REQUIRE(orion2re::lua::Validate_Lua_Network_Setup_Offer(
        &offer,
        orion2re::lua::Get_Active_Registry(),
        &local_digest,
        &mismatch_record_index,
        &diagnostic));
    CHECK(memcmp(local_digest.bytes,
              expected_digest.bytes,
              sizeof(local_digest.bytes)) == 0);

    orion2re::lua::Set_Lua_Network_Offer_Context(41, nullptr);
    uint8_t rejected_result[128] = {};
    uint32_t rejected_result_size = 0;
    REQUIRE(orion2re::lua::Process_Lua_Network_Setup_Offer(
        packet.data(),
        packet_size,
        orion2re::lua::LUA_NETWORK_SETUP_LOAD_GAME,
        rejected_result,
        sizeof(rejected_result),
        &rejected_result_size));
    CHECK_FALSE(orion2re::lua::Validate_Lua_Network_Setup_Result(
        rejected_result, rejected_result_size, &diagnostic));
    CHECK(diagnostic == "Lua setup offer has the wrong setup kind");
    REQUIRE(orion2re::lua::Write_Lua_Network_Setup_Decision(
        rejected_result,
        sizeof(rejected_result),
        false,
        diagnostic,
        &rejected_result_size));
    CHECK_FALSE(orion2re::lua::Validate_Lua_Network_Setup_Result(
        rejected_result, rejected_result_size, &diagnostic));
    CHECK(diagnostic == "Lua setup offer has the wrong setup kind");

    orion2re::lua::s_lua_registry reordered =
        *orion2re::lua::Get_Active_Registry();
    REQUIRE(reordered.runtime_packages.size() == 1);
    REQUIRE(reordered.runtime_packages[0].callbacks.size() == 1);
    reordered.runtime_packages[0].callbacks[0].registration_ordinal = 1;
    CHECK_FALSE(orion2re::lua::Validate_Lua_Network_Setup_Offer(&offer,
        &reordered,
        &local_digest,
        &mismatch_record_index,
        &diagnostic));
    CHECK(diagnostic == "Lua network callback 'game.initialized' differs");

    packet[packet_size - 1] ^= 1;
    REQUIRE(orion2re::lua::Read_Lua_Network_Setup_Offer(
        packet.data(),
        packet_size,
        &offer,
        &diagnostic));
    CHECK_FALSE(orion2re::lua::Validate_Lua_Network_Setup_Offer(
        &offer,
        orion2re::lua::Get_Active_Registry(),
        &local_digest,
        &mismatch_record_index,
        &diagnostic));
    CHECK(diagnostic ==
        "Lua network synchronized configuration identity differs");
    CHECK(mismatch_record_index != UINT32_MAX);

    packet[packet_size - 1] ^= 1;
    REQUIRE(orion2re::lua::Read_Lua_Network_Setup_Offer(
        packet.data(),
        packet_size,
        &offer,
        &diagnostic));
    const_cast<uint8_t*>(offer.identity)[offer.identity_size / 2] ^= 1;
    CHECK_FALSE(orion2re::lua::Validate_Lua_Network_Setup_Offer(
        &offer,
        orion2re::lua::Get_Active_Registry(),
        &local_digest,
        &mismatch_record_index,
        &diagnostic));
    CHECK_FALSE(diagnostic.empty());
    const_cast<uint8_t*>(offer.identity)[offer.identity_size / 2] ^= 1;
    CHECK_FALSE(orion2re::lua::Read_Lua_Network_Setup_Offer(
        packet.data(),
        packet_size - 1,
        &offer,
        &diagnostic));

    uint8_t result[128] = {};
    uint32_t result_size = 0;
    orion2re::lua::s_lua_network_result expected_result;
    expected_result.offer_id = 42;
    expected_result.accepted = true;
    expected_result.digest = expected_digest;
    REQUIRE(orion2re::lua::Write_Lua_Network_Result(result,
        sizeof(result),
        &expected_result,
        &result_size));
    orion2re::lua::s_lua_network_result decoded_result;
    REQUIRE(orion2re::lua::Read_Lua_Network_Result(result,
        result_size,
        &decoded_result));
    CHECK(decoded_result.accepted);
    CHECK(decoded_result.offer_id == 42);
    CHECK(memcmp(decoded_result.digest.bytes,
              expected_digest.bytes,
              sizeof(decoded_result.digest.bytes)) == 0);
    orion2re::lua::Set_Lua_Network_Offer_Context(42, &expected_digest);
    REQUIRE(orion2re::lua::Validate_Lua_Network_Setup_Result(
        result, result_size, &diagnostic));
    for (uint32_t truncated_size = 56; truncated_size < 60;
         ++truncated_size) {
        CHECK_FALSE(orion2re::lua::Read_Lua_Network_Result(result,
            truncated_size,
            &decoded_result));
    }

    expected_result.accepted = false;
    expected_result.diagnostic_code =
        orion2re::lua::LUA_NETWORK_DIAGNOSTIC_RUNTIME_IDENTITY;
    expected_result.record_index = 7;
    expected_result.diagnostic = "mods/example/main.lua";
    REQUIRE(orion2re::lua::Write_Lua_Network_Result(result,
        sizeof(result),
        &expected_result,
        &result_size));
    REQUIRE(orion2re::lua::Read_Lua_Network_Result(result,
        result_size,
        &decoded_result));
    CHECK_FALSE(decoded_result.accepted);
    CHECK(decoded_result.diagnostic_code ==
        orion2re::lua::LUA_NETWORK_DIAGNOSTIC_RUNTIME_IDENTITY);
    CHECK(decoded_result.record_index == 7);
    CHECK(decoded_result.diagnostic == "mods/example/main.lua");
    CHECK_FALSE(orion2re::lua::Validate_Lua_Network_Setup_Result(
        result, result_size, &diagnostic));
    CHECK(diagnostic == "mods/example/main.lua");

    REQUIRE(orion2re::lua::Write_Lua_Network_Setup_Decision(result,
        sizeof(result),
        true,
        {},
        &result_size));
    REQUIRE(orion2re::lua::Validate_Lua_Network_Setup_Result(
        result, result_size, &diagnostic));
    REQUIRE(orion2re::lua::Write_Lua_Network_Setup_Decision(result,
        sizeof(result),
        false,
        "host rejected incompatible content",
        &result_size));
    CHECK_FALSE(orion2re::lua::Validate_Lua_Network_Setup_Result(
        result, result_size, &diagnostic));
    CHECK(diagnostic == "host rejected incompatible content");

    orion2re::lua::Shutdown_Lua();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua gameplay host handoff rebuilds once and rejects mismatches") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files,
        "main.lua",
        "rules.modify('population.growth', function(context, value) return value end)\n");
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("example"),
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
    registry.packages_high_to_low_priority.push_back(
        {"example", "1.0.0"});
    orion2re::lua::Commit_Registry(&registry);
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    config::Reset_Lua_Reconstruction_Fake();
    orion2re::lua::Reset_Lua_Network_Session();
    std::string diagnostic;
    REQUIRE(orion2re::lua::Prepare_Lua_Gameplay_Host_Authority(
        101, 202, &diagnostic));
    CHECK(config::Lua_Reconstruction_Calls() == 0);
    REQUIRE(orion2re::lua::Prepare_Lua_Gameplay_Host_Authority(
        202, 202, &diagnostic));
    CHECK(config::Lua_Reconstruction_Calls() == 1);
    REQUIRE(orion2re::lua::Prepare_Lua_Gameplay_Host_Authority(
        202, 202, &diagnostic));
    CHECK(config::Lua_Reconstruction_Calls() == 1);

    config::Set_Lua_Reconstruction_Result(false);
    CHECK_FALSE(orion2re::lua::Prepare_Lua_Gameplay_Host_Authority(
        303, 303, &diagnostic));
    CHECK(diagnostic == "reconstructed Lua identity differs");
    CHECK(config::Lua_Reconstruction_Calls() == 2);
    CHECK_FALSE(orion2re::lua::Prepare_Lua_Gameplay_Host_Authority(
        303, 303, &diagnostic));
    CHECK(config::Lua_Reconstruction_Calls() == 3);

    orion2re::lua::Shutdown_Lua();
    orion2re::lua::Reset_Lua_Network_Session();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua network state replacement commits atomically") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files,
        "main.lua",
        "prototypes.race_traits:add { id = 'scavenger', cost = 5 }\n"
        "events.on('game.initialized', function()\n"
        "  storage.value = (storage.value or 0) + 1\n"
        "end)\n");
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("example"),
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
    registry.packages_high_to_low_priority.push_back(
        {"example", "1.0.0"});
    registry.synchronized_config_hash = "config";
    orion2re::lua::Commit_Registry(&registry);
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    std::string diagnostic;
    REQUIRE(orion2re::lua::Dispatch_Active_Initialized_Event(
        &diagnostic));
    REQUIRE(orion2re::lua::Set_Player_Race_Trait_Assignment(
        orion2re::lua::Get_Active_Game_Traits(),
        1,
        "scavenger",
        true));
    orion2re::lua::game::Restore_Legacy_Ship_Generations();

    std::vector<uint8_t> network_state(
        orion2re::lua::LUA_NETWORK_MAX_BYTES);
    uint32_t network_state_size = 0;
    REQUIRE(orion2re::lua::Capture_Lua_Network_State(
        network_state.data(),
        static_cast<uint32_t>(network_state.size()),
        &network_state_size,
        &diagnostic));
    auto* runtime = orion2re::lua::Find_Mod_Runtime(
        const_cast<orion2re::lua::s_lua_mod_runtime_set*>(
            orion2re::lua::Get_Active_Mod_Runtimes()),
        "example");
    REQUIRE(runtime != nullptr);
    std::vector<uint8_t> captured_storage;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &captured_storage, &diagnostic));

    REQUIRE(orion2re::lua::Dispatch_Active_Initialized_Event(
        &diagnostic));
    REQUIRE(orion2re::lua::Set_Player_Race_Trait_Assignment(
        orion2re::lua::Get_Active_Game_Traits(),
        1,
        "scavenger",
        false));
    REQUIRE(orion2re::lua::game::Advance_Ship_Generation(0));
    std::vector<uint8_t> changed_storage;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &changed_storage, &diagnostic));
    REQUIRE(changed_storage != captured_storage);

    orion2re::lua::s_lua_network_state_transaction* transaction = nullptr;
    REQUIRE(orion2re::lua::Prepare_Lua_Network_State(
        network_state.data(),
        network_state_size,
        static_cast<uint32_t>(MOX::NUM_SHIPS),
        &transaction,
        &diagnostic));
    std::vector<uint8_t> before_commit;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &before_commit, &diagnostic));
    CHECK(before_commit == changed_storage);
    CHECK_FALSE(orion2re::lua::Has_Player_Race_Trait_Assignment(
        orion2re::lua::Get_Active_Game_Traits(),
        1,
        "scavenger"));
    CHECK(orion2re::lua::game::Ship_Generation(0) == 2);

    orion2re::lua::Commit_Lua_Network_State(&transaction);
    CHECK(transaction == nullptr);
    std::vector<uint8_t> restored_storage;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &restored_storage, &diagnostic));
    CHECK(restored_storage == captured_storage);
    CHECK(orion2re::lua::Has_Player_Race_Trait_Assignment(
        orion2re::lua::Get_Active_Game_Traits(),
        1,
        "scavenger"));
    CHECK(orion2re::lua::game::Ship_Generation(0) == 1);

    network_state[8] = 2;
    CHECK_FALSE(orion2re::lua::Prepare_Lua_Network_State(
        network_state.data(),
        network_state_size,
        static_cast<uint32_t>(MOX::NUM_SHIPS),
        &transaction,
        &diagnostic));
    CHECK(transaction == nullptr);
    std::vector<uint8_t> after_rejection;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &after_rejection, &diagnostic));
    CHECK(after_rejection == captured_storage);

    orion2re::lua::Shutdown_Lua();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua new-game setup callbacks execute only on the gameplay host") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files,
        "main.lua",
        "events.on('new_game.technology_setup', function()\n"
        "  storage.calls = (storage.calls or 0) + 1\n"
        "end)\n"
        "events.on('game.initialized', function()\n"
        "  storage.authority = (storage.calls or 0) + 1\n"
        "end)\n");
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("example"),
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
    CHECK(orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
        runtimes, &config_diagnostic.message));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const auto prior_game_type = MOX::game_type;
    orion2re::lua::game::s_technology_setup_event event;
    std::string diagnostic;
    MOX::game_type = 2;
    REQUIRE(orion2re::lua::game::Technology_Setup(&event, &diagnostic));
    auto* runtime = orion2re::lua::Find_Mod_Runtime(
        const_cast<orion2re::lua::s_lua_mod_runtime_set*>(
            orion2re::lua::Get_Active_Mod_Runtimes()),
        "example");
    REQUIRE(runtime != nullptr);
    std::vector<uint8_t> host_storage;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &host_storage, &diagnostic));

    std::vector<uint8_t> network_state(
        orion2re::lua::LUA_NETWORK_MAX_BYTES);
    uint32_t network_state_size = 0;
    REQUIRE(orion2re::lua::Capture_Lua_Network_State(
        network_state.data(),
        static_cast<uint32_t>(network_state.size()),
        &network_state_size,
        &diagnostic));
    REQUIRE(orion2re::lua::Dispatch_Active_Initialized_Event(&diagnostic));
    std::vector<uint8_t> expected_authority_storage;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &expected_authority_storage, &diagnostic));

    orion2re::lua::Reset_Mod_Runtimes();
    orion2re::lua::s_lua_registry reconstructed_registry;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &reconstructed_registry,
        &runtimes,
        &config_diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);
    runtime = orion2re::lua::Find_Mod_Runtime(
        const_cast<orion2re::lua::s_lua_mod_runtime_set*>(
            orion2re::lua::Get_Active_Mod_Runtimes()),
        "example");
    REQUIRE(runtime != nullptr);
    orion2re::lua::s_lua_network_state_transaction* transaction = nullptr;
    REQUIRE(orion2re::lua::Prepare_Lua_Network_State(network_state.data(),
        network_state_size,
        static_cast<uint32_t>(MOX::NUM_SHIPS),
        &transaction,
        &diagnostic));
    orion2re::lua::Commit_Lua_Network_State(&transaction);
    std::vector<uint8_t> reconstructed_storage;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &reconstructed_storage, &diagnostic));
    CHECK(reconstructed_storage == host_storage);
    REQUIRE(orion2re::lua::Dispatch_Active_Initialized_Event(&diagnostic));
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &reconstructed_storage, &diagnostic));
    CHECK(reconstructed_storage == expected_authority_storage);

    MOX::game_type = 3;
    const std::vector<uint8_t> before_client_setup = reconstructed_storage;
    REQUIRE(orion2re::lua::game::Technology_Setup(&event, &diagnostic));
    std::vector<uint8_t> client_storage;
    REQUIRE(orion2re::lua::Serialize_Mod_Storage(
        runtime, &client_storage, &diagnostic));
    CHECK(client_storage == before_client_setup);

    MOX::game_type = prior_game_type;
    orion2re::lua::Shutdown_Lua();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("staged Lua runtime owns ship generations until commit") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files, "main.lua", "return nil\n");
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("example"),
    };
    orion2re::lua::game::Reset_Ship_Generations();
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic config_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &registry,
        &runtimes,
        &config_diagnostic));

    {
        orion2re::lua::game::s_ship_generation_scope scope(
            orion2re::lua::Get_Mod_Runtime_Ship_Generations(runtimes));
        orion2re::lua::game::Restore_Legacy_Ship_Generations();
        REQUIRE(orion2re::lua::game::Advance_Ship_Generation(0));
        CHECK(orion2re::lua::game::Ship_Generation(0) == 2);
    }
    CHECK(orion2re::lua::game::Ship_Generation(0) == 0);
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);
    CHECK(orion2re::lua::game::Ship_Generation(0) == 2);

    orion2re::lua::Shutdown_Lua();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua source hash mismatch can be allowed for mod development") {
    orion2re::lua::s_lua_registry saved;
    saved.packages_high_to_low_priority.push_back({"example", "1.0.0"});
    saved.runtime_packages.resize(1);
    saved.runtime_packages[0].package_id = "example";
    saved.runtime_packages[0].package_version = "1.0.0";
    saved.runtime_packages[0].api = 1;
    saved.runtime_packages[0].sources.push_back(
        {"mods/example/main.lua", "saved hash"});
    saved.synchronized_config_hash = "config";
    orion2re::lua::s_lua_registry active = saved;
    active.runtime_packages[0].sources[0].source_hash = "active hash";
    active.fingerprint = "active fingerprint";
    saved.fingerprint = "saved fingerprint";
    std::string diagnostic;
    CHECK_FALSE(orion2re::lua::Compare_Registry_Save_Compatibility(
        &saved, &active, false, &diagnostic));
    CHECK(diagnostic ==
        "loaded Lua file content differs: 'mods/example/main.lua'");
    CHECK(orion2re::lua::Compare_Registry_Save_Compatibility(
        &saved, &active, true, &diagnostic));
    CHECK(diagnostic.empty());

    active.synchronized_config_hash = "changed config";
    CHECK_FALSE(orion2re::lua::Compare_Registry_Save_Compatibility(
        &saved, &active, true, &diagnostic));
    CHECK(diagnostic ==
        "synchronized gameplay/technology configuration differs");
    CHECK(orion2re::lua::Compare_Registry_Save_Compatibility(
        &saved, &active, true, &diagnostic, false));
    CHECK(diagnostic.empty());

    active = saved;
    active.packages_high_to_low_priority[0].version = "2.0.0";
    active.runtime_packages[0].package_version = "2.0.0";
    active.runtime_packages[0].sources.push_back(
        {"mods/example/main.lua", "active hash"});
    active.runtime_packages[0].sources.push_back(
        {"mods/example/new_module.lua", "new hash"});
    CHECK(orion2re::lua::Compare_Registry_Save_Compatibility(
        &saved, &active, false, &diagnostic));
    CHECK(diagnostic.empty());
}

TEST_CASE("Lua mod runtime retains callbacks globals and storage") {
    const char source[] =
        "local transient = 0\n"
        "events.on('test.tick', function(event)\n"
        "    transient = transient + 1\n"
        "    storage.total = (storage.total or 0) + event.amount\n"
        "    event.report(transient, storage.total)\n"
        "end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = Create_(source, &result);
    REQUIRE(runtime != nullptr);
    REQUIRE(orion2re::lua::Get_Mod_Callback_Count(runtime) == 1);

    orion2re::lua::s_lua_callback_info callback;
    REQUIRE(orion2re::lua::Get_Mod_Callback_Info(runtime, 0, &callback));
    CHECK(callback.kind == orion2re::lua::LUA_CALLBACK_EVENT);
    CHECK(callback.extension_name == "test.tick");
    CHECK(callback.registration_ordinal == 0);
    CHECK(callback.registration_source == "mods/example/main.lua");
    CHECK(callback.registration_line == 2);
    CHECK(callback.callback_source == "mods/example/main.lua");
    CHECK(callback.callback_line == 2);

    s_event_observation observation;
    observation.amount = 4;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.tick", Build_Event_, &observation, &result));
    CHECK(observation.transient == 1);
    CHECK(observation.stored == 4);

    observation.amount = 7;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.tick", Build_Event_, &observation, &result));
    CHECK(observation.transient == 2);
    CHECK(observation.stored == 11);
    CHECK(observation.reports == 2);

    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("Lua rule modifiers compose in registration order") {
    const char source[] =
        "rules.modify('population.growth', function(context, value)\n"
        "    return value + context.bonus\n"
        "end)\n"
        "rules.modify('population.growth', function(context, value)\n"
        "    return value * 2\n"
        "end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = Create_(source, &result);
    REQUIRE(runtime != nullptr);
    REQUIRE(orion2re::lua::Get_Mod_Callback_Count(runtime) == 2);
    orion2re::lua::s_lua_callback_info callback;
    REQUIRE(orion2re::lua::Get_Mod_Callback_Info(runtime, 0, &callback));
    CHECK(callback.kind == orion2re::lua::LUA_CALLBACK_RULE);
    CHECK(callback.extension_name == "population.growth");
    CHECK(callback.registration_ordinal == 0);

    lua_Integer bonus = 3;
    std::int64_t value = 5;
    REQUIRE(orion2re::lua::Dispatch_Mod_Rule(runtime,
        "population.growth",
        Build_Test_Rule_Context_,
        &bonus,
        INT16_MIN,
        INT16_MAX,
        &value,
        &result));
    CHECK(value == 16);
    CHECK(result.value_kind == orion2re::lua::LUA_VALUE_INTEGER);
    CHECK(result.integer_value == 16);

    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("Lua rule dispatch validates arguments and rejects reentry") {
    const char source[] =
        "rules.modify('population.growth', function(context, value)\n"
        "    return value + context.bonus\n"
        "end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = Create_(source, &result);
    REQUIRE(runtime != nullptr);

    lua_Integer bonus = 1;
    std::int64_t value = INT16_MAX + INT64_C(1);
    result.diagnostic = "stale";
    CHECK_FALSE(orion2re::lua::Dispatch_Mod_Rule(runtime,
        "population.growth",
        Build_Test_Rule_Context_,
        &bonus,
        INT16_MIN,
        INT16_MAX,
        &value,
        &result));
    CHECK(result.error == orion2re::lua::LUA_ERROR_RUNTIME);
    CHECK(result.diagnostic.find("outside the permitted range") !=
        std::string::npos);
    CHECK_FALSE(orion2re::lua::Rule_Modifier_Active());

    value = 1;
    s_reentrant_rule_context context;
    context.runtime = runtime;
    REQUIRE(orion2re::lua::Dispatch_Mod_Rule(runtime,
        "population.growth",
        Build_Reentrant_Rule_Context_,
        &context,
        INT16_MIN,
        INT16_MAX,
        &value,
        &result));
    CHECK(context.rejected);
    CHECK(context.diagnostic.find("would reenter package") !=
        std::string::npos);
    CHECK(value == 1);
    CHECK_FALSE(orion2re::lua::Rule_Modifier_Active());

    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("Lua rule modifiers reject invalid results and storage mutation") {
    struct s_rule_failure {
        const char* body;
        const char* diagnostic;
    };
    const s_rule_failure failures[] = {
        {"return nil", "must return an integer"},
        {"return 'bad'", "must return an integer"},
        {"return 40000", "outside the permitted range"},
        {"storage.changed = true; return value", "modified persistent storage"},
        {"storage = {}; return value", "replaced the engine-owned storage root"},
    };
    for (const s_rule_failure& failure : failures) {
        CAPTURE(failure.body);
        const std::string source =
            "rules.modify('population.growth', function(context, value) " +
            std::string(failure.body) + " end)";
        orion2re::lua::s_lua_result result;
        orion2re::lua::s_lua_mod_runtime* runtime =
            Create_(source.c_str(), &result);
        REQUIRE(runtime != nullptr);
        std::vector<uint8_t> storage_before;
        std::string storage_diagnostic;
        REQUIRE(orion2re::lua::Serialize_Mod_Storage(
            runtime, &storage_before, &storage_diagnostic));
        lua_Integer bonus = 0;
        std::int64_t value = 5;
        CHECK_FALSE(orion2re::lua::Dispatch_Mod_Rule(runtime,
            "population.growth",
            Build_Test_Rule_Context_,
            &bonus,
            INT16_MIN,
            INT16_MAX,
            &value,
            &result));
        CHECK(result.diagnostic.find(failure.diagnostic) !=
            std::string::npos);
        CHECK_FALSE(orion2re::lua::Rule_Modifier_Active());
        std::vector<uint8_t> storage_after;
        REQUIRE(orion2re::lua::Serialize_Mod_Storage(
            runtime, &storage_after, &storage_diagnostic));
        CHECK(storage_after == storage_before);
        orion2re::lua::Destroy_Mod_Runtime(runtime);
    }
}

TEST_CASE("population growth rules use package order and immutable context") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"high", "low"});
    Write_Package_File_(&files,
        "low",
        "main.lua",
        "rules.modify('population.growth', function(context, value)\n"
        "  if context.climate ~= defines.climate.gaia then\n"
        "    error('wrong population climate define')\n"
        "  end\n"
        "  if pcall(function() context.climate = 0 end) then\n"
        "    error('population context was mutable')\n"
        "  end\n"
        "  if pcall(function() defines.climate.gaia = 0 end) or\n"
        "      pcall(function() defines.climate = {} end) then\n"
        "    error('Lua defines were mutable')\n"
        "  end\n"
        "  return value + context.race_population\n"
        "end)\n");
    Write_Package_File_(&files,
        "high",
        "main.lua",
        "rules.modify('population.growth', function(context, value)\n"
        "  return value * 2\n"
        "end)\n");
    std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("high"),
        Runtime_Package_("low"),
    };
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    REQUIRE(registry.runtime_packages.size() == 2);
    for (const auto& package : registry.runtime_packages) {
        REQUIRE(package.callbacks.size() == 1);
        CHECK(package.callbacks[0].kind ==
            orion2re::lua::LUA_CALLBACK_RULE);
        CHECK(package.callbacks[0].extension_name ==
            "population.growth");
        CHECK(package.callbacks[0].schema == 1);
    }
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);
    const auto prior_game_type = MOX::game_type;
    MOX::game_type = 0;
    orion2re::lua::game::s_population_growth_context context;
    context.colony_id = 1;
    context.planet_id = 2;
    context.owner_id = 0;
    context.race_id = 0;
    context.race_population = 3;
    context.total_population = 4;
    context.population_limit = 10;
    context.climate = PLANET_CLIMATE_GAIA;
    CHECK(orion2re::lua::game::Modify_Population_Growth(
              &context, 5) == 16);
    context.total_population = 11;
    context.population_limit = 10;
    CHECK(orion2re::lua::game::Modify_Population_Growth(
              &context, 5) == 16);

    MOX::game_type = 2;
    CHECK(orion2re::lua::game::Modify_Population_Growth(
              &context, 5) == 5);
    {
        orion2re::lua::game::s_authoritative_population_growth_scope scope(
            true);
        CHECK(orion2re::lua::game::Modify_Population_Growth(
                  &context, 5) == 16);
    }
    CHECK(orion2re::lua::game::Modify_Population_Growth(
              &context, 5) == 5);
    MOX::game_type = 3;
    CHECK(orion2re::lua::game::Modify_Population_Growth(
              &context, 5) == 5);
    MOX::game_type = prior_game_type;
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua authority reconstruction restores storage and clears transient globals") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files,
        "main.lua",
        "local calls = 0\n"
        "events.on('game.initialized', function() storage.bonus = 7 end)\n"
        "rules.modify('population.growth', function(context, value)\n"
        "  calls = calls + 1\n"
        "  return value + calls + storage.bonus\n"
        "end)\n");
    const std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("example"),
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
    registry.packages_high_to_low_priority.push_back(
        {"example", "1.0.0"});
    orion2re::lua::Commit_Registry(&registry);
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);
    std::string diagnostic;
    REQUIRE(orion2re::lua::Dispatch_Active_Initialized_Event(&diagnostic));
    orion2re::lua::s_lua_save_snapshot snapshot;
    REQUIRE(orion2re::lua::Capture_Lua_Save_Snapshot(
        &snapshot, &diagnostic));

    auto* runtime = orion2re::lua::Find_Mod_Runtime(
        const_cast<orion2re::lua::s_lua_mod_runtime_set*>(
            orion2re::lua::Get_Active_Mod_Runtimes()),
        "example");
    REQUIRE(runtime != nullptr);
    lua_Integer bonus = 0;
    std::int64_t value = 5;
    orion2re::lua::s_lua_result result;
    REQUIRE(orion2re::lua::Dispatch_Mod_Rule(runtime,
        "population.growth",
        Build_Test_Rule_Context_,
        &bonus,
        INT16_MIN,
        INT16_MAX,
        &value,
        &result));
    CHECK(value == 13);
    value = 5;
    REQUIRE(orion2re::lua::Dispatch_Mod_Rule(runtime,
        "population.growth",
        Build_Test_Rule_Context_,
        &bonus,
        INT16_MIN,
        INT16_MAX,
        &value,
        &result));
    CHECK(value == 14);

    orion2re::lua::s_lua_registry reconstructed_registry;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &reconstructed_registry,
        &runtimes,
        &config_diagnostic));
    reconstructed_registry.packages_high_to_low_priority.push_back(
        {"example", "1.0.0"});
    REQUIRE(orion2re::lua::Restore_Lua_Save_Snapshot(&snapshot,
        &reconstructed_registry,
        runtimes,
        &diagnostic,
        false,
        true));
    runtime = orion2re::lua::Find_Mod_Runtime(runtimes, "example");
    REQUIRE(runtime != nullptr);
    value = 5;
    REQUIRE(orion2re::lua::Dispatch_Mod_Rule(runtime,
        "population.growth",
        Build_Test_Rule_Context_,
        &bonus,
        INT16_MIN,
        INT16_MAX,
        &value,
        &result));
    CHECK(value == 13);

    orion2re::lua::Destroy_Mod_Runtime_Set(runtimes);
    orion2re::lua::Shutdown_Lua();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("combat beam damage rules use package order and immutable context") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"high", "low"});
    Write_Package_File_(&files,
        "low",
        "main.lua",
        "rules.modify('combat.beam_damage', function(context, value)\n"
        "  if context.attacker_player_id ~= 2 or\n"
        "      context.target_player_id ~= 1 or context.weapon_id ~= 7 or\n"
        "      context.beam_ordinal ~= 1 or context.beam_count ~= 3 then\n"
        "    error('wrong combat beam damage context')\n"
        "  end\n"
        "  if pcall(function() context.weapon_id = 0 end) then\n"
        "    error('combat beam damage context was mutable')\n"
        "  end\n"
        "  return value + context.beam_count\n"
        "end)\n");
    Write_Package_File_(&files,
        "high",
        "main.lua",
        "rules.modify('combat.beam_damage', function(context, value)\n"
        "  return value * 2\n"
        "end)\n");
    std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("high"),
        Runtime_Package_("low"),
    };
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    REQUIRE(registry.runtime_packages.size() == 2);
    for (const auto& package : registry.runtime_packages) {
        REQUIRE(package.callbacks.size() == 1);
        CHECK(package.callbacks[0].kind ==
            orion2re::lua::LUA_CALLBACK_RULE);
        CHECK(package.callbacks[0].extension_name ==
            "combat.beam_damage");
        CHECK(package.callbacks[0].schema == 1);
    }
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);
    const auto prior_game_type = MOX::game_type;
    MOX::game_type = 0;
    orion2re::lua::game::s_combat_beam_damage_context context;
    context.attacker_player_id = 2;
    context.target_player_id = 1;
    context.weapon_id = 7;
    context.beam_ordinal = 1;
    context.beam_count = 3;
    CHECK(orion2re::lua::game::Modify_Combat_Beam_Damage(
              &context, 5) == 16);

    MOX::game_type = 2;
    CHECK(orion2re::lua::game::Modify_Combat_Beam_Damage(
              &context, 5) == 5);
    MOX::game_type = 3;
    CHECK(orion2re::lua::game::Modify_Combat_Beam_Damage(
              &context, 5) == 5);
    MOX::game_type = prior_game_type;
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("planet generated event implements the legacy deposit behavior") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"high", "low"});
    Write_Package_File_(&files,
        "low",
        "main.lua",
        Load_Lua_Source_("v150i/content/deposits.lua"));
    Write_Package_File_(&files,
        "high",
        "main.lua",
        "events.on('planet.generated', function(event)\n"
        "  local planet = game.planets:get(event.planet_id)\n"
        "  if planet.minerals ~= defines.minerals.abundant or\n"
        "      planet.gravity ~= defines.gravity.normal then\n"
        "    error('generated planet package order was wrong')\n"
        "  end\n"
        "end)\n");
    std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("high"),
        Runtime_Package_("low"),
    };
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    REQUIRE(registry.runtime_packages.size() == 2);
    for (const auto& package : registry.runtime_packages) {
        REQUIRE(package.callbacks.size() == 1);
        CHECK(package.callbacks[0].kind ==
            orion2re::lua::LUA_CALLBACK_EVENT);
        CHECK(package.callbacks[0].extension_name ==
            "planet.generated");
    }
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const auto prior_game_type = MOX::game_type;
    MOX::game_type = 0;
    MOX::planet_count = 2;
    MOX::NUM_STARS = 2;
    MOX::planet[1].star_index = 1;
    MOX::planet[1].orbit = 3;
    MOX::planet[1].planet_type = PLANET_TYPE_PLANET;
    MOX::planet[1].size = PLANET_SIZE_SMALL;
    MOX::planet[1].gravity_class = PLANET_GRAVITY_LOW_G;
    MOX::planet[1].climate = PLANET_CLIMATE_BARREN;
    MOX::planet[1].mineral_class = MINERAL_RESOURCE_POOR;
    MOX::planet[1].planet_special = SYSTEM_SPECIAL_GOLD_DEPOSITS;
    MOX::star[1].planet_index[3] = 1;
    const orion2re::lua::game::s_planet_generated_event event = {1};
    orion2re::lua::game::Planet_Generated(&event);
    CHECK(MOX::planet[1].mineral_class == MINERAL_RESOURCE_ABUNDANT);
    CHECK(MOX::planet[1].gravity_class == PLANET_GRAVITY_NORMAL_G);

    MOX::game_type = 2;
    MOX::planet[1].mineral_class = MINERAL_RESOURCE_POOR;
    MOX::planet[1].gravity_class = PLANET_GRAVITY_LOW_G;
    orion2re::lua::game::Planet_Generated(&event);
    CHECK(MOX::planet[1].mineral_class == MINERAL_RESOURCE_ABUNDANT);
    MOX::game_type = 3;
    MOX::planet[1].mineral_class = MINERAL_RESOURCE_POOR;
    MOX::planet[1].gravity_class = PLANET_GRAVITY_LOW_G;
    orion2re::lua::game::Planet_Generated(&event);
    CHECK(MOX::planet[1].gravity_class == PLANET_GRAVITY_LOW_G);
    MOX::game_type = prior_game_type;
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua save snapshot restores storage and load-derived transient state atomically") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files,
        "main.lua",
        "prototypes.race_traits:add { id = 'scavenger', cost = 5 }\n"
        "local loaded = 0\n"
        "rules.modify('population.growth', function(context, value) return value end)\n"
        "events.on('game.initialized', function() storage.count = 41 end)\n"
        "events.on('game.loaded', function() loaded = storage.count end)\n"
        "events.on('ship.destroyed', function(event)\n"
        "    event.report(loaded, storage.count)\n"
        "end)\n");

    std::vector<config::s_mod_manifest> packages = {
        Runtime_Package_("example"),
    };
    orion2re::lua::s_lua_mod_limits limits;
    config::s_cfg_diagnostic config_diagnostic;
    orion2re::lua::s_lua_registry active_registry;
    orion2re::lua::s_lua_mod_runtime_set* active_candidate = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &active_registry, &active_candidate, &config_diagnostic));
    active_registry.synchronized_config_hash = "config";
    active_registry.fingerprint = "sources";
    orion2re::lua::Commit_Registry(&active_registry);
    orion2re::lua::Commit_Mod_Runtimes(&active_candidate);

    auto* active_runtime = orion2re::lua::Find_Mod_Runtime(
        const_cast<orion2re::lua::s_lua_mod_runtime_set*>(
            orion2re::lua::Get_Active_Mod_Runtimes()),
        "example");
    REQUIRE(active_runtime != nullptr);
    orion2re::lua::s_lua_result result;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        active_runtime, "game.initialized", nullptr, nullptr, &result));
    REQUIRE(orion2re::lua::Set_Player_Race_Trait_Assignment(
        orion2re::lua::Get_Active_Game_Traits(),
        1,
        "scavenger",
        true));
    CHECK_FALSE(orion2re::lua::Set_Player_Race_Trait_Assignment(
        orion2re::lua::Get_Active_Game_Traits(),
        MAX_PLAYERS,
        "scavenger",
        true));

    orion2re::lua::s_lua_save_snapshot captured;
    std::string diagnostic;
    orion2re::lua::game::Restore_Legacy_Ship_Generations();
    REQUIRE(orion2re::lua::Capture_Lua_Save_Snapshot(
        &captured, &diagnostic));
    REQUIRE(captured.trait_assignments.size() == 1);
    CHECK(captured.trait_assignments[0].player_id == 1);
    CHECK(captured.trait_assignments[0].trait_id == "scavenger");
    REQUIRE(captured.ship_generation_count == 2);
    CHECK(captured.ship_generations[0] == 1);
    CHECK(captured.ship_generations[1] == 1);
    const uint32_t encoded_size =
        orion2re::lua::Lua_Save_Snapshot_Size(&captured);
    REQUIRE(encoded_size != 0);
    std::vector<uint8_t> encoded(encoded_size);
    SERIAL::Writer_ writer(encoded.data(), encoded.size(), "Lua save overflow");
    orion2re::lua::Write_Lua_Save_Snapshot(&writer, &captured);
    CHECK(writer.Length() == encoded_size);

    SERIAL::Reader_ reader(encoded.data(), encoded.size(), "Lua save underflow");
    orion2re::lua::s_lua_save_snapshot decoded;
    REQUIRE(orion2re::lua::Read_Lua_Save_Snapshot(
        &reader, &decoded, &diagnostic));
    CHECK(reader.Remaining() == 0);
    REQUIRE(decoded.registry.runtime_packages.size() == 1);
    REQUIRE(decoded.registry.runtime_packages[0].callbacks.size() == 4);
    CHECK(decoded.registry.runtime_packages[0].callbacks[0].kind ==
        orion2re::lua::LUA_CALLBACK_RULE);
    CHECK(decoded.registry.runtime_packages[0].callbacks[0].extension_name ==
        "population.growth");
    REQUIRE(decoded.ship_generation_count == 2);
    CHECK(decoded.ship_generations[0] == 1);
    CHECK(decoded.ship_generations[1] == 1);

    orion2re::lua::s_lua_save_snapshot missing_storage = decoded;
    missing_storage.storage.clear();
    orion2re::lua::s_lua_registry missing_storage_registry;
    orion2re::lua::s_lua_mod_runtime_set* missing_storage_runtime = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &missing_storage_registry,
        &missing_storage_runtime,
        &config_diagnostic));
    missing_storage_registry.synchronized_config_hash = "config";
    missing_storage_registry.fingerprint = "sources";
    CHECK_FALSE(orion2re::lua::Restore_Lua_Save_Snapshot(&missing_storage,
        &missing_storage_registry,
        missing_storage_runtime,
        &diagnostic));
    CHECK(diagnostic ==
        "Lua storage package count differs from active runtime");
    orion2re::lua::Destroy_Mod_Runtime_Set(missing_storage_runtime);

    orion2re::lua::s_lua_registry restored_registry;
    orion2re::lua::s_lua_mod_runtime_set* restored = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &restored_registry, &restored, &config_diagnostic));
    restored_registry.synchronized_config_hash = "config";
    restored_registry.fingerprint = "sources";
    REQUIRE(orion2re::lua::Restore_Lua_Save_Snapshot(
        &decoded, &restored_registry, restored, &diagnostic));
    CHECK(orion2re::lua::Has_Player_Race_Trait_Assignment(
        orion2re::lua::Get_Game_Traits(restored),
        1,
        "scavenger"));

    s_event_observation observation;
    auto* restored_runtime = orion2re::lua::Find_Mod_Runtime(restored, "example");
    REQUIRE(restored_runtime != nullptr);
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        restored_runtime, "ship.destroyed", Build_Event_, &observation, &result));
    CHECK(observation.transient == 41);
    CHECK(observation.stored == 41);

    const orion2re::lua::s_lua_mod_runtime_set* committed_before_failure =
        orion2re::lua::Get_Active_Mod_Runtimes();
    decoded.trait_assignments[0].trait_id = "missing";
    orion2re::lua::s_lua_registry rejected_trait_registry;
    orion2re::lua::s_lua_mod_runtime_set* rejected_trait = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages,
        &limits,
        &rejected_trait_registry,
        &rejected_trait,
        &config_diagnostic));
    rejected_trait_registry.synchronized_config_hash = "config";
    rejected_trait_registry.fingerprint = "sources";
    CHECK_FALSE(orion2re::lua::Restore_Lua_Save_Snapshot(
        &decoded,
        &rejected_trait_registry,
        rejected_trait,
        &diagnostic));
    CHECK(diagnostic.find("unknown trait 'missing'") != std::string::npos);
    CHECK(orion2re::lua::Get_Active_Mod_Runtimes() ==
        committed_before_failure);
    orion2re::lua::Destroy_Mod_Runtime_Set(rejected_trait);

    decoded.trait_assignments[0].trait_id = "scavenger";
    decoded.registry.runtime_packages[0].sources[0].source_hash = "changed";
    orion2re::lua::s_lua_registry rejected_registry;
    orion2re::lua::s_lua_mod_runtime_set* rejected = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &rejected_registry, &rejected, &config_diagnostic));
    rejected_registry.synchronized_config_hash = "config";
    rejected_registry.fingerprint = "changed sources";
    CHECK_FALSE(orion2re::lua::Restore_Lua_Save_Snapshot(
        &decoded, &rejected_registry, rejected, &diagnostic));
    CHECK(diagnostic.find("loaded Lua file content differs") !=
        std::string::npos);
    orion2re::lua::allow_source_hash_mismatch = true;
    CHECK(orion2re::lua::Restore_Lua_Save_Snapshot(
        &decoded, &rejected_registry, rejected, &diagnostic));
    orion2re::lua::allow_source_hash_mismatch = false;
    CHECK(diagnostic.empty());
    rejected_registry.synchronized_config_hash = "changed config";
    orion2re::lua::allow_source_hash_mismatch = true;
    CHECK(orion2re::lua::Restore_Lua_Save_Snapshot(
        &decoded, &rejected_registry, rejected, &diagnostic));
    orion2re::lua::allow_source_hash_mismatch = false;
    CHECK(diagnostic.empty());
    decoded.registry.runtime_packages[0].sources[0].source_hash =
        rejected_registry.runtime_packages[0].sources[0].source_hash;
    CHECK_FALSE(orion2re::lua::Restore_Lua_Save_Snapshot(
        &decoded,
        &rejected_registry,
        rejected,
        &diagnostic,
        false,
        true));
    CHECK(diagnostic.find("synchronized gameplay/technology") !=
        std::string::npos);
    CHECK(orion2re::lua::Get_Active_Mod_Runtimes() ==
        committed_before_failure);

    orion2re::lua::Destroy_Mod_Runtime_Set(rejected);
    orion2re::lua::Destroy_Mod_Runtime_Set(restored);
    orion2re::lua::Shutdown_Lua();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua mod runtime loads nested package modules once") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files, "content/value.lua", "return { amount = 6 }");
    Write_Module_(&files, "content/logic.lua", "local value = require('content.value')\n"
                                               "return { amount = value.amount + 1 }\n");
    const char source[] =
        "local first = require('content.logic')\n"
        "local second = require('content.logic')\n"
        "if first ~= second then error('module cache changed identity') end\n"
        "events.on('test.tick', function(event)\n"
        "    local cached = require('content.logic')\n"
        "    event.report(first.amount, cached.amount)\n"
        "end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = Create_(source, &result);
    REQUIRE(runtime != nullptr);

    CHECK(orion2re::lua::Get_Mod_Module_Count(runtime) == 2);
    orion2re::lua::s_lua_module_info logic;
    REQUIRE(orion2re::lua::Get_Mod_Module_Info(runtime, 0, &logic));
    CHECK(logic.name == "content.logic");
    CHECK(logic.path == "mods/example/content/logic.lua");
    CHECK_FALSE(logic.source_hash.empty());
    orion2re::lua::s_lua_module_info value;
    REQUIRE(orion2re::lua::Get_Mod_Module_Info(runtime, 1, &value));
    CHECK(value.name == "content.value");

    s_event_observation observation;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.tick", Build_Event_, &observation, &result));
    CHECK(observation.transient == 7);
    CHECK(observation.stored == 7);

    orion2re::lua::Destroy_Mod_Runtime(runtime);
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua mod exports marshal values across isolated states") {
    s_module_files files;
    Initialize_Package_Files_(&files, {"consumer", "provider"});
    Write_Package_File_(&files, "provider", "main.lua", "local calls = 0\n"
                                                        "mod.export('math_api', 2, {\n"
                                                        "  calculate = function(input)\n"
                                                        "    calls = calls + 1\n"
                                                        "    return { sum = input.left + input.right, calls = calls }, true\n"
                                                        "  end,\n"
                                                        "})\n");
    Write_Package_File_(&files, "consumer", "main.lua", "local math_api = mod.import('provider', 'math_api')\n"
                                                        "events.on('game.initialized', function(event)\n"
                                                        "  local result, ok = math_api.calculate({\n"
                                                        "    left = event.amount, right = 3, flags = { false, true },\n"
                                                        "  })\n"
                                                        "  if not ok then error('export result missing') end\n"
                                                        "  event.report(result.sum, result.calls)\n"
                                                        "end)\n");

    config::s_mod_manifest provider = Runtime_Package_("provider");
    config::s_mod_manifest consumer = Runtime_Package_("consumer");
    consumer.dependencies.push_back({"provider", ">=1.0.0"});
    std::vector<config::s_mod_manifest> packages = {consumer, provider};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtime_set = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtime_set, &diagnostic));

    const orion2re::lua::s_lua_mod_runtime* provider_runtime =
        orion2re::lua::Find_Mod_Runtime(runtime_set, "provider");
    REQUIRE(provider_runtime != nullptr);
    REQUIRE(orion2re::lua::Get_Mod_Export_Count(provider_runtime) == 1);
    orion2re::lua::s_lua_export_info exported;
    REQUIRE(orion2re::lua::Get_Mod_Export_Info(
        provider_runtime, 0, &exported));
    CHECK(exported.name == "math_api");
    CHECK(exported.version == 2);
    CHECK(exported.methods == std::vector<std::string>{"calculate"});
    CHECK(exported.registration_source == "mods/provider/main.lua");
    REQUIRE(registry.runtime_packages.size() == 2);

    auto* consumer_runtime =
        orion2re::lua::Find_Mod_Runtime(runtime_set, "consumer");
    REQUIRE(consumer_runtime != nullptr);
    s_event_observation observation;
    observation.amount = 4;
    orion2re::lua::s_lua_result result;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(consumer_runtime,
        "game.initialized",
        Build_Event_,
        &observation,
        &result));
    CHECK(observation.transient == 7);
    CHECK(observation.stored == 1);

    observation.amount = 9;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(consumer_runtime,
        "game.initialized",
        Build_Event_,
        &observation,
        &result));
    CHECK(observation.transient == 12);
    CHECK(observation.stored == 2);

    orion2re::lua::Commit_Mod_Runtimes(&runtime_set);
    REQUIRE(runtime_set == nullptr);
    consumer_runtime = (orion2re::lua::s_lua_mod_runtime*)
        orion2re::lua::Find_Mod_Runtime(
            orion2re::lua::Get_Active_Mod_Runtimes(), "consumer");
    REQUIRE(consumer_runtime != nullptr);
    observation.amount = 2;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(consumer_runtime,
        "game.initialized",
        Build_Event_,
        &observation,
        &result));
    CHECK(observation.transient == 5);
    CHECK(observation.stored == 3);
    orion2re::lua::Reset_Mod_Runtimes();
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua mod imports require an active declared dependency") {
    struct s_import_failure {
        bool declared_dependency;
        const char* source;
        const char* message;
    };
    const s_import_failure failures[] = {
        {false, "mod.import('provider', 'api')", "is not a declared dependency"},
        {true, "mod.import('provider', 'missing')", "does not export 'missing'"},
    };
    for (const s_import_failure& failure : failures) {
        CAPTURE(failure.source);
        s_module_files files;
        Initialize_Package_Files_(&files, {"consumer", "provider"});
        Write_Package_File_(&files, "provider", "main.lua", "mod.export('api', 1, { run = function() end })");
        Write_Package_File_(&files, "consumer", "main.lua", failure.source);
        config::s_mod_manifest provider = Runtime_Package_("provider");
        config::s_mod_manifest consumer = Runtime_Package_("consumer");
        if (failure.declared_dependency) {
            consumer.dependencies.push_back({"provider", "1.0.0"});
        }
        std::vector<config::s_mod_manifest> packages = {consumer, provider};
        orion2re::lua::s_lua_mod_limits limits;
        orion2re::lua::s_lua_registry registry;
        orion2re::lua::s_lua_mod_runtime_set* runtime_set = nullptr;
        config::s_cfg_diagnostic diagnostic;
        CHECK_FALSE(orion2re::lua::Stage_Mod_Runtimes(&packages, &limits, &registry, &runtime_set, &diagnostic));
        CHECK(diagnostic.message.find(failure.message) != std::string::npos);
        orion2re::lua::Destroy_Mod_Runtime_Set(runtime_set);
        Shutdown_Module_Files_(&files);
    }
}

TEST_CASE("Lua mod export registration validates interfaces") {
    struct s_export_failure {
        const char* source;
        const char* message;
    };
    const s_export_failure failures[] = {
        {"mod.export('bad-name', 1, { run = function() end })",
            "invalid cross-mod export name"},
        {"mod.export('api', 0, { run = function() end })",
            "export version must be between"},
        {"mod.export('api', 1, {})",
            "must contain at least one method"},
        {"mod.export('api', 1, { value = 1 })",
            "entries must be string-named functions"},
        {"mod.export('api', 1, { run = function() end })\n"
         "mod.export('api', 2, { run = function() end })",
            "duplicate cross-mod export"},
    };
    for (const s_export_failure& failure : failures) {
        CAPTURE(failure.source);
        orion2re::lua::s_lua_result result;
        CHECK(Create_(failure.source, &result) == nullptr);
        CHECK(result.diagnostic.find(failure.message) != std::string::npos);
    }
}

TEST_CASE("Lua mod exports reject unsupported values and provider failures") {
    struct s_call_failure {
        const char* provider_body;
        const char* consumer_argument;
        const char* message;
    };
    const s_call_failure failures[] = {
        {"return function() end", "1", "support only nil, Boolean"},
        {"error('provider failed')", "1", "provider failed"},
        {"return 1", "function() end", "support only nil, Boolean"},
        {"local result = {}; result.self = result; return result", "1", "must not contain aliases or cycles"},
    };
    for (const s_call_failure& failure : failures) {
        CAPTURE(failure.message);
        s_module_files files;
        Initialize_Package_Files_(&files, {"consumer", "provider"});
        Write_Package_File_(&files, "provider", "main.lua", "mod.export('api', 1, { run = function(value) " + std::string(failure.provider_body) + " end })");
        Write_Package_File_(&files, "consumer", "main.lua", "local api = mod.import('provider', 'api')\n"
                                                            "events.on('game.initialized', function()\n"
                                                            "  api.run(" +
                std::string(failure.consumer_argument) + ")\n"
                                                         "end)\n");
        config::s_mod_manifest provider = Runtime_Package_("provider");
        config::s_mod_manifest consumer = Runtime_Package_("consumer");
        consumer.dependencies.push_back({"provider", "1.0.0"});
        std::vector<config::s_mod_manifest> packages = {consumer, provider};
        orion2re::lua::s_lua_mod_limits limits;
        orion2re::lua::s_lua_registry registry;
        orion2re::lua::s_lua_mod_runtime_set* runtime_set = nullptr;
        config::s_cfg_diagnostic diagnostic;
        REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages, &limits, &registry, &runtime_set, &diagnostic));
        auto* runtime =
            orion2re::lua::Find_Mod_Runtime(runtime_set, "consumer");
        REQUIRE(runtime != nullptr);
        orion2re::lua::s_lua_result result;
        CHECK_FALSE(orion2re::lua::Dispatch_Mod_Event(runtime,
            "game.initialized",
            nullptr,
            nullptr,
            &result));
        CHECK(result.diagnostic.find(failure.message) != std::string::npos);
        orion2re::lua::Destroy_Mod_Runtime_Set(runtime_set);
        Shutdown_Module_Files_(&files);
    }
}

TEST_CASE("Lua mod callbacks cannot expand the sealed module set") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files, "late.lua", "return { amount = 1 }");
    const char source[] =
        "events.on('test.tick', function() require('late') end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = Create_(source, &result);
    REQUIRE(runtime != nullptr);

    s_event_observation observation;
    CHECK_FALSE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.tick", Build_Event_, &observation, &result));
    CHECK(result.diagnostic.find("was not loaded during main.lua setup") !=
        std::string::npos);

    orion2re::lua::Destroy_Mod_Runtime(runtime);
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua mod activation includes reached modules in runtime identity") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files, "main.lua", "require('content.identity')");
    Write_Module_(&files, "content/identity.lua", "return { value = 1 }");

    config::s_mod_manifest package;
    package.id = "example";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtime_set = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtime_set, &diagnostic));

    REQUIRE(registry.runtime_packages.size() == 1);
    const auto& sources = registry.runtime_packages[0].sources;
    REQUIRE(sources.size() == 2);
    CHECK(sources[0].path == "mods/example/main.lua");
    CHECK(sources[1].path == "mods/example/content/identity.lua");
    CHECK_FALSE(sources[1].source_hash.empty());

    orion2re::lua::Destroy_Mod_Runtime_Set(runtime_set);
    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua mod runtime rejects invalid missing and cyclic modules") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files, "cycle/first.lua", "return require('cycle.second')");
    Write_Module_(&files, "cycle/second.lua", "return require('cycle.first')");

    struct s_failure {
        const char* source;
        const char* message;
    };
    const s_failure failures[] = {
        {"require('../other.private')", "invalid package Lua module name"},
        {"require('Uppercase')", "invalid package Lua module name"},
        {"require('missing.module')", "package Lua module not found"},
        {"require('cycle.first')", "package Lua module cycle"},
        {"pcall(require, 'missing.module')", "package Lua module not found"},
        {"pcall(require, 123)", "module name must be a string"},
    };
    for (const s_failure& failure : failures) {
        CAPTURE(failure.source);
        orion2re::lua::s_lua_result result;
        CHECK(Create_(failure.source, &result) == nullptr);
        CHECK(result.diagnostic.find(failure.message) != std::string::npos);
    }

    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua mod modules enforce module and package limits") {
    s_module_files files;
    Initialize_Module_Files_(&files);
    Write_Module_(&files, "first.lua", "return 1");
    Write_Module_(&files, "second.lua", "return 2");

    orion2re::lua::s_lua_mod_limits module_limits;
    module_limits.modules = 1;
    orion2re::lua::s_lua_result module_result;
    CHECK(Create_("require('first')\nrequire('second')", &module_result, &module_limits) == nullptr);
    CHECK(module_result.diagnostic.find("module limit exceeded") !=
        std::string::npos);

    const char source[] = "require('first')";
    orion2re::lua::s_lua_mod_limits byte_limits;
    byte_limits.package_source_bytes = sizeof(source) - 1;
    orion2re::lua::s_lua_result byte_result;
    CHECK(Create_(source, &byte_result, &byte_limits) == nullptr);
    CHECK(byte_result.diagnostic.find("package source byte limit") !=
        std::string::npos);

    Shutdown_Module_Files_(&files);
}

TEST_CASE("Lua mod runtimes isolate globals and storage") {
    const char source[] =
        "counter = 0\n"
        "events.on('test.tick', function(event)\n"
        "    counter = counter + 1\n"
        "    storage.total = (storage.total or 0) + event.amount\n"
        "    event.report(counter, storage.total)\n"
        "end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* first = Create_(source, &result);
    REQUIRE(first != nullptr);
    orion2re::lua::s_lua_mod_runtime* second = Create_(source, &result);
    REQUIRE(second != nullptr);

    s_event_observation first_observation;
    first_observation.amount = 3;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        first, "test.tick", Build_Event_, &first_observation, &result));
    first_observation.amount = 5;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        first, "test.tick", Build_Event_, &first_observation, &result));

    s_event_observation second_observation;
    second_observation.amount = 9;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        second, "test.tick", Build_Event_, &second_observation, &result));

    CHECK(first_observation.transient == 2);
    CHECK(first_observation.stored == 8);
    CHECK(second_observation.transient == 1);
    CHECK(second_observation.stored == 9);

    orion2re::lua::Destroy_Mod_Runtime(first);
    orion2re::lua::Destroy_Mod_Runtime(second);
}

TEST_CASE("Lua mod setup rejects unknown events and callback over-registration") {
    orion2re::lua::s_lua_result unknown;
    CHECK(Create_("events.on('missing.event', function() end)", &unknown) ==
        nullptr);
    CHECK(unknown.error == orion2re::lua::LUA_ERROR_RUNTIME);
    CHECK(unknown.diagnostic.find("unknown event 'missing.event'") !=
        std::string::npos);

    orion2re::lua::s_lua_mod_limits limits;
    limits.callbacks = 1;
    orion2re::lua::s_lua_result excessive;
    CHECK(Create_(
              "events.on('test.tick', function() end)\n"
              "events.on('test.tick', function() end)\n",
              &excessive,
              &limits) == nullptr);
    CHECK(excessive.error == orion2re::lua::LUA_ERROR_RUNTIME);
    CHECK(excessive.diagnostic.find("callback registration limit") !=
        std::string::npos);
}

TEST_CASE("Lua mod runtime validates package identity and source accounting") {
    const orion2re::lua::s_lua_event_descriptor events[] = {
        {"test.tick", 1},
    };
    const orion2re::lua::s_lua_mod_limits limits;
    const char source[] = "events.on('test.tick', function() end)";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = nullptr;

    CHECK_FALSE(orion2re::lua::Create_Mod_Runtime(
        "example/..", "1.0.0", source, sizeof(source) - 1, "mods/example/main.lua", sizeof(source) - 1, events, 1, nullptr, 0, &limits, &runtime, &result));
    CHECK(result.diagnostic.find("invalid Lua package ID") !=
        std::string::npos);

    CHECK_FALSE(orion2re::lua::Create_Mod_Runtime(
        "example", "1.0.0", source, sizeof(source) - 1, "mods/example/main.lua", 0, events, 1, nullptr, 0, &limits, &runtime, &result));
    CHECK(result.diagnostic.find("excludes main.lua") != std::string::npos);
}

TEST_CASE("Lua mod callbacks cannot replace the storage root") {
    const char source[] =
        "events.on('test.tick', function(event)\n"
        "    storage = { total = event.amount }\n"
        "end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = Create_(source, &result);
    REQUIRE(runtime != nullptr);

    s_event_observation observation;
    observation.amount = 4;
    CHECK_FALSE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.tick", Build_Event_, &observation, &result));
    CHECK(result.diagnostic.find("replaced the engine-owned storage root") !=
        std::string::npos);

    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("Lua mod runtime restores storage after a failing callback") {
    const char source[] =
        "events.on('test.tick', function()\n"
        "    storage = {}\n"
        "    error('failed after replacement')\n"
        "end)\n";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = Create_(source, &result);
    REQUIRE(runtime != nullptr);

    s_event_observation observation;
    CHECK_FALSE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.tick", Build_Event_, &observation, &result));
    CHECK(result.diagnostic.find("failed after replacement") !=
        std::string::npos);
    CHECK_FALSE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.tick", Build_Event_, &observation, &result));
    CHECK(result.diagnostic.find("failed after replacement") !=
        std::string::npos);

    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("Lua mod runtime owns event descriptor names") {
    std::string event_name = "test.tick";
    const orion2re::lua::s_lua_event_descriptor events[] = {
        {event_name.c_str(), 1},
    };
    const orion2re::lua::s_lua_mod_limits limits;
    const char source[] = "events.on('test.tick', function() end)";
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = nullptr;
    REQUIRE(orion2re::lua::Create_Mod_Runtime(
        "example", "1.0.0", source, sizeof(source) - 1, "mods/example/main.lua", sizeof(source) - 1, events, 1, nullptr, 0, &limits, &runtime, &result));
    event_name = "changed.event";

    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.tick", nullptr, nullptr, &result));
    CHECK_FALSE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "changed.event", nullptr, nullptr, &result));

    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("Lua mod setup handles allocator exhaustion") {
    const char source[] =
        "events.on('test.tick', function() end)\n";
    for (size_t bytes = 1024; bytes <= 64 * 1024; bytes += 1024) {
        CAPTURE(bytes);
        orion2re::lua::s_lua_mod_limits limits;
        limits.runtime.allocator_bytes = bytes;
        orion2re::lua::s_lua_result result;
        orion2re::lua::s_lua_mod_runtime* runtime =
            Create_(source, &result, &limits);
        if (runtime == nullptr) {
            CHECK(result.error == orion2re::lua::LUA_ERROR_MEMORY_LIMIT);
        }
        orion2re::lua::Destroy_Mod_Runtime(runtime);
    }
}

TEST_CASE("Lua mod callbacks receive a fresh instruction budget") {
    const char source[] =
        "events.on('test.tick', function(event)\n"
        "    local sum = 0\n"
        "    for i = 1, 100 do sum = sum + i end\n"
        "    event.report(sum, sum)\n"
        "end)\n";
    orion2re::lua::s_lua_mod_limits limits;
    limits.runtime.instructions = 1000;
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime =
        Create_(source, &result, &limits);
    REQUIRE(runtime != nullptr);

    s_event_observation observation;
    for (int invocation = 0; invocation < 3; ++invocation) {
        REQUIRE(orion2re::lua::Dispatch_Mod_Event(
            runtime, "test.tick", Build_Event_, &observation, &result));
    }
    CHECK(observation.reports == 3);

    orion2re::lua::Destroy_Mod_Runtime(runtime);
}