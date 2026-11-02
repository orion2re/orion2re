#include "doctest/doctest.h"

#include <cstdlib>
#include <limits>
#include <string>

#include "rpc/rpc_json.h"
#include "rpc/rpc_protocol.h"

namespace {
    struct s_allocator {
        size_t remaining = 0;
        size_t live = 0;
    };

    void* Allocate(void* context, size_t size) {
        auto* state = (s_allocator*)context;
        if (state->remaining == 0) return nullptr;
        --state->remaining;
        void* result = std::malloc(size);
        if (result != nullptr) ++state->live;
        return result;
    }

    void* Reallocate(void* context, void* pointer, size_t old_size, size_t size) {
        (void)old_size;
        auto* state = (s_allocator*)context;
        if (state->remaining == 0) return nullptr;
        --state->remaining;
        return std::realloc(pointer, size);
    }

    void Free(void* context, void* pointer) {
        if (pointer != nullptr) --((s_allocator*)context)->live;
        std::free(pointer);
    }
} // namespace

DOCTEST_TEST_CASE("RPC JSON construction copies strings and keys and preserves value types and order") {
    rpc::s_json_builder json;
    rpc::Initialize_Json(&json);
    std::string key = "dynamic-key";
    std::string text = "quote\" slash\\ line\n\t";
    text.push_back('\0');
    text += "\xc3\xa9";
    const std::string expected_text = text;
    auto* root = rpc::Json_Object(&json);
    rpc::Json_Add(&json, root, key, rpc::Json_String_Value(&json, text));
    key.assign("changed-key");
    text.assign("changed-text");
    rpc::Json_Add(&json, root, "temporary", rpc::Json_String_Value(&json, std::to_string(UINT64_MAX)));
    auto* array = rpc::Json_Array(&json);
    rpc::Json_Append(&json, array, rpc::Json_Integer(&json, INT64_MIN));
    rpc::Json_Append(&json, array, rpc::Json_Unsigned(&json, UINT64_MAX));
    rpc::Json_Append(&json, array, rpc::Json_Boolean(&json, false));
    rpc::Json_Append(&json, array, rpc::Json_Null(&json));
    rpc::Json_Append(&json, array, rpc::Json_Array(&json));
    rpc::Json_Append(&json, array, rpc::Json_Object(&json));
    rpc::Json_Add(&json, root, "values", array);
    std::string imported = "{\"availability\":\"current\",\"value\":\"copied\"}";
    rpc::Json_Add(&json, root, "knowledge", rpc::Json_Parse(&json, imported));
    imported.assign(imported.size(), 'x');

    std::string encoded;
    DOCTEST_CHECK(rpc::Write_Json(&json, root, &encoded));
    std::string repeated;
    DOCTEST_CHECK(rpc::Write_Json(&json, root, &repeated));
    DOCTEST_CHECK(encoded == repeated);
    rpc::Shutdown_Json(&json);
    DOCTEST_CHECK(json.document == nullptr);
    auto* document = yyjson_read(encoded.data(), encoded.size(), YYJSON_READ_NOFLAG);
    DOCTEST_REQUIRE(document != nullptr);
    auto* object = yyjson_doc_get_root(document);
    auto* value = yyjson_obj_get(object, "dynamic-key");
    DOCTEST_CHECK(yyjson_is_str(value));
    DOCTEST_CHECK(std::string(yyjson_get_str(value), yyjson_get_len(value)) == expected_text);
    DOCTEST_CHECK(yyjson_obj_get(object, "changed-key") == nullptr);
    DOCTEST_CHECK(encoded.find("dynamic-key") < encoded.find("temporary"));
    DOCTEST_CHECK(encoded.find("temporary") < encoded.find("values"));
    auto* values = yyjson_obj_get(object, "values");
    DOCTEST_CHECK(yyjson_get_sint(yyjson_arr_get(values, 0)) == INT64_MIN);
    DOCTEST_CHECK(yyjson_get_uint(yyjson_arr_get(values, 1)) == UINT64_MAX);
    DOCTEST_CHECK(yyjson_is_false(yyjson_arr_get(values, 2)));
    DOCTEST_CHECK(yyjson_is_null(yyjson_arr_get(values, 3)));
    DOCTEST_CHECK(yyjson_is_arr(yyjson_arr_get(values, 4)));
    DOCTEST_CHECK(yyjson_is_obj(yyjson_arr_get(values, 5)));
    DOCTEST_CHECK(yyjson_get_len(yyjson_obj_get(object, "temporary")) == 20);
    DOCTEST_CHECK(encoded.find(imported) == std::string::npos);
    DOCTEST_CHECK(encoded.find("\"value\":\"copied\"") != std::string::npos);
    yyjson_doc_free(document);
}

DOCTEST_TEST_CASE("RPC JSON allocation failures are sticky and never serialize partial responses") {
    bool failed = false;
    bool succeeded = false;
    for (size_t budget = 0; budget < 16; ++budget) {
        s_allocator state{budget};
        const yyjson_alc allocator = {Allocate, Reallocate, Free, &state};
        rpc::s_json_builder json;
        rpc::Initialize_Json(&json, &allocator);
        auto* root = rpc::Json_Object(
            &json,
            {
                {"first", rpc::Json_Integer(&json, 42)},
                {"large", rpc::Json_String_Value(&json, std::string(32768, 'x'))},
            });
        if (json.failed) {
            failed = true;
            state.remaining = std::numeric_limits<size_t>::max();
            DOCTEST_CHECK(rpc::Json_Boolean(&json, true) == nullptr);
            std::string output = "stale";
            DOCTEST_CHECK_FALSE(rpc::Write_Json(&json, root, &output));
            DOCTEST_CHECK(output.empty());
            rpc::s_request request;
            request.id_json = "7";
            DOCTEST_CHECK(rpc::Result_Response(&json, &request, root) == rpc::Error_Response(&request, rpc::RPC_INTERNAL_ERROR));
        } else {
            succeeded = true;
            std::string output;
            DOCTEST_CHECK(rpc::Write_Json(&json, root, &output));
            DOCTEST_CHECK(output.find("\"first\":42") != std::string::npos);
        }
        rpc::Shutdown_Json(&json);
        DOCTEST_CHECK(state.live == 0);
    }
    DOCTEST_CHECK(failed);
    DOCTEST_CHECK(succeeded);
}

DOCTEST_TEST_CASE("RPC JSON invalid nodes input and UTF-8 fail closed") {
    rpc::s_json_builder json;
    rpc::Initialize_Json(&json);
    auto* root = rpc::Json_Object(&json);
    rpc::Json_Add(&json, root, "missing", nullptr);
    std::string output = "stale";
    DOCTEST_CHECK_FALSE(rpc::Write_Json(&json, root, &output));
    DOCTEST_CHECK(output.empty());
    rpc::Shutdown_Json(&json);

    rpc::Initialize_Json(&json);
    rpc::Json_Append(&json, rpc::Json_Object(&json), rpc::Json_Boolean(&json, true));
    DOCTEST_CHECK(json.failed);
    rpc::Shutdown_Json(&json);

    rpc::Initialize_Json(&json);
    DOCTEST_CHECK(rpc::Json_Parse(&json, "{broken") == nullptr);
    DOCTEST_CHECK(json.failed);
    rpc::Shutdown_Json(&json);

    rpc::Initialize_Json(&json);
    root = rpc::Json_String_Value(&json, "\xff");
    DOCTEST_CHECK_FALSE(rpc::Write_Json(&json, root, &output));
    DOCTEST_CHECK(json.failed);
    DOCTEST_CHECK(output.empty());
    rpc::Shutdown_Json(&json);
}

DOCTEST_TEST_CASE("RPC JSON envelopes preserve string and integer correlation without encoding nodes twice") {
    for (const char* id : {"\"a\\\"b\"", "-9007199254740991"}) {
        rpc::s_request request;
        request.id_json = id;
        rpc::s_json_builder json;
        rpc::Initialize_Json(&json);
        auto* result = rpc::Json_Object(
            &json,
            {
                {"items", rpc::Json_Array(&json)},
                {"revision", rpc::Json_String_Value(&json, "18446744073709551615")},
            });
        const std::string response = rpc::Result_Response(&json, &request, result);
        rpc::Shutdown_Json(&json);
        DOCTEST_CHECK(response == "{\"jsonrpc\":\"2.0\",\"id\":" + request.id_json + ",\"result\":{\"items\":[],\"revision\":\"18446744073709551615\"}}");
        DOCTEST_CHECK(rpc::Result_Response(&request, "{broken") == rpc::Error_Response(&request, rpc::RPC_INTERNAL_ERROR));
    }
}
