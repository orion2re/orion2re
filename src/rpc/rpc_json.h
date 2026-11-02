#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

#include "yyjson.h"

namespace rpc {
    struct s_request;

    // Internal RPC construction state; initialize once and shut down after writing.
    struct s_json_builder {
        yyjson_mut_doc* document = nullptr;
        bool failed = false;
    };

    struct s_json_member {
        std::string_view key;
        yyjson_mut_val* value;
    };

    void Initialize_Json(s_json_builder* json, const yyjson_alc* allocator = nullptr);
    void Shutdown_Json(s_json_builder* json);
    // Nodes belong to the builder's document and must be attached to at most one parent.
    yyjson_mut_val* Json_Object(s_json_builder* json, std::initializer_list<s_json_member> members = {});
    yyjson_mut_val* Json_Array(s_json_builder* json);
    yyjson_mut_val* Json_String_Value(s_json_builder* json, std::string_view value);
    yyjson_mut_val* Json_Integer(s_json_builder* json, int64_t value);
    yyjson_mut_val* Json_Unsigned(s_json_builder* json, uint64_t value);
    yyjson_mut_val* Json_Boolean(s_json_builder* json, bool value);
    yyjson_mut_val* Json_Null(s_json_builder* json);
    void Json_Add(s_json_builder* json, yyjson_mut_val* object, std::string_view key, yyjson_mut_val* value);
    void Json_Append(s_json_builder* json, yyjson_mut_val* array, yyjson_mut_val* value);
    // Import encoded values only at protocol boundaries, not between serializers.
    yyjson_mut_val* Json_Parse(s_json_builder* json, std::string_view encoded);
    bool Write_Json(s_json_builder* json, yyjson_mut_val* root, std::string* output);

    yyjson_mut_val* Describe_Methods(s_json_builder* json, const s_request* request);
    std::string Result_Response(s_json_builder* json, const s_request* request, yyjson_mut_val* result);
    yyjson_mut_val* Json_Notification(s_json_builder* json, std::string_view method, yyjson_mut_val* params);
} // namespace rpc
