#include "lua/lua_runtime.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

extern "C" {
#include <lua.h>
}

namespace {

bool Evaluate_(const char *source, const char *source_name,
               orion2re::lua::s_lua_result *result,
               const orion2re::lua::s_lua_limits *limits = nullptr)
{
    const orion2re::lua::s_lua_limits default_limits;
    return orion2re::lua::Evaluate_Module(
        source, std::strlen(source), source_name,
        limits != nullptr ? limits : &default_limits, result);
}

}

TEST_CASE("Lua allocator preserves alignment data and payload accounting") {
    using namespace orion2re::lua;
    const s_lua_limits limits;
    s_lua_result result;
    s_lua_runtime_state* state = nullptr;
    REQUIRE(Create_Runtime_State(&limits, &state, &result));
    std::unique_ptr<s_lua_runtime_state, decltype(&Destroy_Runtime_State)> runtime(
        state, Destroy_Runtime_State);
    lua_State* lua_state = Get_Runtime_State_Handle(state);
    lua_gc(lua_state, LUA_GCSTOP);

    const auto no_op = [](lua_State*, void*) { return 0; };
    REQUIRE(Execute_Runtime_Operation(state, no_op, nullptr, &result));
    const std::size_t baseline = result.remaining_allocator_bytes;
    void* allocator_state = nullptr;
    const lua_Alloc allocator = lua_getallocf(lua_state, &allocator_state);
    std::size_t size = 0;
    const auto free_block = [&](void* pointer) {
        allocator(allocator_state, pointer, size, 0);
    };
    std::unique_ptr<void, decltype(free_block)> block(nullptr, free_block);

    for (std::size_t requested : {1u, 17u, 257u, 4097u, 7u}) {
        CAPTURE(requested);
        void* resized = allocator(allocator_state, block.get(), size, requested);
        REQUIRE(resized != nullptr);
        block.release();
        block.reset(resized);
        CHECK(reinterpret_cast<std::uintptr_t>(resized) % alignof(std::max_align_t) == 0);
        const auto* bytes = static_cast<const unsigned char*>(resized);
        for (std::size_t index = 0; index < std::min(size, requested); ++index) {
            CHECK(bytes[index] == 0xA5);
        }
        size = requested;
        std::memset(resized, 0xA5, size);
        REQUIRE(Execute_Runtime_Operation(state, no_op, nullptr, &result));
        CHECK(result.remaining_allocator_bytes == baseline + size);
    }

    for (std::size_t rejected : {limits.allocator_bytes + 1,
             std::numeric_limits<std::size_t>::max()}) {
        CHECK(allocator(allocator_state, block.get(), size, rejected) == nullptr);
        const auto* bytes = static_cast<const unsigned char*>(block.get());
        for (std::size_t index = 0; index < size; ++index) {
            CHECK(bytes[index] == 0xA5);
        }
        REQUIRE(Execute_Runtime_Operation(state, no_op, nullptr, &result));
        CHECK(result.remaining_allocator_bytes == baseline + size);
    }

    block.reset();
    CHECK(allocator(allocator_state, nullptr, 0, 0) == nullptr);
    REQUIRE(Execute_Runtime_Operation(state, no_op, nullptr, &result));
    CHECK(result.remaining_allocator_bytes == baseline);
}

TEST_CASE("Lua runtime allocates aligned userdata with and without user values") {
    using namespace orion2re::lua;
    const s_lua_limits limits;
    s_lua_result result;
    s_lua_runtime_state* state = nullptr;
    REQUIRE(Create_Runtime_State(&limits, &state, &result));
    std::unique_ptr<s_lua_runtime_state, decltype(&Destroy_Runtime_State)> runtime(
        state, Destroy_Runtime_State);
    const auto create_userdata = [](lua_State* lua_state, void*) {
        for (int user_values : {0, 1, 3}) {
            void* payload = lua_newuserdatauv(lua_state, sizeof(std::max_align_t), user_values);
            // Lua 5.5.1 aligns userdata payloads with user values to
            // max_align_t (LUAI_MAXALIGN gained long double); 5.5.0
            // payloads for such userdata are only 8-byte aligned.
            // The runtime itself only creates userdata without user
            // values, which is max_align_t-aligned on both versions.
            const std::size_t expected_alignment =
                (user_values == 0 || LUA_VERSION_RELEASE_NUM >= 50501)
                    ? alignof(std::max_align_t)
                    : 8;
            CHECK(reinterpret_cast<std::uintptr_t>(payload) % expected_alignment == 0);
            std::memset(payload, 0xA5, sizeof(std::max_align_t));
            CHECK(lua_rawlen(lua_state, -1) == sizeof(std::max_align_t));
            lua_pop(lua_state, 1);
        }
        return 0;
    };
    REQUIRE(Execute_Runtime_Operation(state, create_userdata, nullptr, &result));
}

TEST_CASE("Lua runtime evaluates signed 64-bit integer modules")
{
    orion2re::lua::s_lua_result result;
    REQUIRE(Evaluate_(
        "local function midpoint(left, right)\n"
        "    return (left + right) // 2\n"
        "end\n"
        "return midpoint(-9, 4)\n",
        "mods/example/scripts/math.lua", &result));

    CHECK(result.value_kind == orion2re::lua::LUA_VALUE_INTEGER);
    CHECK(result.integer_value == -3);
    CHECK(result.remaining_allocator_bytes == 0);
}

TEST_CASE("Lua compiled profile ignores operators and numbers in text and comments")
{
    orion2re::lua::s_lua_result result;
    REQUIRE(Evaluate_(
        "-- 1.5 / 2 ^ 3\n"
        "local short = '1.5 / 2 ^ 3'\n"
        "local long = [=[1.5 / 2 ^ 3]=]\n"
        "return short == long\n",
        "text.lua", &result));

    CHECK(result.value_kind == orion2re::lua::LUA_VALUE_BOOLEAN);
    CHECK(result.boolean_value);
}

TEST_CASE("Lua sequences retain standard one-based behavior")
{
    orion2re::lua::s_lua_result result;
    REQUIRE(Evaluate_(
        "local values = { 10, 20 }\n"
        "table.insert(values, 30)\n"
        "return values[1] + values[2] + values[3] + #values\n",
        "sequences.lua", &result));

    CHECK(result.value_kind == orion2re::lua::LUA_VALUE_INTEGER);
    CHECK(result.integer_value == 63);
}

TEST_CASE("Lua runtime exposes only curated libraries")
{
    orion2re::lua::s_lua_result result;
    REQUIRE(Evaluate_(
        "return debug == nil and io == nil and os == nil and package == nil and "
        "coroutine == nil and load == nil and loadfile == nil and "
        "rawset == nil and setmetatable == nil and "
        "string.dump == nil and string.format == nil and math.abs(-7) == 7 and "
        "tonumber('42') == 42 and tonumber('1.5') == nil",
        "sandbox.lua", &result));

    CHECK(result.value_kind == orion2re::lua::LUA_VALUE_BOOLEAN);
    CHECK(result.boolean_value);
}

TEST_CASE("Lua runtime cannot produce floats through string coercion")
{
    orion2re::lua::s_lua_result result;
    CHECK_FALSE(Evaluate_("return '1.5' + 1", "coercion.lua", &result));

    CHECK(result.error == orion2re::lua::LUA_ERROR_RUNTIME);
    CHECK(result.diagnostic.find("coercion.lua:1") != std::string::npos);
}

TEST_CASE("Lua runtime keeps string methods available")
{
    orion2re::lua::s_lua_result result;
    REQUIRE(Evaluate_(
        "return ('hello'):len() == 5 and ('hello'):sub(1, 2) == 'he'",
        "string-methods.lua", &result));

    CHECK(result.value_kind == orion2re::lua::LUA_VALUE_BOOLEAN);
    CHECK(result.boolean_value);
}

TEST_CASE("Lua runtime evaluates integer arithmetic with loops and calls")
{
    orion2re::lua::s_lua_result result;
    REQUIRE(Evaluate_(
        "local function sum(n)\n"
        "    local total = 0\n"
        "    for i = 1, n do total = total + i end\n"
        "    return total\n"
        "end\n"
        "return sum(10)\n",
        "integer-arithmetic.lua", &result));

    CHECK(result.value_kind == orion2re::lua::LUA_VALUE_INTEGER);
    CHECK(result.integer_value == 55);
}

TEST_CASE("Lua runtime exposes string methods through the string table")
{
    orion2re::lua::s_lua_result result;
    REQUIRE(Evaluate_(
        "return string.len('hello') == 5 and string.sub('hello', 2, 4) == 'ell'",
        "string-table.lua", &result));

    CHECK(result.value_kind == orion2re::lua::LUA_VALUE_BOOLEAN);
    CHECK(result.boolean_value);
}

TEST_CASE("Lua runtime loads scripts using division and float literals")
{
    // The unified behaviour no longer rejects '/' or float literals at
    // load time: both builds use only the public Lua API, so there is no
    // bytecode-level profile check in either build. This pins that such
    // scripts load; it is not an endorsement of floats in game scripts.
    orion2re::lua::s_lua_result result;
    REQUIRE(Evaluate_(
        "local ratio = 1 / 2\n"
        "local unused = 1.5\n"
        "return 42\n",
        "loads-with-floats.lua", &result));

    CHECK(result.value_kind == orion2re::lua::LUA_VALUE_INTEGER);
    CHECK(result.integer_value == 42);
}

TEST_CASE("Lua runtime rejects binary chunks")
{
    const std::string bytecode("\x1bLua", 4);
    const orion2re::lua::s_lua_limits limits;
    orion2re::lua::s_lua_result result;
    CHECK_FALSE(orion2re::lua::Evaluate_Module(
        bytecode.data(), bytecode.size(), "binary.lua", &limits, &result));

    CHECK(result.error == orion2re::lua::LUA_ERROR_INVALID_SOURCE);
}

TEST_CASE("Lua runtime enforces source memory and instruction limits")
{
    SUBCASE("source")
    {
        orion2re::lua::s_lua_limits limits;
        limits.source_bytes = 4;
        orion2re::lua::s_lua_result result;
        CHECK_FALSE(Evaluate_("return 1", "large.lua", &result, &limits));
        CHECK(result.error == orion2re::lua::LUA_ERROR_SOURCE_LIMIT);
    }

    SUBCASE("memory")
    {
        orion2re::lua::s_lua_limits limits;
        limits.allocator_bytes = 32 * 1024;
        orion2re::lua::s_lua_result result;
        CHECK_FALSE(Evaluate_(
            "local values = {}\n"
            "for i = 1, 100000 do values[i] = i end\n"
            "return #values\n",
            "memory.lua", &result, &limits));
        CHECK(result.error == orion2re::lua::LUA_ERROR_MEMORY_LIMIT);
        CHECK(result.remaining_allocator_bytes == 0);
    }

    SUBCASE("non-round instruction limit")
    {
        orion2re::lua::s_lua_limits limits;
        limits.instructions = 1501;
        orion2re::lua::s_lua_result result;
        CHECK_FALSE(Evaluate_("while true do end", "loop.lua", &result, &limits));
        CHECK(result.error == orion2re::lua::LUA_ERROR_INSTRUCTION_LIMIT);
        CHECK(result.instructions_consumed == limits.instructions);
    }

    SUBCASE("zero instruction limit")
    {
        orion2re::lua::s_lua_limits limits;
        limits.instructions = 0;
        orion2re::lua::s_lua_result result;
        CHECK_FALSE(Evaluate_("return 1", "zero.lua", &result, &limits));
        CHECK(result.error == orion2re::lua::LUA_ERROR_INSTRUCTION_LIMIT);
    }

    SUBCASE("instructions")
    {
        orion2re::lua::s_lua_limits limits;
        limits.instructions = 2000;
        orion2re::lua::s_lua_result result;
        CHECK_FALSE(Evaluate_("while true do end", "loop.lua", &result, &limits));
        CHECK(result.error == orion2re::lua::LUA_ERROR_INSTRUCTION_LIMIT);
        CHECK(result.instructions_consumed == limits.instructions);
        CHECK(result.diagnostic.find("loop.lua") != std::string::npos);
        CHECK(result.remaining_allocator_bytes == 0);
    }

    SUBCASE("call depth")
    {
        orion2re::lua::s_lua_limits limits;
        limits.lua_call_depth = 20;
        orion2re::lua::s_lua_result result;
        CHECK_FALSE(Evaluate_(
            "local function recurse() return 1 + recurse() end\n"
            "return recurse()\n",
            "recursion.lua", &result, &limits));
        CHECK(result.error == orion2re::lua::LUA_ERROR_STACK_LIMIT);
        CHECK(result.diagnostic.find("recursion.lua") != std::string::npos);
        CHECK(result.remaining_allocator_bytes == 0);
    }
}

TEST_CASE("Lua runtime reports source locations for syntax and runtime errors")
{
    orion2re::lua::s_lua_result syntax;
    CHECK_FALSE(Evaluate_("local value =\n", "broken.lua", &syntax));
    CHECK(syntax.error == orion2re::lua::LUA_ERROR_SYNTAX);
    CHECK(syntax.diagnostic.find("broken.lua:2") != std::string::npos);

    orion2re::lua::s_lua_result runtime;
    CHECK_FALSE(Evaluate_("\nerror('failed')", "failure.lua", &runtime));
    CHECK(runtime.error == orion2re::lua::LUA_ERROR_RUNTIME);
    CHECK(runtime.diagnostic.find("failure.lua:2") != std::string::npos);
}

TEST_CASE("Lua activation requires a table containing callable exports")
{
    const std::vector<std::string> exports = {"run", "report"};
    const orion2re::lua::s_lua_limits limits;
    orion2re::lua::s_lua_result valid;
    const char valid_source[] =
        "return { run = function() end, report = function() end }";
    REQUIRE(orion2re::lua::Evaluate_Module_Exports(
        valid_source, sizeof(valid_source) - 1, "module.lua", exports.data(),
        exports.size(), &limits, &valid));
    CHECK(valid.remaining_allocator_bytes == 0);

    orion2re::lua::s_lua_result not_table;
    const char value_source[] = "return 1";
    CHECK_FALSE(orion2re::lua::Evaluate_Module_Exports(
        value_source, sizeof(value_source) - 1, "value.lua", exports.data(),
        exports.size(), &limits, &not_table));
    CHECK(not_table.error == orion2re::lua::LUA_ERROR_RUNTIME);
    CHECK(not_table.diagnostic.find("must return a table") != std::string::npos);

    orion2re::lua::s_lua_result missing;
    const char missing_source[] = "return { run = function() end }";
    CHECK_FALSE(orion2re::lua::Evaluate_Module_Exports(
        missing_source, sizeof(missing_source) - 1, "missing.lua",
        exports.data(), exports.size(), &limits, &missing));
    CHECK(missing.error == orion2re::lua::LUA_ERROR_RUNTIME);
    CHECK(missing.diagnostic.find("export 'report' must be a function") !=
          std::string::npos);
}