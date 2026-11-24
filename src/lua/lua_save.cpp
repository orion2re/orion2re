#include "lua/lua_save.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

#include "config/semantic_version.h"
#include "game/consts.h"
#include "game/game_state.h"
#include "game/serialization.h"
#include "lua/lua_game_ship_identity.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_game_traits.h"

using game::current_state;

namespace orion2re::lua {
    namespace {

        constexpr uint32_t LUA_SAVE_MAGIC = 0x554C324F;

        bool Read_U32(SERIAL::Reader_* reader, uint32_t* value) {
            if (reader->Remaining() < sizeof(uint32_t)) return false;
            *value = reader->Read_Uint32();
            return true;
        }

        bool Read_I16(SERIAL::Reader_* reader, std::int16_t* value) {
            if (reader->Remaining() < sizeof(std::int16_t)) return false;
            *value = reader->Read_Int16();
            return true;
        }

        bool Add_Size(uint64_t* total, uint64_t amount) {
            *total += amount;
            return *total <= LUA_SAVE_MAX_BYTES;
        }

        bool Add_String_Size(uint64_t* total, const std::string& value) {
            return value.size() <= LUA_SAVE_MAX_STRING_BYTES &&
                Add_Size(total, sizeof(uint32_t) + value.size());
        }

        void Write_String(SERIAL::Writer_* writer, const std::string& value) {
            writer->Write_Uint32(static_cast<uint32_t>(value.size()));
            writer->Write_Bytes(value.data(), value.size());
        }

        bool Read_String(SERIAL::Reader_* reader, std::string* value) {
            uint32_t size = 0;
            if (!Read_U32(reader, &size)) return false;
            if (size > LUA_SAVE_MAX_STRING_BYTES || size > reader->Remaining()) {
                return false;
            }
            value->resize(size);
            reader->Read_Bytes(value->data(), size);
            return true;
        }

        bool Valid_Count(uint32_t count, uint32_t limit) {
            return count <= limit;
        }

        bool Trait_Assignment_Less(
            const s_lua_saved_trait_assignment& left,
            const s_lua_saved_trait_assignment& right) {
            return left.player_id.id < right.player_id.id ||
                (left.player_id == right.player_id &&
                    left.trait_id < right.trait_id);
        }

        const s_lua_runtime_package_record* Find_Runtime_Package(
            const s_lua_registry& registry,
            const std::string& package_id) {
            for (const s_lua_runtime_package_record& package :
                registry.runtime_packages) {
                if (package.package_id == package_id) {
                    return &package;
                }
            }
            return nullptr;
        }

        const s_lua_saved_storage* Find_Saved_Storage(
            const s_lua_save_snapshot& snapshot,
            const std::string& package_id) {
            for (const s_lua_saved_storage& saved : snapshot.storage) {
                if (saved.package_id == package_id) {
                    return &saved;
                }
            }
            return nullptr;
        }

        bool Build_Configuration_Changes(
            const s_lua_save_snapshot& snapshot,
            const s_lua_registry& active,
            std::vector<s_lua_configuration_change>* changes,
            std::string* diagnostic) {
            const s_lua_registry& saved = snapshot.registry;
            changes->clear();
            if (saved.packages_high_to_low_priority.size() !=
                active.packages_high_to_low_priority.size()) {
                *diagnostic = "Lua package count differs";
                return false;
            }
            try {
                changes->reserve(
                    active.packages_high_to_low_priority.size());
                for (size_t offset =
                         active.packages_high_to_low_priority.size();
                     offset > 0;
                     --offset) {
                    const s_lua_package_record& old_package =
                        saved.packages_high_to_low_priority[offset - 1];
                    const s_lua_package_record& new_package =
                        active.packages_high_to_low_priority[offset - 1];
                    if (old_package.id != new_package.id) {
                        *diagnostic = "Lua package order differs";
                        return false;
                    }
                    const s_lua_runtime_package_record* old_runtime =
                        Find_Runtime_Package(saved, new_package.id);
                    const s_lua_runtime_package_record* new_runtime =
                        Find_Runtime_Package(active, new_package.id);
                    if ((old_runtime != nullptr &&
                            !config::Semantic_Versions_Equal(
                                old_runtime->package_version,
                                old_package.version)) ||
                        (new_runtime != nullptr &&
                            !config::Semantic_Versions_Equal(
                                new_runtime->package_version,
                                new_package.version))) {
                        *diagnostic =
                            "Lua package and runtime versions differ for package '" +
                            new_package.id + "'";
                        return false;
                    }
                    if (config::Semantic_Versions_Equal(
                            old_package.version, new_package.version)) {
                        continue;
                    }
                    s_lua_configuration_change change;
                    change.package_id = new_package.id;
                    change.old_package_version = old_package.version;
                    change.new_package_version = new_package.version;
                    if (old_runtime != nullptr) {
                        const s_lua_saved_storage* old_storage =
                            Find_Saved_Storage(snapshot, new_package.id);
                        if (old_storage == nullptr ||
                            old_storage->storage_version !=
                                old_runtime->storage_version) {
                            *diagnostic =
                                "saved Lua storage and registry versions differ for package '" +
                                new_package.id + "'";
                            return false;
                        }
                        change.old_storage_version =
                            old_storage->storage_version;
                    }
                    if (new_runtime != nullptr) {
                        change.new_storage_version =
                            new_runtime->storage_version;
                    }
                    changes->push_back(std::move(change));
                }
            } catch (...) {
                changes->clear();
                *diagnostic =
                    "unable to allocate Lua configuration change identity";
                return false;
            }
            return true;
        }

        bool Registry_Size(const s_lua_registry& registry, uint64_t* total) {
            if (!Add_Size(total, sizeof(uint32_t)) ||
                !Add_String_Size(total, registry.lua_version) ||
                !Add_Size(total, sizeof(uint32_t)) ||
                registry.packages_high_to_low_priority.size() > LUA_SAVE_MAX_PACKAGE_COUNT ||
                !Add_Size(total, sizeof(uint32_t))) {
                return false;
            }
            for (const s_lua_package_record& package :
                registry.packages_high_to_low_priority) {
                if (!Add_String_Size(total, package.id) ||
                    !Add_String_Size(total, package.version)) {
                    return false;
                }
            }
            if (registry.runtime_packages.size() > LUA_SAVE_MAX_PACKAGE_COUNT ||
                !Add_Size(total, sizeof(uint32_t))) {
                return false;
            }
            for (const s_lua_runtime_package_record& package :
                registry.runtime_packages) {
                if (!Add_String_Size(total, package.package_id) ||
                    !Add_String_Size(total, package.package_version) ||
                    !Add_Size(total, 2 * sizeof(uint32_t)) ||
                    package.sources.size() > LUA_SAVE_MAX_RECORD_COUNT ||
                    !Add_Size(total, sizeof(uint32_t))) {
                    return false;
                }
                for (const s_lua_runtime_source_record& source : package.sources) {
                    if (!Add_String_Size(total, source.path) ||
                        !Add_String_Size(total, source.source_hash)) {
                        return false;
                    }
                }
                if (package.migrations.size() > LUA_SAVE_MAX_RECORD_COUNT ||
                    !Add_Size(total, sizeof(uint32_t))) {
                    return false;
                }
                for (const s_lua_runtime_migration_record& migration :
                    package.migrations) {
                    if (!Add_Size(total, 2 * sizeof(uint32_t)) ||
                        !Add_String_Size(total, migration.path) ||
                        !Add_String_Size(total, migration.source_hash)) {
                        return false;
                    }
                }
                if (package.services.size() > LUA_SAVE_MAX_RECORD_COUNT ||
                    !Add_Size(total, sizeof(uint32_t))) {
                    return false;
                }
                for (const s_lua_runtime_service_record& service :
                    package.services) {
                    if (!Add_String_Size(total, service.name) ||
                        !Add_Size(total, sizeof(uint32_t))) {
                        return false;
                    }
                }
                if (package.callbacks.size() > LUA_SAVE_MAX_RECORD_COUNT ||
                    !Add_Size(total, sizeof(uint32_t))) {
                    return false;
                }
                for (const s_lua_runtime_callback_record& callback :
                    package.callbacks) {
                    if (!Add_Size(total, sizeof(uint32_t)) ||
                        !Add_String_Size(total, callback.extension_name) ||
                        !Add_Size(total, 2 * sizeof(uint32_t))) {
                        return false;
                    }
                }
            }
            return Add_String_Size(total, registry.prototype_identity) &&
                Add_String_Size(total, registry.synchronized_config_hash) &&
                Add_String_Size(total, registry.fingerprint);
        }

        void Write_Registry(SERIAL::Writer_* writer,
            const s_lua_registry& registry) {
            writer->Write_Uint32(registry.fingerprint_version);
            Write_String(writer, registry.lua_version);
            writer->Write_Uint32(registry.profile_version);
            writer->Write_Uint32(static_cast<uint32_t>(
                registry.packages_high_to_low_priority.size()));
            for (const s_lua_package_record& package :
                registry.packages_high_to_low_priority) {
                Write_String(writer, package.id);
                Write_String(writer, package.version);
            }
            writer->Write_Uint32(
                static_cast<uint32_t>(registry.runtime_packages.size()));
            for (const s_lua_runtime_package_record& package :
                registry.runtime_packages) {
                Write_String(writer, package.package_id);
                Write_String(writer, package.package_version);
                writer->Write_Uint32(package.api);
                writer->Write_Uint32(package.storage_version);
                writer->Write_Uint32(
                    static_cast<uint32_t>(package.sources.size()));
                for (const s_lua_runtime_source_record& source : package.sources) {
                    Write_String(writer, source.path);
                    Write_String(writer, source.source_hash);
                }
                writer->Write_Uint32(
                    static_cast<uint32_t>(package.migrations.size()));
                for (const s_lua_runtime_migration_record& migration :
                    package.migrations) {
                    writer->Write_Uint32(migration.from);
                    writer->Write_Uint32(migration.to);
                    Write_String(writer, migration.path);
                    Write_String(writer, migration.source_hash);
                }
                writer->Write_Uint32(
                    static_cast<uint32_t>(package.services.size()));
                for (const s_lua_runtime_service_record& service :
                    package.services) {
                    Write_String(writer, service.name);
                    writer->Write_Uint32(service.version);
                }
                writer->Write_Uint32(
                    static_cast<uint32_t>(package.callbacks.size()));
                for (const s_lua_runtime_callback_record& callback :
                    package.callbacks) {
                    writer->Write_Uint32(callback.kind);
                    Write_String(writer, callback.extension_name);
                    writer->Write_Uint32(callback.schema);
                    writer->Write_Uint32(static_cast<uint32_t>(
                        callback.registration_ordinal));
                }
            }
            Write_String(writer, registry.prototype_identity);
            Write_String(writer, registry.synchronized_config_hash);
            Write_String(writer, registry.fingerprint);
        }

        bool Read_Registry(SERIAL::Reader_* reader,
            uint32_t save_version,
            s_lua_registry* registry) {
            if (!Read_U32(reader, &registry->fingerprint_version)) return false;
            if (!Read_String(reader, &registry->lua_version)) return false;
            if (!Read_U32(reader, &registry->profile_version)) return false;
            uint32_t count = 0;
            if (!Read_U32(reader, &count)) return false;
            if (!Valid_Count(count, LUA_SAVE_MAX_PACKAGE_COUNT)) return false;
            registry->packages_high_to_low_priority.resize(count);
            for (s_lua_package_record& package :
                registry->packages_high_to_low_priority) {
                if (!Read_String(reader, &package.id) ||
                    !Read_String(reader, &package.version)) {
                    return false;
                }
            }
            if (!Read_U32(reader, &count)) return false;
            if (!Valid_Count(count, LUA_SAVE_MAX_PACKAGE_COUNT)) return false;
            registry->runtime_packages.resize(count);
            for (s_lua_runtime_package_record& package :
                registry->runtime_packages) {
                if (!Read_String(reader, &package.package_id) ||
                    !Read_String(reader, &package.package_version)) {
                    return false;
                }
                if (!Read_U32(reader, &package.api) ||
                    !Read_U32(reader, &package.storage_version)) {
                    return false;
                }
                uint32_t record_count = 0;
                if (!Read_U32(reader, &record_count)) return false;
                if (!Valid_Count(record_count, LUA_SAVE_MAX_RECORD_COUNT)) {
                    return false;
                }
                package.sources.resize(record_count);
                for (s_lua_runtime_source_record& source : package.sources) {
                    if (!Read_String(reader, &source.path) ||
                        !Read_String(reader, &source.source_hash)) {
                        return false;
                    }
                }
                if (!Read_U32(reader, &record_count)) return false;
                if (!Valid_Count(record_count, LUA_SAVE_MAX_RECORD_COUNT)) {
                    return false;
                }
                package.migrations.resize(record_count);
                for (s_lua_runtime_migration_record& migration :
                    package.migrations) {
                    if (!Read_U32(reader, &migration.from) ||
                        !Read_U32(reader, &migration.to) ||
                        !Read_String(reader, &migration.path) ||
                        !Read_String(reader, &migration.source_hash)) {
                        return false;
                    }
                }
                if (!Read_U32(reader, &record_count)) return false;
                if (!Valid_Count(record_count, LUA_SAVE_MAX_RECORD_COUNT)) {
                    return false;
                }
                package.services.resize(record_count);
                for (s_lua_runtime_service_record& service : package.services) {
                    if (!Read_String(reader, &service.name)) return false;
                    if (!Read_U32(reader, &service.version)) return false;
                }
                if (!Read_U32(reader, &record_count)) return false;
                if (!Valid_Count(record_count, LUA_SAVE_MAX_RECORD_COUNT)) {
                    return false;
                }
                package.callbacks.resize(record_count);
                for (s_lua_runtime_callback_record& callback :
                    package.callbacks) {
                    if (save_version >= 4) {
                        uint32_t kind = 0;
                        if (!Read_U32(reader, &kind) ||
                            kind > LUA_CALLBACK_RULE) {
                            return false;
                        }
                        callback.kind =
                            static_cast<e_lua_callback_kind>(kind);
                    }
                    if (!Read_String(reader, &callback.extension_name)) {
                        return false;
                    }
                    uint32_t ordinal = 0;
                    if (!Read_U32(reader, &callback.schema) ||
                        !Read_U32(reader, &ordinal)) {
                        return false;
                    }
                    callback.registration_ordinal = ordinal;
                }
            }
            if (save_version <= 5) {
                const size_t serialized_event_count = save_version < 5 ? 2 : 1;
                for (size_t event_index = 0;
                     event_index < serialized_event_count;
                     ++event_index) {
                    uint32_t listener_count = 0;
                    if (!Read_U32(reader, &listener_count) ||
                        !Valid_Count(listener_count,
                            LUA_SAVE_MAX_RECORD_COUNT)) {
                        return false;
                    }
                    for (uint32_t listener_index = 0;
                         listener_index < listener_count;
                         ++listener_index) {
                        std::string discarded;
                        uint32_t event = 0;
                        uint32_t discarded_u32 = 0;
                        if (!Read_String(reader, &discarded) ||
                            !Read_String(reader, &discarded) ||
                            !Read_String(reader, &discarded) ||
                            !Read_String(reader, &discarded) ||
                            !Read_U32(reader, &event) ||
                            event >= serialized_event_count ||
                            !Read_String(reader, &discarded) ||
                            !Read_U32(reader, &discarded_u32) ||
                            !Read_U32(reader, &discarded_u32) ||
                            !Read_String(reader, &discarded) ||
                            !Read_String(reader, &discarded) ||
                            !Read_U32(reader, &discarded_u32) ||
                            !Read_String(reader, &discarded)) {
                            return false;
                        }
                        for (int constraint_set = 0;
                             constraint_set < 2;
                             ++constraint_set) {
                            uint32_t constraint_count = 0;
                            if (!Read_U32(reader, &constraint_count) ||
                                !Valid_Count(constraint_count,
                                    LUA_SAVE_MAX_RECORD_COUNT)) {
                                return false;
                            }
                            for (uint32_t constraint_index = 0;
                                 constraint_index < constraint_count;
                                 ++constraint_index) {
                                if (!Read_String(reader, &discarded)) {
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
            return Read_String(reader, &registry->prototype_identity) &&
                Read_String(reader, &registry->synchronized_config_hash) &&
                Read_String(reader, &registry->fingerprint);
        }

    } // namespace

    bool Capture_Lua_Save_Snapshot(s_lua_save_snapshot* snapshot,
        std::string* diagnostic) {
        if (snapshot == nullptr || diagnostic == nullptr) return false;
        *snapshot = {};
        diagnostic->clear();
        snapshot->registry = *Get_Active_Registry();
        const s_lua_game_traits* traits = Get_Active_Game_Traits();
        const size_t assignment_count =
            Get_Player_Race_Trait_Assignment_Count(traits);
        if (assignment_count > LUA_SAVE_MAX_RECORD_COUNT) {
            *diagnostic = "Lua race trait assignment limit exceeded";
            return false;
        }
        snapshot->trait_assignments.reserve(assignment_count);
        for (size_t index = 0; index < assignment_count; ++index) {
            s_lua_player_race_trait_assignment assignment;
            if (!Get_Player_Race_Trait_Assignment(
                    traits, index, &assignment)) {
                *diagnostic = "unable to capture Lua race trait assignment";
                return false;
            }
            snapshot->trait_assignments.push_back(
                {assignment.player_id, std::move(assignment.trait_id)});
        }
        std::sort(snapshot->trait_assignments.begin(),
            snapshot->trait_assignments.end(),
            Trait_Assignment_Less);
        size_t generation_count = 0;
        if (!game::Capture_Ship_Generations(
                snapshot->ship_generations,
                MAX_SHIPS,
                &generation_count)) {
            *diagnostic = "unable to capture Lua ship generations";
            return false;
        }
        snapshot->ship_generation_count =
            static_cast<uint32_t>(generation_count);
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        const size_t count = Get_Mod_Runtime_Count(runtimes);
        if (count != snapshot->registry.runtime_packages.size()) {
            *diagnostic = "active Lua runtime and identity package counts differ";
            return false;
        }
        snapshot->storage.reserve(count);
        for (size_t index = 0; index < count; ++index) {
            s_lua_mod_runtime* runtime = const_cast<s_lua_mod_runtime*>(
                Get_Mod_Runtime(runtimes, index));
            s_lua_saved_storage saved;
            saved.package_id = Get_Mod_Runtime_Package_Id(runtime);
            saved.storage_version = Get_Mod_Runtime_Storage_Version(runtime);
            if (!Serialize_Mod_Storage(runtime, &saved.bytes, diagnostic)) {
                return false;
            }
            snapshot->storage.push_back(std::move(saved));
        }
        if (Lua_Save_Snapshot_Size(snapshot) == 0) {
            *diagnostic = "Lua save snapshot exceeds its size limit";
            return false;
        }
        return true;
    }

    uint32_t Lua_Save_Snapshot_Size(const s_lua_save_snapshot* snapshot) {
        if (snapshot == nullptr ||
            snapshot->storage.size() > LUA_SAVE_MAX_PACKAGE_COUNT) {
            return 0;
        }
        uint64_t total = 5 * sizeof(uint32_t);
        if (!Registry_Size(snapshot->registry, &total)) return 0;
        if (snapshot->trait_assignments.size() > LUA_SAVE_MAX_RECORD_COUNT) {
            return 0;
        }
        for (const s_lua_saved_trait_assignment& assignment :
            snapshot->trait_assignments) {
            if (!Add_Size(&total, sizeof(std::int16_t)) ||
                !Add_String_Size(&total, assignment.trait_id)) {
                return 0;
            }
        }
        if (snapshot->ship_generation_count > MAX_SHIPS ||
            !Add_Size(&total,
                sizeof(uint32_t) +
                    snapshot->ship_generation_count * sizeof(uint64_t))) {
            return 0;
        }
        for (const s_lua_saved_storage& saved : snapshot->storage) {
            if (!Add_String_Size(&total, saved.package_id) ||
                !Add_Size(&total, 2 * sizeof(uint32_t) + saved.bytes.size())) {
                return 0;
            }
        }
        return static_cast<uint32_t>(total);
    }

    void Write_Lua_Save_Snapshot(SERIAL::Writer_* writer,
        const s_lua_save_snapshot* snapshot) {
        writer->Write_Uint32(Lua_Save_Snapshot_Size(snapshot) -
            sizeof(uint32_t));
        writer->Write_Uint32(LUA_SAVE_MAGIC);
        writer->Write_Uint32(LUA_SAVE_FORMAT_VERSION);
        Write_Registry(writer, snapshot->registry);
        writer->Write_Uint32(
            static_cast<uint32_t>(snapshot->trait_assignments.size()));
        for (const s_lua_saved_trait_assignment& assignment :
            snapshot->trait_assignments) {
            writer->Write_Int16(static_cast<int16_t>(assignment.player_id.id));
            Write_String(writer, assignment.trait_id);
        }
        writer->Write_Uint32(snapshot->ship_generation_count);
        for (uint32_t index = 0;
             index < snapshot->ship_generation_count;
             ++index) {
            writer->Write_Uint64(snapshot->ship_generations[index]);
        }
        writer->Write_Uint32(static_cast<uint32_t>(snapshot->storage.size()));
        for (const s_lua_saved_storage& saved : snapshot->storage) {
            Write_String(writer, saved.package_id);
            writer->Write_Uint32(saved.storage_version);
            writer->Write_Uint32(static_cast<uint32_t>(saved.bytes.size()));
            writer->Write_Bytes(saved.bytes.data(), saved.bytes.size());
        }
    }

    bool Read_Lua_Save_Snapshot(SERIAL::Reader_* reader,
        s_lua_save_snapshot* snapshot,
        std::string* diagnostic) {
        if (reader == nullptr || snapshot == nullptr || diagnostic == nullptr) {
            return false;
        }
        *snapshot = {};
        diagnostic->clear();
        uint32_t section_size = 0;
        if (!Read_U32(reader, &section_size) ||
            section_size > LUA_SAVE_MAX_BYTES ||
            section_size > reader->Remaining()) {
            *diagnostic = "invalid Lua save snapshot size";
            return false;
        }
        SERIAL::Reader_ section(reader->Ptr(), section_size, "invalid Lua save snapshot");
        uint32_t magic = 0;
        uint32_t version = 0;
        if (!Read_U32(&section, &magic) || magic != LUA_SAVE_MAGIC ||
            !Read_U32(&section, &version) ||
            version < 1 || version > LUA_SAVE_FORMAT_VERSION ||
            !Read_Registry(&section, version, &snapshot->registry)) {
            *diagnostic = "invalid Lua save snapshot";
            return false;
        }
        uint32_t count = 0;
        if (version >= 2) {
            if (!Read_U32(&section, &count) ||
                !Valid_Count(count, LUA_SAVE_MAX_RECORD_COUNT)) {
                *diagnostic = "invalid Lua race trait assignment count";
                return false;
            }
            snapshot->trait_assignments.resize(count);
            for (size_t index = 0;
                 index < snapshot->trait_assignments.size();
                 ++index) {
                s_lua_saved_trait_assignment& assignment =
                    snapshot->trait_assignments[index];
                int16_t slot = -1;
                const bool has_slot = Read_I16(&section, &slot);
                assignment.player_id = ::game::id_player{static_cast<uint32_t>(slot)};
                if (!has_slot || assignment.player_id.id >= MAX_PLAYERS ||
                    !Read_String(&section, &assignment.trait_id) ||
                    (index != 0 &&
                        !Trait_Assignment_Less(
                            snapshot->trait_assignments[index - 1],
                            assignment))) {
                    *diagnostic =
                        "invalid or duplicate Lua race trait assignment";
                    return false;
                }
            }
        }
        if (version >= 3) {
            if (!Read_U32(&section, &snapshot->ship_generation_count) ||
                snapshot->ship_generation_count > MAX_SHIPS ||
                snapshot->ship_generation_count !=
                    static_cast<uint32_t>(static_cast<int16_t>(current_state.ship_slots.size())) ||
                section.Remaining() <
                    snapshot->ship_generation_count * sizeof(uint64_t)) {
                *diagnostic = "invalid Lua ship generation count";
                return false;
            }
            for (uint32_t index = 0;
                 index < snapshot->ship_generation_count;
                 ++index) {
                snapshot->ship_generations[index] = section.Read_Uint64();
            }
        }
        if (!Read_U32(&section, &count)) {
            *diagnostic = "invalid Lua save snapshot";
            return false;
        }
        if (!Valid_Count(count, LUA_SAVE_MAX_PACKAGE_COUNT)) {
            *diagnostic = "Lua save snapshot package limit exceeded";
            return false;
        }
        std::unordered_set<std::string> package_ids;
        snapshot->storage.resize(count);
        for (s_lua_saved_storage& saved : snapshot->storage) {
            if (!Read_String(&section, &saved.package_id) ||
                !package_ids.insert(saved.package_id).second) {
                *diagnostic = "invalid or duplicate Lua storage package identity";
                return false;
            }
            uint32_t size = 0;
            if (!Read_U32(&section, &saved.storage_version) ||
                !Read_U32(&section, &size) ||
                size > LUA_SAVE_MAX_BYTES || size > section.Remaining()) {
                *diagnostic = "invalid Lua storage size";
                return false;
            }
            saved.bytes.resize(size);
            section.Read_Bytes(saved.bytes.data(), size);
        }
        if (section.Remaining() != 0) {
            *diagnostic = "invalid trailing Lua save snapshot data";
            return false;
        }
        reader->Skip(section_size);
        return true;
    }

    bool Restore_Lua_Save_Snapshot(const s_lua_save_snapshot* snapshot,
        const s_lua_registry* candidate_registry,
        s_lua_mod_runtime_set* candidate_runtimes,
        std::string* diagnostic,
        bool defer_lifecycle,
        bool require_exact_identity) {
        if (snapshot == nullptr || candidate_registry == nullptr ||
            candidate_runtimes == nullptr || diagnostic == nullptr) {
            return false;
        }
        game::s_ship_generation_scope generation_scope(
            Get_Mod_Runtime_Ship_Generations(candidate_runtimes));
        if (!Compare_Registry_Save_Compatibility(&snapshot->registry,
                candidate_registry,
                !require_exact_identity && allow_source_hash_mismatch,
                diagnostic,
                require_exact_identity)) {
            return false;
        }
        const size_t runtime_count = Get_Mod_Runtime_Count(candidate_runtimes);
        if (snapshot->storage.size() != runtime_count ||
            candidate_registry->runtime_packages.size() != runtime_count) {
            *diagnostic =
                "Lua storage package count differs from active runtime";
            return false;
        }
        std::unordered_set<std::string> restored_packages;
        for (const s_lua_saved_storage& saved : snapshot->storage) {
            if (!restored_packages.insert(saved.package_id).second ||
                Find_Mod_Runtime(candidate_runtimes,
                    saved.package_id.c_str()) == nullptr) {
                *diagnostic = "Lua storage package '" + saved.package_id +
                    "' is not active or is duplicated";
                return false;
            }
        }
        std::vector<s_lua_player_race_trait_assignment> assignments;
        try {
            assignments.reserve(snapshot->trait_assignments.size());
            for (const s_lua_saved_trait_assignment& saved :
                snapshot->trait_assignments) {
                assignments.push_back({saved.player_id, saved.trait_id});
            }
        } catch (...) {
            *diagnostic = "unable to allocate Lua race trait assignments";
            return false;
        }
        if (!Replace_Player_Race_Trait_Assignments(
                Get_Game_Traits(candidate_runtimes),
                &assignments,
                diagnostic)) {
            return false;
        }
        if (snapshot->ship_generation_count == 0) {
            game::Restore_Legacy_Ship_Generations();
        } else if (!game::Restore_Ship_Generations(
                       snapshot->ship_generations,
                       snapshot->ship_generation_count)) {
            *diagnostic = "invalid Lua ship generations";
            return false;
        }
        for (const s_lua_saved_storage& saved : snapshot->storage) {
            s_lua_mod_runtime* runtime = Find_Mod_Runtime(
                candidate_runtimes, saved.package_id.c_str());
            if (!Restore_Mod_Storage(runtime,
                    saved.storage_version,
                    saved.bytes.data(),
                    saved.bytes.size(),
                    diagnostic)) {
                return false;
            }
        }
        if (!defer_lifecycle &&
            !Dispatch_Lua_Save_Snapshot_Lifecycle(snapshot,
                candidate_registry,
                candidate_runtimes,
                diagnostic)) {
            return false;
        }
        return true;
    }

    bool Dispatch_Lua_Save_Snapshot_Lifecycle(
        const s_lua_save_snapshot* snapshot,
        const s_lua_registry* candidate_registry,
        s_lua_mod_runtime_set* candidate_runtimes,
        std::string* diagnostic) {
        if (snapshot == nullptr || candidate_registry == nullptr ||
            candidate_runtimes == nullptr || diagnostic == nullptr) {
            return false;
        }
        std::vector<s_lua_configuration_change> changes;
        if (!Build_Configuration_Changes(*snapshot,
                *candidate_registry,
                &changes,
                diagnostic) ||
            !Dispatch_Configuration_Changed_Event(
                candidate_runtimes, &changes, diagnostic)) {
            return false;
        }
        return Dispatch_Loaded_Event(candidate_runtimes, diagnostic);
    }

} // namespace orion2re::lua
