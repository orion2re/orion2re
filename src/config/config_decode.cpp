#include "config_decode.h"

#include <climits>
#include <cstdlib>
#include <numeric>
#include <utility>

#include "config/text_encoding.h"

namespace config {
    namespace {
        void Primitive_Range(e_value_type type, int64_t* minimum, int64_t* maximum);
        bool Parse_Integer(const std::string& text, int64_t* value);

        bool Fail(const s_source_location& location, const std::string& message, s_config_write_set* write_set, s_cfg_diagnostic* diagnostic) {
            write_set->writes.clear();
            diagnostic->location = location;
            diagnostic->message = message;
            return false;
        }

        const char* Type_Name(e_value_type type) {
            switch (type) {
                case CONFIG_VALUE_BOOL: return "bool";
                case CONFIG_VALUE_INT8: return "int8";
                case CONFIG_VALUE_UINT8: return "uint8";
                case CONFIG_VALUE_INT16: return "int16";
                case CONFIG_VALUE_UINT16: return "uint16";
                case CONFIG_VALUE_INT32: return "int32";
                case CONFIG_VALUE_UINT32: return "uint32";
                case CONFIG_VALUE_STRING: return "string";
                case CONFIG_VALUE_BITMAP: return "bitmap";
                case CONFIG_VALUE_RATIO: return "ratio";
            }
            return "unknown";
        }

        const s_symbol_descriptor* Find_Symbol(const s_symbol_descriptor* symbols,
            size_t count,
            const std::string& name) {
            for (size_t index = 0; index < count; ++index) {
                if (name == symbols[index].name) {
                    return &symbols[index];
                }
            }
            return nullptr;
        }

        const s_bitmap_symbol_descriptor* Find_Bitmap_Symbol(
            const s_bitmap_symbol_descriptor* symbols, size_t count, const std::string& name) {
            for (size_t index = 0; index < count; ++index) {
                if (name == symbols[index].name) {
                    return &symbols[index];
                }
            }
            return nullptr;
        }

        bool Decode_Utf8(const std::string& text, size_t* offset, uint32_t* codepoint) {
            const uint8_t first = static_cast<uint8_t>(text[*offset]);
            if (first < 0x80) {
                *codepoint = first;
                ++*offset;
                return true;
            }
            size_t count = 0;
            uint32_t value = 0;
            if ((first & 0xE0) == 0xC0) {
                count = 2;
                value = first & 0x1F;
            } else if ((first & 0xF0) == 0xE0) {
                count = 3;
                value = first & 0x0F;
            } else if ((first & 0xF8) == 0xF0) {
                count = 4;
                value = first & 0x07;
            } else {
                return false;
            }
            if (*offset + count > text.size()) {
                return false;
            }
            for (size_t index = 1; index < count; ++index) {
                const uint8_t next = static_cast<uint8_t>(text[*offset + index]);
                if ((next & 0xC0) != 0x80) {
                    return false;
                }
                value = value << 6 | (next & 0x3F);
            }
            *offset += count;
            *codepoint = value;
            return true;
        }

        bool Decode_Bitmap(const s_variable_descriptor* variable,
            const s_cfg_token& token,
            s_typed_value* value,
            std::string* expected) {
            const s_type_descriptor* type = variable->type;
            const size_t byte_count = (type->bitmap_bit_count + 7) / 8;
            value->bitmap_value.assign(byte_count, 0);
            if (token.kind == CFG_TOKEN_IDENTIFIER) {
                const s_bitmap_symbol_descriptor* symbol = Find_Bitmap_Symbol(
                    type->bitmap_values, type->bitmap_value_count, token.value);
                if (symbol == nullptr) {
                    symbol = Find_Bitmap_Symbol(type->bitmap_flags,
                        type->bitmap_flag_count,
                        token.value);
                }
                if (symbol == nullptr) {
                    *expected = "a declared bitmap value or flag";
                    return false;
                }
                value->bitmap_value.assign(symbol->value, symbol->value + byte_count);
                return true;
            }
            if (token.kind != CFG_TOKEN_MASK) {
                *expected = "declared bitmap flags";
                return false;
            }
            size_t start = 0;
            while (start < token.value.size()) {
                const size_t comma = token.value.find(',', start);
                const size_t end = comma == std::string::npos ? token.value.size() : comma;
                const s_bitmap_symbol_descriptor* flag = Find_Bitmap_Symbol(
                    type->bitmap_flags, type->bitmap_flag_count, token.value.substr(start, end - start));
                if (flag == nullptr) {
                    *expected = "declared bitmap flags";
                    return false;
                }
                for (size_t byte = 0; byte < byte_count; ++byte) {
                    value->bitmap_value[byte] |= flag->value[byte];
                }
                start = end + 1;
            }
            return true;
        }

        bool Append_Cp850(uint32_t codepoint, std::string* output) {
            static const uint16_t extended[] = {
                0x00C7,
                0x00FC,
                0x00E9,
                0x00E2,
                0x00E4,
                0x00E0,
                0x00E5,
                0x00E7,
                0x00EA,
                0x00EB,
                0x00E8,
                0x00EF,
                0x00EE,
                0x00EC,
                0x00C4,
                0x00C5,
                0x00C9,
                0x00E6,
                0x00C6,
                0x00F4,
                0x00F6,
                0x00F2,
                0x00FB,
                0x00F9,
                0x00FF,
                0x00D6,
                0x00DC,
                0x00F8,
                0x00A3,
                0x00D8,
                0x00D7,
                0x0192,
                0x00E1,
                0x00ED,
                0x00F3,
                0x00FA,
                0x00F1,
                0x00D1,
                0x00AA,
                0x00BA,
                0x00BF,
                0x00AE,
                0x00AC,
                0x00BD,
                0x00BC,
                0x00A1,
                0x00AB,
                0x00BB,
                0x2591,
                0x2592,
                0x2593,
                0x2502,
                0x2524,
                0x00C1,
                0x00C2,
                0x00C0,
                0x00A9,
                0x2563,
                0x2551,
                0x2557,
                0x255D,
                0x00A2,
                0x00A5,
                0x2510,
                0x2514,
                0x2534,
                0x252C,
                0x251C,
                0x2500,
                0x253C,
                0x00E3,
                0x00C3,
                0x255A,
                0x2554,
                0x2569,
                0x2566,
                0x2560,
                0x2550,
                0x256C,
                0x00A4,
                0x00F0,
                0x00D0,
                0x00CA,
                0x00CB,
                0x00C8,
                0x0131,
                0x00CD,
                0x00CE,
                0x00CF,
                0x2518,
                0x250C,
                0x2588,
                0x2584,
                0x00A6,
                0x00CC,
                0x2580,
                0x00D3,
                0x00DF,
                0x00D4,
                0x00D2,
                0x00F5,
                0x00D5,
                0x00B5,
                0x00FE,
                0x00DE,
                0x00DA,
                0x00DB,
                0x00D9,
                0x00FD,
                0x00DD,
                0x00AF,
                0x00B4,
                0x00AD,
                0x00B1,
                0x2017,
                0x00BE,
                0x00B6,
                0x00A7,
                0x00F7,
                0x00B8,
                0x00B0,
                0x00A8,
                0x00B7,
                0x00B9,
                0x00B3,
                0x00B2,
                0x25A0,
                0x00A0,
            };
            if (codepoint < 0x80) {
                output->push_back(static_cast<char>(codepoint));
                return true;
            }
            for (size_t index = 0; index < sizeof(extended) / sizeof(extended[0]); ++index) {
                if (extended[index] == codepoint) {
                    output->push_back(static_cast<char>(0x80 + index));
                    return true;
                }
            }
            return false;
        }

        bool Decode_String(const s_variable_descriptor* variable,
            const s_cfg_token& token,
            s_typed_value* value,
            std::string* expected) {
            const s_type_descriptor* type = variable->type;
            if (token.kind != CFG_TOKEN_STRING) {
                return false;
            }
            if (!Encode_Utf8_To_Cp850(token.value.data(), token.value.size(), &value->string_value)) {
                *expected = "a CP850-representable string";
                return false;
            }
            const size_t length = value->string_value.size();
            if ((type->has_min_length && length < type->min_length) ||
                (type->has_max_length && length > type->max_length)) {
                *expected = "a string with CP850 byte length ";
                *expected += type->has_min_length ? std::to_string(type->min_length) : "0";
                *expected += "..";
                *expected += type->has_max_length ? std::to_string(type->max_length) : "unbounded";
                return false;
            }
            return true;
        }

        bool Decode_Integer(const s_variable_descriptor* variable,
            const s_cfg_token& token,
            int64_t* integer,
            std::string* expected) {
            const s_type_descriptor* type = variable->type;
            if (token.kind != CFG_TOKEN_INTEGER || !Parse_Integer(token.value, integer)) {
                return false;
            }
            int64_t primitive_minimum = 0;
            int64_t primitive_maximum = 0;
            Primitive_Range(type->value_type, &primitive_minimum, &primitive_maximum);
            const int64_t minimum = type->has_min ? type->min : primitive_minimum;
            const int64_t maximum = type->has_max ? type->max : primitive_maximum;
            if (*integer < minimum || *integer > maximum) {
                *expected += " in range " + std::to_string(minimum) + ".." +
                    std::to_string(maximum);
                return false;
            }
            return true;
        }

        bool Decode_Enum(const s_variable_descriptor* variable,
            const s_cfg_token& token,
            s_typed_value* value,
            std::string* expected) {
            const s_type_descriptor* type = variable->type;
            if (token.kind == CFG_TOKEN_IDENTIFIER) {
                const s_symbol_descriptor* symbol = Find_Symbol(
                    type->values, type->value_count, token.value);
                if (symbol == nullptr) {
                    *expected = "a declared " + *expected + " value";
                    return false;
                }
                value->integer_value = symbol->value;
                return true;
            }
            if (!type->allow_numeric ||
                !Decode_Integer(variable, token, &value->integer_value, expected)) {
                return false;
            }
            if (type->reject_unknown_numeric) {
                for (size_t index = 0; index < type->value_count; ++index) {
                    if (type->values[index].value == value->integer_value) {
                        return true;
                    }
                }
                *expected = "a declared numeric " + *expected + " value";
                return false;
            }
            return true;
        }

        bool Decode_Mask(const s_variable_descriptor* variable,
            const s_cfg_token& token,
            s_typed_value* value,
            std::string* expected) {
            const s_type_descriptor* type = variable->type;
            if (token.kind == CFG_TOKEN_INTEGER) {
                if (!type->allow_numeric ||
                    !Decode_Integer(variable, token, &value->integer_value, expected)) {
                    return false;
                }
            } else if (token.kind == CFG_TOKEN_IDENTIFIER) {
                const s_symbol_descriptor* symbol = Find_Symbol(
                    type->values, type->value_count, token.value);
                if (symbol == nullptr) {
                    symbol = Find_Symbol(type->flags, type->flag_count, token.value);
                }
                if (symbol == nullptr) {
                    *expected = "a declared " + *expected + " value";
                    return false;
                }
                value->integer_value = symbol->value;
            } else if (token.kind == CFG_TOKEN_MASK) {
                value->integer_value = 0;
                size_t start = 0;
                while (start < token.value.size()) {
                    const size_t comma = token.value.find(',', start);
                    const size_t end = comma == std::string::npos ? token.value.size() : comma;
                    const std::string name = token.value.substr(start, end - start);
                    const s_symbol_descriptor* flag = Find_Symbol(
                        type->flags, type->flag_count, name);
                    if (flag == nullptr) {
                        *expected = "declared mask flags";
                        return false;
                    }
                    value->integer_value |= flag->value;
                    start = end + 1;
                }
            } else {
                return false;
            }
            if (type->reject_unknown_numeric &&
                (static_cast<uint64_t>(value->integer_value) & ~type->known_bits) != 0) {
                *expected = "a mask containing only declared bits";
                return false;
            }
            return true;
        }

        bool Decode_Ratio(const s_variable_descriptor* variable,
            const s_cfg_token& token,
            s_typed_value* value,
            std::string* expected) {
            const s_type_descriptor* type = variable->type;
            std::string numerator_text = token.value;
            std::string denominator_text = "1";
            if (token.kind == CFG_TOKEN_RATIO) {
                const size_t slash = token.value.find('/');
                numerator_text = token.value.substr(0, slash);
                denominator_text = token.value.substr(slash + 1);
            } else if (token.kind != CFG_TOKEN_INTEGER) {
                return false;
            }
            if (!Parse_Integer(numerator_text, &value->ratio_numerator) ||
                !Parse_Integer(denominator_text, &value->ratio_denominator) ||
                value->ratio_denominator == 0 || value->ratio_numerator == INT64_MIN ||
                value->ratio_denominator == INT64_MIN) {
                *expected = "a ratio with a nonzero denominator";
                return false;
            }
            if (value->ratio_denominator < 0) {
                value->ratio_numerator = -value->ratio_numerator;
                value->ratio_denominator = -value->ratio_denominator;
            }
            const int64_t divisor = std::gcd(value->ratio_numerator,
                value->ratio_denominator);
            value->ratio_numerator /= divisor;
            value->ratio_denominator /= divisor;
            if (value->ratio_numerator == 0) {
                value->ratio_denominator = 1;
            }
            int64_t minimum = 0;
            int64_t maximum = 0;
            Primitive_Range(type->ratio_component_type, &minimum, &maximum);
            if (value->ratio_numerator < minimum || value->ratio_numerator > maximum ||
                value->ratio_denominator < minimum || value->ratio_denominator > maximum) {
                *expected = "a ratio whose normalized components fit " +
                    std::string(Type_Name(type->ratio_component_type));
                return false;
            }
            return true;
        }

        void Primitive_Range(e_value_type type, int64_t* minimum, int64_t* maximum) {
            switch (type) {
                case CONFIG_VALUE_INT8:
                    *minimum = INT8_MIN;
                    *maximum = INT8_MAX;
                    break;
                case CONFIG_VALUE_UINT8:
                    *minimum = 0;
                    *maximum = UINT8_MAX;
                    break;
                case CONFIG_VALUE_INT16:
                    *minimum = INT16_MIN;
                    *maximum = INT16_MAX;
                    break;
                case CONFIG_VALUE_UINT16:
                    *minimum = 0;
                    *maximum = UINT16_MAX;
                    break;
                case CONFIG_VALUE_INT32:
                    *minimum = INT32_MIN;
                    *maximum = INT32_MAX;
                    break;
                case CONFIG_VALUE_UINT32:
                    *minimum = 0;
                    *maximum = UINT32_MAX;
                    break;
                default:
                    *minimum = 0;
                    *maximum = 0;
                    break;
            }
        }

        int Digit(char character, uint32_t base) {
            int value = -1;
            if (character >= '0' && character <= '9') {
                value = character - '0';
            } else if (character >= 'a' && character <= 'f') {
                value = character - 'a' + 10;
            } else if (character >= 'A' && character <= 'F') {
                value = character - 'A' + 10;
            }
            return value >= 0 && static_cast<uint32_t>(value) < base ? value : -1;
        }

        bool Parse_Integer(const std::string& text, int64_t* value) {
            size_t offset = 0;
            bool negative = false;
            if (text[offset] == '+' || text[offset] == '-') {
                negative = text[offset] == '-';
                ++offset;
            }

            uint32_t base = 10;
            if (offset + 2 <= text.size() && text[offset] == '0' &&
                (text[offset + 1] == 'x' || text[offset + 1] == 'X')) {
                base = 16;
                offset += 2;
            } else if (offset + 2 <= text.size() && text[offset] == '0' &&
                (text[offset + 1] == 'b' || text[offset + 1] == 'B')) {
                base = 2;
                offset += 2;
            }

            const uint64_t limit = negative ? UINT64_C(9223372036854775808) : UINT64_C(9223372036854775807);
            uint64_t magnitude = 0;
            for (; offset < text.size(); ++offset) {
                const int digit = Digit(text[offset], base);
                if (digit < 0 || magnitude > (limit - static_cast<uint32_t>(digit)) / base) {
                    return false;
                }
                magnitude = magnitude * base + static_cast<uint32_t>(digit);
            }
            if (negative && magnitude == UINT64_C(9223372036854775808)) {
                *value = INT64_MIN;
            } else if (negative) {
                *value = -static_cast<int64_t>(magnitude);
            } else {
                *value = static_cast<int64_t>(magnitude);
            }
            return true;
        }

        bool Decode_Bool(const s_cfg_token& token, s_typed_value* value) {
            if (token.kind != CFG_TOKEN_IDENTIFIER) {
                return false;
            }
            if (token.value == "true") {
                value->bool_value = true;
                return true;
            }
            if (token.value == "false") {
                value->bool_value = false;
                return true;
            }
            return false;
        }

        bool Decode_Value(const s_variable_descriptor* variable,
            const s_cfg_token& token,
            s_typed_value* value,
            std::string* expected) {
            const s_type_descriptor* type = variable->type;
            value->type = type->value_type;
            *expected = Type_Name(type->value_type);
            if (type->scalar_kind == CONFIG_SCALAR_ENUM) {
                return Decode_Enum(variable, token, value, expected);
            }
            if (type->scalar_kind == CONFIG_SCALAR_MASK) {
                return Decode_Mask(variable, token, value, expected);
            }
            if (type->scalar_kind == CONFIG_SCALAR_BITMAP) {
                return Decode_Bitmap(variable, token, value, expected);
            }
            if (type->scalar_kind == CONFIG_SCALAR_RATIO) {
                return Decode_Ratio(variable, token, value, expected);
            }
            if (type->value_type == CONFIG_VALUE_BOOL) {
                return Decode_Bool(token, value);
            }
            if (type->value_type == CONFIG_VALUE_STRING) {
                return Decode_String(variable, token, value, expected);
            }
            return Decode_Integer(variable, token, &value->integer_value, expected);
        }

        bool Parse_Index(const s_cfg_token& token, int32_t* value) {
            int64_t parsed = 0;
            return token.kind == CFG_TOKEN_INTEGER && Parse_Integer(token.value, &parsed) &&
                parsed >= INT32_MIN && parsed <= INT32_MAX &&
                (*value = static_cast<int32_t>(parsed), true);
        }

        uint32_t Find_Child(const s_runtime_schema* schema, uint32_t parent, const s_cfg_token& token, int32_t* resolved_index, t_variable_id* variable_offset) {
            bool has_index = false;
            int32_t external_index = 0;
            if (schema->nodes[parent].kind == CONFIG_NODE_ARRAY ||
                schema->nodes[parent].kind == CONFIG_NODE_MAP ||
                schema->nodes[parent].kind == CONFIG_NODE_VECTOR) {
                has_index = Parse_Index(token, &external_index);
                if (!has_index && token.kind == CFG_TOKEN_IDENTIFIER) {
                    for (size_t index = 0; index < schema->array_index_alias_count; ++index) {
                        const s_array_index_alias_descriptor& alias =
                            schema->array_index_aliases[index];
                        if (alias.array_node == parent && token.value == alias.name) {
                            external_index = alias.external_index;
                            has_index = true;
                            break;
                        }
                    }
                }
                const s_schema_node_descriptor& array = schema->nodes[parent];
                const int64_t offset = static_cast<int64_t>(external_index) -
                    array.array_index_base;
                if (has_index && array.array_element_variable_count != 0 && offset >= 0 &&
                    (array.kind == CONFIG_NODE_VECTOR ||
                        offset < array.array_element_count)) {
                    const uint64_t leaf_offset = static_cast<uint64_t>(offset) *
                        array.array_element_variable_count;
                    if (leaf_offset > UINT32_MAX ||
                        leaf_offset > UINT32_MAX - *variable_offset) {
                        return CONFIG_NO_NODE;
                    }
                    *resolved_index = external_index;
                    if (array.kind == CONFIG_NODE_MAP) {
                        *variable_offset = static_cast<t_variable_id>(offset);
                    } else {
                        *variable_offset += static_cast<t_variable_id>(leaf_offset);
                    }
                    return array.first_child;
                }
            }
            for (uint32_t index = 0; index < schema->node_count; ++index) {
                const s_schema_node_descriptor& node = schema->nodes[index];
                if (node.parent != parent) {
                    continue;
                }
                if (schema->nodes[parent].kind == CONFIG_NODE_ARRAY ||
                    schema->nodes[parent].kind == CONFIG_NODE_MAP ||
                    schema->nodes[parent].kind == CONFIG_NODE_VECTOR) {
                    if (has_index && node.external_index == external_index) {
                        *resolved_index = external_index;
                        return index;
                    }
                } else if (token.kind == CFG_TOKEN_IDENTIFIER && token.value == node.id) {
                    return index;
                }
            }
            return CONFIG_NO_NODE;
        }

        bool Same_Integer_Value(const s_config_write& left,
            const s_config_write& right) {
            return left.value.type == right.value.type &&
                left.value.integer_value == right.value.integer_value;
        }
    } // namespace

    bool Decode_Assignment(const s_cfg_statement* statement, s_config_write_set* write_set, s_cfg_diagnostic* diagnostic) {
        return Decode_Assignment_With_Schema(statement, Get_Runtime_Schema(), write_set, diagnostic);
    }

    bool Decode_Assignment_With_Schema(const s_cfg_statement* statement,
        const s_runtime_schema* schema,
        s_config_write_set* write_set,
        s_cfg_diagnostic* diagnostic) {
        if (statement == nullptr || write_set == nullptr || diagnostic == nullptr) {
            return false;
        }
        write_set->writes.clear();
        *diagnostic = {};
        if (statement->kind != CFG_STATEMENT_ASSIGNMENT) {
            return Fail(statement->location, "expected an assignment statement", write_set, diagnostic);
        }
        if (statement->path.empty()) {
            return Fail(statement->location, "assignment path is empty", write_set, diagnostic);
        }
        if (!statement->groups.empty()) {
            if (!statement->values.empty()) {
                return Fail(statement->location,
                    "assignment cannot mix positional values and labeled groups",
                    write_set,
                    diagnostic);
            }
            s_config_write_set combined;
            for (const s_cfg_assignment_group& group : statement->groups) {
                s_cfg_statement expanded;
                expanded.kind = CFG_STATEMENT_ASSIGNMENT;
                expanded.location = statement->location;
                expanded.statement_number = statement->statement_number;
                expanded.path = statement->path;
                expanded.path.push_back(group.selector);
                expanded.values = group.values;

                s_config_write_set group_writes;
                if (!Decode_Assignment_With_Schema(&expanded, schema, &group_writes, diagnostic)) {
                    write_set->writes.clear();
                    return false;
                }
                for (s_config_write& write : group_writes.writes) {
                    for (const s_config_write& existing : combined.writes) {
                        if (existing.variable_id == write.variable_id &&
                            existing.leaf_index == write.leaf_index) {
                            return Fail(write.provenance,
                                "labeled assignment writes '" +
                                    write.canonical_path + "' more than once",
                                write_set,
                                diagnostic);
                        }
                    }
                    combined.writes.push_back(std::move(write));
                }
            }
            *write_set = std::move(combined);
            return true;
        }

        const s_cfg_token& root = statement->path.front();
        uint32_t selected = CONFIG_NO_NODE;
        for (uint32_t index = 0; index < schema->node_count; ++index) {
            if (schema->nodes[index].parent == CONFIG_NO_NODE &&
                root.value == schema->nodes[index].id) {
                selected = index;
                break;
            }
        }
        std::string canonical_path = root.value;
        t_variable_id selected_variable = CONFIG_NO_VARIABLE;
        t_variable_id variable_offset = 0;
        if (selected == CONFIG_NO_NODE) {
            return Fail(root.location, "unknown configuration variable '" + canonical_path + "'", write_set, diagnostic);
        }
        for (size_t path_index = 1; path_index < statement->path.size(); ++path_index) {
            const s_cfg_token& token = statement->path[path_index];
            if (schema->nodes[selected].kind == CONFIG_NODE_SCALAR) {
                canonical_path += "." + token.value;
                return Fail(root.location, "unknown configuration variable '" + canonical_path + "'", write_set, diagnostic);
            }
            const bool array_selector = schema->nodes[selected].kind == CONFIG_NODE_ARRAY ||
                schema->nodes[selected].kind == CONFIG_NODE_MAP ||
                schema->nodes[selected].kind == CONFIG_NODE_VECTOR;
            int32_t resolved_index = 0;
            const uint32_t parent = selected;
            selected = Find_Child(schema, parent, token, &resolved_index, &variable_offset);
            if (selected == CONFIG_NO_NODE) {
                canonical_path += "." + token.value;
                return Fail(token.location, "unknown configuration variable '" + canonical_path + "'", write_set, diagnostic);
            }
            if (schema->nodes[parent].kind == CONFIG_NODE_MAP) {
                selected_variable = schema->nodes[parent].first_variable + variable_offset;
                variable_offset = 0;
            } else if (schema->nodes[parent].kind == CONFIG_NODE_VECTOR) {
                selected_variable = schema->nodes[parent].first_variable;
            }
            canonical_path += "." + (array_selector ? std::to_string(resolved_index) : token.value);
        }
        if (statement->values.empty()) {
            return Fail(statement->location, "assignment requires a value", write_set, diagnostic);
        }
        const s_schema_node_descriptor& selected_node = schema->nodes[selected];
        if (selected_node.kind == CONFIG_NODE_SCALAR &&
            selected_node.parent != CONFIG_NO_NODE &&
            schema->nodes[selected_node.parent].implicit_terminator) {
            return Fail(statement->path.back().location,
                "implicit lists must be assigned as a complete list",
                write_set,
                diagnostic);
        }
        if (selected_variable == CONFIG_NO_VARIABLE &&
            selected_node.kind == CONFIG_NODE_NAMESPACE) {
            return Fail(statement->path.back().location,
                "configuration namespaces are not assignable",
                write_set,
                diagnostic);
        }
        if (selected_variable == CONFIG_NO_VARIABLE && !selected_node.has_terminator) {
            for (size_t node_index = 0; node_index < schema->node_count; ++node_index) {
                if (schema->nodes[node_index].parent == selected &&
                    schema->nodes[node_index].has_terminator) {
                    return Fail(statement->path.back().location,
                        "terminated arrays must be selected directly",
                        write_set,
                        diagnostic);
                }
            }
        }
        uint32_t boundary = selected;
        if (selected_node.kind == CONFIG_NODE_SCALAR &&
            selected_node.parent != CONFIG_NO_NODE &&
            schema->nodes[selected_node.parent].kind != CONFIG_NODE_NAMESPACE) {
            boundary = selected_node.parent;
        }
        const t_variable_id first_variable = selected_variable != CONFIG_NO_VARIABLE ? selected_variable : selected_node.first_variable + variable_offset;
        const t_variable_id last_variable = selected_variable != CONFIG_NO_VARIABLE ? selected_variable : schema->nodes[boundary].last_variable + variable_offset;
        if (first_variable == CONFIG_NO_VARIABLE || first_variable > last_variable ||
            last_variable >= schema->variable_count) {
            return Fail(root.location, "selected configuration object has no writable values", write_set, diagnostic);
        }

        struct s_write_position {
            t_variable_id variable_id;
            uint32_t leaf_index;
            bool aggregate;
        };
        std::vector<s_write_position> positions;
        size_t available = 0;
        const bool dynamic_vector_boundary = schema->variables != nullptr &&
            first_variable == last_variable &&
            schema->variables[first_variable].vector &&
            schema->nodes[boundary].kind == CONFIG_NODE_VECTOR;
        for (t_variable_id variable_id = first_variable;
             variable_id <= last_variable;
             ++variable_id) {
            const bool aggregate = schema->variables != nullptr &&
                schema->variables[variable_id].aggregate;
            uint32_t first_leaf = 0;
            uint32_t last_leaf = 0;
            if (aggregate) {
                const s_runtime_variable_descriptor& descriptor =
                    schema->variables[variable_id];
                if (variable_id == first_variable &&
                    (selected_variable != CONFIG_NO_VARIABLE ||
                        selected_node.first_variable == selected_node.last_variable)) {
                    first_leaf = selected_node.first_leaf +
                        static_cast<uint32_t>(variable_offset);
                }
                if (dynamic_vector_boundary) {
                    if (statement->values.size() > UINT32_MAX - first_leaf) {
                        return Fail(root.location,
                            "selected vector is too large",
                            write_set,
                            diagnostic);
                    }
                    last_leaf = first_leaf +
                        static_cast<uint32_t>(statement->values.size() - 1);
                } else if (variable_id == last_variable &&
                    selected_variable != CONFIG_NO_VARIABLE) {
                    last_leaf = schema->nodes[boundary].last_leaf +
                        static_cast<uint32_t>(variable_offset);
                } else {
                    const uint64_t leaf_count = descriptor.vector ? static_cast<uint64_t>(descriptor.default_element_count) *
                            descriptor.element_leaf_count
                                                                  : descriptor.element_leaf_count;
                    if (leaf_count == 0 || leaf_count > UINT32_MAX) {
                        return Fail(root.location,
                            "selected configuration object has no writable values",
                            write_set,
                            diagnostic);
                    }
                    last_leaf = static_cast<uint32_t>(leaf_count - 1);
                }
                if (first_leaf > last_leaf) {
                    return Fail(root.location,
                        "selected configuration object has no writable values",
                        write_set,
                        diagnostic);
                }
            }
            const size_t count = static_cast<size_t>(last_leaf - first_leaf) + 1;
            if (count > SIZE_MAX - available) {
                return Fail(root.location,
                    "selected configuration object is too large",
                    write_set,
                    diagnostic);
            }
            available += count;
            const size_t needed = selected_node.implicit_terminator ? available : statement->values.size();
            for (uint32_t leaf = first_leaf;
                 leaf <= last_leaf && positions.size() < needed;
                 ++leaf) {
                positions.push_back({variable_id, leaf, aggregate});
            }
        }
        if (selected_variable == CONFIG_NO_VARIABLE &&
            selected_node.implicit_terminator &&
            statement->values.size() >= available) {
            return Fail(statement->values[available - 1].location,
                "implicit list exceeds its maximum item count",
                write_set,
                diagnostic);
        }
        if (statement->values.size() > available) {
            if ((selected_variable != CONFIG_NO_VARIABLE ||
                    selected_node.kind == CONFIG_NODE_SCALAR) &&
                selected_node.parent == CONFIG_NO_NODE) {
                return Fail(statement->values[available].location,
                    "value " + std::to_string(available + 1) +
                        " exceeds scalar parameter '" + canonical_path + "'",
                    write_set,
                    diagnostic);
            }
            return Fail(statement->values[available].location,
                "value " + std::to_string(available + 1) +
                    " exceeds selected configuration object '" + canonical_path + "'",
                write_set,
                diagnostic);
        }

        bool has_write = false;
        bool has_terminator = false;
        for (size_t value_index = 0; value_index < statement->values.size(); ++value_index) {
            const s_cfg_token& token = statement->values[value_index];
            if (selected_node.implicit_terminator && token.kind == CFG_TOKEN_SKIP) {
                return Fail(token.location, "implicit lists do not support skipped values", write_set, diagnostic);
            }
            if (token.kind == CFG_TOKEN_SKIP) {
                continue;
            }
            const s_write_position& position = positions[value_index];
            const t_variable_id variable_id = position.variable_id;
            const uint32_t leaf_index = position.leaf_index;
            const s_schema_node_descriptor* variable = position.aggregate ? Get_Variable_Leaf_Node(schema, variable_id, leaf_index) : Get_Variable_Node(schema, variable_id);
            if (variable == nullptr || variable->type == nullptr) {
                return Fail(token.location, "configuration schema has an invalid leaf range", write_set, diagnostic);
            }
            s_config_write write;
            write.variable_id = variable_id;
            write.leaf_index = leaf_index;
            if (!(position.aggregate ? Get_Variable_Leaf_Name(schema, variable_id, leaf_index, &write.canonical_path) : Get_Variable_Name(schema, variable_id, &write.canonical_path))) {
                return Fail(token.location, "configuration schema has an invalid leaf path", write_set, diagnostic);
            }
            write.provenance = token.location;
            std::string expected;
            s_variable_descriptor descriptor = {};
            descriptor.name = write.canonical_path.c_str();
            descriptor.type = variable->type;
            if (!Decode_Value(&descriptor, token, &write.value, &expected)) {
                const std::string value = write.canonical_path == "interface.rpc_key" ? "<redacted>" : token.raw;
                return Fail(token.location, "invalid value '" + value + "' for '" + write.canonical_path + "'; expected " + expected, write_set, diagnostic);
            }
            if (selected_node.implicit_terminator &&
                write.value.integer_value == selected_node.terminator) {
                return Fail(token.location,
                    "implicit list terminators cannot be assigned explicitly",
                    write_set,
                    diagnostic);
            }
            if (selected_node.unique_values) {
                for (const s_config_write& existing : write_set->writes) {
                    if (Same_Integer_Value(existing, write)) {
                        return Fail(token.location, "list values must be unique", write_set, diagnostic);
                    }
                }
            }
            has_terminator = has_terminator ||
                (selected_variable == CONFIG_NO_VARIABLE && selected_node.has_terminator &&
                    write.value.integer_value == selected_node.terminator);
            write_set->writes.push_back(std::move(write));
            has_write = true;
        }
        if (selected_variable == CONFIG_NO_VARIABLE && selected_node.implicit_terminator) {
            for (size_t value_index = statement->values.size();
                 value_index < available;
                 ++value_index) {
                const s_write_position& position = positions[value_index];
                s_config_write write;
                write.variable_id = position.variable_id;
                write.leaf_index = position.leaf_index;
                if (!(position.aggregate ? Get_Variable_Leaf_Name(schema, write.variable_id, write.leaf_index, &write.canonical_path) : Get_Variable_Name(schema, write.variable_id, &write.canonical_path))) {
                    return Fail(statement->location,
                        "configuration schema has an invalid implicit list range",
                        write_set,
                        diagnostic);
                }
                const s_schema_node_descriptor* variable =
                    position.aggregate ? Get_Variable_Leaf_Node(schema, write.variable_id, write.leaf_index) : Get_Variable_Node(schema, write.variable_id);
                if (variable == nullptr || variable->type == nullptr) {
                    return Fail(statement->location,
                        "configuration schema has an invalid implicit list leaf",
                        write_set,
                        diagnostic);
                }
                write.value.type = variable->type->value_type;
                write.value.integer_value = selected_node.terminator;
                write.provenance = statement->values.back().location;
                write_set->writes.push_back(std::move(write));
            }
            has_terminator = true;
        }
        if (!has_write) {
            return Fail(statement->values.front().location,
                "an assignment containing only skips has no effect",
                write_set,
                diagnostic);
        }
        if (selected_variable == CONFIG_NO_VARIABLE &&
            selected_node.has_terminator && !has_terminator) {
            return Fail(statement->values.back().location,
                "assignment to '" + canonical_path +
                    "' requires its terminator value",
                write_set,
                diagnostic);
        }
        return true;
    }

} // namespace config