#include "rpc_protocol.h"

#include <cstdlib>
#include <cstring>
#include <set>
#include <string_view>
#include <vector>

#include "yyjson.h"
#include <uv.h>

namespace rpc {
    namespace {
        struct s_error_descriptor {
            int32_t code;
            const char* message;
        };

#include "generated/rpc_contract_data.inc"

        bool Is_Text(yyjson_val* value, const char* text) {
            return yyjson_is_str(value) && yyjson_get_len(value) == std::strlen(text) &&
                std::memcmp(yyjson_get_str(value), text, yyjson_get_len(value)) == 0;
        }

        bool Depth_Allowed(const char* bytes, size_t size) {
            uint32_t depth = 0;
            bool quoted = false;
            bool escaped = false;
            for (size_t index = 0; index < size; ++index) {
                const char value = bytes[index];
                if (quoted) {
                    if (escaped) {
                        escaped = false;
                    } else if (value == '\\') {
                        escaped = true;
                    } else if (value == '"') {
                        quoted = false;
                    }
                } else if (value == '"') {
                    quoted = true;
                } else if (value == '[' || value == '{') {
                    if (++depth > RPC_MAX_DEPTH) {
                        return false;
                    }
                } else if ((value == ']' || value == '}') && depth > 0) {
                    --depth;
                }
            }
            return true;
        }

        bool Unique_Keys(yyjson_val* value) {
            if (yyjson_is_obj(value)) {
                std::set<std::string_view> keys;
                yyjson_obj_iter iter = yyjson_obj_iter_with(value);
                while (yyjson_val* key = yyjson_obj_iter_next(&iter)) {
                    if (!keys.emplace(yyjson_get_str(key), yyjson_get_len(key)).second ||
                        !Unique_Keys(yyjson_obj_iter_get_val(key))) {
                        return false;
                    }
                }
            } else if (yyjson_is_arr(value)) {
                size_t index, count;
                yyjson_val* item;
                yyjson_arr_foreach(value, index, count, item) {
                    if (!Unique_Keys(item)) {
                        return false;
                    }
                }
            }
            return true;
        }

        std::string Encode(yyjson_val* value) {
            size_t size = 0;
            char* text = yyjson_val_write(value, YYJSON_WRITE_NOFLAG, &size);
            if (text == nullptr) {
                return {};
            }
            std::string result(text, size);
            std::free(text);
            return result;
        }

        bool Valid_Id(yyjson_val* id) {
            if (yyjson_is_str(id)) {
                return yyjson_get_len(id) > 0 && yyjson_get_len(id) <= RPC_MAX_REQUEST_ID_BYTES;
            }
            if (yyjson_is_uint(id)) {
                return yyjson_get_uint(id) <= 9007199254740991ULL;
            }
            return yyjson_is_sint(id) && yyjson_get_sint(id) >= -9007199254740991LL &&
                yyjson_get_sint(id) <= 9007199254740991LL;
        }

        bool In_Array(yyjson_val* array, const char* text, size_t size) {
            size_t index, count;
            yyjson_val* item;
            yyjson_arr_foreach(array, index, count, item) {
                if (yyjson_is_str(item) && yyjson_get_len(item) == size &&
                    std::memcmp(yyjson_get_str(item), text, size) == 0) {
                    return true;
                }
            }
            return false;
        }

        bool Validate_Value(yyjson_val* value, yyjson_val* definition, yyjson_val* types, uint32_t depth) {
            if (definition == nullptr || depth > RPC_MAX_DEPTH) {
                return false;
            }
            yyjson_val* kind = yyjson_obj_get(definition, "kind");
            if (Is_Text(kind, "string")) {
                if (!yyjson_is_str(value)) {
                    return false;
                }
                const size_t size = yyjson_get_len(value);
                if (size < yyjson_get_uint(yyjson_obj_get(definition, "minBytes")) ||
                    size > yyjson_get_uint(yyjson_obj_get(definition, "maxBytes"))) {
                    return false;
                }
                // Pattern-bearing scalar formats are not enabled in the initial request surface.
                return yyjson_obj_get(definition, "pattern") == nullptr;
            }
            if (Is_Text(kind, "boolean")) {
                return yyjson_is_bool(value);
            }
            if (Is_Text(kind, "integer")) {
                if (!yyjson_is_int(value)) {
                    return false;
                }
                const int64_t minimum = yyjson_get_sint(yyjson_obj_get(definition, "min"));
                const int64_t maximum = yyjson_get_sint(yyjson_obj_get(definition, "max"));
                if (yyjson_is_uint(value)) {
                    return maximum >= 0 && yyjson_get_uint(value) <= (uint64_t)maximum &&
                        (minimum <= 0 || yyjson_get_uint(value) >= (uint64_t)minimum);
                }
                return yyjson_get_sint(value) >= minimum && yyjson_get_sint(value) <= maximum;
            }
            if (Is_Text(kind, "enum")) {
                return yyjson_is_str(value) && In_Array(yyjson_obj_get(definition, "values"), yyjson_get_str(value), yyjson_get_len(value));
            }
            if (Is_Text(kind, "object")) {
                if (!yyjson_is_obj(value)) {
                    return false;
                }
                yyjson_val* fields = yyjson_obj_get(definition, "fields");
                yyjson_val* optional = yyjson_obj_get(definition, "optional");
                yyjson_obj_iter iter = yyjson_obj_iter_with(fields);
                while (yyjson_val* field = yyjson_obj_iter_next(&iter)) {
                    yyjson_val* item = yyjson_obj_getn(value, yyjson_get_str(field), yyjson_get_len(field));
                    if (item == nullptr) {
                        if (!In_Array(optional, yyjson_get_str(field), yyjson_get_len(field))) {
                            return false;
                        }
                    } else {
                        yyjson_val* type = yyjson_obj_iter_get_val(field);
                        if (!Validate_Value(item, yyjson_obj_getn(types, yyjson_get_str(type), yyjson_get_len(type)), types, depth + 1)) {
                            return false;
                        }
                    }
                }
                iter = yyjson_obj_iter_with(value);
                while (yyjson_val* field = yyjson_obj_iter_next(&iter)) {
                    if (yyjson_obj_getn(fields, yyjson_get_str(field), yyjson_get_len(field)) == nullptr) {
                        return false;
                    }
                }
                return true;
            }
            if (Is_Text(kind, "array")) {
                if (!yyjson_is_arr(value)) {
                    return false;
                }
                const size_t size = yyjson_arr_size(value);
                if (size > yyjson_get_uint(yyjson_obj_get(definition, "maxItems")) ||
                    size < yyjson_get_uint(yyjson_obj_get(definition, "minItems"))) {
                    return false;
                }
                yyjson_val* type = yyjson_obj_get(definition, "items");
                yyjson_val* item_definition = yyjson_obj_getn(types, yyjson_get_str(type), yyjson_get_len(type));
                std::set<std::string> items;
                size_t index, count;
                yyjson_val* item;
                yyjson_arr_foreach(value, index, count, item) {
                    if (!Validate_Value(item, item_definition, types, depth + 1) ||
                        (yyjson_get_bool(yyjson_obj_get(definition, "unique")) && !items.insert(Encode(item)).second)) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }
    } // namespace

    bool Initialize_Protocol(s_protocol* protocol) {
        if (protocol == nullptr || protocol->definitions != nullptr) {
            return false;
        }
        protocol->definitions = yyjson_read(rpc_types_json, sizeof(rpc_types_json) - 1, YYJSON_READ_NOFLAG);
        return protocol->definitions != nullptr;
    }

    const s_protocol_limits* Protocol_Limits() {
        return &rpc_limits;
    }

    void Shutdown_Protocol(s_protocol* protocol) {
        if (protocol != nullptr && protocol->definitions != nullptr) {
            yyjson_doc_free((yyjson_doc*)protocol->definitions);
            protocol->definitions = nullptr;
        }
    }

    int32_t Parse_Request(const char* bytes, size_t size, s_request* request) {
        if (request == nullptr) {
            return RPC_INTERNAL_ERROR;
        }
        *request = {};
        if (size > RPC_MAX_INPUT_BYTES) {
            return RPC_LIMIT_EXCEEDED;
        }
        if (bytes == nullptr || size == 0) {
            return RPC_PARSE_ERROR;
        }
        if (!Depth_Allowed(bytes, size)) {
            return RPC_LIMIT_EXCEEDED;
        }
        const size_t pool_size = yyjson_read_max_memory_usage(size, YYJSON_READ_NOFLAG);
        std::vector<uint8_t> pool(pool_size);
        yyjson_alc allocator;
        if (!yyjson_alc_pool_init(&allocator, pool.data(), pool.size())) {
            return RPC_INTERNAL_ERROR;
        }
        yyjson_doc* document = yyjson_read_opts((char*)bytes, size, YYJSON_READ_NOFLAG, &allocator, nullptr);
        if (document == nullptr) {
            return RPC_PARSE_ERROR;
        }
        yyjson_val* root = yyjson_doc_get_root(document);
        int32_t result = RPC_INVALID_REQUEST;
        if (!Unique_Keys(root)) {
            result = RPC_PARSE_ERROR;
        } else if (yyjson_is_obj(root) && Is_Text(yyjson_obj_get(root, "jsonrpc"), "2.0") &&
            yyjson_is_str(yyjson_obj_get(root, "method"))) {
            yyjson_val* method = yyjson_obj_get(root, "method");
            request->method.assign(yyjson_get_str(method), yyjson_get_len(method));
            yyjson_val* id = yyjson_obj_get(root, "id");
            if (id == nullptr) {
                request->notification = true;
                result = 0;
            } else if (Valid_Id(id)) {
                bool valid = true;
                yyjson_obj_iter iter = yyjson_obj_iter_with(root);
                while (yyjson_val* key = yyjson_obj_iter_next(&iter)) {
                    valid &= Is_Text(key, "jsonrpc") || Is_Text(key, "method") || Is_Text(key, "id") || Is_Text(key, "params");
                }
                if (valid) {
                    request->id_json = Encode(id);
                    yyjson_val* params = yyjson_obj_get(root, "params");
                    request->params_json = params != nullptr ? Encode(params) : "{}";
                    result = request->id_json.empty() || request->params_json.empty() ? RPC_INTERNAL_ERROR : 0;
                }
            }
        }
        yyjson_doc_free(document);
        return result;
    }

    const s_method_descriptor* Find_Method(std::string_view name) {
        for (const s_method_descriptor& method : rpc_methods) {
            if (name == method.name) {
                return &method;
            }
        }
        return nullptr;
    }

    bool Validate_Params(const s_protocol* protocol, const s_request* request) {
        if (protocol == nullptr || protocol->definitions == nullptr || request == nullptr) {
            return false;
        }
        const s_method_descriptor* method = Find_Method(request->method);
        if (method == nullptr || !method->implemented || request->params_json.size() > RPC_MAX_INPUT_BYTES ||
            !Depth_Allowed(request->params_json.data(), request->params_json.size())) {
            return false;
        }
        yyjson_doc* document = yyjson_read(request->params_json.data(), request->params_json.size(), YYJSON_READ_NOFLAG);
        if (document == nullptr) {
            return false;
        }
        yyjson_val* types = yyjson_doc_get_root((yyjson_doc*)protocol->definitions);
        const bool valid = Unique_Keys(yyjson_doc_get_root(document)) &&
            Validate_Value(yyjson_doc_get_root(document), yyjson_obj_get(types, method->request_type), types, 0);
        yyjson_doc_free(document);
        return valid;
    }

    bool Read_Hello(const s_request* request, std::string* key, bool* controller) {
        if (request == nullptr || key == nullptr || controller == nullptr) {
            return false;
        }
        yyjson_doc* document = yyjson_read(request->params_json.data(), request->params_json.size(), YYJSON_READ_NOFLAG);
        if (document == nullptr) {
            return false;
        }
        yyjson_val* root = yyjson_doc_get_root(document);
        yyjson_val* supplied = yyjson_obj_get(root, "key");
        const bool valid = yyjson_is_str(supplied);
        if (valid) {
            key->assign(yyjson_get_str(supplied), yyjson_get_len(supplied));
            *controller = Is_Text(yyjson_obj_get(root, "requestedRole"), "controller");
        }
        yyjson_doc_free(document);
        return valid;
    }

    std::string Json_String(std::string_view text) {
        yyjson_mut_doc* document = yyjson_mut_doc_new(nullptr);
        if (document == nullptr) {
            return {};
        }
        yyjson_mut_doc_set_root(document, yyjson_mut_strncpy(document, text.empty() ? "" : text.data(), text.size()));
        size_t size = 0;
        char* encoded = yyjson_mut_write(document, YYJSON_WRITE_NOFLAG, &size);
        std::string result = encoded != nullptr ? std::string(encoded, size) : std::string();
        std::free(encoded);
        yyjson_mut_doc_free(document);
        return result;
    }

    std::string Describe_Methods(const s_request* request) {
        if (request == nullptr) {
            return {};
        }
        yyjson_doc* document = yyjson_read(request->params_json.data(), request->params_json.size(), YYJSON_READ_NOFLAG);
        if (document == nullptr) {
            return {};
        }
        yyjson_val* selected = yyjson_obj_get(yyjson_doc_get_root(document), "methods");
        std::string result = "{\"contractVersion\":\"orion2.rpc/0\",\"methods\":[";
        bool first = true;
        for (const s_method_descriptor& method : rpc_methods) {
            if (!method.implemented || (selected != nullptr && !In_Array(selected, method.name, std::strlen(method.name)))) {
                continue;
            }
            if (!first) {
                result += ',';
            }
            first = false;
            result += "{\"name\":" + Json_String(method.name) + ",\"role\":" + Json_String(method.role) +
                ",\"availability\":" + Json_String(method.availability) + ",\"requestType\":" + Json_String(method.request_type) +
                ",\"resultType\":" + Json_String(method.result_type) + "}";
        }
        yyjson_doc_free(document);
        return result + "]}";
    }

    std::string Error_Response(const s_request* request, int32_t code) {
        const char* message = "Internal error";
        for (const s_error_descriptor& error : rpc_errors) {
            if (error.code == code) {
                message = error.message;
                break;
            }
        }
        return "{\"jsonrpc\":\"2.0\",\"id\":" + (request != nullptr && !request->id_json.empty() ? request->id_json : "null") +
            ",\"error\":{\"code\":" + std::to_string(code) + ",\"message\":" + Json_String(message) + "}}";
    }

    std::string Result_Response(const s_request* request, std::string_view result_json) {
        std::string response = "{\"jsonrpc\":\"2.0\",\"id\":" + request->id_json + ",\"result\":";
        response.append(result_json);
        return response + "}";
    }

    bool Keys_Match(std::string_view configured, std::string_view supplied) {
        if (configured.empty() || configured.size() != supplied.size()) {
            return false;
        }
        unsigned char different = 0;
        for (size_t index = 0; index < configured.size(); ++index) {
            different |= (unsigned char)configured[index] ^ (unsigned char)supplied[index];
        }
        return different == 0;
    }

    uint64_t Monotonic_Us() {
        return uv_hrtime() / 1000;
    }

    uint64_t Monotonic_Ms() {
        return Monotonic_Us() / 1000;
    }

    bool New_Opaque_Id(std::string* id) {
        uint8_t bytes[16];
        if (id == nullptr || uv_random(nullptr, nullptr, bytes, sizeof(bytes), 0, nullptr) != 0) {
            return false;
        }
        constexpr char hex[] = "0123456789abcdef";
        id->clear();
        id->reserve(32);
        for (uint8_t byte : bytes) {
            *id += hex[byte >> 4];
            *id += hex[byte & 15];
        }
        return true;
    }
} // namespace rpc
