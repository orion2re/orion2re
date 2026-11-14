#include "generate_cpp.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>

#include "file_util.h"

namespace config::tool {
    namespace {
        const char* const GENERATED_FILES[] = {
            "config.cpp",
            "config_enums.cpp",
            "config_enums.h",
            "config_groups.h",
            "settings_metadata.h",
        };

        std::string Join_Path_(const char* directory, const char* name) {
            return (std::filesystem::path(directory) / name).string();
        }

        std::string Cpp_String_(const std::string& value) {
            std::string output = "\"";
            for (uint8_t byte : value) {
                switch (byte) {
                    case '\\': output += "\\\\"; break;
                    case '"': output += "\\\""; break;
                    case '\n': output += "\\n"; break;
                    case '\r': output += "\\r"; break;
                    case '\t': output += "\\t"; break;
                    default:
                        if (byte < 0x20 || byte >= 0x7F) {
                            char escaped[8];
                            std::snprintf(escaped, sizeof(escaped), "\\%03o", byte);
                            output += escaped;
                        } else {
                            output += static_cast<char>(byte);
                        }
                        break;
                }
            }
            output += '"';
            return output;
        }

        std::string Enum_Header_() {
            return "// Generated from specs/config/main.json5. Do not edit.\n"
                   "#pragma once\n\n"
                   "#include <cstddef>\n"
                   "#include <cstdint>\n\n"
                   "namespace config {\n\n"
                   "struct s_generated_enum_symbol {\n"
                   "    const char* name;\n"
                   "    std::int64_t value;\n"
                   "};\n\n"
                   "struct s_generated_enum {\n"
                   "    const char* name;\n"
                   "    const s_generated_enum_symbol* values;\n"
                   "    std::size_t value_count;\n"
                   "};\n\n"
                   "extern const s_generated_enum generated_enums[];\n"
                   "extern const std::size_t generated_enum_count;\n\n"
                   "const s_generated_enum* Find_Generated_Enum(const char* name);\n"
                   "const s_generated_enum_symbol* Find_Generated_Enum_Symbol(\n"
                   "    const s_generated_enum* type, const char* name);\n"
                   "const s_generated_enum_symbol* Find_Generated_Enum_Value(\n"
                   "    const s_generated_enum* type, std::int64_t value);\n\n"
                   "} // namespace config\n";
        }

        std::string Enum_Source_(const s_schema* schema) {
            std::string output =
                "// Generated from specs/config/main.json5. Do not edit.\n"
                "#include \"config/generated/config_enums.h\"\n\n"
                "#include <cstring>\n\n"
                "namespace config {\n\n";
            std::vector<const s_type_declaration*> enums;
            for (const s_type_declaration& declaration :
                schema->type_declarations) {
                if (declaration.type.scalar_kind == SCALAR_ENUM &&
                    !declaration.type.values.empty()) {
                    enums.push_back(&declaration);
                }
            }
            for (size_t index = 0; index < enums.size(); ++index) {
                output += "static const s_generated_enum_symbol generated_enum_values_" +
                    std::to_string(index) + "[] = {\n";
                for (const s_symbol& symbol : enums[index]->type.values) {
                    output += "    { " + Cpp_String_(symbol.name) + ", " +
                        std::to_string(symbol.value) + " },\n";
                }
                output += "};\n\n";
            }
            output += "const s_generated_enum generated_enums[] = {\n";
            if (enums.empty()) {
                output += "    { nullptr, nullptr, 0 },\n";
            } else {
                for (size_t index = 0; index < enums.size(); ++index) {
                    output += "    { " + Cpp_String_(enums[index]->id) +
                        ", generated_enum_values_" + std::to_string(index) +
                        ", sizeof(generated_enum_values_" +
                        std::to_string(index) +
                        ") / sizeof(generated_enum_values_" +
                        std::to_string(index) + "[0]) },\n";
                }
            }
            output +=
                "};\n\n"
                "const std::size_t generated_enum_count = " +
                std::to_string(enums.size()) + ";\n\n"
                                               "const s_generated_enum* Find_Generated_Enum(const char* name) {\n"
                                               "    if (name == nullptr) return nullptr;\n"
                                               "    for (std::size_t index = 0; index < generated_enum_count; ++index) {\n"
                                               "        if (generated_enums[index].name != nullptr &&\n"
                                               "            std::strcmp(generated_enums[index].name, name) == 0) {\n"
                                               "            return &generated_enums[index];\n"
                                               "        }\n"
                                               "    }\n"
                                               "    return nullptr;\n"
                                               "}\n\n"
                                               "const s_generated_enum_symbol* Find_Generated_Enum_Symbol(\n"
                                               "    const s_generated_enum* type, const char* name) {\n"
                                               "    if (type == nullptr || type->values == nullptr || name == nullptr) return nullptr;\n"
                                               "    for (std::size_t index = 0; index < type->value_count; ++index) {\n"
                                               "        if (type->values[index].name != nullptr &&\n"
                                               "            std::strcmp(type->values[index].name, name) == 0) {\n"
                                               "            return &type->values[index];\n"
                                               "        }\n"
                                               "    }\n"
                                               "    return nullptr;\n"
                                               "}\n\n"
                                               "const s_generated_enum_symbol* Find_Generated_Enum_Value(\n"
                                               "    const s_generated_enum* type, std::int64_t value) {\n"
                                               "    if (type == nullptr || type->values == nullptr) return nullptr;\n"
                                               "    for (std::size_t index = 0; index < type->value_count; ++index) {\n"
                                               "        if (type->values[index].value == value) return &type->values[index];\n"
                                               "    }\n"
                                               "    return nullptr;\n"
                                               "}\n\n"
                                               "} // namespace config\n";
            return output;
        }

        bool Group_Selected_(const s_schema* schema, const s_parameter& parameter, bool synchronize) {
            for (const s_group& group : schema->groups) {
                if (group.id == parameter.group) {
                    return synchronize ? group.synchronize : group.save;
                }
            }
            return false;
        }

        std::vector<size_t> Snapshot_Parameters_(const s_schema* schema,
            bool synchronize) {
            std::vector<size_t> parameters;
            for (size_t index = 0; index < schema->parameters.size(); ++index) {
                if (Group_Selected_(schema, schema->parameters[index], synchronize)) {
                    parameters.push_back(index);
                }
            }
            std::sort(parameters.begin(), parameters.end(), [schema](size_t left, size_t right) {
                return schema->parameters[left].path < schema->parameters[right].path;
            });
            return parameters;
        }

        uint32_t Snapshot_Group_Mask_(const s_schema* schema, bool synchronize) {
            uint32_t mask = 0;
            for (size_t index = 0; index < schema->groups.size(); ++index) {
                const s_group& group = schema->groups[index];
                if (synchronize ? group.synchronize : group.save) {
                    mask |= UINT32_C(1) << index;
                }
            }
            return mask;
        }

        std::string Group_Constant_(const std::string& id) {
            std::string output = "CONFIG_GROUP_";
            for (char character : id) {
                if (character >= 'a' && character <= 'z') {
                    output += static_cast<char>(character - 'a' + 'A');
                } else if ((character >= 'A' && character <= 'Z') ||
                    (character >= '0' && character <= '9')) {
                    output += character;
                } else {
                    output += '_';
                }
            }
            return output;
        }

        std::string Group_Header_(const s_schema* schema) {
            std::string output =
                "// Generated from specs/config/main.json5. Do not edit.\n"
                "#pragma once\n\n"
                "namespace config {\n\n"
                "enum t_config_group {\n";
            for (const s_group& group : schema->groups) {
                output += "    " + Group_Constant_(group.id) + ",\n";
            }
            output +=
                "    CONFIG_GROUP_COUNT,\n"
                "};\n\n"
                "}\n";
            return output;
        }

        // One row per parameter in the interface and
        // synchronized_interface groups, in spec declaration order: the
        // settings UI reads its titles and descriptions from here instead
        // of hardcoding them. restart_required is true when the spec
        // remarks say the change takes effect the next time the game
        // starts; that phrase is the spec's own restart signal, so the
        // generator derives the flag from it rather than from a separate
        // list that could drift.
        std::string Settings_Metadata_Header_(const s_schema* schema) {
            std::string output =
                "// Generated from specs/config/main.json5. Do not edit.\n"
                "#pragma once\n\n"
                "#include <cstddef>\n\n"
                "namespace config {\n\n"
                "struct s_settings_metadata {\n"
                "    // Dotted path, for example \"interface.window_width\".\n"
                "    const char* path;\n"
                "    const char* title;\n"
                "    const char* description;\n"
                "    // \"interface\" or \"synchronized_interface\".\n"
                "    const char* group;\n"
                "    // True when the spec remarks say the change takes\n"
                "    // effect the next time the game starts.\n"
                "    bool restart_required;\n"
                "};\n\n"
                "inline const s_settings_metadata settings_metadata[] = {\n";
            size_t count = 0;
            for (const s_parameter_declaration& parameter :
                schema->parameter_declarations) {
                if (parameter.group != "interface" &&
                    parameter.group != "synchronized_interface") {
                    continue;
                }
                const bool restart_required =
                    parameter.metadata.remarks.find(
                        "Changes take effect the next time the game starts") !=
                    std::string::npos;
                output += "    { " +
                    Cpp_String_(parameter.name_space + "." + parameter.id) + ", " +
                    Cpp_String_(parameter.metadata.title) + ", " +
                    Cpp_String_(parameter.metadata.description) + ", " +
                    Cpp_String_(parameter.group) + ", " +
                    (restart_required ? "true" : "false") + " },\n";
                ++count;
            }
            output +=
                "};\n\n"
                "inline const std::size_t settings_metadata_count = " +
                std::to_string(count) + ";\n\n"
                "} // namespace config\n";
            return output;
        }

        const char* Cpp_Type_(e_primitive_type type) {
            switch (type) {
                case PRIMITIVE_BOOL: return "CONFIG_VALUE_BOOL";
                case PRIMITIVE_INT8: return "CONFIG_VALUE_INT8";
                case PRIMITIVE_UINT8: return "CONFIG_VALUE_UINT8";
                case PRIMITIVE_INT16: return "CONFIG_VALUE_INT16";
                case PRIMITIVE_UINT16: return "CONFIG_VALUE_UINT16";
                case PRIMITIVE_INT32: return "CONFIG_VALUE_INT32";
                case PRIMITIVE_UINT32: return "CONFIG_VALUE_UINT32";
                case PRIMITIVE_STRING: return "CONFIG_VALUE_STRING";
                case PRIMITIVE_BITMAP: return "CONFIG_VALUE_BITMAP";
                case PRIMITIVE_RATIO: return "CONFIG_VALUE_RATIO";
            }
            return "CONFIG_VALUE_BOOL";
        }

        const char* Scalar_Kind_(e_scalar_kind kind) {
            switch (kind) {
                case SCALAR_PRIMITIVE: return "CONFIG_SCALAR_PRIMITIVE";
                case SCALAR_ENUM: return "CONFIG_SCALAR_ENUM";
                case SCALAR_MASK: return "CONFIG_SCALAR_MASK";
                case SCALAR_BITMAP: return "CONFIG_SCALAR_BITMAP";
                case SCALAR_RATIO: return "CONFIG_SCALAR_RATIO";
            }
            return "CONFIG_SCALAR_PRIMITIVE";
        }

        const char* Node_Kind_(e_node_kind kind) {
            switch (kind) {
                case NODE_SCALAR: return "CONFIG_NODE_SCALAR";
                case NODE_RECORD: return "CONFIG_NODE_RECORD";
                case NODE_ARRAY: return "CONFIG_NODE_ARRAY";
                case NODE_MAP: return "CONFIG_NODE_MAP";
                case NODE_VECTOR: return "CONFIG_NODE_VECTOR";
                case NODE_NAMESPACE: return "CONFIG_NODE_NAMESPACE";
            }
            return "CONFIG_NODE_SCALAR";
        }

        std::string Byte_Array_(const std::vector<uint8_t>& value);

        std::string Byte_Array_(const std::vector<uint8_t>& value) {
            std::string output = "{ ";
            for (size_t index = 0; index < value.size(); ++index) {
                output += index == 0 ? "" : ", ";
                output += std::to_string(value[index]);
            }
            return output + " }";
        }

        bool Same_Symbols_(const std::vector<s_symbol>& left,
            const std::vector<s_symbol>& right) {
            if (left.size() != right.size()) {
                return false;
            }
            for (size_t index = 0; index < left.size(); ++index) {
                if (left[index].name != right[index].name ||
                    left[index].value != right[index].value) {
                    return false;
                }
            }
            return true;
        }

        const s_parameter& Leaf_Parameter_(const s_schema* schema, const s_node& node) {
            const s_parameter& variable = schema->parameters[node.parameter];
            if (!variable.aggregate) return variable;
            return variable.aggregate_leaves[node.leaf_index % variable.element_leaf_count];
        }

        bool Same_Bitmap_Symbols_(const std::vector<s_bitmap_symbol>& left,
            const std::vector<s_bitmap_symbol>& right) {
            if (left.size() != right.size()) {
                return false;
            }
            for (size_t index = 0; index < left.size(); ++index) {
                if (left[index].name != right[index].name ||
                    left[index].value != right[index].value) {
                    return false;
                }
            }
            return true;
        }

        bool Same_Runtime_Type_(const s_parameter& left, const s_parameter& right) {
            return left.type == right.type &&
                left.constraints.has_min == right.constraints.has_min &&
                left.constraints.has_max == right.constraints.has_max &&
                left.constraints.min == right.constraints.min &&
                left.constraints.max == right.constraints.max &&
                left.scalar_kind == right.scalar_kind &&
                Same_Symbols_(left.values, right.values) &&
                Same_Symbols_(left.flags, right.flags) &&
                Same_Bitmap_Symbols_(left.bitmap_values, right.bitmap_values) &&
                Same_Bitmap_Symbols_(left.bitmap_flags, right.bitmap_flags) &&
                left.bitmap_known_bits == right.bitmap_known_bits &&
                left.bitmap_bit_count == right.bitmap_bit_count &&
                left.allow_numeric == right.allow_numeric &&
                left.reject_unknown_numeric == right.reject_unknown_numeric &&
                left.known_bits == right.known_bits &&
                left.constraints.has_min_length == right.constraints.has_min_length &&
                left.constraints.has_max_length == right.constraints.has_max_length &&
                left.constraints.min_length == right.constraints.min_length &&
                left.constraints.max_length == right.constraints.max_length &&
                left.ratio_component_type == right.ratio_component_type &&
                left.string_storage == right.string_storage &&
                left.nullable == right.nullable;
        }

        bool Same_Target_Expression_(const s_native_target_expression& left,
            const s_native_target_expression& right) {
            if (left.root != right.root || left.components.size() != right.components.size()) {
                return false;
            }
            for (size_t index = 0; index < left.components.size(); ++index) {
                if (left.components[index].array_index != right.components[index].array_index ||
                    left.components[index].member != right.components[index].member) {
                    return false;
                }
            }
            return true;
        }

        bool Same_Target_Expressions_(const s_parameter& left, const s_parameter& right) {
            if (left.native_target_expressions.size() != right.native_target_expressions.size()) {
                return false;
            }
            for (size_t index = 0; index < left.native_target_expressions.size(); ++index) {
                if (!Same_Target_Expression_(left.native_target_expressions[index],
                        right.native_target_expressions[index])) {
                    return false;
                }
            }
            return true;
        }

        template <typename T, typename Equal>
        size_t Intern_Table_(const T& table, const std::vector<T>& tables, Equal equal) {
            for (size_t index = 0; index < tables.size(); ++index) {
                if (equal(table, tables[index])) {
                    return index;
                }
            }
            return tables.size();
        }

        bool Same_Layout_(const s_schema* schema,
            const std::vector<std::vector<uint32_t>>& children,
            uint32_t left_index,
            uint32_t right_index) {
            const s_node& left = schema->nodes[left_index];
            const s_node& right = schema->nodes[right_index];
            if (left.id != right.id || left.kind != right.kind ||
                left.last_parameter - left.first_parameter !=
                    right.last_parameter - right.first_parameter ||
                left.array_count != right.array_count ||
                left.array_index_base != right.array_index_base ||
                left.has_terminator != right.has_terminator ||
                left.terminator != right.terminator ||
                left.implicit_terminator != right.implicit_terminator ||
                left.unique_values != right.unique_values ||
                !Same_Symbols_(left.index_aliases, right.index_aliases) ||
                children[left_index].size() != children[right_index].size()) {
                return false;
            }
            if (left.kind == NODE_SCALAR) {
                const s_parameter& left_parameter = Leaf_Parameter_(schema, left);
                const s_parameter& right_parameter = Leaf_Parameter_(schema, right);
                return Same_Runtime_Type_(left_parameter, right_parameter) &&
                    Same_Target_Expressions_(left_parameter, right_parameter);
            }
            for (size_t child = 0; child < children[left_index].size(); ++child) {
                const uint32_t left_child = children[left_index][child];
                const uint32_t right_child = children[right_index][child];
                if (schema->nodes[left_child].external_index !=
                        schema->nodes[right_child].external_index ||
                    !Same_Layout_(schema, children, left_child, right_child)) {
                    return false;
                }
            }
            return true;
        }

        void Map_Subtree_(const std::vector<std::vector<uint32_t>>& children,
            uint32_t representative,
            uint32_t duplicate,
            std::vector<bool>* omitted_nodes,
            std::vector<uint32_t>* node_representatives) {
            (*omitted_nodes)[duplicate] = true;
            (*node_representatives)[duplicate] = representative;
            for (size_t child = 0; child < children[representative].size(); ++child) {
                Map_Subtree_(children, children[representative][child], children[duplicate][child], omitted_nodes, node_representatives);
            }
        }

        std::string Source_(const s_schema* schema) {
            std::string output =
                "// Generated from specs/config/main.json5. Do not edit.\n"
                "#include \"config/config_runtime.h\"\n"
                "#include <cstring>\n"
                "#include <type_traits>\n";
            output += "#include <utility>\n";
            std::set<std::string> headers;
            for (const s_parameter& parameter : schema->parameters) {
                if (!parameter.binding.header.empty()) {
                    headers.insert(parameter.binding.header);
                }
            }
            for (const std::string& header : headers) {
                output += "#include " + Cpp_String_(header) + "\n";
            }
            output += "\n";
            for (const s_parameter& parameter : schema->parameters) {
                if (parameter.type != PRIMITIVE_STRING ||
                    parameter.string_storage != STRING_STORAGE_ARENA) {
                    continue;
                }
                for (const std::string& target : parameter.native_targets) {
                    output += "static_assert(std::is_same_v<std::remove_reference_t<decltype((" +
                        target + "))>, char*>);\n";
                }
            }
            output += "\n";
            output +=
                "namespace config {\n\n";
            output += "const s_config_group_descriptor config_groups[] = {\n";
            for (const s_group& group : schema->groups) {
                output += "    { " + Cpp_String_(group.id) + ", " +
                    (group.save ? "true" : "false") + ", " +
                    (group.synchronize ? "true" : "false") + " },\n";
            }
            output += "};\n\n";
            output += "struct s_generated_group_range {\n"
                      "    t_variable_id last_variable;\n"
                      "    t_config_group group;\n"
                      "};\n\n"
                      "static const s_generated_group_range generated_variable_groups[] = {\n";
            for (size_t first = 0; first < schema->parameters.size();) {
                size_t last = first;
                while (last + 1 < schema->parameters.size() &&
                    schema->parameters[last + 1].group == schema->parameters[first].group) {
                    ++last;
                }
                output += "    { " + std::to_string(last) + ", " +
                    Group_Constant_(schema->parameters[first].group) + " },\n";
                first = last + 1;
            }
            output += "};\n\n";
            output +=
                "static_assert(sizeof(config_groups) / sizeof(config_groups[0]) == "
                "CONFIG_GROUP_COUNT);\n\n"
                "t_config_group Get_Variable_Group(t_variable_id variable_id) {\n"
                "    if (variable_id >= variable_count) return CONFIG_GROUP_COUNT;\n"
                "    for (const s_generated_group_range& range : generated_variable_groups) {\n"
                "        if (variable_id <= range.last_variable) return range.group;\n"
                "    }\n"
                "    return CONFIG_GROUP_COUNT;\n"
                "}\n\n"
                "const s_config_group_descriptor* Get_Config_Group(t_config_group group) {\n"
                "    if (group < 0 || group >= CONFIG_GROUP_COUNT) return nullptr;\n"
                "    return &config_groups[group];\n"
                "}\n\n";
            const std::vector<size_t> synchronized_parameters =
                Snapshot_Parameters_(schema, true);
            const std::vector<size_t> saved_parameters =
                Snapshot_Parameters_(schema, false);
            const auto append_snapshot = [&](const char* name,
                                             const std::vector<size_t>& parameters,
                                             bool synchronize) {
                output += "static const uint32_t generated_" +
                    std::string(name) + "_variables[] = {\n";
                for (size_t parameter : parameters) {
                    output += "    " + std::to_string(parameter) + ",\n";
                }
                output += "};\n\n";
                output += "const s_config_snapshot_descriptor " + std::string(name) +
                    "_config_snapshot = { generated_" + name + "_variables, " +
                    std::to_string(parameters.size()) + ", " +
                    std::to_string(Snapshot_Group_Mask_(schema, synchronize)) +
                    " };\n\n";
            };
            append_snapshot("synchronized", synchronized_parameters, true);
            append_snapshot("saved", saved_parameters, false);
            std::vector<const s_parameter*> leaf_parameters;
            for (const s_parameter& parameter : schema->parameters) {
                if (parameter.aggregate) {
                    for (const s_parameter& leaf : parameter.aggregate_leaves) {
                        leaf_parameters.push_back(&leaf);
                    }
                } else {
                    leaf_parameters.push_back(&parameter);
                }
            }
            std::vector<std::vector<s_symbol>> value_tables;
            std::vector<std::vector<s_symbol>> flag_tables;
            std::vector<std::vector<s_bitmap_symbol>> bitmap_value_tables;
            std::vector<std::vector<s_bitmap_symbol>> bitmap_flag_tables;
            std::vector<std::vector<uint8_t>> bitmap_known_bits_tables;
            std::vector<size_t> value_table_ids(leaf_parameters.size(), SIZE_MAX);
            std::vector<size_t> flag_table_ids(leaf_parameters.size(), SIZE_MAX);
            std::vector<size_t> bitmap_value_table_ids(leaf_parameters.size(), SIZE_MAX);
            std::vector<size_t> bitmap_flag_table_ids(leaf_parameters.size(), SIZE_MAX);
            std::vector<size_t> bitmap_known_bits_table_ids(leaf_parameters.size(), SIZE_MAX);
            for (size_t index = 0; index < leaf_parameters.size(); ++index) {
                const s_parameter& parameter = *leaf_parameters[index];
                if (!parameter.values.empty()) {
                    value_table_ids[index] = Intern_Table_(
                        parameter.values, value_tables, Same_Symbols_);
                    if (value_table_ids[index] == value_tables.size()) {
                        value_tables.push_back(parameter.values);
                    }
                }
                if (!parameter.flags.empty()) {
                    flag_table_ids[index] = Intern_Table_(
                        parameter.flags, flag_tables, Same_Symbols_);
                    if (flag_table_ids[index] == flag_tables.size()) {
                        flag_tables.push_back(parameter.flags);
                    }
                }
                if (!parameter.bitmap_values.empty()) {
                    bitmap_value_table_ids[index] = Intern_Table_(
                        parameter.bitmap_values, bitmap_value_tables, Same_Bitmap_Symbols_);
                    if (bitmap_value_table_ids[index] == bitmap_value_tables.size()) {
                        bitmap_value_tables.push_back(parameter.bitmap_values);
                    }
                }
                if (!parameter.bitmap_flags.empty()) {
                    bitmap_flag_table_ids[index] = Intern_Table_(
                        parameter.bitmap_flags, bitmap_flag_tables, Same_Bitmap_Symbols_);
                    if (bitmap_flag_table_ids[index] == bitmap_flag_tables.size()) {
                        bitmap_flag_tables.push_back(parameter.bitmap_flags);
                    }
                }
                if (!parameter.bitmap_known_bits.empty()) {
                    bitmap_known_bits_table_ids[index] = Intern_Table_(
                        parameter.bitmap_known_bits, bitmap_known_bits_tables, [](const std::vector<uint8_t>& left, const std::vector<uint8_t>& right) {
                            return left == right;
                        });
                    if (bitmap_known_bits_table_ids[index] == bitmap_known_bits_tables.size()) {
                        bitmap_known_bits_tables.push_back(parameter.bitmap_known_bits);
                    }
                }
            }
            for (size_t index = 0; index < value_tables.size(); ++index) {
                output += "static const s_symbol_descriptor generated_values_" +
                    std::to_string(index) + "[] = {\n";
                for (const s_symbol& symbol : value_tables[index]) {
                    output += "    { " + Cpp_String_(symbol.name) + ", " +
                        std::to_string(symbol.value) + " },\n";
                }
                output += "};\n\n";
            }
            for (size_t index = 0; index < flag_tables.size(); ++index) {
                output += "static const s_symbol_descriptor generated_flags_" +
                    std::to_string(index) + "[] = {\n";
                for (const s_symbol& symbol : flag_tables[index]) {
                    output += "    { " + Cpp_String_(symbol.name) + ", " +
                        std::to_string(symbol.value) + " },\n";
                }
                output += "};\n\n";
            }
            for (size_t index = 0; index < bitmap_value_tables.size(); ++index) {
                for (size_t symbol = 0; symbol < bitmap_value_tables[index].size(); ++symbol) {
                    output += "static const uint8_t generated_bitmap_value_" +
                        std::to_string(index) + "_symbol_" + std::to_string(symbol) +
                        "[] = " + Byte_Array_(bitmap_value_tables[index][symbol].value) +
                        ";\n";
                }
                output += "static const s_bitmap_symbol_descriptor generated_bitmap_values_" +
                    std::to_string(index) + "[] = {\n";
                for (size_t symbol = 0; symbol < bitmap_value_tables[index].size(); ++symbol) {
                    output += "    { " + Cpp_String_(bitmap_value_tables[index][symbol].name) +
                        ", generated_bitmap_value_" + std::to_string(index) + "_symbol_" +
                        std::to_string(symbol) + " },\n";
                }
                output += "};\n\n";
            }
            for (size_t index = 0; index < bitmap_flag_tables.size(); ++index) {
                for (size_t symbol = 0; symbol < bitmap_flag_tables[index].size(); ++symbol) {
                    output += "static const uint8_t generated_bitmap_flag_" +
                        std::to_string(index) + "_symbol_" + std::to_string(symbol) +
                        "[] = " + Byte_Array_(bitmap_flag_tables[index][symbol].value) +
                        ";\n";
                }
                output += "static const s_bitmap_symbol_descriptor generated_bitmap_flags_" +
                    std::to_string(index) + "[] = {\n";
                for (size_t symbol = 0; symbol < bitmap_flag_tables[index].size(); ++symbol) {
                    output += "    { " + Cpp_String_(bitmap_flag_tables[index][symbol].name) +
                        ", generated_bitmap_flag_" + std::to_string(index) + "_symbol_" +
                        std::to_string(symbol) + " },\n";
                }
                output += "};\n\n";
            }
            for (size_t index = 0; index < bitmap_known_bits_tables.size(); ++index) {
                output += "static const uint8_t generated_bitmap_known_bits_" +
                    std::to_string(index) + "[] = " +
                    Byte_Array_(bitmap_known_bits_tables[index]) + ";\n\n";
            }
            std::vector<size_t> type_representatives;
            std::vector<size_t> type_ids(leaf_parameters.size(), SIZE_MAX);
            for (size_t index = 0; index < leaf_parameters.size(); ++index) {
                for (size_t type_id = 0; type_id < type_representatives.size(); ++type_id) {
                    if (Same_Runtime_Type_(*leaf_parameters[index],
                            *leaf_parameters[type_representatives[type_id]])) {
                        type_ids[index] = type_id;
                        break;
                    }
                }
                if (type_ids[index] == SIZE_MAX) {
                    type_ids[index] = type_representatives.size();
                    type_representatives.push_back(index);
                }
            }
            const auto leaf_parameter_index = [&](const s_parameter* parameter) {
                return static_cast<size_t>(std::find(leaf_parameters.begin(),
                                               leaf_parameters.end(),
                                               parameter) -
                    leaf_parameters.begin());
            };
            for (size_t type_id = 0; type_id < type_representatives.size(); ++type_id) {
                const size_t index = type_representatives[type_id];
                const s_parameter& parameter = *leaf_parameters[index];
                const std::string values = parameter.values.empty() ? "nullptr" : "generated_values_" + std::to_string(value_table_ids[index]);
                const std::string flags = parameter.flags.empty() ? "nullptr" : "generated_flags_" + std::to_string(flag_table_ids[index]);
                const std::string bitmap_values = parameter.bitmap_values.empty() ? "nullptr" : "generated_bitmap_values_" + std::to_string(bitmap_value_table_ids[index]);
                const std::string bitmap_flags = parameter.bitmap_flags.empty() ? "nullptr" : "generated_bitmap_flags_" + std::to_string(bitmap_flag_table_ids[index]);
                const std::string bitmap_known_bits = parameter.bitmap_known_bits.empty() ? "nullptr" : "generated_bitmap_known_bits_" + std::to_string(bitmap_known_bits_table_ids[index]);
                output += "static const s_type_descriptor generated_type_" +
                    std::to_string(type_id) + " = { " + Cpp_Type_(parameter.type) + ", " +
                    (parameter.constraints.has_min ? "true" : "false") + ", " +
                    (parameter.constraints.has_max ? "true" : "false") + ", " +
                    std::to_string(parameter.constraints.min) + ", " +
                    std::to_string(parameter.constraints.max) + ", " +
                    Scalar_Kind_(parameter.scalar_kind) + ", " + values + ", " +
                    std::to_string(parameter.values.size()) + ", " + flags + ", " +
                    std::to_string(parameter.flags.size()) + ", " + bitmap_values + ", " +
                    std::to_string(parameter.bitmap_values.size()) + ", " + bitmap_flags + ", " +
                    std::to_string(parameter.bitmap_flags.size()) + ", " + bitmap_known_bits + ", " +
                    std::to_string(parameter.bitmap_bit_count) + ", " +
                    (parameter.allow_numeric ? "true" : "false") + ", " +
                    (parameter.reject_unknown_numeric ? "true" : "false") + ", " +
                    std::to_string(parameter.known_bits) + ", " +
                    (parameter.constraints.has_min_length ? "true" : "false") + ", " +
                    (parameter.constraints.has_max_length ? "true" : "false") + ", " +
                    std::to_string(parameter.constraints.min_length) + ", " +
                    std::to_string(parameter.constraints.max_length) + ", " +
                    Cpp_Type_(parameter.ratio_component_type) + " };\n";
            }
            output += "\n";
            output += "\nconst size_t variable_count = " +
                std::to_string(schema->parameters.size()) + ";\n\n";
            size_t maximum_target_count = 0;
            for (const s_parameter* parameter : leaf_parameters) {
                maximum_target_count = std::max(maximum_target_count,
                    parameter->native_targets.size());
            }
            output += "static_assert(CONFIG_MAX_VARIABLE_TARGETS >= " +
                std::to_string(maximum_target_count) + ");\n\n";
            std::vector<std::vector<uint32_t>> model_children(schema->nodes.size());
            for (uint32_t node_index = 0; node_index < schema->nodes.size(); ++node_index) {
                const uint32_t parent = schema->nodes[node_index].parent;
                if (parent != UINT32_MAX) model_children[parent].push_back(node_index);
            }
            std::vector<bool> omitted_nodes(schema->nodes.size(), false);
            std::vector<uint32_t> node_representatives(schema->nodes.size());
            for (uint32_t node_index = 0; node_index < schema->nodes.size(); ++node_index) {
                node_representatives[node_index] = node_index;
            }
            std::vector<bool> repeated_layout_arrays(schema->nodes.size(), false);
            for (uint32_t node_index = 0; node_index < schema->nodes.size(); ++node_index) {
                const s_node& node = schema->nodes[node_index];
                if (omitted_nodes[node_index] ||
                    (node.kind != NODE_ARRAY && node.kind != NODE_MAP &&
                        node.kind != NODE_VECTOR) ||
                    model_children[node_index].size() != static_cast<size_t>(node.array_count + (node.kind == NODE_VECTOR ? 1 : 0)) ||
                    model_children[node_index].empty()) {
                    continue;
                }
                const uint32_t representative = model_children[node_index].front();
                bool equivalent = true;
                for (size_t element = 1; element < model_children[node_index].size(); ++element) {
                    equivalent = equivalent &&
                        Same_Layout_(schema, model_children, representative, model_children[node_index][element]);
                }
                if (!equivalent) continue;
                repeated_layout_arrays[node_index] = true;
                for (size_t element = 1; element < model_children[node_index].size(); ++element) {
                    Map_Subtree_(model_children, representative, model_children[node_index][element], &omitted_nodes, &node_representatives);
                }
            }
            for (uint32_t node_index = 0; node_index < schema->nodes.size(); ++node_index) {
                while (node_representatives[node_index] !=
                    node_representatives[node_representatives[node_index]]) {
                    node_representatives[node_index] =
                        node_representatives[node_representatives[node_index]];
                }
            }
            std::vector<uint32_t> runtime_node_ids(schema->nodes.size(), UINT32_MAX);
            std::vector<uint32_t> runtime_nodes;
            for (uint32_t node_index = 0; node_index < schema->nodes.size(); ++node_index) {
                if (!omitted_nodes[node_index]) {
                    runtime_node_ids[node_index] = static_cast<uint32_t>(runtime_nodes.size());
                    runtime_nodes.push_back(node_index);
                }
            }
            output += "\n";
            for (const s_parameter& parameter : schema->parameters) {
                if (parameter.scalar_kind != SCALAR_BITMAP) {
                    continue;
                }
                const size_t byte_count = (parameter.bitmap_bit_count + 7) / 8;
                for (const std::string& target : parameter.native_targets) {
                    output += "static_assert(sizeof(" + target + ") == " +
                        std::to_string(byte_count) + ");\n";
                }
            }
            output += "\n";
            size_t array_index_alias_count = 0;
            for (size_t node_index = 0; node_index < schema->nodes.size(); ++node_index) {
                if (!omitted_nodes[node_index]) {
                    array_index_alias_count += schema->nodes[node_index].index_aliases.size();
                }
            }
            if (array_index_alias_count != 0) {
                output += "static const s_array_index_alias_descriptor generated_array_index_aliases[] = {\n";
                for (size_t node_index = 0; node_index < schema->nodes.size(); ++node_index) {
                    for (const s_symbol& alias : schema->nodes[node_index].index_aliases) {
                        if (omitted_nodes[node_index]) continue;
                        output += "    { " + std::to_string(runtime_node_ids[node_index]) + ", " +
                            Cpp_String_(alias.name) + ", " +
                            std::to_string(alias.value) + " },\n";
                    }
                }
                output +=
                    "};\n\n"
                    "const s_array_index_alias_descriptor* const array_index_aliases = "
                    "generated_array_index_aliases;\n\n";
            } else {
                output += "const s_array_index_alias_descriptor* const array_index_aliases = nullptr;\n\n";
            }
            output += "const size_t array_index_alias_count = " +
                std::to_string(array_index_alias_count) + ";\n\n";
            output += "const s_schema_node_descriptor schema_nodes[] = {\n";
            std::vector<uint32_t> first_children(runtime_nodes.size(), UINT32_MAX);
            std::vector<uint32_t> next_siblings(runtime_nodes.size(), UINT32_MAX);
            std::vector<uint32_t> last_children(runtime_nodes.size(), UINT32_MAX);
            for (uint32_t runtime_index = 0; runtime_index < runtime_nodes.size(); ++runtime_index) {
                const uint32_t node_index = runtime_nodes[runtime_index];
                const uint32_t parent = schema->nodes[node_index].parent;
                if (parent == UINT32_MAX) {
                    continue;
                }
                const uint32_t runtime_parent = runtime_node_ids[parent];
                if (first_children[runtime_parent] == UINT32_MAX) {
                    first_children[runtime_parent] = runtime_index;
                } else {
                    next_siblings[last_children[runtime_parent]] = runtime_index;
                }
                last_children[runtime_parent] = runtime_index;
            }
            if (schema->nodes.empty()) {
                for (size_t index = 0; index < schema->parameters.size(); ++index) {
                    const s_parameter& parameter = schema->parameters[index];
                    output += "    { " + Cpp_String_(parameter.path) +
                        ", CONFIG_NODE_SCALAR, CONFIG_NO_NODE, 0, " +
                        std::to_string(index) + ", " + std::to_string(index) +
                        ", " + std::to_string(index) + ", false, 0, CONFIG_NO_NODE, " +
                        "CONFIG_NO_NODE, &generated_type_" +
                        std::to_string(type_ids[index]) +
                        ", 0, 0, 0, false, false, 0, 0, 0 },\n";
                }
            } else {
                for (size_t runtime_index = 0; runtime_index < runtime_nodes.size(); ++runtime_index) {
                    const size_t node_index = runtime_nodes[runtime_index];
                    const s_node& node = schema->nodes[node_index];
                    const std::string parent = node.parent == UINT32_MAX ? "CONFIG_NO_NODE" : std::to_string(runtime_node_ids[node.parent]);
                    const std::string first = node.first_parameter == UINT32_MAX ? "CONFIG_NO_VARIABLE" : std::to_string(node.first_parameter);
                    const std::string last = node.last_parameter == UINT32_MAX ? "CONFIG_NO_VARIABLE" : std::to_string(node.last_parameter);
                    const std::string parameter = node.parameter == UINT32_MAX ? "CONFIG_NO_VARIABLE" : std::to_string(node.parameter);
                    const std::string first_child = first_children[runtime_index] == UINT32_MAX ? "CONFIG_NO_NODE" : std::to_string(first_children[runtime_index]);
                    const std::string next_sibling = next_siblings[runtime_index] == UINT32_MAX ? "CONFIG_NO_NODE" : std::to_string(next_siblings[runtime_index]);
                    const uint32_t element_variable_count = repeated_layout_arrays[node_index] ? (node.kind == NODE_MAP || node.kind == NODE_VECTOR ? schema->parameters[node.first_parameter].element_leaf_count : node.first_parameter == node.last_parameter && schema->parameters[node.first_parameter].aggregate ? static_cast<uint32_t>((node.last_leaf - node.first_leaf + 1) / node.array_count)
                                                                                                                                                                                                                                                                                                                      : static_cast<uint32_t>((node.last_parameter - node.first_parameter + 1) / node.array_count))
                                                                                               : 0;
                    const std::string runtime_type = node.parameter != UINT32_MAX ? "&generated_type_" + std::to_string(type_ids[leaf_parameter_index(&Leaf_Parameter_(schema, node))]) : "nullptr";
                    output += "    { " + Cpp_String_(node.id) + ", " +
                        Node_Kind_(node.kind) + ", " + parent + ", " +
                        std::to_string(node.external_index) + ", " + first +
                        ", " + last + ", " + parameter + ", " +
                        (node.has_terminator ? "true" : "false") + ", " +
                        std::to_string(node.terminator) + ", " + first_child + ", " +
                        next_sibling + ", " + runtime_type + ", " +
                        std::to_string(repeated_layout_arrays[node_index] ? node.array_index_base : 0) +
                        ", " +
                        std::to_string(repeated_layout_arrays[node_index] ? node.array_count : 0) + ", " +
                        std::to_string(element_variable_count) + ", " +
                        (node.implicit_terminator ? "true" : "false") + ", " +
                        (node.unique_values ? "true" : "false") + ", " +
                        std::to_string(node.first_leaf) + ", " +
                        std::to_string(node.last_leaf) + ", " +
                        std::to_string(node.leaf_index) + " },\n";
                }
            }
            output +=
                "};\n\n"
                "const size_t schema_node_count = sizeof(schema_nodes) / sizeof(schema_nodes[0]);\n\n";
            output += "const s_runtime_variable_descriptor runtime_variables[] = {\n";
            for (const s_parameter& parameter : schema->parameters) {
                bool has_arena_strings = parameter.type == PRIMITIVE_STRING &&
                    parameter.string_storage == STRING_STORAGE_ARENA;
                for (const s_parameter& leaf : parameter.aggregate_leaves) {
                    has_arena_strings = has_arena_strings ||
                        (leaf.type == PRIMITIVE_STRING &&
                            leaf.string_storage == STRING_STORAGE_ARENA);
                }
                output += "    { " + Cpp_String_(parameter.path) + ", " +
                    std::to_string(runtime_node_ids[node_representatives[parameter.node]]) + ", " +
                    Group_Constant_(parameter.group) + ", " +
                    (parameter.aggregate ? "true" : "false") + ", " +
                    (parameter.vector ? "true" : "false") + ", " +
                    std::to_string(parameter.default_element_count) + ", " +
                    std::to_string(parameter.element_leaf_count) + ", " +
                    (has_arena_strings ? "true" : "false") + ", " +
                    ((parameter.aggregate ? std::all_of(parameter.aggregate_leaves.begin(), parameter.aggregate_leaves.end(),
                        [](const s_parameter& leaf) { return leaf.default_from_native; }) : parameter.default_from_native) ? "true" : "false") + " },\n";
            }
            output += "};\n\n";
            for (size_t index = 0; index < leaf_parameters.size(); ++index) {
                const s_parameter& parameter = *leaf_parameters[index];
                if (!parameter.default_from_native && parameter.type == PRIMITIVE_BITMAP) {
                    output += "static const uint8_t generated_default_bitmap_" +
                        std::to_string(index) + "[] = " +
                        Byte_Array_(parameter.default_bitmap) + ";\n";
                }
            }
            output +=
                "\nstruct s_generated_default {\n"
                "    t_variable_id variable_id;\n"
                "    uint32_t leaf_index;\n"
                "    int64_t first;\n"
                "    int64_t second;\n"
                "    const void* data;\n"
                "    size_t size;\n"
                "};\n\n";
            size_t generated_default_count = 0;
            for (const s_parameter& parameter : schema->parameters) {
                if (parameter.aggregate) {
                    for (const s_parameter& leaf : parameter.aggregate_leaves) {
                        if (!leaf.default_from_native) ++generated_default_count;
                    }
                } else if (!parameter.default_from_native) {
                    ++generated_default_count;
                }
            }
            if (generated_default_count != 0) {
                output += "static const s_generated_default generated_defaults[] = {\n";
                const auto append_default = [&](size_t variable_id, uint32_t leaf_index, const s_parameter& parameter) {
                    if (parameter.default_from_native) return;
                    int64_t first = parameter.default_integer;
                    int64_t second = 0;
                    std::string data = "nullptr";
                    size_t size = 0;
                    if (parameter.type == PRIMITIVE_BOOL) {
                        first = parameter.default_bool ? 1 : 0;
                    } else if (parameter.type == PRIMITIVE_STRING) {
                        const std::string encoded = Encode_Game_String(parameter.default_string);
                        data = Cpp_String_(encoded);
                        size = encoded.size();
                    } else if (parameter.type == PRIMITIVE_BITMAP) {
                        data = "generated_default_bitmap_" +
                            std::to_string(leaf_parameter_index(&parameter));
                        size = parameter.default_bitmap.size();
                    } else if (parameter.type == PRIMITIVE_RATIO) {
                        first = parameter.default_ratio_numerator;
                        second = parameter.default_ratio_denominator;
                    }
                    output += "    { " + std::to_string(variable_id) + ", " +
                        std::to_string(leaf_index) + ", " + std::to_string(first) +
                        ", " + std::to_string(second) + ", " + data + ", " +
                        std::to_string(size) + " },\n";
                };
                for (size_t index = 0; index < schema->parameters.size(); ++index) {
                    const s_parameter& parameter = schema->parameters[index];
                    if (parameter.aggregate) {
                        for (uint32_t leaf = 0;
                             leaf < parameter.aggregate_leaves.size();
                             ++leaf) {
                            append_default(index, leaf, parameter.aggregate_leaves[leaf]);
                        }
                    } else {
                        append_default(index, 0, parameter);
                    }
                }
                output += "};\n\n";
            }
            output += "namespace generated {\n\n"
                      "bool Get_Default_Leaf_Value(t_variable_id variable_id, uint32_t leaf_index, s_typed_value* value) {\n"
                      "    if (value == nullptr || variable_id >= variable_count) return false;\n"
                      "    if (runtime_variables[variable_id].native_defaults) return Get_Current_Leaf_Value(variable_id, leaf_index, value);\n";
            if (generated_default_count == 0) {
                output +=
                    "    s_typed_value current;\n"
                    "    if (!Get_Current_Value(variable_id, &current)) return false;\n"
                    "    if (current.type != CONFIG_VALUE_AGGREGATE) { *value = std::move(current); return leaf_index == 0; }\n"
                    "    if (leaf_index >= current.aggregate_values.size()) return false;\n"
                    "    *value = std::move(current.aggregate_values[leaf_index]);\n"
                    "    return true;\n"
                    "}\n\n";
            } else {
                output +=
                    "    const s_runtime_variable_descriptor& descriptor = runtime_variables[variable_id];\n"
                    "    uint32_t default_leaf_index = leaf_index;\n"
                    "    if (descriptor.vector && leaf_index >=\n"
                    "            descriptor.default_element_count * descriptor.element_leaf_count) {\n"
                    "        default_leaf_index = descriptor.default_element_count *\n"
                    "            descriptor.element_leaf_count +\n"
                    "            leaf_index % descriptor.element_leaf_count;\n"
                    "    }\n"
                    "    const s_type_descriptor* type = Get_Variable_Leaf_Type(variable_id, leaf_index);\n"
                    "    if (type == nullptr) return false;\n"
                    "    size_t first = 0;\n"
                    "    size_t last = sizeof(generated_defaults) / sizeof(generated_defaults[0]);\n"
                    "    while (first < last) {\n"
                    "        const size_t middle = first + (last - first) / 2;\n"
                    "        const s_generated_default& candidate = generated_defaults[middle];\n"
                    "        if (candidate.variable_id < variable_id ||\n"
                    "            (candidate.variable_id == variable_id && candidate.leaf_index < default_leaf_index)) {\n"
                    "            first = middle + 1;\n"
                    "        } else {\n"
                    "            last = middle;\n"
                    "        }\n"
                    "    }\n"
                    "    if (first == sizeof(generated_defaults) / sizeof(generated_defaults[0]) ||\n"
                    "        generated_defaults[first].variable_id != variable_id ||\n"
                    "        generated_defaults[first].leaf_index != default_leaf_index) {\n"
                    "        return Get_Current_Leaf_Value(variable_id, leaf_index, value);\n"
                    "    }\n"
                    "    const s_generated_default& generated = generated_defaults[first];\n"
                    "    *value = {};\n"
                    "    value->type = type->value_type;\n"
                    "    switch (type->value_type) {\n"
                    "    case CONFIG_VALUE_BOOL:\n"
                    "        value->bool_value = generated.first != 0;\n"
                    "        break;\n"
                    "    case CONFIG_VALUE_STRING:\n"
                    "        value->string_value.assign(static_cast<const char*>(generated.data), generated.size);\n"
                    "        break;\n"
                    "    case CONFIG_VALUE_BITMAP: {\n"
                    "        const uint8_t* bytes = static_cast<const uint8_t*>(generated.data);\n"
                    "        value->bitmap_value.assign(bytes, bytes + generated.size);\n"
                    "        break;\n"
                    "    }\n"
                    "    case CONFIG_VALUE_RATIO:\n"
                    "        value->ratio_numerator = generated.first;\n"
                    "        value->ratio_denominator = generated.second;\n"
                    "        break;\n"
                    "    default:\n"
                    "        value->integer_value = generated.first;\n"
                    "        break;\n"
                    "    }\n"
                    "    return true;\n"
                    "}\n\n";
            }
            output +=
                "bool Get_Default_Value(t_variable_id variable_id, s_typed_value* value) {\n"
                "    if (value == nullptr || variable_id >= variable_count) return false;\n"
                "    const s_runtime_variable_descriptor& descriptor = runtime_variables[variable_id];\n"
                "    if (descriptor.native_defaults) return Get_Current_Value(variable_id, value);\n"
                "    if (!descriptor.aggregate) return Get_Default_Leaf_Value(variable_id, 0, value);\n"
                "    const uint32_t leaf_count = descriptor.vector ?\n"
                "        descriptor.default_element_count * descriptor.element_leaf_count :\n"
                "        descriptor.element_leaf_count;\n"
                "    *value = {};\n"
                "    value->type = CONFIG_VALUE_AGGREGATE;\n"
                "    value->aggregate_values.resize(leaf_count);\n"
                "    for (uint32_t leaf_index = 0; leaf_index < leaf_count; ++leaf_index) {\n"
                "        if (!Get_Default_Leaf_Value(variable_id, leaf_index,\n"
                "                &value->aggregate_values[leaf_index])) return false;\n"
                "    }\n"
                "    return true;\n"
                "}\n\n";
            for (size_t runtime_index = 0; runtime_index < runtime_nodes.size(); ++runtime_index) {
                const uint32_t node_index = runtime_nodes[runtime_index];
                const s_node& node = schema->nodes[node_index];
                if (node.kind != NODE_SCALAR) continue;
                const s_parameter& parameter = Leaf_Parameter_(schema, node);
                output += "static bool Resolve_Generated_Variable_" +
                    std::to_string(runtime_index) +
                    "(t_variable_id variable_id, uint32_t leaf_index, s_resolved_variable* variable) {\n"
                    "    variable->type = &generated_type_" +
                    std::to_string(type_ids[leaf_parameter_index(&parameter)]) + ";\n";
                std::set<uint32_t> emitted_indices;
                for (size_t target_index = 0;
                     target_index < parameter.native_target_expressions.size();
                     ++target_index) {
                    const s_native_target_expression& target =
                        parameter.native_target_expressions[target_index];
                    std::string expression = target.root;
                    for (const s_native_target_component& component : target.components) {
                        if (!component.array_index) {
                            expression += "." + component.member;
                            continue;
                        }
                        const uint32_t representative =
                            node_representatives[component.array_node];
                        if (!repeated_layout_arrays[representative]) {
                            expression += "[" + std::to_string(component.index) + "]";
                            continue;
                        }
                        const uint32_t runtime_node = runtime_node_ids[representative];
                        const std::string index_name =
                            "generated_index_" + std::to_string(runtime_node);
                        if (emitted_indices.insert(runtime_node).second) {
                            output += "    uint32_t " + index_name + " = 0;\n"
                                                                     "    if (!Get_Array_Element_Offset("
                                                                     "Get_Runtime_Schema(), variable_id, leaf_index, " +
                                std::to_string(runtime_node) + ", &" + index_name +
                                ")) return false;\n";
                        }
                        expression += "[" + index_name + "]";
                    }
                    const char* storage = parameter.type != PRIMITIVE_STRING ? "s_variable_target::CONFIG_TARGET_VALUE" : parameter.string_storage == STRING_STORAGE_ARENA ? parameter.nullable ? "s_variable_target::CONFIG_TARGET_NULLABLE_ARENA_STRING_POINTER" : "s_variable_target::CONFIG_TARGET_ARENA_STRING_POINTER"
                                                                                                                                                                           : "s_variable_target::CONFIG_TARGET_STRING_BUFFER";
                    output += "    variable->targets[" + std::to_string(target_index) +
                        "] = { (void*)&" + expression + ", sizeof(" + expression +
                        "), " + storage + " };\n";
                }
                output += "    variable->target_count = " +
                    std::to_string(parameter.native_targets.size()) + ";\n"
                                                                      "    return true;\n"
                                                                      "}\n\n";
            }
            output += "using t_generated_variable_resolver = bool (*)(t_variable_id, uint32_t, "
                      "s_resolved_variable*);\n\n"
                      "static const t_generated_variable_resolver generated_variable_resolvers[] = {\n";
            for (size_t runtime_index = 0; runtime_index < runtime_nodes.size(); ++runtime_index) {
                output += schema->nodes[runtime_nodes[runtime_index]].kind == NODE_SCALAR ? "    Resolve_Generated_Variable_" + std::to_string(runtime_index) + ",\n" : "    nullptr,\n";
            }
            output += "};\n\n"
                      "bool Resolve_Variable(t_variable_id variable_id, uint32_t leaf_index, "
                      "s_resolved_variable* variable) {\n"
                      "    if (variable == nullptr) return false;\n"
                      "    *variable = {};\n"
                      "    const s_schema_node_descriptor* leaf = Get_Variable_Leaf_Node(\n"
                      "        Get_Runtime_Schema(), variable_id, leaf_index);\n"
                      "    if (leaf == nullptr) return false;\n"
                      "    const size_t node_index = static_cast<size_t>(leaf - schema_nodes);\n"
                      "    if (node_index >= sizeof(generated_variable_resolvers) /\n"
                      "            sizeof(generated_variable_resolvers[0]) ||\n"
                      "        generated_variable_resolvers[node_index] == nullptr) {\n"
                      "        return false;\n"
                      "    }\n"
                      "    return generated_variable_resolvers[node_index](variable_id, leaf_index, variable);\n"
                      "}\n\n"
                      "bool Resize_Vector(t_variable_id variable_id, size_t element_count) {\n"
                      "    switch (variable_id) {\n";
            for (size_t index = 0; index < schema->parameters.size(); ++index) {
                const s_parameter& parameter = schema->parameters[index];
                if (!parameter.vector || parameter.aggregate_leaves.empty()) continue;
                const s_native_target_expression& target =
                    parameter.aggregate_leaves.front().native_target_expressions.front();
                std::string expression = target.root;
                for (const s_native_target_component& component : target.components) {
                    if (component.array_index) break;
                    expression += "." + component.member;
                }
                output += "    case " + std::to_string(index) + ": " + expression +
                    ".resize(element_count); return true;\n";
            }
            output += "    default: return false;\n"
                      "    }\n"
                      "}\n\n"
                      "size_t Get_Vector_Size(t_variable_id variable_id) {\n"
                      "    switch (variable_id) {\n";
            for (size_t index = 0; index < schema->parameters.size(); ++index) {
                const s_parameter& parameter = schema->parameters[index];
                if (!parameter.vector || parameter.aggregate_leaves.empty()) continue;
                const s_native_target_expression& target =
                    parameter.aggregate_leaves.front().native_target_expressions.front();
                std::string expression = target.root;
                for (const s_native_target_component& component : target.components) {
                    if (component.array_index) break;
                    expression += "." + component.member;
                }
                output += "    case " + std::to_string(index) + ": return " + expression +
                    ".size();\n";
            }
            output += "    default: return 0;\n"
                      "    }\n"
                      "}\n\n"
                      "}\n\n"
                      "}\n";
            return output;
        }
    } // namespace

    bool Generate_Cpp(const s_schema* schema, const char* output_directory, std::string* diagnostic) {
        if (!Validate_Runtime_Layout(schema, diagnostic)) {
            return false;
        }
        std::error_code error;
        std::filesystem::create_directories(output_directory, error);
        if (error) {
            *diagnostic = std::string(output_directory) + ": unable to create output directory";
            return false;
        }
        const std::string output_path = Join_Path_(output_directory, "config.cpp");
        const std::string enum_source_path =
            Join_Path_(output_directory, "config_enums.cpp");
        const std::string enum_header_path =
            Join_Path_(output_directory, "config_enums.h");
        const std::string group_header_path =
            Join_Path_(output_directory, "config_groups.h");
        const std::string settings_metadata_path =
            Join_Path_(output_directory, "settings_metadata.h");
        return Write_File_If_Different(
                   group_header_path.c_str(), Group_Header_(schema), diagnostic) &&
            Write_File_If_Different(settings_metadata_path.c_str(),
                Settings_Metadata_Header_(schema), diagnostic) &&
            Write_File_If_Different(
                enum_header_path.c_str(), Enum_Header_(), diagnostic) &&
            Write_File_If_Different(
                enum_source_path.c_str(), Enum_Source_(schema), diagnostic) &&
            Write_File_If_Different(output_path.c_str(), Source_(schema), diagnostic);
    }

    bool Check_Generated_Cpp(const s_schema* schema, const char* committed_directory, const char* temporary_directory, std::string* diagnostic) {
        if (!Generate_Cpp(schema, temporary_directory, diagnostic)) {
            return false;
        }
        for (const char* name : GENERATED_FILES) {
            std::string expected;
            std::string actual;
            const std::string committed_path = Join_Path_(committed_directory, name);
            const std::string temporary_path = Join_Path_(temporary_directory, name);
            if (!Read_File(committed_path.c_str(), &expected)) {
                *diagnostic = committed_path + ": generated file is missing";
                return false;
            }
            if (!Read_File(temporary_path.c_str(), &actual) || expected != actual) {
                *diagnostic = committed_path + ": generated file is stale";
                return false;
            }
        }
        for (const std::filesystem::directory_entry& entry :
            std::filesystem::directory_iterator(committed_directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            bool known = false;
            for (const char* name : GENERATED_FILES) {
                if (entry.path().filename() == name) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                *diagnostic = entry.path().string() + ": unexpected generated file";
                return false;
            }
        }
        return true;
    }

} // namespace config::tool