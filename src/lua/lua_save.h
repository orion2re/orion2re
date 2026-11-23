#ifndef ORION2RE_LUA_SAVE_H
#define ORION2RE_LUA_SAVE_H

#include <cstdint>
#include "game/object_ids.h"
#include <string>
#include <vector>

#include "lua/lua_registry.h"

namespace SERIAL {
    class Reader_;
    class Writer_;
} // namespace SERIAL

namespace orion2re::lua {

    inline constexpr uint32_t LUA_SAVE_FORMAT_VERSION = 6;
    inline constexpr uint32_t LUA_SAVE_MAX_BYTES = 4 * 1024 * 1024;
    inline constexpr uint32_t LUA_SAVE_MAX_STRING_BYTES = 1024 * 1024;
    inline constexpr uint32_t LUA_SAVE_MAX_PACKAGE_COUNT = 4096;
    inline constexpr uint32_t LUA_SAVE_MAX_RECORD_COUNT = 262'144;
    inline bool allow_source_hash_mismatch = false;

    struct s_lua_mod_runtime_set;

    struct s_lua_saved_storage {
        std::string package_id;
        uint32_t storage_version = 0;
        std::vector<uint8_t> bytes;
    };

    struct s_lua_saved_trait_assignment {
        ::game::id_player player_id = ::game::NO_PLAYER;
        std::string trait_id;
    };

    struct s_lua_save_snapshot {
        s_lua_registry registry;
        std::vector<s_lua_saved_trait_assignment> trait_assignments;
        uint32_t ship_generation_count = 0;
        uint64_t ship_generations[MAX_SHIPS] = {};
        std::vector<s_lua_saved_storage> storage;
    };

    bool Capture_Lua_Save_Snapshot(s_lua_save_snapshot* snapshot,
        std::string* diagnostic);
    uint32_t Lua_Save_Snapshot_Size(const s_lua_save_snapshot* snapshot);
    void Write_Lua_Save_Snapshot(SERIAL::Writer_* writer,
        const s_lua_save_snapshot* snapshot);
    bool Read_Lua_Save_Snapshot(SERIAL::Reader_* reader,
        s_lua_save_snapshot* snapshot,
        std::string* diagnostic);
    bool Restore_Lua_Save_Snapshot(const s_lua_save_snapshot* snapshot,
        const s_lua_registry* candidate_registry,
        s_lua_mod_runtime_set* candidate_runtimes,
        std::string* diagnostic,
        bool defer_lifecycle = false,
        bool require_exact_identity = false);
    bool Dispatch_Lua_Save_Snapshot_Lifecycle(
        const s_lua_save_snapshot* snapshot,
        const s_lua_registry* candidate_registry,
        s_lua_mod_runtime_set* candidate_runtimes,
        std::string* diagnostic);

} // namespace orion2re::lua

#endif
