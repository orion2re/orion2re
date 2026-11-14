#include "config_snapshot.h"

#include <algorithm>
#include <cstring>

#include "config/config_bits.h"
#include "config/config_decode.h"
#include "config/config_loader.h"
#include "config/semantic_version.h"
#include "game/serialization.h"

namespace config {
    namespace {
        constexpr uint32_t CONFIG_SNAPSHOT_MAGIC = 0x4746434f;
        constexpr uint16_t CONFIG_SNAPSHOT_VERSION = 1;
        constexpr uint16_t CONFIG_SNAPSHOT_HEADER_SIZE_VALUES_ONLY = 16;
        constexpr uint16_t CONFIG_SNAPSHOT_HEADER_SIZE_WITH_MOD_IDS = 20;
        constexpr uint16_t CONFIG_SNAPSHOT_HEADER_SIZE_WITH_MOD_VERSIONS = 24;
        constexpr uint32_t CONFIG_SNAPSHOT_ENTRY_HEADER_SIZE = 8;
        constexpr uint32_t CONFIG_SNAPSHOT_MOD_HEADER_SIZE = 4;

        struct s_descriptor_metadata {
            std::vector<uint32_t> ordinals;
            bool valid = false;
        };

        struct s_snapshot_entry {
            uint32_t ordinal = 0;
            t_variable_id variable_id = CONFIG_NO_VARIABLE;
            uint32_t leaf_index = 0;
            std::string path;
            uint32_t value_size = 0;
        };

        bool Validate_Descriptor(
            const s_config_snapshot_descriptor* descriptor);

        uint32_t Value_Size(const s_typed_value* value,
            const s_type_descriptor* type) {
            switch (type->value_type) {
                case CONFIG_VALUE_BOOL:
                case CONFIG_VALUE_INT8:
                case CONFIG_VALUE_UINT8:
                    return 1;
                case CONFIG_VALUE_INT16:
                case CONFIG_VALUE_UINT16:
                    return 2;
                case CONFIG_VALUE_INT32:
                case CONFIG_VALUE_UINT32:
                    return 4;
                case CONFIG_VALUE_STRING:
                    if (value->string_value.size() > UINT32_MAX) return 0;
                    return static_cast<uint32_t>(value->string_value.size());
                case CONFIG_VALUE_BITMAP:
                    if (value->bitmap_value.size() > UINT32_MAX) return 0;
                    return static_cast<uint32_t>(value->bitmap_value.size());
                case CONFIG_VALUE_RATIO:
                    return 16;
            }
            return 0;
        }

        bool Add_Size(uint32_t value, uint32_t* size) {
            if (value > CONFIG_MAX_SNAPSHOT_SIZE - *size) return false;
            *size += value;
            return true;
        }

        bool Build_Descriptor_Metadata(
            const s_config_snapshot_descriptor* descriptor,
            bool validate_paths,
            s_descriptor_metadata* metadata) {
            if (descriptor == nullptr || metadata == nullptr ||
                descriptor->variables == nullptr ||
                descriptor->variable_count == 0 ||
                (validate_paths && !Validate_Descriptor(descriptor))) {
                return false;
            }
            metadata->ordinals.assign(variable_count, 0);
            for (uint32_t index = 0;
                 index < descriptor->variable_count;
                 ++index) {
                const t_variable_id variable_id = descriptor->variables[index];
                if (variable_id >= variable_count ||
                    metadata->ordinals[variable_id] != 0 ||
                    (descriptor->group_mask &
                        (UINT32_C(1) << Get_Variable_Group(variable_id))) == 0) {
                    metadata->ordinals.clear();
                    return false;
                }
                metadata->ordinals[variable_id] = index + 1;
            }
            metadata->valid = true;
            return true;
        }

        s_descriptor_metadata Build_Known_Descriptor_Metadata(
            const s_config_snapshot_descriptor* descriptor) {
            s_descriptor_metadata metadata;
            Build_Descriptor_Metadata(descriptor, false, &metadata);
            return metadata;
        }

        const s_descriptor_metadata* Descriptor_Metadata(
            const s_config_snapshot_descriptor* descriptor,
            s_descriptor_metadata* temporary) {
            static const s_descriptor_metadata saved =
                Build_Known_Descriptor_Metadata(&saved_config_snapshot);
            static const s_descriptor_metadata synchronized =
                Build_Known_Descriptor_Metadata(&synchronized_config_snapshot);
            if (descriptor == &saved_config_snapshot) return &saved;
            if (descriptor == &synchronized_config_snapshot) {
                return &synchronized;
            }
            return Build_Descriptor_Metadata(descriptor, true, temporary)
                ? temporary
                : nullptr;
        }

        bool Valid_Mod_Id(const std::string& mod_id) {
            if (mod_id.empty() || mod_id.size() > CONFIG_MAX_SNAPSHOT_MOD_ID_SIZE ||
                mod_id.size() > UINT16_MAX || mod_id.front() < 'a' ||
                mod_id.front() > 'z') {
                return false;
            }
            for (char character : mod_id) {
                if (!((character >= 'a' && character <= 'z') ||
                        (character >= '0' && character <= '9') || character == '_' ||
                        character == '-')) {
                    return false;
                }
            }
            return true;
        }

        bool Valid_Mod_Version(const std::string& version) {
            if (version.empty() ||
                version.size() > CONFIG_MAX_SNAPSHOT_MOD_VERSION_SIZE) {
                return false;
            }
            s_semantic_version parsed;
            return Parse_Semantic_Version(version, &parsed);
        }

        bool Validate_Mods(const std::vector<s_mod_requirement>* required_mods) {
            if (required_mods == nullptr) return true;
            if (required_mods->size() > CONFIG_MAX_SNAPSHOT_MOD_COUNT) return false;
            for (size_t index = 0; index < required_mods->size(); ++index) {
                const s_mod_requirement& required = (*required_mods)[index];
                if (!Valid_Mod_Id(required.id) || required.version.empty() ||
                    required.version.size() > CONFIG_MAX_SNAPSHOT_MOD_VERSION_SIZE ||
                    required.version.size() > UINT16_MAX ||
                    !Valid_Mod_Version(required.version)) {
                    return false;
                }
                for (size_t prior = 0; prior < index; ++prior) {
                    if ((*required_mods)[prior].id == required.id) return false;
                }
            }
            return true;
        }

        bool Snapshot_Path(t_variable_id variable_id, std::string* path) {
            if (variable_id >= variable_count ||
                !Get_Variable_Name(variable_id, path) || path->empty()) {
                return false;
            }
            if (path->size() > CONFIG_MAX_SNAPSHOT_PATH_SIZE ||
                path->size() > UINT16_MAX) {
                return false;
            }
            t_variable_id resolved = CONFIG_NO_VARIABLE;
            return Find_Variable(path->data(), path->size(), &resolved) &&
                resolved == variable_id;
        }

        bool Validate_Descriptor(const s_config_snapshot_descriptor* descriptor) {
            if (descriptor == nullptr || descriptor->variables == nullptr ||
                descriptor->variable_count == 0) {
                return false;
            }
            std::string previous_path;
            for (uint32_t index = 0; index < descriptor->variable_count; ++index) {
                const t_variable_id variable_id = descriptor->variables[index];
                std::string path;
                if (!Snapshot_Path(variable_id, &path)) {
                    return false;
                }
                const t_config_group group = Get_Variable_Group(variable_id);
                if ((descriptor->group_mask & (UINT32_C(1) << group)) == 0 ||
                    (!previous_path.empty() && previous_path >= path)) {
                    return false;
                }
                previous_path = std::move(path);
            }
            return true;
        }

        bool Valid_String(const char* value, uint32_t size) {
            for (uint32_t index = 0; index < size; ++index) {
                if (value[index] == '\0') return false;
            }
            return true;
        }

        void Write_Value(SERIAL::Writer_* writer, const s_typed_value* value, const s_type_descriptor* type) {
            switch (type->value_type) {
                case CONFIG_VALUE_BOOL:
                    writer->Write_Uint8(value->bool_value ? 1 : 0);
                    break;
                case CONFIG_VALUE_INT8:
                    writer->Write_Int8(static_cast<int8_t>(value->integer_value));
                    break;
                case CONFIG_VALUE_UINT8:
                    writer->Write_Uint8(static_cast<uint8_t>(value->integer_value));
                    break;
                case CONFIG_VALUE_INT16:
                    writer->Write_Int16(static_cast<int16_t>(value->integer_value));
                    break;
                case CONFIG_VALUE_UINT16:
                    writer->Write_Uint16(static_cast<uint16_t>(value->integer_value));
                    break;
                case CONFIG_VALUE_INT32:
                    writer->Write_Int32(static_cast<int32_t>(value->integer_value));
                    break;
                case CONFIG_VALUE_UINT32:
                    writer->Write_Uint32(static_cast<uint32_t>(value->integer_value));
                    break;
                case CONFIG_VALUE_STRING:
                    writer->Write_Bytes(value->string_value.data(), value->string_value.size());
                    break;
                case CONFIG_VALUE_BITMAP:
                    writer->Write_Bytes(value->bitmap_value.data(), value->bitmap_value.size());
                    break;
                case CONFIG_VALUE_RATIO:
                    writer->Write_Uint64(static_cast<uint64_t>(value->ratio_numerator));
                    writer->Write_Uint64(static_cast<uint64_t>(value->ratio_denominator));
                    break;
            }
        }

        bool Read_Value(SERIAL::Reader_* reader, const s_type_descriptor* type, uint32_t value_size, s_typed_value* value) {
            if (value_size > reader->Remaining()) return false;
            *value = {};
            value->type = type->value_type;
            switch (type->value_type) {
                case CONFIG_VALUE_BOOL: {
                    if (value_size != 1) return false;
                    const uint8_t boolean = reader->Read_Uint8();
                    if (boolean > 1) return false;
                    value->bool_value = boolean != 0;
                    return true;
                }
                case CONFIG_VALUE_INT8:
                    if (value_size != 1) return false;
                    value->integer_value = reader->Read_Int8();
                    return true;
                case CONFIG_VALUE_UINT8:
                    if (value_size != 1) return false;
                    value->integer_value = reader->Read_Uint8();
                    return true;
                case CONFIG_VALUE_INT16:
                    if (value_size != 2) return false;
                    value->integer_value = reader->Read_Int16();
                    return true;
                case CONFIG_VALUE_UINT16:
                    if (value_size != 2) return false;
                    value->integer_value = reader->Read_Uint16();
                    return true;
                case CONFIG_VALUE_INT32:
                    if (value_size != 4) return false;
                    value->integer_value = reader->Read_Int32();
                    return true;
                case CONFIG_VALUE_UINT32:
                    if (value_size != 4) return false;
                    value->integer_value = reader->Read_Uint32();
                    return true;
                case CONFIG_VALUE_STRING:
                    if (value_size > CONFIG_MAX_SNAPSHOT_SIZE) return false;
                    value->string_value.resize(value_size);
                    reader->Read_Bytes(value->string_value.data(), value_size);
                    return Valid_String(value->string_value.data(), value_size);
                case CONFIG_VALUE_BITMAP: {
                    const uint32_t bitmap_size =
                        static_cast<uint32_t>((type->bitmap_bit_count + 7) / 8);
                    if (value_size != bitmap_size) return false;
                    value->bitmap_value.resize(value_size);
                    reader->Read_Bytes(value->bitmap_value.data(), value_size);
                    return true;
                }
                case CONFIG_VALUE_RATIO:
                    if (value_size != 16) return false;
                    value->ratio_numerator = static_cast<int64_t>(reader->Read_Uint64());
                    value->ratio_denominator = static_cast<int64_t>(reader->Read_Uint64());
                    return true;
            }
            return false;
        }

        bool Add_Entry(const s_config_snapshot_descriptor* descriptor,
            const s_descriptor_metadata* metadata,
            const s_config_state* state,
            t_variable_id variable_id,
            uint32_t leaf_index,
            std::vector<s_snapshot_entry>* entries,
            uint32_t* size) {
            const uint32_t ordinal = metadata->ordinals[variable_id];
            if (ordinal == 0) return true;
            const s_type_descriptor* type = Get_Variable_Leaf_Type(variable_id, leaf_index);
            if (type == nullptr) return false;
            s_snapshot_entry entry;
            entry.ordinal = ordinal;
            entry.variable_id = variable_id;
            entry.leaf_index = leaf_index;
            if (!Get_Variable_Leaf_Name(variable_id, leaf_index, &entry.path) ||
                entry.path.empty() ||
                entry.path.size() > CONFIG_MAX_SNAPSHOT_PATH_SIZE ||
                entry.path.size() > UINT16_MAX) {
                return false;
            }
            if (descriptor != &saved_config_snapshot &&
                descriptor != &synchronized_config_snapshot) {
                s_variable_accessor resolved;
                if (!Find_Variable_Accessor(entry.path.data(), entry.path.size(), &resolved) ||
                    resolved.variable_id != variable_id || resolved.leaf_index != leaf_index) {
                    return false;
                }
            }
            const s_typed_value& variable = state->values[variable_id];
            const s_typed_value& value = variable.type == CONFIG_VALUE_AGGREGATE ? variable.aggregate_values[leaf_index] : variable;
            entry.value_size = Value_Size(&value, type);
            if (entry.value_size == 0 &&
                type->value_type != CONFIG_VALUE_STRING &&
                type->value_type != CONFIG_VALUE_BITMAP) {
                return false;
            }
            if (!Add_Size(CONFIG_SNAPSHOT_ENTRY_HEADER_SIZE, size) ||
                !Add_Size(static_cast<uint32_t>(entry.path.size()), size) ||
                !Add_Size(entry.value_size, size)) {
                return false;
            }
            entries->push_back(std::move(entry));
            return true;
        }

        bool Add_Variable_Entries(const s_config_snapshot_descriptor* descriptor,
            const s_descriptor_metadata* metadata,
            const s_config_state* state,
            const s_config_state* defaults,
            t_variable_id variable_id,
            std::vector<s_snapshot_entry>* entries,
            uint32_t* size) {
            const s_typed_value& value = state->values[variable_id];
            if (value.type != CONFIG_VALUE_AGGREGATE) {
                return Add_Entry(descriptor, metadata, state, variable_id, 0, entries, size);
            }
            const bool vector = runtime_variables[variable_id].vector;
            if (State_Tracks_Non_Default_Values(state) && !vector) {
                const s_non_default_aggregate* aggregate =
                    Get_State_Non_Default_Aggregate(state, variable_id);
                if (aggregate == nullptr) return false;
                const std::vector<uint64_t>& leaves = aggregate->leaves;
                for (size_t word_index = 0;
                     word_index < leaves.size();
                     ++word_index) {
                    uint64_t word = leaves[word_index];
                    while (word != 0) {
                        const unsigned int bit = Count_Trailing_Zero_Bits(word);
                        const uint32_t leaf = static_cast<uint32_t>(
                            word_index * 64 + bit);
                        if (leaf >= value.aggregate_values.size() ||
                            !Add_Entry(descriptor,
                                metadata,
                                state,
                                variable_id,
                                leaf,
                                entries,
                                size)) {
                            return false;
                        }
                        word &= word - 1;
                    }
                }
                return true;
            }
            const s_typed_value* default_value = defaults != nullptr
                ? &defaults->values[variable_id]
                : nullptr;
            for (uint32_t leaf = 0; leaf < value.aggregate_values.size(); ++leaf) {
                if (!vector && default_value != nullptr &&
                    leaf < default_value->aggregate_values.size() &&
                    Config_Values_Equal(value.aggregate_values[leaf],
                        default_value->aggregate_values[leaf])) continue;
                if (!Add_Entry(descriptor, metadata, state, variable_id, leaf, entries, size)) return false;
            }
            return true;
        }

        bool Snapshot_Entry_Less(const s_snapshot_entry& left,
            const s_snapshot_entry& right) {
            return left.ordinal < right.ordinal ||
                (left.ordinal == right.ordinal &&
                    left.leaf_index < right.leaf_index);
        }
    } // namespace

    bool Prepare_Config_Snapshot(
        const s_config_snapshot_descriptor* descriptor,
        const s_config_state* state,
        const std::vector<s_mod_requirement>* required_mods,
        s_prepared_config_snapshot* snapshot) {
        if (snapshot == nullptr) return false;
        snapshot->bytes.clear();
        s_descriptor_metadata temporary_metadata;
        const s_descriptor_metadata* metadata =
            Descriptor_Metadata(descriptor, &temporary_metadata);
        if (metadata == nullptr || !metadata->valid || state == nullptr ||
            state->values.size() != variable_count ||
            ((!State_Tracks_Non_Default_Values(state) || !state->validated) &&
                !Validate_State(state)) ||
            !Validate_Mods(required_mods)) {
            return false;
        }
        uint32_t size = required_mods == nullptr
            ? CONFIG_SNAPSHOT_HEADER_SIZE_VALUES_ONLY
            : CONFIG_SNAPSHOT_HEADER_SIZE_WITH_MOD_VERSIONS;
        if (required_mods != nullptr) {
            for (const s_mod_requirement& required : *required_mods) {
                if (!Add_Size(CONFIG_SNAPSHOT_MOD_HEADER_SIZE, &size) ||
                    !Add_Size(static_cast<uint32_t>(required.id.size()), &size) ||
                    !Add_Size(static_cast<uint32_t>(required.version.size()),
                        &size)) {
                    return false;
                }
            }
        }

        std::vector<s_snapshot_entry> entries;
        s_config_state defaults;
        if (State_Tracks_Non_Default_Values(state)) {
            entries.reserve(state->non_default_variable_ids.size());
            for (t_variable_id variable_id :
                state->non_default_variable_ids) {
                if (!Add_Variable_Entries(descriptor,
                        metadata,
                        state,
                        nullptr,
                        variable_id,
                        &entries,
                        &size)) {
                    return false;
                }
            }
            std::sort(entries.begin(), entries.end(), Snapshot_Entry_Less);
        } else {
            if (!Initialize_Defaults(&defaults)) return false;
            entries.reserve(descriptor->variable_count);
            for (uint32_t index = 0;
                 index < descriptor->variable_count;
                 ++index) {
                const t_variable_id variable_id = descriptor->variables[index];
                if (Config_Values_Equal(state->values[variable_id],
                        defaults.values[variable_id])) {
                    continue;
                }
                if (!Add_Variable_Entries(descriptor,
                        metadata,
                        state,
                        &defaults,
                        variable_id,
                        &entries,
                        &size)) {
                    return false;
                }
            }
        }

        snapshot->bytes.resize(size);
        SERIAL::Writer_ writer(snapshot->bytes.data(),
            size,
            "configuration snapshot overflow");
        writer.Write_Uint32(CONFIG_SNAPSHOT_MAGIC);
        const uint16_t header_size = required_mods == nullptr
            ? CONFIG_SNAPSHOT_HEADER_SIZE_VALUES_ONLY
            : CONFIG_SNAPSHOT_HEADER_SIZE_WITH_MOD_VERSIONS;
        writer.Write_Uint16(CONFIG_SNAPSHOT_VERSION);
        writer.Write_Uint16(header_size);
        writer.Write_Uint32(size - header_size);
        writer.Write_Uint32(static_cast<uint32_t>(entries.size()));
        if (required_mods != nullptr) {
            writer.Write_Uint32(static_cast<uint32_t>(required_mods->size()));
            writer.Write_Uint32(0);
            for (const s_mod_requirement& required : *required_mods) {
                writer.Write_Uint16(static_cast<uint16_t>(required.id.size()));
                writer.Write_Uint16(
                    static_cast<uint16_t>(required.version.size()));
                writer.Write_Bytes(required.id.data(), required.id.size());
                writer.Write_Bytes(required.version.data(),
                    required.version.size());
            }
        }
        for (const s_snapshot_entry& entry : entries) {
            const s_type_descriptor* type =
                Get_Variable_Leaf_Type(entry.variable_id, entry.leaf_index);
            const s_typed_value& variable = state->values[entry.variable_id];
            const s_typed_value& value = variable.type == CONFIG_VALUE_AGGREGATE ? variable.aggregate_values[entry.leaf_index] : variable;
            writer.Write_Uint16(static_cast<uint16_t>(entry.path.size()));
            writer.Write_Uint8(static_cast<uint8_t>(type->value_type));
            writer.Write_Uint8(0);
            writer.Write_Uint32(entry.value_size);
            writer.Write_Bytes(entry.path.data(), entry.path.size());
            Write_Value(&writer, &value, type);
        }
        return writer.Length() == size;
    }

    void Write_Prepared_Config_Snapshot(SERIAL::Writer_* writer,
        const s_prepared_config_snapshot* snapshot) {
        if (writer == nullptr || snapshot == nullptr ||
            snapshot->bytes.empty()) {
            exitm::Exit_With_Message("invalid prepared configuration snapshot");
        }
        writer->Write_Bytes(snapshot->bytes.data(), snapshot->bytes.size());
    }

    uint32_t Config_Snapshot_Size(const s_config_snapshot_descriptor* descriptor,
        const s_config_state* state,
        const std::vector<s_mod_requirement>* required_mods) {
        s_prepared_config_snapshot snapshot;
        return Prepare_Config_Snapshot(
                   descriptor, state, required_mods, &snapshot)
            ? static_cast<uint32_t>(snapshot.bytes.size())
            : 0;
    }

    void Write_Config_Snapshot(SERIAL::Writer_* writer,
        const s_config_snapshot_descriptor* descriptor,
        const s_config_state* state,
        const std::vector<s_mod_requirement>* required_mods) {
        s_prepared_config_snapshot snapshot;
        if (!Prepare_Config_Snapshot(
                descriptor, state, required_mods, &snapshot)) {
            exitm::Exit_With_Message("invalid configuration snapshot");
        }
        Write_Prepared_Config_Snapshot(writer, &snapshot);
    }

    bool Read_Config_Snapshot(SERIAL::Reader_* reader,
        const s_config_snapshot_descriptor* descriptor,
        s_config_snapshot* snapshot) {
        s_descriptor_metadata temporary_metadata;
        const s_descriptor_metadata* metadata =
            Descriptor_Metadata(descriptor, &temporary_metadata);
        if (reader == nullptr || snapshot == nullptr || metadata == nullptr ||
            !metadata->valid) {
            return false;
        }
        if (reader->Failed() || reader->Remaining() < 8) return false;
        if (reader->Read_Uint32() != CONFIG_SNAPSHOT_MAGIC) {
            return false;
        }
        const uint16_t version = reader->Read_Uint16();
        const uint16_t header_size = reader->Read_Uint16();
        const bool has_required_mods =
            header_size == CONFIG_SNAPSHOT_HEADER_SIZE_WITH_MOD_IDS ||
            header_size == CONFIG_SNAPSHOT_HEADER_SIZE_WITH_MOD_VERSIONS;
        const bool has_mod_versions =
            header_size == CONFIG_SNAPSHOT_HEADER_SIZE_WITH_MOD_VERSIONS;
        if (version != CONFIG_SNAPSHOT_VERSION ||
            (!has_required_mods &&
                header_size != CONFIG_SNAPSHOT_HEADER_SIZE_VALUES_ONLY)) {
            return false;
        }
        if (reader->Remaining() < header_size - 8u) return false;
        const uint32_t payload_size = reader->Read_Uint32();
        const uint32_t entry_count = reader->Read_Uint32();
        const uint32_t mod_count = has_required_mods ? reader->Read_Uint32() : 0;
        if (has_mod_versions && reader->Read_Uint32() != 0) return false;
        s_config_state candidate;
        if (payload_size > CONFIG_MAX_SNAPSHOT_SIZE - header_size ||
            payload_size > reader->Remaining() ||
            entry_count > CONFIG_MAX_SNAPSHOT_SIZE / CONFIG_SNAPSHOT_ENTRY_HEADER_SIZE ||
            mod_count > CONFIG_MAX_SNAPSHOT_MOD_COUNT ||
            !Initialize_Tracked_Defaults(&candidate)) {
            return false;
        }
        SERIAL::Reader_ payload_reader(reader->Ptr(), payload_size, "invalid config snapshot payload");
        SERIAL::Reader_* outer_reader = reader;
        reader = &payload_reader;
        std::vector<s_mod_requirement> required_mods;
        required_mods.reserve(mod_count);
        for (uint32_t index = 0; index < mod_count; ++index) {
            if (reader->Remaining() < (has_mod_versions ? 4u : 2u)) return false;
            const uint16_t mod_id_size = reader->Read_Uint16();
            const uint16_t mod_version_size =
                has_mod_versions ? reader->Read_Uint16() : 0;
            if (mod_id_size == 0 || mod_id_size > CONFIG_MAX_SNAPSHOT_MOD_ID_SIZE) {
                return false;
            }
            if (has_mod_versions &&
                (mod_version_size == 0 ||
                    mod_version_size > CONFIG_MAX_SNAPSHOT_MOD_VERSION_SIZE)) {
                return false;
            }
            if (static_cast<uint32_t>(mod_id_size) + mod_version_size > reader->Remaining()) return false;
            s_mod_requirement required;
            required.id.resize(mod_id_size);
            reader->Read_Bytes(required.id.data(), required.id.size());
            required.version.resize(mod_version_size);
            reader->Read_Bytes(required.version.data(), required.version.size());
            if (!Valid_Mod_Id(required.id) ||
                (has_mod_versions && !Valid_Mod_Version(required.version))) {
                return false;
            }
            for (const s_mod_requirement& prior : required_mods) {
                if (prior.id == required.id) return false;
            }
            required_mods.push_back(std::move(required));
        }
        uint32_t prior_ordinal = 0;
        t_variable_id prior_variable = CONFIG_NO_VARIABLE;
        uint32_t prior_leaf = 0;
        for (uint32_t index = 0; index < entry_count; ++index) {
            if (reader->Remaining() < CONFIG_SNAPSHOT_ENTRY_HEADER_SIZE) return false;
            const uint16_t path_size = reader->Read_Uint16();
            const uint8_t value_type = reader->Read_Uint8();
            const uint8_t flags = reader->Read_Uint8();
            const uint32_t value_size = reader->Read_Uint32();
            if (path_size == 0 || path_size > CONFIG_MAX_SNAPSHOT_PATH_SIZE ||
                flags != 0 || path_size > reader->Remaining() ||
                value_size > reader->Remaining() - path_size) {
                return false;
            }
            char path[CONFIG_MAX_SNAPSHOT_PATH_SIZE + 1];
            reader->Read_Bytes(path, path_size);
            path[path_size] = '\0';
            s_variable_accessor accessor;
            std::string canonical_path;
            if (!Find_Variable_Accessor(path, path_size, &accessor) ||
                accessor.variable_id >= metadata->ordinals.size() ||
                metadata->ordinals[accessor.variable_id] == 0 ||
                !Get_Variable_Leaf_Name(accessor.variable_id, accessor.leaf_index, &canonical_path) ||
                canonical_path.size() != path_size ||
                memcmp(canonical_path.data(), path, path_size) != 0 ||
                (descriptor->group_mask &
                    (UINT32_C(1) << Get_Variable_Group(accessor.variable_id))) == 0) {
                return false;
            }
            const uint32_t ordinal = metadata->ordinals[accessor.variable_id];
            if (ordinal < prior_ordinal ||
                (ordinal == prior_ordinal &&
                    (accessor.variable_id != prior_variable ||
                        accessor.leaf_index <= prior_leaf))) {
                return false;
            }
            prior_ordinal = ordinal;
            prior_variable = accessor.variable_id;
            prior_leaf = accessor.leaf_index;
            const s_type_descriptor* type = Get_Variable_Leaf_Type(
                accessor.variable_id, accessor.leaf_index);
            s_typed_value value;
            if (type == nullptr || value_type != type->value_type ||
                !Read_Value(reader, type, value_size, &value) ||
                !Set_State_Leaf_Value(&candidate, accessor.variable_id, accessor.leaf_index, value)) {
                return false;
            }
        }
        if (reader->Remaining() != 0) {
            return false;
        }
        if (!Validate_State(&candidate)) return false;
        snapshot->state = std::move(candidate);
        snapshot->required_mods = std::move(required_mods);
        snapshot->has_required_mods = has_required_mods;
        outer_reader->Skip(payload_size);
        return true;
    }

    bool Read_Config_Snapshot(SERIAL::Reader_* reader,
        const s_config_snapshot_descriptor* descriptor,
        s_config_state* state) {
        if (state == nullptr) return false;
        s_config_snapshot snapshot;
        if (!Read_Config_Snapshot(reader, descriptor, &snapshot)) return false;
        *state = std::move(snapshot.state);
        return true;
    }

    bool Apply_Config_Snapshot(SERIAL::Reader_* reader,
        const s_config_snapshot_descriptor* descriptor) {
        s_config_snapshot snapshot;
        if (!Read_Config_Snapshot(reader, descriptor, &snapshot)) return false;
        s_cfg_diagnostic diagnostic;
        if (snapshot.has_required_mods) {
            return Reapply_Config_Snapshot(
                snapshot.required_mods, descriptor, &snapshot.state, &diagnostic);
        }
        return Commit_Config_Snapshot(descriptor, &snapshot.state, &diagnostic);
    }

} // namespace config