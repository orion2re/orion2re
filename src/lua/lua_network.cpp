#include "pch.h"
#include "game/game_wire_version.h"

#include "lua/lua_network.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <string_view>
#include <vector>

#include "config/config_loader.h"
#include "config/config_snapshot.h"
#include "game/serialization.h"
#include "game/sha256.h"
#include "lua/lua_game_ship_identity.h"
#include "lua/lua_game_traits.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_registry.h"
#include "lua/lua_save.h"
#include "lua/lua_storage.h"

namespace orion2re::lua {
    namespace {

        constexpr uint32_t LUID_MAGIC = 0x4449554C;
        constexpr uint16_t LUID_VERSION = 3;
        constexpr uint32_t LUOF_MAGIC = 0x464F554C;
        constexpr uint32_t LURS_MAGIC = 0x5352554C;
        constexpr uint16_t LUA_NETWORK_PROTOCOL_VERSION = 1;
        constexpr uint32_t LUA_NETWORK_HOOK_ALLOWLIST_VERSION = 3;
        constexpr uint32_t LUA_NETWORK_RESULT_HEADER_BYTES = 60;
        constexpr uint32_t LUA_NETWORK_OFFER_HEADER_BYTES = 24;
        constexpr uint32_t LUA_NETWORK_COMMIT_MAGIC = 0x434E554C;
        constexpr uint32_t LUA_NETWORK_COMMIT_BYTES = 12;
        constexpr uint32_t LUA_NETWORK_PAYLOAD_SAVED_STATE = 1;
        constexpr uint32_t LUNS_MAGIC = 0x534E554C;
        constexpr uint16_t LUNS_VERSION = 1;
        constexpr uint16_t LUNS_HEADER_BYTES = 16;
        config::s_network_config_candidate* pending_config_ = nullptr;
        uint64_t pending_offer_id_ = 0;
        std::vector<uint8_t> pending_configuration_;
        e_lua_network_setup_kind pending_setup_kind_ =
            LUA_NETWORK_SETUP_NEW_GAME;
        std::vector<uint8_t> queued_load_state_;
        uint64_t current_offer_id_ = 0;
        s_lua_network_digest expected_compatibility_digest_ = {};
        int32_t gameplay_host_authority_id_ = -1;

        struct s_identity_span {
            size_t begin = 0;
            size_t end = 0;
            uint32_t record_index = UINT32_MAX;
            std::string description;
        };

        void Add_Identity_Span(std::vector<s_identity_span>* spans,
            const std::vector<uint8_t>& bytes,
            uint32_t record_index,
            std::string description) {
            if (spans != nullptr) {
                const size_t begin = spans->empty() ? 0 : spans->back().end;
                spans->push_back({begin,
                    bytes.size(),
                    record_index,
                    std::move(description)});
            }
        }

        void Append_U16(std::vector<uint8_t>* bytes, uint16_t value) {
            bytes->push_back(static_cast<uint8_t>(value >> 8));
            bytes->push_back(static_cast<uint8_t>(value));
        }

        void Append_U32(std::vector<uint8_t>* bytes, uint32_t value) {
            for (int shift = 24; shift >= 0; shift -= 8) {
                bytes->push_back(static_cast<uint8_t>(value >> shift));
            }
        }

        bool Append_String(
            std::vector<uint8_t>* bytes, const std::string& value) {
            if (value.size() > LUA_SAVE_MAX_STRING_BYTES ||
                value.size() > UINT32_MAX) {
                return false;
            }
            Append_U32(bytes, static_cast<uint32_t>(value.size()));
            bytes->insert(bytes->end(), value.begin(), value.end());
            return bytes->size() <= LUA_NETWORK_MAX_BYTES;
        }

        bool Network_Event_Allowed(std::string_view name) {
            return name == "game.initialized" || name == "game.loaded" ||
                name == "game.configuration_changed" ||
                name == "new_game.technology_setup" ||
                name == "new_game.player_setup" ||
                name == "new_game.home_system_generated" ||
                name == "planet.generated";
        }

        bool Network_Rule_Allowed(std::string_view name) {
            return name == "population.growth";
        }

        bool Validate_Network_Hook_Allowlist(
            const s_lua_registry* registry, std::string* diagnostic) {
            for (const s_lua_runtime_package_record& package :
                registry->runtime_packages) {
                for (const s_lua_runtime_service_record& service :
                    package.services) {
                    if (service.name != "galaxy_generator" ||
                        service.version != 1) {
                        *diagnostic = "Lua package '" + package.package_id +
                            "' registers network-disabled service '" +
                            service.name + "'";
                        return false;
                    }
                }
                for (const s_lua_runtime_callback_record& callback :
                    package.callbacks) {
                    const bool allowed = callback.kind == LUA_CALLBACK_EVENT
                        ? Network_Event_Allowed(callback.extension_name)
                        : Network_Rule_Allowed(callback.extension_name);
                    if (!allowed) {
                        *diagnostic = "Lua package '" + package.package_id +
                            "' registers network-disabled " +
                            (callback.kind == LUA_CALLBACK_RULE ? "rule '" : "event '") +
                            callback.extension_name + "'";
                        return false;
                    }
                }
            }
            return true;
        }

        bool Build_Compatibility_Record(
            const s_lua_registry* registry,
            std::vector<uint8_t>* bytes,
            s_lua_network_digest* digest,
            std::string* diagnostic,
            std::vector<s_identity_span>* spans = nullptr) {
            if (registry == nullptr || bytes == nullptr || digest == nullptr ||
                diagnostic == nullptr) {
                return false;
            }
            bytes->clear();
            if (spans != nullptr) spans->clear();
            diagnostic->clear();
            if (!Validate_Network_Hook_Allowlist(registry, diagnostic)) {
                return false;
            }
            try {
                Append_U32(bytes, LUID_MAGIC);
                Append_U16(bytes, LUID_VERSION);
                Append_U16(bytes, 8);
                if (!Append_String(bytes, NETPROTO::GAME_WIRE_VERSION) ||
                    !Append_String(bytes, registry->lua_version)) {
                    *diagnostic = "Lua network identity string limit exceeded";
                    return false;
                }
                Append_U32(bytes, registry->profile_version);
                Append_U32(bytes, registry->fingerprint_version);
                Append_U32(bytes, LUA_STORAGE_FORMAT_VERSION);
                Append_U32(bytes, LUA_NETWORK_HOOK_ALLOWLIST_VERSION);
                Add_Identity_Span(spans,
                    *bytes,
                    UINT32_MAX,
                    "engine or Lua runtime profile");
                if (registry->packages_high_to_low_priority.size() >
                    LUA_SAVE_MAX_PACKAGE_COUNT) {
                    *diagnostic = "Lua network package limit exceeded";
                    return false;
                }
                Append_U32(bytes, static_cast<uint32_t>(registry->packages_high_to_low_priority.size()));
                uint32_t record_index = 0;
                Add_Identity_Span(spans,
                    *bytes,
                    record_index,
                    "package list");
                for (const s_lua_package_record& package :
                    registry->packages_high_to_low_priority) {
                    if (!Append_String(bytes, package.id) ||
                        !Append_String(bytes, package.version)) {
                        *diagnostic = "Lua network package identity limit exceeded";
                        return false;
                    }
                    Add_Identity_Span(spans,
                        *bytes,
                        record_index++,
                        "package '" + package.id + "'");
                }
                if (registry->runtime_packages.size() >
                    LUA_SAVE_MAX_PACKAGE_COUNT) {
                    *diagnostic = "Lua network runtime package limit exceeded";
                    return false;
                }
                Append_U32(bytes,
                    static_cast<uint32_t>(registry->runtime_packages.size()));
                Add_Identity_Span(spans,
                    *bytes,
                    record_index,
                    "runtime package list");
                for (const s_lua_runtime_package_record& package :
                    registry->runtime_packages) {
                    if (!Append_String(bytes, package.package_id) ||
                        !Append_String(bytes, package.package_version)) {
                        *diagnostic = "Lua network runtime identity limit exceeded";
                        return false;
                    }
                    Append_U32(bytes, package.api);
                    Append_U32(bytes, package.storage_version);
                    if (package.sources.size() > LUA_SAVE_MAX_RECORD_COUNT ||
                        package.migrations.size() > LUA_SAVE_MAX_RECORD_COUNT ||
                        package.services.size() > LUA_SAVE_MAX_RECORD_COUNT ||
                        package.callbacks.size() > LUA_SAVE_MAX_RECORD_COUNT) {
                        *diagnostic = "Lua network identity record limit exceeded";
                        return false;
                    }
                    Add_Identity_Span(spans,
                        *bytes,
                        record_index++,
                        "runtime package '" + package.package_id + "'");
                    Append_U32(bytes,
                        static_cast<uint32_t>(package.sources.size()));
                    Add_Identity_Span(spans,
                        *bytes,
                        record_index,
                        "source list for package '" + package.package_id + "'");
                    for (const s_lua_runtime_source_record& source :
                        package.sources) {
                        if (!Append_String(bytes, source.path) ||
                            !Append_String(bytes, source.source_hash)) {
                            *diagnostic = "Lua network source identity limit exceeded";
                            return false;
                        }
                        Add_Identity_Span(spans,
                            *bytes,
                            record_index++,
                            "source '" + source.path + "'");
                    }
                    Append_U32(bytes,
                        static_cast<uint32_t>(package.migrations.size()));
                    Add_Identity_Span(spans,
                        *bytes,
                        record_index,
                        "migration list for package '" + package.package_id + "'");
                    for (const s_lua_runtime_migration_record& migration :
                        package.migrations) {
                        Append_U32(bytes, migration.from);
                        Append_U32(bytes, migration.to);
                        if (!Append_String(bytes, migration.path) ||
                            !Append_String(bytes, migration.source_hash)) {
                            *diagnostic = "Lua network migration identity limit exceeded";
                            return false;
                        }
                        Add_Identity_Span(spans,
                            *bytes,
                            record_index++,
                            "migration '" + migration.path + "'");
                    }
                    Append_U32(bytes,
                        static_cast<uint32_t>(package.services.size()));
                    Add_Identity_Span(spans,
                        *bytes,
                        record_index,
                        "service list for package '" + package.package_id + "'");
                    for (const s_lua_runtime_service_record& service :
                        package.services) {
                        if (!Append_String(bytes, service.name)) {
                            *diagnostic = "Lua network service identity limit exceeded";
                            return false;
                        }
                        Append_U32(bytes, service.version);
                        Add_Identity_Span(spans,
                            *bytes,
                            record_index++,
                            "service '" + service.name + "'");
                    }
                    Append_U32(bytes,
                        static_cast<uint32_t>(package.callbacks.size()));
                    Add_Identity_Span(spans,
                        *bytes,
                        record_index,
                        "callback list for package '" + package.package_id + "'");
                    for (const s_lua_runtime_callback_record& callback :
                        package.callbacks) {
                        if (callback.registration_ordinal > UINT32_MAX ||
                            !Append_String(bytes, callback.extension_name) ||
                            !Append_String(bytes, callback.registration_source) ||
                            !Append_String(bytes, callback.callback_source) ||
                            !Append_String(bytes, callback.function_name)) {
                            *diagnostic =
                                "Lua network callback identity limit exceeded";
                            return false;
                        }
                        Append_U32(bytes, callback.kind);
                        Append_U32(bytes, callback.schema);
                        Append_U32(bytes, static_cast<uint32_t>(callback.registration_ordinal));
                        Append_U32(bytes, static_cast<uint32_t>(callback.registration_line));
                        Append_U32(bytes,
                            static_cast<uint32_t>(callback.callback_line));
                        Add_Identity_Span(spans,
                            *bytes,
                            record_index++,
                            "callback '" + callback.extension_name + "'");
                    }
                }
                if (!Append_String(bytes, registry->prototype_identity)) {
                    *diagnostic = "Lua network identity exceeds its size limit";
                    return false;
                }
                Add_Identity_Span(spans,
                    *bytes,
                    record_index++,
                    "prototype identity");
                if (!Append_String(bytes, registry->synchronized_config_hash) ||
                    bytes->size() > LUA_NETWORK_MAX_BYTES) {
                    *diagnostic = "Lua network identity exceeds its size limit";
                    return false;
                }
                Add_Identity_Span(spans,
                    *bytes,
                    record_index,
                    "synchronized configuration identity");
            } catch (...) {
                bytes->clear();
                *diagnostic = "unable to allocate Lua network identity";
                return false;
            }
            return crypto::sha256(bytes->data(),
                bytes->size(),
                digest->bytes,
                sizeof(digest->bytes));
        }

        bool Read_U32(SERIAL::Reader_* reader, uint32_t* value) {
            if (reader->Remaining() < sizeof(uint32_t)) return false;
            *value = reader->Read_Uint32();
            return true;
        }

        bool Read_String(SERIAL::Reader_* reader, std::string* value) {
            uint32_t size = 0;
            if (!Read_U32(reader, &size) ||
                size > LUA_SAVE_MAX_STRING_BYTES ||
                size > reader->Remaining()) {
                return false;
            }
            value->resize(size);
            reader->Read_Bytes(value->data(), size);
            return true;
        }

        bool Assignment_Less(
            const s_lua_player_race_trait_assignment& left,
            const s_lua_player_race_trait_assignment& right) {
            return left.player_id.id < right.player_id.id ||
                (left.player_id == right.player_id &&
                    left.trait_id < right.trait_id);
        }

        void Clear_Pending_Setup() {
            config::Discard_Network_Config_Snapshot(&pending_config_);
            pending_offer_id_ = 0;
            pending_configuration_.clear();
            pending_setup_kind_ = LUA_NETWORK_SETUP_NEW_GAME;
        }

    } // namespace

    struct s_lua_network_state_transaction {
        std::vector<s_lua_storage_restore_transaction*> storage;
        s_lua_game_traits_assignment_transaction* trait_assignments = nullptr;
        uint32_t ship_generation_count = 0;
        uint64_t ship_generations[MAX_SHIPS] = {};
    };

    bool Network_Lua_Enabled() {
        const s_lua_registry* registry = Get_Active_Registry();
        if (registry == nullptr) {
            return false;
        }
        return !registry->runtime_packages.empty();
    }

    bool Write_Lua_Network_Setup_Offer(
        void* buffer,
        uint32_t capacity,
        e_lua_network_setup_kind kind,
        uint64_t offer_id,
        const void* configuration,
        uint32_t configuration_size,
        uint32_t* size,
        s_lua_network_digest* digest,
        std::string* diagnostic) {
        if (buffer == nullptr || configuration == nullptr || size == nullptr ||
            digest == nullptr || diagnostic == nullptr ||
            configuration_size == 0 || offer_id == 0 ||
            (kind != LUA_NETWORK_SETUP_NEW_GAME &&
                kind != LUA_NETWORK_SETUP_LOAD_GAME)) {
            return false;
        }
        std::vector<uint8_t> identity;
        if (!Build_Compatibility_Record(
                Get_Active_Registry(), &identity, digest, diagnostic)) {
            return false;
        }
        const uint64_t total = LUA_NETWORK_OFFER_HEADER_BYTES +
            static_cast<uint64_t>(configuration_size) + identity.size();
        if (total > capacity || total > LUA_NETWORK_MAX_BYTES) {
            *diagnostic = "Lua network setup offer exceeds its size limit";
            return false;
        }
        SERIAL::Writer_ writer(buffer, capacity, "Lua network setup offer overflow");
        writer.Write_Uint32(LUOF_MAGIC);
        writer.Write_Uint16(LUA_NETWORK_PROTOCOL_VERSION);
        writer.Write_Uint8(static_cast<uint8_t>(kind));
        writer.Write_Uint8(0);
        writer.Write_Uint64(offer_id);
        writer.Write_Uint32(configuration_size);
        writer.Write_Uint32(static_cast<uint32_t>(identity.size()));
        writer.Write_Bytes(configuration, configuration_size);
        writer.Write_Bytes(identity.data(), identity.size());
        *size = writer.Length();
        return true;
    }

    bool Begin_Lua_Network_Setup_Offer(
        void* buffer,
        uint32_t capacity,
        e_lua_network_setup_kind kind,
        uint32_t* size,
        std::string* diagnostic) {
        if (buffer == nullptr || size == nullptr || diagnostic == nullptr) {
            return false;
        }
        const config::s_active_config* active = config::Get_Active_Config();
        if (active == nullptr) {
            *diagnostic = "unable to capture Lua multiplayer configuration";
            return false;
        }
        const config::s_prepared_config_snapshot* configuration = nullptr;
        config::s_cfg_diagnostic config_diagnostic;
        if (!config::Get_Prepared_Active_Config_Snapshot(
                &config::synchronized_config_snapshot,
                &configuration,
                &config_diagnostic)) {
            *diagnostic = "unable to build Lua multiplayer configuration";
            return false;
        }
        uint64_t offer_id = current_offer_id_ + 1;
        if (offer_id == 0) offer_id = 1;
        s_lua_network_digest digest;
        if (!Write_Lua_Network_Setup_Offer(buffer,
                capacity,
                kind,
                offer_id,
                configuration->bytes.data(),
                static_cast<uint32_t>(configuration->bytes.size()),
                size,
                &digest,
                diagnostic)) {
            return false;
        }
        Set_Lua_Network_Offer_Context(offer_id, &digest);
        return true;
    }

    bool Read_Lua_Network_Setup_Offer(
        const void* buffer,
        uint32_t size,
        s_lua_network_setup_offer* offer,
        std::string* diagnostic) {
        if (buffer == nullptr || offer == nullptr || diagnostic == nullptr ||
            size < LUA_NETWORK_OFFER_HEADER_BYTES ||
            size > LUA_NETWORK_MAX_BYTES) {
            return false;
        }
        *offer = {};
        SERIAL::Reader_ reader(buffer, size, "invalid Lua network setup offer");
        const uint32_t magic = reader.Read_Uint32();
        const uint16_t version = reader.Read_Uint16();
        const uint8_t kind = reader.Read_Uint8();
        const uint8_t reserved = reader.Read_Uint8();
        const uint64_t offer_id = reader.Read_Uint64();
        const uint32_t configuration_size = reader.Read_Uint32();
        const uint32_t identity_size = reader.Read_Uint32();
        const uint64_t expected = LUA_NETWORK_OFFER_HEADER_BYTES +
            static_cast<uint64_t>(configuration_size) + identity_size;
        if (magic != LUOF_MAGIC || version != LUA_NETWORK_PROTOCOL_VERSION ||
            reserved != 0 || kind > LUA_NETWORK_SETUP_LOAD_GAME ||
            offer_id == 0 || configuration_size == 0 || identity_size == 0 ||
            expected != size) {
            *diagnostic = "invalid Lua network setup offer";
            return false;
        }
        offer->kind = static_cast<e_lua_network_setup_kind>(kind);
        offer->offer_id = offer_id;
        offer->configuration = reader.Ptr();
        offer->configuration_size = configuration_size;
        reader.Skip(configuration_size);
        offer->identity = reader.Ptr();
        offer->identity_size = identity_size;
        diagnostic->clear();
        return true;
    }

    bool Validate_Lua_Network_Setup_Offer(
        const s_lua_network_setup_offer* offer,
        const s_lua_registry* registry,
        s_lua_network_digest* local_digest,
        uint32_t* mismatch_record_index,
        std::string* diagnostic) {
        if (offer == nullptr || registry == nullptr || local_digest == nullptr ||
            mismatch_record_index == nullptr || diagnostic == nullptr ||
            offer->identity == nullptr ||
            offer->identity_size == 0) {
            return false;
        }
        *mismatch_record_index = UINT32_MAX;
        std::vector<uint8_t> local_identity;
        std::vector<s_identity_span> spans;
        if (!Build_Compatibility_Record(
                registry,
                &local_identity,
                local_digest,
                diagnostic,
                &spans)) {
            return false;
        }
        if (local_identity.size() != offer->identity_size ||
            memcmp(local_identity.data(),
                offer->identity,
                offer->identity_size) != 0) {
            const size_t common_size = std::min(
                local_identity.size(),
                static_cast<size_t>(offer->identity_size));
            size_t mismatch = 0;
            while (mismatch < common_size &&
                local_identity[mismatch] == offer->identity[mismatch]) {
                ++mismatch;
            }
            const auto span = std::find_if(spans.begin(),
                spans.end(),
                [mismatch](const s_identity_span& value) {
                    return mismatch >= value.begin && mismatch < value.end;
                });
            if (span == spans.end()) {
                *diagnostic = "Lua network identity length differs";
            } else {
                *mismatch_record_index = span->record_index;
                *diagnostic = "Lua network " + span->description + " differs";
            }
            return false;
        }
        return true;
    }

    bool Write_Lua_Network_Result(
        void* buffer,
        uint32_t capacity,
        const s_lua_network_result* result,
        uint32_t* size) {
        if (buffer == nullptr || result == nullptr || size == nullptr ||
            result->offer_id == 0 ||
            result->diagnostic.size() > LUA_NETWORK_MAX_DIAGNOSTIC_BYTES ||
            capacity < LUA_NETWORK_RESULT_HEADER_BYTES +
                    result->diagnostic.size()) {
            return false;
        }
        SERIAL::Writer_ writer(buffer, capacity, "Lua network result overflow");
        writer.Write_Uint32(LURS_MAGIC);
        writer.Write_Uint16(LUA_NETWORK_PROTOCOL_VERSION);
        writer.Write_Uint8(result->accepted ? 1 : 0);
        writer.Write_Uint8(0);
        writer.Write_Uint64(result->offer_id);
        writer.Write_Bytes(result->digest.bytes, sizeof(result->digest.bytes));
        writer.Write_Uint16(static_cast<uint16_t>(result->diagnostic_code));
        writer.Write_Uint16(0);
        writer.Write_Uint32(result->record_index);
        writer.Write_Uint32(static_cast<uint32_t>(result->diagnostic.size()));
        writer.Write_Bytes(
            result->diagnostic.data(), result->diagnostic.size());
        *size = writer.Length();
        return true;
    }

    bool Read_Lua_Network_Result(
        const void* buffer,
        uint32_t size,
        s_lua_network_result* result) {
        if (buffer == nullptr || result == nullptr ||
            size < LUA_NETWORK_RESULT_HEADER_BYTES ||
            size > LUA_NETWORK_RESULT_HEADER_BYTES +
                    LUA_NETWORK_MAX_DIAGNOSTIC_BYTES) {
            return false;
        }
        *result = {};
        SERIAL::Reader_ reader(buffer, size, "invalid Lua network result");
        const uint32_t magic = reader.Read_Uint32();
        const uint16_t version = reader.Read_Uint16();
        const uint8_t status = reader.Read_Uint8();
        const uint8_t reserved = reader.Read_Uint8();
        result->offer_id = reader.Read_Uint64();
        reader.Read_Bytes(
            result->digest.bytes, sizeof(result->digest.bytes));
        const uint16_t diagnostic_code = reader.Read_Uint16();
        const uint16_t diagnostic_reserved = reader.Read_Uint16();
        result->record_index = reader.Read_Uint32();
        const uint32_t diagnostic_size = reader.Read_Uint32();
        if (magic != LURS_MAGIC ||
            version != LUA_NETWORK_PROTOCOL_VERSION || reserved != 0 ||
            status > 1 || result->offer_id == 0 ||
            diagnostic_reserved != 0 ||
            diagnostic_code > LUA_NETWORK_DIAGNOSTIC_RUNTIME_IDENTITY ||
            diagnostic_size > LUA_NETWORK_MAX_DIAGNOSTIC_BYTES ||
            diagnostic_size != reader.Remaining()) {
            return false;
        }
        result->accepted = status != 0;
        result->diagnostic_code =
            static_cast<e_lua_network_diagnostic_code>(diagnostic_code);
        result->diagnostic.resize(diagnostic_size);
        reader.Read_Bytes(result->diagnostic.data(), diagnostic_size);
        return true;
    }

    bool Validate_Lua_Network_Setup_Result(
        const void* buffer,
        uint32_t size,
        std::string* diagnostic) {
        if (diagnostic == nullptr) {
            return false;
        }
        s_lua_network_result result;
        if (!Read_Lua_Network_Result(buffer, size, &result)) {
            *diagnostic = "A peer sent a malformed Lua setup result.";
            return false;
        }
        if (result.offer_id != current_offer_id_) {
            *diagnostic = "A peer answered an obsolete Lua setup offer.";
            return false;
        }
        if (!result.accepted) {
            *diagnostic = result.diagnostic.empty()
                ? "A peer rejected the Lua multiplayer setup."
                : result.diagnostic;
            return false;
        }
        if (memcmp(result.digest.bytes,
                expected_compatibility_digest_.bytes,
                sizeof(result.digest.bytes)) != 0) {
            *diagnostic = "Lua network compatibility digest differs";
            return false;
        }
        diagnostic->clear();
        return true;
    }

    bool Write_Lua_Network_Setup_Decision(
        void* buffer,
        uint32_t capacity,
        bool accepted,
        const std::string& diagnostic,
        uint32_t* size) {
        if (current_offer_id_ == 0) {
            return false;
        }
        s_lua_network_result result;
        result.offer_id = current_offer_id_;
        result.accepted = accepted;
        result.digest = expected_compatibility_digest_;
        result.diagnostic_code = accepted
            ? LUA_NETWORK_DIAGNOSTIC_NONE
            : LUA_NETWORK_DIAGNOSTIC_RUNTIME_IDENTITY;
        if (!accepted) {
            result.diagnostic = diagnostic.substr(
                0, LUA_NETWORK_MAX_DIAGNOSTIC_BYTES);
        }
        return Write_Lua_Network_Result(
            buffer, capacity, &result, size);
    }

    bool Process_Lua_Network_Setup_Offer(
        const void* payload,
        uint32_t payload_size,
        e_lua_network_setup_kind expected_kind,
        void* result_buffer,
        uint32_t result_capacity,
        uint32_t* result_size) {
        if (result_buffer == nullptr || result_size == nullptr) {
            return false;
        }
        *result_size = 0;
        s_lua_network_result result;
        result.diagnostic_code = LUA_NETWORK_DIAGNOSTIC_INVALID_OFFER;
        s_lua_network_setup_offer offer;
        std::string diagnostic;
        if (!Read_Lua_Network_Setup_Offer(
                payload, payload_size, &offer, &diagnostic)) {
            return false;
        }
        result.offer_id = offer.offer_id;
        current_offer_id_ = offer.offer_id;
        if (offer.kind != expected_kind) {
            result.diagnostic_code = LUA_NETWORK_DIAGNOSTIC_WRONG_SETUP_KIND;
            diagnostic = "Lua setup offer has the wrong setup kind";
        } else if (pending_config_ != nullptr) {
            diagnostic = "another Lua setup offer is already pending";
        } else {
            config::s_cfg_diagnostic config_diagnostic;
            if (!config::Stage_Network_Config_Snapshot(offer.configuration,
                    offer.configuration_size,
                    &pending_config_,
                    &config_diagnostic)) {
                result.diagnostic_code = LUA_NETWORK_DIAGNOSTIC_CONFIGURATION;
                diagnostic = config_diagnostic.message;
            } else if (!Validate_Lua_Network_Setup_Offer(&offer,
                           config::Get_Network_Config_Candidate_Registry(
                               pending_config_),
                           &result.digest,
                           &result.record_index,
                           &diagnostic)) {
                result.diagnostic_code = LUA_NETWORK_DIAGNOSTIC_RUNTIME_IDENTITY;
                Clear_Pending_Setup();
            } else {
                result.accepted = true;
                result.diagnostic_code = LUA_NETWORK_DIAGNOSTIC_NONE;
                expected_compatibility_digest_ = result.digest;
                pending_offer_id_ = offer.offer_id;
                pending_configuration_.assign(offer.configuration,
                    offer.configuration + offer.configuration_size);
                pending_setup_kind_ = offer.kind;
            }
        }
        if (!result.accepted) {
            if (diagnostic.empty()) {
                diagnostic = "invalid Lua setup offer";
            }
            result.diagnostic = diagnostic.substr(
                0, LUA_NETWORK_MAX_DIAGNOSTIC_BYTES);
        }
        if (!Write_Lua_Network_Result(result_buffer,
                result_capacity,
                &result,
                result_size)) {
            Clear_Pending_Setup();
            return false;
        }
        return true;
    }

    void Set_Lua_Network_Offer_Context(
        uint64_t offer_id,
        const s_lua_network_digest* compatibility_digest) {
        current_offer_id_ = offer_id;
        if (compatibility_digest != nullptr) {
            expected_compatibility_digest_ = *compatibility_digest;
        }
    }

    bool Write_Lua_Network_Setup_Commit(
        void* buffer,
        uint32_t capacity,
        uint32_t* size) {
        if (buffer == nullptr || size == nullptr ||
            capacity < LUA_NETWORK_COMMIT_BYTES || current_offer_id_ == 0) {
            return false;
        }
        SERIAL::Writer_ writer(buffer,
            capacity,
            "Lua network setup commit overflow");
        writer.Write_Uint32(LUA_NETWORK_COMMIT_MAGIC);
        writer.Write_Uint64(current_offer_id_);
        *size = writer.Length();
        return true;
    }

    void Reset_Lua_Network_Session() {
        Clear_Pending_Setup();
        queued_load_state_.clear();
        gameplay_host_authority_id_ = -1;
    }

    bool Prepare_Lua_Gameplay_Host_Authority(
        int32_t elected_network_id,
        int32_t local_network_id,
        std::string* diagnostic) {
        if (elected_network_id < 0 || local_network_id < 0 ||
            diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        if (gameplay_host_authority_id_ == elected_network_id) {
            return true;
        }
        if (Network_Lua_Enabled() &&
            elected_network_id == local_network_id &&
            !config::Reconstruct_Active_Lua_Runtimes(diagnostic)) {
            return false;
        }
        gameplay_host_authority_id_ = elected_network_id;
        return true;
    }

    bool Commit_Lua_Network_Setup(
        e_lua_network_setup_kind expected_kind,
        const void* payload,
        uint32_t payload_size,
        uint32_t prefix_size,
        std::string* diagnostic) {
        if (diagnostic == nullptr || pending_config_ == nullptr ||
            pending_setup_kind_ != expected_kind || payload == nullptr ||
            payload_size < prefix_size + LUA_NETWORK_COMMIT_BYTES) {
            Clear_Pending_Setup();
            return false;
        }
        SERIAL::Reader_ reader(
            static_cast<const uint8_t*>(payload) + prefix_size,
            payload_size - prefix_size,
            "invalid Lua network setup commit");
        if (reader.Read_Uint32() != LUA_NETWORK_COMMIT_MAGIC ||
            reader.Read_Uint64() != pending_offer_id_ ||
            reader.Remaining() != pending_configuration_.size() ||
            memcmp(reader.Ptr(),
                pending_configuration_.data(),
                pending_configuration_.size()) != 0) {
            Clear_Pending_Setup();
            *diagnostic = "invalid Lua network setup commit";
            return false;
        }
        const s_lua_registry* candidate_registry =
            config::Get_Network_Config_Candidate_Registry(pending_config_);
        const bool candidate_has_lua = candidate_registry != nullptr &&
            !candidate_registry->runtime_packages.empty();
        if (expected_kind == LUA_NETWORK_SETUP_NEW_GAME ||
            !candidate_has_lua) {
            config::s_cfg_diagnostic config_diagnostic;
            if (!config::Commit_Network_Config_Snapshot(
                    &pending_config_, &config_diagnostic)) {
                Clear_Pending_Setup();
                *diagnostic = config_diagnostic.message;
                return false;
            }
            pending_configuration_.clear();
        }
        pending_offer_id_ = 0;
        diagnostic->clear();
        return true;
    }

    bool Has_Lua_Network_Setup_Commit(
        const void* payload,
        uint32_t payload_size,
        uint32_t prefix_size) {
        if (payload == nullptr ||
            payload_size < prefix_size + LUA_NETWORK_COMMIT_BYTES) {
            return false;
        }
        uint32_t magic = 0;
        memcpy(&magic,
            static_cast<const uint8_t*>(payload) + prefix_size,
            sizeof(magic));
        return magic == LUA_NETWORK_COMMIT_MAGIC;
    }

    bool Has_Pending_Lua_Network_Setup() {
        return pending_config_ != nullptr;
    }

    void Discard_Pending_Lua_Network_Setup() {
        Clear_Pending_Setup();
    }

    bool Finalize_Pending_Lua_Network_Setup(
        const void* lua_snapshot,
        uint32_t lua_snapshot_size,
        std::string* diagnostic) {
        if (pending_config_ == nullptr) {
            return false;
        }
        const bool finalized = config::Finalize_Network_Config_Snapshot(
            &pending_config_,
            lua_snapshot,
            lua_snapshot_size,
            diagnostic);
        if (finalized) {
            pending_configuration_.clear();
        }
        return finalized;
    }

    bool Queue_Lua_Network_Load_State(
        const s_lua_save_snapshot* snapshot,
        std::string* diagnostic) {
        if (snapshot == nullptr || diagnostic == nullptr) {
            return false;
        }
        const uint32_t payload_size = Lua_Save_Snapshot_Size(snapshot);
        if (payload_size == 0 || payload_size > LUA_NETWORK_MAX_BYTES) {
            *diagnostic = "Lua save snapshot exceeds its network size limit";
            return false;
        }
        try {
            queued_load_state_.resize(payload_size);
        } catch (...) {
            queued_load_state_.clear();
            *diagnostic = "unable to allocate queued Lua load state";
            return false;
        }
        SERIAL::Writer_ writer(queued_load_state_.data(),
            payload_size,
            "network Lua save snapshot overflow");
        Write_Lua_Save_Snapshot(&writer, snapshot);
        diagnostic->clear();
        return true;
    }

    void Clear_Queued_Lua_Network_Load_State() {
        queued_load_state_.clear();
    }

    bool Has_Queued_Lua_Network_Load_State() {
        return !queued_load_state_.empty();
    }

    bool Capture_Lua_Network_Payload(
        void* buffer,
        uint32_t capacity,
        uint32_t* size,
        uint32_t* flags,
        std::string* diagnostic) {
        if (buffer == nullptr || size == nullptr || flags == nullptr ||
            diagnostic == nullptr) {
            return false;
        }
        *flags = 0;
        if (!queued_load_state_.empty()) {
            if (queued_load_state_.size() > capacity) {
                *diagnostic = "queued Lua load state exceeds network capacity";
                return false;
            }
            memcpy(buffer, queued_load_state_.data(), queued_load_state_.size());
            *size = static_cast<uint32_t>(queued_load_state_.size());
            *flags = LUA_NETWORK_PAYLOAD_SAVED_STATE;
            queued_load_state_.clear();
            return true;
        }
        return Capture_Lua_Network_State(
            buffer, capacity, size, diagnostic);
    }

    bool Prepare_Lua_Network_Payload(
        const void* buffer,
        uint32_t size,
        uint32_t flags,
        uint32_t native_ship_count,
        s_lua_network_state_transaction** transaction,
        bool* committed,
        std::string* diagnostic) {
        if (transaction == nullptr || committed == nullptr ||
            diagnostic == nullptr ||
            (flags & ~LUA_NETWORK_PAYLOAD_SAVED_STATE) != 0) {
            return false;
        }
        *transaction = nullptr;
        *committed = false;
        if ((flags & LUA_NETWORK_PAYLOAD_SAVED_STATE) != 0) {
            if (!Finalize_Pending_Lua_Network_Setup(
                    buffer, size, diagnostic)) {
                Discard_Pending_Lua_Network_Setup();
                return false;
            }
            *committed = true;
            return true;
        }
        return Prepare_Lua_Network_State(buffer,
            size,
            native_ship_count,
            transaction,
            diagnostic);
    }

    bool Capture_Lua_Network_State(
        void* buffer,
        uint32_t capacity,
        uint32_t* size,
        std::string* diagnostic) {
        if (buffer == nullptr || size == nullptr || diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        const size_t runtime_count = Get_Mod_Runtime_Count(runtimes);
        if (runtime_count > LUA_SAVE_MAX_PACKAGE_COUNT) {
            *diagnostic = "Lua network storage package limit exceeded";
            return false;
        }
        std::vector<s_lua_saved_storage> storage;
        std::vector<s_lua_player_race_trait_assignment> assignments;
        uint64_t generations[MAX_SHIPS] = {};
        size_t generation_count = 0;
        try {
            storage.reserve(runtime_count);
            for (size_t index = 0; index < runtime_count; ++index) {
                s_lua_mod_runtime* runtime = const_cast<s_lua_mod_runtime*>(
                    Get_Mod_Runtime(runtimes, index));
                s_lua_saved_storage saved;
                saved.package_id = Get_Mod_Runtime_Package_Id(runtime);
                saved.storage_version =
                    Get_Mod_Runtime_Storage_Version(runtime);
                if (!Serialize_Mod_Storage(
                        runtime, &saved.bytes, diagnostic)) {
                    return false;
                }
                storage.push_back(std::move(saved));
            }
            const s_lua_game_traits* traits = Get_Active_Game_Traits();
            const size_t assignment_count =
                Get_Player_Race_Trait_Assignment_Count(traits);
            if (assignment_count > LUA_SAVE_MAX_RECORD_COUNT) {
                *diagnostic = "Lua network trait assignment limit exceeded";
                return false;
            }
            assignments.reserve(assignment_count);
            for (size_t index = 0; index < assignment_count; ++index) {
                s_lua_player_race_trait_assignment assignment;
                if (!Get_Player_Race_Trait_Assignment(
                        traits, index, &assignment)) {
                    *diagnostic = "unable to capture Lua network trait assignment";
                    return false;
                }
                assignments.push_back(std::move(assignment));
            }
            std::sort(assignments.begin(), assignments.end(), Assignment_Less);
        } catch (...) {
            *diagnostic = "unable to allocate Lua network state";
            return false;
        }
        if (!game::Capture_Ship_Generations(
                generations, MAX_SHIPS, &generation_count)) {
            *diagnostic = "unable to capture Lua network ship generations";
            return false;
        }
        uint64_t required = LUNS_HEADER_BYTES + sizeof(uint32_t);
        for (const s_lua_saved_storage& saved : storage) {
            required += sizeof(uint32_t) + saved.package_id.size() +
                2 * sizeof(uint32_t) + saved.bytes.size();
        }
        required += sizeof(uint32_t);
        for (const s_lua_player_race_trait_assignment& assignment :
            assignments) {
            required += sizeof(int16_t) + sizeof(uint32_t) +
                assignment.trait_id.size();
        }
        required += sizeof(uint32_t) +
            generation_count * sizeof(uint64_t);
        if (required > capacity || required > LUA_NETWORK_MAX_BYTES) {
            *diagnostic = "Lua network state exceeds its size limit";
            return false;
        }
        SERIAL::Writer_ writer(buffer, capacity, "Lua network state overflow");
        writer.Write_Uint32(LUNS_MAGIC);
        writer.Write_Uint16(LUNS_VERSION);
        writer.Write_Uint16(LUNS_HEADER_BYTES);
        writer.Write_Uint32(static_cast<uint32_t>(storage.size()));
        writer.Write_Uint32(0);
        for (const s_lua_saved_storage& saved : storage) {
            writer.Write_Uint32(static_cast<uint32_t>(saved.package_id.size()));
            writer.Write_Bytes(saved.package_id.data(), saved.package_id.size());
            writer.Write_Uint32(saved.storage_version);
            writer.Write_Uint32(static_cast<uint32_t>(saved.bytes.size()));
            writer.Write_Bytes(saved.bytes.data(), saved.bytes.size());
        }
        writer.Write_Uint32(static_cast<uint32_t>(assignments.size()));
        for (const s_lua_player_race_trait_assignment& assignment :
            assignments) {
            writer.Write_Int16(static_cast<int16_t>(assignment.player_id.id));
            writer.Write_Uint32(static_cast<uint32_t>(
                assignment.trait_id.size()));
            writer.Write_Bytes(
                assignment.trait_id.data(), assignment.trait_id.size());
        }
        writer.Write_Uint32(static_cast<uint32_t>(generation_count));
        for (size_t index = 0; index < generation_count; ++index) {
            writer.Write_Uint64(generations[index]);
        }
        *size = writer.Length();
        return true;
    }

    bool Prepare_Lua_Network_State(
        const void* buffer,
        uint32_t size,
        uint32_t native_ship_count,
        s_lua_network_state_transaction** transaction,
        std::string* diagnostic) {
        if (buffer == nullptr || transaction == nullptr ||
            diagnostic == nullptr || size > LUA_NETWORK_MAX_BYTES) {
            return false;
        }
        *transaction = nullptr;
        diagnostic->clear();
        SERIAL::Reader_ reader(buffer, size, "invalid Lua network state");
        if (reader.Remaining() < LUNS_HEADER_BYTES) {
            *diagnostic = "Lua network state header is truncated";
            return false;
        }
        const uint32_t magic = reader.Read_Uint32();
        const uint16_t version = reader.Read_Uint16();
        const uint16_t header_size = reader.Read_Uint16();
        const uint32_t package_count = reader.Read_Uint32();
        const uint32_t rng_count = reader.Read_Uint32();
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        if (magic != LUNS_MAGIC || version != LUNS_VERSION ||
            header_size != LUNS_HEADER_BYTES || rng_count != 0 ||
            package_count > LUA_SAVE_MAX_PACKAGE_COUNT ||
            package_count != Get_Mod_Runtime_Count(runtimes)) {
            *diagnostic = "invalid Lua network state header";
            return false;
        }
        auto* prepared = new (std::nothrow) s_lua_network_state_transaction;
        if (prepared == nullptr) {
            *diagnostic = "unable to allocate Lua network state transaction";
            return false;
        }
        const auto discard = [&]() {
            for (s_lua_storage_restore_transaction*& storage :
                prepared->storage) {
                Discard_Active_Mod_Storage_Restore(&storage);
            }
            Discard_Player_Race_Trait_Assignment_Replacement(
                &prepared->trait_assignments);
            delete prepared;
        };
        try {
            prepared->storage.reserve(package_count);
            for (uint32_t index = 0; index < package_count; ++index) {
                std::string package_id;
                uint32_t storage_version = 0;
                uint32_t storage_size = 0;
                if (!Read_String(&reader, &package_id) ||
                    !Read_U32(&reader, &storage_version) ||
                    !Read_U32(&reader, &storage_size) ||
                    storage_size > LUA_SAVE_MAX_BYTES ||
                    storage_size > reader.Remaining()) {
                    *diagnostic = "invalid Lua network storage record";
                    discard();
                    return false;
                }
                s_lua_mod_runtime* runtime = const_cast<s_lua_mod_runtime*>(
                    Get_Mod_Runtime(runtimes, index));
                if (runtime == nullptr ||
                    package_id != Get_Mod_Runtime_Package_Id(runtime)) {
                    *diagnostic = "Lua network storage package order differs";
                    discard();
                    return false;
                }
                s_lua_storage_restore_transaction* storage = nullptr;
                if (!Prepare_Active_Mod_Storage_Restore(runtime,
                        storage_version,
                        reader.Ptr(),
                        storage_size,
                        &storage,
                        diagnostic)) {
                    discard();
                    return false;
                }
                prepared->storage.push_back(storage);
                reader.Skip(storage_size);
            }
            uint32_t assignment_count = 0;
            if (!Read_U32(&reader, &assignment_count) ||
                assignment_count > LUA_SAVE_MAX_RECORD_COUNT) {
                *diagnostic = "invalid Lua network trait assignment count";
                discard();
                return false;
            }
            std::vector<s_lua_player_race_trait_assignment> assignments;
            assignments.reserve(assignment_count);
            for (uint32_t index = 0; index < assignment_count; ++index) {
                if (reader.Remaining() < sizeof(int16_t)) {
                    *diagnostic = "invalid Lua network trait assignment";
                    discard();
                    return false;
                }
                s_lua_player_race_trait_assignment assignment;
                assignment.player_id = ::game::id_player{static_cast<uint32_t>(reader.Read_Int16())};
                if (assignment.player_id.id >= MAX_PLAYERS || !Read_String(&reader, &assignment.trait_id) ||
                    (index != 0 &&
                        !Assignment_Less(assignments.back(), assignment))) {
                    *diagnostic =
                        "invalid or duplicate Lua network trait assignment";
                    discard();
                    return false;
                }
                assignments.push_back(std::move(assignment));
            }
            if (!Prepare_Player_Race_Trait_Assignment_Replacement(
                    Get_Active_Game_Traits(),
                    &assignments,
                    &prepared->trait_assignments,
                    diagnostic)) {
                discard();
                return false;
            }
            if (!Read_U32(&reader, &prepared->ship_generation_count) ||
                prepared->ship_generation_count > MAX_SHIPS ||
                prepared->ship_generation_count != native_ship_count ||
                reader.Remaining() !=
                    prepared->ship_generation_count * sizeof(uint64_t)) {
                *diagnostic = "invalid Lua network ship generation count";
                discard();
                return false;
            }
            for (uint32_t index = 0;
                 index < prepared->ship_generation_count;
                 ++index) {
                prepared->ship_generations[index] = reader.Read_Uint64();
            }
        } catch (...) {
            *diagnostic = "unable to allocate Lua network state data";
            discard();
            return false;
        }
        *transaction = prepared;
        return true;
    }

    void Commit_Lua_Network_State(
        s_lua_network_state_transaction** transaction) {
        if (transaction == nullptr || *transaction == nullptr) return;
        s_lua_network_state_transaction* prepared = *transaction;
        for (s_lua_storage_restore_transaction*& storage : prepared->storage) {
            Commit_Active_Mod_Storage_Restore(&storage);
        }
        Commit_Player_Race_Trait_Assignment_Replacement(
            &prepared->trait_assignments);
        game::Restore_Ship_Generations(prepared->ship_generations,
            prepared->ship_generation_count);
        delete prepared;
        *transaction = nullptr;
    }

    void Discard_Lua_Network_State(
        s_lua_network_state_transaction** transaction) {
        if (transaction == nullptr || *transaction == nullptr) return;
        s_lua_network_state_transaction* prepared = *transaction;
        for (s_lua_storage_restore_transaction*& storage : prepared->storage) {
            Discard_Active_Mod_Storage_Restore(&storage);
        }
        Discard_Player_Race_Trait_Assignment_Replacement(
            &prepared->trait_assignments);
        delete prepared;
        *transaction = nullptr;
    }

} // namespace orion2re::lua
