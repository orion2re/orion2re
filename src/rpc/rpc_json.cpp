#include "rpc_json.h"

#include <cstdlib>

namespace rpc {
    namespace {
        yyjson_mut_val* Check_Value(s_json_builder* json, yyjson_mut_val* value) {
            if (value == nullptr) json->failed = true;
            return value;
        }
    } // namespace

    void Initialize_Json(s_json_builder* json, const yyjson_alc* allocator) {
        json->document = yyjson_mut_doc_new(allocator);
        json->failed = json->document == nullptr;
    }

    void Shutdown_Json(s_json_builder* json) {
        yyjson_mut_doc_free(json->document);
        *json = {};
    }

    yyjson_mut_val* Json_Object(s_json_builder* json, std::initializer_list<s_json_member> members) {
        if (json->failed) return nullptr;
        auto* object = Check_Value(json, yyjson_mut_obj(json->document));
        for (const auto& member : members) Json_Add(json, object, member.key, member.value);
        return object;
    }

    yyjson_mut_val* Json_Array(s_json_builder* json) {
        return json->failed ? nullptr : Check_Value(json, yyjson_mut_arr(json->document));
    }

    yyjson_mut_val* Json_String_Value(s_json_builder* json, std::string_view value) {
        return json->failed ? nullptr : Check_Value(json, yyjson_mut_strncpy(json->document, value.empty() ? "" : value.data(), value.size()));
    }

    yyjson_mut_val* Json_Integer(s_json_builder* json, int64_t value) {
        return json->failed ? nullptr : Check_Value(json, yyjson_mut_sint(json->document, value));
    }

    yyjson_mut_val* Json_Unsigned(s_json_builder* json, uint64_t value) {
        return json->failed ? nullptr : Check_Value(json, yyjson_mut_uint(json->document, value));
    }

    yyjson_mut_val* Json_Boolean(s_json_builder* json, bool value) {
        return json->failed ? nullptr : Check_Value(json, yyjson_mut_bool(json->document, value));
    }

    yyjson_mut_val* Json_Null(s_json_builder* json) {
        return json->failed ? nullptr : Check_Value(json, yyjson_mut_null(json->document));
    }

    void Json_Add(s_json_builder* json, yyjson_mut_val* object, std::string_view key, yyjson_mut_val* value) {
        if (json->failed) return;
        auto* name = Json_String_Value(json, key);
        if (!yyjson_mut_is_obj(object) || value == nullptr || name == nullptr || !yyjson_mut_obj_add(object, name, value)) {
            json->failed = true;
        }
    }

    void Json_Append(s_json_builder* json, yyjson_mut_val* array, yyjson_mut_val* value) {
        if (json->failed) return;
        if (!yyjson_mut_is_arr(array) || value == nullptr || !yyjson_mut_arr_append(array, value)) json->failed = true;
    }

    yyjson_mut_val* Json_Parse(s_json_builder* json, std::string_view encoded) {
        if (json->failed) return nullptr;
        auto* document = yyjson_read(encoded.data(), encoded.size(), YYJSON_READ_NOFLAG);
        if (document == nullptr) {
            json->failed = true;
            return nullptr;
        }
        auto* value = Check_Value(json, yyjson_val_mut_copy(json->document, yyjson_doc_get_root(document)));
        yyjson_doc_free(document);
        return value;
    }

    bool Write_Json(s_json_builder* json, yyjson_mut_val* root, std::string* output) {
        output->clear();
        if (json->failed || root == nullptr) {
            json->failed = true;
            return false;
        }
        yyjson_mut_doc_set_root(json->document, root);
        size_t size = 0;
        char* encoded = yyjson_mut_write(json->document, YYJSON_WRITE_NOFLAG, &size);
        if (encoded == nullptr) {
            json->failed = true;
            return false;
        }
        output->assign(encoded, size);
        std::free(encoded);
        return true;
    }

    yyjson_mut_val* Json_Notification(s_json_builder* json, std::string_view method, yyjson_mut_val* params) {
        return Json_Object(
            json,
            {
                {"jsonrpc", Json_String_Value(json, "2.0")},
                {"method", Json_String_Value(json, method)},
                {"params", params},
            });
    }
} // namespace rpc
