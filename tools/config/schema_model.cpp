#include "schema_model.h"

#include <climits>
#include <numeric>
#include <utility>

#include "json5_reader.h"

namespace config::tool {
    namespace {
        bool Optional_Bool_(const s_json_value* object, const char* name, bool default_value, const std::string& path, bool* output, std::string* diagnostic);

        const s_json_value* Member_(const s_json_value* object, const char* name) {
            if (object->kind != JSON_OBJECT) {
                return nullptr;
            }
            for (size_t index = 0; index < object->member_names.size(); ++index) {
                if (object->member_names[index] == name) {
                    return &object->values[index];
                }
            }
            return nullptr;
        }

        bool Has_Only_Members_(const s_json_value* object, const std::vector<std::string>& allowed, const std::string& path, std::string* diagnostic) {
            if (object->kind != JSON_OBJECT) {
                *diagnostic = path + ": expected an object";
                return false;
            }
            for (const std::string& name : object->member_names) {
                bool found = false;
                for (const std::string& allowed_name : allowed) {
                    if (name == allowed_name) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    *diagnostic = path + ": unknown property '" + name + "'";
                    return false;
                }
            }
            return true;
        }

        bool Nullable_(const s_json_value* declaration, const std::string& path, e_node_kind node_kind, e_primitive_type primitive, e_string_storage storage, bool* nullable, std::string* diagnostic) {
            if (!Optional_Bool_(declaration, "nullable", false, path, nullable, diagnostic)) {
                return false;
            }
            if (*nullable && (node_kind != NODE_SCALAR || primitive != PRIMITIVE_STRING || storage != STRING_STORAGE_ARENA)) {
                *diagnostic = path +
                    ".nullable: requires an arena-backed string leaf";
                return false;
            }
            return true;
        }

        bool String_Storage_(const s_json_value* declaration, const std::string& path, e_node_kind node_kind, e_primitive_type primitive, e_string_storage* storage, std::string* diagnostic) {
            const s_json_value* value = Member_(declaration, "string_storage");
            if (value == nullptr) {
                return true;
            }
            if (node_kind != NODE_SCALAR || primitive != PRIMITIVE_STRING) {
                *diagnostic = path + ".string_storage: requires a string leaf";
                return false;
            }
            if (value->kind != JSON_STRING ||
                (value->string_value != "buffer" && value->string_value != "arena")) {
                *diagnostic = path +
                    ".string_storage: expected 'buffer' or 'arena'";
                return false;
            }
            *storage = value->string_value == "arena" ? STRING_STORAGE_ARENA : STRING_STORAGE_BUFFER;
            return true;
        }

        bool Required_String_(const s_json_value* object, const char* name, const std::string& path, std::string* output, std::string* diagnostic) {
            const s_json_value* value = Member_(object, name);
            if (value == nullptr || value->kind != JSON_STRING || value->string_value.empty()) {
                *diagnostic = path + "." + name + ": expected a non-empty string";
                return false;
            }
            *output = value->string_value;
            return true;
        }

        bool Optional_String_(const s_json_value* object, const char* name, const std::string& path, std::string* output, std::string* diagnostic) {
            const s_json_value* value = Member_(object, name);
            if (value == nullptr) {
                return true;
            }
            if (value->kind != JSON_STRING) {
                *diagnostic = path + "." + name + ": expected a string";
                return false;
            }
            *output = value->string_value;
            return true;
        }

        bool Required_Bool_(const s_json_value* object, const char* name, const std::string& path, bool* output, std::string* diagnostic) {
            const s_json_value* value = Member_(object, name);
            if (value == nullptr || value->kind != JSON_BOOL) {
                *diagnostic = path + "." + name + ": expected a boolean";
                return false;
            }
            *output = value->bool_value;
            return true;
        }

        bool Integer_(const s_json_value* value, int64_t* output) {
            if (value->kind == JSON_SIGNED) {
                *output = value->signed_value;
                return true;
            }
            if (value->kind == JSON_UNSIGNED && value->unsigned_value <= INT64_MAX) {
                *output = static_cast<int64_t>(value->unsigned_value);
                return true;
            }
            return false;
        }

        bool Decode_Utf8_(const std::string& text, size_t* offset, uint32_t* codepoint) {
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

        bool Cp850_Byte_(uint32_t codepoint, uint8_t* byte) {
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
                *byte = static_cast<uint8_t>(codepoint);
                return true;
            }
            for (size_t index = 0; index < sizeof(extended) / sizeof(extended[0]); ++index) {
                if (extended[index] == codepoint) {
                    *byte = static_cast<uint8_t>(0x80 + index);
                    return true;
                }
            }
            return false;
        }

        bool Canonical_Name_(const std::string& name) {
            if (name.empty() || name.front() < 'a' || name.front() > 'z' || name.back() == '_') {
                return false;
            }
            bool underscore = false;
            for (char character : name) {
                if (character == '_') {
                    if (underscore) {
                        return false;
                    }
                    underscore = true;
                } else if ((character < 'a' || character > 'z') &&
                    (character < '0' || character > '9')) {
                    return false;
                } else {
                    underscore = false;
                }
            }
            return true;
        }

        size_t Encoded_Length_(const std::string& value) {
            const std::string encoded = Encode_Game_String(value);
            if (!value.empty() && encoded.empty()) {
                return SIZE_MAX;
            }
            return encoded.size();
        }

        bool Metadata_(const s_json_value* object, const std::string& path, s_metadata* metadata, std::string* diagnostic) {
            if (!Required_String_(object, "title", path, &metadata->title, diagnostic) ||
                !Required_String_(object, "description", path, &metadata->description, diagnostic) ||
                !Optional_String_(object, "remarks", path, &metadata->remarks, diagnostic) ||
                !Optional_String_(object, "units", path, &metadata->units, diagnostic)) {
                return false;
            }
            const s_json_value* examples = Member_(object, "examples");
            if (examples != nullptr) {
                if (examples->kind != JSON_ARRAY) {
                    *diagnostic = path + ".examples: expected an array";
                    return false;
                }
                for (const s_json_value& example : examples->values) {
                    if (example.kind != JSON_STRING) {
                        *diagnostic = path + ".examples: expected strings";
                        return false;
                    }
                    metadata->examples.push_back(example.string_value);
                }
            }
            return true;
        }

        bool Primitive_(const std::string& name, e_primitive_type* type) {
            const char* names[] = {"bool", "int8", "uint8", "int16", "uint16", "int32", "uint32", "string"};
            for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
                if (name == names[index]) {
                    *type = static_cast<e_primitive_type>(index);
                    return true;
                }
            }
            return false;
        }

        void Primitive_Range_(e_primitive_type type, int64_t* minimum, int64_t* maximum) {
            switch (type) {
                case PRIMITIVE_INT8:
                    *minimum = INT8_MIN;
                    *maximum = INT8_MAX;
                    break;
                case PRIMITIVE_UINT8:
                    *minimum = 0;
                    *maximum = UINT8_MAX;
                    break;
                case PRIMITIVE_INT16:
                    *minimum = INT16_MIN;
                    *maximum = INT16_MAX;
                    break;
                case PRIMITIVE_UINT16:
                    *minimum = 0;
                    *maximum = UINT16_MAX;
                    break;
                case PRIMITIVE_INT32:
                    *minimum = INT32_MIN;
                    *maximum = INT32_MAX;
                    break;
                case PRIMITIVE_UINT32:
                    *minimum = 0;
                    *maximum = UINT32_MAX;
                    break;
                default:
                    *minimum = 0;
                    *maximum = 0;
                    break;
            }
        }

        bool Constraints_(const s_json_value* object, const std::string& path, e_primitive_type type, s_constraints* constraints, std::string* diagnostic) {
            if (object == nullptr) {
                return true;
            }
            if (!Has_Only_Members_(object, {"min", "max", "min_length", "max_length", "encoding"}, path, diagnostic)) {
                return false;
            }
            const s_json_value* min = Member_(object, "min");
            const s_json_value* max = Member_(object, "max");
            const s_json_value* min_length = Member_(object, "min_length");
            const s_json_value* max_length = Member_(object, "max_length");
            if (type == PRIMITIVE_STRING) {
                int64_t value = 0;
                if (min != nullptr || max != nullptr) {
                    *diagnostic = path + ": numeric constraints cannot be used with string";
                    return false;
                }
                if (min_length != nullptr) {
                    if (!Integer_(min_length, &value) || value < 0) {
                        *diagnostic = path + ".min_length: expected a non-negative integer";
                        return false;
                    }
                    constraints->has_min_length = true;
                    constraints->min_length = static_cast<size_t>(value);
                }
                if (max_length != nullptr) {
                    if (!Integer_(max_length, &value) || value < 0) {
                        *diagnostic = path + ".max_length: expected a non-negative integer";
                        return false;
                    }
                    constraints->has_max_length = true;
                    constraints->max_length = static_cast<size_t>(value);
                }
                if (!Optional_String_(object, "encoding", path, &constraints->encoding, diagnostic)) {
                    return false;
                }
                if (constraints->encoding.empty()) {
                    constraints->encoding = "cp850";
                } else if (constraints->encoding != "cp850") {
                    *diagnostic = path + ".encoding: only 'cp850' is supported";
                    return false;
                }
                if (constraints->has_min_length && constraints->has_max_length &&
                    constraints->min_length > constraints->max_length) {
                    *diagnostic = path + ": min_length exceeds max_length";
                    return false;
                }
                return true;
            }

            if (min_length != nullptr || max_length != nullptr || Member_(object, "encoding") != nullptr) {
                *diagnostic = path + ": string constraints require type 'string'";
                return false;
            }
            if (type == PRIMITIVE_BOOL && (min != nullptr || max != nullptr)) {
                *diagnostic = path + ": bool does not accept numeric constraints";
                return false;
            }
            int64_t primitive_min = 0;
            int64_t primitive_max = 0;
            Primitive_Range_(type, &primitive_min, &primitive_max);
            if (min != nullptr) {
                if (!Integer_(min, &constraints->min) || constraints->min < primitive_min || constraints->min > primitive_max) {
                    *diagnostic = path + ".min: value is outside the primitive range";
                    return false;
                }
                constraints->has_min = true;
            }
            if (max != nullptr) {
                if (!Integer_(max, &constraints->max) || constraints->max < primitive_min || constraints->max > primitive_max) {
                    *diagnostic = path + ".max: value is outside the primitive range";
                    return false;
                }
                constraints->has_max = true;
            }
            if (constraints->has_min && constraints->has_max && constraints->min > constraints->max) {
                *diagnostic = path + ": min exceeds max";
                return false;
            }
            return true;
        }

        bool Group_(const std::string& id, const s_json_value* value, const std::string& root, s_group* group, std::string* diagnostic) {
            const std::string path = root + ".groups." + id;
            if (!Canonical_Name_(id)) {
                *diagnostic = path + ": group names must use lower_snake_case";
                return false;
            }
            if (!Has_Only_Members_(value, {"title", "description", "remarks", "units", "examples", "save", "synchronize"}, path, diagnostic) ||
                !Metadata_(value, path, &group->metadata, diagnostic) ||
                !Required_Bool_(value, "save", path, &group->save, diagnostic) ||
                !Required_Bool_(value, "synchronize", path, &group->synchronize, diagnostic)) {
                return false;
            }
            group->id = id;
            return true;
        }

        bool Documentation_Categories_(const s_json_value* value, const std::string& root, std::vector<s_documentation_category>* categories, std::string* diagnostic) {
            if (value == nullptr) {
                return true;
            }
            if (value->kind != JSON_OBJECT || value->values.empty()) {
                *diagnostic = root + ".documentation_categories: expected a non-empty object";
                return false;
            }
            std::vector<std::string> assigned_namespaces;
            for (size_t index = 0; index < value->member_names.size(); ++index) {
                const std::string& id = value->member_names[index];
                const s_json_value& declaration = value->values[index];
                const std::string path = root + ".documentation_categories." + id;
                if (!Canonical_Name_(id)) {
                    *diagnostic = path + ": category names must use lower_snake_case";
                    return false;
                }
                s_documentation_category category;
                category.id = id;
                if (!Has_Only_Members_(&declaration,
                        {"title", "description", "namespaces"},
                        path,
                        diagnostic) ||
                    !Required_String_(&declaration, "title", path, &category.metadata.title, diagnostic) ||
                    !Required_String_(&declaration, "description", path, &category.metadata.description, diagnostic)) {
                    return false;
                }
                const s_json_value* namespaces = Member_(&declaration, "namespaces");
                if (namespaces == nullptr || namespaces->kind != JSON_ARRAY ||
                    namespaces->values.empty()) {
                    *diagnostic = path + ".namespaces: expected a non-empty array";
                    return false;
                }
                for (const s_json_value& name_space : namespaces->values) {
                    if (name_space.kind != JSON_STRING ||
                        !Canonical_Name_(name_space.string_value)) {
                        *diagnostic = path +
                            ".namespaces: expected lower_snake_case strings";
                        return false;
                    }
                    for (const std::string& assigned : assigned_namespaces) {
                        if (assigned == name_space.string_value) {
                            *diagnostic = path + ".namespaces: namespace '" + assigned +
                                "' belongs to more than one category";
                            return false;
                        }
                    }
                    assigned_namespaces.push_back(name_space.string_value);
                    category.namespaces.push_back(name_space.string_value);
                }
                categories->push_back(std::move(category));
            }
            return true;
        }

        struct s_resolved_type {
            e_node_kind node_kind = NODE_SCALAR;
            e_primitive_type primitive = PRIMITIVE_BOOL;
            e_scalar_kind scalar_kind = SCALAR_PRIMITIVE;
            s_constraints constraints;
            std::vector<s_symbol> values;
            std::vector<s_symbol> flags;
            std::vector<s_bitmap_symbol> bitmap_values;
            std::vector<s_bitmap_symbol> bitmap_flags;
            std::vector<uint8_t> bitmap_known_bits;
            size_t bitmap_bit_count = 0;
            bool allow_numeric = false;
            bool reject_unknown_numeric = false;
            uint64_t known_bits = 0;
            e_primitive_type ratio_component_type = PRIMITIVE_INT16;
            const s_json_value* aggregate = nullptr;
        };

        bool Optional_Bool_(const s_json_value* object, const char* name, bool default_value, const std::string& path, bool* output, std::string* diagnostic) {
            const s_json_value* value = Member_(object, name);
            if (value == nullptr) {
                *output = default_value;
                return true;
            }
            if (value->kind != JSON_BOOL) {
                *diagnostic = path + "." + name + ": expected a boolean";
                return false;
            }
            *output = value->bool_value;
            return true;
        }

        void Set_Bitmap_Bit_(std::vector<uint8_t>* bitmap, size_t bit) {
            (*bitmap)[bit >> 3] |= static_cast<uint8_t>(1U << (bit & 7));
        }

        const s_bitmap_symbol* Find_Bitmap_Symbol_(
            const std::vector<s_bitmap_symbol>& symbols, const std::string& name) {
            for (const s_bitmap_symbol& symbol : symbols) {
                if (symbol.name == name) {
                    return &symbol;
                }
            }
            return nullptr;
        }

        bool Bitmap_Flags_(const s_json_value* object, const std::string& path, size_t bit_count, std::vector<s_bitmap_symbol>* flags, std::vector<uint8_t>* known_bits, std::string* diagnostic) {
            if (object == nullptr || object->kind != JSON_OBJECT || object->values.empty()) {
                *diagnostic = path + ": bitmap requires a non-empty flag object";
                return false;
            }
            const size_t byte_count = (bit_count + 7) / 8;
            known_bits->assign(byte_count, 0);
            for (size_t index = 0; index < object->member_names.size(); ++index) {
                const std::string& name = object->member_names[index];
                if (!Canonical_Name_(name)) {
                    *diagnostic = path + "." + name + ": names must use lower_snake_case";
                    return false;
                }
                s_bitmap_symbol flag;
                flag.name = name;
                const s_json_value* value = &object->values[index];
                if (value->kind == JSON_OBJECT) {
                    if (!Has_Only_Members_(value, {"bit", "description"}, path + "." + name, diagnostic) ||
                        !Required_String_(value, "description", path + "." + name, &flag.description, diagnostic)) {
                        return false;
                    }
                    value = Member_(value, "bit");
                }
                int64_t bit = 0;
                if (value == nullptr || !Integer_(value, &bit) || bit < 0 ||
                    static_cast<uint64_t>(bit) >= bit_count) {
                    *diagnostic = path + "." + name +
                        ": expected a bit index within the bitmap";
                    return false;
                }
                const size_t byte = static_cast<size_t>(bit) >> 3;
                const uint8_t mask = static_cast<uint8_t>(1U << (bit & 7));
                if (((*known_bits)[byte] & mask) != 0) {
                    *diagnostic = path + "." + name + ": duplicate bit index";
                    return false;
                }
                flag.value.assign(byte_count, 0);
                Set_Bitmap_Bit_(&flag.value, static_cast<size_t>(bit));
                (*known_bits)[byte] |= mask;
                flags->push_back(std::move(flag));
            }
            return true;
        }

        bool Bitmap_Values_(const s_json_value* object, const std::string& path, size_t byte_count, const std::vector<s_bitmap_symbol>& flags, std::vector<s_bitmap_symbol>* values, std::string* diagnostic) {
            if (object == nullptr) {
                return true;
            }
            if (object->kind != JSON_OBJECT) {
                *diagnostic = path + ": expected an object";
                return false;
            }
            for (size_t index = 0; index < object->member_names.size(); ++index) {
                const std::string& name = object->member_names[index];
                if (!Canonical_Name_(name)) {
                    *diagnostic = path + "." + name + ": names must use lower_snake_case";
                    return false;
                }
                s_bitmap_symbol composite;
                composite.name = name;
                composite.value.assign(byte_count, 0);
                const s_json_value* flag_names = &object->values[index];
                if (flag_names->kind == JSON_OBJECT) {
                    if (!Has_Only_Members_(flag_names, {"flags", "description"}, path + "." + name, diagnostic) ||
                        !Required_String_(flag_names, "description", path + "." + name, &composite.description, diagnostic)) {
                        return false;
                    }
                    flag_names = Member_(flag_names, "flags");
                }
                if (flag_names == nullptr || flag_names->kind != JSON_ARRAY) {
                    *diagnostic = path + "." + name + ": expected an array of flag names";
                    return false;
                }
                for (const s_json_value& flag_name : flag_names->values) {
                    if (flag_name.kind != JSON_STRING) {
                        *diagnostic = path + "." + name + ": expected flag-name strings";
                        return false;
                    }
                    const s_bitmap_symbol* flag = Find_Bitmap_Symbol_(flags,
                        flag_name.string_value);
                    if (flag == nullptr) {
                        *diagnostic = path + "." + name + ": unknown bitmap flag '" +
                            flag_name.string_value + "'";
                        return false;
                    }
                    for (size_t byte = 0; byte < byte_count; ++byte) {
                        composite.value[byte] |= flag->value[byte];
                    }
                }
                values->push_back(std::move(composite));
            }
            return true;
        }

        bool Find_Named_(const s_json_value* root, const char* collection, const std::string& name, const s_json_value** value) {
            const s_json_value* object = Member_(root, collection);
            if (object == nullptr || object->kind != JSON_OBJECT) {
                return false;
            }
            *value = Member_(object, name.c_str());
            return *value != nullptr;
        }

        bool Symbols_(const s_json_value* object, const std::string& path, std::vector<s_symbol>* symbols, uint64_t* known_bits, std::string* diagnostic) {
            if (object == nullptr) {
                return true;
            }
            if (object->kind != JSON_OBJECT) {
                *diagnostic = path + ": expected an object";
                return false;
            }
            for (size_t index = 0; index < object->member_names.size(); ++index) {
                const std::string& name = object->member_names[index];
                if (!Canonical_Name_(name)) {
                    *diagnostic = path + "." + name + ": names must use lower_snake_case";
                    return false;
                }
                s_symbol symbol;
                const s_json_value* value = &object->values[index];
                if (value->kind == JSON_OBJECT) {
                    if (!Has_Only_Members_(value, {"value", "description"}, path + "." + name, diagnostic) ||
                        !Required_String_(value, "description", path + "." + name, &symbol.description, diagnostic)) {
                        return false;
                    }
                    value = Member_(value, "value");
                }
                symbol.name = name;
                if (value == nullptr || !Integer_(value, &symbol.value)) {
                    *diagnostic = path + "." + name + ": expected an integer value";
                    return false;
                }
                for (const s_symbol& existing : *symbols) {
                    if (existing.value == symbol.value) {
                        *diagnostic = path + "." + name + ": duplicate numeric value";
                        return false;
                    }
                }
                symbols->push_back(std::move(symbol));
                if (known_bits != nullptr) {
                    *known_bits |= static_cast<uint64_t>(symbols->back().value);
                }
            }
            return true;
        }

        void Merge_Constraints_(const s_constraints& additional, s_constraints* constraints) {
            if (additional.has_min) {
                constraints->has_min = true;
                constraints->min = additional.min;
            }
            if (additional.has_max) {
                constraints->has_max = true;
                constraints->max = additional.max;
            }
            if (additional.has_min_length) {
                constraints->has_min_length = true;
                constraints->min_length = additional.min_length;
            }
            if (additional.has_max_length) {
                constraints->has_max_length = true;
                constraints->max_length = additional.max_length;
            }
            if (!additional.encoding.empty()) {
                constraints->encoding = additional.encoding;
            }
        }

        bool Narrow_Constraints_(const s_constraints& base, const s_constraints& field, const std::string& path, s_constraints* result, std::string* diagnostic) {
            *result = base;
            if ((field.has_min && base.has_min && field.min < base.min) ||
                (field.has_max && base.has_max && field.max > base.max) ||
                (field.has_min_length && base.has_min_length &&
                    field.min_length < base.min_length) ||
                (field.has_max_length && base.has_max_length &&
                    field.max_length > base.max_length)) {
                *diagnostic = path + ": field constraints cannot broaden the referenced type";
                return false;
            }
            Merge_Constraints_(field, result);
            if ((result->has_min && result->has_max && result->min > result->max) ||
                (result->has_min_length && result->has_max_length &&
                    result->min_length > result->max_length)) {
                *diagnostic = path + ": field constraints define an empty range";
                return false;
            }
            return true;
        }

        bool Resolve_Type_(const s_json_value* root, const s_json_value* declaration, const std::string& path, std::vector<std::string>* stack, s_resolved_type* resolved, std::string* diagnostic) {
            if (declaration == nullptr) {
                *diagnostic = path + ": missing type";
                return false;
            }
            if (declaration->kind == JSON_STRING) {
                if (Primitive_(declaration->string_value, &resolved->primitive)) {
                    resolved->node_kind = NODE_SCALAR;
                    resolved->scalar_kind = SCALAR_PRIMITIVE;
                    return true;
                }
                const s_json_value* named = nullptr;
                const char* collection = nullptr;
                if (Find_Named_(root, "types", declaration->string_value, &named)) {
                    collection = "types";
                } else if (Find_Named_(root, "records", declaration->string_value, &named)) {
                    collection = "records";
                } else {
                    *diagnostic = path + ": unknown type '" + declaration->string_value + "'";
                    return false;
                }
                for (const std::string& active : *stack) {
                    if (active == declaration->string_value) {
                        *diagnostic = path + ": recursive value type '" + declaration->string_value + "'";
                        return false;
                    }
                }
                stack->push_back(declaration->string_value);
                bool success = false;
                if (std::string(collection) == "records") {
                    resolved->node_kind = NODE_RECORD;
                    resolved->aggregate = named;
                    success = true;
                } else {
                    success = Resolve_Type_(root, named, path, stack, resolved, diagnostic);
                }
                stack->pop_back();
                return success;
            }
            if (declaration->kind != JSON_OBJECT) {
                *diagnostic = path + ": expected a type name or object";
                return false;
            }
            if (Member_(declaration, "string_storage") != nullptr) {
                *diagnostic = path +
                    ".string_storage: declare string storage on a writable parameter or record field";
                return false;
            }

            const s_json_value* kind_value = Member_(declaration, "kind");
            if (kind_value == nullptr) {
                return Resolve_Type_(root, Member_(declaration, "type"), path + ".type", stack, resolved, diagnostic);
            }
            if (kind_value->kind != JSON_STRING) {
                *diagnostic = path + ".kind: expected a string";
                return false;
            }
            const std::string& kind = kind_value->string_value;
            if (kind == "record") {
                resolved->node_kind = NODE_RECORD;
                resolved->aggregate = declaration;
                return true;
            }
            if (kind == "array" || kind == "map" || kind == "vector") {
                resolved->node_kind = kind == "array" ? NODE_ARRAY : kind == "map" ? NODE_MAP
                                                                                   : NODE_VECTOR;
                resolved->aggregate = declaration;
                return true;
            }
            if (kind == "alias") {
                if (!Resolve_Type_(root, Member_(declaration, "type"), path + ".type", stack, resolved, diagnostic)) {
                    return false;
                }
                s_constraints constraints;
                if (!Constraints_(Member_(declaration, "constraints"), path + ".constraints", resolved->primitive, &constraints, diagnostic)) {
                    return false;
                }
                s_constraints narrowed;
                if (!Narrow_Constraints_(resolved->constraints, constraints, path + ".constraints", &narrowed, diagnostic)) {
                    return false;
                }
                resolved->constraints = narrowed;
                return true;
            }
            if (kind == "enum" || kind == "mask") {
                const s_json_value* base = Member_(declaration, "base_type");
                s_json_value default_base;
                if (base == nullptr && kind == "mask") {
                    default_base.kind = JSON_STRING;
                    default_base.string_value = "uint32";
                    base = &default_base;
                }
                s_resolved_type base_type;
                if (!Resolve_Type_(root, base, path + ".base_type", stack, &base_type, diagnostic) ||
                    base_type.node_kind != NODE_SCALAR ||
                    base_type.primitive == PRIMITIVE_BOOL ||
                    base_type.primitive == PRIMITIVE_STRING ||
                    base_type.primitive == PRIMITIVE_RATIO) {
                    *diagnostic = path + ".base_type: expected an integer primitive or alias";
                    return false;
                }
                resolved->primitive = base_type.primitive;
                resolved->constraints = base_type.constraints;
                resolved->scalar_kind = kind == "enum" ? SCALAR_ENUM : SCALAR_MASK;
                if (!Optional_Bool_(declaration, "allow_numeric", false, path, &resolved->allow_numeric, diagnostic)) {
                    return false;
                }
                const char* reject_name = kind == "enum" ? "reject_unknown_numeric" : "reject_unknown_bits";
                if (!Optional_Bool_(declaration, reject_name, false, path, &resolved->reject_unknown_numeric, diagnostic) ||
                    !Symbols_(Member_(declaration, "values"), path + ".values", &resolved->values, nullptr, diagnostic)) {
                    return false;
                }
                if (kind == "mask" &&
                    !Symbols_(Member_(declaration, "flags"), path + ".flags", &resolved->flags, &resolved->known_bits, diagnostic)) {
                    return false;
                }
                if (kind == "enum" && resolved->values.empty()) {
                    *diagnostic = path + ".values: enum requires at least one value";
                    return false;
                }
                int64_t minimum = 0;
                int64_t maximum = 0;
                Primitive_Range_(resolved->primitive, &minimum, &maximum);
                for (const s_symbol& symbol : resolved->values) {
                    if (symbol.value < minimum || symbol.value > maximum) {
                        *diagnostic = path + ".values." + symbol.name +
                            ": value is outside the base type range";
                        return false;
                    }
                    if ((resolved->constraints.has_min &&
                            symbol.value < resolved->constraints.min) ||
                        (resolved->constraints.has_max &&
                            symbol.value > resolved->constraints.max)) {
                        *diagnostic = path + ".values." + symbol.name +
                            ": value violates base alias constraints";
                        return false;
                    }
                }
                for (const s_symbol& flag : resolved->flags) {
                    const uint64_t bits = static_cast<uint64_t>(flag.value);
                    if (flag.value <= 0 || (bits & (bits - 1)) != 0 ||
                        flag.value < minimum || flag.value > maximum) {
                        *diagnostic = path + ".flags." + flag.name +
                            ": flags must be single positive bits in the base type range";
                        return false;
                    }
                }
                if (kind == "mask") {
                    for (const s_symbol& symbol : resolved->values) {
                        if ((static_cast<uint64_t>(symbol.value) &
                                ~resolved->known_bits) != 0) {
                            *diagnostic = path + ".values." + symbol.name +
                                ": composite contains unknown bits";
                            return false;
                        }
                    }
                }
                return true;
            }
            if (kind == "bitmap") {
                int64_t bit_count = 0;
                const s_json_value* bit_count_value = Member_(declaration, "bit_count");
                if (bit_count_value == nullptr || !Integer_(bit_count_value, &bit_count) ||
                    bit_count <= 0 || static_cast<uint64_t>(bit_count) > SIZE_MAX - 7) {
                    *diagnostic = path + ".bit_count: expected a positive bitmap size";
                    return false;
                }
                resolved->primitive = PRIMITIVE_BITMAP;
                resolved->scalar_kind = SCALAR_BITMAP;
                resolved->bitmap_bit_count = static_cast<size_t>(bit_count);
                if (!Bitmap_Flags_(Member_(declaration, "flags"), path + ".flags", resolved->bitmap_bit_count, &resolved->bitmap_flags, &resolved->bitmap_known_bits, diagnostic) ||
                    !Bitmap_Values_(Member_(declaration, "values"), path + ".values", (resolved->bitmap_bit_count + 7) / 8, resolved->bitmap_flags, &resolved->bitmap_values, diagnostic)) {
                    return false;
                }
                return true;
            }
            if (kind == "ratio") {
                const s_json_value* component = Member_(declaration, "component_type");
                s_resolved_type component_type;
                if (!Resolve_Type_(root, component, path + ".component_type", stack, &component_type, diagnostic) ||
                    component_type.node_kind != NODE_SCALAR ||
                    component_type.primitive == PRIMITIVE_BOOL ||
                    component_type.primitive == PRIMITIVE_STRING ||
                    component_type.primitive == PRIMITIVE_RATIO) {
                    *diagnostic = path + ".component_type: expected an integer primitive";
                    return false;
                }
                resolved->primitive = PRIMITIVE_RATIO;
                resolved->scalar_kind = SCALAR_RATIO;
                resolved->ratio_component_type = component_type.primitive;
                return true;
            }
            *diagnostic = path + ".kind: unsupported type kind '" + kind + "'";
            return false;
        }

        const s_json_value* Object_Member_(const s_json_value* object, const std::string& name) {
            return object != nullptr && object->kind == JSON_OBJECT ? Member_(object, name.c_str()) : nullptr;
        }

        bool Parse_Decimal_(const std::string& text, int64_t* value) {
            if (text.empty()) {
                return false;
            }
            size_t offset = text.front() == '-' || text.front() == '+' ? 1 : 0;
            if (offset == text.size()) {
                return false;
            }
            uint64_t magnitude = 0;
            for (; offset < text.size(); ++offset) {
                if (text[offset] < '0' || text[offset] > '9') {
                    return false;
                }
                const uint32_t digit = static_cast<uint32_t>(text[offset] - '0');
                if (magnitude > (UINT64_C(9223372036854775807) - digit) / 10) {
                    return false;
                }
                magnitude = magnitude * 10 + digit;
            }
            *value = text.front() == '-' ? -static_cast<int64_t>(magnitude) : static_cast<int64_t>(magnitude);
            return true;
        }

        const s_symbol* Find_Symbol_(const std::vector<s_symbol>& symbols,
            const std::string& name) {
            for (const s_symbol& symbol : symbols) {
                if (symbol.name == name) {
                    return &symbol;
                }
            }
            return nullptr;
        }

        bool Decode_Default_(const s_resolved_type& type, const s_json_value* value, const std::string& path, s_parameter* parameter, std::string* diagnostic) {
            if (value == nullptr) {
                *diagnostic = path + ": every writable leaf requires a default";
                return false;
            }
            if (type.scalar_kind == SCALAR_ENUM) {
                if (value->kind == JSON_STRING) {
                    const s_symbol* symbol = Find_Symbol_(type.values, value->string_value);
                    if (symbol == nullptr) {
                        *diagnostic = path + ": unknown enum value '" + value->string_value + "'";
                        return false;
                    }
                    parameter->default_integer = symbol->value;
                    return true;
                }
                if (!type.allow_numeric) {
                    *diagnostic = path + ": numeric enum defaults are not allowed";
                    return false;
                }
            } else if (type.scalar_kind == SCALAR_MASK) {
                if (value->kind == JSON_STRING) {
                    const s_symbol* composite = Find_Symbol_(type.values, value->string_value);
                    if (composite != nullptr) {
                        parameter->default_integer = composite->value;
                        return true;
                    }
                    size_t start = 0;
                    while (start < value->string_value.size()) {
                        const size_t comma = value->string_value.find(',', start);
                        const size_t end = comma == std::string::npos ? value->string_value.size() : comma;
                        const s_symbol* flag = Find_Symbol_(
                            type.flags, value->string_value.substr(start, end - start));
                        if (flag == nullptr) {
                            *diagnostic = path + ": unknown mask flag";
                            return false;
                        }
                        parameter->default_integer |= flag->value;
                        start = end + 1;
                    }
                    return true;
                }
                if (!type.allow_numeric) {
                    *diagnostic = path + ": numeric mask defaults are not allowed";
                    return false;
                }
            } else if (type.scalar_kind == SCALAR_BITMAP) {
                if (value->kind != JSON_STRING) {
                    *diagnostic = path + ": bitmap defaults must use named flags";
                    return false;
                }
                const size_t byte_count = (type.bitmap_bit_count + 7) / 8;
                parameter->default_bitmap.assign(byte_count, 0);
                const s_bitmap_symbol* composite = Find_Bitmap_Symbol_(
                    type.bitmap_values, value->string_value);
                if (composite != nullptr) {
                    parameter->default_bitmap = composite->value;
                    return true;
                }
                size_t start = 0;
                while (start < value->string_value.size()) {
                    const size_t comma = value->string_value.find(',', start);
                    const size_t end = comma == std::string::npos ? value->string_value.size() : comma;
                    const s_bitmap_symbol* flag = Find_Bitmap_Symbol_(
                        type.bitmap_flags, value->string_value.substr(start, end - start));
                    if (flag == nullptr) {
                        *diagnostic = path + ": unknown bitmap flag";
                        return false;
                    }
                    for (size_t byte = 0; byte < byte_count; ++byte) {
                        parameter->default_bitmap[byte] |= flag->value[byte];
                    }
                    start = end + 1;
                }
                return true;
            } else if (type.scalar_kind == SCALAR_RATIO) {
                if (value->kind == JSON_STRING) {
                    const size_t slash = value->string_value.find('/');
                    if (slash == std::string::npos ||
                        !Parse_Decimal_(value->string_value.substr(0, slash),
                            &parameter->default_ratio_numerator) ||
                        !Parse_Decimal_(value->string_value.substr(slash + 1),
                            &parameter->default_ratio_denominator) ||
                        parameter->default_ratio_denominator == 0) {
                        *diagnostic = path + ": invalid ratio default";
                        return false;
                    }
                } else if (Integer_(value, &parameter->default_ratio_numerator)) {
                    parameter->default_ratio_denominator = 1;
                } else {
                    *diagnostic = path + ": expected a ratio string or integer";
                    return false;
                }
                if (parameter->default_ratio_denominator < 0) {
                    parameter->default_ratio_numerator = -parameter->default_ratio_numerator;
                    parameter->default_ratio_denominator = -parameter->default_ratio_denominator;
                }
                const int64_t divisor = std::gcd(parameter->default_ratio_numerator,
                    parameter->default_ratio_denominator);
                parameter->default_ratio_numerator /= divisor;
                parameter->default_ratio_denominator /= divisor;
                int64_t minimum = 0;
                int64_t maximum = 0;
                Primitive_Range_(type.ratio_component_type, &minimum, &maximum);
                if (parameter->default_ratio_numerator < minimum ||
                    parameter->default_ratio_numerator > maximum ||
                    parameter->default_ratio_denominator < minimum ||
                    parameter->default_ratio_denominator > maximum) {
                    *diagnostic = path + ": normalized ratio components exceed their type";
                    return false;
                }
                return true;
            }

            if (type.primitive == PRIMITIVE_BOOL) {
                if (value->kind != JSON_BOOL) {
                    *diagnostic = path + ": expected a boolean";
                    return false;
                }
                parameter->default_bool = value->bool_value;
                return true;
            }
            if (type.primitive == PRIMITIVE_STRING) {
                if (value->kind != JSON_STRING) {
                    *diagnostic = path + ": expected a string";
                    return false;
                }
                const size_t length = Encoded_Length_(value->string_value);
                if (length == SIZE_MAX) {
                    *diagnostic = path + ": string contains text not representable in CP850";
                    return false;
                }
                if ((type.constraints.has_min_length && length < type.constraints.min_length) ||
                    (type.constraints.has_max_length && length > type.constraints.max_length)) {
                    *diagnostic = path + ": string violates length constraints";
                    return false;
                }
                parameter->default_string = value->string_value;
                return true;
            }
            if (!Integer_(value, &parameter->default_integer)) {
                *diagnostic = path + ": expected an integer";
                return false;
            }
            int64_t minimum = 0;
            int64_t maximum = 0;
            Primitive_Range_(type.primitive, &minimum, &maximum);
            if ((type.constraints.has_min && parameter->default_integer < type.constraints.min) ||
                (type.constraints.has_max && parameter->default_integer > type.constraints.max) ||
                parameter->default_integer < minimum || parameter->default_integer > maximum) {
                *diagnostic = path + ": integer violates type or field constraints";
                return false;
            }
            if ((type.scalar_kind == SCALAR_ENUM && type.reject_unknown_numeric &&
                    Find_Symbol_(type.values, std::to_string(parameter->default_integer)) == nullptr)) {
                bool found = false;
                for (const s_symbol& symbol : type.values) {
                    found = found || symbol.value == parameter->default_integer;
                }
                if (!found) {
                    *diagnostic = path + ": unknown numeric enum default";
                    return false;
                }
            }
            if (type.scalar_kind == SCALAR_MASK && type.reject_unknown_numeric &&
                (static_cast<uint64_t>(parameter->default_integer) & ~type.known_bits) != 0) {
                *diagnostic = path + ": mask default contains unknown bits";
                return false;
            }
            return true;
        }

        bool Binding_Members_(const s_json_value* declaration, const std::string& path, std::vector<std::string>* members, std::string* diagnostic) {
            const s_json_value* binding = Member_(declaration, "bind");
            if (binding == nullptr) {
                return true;
            }
            if (binding->kind != JSON_ARRAY || binding->values.empty()) {
                *diagnostic = path + ".bind: expected a non-empty member-path array";
                return false;
            }
            for (const s_json_value& member : binding->values) {
                if (member.kind != JSON_STRING || member.string_value.empty()) {
                    *diagnostic = path + ".bind: expected non-empty member paths";
                    return false;
                }
                members->push_back(member.string_value);
            }
            return true;
        }

        std::vector<std::string> Append_Bindings_(const std::vector<std::string>& bases,
            const std::vector<std::string>& members) {
            if (members.empty()) {
                return bases;
            }
            std::vector<std::string> targets;
            for (const std::string& base : bases) {
                for (const std::string& member : members) {
                    targets.push_back(base + "." + member);
                }
            }
            return targets;
        }

        std::vector<s_native_target_expression> Append_Binding_Expressions_(
            const std::vector<s_native_target_expression>& bases,
            const std::vector<std::string>& members) {
            if (members.empty()) {
                return bases;
            }
            std::vector<s_native_target_expression> targets;
            for (const s_native_target_expression& base : bases) {
                for (const std::string& member : members) {
                    s_native_target_expression target = base;
                    target.components.push_back({false, member, UINT32_MAX, 0});
                    targets.push_back(std::move(target));
                }
            }
            return targets;
        }

        bool Resolve_Group_(const s_json_value* declaration,
            const std::string& inherited_group,
            const std::string& path,
            const s_schema* schema,
            std::string* group,
            std::string* diagnostic) {
            *group = inherited_group;
            if (!Optional_String_(declaration, "group", path, group, diagnostic)) {
                return false;
            }
            for (const s_group& candidate : schema->groups) {
                if (candidate.id == *group) {
                    return true;
                }
            }
            *diagnostic = path + ".group: unknown group '" + *group + "'";
            return false;
        }

        bool Build_Node_(const s_json_value* root, const std::string& id, const s_json_value* declaration, const s_json_value* inherited_default, const std::string& canonical_path, const std::string& schema_path, const std::string& group, const s_metadata& inherited_metadata, const s_binding& root_binding, const std::vector<std::string>& bases, const std::vector<s_native_target_expression>& base_expressions, uint32_t parent, int32_t external_index, bool default_from_native, bool parameter_root, std::vector<const s_json_value*>* aggregate_stack, s_schema* schema, std::string* diagnostic) {
            std::string effective_group;
            if (!Resolve_Group_(declaration, group, schema_path, schema, &effective_group, diagnostic)) {
                return false;
            }
            std::vector<std::string> stack;
            s_resolved_type type;
            const s_json_value* type_declaration = Member_(declaration, "type");
            if (!Resolve_Type_(root, type_declaration, schema_path + ".type", &stack, &type, diagnostic)) {
                return false;
            }
            if (!parameter_root && Member_(declaration, "default_source") != nullptr) {
                *diagnostic = schema_path +
                    ".default_source: accepted only on a top-level parameter";
                return false;
            }
            if (type.node_kind != NODE_SCALAR) {
                for (const s_json_value* active : *aggregate_stack) {
                    if (active == type.aggregate) {
                        *diagnostic = schema_path + ": recursive aggregate value type";
                        return false;
                    }
                }
                if (aggregate_stack->size() >= 64) {
                    *diagnostic = schema_path + ": aggregate nesting exceeds 64 levels";
                    return false;
                }
                aggregate_stack->push_back(type.aggregate);
            }
            s_constraints field_constraints;
            if (type.node_kind == NODE_SCALAR &&
                !Constraints_(Member_(declaration, "constraints"), schema_path + ".constraints", type.primitive, &field_constraints, diagnostic)) {
                return false;
            }
            s_constraints effective_constraints;
            if (!Narrow_Constraints_(type.constraints, field_constraints, schema_path + ".constraints", &effective_constraints, diagnostic)) {
                return false;
            }
            type.constraints = effective_constraints;

            std::vector<std::string> members;
            if (!Binding_Members_(declaration, schema_path, &members, diagnostic)) {
                return false;
            }
            const std::vector<std::string> targets = Append_Bindings_(bases, members);
            const std::vector<s_native_target_expression> target_expressions =
                Append_Binding_Expressions_(base_expressions, members);
            const uint32_t node_index = static_cast<uint32_t>(schema->nodes.size());
            s_node node;
            node.id = id;
            node.path = canonical_path;
            node.kind = type.node_kind;
            node.parent = parent;
            node.external_index = external_index;
            schema->nodes.push_back(std::move(node));

            const s_json_value* declared_default = Member_(declaration, "default");
            if (default_from_native && declared_default != nullptr) {
                *diagnostic = schema_path +
                    ": default_source cannot be combined with default";
                return false;
            }
            if (inherited_default != nullptr && declared_default != nullptr) {
                *diagnostic = schema_path + ".default: aggregate and descendant defaults overlap";
                return false;
            }
            const s_json_value* local_default = inherited_default != nullptr ? inherited_default : declared_default;
            if (type.node_kind == NODE_SCALAR) {
                if (targets.empty()) {
                    *diagnostic = schema_path + ".bind: writable leaf has no native binding";
                    return false;
                }
                s_parameter parameter;
                parameter.path = canonical_path;
                parameter.metadata = inherited_metadata;
                if (!Optional_String_(declaration, "title", schema_path, &parameter.metadata.title, diagnostic) ||
                    !Optional_String_(declaration, "description", schema_path, &parameter.metadata.description, diagnostic) ||
                    !Optional_String_(declaration, "remarks", schema_path, &parameter.metadata.remarks, diagnostic) ||
                    !Optional_String_(declaration, "units", schema_path, &parameter.metadata.units, diagnostic)) {
                    return false;
                }
                parameter.group = effective_group;
                parameter.type = type.primitive;
                parameter.scalar_kind = type.scalar_kind;
                parameter.constraints = type.constraints;
                parameter.values = type.values;
                parameter.flags = type.flags;
                parameter.bitmap_values = type.bitmap_values;
                parameter.bitmap_flags = type.bitmap_flags;
                parameter.bitmap_known_bits = type.bitmap_known_bits;
                parameter.bitmap_bit_count = type.bitmap_bit_count;
                parameter.allow_numeric = type.allow_numeric;
                parameter.reject_unknown_numeric = type.reject_unknown_numeric;
                parameter.known_bits = type.known_bits;
                parameter.ratio_component_type = type.ratio_component_type;
                if (!String_Storage_(declaration, schema_path, type.node_kind, type.primitive, &parameter.string_storage, diagnostic)) {
                    return false;
                }
                if (!Nullable_(declaration, schema_path, type.node_kind, type.primitive, parameter.string_storage, &parameter.nullable, diagnostic)) {
                    return false;
                }
                parameter.default_from_native = default_from_native;
                parameter.binding = root_binding;
                parameter.native_targets = targets;
                parameter.native_target_expressions = target_expressions;
                if (type.scalar_kind == SCALAR_RATIO && targets.size() != 2) {
                    *diagnostic = schema_path +
                        ".bind: ratio leaves require numerator and denominator targets";
                    return false;
                }
                if (!default_from_native &&
                    !Decode_Default_(type, local_default, schema_path + ".default", &parameter, diagnostic)) {
                    return false;
                }
                const uint32_t parameter_index = static_cast<uint32_t>(schema->parameters.size());
                schema->parameters.push_back(std::move(parameter));
                schema->nodes[node_index].first_parameter = parameter_index;
                schema->nodes[node_index].last_parameter = parameter_index;
                schema->nodes[node_index].parameter = parameter_index;
                return true;
            }

            if (type.node_kind == NODE_RECORD) {
                const s_json_value* fields = Member_(type.aggregate, "fields");
                if (fields == nullptr || fields->kind != JSON_ARRAY || fields->values.empty()) {
                    *diagnostic = schema_path + ".fields: record requires a non-empty field array";
                    return false;
                }
                if (local_default != nullptr && local_default->kind != JSON_OBJECT) {
                    *diagnostic = schema_path + ".default: record default must be an object";
                    return false;
                }
                std::vector<std::string> field_ids;
                for (const s_json_value& field : fields->values) {
                    std::string field_id;
                    if (field.kind != JSON_OBJECT ||
                        !Required_String_(&field, "id", schema_path + ".fields", &field_id, diagnostic) ||
                        !Canonical_Name_(field_id)) {
                        *diagnostic = schema_path + ".fields: invalid field declaration";
                        return false;
                    }
                    for (const std::string& existing : field_ids) {
                        if (existing == field_id) {
                            *diagnostic = schema_path + ".fields." + field_id +
                                ": duplicate field ID";
                            return false;
                        }
                    }
                    field_ids.push_back(field_id);
                    const s_json_value* field_default = Object_Member_(local_default, field_id);
                    if (local_default != nullptr && field_default == nullptr) {
                        *diagnostic = schema_path + ".default: record default is missing field '" +
                            field_id + "'";
                        return false;
                    }
                    if (!Build_Node_(root, field_id, &field, field_default, canonical_path + "." + field_id, schema_path + ".fields." + field_id, effective_group, inherited_metadata, root_binding, targets, target_expressions, node_index, 0, default_from_native, false, aggregate_stack, schema, diagnostic)) {
                        return false;
                    }
                }
                if (local_default != nullptr) {
                    for (const std::string& default_name : local_default->member_names) {
                        bool found = false;
                        for (const std::string& field_id : field_ids) {
                            found = found || default_name == field_id;
                        }
                        if (!found) {
                            *diagnostic = schema_path + ".default: unknown record field '" +
                                default_name + "'";
                            return false;
                        }
                    }
                }
            } else {
                int64_t count = 0;
                int64_t index_base = type.node_kind == NODE_VECTOR ? 0 : 1;
                const s_json_value* count_value = Member_(type.aggregate, "count");
                const s_json_value* index_value = Member_(type.aggregate, "index_base");
                const s_json_value* index_type_value = Member_(type.aggregate, "index_type");
                const s_json_value* element = Member_(type.aggregate, "element");
                const s_json_value* fill = Member_(type.aggregate, "fill");
                const s_json_value* terminator = Member_(type.aggregate, "terminator");
                if (!Optional_Bool_(type.aggregate, "implicit_terminator", false, schema_path, &schema->nodes[node_index].implicit_terminator, diagnostic) ||
                    !Optional_Bool_(type.aggregate, "unique", false, schema_path, &schema->nodes[node_index].unique_values, diagnostic)) {
                    return false;
                }
                if (count_value == nullptr || !Integer_(count_value, &count) ||
                    count < (type.node_kind == NODE_VECTOR ? 0 : 1) ||
                    count > INT32_MAX || element == nullptr || element->kind != JSON_OBJECT ||
                    (index_value != nullptr && !Integer_(index_value, &index_base)) ||
                    index_base < INT32_MIN || index_base > INT32_MAX ||
                    index_base + count - 1 > INT32_MAX) {
                    *diagnostic = schema_path + ": invalid array count, index base, or element";
                    return false;
                }
                if (type.node_kind == NODE_VECTOR && index_base != 0) {
                    *diagnostic = schema_path + ".index_base: vectors use zero-based indexes";
                    return false;
                }
                if (type.node_kind == NODE_VECTOR && fill == nullptr &&
                    !default_from_native) {
                    *diagnostic = schema_path +
                        ".fill: vectors require a default element for automatic growth";
                    return false;
                }
                if (type.node_kind == NODE_VECTOR && default_from_native) {
                    *diagnostic = schema_path +
                        ".default_source: vectors require logical fill defaults for automatic growth";
                    return false;
                }
                std::vector<std::string> element_stack;
                s_resolved_type element_type;
                if (!Resolve_Type_(root, Member_(element, "type"), schema_path + ".element.type", &element_stack, &element_type, diagnostic)) {
                    return false;
                }
                schema->nodes[node_index].array_count = count;
                schema->nodes[node_index].array_index_base = index_base;
                schema->nodes[node_index].explicit_default_count =
                    local_default != nullptr ? local_default->values.size() : 0;
                schema->nodes[node_index].native_targets = targets;
                schema->nodes[node_index].native_target_expressions = target_expressions;
                if (index_type_value != nullptr) {
                    if (index_type_value->kind != JSON_STRING) {
                        *diagnostic = schema_path + ".index_type: expected an enum type name";
                        return false;
                    }
                    const s_json_value* index_type_declaration = nullptr;
                    if (!Find_Named_(root, "types", index_type_value->string_value, &index_type_declaration)) {
                        *diagnostic = schema_path + ".index_type: unknown type '" +
                            index_type_value->string_value + "'";
                        return false;
                    }
                    std::vector<std::string> index_stack;
                    s_resolved_type index_type;
                    if (!Resolve_Type_(root, index_type_declaration, schema_path + ".index_type", &index_stack, &index_type, diagnostic)) {
                        return false;
                    }
                    if (index_type.node_kind != NODE_SCALAR ||
                        index_type.scalar_kind != SCALAR_ENUM) {
                        *diagnostic = schema_path + ".index_type: expected an enum type";
                        return false;
                    }
                    const int64_t last_index = index_base + count - 1;
                    for (const s_symbol& alias : index_type.values) {
                        if (alias.value < index_base || alias.value > last_index) {
                            *diagnostic = schema_path + ".index_type." + alias.name +
                                ": value is outside the array's external index range";
                            return false;
                        }
                    }
                    schema->nodes[node_index].index_aliases = index_type.values;
                }
                if (local_default != nullptr) {
                    if (local_default->kind != JSON_ARRAY ||
                        (type.node_kind == NODE_VECTOR ? local_default->values.size() != static_cast<size_t>(count) : local_default->values.size() > static_cast<size_t>(count)) ||
                        (type.node_kind != NODE_VECTOR && fill == nullptr &&
                            local_default->values.size() != static_cast<size_t>(count))) {
                        *diagnostic = schema_path + ".default: collection default has the wrong length";
                        return false;
                    }
                } else if (fill == nullptr && !default_from_native) {
                    *diagnostic = schema_path + ".default: collection requires an exact default or fill value";
                    return false;
                }
                if (terminator != nullptr) {
                    s_parameter decoded;
                    if (element_type.node_kind != NODE_SCALAR ||
                        element_type.primitive == PRIMITIVE_BOOL ||
                        element_type.primitive == PRIMITIVE_STRING ||
                        element_type.primitive == PRIMITIVE_BITMAP ||
                        element_type.primitive == PRIMITIVE_RATIO ||
                        !Decode_Default_(element_type, terminator, schema_path + ".terminator", &decoded, diagnostic)) {
                        if (diagnostic->empty()) {
                            *diagnostic = schema_path +
                                ".terminator: requires an integer array element";
                        }
                        return false;
                    }
                    schema->nodes[node_index].has_terminator = true;
                    schema->nodes[node_index].terminator = decoded.default_integer;
                }
                if (schema->nodes[node_index].implicit_terminator) {
                    if (terminator == nullptr || fill == nullptr ||
                        element_type.node_kind != NODE_SCALAR ||
                        element_type.primitive == PRIMITIVE_BOOL ||
                        element_type.primitive == PRIMITIVE_STRING ||
                        element_type.primitive == PRIMITIVE_BITMAP ||
                        element_type.primitive == PRIMITIVE_RATIO) {
                        *diagnostic = schema_path +
                            ": implicit terminators require a filled integer list with a terminator";
                        return false;
                    }
                    s_parameter decoded_fill;
                    if (!Decode_Default_(element_type, fill, schema_path + ".fill", &decoded_fill, diagnostic) ||
                        decoded_fill.default_integer != schema->nodes[node_index].terminator) {
                        *diagnostic = schema_path +
                            ": implicit list fill must equal its terminator";
                        return false;
                    }
                }
                const int64_t materialized_count = count +
                    (type.node_kind == NODE_VECTOR ? 1 : 0);
                for (int64_t index = 0; index < materialized_count; ++index) {
                    const int64_t external = index_base + index;
                    std::vector<std::string> element_targets;
                    for (const std::string& target : targets) {
                        element_targets.push_back(target + "[" + std::to_string(index) + "]");
                    }
                    std::vector<s_native_target_expression> element_target_expressions =
                        target_expressions;
                    for (s_native_target_expression& target : element_target_expressions) {
                        target.components.push_back(
                            {true, {}, node_index, index});
                    }
                    const s_json_value* element_default =
                        local_default != nullptr &&
                            static_cast<size_t>(index) < local_default->values.size()
                        ? &local_default->values[static_cast<size_t>(index)]
                        : fill;
                    if (!Build_Node_(root, "", element, element_default, canonical_path + "." + std::to_string(external), schema_path + ".element", effective_group, inherited_metadata, root_binding, element_targets, element_target_expressions, node_index, (int32_t)external, default_from_native, false, aggregate_stack, schema, diagnostic)) {
                        return false;
                    }
                }
            }
            aggregate_stack->pop_back();
            if (schema->parameters.empty()) {
                *diagnostic = schema_path + ": aggregate has no writable leaves";
                return false;
            }
            schema->nodes[node_index].first_parameter =
                schema->nodes[node_index + 1].first_parameter;
            schema->nodes[node_index].last_parameter =
                static_cast<uint32_t>(schema->parameters.size() - 1);
            return true;
        }

        bool Rich_Parameter_(const s_json_value* root, const std::string& id, const s_json_value* value, const std::string& root_path, uint32_t parent, const std::string& name_space, s_schema* schema, std::string* diagnostic) {
            const std::string path = root_path + ".parameters." + id;
            s_metadata metadata;
            std::string group;
            if (!Canonical_Name_(id)) {
                *diagnostic = path + ": parameter names must use lower_snake_case";
                return false;
            }
            if (!Has_Only_Members_(value,
                    {"title", "description", "remarks", "units", "examples", "group", "namespace", "type", "constraints", "default", "default_source", "include_default_configuration", "bind_root", "bind", "string_storage", "nullable"},
                    path,
                    diagnostic) ||
                !Metadata_(value, path, &metadata, diagnostic) ||
                !Required_String_(value, "group", path, &group, diagnostic)) {
                return false;
            }
            bool default_from_native = false;
            const s_json_value* default_source = Member_(value, "default_source");
            if (default_source != nullptr) {
                if (default_source->kind != JSON_STRING ||
                    default_source->string_value != "native") {
                    *diagnostic = path +
                        ".default_source: expected exactly 'native'";
                    return false;
                }
                if (Member_(value, "default") != nullptr) {
                    *diagnostic = path +
                        ": default_source cannot be combined with default";
                    return false;
                }
                default_from_native = true;
            }
            s_binding binding;
            const s_json_value* bind_root = Member_(value, "bind_root");
            if (bind_root == nullptr ||
                !Has_Only_Members_(bind_root, {"header", "symbol"}, path + ".bind_root", diagnostic) ||
                !Required_String_(bind_root, "header", path + ".bind_root", &binding.header, diagnostic) ||
                !Required_String_(bind_root, "symbol", path + ".bind_root", &binding.symbol, diagnostic)) {
                return false;
            }
            std::vector<std::string> bases = {binding.symbol};
            std::vector<s_native_target_expression> base_expressions = {
                {binding.symbol, {}},
            };
            std::vector<const s_json_value*> aggregate_stack;
            const std::string canonical_path = name_space.empty() ? id : name_space + "." + id;
            return Build_Node_(root, id, value, nullptr, canonical_path, path, group, metadata, binding, bases, base_expressions, parent, 0, default_from_native, true, &aggregate_stack, schema, diagnostic);
        }

        void Assign_Aggregate_Subtree_(uint32_t node_index,
            const std::vector<std::vector<uint32_t>>& children,
            uint32_t parameter,
            uint32_t first_leaf,
            std::vector<uint8_t>* node_states,
            s_schema* schema) {
            s_node& node = schema->nodes[node_index];
            const uint32_t old_first = node.first_parameter;
            node.first_leaf = first_leaf;
            node.last_leaf = first_leaf + node.last_parameter - old_first;
            node.leaf_index = node.kind == NODE_SCALAR ? first_leaf : 0;
            node.first_parameter = parameter;
            node.last_parameter = parameter;
            node.parameter = node.kind == NODE_SCALAR ? parameter : UINT32_MAX;
            for (uint32_t child : children[node_index]) {
                const uint32_t child_first = schema->nodes[child].first_parameter;
                Assign_Aggregate_Subtree_(child, children, parameter, first_leaf + child_first - old_first, node_states, schema);
            }
            (*node_states)[node_index] = 2;
        }

        bool Normalize_Node_(uint32_t node_index,
            const std::vector<std::vector<uint32_t>>& children,
            std::vector<uint8_t>* node_states,
            std::vector<bool>* parameter_seen,
            std::vector<s_parameter>* old_parameters,
            s_schema* schema,
            std::string* diagnostic) {
            if ((*node_states)[node_index] != 0) {
                *diagnostic = "configuration schema node graph is cyclic";
                return false;
            }
            (*node_states)[node_index] = 1;
            s_node& node = schema->nodes[node_index];
            node.first_parameter = static_cast<uint32_t>(schema->parameters.size());
            if (node.kind == NODE_SCALAR) {
                if (!children[node_index].empty() || node.parameter >= old_parameters->size() ||
                    (*parameter_seen)[node.parameter]) {
                    *diagnostic = "configuration schema scalar has an invalid parameter mapping";
                    return false;
                }
                (*parameter_seen)[node.parameter] = true;
                schema->parameters.push_back(std::move((*old_parameters)[node.parameter]));
                node.parameter = node.first_parameter;
                node.last_parameter = node.first_parameter;
                node.first_leaf = 0;
                node.last_leaf = 0;
                node.leaf_index = 0;
                schema->parameters.back().node = node_index;
            } else if (node.kind == NODE_MAP || node.kind == NODE_VECTOR) {
                if (children[node_index].empty()) {
                    *diagnostic = "configuration schema aggregate has no writable descendants";
                    return false;
                }
                const s_node& first_element_node = schema->nodes[children[node_index].front()];
                const uint32_t element_leaf_count =
                    first_element_node.last_parameter - first_element_node.first_parameter + 1;
                const size_t parameter_count = node.kind == NODE_MAP ? children[node_index].size() : 1;
                node.first_parameter = static_cast<uint32_t>(schema->parameters.size());
                for (size_t element = 0; element < parameter_count; ++element) {
                    const s_node& element_node = schema->nodes[children[node_index][element]];
                    s_parameter aggregate = (*old_parameters)[element_node.first_parameter];
                    aggregate.path = node.kind == NODE_MAP ? element_node.path : node.path;
                    aggregate.aggregate = true;
                    aggregate.vector = node.kind == NODE_VECTOR;
                    aggregate.node = children[node_index][element];
                    aggregate.default_element_count = node.kind == NODE_VECTOR ? static_cast<uint32_t>(node.array_count) : 0;
                    aggregate.element_leaf_count = element_leaf_count;
                    aggregate.aggregate_leaves.clear();
                    const uint32_t last_leaf = node.kind == NODE_VECTOR ? node.last_parameter : element_node.last_parameter;
                    for (uint32_t leaf = element_node.first_parameter; leaf <= last_leaf; ++leaf) {
                        if (leaf >= old_parameters->size() || (*parameter_seen)[leaf]) {
                            *diagnostic = "configuration schema aggregate has an invalid leaf mapping";
                            return false;
                        }
                        (*parameter_seen)[leaf] = true;
                        aggregate.aggregate_leaves.push_back(std::move((*old_parameters)[leaf]));
                    }
                    const uint32_t parameter = static_cast<uint32_t>(schema->parameters.size());
                    schema->parameters.push_back(std::move(aggregate));
                    if (node.kind == NODE_MAP) {
                        Assign_Aggregate_Subtree_(children[node_index][element], children, parameter, 0, node_states, schema);
                    }
                }
                if (node.kind == NODE_VECTOR) {
                    const uint32_t parameter = node.first_parameter;
                    for (size_t element = 0; element < children[node_index].size(); ++element) {
                        Assign_Aggregate_Subtree_(children[node_index][element], children, parameter, static_cast<uint32_t>(element) * element_leaf_count, node_states, schema);
                    }
                    node.parameter = parameter;
                    node.first_leaf = 0;
                    node.last_leaf = static_cast<uint32_t>(children[node_index].size()) *
                            element_leaf_count -
                        1;
                } else {
                    node.parameter = UINT32_MAX;
                }
                node.last_parameter = static_cast<uint32_t>(schema->parameters.size() - 1);
            } else {
                if (children[node_index].empty()) {
                    *diagnostic = "configuration schema aggregate has no writable descendants";
                    return false;
                }
                node.parameter = UINT32_MAX;
                for (uint32_t child : children[node_index]) {
                    if (!Normalize_Node_(child, children, node_states, parameter_seen, old_parameters, schema, diagnostic)) {
                        return false;
                    }
                }
                node.last_parameter = static_cast<uint32_t>(schema->parameters.size() - 1);
            }
            (*node_states)[node_index] = 2;
            return true;
        }

        bool Normalize_Runtime_Layout_(s_schema* schema, std::string* diagnostic) {
            std::vector<std::vector<uint32_t>> children(schema->nodes.size());
            std::vector<uint32_t> roots;
            for (uint32_t node_index = 0; node_index < schema->nodes.size(); ++node_index) {
                const uint32_t parent = schema->nodes[node_index].parent;
                if (parent == UINT32_MAX) {
                    roots.push_back(node_index);
                } else if (parent < schema->nodes.size()) {
                    children[parent].push_back(node_index);
                } else {
                    *diagnostic = "configuration schema node has an invalid parent";
                    return false;
                }
            }
            if (roots.empty()) {
                *diagnostic = "configuration schema has no root nodes";
                return false;
            }

            std::vector<s_parameter> old_parameters = std::move(schema->parameters);
            schema->parameters.clear();
            schema->parameters.reserve(old_parameters.size());
            std::vector<uint8_t> node_states(schema->nodes.size(), 0);
            std::vector<bool> parameter_seen(old_parameters.size(), false);
            for (uint32_t root : roots) {
                if (!Normalize_Node_(root, children, &node_states, &parameter_seen, &old_parameters, schema, diagnostic)) {
                    return false;
                }
            }
            for (uint8_t state : node_states) {
                if (state != 2) {
                    *diagnostic = "configuration schema contains an unreachable node";
                    return false;
                }
            }
            for (bool seen : parameter_seen) {
                if (!seen) {
                    *diagnostic = "configuration schema contains an unreachable parameter";
                    return false;
                }
            }
            return true;
        }

        std::string Default_Text_(const s_json_value* value) {
            if (value == nullptr) {
                return {};
            }
            switch (value->kind) {
                case JSON_NULL: return "null";
                case JSON_BOOL: return value->bool_value ? "true" : "false";
                case JSON_SIGNED: return std::to_string(value->signed_value);
                case JSON_UNSIGNED: return std::to_string(value->unsigned_value);
                case JSON_REAL: return std::to_string(value->real_value);
                case JSON_STRING: {
                    std::string text = "\"";
                    for (char character : value->string_value) {
                        if (character == '\\' || character == '"') {
                            text += '\\';
                        }
                        text += character;
                    }
                    return text + "\"";
                }
                case JSON_ARRAY: {
                    std::string text = "[";
                    for (size_t index = 0; index < value->values.size(); ++index) {
                        text += index == 0 ? "" : ", ";
                        text += Default_Text_(&value->values[index]);
                    }
                    return text + "]";
                }
                case JSON_OBJECT: {
                    std::string text = "{ ";
                    for (size_t index = 0; index < value->values.size(); ++index) {
                        text += index == 0 ? "" : ", ";
                        text += value->member_names[index] + ": " +
                            Default_Text_(&value->values[index]);
                    }
                    return text + " }";
                }
            }
            return {};
        }

        bool Documentation_Metadata_(const s_json_value* declaration,
            const std::string& path,
            bool* internal,
            s_metadata* metadata,
            std::string* diagnostic) {
            if (!Optional_Bool_(declaration, "internal", false, path, internal, diagnostic)) {
                return false;
            }
            if (!*internal) {
                return Metadata_(declaration, path, metadata, diagnostic);
            }
            return Optional_String_(declaration, "title", path, &metadata->title, diagnostic) &&
                Optional_String_(declaration, "description", path, &metadata->description, diagnostic) &&
                Optional_String_(declaration, "remarks", path, &metadata->remarks, diagnostic) &&
                Optional_String_(declaration, "units", path, &metadata->units, diagnostic);
        }

        bool Document_Type_(const s_json_value* root, const s_json_value* declaration, const std::string& path, const std::string& inherited_group, s_documented_type* documented, std::string* diagnostic);

        bool Document_Fields_(const s_json_value* root, const s_json_value* fields, const std::string& path, const std::string& inherited_group, bool require_documentation, std::vector<s_documented_field>* documented_fields, std::string* diagnostic) {
            if (fields == nullptr || fields->kind != JSON_ARRAY || fields->values.empty()) {
                *diagnostic = path + ": record requires a non-empty field array";
                return false;
            }
            for (const s_json_value& field : fields->values) {
                s_documented_field documented;
                if (field.kind != JSON_OBJECT ||
                    !Required_String_(&field, "id", path, &documented.id, diagnostic) ||
                    !Canonical_Name_(documented.id)) {
                    *diagnostic = path + ": invalid field declaration";
                    return false;
                }
                const std::string field_path = path + "." + documented.id;
                documented.group = inherited_group;
                if (!Optional_String_(&field, "group", field_path, &documented.group, diagnostic)) {
                    return false;
                }
                if (!documented.group.empty()) {
                    const s_json_value* groups = Member_(root, "groups");
                    if (groups == nullptr ||
                        Member_(groups, documented.group.c_str()) == nullptr) {
                        *diagnostic = field_path + ".group: unknown group '" +
                            documented.group + "'";
                        return false;
                    }
                }
                if (!Optional_Bool_(&field, "internal", false, field_path, &documented.internal, diagnostic)) {
                    return false;
                }
                if (require_documentation && !documented.internal) {
                    if (!Metadata_(&field, field_path, &documented.metadata, diagnostic)) {
                        return false;
                    }
                } else if (!Optional_String_(&field, "title", field_path, &documented.metadata.title, diagnostic) ||
                    !Optional_String_(&field, "description", field_path, &documented.metadata.description, diagnostic)) {
                    return false;
                }
                if (!Optional_String_(&field, "remarks", field_path, &documented.metadata.remarks, diagnostic) ||
                    !Optional_String_(&field, "units", field_path, &documented.metadata.units, diagnostic)) {
                    return false;
                }
                documented.type = std::make_shared<s_documented_type>();
                if (!Document_Type_(root, Member_(&field, "type"), field_path + ".type", documented.group, documented.type.get(), diagnostic)) {
                    return false;
                }
                s_constraints field_constraints;
                if (documented.type->node_kind == NODE_SCALAR &&
                    !Constraints_(Member_(&field, "constraints"), field_path + ".constraints", documented.type->primitive, &field_constraints, diagnostic)) {
                    return false;
                }
                s_constraints effective_constraints;
                if (!Narrow_Constraints_(documented.type->constraints, field_constraints, field_path + ".constraints", &effective_constraints, diagnostic)) {
                    return false;
                }
                documented.type->constraints = effective_constraints;
                s_resolved_type resolved;
                resolved.node_kind = documented.type->node_kind;
                resolved.primitive = documented.type->primitive;
                if (!String_Storage_(&field, field_path, resolved.node_kind, resolved.primitive, &documented.type->string_storage, diagnostic)) {
                    return false;
                }
                if (!Nullable_(&field, field_path, resolved.node_kind, resolved.primitive, documented.type->string_storage, &documented.type->nullable, diagnostic)) {
                    return false;
                }
                const s_json_value* default_value = Member_(&field, "default");
                documented.has_default = default_value != nullptr;
                documented.default_value = Default_Text_(default_value);
                documented_fields->push_back(std::move(documented));
            }
            return true;
        }

        void Copy_Resolved_Type_(const s_resolved_type& resolved,
            s_documented_type* documented) {
            documented->node_kind = resolved.node_kind;
            documented->primitive = resolved.primitive;
            documented->scalar_kind = resolved.scalar_kind;
            documented->constraints = resolved.constraints;
            documented->values = resolved.values;
            documented->flags = resolved.flags;
            documented->bitmap_values = resolved.bitmap_values;
            documented->bitmap_flags = resolved.bitmap_flags;
            documented->bitmap_bit_count = resolved.bitmap_bit_count;
            documented->allow_numeric = resolved.allow_numeric;
            documented->reject_unknown_numeric = resolved.reject_unknown_numeric;
        }

        bool Document_Type_(const s_json_value* root, const s_json_value* declaration, const std::string& path, const std::string& inherited_group, s_documented_type* documented, std::string* diagnostic) {
            if (declaration == nullptr) {
                *diagnostic = path + ": missing type";
                return false;
            }
            std::vector<std::string> stack;
            s_resolved_type resolved;
            if (!Resolve_Type_(root, declaration, path, &stack, &resolved, diagnostic)) {
                return false;
            }
            Copy_Resolved_Type_(resolved, documented);
            if (declaration->kind == JSON_STRING) {
                if (!Primitive_(declaration->string_value, &documented->primitive)) {
                    documented->reference = declaration->string_value;
                }
                return true;
            }
            const s_json_value* kind_value = Member_(declaration, "kind");
            if (kind_value == nullptr) {
                return Document_Type_(root, Member_(declaration, "type"), path + ".type", inherited_group, documented, diagnostic);
            }
            if (kind_value->string_value == "record") {
                documented->node_kind = NODE_RECORD;
                return Document_Fields_(root, Member_(declaration, "fields"), path + ".fields", inherited_group, true, &documented->fields, diagnostic);
            }
            if (kind_value->string_value == "array" ||
                kind_value->string_value == "map" ||
                kind_value->string_value == "vector") {
                documented->node_kind = kind_value->string_value == "array" ? NODE_ARRAY : kind_value->string_value == "map" ? NODE_MAP
                                                                                                                             : NODE_VECTOR;
                if (!Integer_(Member_(declaration, "count"), &documented->array_count)) {
                    *diagnostic = path + ".count: expected an integer";
                    return false;
                }
                const s_json_value* index_base = Member_(declaration, "index_base");
                if (index_base != nullptr &&
                    !Integer_(index_base, &documented->array_index_base)) {
                    *diagnostic = path + ".index_base: expected an integer";
                    return false;
                }
                const s_json_value* index_type = Member_(declaration, "index_type");
                if (index_type != nullptr) {
                    documented->array_index_type = index_type->string_value;
                }
                const s_json_value* element = Member_(declaration, "element");
                std::string element_group = inherited_group;
                if (!Optional_String_(element, "group", path + ".element", &element_group, diagnostic)) {
                    return false;
                }
                if (!element_group.empty()) {
                    const s_json_value* groups = Member_(root, "groups");
                    if (groups == nullptr || Member_(groups, element_group.c_str()) == nullptr) {
                        *diagnostic = path + ".element.group: unknown group '" +
                            element_group + "'";
                        return false;
                    }
                }
                documented->array_element = std::make_shared<s_documented_type>();
                return Document_Type_(root, Member_(element, "type"), path + ".element.type", element_group, documented->array_element.get(), diagnostic);
            }
            return true;
        }

        void Add_Reference_(const s_documented_type& type,
            const s_documentation_reference& source,
            s_schema* schema) {
            if (!type.reference.empty()) {
                for (s_type_declaration& declaration : schema->type_declarations) {
                    if (declaration.id == type.reference) {
                        declaration.used_by.push_back(source);
                    }
                }
                for (s_record_declaration& declaration : schema->record_declarations) {
                    if (declaration.id == type.reference) {
                        declaration.used_by.push_back(source);
                    }
                }
            }
            if (type.array_element != nullptr) {
                Add_Reference_(*type.array_element, source, schema);
            }
            if (!type.array_index_type.empty()) {
                s_documented_type index_type;
                index_type.reference = type.array_index_type;
                Add_Reference_(index_type, source, schema);
            }
            for (const s_documented_field& field : type.fields) {
                if (field.type != nullptr) {
                    Add_Reference_(*field.type, source, schema);
                }
            }
        }

        bool Build_Documentation_Model_(const s_json_value* root, const std::string& schema_path, s_schema* schema, std::string* diagnostic) {
            const s_json_value* types = Member_(root, "types");
            const s_json_value* records = Member_(root, "records");
            const s_json_value* parameters = Member_(root, "parameters");
            for (size_t index = 0; index < types->member_names.size(); ++index) {
                s_type_declaration declaration;
                declaration.id = types->member_names[index];
                const std::string path = schema_path + ".types." + declaration.id;
                if (!Documentation_Metadata_(&types->values[index], path, &declaration.internal, &declaration.metadata, diagnostic) ||
                    !Document_Type_(root, &types->values[index], path, "", &declaration.type, diagnostic)) {
                    return false;
                }
                schema->type_declarations.push_back(std::move(declaration));
            }
            for (size_t index = 0; index < records->member_names.size(); ++index) {
                s_record_declaration declaration;
                declaration.id = records->member_names[index];
                const std::string path = schema_path + ".records." + declaration.id;
                if (!Documentation_Metadata_(&records->values[index], path, &declaration.internal, &declaration.metadata, diagnostic) ||
                    !Document_Fields_(root, Member_(&records->values[index], "fields"), path + ".fields", "", !declaration.internal, &declaration.fields, diagnostic)) {
                    return false;
                }
                schema->record_declarations.push_back(std::move(declaration));
            }
            for (size_t index = 0; index < parameters->member_names.size(); ++index) {
                s_parameter_declaration declaration;
                declaration.id = parameters->member_names[index];
                const s_json_value& parameter = parameters->values[index];
                const std::string path = schema_path + ".parameters." + declaration.id;
                if (!Optional_String_(&parameter, "namespace", path, &declaration.name_space, diagnostic) ||
                    !Optional_Bool_(&parameter, "include_default_configuration", true, path, &declaration.include_default_configuration, diagnostic)) {
                    return false;
                }
                if (!Metadata_(&parameter, path, &declaration.metadata, diagnostic) ||
                    !Required_String_(&parameter, "group", path, &declaration.group, diagnostic) ||
                    !Document_Type_(root, Member_(&parameter, "type"), path + ".type", declaration.group, &declaration.type, diagnostic)) {
                    return false;
                }
                if (!schema->documentation_categories.empty()) {
                    for (const s_documentation_category& category :
                        schema->documentation_categories) {
                        for (const std::string& category_namespace : category.namespaces) {
                            if (category_namespace == declaration.name_space) {
                                declaration.documentation_category = category.id;
                            }
                        }
                    }
                    if (declaration.documentation_category.empty()) {
                        *diagnostic = path + ".namespace: namespace '" +
                            declaration.name_space +
                            "' has no documentation category";
                        return false;
                    }
                } else {
                    declaration.documentation_category = declaration.group;
                }
                s_constraints parameter_constraints;
                if (declaration.type.node_kind == NODE_SCALAR &&
                    !Constraints_(Member_(&parameter, "constraints"), path + ".constraints", declaration.type.primitive, &parameter_constraints, diagnostic)) {
                    return false;
                }
                s_constraints effective_constraints;
                if (!Narrow_Constraints_(declaration.type.constraints, parameter_constraints, path + ".constraints", &effective_constraints, diagnostic)) {
                    return false;
                }
                declaration.type.constraints = effective_constraints;
                s_resolved_type resolved;
                resolved.node_kind = declaration.type.node_kind;
                resolved.primitive = declaration.type.primitive;
                if (!String_Storage_(&parameter, path, resolved.node_kind, resolved.primitive, &declaration.type.string_storage, diagnostic)) {
                    return false;
                }
                if (!Nullable_(&parameter, path, resolved.node_kind, resolved.primitive, declaration.type.string_storage, &declaration.type.nullable, diagnostic)) {
                    return false;
                }
                const s_json_value* default_value = Member_(&parameter, "default");
                declaration.has_default = default_value != nullptr;
                declaration.default_value = Default_Text_(default_value);
                declaration.default_from_native = Member_(&parameter, "default_source") != nullptr;
                const s_json_value* bind_root = Member_(&parameter, "bind_root");
                if (bind_root != nullptr) {
                    Required_String_(bind_root, "header", path + ".bind_root", &declaration.binding.header, diagnostic);
                    Required_String_(bind_root, "symbol", path + ".bind_root", &declaration.binding.symbol, diagnostic);
                }
                schema->parameter_declarations.push_back(std::move(declaration));
            }
            for (const s_parameter_declaration& parameter : schema->parameter_declarations) {
                Add_Reference_(parameter.type,
                    {parameter.metadata.title, "parameter-" + parameter.id},
                    schema);
            }
            for (const s_type_declaration& declaration : schema->type_declarations) {
                Add_Reference_(declaration.type,
                    {declaration.metadata.title, "type-" + declaration.id},
                    schema);
            }
            for (const s_record_declaration& declaration : schema->record_declarations) {
                s_documented_type record;
                record.fields = declaration.fields;
                Add_Reference_(record,
                    {declaration.metadata.title, "record-" + declaration.id},
                    schema);
            }
            return true;
        }
    } // namespace

    std::string Encode_Game_String(const std::string& text) {
        std::string encoded;
        size_t offset = 0;
        while (offset < text.size()) {
            uint32_t codepoint = 0;
            uint8_t byte = 0;
            if (!Decode_Utf8_(text, &offset, &codepoint) || !Cp850_Byte_(codepoint, &byte)) {
                return {};
            }
            encoded.push_back(static_cast<char>(byte));
        }
        return encoded;
    }

    std::string Decode_Game_String(const std::string& text) {
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
        std::string decoded;
        for (uint8_t byte : text) {
            const uint32_t codepoint = byte < 0x80 ? byte : extended[byte - 0x80];
            if (codepoint < 0x80) {
                decoded.push_back(static_cast<char>(codepoint));
            } else if (codepoint < 0x800) {
                decoded.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                decoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else {
                decoded.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                decoded.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                decoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        }
        return decoded;
    }

    const char* Primitive_Name(e_primitive_type type) {
        static const char* names[] = {"bool", "int8", "uint8", "int16", "uint16", "int32", "uint32", "string", "bitmap", "ratio"};
        return names[type];
    }

    bool Validate_Runtime_Layout(const s_schema* schema, std::string* diagnostic) {
        if (schema == nullptr || diagnostic == nullptr) {
            return false;
        }
        std::vector<std::vector<uint32_t>> children(schema->nodes.size());
        std::vector<uint32_t> roots;
        for (uint32_t node_index = 0; node_index < schema->nodes.size(); ++node_index) {
            const uint32_t parent = schema->nodes[node_index].parent;
            if (parent == UINT32_MAX) {
                roots.push_back(node_index);
            } else if (parent < schema->nodes.size()) {
                children[parent].push_back(node_index);
            } else {
                *diagnostic = "configuration schema node has an invalid parent";
                return false;
            }
        }
        if (roots.empty() || schema->parameters.empty()) {
            *diagnostic = "configuration schema runtime layout is empty";
            return false;
        }

        std::vector<uint8_t> states(schema->nodes.size(), 0);
        std::vector<uint32_t> parameter_owners(schema->parameters.size(), 0);
        const auto validate_node = [&](const auto& self, uint32_t node_index) -> bool {
            if (states[node_index] != 0) {
                *diagnostic = "configuration schema node graph is cyclic";
                return false;
            }
            states[node_index] = 1;
            const s_node& node = schema->nodes[node_index];
            if (node.first_parameter > node.last_parameter ||
                node.last_parameter >= schema->parameters.size()) {
                *diagnostic = "configuration schema node has an invalid variable range";
                return false;
            }
            if (node.kind == NODE_SCALAR) {
                if (!children[node_index].empty() || node.parameter != node.first_parameter ||
                    node.parameter != node.last_parameter || node.parameter >= schema->parameters.size()) {
                    *diagnostic = "configuration schema scalar has an invalid variable range";
                    return false;
                }
                if (!schema->parameters[node.parameter].aggregate) {
                    ++parameter_owners[node.parameter];
                }
            } else {
                if (children[node_index].empty() ||
                    (node.kind != NODE_VECTOR && node.parameter != UINT32_MAX)) {
                    *diagnostic = "configuration schema aggregate has an invalid variable mapping";
                    return false;
                }
                const bool aggregate_backed = node.first_parameter == node.last_parameter &&
                    node.first_parameter < schema->parameters.size() &&
                    schema->parameters[node.first_parameter].aggregate;
                uint32_t expected = node.first_parameter;
                for (uint32_t child : children[node_index]) {
                    if (!self(self, child)) {
                        return false;
                    }
                    const s_node& child_node = schema->nodes[child];
                    if (!aggregate_backed && node.kind != NODE_MAP && node.kind != NODE_VECTOR &&
                        child_node.first_parameter != expected) {
                        *diagnostic = "configuration schema sibling variable ranges overlap or have gaps";
                        return false;
                    }
                    expected = child_node.last_parameter + 1;
                }
                if (!aggregate_backed && node.kind != NODE_MAP && node.kind != NODE_VECTOR &&
                    expected != node.last_parameter + 1) {
                    *diagnostic = "configuration schema aggregate range does not match its descendants";
                    return false;
                }
                if ((node.kind == NODE_MAP || node.kind == NODE_VECTOR) &&
                    node.first_parameter <= node.last_parameter) {
                    for (uint32_t variable = node.first_parameter;
                         variable <= node.last_parameter;
                         ++variable) {
                        if (schema->parameters[variable].node >= schema->nodes.size()) {
                            *diagnostic = "configuration schema aggregate variable has no owner";
                            return false;
                        }
                        ++parameter_owners[variable];
                    }
                }
            }
            states[node_index] = 2;
            return true;
        };

        uint32_t expected_root_parameter = 0;
        for (uint32_t root : roots) {
            if (!validate_node(validate_node, root)) {
                return false;
            }
            const s_node& root_node = schema->nodes[root];
            if (root_node.first_parameter != expected_root_parameter) {
                *diagnostic = "configuration schema root variable ranges overlap or have gaps";
                return false;
            }
            expected_root_parameter = root_node.last_parameter + 1;
        }
        if (expected_root_parameter != schema->parameters.size()) {
            *diagnostic = "configuration schema root ranges do not cover every variable";
            return false;
        }
        for (uint8_t state : states) {
            if (state != 2) {
                *diagnostic = "configuration schema contains an unreachable node";
                return false;
            }
        }
        for (uint32_t owners : parameter_owners) {
            if (owners != 1) {
                *diagnostic = "configuration schema variable does not have exactly one scalar owner";
                return false;
            }
        }
        return true;
    }

    bool Load_And_Validate_Schema(const char* path, s_schema* schema, std::string* diagnostic) {
        if (path == nullptr || schema == nullptr || diagnostic == nullptr) {
            return false;
        }
        s_schema loaded;
        s_json_value root;
        if (!Read_Json5_File(path, &root, diagnostic)) {
            return false;
        }
        const std::string schema_path(path);
        if (!Has_Only_Members_(&root,
                {"groups", "documentation_categories", "types", "records", "parameters"},
                schema_path,
                diagnostic)) {
            return false;
        }
        const s_json_value* groups = Member_(&root, "groups");
        const s_json_value* documentation_categories =
            Member_(&root, "documentation_categories");
        const s_json_value* types = Member_(&root, "types");
        const s_json_value* records = Member_(&root, "records");
        const s_json_value* parameters = Member_(&root, "parameters");
        if (groups == nullptr || groups->kind != JSON_OBJECT || groups->values.empty()) {
            *diagnostic = schema_path + ".groups: expected a non-empty object";
            return false;
        }
        if (types == nullptr || types->kind != JSON_OBJECT) {
            *diagnostic = schema_path + ".types: expected an object";
            return false;
        }
        if (records == nullptr || records->kind != JSON_OBJECT) {
            *diagnostic = schema_path + ".records: expected an object";
            return false;
        }
        if (parameters == nullptr || parameters->kind != JSON_OBJECT || parameters->values.empty()) {
            *diagnostic = schema_path + ".parameters: expected a non-empty object";
            return false;
        }

        for (size_t index = 0; index < groups->member_names.size(); ++index) {
            loaded.groups.emplace_back();
            if (!Group_(groups->member_names[index], &groups->values[index], schema_path, &loaded.groups.back(), diagnostic)) {
                return false;
            }
        }
        if (!Documentation_Categories_(documentation_categories, schema_path, &loaded.documentation_categories, diagnostic)) {
            return false;
        }
        for (size_t index = 0; index < types->member_names.size(); ++index) {
            const std::string& id = types->member_names[index];
            const std::string path = schema_path + ".types." + id;
            if (!Canonical_Name_(id)) {
                *diagnostic = path + ": type names must use lower_snake_case";
                return false;
            }
            std::vector<std::string> stack;
            s_resolved_type resolved;
            if (!Resolve_Type_(&root, &types->values[index], path, &stack, &resolved, diagnostic)) {
                return false;
            }
        }
        for (const s_group& group : loaded.groups) {
            for (size_t index = 0; index < parameters->member_names.size(); ++index) {
                const s_json_value& parameter_value = parameters->values[index];
                const s_json_value* parameter_group = Member_(&parameter_value, "group");
                if (parameter_group == nullptr || parameter_group->kind != JSON_STRING ||
                    parameter_group->string_value != group.id) {
                    continue;
                }
                std::string name_space;
                if (!Optional_String_(&parameter_value, "namespace", schema_path + ".parameters." + parameters->member_names[index], &name_space, diagnostic) ||
                    (!name_space.empty() && !Canonical_Name_(name_space))) {
                    if (diagnostic->empty()) {
                        *diagnostic = schema_path + ".parameters." +
                            parameters->member_names[index] +
                            ".namespace: namespaces must use lower_snake_case";
                    }
                    return false;
                }
                uint32_t parent = UINT32_MAX;
                if (!name_space.empty()) {
                    for (size_t other = 0; other < parameters->member_names.size(); ++other) {
                        const s_json_value* other_namespace =
                            Member_(&parameters->values[other], "namespace");
                        if (parameters->member_names[other] == name_space &&
                            other_namespace == nullptr) {
                            *diagnostic = schema_path + ".parameters." +
                                parameters->member_names[index] +
                                ".namespace: namespace conflicts with a top-level parameter";
                            return false;
                        }
                    }
                    for (size_t node_index = 0; node_index < loaded.nodes.size(); ++node_index) {
                        if (loaded.nodes[node_index].parent == UINT32_MAX &&
                            loaded.nodes[node_index].id == name_space) {
                            parent = static_cast<uint32_t>(node_index);
                            break;
                        }
                    }
                    if (parent == UINT32_MAX) {
                        s_node node;
                        node.id = name_space;
                        node.kind = NODE_NAMESPACE;
                        parent = static_cast<uint32_t>(loaded.nodes.size());
                        loaded.nodes.push_back(std::move(node));
                    }
                }
                if (!Rich_Parameter_(&root, parameters->member_names[index], &parameter_value, schema_path, parent, name_space, &loaded, diagnostic)) {
                    return false;
                }
            }
        }
        for (size_t index = 0; index < parameters->member_names.size(); ++index) {
            const s_json_value& parameter_value = parameters->values[index];
            const s_json_value* parameter_group = Member_(&parameter_value, "group");
            bool group_found = false;
            if (parameter_group != nullptr && parameter_group->kind == JSON_STRING) {
                for (const s_group& group : loaded.groups) {
                    group_found = group_found || group.id == parameter_group->string_value;
                }
            }
            if (!group_found) {
                const std::string name = parameter_group != nullptr &&
                        parameter_group->kind == JSON_STRING
                    ? parameter_group->string_value
                    : "";
                *diagnostic = schema_path + ".parameters." + parameters->member_names[index] +
                    ".group: unknown group '" + name + "'";
                return false;
            }
        }
        if (!Normalize_Runtime_Layout_(&loaded, diagnostic) ||
            !Validate_Runtime_Layout(&loaded, diagnostic)) {
            return false;
        }
        if (!Build_Documentation_Model_(&root, schema_path, &loaded, diagnostic)) {
            return false;
        }
        *schema = std::move(loaded);
        return true;
    }

} // namespace config::tool