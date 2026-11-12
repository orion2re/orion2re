#include "pch.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "config/config_decode.h"
#include "config/config_identity.h"
#include "config/config_loader.h"
#include "config/config_snapshot.h"
#include "config/mod_manifest.h"
#include "config/mod_manager.h"
#include "doctest/doctest.h"
#include "game/build_queue.h"
#include "game/netmox.h"
#include "game/netstart_roster.h"
#include "game/netturn.h"
#include "game/techdata.h"
#include "game/vfs.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_network.h"
#include "lua/lua_registry.h"
#include "lua/lua_save.h"
#include "game/arena.h"
#include "miniz.h"
#include "version.h"

extern "C" {
#include <lua.h>
}

namespace TEST_CONFIG_REFRESH {
    extern int refresh_count;
    extern int group_refresh_count[config::CONFIG_GROUP_COUNT];
    extern bool observed_committed_state;
    void Reset_();
    void Expect_(uint8_t scroll_interval, const char* path, const char* contents);
} // namespace TEST_CONFIG_REFRESH

namespace TEST_CONFIG_NETTURN {
    extern void (*chat_box_input_loop_hook)();
    extern void (*next_turn_hook)();
    extern int next_turn_calls;
    void Reset_();
} // namespace TEST_CONFIG_NETTURN

namespace {
    namespace fs = std::filesystem;

    class s_config_test_directory {
      public:
        s_config_test_directory() {
            const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
            root_path_ = fs::temp_directory_path() /
                ("orion2re-config-loader-" + std::to_string(unique_id));
            fs::create_directories(root_path_);
        }

        ~s_config_test_directory() {
            orion2re::lua::Reset_Mod_Runtimes();
            orion2re::lua::Reset_Registry();
            config::internal::Reset_Loaded_Mods();
            vfs::Shutdown();
            std::error_code error;
            fs::remove_all(root_path_, error);
        }

        void Initialize_Vfs() const {
            const std::string root = root_path_.string();
            DOCTEST_REQUIRE(vfs::Init(root.c_str()) == 1);
        }

        void Write(const fs::path& relative_path, const std::string& contents) const {
            const fs::path full_path = root_path_ / relative_path;
            fs::create_directories(full_path.parent_path());
            std::ofstream output(full_path, std::ios::binary);
            DOCTEST_REQUIRE(output.good());
            output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
            DOCTEST_REQUIRE(output.good());
        }

        void Write_Zip(const fs::path& relative_path,
            const std::vector<std::pair<std::string, std::string>>& entries) const {
            const fs::path full_path = root_path_ / relative_path;
            fs::create_directories(full_path.parent_path());
            mz_zip_archive archive{};
            mz_zip_zero_struct(&archive);
            DOCTEST_REQUIRE(mz_zip_writer_init_file(
                                &archive, full_path.string().c_str(), 0) == MZ_TRUE);
            for (const auto& entry : entries) {
                DOCTEST_REQUIRE(mz_zip_writer_add_mem(
                                    &archive, entry.first.c_str(), entry.second.data(), entry.second.size(), MZ_BEST_COMPRESSION) == MZ_TRUE);
            }
            DOCTEST_REQUIRE(mz_zip_writer_finalize_archive(&archive) == MZ_TRUE);
            DOCTEST_REQUIRE(mz_zip_writer_end(&archive) == MZ_TRUE);
        }

        void Copy_Tree(const fs::path& source,
            const fs::path& relative_destination) const {
            const fs::path destination = root_path_ / relative_destination;
            fs::create_directories(destination.parent_path());
            std::error_code error;
            fs::copy(source,
                destination,
                fs::copy_options::recursive |
                    fs::copy_options::overwrite_existing,
                error);
            DOCTEST_REQUIRE_FALSE(error);
        }

      private:
        fs::path root_path_;
    };

    void Set_Native_Defaults_() {
        COMBINIT::scroll_interval = 1;
        CMBTFIR2::combat_bonus.battle_scanner_beam_offense = 50;
        REPORT::no_outpost_prompt = 0;
    }

    config::t_variable_id Variable_Id_(const char* path) {
        config::t_variable_id id = config::CONFIG_NO_VARIABLE;
        config::Find_Variable(path, std::strlen(path), &id);
        return id;
    }

    void Check_Effective_Config_Equals_(const config::s_effective_config& actual,
        const config::s_effective_config& expected) {
        DOCTEST_REQUIRE(actual.state.values.size() == expected.state.values.size());
        DOCTEST_REQUIRE(actual.provenance.size() == expected.provenance.size());
        DOCTEST_REQUIRE(actual.prior_values.size() == expected.prior_values.size());
        for (size_t variable_id = 0; variable_id < actual.state.values.size(); ++variable_id) {
            const config::s_typed_value& actual_value = actual.state.values[variable_id];
            const config::s_typed_value& expected_value = expected.state.values[variable_id];
            DOCTEST_CHECK(config::Config_Values_Equal(actual_value, expected_value));
            DOCTEST_CHECK(actual_value.type == expected_value.type);
            DOCTEST_CHECK(actual_value.bool_value == expected_value.bool_value);
            DOCTEST_CHECK(actual_value.integer_value == expected_value.integer_value);
            DOCTEST_CHECK(actual_value.string_value == expected_value.string_value);
            DOCTEST_CHECK(actual_value.bitmap_value == expected_value.bitmap_value);
            DOCTEST_CHECK(actual_value.ratio_numerator == expected_value.ratio_numerator);
            DOCTEST_CHECK(actual_value.ratio_denominator == expected_value.ratio_denominator);

            const config::s_value_provenance& actual_provenance =
                actual.provenance[variable_id];
            const config::s_value_provenance& expected_provenance =
                expected.provenance[variable_id];
            DOCTEST_CHECK(actual_provenance.has_override ==
                expected_provenance.has_override);
            DOCTEST_CHECK(actual_provenance.package_id == expected_provenance.package_id);
            DOCTEST_CHECK(actual_provenance.location.source ==
                expected_provenance.location.source);
            DOCTEST_CHECK(actual_provenance.location.offset ==
                expected_provenance.location.offset);
            DOCTEST_CHECK(actual_provenance.location.line ==
                expected_provenance.location.line);
            DOCTEST_CHECK(actual_provenance.location.column ==
                expected_provenance.location.column);
            DOCTEST_CHECK(actual_provenance.statement == expected_provenance.statement);
            DOCTEST_CHECK(actual.prior_values[variable_id].size() ==
                expected.prior_values[variable_id].size());
        }
    }

    void Check_Active_Caches_() {
        const config::s_active_config* active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        config::s_cfg_diagnostic diagnostic;
        for (const auto* descriptor : {&config::saved_config_snapshot, &config::synchronized_config_snapshot}) {
            const config::s_prepared_config_snapshot* cached = nullptr;
            DOCTEST_REQUIRE(config::Get_Prepared_Active_Config_Snapshot(descriptor, &cached, &diagnostic));
            config::s_prepared_config_snapshot fresh;
            DOCTEST_REQUIRE(config::Prepare_Config_Snapshot(descriptor,
                &active->baseline.effective.state,
                &active->required_mods,
                &fresh));
            DOCTEST_CHECK(cached->bytes == fresh.bytes);
        }
        const std::vector<uint8_t>* cached_identity = nullptr;
        DOCTEST_REQUIRE(config::Get_Canonical_Active_Config_Identity(&cached_identity, &diagnostic));
        std::vector<uint8_t> fresh_identity;
        std::string identity_diagnostic;
        DOCTEST_REQUIRE(config::Build_Sparse_Config_Identity(&config::synchronized_config_snapshot,
            &active->baseline.effective.state,
            &fresh_identity,
            &identity_diagnostic));
        DOCTEST_CHECK(*cached_identity == fresh_identity);
    }

    std::string Manifest_(const std::string& id, const std::string& extra = {}) {
        return "{\n"
               "  id: \"" +
            id + "\",\n"
                 "  name: \"" +
            id + "\",\n"
                 "  version: \"1.0.0\",\n"
                 "  requires: { engine: \">=2.0.0 <3.0.0\" },\n"
                 "  config: \"config/main.cfg\",\n" +
            extra +
            "}\n";
    }

    std::string Versioned_Manifest_(const std::string& id,
        const std::string& version,
        const std::string& compatible_versions = {},
        const std::string& extra = {}) {
        return "{\n"
               "  id: \"" +
            id + "\",\n"
                 "  name: \"" +
            id + "\",\n"
                 "  version: \"" +
            version + "\",\n" +
            (compatible_versions.empty() ? std::string() : "  compatible_versions: \"" + compatible_versions + "\",\n") +
            "  requires: { engine: \">=2.0.0 <3.0.0\" },\n"
            "  config: \"config/main.cfg\",\n" +
            extra +
            "}\n";
    }

    std::string Read_Vfs_Text_(const char* path) {
        s_vfs_file* file = vfs::fopen(path, "rb");
        DOCTEST_REQUIRE(file != nullptr);
        const int32_t size = vfs::fsize(file);
        DOCTEST_REQUIRE(size >= 0);
        std::string result(static_cast<size_t>(size), '\0');
        if (!result.empty()) {
            DOCTEST_REQUIRE(vfs::fread(result.data(), 1, result.size(), file) ==
                result.size());
        }
        DOCTEST_REQUIRE(vfs::fclose(file) == 0);
        return result;
    }

    std::string Lua_Fingerprint_Mismatch_(
        const orion2re::lua::s_lua_registry* expected,
        const orion2re::lua::s_lua_registry* actual) {
        std::string diagnostic;
        orion2re::lua::Compare_Registry_Fingerprints(
            expected, actual, &diagnostic);
        return diagnostic;
    }

    bool Unused_Document_Loader_(const std::string&, config::s_cfg_document*, config::s_cfg_diagnostic*) {
        return false;
    }

    int8_t Fail_Mount_Commit_(s_vfs_mount_transaction*) {
        return 0;
    }

    bool Fail_After_Group_Merge_(config::t_config_group group,
        const config::s_effective_config* candidate,
        config::s_effective_config* effective) {
        static int merge_count;
        if (group == config::CONFIG_GROUP_INTERFACE) merge_count = 0;
        merge_count++;
        DOCTEST_REQUIRE(config::Merge_Config_Group(group, candidate, effective));
        return merge_count < 2;
    }

    bool Reload_All_Groups_(const config::s_config_group_descriptor*, bool) {
        return true;
    }

    bool Merge_With_Inactive_Value_Change_(config::t_config_group group,
        const config::s_effective_config* candidate,
        config::s_effective_config* effective) {
        if (!config::Merge_Config_Group(group, candidate, effective)) {
            return false;
        }
        if (group == config::CONFIG_GROUP_INTERFACE) {
            const config::t_variable_id variable_id =
                Variable_Id_("interface.scroll_interval");
            effective->state.values[variable_id].bool_value = true;
        }
        return true;
    }

    int Build_Empty_Rule_Context_(lua_State* lua_state, void*) {
        lua_newtable(lua_state);
        return 1;
    }

    std::int64_t Dispatch_Test_Growth_Rule_() {
        auto* runtime = const_cast<orion2re::lua::s_lua_mod_runtime*>(
            orion2re::lua::Find_Mod_Runtime(
                orion2re::lua::Get_Active_Mod_Runtimes(),
                "handoff_runtime"));
        DOCTEST_REQUIRE(runtime != nullptr);
        std::int64_t value = 5;
        orion2re::lua::s_lua_result result;
        DOCTEST_REQUIRE(orion2re::lua::Dispatch_Mod_Rule(runtime,
            "population.growth",
            Build_Empty_Rule_Context_,
            nullptr,
            INT16_MIN,
            INT16_MAX,
            &value,
            &result));
        return value;
    }

    std::int64_t netturn_growth_result;

    void Dispatch_Netturn_Growth_Rule_() {
        netturn_growth_result = Dispatch_Test_Growth_Rule_();
    }

    void Release_Client_Next_Turn_Wait_() {
        NETMOX::net.sync_state = NET_SYNC_STATE_WAITING_FOR_GAME_DATA;
        MOX::player[MOX::PLAYER_NUM].eliminated = 1;
    }
} // namespace

DOCTEST_TEST_CASE("engine version components and menu label agree") {
    const std::string version = std::to_string(orion2re::ENGINE_VERSION_MAJOR) + "." +
        std::to_string(orion2re::ENGINE_VERSION_MINOR) + "." +
        std::to_string(orion2re::ENGINE_VERSION_PATCH);
    DOCTEST_CHECK(version == orion2re::ENGINE_VERSION);
    DOCTEST_CHECK(version == GAME_VERSION);
    DOCTEST_CHECK("Version " + version == GAME_VERSION_LABEL);
}

DOCTEST_TEST_CASE("all shipped mod manifests have valid version structure") {
    s_config_test_directory files;
    std::vector<std::string> package_ids;
    for (const auto& entry : fs::directory_iterator(LUA_SOURCE_PACKAGE_DIR)) {
        if (entry.is_directory() && fs::is_regular_file(entry.path() / "mod.json5")) {
            const std::string package_id = entry.path().filename().string();
            package_ids.push_back(package_id);
            files.Copy_Tree(entry.path(), fs::path("data/mods") / package_id);
        }
    }
    DOCTEST_REQUIRE_FALSE(package_ids.empty());
    std::sort(package_ids.begin(), package_ids.end());
    files.Initialize_Vfs();
    struct s_mount_cleanup {
        s_vfs_mount_transaction* transaction = vfs::Begin_Mount_Transaction();
        ~s_mount_cleanup() {
            if (transaction != nullptr) {
                vfs::Discard_Mount_Transaction(transaction);
            }
        }
    } mount;
    DOCTEST_REQUIRE(mount.transaction != nullptr);
    for (const auto& package_id : package_ids) {
        DOCTEST_CAPTURE(package_id);
        std::string mount_diagnostic;
        DOCTEST_INFO(mount_diagnostic);
        DOCTEST_REQUIRE(vfs::Stage_Package_Mount(
            mount.transaction, package_id.c_str(), &mount_diagnostic));
        config::s_mod_manifest manifest;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_INFO(diagnostic.message);
        DOCTEST_REQUIRE(config::Load_Mod_Manifest(
            package_id, &manifest, &diagnostic));
        DOCTEST_CHECK(config::Semantic_Version_Matches(
            {2, 0, 0}, manifest.engine_range));
        DOCTEST_CHECK_FALSE(config::Semantic_Version_Matches(
            {1, 99, 99}, manifest.engine_range));
        config::s_semantic_version version;
        DOCTEST_CHECK(config::Parse_Semantic_Version(manifest.version_text, &version));
        for (const auto& dependency : manifest.dependencies) {
            DOCTEST_CHECK(config::Validate_Semantic_Version_Range(dependency.version));
        }
        for (const auto& dependency : manifest.optional_dependencies) {
            DOCTEST_CHECK(config::Validate_Semantic_Version_Range(dependency.version));
        }
    }
}

DOCTEST_TEST_CASE("Lua runtime manifest is strict and simulation-only") {
    s_config_test_directory files;
    files.Write(fs::path("data") / "mods" / "scripted" / "mod.json5",
        Manifest_("scripted",
            "  lua: {\n"
            "    api: 1, storage_version: 2,\n"
            "    migrations: [\n"
            "      { from: 1, to: 2, script: \"migrations/001-002.lua\" },\n"
            "    ],\n"
            "  },\n"));
    files.Write(fs::path("data") / "mods" / "with_kind" / "mod.json5",
        Manifest_("with_kind",
            "  lua: { api: 1, kind: \"client_only\", storage_version: 1 },\n"));
    files.Write(fs::path("data") / "mods" / "with_services" / "mod.json5",
        Manifest_("with_services",
            "  lua: { api: 1, storage_version: 1, services: [] },\n"));
    files.Initialize_Vfs();
    s_vfs_mount_transaction* transaction = vfs::Begin_Mount_Transaction();
    DOCTEST_REQUIRE(transaction != nullptr);
    std::string mount_diagnostic;
    DOCTEST_REQUIRE(vfs::Stage_Package_Mount(
        transaction, "scripted", &mount_diagnostic));
    DOCTEST_REQUIRE(vfs::Stage_Package_Mount(
        transaction, "with_kind", &mount_diagnostic));
    DOCTEST_REQUIRE(vfs::Stage_Package_Mount(
        transaction, "with_services", &mount_diagnostic));
    config::s_mod_manifest manifest;
    config::s_cfg_diagnostic diagnostic;

    DOCTEST_REQUIRE(config::Load_Mod_Manifest(
        "scripted", &manifest, &diagnostic));
    DOCTEST_REQUIRE(manifest.has_lua_runtime);
    DOCTEST_CHECK(manifest.lua_runtime.api == 1);
    DOCTEST_CHECK(manifest.lua_runtime.storage_version == 2);
    DOCTEST_REQUIRE(manifest.lua_runtime.migrations.size() == 1);
    DOCTEST_CHECK(manifest.lua_runtime.migrations[0].from == 1);
    DOCTEST_CHECK(manifest.lua_runtime.migrations[0].to == 2);
    DOCTEST_CHECK(manifest.lua_runtime.migrations[0].script ==
        "migrations/001-002.lua");

    DOCTEST_CHECK_FALSE(config::Load_Mod_Manifest(
        "with_kind", &manifest, &diagnostic));
    DOCTEST_CHECK(diagnostic.message ==
        "unknown manifest property 'lua.kind'");
    DOCTEST_CHECK_FALSE(config::Load_Mod_Manifest(
        "with_services", &manifest, &diagnostic));
    DOCTEST_CHECK(diagnostic.message ==
        "unknown manifest property 'lua.services'");
    vfs::Discard_Mount_Transaction(transaction);
}

DOCTEST_TEST_CASE("all shipped mods load their configuration and Lua successfully") {
    s_config_test_directory files;
    std::vector<std::string> package_ids;
    for (const auto& entry : fs::directory_iterator(LUA_SOURCE_PACKAGE_DIR)) {
        if (entry.is_directory() && fs::is_regular_file(entry.path() / "mod.json5")) {
            const std::string package_id = entry.path().filename().string();
            package_ids.push_back(package_id);
            files.Copy_Tree(entry.path(), fs::path("data/mods") / package_id);
        }
    }
    DOCTEST_REQUIRE_FALSE(package_ids.empty());
    std::sort(package_ids.begin(), package_ids.end());
    files.Initialize_Vfs();
    for (const auto& package_id : package_ids) {
        DOCTEST_CAPTURE(package_id);
        files.Write("main.cfg", "enable " + package_id + ";\n");
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_INFO(diagnostic.message);
        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(config::Find_Loaded_Mod(package_id) != nullptr);
        for (const auto& mod : config::Get_Loaded_Mods()) {
            if (mod.manifest.has_lua_runtime) {
                DOCTEST_CAPTURE(mod.manifest.id);
                DOCTEST_CHECK(orion2re::lua::Find_Mod_Runtime(
                                  orion2re::lua::Get_Active_Mod_Runtimes(),
                                  mod.manifest.id.c_str()) != nullptr);
            }
        }
    }
}

DOCTEST_TEST_CASE("network debug logging is local default-off and reloadable") {
    s_config_test_directory files;
    files.Write("orion2re.cfg", "");
    files.Initialize_Vfs();
    struct s_logging_cleanup {
        bool prior = NETMOX::net_debug_enabled;
        ~s_logging_cleanup() {
            NETMOX::net_debug_enabled = prior;
        }
    } cleanup;
    DOCTEST_CHECK_FALSE(NETMOX::net_debug_enabled);
    const auto variable = Variable_Id_("interface.net_debug_enabled");
    DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
    config::s_config_state defaults;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&defaults));
    DOCTEST_CHECK_FALSE(defaults.values.Read(variable).bool_value);
    config::s_effective_config effective;
    config::s_cfg_diagnostic diagnostic;
    bool loaded = false;
    DOCTEST_REQUIRE(config::Load_Startup_Config(
        "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
    DOCTEST_REQUIRE(loaded);
    DOCTEST_CHECK_FALSE(NETMOX::net_debug_enabled);
    DOCTEST_CHECK_FALSE(effective.state.values.Read(variable).bool_value);
    DOCTEST_CHECK(config::Get_Variable_Group(variable) == config::CONFIG_GROUP_INTERFACE);

    const config::s_prepared_config_snapshot* prepared = nullptr;
    DOCTEST_REQUIRE(config::Get_Prepared_Active_Config_Snapshot(
        &config::synchronized_config_snapshot, &prepared, &diagnostic));
    const auto synchronized_before = prepared->bytes;
    DOCTEST_REQUIRE(config::Get_Prepared_Active_Config_Snapshot(
        &config::saved_config_snapshot, &prepared, &diagnostic));
    const auto saved_before = prepared->bytes;

    files.Write("orion2re.cfg", "interface net_debug_enabled = true;\n");
    DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(true, &diagnostic));
    DOCTEST_CHECK(NETMOX::net_debug_enabled);
    DOCTEST_REQUIRE(config::Get_Prepared_Active_Config_Snapshot(
        &config::synchronized_config_snapshot, &prepared, &diagnostic));
    DOCTEST_CHECK(prepared->bytes == synchronized_before);
    DOCTEST_REQUIRE(config::Get_Prepared_Active_Config_Snapshot(
        &config::saved_config_snapshot, &prepared, &diagnostic));
    DOCTEST_CHECK(prepared->bytes == saved_before);

    files.Write("orion2re.cfg", "");
    DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(true, &diagnostic));
    DOCTEST_CHECK_FALSE(NETMOX::net_debug_enabled);
    DOCTEST_CHECK_FALSE(config::Get_Active_Config()->baseline.effective.state.values.Read(variable).bool_value);

    files.Write("orion2re.cfg", "interface net_debug_enabled = true;\n");
    DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(true, &diagnostic));
    DOCTEST_CHECK(NETMOX::net_debug_enabled);

    files.Write("orion2re.cfg", "interface net_debug_enabled = false;\n");
    DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(true, &diagnostic));
    DOCTEST_CHECK_FALSE(NETMOX::net_debug_enabled);
    DOCTEST_CHECK_FALSE(config::Get_Active_Config()->baseline.effective.state.values.Read(variable).bool_value);
}

DOCTEST_TEST_CASE("150i multiplayer initializes config Lua and negotiated state with AI slots") {
    using namespace orion2re::lua;
    for (const char* profile : {"v150i", "v150i-mp"}) {
        DOCTEST_INFO("profile: " << profile);
        s_config_test_directory files;
        for (const char* package : {"v150", "v150i", "mirror_hw", "v150i-mp"}) {
            files.Copy_Tree(fs::path(LUA_SOURCE_PACKAGE_DIR) / package,
                fs::path("data/mods") / package);
        }
        files.Write("host.cfg", std::string("enable ") + profile + ";\n");
        files.Write("client.cfg", "");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic config_diagnostic;
        std::string diagnostic;
        DOCTEST_INFO(config_diagnostic.message);
        DOCTEST_INFO(diagnostic);
        Reset_Lua_Network_Session();
        struct s_session_cleanup {
            ~s_session_cleanup() {
                Reset_Lua_Network_Session();
            }
        } cleanup;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "host.cfg", "base_game", &effective, &loaded, &config_diagnostic));
        DOCTEST_REQUIRE(loaded);
        DOCTEST_REQUIRE(Network_Lua_Enabled());
        DOCTEST_REQUIRE(Check_Network_Mod_Runtime_Compatibility(
            Get_Active_Mod_Runtimes(), &diagnostic));
        const auto host_registry = *Get_Active_Registry();
        const auto host_requirements = config::Get_Active_Config()->required_mods;

        // Fresh allocations contain zero IDs; a second lobby may contain stale IDs.
        s_player players[MAX_PLAYERS] = {};
        players[MAX_PLAYERS - 1].network_player_id = 303;
        const int16_t users[] = {101, 202};
        s_net_pack_start_game start = {};
        const int16_t local = NETSTART::Assign_New_Game_Network_Roster_(
            players, &start, users, 2, 202);
        DOCTEST_REQUIRE(local == 1);
        // Player and start records are packed; double parentheses keep
        // doctest from binding references to their misaligned members.
        for (int index = 0; index < MAX_PLAYERS; ++index) {
            DOCTEST_CHECK((players[index].network_player_id ==
                (index < 2 ? users[index] : -1)));
            DOCTEST_CHECK((start.net_ids[index] == (index < 2 ? users[index] : 0)));
        }

        std::vector<uint8_t> offer_bytes(LUA_NETWORK_MAX_BYTES);
        uint32_t offer_size = 0;
        DOCTEST_REQUIRE(Begin_Lua_Network_Setup_Offer(offer_bytes.data(),
            static_cast<uint32_t>(offer_bytes.size()),
            LUA_NETWORK_SETUP_NEW_GAME,
            &offer_size,
            &diagnostic));
        s_lua_network_setup_offer offer;
        DOCTEST_REQUIRE(Read_Lua_Network_Setup_Offer(
            offer_bytes.data(), offer_size, &offer, &diagnostic));
        s_lua_network_digest compatibility;
        uint32_t mismatch = UINT32_MAX;
        DOCTEST_REQUIRE(Validate_Lua_Network_Setup_Offer(
            &offer, &host_registry, &compatibility, &mismatch, &diagnostic));
        uint8_t commit[16] = {};
        uint32_t commit_size = 0;
        DOCTEST_REQUIRE(Write_Lua_Network_Setup_Commit(commit, sizeof(commit), &commit_size));
        std::vector<uint8_t> start_bytes(sizeof(start) + commit_size + offer.configuration_size);
        SERIAL::Writer_ writer(start_bytes.data(),
            static_cast<uint32_t>(start_bytes.size()),
            "test startup packet overflow");
        writer.Write_Bytes(&start, sizeof(start));
        writer.Write_Bytes(commit, commit_size);
        writer.Write_Bytes(offer.configuration, offer.configuration_size);

        DOCTEST_REQUIRE(Dispatch_Active_Initialized_Event(&diagnostic));
        std::vector<uint8_t> host_state(LUA_NETWORK_MAX_BYTES);
        uint32_t host_state_size = 0;
        DOCTEST_REQUIRE(Capture_Lua_Network_State(host_state.data(),
            static_cast<uint32_t>(host_state.size()),
            &host_state_size,
            &diagnostic));
        host_state.resize(host_state_size);

        // Run the client from a vanilla configuration, with the required mods installed.
        Reset_Lua_Network_Session();
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "client.cfg", "base_game", &effective, &loaded, &config_diagnostic));
        DOCTEST_REQUIRE_FALSE(Network_Lua_Enabled());
        uint8_t result_bytes[2048] = {};
        uint32_t result_size = 0;
        DOCTEST_REQUIRE(Process_Lua_Network_Setup_Offer(offer_bytes.data(),
            offer_size,
            LUA_NETWORK_SETUP_NEW_GAME,
            result_bytes,
            sizeof(result_bytes),
            &result_size));
        s_lua_network_result result;
        DOCTEST_REQUIRE(Read_Lua_Network_Result(result_bytes, result_size, &result));
        DOCTEST_INFO(result.diagnostic);
        DOCTEST_REQUIRE(result.accepted);
        DOCTEST_REQUIRE_FALSE(Network_Lua_Enabled());
        DOCTEST_REQUIRE(Commit_Lua_Network_Setup(LUA_NETWORK_SETUP_NEW_GAME,
            start_bytes.data(),
            static_cast<uint32_t>(start_bytes.size()),
            sizeof(start),
            &diagnostic));
        DOCTEST_CHECK_FALSE(Has_Pending_Lua_Network_Setup());
        DOCTEST_REQUIRE(Network_Lua_Enabled());
        DOCTEST_CHECK(config::Get_Active_Config()->required_mods == host_requirements);
        DOCTEST_CHECK(Get_Active_Registry()->fingerprint == host_registry.fingerprint);
        DOCTEST_CHECK(Get_Active_Registry()->synchronized_config_hash ==
            host_registry.synchronized_config_hash);
        Check_Active_Caches_();

        s_lua_network_state_transaction* transaction = nullptr;
        DOCTEST_REQUIRE(Prepare_Lua_Network_State(host_state.data(),
            host_state_size,
            static_cast<uint32_t>(MOX::NUM_SHIPS),
            &transaction,
            &diagnostic));
        Commit_Lua_Network_State(&transaction);
        std::vector<uint8_t> client_state(LUA_NETWORK_MAX_BYTES);
        uint32_t client_state_size = 0;
        DOCTEST_REQUIRE(Capture_Lua_Network_State(client_state.data(),
            static_cast<uint32_t>(client_state.size()),
            &client_state_size,
            &diagnostic));
        client_state.resize(client_state_size);
        DOCTEST_REQUIRE(client_state == host_state);
    }
}

DOCTEST_TEST_CASE("cycle detection follows case-insensitive VFS path identity") {
    s_config_test_directory files;
    files.Write("Config/A.cfg", "include a.CFG;\n");
    files.Initialize_Vfs();
    config::s_effective_config effective;
    config::Initialize_Effective_Config(&effective);
    config::s_cfg_diagnostic diagnostic;

    DOCTEST_CHECK_FALSE(config::Load_Config_File(
        "CONFIG/A.CFG", "base_game", &effective, &diagnostic));
    DOCTEST_CHECK(diagnostic.message ==
        "include cycle: config/a.cfg -> config/a.cfg");
    DOCTEST_CHECK(diagnostic.location.source == "config/a.cfg");
}

DOCTEST_TEST_CASE("persistent Lua activation is atomic and identity-bearing") {
    s_config_test_directory files;
    files.Write("valid.cfg", "enable stable_runtime;\n");
    files.Write("invalid.cfg", "enable broken_runtime;\n");
    const std::string lua_manifest =
        "  lua: { api: 1, storage_version: 1 },\n";
    files.Write(fs::path("data") / "mods" / "stable_runtime" / "mod.json5",
        Manifest_("stable_runtime", lua_manifest));
    files.Write(fs::path("data") / "mods" / "stable_runtime" / "config" /
            "main.cfg",
        "interface scroll_interval = 74;\n");
    files.Write(fs::path("data") / "mods" / "stable_runtime" / "main.lua",
        "events.on('game.initialized', function() end)\n");
    files.Write(fs::path("data") / "mods" / "stable_runtime" / "data" /
            "stable.dat",
        "stable");
    files.Write(fs::path("data") / "mods" / "broken_runtime" / "mod.json5",
        Manifest_("broken_runtime", lua_manifest));
    files.Write(fs::path("data") / "mods" / "broken_runtime" / "config" /
            "main.cfg",
        "interface scroll_interval = 85;\n");
    files.Write(fs::path("data") / "mods" / "broken_runtime" / "main.lua",
        "error('persistent setup failed')\n");
    files.Write(fs::path("data") / "mods" / "broken_runtime" / "data" /
            "broken.dat",
        "broken");
    files.Initialize_Vfs();
    Set_Native_Defaults_();
    config::s_effective_config effective;
    config::s_cfg_diagnostic diagnostic;

    DOCTEST_REQUIRE(config::Load_Config_File(
        "valid.cfg", "base_game", &effective, &diagnostic));
    const orion2re::lua::s_lua_mod_runtime_set* active_runtimes =
        orion2re::lua::Get_Active_Mod_Runtimes();
    DOCTEST_REQUIRE(orion2re::lua::Get_Mod_Runtime_Count(active_runtimes) == 1);
    const orion2re::lua::s_lua_mod_runtime* stable_runtime =
        orion2re::lua::Find_Mod_Runtime(active_runtimes, "stable_runtime");
    DOCTEST_REQUIRE(stable_runtime != nullptr);
    DOCTEST_CHECK(orion2re::lua::Get_Mod_Callback_Count(stable_runtime) == 1);
    const orion2re::lua::s_lua_registry* active_registry =
        orion2re::lua::Get_Active_Registry();
    DOCTEST_REQUIRE(active_registry->runtime_packages.size() == 1);
    DOCTEST_REQUIRE(active_registry->runtime_packages[0].sources.size() == 1);
    DOCTEST_CHECK(active_registry->runtime_packages[0].sources[0].path ==
        "mods/stable_runtime/main.lua");
    DOCTEST_CHECK_FALSE(
        active_registry->runtime_packages[0].sources[0].source_hash.empty());
    const std::string fingerprint = active_registry->fingerprint;

    DOCTEST_CHECK_FALSE(config::Load_Config_File(
        "invalid.cfg", "base_game", &effective, &diagnostic));
    DOCTEST_CHECK(diagnostic.message.find("persistent setup failed") !=
        std::string::npos);
    DOCTEST_CHECK(COMBINIT::scroll_interval == 74);
    DOCTEST_CHECK(Read_Vfs_Text_("stable.dat") == "stable");
    DOCTEST_CHECK(vfs::File_Exists("broken.dat") == 0);
    active_runtimes = orion2re::lua::Get_Active_Mod_Runtimes();
    DOCTEST_REQUIRE(orion2re::lua::Get_Mod_Runtime_Count(active_runtimes) == 1);
    DOCTEST_CHECK(orion2re::lua::Find_Mod_Runtime(
                      active_runtimes, "stable_runtime") == stable_runtime);
    DOCTEST_CHECK(orion2re::lua::Get_Active_Registry()->fingerprint ==
        fingerprint);

    files.Write(fs::path("data") / "mods" / "stable_runtime" / "main.lua",
        "events.on('game.initialized', function()\n"
        "  local changed = true\n"
        "end)\n");
    DOCTEST_REQUIRE(config::Load_Config_File(
        "valid.cfg", "base_game", &effective, &diagnostic));
    DOCTEST_CHECK(orion2re::lua::Get_Active_Registry()->fingerprint !=
        fingerprint);
}

DOCTEST_TEST_CASE("persistent Lua setup follows low-to-high package priority") {
    s_config_test_directory files;
    files.Write("main.cfg", "enable high_runtime;\nenable low_runtime;\n");
    const std::string lua_manifest =
        "  lua: { api: 1, storage_version: 1 },\n";
    for (const std::string& id :
        {std::string("high_runtime"), std::string("low_runtime")}) {
        files.Write(fs::path("data") / "mods" / id / "mod.json5",
            Manifest_(id, lua_manifest));
        files.Write(fs::path("data") / "mods" / id / "config" / "main.cfg",
            "");
        files.Write(fs::path("data") / "mods" / id / "main.lua",
            "events.on('game.initialized', function() end)\n");
    }
    files.Initialize_Vfs();
    config::s_effective_config effective;
    config::s_cfg_diagnostic diagnostic;

    DOCTEST_REQUIRE(config::Load_Config_File(
        "main.cfg", "base_game", &effective, &diagnostic));
    const orion2re::lua::s_lua_registry* registry =
        orion2re::lua::Get_Active_Registry();
    DOCTEST_REQUIRE(registry->runtime_packages.size() == 2);
    DOCTEST_CHECK(registry->runtime_packages[0].package_id == "low_runtime");
    DOCTEST_CHECK(registry->runtime_packages[1].package_id == "high_runtime");
    const orion2re::lua::s_lua_mod_runtime_set* runtimes =
        orion2re::lua::Get_Active_Mod_Runtimes();
    DOCTEST_REQUIRE(orion2re::lua::Get_Mod_Runtime_Count(runtimes) == 2);
    DOCTEST_CHECK(orion2re::lua::Find_Mod_Runtime(
                      runtimes, "low_runtime") != nullptr);
    DOCTEST_CHECK(orion2re::lua::Find_Mod_Runtime(
                      runtimes, "high_runtime") != nullptr);
}

DOCTEST_TEST_CASE(
    "persistent Lua source identity ignores migration metadata and validates registrations") {
    s_config_test_directory files;
    files.Write("main.cfg", "enable identity_runtime;\n");
    const auto write_manifest = [&files](const std::string& migration) {
        files.Write(fs::path("data") / "mods" / "identity_runtime" /
                "mod.json5",
            Manifest_("identity_runtime",
                "  lua: { api: 1, storage_version: 2,\n"
                "    migrations: [{ from: " +
                    migration +
                    ", to: 2, script: \"migrate.lua\" }],\n"
                    "  },\n"));
    };
    write_manifest("1");
    files.Write(fs::path("data") / "mods" / "identity_runtime" / "config" /
            "main.cfg",
        "");
    files.Write(fs::path("data") / "mods" / "identity_runtime" / "main.lua",
        "services.register('galaxy_generator', 1, function() end)\n"
        "events.on('game.initialized', function() end)\n");
    files.Write(fs::path("data") / "mods" / "identity_runtime" /
            "migrate.lua",
        "return storage\n");
    files.Initialize_Vfs();
    config::s_effective_config effective;
    config::s_cfg_diagnostic diagnostic;

    DOCTEST_REQUIRE(config::Load_Config_File(
        "main.cfg", "base_game", &effective, &diagnostic));
    const std::string original =
        orion2re::lua::Get_Active_Registry()->fingerprint;

    write_manifest("0");
    DOCTEST_REQUIRE(config::Load_Config_File(
        "main.cfg", "base_game", &effective, &diagnostic));
    const std::string changed_edge =
        orion2re::lua::Get_Active_Registry()->fingerprint;
    DOCTEST_CHECK(changed_edge == original);

    files.Write(fs::path("data") / "mods" / "identity_runtime" /
            "main.lua",
        "services.register('galaxy_generator', 2, function() end)\n"
        "events.on('game.initialized', function() end)\n");
    DOCTEST_CHECK_FALSE(config::Load_Config_File(
        "main.cfg", "base_game", &effective, &diagnostic));
    DOCTEST_CHECK(diagnostic.message.find(
                      "unsupported Lua service") != std::string::npos);
    DOCTEST_CHECK(orion2re::lua::Get_Active_Registry()->fingerprint ==
        changed_edge);

    files.Write(fs::path("data") / "mods" / "identity_runtime" /
            "main.lua",
        "services.register('galaxy_generator', 1, function() end)\n"
        "events.on('game.initialized', function() end)\n");
    files.Write(fs::path("data") / "mods" / "identity_runtime" /
            "migrate.lua",
        "return nil\n");
    DOCTEST_REQUIRE(config::Load_Config_File(
        "main.cfg", "base_game", &effective, &diagnostic));
    DOCTEST_CHECK(orion2re::lua::Get_Active_Registry()->fingerprint !=
        changed_edge);
}

DOCTEST_TEST_SUITE("configuration include loader") {
    DOCTEST_TEST_CASE("successful reload refreshes after configuration and VFS commit") {
        s_config_test_directory files;
        files.Write("main.cfg", "interface scroll_interval = 77;\nenable refresh_mod;\n");
        files.Write("resource.txt", "base");
        files.Write(fs::path("data") / "mods" / "refresh_mod" / "mod.json5",
            Manifest_("refresh_mod"));
        files.Write(fs::path("data") / "mods" / "refresh_mod" / "config" / "main.cfg",
            "");
        files.Write(fs::path("data") / "mods" / "refresh_mod" / "data" / "resource.txt",
            "mod");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        TEST_CONFIG_REFRESH::Reset_();
        TEST_CONFIG_REFRESH::Expect_(77, "resource.txt", "mod");
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 1);
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::observed_committed_state);
    }

    DOCTEST_TEST_CASE("failed reload does not refresh game resources") {
        s_config_test_directory files;
        files.Write("main.cfg", "interface scroll_interval = 999;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        TEST_CONFIG_REFRESH::Reset_();
        config::s_effective_config effective;
        DOCTEST_REQUIRE(config::Initialize_Effective_Config(&effective));
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 0);
    }

    DOCTEST_TEST_CASE("nested includes resolve relative paths in statement order") {
        const config::t_variable_id no_outpost_prompt =
            Variable_Id_("interface.no_outpost_prompt");
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        const config::t_variable_id beam_offense =
            Variable_Id_("combat.bonus.battle_scanner_beam_offense");
        DOCTEST_REQUIRE(beam_offense != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("config/main.cfg",
            "interface scroll_interval = 60;\n"
            "include balance/first.cfg;\n"
            "interface no_outpost_prompt = true;\n");
        files.Write("config/balance/first.cfg",
            "combat bonus battle_scanner_beam_offense = 25;\n"
            "include ../shared.cfg;\n"
            "interface scroll_interval = 80;\n");
        files.Write("config/shared.cfg", "interface scroll_interval = 70;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "config/main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(effective.state.values[no_outpost_prompt].bool_value);
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 80);
        DOCTEST_CHECK(effective.state.values[beam_offense].integer_value == 25);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 80);

        config::t_variable_id interval_index = 0;
        DOCTEST_REQUIRE(config::Find_Variable(
            "interface.scroll_interval", std::strlen("interface.scroll_interval"), &interval_index));
        DOCTEST_CHECK(effective.provenance[interval_index].location.source ==
            "config/balance/first.cfg");
        DOCTEST_CHECK(effective.provenance[interval_index].location.line == 3);
        DOCTEST_CHECK(effective.provenance[interval_index].statement == 3);
        DOCTEST_REQUIRE(effective.prior_values[interval_index].size() == 3);
        DOCTEST_CHECK(effective.prior_values[interval_index][0].value.integer_value == 1);
        DOCTEST_CHECK(effective.prior_values[interval_index][1].value.integer_value == 60);
        DOCTEST_CHECK(effective.prior_values[interval_index][2].value.integer_value == 70);
    }

    DOCTEST_TEST_CASE("stop ends only the included file") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        const config::t_variable_id beam_offense =
            Variable_Id_("combat.bonus.battle_scanner_beam_offense");
        DOCTEST_REQUIRE(beam_offense != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("main.cfg",
            "include child.cfg;\n"
            "interface scroll_interval = 75;\n");
        files.Write("child.cfg",
            "combat bonus battle_scanner_beam_offense = 30;\n"
            "stop;\n"
            "combat bonus battle_scanner_beam_offense = 99;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 75);
        DOCTEST_CHECK(effective.state.values[beam_offense].integer_value == 30);
    }

    DOCTEST_TEST_CASE("include cycles report the normalized include chain") {
        s_config_test_directory files;
        files.Write("config/a.cfg", "include sub/b.cfg;\n");
        files.Write("config/sub/b.cfg", "include ../a.cfg;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::Initialize_Effective_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "config/a.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "include cycle: config/a.cfg -> config/sub/b.cfg -> config/a.cfg");
        DOCTEST_CHECK(diagnostic.location.source == "config/sub/b.cfg");
        DOCTEST_CHECK(diagnostic.location.line == 1);
    }

    DOCTEST_TEST_CASE("missing and malformed includes are distinct and roll back") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(scroll_interval != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("missing-main.cfg",
            "interface scroll_interval = 80;\n"
            "include absent.cfg;\n");
        files.Write("malformed-main.cfg",
            "interface scroll_interval = 80;\n"
            "include malformed.cfg;\n");
        files.Write("malformed.cfg", "interface scroll_interval = 0x;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "missing-main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "configuration file not found: 'absent.cfg'");
        DOCTEST_CHECK(diagnostic.location.source == "missing-main.cfg");
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 1);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 1);

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "malformed-main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message == "malformed numeric value '0x'");
        DOCTEST_CHECK(diagnostic.location.source == "malformed.cfg");
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 1);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 1);
    }

    DOCTEST_TEST_CASE("invalid include paths cannot escape the VFS root") {
        s_config_test_directory files;
        files.Write("config/main.cfg", "include ../../outside.cfg;\n");
        files.Write("absolute.cfg", "include /outside.cfg;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::Initialize_Effective_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "config/main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message == "invalid include path '../../outside.cfg'");

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "absolute.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message == "invalid include path '/outside.cfg'");
    }

    DOCTEST_TEST_CASE("missing enabled package is an error and rolls back configuration") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(scroll_interval != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("main.cfg",
            "interface scroll_interval = 80;\n"
            "enable balance_mod;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message == "package not found: 'balance_mod'");
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 1);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 1);
    }

    DOCTEST_TEST_CASE("first enable wins configuration and VFS resource precedence") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(scroll_interval != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("main.cfg", "enable first;\nenable second;\n");
        files.Write(fs::path("data") / "mods" / "first" / "mod.json5",
            Manifest_("first"));
        files.Write(fs::path("data") / "mods" / "first" / "config" / "main.cfg",
            "interface scroll_interval = 81;\n");
        files.Write(fs::path("data") / "mods" / "first" / "data" / "priority.dat",
            "first");
        files.Write(fs::path("data") / "mods" / "second" / "mod.json5",
            Manifest_("second"));
        files.Write(fs::path("data") / "mods" / "second" / "config" / "main.cfg",
            "interface scroll_interval = 62;\n");
        files.Write(fs::path("data") / "mods" / "second" / "data" / "priority.dat",
            "second");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 81);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 81);
        DOCTEST_CHECK(Read_Vfs_Text_("priority.dat") == "first");
        DOCTEST_CHECK(effective.provenance[scroll_interval].package_id == "first");
    }

    DOCTEST_TEST_CASE("loaded mod API exposes resolved manifests in priority order") {
        s_config_test_directory files;
        files.Write("main.cfg", "enable parent;\nenable second;\n");
        files.Write(fs::path("data") / "mods" / "parent" / "mod.json5",
            "{ id: \"parent\", name: \"Parent Mod\", version: \"2.3.4\",\n"
            "  requires: { engine: \">=2.0.0 <3.0.0\" },\n"
            "  dependencies: [{ id: \"dependency\", version: \"1.0.0\" }],\n"
            "  config: \"config/main.cfg\" }\n");
        files.Write(fs::path("data") / "mods" / "parent" / "config" /
                "main.cfg",
            "");
        files.Write(fs::path("data") / "mods" / "dependency" / "mod.json5",
            Manifest_("dependency"));
        files.Write(fs::path("data") / "mods" / "dependency" / "config" /
                "main.cfg",
            "");
        files.Write(fs::path("data") / "mods" / "second" / "mod.json5",
            Manifest_("second"));
        files.Write(fs::path("data") / "mods" / "second" / "config" /
                "main.cfg",
            "");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));

        const config::t_loaded_mod_list& loaded_mods =
            config::Get_Loaded_Mods();
        DOCTEST_REQUIRE(loaded_mods.size() == 3);
        DOCTEST_CHECK(loaded_mods[0].priority_index == 0);
        DOCTEST_CHECK(loaded_mods[0].manifest.id == "parent");
        DOCTEST_CHECK(loaded_mods[0].manifest.name == "Parent Mod");
        DOCTEST_CHECK(loaded_mods[0].manifest.version_text == "2.3.4");
        DOCTEST_CHECK(loaded_mods[0].manifest.version.major == 2);
        DOCTEST_CHECK(loaded_mods[0].manifest.version.minor == 3);
        DOCTEST_CHECK(loaded_mods[0].manifest.version.patch == 4);
        DOCTEST_CHECK(loaded_mods[1].priority_index == 1);
        DOCTEST_CHECK(loaded_mods[1].manifest.id == "second");
        DOCTEST_CHECK(loaded_mods[2].priority_index == 2);
        DOCTEST_CHECK(loaded_mods[2].manifest.id == "dependency");
        DOCTEST_CHECK(config::Find_Loaded_Mod("parent") == &loaded_mods[0]);
        DOCTEST_CHECK(config::Find_Loaded_Mod("missing") == nullptr);
    }

    DOCTEST_TEST_CASE("short mod versions are zero-filled and preserve display text") {
        s_config_test_directory files;
        files.Write("main.cfg", "enable major;\nenable minor;\n");
        files.Write(fs::path("data") / "mods" / "major" / "mod.json5",
            Versioned_Manifest_("major", "2"));
        files.Write(fs::path("data") / "mods" / "major" / "config" /
                "main.cfg",
            "");
        files.Write(fs::path("data") / "mods" / "minor" / "mod.json5",
            Versioned_Manifest_("minor", "3.4"));
        files.Write(fs::path("data") / "mods" / "minor" / "config" /
                "main.cfg",
            "");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        const config::t_loaded_mod_list& loaded_mods =
            config::Get_Loaded_Mods();
        DOCTEST_REQUIRE(loaded_mods.size() == 2);
        DOCTEST_CHECK(loaded_mods[0].manifest.version_text == "2");
        DOCTEST_CHECK(loaded_mods[0].manifest.version.major == 2);
        DOCTEST_CHECK(loaded_mods[0].manifest.version.minor ==
            config::SEMANTIC_VERSION_COMPONENT_MISSING);
        DOCTEST_CHECK(loaded_mods[0].manifest.version.patch ==
            config::SEMANTIC_VERSION_COMPONENT_MISSING);
        DOCTEST_CHECK(loaded_mods[1].manifest.version_text == "3.4");
        DOCTEST_CHECK(loaded_mods[1].manifest.version.major == 3);
        DOCTEST_CHECK(loaded_mods[1].manifest.version.minor == 4);
        DOCTEST_CHECK(loaded_mods[1].manifest.version.patch ==
            config::SEMANTIC_VERSION_COMPONENT_MISSING);
        DOCTEST_CHECK(config::Validate_Semantic_Version_Range(
            ">=2 <3.4.1"));
        DOCTEST_CHECK(config::Semantic_Version_Matches(
            loaded_mods[0].manifest.version, ">=2 <2.1"));
        DOCTEST_CHECK_FALSE(config::Semantic_Version_Matches(
            loaded_mods[0].manifest.version, ">=2.1"));
        DOCTEST_CHECK(config::Semantic_Versions_Equal("2", "2.0.0"));
        DOCTEST_CHECK(config::Semantic_Versions_Equal("3.4", "3.4.0"));
        config::s_semantic_version invalid;
        DOCTEST_CHECK_FALSE(config::Parse_Semantic_Version("1.", &invalid));
        DOCTEST_CHECK_FALSE(config::Parse_Semantic_Version("1.2.3.4", &invalid));
    }

    DOCTEST_TEST_CASE("package manifest description is optional presentation metadata") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(scroll_interval != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("main.cfg", "enable described;\n");
        files.Write(fs::path("data") / "mods" / "described" / "mod.json5",
            Manifest_("described",
                "  description: \"A package description.\",\n"));
        files.Write(fs::path("data") / "mods" / "described" / "config" / "main.cfg",
            "interface scroll_interval = 76;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 76);
        config::s_mod_manifest manifest;
        DOCTEST_REQUIRE(config::Load_Mod_Manifest("described", &manifest, &diagnostic));
        DOCTEST_CHECK(manifest.description == "A package description.");
    }

    DOCTEST_TEST_CASE("removed Lua integration manifest is rejected") {
        s_config_test_directory files;
        files.Write("main.cfg", "enable scripted;\n");
        files.Write(fs::path("data") / "mods" / "scripted" / "mod.json5",
            Manifest_("scripted",
                "  integrations: { lua: { api: 1, listeners: [] } },\n"));
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "unknown manifest property 'integrations'");
    }

    DOCTEST_TEST_CASE("direct dependency is loaded once below its package") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        const config::t_variable_id beam_offense =
            Variable_Id_("combat.bonus.battle_scanner_beam_offense");
        DOCTEST_REQUIRE(beam_offense != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("main.cfg", "enable parent;\n");
        files.Write(fs::path("data") / "mods" / "parent" / "mod.json5",
            Manifest_("parent",
                "  dependencies: [{ id: \"shared\", version: \">=1.0.0\" }],\n"));
        files.Write(fs::path("data") / "mods" / "parent" / "config" / "main.cfg",
            "interface scroll_interval = 77;\n");
        files.Write(fs::path("data") / "mods" / "shared" / "mod.json5",
            Manifest_("shared"));
        files.Write(fs::path("data") / "mods" / "shared" / "config" / "main.cfg",
            "interface scroll_interval = 55;\n"
            "combat bonus battle_scanner_beam_offense = 42;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 77);
        DOCTEST_CHECK(effective.state.values[beam_offense].integer_value == 42);
        DOCTEST_CHECK(effective.provenance[scroll_interval].package_id == "parent");
        DOCTEST_CHECK(effective.provenance[beam_offense].package_id == "shared");
    }

    DOCTEST_TEST_CASE("later explicit enable outranks dependencies of an earlier package") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(scroll_interval != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("main.cfg", "enable parent;\nenable explicit_mod;\n");
        files.Write(fs::path("data") / "mods" / "parent" / "mod.json5",
            Manifest_("parent",
                "  dependencies: [{ id: \"dependency\", version: \"1.0.0\" }],\n"));
        files.Write(fs::path("data") / "mods" / "parent" / "config" / "main.cfg",
            "interface no_outpost_prompt = true;\n");
        files.Write(fs::path("data") / "mods" / "dependency" / "mod.json5",
            Manifest_("dependency"));
        files.Write(fs::path("data") / "mods" / "dependency" / "config" / "main.cfg",
            "interface scroll_interval = 51;\n");
        files.Write(fs::path("data") / "mods" / "dependency" / "data" / "priority.dat",
            "dependency");
        files.Write(fs::path("data") / "mods" / "explicit_mod" / "mod.json5",
            Manifest_("explicit_mod"));
        files.Write(fs::path("data") / "mods" / "explicit_mod" / "config" / "main.cfg",
            "interface scroll_interval = 69;\n");
        files.Write(fs::path("data") / "mods" / "explicit_mod" / "data" / "priority.dat",
            "explicit");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 69);
        DOCTEST_CHECK(effective.provenance[scroll_interval].package_id == "explicit_mod");
        DOCTEST_CHECK(Read_Vfs_Text_("priority.dat") == "explicit");
    }

    DOCTEST_TEST_CASE("nested enable resolves a package and preserves parent priority") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(scroll_interval != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("main.cfg", "enable parent;\n");
        files.Write(fs::path("data") / "mods" / "parent" / "mod.json5",
            Manifest_("parent"));
        files.Write(fs::path("data") / "mods" / "parent" / "config" / "main.cfg",
            "enable nested;\ninterface scroll_interval = 79;\n");
        files.Write(fs::path("data") / "mods" / "nested" / "mod.json5",
            Manifest_("nested"));
        files.Write(fs::path("data") / "mods" / "nested" / "config" / "main.cfg",
            "interface scroll_interval = 58;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 79);
    }

    DOCTEST_TEST_CASE("ZIP package manifest config and data use projected virtual paths") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(scroll_interval != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("main.cfg", "enable zipped;\n");
        files.Write_Zip(fs::path("data") / "mods" / "zipped.zip", {
                                                                      {"mod.json5", Manifest_("zipped")},
                                                                      {"config/main.cfg", "interface scroll_interval = 73;\n"},
                                                                      {"data/zipped.dat", "zip-resource"},
                                                                  });
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(effective.state.values[scroll_interval].integer_value == 73);
        DOCTEST_CHECK(Read_Vfs_Text_("zipped.dat") == "zip-resource");
    }

    DOCTEST_TEST_CASE("incompatible engine manifest fails with semantic version diagnostic") {
        s_config_test_directory files;
        files.Write("main.cfg", "enable future;\n");
        files.Write(fs::path("data") / "mods" / "future" / "mod.json5",
            "{ id: \"future\", name: \"Future\", version: \"1.0.0\",\n"
            "  requires: { engine: \">=3.0.0\" }, config: \"config/main.cfg\" }\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::Initialize_Effective_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            std::string("package requires engine '>=3.0.0' but this engine is ") + orion2re::ENGINE_VERSION);
        DOCTEST_CHECK(diagnostic.location.source == "mods/future/mod.json5");
    }

    DOCTEST_TEST_CASE("invalid compatible version range is rejected") {
        s_config_test_directory files;
        files.Write("main.cfg", "enable invalid_compatibility;\n");
        files.Write(fs::path("data") / "mods" / "invalid_compatibility" / "mod.json5",
            Versioned_Manifest_(
                "invalid_compatibility", "1.0.0", "^1.0.0"));
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "invalid compatible version range '^1.0.0'");
    }

    DOCTEST_TEST_CASE("dependency cycle fails without committing configuration or resources") {
        s_config_test_directory files;
        files.Write("main.cfg", "interface scroll_interval = 88;\nenable alpha;\n");
        files.Write(fs::path("data") / "mods" / "alpha" / "mod.json5",
            Manifest_("alpha",
                "  dependencies: [{ id: \"beta\", version: \"1.0.0\" }],\n"));
        files.Write(fs::path("data") / "mods" / "alpha" / "config" / "main.cfg", "");
        files.Write(fs::path("data") / "mods" / "alpha" / "data" / "failed.dat", "bad");
        files.Write(fs::path("data") / "mods" / "beta" / "mod.json5",
            Manifest_("beta",
                "  dependencies: [{ id: \"alpha\", version: \"1.0.0\" }],\n"));
        files.Write(fs::path("data") / "mods" / "beta" / "config" / "main.cfg", "");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "mod dependency cycle: alpha -> beta -> alpha");
        DOCTEST_CHECK(COMBINIT::scroll_interval == 1);
        DOCTEST_CHECK(vfs::File_Exists("failed.dat") == 0);
    }

    DOCTEST_TEST_CASE("conflicting packages fail atomically") {
        s_config_test_directory files;
        files.Write("main.cfg", "enable alpha;\nenable beta;\n");
        files.Write(fs::path("data") / "mods" / "alpha" / "mod.json5",
            Manifest_("alpha", "  conflicts: [\"beta\"],\n"));
        files.Write(fs::path("data") / "mods" / "alpha" / "config" / "main.cfg", "");
        files.Write(fs::path("data") / "mods" / "beta" / "mod.json5",
            Manifest_("beta"));
        files.Write(fs::path("data") / "mods" / "beta" / "config" / "main.cfg", "");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::Initialize_Effective_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "package 'alpha' conflicts with active package 'beta'");
    }

    DOCTEST_TEST_CASE("optional missing dependency is skipped but an installed one is validated") {
        s_config_test_directory files;
        files.Write("main.cfg", "enable parent;\n");
        files.Write(fs::path("data") / "mods" / "parent" / "mod.json5",
            Manifest_("parent",
                "  optional_dependencies: [{ id: \"optional\", version: \">=2.0.0\" }],\n"));
        files.Write(fs::path("data") / "mods" / "parent" / "config" / "main.cfg",
            "interface scroll_interval = 64;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 64);
    }

    DOCTEST_TEST_CASE("dependency version mismatch is fatal") {
        s_config_test_directory files;
        files.Write("main.cfg", "enable parent;\n");
        files.Write(fs::path("data") / "mods" / "parent" / "mod.json5",
            Manifest_("parent",
                "  dependencies: [{ id: \"old\", version: \">=2.0.0\" }],\n"));
        files.Write(fs::path("data") / "mods" / "parent" / "config" / "main.cfg", "");
        files.Write(fs::path("data") / "mods" / "old" / "mod.json5", Manifest_("old"));
        files.Write(fs::path("data") / "mods" / "old" / "config" / "main.cfg", "");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::Initialize_Effective_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "package 'old' version 1.0.0 does not satisfy '>=2.0.0'");
    }

    DOCTEST_TEST_CASE("malformed duplicate and mismatched manifests report distinct errors") {
        s_config_test_directory files;
        files.Write("malformed.cfg", "enable malformed;\n");
        files.Write("duplicate.cfg", "enable duplicate;\n");
        files.Write("mismatch.cfg", "enable mismatch;\n");
        files.Write("capabilities.cfg", "enable obsolete;\n");
        files.Write("description.cfg", "enable invalid_description;\n");
        files.Write(fs::path("data") / "mods" / "malformed" / "mod.json5", "{ id:");
        files.Write(fs::path("data") / "mods" / "duplicate" / "mod.json5",
            "{ id: \"duplicate\", id: \"duplicate\", name: \"Duplicate\",\n"
            "  version: \"1.0.0\", requires: { engine: \">=2.0.0 <3.0.0\" },\n"
            "  config: \"config/main.cfg\" }\n");
        files.Write(fs::path("data") / "mods" / "mismatch" / "mod.json5",
            "{ id: \"other\", name: \"Other\", version: \"1.0.0\",\n"
            "  requires: { engine: \">=2.0.0 <3.0.0\" }, config: \"config/main.cfg\" }\n");
        files.Write(fs::path("data") / "mods" / "obsolete" / "mod.json5",
            "{ id: \"obsolete\", name: \"Obsolete\", version: \"1.0.0\",\n"
            "  requires: { engine: \">=2.0.0 <3.0.0\" }, config: \"config/main.cfg\",\n"
            "  capabilities: [\"configuration\"] }\n");
        files.Write(fs::path("data") / "mods" / "invalid_description" / "mod.json5",
            "{ id: \"invalid_description\", name: \"Invalid description\",\n"
            "  description: \"\", version: \"1.0.0\",\n"
            "  requires: { engine: \">=2.0.0 <3.0.0\" }, config: \"config/main.cfg\" }\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::Initialize_Effective_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "malformed.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message.find("invalid mod manifest:") == 0);
        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "duplicate.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message == "duplicate manifest property 'id'");
        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "mismatch.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "manifest ID 'other' does not match package 'mismatch'");
        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "capabilities.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message == "unknown manifest property 'capabilities'");
        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "description.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "manifest property 'description' must be a non-empty string");
    }

    DOCTEST_TEST_CASE("load after constraint rejects contradictory explicit priority") {
        s_config_test_directory files;
        files.Write("main.cfg", "enable first;\nenable second;\n");
        files.Write(fs::path("data") / "mods" / "first" / "mod.json5",
            Manifest_("first"));
        files.Write(fs::path("data") / "mods" / "first" / "config" / "main.cfg", "");
        files.Write(fs::path("data") / "mods" / "second" / "mod.json5",
            Manifest_("second", "  load_after: [\"first\"],\n"));
        files.Write(fs::path("data") / "mods" / "second" / "config" / "main.cfg", "");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::Initialize_Effective_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message == "package 'second' must load after 'first'");
    }

    DOCTEST_TEST_CASE("failed reload preserves previously committed package layers and values") {
        s_config_test_directory files;
        files.Write("valid.cfg", "enable stable;\n");
        files.Write("invalid.cfg", "enable broken;\n");
        files.Write(fs::path("data") / "mods" / "stable" / "mod.json5",
            Manifest_("stable"));
        files.Write(fs::path("data") / "mods" / "stable" / "config" / "main.cfg",
            "interface scroll_interval = 74;\n");
        files.Write(fs::path("data") / "mods" / "stable" / "data" / "stable.dat", "stable");
        files.Write(fs::path("data") / "mods" / "broken" / "mod.json5",
            Manifest_("broken"));
        files.Write(fs::path("data") / "mods" / "broken" / "config" / "main.cfg",
            "interface scroll_interval = 256;\n");
        files.Write(fs::path("data") / "mods" / "broken" / "data" / "broken.dat", "broken");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "valid.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_REQUIRE(config::Get_Loaded_Mods().size() == 1);
        DOCTEST_CHECK(config::Get_Loaded_Mods()[0].manifest.id == "stable");
        DOCTEST_CHECK(Read_Vfs_Text_("stable.dat") == "stable");
        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "invalid.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 74);
        DOCTEST_CHECK(Read_Vfs_Text_("stable.dat") == "stable");
        DOCTEST_CHECK(vfs::File_Exists("broken.dat") == 0);
        DOCTEST_REQUIRE(config::Get_Loaded_Mods().size() == 1);
        DOCTEST_CHECK(config::Get_Loaded_Mods()[0].manifest.id == "stable");
    }

    DOCTEST_TEST_CASE("Lua fingerprints are canonical across directory and ZIP packages") {
        const std::string runtime =
            "  lua: { api: 1, storage_version: 1 },\n";
        const std::string source =
            "events.on('game.initialized', function() end)\n";
        orion2re::lua::s_lua_registry directory_registry;
        {
            s_config_test_directory files;
            files.Write("main.cfg", "enable scripted;\n");
            files.Write(fs::path("data") / "mods" / "scripted" / "mod.json5",
                Manifest_("scripted", runtime));
            files.Write(fs::path("data") / "mods" / "scripted" / "config" /
                    "main.cfg",
                "");
            files.Write(fs::path("data") / "mods" / "scripted" / "main.lua",
                source);
            files.Initialize_Vfs();
            config::s_effective_config effective;
            config::s_cfg_diagnostic diagnostic;
            DOCTEST_REQUIRE(config::Load_Config_File(
                "main.cfg", "base_game", &effective, &diagnostic));
            directory_registry = *orion2re::lua::Get_Active_Registry();
            DOCTEST_REQUIRE_FALSE(directory_registry.fingerprint.empty());
            DOCTEST_CHECK(directory_registry.fingerprint_serialization.size() > 32);
            DOCTEST_CHECK(directory_registry.fingerprint_serialization[0] == 'O');
            DOCTEST_CHECK(directory_registry.fingerprint_serialization[1] == '2');
            DOCTEST_CHECK(directory_registry.fingerprint_serialization[2] == 'L');
            DOCTEST_CHECK(directory_registry.fingerprint_serialization[3] == 'S');
        }

        s_config_test_directory files;
        files.Write("main.cfg", "enable scripted;\n");
        files.Write_Zip(fs::path("data") / "mods" / "scripted.zip", {
                                                                        {"mod.json5", Manifest_("scripted", runtime)},
                                                                        {"config/main.cfg", ""},
                                                                        {"main.lua", source},
                                                                    });
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(config::Load_Config_File(
            "main.cfg", "base_game", &effective, &diagnostic));
        const orion2re::lua::s_lua_registry* archive_registry =
            orion2re::lua::Get_Active_Registry();
        DOCTEST_CHECK(archive_registry->fingerprint == directory_registry.fingerprint);
        DOCTEST_CHECK(archive_registry->fingerprint_serialization ==
            directory_registry.fingerprint_serialization);
        std::string mismatch;
        DOCTEST_CHECK(orion2re::lua::Compare_Registry_Fingerprints(
            &directory_registry, archive_registry, &mismatch));
    }

    DOCTEST_TEST_CASE("Lua source hashes are independent of package order and configuration") {
        s_config_test_directory files;
        files.Write("first.cfg", "enable first;\nenable second;\n");
        files.Write("second.cfg", "enable second;\nenable first;\n");
        files.Write("configured.cfg",
            "race picks starting = 12;\nenable first;\nenable second;\n");
        const std::string runtime =
            "  lua: { api: 1, storage_version: 1 },\n";
        for (const std::string& id : {std::string("first"), std::string("second")}) {
            files.Write(fs::path("data") / "mods" / id / "mod.json5",
                Manifest_(id, runtime));
            files.Write(fs::path("data") / "mods" / id / "config" / "main.cfg", "");
            files.Write(fs::path("data") / "mods" / id / "main.lua",
                "events.on('game.initialized', function() end)\n");
        }
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "first.cfg", "base_game", &effective, &diagnostic));
        const orion2re::lua::s_lua_registry first =
            *orion2re::lua::Get_Active_Registry();
        DOCTEST_REQUIRE(config::Load_Config_File(
            "second.cfg", "base_game", &effective, &diagnostic));
        const orion2re::lua::s_lua_registry reordered =
            *orion2re::lua::Get_Active_Registry();
        DOCTEST_CHECK(first.fingerprint == reordered.fingerprint);
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&first, &reordered) ==
            "Lua package order differs at index 0: expected 'first', got 'second'");

        DOCTEST_REQUIRE(config::Load_Config_File(
            "configured.cfg", "base_game", &effective, &diagnostic));
        const orion2re::lua::s_lua_registry configured =
            *orion2re::lua::Get_Active_Registry();
        DOCTEST_CHECK(first.fingerprint == configured.fingerprint);
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&first, &configured) ==
            "synchronized gameplay/technology configuration differs");

        files.Write(fs::path("data") / "mods" / "first" / "main.lua",
            "events.on('game.initialized', function() return 1 end)\n");
        DOCTEST_REQUIRE(config::Load_Config_File(
            "first.cfg", "base_game", &effective, &diagnostic));
        const orion2re::lua::s_lua_registry changed_source =
            *orion2re::lua::Get_Active_Registry();
        DOCTEST_CHECK(first.fingerprint != changed_source.fingerprint);
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&first, &changed_source)
                          .find("source hash differs") != std::string::npos);
    }

    DOCTEST_TEST_CASE("Lua compatibility identifies package runtime config and source mismatches") {
        orion2re::lua::s_lua_registry expected;
        expected.packages_high_to_low_priority = {{"example", "1.0.0"}};
        orion2re::lua::s_lua_runtime_package_record runtime_package;
        runtime_package.package_id = "example";
        runtime_package.package_version = "1.0.0";
        runtime_package.api = 1;
        runtime_package.storage_version = 1;
        runtime_package.sources.push_back(
            {"mods/example/main.lua", "hash"});
        expected.runtime_packages.push_back(runtime_package);
        expected.synchronized_config_hash = "config";
        expected.fingerprint = "expected";

        orion2re::lua::s_lua_registry actual = expected;
        actual.fingerprint = "actual";
        actual.packages_high_to_low_priority[0].version = "2.0.0";
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&expected, &actual)
                          .find("version differs") != std::string::npos);
        actual = expected;
        actual.packages_high_to_low_priority[0].version = "1";
        actual.runtime_packages[0].package_version = "1.0";
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&expected, &actual).empty());
        actual = expected;
        actual.fingerprint = "actual";
        actual.lua_version = "5.6.0";
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&expected, &actual)
                          .find("runtime version differs") != std::string::npos);
        actual = expected;
        actual.fingerprint = "actual";
        actual.runtime_packages[0].api = 2;
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&expected, &actual)
                          .find("package/API/storage identity differs") !=
            std::string::npos);
        actual = expected;
        actual.fingerprint = "actual";
        actual.runtime_packages[0].storage_version = 2;
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&expected, &actual)
                          .find("package/API/storage identity differs") !=
            std::string::npos);
        actual = expected;
        actual.fingerprint = "actual";
        actual.runtime_packages[0].sources[0].path =
            "mods/example/other.lua";
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&expected, &actual)
                          .find("source path differs") != std::string::npos);
        actual = expected;
        actual.fingerprint = "actual";
        actual.runtime_packages[0].sources[0].source_hash =
            "other-hash";
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&expected, &actual)
                          .find("source hash differs") != std::string::npos);
        actual = expected;
        actual.packages_high_to_low_priority[0].version = "2.0.0";
        actual.fingerprint = expected.fingerprint;
        DOCTEST_CHECK(Lua_Fingerprint_Mismatch_(&expected, &actual)
                          .find("version differs") != std::string::npos);
    }

    DOCTEST_TEST_CASE("Lua save reader discards removed listeners from versions 4 and 5") {
        const auto append_u32 = [](std::vector<uint8_t>* bytes, uint32_t value) {
            for (int shift = 0; shift < 32; shift += 8) {
                bytes->push_back(static_cast<uint8_t>(value >> shift));
            }
        };
        const auto append_string = [&append_u32](std::vector<uint8_t>* bytes,
                                       const std::string& value) {
            append_u32(bytes, static_cast<uint32_t>(value.size()));
            bytes->insert(bytes->end(), value.begin(), value.end());
        };
        const auto store_u32 = [](std::vector<uint8_t>* bytes,
                                   size_t offset,
                                   uint32_t value) {
            for (int shift = 0; shift < 32; shift += 8) {
                (*bytes)[offset++] = static_cast<uint8_t>(value >> shift);
            }
        };

        for (const uint32_t version : {4U, 5U}) {
            DOCTEST_CAPTURE(version);
            orion2re::lua::s_lua_save_snapshot snapshot;
            const std::string marker = "historical-prototype-marker";
            snapshot.registry.prototype_identity = marker;
            const uint32_t current_size =
                orion2re::lua::Lua_Save_Snapshot_Size(&snapshot);
            DOCTEST_REQUIRE(current_size != 0);
            std::vector<uint8_t> encoded(current_size);
            SERIAL::Writer_ writer(
                encoded.data(), encoded.size(), "Lua save snapshot overflow");
            orion2re::lua::Write_Lua_Save_Snapshot(&writer, &snapshot);
            DOCTEST_REQUIRE(writer.Length() == current_size);

            const auto marker_position = std::search(
                encoded.begin(), encoded.end(), marker.begin(), marker.end());
            DOCTEST_REQUIRE(marker_position != encoded.end());
            const size_t marker_offset =
                static_cast<size_t>(marker_position - encoded.begin());
            DOCTEST_REQUIRE(marker_offset >= sizeof(uint32_t));

            std::vector<uint8_t> legacy_listeners;
            if (version == 4) {
                append_u32(&legacy_listeners, 0);
            }
            append_u32(&legacy_listeners, 1);
            append_string(&legacy_listeners, version == 4 ? "example:generated" : "example:pregame");
            append_string(&legacy_listeners, "example");
            append_string(&legacy_listeners, "1.0.0");
            append_string(&legacy_listeners, version == 4 ? "generated" : "pregame");
            append_u32(&legacy_listeners, version == 4 ? 1 : 0);
            append_string(&legacy_listeners, version == 4 ? "new_game.generated" : "new_game.before_race_selection");
            append_u32(&legacy_listeners, 1);
            append_u32(&legacy_listeners, 1);
            append_string(&legacy_listeners, "mods/example/legacy.lua");
            append_string(&legacy_listeners, "run");
            append_u32(&legacy_listeners, 0);
            append_string(&legacy_listeners, "source-hash");
            append_u32(&legacy_listeners, 0);
            append_u32(&legacy_listeners, 0);

            encoded.insert(encoded.begin() +
                    static_cast<std::vector<uint8_t>::difference_type>(
                        marker_offset - sizeof(uint32_t)),
                legacy_listeners.begin(),
                legacy_listeners.end());
            store_u32(&encoded, 0, static_cast<uint32_t>(encoded.size() - sizeof(uint32_t)));
            store_u32(&encoded, 2 * sizeof(uint32_t), version);

            SERIAL::Reader_ reader(
                encoded.data(), encoded.size(), "Lua save snapshot underflow");
            orion2re::lua::s_lua_save_snapshot decoded;
            std::string diagnostic;
            DOCTEST_REQUIRE(orion2re::lua::Read_Lua_Save_Snapshot(
                &reader, &decoded, &diagnostic));
            DOCTEST_CHECK(reader.Remaining() == 0);
            DOCTEST_CHECK(decoded.registry.prototype_identity == marker);
        }
    }

    DOCTEST_TEST_CASE("mount commit failure preserves configuration metadata arenas and mounts") {
        s_config_test_directory files;
        files.Write("stable.cfg", "interface scroll_interval = 74;\nenable stable;\n");
        files.Write(fs::path("data") / "mods" / "stable" / "mod.json5",
            Manifest_("stable"));
        files.Write(fs::path("data") / "mods" / "stable" / "config" / "main.cfg", "");
        files.Write(fs::path("data") / "mods" / "stable" / "data" / "stable.dat", "stable");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "stable.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_REQUIRE(loaded);

        const config::s_effective_config prior_effective = effective;
        const config::s_active_config* active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        const std::vector<std::string> prior_package_order = active->package_order;
        char* prior_tech_name = TECHDATA::shields[0].name;
        arena::s_arena* prior_arena =
            config::Acquire_String_Arena(config::CONFIG_GROUP_TECHNOLOGY);
        DOCTEST_REQUIRE(prior_arena != nullptr);
        TEST_CONFIG_REFRESH::Reset_();

        config::s_cfg_document candidate_document;
        const char candidate_source[] = "interface scroll_interval = 88;\n";
        DOCTEST_REQUIRE(config::Parse_Cfg(
            "candidate.cfg", candidate_source, sizeof(candidate_source) - 1, &candidate_document, &diagnostic));
        DOCTEST_CHECK_FALSE(config::Apply_Config_With_Mods(
            candidate_document, "base_game", &effective, &diagnostic, Unused_Document_Loader_, nullptr, Fail_Mount_Commit_));

        DOCTEST_CHECK(diagnostic.message == "unable to commit VFS mount transaction");
        DOCTEST_CHECK(COMBINIT::scroll_interval == 74);
        Check_Effective_Config_Equals_(effective, prior_effective);
        DOCTEST_CHECK(Read_Vfs_Text_("stable.dat") == "stable");
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 0);
        active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        DOCTEST_CHECK(active->package_order == prior_package_order);
        DOCTEST_CHECK(TECHDATA::shields[0].name == prior_tech_name);
        arena::s_arena* current_arena =
            config::Acquire_String_Arena(config::CONFIG_GROUP_TECHNOLOGY);
        DOCTEST_CHECK(current_arena == prior_arena);
        arena::Release(current_arena);
        arena::Release(prior_arena);
    }

    DOCTEST_TEST_CASE("successful reload rebuilds configuration and package layers from defaults") {
        const config::t_variable_id scroll_interval =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(scroll_interval != config::CONFIG_NO_VARIABLE);
        s_config_test_directory files;
        files.Write("modded.cfg", "enable stable;\n");
        files.Write("plain.cfg", "combat bonus battle_scanner_beam_offense = 31;\n");
        files.Write(fs::path("data") / "mods" / "stable" / "mod.json5",
            Manifest_("stable"));
        files.Write(fs::path("data") / "mods" / "stable" / "config" / "main.cfg",
            "interface scroll_interval = 74;\n");
        files.Write(fs::path("data") / "mods" / "stable" / "data" / "stable.dat", "stable");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::Read_Effective_Native_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_REQUIRE(config::Load_Config_File(
            "modded.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 74);
        DOCTEST_CHECK(Read_Vfs_Text_("stable.dat") == "stable");

        DOCTEST_REQUIRE(config::Load_Config_File(
            "plain.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 1);
        DOCTEST_CHECK(CMBTFIR2::combat_bonus.battle_scanner_beam_offense == 31);
        DOCTEST_CHECK(vfs::File_Exists("stable.dat") == 0);
        DOCTEST_CHECK_FALSE(effective.provenance[scroll_interval].has_override);
        DOCTEST_CHECK(effective.prior_values[scroll_interval].empty());
    }

    DOCTEST_TEST_CASE("ambiguous package forms and unsafe ZIP paths are fatal") {
        s_config_test_directory files;
        files.Write("ambiguous.cfg", "enable ambiguous;\n");
        files.Write("unsafe.cfg", "enable unsafe;\n");
        files.Write(fs::path("data") / "mods" / "ambiguous" / "mod.json5",
            Manifest_("ambiguous"));
        files.Write_Zip(fs::path("data") / "mods" / "ambiguous.zip",
            {{"mod.json5", Manifest_("ambiguous")}});
        files.Write_Zip(fs::path("data") / "mods" / "unsafe.zip", {
                                                                      {"mod.json5", Manifest_("unsafe")},
                                                                      {"../outside.dat", "unsafe"},
                                                                  });
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::Initialize_Effective_Config(&effective);
        config::s_cfg_diagnostic diagnostic;

        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "ambiguous.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "package 'ambiguous' has both directory and ZIP forms");
        DOCTEST_CHECK_FALSE(config::Load_Config_File(
            "unsafe.cfg", "base_game", &effective, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "unsafe path in package ZIP: '../outside.dat'");
    }

    DOCTEST_TEST_CASE("startup config uses defaults when absent") {
        s_config_test_directory files;
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = true;

        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_CHECK_FALSE(loaded);
    }

    DOCTEST_TEST_CASE("optional user config loads without a main config and can enable mods") {
        s_config_test_directory files;
        files.Write("data/user.cfg", "interface scroll_interval = 75;\nenable personal;\n");
        files.Write("data/mods/personal/mod.json5", Manifest_("personal"));
        files.Write("data/mods/personal/config/main.cfg",
            "interface scroll_interval = 60;\n"
            "combat bonus battle_scanner_beam_offense = 31;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;

        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_CHECK(loaded);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 60);
        DOCTEST_CHECK(CMBTFIR2::combat_bonus.battle_scanner_beam_offense == 31);
        DOCTEST_CHECK(config::Find_Loaded_Mod("personal") != nullptr);
    }

    DOCTEST_TEST_CASE("main and user configs validate their combined final state") {
        s_config_test_directory files;
        files.Write("configs/main.cfg",
            "interface scroll_interval = 20;\n"
            "map star_count huge = 72;\n"
            "stop;\n"
            "interface scroll_interval = 256;\n");
        files.Write("data/user.cfg", "include preferences/build.cfg;\n");
        files.Write("data/preferences/build.cfg",
            "colony build_queue 1 = prepend design1 end;\n"
            "include display.cfg;\n");
        files.Write("data/preferences/display.cfg",
            "interface scroll_interval = 75;\nmap star_count huge = 70;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;

        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "configs/main.cfg", "base_game", &effective, &loaded, &diagnostic, true));
        DOCTEST_CHECK(loaded);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 75);
        DOCTEST_CHECK(BUILD_QUEUE::presets[1][0] == BUILD_QUEUE::INSTRUCTION_PREPEND);
        const auto scroll_interval = Variable_Id_("interface.scroll_interval");
        DOCTEST_CHECK(effective.provenance[scroll_interval].location.source == "preferences/display.cfg");

        files.Write("data/preferences/display.cfg",
            "interface scroll_interval = 80;\nmap star_count huge = 70;\n");
        DOCTEST_REQUIRE(config::Reload_All_Active_Config_Groups(&diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 80);
    }

    DOCTEST_TEST_CASE("user config changes are discovered and errors preserve active state") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "interface scroll_interval = 20;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));

        files.Write("data/user.cfg", "interface scroll_interval = 75;\n");
        DOCTEST_REQUIRE(config::Reload_All_Active_Config_Groups(&diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 75);
        files.Write("data/user.cfg",
            "interface scroll_interval = 80;\ninterface scroll_interval = 256;\n");
        DOCTEST_CHECK_FALSE(config::Reload_All_Active_Config_Groups(&diagnostic));
        DOCTEST_CHECK(diagnostic.location.source == "user.cfg");
        DOCTEST_CHECK(diagnostic.location.line == 2);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 75);
        DOCTEST_CHECK_FALSE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_CHECK_FALSE(loaded);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 75);

        files.Write("data/user.cfg", "");
        DOCTEST_REQUIRE(config::Reload_All_Active_Config_Groups(&diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 20);
    }

    DOCTEST_TEST_CASE("snapshot restoration retains local user config settings") {
        s_config_test_directory files;
        files.Write("data/user.cfg", "interface scroll_interval = 75;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        const auto snapshot_state = effective.state;
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot({},
            &config::saved_config_snapshot,
            &snapshot_state,
            &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 75);
        DOCTEST_REQUIRE(config::Restore_Launch_Config(&diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 75);

        config::s_prepared_config_snapshot network;
        const std::vector<config::s_mod_requirement> required_mods;
        DOCTEST_REQUIRE(config::Prepare_Config_Snapshot(&config::synchronized_config_snapshot,
            &snapshot_state,
            &required_mods,
            &network));
        config::s_network_config_candidate* candidate = nullptr;
        DOCTEST_REQUIRE(config::Stage_Network_Config_Snapshot(network.bytes.data(),
            static_cast<uint32_t>(network.bytes.size()),
            &candidate,
            &diagnostic));
        DOCTEST_REQUIRE(config::Commit_Network_Config_Snapshot(&candidate, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 75);
    }

    DOCTEST_TEST_CASE("explicit startup config must exist without changing native state") {
        s_config_test_directory files;
        files.Write("data/user.cfg", "interface scroll_interval = 75;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = true;
        const auto original = COMBINIT::scroll_interval;

        DOCTEST_CHECK_FALSE(config::Load_Startup_Config(
            "configs/missing.cfg", "base_game", &effective, &loaded, &diagnostic, true));
        DOCTEST_CHECK_FALSE(loaded);
        DOCTEST_CHECK(diagnostic.location.source == "configs/missing.cfg");
        DOCTEST_CHECK(diagnostic.message == "selected startup configuration not found");
        DOCTEST_CHECK(COMBINIT::scroll_interval == original);
    }

    DOCTEST_TEST_CASE("selected startup config replaces default and remains the reload entry point") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "interface scroll_interval = 20;\n");
        files.Write("configs/common.cfg", "interface scroll_interval = 60;\n");
        files.Write("configs/test.cfg", "include common.cfg;\ninterface scroll_interval = 75;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;

        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "configs/test.cfg", "base_game", &effective, &loaded, &diagnostic, true));
        DOCTEST_CHECK(loaded);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 75);
        DOCTEST_REQUIRE(config::Get_Active_Config() != nullptr);
        DOCTEST_CHECK(config::Get_Active_Config()->entry_path == "configs/test.cfg");

        files.Write("configs/test.cfg", "include common.cfg;\ninterface scroll_interval = 80;\n");
        DOCTEST_REQUIRE(config::Reload_All_Active_Config_Groups(&diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 80);
        DOCTEST_CHECK(config::Get_Active_Config()->entry_path == "configs/test.cfg");
    }

    DOCTEST_TEST_CASE("explicit startup config rejects absolute and escaping paths") {
        s_config_test_directory files;
        files.Initialize_Vfs();
        for (const char* path : {"../escape.cfg", "/absolute.cfg", "C:\\absolute.cfg"}) {
            DOCTEST_CAPTURE(path);
            config::s_effective_config effective;
            config::s_cfg_diagnostic diagnostic;
            bool loaded = true;
            DOCTEST_CHECK_FALSE(config::Load_Startup_Config(
                path, "base_game", &effective, &loaded, &diagnostic, true));
            DOCTEST_CHECK_FALSE(loaded);
            DOCTEST_CHECK(diagnostic.message == "invalid configuration entry path");
        }
    }

    DOCTEST_TEST_CASE("invalid startup config preserves native state") {
        s_config_test_directory files;
        files.Write("orion2re.cfg",
            "interface scroll_interval = 80;\n"
            "interface scroll_interval = 256;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = true;

        DOCTEST_CHECK_FALSE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_CHECK_FALSE(loaded);
        DOCTEST_CHECK(diagnostic.location.source == "orion2re.cfg");
        DOCTEST_CHECK(diagnostic.location.line == 2);
    }

    DOCTEST_TEST_CASE("valid startup config commits defaults and overrides together") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "interface scroll_interval = 75;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;

        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_CHECK(loaded);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 75);
    }

    DOCTEST_TEST_CASE("reload policy treats save and synchronization independently") {
        const config::s_config_group_descriptor local = {"local", false, false};
        const config::s_config_group_descriptor synchronized = {"sync", false, true};
        const config::s_config_group_descriptor saved = {"saved", true, false};
        const config::s_config_group_descriptor saved_and_synchronized = {
            "saved_sync",
            true,
            true,
        };

        DOCTEST_CHECK(config::Can_Reload_Config_Group(&local, false));
        DOCTEST_CHECK(config::Can_Reload_Config_Group(&local, true));
        DOCTEST_CHECK(config::Can_Reload_Config_Group(&synchronized, false));
        DOCTEST_CHECK_FALSE(config::Can_Reload_Config_Group(&synchronized, true));
        DOCTEST_CHECK_FALSE(config::Can_Reload_Config_Group(&saved, false));
        DOCTEST_CHECK_FALSE(config::Can_Reload_Config_Group(&saved, true));
        DOCTEST_CHECK_FALSE(config::Can_Reload_Config_Group(
            &saved_and_synchronized, false));
        DOCTEST_CHECK_FALSE(config::Can_Reload_Config_Group(
            &saved_and_synchronized, true));
    }

    DOCTEST_TEST_CASE("active reload applies interface and build presets but not gameplay") {
        s_config_test_directory files;
        files.Write("orion2re.cfg",
            "interface scroll_interval = 2;\n"
            "colony build_queue 1 = prepend design1 end;\n"
            "map star_count maximum = 100;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_REQUIRE(loaded);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 2);
        DOCTEST_CHECK(BUILD_QUEUE::presets[1][1] == BUILD_QUEUE::INSTRUCTION_DESIGN1);
        DOCTEST_CHECK(MAPGEN::maximum_star_count == 100);

        TEST_CONFIG_REFRESH::Reset_();
        files.Write("orion2re.cfg",
            "interface scroll_interval = 3;\n"
            "colony build_queue 1 = prepend design2 end;\n"
            "map star_count maximum = 101;\n");
        DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(false, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 3);
        DOCTEST_CHECK(MAPGEN::maximum_star_count == 100);
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 1);
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::group_refresh_count[config::CONFIG_GROUP_INTERFACE] == 1);
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::group_refresh_count[config::CONFIG_GROUP_TECHNOLOGY] == 0);

        const config::s_active_config* active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        const config::t_variable_id queue_instruction =
            Variable_Id_("colony.build_queue.1.1");
        DOCTEST_REQUIRE(queue_instruction != config::CONFIG_NO_VARIABLE);
        DOCTEST_CHECK(config::Get_Variable_Group(queue_instruction) ==
            config::CONFIG_GROUP_INTERFACE);
        DOCTEST_CHECK(active->baseline.effective.state.values[queue_instruction].integer_value ==
            BUILD_QUEUE::INSTRUCTION_DESIGN2);
        DOCTEST_CHECK(BUILD_QUEUE::presets[1][1] == BUILD_QUEUE::INSTRUCTION_DESIGN2);
        const config::t_variable_id maximum_star_count =
            Variable_Id_("map.star_count.4");
        DOCTEST_CHECK(active->baseline.effective.state.values[maximum_star_count].integer_value == 100);

        DOCTEST_REQUIRE(config::Reload_All_Active_Config_Groups(&diagnostic));
        DOCTEST_CHECK(MAPGEN::maximum_star_count == 101);
        active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        DOCTEST_CHECK(active->baseline.effective.state.values[maximum_star_count].integer_value == 101);
    }

    DOCTEST_TEST_CASE("active configuration reload dispatches Lua only for changed groups") {
        s_config_test_directory files;
        files.Write("orion2re.cfg",
            "interface scroll_interval = 2;\n"
            "enable reload_listener;\n");
        files.Write(fs::path("data") / "mods" / "reload_listener" /
                "mod.json5",
            Manifest_("reload_listener",
                "  lua: { api: 1, storage_version: 1 },\n"));
        files.Write(fs::path("data") / "mods" / "reload_listener" /
                "config" / "main.cfg",
            "");
        files.Write(fs::path("data") / "mods" / "reload_listener" /
                "main.lua",
            "local reloads = 0\n"
            "events.on('game.configuration_changed', function(event)\n"
            "  reloads = reloads + 1\n"
            "  if reloads > 2 or event.reason ~= 'configuration_reloaded' or\n"
            "      #event.changes ~= 0 or #event.changed_groups ~= 1 or\n"
            "      event.changed_groups[1] ~= 'interface' then\n"
            "    error('wrong active configuration reload event')\n"
            "  end\n"
            "  if pcall(function() event.changed_groups[1] = 'gameplay' end) then\n"
            "    error('changed group list is mutable')\n"
            "  end\n"
            "  storage.reloads = reloads\n"
            "end)\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_REQUIRE(loaded);

        files.Write("orion2re.cfg",
            "interface scroll_interval = 3;\n"
            "enable reload_listener;\n");
        DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(
            false, &diagnostic));
        DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(
            false, &diagnostic));
        DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(
            false, &diagnostic, Merge_With_Inactive_Value_Change_));
        files.Write("orion2re.cfg",
            "interface scroll_interval = 4;\n"
            "enable reload_listener;\n");
        DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(
            false, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 4);
    }

    DOCTEST_TEST_CASE("local reload preserves adopted synchronized snapshot values") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "interface scroll_interval = 2;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_REQUIRE(loaded);

        config::s_config_state host_state;
        DOCTEST_REQUIRE(config::Read_Current_State(&host_state));
        const config::t_variable_id maximum_star_count =
            Variable_Id_("map.star_count.4");
        DOCTEST_REQUIRE(maximum_star_count != config::CONFIG_NO_VARIABLE);
        host_state.values[maximum_star_count].integer_value = 101;
        DOCTEST_REQUIRE(config::Commit_Config_Snapshot(
            &config::synchronized_config_snapshot, &host_state, &diagnostic));
        DOCTEST_CHECK(MAPGEN::maximum_star_count == 101);

        TEST_CONFIG_REFRESH::Reset_();
        files.Write("orion2re.cfg", "interface scroll_interval = 3;\n");
        DOCTEST_REQUIRE(config::Reload_Active_Config_Groups(true, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 3);
        DOCTEST_CHECK(MAPGEN::maximum_star_count == 101);
        const config::s_active_config* active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        DOCTEST_CHECK(active->baseline.effective.state.values[maximum_star_count].integer_value == 101);
    }

    DOCTEST_TEST_CASE("active reload merge failure preserves native and effective state") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "interface scroll_interval = 2;\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_REQUIRE(loaded);
        const config::s_active_config* active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        const config::s_effective_config prior_effective = active->baseline.effective;
        char* prior_tech_name = TECHDATA::shields[0].name;
        arena::s_arena* prior_arena =
            config::Acquire_String_Arena(config::CONFIG_GROUP_TECHNOLOGY);
        DOCTEST_REQUIRE(prior_arena != nullptr);

        files.Write("orion2re.cfg", "interface scroll_interval = 3;\n");
        TEST_CONFIG_REFRESH::Reset_();
        DOCTEST_CHECK_FALSE(config::Reload_Active_Config_Groups(
            false, &diagnostic, Fail_After_Group_Merge_, Reload_All_Groups_));
        DOCTEST_CHECK(diagnostic.message == "unable to commit configuration group");
        DOCTEST_CHECK(COMBINIT::scroll_interval == 2);
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 0);
        active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        Check_Effective_Config_Equals_(active->baseline.effective, prior_effective);
        DOCTEST_CHECK(TECHDATA::shields[0].name == prior_tech_name);
        arena::s_arena* current_arena =
            config::Acquire_String_Arena(config::CONFIG_GROUP_TECHNOLOGY);
        DOCTEST_CHECK(current_arena == prior_arena);
        arena::Release(current_arena);
        arena::Release(prior_arena);
    }

    DOCTEST_TEST_CASE("active typed writes retain package provenance and restore exactly") {
        s_config_test_directory files;
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = true;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_CHECK_FALSE(loaded);
        const config::s_active_config snapshot = *config::Get_Active_Config();
        const config::t_variable_id variable_id =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(variable_id != config::CONFIG_NO_VARIABLE);

        config::s_config_write write;
        write.variable_id = variable_id;
        write.canonical_path = "interface.scroll_interval";
        write.value.type = config::CONFIG_VALUE_UINT8;
        write.value.integer_value = 7;
        write.provenance.source = "mods/example/scripts/run.lua#prepare";
        write.provenance.line = 4;
        write.provenance.column = 9;
        write.package_id = "example";
        config::s_config_write_set writes;
        writes.writes.push_back(write);
        write.value.integer_value = 9;
        writes.writes.push_back(write);

        TEST_CONFIG_REFRESH::Reset_();
        DOCTEST_REQUIRE(config::Commit_Active_Config_Writes(&writes, &diagnostic));
        const config::s_active_config* active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 9);
        DOCTEST_CHECK(active->baseline.effective.provenance[variable_id].package_id ==
            "example");
        DOCTEST_CHECK(active->baseline.effective.provenance[variable_id].location.source ==
            write.provenance.source);
        DOCTEST_CHECK(active->baseline.effective.provenance[variable_id].location.line == 4);
        DOCTEST_CHECK(active->baseline.effective.provenance[variable_id].location.column == 9);
        DOCTEST_REQUIRE(active->baseline.effective.prior_values[variable_id].size() == 2);
        DOCTEST_CHECK(active->baseline.effective.prior_values[variable_id][0]
                          .value.integer_value ==
            snapshot.baseline.effective.state.values[variable_id].integer_value);
        DOCTEST_CHECK(active->baseline.effective.prior_values[variable_id][1]
                          .value.integer_value == 7);
        DOCTEST_CHECK(active->baseline.effective.prior_values[variable_id][1]
                          .provenance.package_id == "example");

        DOCTEST_REQUIRE(config::Restore_Active_Config(&snapshot, &diagnostic));
        active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        DOCTEST_CHECK(COMBINIT::scroll_interval ==
            snapshot.baseline.effective.state.values[variable_id].integer_value);
        Check_Effective_Config_Equals_(active->baseline.effective, snapshot.baseline.effective);

        config::s_active_config malformed = snapshot;
        malformed.baseline.effective.provenance.pop_back();
        TEST_CONFIG_REFRESH::Reset_();
        DOCTEST_CHECK_FALSE(config::Restore_Active_Config(&malformed, &diagnostic));
        Check_Effective_Config_Equals_(
            config::Get_Active_Config()->baseline.effective, snapshot.baseline.effective);
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 0);
    }

    DOCTEST_TEST_CASE("native configuration writes update active dirty tracking") {
        s_config_test_directory files;
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = true;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_CHECK_FALSE(loaded);
        const config::t_variable_id variable_id =
            Variable_Id_("interface.scroll_interval");
        DOCTEST_REQUIRE(variable_id != config::CONFIG_NO_VARIABLE);

        COMBINIT::scroll_interval = 7;
        DOCTEST_REQUIRE(config::Record_Active_Native_Config_Write(variable_id));
        const config::s_active_config* active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        DOCTEST_CHECK(active->baseline.effective.state.values[variable_id].integer_value == 7);
        DOCTEST_CHECK(config::State_Value_Is_Non_Default(
            &active->baseline.effective.state, variable_id));

        COMBINIT::scroll_interval = 1;
        DOCTEST_REQUIRE(config::Record_Active_Native_Config_Write(variable_id));
        active = config::Get_Active_Config();
        DOCTEST_REQUIRE(active != nullptr);
        DOCTEST_CHECK_FALSE(config::State_Value_Is_Non_Default(
            &active->baseline.effective.state, variable_id));
    }

    DOCTEST_TEST_CASE("active snapshots and identity are cached until configuration changes") {
        s_config_test_directory files;
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = true;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));

        const config::s_prepared_config_snapshot* first_snapshot = nullptr;
        const config::s_prepared_config_snapshot* repeated_snapshot = nullptr;
        const std::vector<uint8_t>* first_identity = nullptr;
        const std::vector<uint8_t>* repeated_identity = nullptr;
        DOCTEST_REQUIRE(config::Get_Prepared_Active_Config_Snapshot(
            &config::synchronized_config_snapshot,
            &first_snapshot,
            &diagnostic));
        DOCTEST_REQUIRE(config::Get_Prepared_Active_Config_Snapshot(
            &config::synchronized_config_snapshot,
            &repeated_snapshot,
            &diagnostic));
        DOCTEST_CHECK(first_snapshot == repeated_snapshot);
        DOCTEST_REQUIRE(config::Get_Canonical_Active_Config_Identity(
            &first_identity, &diagnostic));
        DOCTEST_REQUIRE(config::Get_Canonical_Active_Config_Identity(
            &repeated_identity, &diagnostic));
        DOCTEST_CHECK(first_identity == repeated_identity);
        const std::vector<uint8_t> first_snapshot_bytes = first_snapshot->bytes;
        const std::vector<uint8_t> first_identity_bytes = *first_identity;

        const config::t_variable_id picks =
            Variable_Id_("race.picks.starting");
        DOCTEST_REQUIRE(picks != config::CONFIG_NO_VARIABLE);
        config::s_config_write write;
        write.variable_id = picks;
        write.value = config::Get_Active_Config()->baseline.effective.state.values[picks];
        write.value.integer_value = 12;
        config::s_config_write_set writes;
        writes.writes.push_back(write);
        DOCTEST_REQUIRE(config::Commit_Active_Config_Writes(
            &writes, &diagnostic));

        const config::s_prepared_config_snapshot* changed_snapshot = nullptr;
        const std::vector<uint8_t>* changed_identity = nullptr;
        DOCTEST_REQUIRE(config::Get_Prepared_Active_Config_Snapshot(
            &config::synchronized_config_snapshot,
            &changed_snapshot,
            &diagnostic));
        DOCTEST_REQUIRE(config::Get_Canonical_Active_Config_Identity(
            &changed_identity, &diagnostic));
        DOCTEST_CHECK(changed_snapshot->bytes != first_snapshot_bytes);
        DOCTEST_CHECK(*changed_identity != first_identity_bytes);
    }

    DOCTEST_TEST_CASE("all active configuration mutations invalidate prepared caches") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "map star_count maximum = 100;\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        const config::s_active_config original = *config::Get_Active_Config();
        const config::t_variable_id maximum = Variable_Id_("map.star_count.4");
        DOCTEST_REQUIRE(maximum != config::CONFIG_NO_VARIABLE);
        Check_Active_Caches_();

        MAPGEN::maximum_star_count = 101;
        DOCTEST_REQUIRE(config::Record_Active_Native_Config_Write(maximum));
        Check_Active_Caches_();
        DOCTEST_REQUIRE(config::Restore_Active_Config(&original, &diagnostic));
        Check_Active_Caches_();

        config::s_config_state snapshot = original.baseline.effective.state;
        config::s_typed_value value = snapshot.values[maximum];
        value.integer_value = 102;
        DOCTEST_REQUIRE(config::Set_State_Value(&snapshot, maximum, value));
        DOCTEST_REQUIRE(config::Commit_Config_Snapshot(&config::saved_config_snapshot, &snapshot, &diagnostic));
        Check_Active_Caches_();
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot({}, &config::saved_config_snapshot, &original.baseline.effective.state, &diagnostic));
        Check_Active_Caches_();

        files.Write("orion2re.cfg", "map star_count maximum = 103;\n");
        DOCTEST_REQUIRE(config::Reload_All_Active_Config_Groups(&diagnostic));
        Check_Active_Caches_();

        config::s_prepared_config_snapshot network;
        const std::vector<config::s_mod_requirement> required_mods;
        DOCTEST_REQUIRE(config::Prepare_Config_Snapshot(&config::synchronized_config_snapshot,
            &snapshot,
            &required_mods,
            &network));
        config::s_network_config_candidate* candidate = nullptr;
        DOCTEST_REQUIRE(config::Stage_Network_Config_Snapshot(network.bytes.data(),
            static_cast<uint32_t>(network.bytes.size()),
            &candidate,
            &diagnostic));
        Check_Active_Caches_();
        DOCTEST_REQUIRE(config::Commit_Network_Config_Snapshot(&candidate, &diagnostic));
        DOCTEST_CHECK(candidate == nullptr);
        DOCTEST_CHECK(MAPGEN::maximum_star_count == 102);
        Check_Active_Caches_();

        DOCTEST_REQUIRE(config::Restore_Active_Config(&original, &diagnostic));
        Check_Active_Caches_();
        DOCTEST_REQUIRE(config::Stage_Network_Config_Snapshot(network.bytes.data(),
            static_cast<uint32_t>(network.bytes.size()),
            &candidate,
            &diagnostic));
        orion2re::lua::s_lua_save_snapshot lua_snapshot;
        std::string lua_diagnostic;
        DOCTEST_REQUIRE(orion2re::lua::Capture_Lua_Save_Snapshot(&lua_snapshot, &lua_diagnostic));
        DOCTEST_REQUIRE(orion2re::lua::Finalize_Registry_Fingerprint(
            &lua_snapshot.registry, &snapshot, &lua_diagnostic));
        std::vector<uint8_t> lua_bytes(orion2re::lua::Lua_Save_Snapshot_Size(&lua_snapshot));
        SERIAL::Writer_ writer(lua_bytes.data(), static_cast<uint32_t>(lua_bytes.size()), "Lua snapshot overflow");
        orion2re::lua::Write_Lua_Save_Snapshot(&writer, &lua_snapshot);
        DOCTEST_REQUIRE(config::Finalize_Network_Config_Snapshot(&candidate,
            lua_bytes.data(),
            writer.Length(),
            &lua_diagnostic));
        DOCTEST_CHECK(candidate == nullptr);
        Check_Active_Caches_();

        files.Write("orion2re.cfg", "map star_count huge = 72;\n");
        DOCTEST_CHECK_FALSE(config::Reload_All_Active_Config_Groups(&diagnostic));
        DOCTEST_CHECK_FALSE(diagnostic.message.empty());
        Check_Active_Caches_();
    }

    DOCTEST_TEST_CASE("snapshot overlays validate final values rather than replaced local values") {
        s_config_test_directory files;
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        const config::s_config_state saved = effective.state;
        files.Write("orion2re.cfg", "map star_count huge = 72;\n");

        DOCTEST_SUBCASE("saved overlay") {
            DOCTEST_REQUIRE(config::Reapply_Config_Snapshot({}, &config::saved_config_snapshot, &saved, &diagnostic));
            Check_Effective_Config_Equals_(config::Get_Active_Config()->baseline.effective, effective);
        }
        DOCTEST_SUBCASE("network overlay") {
            config::s_prepared_config_snapshot network;
            const std::vector<config::s_mod_requirement> required_mods;
            DOCTEST_REQUIRE(config::Prepare_Config_Snapshot(&config::synchronized_config_snapshot,
                &saved,
                &required_mods,
                &network));
            config::s_network_config_candidate* candidate = nullptr;
            DOCTEST_REQUIRE(config::Stage_Network_Config_Snapshot(network.bytes.data(),
                static_cast<uint32_t>(network.bytes.size()),
                &candidate,
                &diagnostic));
            DOCTEST_REQUIRE(config::Commit_Network_Config_Snapshot(&candidate, &diagnostic));
            Check_Effective_Config_Equals_(config::Get_Active_Config()->baseline.effective, effective);
        }
        DOCTEST_SUBCASE("without overlay validation still fails with diagnostic") {
            DOCTEST_CHECK_FALSE(config::Load_Startup_Config(
                "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
            DOCTEST_CHECK(diagnostic.message == "unable to validate configuration state");
        }
        DOCTEST_SUBCASE("invalid final overlay is rejected") {
            config::s_config_state invalid = saved;
            const config::t_variable_id huge = Variable_Id_("map.star_count.3");
            DOCTEST_REQUIRE(huge != config::CONFIG_NO_VARIABLE);
            config::s_typed_value value = invalid.values[huge];
            value.integer_value = 72;
            DOCTEST_REQUIRE(config::Set_State_Value(&invalid, huge, value));
            DOCTEST_CHECK_FALSE(config::Reapply_Config_Snapshot({}, &config::saved_config_snapshot, &invalid, &diagnostic));
            DOCTEST_CHECK_FALSE(diagnostic.message.empty());
            Check_Effective_Config_Equals_(config::Get_Active_Config()->baseline.effective, effective);
        }
    }

    DOCTEST_TEST_CASE("non-string transitions retain the technology string arena") {
        s_config_test_directory files;
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        const config::t_variable_id shield_class = Variable_Id_("design.shield.0.shield_class");
        DOCTEST_REQUIRE(shield_class != config::CONFIG_NO_VARIABLE);
        for (const char* path : {"design.shield.0.name", "design.shield.1.name"}) {
            const config::t_variable_id id = Variable_Id_(path);
            DOCTEST_REQUIRE(id != config::CONFIG_NO_VARIABLE);
            config::s_typed_value initial_name = effective.state.values[id];
            initial_name.string_value = "Shield";
            DOCTEST_REQUIRE(config::Set_State_Value(&effective.state, id, initial_name));
        }
        DOCTEST_REQUIRE(config::Commit_State(&effective.state));
        char* original_name = TECHDATA::shields[0].name;
        arena::s_arena* original_arena = config::Acquire_String_Arena(config::CONFIG_GROUP_TECHNOLOGY);
        DOCTEST_REQUIRE(original_arena != nullptr);
        config::s_config_state changed = effective.state;
        config::s_typed_value value = changed.values[shield_class];
        ++value.integer_value;
        DOCTEST_REQUIRE(config::Set_State_Value(&changed, shield_class, value));
        DOCTEST_REQUIRE(config::Commit_Config_Transition(UINT32_MAX, &effective.state, &changed));
        DOCTEST_CHECK(TECHDATA::shields[0].name == original_name);
        arena::s_arena* current_arena = config::Acquire_String_Arena(config::CONFIG_GROUP_TECHNOLOGY);
        DOCTEST_CHECK(current_arena == original_arena);
        arena::Release(current_arena);
        arena::Release(original_arena);
        DOCTEST_REQUIRE(config::Commit_Config_Transition(UINT32_MAX, &changed, &effective.state));

        const config::t_variable_id name = Variable_Id_("design.shield.0.name");
        DOCTEST_REQUIRE(name != config::CONFIG_NO_VARIABLE);
        config::s_typed_value renamed = changed.values[name];
        renamed.string_value = "Renamed shield";
        DOCTEST_REQUIRE(config::Set_State_Value(&changed, name, renamed));
        original_arena = config::Acquire_String_Arena(config::CONFIG_GROUP_TECHNOLOGY);
        char* other_name = TECHDATA::shields[1].name;
        const std::string other_text = other_name;
        const std::string original_text = original_name;
        DOCTEST_REQUIRE(config::Commit_Config_Transition(UINT32_MAX, &effective.state, &changed));
        DOCTEST_CHECK(std::string(TECHDATA::shields[0].name) == renamed.string_value);
        DOCTEST_CHECK(std::string(TECHDATA::shields[1].name) == other_text);
        DOCTEST_CHECK(TECHDATA::shields[1].name != other_name);
        DOCTEST_CHECK(config::Is_Active_String(TECHDATA::shields[1].name));
        DOCTEST_CHECK(std::string(other_name) == other_text);
        DOCTEST_CHECK(std::string(original_name) == original_text);
        arena::Release(original_arena);
        DOCTEST_REQUIRE(config::Commit_Config_Transition(UINT32_MAX, &changed, &effective.state));
    }

    DOCTEST_TEST_CASE("active reload rejects package topology changes without remounting") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "interface scroll_interval = 2;\n");
        files.Write("resource.txt", "base");
        files.Write(fs::path("data") / "mods" / "added" / "mod.json5",
            Manifest_("added"));
        files.Write(fs::path("data") / "mods" / "added" / "config" / "main.cfg",
            "interface scroll_interval = 9;\n");
        files.Write(fs::path("data") / "mods" / "added" / "data" / "resource.txt",
            "mod");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));

        files.Write("orion2re.cfg", "enable added;\n");
        DOCTEST_CHECK_FALSE(config::Reload_Active_Config_Groups(false, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "active package order changed; restart or perform a full load");
        DOCTEST_CHECK(COMBINIT::scroll_interval == 2);
        DOCTEST_CHECK(Read_Vfs_Text_("resource.txt") == "base");
    }

    DOCTEST_TEST_CASE("snapshot reapplication replaces package layers and is repeatable") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "enable local;\n");
        files.Write(fs::path("data") / "mods" / "local" / "mod.json5",
            Manifest_("local"));
        files.Write(fs::path("data") / "mods" / "local" / "config" / "main.cfg",
            "interface scroll_interval = 4;\n");
        files.Write(fs::path("data") / "mods" / "local" / "data" / "resource.txt",
            "local");
        files.Write(fs::path("data") / "mods" / "saved" / "mod.json5",
            Manifest_("saved"));
        files.Write(fs::path("data") / "mods" / "saved" / "config" / "main.cfg",
            "interface scroll_interval = 9;\n");
        files.Write(fs::path("data") / "mods" / "saved" / "data" / "resource.txt",
            "saved");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_REQUIRE(loaded);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 4);
        DOCTEST_CHECK(Read_Vfs_Text_("resource.txt") == "local");

        config::s_config_state snapshot_state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&snapshot_state));
        const config::t_variable_id maximum_star_count =
            Variable_Id_("map.star_count.4");
        snapshot_state.values[maximum_star_count].integer_value = 101;
        const std::vector<config::s_mod_requirement> required_mods = {
            {"saved", "1.0.0"},
        };

        TEST_CONFIG_REFRESH::Reset_();
        for (int application = 0; application < 2; ++application) {
            DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(
                required_mods, &config::synchronized_config_snapshot, &snapshot_state, &diagnostic));
            DOCTEST_CHECK(COMBINIT::scroll_interval == 9);
            DOCTEST_CHECK(MAPGEN::maximum_star_count == 101);
            DOCTEST_CHECK(Read_Vfs_Text_("resource.txt") == "saved");
            const config::s_active_config* active = config::Get_Active_Config();
            DOCTEST_REQUIRE(active != nullptr);
            DOCTEST_CHECK(active->required_mods == required_mods);
        }

        const std::vector<config::s_mod_requirement> no_required_mods;
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(
            no_required_mods, &config::synchronized_config_snapshot, &snapshot_state, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 1);
        DOCTEST_CHECK(vfs::File_Exists("resource.txt") == 0);
        DOCTEST_CHECK(config::Get_Active_Config()->package_order.empty());
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 3);
    }

    DOCTEST_TEST_CASE("snapshot reapplication preserves local Lua hash override") {
        s_config_test_directory files;
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = true;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_CHECK_FALSE(loaded);

        const config::t_variable_id variable_id = Variable_Id_(
            "interface.allow_lua_source_hash_mismatch");
        DOCTEST_REQUIRE(variable_id != config::CONFIG_NO_VARIABLE);
        config::s_config_write write;
        write.variable_id = variable_id;
        write.canonical_path = "interface.allow_lua_source_hash_mismatch";
        write.value.type = config::CONFIG_VALUE_BOOL;
        write.value.bool_value = true;
        config::s_config_write_set writes;
        writes.writes.push_back(write);
        DOCTEST_REQUIRE(config::Commit_Active_Config_Writes(
            &writes, &diagnostic));
        DOCTEST_REQUIRE(orion2re::lua::allow_source_hash_mismatch);

        config::s_config_state snapshot_state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&snapshot_state));
        const std::vector<config::s_mod_requirement> no_required_mods;
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(no_required_mods,
            &config::synchronized_config_snapshot,
            &snapshot_state,
            &diagnostic));
        DOCTEST_CHECK(orion2re::lua::allow_source_hash_mismatch);
        DOCTEST_CHECK(config::Get_Active_Config()
                          ->baseline.effective.state.values[variable_id]
                          .bool_value);
        orion2re::lua::allow_source_hash_mismatch = false;
    }

    DOCTEST_TEST_CASE("same-package restore invalidates cached includes and manifests") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "enable cached;\nrace picks starting = 12;\n");
        files.Write(fs::path("data/mods/cached/mod.json5"), Manifest_("cached"));
        files.Write(fs::path("data/mods/cached/config/main.cfg"), "include local.cfg;\n");
        files.Write(fs::path("data/mods/cached/config/local.cfg"), "interface scroll_interval = 7;\n");
        files.Initialize_Vfs();
        config::s_effective_config launch;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config("orion2re.cfg", "base_game", &launch, &loaded, &diagnostic));
        const auto requirements = config::Get_Active_Config()->required_mods;
        config::s_config_state saved;
        DOCTEST_REQUIRE(config::Initialize_Tracked_Defaults(&saved));
        for (int repeat = 0; repeat < 2; ++repeat) {
            DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(requirements, &config::saved_config_snapshot, &saved, &diagnostic));
            DOCTEST_CHECK(COMBINIT::scroll_interval == 7);
            const auto picks = Variable_Id_("race.picks.starting");
            DOCTEST_CHECK(config::Get_Active_Config()->baseline.effective.state.values[picks].integer_value == saved.values.Read(picks).integer_value);
        }
        DOCTEST_REQUIRE(config::Restore_Launch_Config(&diagnostic));
        Check_Effective_Config_Equals_(config::Get_Active_Config()->baseline.effective, launch);
        files.Write(fs::path("data/mods/cached/config/local.cfg"), "interface scroll_interval = 9;\n");
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(requirements, &config::saved_config_snapshot, &saved, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 9);
        files.Write(fs::path("data/mods/cached/mod.json5"), Manifest_("cached", "conflicts: [\"cached\"],\n"));
        DOCTEST_CHECK_FALSE(config::Reapply_Config_Snapshot(requirements, &config::saved_config_snapshot, &saved, &diagnostic));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 9);
    }

    DOCTEST_TEST_CASE("installed mod explicitly accepts compatible serialized versions") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "enable local;\n");
        files.Write(fs::path("data") / "mods" / "local" / "mod.json5",
            Manifest_("local"));
        files.Write(fs::path("data") / "mods" / "local" / "config" / "main.cfg",
            "interface scroll_interval = 4;\n");
        files.Write(fs::path("data") / "mods" / "local" / "data" / "resource.txt",
            "local");
        files.Write(fs::path("data") / "mods" / "compatible" / "mod.json5",
            Versioned_Manifest_(
                "compatible",
                "1.3.0",
                ">=1.0.0 <1.3.0",
                "  lua: { api: 1, storage_version: 1 },\n"));
        files.Write(fs::path("data") / "mods" / "compatible" / "config" / "main.cfg",
            "interface scroll_interval = 8;\n");
        files.Write(fs::path("data") / "mods" / "compatible" / "main.lua",
            "local upgraded_source = true\n");
        files.Write(fs::path("data") / "mods" / "compatible" / "data" / "resource.txt",
            "compatible");
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));

        config::s_config_state snapshot_state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&snapshot_state));
        const std::vector<config::s_mod_requirement> compatible = {
            {"compatible", "1.2.0"},
        };
        orion2re::lua::s_lua_save_snapshot lua_snapshot;
        const std::vector<config::s_mod_requirement> installed = {
            {"compatible", "1.3.0"},
        };
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(installed,
            &config::synchronized_config_snapshot,
            &snapshot_state,
            &diagnostic));
        std::string lua_diagnostic;
        DOCTEST_REQUIRE(orion2re::lua::Capture_Lua_Save_Snapshot(
            &lua_snapshot, &lua_diagnostic));
        DOCTEST_REQUIRE(lua_snapshot.registry.runtime_packages.size() == 1);
        lua_snapshot.registry.packages_high_to_low_priority[0].version =
            "1.2.0";
        lua_snapshot.registry.runtime_packages[0].package_version = "1.2.0";
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(
            compatible,
            &config::synchronized_config_snapshot,
            &snapshot_state,
            &diagnostic,
            &lua_snapshot));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 8);
        DOCTEST_CHECK(Read_Vfs_Text_("resource.txt") == "compatible");
        DOCTEST_CHECK(config::Get_Active_Config()->required_mods ==
            std::vector<config::s_mod_requirement>{{"compatible", "1.3.0"}});

        TEST_CONFIG_REFRESH::Reset_();
        const std::vector<config::s_mod_requirement> incompatible = {
            {"compatible", "1.3.1"},
        };
        DOCTEST_CHECK_FALSE(config::Reapply_Config_Snapshot(
            incompatible, &config::synchronized_config_snapshot, &snapshot_state, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "installed package 'compatible' version 1.3.0 is not compatible with required version 1.3.1");
        DOCTEST_CHECK(COMBINIT::scroll_interval == 8);
        DOCTEST_CHECK(Read_Vfs_Text_("resource.txt") == "compatible");
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 0);

        const std::vector<config::s_mod_requirement> missing = {
            {"missing", "1.0.0"},
        };
        DOCTEST_CHECK_FALSE(config::Reapply_Config_Snapshot(
            missing, &config::synchronized_config_snapshot, &snapshot_state, &diagnostic));
        DOCTEST_CHECK(diagnostic.message == "package not found: 'missing'");
        DOCTEST_CHECK(COMBINIT::scroll_interval == 8);
        DOCTEST_CHECK(Read_Vfs_Text_("resource.txt") == "compatible");
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 0);
    }

    DOCTEST_TEST_CASE("Lua save callback failure preserves the active configuration transaction") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "enable persistent;\n");
        files.Write(fs::path("data") / "mods" / "persistent" / "mod.json5",
            Manifest_("persistent",
                "  lua: { api: 1, storage_version: 1 },\n"));
        files.Write(fs::path("data") / "mods" / "persistent" / "config" /
                "main.cfg",
            "interface scroll_interval = 8;\n");
        files.Write(fs::path("data") / "mods" / "persistent" / "main.lua",
            "events.on('game.loaded', function() error('load rejected') end)\n");
        files.Write(fs::path("data") / "mods" / "persistent" / "data" /
                "resource.txt",
            "persistent");
        files.Initialize_Vfs();
        Set_Native_Defaults_();

        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_REQUIRE(loaded);
        DOCTEST_REQUIRE(orion2re::lua::Get_Mod_Runtime_Count(
                            orion2re::lua::Get_Active_Mod_Runtimes()) == 1);
        const std::string active_fingerprint =
            orion2re::lua::Get_Active_Registry()->fingerprint;

        orion2re::lua::s_lua_save_snapshot lua_snapshot;
        std::string lua_diagnostic;
        DOCTEST_REQUIRE(orion2re::lua::Capture_Lua_Save_Snapshot(
            &lua_snapshot, &lua_diagnostic));
        DOCTEST_REQUIRE(lua_snapshot.registry.runtime_packages.size() == 1);
        DOCTEST_REQUIRE(
            lua_snapshot.registry.runtime_packages[0].sources.size() == 1);

        config::s_config_state snapshot_state;
        DOCTEST_REQUIRE(config::Read_Current_State(&snapshot_state));
        const config::t_variable_id saved_variable =
            Variable_Id_("race.picks.starting");
        DOCTEST_REQUIRE(saved_variable != config::CONFIG_NO_VARIABLE);
        snapshot_state.values[saved_variable].integer_value = 12;
        const std::vector<config::s_mod_requirement> required_mods = {
            {"persistent", "1.0.0"},
        };
        TEST_CONFIG_REFRESH::Reset_();
        DOCTEST_CHECK_FALSE(config::Reapply_Config_Snapshot(required_mods,
            &config::saved_config_snapshot,
            &snapshot_state,
            &diagnostic,
            &lua_snapshot));
        DOCTEST_CHECK(diagnostic.message.find("load rejected") !=
            std::string::npos);
        DOCTEST_CHECK(COMBINIT::scroll_interval == 8);
        DOCTEST_CHECK(Read_Vfs_Text_("resource.txt") == "persistent");
        DOCTEST_CHECK(orion2re::lua::Get_Active_Registry()->fingerprint ==
            active_fingerprint);
        DOCTEST_CHECK(orion2re::lua::Get_Mod_Runtime_Count(
                          orion2re::lua::Get_Active_Mod_Runtimes()) == 1);
        DOCTEST_CHECK(TEST_CONFIG_REFRESH::refresh_count == 0);

        lua_snapshot.registry.runtime_packages[0].sources[0].source_hash =
            "changed source hash";
        DOCTEST_CHECK_FALSE(config::Reapply_Config_Snapshot(required_mods,
            &config::saved_config_snapshot,
            &snapshot_state,
            &diagnostic,
            &lua_snapshot));
        DOCTEST_CHECK(diagnostic.message.find(
                          "loaded Lua file content differs") !=
            std::string::npos);

        lua_snapshot.registry.runtime_packages[0].sources[0].source_hash =
            orion2re::lua::Get_Active_Registry()
                ->runtime_packages[0]
                .sources[0]
                .source_hash;
        lua_snapshot.registry.synchronized_config_hash =
            "unrelated config hash";
        DOCTEST_CHECK_FALSE(config::Reapply_Config_Snapshot(required_mods,
            &config::saved_config_snapshot,
            &snapshot_state,
            &diagnostic,
            &lua_snapshot));
        DOCTEST_CHECK(diagnostic.message.find("load rejected") !=
            std::string::npos);
    }

    DOCTEST_TEST_CASE("failed network Lua load restores candidate configuration") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "enable persistent;\n");
        files.Write(fs::path("data") / "mods" / "persistent" / "mod.json5",
            Manifest_("persistent",
                "  lua: { api: 1, storage_version: 1 },\n"));
        files.Write(fs::path("data") / "mods" / "persistent" / "config" /
                "main.cfg",
            "interface scroll_interval = 8;\n");
        files.Write(fs::path("data") / "mods" / "persistent" / "main.lua",
            "events.on('game.loaded', function() error('load rejected') end)\n");
        files.Initialize_Vfs();
        Set_Native_Defaults_();

        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_REQUIRE(loaded);

        orion2re::lua::s_lua_save_snapshot lua_snapshot;
        std::string lua_diagnostic;
        DOCTEST_REQUIRE(orion2re::lua::Capture_Lua_Save_Snapshot(
            &lua_snapshot, &lua_diagnostic));

        config::s_config_state candidate_state;
        DOCTEST_REQUIRE(config::Read_Current_State(&candidate_state));
        const config::t_variable_id maximum_star_count =
            Variable_Id_("map.star_count.4");
        DOCTEST_REQUIRE(maximum_star_count != config::CONFIG_NO_VARIABLE);
        const int64_t prior_star_count =
            candidate_state.values[maximum_star_count].integer_value;
        candidate_state.values[maximum_star_count].integer_value = 101;
        const std::vector<config::s_mod_requirement> required_mods = {
            {"persistent", "1.0.0"},
        };
        DOCTEST_REQUIRE(orion2re::lua::Finalize_Registry_Fingerprint(
            &lua_snapshot.registry, &candidate_state, &lua_diagnostic));
        const uint32_t lua_size =
            orion2re::lua::Lua_Save_Snapshot_Size(&lua_snapshot);
        std::vector<uint8_t> lua_bytes(lua_size);
        SERIAL::Writer_ lua_writer(
            lua_bytes.data(), lua_size, "Lua snapshot overflow");
        orion2re::lua::Write_Lua_Save_Snapshot(&lua_writer, &lua_snapshot);
        const uint32_t config_size = config::Config_Snapshot_Size(
            &config::synchronized_config_snapshot,
            &candidate_state,
            &required_mods);
        std::vector<uint8_t> config_bytes(config_size);
        SERIAL::Writer_ config_writer(
            config_bytes.data(), config_size, "config snapshot overflow");
        config::Write_Config_Snapshot(&config_writer,
            &config::synchronized_config_snapshot,
            &candidate_state,
            &required_mods);

        config::s_network_config_candidate* candidate = nullptr;
        DOCTEST_REQUIRE(config::Stage_Network_Config_Snapshot(
            config_bytes.data(),
            config_writer.Length(),
            &candidate,
            &diagnostic));
        DOCTEST_CHECK_FALSE(config::Finalize_Network_Config_Snapshot(
            &candidate,
            lua_bytes.data(),
            lua_writer.Length(),
            &lua_diagnostic));
        DOCTEST_CHECK(lua_diagnostic.find("load rejected") !=
            std::string::npos);
        DOCTEST_CHECK(MAPGEN::maximum_star_count == prior_star_count);
        DOCTEST_REQUIRE(candidate != nullptr);
        config::Discard_Network_Config_Snapshot(&candidate);
        DOCTEST_CHECK(candidate == nullptr);
    }

    DOCTEST_TEST_CASE("network finalization restores recorded native writes on failure") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "enable restore_runtime;\n");
        files.Write("data/mods/restore_runtime/mod.json5", Manifest_("restore_runtime", "lua: { api: 1, storage_version: 1 },\n"));
        files.Write("data/mods/restore_runtime/config/main.cfg", "");
        bool reject = false;
        DOCTEST_SUBCASE("successful finalization") {}
        DOCTEST_SUBCASE("failed callback") { reject = true; }
        files.Write("data/mods/restore_runtime/main.lua", reject
            ? "events.on('game.loaded', function() error('reject network restore') end)\n"
            : "events.on('game.loaded', function() end)\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config("orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        const auto maximum = Variable_Id_("map.star_count.4");
        auto target = effective.state;
        auto value = target.values.Read(maximum);
        value.integer_value = 101;
        DOCTEST_REQUIRE(config::Set_State_Value(&target, maximum, value));
        config::s_prepared_config_snapshot network;
        DOCTEST_REQUIRE(config::Prepare_Config_Snapshot(&config::synchronized_config_snapshot, &target,
            &config::Get_Active_Config()->required_mods, &network));
        orion2re::lua::s_lua_save_snapshot lua_snapshot;
        std::string error;
        DOCTEST_REQUIRE(orion2re::lua::Capture_Lua_Save_Snapshot(&lua_snapshot, &error));
        DOCTEST_REQUIRE(orion2re::lua::Finalize_Registry_Fingerprint(&lua_snapshot.registry, &target, &error));
        std::vector<uint8_t> lua_bytes(orion2re::lua::Lua_Save_Snapshot_Size(&lua_snapshot));
        SERIAL::Writer_ writer(lua_bytes.data(), static_cast<uint32_t>(lua_bytes.size()), "test Lua overflow");
        orion2re::lua::Write_Lua_Save_Snapshot(&writer, &lua_snapshot);
        config::s_network_config_candidate* candidate = nullptr;
        DOCTEST_REQUIRE(config::Stage_Network_Config_Snapshot(network.bytes.data(), static_cast<uint32_t>(network.bytes.size()), &candidate, &diagnostic));
        // This happens after staging: rollback must use the latest active state,
        // not the state that was current when the network offer arrived.
        MAPGEN::maximum_star_count = 102;
        DOCTEST_REQUIRE(config::Record_Active_Native_Config_Write(maximum));
        const auto prior = config::Get_Active_Config()->baseline.effective;
        DOCTEST_CHECK(config::Finalize_Network_Config_Snapshot(&candidate, lua_bytes.data(), writer.Length(), &error) == !reject);
        DOCTEST_CHECK(MAPGEN::maximum_star_count == (reject ? 102 : 101));
        if (reject) {
            Check_Effective_Config_Equals_(config::Get_Active_Config()->baseline.effective, prior);
            DOCTEST_CHECK(error.find("reject network restore") != std::string::npos);
        } else {
            DOCTEST_CHECK(candidate == nullptr);
            DOCTEST_CHECK(config::Get_Active_Config()->baseline.effective.state.values[maximum].integer_value == 101);
        }
        config::Discard_Network_Config_Snapshot(&candidate);
    }

    DOCTEST_TEST_CASE("host reconstruction preserves authoritative local and recorded native settings") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "interface scroll_interval = 2;\nenable persistent;\n");
        files.Write("data/mods/persistent/mod.json5", Manifest_("persistent", "lua: { api: 1, storage_version: 1 },\n"));
        files.Write("data/mods/persistent/config/main.cfg", "");
        files.Write("data/mods/persistent/main.lua", "events.on('game.loaded', function() end)\n");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config("orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        const auto maximum = Variable_Id_("map.star_count.4");
        MAPGEN::maximum_star_count = 102;
        DOCTEST_REQUIRE(config::Record_Active_Native_Config_Write(maximum));
        files.Write("orion2re.cfg", "interface scroll_interval = 9;\nenable persistent;\n");
        const auto prior = config::Get_Active_Config()->baseline.effective;
        std::string error;
        DOCTEST_REQUIRE(config::Reconstruct_Active_Lua_Runtimes(&error));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 2);
        DOCTEST_CHECK(MAPGEN::maximum_star_count == 102);
        Check_Effective_Config_Equals_(config::Get_Active_Config()->baseline.effective, prior);
        files.Write("data/mods/persistent/main.lua", "error('changed source')\n");
        DOCTEST_CHECK_FALSE(config::Reconstruct_Active_Lua_Runtimes(&error));
        DOCTEST_CHECK(COMBINIT::scroll_interval == 2);
        DOCTEST_CHECK(MAPGEN::maximum_star_count == 102);
        Check_Effective_Config_Equals_(config::Get_Active_Config()->baseline.effective, prior);
    }

    DOCTEST_TEST_CASE("gameplay host promotion reconstructs clean exact state before next turn") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "enable handoff_runtime;\n");
        files.Write(fs::path("data") / "mods" / "handoff_runtime" /
                "mod.json5",
            Manifest_("handoff_runtime",
                "  lua: { api: 1, storage_version: 1 },\n"));
        files.Write(fs::path("data") / "mods" / "handoff_runtime" /
                "config" / "main.cfg",
            "");
        const fs::path source_path = fs::path("data") / "mods" /
            "handoff_runtime" / "main.lua";
        const std::string source =
            "local transient = 0\n"
            "events.on('game.initialized', function()\n"
            "  if storage.bonus == nil then\n"
            "    transient = 20\n"
            "    storage.bonus = 7\n"
            "  else\n"
            "    transient = 99\n"
            "    storage.bonus = 99\n"
            "  end\n"
            "end)\n"
            "events.on('game.loaded', function()\n"
            "  transient = storage.bonus\n"
            "end)\n"
            "rules.modify('population.growth', function(context, value)\n"
            "  return value + transient + storage.bonus\n"
            "end)\n";
        files.Write(source_path, source);
        files.Initialize_Vfs();
        Set_Native_Defaults_();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));
        DOCTEST_REQUIRE(loaded);
        std::string lua_diagnostic;
        DOCTEST_REQUIRE(orion2re::lua::Dispatch_Active_Initialized_Event(
            &lua_diagnostic));
        DOCTEST_CHECK(Dispatch_Test_Growth_Rule_() == 32);

        std::vector<uint8_t> transferred_state(
            orion2re::lua::LUA_NETWORK_MAX_BYTES);
        uint32_t transferred_state_size = 0;
        DOCTEST_REQUIRE(orion2re::lua::Capture_Lua_Network_State(
            transferred_state.data(),
            static_cast<uint32_t>(transferred_state.size()),
            &transferred_state_size,
            &lua_diagnostic));
        DOCTEST_REQUIRE(orion2re::lua::Dispatch_Active_Initialized_Event(
            &lua_diagnostic));
        DOCTEST_CHECK(Dispatch_Test_Growth_Rule_() == 203);
        orion2re::lua::s_lua_network_state_transaction* transaction = nullptr;
        DOCTEST_REQUIRE(orion2re::lua::Prepare_Lua_Network_State(
            transferred_state.data(),
            transferred_state_size,
            0,
            &transaction,
            &lua_diagnostic));
        orion2re::lua::Commit_Lua_Network_State(&transaction);
        DOCTEST_CHECK(transaction == nullptr);
        DOCTEST_CHECK(Dispatch_Test_Growth_Rule_() == 111);

        s_player host{};
        host.objectives = PLAYER_OBJECTIVE_HUMAN;
        host.network_player_id = 202;
        host.surrender_to = -1;
        MOX::player = &host;
        MOX::PLAYER_NUM = 0;
        MOX::NUM_PLAYERS = 1;
        MOX::game_type = 3;
        NETMOX::net = {};
        NETMOX::net.net_host_id = 202;
        NETMOX::net.num_connected_players = 2;
        TEST_CONFIG_NETTURN::Reset_();
        TEST_CONFIG_NETTURN::next_turn_hook =
            &Dispatch_Netturn_Growth_Rule_;
        netturn_growth_result = -1;
        orion2re::lua::Reset_Lua_Network_Session();

        NETTURN::Net_Next_Turn();

        DOCTEST_CHECK(TEST_CONFIG_NETTURN::next_turn_calls == 1);
        DOCTEST_CHECK(netturn_growth_result == 19);
        const auto* reconstructed_runtime =
            orion2re::lua::Find_Mod_Runtime(
                orion2re::lua::Get_Active_Mod_Runtimes(),
                "handoff_runtime");
        DOCTEST_REQUIRE(reconstructed_runtime != nullptr);
        const std::string fingerprint =
            orion2re::lua::Get_Active_Registry()->fingerprint;

        files.Write(source_path, source + "local changed = true\n");
        orion2re::lua::Reset_Lua_Network_Session();
        TEST_CONFIG_NETTURN::next_turn_calls = 0;
        DOCTEST_CHECK_THROWS_WITH_AS(NETTURN::Net_Next_Turn(),
            doctest::Contains("source hash differs"),
            std::runtime_error);
        DOCTEST_CHECK(TEST_CONFIG_NETTURN::next_turn_calls == 0);
        DOCTEST_CHECK(orion2re::lua::Find_Mod_Runtime(
                          orion2re::lua::Get_Active_Mod_Runtimes(),
                          "handoff_runtime") == reconstructed_runtime);
        DOCTEST_CHECK(orion2re::lua::Get_Active_Registry()->fingerprint ==
            fingerprint);
        DOCTEST_CHECK(Dispatch_Test_Growth_Rule_() == 19);

        files.Write(source_path, source);
        orion2re::lua::Reset_Lua_Network_Session();
        s_player clients[2]{};
        clients[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        clients[0].network_player_id = 101;
        clients[0].surrender_to = -1;
        clients[1].objectives = PLAYER_OBJECTIVE_HUMAN;
        clients[1].network_player_id = 202;
        clients[1].surrender_to = -1;
        MOX::player = clients;
        MOX::PLAYER_NUM = 1;
        MOX::NUM_PLAYERS = 2;
        MOX::game_type = 3;
        NETTURN::waiting_message_fmt = const_cast<char*>("");
        NETMOX::net = {};
        NETMOX::net.net_host_id = 202;
        NETMOX::net.num_connected_players = 2;
        TEST_CONFIG_NETTURN::Reset_();
        TEST_CONFIG_NETTURN::next_turn_hook =
            &Dispatch_Netturn_Growth_Rule_;
        TEST_CONFIG_NETTURN::chat_box_input_loop_hook =
            &Release_Client_Next_Turn_Wait_;
        netturn_growth_result = -1;

        NETTURN::Net_Next_Turn();

        DOCTEST_CHECK(TEST_CONFIG_NETTURN::next_turn_calls == 0);
        DOCTEST_CHECK(netturn_growth_result == -1);
        TEST_CONFIG_NETTURN::Reset_();
        MOX::player = nullptr;
        MOX::PLAYER_NUM = 0;
        MOX::NUM_PLAYERS = 0;
        NETMOX::net = {};
        orion2re::lua::Reset_Lua_Network_Session();
    }

    DOCTEST_TEST_CASE("different mod version requires an explicit compatibility range") {
        s_config_test_directory files;
        files.Write("orion2re.cfg", "enable strict;\n");
        files.Write(fs::path("data") / "mods" / "strict" / "mod.json5",
            Versioned_Manifest_("strict", "2.0.0"));
        files.Write(fs::path("data") / "mods" / "strict" / "config" / "main.cfg",
            "");
        files.Initialize_Vfs();
        config::s_effective_config effective;
        config::s_cfg_diagnostic diagnostic;
        bool loaded = false;
        DOCTEST_REQUIRE(config::Load_Startup_Config(
            "orion2re.cfg", "base_game", &effective, &loaded, &diagnostic));

        config::s_config_state snapshot_state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&snapshot_state));
        const std::vector<config::s_mod_requirement> exact = {
            {"strict", "2.0.0"},
        };
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(
            exact, &config::synchronized_config_snapshot, &snapshot_state, &diagnostic));
        const std::vector<config::s_mod_requirement> equivalent = {
            {"strict", "2"},
        };
        DOCTEST_REQUIRE(config::Reapply_Config_Snapshot(
            equivalent, &config::synchronized_config_snapshot, &snapshot_state, &diagnostic));

        const std::vector<config::s_mod_requirement> different = {
            {"strict", "1.9.0"},
        };
        DOCTEST_CHECK_FALSE(config::Reapply_Config_Snapshot(
            different, &config::synchronized_config_snapshot, &snapshot_state, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            "installed package 'strict' version 2.0.0 is not compatible with required version 1.9.0");
    }
}