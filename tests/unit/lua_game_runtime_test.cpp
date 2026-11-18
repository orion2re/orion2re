#include "../fixtures/planet_state.h"
#include "lua/lua_game.h"

#include <doctest/doctest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "config/generated/config_enums.h"
#include "config/mod_manifest.h"
#include "game/raceopt.h"
#include "game/random.h"
#include "game/serialization.h"
#include "game/sha256.h"
#include "game/vfs.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_game_internal.h"
#include "lua/lua_game_ship_identity.h"
#include "lua/lua_game_traits.h"
#include "lua/lua_registry.h"
#include "lua/lua_save.h"

extern "C" {
#include <lua.h>
}

namespace TEST_LUA_GAME {
    void Reset_();
}

namespace {

    namespace fs = std::filesystem;

    struct s_package_files {
        fs::path root;
    };

    void Write_File_(const s_package_files& files,
        const fs::path& relative_path,
        const std::string& source) {
        const fs::path path =
            files.root / "data" / "mods" / "scavenger" / relative_path;
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(source.data(), static_cast<std::streamsize>(source.size()));
        DOCTEST_REQUIRE(file.good());
    }

    void Initialize_Files_(s_package_files* files) {
        static unsigned int next_id;
        files->root = fs::temp_directory_path() /
            ("orion2re-lua-game-" + std::to_string(next_id++));
        fs::create_directories(
            files->root / "data" / "mods" / "scavenger");
        const std::string root = files->root.string();
        DOCTEST_REQUIRE(vfs::Init(root.c_str()) == 1);
        s_vfs_mount_transaction* transaction =
            vfs::Begin_Mount_Transaction();
        DOCTEST_REQUIRE(transaction != nullptr);
        std::string diagnostic;
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(
                            transaction, "scavenger", &diagnostic) == 1);
        DOCTEST_REQUIRE(vfs::Order_Package_Mounts(
                            transaction, {"scavenger"}, &diagnostic) == 1);
    }

    void Shutdown_Files_(s_package_files* files) {
        orion2re::lua::game::Reset_Semantic_State();
        orion2re::lua::Reset_Mod_Runtimes();
        vfs::Shutdown();
        std::error_code error;
        fs::remove_all(files->root, error);
    }

    int Build_Empty_Context_(lua_State* lua_state, void*) {
        lua_newtable(lua_state);
        return 1;
    }

    int Build_Integer_Context_(lua_State* lua_state, void* user_data) {
        lua_pushinteger(lua_state,
            *static_cast<const lua_Integer*>(user_data));
        return 1;
    }

    int Build_Player_Setup_Event_Context_(lua_State* lua_state,
        void* user_data) {
        const auto* event = static_cast<const
            orion2re::lua::game::s_player_setup_event*>(user_data);
        lua_newtable(lua_state);
        lua_pushinteger(lua_state, event->player_id);
        lua_setfield(lua_state, -2, "player_id");
        return 1;
    }

    void Copy_Lua_Migration_Fixture_(const s_package_files& files,
        const char* fixture_name) {
        const fs::path source =
            fs::path(LUA_MIGRATION_FIXTURE_DIR) / fixture_name;
        const fs::path destination =
            files.root / "data" / "mods" / "scavenger";
        DOCTEST_REQUIRE(fs::is_directory(source));
        std::error_code error;
        fs::copy(source,
            destination,
            fs::copy_options::recursive | fs::copy_options::overwrite_existing,
            error);
        DOCTEST_REQUIRE_FALSE(error);
        DOCTEST_REQUIRE(fs::is_regular_file(destination / "main.lua"));
    }

    void Copy_Lua_Source_Package_(const s_package_files& files,
        const char* package_id) {
        const fs::path source =
            fs::path(LUA_SOURCE_PACKAGE_DIR) / package_id;
        const fs::path destination =
            files.root / "data" / "mods" / "scavenger";
        DOCTEST_REQUIRE(fs::is_directory(source));
        std::error_code error;
        fs::copy(source,
            destination,
            fs::copy_options::recursive | fs::copy_options::overwrite_existing,
            error);
        DOCTEST_REQUIRE_FALSE(error);
        DOCTEST_REQUIRE(fs::is_regular_file(destination / "main.lua"));
    }

    void Write_Techtree_Source_Package_(const s_package_files& files) {
        Copy_Lua_Source_Package_(files, "v150i");
    }

    void Write_ICE_Source_Package_(const s_package_files& files) {
        Copy_Lua_Source_Package_(files, "ice");
    }

    void Write_VDC_Source_Package_(const s_package_files& files) {
        Copy_Lua_Source_Package_(files, "vdc-gm3");
    }

    void Write_RandomTech_Source_Package_(const s_package_files& files) {
        Copy_Lua_Source_Package_(files, "randtech");
    }

    struct s_technology_state_guard {
        s_tech_app_data applications[TECH_APP_COUNT];
        s_tech_field_data fields[TECH_FIELD_COUNT];
        std::int16_t starting_fields[6];

        s_technology_state_guard() {
            std::memcpy(applications,
                TECHDATA::technology_applications,
                sizeof(applications));
            std::memcpy(
                fields, TECHDATA::technology_fields, sizeof(fields));
            std::memcpy(starting_fields,
                TECHDATA::starting_tech_field_ids,
                sizeof(starting_fields));
        }

        ~s_technology_state_guard() {
            Restore_();
        }

        void Restore_() const {
            std::memcpy(TECHDATA::technology_applications,
                applications,
                sizeof(applications));
            std::memcpy(
                TECHDATA::technology_fields, fields, sizeof(fields));
            std::memcpy(TECHDATA::starting_tech_field_ids,
                starting_fields,
                sizeof(starting_fields));
        }
    };

    std::string Technology_Graph_Digest_() {
        std::string graph;
        for (const s_tech_app_data& application :
            TECHDATA::technology_applications) {
            graph += std::to_string(application.tech_field_id) + ",";
        }
        for (const s_tech_field_data& field : TECHDATA::technology_fields) {
            graph += std::to_string(field.prev_field_id) + "," +
                std::to_string(field.next_field_id) + ":";
            for (const int16_t application_id : field.tech) {
                graph += std::to_string(application_id) + ",";
            }
        }
        return orion2re::crypto::sha256_hex(graph);
    }

    void Write_VDC_Home_System_Interaction_Fixture_(
        const s_package_files& files) {
        Copy_Lua_Source_Package_(files, "vdc-gm3");
        Copy_Lua_Migration_Fixture_(files, "vdc");
        const fs::path source = fs::path(LUA_MIGRATION_FIXTURE_DIR) /
            "vdc" / "interaction_main.lua";
        const fs::path destination =
            files.root / "data" / "mods" / "scavenger" / "main.lua";
        std::error_code error;
        fs::copy_file(source,
            destination,
            fs::copy_options::overwrite_existing,
            error);
        DOCTEST_REQUIRE_FALSE(error);
    }

    void Reset_Technology_Player_(std::int16_t player_id) {
        memset(MOX::player[player_id].tech_applications,
            TECH_RESEARCH_STATUS_UNAVAILABLE,
            sizeof(MOX::player[player_id].tech_applications));
        MOX::player[player_id].objectives = PLAYER_OBJECTIVE_HUMAN;
        MOX::player[player_id].traits[TRAIT_CURRENT_GOVERNMENT] =
            GOVERNMENT_DEMOCRACY;
        MOX::player[player_id].traits[TRAIT_HEAVY_G_WORLD] = 0;
        MOX::player[player_id].traits[TRAIT_EATS_MINERALS] = 0;
        MOX::player[player_id].traits[TRAIT_UNCREATIVE] = 0;
        MOX::player[player_id].traits[TRAIT_ENVIRONMENT_IMMUNE] = 0;
    }

    void Block_VDC_Technology_Repairs_(std::int16_t player_id) {
        Reset_Technology_Player_(player_id);
        MOX::player[player_id].traits[TRAIT_UNCREATIVE] = 1;
        const std::int16_t blockers[] = {
            TECH_APP_ZORTRIUM_ARMOR,
            TECH_APP_PLANETARY_SUPERCOMPUTER,
            TECH_APP_POSITRONIC_COMPUTER,
            TECH_APP_EVOLUTIONARY_MUTATION,
            TECH_APP_ANTI_MATTER_BOMB,
            TECH_APP_CLASS_V_SHIELD,
            TECH_APP_HARD_SHIELDS,
            TECH_APP_DISRUPTER_CANNON,
            TECH_APP_HOLO_SIMULATOR,
            TECH_APP_HEIGHTENED_INTELLIGENCE,
            TECH_APP_CLONING_CENTER,
            TECH_APP_SPACEPORT,
            TECH_APP_POLLUTION_PROCESSOR,
            TECH_APP_SOIL_ENRICHMENT,
        };
        for (const std::int16_t blocker : blockers) {
            MOX::player[player_id].tech_applications[blocker] =
                TECH_RESEARCH_STATUS_AVAILABLE;
        }
    }

} // namespace

TEST_CASE("RandomTech package builds a valid global technology graph") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_technology_state_guard technology_guard;
    s_package_files files;
    Initialize_Files_(&files);
    Write_RandomTech_Source_Package_(files);

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic stage_diagnostic;
    CAPTURE(stage_diagnostic.message);
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &stage_diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    std::string diagnostic;
    game_random::Set_Random_Seed(0x12345678);
    CAPTURE(diagnostic);
    REQUIRE(TECHINIT::Finalize_Configured_New_Game_Technology(&diagnostic));
    CHECK(game_random::Get_Random_Seed() != 0x12345678);

    int changed_applications = 0;
    int full_fields = 0;
    int underfilled_fields = 0;
    for (std::int16_t application_id = 0;
         application_id < TECH_APP_COUNT;
         ++application_id) {
        const s_tech_app_data& application =
            TECHDATA::technology_applications[application_id];
        CHECK(application.tech_app_id == application_id);
        changed_applications +=
            application.tech_field_id !=
            technology_guard.applications[application_id].tech_field_id;
    }
    CHECK(changed_applications > 100);
    for (std::int16_t field_id = 0; field_id < TECH_FIELD_COUNT;
         ++field_id) {
        const s_tech_field_data& field =
            TECHDATA::technology_fields[field_id];
        CHECK(field.tech_field_id == field_id);
        CHECK(field.cost == technology_guard.fields[field_id].cost);
        int application_count = 0;
        for (const std::int16_t application_id : field.tech) {
            if (application_id == 0) continue;
            ++application_count;
            CHECK(TECHDATA::technology_applications[application_id]
                      .tech_field_id == field_id);
        }
        CHECK(application_count <= 4);
        if (field_id > TECH_FIELD_STARTING_TECH &&
            field_id < TECH_FIELD_XENON_TECHNOLOGY) {
            full_fields += application_count == 4;
            underfilled_fields += application_count < 4;
        }
    }
    CHECK(full_fields > 0);
    CHECK(underfilled_fields > 0);
    const std::int16_t preserved_links[][2] = {
        {TECH_FIELD_ENGINEERING, TECH_FIELD_ADVANCED_ENGINEERING},
        {TECH_FIELD_CHEMISTRY, TECH_FIELD_ADVANCED_METALLURGY},
        {TECH_FIELD_NUCLEAR_FISSION, TECH_FIELD_COLD_FUSION},
        {TECH_FIELD_COLD_FUSION, TECH_FIELD_ADVANCED_FUSION},
        {TECH_FIELD_PHYSICS, TECH_FIELD_FUSION_PHYSICS},
        {TECH_FIELD_ELECTRONICS, TECH_FIELD_OPTRONICS},
    };
    for (const auto& link : preserved_links) {
        CHECK(TECHDATA::technology_fields[link[0]].next_field_id == link[1]);
        CHECK(TECHDATA::technology_fields[link[1]].prev_field_id == link[0]);
    }
    for (std::int16_t application_id = TECH_APP_BIOLOGY;
         application_id < TECH_APP_COUNT;
         ++application_id) {
        CHECK(TECHDATA::technology_applications[application_id]
                  .tech_field_id ==
            technology_guard.applications[application_id].tech_field_id);
    }
    const std::int16_t government_applications[] = {
        TECH_APP_FEDERATION,
        TECH_APP_CONFEDERATION,
        TECH_APP_GALACTIC_UNIFICATION,
        TECH_APP_IMPERIUM,
    };
    for (const std::int16_t application_id : government_applications) {
        CHECK(TECHDATA::technology_applications[application_id]
                  .tech_field_id == TECH_FIELD_ADVANCED_GOVERNMENTS);
    }
    CHECK(std::memcmp(TECHDATA::starting_tech_field_ids,
              technology_guard.starting_fields,
              sizeof(technology_guard.starting_fields)) == 0);

    CHECK(game_random::Get_Random_Seed() == 1530528153u);
    CHECK(Technology_Graph_Digest_() ==
        "a4572998dba2a43a00e62e1b456db85215c670b715207102d04636097bf0a021");
    technology_guard.Restore_();
    game_random::Set_Random_Seed(1);
    REQUIRE(TECHINIT::Finalize_Configured_New_Game_Technology(&diagnostic));
    CHECK(game_random::Get_Random_Seed() == 2420584270u);
    CHECK(Technology_Graph_Digest_() ==
        "90689f59f42f4b19937d9061e03b0377eae0413c36c47be7d93b8ce35d70a72d");
    technology_guard.Restore_();
    game_random::Set_Random_Seed(2);
    REQUIRE(TECHINIT::Finalize_Configured_New_Game_Technology(&diagnostic));
    CHECK(game_random::Get_Random_Seed() == 112292373u);
    CHECK(Technology_Graph_Digest_() ==
        "550f7dc202e8b309ed6b9b675318cf1c64401c92e27c782d7d18df9e0fb71b94");

    Shutdown_Files_(&files);
}

TEST_CASE("technology setup composes writes and preserves completed RNG draws") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_technology_state_guard technology_guard;
    s_package_files files;
    Initialize_Files_(&files);
    Write_File_(files,
        "main.lua",
        "events.on('new_game.technology_setup', function(event)\n"
        "  if (event.setup_sequence ~= 7 and event.setup_sequence ~= 1) or\n"
        "      pcall(function() event.setup_sequence = 1 end) then\n"
        "    error('invalid technology setup payload')\n"
        "  end\n"
        "  storage.first = game.rng:integer(0, 100)\n"
        "  game.technology.applications:get('plasma_cannon').field =\n"
        "    game.technology.fields:get('advanced_biology')\n"
        "end)\n"
        "events.on('new_game.technology_setup', function()\n"
        "  local application = game.technology.applications:get('plasma_cannon')\n"
        "  if application.field.id ~= 'advanced_biology' then\n"
        "    error('earlier write was not visible')\n"
        "  end\n"
        "  storage.second = game.rng:integer(0, 100)\n"
        "  application.field = game.technology.fields:get('plasma_physics')\n"
        "end)\n"
        "events.on('new_game.player_setup', function()\n"
        "  if game.technology.applications:get('plasma_cannon').field.id ~=\n"
        "      'plasma_physics' then\n"
        "    error('player setup did not observe installed technology')\n"
        "  end\n"
        "end)\n");

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic stage_diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &stage_diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    constexpr uint32_t SEED = 0x24681357;
    game_random::Set_Random_Seed(SEED);
    game_random::Random(100);
    game_random::Random(100);
    const uint32_t expected_seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(SEED);
    const orion2re::lua::game::s_technology_setup_event event = {7};
    std::string diagnostic;
    CAPTURE(diagnostic);
    REQUIRE(orion2re::lua::game::Technology_Setup(&event, &diagnostic));
    REQUIRE(TECHINIT::Validate_And_Rebuild_Technology(
        TECHDATA::technology_applications,
        TECHDATA::technology_fields,
        TECHDATA::starting_tech_field_ids,
        &diagnostic));
    CHECK(game_random::Get_Random_Seed() == expected_seed);
    CHECK(TECHDATA::technology_applications[TECH_APP_PLASMA_CANNON]
              .tech_field_id == TECH_FIELD_PLASMA_PHYSICS);
    const orion2re::lua::game::s_player_setup_event player_event = {0};
    orion2re::lua::game::Player_Setup(&player_event);

    const std::int16_t installed_field =
        TECHDATA::technology_applications[TECH_APP_PLASMA_CANNON]
            .tech_field_id;
    orion2re::lua::Reset_Mod_Runtimes();
    orion2re::lua::s_lua_registry reconstructed_registry;
    orion2re::lua::s_lua_mod_runtime_set* reconstructed = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &reconstructed_registry,
        &reconstructed,
        &stage_diagnostic));
    std::string load_diagnostic;
    REQUIRE(orion2re::lua::Dispatch_Loaded_Event(
        reconstructed, &load_diagnostic));
    CHECK(TECHDATA::technology_applications[TECH_APP_PLASMA_CANNON]
              .tech_field_id == installed_field);
    CHECK(game_random::Get_Random_Seed() == expected_seed);
    orion2re::lua::Commit_Mod_Runtimes(&reconstructed);
    CHECK(TECHDATA::technology_applications[TECH_APP_PLASMA_CANNON]
              .tech_field_id == installed_field);
    CHECK(game_random::Get_Random_Seed() == expected_seed);

    MOX::game_type = 2;
    game_random::Set_Random_Seed(SEED);
    REQUIRE(TECHINIT::Finalize_Configured_New_Game_Technology(&diagnostic));
    CHECK(game_random::Get_Random_Seed() == expected_seed);
    MOX::game_type = 3;
    game_random::Set_Random_Seed(SEED);
    REQUIRE(TECHINIT::Finalize_Configured_New_Game_Technology(&diagnostic));
    CHECK(game_random::Get_Random_Seed() == SEED);
    Shutdown_Files_(&files);
}

TEST_CASE("technology setup failures preserve completed live writes and RNG draws") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_technology_state_guard technology_guard;
    s_package_files files;
    Initialize_Files_(&files);
    const auto stage = [&](const std::string& source,
                           std::uint64_t instructions = 1'000'000) {
        Write_File_(files, "main.lua", source);
        config::s_mod_manifest package;
        package.id = "scavenger";
        package.version_text = "1.0.0";
        package.has_lua_runtime = true;
        package.lua_runtime.api = 1;
        package.lua_runtime.storage_version = 1;
        std::vector<config::s_mod_manifest> packages = {package};
        orion2re::lua::s_lua_mod_limits limits;
        limits.runtime.instructions = instructions;
        orion2re::lua::s_lua_registry registry;
        orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
        config::s_cfg_diagnostic diagnostic;
        REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
            &packages, &limits, &registry, &runtimes, &diagnostic));
        orion2re::lua::Commit_Mod_Runtimes(&runtimes);
    };
    const auto unchanged = [&]() {
        CHECK(std::memcmp(TECHDATA::technology_applications,
                  technology_guard.applications,
                  sizeof(technology_guard.applications)) == 0);
        CHECK(std::memcmp(TECHDATA::technology_fields,
                  technology_guard.fields,
                  sizeof(technology_guard.fields)) == 0);
    };

    constexpr uint32_t SEED = 0x13572468;
    stage("events.on('new_game.technology_setup', function()\n"
          "  game.rng:integer(0, 100)\n"
          "  game.technology.applications:get('plasma_cannon').field =\n"
          "    game.technology.fields:get('advanced_biology')\n"
          "  error('stop')\n"
          "end)\n");
    game_random::Set_Random_Seed(SEED);
    game_random::Random(100);
    const uint32_t one_draw_seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(SEED);
    std::string diagnostic;
    CHECK_FALSE(TECHINIT::Finalize_Configured_New_Game_Technology(
        &diagnostic));
    CHECK(game_random::Get_Random_Seed() == one_draw_seed);
    CHECK(TECHDATA::technology_applications[TECH_APP_PLASMA_CANNON]
              .tech_field_id == TECH_FIELD_ADVANCED_BIOLOGY);
    technology_guard.Restore_();
    unchanged();

    orion2re::lua::Reset_Mod_Runtimes();
    stage("events.on('new_game.technology_setup', function()\n"
          "  game.rng:integer(0, 100)\n"
          "  while true do end\n"
          "end)\n",
        2000);
    game_random::Set_Random_Seed(SEED);
    CHECK_FALSE(TECHINIT::Finalize_Configured_New_Game_Technology(
        &diagnostic));
    CHECK(game_random::Get_Random_Seed() == one_draw_seed);
    unchanged();

    orion2re::lua::Reset_Mod_Runtimes();
    stage("events.on('new_game.technology_setup', function()\n"
          "  game.rng:integer(0, 100)\n"
          "  local applications = game.technology.applications\n"
          "  local field = game.technology.fields:get('advanced_biology')\n"
          "  applications:get('achilles_targeting_unit').field = field\n"
          "  applications:get('adamantium_armor').field = field\n"
          "  applications:get('advanced_city_planning').field = field\n"
          "  applications:get('advanced_damage_control').field = field\n"
          "  applications:get('alien_management_center').field = field\n"
          "end)\n");
    game_random::Set_Random_Seed(SEED);
    CHECK_FALSE(TECHINIT::Finalize_Configured_New_Game_Technology(
        &diagnostic));
    CAPTURE(diagnostic);
    CHECK(game_random::Get_Random_Seed() == one_draw_seed);
    const std::int16_t reassigned_applications[] = {
        TECH_APP_ACHILLES_TARGETING_UNIT,
        TECH_APP_ADAMANTIUM_ARMOR,
        TECH_APP_ADVANCED_CITY_PLANNING,
        TECH_APP_ADVANCED_DAMAGE_CONTROL,
        TECH_APP_ALIEN_MANAGEMENT_CENTER,
    };
    for (const std::int16_t application_id : reassigned_applications) {
        CHECK(TECHDATA::technology_applications[application_id]
                  .tech_field_id == TECH_FIELD_ADVANCED_BIOLOGY);
    }
    technology_guard.Restore_();
    unchanged();
    Shutdown_Files_(&files);
}

TEST_CASE("game RNG directly advances the native deterministic stream") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();

    constexpr uint32_t INITIAL_SEED = 0x12345678;
    game_random::Set_Random_Seed(INITIAL_SEED);
    lua_Integer expected_setup =
        static_cast<lua_Integer>(game_random::Random(20)) - 10;
    lua_Integer expected_read_only =
        static_cast<lua_Integer>(game_random::Random(200)) - 100;
    lua_Integer expected_mutable =
        static_cast<lua_Integer>(game_random::Random(10)) + 10;
    lua_Integer expected_rule =
        static_cast<lua_Integer>(game_random::Random(20)) + 20;
    const uint32_t expected_seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(INITIAL_SEED);

    const orion2re::lua::s_lua_event_descriptor events[] = {
        {"test.setup", 1},
        {"test.read_only", 1},
        {"test.mutable", 1, orion2re::lua::LUA_GAME_STATE_MUTABLE},
        {"test.invalid", 1},
    };
    const orion2re::lua::s_lua_rule_descriptor rules[] = {
        {"test.value", 1},
    };
    const char source[] =
        "local setup_roll = game.rng:integer(-10, 10)\n"
        "if game.players ~= nil or storage ~= nil then\n"
        "  error('setup exposed live game or storage state')\n"
        "end\n"
        "events.on('test.setup', function(expected)\n"
        "  if setup_roll ~= expected then error('wrong setup RNG result') end\n"
        "end)\n"
        "events.on('test.read_only', function(expected)\n"
        "  if game.rng:integer(-100, 100) ~= expected then\n"
        "    error('wrong read-only event RNG result')\n"
        "  end\n"
        "end)\n"
        "events.on('test.mutable', function(expected)\n"
        "  if game.rng:integer(10, 20) ~= expected then\n"
        "    error('wrong mutable event RNG result')\n"
        "  end\n"
        "end)\n"
        "events.on('test.invalid', function()\n"
        "  local seed = game.rng:get_seed()\n"
        "  game.rng:set_seed(4294967295)\n"
        "  if game.rng:get_seed() ~= 4294967295 then\n"
        "    error('RNG seed did not round trip')\n"
        "  end\n"
        "  game.rng:set_seed(seed)\n"
        "  local seed_was_not_advanced =\n"
        "    not pcall(function() game.rng:integer(4, 4) end) and\n"
        "    not pcall(function() game.rng:integer(5, 4) end) and\n"
        "    not pcall(function() game.rng:integer(0, 4294967295) end) and\n"
        "    not pcall(function() game.rng:integer(-9223372036854775807,\n"
        "      9223372036854775807) end)\n"
        "  if not seed_was_not_advanced or\n"
        "      pcall(function() game.rng:set_seed(-1) end) or\n"
        "      pcall(function() game.rng:set_seed(4294967296) end) or\n"
        "      pcall(function() game.rng.integer = nil end) then\n"
        "    error('invalid RNG access succeeded')\n"
        "  end\n"
        "end)\n"
        "rules.modify('test.value', function(expected, value)\n"
        "  local roll = game.rng:integer(20, 40)\n"
        "  if roll ~= expected then error('wrong rule RNG result') end\n"
        "  return roll\n"
        "end)\n";
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = nullptr;
    CAPTURE(result.diagnostic);
    REQUIRE(orion2re::lua::Create_Mod_Runtime("rng",
        "1.0.0",
        source,
        sizeof(source) - 1,
        "mods/rng/main.lua",
        sizeof(source) - 1,
        events,
        std::size(events),
        rules,
        std::size(rules),
        &limits,
        &runtime,
        &result));

    REQUIRE(orion2re::lua::Dispatch_Mod_Event(runtime,
        "test.setup",
        Build_Integer_Context_,
        &expected_setup,
        &result));
    MOX::game_type = 2;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(runtime,
        "test.read_only",
        Build_Integer_Context_,
        &expected_read_only,
        &result));
    MOX::game_type = 3;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(runtime,
        "test.mutable",
        Build_Integer_Context_,
        &expected_mutable,
        &result));
    std::int64_t rule_result = 0;
    REQUIRE(orion2re::lua::Dispatch_Mod_Rule(runtime,
        "test.value",
        Build_Integer_Context_,
        &expected_rule,
        0,
        100,
        &rule_result,
        &result));
    CHECK(rule_result == expected_rule);
    CHECK(game_random::Get_Random_Seed() == expected_seed);
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.invalid", nullptr, nullptr, &result));
    CHECK(game_random::Get_Random_Seed() == expected_seed);
    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("player trait reads include canonical built-in traits") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();

    const orion2re::lua::s_lua_event_descriptor events[] = {
        {"test.selected", 1},
        {"test.poor", 1},
        {"test.false", 1},
        {"test.invalid", 1},
    };
    const char source[] =
        "local traits = {\n"
        "  'low_gravity', 'high_gravity', 'aquatic', 'subterranean',\n"
        "  'large_home_world', 'rich_home_world', 'poor_home_world',\n"
        "  'artifacts_home_world', 'cybernetic', 'lithovore', 'repulsive',\n"
        "  'charismatic', 'uncreative', 'creative', 'tolerant',\n"
        "  'fantastic_traders', 'telepathic', 'lucky', 'omniscient',\n"
        "  'stealthy_ships', 'trans_dimensional', 'warlord',\n"
        "}\n"
        "local selected_index = 1\n"
        "events.on('test.selected', function()\n"
        "  local player = game.players:get(0)\n"
        "  for index, trait in ipairs(traits) do\n"
        "    if player:has_trait(trait) ~= (index == selected_index) then\n"
        "      error('selected built-in trait mismatch: ' .. trait)\n"
        "    end\n"
        "  end\n"
        "  selected_index = selected_index + 1\n"
        "  if pcall(function() return player:has_trait('missing') end) then\n"
        "    error('unknown trait was accepted')\n"
        "  end\n"
        "end)\n"
        "events.on('test.poor', function()\n"
        "  local player = game.players:get(0)\n"
        "  if player:has_trait('rich_home_world') or\n"
        "      not player:has_trait('poor_home_world') then\n"
        "    error('signed home-world trait mismatch')\n"
        "  end\n"
        "end)\n"
        "events.on('test.false', function()\n"
        "  local player = game.players:get(0)\n"
        "  for _, trait in ipairs(traits) do\n"
        "    if player:has_trait(trait) then\n"
        "      error('false built-in trait mismatch: ' .. trait)\n"
        "    end\n"
        "  end\n"
        "end)\n"
        "events.on('test.invalid', function()\n"
        "  if pcall(function()\n"
        "      return game.players:get(0):has_trait('low_gravity')\n"
        "    end) then\n"
        "    error('invalid native trait value was accepted')\n"
        "  end\n"
        "end)\n";
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = nullptr;
    REQUIRE(orion2re::lua::Create_Mod_Runtime("traits",
        "1.0.0",
        source,
        sizeof(source) - 1,
        "mods/traits/main.lua",
        sizeof(source) - 1,
        events,
        std::size(events),
        nullptr,
        0,
        &limits,
        &runtime,
        &result));

    CAPTURE(result.diagnostic);
    for (int index = 0; index < raceopt::SPECIAL_PICK_COUNT; ++index) {
        memset(MOX::player[0].traits + TRAIT_LOW_G_WORLD,
            0,
            TRAIT_COUNT - TRAIT_LOW_G_WORLD);
        const raceopt::s_special_pick_identity& identity =
            raceopt::special_pick_identities[index];
        MOX::player[0].traits[identity.trait] = identity.selected_value;
        REQUIRE(orion2re::lua::Dispatch_Mod_Event(
            runtime, "test.selected", nullptr, nullptr, &result));
    }
    MOX::player[0].traits[TRAIT_RICH_HOME_WORLD] = -1;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.poor", nullptr, nullptr, &result));
    memset(MOX::player[0].traits + TRAIT_LOW_G_WORLD,
        0,
        TRAIT_COUNT - TRAIT_LOW_G_WORLD);
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.false", nullptr, nullptr, &result));
    MOX::player[0].traits[TRAIT_LOW_G_WORLD] = 2;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.invalid", nullptr, nullptr, &result));
    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("built-in player trait reads reject malformed metadata and players") {
    TEST_LUA_GAME::Reset_();
    using orion2re::lua::game::internal::e_player_builtin_trait_result;
    using orion2re::lua::game::internal::Read_Player_Builtin_Trait;

    const config::s_generated_enum* metadata =
        config::Find_Generated_Enum("race_special_choice");
    REQUIRE(metadata != nullptr);
    bool has_trait = true;
    CHECK(Read_Player_Builtin_Trait(
              -1, "low_gravity", metadata, &has_trait) ==
        e_player_builtin_trait_result::invalid);
    CHECK_FALSE(has_trait);
    CHECK(Read_Player_Builtin_Trait(
              MOX::NUM_PLAYERS, "low_gravity", metadata, &has_trait) ==
        e_player_builtin_trait_result::invalid);

    config::s_generated_enum malformed = *metadata;
    --malformed.value_count;
    CHECK(Read_Player_Builtin_Trait(
              0, "low_gravity", &malformed, &has_trait) ==
        e_player_builtin_trait_result::invalid);
    malformed = *metadata;
    malformed.name = "wrong_trait_type";
    CHECK(Read_Player_Builtin_Trait(
              0, "low_gravity", &malformed, &has_trait) ==
        e_player_builtin_trait_result::invalid);

    std::vector<config::s_generated_enum_symbol> symbols(
        metadata->values, metadata->values + metadata->value_count);
    malformed = *metadata;
    malformed.values = symbols.data();
    symbols[0].value = 1;
    CHECK(Read_Player_Builtin_Trait(
              0, "low_gravity", &malformed, &has_trait) ==
        e_player_builtin_trait_result::invalid);
    symbols[0] = metadata->values[0];
    symbols[0].name = nullptr;
    CHECK(Read_Player_Builtin_Trait(
              0, "low_gravity", &malformed, &has_trait) ==
        e_player_builtin_trait_result::invalid);
    const char* first_name = symbols[0].name;
    symbols[0].name = symbols[1].name;
    symbols[1].name = first_name;
    CHECK(Read_Player_Builtin_Trait(
              0, "low_gravity", &malformed, &has_trait) ==
        e_player_builtin_trait_result::invalid);

    MOX::player[0].traits[TRAIT_LOW_G_WORLD] = -1;
    CHECK(Read_Player_Builtin_Trait(
              0, "low_gravity", metadata, &has_trait) ==
        e_player_builtin_trait_result::invalid);
    MOX::player[0].traits[TRAIT_RICH_HOME_WORLD] = -2;
    CHECK(Read_Player_Builtin_Trait(
              0, "rich_home_world", metadata, &has_trait) ==
        e_player_builtin_trait_result::invalid);
    MOX::player[0].traits[TRAIT_RICH_HOME_WORLD] = 2;
    CHECK(Read_Player_Builtin_Trait(
              0, "poor_home_world", metadata, &has_trait) ==
        e_player_builtin_trait_result::invalid);
    MOX::player[0].traits[TRAIT_RICH_HOME_WORLD] = 0;
    MOX::player[0].traits[TRAIT_WARLORD] = 2;
    CHECK(Read_Player_Builtin_Trait(
              0, "low_gravity", metadata, &has_trait) ==
        e_player_builtin_trait_result::invalid);
}

TEST_CASE("scavenger trait awards research and retains event history") {
    TEST_LUA_GAME::Reset_();
    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_PLASMA_RIFLE] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_PLASMA_TORPEDOES] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    orion2re::lua::game::Reset_Semantic_State();
    s_package_files files;
    Initialize_Files_(&files);
    Write_File_(files, "content/scavenger.lua", "return { id = 'scavenger', cost = 5 }\n");
    Write_File_(files, "main.lua", "local scavenger = require('content.scavenger')\n"
                                   "prototypes.race_traits:add(scavenger)\n"
                                   "local prior_killer\n"
                                   "events.on('game.loaded', function(event)\n"
                                   "  if getmetatable(event) ~= 'immutable Lua event object' then\n"
                                   "    error('loaded event metatable is exposed')\n"
                                   "  end\n"
                                   "  if pcall(function() event.changed = true end) then\n"
                                   "    error('loaded event is mutable')\n"
                                   "  end\n"
                                   "  if #game.players:all() ~= 2 then error('loaded game read failed') end\n"
                                   "  if pcall(function() game.players:get(0):add_research(1) end) then\n"
                                   "    error('loaded event mutated player state')\n"
                                   "  end\n"
                                   "end)\n"
                                   "events.on('ship.destroyed', function(event)\n"
                                   "  local players = game.players:all()\n"
                                   "  if #players ~= 2 or players[1].id ~= 0 or players[2].id ~= 1 or\n"
                                   "      game.players:get(0) ~= players[1] or game.players:get(9) ~= nil or\n"
                                   "      not players[1].is_human or players[2].is_human or\n"
                                   "      players[1].government ~= defines.government.democracy or\n"
                                   "      players[2].government ~= defines.government.unification or\n"
                                   "      players[1].home_planet ~= game.planets:get(0) or\n"
                                   "      players[1].home_system ~= game.systems:get(0) or\n"
                                   "      players[2].home_planet ~= nil or players[2].home_system ~= nil then\n"
                                   "    error('player collection is not stable and ordered')\n"
                                   "  end\n"
                                   "  local ships = game.ships:all()\n"
                                   "  if #ships ~= 2 or ships[1].id ~= 0 or ships[2].id ~= 1 or\n"
                                   "      game.ships:get(0) ~= event.killer or game.ships:get(9) ~= nil then\n"
                                   "    error('ship collection is not stable and ordered')\n"
                                   "  end\n"
                                   "  local planets = game.planets:all()\n"
                                   "  local systems = game.systems:all()\n"
                                   "  local inner = systems[1].planets:all()\n"
                                   "  if #planets ~= 3 or planets[1].id ~= 0 or planets[3].id ~= 2 or\n"
                                   "      #systems ~= 2 or systems[1].id ~= 0 or systems[2].id ~= 1 or\n"
                                   "      #inner ~= 2 or inner[1].id ~= 0 or inner[2].id ~= 2 or\n"
                                   "      planets[1] ~= game.planets:get(0) or\n"
                                   "      planets[1].system ~= systems[1] or planets[1].orbit ~= 0 or\n"
                                   "      planets[1].planet_type ~= defines.planet_type.planet or\n"
                                   "      planets[1].size ~= defines.planet_size.medium or\n"
                                   "      planets[1].gravity ~= defines.gravity.normal or\n"
                                   "      planets[1].climate ~= defines.climate.terran or\n"
                                   "      planets[1].minerals ~= defines.minerals.abundant or\n"
                                   "      planets[1].special ~= defines.planet_special.none then\n"
                                   "    error('planet or system collection is not stable and ordered')\n"
                                   "  end\n"
                                   "  if pcall(function() planets[1].size = 4 end) then\n"
                                   "    error('planet proxy is mutable')\n"
                                   "  end\n"
                                   "  local technology = game.technology\n"
                                   "  local applications = technology.applications:all()\n"
                                   "  local fields = technology.fields:all()\n"
                                   "  local plasma = technology.applications:get('plasma_cannon')\n"
                                   "  local plasma_field = technology.fields:get('plasma_physics')\n"
                                   "  local field_apps = plasma_field.applications:all()\n"
                                   "  local player_technologies = players[1].technologies\n"
                                   "  local alternatives = player_technologies:alternatives('plasma_physics')\n"
                                   "  local starting = technology.starting_fields:all()\n"
                                   "  local found_plasma = false\n"
                                   "  for _, application in ipairs(field_apps) do\n"
                                   "    found_plasma = found_plasma or application == plasma\n"
                                   "  end\n"
                                   "  if #applications ~= 212 or applications[1].id ~= 'no_tech' or\n"
                                   "      applications[138].id ~= 'plasma_cannon' or\n"
                                   "      #fields ~= 83 or fields[1].id ~= 'starting_tech' or\n"
                                   "      fields[60].id ~= 'plasma_physics' or plasma.field ~= plasma_field or\n"
                                   "      plasma_field.cost ~= 3500 or\n"
                                   "      plasma_field.previous.id ~= 'multi_phased_physics' or\n"
                                   "      plasma_field.next.id ~= 'multi_dimensional_physics' or\n"
                                   "      not found_plasma or #starting ~= 6 or\n"
                                   "      starting[1].id ~= 'engineering' or\n"
                                   "      technology.starting_fields:get(2).id ~= 'nuclear_fission' or\n"
                                   "      technology.starting_fields:get(0) ~= nil or\n"
                                   "      applications[126].field ~= nil or\n"
                                   "      technology.applications:get('missing') ~= nil or\n"
                                   "      technology.fields:get('missing') ~= nil then\n"
                                   "    error('technology graph is not stable and ordered')\n"
                                   "  end\n"
                                   "  if player_technologies ~= players[1].technologies or\n"
                                   "      player_technologies:status('plasma_cannon') ~=\n"
                                   "        defines.technology_status.available or\n"
                                   "      not player_technologies:is_available('plasma_cannon') or\n"
                                   "      player_technologies:is_researched('plasma_cannon') or\n"
                                   "      player_technologies:status('plasma_rifle') ~=\n"
                                   "        defines.technology_status.researched or\n"
                                   "      not player_technologies:is_researched('plasma_rifle') or\n"
                                   "      player_technologies:status('plasma_torpedo') ~=\n"
                                   "        defines.technology_status.unavailable or\n"
                                   "      player_technologies:is_available('plasma_torpedo') or\n"
                                   "      player_technologies:field_of('plasma_cannon') ~= plasma_field or\n"
                                   "      player_technologies:field_of('phase_shifter') ~= nil or\n"
                                   "      #alternatives ~= #field_apps then\n"
                                   "    error('player technology state is invalid')\n"
                                   "  end\n"
                                   "  for index, application in ipairs(alternatives) do\n"
                                   "    if application ~= field_apps[index] then\n"
                                   "      error('player technology alternatives are not stable')\n"
                                   "    end\n"
                                   "  end\n"
                                   "  if pcall(function() return player_technologies:status('missing') end) or\n"
                                   "      pcall(function() return player_technologies:field_of(137) end) or\n"
                                   "      pcall(function() return player_technologies:alternatives('missing') end) or\n"
                                   "      player_technologies.make_available ~= nil or\n"
                                   "      player_technologies.make_unavailable ~= nil or\n"
                                   "      player_technologies.replace_initial_choice ~= nil or\n"
                                   "      pcall(function() player_technologies.status = nil end) or\n"
                                   "      pcall(function() defines.technology_status.researched = 3 end) then\n"
                                   "    error('player technology API accepted invalid access')\n"
                                   "  end\n"
                                   "  if pcall(function() plasma.field = fields[1] end) or\n"
                                   "      pcall(function() plasma_field.cost = 1 end) then\n"
                                   "    error('technology graph is mutable')\n"
                                   "  end\n"
                                   "  if defines.ship_special.augmented_engines == nil or\n"
                                   "      defines.ship_special.reinforced_hull == nil or\n"
                                   "      defines.ship_special.augmented_engines ==\n"
                                   "        defines.ship_special.reinforced_hull or\n"
                                   "      pcall(function() defines.ship_special.reinforced_hull = 0 end) then\n"
                                   "    error('ship special definitions are unavailable or mutable')\n"
                                   "  end\n"
                                   "  if prior_killer then\n"
                                   "    if prior_killer.kind ~= 'ship' or prior_killer.id ~= 0 or\n"
                                   "        not prior_killer.valid then\n"
                                   "      error('ship proxy did not retain stable identity')\n"
                                   "    end\n"
                                   "  end\n"
                                   "  local mutable = pcall(function() event.sequence = 0 end)\n"
                                   "  if mutable then error('event payload is mutable') end\n"
                                   "  if event.phase ~= 'combat' then error('wrong phase') end\n"
                                   "  if event.destroyed_ship.id ~= 1 or\n"
                                   "      event.destroyed_ship.owner.id ~= 1 or\n"
                                   "      event.destroyed_ship.cost ~= 200 or\n"
                                   "      event.destroyed_ship.name ~= 'Target' then\n"
                                   "    error('wrong destroyed ship record')\n"
                                   "  end\n"
                                   "  local owner = event.killer and event.killer.owner\n"
                                   "  if owner ~= game.players:get(0) then error('player identity mismatch') end\n"
                                   "  if owner and owner:has_trait('scavenger') then\n"
                                   "    storage.scavenged_ships = (storage.scavenged_ships or 0) + 1\n"
                                   "    owner:add_research(event.destroyed_ship.cost // 20 +\n"
                                   "      storage.scavenged_ships - 1)\n"
                                   "  end\n"
                                   "  prior_killer = event.killer\n"
                                   "end)\n");

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const orion2re::lua::s_lua_game_traits* traits =
        orion2re::lua::Get_Active_Game_Traits();
    CHECK_FALSE(registry.prototype_identity.empty());
    REQUIRE(orion2re::lua::Get_Race_Trait_Prototype_Count(traits) == 1);
    orion2re::lua::s_lua_race_trait_prototype trait;
    REQUIRE(orion2re::lua::Find_Race_Trait_Prototype(
        traits, "scavenger", &trait));
    CHECK(trait.cost == 5);
    REQUIRE(orion2re::lua::game::Set_Player_Race_Trait(
        0, "scavenger", true));
    std::string load_diagnostic;
    CAPTURE(load_diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Active_Loaded_Event(&load_diagnostic));
    CHECK((MOX::player[0].research_accumulated == 0));

    MOX::combat_data[0].ship_idx = 0;
    MOX::combat_data[0].owner = 0;
    strcpy(MOX::combat_data[0].name, "Hunter");
    MOX::combat_data[1].ship_idx = 1;
    MOX::combat_data[1].owner = 1;
    strcpy(MOX::combat_data[1].name, "Target");
    MOX::ship[1].d.cost = 200;

    const orion2re::lua::game::s_ship_destroyed_event event = {
        1,
        0,
        1,
    };
    orion2re::lua::game::Ship_Destroyed(&event);
    CHECK((MOX::player[0].research_accumulated == 10));
    orion2re::lua::game::Ship_Destroyed(&event);
    CHECK((MOX::player[0].research_accumulated == 21));

    Shutdown_Files_(&files);
}

TEST_CASE("race trait prototype registration rejects invalid definitions") {
    struct s_case {
        const char* prototype;
        const char* message;
    };
    const s_case cases[] = {
        {"{ id = 'Bad-Trait', cost = 1 }", "invalid race trait prototype ID"},
        {"{ id = 'scavenger', cost = 40000 }", "signed 16-bit range"},
    };
    for (const s_case& value : cases) {
        CAPTURE(value.prototype);
        s_package_files files;
        Initialize_Files_(&files);
        Write_File_(files, "main.lua", "prototypes.race_traits:add(" + std::string(value.prototype) + ")\n");
        config::s_mod_manifest package;
        package.id = "scavenger";
        package.version_text = "1.0.0";
        package.has_lua_runtime = true;
        package.lua_runtime.api = 1;
        package.lua_runtime.storage_version = 1;
        std::vector<config::s_mod_manifest> packages = {package};
        orion2re::lua::s_lua_mod_limits limits;
        orion2re::lua::s_lua_registry registry;
        orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
        config::s_cfg_diagnostic diagnostic;
        CHECK_FALSE(orion2re::lua::Stage_Mod_Runtimes(
            &packages, &limits, &registry, &runtimes, &diagnostic));
        CHECK(diagnostic.message.find(value.message) != std::string::npos);
        orion2re::lua::Destroy_Mod_Runtime_Set(runtimes);
        Shutdown_Files_(&files);
    }
}

TEST_CASE("player technology mutations use existing callback capabilities") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_PLASMA_RIFLE] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_PLASMA_WEB] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;

    const orion2re::lua::s_lua_event_descriptor events[] = {
        {"test.read_only", 1},
        {"test.mutable", 1, orion2re::lua::LUA_GAME_STATE_MUTABLE},
        {"test.invalid_status", 1, orion2re::lua::LUA_GAME_STATE_MUTABLE},
        {"new_game.player_setup",
            1,
            orion2re::lua::LUA_GAME_STATE_PLAYER_SETUP_MUTABLE},
    };
    const char source[] =
        "events.on('test.read_only', function()\n"
        "  local technologies = game.players:get(0).technologies\n"
        "  if pcall(function()\n"
        "      technologies:set_available('plasma_cannon', false)\n"
        "    end) or pcall(function()\n"
        "      technologies:set_field_application(\n"
        "        'plasma_physics', 'plasma_web')\n"
        "    end) then\n"
        "    error('read-only callback mutated player technologies')\n"
        "  end\n"
        "end)\n"
        "events.on('test.mutable', function()\n"
        "  local technologies = game.players:get(0).technologies\n"
        "  technologies:set_available('phase_shifter', true)\n"
        "  technologies:set_field_application(\n"
        "    'plasma_physics', 'plasma_web')\n"
        "  if not technologies:is_available('phase_shifter') or\n"
        "      not technologies:is_available('plasma_web') or\n"
        "      technologies:is_available('plasma_cannon') then\n"
        "    error('technology mutation is not immediately visible')\n"
        "  end\n"
        "  local invalid = {\n"
        "    function() technologies:set_available('no_tech', true) end,\n"
        "    function() technologies:set_available('plasma_rifle', false) end,\n"
        "    function() technologies:set_available('plasma_web', 1) end,\n"
        "    function() technologies:replace_available(\n"
        "      'plasma_cannon', 'plasma_rifle', 'plasma_web', true) end,\n"
        "    function() technologies:set_field_application(\n"
        "      'plasma_physics', 'plasma_torpedo') end,\n"
        "  }\n"
        "  for _, operation in ipairs(invalid) do\n"
        "    if pcall(operation) then error('invalid technology mutation succeeded') end\n"
        "  end\n"
        "end)\n"
        "events.on('test.invalid_status', function()\n"
        "  local technologies = game.players:get(0).technologies\n"
        "  if pcall(function() technologies:set_field_application(\n"
        "      'plasma_physics', 'plasma_web') end) then\n"
        "    error('malformed technology status was accepted')\n"
        "  end\n"
        "end)\n"
        "events.on('new_game.player_setup', function()\n"
        "  local technologies = game.players:get(0).technologies\n"
        "  technologies:replace_available(\n"
        "    'plasma_cannon', 'plasma_rifle', 'plasma_web', true)\n"
        "  technologies:replace_available(\n"
        "    'plasma_rifle', 'plasma_cannon', 'plasma_web', false)\n"
        "  local invalid = {\n"
        "    function() technologies:replace_available(\n"
        "      'no_tech', 'plasma_rifle', 'plasma_web', true) end,\n"
        "    function() technologies:replace_available(\n"
        "      'plasma_cannon', 'plasma_cannon', 'plasma_web', true) end,\n"
        "  }\n"
        "  for _, operation in ipairs(invalid) do\n"
        "    if pcall(operation) then\n"
        "      error('invalid technology replacement succeeded')\n"
        "    end\n"
        "  end\n"
        "  technologies:set_status(\n"
        "    'phase_shifter', defines.technology_status.researched)\n"
        "  if technologies:status('phase_shifter') ~=\n"
        "      defines.technology_status.researched or\n"
        "      pcall(function() technologies:set_status('phase_shifter', 3) end) then\n"
        "    error('technology status conversion failed')\n"
        "  end\n"
        "end)\n";
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = nullptr;
    REQUIRE(orion2re::lua::Create_Mod_Runtime("technology_mutation",
        "1.0.0",
        source,
        sizeof(source) - 1,
        "mods/technology_mutation/main.lua",
        sizeof(source) - 1,
        events,
        std::size(events),
        nullptr,
        0,
        &limits,
        &runtime,
        &result));

    CAPTURE(result.diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.read_only", nullptr, nullptr, &result));
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] = 2;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.invalid_status", nullptr, nullptr, &result));
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_WEB] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.mutable", nullptr, nullptr, &result));
    CHECK(MOX::player[0].tech_applications[TECH_APP_PHASE_SHIFTER] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_RIFLE] ==
        TECH_RESEARCH_STATUS_RESEARCHED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_WEB] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_PLASMA_RIFLE] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_PLASMA_WEB] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "new_game.player_setup", nullptr, nullptr, &result));
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_RIFLE] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_WEB] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PHASE_SHIFTER] ==
        TECH_RESEARCH_STATUS_RESEARCHED);
    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("new game player setup dispatches a mutable player ID event") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[1].tech_applications[TECH_APP_PHASE_SHIFTER] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;

    s_package_files files;
    Initialize_Files_(&files);
    // clang-format off
    Write_File_(files, "main.lua",
        "events.on('new_game.player_setup', function(event)\n"
        "  if getmetatable(event) ~= 'immutable Lua event object' or\n"
        "      event.player_id == nil or event.extra ~= nil or\n"
        "      pcall(function() event.player_id = 7 end) then\n"
        "    error('invalid player setup payload')\n"
        "  end\n"
        "  local technologies = game.players:get(event.player_id).technologies\n"
        "  if event.player_id == 0 then\n"
        "    technologies:set_field_application(\n"
        "      'plasma_physics', 'plasma_web')\n"
        "  elseif event.player_id == 1 then\n"
        "    technologies:set_available('phase_shifter', true)\n"
        "  else\n"
        "    error('unexpected player ID')\n"
        "  end\n"
        "end)\n");
    // clang-format on
    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const orion2re::lua::game::s_player_setup_event first = {0};
    const orion2re::lua::game::s_player_setup_event second = {1};
    orion2re::lua::game::Player_Setup(&first);
    orion2re::lua::game::Player_Setup(&second);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_WEB] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    CHECK(MOX::player[1].tech_applications[TECH_APP_PHASE_SHIFTER] ==
        TECH_RESEARCH_STATUS_AVAILABLE);

    MOX::game_type = 2;
    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    orion2re::lua::game::Player_Setup(&first);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_WEB] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    MOX::game_type = 3;
    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_PLASMA_WEB] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    orion2re::lua::game::Player_Setup(&first);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_WEB] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    Shutdown_Files_(&files);
}

TEST_CASE("planet mutation requires a staged home-system callback") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_package_files files;
    Initialize_Files_(&files);
    Write_File_(files, "main.lua", "events.on('new_game.player_setup', function(event)\n"
                                   "  local planet = game.players:get(event.player_id).home_planet\n"
                                   "  if pcall(function() planet.size = defines.planet_size.huge end) or\n"
                                   "      pcall(function() planet.climate = defines.climate.gaia end) or\n"
                                   "      pcall(function() planet:set_special(\n"
                                   "        defines.planet_special.gold_deposits) end) then\n"
                                   "    error('planet mutation escaped staged home-system setup')\n"
                                   "  end\n"
                                   "end)\n");
    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const orion2re::lua::game::s_player_setup_event event = {0};
    orion2re::lua::game::Player_Setup(&event);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_MEDIUM);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate == PLANET_CLIMATE_TERRAN);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_NO_SPECIAL);
    Shutdown_Files_(&files);
}

TEST_CASE("home-system planet mutations validate semantic operations") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_package_files files;
    Initialize_Files_(&files);
    Write_File_(files, "main.lua", "events.on('new_game.home_system_generated', function(event)\n"
                                   "  local player = game.players:get(event.player_id)\n"
                                   "  local planet = player.home_planet\n"
                                   "  if pcall(function() planet.size = 5 end) or\n"
                                   "      pcall(function() planet.climate = 10 end) or\n"
                                   "      pcall(function() planet.food_base = 6 end) or\n"
                                   "      pcall(function() planet:set_special(\n"
                                   "        defines.planet_special.space_debris) end) or\n"
                                   "      pcall(function() player.technologies:set_available(\n"
                                   "        'plasma_cannon', true) end) then\n"
                                   "    error('invalid home-system mutation was accepted')\n"
                                   "  end\n"
                                   "end)\n");
    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    const orion2re::lua::game::s_home_system_generated_event event = {0, 0};
    orion2re::lua::game::Home_System_Generated(&::game::world, &event);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_MEDIUM);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate == PLANET_CLIMATE_TERRAN);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_NO_SPECIAL);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    Shutdown_Files_(&files);
}

TEST_CASE("source-updated TTI repairs player technology choices") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_package_files files;
    Initialize_Files_(&files);
    Write_Techtree_Source_Package_(files);

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    CAPTURE(diagnostic.message);
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const orion2re::lua::game::s_player_setup_event setup = {0};

    Reset_Technology_Player_(0);
    MOX::player[0].tech_applications[TECH_APP_EXTENDED_FUEL_TANKS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] ==
        TECH_RESEARCH_STATUS_AVAILABLE);

    Reset_Technology_Player_(0);
    MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);

    Reset_Technology_Player_(0);
    MOX::player[0].tech_applications[TECH_APP_EXTENDED_FUEL_TANKS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] ==
        TECH_RESEARCH_STATUS_AVAILABLE);

    Reset_Technology_Player_(0);
    MOX::player[0].tech_applications[TECH_APP_EXTENDED_FUEL_TANKS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(MOX::player[0].tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    MOX::player[0].traits[TRAIT_EATS_MINERALS] = 1;
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(MOX::player[0].tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    Reset_Technology_Player_(0);
    MOX::player[0].traits[TRAIT_EATS_MINERALS] = 1;
    MOX::player[0].tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(MOX::player[0].tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] ==
        TECH_RESEARCH_STATUS_RESEARCHED);

    constexpr uint32_t NO_DRAW_SEED = 0x12345678;
    Reset_Technology_Player_(0);
    MOX::player[0].tech_applications[TECH_APP_EXTENDED_FUEL_TANKS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    game_random::Set_Random_Seed(NO_DRAW_SEED);
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(game_random::Get_Random_Seed() == NO_DRAW_SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] ==
        TECH_RESEARCH_STATUS_AVAILABLE);

    const std::int16_t shield_applications[] = {
        TECH_APP_CLASS_I_SHIELD,
        TECH_APP_CLASS_III_SHIELD,
        TECH_APP_CLASS_V_SHIELD,
        TECH_APP_CLASS_VII_SHIELD,
        TECH_APP_CLASS_X_SHIELD,
    };
    for (const std::int16_t shield_application : shield_applications) {
        CAPTURE(shield_application);
        Reset_Technology_Player_(0);
        MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
        MOX::player[0].tech_applications[TECH_APP_EXTENDED_FUEL_TANKS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[shield_application] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        game_random::Set_Random_Seed(NO_DRAW_SEED);
        orion2re::lua::game::Player_Setup(&setup);
        CHECK(game_random::Get_Random_Seed() == NO_DRAW_SEED);
        CHECK(MOX::player[0]
                  .tech_applications[TECH_APP_SHIELD_CAPACITORS] ==
            TECH_RESEARCH_STATUS_AVAILABLE);
    }

    Reset_Technology_Player_(0);
    MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
    game_random::Set_Random_Seed(NO_DRAW_SEED);
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(game_random::Get_Random_Seed() == NO_DRAW_SEED);

    Reset_Technology_Player_(0);
    MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
    MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    game_random::Set_Random_Seed(NO_DRAW_SEED);
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(game_random::Get_Random_Seed() == NO_DRAW_SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_CLOAKING_DEVICE] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_STASIS_FIELD] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);

    bool ion_choices[2] = {};
    bool phasor_choices[2] = {};
    bool distortion_choices[2] = {};
    const uint32_t random_seeds[] = {1, 2};
    for (const uint32_t seed : random_seeds) {
        game_random::Set_Random_Seed(seed);
        const uint32_t ion_choice = game_random::Random(2);
        const uint32_t phasor_choice = game_random::Random(2);
        const uint32_t distortion_choice = game_random::Random(2);
        const uint32_t expected_seed = game_random::Get_Random_Seed();
        REQUIRE(ion_choice >= 1);
        REQUIRE(ion_choice <= 2);
        REQUIRE(phasor_choice >= 1);
        REQUIRE(phasor_choice <= 2);
        REQUIRE(distortion_choice >= 1);
        REQUIRE(distortion_choice <= 2);
        ion_choices[ion_choice - 1] = true;
        phasor_choices[phasor_choice - 1] = true;
        distortion_choices[distortion_choice - 1] = true;

        Reset_Technology_Player_(0);
        MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
        MOX::player[0].tech_applications[TECH_APP_EXTENDED_FUEL_TANKS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_MULTI_PHASED_SHIELDS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        game_random::Set_Random_Seed(seed);
        orion2re::lua::game::Player_Setup(&setup);

        CAPTURE(seed);
        CHECK(game_random::Get_Random_Seed() == expected_seed);
        CHECK(MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[ion_choice == 1 ? TECH_APP_ION_DRIVE
                                                     : TECH_APP_ION_PULSE_CANNON] ==
            TECH_RESEARCH_STATUS_AVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[ion_choice == 1 ? TECH_APP_ION_PULSE_CANNON
                                                     : TECH_APP_ION_DRIVE] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[TECH_APP_MULTI_PHASED_SHIELDS] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[phasor_choice == 1 ? TECH_APP_PHASOR
                                                        : TECH_APP_PHASOR_RIFLE] ==
            TECH_RESEARCH_STATUS_AVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[phasor_choice == 1 ? TECH_APP_PHASOR_RIFLE
                                                        : TECH_APP_PHASOR] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[distortion_choice == 1
                          ? TECH_APP_CLOAKING_DEVICE
                          : TECH_APP_STASIS_FIELD] ==
            TECH_RESEARCH_STATUS_AVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[distortion_choice == 1
                          ? TECH_APP_STASIS_FIELD
                          : TECH_APP_CLOAKING_DEVICE] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
    }
    CHECK(ion_choices[0]);
    CHECK(ion_choices[1]);
    CHECK(phasor_choices[0]);
    CHECK(phasor_choices[1]);
    CHECK(distortion_choices[0]);
    CHECK(distortion_choices[1]);

    const uint8_t persisted_ion_drive =
        MOX::player[0].tech_applications[TECH_APP_ION_DRIVE];
    const uint8_t persisted_ion_pulse_cannon =
        MOX::player[0].tech_applications[TECH_APP_ION_PULSE_CANNON];
    const uint8_t persisted_phasor =
        MOX::player[0].tech_applications[TECH_APP_PHASOR];
    const uint8_t persisted_phasor_rifle =
        MOX::player[0].tech_applications[TECH_APP_PHASOR_RIFLE];
    const uint8_t persisted_cloaking_device =
        MOX::player[0].tech_applications[TECH_APP_CLOAKING_DEVICE];
    const uint8_t persisted_stasis_field =
        MOX::player[0].tech_applications[TECH_APP_STASIS_FIELD];
    const uint32_t persisted_seed = game_random::Get_Random_Seed();
    orion2re::lua::Reset_Mod_Runtimes();
    orion2re::lua::s_lua_registry reconstructed_registry;
    orion2re::lua::s_lua_mod_runtime_set* reconstructed = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &reconstructed_registry,
        &reconstructed,
        &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&reconstructed);
    CHECK(game_random::Get_Random_Seed() == persisted_seed);
    CHECK(MOX::player[0].tech_applications[TECH_APP_ION_DRIVE] ==
        persisted_ion_drive);
    CHECK(MOX::player[0].tech_applications[TECH_APP_ION_PULSE_CANNON] ==
        persisted_ion_pulse_cannon);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PHASOR] ==
        persisted_phasor);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PHASOR_RIFLE] ==
        persisted_phasor_rifle);
    CHECK(MOX::player[0].tech_applications[TECH_APP_CLOAKING_DEVICE] ==
        persisted_cloaking_device);
    CHECK(MOX::player[0].tech_applications[TECH_APP_STASIS_FIELD] ==
        persisted_stasis_field);

    Shutdown_Files_(&files);
}

TEST_CASE("source-updated ICE repairs player technology choices") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_package_files files;
    Initialize_Files_(&files);
    Write_ICE_Source_Package_(files);

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    CAPTURE(diagnostic.message);
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const orion2re::lua::game::s_player_setup_event setup = {0};
    constexpr uint32_t NO_DRAW_SEED = 0x12345678;

    Reset_Technology_Player_(0);
    MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    game_random::Set_Random_Seed(NO_DRAW_SEED);
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(game_random::Get_Random_Seed() == NO_DRAW_SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] ==
        TECH_RESEARCH_STATUS_AVAILABLE);

    const std::int16_t shield_applications[] = {
        TECH_APP_CLASS_I_SHIELD,
        TECH_APP_CLASS_III_SHIELD,
        TECH_APP_CLASS_V_SHIELD,
        TECH_APP_CLASS_VII_SHIELD,
        TECH_APP_CLASS_X_SHIELD,
    };
    const uint8_t present_statuses[] = {
        TECH_RESEARCH_STATUS_AVAILABLE,
        TECH_RESEARCH_STATUS_RESEARCHED,
    };
    for (const uint8_t present_status : present_statuses) {
        for (const std::int16_t shield_application : shield_applications) {
            CAPTURE(present_status);
            CAPTURE(shield_application);
            Reset_Technology_Player_(0);
            MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
            MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] =
                TECH_RESEARCH_STATUS_AVAILABLE;
            MOX::player[0].tech_applications[shield_application] =
                present_status;
            game_random::Set_Random_Seed(NO_DRAW_SEED);
            orion2re::lua::game::Player_Setup(&setup);
            CHECK(game_random::Get_Random_Seed() == NO_DRAW_SEED);
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_SHIELD_CAPACITORS] ==
                TECH_RESEARCH_STATUS_AVAILABLE);
        }
    }

    Reset_Technology_Player_(0);
    MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
    game_random::Set_Random_Seed(NO_DRAW_SEED);
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(game_random::Get_Random_Seed() == NO_DRAW_SEED);

    game_random::Set_Random_Seed(1);
    const uint32_t hard_choice_without_fuel = game_random::Random(2);
    const uint32_t hard_seed_without_fuel = game_random::Get_Random_Seed();
    REQUIRE(hard_choice_without_fuel == 1);
    Reset_Technology_Player_(0);
    MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
    MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    game_random::Set_Random_Seed(1);
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(game_random::Get_Random_Seed() == hard_seed_without_fuel);
    CHECK(MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_CLOAKING_DEVICE] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_WARP_INTERDICTOR] ==
        TECH_RESEARCH_STATUS_AVAILABLE);

    bool capacitor_choices[2] = {};
    bool multi_phased_choices[2] = {};
    bool hard_choices[2] = {};
    const uint32_t random_seeds[] = {1, 2};
    for (const uint32_t seed : random_seeds) {
        game_random::Set_Random_Seed(seed);
        const uint32_t capacitor_choice = game_random::Random(2);
        const uint32_t multi_phased_choice = game_random::Random(2);
        const uint32_t hard_choice = game_random::Random(2);
        const uint32_t expected_seed = game_random::Get_Random_Seed();
        REQUIRE(capacitor_choice >= 1);
        REQUIRE(capacitor_choice <= 2);
        REQUIRE(multi_phased_choice >= 1);
        REQUIRE(multi_phased_choice <= 2);
        REQUIRE(hard_choice >= 1);
        REQUIRE(hard_choice <= 2);
        capacitor_choices[capacitor_choice - 1] = true;
        multi_phased_choices[multi_phased_choice - 1] = true;
        hard_choices[hard_choice - 1] = true;

        Reset_Technology_Player_(0);
        MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
        MOX::player[0].tech_applications[TECH_APP_EXTENDED_FUEL_TANKS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_MULTI_PHASED_SHIELDS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_INERTIAL_STABILIZER] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_TRACTOR_BEAM] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_STEALTH_FIELD] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_WARP_DISSIPATER] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_CLOAKING_DEVICE] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_WARP_INTERDICTOR] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        game_random::Set_Random_Seed(seed);
        orion2re::lua::game::Player_Setup(&setup);

        CAPTURE(seed);
        CHECK(game_random::Get_Random_Seed() == expected_seed);
        CHECK(MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[capacitor_choice == 1
                          ? TECH_APP_INERTIAL_STABILIZER
                          : TECH_APP_TRACTOR_BEAM] ==
            TECH_RESEARCH_STATUS_AVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[capacitor_choice == 1
                          ? TECH_APP_TRACTOR_BEAM
                          : TECH_APP_INERTIAL_STABILIZER] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[TECH_APP_MULTI_PHASED_SHIELDS] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[multi_phased_choice == 1
                          ? TECH_APP_STEALTH_FIELD
                          : TECH_APP_WARP_DISSIPATER] ==
            TECH_RESEARCH_STATUS_AVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[multi_phased_choice == 1
                          ? TECH_APP_WARP_DISSIPATER
                          : TECH_APP_STEALTH_FIELD] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[hard_choice == 1
                          ? TECH_APP_CLOAKING_DEVICE
                          : TECH_APP_WARP_INTERDICTOR] ==
            TECH_RESEARCH_STATUS_AVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[hard_choice == 1
                          ? TECH_APP_WARP_INTERDICTOR
                          : TECH_APP_CLOAKING_DEVICE] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
    }
    CHECK(capacitor_choices[0]);
    CHECK(capacitor_choices[1]);
    CHECK(multi_phased_choices[0]);
    CHECK(multi_phased_choices[1]);
    CHECK(hard_choices[0]);
    CHECK(hard_choices[1]);

    game_random::Set_Random_Seed(1);
    game_random::Random(2);
    game_random::Random(2);
    game_random::Random(2);
    const uint32_t researched_target_seed =
        game_random::Get_Random_Seed();
    Reset_Technology_Player_(0);
    MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
    MOX::player[0].tech_applications[TECH_APP_EXTENDED_FUEL_TANKS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_MULTI_PHASED_SHIELDS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_INERTIAL_STABILIZER] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_TRACTOR_BEAM] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_STEALTH_FIELD] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_WARP_DISSIPATER] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_CLOAKING_DEVICE] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    MOX::player[0].tech_applications[TECH_APP_WARP_INTERDICTOR] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    game_random::Set_Random_Seed(1);
    orion2re::lua::game::Player_Setup(&setup);
    CHECK(game_random::Get_Random_Seed() == researched_target_seed);
    CHECK(MOX::player[0].tech_applications[TECH_APP_SHIELD_CAPACITORS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0]
              .tech_applications[TECH_APP_MULTI_PHASED_SHIELDS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_HARD_SHIELDS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);

    const uint8_t persisted_inertial_stabilizer =
        MOX::player[0].tech_applications[TECH_APP_INERTIAL_STABILIZER];
    const uint8_t persisted_tractor_beam =
        MOX::player[0].tech_applications[TECH_APP_TRACTOR_BEAM];
    const uint8_t persisted_stealth_field =
        MOX::player[0].tech_applications[TECH_APP_STEALTH_FIELD];
    const uint8_t persisted_warp_dissipator =
        MOX::player[0].tech_applications[TECH_APP_WARP_DISSIPATER];
    const uint8_t persisted_cloaking_device =
        MOX::player[0].tech_applications[TECH_APP_CLOAKING_DEVICE];
    const uint8_t persisted_warp_interdictor =
        MOX::player[0].tech_applications[TECH_APP_WARP_INTERDICTOR];
    const uint32_t persisted_seed = game_random::Get_Random_Seed();
    orion2re::lua::Reset_Mod_Runtimes();
    orion2re::lua::s_lua_registry reconstructed_registry;
    orion2re::lua::s_lua_mod_runtime_set* reconstructed = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &reconstructed_registry,
        &reconstructed,
        &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&reconstructed);
    CHECK(game_random::Get_Random_Seed() == persisted_seed);
    CHECK(MOX::player[0].tech_applications[TECH_APP_INERTIAL_STABILIZER] ==
        persisted_inertial_stabilizer);
    CHECK(MOX::player[0].tech_applications[TECH_APP_TRACTOR_BEAM] ==
        persisted_tractor_beam);
    CHECK(MOX::player[0].tech_applications[TECH_APP_STEALTH_FIELD] ==
        persisted_stealth_field);
    CHECK(MOX::player[0].tech_applications[TECH_APP_WARP_DISSIPATER] ==
        persisted_warp_dissipator);
    CHECK(MOX::player[0].tech_applications[TECH_APP_CLOAKING_DEVICE] ==
        persisted_cloaking_device);
    CHECK(MOX::player[0].tech_applications[TECH_APP_WARP_INTERDICTOR] ==
        persisted_warp_interdictor);

    Shutdown_Files_(&files);
}

TEST_CASE("source-updated ICE repairs homeworld state") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_package_files files;
    Initialize_Files_(&files);
    Write_ICE_Source_Package_(files);

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    CAPTURE(diagnostic.message);
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const orion2re::lua::game::s_home_system_generated_event event = {0, 0};
    const auto reset_homeworld = []() {
        memset(MOX::player[0].traits, 0, sizeof(MOX::player[0].traits));
        MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size = PLANET_SIZE_MEDIUM;
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate = PLANET_CLIMATE_TERRAN;
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).food_per_farmer =
            MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN];
        for (auto& [id, planet] : game::world.planets) {
            if (planet.star_index == 0) planet.planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
        }
        MOX::star[0].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
    };
    const auto dispatch = [&]() {
        orion2re::lua::game::Home_System_Generated(&::game::world, &event);
    };

    reset_homeworld();
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_LARGE);

    reset_homeworld();
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::player[0].traits[TRAIT_LARGE_HOME_WORLD] = 1;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_HUGE);

    reset_homeworld();
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_GOLD_DEPOSITS);
    CHECK(MOX::star[0].system_special == SYSTEM_SPECIAL_GOLD_DEPOSITS);

    reset_homeworld();
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    MOX::player[0].traits[TRAIT_ANCIENT_HOME_WORLD] = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
    MOX::star[0].system_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_ORION_STAR);
    CHECK(MOX::star[0].system_special == SYSTEM_SPECIAL_ORION_STAR);

    reset_homeworld();
    MOX::player[0].objectives = PLAYER_OBJECTIVE_EXPANSIONIST;
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::player[0].traits[TRAIT_LARGE_HOME_WORLD] = 1;
    MOX::player[0].traits[TRAIT_LOW_G_WORLD] = 1;
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_SMALL);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate == PLANET_CLIMATE_GAIA);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).food_per_farmer ==
        MOX::food_per_farmer_table[PLANET_CLIMATE_GAIA]);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_GOLD_DEPOSITS);

    const int8_t persisted_size = test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size;
    const int8_t persisted_climate = test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate;
    const uint8_t persisted_food_per_farmer = test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).food_per_farmer;
    const int8_t persisted_special = test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special;
    const int8_t persisted_system_special = MOX::star[0].system_special;
    MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    orion2re::lua::Reset_Mod_Runtimes();
    orion2re::lua::s_lua_registry reconstructed_registry;
    orion2re::lua::s_lua_mod_runtime_set* reconstructed = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &reconstructed_registry,
        &reconstructed,
        &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&reconstructed);
    std::string loaded_diagnostic;
    REQUIRE(orion2re::lua::Dispatch_Active_Loaded_Event(
        &loaded_diagnostic));
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == persisted_size);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate == persisted_climate);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).food_per_farmer == persisted_food_per_farmer);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == persisted_special);
    CHECK(MOX::star[0].system_special == persisted_system_special);

    reset_homeworld();
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::game_type = 2;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_LARGE);
    MOX::game_type = 3;
    reset_homeworld();
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_MEDIUM);

    Shutdown_Files_(&files);
}

TEST_CASE("source-updated VDC repairs home-system state") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_package_files files;
    Initialize_Files_(&files);
    Write_VDC_Source_Package_(files);

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    CAPTURE(diagnostic.message);
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const orion2re::lua::game::s_home_system_generated_event event = {0, 0};
    const auto reset_home_system = []() {
        memset(MOX::player[0].traits, 0, sizeof(MOX::player[0].traits));
        MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size = PLANET_SIZE_MEDIUM;
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate = PLANET_CLIMATE_TERRAN;
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(1))).size = PLANET_SIZE_LARGE;
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(1))).climate = PLANET_CLIMATE_SWAMP;
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).size = PLANET_SIZE_SMALL;
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).climate = PLANET_CLIMATE_TOXIC;
        for (auto& [id, planet] : game::world.planets) {
            planet.planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
        }
        for (std::int16_t system_id = 0;
             system_id < MOX::NUM_STARS;
             ++system_id) {
            MOX::star[system_id].system_special =
                SYSTEM_SPECIAL_NO_SPECIAL;
        }
    };
    const auto dispatch = [&]() {
        orion2re::lua::game::Home_System_Generated(&::game::world, &event);
    };

    reset_home_system();
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_MEDIUM);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate == PLANET_CLIMATE_TERRAN);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_NO_SPECIAL);

    reset_home_system();
    MOX::player[0].objectives = PLAYER_OBJECTIVE_EXPANSIONIST;
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate = PLANET_CLIMATE_SWAMP;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).climate = PLANET_CLIMATE_SWAMP;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_HUGE);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).size == PLANET_SIZE_HUGE);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(1))).size == PLANET_SIZE_LARGE);

    reset_home_system();
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).climate = PLANET_CLIMATE_SWAMP;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_MEDIUM);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).size == PLANET_SIZE_HUGE);

    reset_home_system();
    MOX::player[0].traits[TRAIT_LARGE_HOME_WORLD] = 1;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_MEDIUM);

    reset_home_system();
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::player[0].traits[TRAIT_LARGE_HOME_WORLD] = 1;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_HUGE);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).size == PLANET_SIZE_SMALL);

    for (int tolerant = 0; tolerant <= 1; ++tolerant) {
        for (int subterranean = 0; subterranean <= 1; ++subterranean) {
            reset_home_system();
            MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] =
                static_cast<std::int8_t>(tolerant);
            MOX::player[0].traits[TRAIT_SUBTERRANEAN] =
                static_cast<std::int8_t>(subterranean);
            dispatch();
            CAPTURE(tolerant);
            CAPTURE(subterranean);
            CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate ==
                (tolerant != 0 && subterranean != 0
                        ? PLANET_CLIMATE_GAIA
                        : PLANET_CLIMATE_TERRAN));
        }
    }

    reset_home_system();
    MOX::player[0].traits[TRAIT_ANCIENT_HOME_WORLD] = 1;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_NO_SPECIAL);

    reset_home_system();
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_GOLD_DEPOSITS);
    CHECK(MOX::star[0].system_special == SYSTEM_SPECIAL_GOLD_DEPOSITS);

    reset_home_system();
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    MOX::player[0].traits[TRAIT_ANCIENT_HOME_WORLD] = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
    MOX::star[0].system_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_ORION_STAR);
    CHECK(MOX::star[0].system_special == SYSTEM_SPECIAL_ORION_STAR);

    reset_home_system();
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::player[0].traits[TRAIT_LARGE_HOME_WORLD] = 1;
    MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] = 1;
    MOX::player[0].traits[TRAIT_SUBTERRANEAN] = 1;
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    MOX::player[0].traits[TRAIT_ANCIENT_HOME_WORLD] = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate = PLANET_CLIMATE_SWAMP;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).climate = PLANET_CLIMATE_SWAMP;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_HUGE);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate == PLANET_CLIMATE_GAIA);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_ORION_STAR);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).size == PLANET_SIZE_HUGE);

    const int8_t persisted_size = test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size;
    const int8_t persisted_climate = test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate;
    const int8_t persisted_special = test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special;
    const int8_t persisted_system_special = MOX::star[0].system_special;
    memset(MOX::player[0].traits, 0, sizeof(MOX::player[0].traits));
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    orion2re::lua::Reset_Mod_Runtimes();
    orion2re::lua::s_lua_registry reconstructed_registry;
    orion2re::lua::s_lua_mod_runtime_set* reconstructed = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &reconstructed_registry,
        &reconstructed,
        &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&reconstructed);
    std::string loaded_diagnostic;
    REQUIRE(orion2re::lua::Dispatch_Active_Loaded_Event(
        &loaded_diagnostic));
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == persisted_size);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate == persisted_climate);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == persisted_special);
    CHECK(MOX::star[0].system_special == persisted_system_special);

    reset_home_system();
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::player[0].traits[TRAIT_LARGE_HOME_WORLD] = 1;
    MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] = 1;
    MOX::player[0].traits[TRAIT_SUBTERRANEAN] = 1;
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).climate = PLANET_CLIMATE_SWAMP;
    MOX::game_type = 2;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_HUGE);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate == PLANET_CLIMATE_GAIA);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_GOLD_DEPOSITS);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).size == PLANET_SIZE_HUGE);
    MOX::game_type = 3;
    reset_home_system();
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::player[0].traits[TRAIT_LARGE_HOME_WORLD] = 1;
    MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] = 1;
    MOX::player[0].traits[TRAIT_SUBTERRANEAN] = 1;
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).climate = PLANET_CLIMATE_SWAMP;
    dispatch();
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).size == PLANET_SIZE_MEDIUM);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate == PLANET_CLIMATE_TERRAN);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).planet_special == SYSTEM_SPECIAL_NO_SPECIAL);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).size == PLANET_SIZE_SMALL);

    Shutdown_Files_(&files);
}

TEST_CASE("VDC home-system state is observed by planet generated event") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_package_files files;
    Initialize_Files_(&files);
    Write_VDC_Home_System_Interaction_Fixture_(files);

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    CAPTURE(diagnostic.message);
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] = 1;
    MOX::player[0].traits[TRAIT_SUBTERRANEAN] = 1;
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).climate = PLANET_CLIMATE_SWAMP;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).mineral_class = MINERAL_RESOURCE_POOR;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).gravity_class = PLANET_GRAVITY_LOW_G;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(2))).planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
    MOX::star[0].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
    const orion2re::lua::game::s_home_system_generated_event event = {0, 0};
    orion2re::lua::game::Home_System_Generated(&::game::world, &event);

    const orion2re::lua::game::s_planet_generated_event planet_event = {0};
    orion2re::lua::game::Planet_Generated(&::game::world, &planet_event);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).mineral_class == MINERAL_RESOURCE_RICH);
    CHECK(test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).gravity_class == PLANET_GRAVITY_NORMAL_G);

    Shutdown_Files_(&files);
}

TEST_CASE("source-updated VDC repairs player technology choices") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    s_package_files files;
    Initialize_Files_(&files);
    Write_VDC_Source_Package_(files);

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    CAPTURE(diagnostic.message);
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    const orion2re::lua::game::s_player_setup_event setup = {
        static_cast<std::int16_t>(MOX::NUM_PLAYERS - 1),
    };
    constexpr uint32_t SEED = 0x12345678;
    const auto dispatch = [&](uint32_t seed) {
        game_random::Set_Random_Seed(seed);
        orion2re::lua::game::Player_Setup(&setup);
    };

    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].objectives = PLAYER_OBJECTIVE_EXPANSIONIST;
    MOX::player[0].tech_applications[TECH_APP_PLANETARY_SUPERCOMPUTER] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    dispatch(SEED);
    CHECK(game_random::Get_Random_Seed() == SEED);
    CHECK(MOX::player[0]
              .tech_applications[TECH_APP_PLANETARY_SUPERCOMPUTER] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].traits[TRAIT_UNCREATIVE] = 0;
    MOX::player[0].tech_applications[TECH_APP_PLANETARY_SUPERCOMPUTER] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    dispatch(SEED);
    CHECK(game_random::Get_Random_Seed() == SEED);
    CHECK(MOX::player[0]
              .tech_applications[TECH_APP_PLANETARY_SUPERCOMPUTER] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);

    Block_VDC_Technology_Repairs_(0);
    dispatch(SEED);
    CHECK(game_random::Get_Random_Seed() == SEED);
    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    dispatch(SEED);
    CHECK(game_random::Get_Random_Seed() == SEED);

    bool armor_choices[3] = {};
    const uint32_t armor_seeds[] = {1, 2, 3};
    const std::int16_t armor_applications[] = {
        TECH_APP_ZORTRIUM_ARMOR,
        TECH_APP_NEUTRONIUM_ARMOR,
        TECH_APP_ADAMANTIUM_ARMOR,
    };
    for (const uint32_t seed : armor_seeds) {
        game_random::Set_Random_Seed(seed);
        const uint32_t choice = game_random::Random(3);
        armor_choices[choice - 1] = true;
        Block_VDC_Technology_Repairs_(0);
        MOX::player[0].tech_applications[TECH_APP_ZORTRIUM_ARMOR] =
            TECH_RESEARCH_STATUS_UNAVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_NANO_DISASSEMBLERS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_ZEON_MISSILE] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_MICROLITE_CONSTRUCTION] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_URRIDIUM_FUEL_CELLS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_THORIUM_FUEL_CELLS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        dispatch(seed);
        CAPTURE(seed);
        CHECK(game_random::Get_Random_Seed() == seed);
        for (std::size_t index = 0; index < std::size(armor_applications);
             ++index) {
            CHECK(MOX::player[0]
                      .tech_applications[armor_applications[index]] ==
                (index == choice - 1 ? TECH_RESEARCH_STATUS_AVAILABLE
                                     : TECH_RESEARCH_STATUS_UNAVAILABLE));
        }
        if (choice == 1) {
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_NANO_DISASSEMBLERS] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE);
            CHECK(MOX::player[0].tech_applications[TECH_APP_ZEON_MISSILE] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE);
        } else if (choice == 2) {
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_MICROLITE_CONSTRUCTION] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE);
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_URRIDIUM_FUEL_CELLS] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE);
        } else {
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_THORIUM_FUEL_CELLS] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE);
        }
    }
    CHECK(armor_choices[0]);
    CHECK(armor_choices[1]);
    CHECK(armor_choices[2]);

    const uint8_t present_statuses[] = {
        TECH_RESEARCH_STATUS_AVAILABLE,
        TECH_RESEARCH_STATUS_RESEARCHED,
    };
    for (const uint8_t status : present_statuses) {
        for (const std::int16_t application : armor_applications) {
            Block_VDC_Technology_Repairs_(0);
            MOX::player[0].tech_applications[TECH_APP_ZORTRIUM_ARMOR] =
                TECH_RESEARCH_STATUS_UNAVAILABLE;
            MOX::player[0].tech_applications[application] = status;
            dispatch(SEED);
            CAPTURE(status);
            CAPTURE(application);
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_NEUTRONIUM_ARMOR] ==
                (application == TECH_APP_NEUTRONIUM_ARMOR
                        ? status
                        : TECH_RESEARCH_STATUS_UNAVAILABLE));
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_ADAMANTIUM_ARMOR] ==
                (application == TECH_APP_ADAMANTIUM_ARMOR
                        ? status
                        : TECH_RESEARCH_STATUS_UNAVAILABLE));
        }
    }

    struct s_unavailable_repair {
        std::int16_t application;
        std::int16_t cleared_first;
        std::int16_t cleared_second;
    };
    const s_unavailable_repair unavailable_repairs[] = {
        {TECH_APP_PLANETARY_SUPERCOMPUTER,
            TECH_APP_OPTRONIC_COMPUTER,
            TECH_APP_VIRTUAL_REALITY_NETWORK},
        {TECH_APP_EVOLUTIONARY_MUTATION, TECH_APP_ANDROID_WORKERS, -1},
        {TECH_APP_ANTI_MATTER_BOMB,
            TECH_APP_NEUTRON_BLASTER,
            TECH_APP_NEUTRON_SCANNER},
        {TECH_APP_CLASS_V_SHIELD,
            TECH_APP_GAUSS_CANNON,
            TECH_APP_WIDE_AREA_JAMMER},
        {TECH_APP_HARD_SHIELDS, TECH_APP_REFLECTION_FIELD, -1},
        {TECH_APP_HOLO_SIMULATOR,
            TECH_APP_DAUNTLESS_GUIDANCE_SYSTEM,
            TECH_APP_NEURAL_SCANNER},
        {TECH_APP_HEIGHTENED_INTELLIGENCE,
            TECH_APP_ANDROID_FARMERS,
            TECH_APP_ANDROID_SCIENTISTS},
        {TECH_APP_CLONING_CENTER, TECH_APP_SUBTERRANEAN_FARMS, -1},
        {TECH_APP_SPACEPORT, TECH_APP_ALIEN_MANAGEMENT_CENTER, -1},
    };
    for (const s_unavailable_repair& repair : unavailable_repairs) {
        Block_VDC_Technology_Repairs_(0);
        MOX::player[0].tech_applications[repair.application] =
            TECH_RESEARCH_STATUS_UNAVAILABLE;
        MOX::player[0].tech_applications[repair.cleared_first] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        if (repair.cleared_second != -1) {
            MOX::player[0].tech_applications[repair.cleared_second] =
                TECH_RESEARCH_STATUS_AVAILABLE;
        }
        dispatch(SEED);
        CAPTURE(repair.application);
        CHECK(MOX::player[0].tech_applications[repair.application] ==
            TECH_RESEARCH_STATUS_AVAILABLE);
        CHECK(MOX::player[0].tech_applications[repair.cleared_first] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        if (repair.cleared_second != -1) {
            CHECK(MOX::player[0].tech_applications[repair.cleared_second] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE);
        }
        for (const uint8_t status : present_statuses) {
            Block_VDC_Technology_Repairs_(0);
            MOX::player[0].tech_applications[repair.application] = status;
            MOX::player[0].tech_applications[repair.cleared_first] =
                TECH_RESEARCH_STATUS_AVAILABLE;
            dispatch(SEED);
            CHECK(MOX::player[0].tech_applications[repair.application] ==
                status);
            CHECK(MOX::player[0].tech_applications[repair.cleared_first] ==
                TECH_RESEARCH_STATUS_AVAILABLE);
        }
    }

    const std::int16_t computer_applications[] = {
        TECH_APP_POSITRONIC_COMPUTER,
        TECH_APP_CYBERTRONIC_COMPUTER,
        TECH_APP_MOLECULARTRONIC_COMPUTER,
    };
    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].tech_applications[TECH_APP_POSITRONIC_COMPUTER] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_ACHILLES_TARGETING_UNIT] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    dispatch(SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_CYBERTRONIC_COMPUTER] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    CHECK(MOX::player[0]
              .tech_applications[TECH_APP_ACHILLES_TARGETING_UNIT] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    for (const uint8_t status : present_statuses) {
        for (const std::int16_t application : computer_applications) {
            Block_VDC_Technology_Repairs_(0);
            MOX::player[0].tech_applications[TECH_APP_POSITRONIC_COMPUTER] =
                TECH_RESEARCH_STATUS_UNAVAILABLE;
            MOX::player[0].tech_applications[application] = status;
            MOX::player[0]
                .tech_applications[TECH_APP_ACHILLES_TARGETING_UNIT] =
                TECH_RESEARCH_STATUS_AVAILABLE;
            dispatch(SEED);
            CAPTURE(status);
            CAPTURE(application);
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_ACHILLES_TARGETING_UNIT] ==
                TECH_RESEARCH_STATUS_AVAILABLE);
        }
    }

    const uint8_t exact_statuses[] = {
        TECH_RESEARCH_STATUS_UNAVAILABLE,
        TECH_RESEARCH_STATUS_AVAILABLE,
        TECH_RESEARCH_STATUS_RESEARCHED,
    };
    for (const uint8_t bomber_status : exact_statuses) {
        for (const uint8_t fighter_status : exact_statuses) {
            Block_VDC_Technology_Repairs_(0);
            MOX::player[0].tech_applications[TECH_APP_BOMBER_BAYS] =
                bomber_status;
            MOX::player[0].tech_applications[TECH_APP_HEAVY_FIGHTER_BAYS] =
                fighter_status;
            dispatch(SEED);
            const bool repaired =
                bomber_status == TECH_RESEARCH_STATUS_AVAILABLE &&
                fighter_status == TECH_RESEARCH_STATUS_AVAILABLE;
            CAPTURE(bomber_status);
            CAPTURE(fighter_status);
            CHECK(MOX::player[0].tech_applications[TECH_APP_BOMBER_BAYS] ==
                bomber_status);
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_HEAVY_FIGHTER_BAYS] ==
                (repaired ? TECH_RESEARCH_STATUS_UNAVAILABLE : fighter_status));
            CHECK(MOX::player[0].tech_applications[TECH_APP_POWERED_ARMOR] ==
                (repaired ? TECH_RESEARCH_STATUS_AVAILABLE
                          : TECH_RESEARCH_STATUS_UNAVAILABLE));
        }
    }

    const std::int16_t torpedoes[] = {
        TECH_APP_ANTI_MATTER_TORPEDOES,
        TECH_APP_PROTON_TORPEDOES,
        TECH_APP_PLASMA_TORPEDOES,
    };
    for (unsigned int mask = 0; mask < 8; ++mask) {
        Block_VDC_Technology_Repairs_(0);
        for (std::size_t index = 0; index < std::size(torpedoes); ++index) {
            MOX::player[0].tech_applications[torpedoes[index]] =
                (mask & (1u << index)) != 0
                ? TECH_RESEARCH_STATUS_AVAILABLE
                : TECH_RESEARCH_STATUS_UNAVAILABLE;
        }
        MOX::player[0].tech_applications[TECH_APP_NEUTRONIUM_BOMB] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_PARTICLE_BEAM] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        dispatch(SEED);
        const bool repaired =
            ((mask & 1u) != 0) + ((mask & 2u) != 0) + ((mask & 4u) != 0) >= 2;
        CAPTURE(mask);
        CHECK(MOX::player[0].tech_applications[TECH_APP_DISRUPTER_CANNON] ==
            (repaired ? TECH_RESEARCH_STATUS_UNAVAILABLE
                      : TECH_RESEARCH_STATUS_AVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_NEUTRONIUM_BOMB] ==
            (repaired ? TECH_RESEARCH_STATUS_UNAVAILABLE
                      : TECH_RESEARCH_STATUS_AVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_PARTICLE_BEAM] ==
            (repaired ? TECH_RESEARCH_STATUS_UNAVAILABLE
                      : TECH_RESEARCH_STATUS_AVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_SENSORS] ==
            (repaired ? TECH_RESEARCH_STATUS_AVAILABLE
                      : TECH_RESEARCH_STATUS_UNAVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_MAULER_DEVICE] ==
            (repaired ? TECH_RESEARCH_STATUS_AVAILABLE
                      : TECH_RESEARCH_STATUS_UNAVAILABLE));
    }
    for (std::size_t researched = 0; researched < std::size(torpedoes);
         ++researched) {
        Block_VDC_Technology_Repairs_(0);
        for (const std::int16_t torpedo : torpedoes) {
            MOX::player[0].tech_applications[torpedo] =
                TECH_RESEARCH_STATUS_AVAILABLE;
        }
        MOX::player[0].tech_applications[torpedoes[researched]] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        dispatch(SEED);
        CHECK(MOX::player[0].tech_applications[TECH_APP_SENSORS] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
    }

    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].tech_applications[TECH_APP_DISRUPTER_CANNON] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_PHASE_SHIFTER] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_DEATH_SPORES] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    dispatch(SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_MAULER_DEVICE] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PHASE_SHIFTER] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_DEATH_SPORES] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    const std::int16_t beam_weapons[] = {
        TECH_APP_DISRUPTER_CANNON,
        TECH_APP_DEATH_RAY,
        TECH_APP_MAULER_DEVICE,
    };
    for (const uint8_t status : present_statuses) {
        for (const std::int16_t application : beam_weapons) {
            Block_VDC_Technology_Repairs_(0);
            MOX::player[0].tech_applications[TECH_APP_DISRUPTER_CANNON] =
                TECH_RESEARCH_STATUS_UNAVAILABLE;
            MOX::player[0].tech_applications[application] = status;
            MOX::player[0].tech_applications[TECH_APP_PHASE_SHIFTER] =
                TECH_RESEARCH_STATUS_AVAILABLE;
            dispatch(SEED);
            CAPTURE(status);
            CAPTURE(application);
            CHECK(MOX::player[0].tech_applications[TECH_APP_PHASE_SHIFTER] ==
                TECH_RESEARCH_STATUS_AVAILABLE);
        }
    }

    const std::int16_t missiles[] = {
        TECH_APP_MERCULITE_MISSILE,
        TECH_APP_PULSON_MISSILE,
        TECH_APP_ZEON_MISSILE,
    };
    for (unsigned int mask = 0; mask < 8; ++mask) {
        Block_VDC_Technology_Repairs_(0);
        for (std::size_t index = 0; index < std::size(missiles); ++index) {
            MOX::player[0].tech_applications[missiles[index]] =
                (mask & (1u << index)) != 0
                ? TECH_RESEARCH_STATUS_AVAILABLE
                : TECH_RESEARCH_STATUS_UNAVAILABLE;
        }
        MOX::player[0].tech_applications[TECH_APP_NEUTRON_BLASTER] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_FUSION_BEAM] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_FUSION_RIFLE] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        dispatch(SEED);
        const bool zeon_repair =
            (mask & 4u) != 0 && (mask & 3u) != 0;
        const bool fusion_repair = (mask & 7u) == 3u;
        CAPTURE(mask);
        CHECK(MOX::player[0].tech_applications[TECH_APP_ANTI_MATTER_BOMB] ==
            (zeon_repair || fusion_repair ? TECH_RESEARCH_STATUS_UNAVAILABLE
                                          : TECH_RESEARCH_STATUS_AVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_NEUTRON_BLASTER] ==
            (zeon_repair || fusion_repair
                    ? TECH_RESEARCH_STATUS_UNAVAILABLE
                    : TECH_RESEARCH_STATUS_AVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_NEUTRON_SCANNER] ==
            (zeon_repair || fusion_repair ? TECH_RESEARCH_STATUS_AVAILABLE
                                          : TECH_RESEARCH_STATUS_UNAVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_FUSION_BOMB] ==
            (fusion_repair ? TECH_RESEARCH_STATUS_AVAILABLE
                           : TECH_RESEARCH_STATUS_UNAVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_FUSION_BEAM] ==
            (fusion_repair ? TECH_RESEARCH_STATUS_UNAVAILABLE
                           : TECH_RESEARCH_STATUS_AVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_FUSION_RIFLE] ==
            (fusion_repair ? TECH_RESEARCH_STATUS_UNAVAILABLE
                           : TECH_RESEARCH_STATUS_AVAILABLE));
    }
    for (std::size_t researched = 0; researched < std::size(missiles);
         ++researched) {
        Block_VDC_Technology_Repairs_(0);
        for (const std::int16_t missile : missiles) {
            MOX::player[0].tech_applications[missile] =
                TECH_RESEARCH_STATUS_AVAILABLE;
        }
        MOX::player[0].tech_applications[missiles[researched]] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        dispatch(SEED);
        CHECK(MOX::player[0].tech_applications[TECH_APP_NEUTRON_SCANNER] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
    }

    for (const GOVERNMENT government :
        {GOVERNMENT_FEDERATION, GOVERNMENT_UNIFICATION}) {
        Block_VDC_Technology_Repairs_(0);
        MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = government;
        MOX::player[0].tech_applications[TECH_APP_HOLO_SIMULATOR] =
            TECH_RESEARCH_STATUS_UNAVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_HEIGHTENED_INTELLIGENCE] =
            TECH_RESEARCH_STATUS_UNAVAILABLE;
        dispatch(SEED);
        const uint8_t expected = government == GOVERNMENT_FEDERATION
            ? TECH_RESEARCH_STATUS_AVAILABLE
            : TECH_RESEARCH_STATUS_UNAVAILABLE;
        CAPTURE(government);
        CHECK(MOX::player[0].tech_applications[TECH_APP_HOLO_SIMULATOR] ==
            expected);
        CHECK(MOX::player[0]
                  .tech_applications[TECH_APP_HEIGHTENED_INTELLIGENCE] ==
            expected);
    }

    bool gravity_choices[2] = {};
    const uint32_t gravity_seeds[] = {1, 2};
    for (const uint32_t seed : gravity_seeds) {
        game_random::Set_Random_Seed(seed);
        game_random::Random(3);
        const uint32_t choice = game_random::Random(2);
        gravity_choices[choice - 1] = true;
        Block_VDC_Technology_Repairs_(0);
        MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
        MOX::player[0]
            .tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_ANTI_GRAV_HARNESS] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        MOX::player[0].tech_applications[TECH_APP_GRAVITON_BEAM] =
            TECH_RESEARCH_STATUS_AVAILABLE;
        dispatch(seed);
        CAPTURE(seed);
        CHECK(game_random::Get_Random_Seed() == seed);
        CHECK(MOX::player[0]
                  .tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] ==
            TECH_RESEARCH_STATUS_UNAVAILABLE);
        CHECK(MOX::player[0]
                  .tech_applications[TECH_APP_ANTI_GRAV_HARNESS] ==
            (choice == 1 ? TECH_RESEARCH_STATUS_AVAILABLE
                         : TECH_RESEARCH_STATUS_UNAVAILABLE));
        CHECK(MOX::player[0].tech_applications[TECH_APP_GRAVITON_BEAM] ==
            (choice == 2 ? TECH_RESEARCH_STATUS_AVAILABLE
                         : TECH_RESEARCH_STATUS_UNAVAILABLE));
    }
    CHECK(gravity_choices[0]);
    CHECK(gravity_choices[1]);
    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::player[0]
        .tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    dispatch(SEED);
    CHECK(game_random::Get_Random_Seed() == SEED);
    CHECK(MOX::player[0]
              .tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] ==
        TECH_RESEARCH_STATUS_RESEARCHED);

    const std::int16_t pollution_applications[] = {
        TECH_APP_POLLUTION_PROCESSOR,
        TECH_APP_ATMOSPHERIC_RENEWER,
        TECH_APP_NANO_DISASSEMBLERS,
    };
    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].tech_applications[TECH_APP_POLLUTION_PROCESSOR] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_BATTLESTATION] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_TITAN_CONSTRUCTION] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    dispatch(SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_CORE_WASTE_DUMPS] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_BATTLESTATION] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_TITAN_CONSTRUCTION] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    for (const uint8_t status : present_statuses) {
        for (const std::int16_t application : pollution_applications) {
            Block_VDC_Technology_Repairs_(0);
            MOX::player[0].tech_applications[TECH_APP_POLLUTION_PROCESSOR] =
                TECH_RESEARCH_STATUS_UNAVAILABLE;
            MOX::player[0].tech_applications[application] = status;
            dispatch(SEED);
            CAPTURE(status);
            CAPTURE(application);
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_CORE_WASTE_DUMPS] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE);
        }
    }
    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].traits[TRAIT_ENVIRONMENT_IMMUNE] = 1;
    MOX::player[0].tech_applications[TECH_APP_POLLUTION_PROCESSOR] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    dispatch(SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_CORE_WASTE_DUMPS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);

    const std::int16_t farming_applications[] = {
        TECH_APP_SOIL_ENRICHMENT,
        TECH_APP_WEATHER_CONTROL_SYSTEM,
        TECH_APP_ANDROID_FARMERS,
    };
    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].tech_applications[TECH_APP_SOIL_ENRICHMENT] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_MICROBIOTICS] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    dispatch(SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_SOIL_ENRICHMENT] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_MICROBIOTICS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    for (const uint8_t status : present_statuses) {
        for (const std::int16_t application : farming_applications) {
            Block_VDC_Technology_Repairs_(0);
            MOX::player[0].tech_applications[TECH_APP_SOIL_ENRICHMENT] =
                TECH_RESEARCH_STATUS_UNAVAILABLE;
            MOX::player[0].tech_applications[application] = status;
            MOX::player[0].tech_applications[TECH_APP_MICROBIOTICS] =
                TECH_RESEARCH_STATUS_AVAILABLE;
            dispatch(SEED);
            CAPTURE(status);
            CAPTURE(application);
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_MICROBIOTICS] ==
                TECH_RESEARCH_STATUS_AVAILABLE);
        }
    }

    for (const uint8_t weather_status : exact_statuses) {
        for (const uint8_t antidote_status : exact_statuses) {
            Block_VDC_Technology_Repairs_(0);
            MOX::player[0].traits[TRAIT_EATS_MINERALS] = 1;
            MOX::player[0]
                .tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] =
                weather_status;
            MOX::player[0].tech_applications[TECH_APP_UNIVERSAL_ANTIDOTE] =
                antidote_status;
            dispatch(SEED);
            const bool repaired =
                (weather_status == TECH_RESEARCH_STATUS_AVAILABLE &&
                    antidote_status == TECH_RESEARCH_STATUS_UNAVAILABLE) ||
                (weather_status == TECH_RESEARCH_STATUS_UNAVAILABLE &&
                    antidote_status == TECH_RESEARCH_STATUS_AVAILABLE);
            CAPTURE(weather_status);
            CAPTURE(antidote_status);
            CHECK(MOX::player[0].tech_applications[TECH_APP_PSIONICS] ==
                (repaired ? TECH_RESEARCH_STATUS_AVAILABLE
                          : TECH_RESEARCH_STATUS_UNAVAILABLE));
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] ==
                (repaired ? TECH_RESEARCH_STATUS_UNAVAILABLE : weather_status));
            CHECK(MOX::player[0]
                      .tech_applications[TECH_APP_UNIVERSAL_ANTIDOTE] ==
                (repaired ? TECH_RESEARCH_STATUS_UNAVAILABLE : antidote_status));
        }
    }

    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].tech_applications[TECH_APP_ANTI_MATTER_BOMB] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_PULSON_MISSILE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_ZEON_MISSILE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    dispatch(SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_ANTI_MATTER_BOMB] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_NEUTRON_SCANNER] ==
        TECH_RESEARCH_STATUS_AVAILABLE);

    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].tech_applications[TECH_APP_ZORTRIUM_ARMOR] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_POLLUTION_PROCESSOR] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    dispatch(1);
    CHECK(MOX::player[0].tech_applications[TECH_APP_NANO_DISASSEMBLERS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_CORE_WASTE_DUMPS] ==
        TECH_RESEARCH_STATUS_AVAILABLE);

    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].tech_applications[TECH_APP_HEIGHTENED_INTELLIGENCE] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_SOIL_ENRICHMENT] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    dispatch(SEED);
    CHECK(MOX::player[0].tech_applications[TECH_APP_ANDROID_FARMERS] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    CHECK(MOX::player[0].tech_applications[TECH_APP_SOIL_ENRICHMENT] ==
        TECH_RESEARCH_STATUS_AVAILABLE);

    Block_VDC_Technology_Repairs_(0);
    MOX::player[0].traits[TRAIT_HEAVY_G_WORLD] = 1;
    MOX::player[0].traits[TRAIT_EATS_MINERALS] = 1;
    MOX::player[0].tech_applications[TECH_APP_ZORTRIUM_ARMOR] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    MOX::player[0]
        .tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_PULSON_MISSILE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_ZEON_MISSILE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    MOX::player[0].tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    dispatch(2);
    const uint8_t persisted_neutronium =
        MOX::player[0].tech_applications[TECH_APP_NEUTRONIUM_ARMOR];
    const uint8_t persisted_harness =
        MOX::player[0].tech_applications[TECH_APP_ANTI_GRAV_HARNESS];
    const uint8_t persisted_anti_matter_bomb =
        MOX::player[0].tech_applications[TECH_APP_ANTI_MATTER_BOMB];
    const uint8_t persisted_neutron_scanner =
        MOX::player[0].tech_applications[TECH_APP_NEUTRON_SCANNER];
    const uint8_t persisted_weather =
        MOX::player[0].tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM];
    const uint8_t persisted_psionics =
        MOX::player[0].tech_applications[TECH_APP_PSIONICS];
    const uint32_t persisted_seed = game_random::Get_Random_Seed();
    orion2re::lua::Reset_Mod_Runtimes();
    orion2re::lua::s_lua_registry reconstructed_registry;
    orion2re::lua::s_lua_mod_runtime_set* reconstructed = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(&packages,
        &limits,
        &reconstructed_registry,
        &reconstructed,
        &diagnostic));
    orion2re::lua::Commit_Mod_Runtimes(&reconstructed);
    CHECK(game_random::Get_Random_Seed() == persisted_seed);
    CHECK(MOX::player[0].tech_applications[TECH_APP_NEUTRONIUM_ARMOR] ==
        persisted_neutronium);
    CHECK(MOX::player[0].tech_applications[TECH_APP_ANTI_GRAV_HARNESS] ==
        persisted_harness);
    CHECK(MOX::player[0].tech_applications[TECH_APP_ANTI_MATTER_BOMB] ==
        persisted_anti_matter_bomb);
    CHECK(MOX::player[0].tech_applications[TECH_APP_NEUTRON_SCANNER] ==
        persisted_neutron_scanner);
    CHECK(MOX::player[0]
              .tech_applications[TECH_APP_WEATHER_CONTROL_SYSTEM] ==
        persisted_weather);
    CHECK(MOX::player[0].tech_applications[TECH_APP_PSIONICS] ==
        persisted_psionics);

    auto* runtime = const_cast<orion2re::lua::s_lua_mod_runtime*>(
        orion2re::lua::Find_Mod_Runtime(
            orion2re::lua::Get_Active_Mod_Runtimes(), "scavenger"));
    REQUIRE(runtime != nullptr);
    REQUIRE(MOX::NUM_PLAYERS == 2);
    Reset_Technology_Player_(0);
    Reset_Technology_Player_(1);
    MOX::player[1].traits[TRAIT_UNCREATIVE] = 1;
    MOX::player[1].tech_applications[TECH_APP_ZORTRIUM_ARMOR] = 2;
    uint8_t player_technologies[2][TECH_APP_COUNT];
    std::memcpy(player_technologies[0],
        MOX::player[0].tech_applications,
        sizeof(player_technologies[0]));
    std::memcpy(player_technologies[1],
        MOX::player[1].tech_applications,
        sizeof(player_technologies[1]));
    game_random::Set_Random_Seed(SEED);
    const orion2re::lua::game::s_player_setup_event failed_setup = {1};
    orion2re::lua::s_lua_result failed_result;
    CHECK_FALSE(orion2re::lua::Dispatch_Mod_Event(runtime,
        "new_game.player_setup",
        Build_Player_Setup_Event_Context_,
        const_cast<orion2re::lua::game::s_player_setup_event*>(
            &failed_setup),
        &failed_result));
    CHECK(failed_result.diagnostic.find(
              "invalid technology status 2 for application 'zortrium_armor'") !=
        std::string::npos);
    CHECK(game_random::Get_Random_Seed() == SEED);
    CHECK(std::memcmp(player_technologies[0],
              MOX::player[0].tech_applications,
              sizeof(player_technologies[0])) == 0);
    CHECK(std::memcmp(player_technologies[1],
              MOX::player[1].tech_applications,
              sizeof(player_technologies[1])) == 0);

    Shutdown_Files_(&files);
}

TEST_CASE("stable game collections honor callback capabilities") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    const orion2re::lua::s_lua_event_descriptor events[] = {
        {"game.loaded", 1},
        {"test.invalid_counts", 1},
        {"test.invalid_player", 1},
        {"test.invalid_player_technology", 1},
        {"test.invalid_player_technology_field", 1},
        {"test.invalid_technology", 1},
        {"test.invalid_technology_field", 1},
        {"test.mutable", 1, orion2re::lua::LUA_GAME_STATE_MUTABLE},
        {"test.topology_valid", 1},
        {"test.topology", 1},
    };
    const orion2re::lua::s_lua_rule_descriptor rules[] = {
        {"test.value", 1},
    };
    const char source[] =
        "events.on('game.loaded', function()\n"
        "  if #game.players:all() ~= 2 or game.players:get(0).id ~= 0 then\n"
        "    error('read-only collection access failed')\n"
        "  end\n"
        "  if pcall(function() game.players:get(0):add_research(1) end) then\n"
        "    error('loaded event mutated player state')\n"
        "  end\n"
        "end)\n"
        "events.on('test.invalid_player_technology', function()\n"
        "  local technologies = game.players:get(0).technologies\n"
        "  local status_ok = pcall(function()\n"
        "    return technologies:status('plasma_cannon')\n"
        "  end)\n"
        "  local available_ok = pcall(function()\n"
        "    return technologies:is_available('plasma_cannon')\n"
        "  end)\n"
        "  local researched_ok = pcall(function()\n"
        "    return technologies:is_researched('plasma_cannon')\n"
        "  end)\n"
        "  local field_ok = pcall(function()\n"
        "    return technologies:field_of('plasma_cannon')\n"
        "  end)\n"
        "  local alternatives_ok = pcall(function()\n"
        "    return technologies:alternatives('plasma_physics')\n"
        "  end)\n"
        "  if status_ok or available_ok or researched_ok or field_ok or\n"
        "      alternatives_ok then\n"
        "    error('invalid player technology state was accepted')\n"
        "  end\n"
        "end)\n"
        "events.on('test.invalid_counts', function()\n"
        "  local collections = {\n"
        "    game.players, game.ships, game.planets, game.systems,\n"
        "  }\n"
        "  for _, collection in ipairs(collections) do\n"
        "    if collection:get(0) ~= nil or\n"
        "        pcall(function() collection:all() end) then\n"
        "      error('invalid native collection count was accepted')\n"
        "    end\n"
        "  end\n"
        "end)\n"
        "events.on('test.invalid_player', function()\n"
        "  local player = game.players:get(0)\n"
        "  local government_ok = pcall(function() return player.government end)\n"
        "  local planet_ok = pcall(function() return player.home_planet end)\n"
        "  local system_ok = pcall(function() return player.home_system end)\n"
        "  if government_ok or planet_ok or system_ok then\n"
        "    error('invalid native player relationship was accepted')\n"
        "  end\n"
        "end)\n"
        "events.on('test.invalid_player_technology_field', function()\n"
        "  local technologies = game.players:get(0).technologies\n"
        "  local field_ok = pcall(function()\n"
        "    return technologies:field_of('plasma_cannon')\n"
        "  end)\n"
        "  local alternatives_ok = pcall(function()\n"
        "    return technologies:alternatives('plasma_physics')\n"
        "  end)\n"
        "  if field_ok or alternatives_ok then\n"
        "    error('invalid player technology field status was accepted')\n"
        "  end\n"
        "end)\n"
        "events.on('test.invalid_technology', function()\n"
        "  local application_missing =\n"
        "    game.technology.applications:get('plasma_cannon') == nil\n"
        "  local applications_ok =\n"
        "    pcall(function() return game.technology.applications:all() end)\n"
        "  local next_ok = pcall(function()\n"
        "    return game.technology.fields:get('plasma_physics').next\n"
        "  end)\n"
        "  if not application_missing or applications_ok or next_ok then\n"
        "    error('invalid native technology state was accepted')\n"
        "  end\n"
        "end)\n"
        "events.on('test.invalid_technology_field', function()\n"
        "  local technology = game.technology\n"
        "  local application_field_ok = pcall(function()\n"
        "    return technology.applications:get('plasma_cannon').field\n"
        "  end)\n"
        "  local previous_ok = pcall(function()\n"
        "    return technology.fields:get('plasma_physics').previous\n"
        "  end)\n"
        "  local starting_ok = pcall(function()\n"
        "    return technology.starting_fields:all()\n"
        "  end)\n"
        "  local membership_ok = pcall(function()\n"
        "    return technology.fields:get('plasma_physics').applications:all()\n"
        "  end)\n"
        "  if application_field_ok or previous_ok or starting_ok or membership_ok then\n"
        "    error('invalid technology field relationship was accepted')\n"
        "  end\n"
        "end)\n"
        "events.on('test.mutable', function() game = {} end)\n"
        "rules.modify('test.value', function(context, value)\n"
        "  if game.players:get(0).id ~= 0 then error('rule read failed') end\n"
        "  if game.planets:get(0).system ~= game.systems:get(0) then\n"
        "    error('rule planet read failed')\n"
        "  end\n"
        "  if pcall(function() game.players:get(0):add_research(1) end) then\n"
        "    error('rule mutated player state')\n"
        "  end\n"
        "  return value\n"
        "end)\n"
        "local retained_planet\n"
        "local retained_player\n"
        "local retained_home\n"
        "local retained_system\n"
        "events.on('test.topology_valid', function()\n"
        "  if not retained_planet or not retained_planet.valid or\n"
        "      not retained_system.valid or not retained_player.valid or\n"
        "      retained_home ~= retained_player.home_planet then\n"
        "    error('topology reference unexpectedly expired')\n"
        "  end\n"
        "end)\n"
        "events.on('test.topology', function()\n"
        "  if not retained_planet then\n"
        "    retained_planet = game.planets:get(0)\n"
        "    retained_player = game.players:get(0)\n"
        "    retained_home = retained_player.home_planet\n"
        "    retained_system = game.systems:get(0)\n"
        "    return\n"
        "  end\n"
        "  if retained_planet.valid or retained_system.valid or\n"
        "      retained_home.valid or not retained_player.valid then\n"
        "    error('retained topology reference did not expire')\n"
        "  end\n"
        "  if retained_player.home_planet ~= game.planets:get(0) or\n"
        "      retained_player.home_system ~= game.systems:get(0) then\n"
        "    error('player did not resolve the replacement topology')\n"
        "  end\n"
        "  if retained_planet == game.planets:get(0) or\n"
        "      retained_system == game.systems:get(0) then\n"
        "    error('retained topology reference silently retargeted')\n"
        "  end\n"
        "  if pcall(function() return retained_planet.size end) or\n"
        "      pcall(function() return retained_system.x end) then\n"
        "    error('expired topology reference remained readable')\n"
        "  end\n"
        "end)\n";
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_result result;
    orion2re::lua::s_lua_mod_runtime* runtime = nullptr;
    REQUIRE(orion2re::lua::Create_Mod_Runtime(
        "collections",
        "1.0.0",
        source,
        sizeof(source) - 1,
        "mods/collections/main.lua",
        sizeof(source) - 1,
        events,
        std::size(events),
        rules,
        std::size(rules),
        &limits,
        &runtime,
        &result));

    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "game.loaded", nullptr, nullptr, &result));
    CHECK((MOX::player[0].research_accumulated == 0));

    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.topology", nullptr, nullptr, &result));
    orion2re::lua::game::s_semantic_state_replacement replacement;
    orion2re::lua::game::Begin_Semantic_State_Replacement(&replacement);
    orion2re::lua::game::Abort_Semantic_State_Replacement(&replacement);
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.topology_valid", nullptr, nullptr, &result));
    orion2re::lua::game::Begin_Semantic_State_Replacement(&replacement);
    orion2re::lua::game::Commit_Semantic_State_Replacement(&replacement);
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.topology", nullptr, nullptr, &result));

    std::int64_t value = 7;
    CAPTURE(result.diagnostic);
    REQUIRE(orion2re::lua::Dispatch_Mod_Rule(runtime,
        "test.value",
        Build_Empty_Context_,
        nullptr,
        0,
        10,
        &value,
        &result));
    CHECK(value == 7);
    CHECK((MOX::player[0].research_accumulated == 0));

    CHECK_FALSE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.mutable", nullptr, nullptr, &result));
    CHECK(result.diagnostic.find("replaced the engine-owned game root") !=
        std::string::npos);
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "game.loaded", nullptr, nullptr, &result));

    MOX::NUM_PLAYERS = MAX_PLAYERS + 1;
    MOX::NUM_SHIPS = MAX_SHIPS + 1;
    test::Resize_Planets(&::game::world, MAX_PLANETS + 1);
    MOX::NUM_STARS = MAX_STARS + 1;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.invalid_counts", nullptr, nullptr, &result));
    MOX::NUM_PLAYERS = 2;
    MOX::NUM_SHIPS = 2;
    test::Resize_Planets(&::game::world, 3);
    MOX::NUM_STARS = 2;

    MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_COUNT;
    MOX::player[0].home_planet_id = game::id_planet{99};
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.invalid_player", nullptr, nullptr, &result));
    MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] =
        GOVERNMENT_DEMOCRACY;
    MOX::player[0].home_planet_id = game::id_planet{0};

    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] = 2;
    TECHDATA::technology_applications[TECH_APP_PLASMA_CANNON]
        .tech_field_id = TECH_FIELD_COUNT;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(runtime,
        "test.invalid_player_technology",
        nullptr,
        nullptr,
        &result));
    MOX::player[0].tech_applications[TECH_APP_PLASMA_CANNON] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    TECHDATA::technology_applications[TECH_APP_PLASMA_CANNON]
        .tech_field_id = TECH_FIELD_PLASMA_PHYSICS;

    MOX::player[0].tech_fields[TECH_FIELD_PLASMA_PHYSICS] = 4;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(runtime,
        "test.invalid_player_technology_field",
        nullptr,
        nullptr,
        &result));
    MOX::player[0].tech_fields[TECH_FIELD_PLASMA_PHYSICS] = 0;

    TECHDATA::technology_applications[137].tech_app_id = 0;
    TECHDATA::technology_fields[59].next_field_id = TECH_FIELD_COUNT;
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.invalid_technology", nullptr, nullptr, &result));
    TECHDATA::technology_applications[137].tech_app_id = 137;
    TECHDATA::technology_fields[59].next_field_id = 51;

    const std::int16_t prior_application_fields[] = {
        TECHDATA::technology_applications[0].tech_field_id,
        TECHDATA::technology_applications[1].tech_field_id,
        TECHDATA::technology_applications[2].tech_field_id,
        TECHDATA::technology_applications[3].tech_field_id,
        TECHDATA::technology_applications[4].tech_field_id,
    };
    const std::int16_t prior_starting_field =
        TECHDATA::starting_tech_field_ids[0];
    const std::int16_t prior_previous_field =
        TECHDATA::technology_fields[59].prev_field_id;
    TECHDATA::technology_applications[137].tech_field_id = TECH_FIELD_COUNT;
    TECHDATA::technology_fields[59].prev_field_id = TECH_FIELD_COUNT;
    TECHDATA::starting_tech_field_ids[0] = TECH_FIELD_COUNT;
    for (std::int16_t application_id = 0; application_id < 5;
         ++application_id) {
        TECHDATA::technology_applications[application_id].tech_field_id = 59;
    }
    REQUIRE(orion2re::lua::Dispatch_Mod_Event(
        runtime, "test.invalid_technology_field", nullptr, nullptr, &result));
    TECHDATA::technology_applications[137].tech_field_id = 59;
    TECHDATA::technology_fields[59].prev_field_id = prior_previous_field;
    TECHDATA::starting_tech_field_ids[0] = prior_starting_field;
    for (std::int16_t application_id = 0; application_id < 5;
         ++application_id) {
        TECHDATA::technology_applications[application_id].tech_field_id =
            prior_application_fields[application_id];
    }

    orion2re::lua::Destroy_Mod_Runtime(runtime);
}

TEST_CASE("stored ship references survive recreation and expire on slot reuse") {
    TEST_LUA_GAME::Reset_();
    orion2re::lua::game::Reset_Semantic_State();
    orion2re::lua::game::Restore_Legacy_Ship_Generations();
    s_package_files files;
    Initialize_Files_(&files);
    Write_File_(files, "main.lua", "events.on('ship.destroyed', function(event)\n"
                                   "  if not storage.ship then\n"
                                   "    if game.ships:get(0) ~= event.killer then error('lookup identity mismatch') end\n"
                                   "    storage.ship = event.killer\n"
                                   "  elseif storage.ship.kind ~= 'ship' or storage.ship.id ~= 0 then\n"
                                   "    error('stored ship identity changed')\n"
                                   "  elseif storage.ship.valid then\n"
                                   "    if storage.ship ~= game.ships:get(0) then error('restored identity mismatch') end\n"
                                   "    storage.ship.owner:add_research(3)\n"
                                   "  else\n"
                                   "    if storage.ship == game.ships:get(0) then error('reused slot retained identity') end\n"
                                   "    local live = pcall(function() return storage.ship.owner end)\n"
                                   "    if live then error('expired ship reference remained usable') end\n"
                                   "    event.killer.owner:add_research(7)\n"
                                   "  end\n"
                                   "end)\n");

    config::s_mod_manifest package;
    package.id = "scavenger";
    package.version_text = "1.0.0";
    package.has_lua_runtime = true;
    package.lua_runtime.api = 1;
    package.lua_runtime.storage_version = 1;
    std::vector<config::s_mod_manifest> packages = {package};
    orion2re::lua::s_lua_mod_limits limits;
    orion2re::lua::s_lua_registry registry;
    orion2re::lua::s_lua_mod_runtime_set* runtimes = nullptr;
    config::s_cfg_diagnostic diagnostic;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages, &limits, &registry, &runtimes, &diagnostic));
    registry.packages_high_to_low_priority.push_back(
        {"scavenger", "1.0.0"});
    orion2re::lua::Commit_Registry(&registry);
    orion2re::lua::Commit_Mod_Runtimes(&runtimes);

    MOX::ship[0].owner = 0;
    MOX::ship[0].status = 0;
    MOX::combat_data[0].ship_idx = 0;
    MOX::combat_data[0].owner = 0;
    MOX::combat_data[1].ship_idx = 1;
    MOX::combat_data[1].owner = 1;
    const orion2re::lua::game::s_ship_destroyed_event event = {
        1,
        0,
        1,
    };
    orion2re::lua::game::Ship_Destroyed(&event);

    orion2re::lua::s_lua_save_snapshot captured;
    std::string storage_diagnostic;
    REQUIRE(orion2re::lua::Capture_Lua_Save_Snapshot(
        &captured, &storage_diagnostic));
    REQUIRE(captured.ship_generation_count == 2);
    CHECK(captured.ship_generations[0] == 1);
    const uint32_t snapshot_size =
        orion2re::lua::Lua_Save_Snapshot_Size(&captured);
    REQUIRE(snapshot_size != 0);
    std::vector<uint8_t> encoded(snapshot_size);
    SERIAL::Writer_ writer(
        encoded.data(), encoded.size(), "Lua save snapshot overflow");
    orion2re::lua::Write_Lua_Save_Snapshot(&writer, &captured);
    REQUIRE(writer.Length() == snapshot_size);
    SERIAL::Reader_ reader(
        encoded.data(), encoded.size(), "Lua save snapshot underflow");
    orion2re::lua::s_lua_save_snapshot decoded;
    REQUIRE(orion2re::lua::Read_Lua_Save_Snapshot(
        &reader, &decoded, &storage_diagnostic));
    CHECK(reader.Remaining() == 0);

    REQUIRE(orion2re::lua::game::Advance_Ship_Generation(0));
    CHECK(orion2re::lua::game::Ship_Generation(0) == 2);

    orion2re::lua::s_lua_registry restored_registry;
    orion2re::lua::s_lua_mod_runtime_set* restored = nullptr;
    REQUIRE(orion2re::lua::Stage_Mod_Runtimes(
        &packages,
        &limits,
        &restored_registry,
        &restored,
        &diagnostic));
    restored_registry.packages_high_to_low_priority.push_back(
        {"scavenger", "1.0.0"});
    REQUIRE(orion2re::lua::Restore_Lua_Save_Snapshot(&decoded,
        &restored_registry,
        restored,
        &storage_diagnostic));
    CHECK(orion2re::lua::game::Ship_Generation(0) == 2);
    orion2re::lua::Commit_Registry(&restored_registry);
    orion2re::lua::Commit_Mod_Runtimes(&restored);
    CHECK(orion2re::lua::game::Ship_Generation(0) == 1);

    orion2re::lua::game::Ship_Destroyed(&event);
    CHECK((MOX::player[0].research_accumulated == 3));

    REQUIRE(orion2re::lua::game::Advance_Ship_Generation(0));
    orion2re::lua::game::Ship_Destroyed(&event);
    CHECK((MOX::player[0].research_accumulated == 10));

    Shutdown_Files_(&files);
}