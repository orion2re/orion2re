#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace config::tool {

    enum e_primitive_type {
        PRIMITIVE_BOOL,
        PRIMITIVE_INT8,
        PRIMITIVE_UINT8,
        PRIMITIVE_INT16,
        PRIMITIVE_UINT16,
        PRIMITIVE_INT32,
        PRIMITIVE_UINT32,
        PRIMITIVE_STRING,
        PRIMITIVE_BITMAP,
        PRIMITIVE_RATIO,
    };

    enum e_string_storage {
        STRING_STORAGE_BUFFER,
        STRING_STORAGE_ARENA,
    };

    enum e_scalar_kind {
        SCALAR_PRIMITIVE,
        SCALAR_ENUM,
        SCALAR_MASK,
        SCALAR_BITMAP,
        SCALAR_RATIO,
    };

    enum e_node_kind {
        NODE_SCALAR,
        NODE_RECORD,
        NODE_ARRAY,
        NODE_MAP,
        NODE_VECTOR,
        NODE_NAMESPACE,
    };

    struct s_symbol {
        std::string name;
        std::string description;
        int64_t value = 0;
    };

    struct s_bitmap_symbol {
        std::string name;
        std::string description;
        std::vector<uint8_t> value;
    };

    struct s_metadata {
        std::string title;
        std::string description;
        std::string remarks;
        std::string units;
        std::vector<std::string> examples;
    };

    struct s_constraints {
        bool has_min = false;
        bool has_max = false;
        int64_t min = 0;
        int64_t max = 0;
        bool has_min_length = false;
        bool has_max_length = false;
        size_t min_length = 0;
        size_t max_length = 0;
        std::string encoding;
    };

    struct s_binding {
        std::string header;
        std::string symbol;
    };

    struct s_native_target_component {
        bool array_index = false;
        std::string member;
        uint32_t array_node = UINT32_MAX;
        int64_t index = 0;
    };

    struct s_native_target_expression {
        std::string root;
        std::vector<s_native_target_component> components;
    };

    struct s_documented_type;

    struct s_documented_field {
        std::string id;
        s_metadata metadata;
        std::string group;
        bool internal = false;
        std::shared_ptr<s_documented_type> type;
        bool has_default = false;
        std::string default_value;
    };

    struct s_documented_type {
        e_node_kind node_kind = NODE_SCALAR;
        e_primitive_type primitive = PRIMITIVE_BOOL;
        e_scalar_kind scalar_kind = SCALAR_PRIMITIVE;
        std::string reference;
        s_constraints constraints;
        std::vector<s_symbol> values;
        std::vector<s_symbol> flags;
        std::vector<s_bitmap_symbol> bitmap_values;
        std::vector<s_bitmap_symbol> bitmap_flags;
        size_t bitmap_bit_count = 0;
        bool allow_numeric = false;
        bool reject_unknown_numeric = false;
        int64_t array_count = 0;
        int64_t array_index_base = 1;
        std::string array_index_type;
        std::shared_ptr<s_documented_type> array_element;
        std::vector<s_documented_field> fields;
        e_string_storage string_storage = STRING_STORAGE_BUFFER;
        bool nullable = false;
    };

    struct s_documentation_reference {
        std::string title;
        std::string anchor;
    };

    struct s_type_declaration {
        std::string id;
        s_metadata metadata;
        bool internal = false;
        s_documented_type type;
        std::vector<s_documentation_reference> used_by;
    };

    struct s_record_declaration {
        std::string id;
        s_metadata metadata;
        bool internal = false;
        std::vector<s_documented_field> fields;
        std::vector<s_documentation_reference> used_by;
    };

    struct s_parameter_declaration {
        std::string id;
        std::string name_space;
        std::string documentation_category;
        bool include_default_configuration = true;
        s_metadata metadata;
        std::string group;
        s_documented_type type;
        bool has_default = false;
        std::string default_value;
        bool default_from_native = false;
        s_binding binding;
    };

    struct s_group {
        std::string id;
        s_metadata metadata;
        bool save = false;
        bool synchronize = false;
    };

    struct s_documentation_category {
        std::string id;
        s_metadata metadata;
        std::vector<std::string> namespaces;
    };

    struct s_parameter {
        std::string path;
        s_metadata metadata;
        std::string group;
        e_primitive_type type = PRIMITIVE_BOOL;
        e_scalar_kind scalar_kind = SCALAR_PRIMITIVE;
        s_constraints constraints;
        bool default_bool = false;
        int64_t default_integer = 0;
        std::string default_string;
        std::vector<uint8_t> default_bitmap;
        int64_t default_ratio_numerator = 0;
        int64_t default_ratio_denominator = 1;
        e_primitive_type ratio_component_type = PRIMITIVE_INT16;
        std::vector<s_symbol> values;
        std::vector<s_symbol> flags;
        std::vector<s_bitmap_symbol> bitmap_values;
        std::vector<s_bitmap_symbol> bitmap_flags;
        std::vector<uint8_t> bitmap_known_bits;
        size_t bitmap_bit_count = 0;
        bool allow_numeric = false;
        bool reject_unknown_numeric = false;
        uint64_t known_bits = 0;
        s_binding binding;
        std::vector<std::string> native_targets;
        std::vector<s_native_target_expression> native_target_expressions;
        e_string_storage string_storage = STRING_STORAGE_BUFFER;
        bool nullable = false;
        bool default_from_native = false;
        bool aggregate = false;
        bool vector = false;
        uint32_t node = UINT32_MAX;
        uint32_t leaf_index = 0;
        uint32_t default_element_count = 0;
        uint32_t element_leaf_count = 1;
        std::vector<s_parameter> aggregate_leaves;
    };

    struct s_node {
        std::string id;
        std::string path;
        e_node_kind kind = NODE_SCALAR;
        uint32_t parent = UINT32_MAX;
        int32_t external_index = 0;
        int64_t array_count = 0;
        int64_t array_index_base = 0;
        size_t explicit_default_count = 0;
        std::vector<std::string> native_targets;
        std::vector<s_native_target_expression> native_target_expressions;
        bool has_terminator = false;
        int64_t terminator = 0;
        bool implicit_terminator = false;
        bool unique_values = false;
        std::vector<s_symbol> index_aliases;
        uint32_t first_parameter = UINT32_MAX;
        uint32_t last_parameter = UINT32_MAX;
        uint32_t parameter = UINT32_MAX;
        uint32_t first_leaf = 0;
        uint32_t last_leaf = 0;
        uint32_t leaf_index = 0;
    };

    struct s_schema {
        std::vector<s_group> groups;
        std::vector<s_documentation_category> documentation_categories;
        std::vector<s_type_declaration> type_declarations;
        std::vector<s_record_declaration> record_declarations;
        std::vector<s_parameter_declaration> parameter_declarations;
        std::vector<s_parameter> parameters;
        std::vector<s_node> nodes;
    };

    bool Load_And_Validate_Schema(const char* path, s_schema* schema, std::string* diagnostic);
    bool Validate_Runtime_Layout(const s_schema* schema, std::string* diagnostic);
    const char* Primitive_Name(e_primitive_type type);
    std::string Encode_Game_String(const std::string& value);
    std::string Decode_Game_String(const std::string& value);

} // namespace config::tool