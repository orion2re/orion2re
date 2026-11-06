#include "lua/lua_runtime.h"

#include <algorithm>
#include <charconv>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

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
        // The binary-chunk reader below is written for the Lua 5.5 chunk
        // format, which is identical in 5.5.0 and 5.5.1 (LUAC_VERSION 0x55,
        // LUAC_FORMAT 0). The reader also validates the chunk header at
        // runtime and rejects anything it does not recognise, so a future
        // format change fails closed instead of misreading the chunk.
        static_assert(LUA_VERSION_RELEASE_NUM == 50500 ||
                      LUA_VERSION_RELEASE_NUM == 50501);
        static_assert(sizeof(int) == 4);
        static_assert(sizeof(lua_Number) == 8);

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

        // Opcodes of the Lua 5.5 virtual machine, in lopcodes.h enum order.
        // Only the opcodes the profile inspects are listed; the numbers are
        // part of the fixed Lua 5.5 binary chunk format.
        enum : std::uint32_t {
            kOpLoadF = 2,
            kOpLoadK = 3,
            kOpLoadKX = 4,
            kOpSetTabUp = 15,
            kOpSetTable = 16,
            kOpSetI = 17,
            kOpSetField = 18,
            kOpAddK = 22,
            kOpSubK = 23,
            kOpMulK = 24,
            kOpModK = 25,
            kOpPowK = 26,
            kOpDivK = 27,
            kOpIdivK = 28,
            kOpBandK = 29,
            kOpBorK = 30,
            kOpBxorK = 31,
            kOpPow = 38,
            kOpDiv = 39,
            kOpMmbinK = 48,
            kOpEqK = 60,
            kOpEqI = 61,
            kOpLtI = 62,
            kOpLeI = 63,
            kOpGtI = 64,
            kOpGeI = 65,
            kOpExtraArg = 84,
        };

        // Instruction fields of the Lua 5.5 format: the opcode occupies the
        // low 7 bits, A the next 8, the k flag 1 bit, then B (8 bits) and
        // C (8 bits); Bx spans the B/k/C fields, Ax spans A/k/B/C.
        std::uint32_t Chunk_Opcode(std::uint32_t instruction) {
            return instruction & 0x7fu;
        }

        std::uint32_t Chunk_Arg_B(std::uint32_t instruction) {
            return (instruction >> 16) & 0xffu;
        }

        std::uint32_t Chunk_Arg_C(std::uint32_t instruction) {
            return (instruction >> 24) & 0xffu;
        }

        std::uint32_t Chunk_Arg_Bx(std::uint32_t instruction) {
            return (instruction >> 16) & 0x1ffffu;
        }

        std::uint32_t Chunk_Arg_Ax(std::uint32_t instruction) {
            return (instruction >> 7) & 0x1ffffffu;
        }

        bool Chunk_Arg_K(std::uint32_t instruction) {
            return ((instruction >> 15) & 1u) != 0;
        }

        // Constant tags as stored in a Lua 5.5 chunk (lobject.h variant tags).
        enum : std::uint8_t {
            kChunkTagNil = 0,
            kChunkTagFalse = 1,
            kChunkTagNumberInt = 3,
            kChunkTagShortString = 4,
            kChunkTagTrue = 17,
            kChunkTagNumberFloat = 19,
            kChunkTagLongString = 20,
        };

        // Absolute line entries from a chunk: the source line that holds at
        // the given instruction, as recorded by the compiler.
        struct s_chunk_abs_line {
            std::int32_t pc = 0;
            std::int32_t line = 0;
        };

        static_assert(sizeof(s_chunk_abs_line) == 8);

        // One compiled function from a chunk, with the fields the profile
        // inspects: the instruction stream, which constants are floats, the
        // line information, and the nested functions.
        struct s_chunk_function {
            int linedefined = 0;
            std::vector<std::uint32_t> code;
            std::vector<std::uint8_t> constant_is_float;
            std::vector<std::int8_t> lineinfo;
            std::vector<s_chunk_abs_line> abslineinfo;
            std::vector<s_chunk_function> subfunctions;
        };

        // Bounds-checked cursor over a binary chunk. Every read fails
        // closed on truncation, overflow or an inconsistent count, so a
        // malformed chunk is rejected instead of misread.
        struct s_chunk_reader {
            s_chunk_reader(const char* data, std::size_t size)
                : data(data), size(size) {
            }

            bool Read_Bytes(void* destination, std::size_t count) {
                if (failed || position > size || count > size - position) {
                    failed = true;
                    return false;
                }
                std::memcpy(destination, data + position, count);
                position += count;
                return true;
            }

            bool Read_Byte(std::uint8_t* value) {
                return Read_Bytes(value, 1);
            }

            // MSB varints, most significant group first.
            bool Read_Varint(std::uint64_t* value) {
                std::uint64_t result = 0;
                for (int index = 0; index < 10; ++index) {
                    std::uint8_t byte = 0;
                    if (!Read_Byte(&byte)) {
                        return false;
                    }
                    if (result > (std::numeric_limits<std::uint64_t>::max() >> 7)) {
                        failed = true;
                        return false;
                    }
                    result = (result << 7) | (byte & 0x7f);
                    if ((byte & 0x80) == 0) {
                        *value = result;
                        return true;
                    }
                }
                failed = true;
                return false;
            }

            bool Read_Varint_As_Int(int* value) {
                std::uint64_t result = 0;
                if (!Read_Varint(&result) || result > (std::uint64_t)INT_MAX) {
                    return false;
                }
                *value = (int)result;
                return true;
            }

            bool Read_Align(std::size_t alignment) {
                const std::size_t padding =
                    (alignment - (position % alignment)) % alignment;
                if (padding > size - position) {
                    failed = true;
                    return false;
                }
                position += padding;
                return true;
            }

            bool Read_String() {
                std::uint64_t string_size = 0;
                if (!Read_Varint(&string_size)) {
                    return false;
                }
                if (string_size == 0) {
                    std::uint64_t index = 0;
                    return Read_Varint(&index);
                }
                if (string_size > size - position) {
                    failed = true;
                    return false;
                }
                position += (std::size_t)string_size;
                return true;
            }

            const char* data = nullptr;
            std::size_t size = 0;
            std::size_t position = 0;
            bool failed = false;
        };

        template <typename T>
        bool Read_Chunk_Number_Info(s_chunk_reader* reader, T expected) {
            std::uint8_t size = 0;
            if (!reader->Read_Byte(&size) || size != sizeof(T)) {
                return false;
            }
            T value{};
            if (!reader->Read_Bytes(&value, sizeof(value)) || value != expected) {
                return false;
            }
            return true;
        }

        bool Read_Chunk_Header(s_chunk_reader* reader) {
            static constexpr char kSignature[] = {'\x1b', 'L', 'u', 'a'};
            static constexpr char kData[] = {'\x19', '\x93', '\r', '\n', '\x1a', '\n'};
            char signature[sizeof(kSignature)] = {};
            if (!reader->Read_Bytes(signature, sizeof(signature)) ||
                std::memcmp(signature, kSignature, sizeof(signature)) != 0) {
                return false;
            }
            std::uint8_t version = 0;
            std::uint8_t format = 0;
            if (!reader->Read_Byte(&version) || version != 0x55 ||
                !reader->Read_Byte(&format) || format != 0) {
                return false;
            }
            char data[sizeof(kData)] = {};
            if (!reader->Read_Bytes(data, sizeof(data)) ||
                std::memcmp(data, kData, sizeof(data)) != 0) {
                return false;
            }
            return Read_Chunk_Number_Info<int>(reader, -0x5678) &&
                   Read_Chunk_Number_Info<std::uint32_t>(reader, 0x12345678u) &&
                   Read_Chunk_Number_Info<lua_Integer>(reader, (lua_Integer)-0x5678) &&
                   Read_Chunk_Number_Info<lua_Number>(reader, (lua_Number)-370.5);
        }

        bool Parse_Chunk_Function(s_chunk_reader* reader, s_chunk_function* function, int depth) {
            constexpr int kMaxChunkFunctionDepth = 1024;
            if (depth > kMaxChunkFunctionDepth) {
                return false;
            }
            int lastlinedefined = 0;
            if (!reader->Read_Varint_As_Int(&function->linedefined) ||
                !reader->Read_Varint_As_Int(&lastlinedefined)) {
                return false;
            }
            std::uint8_t numparams = 0;
            std::uint8_t flag = 0;
            std::uint8_t maxstacksize = 0;
            if (!reader->Read_Byte(&numparams) || !reader->Read_Byte(&flag) ||
                !reader->Read_Byte(&maxstacksize)) {
                return false;
            }

            int code_size = 0;
            if (!reader->Read_Varint_As_Int(&code_size) || code_size < 0 ||
                (std::uint64_t)code_size > (reader->size - reader->position) / sizeof(std::uint32_t)) {
                return false;
            }
            if (!reader->Read_Align(sizeof(std::uint32_t))) {
                return false;
            }
            function->code.resize((std::size_t)code_size);
            if (!reader->Read_Bytes(function->code.data(),
                    (std::size_t)code_size * sizeof(std::uint32_t))) {
                return false;
            }

            int constant_count = 0;
            if (!reader->Read_Varint_As_Int(&constant_count) || constant_count < 0 ||
                (std::uint64_t)constant_count > reader->size - reader->position) {
                return false;
            }
            function->constant_is_float.assign((std::size_t)constant_count, 0);
            for (int index = 0; index < constant_count; ++index) {
                std::uint8_t tag = 0;
                if (!reader->Read_Byte(&tag)) {
                    return false;
                }
                switch (tag) {
                    case kChunkTagNumberFloat: {
                        lua_Number value = 0;
                        if (!reader->Read_Bytes(&value, sizeof(value))) {
                            return false;
                        }
                        function->constant_is_float[(std::size_t)index] = 1;
                        break;
                    }
                    case kChunkTagNumberInt: {
                        std::uint64_t value = 0;
                        if (!reader->Read_Varint(&value)) {
                            return false;
                        }
                        break;
                    }
                    case kChunkTagShortString:
                    case kChunkTagLongString:
                        if (!reader->Read_String()) {
                            return false;
                        }
                        break;
                    case kChunkTagNil:
                    case kChunkTagFalse:
                    case kChunkTagTrue:
                        break;
                    default:
                        return false;
                }
            }

            int upvalue_count = 0;
            if (!reader->Read_Varint_As_Int(&upvalue_count) || upvalue_count < 0 ||
                (std::uint64_t)upvalue_count > (reader->size - reader->position) / 3) {
                return false;
            }
            reader->position += (std::size_t)upvalue_count * 3;

            int subfunction_count = 0;
            if (!reader->Read_Varint_As_Int(&subfunction_count) || subfunction_count < 0 ||
                (std::uint64_t)subfunction_count > reader->size - reader->position) {
                return false;
            }
            function->subfunctions.resize((std::size_t)subfunction_count);
            for (int index = 0; index < subfunction_count; ++index) {
                if (!Parse_Chunk_Function(reader, &function->subfunctions[(std::size_t)index], depth + 1)) {
                    return false;
                }
            }

            if (!reader->Read_String()) {
                return false;
            }

            int lineinfo_size = 0;
            if (!reader->Read_Varint_As_Int(&lineinfo_size) || lineinfo_size < 0 ||
                (std::uint64_t)lineinfo_size > reader->size - reader->position) {
                return false;
            }
            function->lineinfo.resize((std::size_t)lineinfo_size);
            if (!reader->Read_Bytes(function->lineinfo.data(), (std::size_t)lineinfo_size)) {
                return false;
            }

            int abslineinfo_size = 0;
            if (!reader->Read_Varint_As_Int(&abslineinfo_size) || abslineinfo_size < 0 ||
                (std::uint64_t)abslineinfo_size > (reader->size - reader->position) / sizeof(s_chunk_abs_line)) {
                return false;
            }
            if (abslineinfo_size > 0 && !reader->Read_Align(sizeof(std::int32_t))) {
                return false;
            }
            function->abslineinfo.resize((std::size_t)abslineinfo_size);
            if (!reader->Read_Bytes(function->abslineinfo.data(),
                    (std::size_t)abslineinfo_size * sizeof(s_chunk_abs_line))) {
                return false;
            }

            int locvar_count = 0;
            if (!reader->Read_Varint_As_Int(&locvar_count) || locvar_count < 0 ||
                (std::uint64_t)locvar_count > (reader->size - reader->position) / 3) {
                return false;
            }
            for (int index = 0; index < locvar_count; ++index) {
                int startpc = 0;
                int endpc = 0;
                if (!reader->Read_String() || !reader->Read_Varint_As_Int(&startpc) ||
                    !reader->Read_Varint_As_Int(&endpc)) {
                    return false;
                }
            }

            int upvalue_name_count = 0;
            if (!reader->Read_Varint_As_Int(&upvalue_name_count) || upvalue_name_count < 0 ||
                (std::uint64_t)upvalue_name_count > reader->size - reader->position) {
                return false;
            }
            for (int index = 0; index < upvalue_name_count; ++index) {
                if (!reader->Read_String()) {
                    return false;
                }
            }
            return true;
        }

        void Set_Opcode_Error(s_profile_error* error, std::uint32_t opcode, int line) {
            switch (opcode) {
                case kOpDiv:
                case kOpDivK:
                    Set_Profile_Error(
                        error, LUA_ERROR_LANGUAGE_PROFILE, line, "operator '/' is unavailable; use reviewed integer division '//'");
                    return;
                case kOpPow:
                case kOpPowK:
                    Set_Profile_Error(
                        error, LUA_ERROR_LANGUAGE_PROFILE, line, "operator '^' is unavailable in the integer-only profile");
                    return;
                default:
                    Set_Profile_Error(
                        error, LUA_ERROR_LANGUAGE_PROFILE, line, "floating-point values are unavailable in the integer-only profile");
                    return;
            }
        }

        int Referenced_Constant(const std::vector<std::uint32_t>& code, int pc) {
            const std::uint32_t instruction = code[(std::size_t)pc];
            switch (Chunk_Opcode(instruction)) {
                case kOpLoadK:
                    return (int)Chunk_Arg_Bx(instruction);
                case kOpLoadKX:
                    if (pc + 1 < (int)code.size() &&
                        Chunk_Opcode(code[(std::size_t)pc + 1]) == kOpExtraArg) {
                        return (int)Chunk_Arg_Ax(code[(std::size_t)pc + 1]);
                    }
                    break;
                case kOpAddK:
                case kOpSubK:
                case kOpMulK:
                case kOpModK:
                case kOpPowK:
                case kOpDivK:
                case kOpIdivK:
                case kOpBandK:
                case kOpBorK:
                case kOpBxorK:
                    return (int)Chunk_Arg_C(instruction);
                case kOpMmbinK:
                case kOpEqK:
                    return (int)Chunk_Arg_B(instruction);
                case kOpSetTabUp:
                case kOpSetTable:
                case kOpSetI:
                case kOpSetField:
                    if (Chunk_Arg_K(instruction)) {
                        return (int)Chunk_Arg_C(instruction);
                    }
                    break;
                default:
                    break;
            }
            return -1;
        }

        // The source line of an instruction, following luaG_getfuncline:
        // the last absolute line at or before the instruction, plus the
        // lineinfo deltas after it. -1 when the chunk carries no line info.
        int Chunk_Function_Line(const s_chunk_function& function, int pc) {
            if (function.lineinfo.empty() || pc < 0 ||
                pc >= (int)function.lineinfo.size()) {
                return -1;
            }
            int basepc = -1;
            int baseline = function.linedefined;
            for (const auto& entry : function.abslineinfo) {
                if (entry.pc > pc) {
                    break;
                }
                basepc = entry.pc;
                baseline = entry.line;
            }
            int line = baseline;
            for (int index = basepc + 1; index <= pc; ++index) {
                const int delta = function.lineinfo[(std::size_t)index];
                if (delta == -128) {
                    return -1;
                }
                line += delta;
            }
            return line;
        }

        bool Validate_Chunk_Function(const s_chunk_function& function, s_profile_error* error) {
            for (int pc = 0; pc < (int)function.code.size(); ++pc) {
                const std::uint32_t instruction = function.code[(std::size_t)pc];
                const std::uint32_t opcode = Chunk_Opcode(instruction);
                const int line = Chunk_Function_Line(function, pc);

                if (opcode == kOpLoadF || opcode == kOpDiv || opcode == kOpDivK ||
                    opcode == kOpPow || opcode == kOpPowK) {
                    Set_Opcode_Error(error, opcode, line);
                    return false;
                }
                // The C field of an immediate comparison holds the
                // isfloat flag: the immediate was written as a float
                // (for example `value < 1.0`) even when it converts to
                // an integer exactly.
                if ((opcode == kOpEqI || opcode == kOpLtI || opcode == kOpLeI ||
                        opcode == kOpGtI || opcode == kOpGeI) &&
                    Chunk_Arg_C(instruction) != 0) {
                    Set_Opcode_Error(error, kOpLoadF, line);
                    return false;
                }

                const int constant_index = Referenced_Constant(function.code, pc);
                if (constant_index >= 0 &&
                    constant_index < (int)function.constant_is_float.size() &&
                    function.constant_is_float[(std::size_t)constant_index] != 0) {
                    Set_Opcode_Error(error, opcode, line);
                    return false;
                }
            }

            for (std::size_t index = 0; index < function.constant_is_float.size(); ++index) {
                if (function.constant_is_float[index] != 0) {
                    Set_Opcode_Error(error, kOpLoadF, std::max(1, function.linedefined));
                    return false;
                }
            }
            for (const auto& subfunction : function.subfunctions) {
                if (!Validate_Chunk_Function(subfunction, error)) {
                    return false;
                }
            }
            return true;
        }

        constexpr std::size_t kMaxChunkBytes = 16 * 1024 * 1024;

        int Dump_Chunk(lua_State*, const void* data, std::size_t size, void* user_data) {
            auto* chunk = (std::string*)user_data;
            if (data == nullptr || size == 0) {
                return 0;
            }
            if (chunk->size() + size > kMaxChunkBytes) {
                return 1;
            }
            chunk->append((const char*)data, size);
            return 0;
        }

        bool Validate_Loaded_Chunk(lua_State* lua_state, s_profile_error* error) {
            if (!lua_isfunction(lua_state, -1)) {
                Set_Profile_Error(error, LUA_ERROR_LANGUAGE_PROFILE, 1, "compiled chunk did not produce a Lua prototype");
                return false;
            }
            // Serialise the loaded function with the public lua_dump and
            // walk the resulting chunk. The dump is produced by the same
            // Lua that compiled the source, so its header is validated
            // against the host representation and any mismatch is
            // rejected rather than trusted.
            std::string chunk;
            if (lua_dump(lua_state, Dump_Chunk, &chunk, 0) != LUA_OK) {
                Set_Profile_Error(error, LUA_ERROR_LANGUAGE_PROFILE, 1, "compiled chunk could not be inspected");
                return false;
            }
            s_chunk_reader reader(chunk.data(), chunk.size());
            s_chunk_function function;
            std::uint8_t main_upvalue_count = 0;
            if (!Read_Chunk_Header(&reader) ||
                // luaU_dump writes the main function's upvalue count
                // once, ahead of the first function.
                !reader.Read_Byte(&main_upvalue_count) ||
                !Parse_Chunk_Function(&reader, &function, 0) ||
                reader.position != reader.size) {
                Set_Profile_Error(error, LUA_ERROR_LANGUAGE_PROFILE, 1, "compiled chunk is malformed");
                return false;
            }
            return Validate_Chunk_Function(function, error);
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