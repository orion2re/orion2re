#include "config_loader.h"

#include <string>
#include <utility>
#include <vector>

#include "config/cfg_parser.h"
#include "config/config_profile.h"
#include "config/config_decode.h"
#include "config/config_identity.h"
#include "config/config_snapshot.h"
#include "config/mod_manager.h"
#include "game/serialization.h"
#include "game/config_refresh.h"
#include "game/vfs.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_save.h"

namespace config {
    struct s_network_config_candidate {
        s_config_with_mods_transaction* transaction = nullptr;
    };

    namespace {
        constexpr size_t CONFIG_MAX_INCLUDE_DEPTH = 64;
        constexpr size_t CONFIG_MAX_FILE_SIZE = 16 * 1024 * 1024;
        constexpr size_t CONFIG_MAX_PATH = 1024;
        s_active_config active_config;
        s_active_config launch_config;
        s_cfg_document launch_document;
        bool loaded_game_config = false;
        bool active_config_ready;
        s_prepared_config_snapshot saved_snapshot_cache;
        s_prepared_config_snapshot synchronized_snapshot_cache;
        std::vector<uint8_t> synchronized_identity_cache;
        bool saved_snapshot_ready;
        bool synchronized_snapshot_ready;
        bool synchronized_identity_ready;
        struct s_document_cache {
            s_cfg_document document;
            std::vector<std::pair<std::string, std::string>> sources;
        };
        std::unordered_map<std::string, s_document_cache> document_cache;
        uint64_t next_source_revision = 1;

        struct s_include_frame {
            std::string path;
        };

        struct s_loader {
            std::vector<s_include_frame> stack;
            s_cfg_document flattened;
            s_cfg_diagnostic* diagnostic = nullptr;
            std::vector<std::pair<std::string, std::string>> sources;
        };

        bool Fail(s_loader* loader, const s_source_location& location, const std::string& message) {
            loader->diagnostic->location = location;
            loader->diagnostic->message = message;
            return false;
        }

        bool Canonicalize_Path(const std::string& path, std::string* canonical) {
            char buffer[CONFIG_MAX_PATH];
            if (vfs::Canonicalize_Asset_Path(path.c_str(), buffer, sizeof(buffer)) == 0) {
                return false;
            }
            *canonical = buffer;
            return true;
        }

        std::string Directory(const std::string& path) {
            const size_t slash = path.rfind('/');
            return slash == std::string::npos ? std::string() : path.substr(0, slash);
        }

        bool Resolve_Include(const std::string& current_path, const s_cfg_token& token, std::string* resolved) {
            const std::string directory = Directory(current_path);
            const std::string combined = directory.empty() ? token.value : directory + "/" + token.value;
            return Canonicalize_Path(combined, resolved);
        }

        bool Read_File(s_loader* loader, const std::string& path, const s_source_location& location, std::string* source) {
            s_vfs_file* file = vfs::fopen(path.c_str(), "rb");
            if (file == nullptr) {
                return Fail(loader, location, "configuration file not found: '" + path + "'");
            }
            const int32_t size = vfs::fsize(file);
            if (size < 0 || static_cast<size_t>(size) > CONFIG_MAX_FILE_SIZE) {
                vfs::fclose(file);
                return Fail(loader, location, "configuration file has an invalid size: '" + path + "'");
            }
            source->resize(static_cast<size_t>(size));
            const size_t count = source->empty() ? 0 : vfs::fread(source->data(), 1, source->size(), file);
            const int32_t close_result = vfs::fclose(file);
            if (count != source->size() || close_result != 0) {
                return Fail(loader, location, "unable to read configuration file: '" + path + "'");
            }
            return true;
        }

        std::string Include_Chain(const s_loader* loader, size_t cycle_start, const std::string& repeated_path) {
            std::string chain;
            for (size_t index = cycle_start; index < loader->stack.size(); ++index) {
                if (!chain.empty()) {
                    chain += " -> ";
                }
                chain += loader->stack[index].path;
            }
            chain += " -> " + repeated_path;
            return chain;
        }

        bool Load_Recursive(s_loader* loader, const std::string& path, const s_source_location& include_location) {
            if (loader->stack.size() >= CONFIG_MAX_INCLUDE_DEPTH) {
                return Fail(loader, include_location, "include depth exceeds 64 files");
            }
            for (size_t index = 0; index < loader->stack.size(); ++index) {
                if (loader->stack[index].path == path) {
                    return Fail(loader, include_location, "include cycle: " + Include_Chain(loader, index, path));
                }
            }

            std::string source;
            if (!Read_File(loader, path, include_location, &source)) {
                return false;
            }
            loader->sources.emplace_back(path, source);
            s_cfg_document document;
            if (!Parse_Cfg(path.c_str(), source.data(), source.size(), &document, loader->diagnostic)) {
                return false;
            }

            s_include_frame frame;
            frame.path = path;
            loader->stack.push_back(std::move(frame));
            for (s_cfg_statement& statement : document.statements) {
                if (statement.kind == CFG_STATEMENT_STOP) {
                    break;
                }
                if (statement.kind == CFG_STATEMENT_INCLUDE) {
                    std::string include_path;
                    if (!Resolve_Include(path, statement.argument, &include_path)) {
                        loader->stack.pop_back();
                        return Fail(loader, statement.argument.location, "invalid include path '" + statement.argument.value + "'");
                    }
                    if (!Load_Recursive(loader, include_path, statement.argument.location)) {
                        loader->stack.pop_back();
                        return false;
                    }
                } else {
                    loader->flattened.statements.push_back(std::move(statement));
                }
            }
            loader->stack.pop_back();
            return true;
        }

        bool Load_Document(const std::string& entry_path, s_cfg_document* document, s_cfg_diagnostic* diagnostic) {
            s_loader loader;
            loader.diagnostic = diagnostic;
            const auto cached = document_cache.find(entry_path);
            if (cached != document_cache.end()) {
                bool unchanged = true;
                for (const auto& prior : cached->second.sources) {
                    std::string source;
                    s_source_location location;
                    location.source = prior.first;
                    if (!Read_File(&loader, prior.first, location, &source) || source != prior.second) {
                        unchanged = false;
                        break;
                    }
                }
                if (unchanged) {
                    *document = cached->second.document;
                    return true;
                }
                *diagnostic = {};
            }
            s_source_location entry_location;
            entry_location.source = entry_path;
            if (!Load_Recursive(&loader, entry_path, entry_location)) {
                return false;
            }
            *document = std::move(loader.flattened);
            document->source_revision = next_source_revision++;
            document_cache[entry_path] = {*document, std::move(loader.sources)};
            return true;
        }

        bool Load_Startup_Document(const std::string& entry_path,
            s_cfg_document* document,
            s_cfg_diagnostic* diagnostic) {
            *document = {};
            for (const std::string& path : {entry_path, std::string("user.cfg")}) {
                if (vfs::File_Exists(path.c_str()) == 0) continue;
                s_cfg_document part;
                if (!Load_Document(path, &part, diagnostic)) return false;
                for (auto& statement : part.statements) {
                    document->statements.push_back(std::move(statement));
                }
                if (entry_path == "user.cfg") break;
            }
            return true;
        }

        bool Config_Group_Changed(t_config_group group,
            const s_config_state& prior,
            const s_config_state& current) {
            if (prior.values.Shares_Baseline(current.values)) {
                const auto unchanged = [&](size_t index, const s_typed_value&) {
                    const auto id = static_cast<t_variable_id>(index);
                    return Get_Variable_Group(id) != group || Config_Values_Equal(prior.values[id], current.values[id]);
                };
                return !prior.values.Visit_Stored(unchanged) || !current.values.Visit_Stored(unchanged);
            }
            for (t_variable_id variable_id = 0;
                 variable_id < variable_count;
                 ++variable_id) {
                if (Get_Variable_Group(variable_id) == group &&
                    !Config_Values_Equal(prior.values[variable_id],
                        current.values[variable_id])) {
                    return true;
                }
            }
            return false;
        }

        bool Reload_All_Config_Groups(
            const s_config_group_descriptor* descriptor, bool) {
            return descriptor != nullptr;
        }

        void Invalidate_Snapshot_Cache() {
            saved_snapshot_cache.bytes.clear();
            synchronized_snapshot_cache.bytes.clear();
            synchronized_identity_cache.clear();
            saved_snapshot_ready = false;
            synchronized_snapshot_ready = false;
            synchronized_identity_ready = false;
        }

        // Builds the document a candidate mod set would be loaded from:
        // the active entry document with its enable lines replaced by the
        // candidate set, in order. The file itself is untouched.
        bool Build_Mod_Set_Document(const std::vector<std::string>& enabled_mods,
            s_cfg_document* document,
            s_cfg_diagnostic* diagnostic) {
            s_cfg_document active;
            if (vfs::File_Exists(active_config.entry_path.c_str()) != 0 &&
                !Load_Document(active_config.entry_path, &active, diagnostic)) {
                return false;
            }
            document->statements.clear();
            document->source_revision = 0;
            for (const s_cfg_statement& statement : active.statements) {
                if (statement.kind != CFG_STATEMENT_ENABLE) {
                    document->statements.push_back(statement);
                }
            }
            for (const std::string& package_id : enabled_mods) {
                if (package_id.empty()) {
                    diagnostic->message = "mod set contains an empty package ID";
                    return false;
                }
                s_cfg_statement enable;
                enable.kind = CFG_STATEMENT_ENABLE;
                enable.statement_number =
                    static_cast<uint32_t>(document->statements.size() + 1);
                enable.location.source = active_config.entry_path;
                enable.argument.kind = CFG_TOKEN_IDENTIFIER;
                enable.argument.location.source = active_config.entry_path;
                enable.argument.raw = package_id;
                enable.argument.value = package_id;
                document->statements.push_back(std::move(enable));
            }
            return true;
        }

    } // namespace

    bool Load_Config_File(const char* entry_path, const char* package_id, s_effective_config* effective, s_cfg_diagnostic* diagnostic) {
        if (entry_path == nullptr || package_id == nullptr ||
            effective == nullptr || diagnostic == nullptr) {
            return false;
        }
        *diagnostic = {};
        std::string normalized_entry;
        if (!Canonicalize_Path(entry_path, &normalized_entry)) {
            diagnostic->location.source = entry_path;
            diagnostic->message = "invalid configuration entry path";
            return false;
        }

        s_cfg_document document;
        if (!Load_Document(normalized_entry, &document, diagnostic)) {
            return false;
        }
        return Apply_Config_With_Mods(document, package_id, effective, diagnostic, Load_Document,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            CONFIG_PRECEDENCE_ENTRY_OVERRIDES_PACKAGES);
    }

    bool Load_Startup_Config(const char* entry_path, const char* package_id, s_effective_config* effective, bool* loaded, s_cfg_diagnostic* diagnostic, bool require_file) {
        s_config_profile profile("startup");
        if (entry_path == nullptr || package_id == nullptr || effective == nullptr ||
            loaded == nullptr || diagnostic == nullptr) {
            return false;
        }
        *loaded = false;
        *diagnostic = {};
        std::string normalized_entry;
        if (!Canonicalize_Path(entry_path, &normalized_entry)) {
            diagnostic->location.source = entry_path;
            diagnostic->message = "invalid configuration entry path";
            return false;
        }
        if (!Initialize_Effective_Config(effective)) {
            diagnostic->message = "unable to initialize configuration defaults";
            return false;
        }
        if (require_file && vfs::File_Exists(normalized_entry.c_str()) == 0) {
            diagnostic->location.source = entry_path;
            diagnostic->message = "selected startup configuration not found";
            return false;
        }
        if (vfs::File_Exists(normalized_entry.c_str()) == 0 &&
            vfs::File_Exists("user.cfg") == 0) {
            if (!Commit_Initial_State(&effective->state)) {
                diagnostic->message = "unable to commit configuration defaults";
                return false;
            }
            internal::Reset_Loaded_Mods();
            active_config = {};
            active_config.baseline.effective = *effective;
            active_config.entry_path = entry_path;
            active_config.base_package = package_id;
            active_config_ready = true;
            launch_config = active_config;
            launch_document = {};
            loaded_game_config = false;
            Invalidate_Snapshot_Cache();
            return true;
        }
        s_cfg_document document;
        if (!Load_Startup_Document(normalized_entry, &document, diagnostic)) {
            return false;
        }
        std::vector<std::string> package_order;
        std::vector<s_mod_requirement> required_mods;
        if (!Apply_Config_With_Mods(document, package_id, effective, diagnostic, Load_Document, &package_order, nullptr, nullptr, nullptr, nullptr, &required_mods, nullptr, nullptr, nullptr, CONFIG_PRECEDENCE_ENTRY_OVERRIDES_PACKAGES)) {
            return false;
        }
        active_config = {};
        active_config.baseline.effective = *effective;
        active_config.entry_path = normalized_entry;
        active_config.base_package = package_id;
        active_config.package_order = std::move(package_order);
        active_config.required_mods = std::move(required_mods);
        launch_config = active_config;
        launch_document = document;
        loaded_game_config = false;
        active_config_ready = true;
        Invalidate_Snapshot_Cache();
        *loaded = true;
        return true;
    }

    bool Commit_Active_Config_Writes(const s_config_write_set* writes,
        s_cfg_diagnostic* diagnostic) {
        if (writes == nullptr || diagnostic == nullptr || !active_config_ready) {
            return false;
        }
        *diagnostic = {};
        s_active_config candidate = active_config;
        for (const s_config_write& write : writes->writes) {
            s_prior_value prior;
            prior.leaf_index = write.leaf_index;
            if (!Read_State_Leaf_Value(&candidate.baseline.effective.state,
                    write.variable_id,
                    write.leaf_index,
                    &prior.value)) {
                diagnostic->message =
                    "unable to record active configuration override history";
                return false;
            }
            prior.provenance =
                candidate.baseline.effective.provenance.Read(write.variable_id);
            s_config_write_set single_write;
            single_write.writes.push_back(write);
            if (!Apply_Write_Set(&single_write,
                    &candidate.baseline.effective.state,
                    diagnostic)) {
                if (diagnostic->message.empty()) {
                    diagnostic->message =
                        "unable to commit active configuration overrides";
                }
                return false;
            }
            candidate.baseline.effective.prior_values[write.variable_id].push_back(std::move(prior));
            candidate.baseline.effective.provenance[write.variable_id].has_override = true;
            candidate.baseline.effective.provenance[write.variable_id].package_id =
                write.package_id.empty() ? "command_line" : write.package_id;
            candidate.baseline.effective.provenance[write.variable_id].location = write.provenance;
        }
        static_assert(CONFIG_GROUP_COUNT <= 32);
        const uint32_t group_mask = CONFIG_GROUP_COUNT == 32
            ? UINT32_MAX
            : (UINT32_C(1) << CONFIG_GROUP_COUNT) - 1;
        if (!Commit_Config_Transition(group_mask,
                &active_config.baseline.effective.state,
                &candidate.baseline.effective.state)) {
            diagnostic->message = "unable to commit active configuration overrides";
            return false;
        }
        active_config = std::move(candidate);
        if (!loaded_game_config) launch_config = active_config;
        Invalidate_Snapshot_Cache();
        CONFIG_REFRESH::Refresh_After_Config_Reload();
        return true;
    }

    bool Record_Active_Native_Config_Write(t_variable_id variable_id) {
        if (!active_config_ready || variable_id >= variable_count) return false;
        s_typed_value value;
        if (!generated::Get_Current_Value(variable_id, &value) ||
            !Set_State_Value(
                &active_config.baseline.effective.state, variable_id, value)) {
            return false;
        }
        Invalidate_Snapshot_Cache();
        return true;
    }

    bool Restore_Active_Config(const s_active_config* snapshot,
        s_cfg_diagnostic* diagnostic) {
        if (snapshot == nullptr || diagnostic == nullptr || !active_config_ready ||
            snapshot->baseline.effective.state.values.size() != variable_count ||
            snapshot->baseline.effective.provenance.size() != variable_count ||
            snapshot->baseline.effective.prior_values.size() != variable_count) {
            return false;
        }
        *diagnostic = {};
        static_assert(CONFIG_GROUP_COUNT <= 32);
        const uint32_t group_mask = CONFIG_GROUP_COUNT == 32
            ? UINT32_MAX
            : (UINT32_C(1) << CONFIG_GROUP_COUNT) - 1;
        if (!Commit_Config_Transition(group_mask,
                &active_config.baseline.effective.state,
                &snapshot->baseline.effective.state)) {
            diagnostic->message = "unable to restore active configuration";
            return false;
        }
        active_config = *snapshot;
        Invalidate_Snapshot_Cache();
        CONFIG_REFRESH::Refresh_After_Config_Reload();
        return true;
    }

    const s_active_config* Get_Active_Config() {
        return active_config_ready ? &active_config : nullptr;
    }

    bool Restore_Launch_Config(s_cfg_diagnostic* diagnostic) {
        if (diagnostic == nullptr) return false;
        *diagnostic = {};
        if (!loaded_game_config || !active_config_ready) return true;
        s_effective_config restored;
        std::vector<std::string> order;
        std::vector<s_mod_requirement> requirements;
        if (!Apply_Config_With_Mods(launch_document, launch_config.base_package.c_str(),
                &restored, diagnostic, Load_Document, &order, nullptr,
                &launch_config.required_mods, nullptr, nullptr, &requirements, nullptr,
                &active_config.baseline.effective.state, &launch_config.baseline.effective)) return false;
        active_config = launch_config;
        active_config.baseline.effective = std::move(restored);
        active_config.package_order = std::move(order);
        active_config.required_mods = std::move(requirements);
        loaded_game_config = false;
        Invalidate_Snapshot_Cache();
        return true;
    }

    void Shutdown_Config_Loader() {
        document_cache.clear();
        launch_config = {};
        launch_document = {};
        loaded_game_config = false;
        active_config = {};
        active_config_ready = false;
        internal::Reset_Loaded_Mods();
        Invalidate_Snapshot_Cache();
    }

    bool Get_Prepared_Active_Config_Snapshot(
        const s_config_snapshot_descriptor* descriptor,
        const s_prepared_config_snapshot** snapshot,
        s_cfg_diagnostic* diagnostic) {
        if (!active_config_ready || descriptor == nullptr ||
            snapshot == nullptr || diagnostic == nullptr) {
            return false;
        }
        *diagnostic = {};
        s_prepared_config_snapshot* cache = nullptr;
        bool* ready = nullptr;
        if (descriptor == &saved_config_snapshot) {
            cache = &saved_snapshot_cache;
            ready = &saved_snapshot_ready;
        } else if (descriptor == &synchronized_config_snapshot) {
            cache = &synchronized_snapshot_cache;
            ready = &synchronized_snapshot_ready;
        } else {
            diagnostic->message =
                "configuration snapshot descriptor is not cacheable";
            return false;
        }
        if (!*ready &&
            !Prepare_Config_Snapshot(descriptor,
                &active_config.baseline.effective.state,
                &active_config.required_mods,
                cache)) {
            diagnostic->message = "unable to prepare active configuration snapshot";
            return false;
        }
        *ready = true;
        *snapshot = cache;
        return true;
    }

    bool Get_Canonical_Active_Config_Identity(
        const std::vector<uint8_t>** identity,
        s_cfg_diagnostic* diagnostic) {
        if (!active_config_ready || identity == nullptr ||
            diagnostic == nullptr) {
            return false;
        }
        *diagnostic = {};
        if (!synchronized_identity_ready) {
            std::string identity_diagnostic;
            if (!Build_Sparse_Config_Identity(
                    &synchronized_config_snapshot,
                    &active_config.baseline.effective.state,
                    &synchronized_identity_cache,
                    &identity_diagnostic)) {
                diagnostic->message = identity_diagnostic.empty()
                    ? "unable to build active configuration identity"
                    : identity_diagnostic;
                return false;
            }
            synchronized_identity_ready = true;
        }
        *identity = &synchronized_identity_cache;
        return true;
    }

    bool Commit_Config_Snapshot(const s_config_snapshot_descriptor* descriptor,
        const s_config_state* state,
        s_cfg_diagnostic* diagnostic) {
        if (descriptor == nullptr || state == nullptr || diagnostic == nullptr ||
            descriptor->variables == nullptr || descriptor->variable_count == 0 ||
            state->values.size() != variable_count) {
            return false;
        }
        *diagnostic = {};
        for (uint32_t index = 0;
             descriptor != &saved_config_snapshot && descriptor != &synchronized_config_snapshot && index < descriptor->variable_count;
             ++index) {
            const t_variable_id variable_id = descriptor->variables[index];
            if (variable_id >= variable_count ||
                (descriptor->group_mask &
                    (UINT32_C(1) << Get_Variable_Group(variable_id))) == 0) {
                diagnostic->message = "invalid configuration snapshot descriptor";
                return false;
            }
        }
        if (!active_config_ready) {
            if (!Commit_Config_Groups(descriptor->group_mask, state)) {
                diagnostic->message = "unable to commit configuration snapshot";
                return false;
            }
        } else {
            s_active_config merged = active_config;
            if (!Overlay_Config_State(descriptor,
                    state,
                    &merged.baseline.effective.state)) {
                diagnostic->message = "invalid configuration snapshot state";
                return false;
            }
            const auto included = [&](size_t id) {
                return Snapshot_Contains_Variable(descriptor, static_cast<t_variable_id>(id));
            };
            merged.baseline.effective.provenance.Erase_Stored_If(included);
            merged.baseline.effective.prior_values.Erase_Stored_If(included);
            if (!Commit_Config_Transition(descriptor->group_mask,
                    &active_config.baseline.effective.state,
                    &merged.baseline.effective.state)) {
                diagnostic->message = "unable to commit configuration snapshot";
                return false;
            }
            active_config = std::move(merged);
            loaded_game_config = true;
            Invalidate_Snapshot_Cache();
        }

        bool refreshed_groups[CONFIG_GROUP_COUNT] = {};
        for (int group_index = 0; group_index < CONFIG_GROUP_COUNT; ++group_index) {
            refreshed_groups[group_index] =
                (descriptor->group_mask & (UINT32_C(1) << group_index)) != 0;
        }
        CONFIG_REFRESH::Refresh_After_Config_Reload(refreshed_groups);
        return true;
    }

    bool Reapply_Config_Snapshot(const std::vector<s_mod_requirement>& required_mods,
        const s_config_snapshot_descriptor* descriptor,
        const s_config_state* state,
        s_cfg_diagnostic* diagnostic,
        const orion2re::lua::s_lua_save_snapshot* lua_snapshot) {
        s_config_profile profile("restore-snapshot");
        if (descriptor == nullptr || state == nullptr || diagnostic == nullptr) {
            return false;
        }
        *diagnostic = {};
        if (!active_config_ready) {
            diagnostic->message = "active configuration is not initialized";
            return false;
        }
        constexpr char HASH_MISMATCH_SETTING[] =
            "interface.allow_lua_source_hash_mismatch";
        t_variable_id hash_mismatch_id = CONFIG_NO_VARIABLE;
        const bool preserve_hash_mismatch_override =
            Find_Variable(HASH_MISMATCH_SETTING,
                sizeof(HASH_MISMATCH_SETTING) - 1,
                &hash_mismatch_id) &&
            active_config.baseline.effective.provenance.Read(hash_mismatch_id).has_override;
        s_typed_value hash_mismatch_value;
        s_value_provenance hash_mismatch_provenance;
        std::vector<s_prior_value> hash_mismatch_prior_values;
        if (preserve_hash_mismatch_override) {
            hash_mismatch_value =
                active_config.baseline.effective.state.values.Read(hash_mismatch_id);
            hash_mismatch_provenance =
                active_config.baseline.effective.provenance.Read(hash_mismatch_id);
            hash_mismatch_prior_values =
                active_config.baseline.effective.prior_values.Read(hash_mismatch_id);
        }

        s_cfg_document document;
        if (!Load_Startup_Document(active_config.entry_path, &document, diagnostic)) {
            return false;
        }

        s_effective_config effective;
        std::vector<std::string> package_order;
        std::vector<s_mod_requirement> resolved_packages;
        if (!Apply_Config_With_Mods(
                document, active_config.base_package.c_str(), &effective, diagnostic, Load_Document, &package_order, nullptr, &required_mods, descriptor, state, &resolved_packages, lua_snapshot, &active_config.baseline.effective.state)) {
            return false;
        }
        if (preserve_hash_mismatch_override) {
            if (!Set_State_Value(&effective.state,
                    hash_mismatch_id,
                    hash_mismatch_value)) {
                diagnostic->message =
                    "unable to preserve Lua source hash override";
                return false;
            }
            effective.provenance[hash_mismatch_id] =
                std::move(hash_mismatch_provenance);
            effective.prior_values[hash_mismatch_id] =
                std::move(hash_mismatch_prior_values);
            orion2re::lua::allow_source_hash_mismatch =
                hash_mismatch_value.bool_value;
        }

        active_config.baseline.effective = std::move(effective);
        active_config.package_order = std::move(package_order);
        active_config.required_mods = std::move(resolved_packages);
        loaded_game_config = true;
        Invalidate_Snapshot_Cache();
        return true;
    }

    bool Stage_Network_Config_Snapshot(const void* bytes,
        uint32_t size,
        s_network_config_candidate** candidate,
        s_cfg_diagnostic* diagnostic) {
        if (bytes == nullptr || size == 0 || candidate == nullptr ||
            diagnostic == nullptr) {
            return false;
        }
        *candidate = nullptr;
        *diagnostic = {};
        if (!active_config_ready) {
            diagnostic->message = "active configuration is not initialized";
            return false;
        }
        SERIAL::Reader_ reader(bytes, size, "invalid network configuration snapshot");
        s_config_snapshot snapshot;
        if (!Read_Config_Snapshot(
                &reader, &synchronized_config_snapshot, &snapshot) ||
            reader.Remaining() != 0 || !snapshot.has_required_mods) {
            diagnostic->message = "invalid network configuration snapshot";
            return false;
        }
        s_cfg_document document;
        if (!Load_Startup_Document(active_config.entry_path, &document, diagnostic)) {
            return false;
        }
        auto* staged = new (std::nothrow) s_network_config_candidate;
        if (staged == nullptr) {
            diagnostic->message = "unable to allocate network configuration";
            return false;
        }
        if (!Stage_Config_With_Mods_Transaction(document,
                active_config.base_package.c_str(),
                diagnostic,
                Load_Document,
                &snapshot.required_mods,
                &synchronized_config_snapshot,
                &snapshot.state,
                &staged->transaction)) {
            delete staged;
            return false;
        }
        *candidate = staged;
        return true;
    }

    const orion2re::lua::s_lua_registry* Get_Network_Config_Candidate_Registry(
        const s_network_config_candidate* candidate) {
        return candidate == nullptr
            ? nullptr
            : Get_Staged_Config_Registry(candidate->transaction);
    }

    bool Finalize_Network_Config_Snapshot(
        s_network_config_candidate** candidate,
        const void* lua_snapshot,
        uint32_t lua_snapshot_size,
        std::string* diagnostic) {
        if (candidate == nullptr || *candidate == nullptr ||
            lua_snapshot == nullptr ||
            lua_snapshot_size == 0 || diagnostic == nullptr || !active_config_ready) {
            return false;
        }
        SERIAL::Reader_ reader(lua_snapshot,
            lua_snapshot_size,
            "invalid network Lua save snapshot");
        orion2re::lua::s_lua_save_snapshot snapshot;
        if (!orion2re::lua::Read_Lua_Save_Snapshot(
                &reader, &snapshot, diagnostic) ||
            reader.Remaining() != 0 ||
            !Restore_Lua_Save_To_Config_Transaction(
                (*candidate)->transaction, &snapshot,
                &active_config.baseline.effective.state, diagnostic)) {
            return false;
        }
        s_cfg_diagnostic config_diagnostic;
        if (!Commit_Config_With_Mods_Transaction(
                &(*candidate)->transaction,
                &active_config.baseline.effective,
                &active_config.package_order,
                &active_config.required_mods,
                &config_diagnostic)) {
            *diagnostic = config_diagnostic.message;
            return false;
        }
        Invalidate_Snapshot_Cache();
        delete *candidate;
        loaded_game_config = true;
        *candidate = nullptr;
        return true;
    }

    bool Commit_Network_Config_Snapshot(
        s_network_config_candidate** candidate,
        s_cfg_diagnostic* diagnostic) {
        if (candidate == nullptr || *candidate == nullptr ||
            diagnostic == nullptr) {
            return false;
        }
        s_network_config_candidate* staged = *candidate;
        if (!Commit_Config_With_Mods_Transaction(&staged->transaction,
                &active_config.baseline.effective,
                &active_config.package_order,
                &active_config.required_mods,
                diagnostic)) {
            return false;
        }
        Invalidate_Snapshot_Cache();
        delete staged;
        loaded_game_config = true;
        *candidate = nullptr;
        return true;
    }

    void Discard_Network_Config_Snapshot(
        s_network_config_candidate** candidate) {
        if (candidate == nullptr || *candidate == nullptr) {
            return;
        }
        s_network_config_candidate* staged = *candidate;
        Discard_Config_With_Mods_Transaction(&staged->transaction);
        delete staged;
        *candidate = nullptr;
    }

    bool Reconstruct_Active_Lua_Runtimes(std::string* diagnostic) {
        if (diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        if (!active_config_ready) {
            *diagnostic = "active configuration is not initialized";
            return false;
        }
        orion2re::lua::s_lua_save_snapshot snapshot;
        if (!orion2re::lua::Capture_Lua_Save_Snapshot(
                &snapshot, diagnostic)) {
            return false;
        }
        s_cfg_document document;
        s_cfg_diagnostic config_diagnostic;
        if (!Load_Startup_Document(
                active_config.entry_path, &document, &config_diagnostic)) {
            *diagnostic = config_diagnostic.message;
            return false;
        }
        const s_effective_config prior = active_config.baseline.effective;
        // Recorded native writes invalidate the loader identity cache, not the
        // active Lua registry. Compare against the current authoritative values.
        if (!orion2re::lua::Finalize_Registry_Fingerprint(
                &snapshot.registry, &prior.state, diagnostic)) return false;
        s_config_with_mods_transaction* transaction = nullptr;
        if (!Stage_Config_With_Mods_Transaction(document,
                active_config.base_package.c_str(),
                &config_diagnostic,
                Load_Document,
                &active_config.required_mods,
                nullptr,
                nullptr,
                &transaction,
                &prior)) {
            *diagnostic = config_diagnostic.message;
            return false;
        }
        if (!orion2re::lua::Compare_Registry_Fingerprints(
                &snapshot.registry,
                Get_Staged_Config_Registry(transaction),
                diagnostic)) {
            Discard_Config_With_Mods_Transaction(&transaction);
            return false;
        }
        if (!Restore_Lua_Save_To_Config_Transaction(
                transaction, &snapshot, &prior.state, diagnostic)) {
            Discard_Config_With_Mods_Transaction(&transaction);
            return false;
        }
        if (!Commit_Reconstructed_Lua_Runtimes_Transaction(
                &transaction, &config_diagnostic)) {
            *diagnostic = config_diagnostic.message;
            Discard_Config_With_Mods_Transaction(&transaction);
            return false;
        }
        return true;
    }

    bool Can_Reload_Config_Group(const s_config_group_descriptor* descriptor,
        bool network_game) {
        return descriptor != nullptr && !descriptor->save &&
            (!network_game || !descriptor->synchronize);
    }

    bool Reload_Active_Config_Groups(bool network_game,
        s_cfg_diagnostic* diagnostic,
        t_config_group_merger merge_group,
        t_config_group_reload_policy reload_policy) {
        if (diagnostic == nullptr) return false;
        *diagnostic = {};
        if (!active_config_ready) {
            diagnostic->message = "active configuration is not initialized";
            return false;
        }
        s_cfg_document document;
        if (!Load_Startup_Document(active_config.entry_path, &document, diagnostic)) {
            return false;
        }
        s_effective_config candidate;
        std::vector<std::string> package_order;
        if (!Stage_Config_With_Mods(document, active_config.base_package.c_str(), &candidate, &package_order, diagnostic, Load_Document, nullptr, CONFIG_PRECEDENCE_ENTRY_OVERRIDES_PACKAGES)) {
            return false;
        }
        if (package_order != active_config.package_order) {
            diagnostic->message =
                "active package order changed; restart or perform a full load";
            return false;
        }
        if (merge_group == nullptr) merge_group = Merge_Config_Group;
        if (reload_policy == nullptr) reload_policy = Can_Reload_Config_Group;
        s_effective_config merged = active_config.baseline.effective;
        bool reloaded_groups[CONFIG_GROUP_COUNT] = {};
        for (int group_index = 0; group_index < CONFIG_GROUP_COUNT; ++group_index) {
            const t_config_group group = (t_config_group)group_index;
            const s_config_group_descriptor* descriptor = Get_Config_Group(group);
            if (!reload_policy(descriptor, network_game)) {
                continue;
            }
            if (!merge_group(group, &candidate, &merged)) {
                diagnostic->message = "unable to commit configuration group";
                return false;
            }
            reloaded_groups[group] = true;
        }
        uint32_t reloaded_group_mask = 0;
        for (int group_index = 0; group_index < CONFIG_GROUP_COUNT; ++group_index) {
            if (reloaded_groups[group_index]) {
                reloaded_group_mask |= UINT32_C(1) << group_index;
            }
        }
        if (!Commit_Config_Transition(reloaded_group_mask,
                &active_config.baseline.effective.state,
                &merged.state)) {
            diagnostic->message = "unable to commit configuration state";
            return false;
        }
        std::vector<std::string> changed_groups;
        for (int group_index = 0;
             group_index < CONFIG_GROUP_COUNT;
             ++group_index) {
            if (!reloaded_groups[group_index]) {
                continue;
            }
            const t_config_group group =
                static_cast<t_config_group>(group_index);
            if (Config_Group_Changed(group,
                    active_config.baseline.effective.state,
                    merged.state)) {
                changed_groups.push_back(Get_Config_Group(group)->id);
            }
        }
        active_config.baseline.effective = std::move(merged);
        Invalidate_Snapshot_Cache();
        CONFIG_REFRESH::Refresh_After_Config_Reload(reloaded_groups);
        if (!network_game) {
            std::string lua_diagnostic;
            if (!orion2re::lua::Dispatch_Active_Configuration_Reloaded_Event(
                    &changed_groups, &lua_diagnostic)) {
                diagnostic->message = lua_diagnostic;
                return false;
            }
        }
        return true;
    }

    bool Reload_All_Active_Config_Groups(s_cfg_diagnostic* diagnostic) {
        return Reload_Active_Config_Groups(
            false, diagnostic, nullptr, Reload_All_Config_Groups);
    }

    bool Preview_Enabled_Mods(const std::vector<std::string>& enabled_mods,
        s_mod_set_preview* preview,
        s_cfg_diagnostic* diagnostic) {
        if (preview == nullptr || diagnostic == nullptr) return false;
        *preview = {};
        *diagnostic = {};
        if (!active_config_ready) {
            diagnostic->message = "active configuration is not initialized";
            return false;
        }
        s_cfg_document document;
        if (!Build_Mod_Set_Document(enabled_mods, &document, diagnostic)) {
            return false;
        }
        s_effective_config candidate;
        std::vector<std::string> package_order;
        std::vector<s_mod_requirement> resolved;
        if (!Stage_Config_With_Mods(document, active_config.base_package.c_str(),
                &candidate, &package_order, diagnostic, Load_Document, &resolved,
                CONFIG_PRECEDENCE_ENTRY_OVERRIDES_PACKAGES)) {
            return false;
        }
        preview->resolved_mods = std::move(resolved);
        preview->package_order = std::move(package_order);
        return true;
    }

    bool Apply_Enabled_Mods(const std::vector<std::string>& enabled_mods,
        s_cfg_diagnostic* diagnostic) {
        if (diagnostic == nullptr) return false;
        *diagnostic = {};
        if (!active_config_ready) {
            diagnostic->message = "active configuration is not initialized";
            return false;
        }
        if (loaded_game_config) {
            diagnostic->message =
                "cannot change mods while a game is loaded; return to the main menu";
            return false;
        }
        s_cfg_document document;
        if (!Build_Mod_Set_Document(enabled_mods, &document, diagnostic)) {
            return false;
        }
        s_effective_config effective;
        std::vector<std::string> package_order;
        std::vector<s_mod_requirement> resolved;
        if (!Apply_Config_With_Mods(document, active_config.base_package.c_str(),
                &effective, diagnostic, Load_Document, &package_order, nullptr,
                nullptr, nullptr, nullptr, &resolved, nullptr,
                &active_config.baseline.effective.state, nullptr,
                CONFIG_PRECEDENCE_ENTRY_OVERRIDES_PACKAGES)) {
            return false;
        }
        active_config.baseline.effective = std::move(effective);
        active_config.package_order = std::move(package_order);
        active_config.required_mods = std::move(resolved);
        // Returning to the main menu restores the launch config, so the
        // launch config must follow the picked set or the apply is undone.
        launch_config = active_config;
        launch_document = std::move(document);
        Invalidate_Snapshot_Cache();
        return true;
    }

} // namespace config