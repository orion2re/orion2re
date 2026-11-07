#include "lua/lua_runtime.h"

#include <algorithm>
#include <charconv>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace orion2re::lua {
    struct s_lua_runtime_state {
        s_lua_limits limits;
        lua_State* lua_state = nullptr;
        std::size_t allocated = 0;
        std::size_t peak_allocated = 0;
        std::uint64_t instructions_consumed = 0;
        int hook_interval = 1;
        bool memory_exhausted = false;
        bool instruction_exhausted = false;
        bool stack_exhausted = false;
        e_lua_error nested_load_error = LUA_ERROR_NONE;
        std::string nested_load_diagnostic;
        const char* invoked_export = nullptr;
        int invoked_reference = LUA_NOREF;
        t_lua_state_operation operation = nullptr;
        void* operation_user_data = nullptr;
        t_lua_argument_builder build_arguments = nullptr;
        void* argument_user_data = nullptr;
        t_lua_result_consumer consume_results = nullptr;
        void* result_user_data = nullptr;
    };

    namespace {

        static_assert(sizeof(lua_Integer) == sizeof(std::int64_t));
        static_assert(LUA_MAXINTEGER == INT64_MAX);
        static_assert(LUA_MININTEGER == INT64_MIN);

        // Keep the payload after this header aligned for every Lua object,
        // including userdata whose alignment can exceed that of size_t.
        struct alignas(std::max_align_t) s_allocation_header {
            std::size_t size;
        };

        struct s_profile_error {
            e_lua_error error = LUA_ERROR_NONE;
            int line = 1;
            const char* message = nullptr;
        };

        enum e_runtime_operation {
            RUNTIME_EVALUATE_MODULE,
            RUNTIME_VALIDATE_EXPORTS,
            RUNTIME_INVOKE_EXPORT,
        };

        struct s_runtime_operation {
            e_runtime_operation kind = RUNTIME_EVALUATE_MODULE;
            const std::string* required_exports = nullptr;
            std::size_t required_export_count = 0;
            const char* invoked_export = nullptr;
            t_lua_argument_builder build_arguments = nullptr;
            void* argument_user_data = nullptr;
        };

        bool Is_Ascii_Space(char value) {
            return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
                value == '\f' || value == '\v';
        }

        bool Valid_Utf8(const char* source, std::size_t source_size) {
            std::size_t position = 0;
            while (position < source_size) {
                const auto first = (unsigned char)source[position++];
                if (first == 0) {
                    return false;
                }
                if (first < 0x80) {
                    continue;
                }

                int continuation_count = 0;
                std::uint32_t codepoint = 0;
                if (first >= 0xc2 && first <= 0xdf) {
                    continuation_count = 1;
                    codepoint = first & 0x1f;
                } else if (first >= 0xe0 && first <= 0xef) {
                    continuation_count = 2;
                    codepoint = first & 0x0f;
                } else if (first >= 0xf0 && first <= 0xf4) {
                    continuation_count = 3;
                    codepoint = first & 0x07;
                } else {
                    return false;
                }

                if (position + continuation_count > source_size) {
                    return false;
                }
                for (int index = 0; index < continuation_count; ++index) {
                    const auto continuation = (unsigned char)source[position++];
                    if ((continuation & 0xc0) != 0x80) {
                        return false;
                    }
                    codepoint = (codepoint << 6) | (continuation & 0x3f);
                }

                if ((continuation_count == 2 && codepoint < 0x800) ||
                    (continuation_count == 3 && codepoint < 0x10000) ||
                    codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
                    return false;
                }
            }
            return true;
        }

        void Set_Profile_Error(s_profile_error* error, e_lua_error code, int line, const char* message) {
            error->error = code;
            error->line = line;
            error->message = message;
        }

        bool Validate_Source(const char* source, std::size_t source_size, s_profile_error* error) {
            if (!Valid_Utf8(source, source_size)) {
                Set_Profile_Error(error, LUA_ERROR_INVALID_SOURCE, 1, "source must be non-NUL UTF-8 text");
                return false;
            }
            if (source_size != 0 && (unsigned char)source[0] == 0x1b) {
                Set_Profile_Error(error, LUA_ERROR_INVALID_SOURCE, 1, "external bytecode is unavailable");
                return false;
            }
            *error = {};
            return true;
        }

        // The language profile is enforced only where the public Lua API
        // permits (see Remove_String_Arithmetic_Metamethods and
        // Checked_Tonumber). Bytecode-level checks were removed from both
        // builds: they needed the Lua internals (Proto, opcodes,
        // luaG_getfuncline), which the system library does not ship, and the
        // two builds must behave identically. Validate_Loaded_Chunk is kept
        // as a stub so the load path is unchanged.
        bool Validate_Loaded_Chunk(lua_State*, s_profile_error*) {
            return true;
        }

        void* Limited_Allocator(void* user_data, void* pointer, std::size_t, std::size_t new_size) {
            auto& state = *(s_lua_runtime_state*)user_data;
            auto* header = pointer == nullptr
                ? nullptr
                : (s_allocation_header*)pointer - 1;
            const std::size_t old_size = header == nullptr ? 0 : header->size;

            if (new_size == 0) {
                std::free(header);
                state.allocated -= old_size;
                return nullptr;
            }
            if (new_size > std::numeric_limits<std::size_t>::max() - sizeof(s_allocation_header) ||
                new_size > state.limits.allocator_bytes - std::min(state.allocated, old_size)) {
                state.memory_exhausted = true;
                return nullptr;
            }

            auto* new_header = (s_allocation_header*)std::realloc(
                header, sizeof(s_allocation_header) + new_size);
            if (new_header == nullptr) {
                state.memory_exhausted = true;
                return nullptr;
            }
            new_header->size = new_size;
            state.allocated = state.allocated - old_size + new_size;
            state.peak_allocated = std::max(state.peak_allocated, state.allocated);
            return new_header + 1;
        }

        s_lua_runtime_state& Invocation_State(lua_State* lua_state) {
            return **(s_lua_runtime_state**)lua_getextraspace(lua_state);
        }

        void Instruction_Hook(lua_State* lua_state, lua_Debug* debug) {
            auto& state = Invocation_State(lua_state);
            lua_getinfo(lua_state, "Sl", debug);
            if (debug->event == LUA_HOOKCOUNT) {
                state.instructions_consumed += (std::uint64_t)state.hook_interval;
                if (state.instructions_consumed >= state.limits.instructions) {
                    state.instruction_exhausted = true;
                    luaL_error(lua_state,
                        "%s:%d: instruction limit exceeded (%llu instructions)",
                        debug->short_src,
                        debug->currentline,
                        (unsigned long long)state.limits.instructions);
                }
                const std::uint64_t remaining =
                    state.limits.instructions - state.instructions_consumed;
                state.hook_interval =
                    (int)std::min<std::uint64_t>(remaining, 1000);
                lua_sethook(lua_state, Instruction_Hook, LUA_MASKCALL | LUA_MASKCOUNT, state.hook_interval);
            } else if (debug->event == LUA_HOOKCALL &&
                lua_getstack(lua_state, (int)state.limits.lua_call_depth, debug)) {
                state.stack_exhausted = true;
                luaL_error(lua_state, "%s:%d: Lua call depth limit exceeded (%u calls)", debug->short_src, debug->currentline, state.limits.lua_call_depth);
            }
        }

        bool Parse_Integer(const char* text, std::size_t text_size, int base, lua_Integer* result) {
            std::size_t begin = 0;
            std::size_t end = text_size;
            while (begin < end && Is_Ascii_Space(text[begin])) {
                ++begin;
            }
            while (end > begin && Is_Ascii_Space(text[end - 1])) {
                --end;
            }

            bool negative = false;
            if (begin < end && (text[begin] == '+' || text[begin] == '-')) {
                negative = text[begin] == '-';
                ++begin;
            }
            if (begin == end) {
                return false;
            }
            if (base == 10 && end - begin > 2 && text[begin] == '0' &&
                (text[begin + 1] == 'x' || text[begin + 1] == 'X')) {
                base = 16;
                begin += 2;
            }

            std::uint64_t magnitude = 0;
            const auto conversion = std::from_chars(text + begin, text + end, magnitude, base);
            if (conversion.ec != std::errc() || conversion.ptr != text + end) {
                return false;
            }
            const std::uint64_t maximum = negative
                ? (std::uint64_t)INT64_MAX + 1
                : (std::uint64_t)INT64_MAX;
            if (magnitude > maximum) {
                return false;
            }
            if (negative && magnitude == maximum) {
                *result = LUA_MININTEGER;
            } else {
                *result = (lua_Integer)magnitude;
                if (negative) {
                    *result = -*result;
                }
            }
            return true;
        }

        int Checked_Tonumber(lua_State* lua_state) {
            if (lua_isinteger(lua_state, 1) && lua_isnoneornil(lua_state, 2)) {
                lua_settop(lua_state, 1);
                return 1;
            }
            std::size_t length = 0;
            const char* text = luaL_checklstring(lua_state, 1, &length);
            const int base = (int)luaL_optinteger(lua_state, 2, 10);
            luaL_argcheck(lua_state, base >= 2 && base <= 36, 2, "base out of range");

            lua_Integer value = 0;
            if (!Parse_Integer(text, length, base, &value)) {
                lua_pushnil(lua_state);
            } else {
                lua_pushinteger(lua_state, value);
            }
            return 1;
        }

        int Checked_Abs(lua_State* lua_state) {
            const lua_Integer value = luaL_checkinteger(lua_state, 1);
            luaL_argcheck(lua_state, value != LUA_MININTEGER, 1, "absolute value overflows a 64-bit integer");
            lua_pushinteger(lua_state, value < 0 ? -value : value);
            return 1;
        }

        int Integer_Min(lua_State* lua_state) {
            const int count = lua_gettop(lua_state);
            luaL_argcheck(lua_state, count > 0, 1, "value expected");
            lua_Integer value = luaL_checkinteger(lua_state, 1);
            for (int index = 2; index <= count; ++index) {
                value = std::min(value, luaL_checkinteger(lua_state, index));
            }
            lua_pushinteger(lua_state, value);
            return 1;
        }

        int Integer_Max(lua_State* lua_state) {
            const int count = lua_gettop(lua_state);
            luaL_argcheck(lua_state, count > 0, 1, "value expected");
            lua_Integer value = luaL_checkinteger(lua_state, 1);
            for (int index = 2; index <= count; ++index) {
                value = std::max(value, luaL_checkinteger(lua_state, index));
            }
            lua_pushinteger(lua_state, value);
            return 1;
        }

        void Remove_Field(lua_State* lua_state, int table_index, const char* field) {
            table_index = lua_absindex(lua_state, table_index);
            lua_pushnil(lua_state);
            lua_setfield(lua_state, table_index, field);
        }

        // The vendored Lua build defined LUA_NOCVTS2N, which removes the
        // string library's arithmetic metamethods so '1.5' + 1 errors
        // instead of coercing to a float. A stock system Lua ships those
        // metamethods, so remove them from the shared string metatable
        // here, through the public API, to keep the integer-only profile
        // identical with either library. String methods stay available
        // through __index, and comparisons never coerced in either build.
        void Remove_String_Arithmetic_Metamethods(lua_State* lua_state) {
            lua_pushliteral(lua_state, "");
            if (lua_getmetatable(lua_state, -1) == 0) {
                lua_pop(lua_state, 1);
                return;
            }
            if (!lua_istable(lua_state, -1)) {
                lua_pop(lua_state, 2);
                return;
            }
            static const char* const fields[] = {
                "__add", "__sub", "__mul", "__mod",
                "__pow", "__div", "__idiv", "__unm",
            };
            for (const char* field : fields) {
                lua_pushnil(lua_state);
                lua_setfield(lua_state, -2, field);
            }
            lua_pop(lua_state, 2);
        }

        void Open_Libraries(lua_State* lua_state) {
            luaL_requiref(lua_state, LUA_GNAME, luaopen_base, 1);
            Remove_Field(lua_state, -1, "collectgarbage");
            Remove_Field(lua_state, -1, "dofile");
            Remove_Field(lua_state, -1, "load");
            Remove_Field(lua_state, -1, "loadfile");
            Remove_Field(lua_state, -1, "print");
            Remove_Field(lua_state, -1, "rawset");
            Remove_Field(lua_state, -1, "setmetatable");
            Remove_Field(lua_state, -1, "warn");
            lua_pushcfunction(lua_state, Checked_Tonumber);
            lua_setfield(lua_state, -2, "tonumber");
            lua_pop(lua_state, 1);

            luaL_requiref(lua_state, LUA_TABLIBNAME, luaopen_table, 1);
            lua_pop(lua_state, 1);

            luaL_requiref(lua_state, LUA_STRLIBNAME, luaopen_string, 1);
            Remove_Field(lua_state, -1, "dump");
            Remove_Field(lua_state, -1, "format");
            Remove_Field(lua_state, -1, "lower");
            Remove_Field(lua_state, -1, "pack");
            Remove_Field(lua_state, -1, "packsize");
            Remove_Field(lua_state, -1, "unpack");
            Remove_Field(lua_state, -1, "upper");
            lua_pop(lua_state, 1);
            Remove_String_Arithmetic_Metamethods(lua_state);

            lua_newtable(lua_state);
            lua_pushcfunction(lua_state, Checked_Abs);
            lua_setfield(lua_state, -2, "abs");
            lua_pushcfunction(lua_state, Integer_Min);
            lua_setfield(lua_state, -2, "min");
            lua_pushcfunction(lua_state, Integer_Max);
            lua_setfield(lua_state, -2, "max");
            lua_pushinteger(lua_state, LUA_MININTEGER);
            lua_setfield(lua_state, -2, "mininteger");
            lua_pushinteger(lua_state, LUA_MAXINTEGER);
            lua_setfield(lua_state, -2, "maxinteger");
            lua_setglobal(lua_state, LUA_MATHLIBNAME);
        }

        int Invoke_Export(lua_State* lua_state) {
            auto& state = Invocation_State(lua_state);
            luaL_checktype(lua_state, 1, LUA_TTABLE);
            lua_pushstring(lua_state, state.invoked_export);
            lua_rawget(lua_state, 1);
            if (!lua_isfunction(lua_state, -1)) {
                lua_pushliteral(lua_state, "module export '");
                lua_pushstring(lua_state, state.invoked_export);
                lua_pushliteral(lua_state, "' must be a function");
                lua_concat(lua_state, 3);
                return lua_error(lua_state);
            }
            const int initial_top = lua_gettop(lua_state);
            int argument_count = 0;
            try {
                argument_count = state.build_arguments == nullptr
                    ? 0
                    : state.build_arguments(lua_state,
                          state.argument_user_data);
            } catch (...) {
                return luaL_error(lua_state,
                    "exception while building Lua invocation arguments");
            }
            if (argument_count < 0 ||
                lua_gettop(lua_state) - initial_top != argument_count) {
                return luaL_error(lua_state, "unable to build Lua invocation arguments");
            }
            lua_call(lua_state, argument_count, 0);
            return 0;
        }

        int Invoke_Reference_Results(lua_State* lua_state) {
            auto& state = Invocation_State(lua_state);
            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, state.invoked_reference);
            if (!lua_isfunction(lua_state, -1)) {
                return luaL_error(lua_state,
                    "invalid persistent Lua function reference");
            }
            const int argument_count = state.build_arguments == nullptr
                ? 0
                : state.build_arguments(lua_state, state.argument_user_data);
            if (argument_count < 0 || lua_gettop(lua_state) != argument_count + 1) {
                return luaL_error(lua_state,
                    "unable to build Lua invocation arguments");
            }
            lua_call(lua_state, argument_count, LUA_MULTRET);
            const int result_count = lua_gettop(lua_state);
            const int consumed = state.consume_results == nullptr
                ? 0
                : state.consume_results(lua_state, result_count, state.result_user_data);
            if (consumed != 0) {
                return lua_error(lua_state);
            }
            lua_settop(lua_state, 0);
            return 0;
        }

        int Execute_Operation(lua_State* lua_state) {
            auto& state = Invocation_State(lua_state);
            return state.operation(lua_state, state.operation_user_data);
        }

        int Invoke_Reference(lua_State* lua_state) {
            auto& state = Invocation_State(lua_state);
            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, state.invoked_reference);
            if (!lua_isfunction(lua_state, -1)) {
                return luaL_error(lua_state, "invalid persistent Lua callback reference");
            }
            const int initial_top = lua_gettop(lua_state);
            int argument_count = 0;
            try {
                argument_count = state.build_arguments == nullptr
                    ? 0
                    : state.build_arguments(lua_state,
                          state.argument_user_data);
            } catch (...) {
                return luaL_error(lua_state,
                    "exception while building Lua invocation arguments");
            }
            if (argument_count < 0 ||
                lua_gettop(lua_state) - initial_top != argument_count) {
                return luaL_error(lua_state, "unable to build Lua invocation arguments");
            }
            lua_call(lua_state, argument_count, 0);
            return 0;
        }

        int Initialize_Environment(lua_State* lua_state) {
            Open_Libraries(lua_state);
            return 0;
        }

        std::string Diagnostic_Text(lua_State* lua_state,
            const s_lua_limits* limits) {
            std::size_t length = 0;
            const char* message = lua_tolstring(lua_state, -1, &length);
            if (message == nullptr) {
                return "Lua failed without a string diagnostic";
            }
            length = std::min(length, limits->diagnostic_bytes);
            return std::string(message, length);
        }

        e_lua_error Status_Error(int status, const s_lua_runtime_state* state) {
            if (state->instruction_exhausted) {
                return LUA_ERROR_INSTRUCTION_LIMIT;
            }
            if (state->memory_exhausted || status == LUA_ERRMEM) {
                return LUA_ERROR_MEMORY_LIMIT;
            }
            if (state->stack_exhausted) {
                return LUA_ERROR_STACK_LIMIT;
            }
            if (state->nested_load_error != LUA_ERROR_NONE) {
                return state->nested_load_error;
            }
            if (status == LUA_ERRSYNTAX) {
                return LUA_ERROR_SYNTAX;
            }
            return LUA_ERROR_RUNTIME;
        }

        bool Validate_Runtime_Input(const char* source, std::size_t source_size, const char* source_name, const s_lua_limits* limits, s_lua_result* result) {
            if (source == nullptr || source_name == nullptr || limits == nullptr) {
                result->error = LUA_ERROR_INVALID_SOURCE;
                result->diagnostic = "invalid Lua runtime input";
                return false;
            }
            if (source_size > limits->source_bytes) {
                result->error = LUA_ERROR_SOURCE_LIMIT;
                result->diagnostic = "source byte limit exceeded";
                return false;
            }
            s_profile_error source_error;
            if (!Validate_Source(source, source_size, &source_error)) {
                result->error = source_error.error;
                result->diagnostic.assign(source_name);
                result->diagnostic += ':' + std::to_string(source_error.line) + ": ";
                result->diagnostic += source_error.message;
                if (result->diagnostic.size() > limits->diagnostic_bytes) {
                    result->diagnostic.resize(limits->diagnostic_bytes);
                }
                return false;
            }
            return true;
        }

        bool Validate_Limits(const s_lua_limits* limits, s_lua_result* result) {
            if (limits->instructions == 0) {
                result->error = LUA_ERROR_INSTRUCTION_LIMIT;
                result->diagnostic = "Lua instruction limit must be nonzero";
                return false;
            }
            if (limits->allocator_bytes < sizeof(s_allocation_header)) {
                result->error = LUA_ERROR_MEMORY_LIMIT;
                result->diagnostic = "Lua allocator limit is too small";
                return false;
            }
            return true;
        }

        void Begin_Invocation(s_lua_runtime_state* state) {
            state->peak_allocated = state->allocated;
            state->instructions_consumed = 0;
            state->memory_exhausted = false;
            state->instruction_exhausted = false;
            state->stack_exhausted = false;
            state->nested_load_error = LUA_ERROR_NONE;
            state->nested_load_diagnostic.clear();
            state->hook_interval =
                (int)std::min<std::uint64_t>(state->limits.instructions, 1000);
            lua_sethook(state->lua_state, Instruction_Hook, LUA_MASKCALL | LUA_MASKCOUNT, state->hook_interval);
        }

        void Complete_Invocation(const s_lua_runtime_state* state,
            s_lua_result* result) {
            result->peak_allocator_bytes = state->peak_allocated;
            result->remaining_allocator_bytes = state->allocated;
            result->instructions_consumed = state->instructions_consumed;
        }

        void Capture_Result(lua_State* lua_state, s_lua_result* result) {
            switch (lua_type(lua_state, -1)) {
                case LUA_TNIL:
                    result->value_kind = LUA_VALUE_NIL;
                    break;
                case LUA_TBOOLEAN:
                    result->value_kind = LUA_VALUE_BOOLEAN;
                    result->boolean_value = lua_toboolean(lua_state, -1) != 0;
                    break;
                case LUA_TNUMBER:
                    if (lua_isinteger(lua_state, -1)) {
                        result->value_kind = LUA_VALUE_INTEGER;
                        result->integer_value = (std::int64_t)lua_tointeger(lua_state, -1);
                    } else {
                        result->error = LUA_ERROR_LANGUAGE_PROFILE;
                        result->diagnostic = "module returned a floating-point value";
                    }
                    break;
                case LUA_TSTRING: {
                    result->value_kind = LUA_VALUE_STRING;
                    std::size_t length = 0;
                    const char* value = lua_tolstring(lua_state, -1, &length);
                    result->string_value.assign(value, length);
                    break;
                }
                default:
                    result->value_kind = LUA_VALUE_OTHER;
                    break;
            }
        }

        void Set_Activation_Error(s_lua_result* result, const char* source_name, const std::string& message, const s_lua_limits* limits) {
            result->error = LUA_ERROR_RUNTIME;
            result->diagnostic.assign(source_name);
            result->diagnostic += ": ";
            result->diagnostic += message;
            if (result->diagnostic.size() > limits->diagnostic_bytes) {
                result->diagnostic.resize(limits->diagnostic_bytes);
            }
        }

        void Validate_Module_Exports(lua_State* lua_state,
            const std::string* required_exports,
            std::size_t required_export_count,
            const char* source_name,
            const s_lua_limits* limits,
            s_lua_result* result) {
            if (!lua_istable(lua_state, -1)) {
                Set_Activation_Error(result, source_name, "module must return a table", limits);
                return;
            }
            result->value_kind = LUA_VALUE_OTHER;
            for (std::size_t index = 0; index < required_export_count; ++index) {
                const std::string& name = required_exports[index];
                lua_pushlstring(lua_state, name.data(), name.size());
                lua_rawget(lua_state, -2);
                const bool callable = lua_isfunction(lua_state, -1);
                lua_pop(lua_state, 1);
                if (!callable) {
                    Set_Activation_Error(
                        result, source_name, "module export '" + name + "' must be a function", limits);
                    return;
                }
            }
        }

        bool Run_Module(const char* source, std::size_t source_size, const char* source_name, const s_runtime_operation* operation, const s_lua_limits* limits, s_lua_result* result) {
            *result = {};
            if (source == nullptr || source_name == nullptr || operation == nullptr ||
                limits == nullptr) {
                result->error = LUA_ERROR_INVALID_SOURCE;
                result->diagnostic = "invalid Lua runtime input";
                return false;
            }
            if (source_size > limits->source_bytes) {
                result->error = LUA_ERROR_SOURCE_LIMIT;
                result->diagnostic = "source byte limit exceeded";
                return false;
            }

            s_profile_error source_error;
            if (!Validate_Source(source, source_size, &source_error)) {
                result->error = source_error.error;
                result->diagnostic.assign(source_name);
                result->diagnostic += ':' + std::to_string(source_error.line) + ": ";
                result->diagnostic += source_error.message;
                if (result->diagnostic.size() > limits->diagnostic_bytes) {
                    result->diagnostic.resize(limits->diagnostic_bytes);
                }
                return false;
            }

            s_lua_runtime_state state;
            state.limits = *limits;
            state.hook_interval =
                (int)std::min<std::uint64_t>(limits->instructions, 1000);
            if (limits->instructions == 0) {
                result->error = LUA_ERROR_INSTRUCTION_LIMIT;
                result->diagnostic = "Lua instruction limit must be nonzero";
                return false;
            }
            if (limits->allocator_bytes < sizeof(s_allocation_header)) {
                result->error = LUA_ERROR_MEMORY_LIMIT;
                result->diagnostic = "Lua allocator limit is too small";
                return false;
            }

            lua_State* lua_state = lua_newstate(Limited_Allocator, &state, 0);
            if (lua_state == nullptr) {
                result->error = LUA_ERROR_MEMORY_LIMIT;
                result->diagnostic = "memory limit exceeded while creating Lua state";
                result->peak_allocator_bytes = state.peak_allocated;
                return false;
            }
            *(s_lua_runtime_state**)lua_getextraspace(lua_state) = &state;
            lua_sethook(lua_state, Instruction_Hook, LUA_MASKCALL | LUA_MASKCOUNT, state.hook_interval);

            lua_pushcfunction(lua_state, Initialize_Environment);
            int status = lua_pcall(lua_state, 0, 0, 0);
            std::string chunk_name = "@";
            chunk_name += source_name;
            if (status == LUA_OK) {
                status = luaL_loadbufferx(lua_state, source, source_size, chunk_name.c_str(), "t");
            }
            if (status == LUA_OK) {
                s_profile_error profile_error;
                if (!Validate_Loaded_Chunk(lua_state, &profile_error)) {
                    result->error = profile_error.error;
                    result->diagnostic.assign(source_name);
                    if (profile_error.line >= 0) {
                        result->diagnostic += ':' + std::to_string(profile_error.line);
                    }
                    result->diagnostic += ": ";
                    result->diagnostic += profile_error.message;
                    if (result->diagnostic.size() > limits->diagnostic_bytes) {
                        result->diagnostic.resize(limits->diagnostic_bytes);
                    }
                } else {
                    status = lua_pcall(lua_state, 0, 1, 0);
                }
            }
            if (result->error == LUA_ERROR_NONE) {
                if (status != LUA_OK) {
                    result->error = Status_Error(status, &state);
                    result->diagnostic = Diagnostic_Text(lua_state, limits);
                } else if (operation->kind == RUNTIME_VALIDATE_EXPORTS) {
                    Validate_Module_Exports(lua_state, operation->required_exports, operation->required_export_count, source_name, limits, result);
                } else if (operation->kind == RUNTIME_INVOKE_EXPORT) {
                    if (!lua_istable(lua_state, -1)) {
                        Set_Activation_Error(result, source_name, "module must return a table", limits);
                    } else {
                        state.invoked_export = operation->invoked_export;
                        state.build_arguments = operation->build_arguments;
                        state.argument_user_data = operation->argument_user_data;
                        lua_pushcfunction(lua_state, Invoke_Export);
                        lua_insert(lua_state, -2);
                        status = lua_pcall(lua_state, 1, 0, 0);
                        if (status != LUA_OK) {
                            result->error = Status_Error(status, &state);
                            result->diagnostic = Diagnostic_Text(lua_state, limits);
                        } else {
                            result->value_kind = LUA_VALUE_NIL;
                        }
                    }
                } else {
                    Capture_Result(lua_state, result);
                }
            }

            result->peak_allocator_bytes = state.peak_allocated;
            result->instructions_consumed = state.instructions_consumed;
            lua_close(lua_state);
            result->remaining_allocator_bytes = state.allocated;
            return result->error == LUA_ERROR_NONE;
        }

    } // namespace

    bool Evaluate_Module(const char* source, std::size_t source_size, const char* source_name, const s_lua_limits* limits, s_lua_result* result) {
        if (result == nullptr) {
            return false;
        }
        const s_runtime_operation operation;
        return Run_Module(source, source_size, source_name, &operation, limits, result);
    }

    bool Evaluate_Module_Exports(const char* source, std::size_t source_size, const char* source_name, const std::string* required_exports, std::size_t required_export_count, const s_lua_limits* limits, s_lua_result* result) {
        if (result == nullptr ||
            (required_export_count != 0 && required_exports == nullptr)) {
            return false;
        }
        s_runtime_operation operation;
        operation.kind = RUNTIME_VALIDATE_EXPORTS;
        operation.required_exports = required_exports;
        operation.required_export_count = required_export_count;
        return Run_Module(source, source_size, source_name, &operation, limits, result);
    }

    bool Invoke_Module_Export(const char* source, std::size_t source_size, const char* source_name, const char* export_name, t_lua_argument_builder build_arguments, void* user_data, const s_lua_limits* limits, s_lua_result* result) {
        if (result == nullptr || export_name == nullptr) {
            return false;
        }
        s_runtime_operation operation;
        operation.kind = RUNTIME_INVOKE_EXPORT;
        operation.invoked_export = export_name;
        operation.build_arguments = build_arguments;
        operation.argument_user_data = user_data;
        return Run_Module(source, source_size, source_name, &operation, limits, result);
    }

    bool Create_Runtime_State(const s_lua_limits* limits,
        s_lua_runtime_state** runtime,
        s_lua_result* result) {
        if (runtime == nullptr || result == nullptr || limits == nullptr) {
            return false;
        }
        *runtime = nullptr;
        *result = {};
        if (!Validate_Limits(limits, result)) {
            return false;
        }

        auto* state = new (std::nothrow) s_lua_runtime_state;
        if (state == nullptr) {
            result->error = LUA_ERROR_MEMORY_LIMIT;
            result->diagnostic = "unable to allocate Lua runtime state";
            return false;
        }
        state->limits = *limits;
        state->lua_state = lua_newstate(Limited_Allocator, state, 0);
        if (state->lua_state == nullptr) {
            result->error = LUA_ERROR_MEMORY_LIMIT;
            result->diagnostic = "memory limit exceeded while creating Lua state";
            result->peak_allocator_bytes = state->peak_allocated;
            delete state;
            return false;
        }
        *(s_lua_runtime_state**)lua_getextraspace(state->lua_state) = state;
        Begin_Invocation(state);
        lua_pushcfunction(state->lua_state, Initialize_Environment);
        const int status = lua_pcall(state->lua_state, 0, 0, 0);
        if (status != LUA_OK) {
            result->error = Status_Error(status, state);
            result->diagnostic = Diagnostic_Text(state->lua_state, limits);
            result->peak_allocator_bytes = state->peak_allocated;
            lua_close(state->lua_state);
            result->remaining_allocator_bytes = state->allocated;
            delete state;
            return false;
        }
        Complete_Invocation(state, result);
        *runtime = state;
        return true;
    }

    void Destroy_Runtime_State(s_lua_runtime_state* runtime) {
        if (runtime != nullptr) {
            lua_close(runtime->lua_state);
            delete runtime;
        }
    }

    lua_State* Get_Runtime_State_Handle(s_lua_runtime_state* runtime) {
        return runtime == nullptr ? nullptr : runtime->lua_state;
    }

    bool Load_Runtime_Source_Chunk(lua_State* lua_state, const char* source, std::size_t source_size, const char* source_name) {
        if (lua_state == nullptr) {
            return false;
        }
        s_lua_runtime_state& runtime = Invocation_State(lua_state);
        s_lua_result result;
        if (!Validate_Runtime_Input(source, source_size, source_name, &runtime.limits, &result)) {
            runtime.nested_load_error = result.error;
            runtime.nested_load_diagnostic = result.diagnostic;
            lua_pushlstring(lua_state, result.diagnostic.data(), result.diagnostic.size());
            return false;
        }

        std::string chunk_name = "@";
        chunk_name += source_name;
        const int status = luaL_loadbufferx(lua_state, source, source_size, chunk_name.c_str(), "t");
        if (status != LUA_OK) {
            runtime.nested_load_error = Status_Error(status, &runtime);
            runtime.nested_load_diagnostic = Diagnostic_Text(lua_state,
                &runtime.limits);
            return false;
        }
        s_profile_error profile_error;
        if (Validate_Loaded_Chunk(lua_state, &profile_error)) {
            return true;
        }

        runtime.nested_load_error = profile_error.error;
        lua_pop(lua_state, 1);
        std::string diagnostic = source_name;
        if (profile_error.line >= 0) {
            diagnostic += ':' + std::to_string(profile_error.line);
        }
        diagnostic += ": ";
        diagnostic += profile_error.message;
        if (diagnostic.size() > runtime.limits.diagnostic_bytes) {
            diagnostic.resize(runtime.limits.diagnostic_bytes);
        }
        runtime.nested_load_diagnostic = diagnostic;
        lua_pushlstring(lua_state, diagnostic.data(), diagnostic.size());
        return false;
    }

    void Mark_Runtime_Invocation_Error(lua_State* lua_state,
        e_lua_error error) {
        if (lua_state == nullptr || error == LUA_ERROR_NONE) {
            return;
        }
        s_lua_runtime_state& runtime = Invocation_State(lua_state);
        if (runtime.nested_load_error != LUA_ERROR_NONE) {
            return;
        }
        runtime.nested_load_error = error;
        runtime.nested_load_diagnostic = Diagnostic_Text(lua_state,
            &runtime.limits);
    }

    bool Execute_Runtime_Operation(s_lua_runtime_state* runtime,
        t_lua_state_operation operation,
        void* user_data,
        s_lua_result* result) {
        if (runtime == nullptr || operation == nullptr || result == nullptr) {
            return false;
        }
        *result = {};
        Begin_Invocation(runtime);
        runtime->operation = operation;
        runtime->operation_user_data = user_data;
        lua_State* lua_state = runtime->lua_state;
        lua_settop(lua_state, 0);
        lua_pushcfunction(lua_state, Execute_Operation);
        const int status = lua_pcall(lua_state, 0, 0, 0);
        if (status != LUA_OK) {
            result->error = Status_Error(status, runtime);
            result->diagnostic = Diagnostic_Text(lua_state, &runtime->limits);
        } else if (runtime->nested_load_error != LUA_ERROR_NONE) {
            result->error = runtime->nested_load_error;
            result->diagnostic = runtime->nested_load_diagnostic;
        } else {
            result->value_kind = LUA_VALUE_NIL;
        }
        Complete_Invocation(runtime, result);
        lua_settop(lua_state, 0);
        return result->error == LUA_ERROR_NONE;
    }

    bool Execute_Runtime_Source(s_lua_runtime_state* runtime, const char* source, std::size_t source_size, const char* source_name, s_lua_result* result) {
        if (runtime == nullptr || result == nullptr) {
            return false;
        }
        *result = {};
        if (!Validate_Runtime_Input(source, source_size, source_name, &runtime->limits, result)) {
            return false;
        }

        Begin_Invocation(runtime);
        lua_State* lua_state = runtime->lua_state;
        lua_settop(lua_state, 0);
        std::string chunk_name = "@";
        chunk_name += source_name;
        int status = luaL_loadbufferx(
            lua_state, source, source_size, chunk_name.c_str(), "t");
        if (status == LUA_OK) {
            s_profile_error profile_error;
            if (!Validate_Loaded_Chunk(lua_state, &profile_error)) {
                result->error = profile_error.error;
                result->diagnostic.assign(source_name);
                if (profile_error.line >= 0) {
                    result->diagnostic += ':' + std::to_string(profile_error.line);
                }
                result->diagnostic += ": ";
                result->diagnostic += profile_error.message;
                if (result->diagnostic.size() > runtime->limits.diagnostic_bytes) {
                    result->diagnostic.resize(runtime->limits.diagnostic_bytes);
                }
            } else {
                status = lua_pcall(lua_state, 0, 0, 0);
            }
        }
        if (result->error == LUA_ERROR_NONE && status != LUA_OK) {
            result->error = Status_Error(status, runtime);
            result->diagnostic = Diagnostic_Text(lua_state, &runtime->limits);
        } else if (result->error == LUA_ERROR_NONE &&
            runtime->nested_load_error != LUA_ERROR_NONE) {
            result->error = runtime->nested_load_error;
            result->diagnostic = runtime->nested_load_diagnostic;
        }
        Complete_Invocation(runtime, result);
        lua_settop(lua_state, 0);
        return result->error == LUA_ERROR_NONE;
    }

    bool Invoke_Runtime_Function(s_lua_runtime_state* runtime,
        int function_reference,
        t_lua_argument_builder build_arguments,
        void* user_data,
        s_lua_result* result) {
        if (runtime == nullptr || result == nullptr ||
            function_reference == LUA_NOREF || function_reference == LUA_REFNIL) {
            return false;
        }
        *result = {};
        Begin_Invocation(runtime);
        runtime->invoked_reference = function_reference;
        runtime->build_arguments = build_arguments;
        runtime->argument_user_data = user_data;
        lua_State* lua_state = runtime->lua_state;
        lua_settop(lua_state, 0);
        lua_pushcfunction(lua_state, Invoke_Reference);
        const int status = lua_pcall(lua_state, 0, 0, 0);
        if (status != LUA_OK) {
            result->error = Status_Error(status, runtime);
            result->diagnostic = Diagnostic_Text(lua_state, &runtime->limits);
        } else if (runtime->nested_load_error != LUA_ERROR_NONE) {
            result->error = runtime->nested_load_error;
            result->diagnostic = runtime->nested_load_diagnostic;
        } else {
            result->value_kind = LUA_VALUE_NIL;
        }
        Complete_Invocation(runtime, result);
        lua_settop(lua_state, 0);
        return result->error == LUA_ERROR_NONE;
    }

    bool Invoke_Runtime_Function_Results(s_lua_runtime_state* runtime,
        int function_reference,
        t_lua_argument_builder build_arguments,
        void* argument_user_data,
        t_lua_result_consumer consume_results,
        void* result_user_data,
        s_lua_result* result) {
        if (runtime == nullptr || result == nullptr ||
            consume_results == nullptr || function_reference == LUA_NOREF ||
            function_reference == LUA_REFNIL) {
            return false;
        }
        *result = {};
        Begin_Invocation(runtime);
        runtime->invoked_reference = function_reference;
        runtime->build_arguments = build_arguments;
        runtime->argument_user_data = argument_user_data;
        runtime->consume_results = consume_results;
        runtime->result_user_data = result_user_data;
        lua_State* lua_state = runtime->lua_state;
        lua_settop(lua_state, 0);
        lua_pushcfunction(lua_state, Invoke_Reference_Results);
        const int status = lua_pcall(lua_state, 0, 0, 0);
        if (status != LUA_OK) {
            result->error = Status_Error(status, runtime);
            result->diagnostic = Diagnostic_Text(lua_state, &runtime->limits);
        } else if (runtime->nested_load_error != LUA_ERROR_NONE) {
            result->error = runtime->nested_load_error;
            result->diagnostic = runtime->nested_load_diagnostic;
        } else {
            result->value_kind = LUA_VALUE_NIL;
        }
        Complete_Invocation(runtime, result);
        lua_settop(lua_state, 0);
        return result->error == LUA_ERROR_NONE;
    }

} // namespace orion2re::lua