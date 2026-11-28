#include "lua/lua_mod_runtime.h"

#include <algorithm>
#include <climits>
#include <iterator>
#include <limits>
#include <new>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "config/mod_manifest.h"
#include "config/generated/config_enums.h"
#include "game/sha256.h"
#include "game/vfs.h"
#include "lua/lua_game_ship_identity.h"
#include "lua/lua_game_state.h"
#include "lua/lua_game_storage_reference.h"
#include "lua/lua_game_traits.h"
#include "lua/lua_registry.h"
#include "lua/lua_storage.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace orion2re::lua {
    namespace {

        struct s_registered_callback {
            s_lua_callback_info info;
            int reference = LUA_NOREF;
        };

        struct s_storage_restore {
            const uint8_t* bytes = nullptr;
            size_t size = 0;
            const s_lua_storage_limits* limits = nullptr;
            int reference = LUA_NOREF;
            std::string diagnostic;
        };

        struct s_migration_context {
            uint32_t from = 0;
            uint32_t to = 0;
            bool install = false;
        };

        struct s_storage_serialization {
            s_lua_mod_runtime* runtime = nullptr;
            std::vector<uint8_t>* bytes = nullptr;
            std::string diagnostic;
        };

        struct s_runtime_migration {
            uint32_t from = 0;
            uint32_t to = 0;
            std::string path;
            std::string source;
        };

        struct s_configuration_changed_context {
            const char* reason = nullptr;
            const std::vector<s_lua_configuration_change>* changes = nullptr;
            const std::vector<std::string>* changed_groups = nullptr;
        };

        struct s_owned_event_descriptor {
            std::string name;
            uint32_t schema = 1;
            e_lua_game_state_capability game_state =
                LUA_GAME_STATE_READ_ONLY;
        };

        struct s_owned_rule_descriptor {
            std::string name;
            uint32_t schema = 1;
        };

        struct s_rule_invocation {
            t_lua_argument_builder build_context = nullptr;
            void* user_data = nullptr;
            std::int64_t value = 0;
            std::int64_t minimum = 0;
            std::int64_t maximum = 0;
            std::string diagnostic;
        };

        struct s_export_method {
            std::string name;
            int reference = LUA_NOREF;
        };

        struct s_registered_export {
            s_lua_export_info info;
            std::vector<s_export_method> methods;
        };

        struct s_registered_service {
            s_lua_service_info info;
            int reference = LUA_NOREF;
        };

        enum e_marshaled_key_kind {
            MARSHALLED_KEY_BOOLEAN,
            MARSHALLED_KEY_INTEGER,
            MARSHALLED_KEY_STRING,
        };

        struct s_marshaled_key {
            e_marshaled_key_kind kind = MARSHALLED_KEY_BOOLEAN;
            bool boolean_value = false;
            lua_Integer integer_value = 0;
            std::string string_value;
        };

        struct s_marshalling_state {
            const s_lua_mod_limits* limits = nullptr;
            size_t values = 0;
            size_t entries = 0;
            size_t bytes = 0;
            std::unordered_set<const void*> tables;
            std::string diagnostic;
        };

        struct s_export_call {
            lua_State* caller_state = nullptr;
            int caller_argument_count = 0;
            int caller_initial_top = 0;
            int caller_result_count = 0;
            const s_lua_mod_limits* limits = nullptr;
            std::string diagnostic;
        };

    } // namespace

    struct s_lua_mod_runtime {
        std::string package_id;
        std::string package_version;
        uint32_t api = 1;
        uint32_t storage_version = 1;
        s_lua_mod_limits limits;
        s_lua_runtime_state* state = nullptr;
        std::vector<s_owned_event_descriptor> event_descriptors;
        std::vector<s_owned_rule_descriptor> rule_descriptors;
        std::vector<s_registered_callback> callbacks;
        std::vector<s_registered_export> exports;
        std::vector<s_registered_service> services;
        std::vector<std::string> dependencies;
        std::vector<s_lua_module_info> modules;
        std::vector<std::string> module_sources;
        std::vector<std::string> loading_modules;
        std::vector<s_runtime_migration> migrations;
        size_t package_source_bytes = 0;
        int module_cache_reference = LUA_NOREF;
        int loading_module_reference = LUA_NOREF;
        int storage_reference = LUA_NOREF;
        int game_reference = LUA_NOREF;
        s_lua_game_traits* game_traits = nullptr;
        s_lua_mod_runtime_set* runtime_set = nullptr;
        bool setup_active = true;
        bool invocation_active = false;
    };

    struct s_lua_mod_runtime_set {
        std::vector<s_lua_mod_runtime*> runtimes;
        s_lua_game_traits* game_traits = nullptr;
        uint64_t ship_generations[MAX_SHIPS] = {};
    };

    struct s_lua_storage_restore_transaction {
        s_lua_mod_runtime* runtime = nullptr;
        int prior_reference = LUA_NOREF;
        int candidate_reference = LUA_NOREF;
    };

    namespace {

        s_lua_mod_runtime_set active_runtimes_;
        thread_local size_t cross_mod_call_depth_ = 0;
        thread_local e_lua_callback_kind active_callback_kind_ =
            LUA_CALLBACK_EVENT;
        thread_local e_lua_game_state_capability active_game_state_ =
            LUA_GAME_STATE_READ_ONLY;
        thread_local size_t active_callback_depth_ = 0;

        constexpr s_lua_event_descriptor RUNTIME_EVENTS_[] = {
            {"game.initialized", 1, LUA_GAME_STATE_MUTABLE},
            {"game.loaded", 1},
            {"game.configuration_changed", 1},
            {"new_game.technology_setup", 1, LUA_GAME_STATE_TECHNOLOGY_MUTABLE},
            {"new_game.player_setup", 1, LUA_GAME_STATE_PLAYER_SETUP_MUTABLE},
            {"new_game.home_system_generated", 1, LUA_GAME_STATE_PLANET_MUTABLE},
            {"planet.generated", 1, LUA_GAME_STATE_PLANET_MUTABLE},
            {"ship.destroyed", 1, LUA_GAME_STATE_MUTABLE},
        };

        constexpr s_lua_rule_descriptor RUNTIME_RULES_[] = {
            {"population.growth", 1},
            {"combat.beam_damage", 1},
        };

        bool Network_Event_Allowed(std::string_view name) {
            return name == "game.initialized" || name == "game.loaded" ||
                name == "game.configuration_changed" ||
                name == "new_game.technology_setup" ||
                name == "new_game.player_setup" ||
                name == "new_game.home_system_generated" ||
                name == "planet.generated";
        }

        bool Network_Rule_Allowed(std::string_view name) {
            return name == "population.growth";
        }

        struct s_creation_context {
            s_lua_mod_runtime_set* runtime_set = nullptr;
            const config::s_mod_manifest* package = nullptr;
            s_lua_game_traits* game_traits = nullptr;
        };

        thread_local s_creation_context creation_context_;

        void Restore_Storage_Global(s_lua_mod_runtime* runtime);
        bool Storage_Global_Unchanged(s_lua_mod_runtime* runtime);
        void Restore_Game_Global(s_lua_mod_runtime* runtime);
        bool Game_Global_Unchanged(s_lua_mod_runtime* runtime);

        struct s_rule_invocation_scope {
            s_lua_mod_runtime* runtime = nullptr;
            lua_State* lua_state = nullptr;
            int persistent_storage_reference = LUA_NOREF;
            int isolated_storage_reference = LUA_NOREF;
            e_lua_callback_kind prior_callback_kind = LUA_CALLBACK_EVENT;
            e_lua_game_state_capability prior_game_state =
                LUA_GAME_STATE_READ_ONLY;

            s_rule_invocation_scope(s_lua_mod_runtime* runtime_value,
                int isolated_reference)
                : runtime(runtime_value),
                  lua_state(Get_Runtime_State_Handle(runtime_value->state)),
                  persistent_storage_reference(runtime_value->storage_reference),
                  isolated_storage_reference(isolated_reference),
                  prior_callback_kind(active_callback_kind_),
                  prior_game_state(active_game_state_) {
                runtime->storage_reference = isolated_storage_reference;
                Restore_Storage_Global(runtime);
                Restore_Game_Global(runtime);
                runtime->invocation_active = true;
                active_callback_kind_ = LUA_CALLBACK_RULE;
                active_game_state_ = LUA_GAME_STATE_READ_ONLY;
                ++active_callback_depth_;
            }

            ~s_rule_invocation_scope() {
                --active_callback_depth_;
                active_game_state_ = prior_game_state;
                active_callback_kind_ = prior_callback_kind;
                runtime->invocation_active = false;
                runtime->storage_reference = persistent_storage_reference;
                Restore_Storage_Global(runtime);
                Restore_Game_Global(runtime);
                luaL_unref(lua_state,
                    LUA_REGISTRYINDEX,
                    isolated_storage_reference);
            }
        };

        struct s_event_invocation_scope {
            e_lua_callback_kind prior_callback_kind = LUA_CALLBACK_EVENT;
            e_lua_game_state_capability prior_game_state =
                LUA_GAME_STATE_READ_ONLY;

            explicit s_event_invocation_scope(
                e_lua_game_state_capability game_state)
                : prior_callback_kind(active_callback_kind_),
                  prior_game_state(active_game_state_) {
                active_callback_kind_ = LUA_CALLBACK_EVENT;
                active_game_state_ = game_state;
                ++active_callback_depth_;
            }

            ~s_event_invocation_scope() {
                --active_callback_depth_;
                active_game_state_ = prior_game_state;
                active_callback_kind_ = prior_callback_kind;
            }
        };

        bool Fail(const std::string& source, const std::string& message, config::s_cfg_diagnostic* diagnostic) {
            diagnostic->location = {};
            diagnostic->location.source = source;
            diagnostic->location.line = 1;
            diagnostic->location.column = 1;
            diagnostic->message = message;
            return false;
        }

        bool Read_Package_Source(const std::string& package_id,
            const std::string& relative_path,
            size_t* package_bytes,
            const s_lua_mod_limits* limits,
            std::string* source,
            std::string* private_path,
            config::s_cfg_diagnostic* diagnostic) {
            *private_path = "mods/" + package_id + "/" + relative_path;
            s_vfs_file* file = vfs::fopen(private_path->c_str(), "rb");
            if (file == nullptr) {
                return Fail(*private_path, "package Lua source not found", diagnostic);
            }
            const int32_t signed_size = vfs::fsize(file);
            if (signed_size < 0) {
                vfs::fclose(file);
                return Fail(*private_path,
                    "package Lua source has an invalid size",
                    diagnostic);
            }
            const size_t size = (size_t)signed_size;
            if (size > limits->runtime.source_bytes) {
                vfs::fclose(file);
                return Fail(*private_path, "Lua source byte limit exceeded", diagnostic);
            }
            if (*package_bytes > limits->package_source_bytes ||
                size > limits->package_source_bytes - *package_bytes) {
                vfs::fclose(file);
                return Fail(*private_path,
                    "Lua package source byte limit exceeded",
                    diagnostic);
            }
            try {
                source->resize(size);
            } catch (...) {
                vfs::fclose(file);
                throw;
            }
            const size_t read = size == 0 ? 0 : vfs::fread(source->data(), 1, size, file);
            const int32_t close_result = vfs::fclose(file);
            if (read != size || close_result != 0) {
                return Fail(*private_path, "unable to read package Lua source", diagnostic);
            }
            *package_bytes += size;
            return true;
        }

        bool Valid_Interface_Name(std::string_view name) {
            if (name.empty()) {
                return false;
            }
            for (size_t index = 0; index < name.size(); ++index) {
                const char value = name[index];
                if ((value < 'a' || value > 'z') && value != '_' &&
                    (index == 0 || value < '0' || value > '9')) {
                    return false;
                }
            }
            return true;
        }

        bool Add_Marshalled_Bytes(s_marshalling_state* state,
            size_t size) {
            if (state->bytes > state->limits->marshalled_bytes ||
                size > state->limits->marshalled_bytes - state->bytes) {
                state->diagnostic = "cross-mod marshalled byte limit exceeded";
                return false;
            }
            state->bytes += size;
            return true;
        }

        bool Add_Marshalled_Value(s_marshalling_state* state) {
            if (state->values >= state->limits->marshalled_values) {
                state->diagnostic = "cross-mod marshalled value limit exceeded";
                return false;
            }
            ++state->values;
            return true;
        }

        bool Read_Marshalled_Key(lua_State* source, int index, s_marshaled_key* key, s_marshalling_state* state) {
            switch (lua_type(source, index)) {
                case LUA_TBOOLEAN:
                    key->kind = MARSHALLED_KEY_BOOLEAN;
                    key->boolean_value = lua_toboolean(source, index) != 0;
                    return true;
                case LUA_TNUMBER:
                    if (!lua_isinteger(source, index)) {
                        state->diagnostic =
                            "cross-mod table key must not be floating-point";
                        return false;
                    }
                    key->kind = MARSHALLED_KEY_INTEGER;
                    key->integer_value = lua_tointeger(source, index);
                    return true;
                case LUA_TSTRING: {
                    key->kind = MARSHALLED_KEY_STRING;
                    size_t size = 0;
                    const char* value = lua_tolstring(source, index, &size);
                    if (!Add_Marshalled_Bytes(state, size)) {
                        return false;
                    }
                    key->string_value.assign(value, size);
                    return true;
                }
                default:
                    state->diagnostic =
                        "cross-mod table keys must be Boolean, integer, or string";
                    return false;
            }
        }

        bool Marshalled_Key_Less(const s_marshaled_key& left,
            const s_marshaled_key& right) {
            if (left.kind != right.kind) {
                return left.kind < right.kind;
            }
            switch (left.kind) {
                case MARSHALLED_KEY_BOOLEAN:
                    return left.boolean_value < right.boolean_value;
                case MARSHALLED_KEY_INTEGER:
                    return left.integer_value < right.integer_value;
                case MARSHALLED_KEY_STRING:
                    return left.string_value < right.string_value;
            }
            return false;
        }

        void Push_Marshalled_Key(lua_State* lua_state,
            const s_marshaled_key& key) {
            switch (key.kind) {
                case MARSHALLED_KEY_BOOLEAN:
                    lua_pushboolean(lua_state, key.boolean_value);
                    return;
                case MARSHALLED_KEY_INTEGER:
                    lua_pushinteger(lua_state, key.integer_value);
                    return;
                case MARSHALLED_KEY_STRING:
                    lua_pushlstring(lua_state, key.string_value.data(), key.string_value.size());
                    return;
            }
        }

        bool Copy_Marshalled_Value(lua_State* source, lua_State* target, int index, size_t depth, s_marshalling_state* state) {
            if (!Add_Marshalled_Value(state)) {
                return false;
            }
            index = lua_absindex(source, index);
            switch (lua_type(source, index)) {
                case LUA_TNIL:
                    lua_pushnil(target);
                    return true;
                case LUA_TBOOLEAN:
                    lua_pushboolean(target, lua_toboolean(source, index));
                    return true;
                case LUA_TNUMBER:
                    if (!lua_isinteger(source, index)) {
                        state->diagnostic =
                            "cross-mod values must not be floating-point";
                        return false;
                    }
                    lua_pushinteger(target, lua_tointeger(source, index));
                    return true;
                case LUA_TSTRING: {
                    size_t size = 0;
                    const char* value = lua_tolstring(source, index, &size);
                    if (!Add_Marshalled_Bytes(state, size)) {
                        return false;
                    }
                    lua_pushlstring(target, value, size);
                    return true;
                }
                case LUA_TTABLE:
                    break;
                default:
                    state->diagnostic =
                        "cross-mod values support only nil, Boolean, integer, string, and table";
                    return false;
            }

            if (depth >= state->limits->marshalling_depth) {
                state->diagnostic = "cross-mod marshalling depth limit exceeded";
                return false;
            }
            if (lua_getmetatable(source, index) != 0) {
                lua_pop(source, 1);
                state->diagnostic =
                    "cross-mod tables must not have metatables";
                return false;
            }
            const void* identity = lua_topointer(source, index);
            if (!state->tables.insert(identity).second) {
                state->diagnostic =
                    "cross-mod tables must not contain aliases or cycles";
                return false;
            }

            std::vector<s_marshaled_key> keys;
            lua_pushnil(source);
            while (lua_next(source, index) != 0) {
                lua_pop(source, 1);
                if (state->entries >= state->limits->marshalled_entries) {
                    lua_pop(source, 1);
                    state->diagnostic =
                        "cross-mod marshalled table entry limit exceeded";
                    return false;
                }
                ++state->entries;
                s_marshaled_key key;
                if (!Read_Marshalled_Key(source, -1, &key, state)) {
                    lua_pop(source, 1);
                    return false;
                }
                keys.push_back(std::move(key));
            }
            std::sort(keys.begin(), keys.end(), Marshalled_Key_Less);

            lua_createtable(target, 0, (int)std::min<size_t>(keys.size(), (size_t)INT_MAX));
            const int target_table = lua_gettop(target);
            for (const s_marshaled_key& key : keys) {
                Push_Marshalled_Key(source, key);
                lua_rawget(source, index);
                Push_Marshalled_Key(target, key);
                if (!Copy_Marshalled_Value(source, target, -1, depth + 1, state)) {
                    lua_pop(source, 1);
                    return false;
                }
                lua_rawset(target, target_table);
                lua_pop(source, 1);
            }
            return true;
        }

        int Build_Export_Arguments(lua_State* provider_state,
            void* user_data) {
            auto* call = (s_export_call*)user_data;
            s_marshalling_state marshalling;
            marshalling.limits = call->limits;
            for (int index = 1; index <= call->caller_argument_count; ++index) {
                if (!Copy_Marshalled_Value(call->caller_state,
                        provider_state,
                        index,
                        0,
                        &marshalling)) {
                    call->diagnostic = marshalling.diagnostic;
                    lua_pushlstring(provider_state, call->diagnostic.data(), call->diagnostic.size());
                    return -1;
                }
            }
            return call->caller_argument_count;
        }

        int Consume_Export_Results(lua_State* provider_state,
            int result_count,
            void* user_data) {
            auto* call = (s_export_call*)user_data;
            s_marshalling_state marshalling;
            marshalling.limits = call->limits;
            for (int index = 1; index <= result_count; ++index) {
                if (!Copy_Marshalled_Value(provider_state,
                        call->caller_state,
                        index,
                        0,
                        &marshalling)) {
                    lua_settop(call->caller_state, call->caller_initial_top);
                    call->diagnostic = marshalling.diagnostic;
                    lua_pushlstring(provider_state, call->diagnostic.data(), call->diagnostic.size());
                    return -1;
                }
            }
            call->caller_result_count = result_count;
            return 0;
        }

        int Reject_Defines_Write(lua_State* lua_state) {
            return luaL_error(lua_state, "Lua defines are immutable");
        }

        void Seal_Defines_Object(lua_State* lua_state,
            int object,
            int values) {
            object = lua_absindex(lua_state, object);
            values = lua_absindex(lua_state, values);
            lua_newtable(lua_state);
            lua_pushvalue(lua_state, values);
            lua_setfield(lua_state, -2, "__index");
            lua_pushcfunction(lua_state, Reject_Defines_Write);
            lua_setfield(lua_state, -2, "__newindex");
            lua_pushliteral(lua_state, "immutable Lua defines");
            lua_setfield(lua_state, -2, "__metatable");
            lua_setmetatable(lua_state, object);
            lua_remove(lua_state, values);
        }

        void Install_Generated_Enum_Defines(lua_State* lua_state,
            int define_values,
            const char* define_name,
            const char* enum_name) {
            const config::s_generated_enum* type =
                config::Find_Generated_Enum(enum_name);
            if (type == nullptr) {
                luaL_error(lua_state,
                    "missing generated enum metadata '%s'",
                    enum_name);
            }
            define_values = lua_absindex(lua_state, define_values);
            lua_newtable(lua_state);
            const int object = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int values = lua_gettop(lua_state);
            for (size_t index = 0; index < type->value_count; ++index) {
                lua_pushinteger(lua_state, type->values[index].value);
                lua_setfield(lua_state, values, type->values[index].name);
            }
            Seal_Defines_Object(lua_state, object, values);
            lua_setfield(lua_state, define_values, define_name);
        }

        void Install_Defines_Api(lua_State* lua_state) {
            lua_newtable(lua_state);
            const int defines = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int define_values = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int climate = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int climate_values = lua_gettop(lua_state);
            constexpr struct {
                const char* name;
                PLANET_CLIMATE value;
            } climates[] = {
                {"toxic", PLANET_CLIMATE_TOXIC},
                {"radiated", PLANET_CLIMATE_RADIATED},
                {"barren", PLANET_CLIMATE_BARREN},
                {"desert", PLANET_CLIMATE_DESERT},
                {"tundra", PLANET_CLIMATE_TUNDRA},
                {"ocean", PLANET_CLIMATE_OCEAN},
                {"swamp", PLANET_CLIMATE_SWAMP},
                {"arid", PLANET_CLIMATE_ARID},
                {"terran", PLANET_CLIMATE_TERRAN},
                {"gaia", PLANET_CLIMATE_GAIA},
            };
            for (const auto& entry : climates) {
                lua_pushinteger(lua_state, entry.value);
                lua_setfield(lua_state, climate_values, entry.name);
            }
            Seal_Defines_Object(lua_state, climate, climate_values);
            lua_setfield(lua_state, define_values, "climate");

            lua_newtable(lua_state);
            const int planet_size = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int planet_size_values = lua_gettop(lua_state);
            constexpr struct {
                const char* name;
                PLANET_SIZE value;
            } planet_sizes[] = {
                {"tiny", PLANET_SIZE_TINY},
                {"small", PLANET_SIZE_SMALL},
                {"medium", PLANET_SIZE_MEDIUM},
                {"large", PLANET_SIZE_LARGE},
                {"huge", PLANET_SIZE_HUGE},
            };
            for (const auto& entry : planet_sizes) {
                lua_pushinteger(lua_state, entry.value);
                lua_setfield(lua_state, planet_size_values, entry.name);
            }
            Seal_Defines_Object(
                lua_state, planet_size, planet_size_values);
            lua_setfield(lua_state, define_values, "planet_size");

            lua_newtable(lua_state);
            const int planet_type = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int planet_type_values = lua_gettop(lua_state);
            constexpr struct {
                const char* name;
                PLANET_TYPE value;
            } planet_types[] = {
                {"asteroid", PLANET_TYPE_ASTEROID},
                {"gas_giant", PLANET_TYPE_GAS_GIANT},
                {"planet", PLANET_TYPE_PLANET},
            };
            for (const auto& entry : planet_types) {
                lua_pushinteger(lua_state, entry.value);
                lua_setfield(lua_state, planet_type_values, entry.name);
            }
            Seal_Defines_Object(
                lua_state, planet_type, planet_type_values);
            lua_setfield(lua_state, define_values, "planet_type");

            lua_newtable(lua_state);
            const int gravity = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int gravity_values = lua_gettop(lua_state);
            constexpr struct {
                const char* name;
                PLANET_GRAVITY value;
            } gravities[] = {
                {"low", PLANET_GRAVITY_LOW_G},
                {"normal", PLANET_GRAVITY_NORMAL_G},
                {"heavy", PLANET_GRAVITY_HEAVY_G},
            };
            for (const auto& entry : gravities) {
                lua_pushinteger(lua_state, entry.value);
                lua_setfield(lua_state, gravity_values, entry.name);
            }
            Seal_Defines_Object(lua_state, gravity, gravity_values);
            lua_setfield(lua_state, define_values, "gravity");

            lua_newtable(lua_state);
            const int minerals = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int mineral_values = lua_gettop(lua_state);
            constexpr struct {
                const char* name;
                MINERAL_RESOURCE value;
            } mineral_resources[] = {
                {"ultra_poor", MINERAL_RESOURCE_ULTRA_POOR},
                {"poor", MINERAL_RESOURCE_POOR},
                {"abundant", MINERAL_RESOURCE_ABUNDANT},
                {"rich", MINERAL_RESOURCE_RICH},
                {"ultra_rich", MINERAL_RESOURCE_ULTRA_RICH},
            };
            for (const auto& entry : mineral_resources) {
                lua_pushinteger(lua_state, entry.value);
                lua_setfield(lua_state, mineral_values, entry.name);
            }
            Seal_Defines_Object(lua_state, minerals, mineral_values);
            lua_setfield(lua_state, define_values, "minerals");

            lua_newtable(lua_state);
            const int government = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int government_values = lua_gettop(lua_state);
            constexpr struct {
                const char* name;
                GOVERNMENT value;
            } governments[] = {
                {"feudal", GOVERNMENT_FEUDAL},
                {"confederation", GOVERNMENT_CONFEDERATION},
                {"dictatorship", GOVERNMENT_DICTATORSHIP},
                {"imperium", GOVERNMENT_IMPERIUM},
                {"democracy", GOVERNMENT_DEMOCRACY},
                {"federation", GOVERNMENT_FEDERATION},
                {"unification", GOVERNMENT_UNIFICATION},
                {"galactic_unification", GOVERNMENT_GALACTIC_UNIFICATION},
            };
            for (const auto& entry : governments) {
                lua_pushinteger(lua_state, entry.value);
                lua_setfield(lua_state, government_values, entry.name);
            }
            Seal_Defines_Object(
                lua_state, government, government_values);
            lua_setfield(lua_state, define_values, "government");

            lua_newtable(lua_state);
            const int planet_special = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int planet_special_values = lua_gettop(lua_state);
            constexpr struct {
                const char* name;
                SYSTEM_SPECIAL value;
            } planet_specials[] = {
                {"none", SYSTEM_SPECIAL_NO_SPECIAL},
                {"stable_wormhole", SYSTEM_SPECIAL_STABLE_WORMHOLE},
                {"space_debris", SYSTEM_SPECIAL_SPACE_DEBRIS},
                {"pirate_cache", SYSTEM_SPECIAL_PIRATE_CACHE},
                {"gold_deposits", SYSTEM_SPECIAL_GOLD_DEPOSITS},
                {"gem_deposits", SYSTEM_SPECIAL_GEM_DEPOSITS},
                {"natives", SYSTEM_SPECIAL_NATIVES},
                {"splinter_colony", SYSTEM_SPECIAL_SPLINTER_COLONY},
                {"marooned_hero", SYSTEM_SPECIAL_MAROONED_HERO},
                {"space_monster", SYSTEM_SPECIAL_SPACE_MONSTER},
                {"ancient_artifacts", SYSTEM_SPECIAL_ANCIENT_ARTIFACTS},
                {"orion", SYSTEM_SPECIAL_ORION_STAR},
            };
            for (const auto& entry : planet_specials) {
                lua_pushinteger(lua_state, entry.value);
                lua_setfield(lua_state, planet_special_values, entry.name);
            }
            Seal_Defines_Object(
                lua_state, planet_special, planet_special_values);
            lua_setfield(lua_state, define_values, "planet_special");

            lua_newtable(lua_state);
            const int star_class = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int star_class_values = lua_gettop(lua_state);
            constexpr struct {
                const char* name;
                STAR_CLASS value;
            } star_classes[] = {
                {"b", STAR_CLASS_B},
                {"f", STAR_CLASS_F},
                {"g", STAR_CLASS_G},
                {"k", STAR_CLASS_K},
                {"m", STAR_CLASS_M},
                {"dwarf", STAR_CLASS_DWARF},
                {"black_hole", STAR_CLASS_BLACK_HOLE},
            };
            for (const auto& entry : star_classes) {
                lua_pushinteger(lua_state, entry.value);
                lua_setfield(lua_state, star_class_values, entry.name);
            }
            Seal_Defines_Object(
                lua_state, star_class, star_class_values);
            lua_setfield(lua_state, define_values, "star_class");

            lua_newtable(lua_state);
            const int leader_status = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int leader_status_values = lua_gettop(lua_state);
            constexpr struct {
                const char* name;
                int value;
            } leader_statuses[] = {
                {"dead", -2},
                {"unowned", -1},
                {"pool", 0},
                {"assigned", 1},
                {"limbo", 2},
                {"for_hire", 4},
                {"marooned", 5},
                {"special", 6},
            };
            for (const auto& entry : leader_statuses) {
                lua_pushinteger(lua_state, entry.value);
                lua_setfield(lua_state, leader_status_values, entry.name);
            }
            Seal_Defines_Object(
                lua_state, leader_status, leader_status_values);
            lua_setfield(lua_state, define_values, "leader_status");
            Install_Generated_Enum_Defines(lua_state,
                define_values,
                "ship_special",
                "ship_special");
            Install_Generated_Enum_Defines(lua_state,
                define_values,
                "technology_status",
                "technology_status");
            Seal_Defines_Object(lua_state, defines, define_values);
            lua_setglobal(lua_state, "defines");
        }

        int Set_Migration_Context(lua_State* lua_state, void* user_data) {
            auto* context = (s_migration_context*)user_data;
            if (!context->install) {
                lua_pushnil(lua_state);
                lua_setglobal(lua_state, "migration");
                return 0;
            }
            lua_newtable(lua_state);
            lua_newtable(lua_state);
            lua_newtable(lua_state);
            lua_pushinteger(lua_state, context->from);
            lua_setfield(lua_state, -2, "from");
            lua_pushinteger(lua_state, context->to);
            lua_setfield(lua_state, -2, "to");
            lua_setfield(lua_state, -2, "__index");
            lua_pushliteral(lua_state,
                "Lua migration metadata is immutable");
            lua_setfield(lua_state, -2, "__newindex");
            lua_pushliteral(lua_state, "Lua migration metadata");
            lua_setfield(lua_state, -2, "__metatable");
            lua_setmetatable(lua_state, -2);
            lua_setglobal(lua_state, "migration");
            return 0;
        }

        int Serialize_Storage(lua_State* lua_state, void* user_data) {
            auto* serialization = (s_storage_serialization*)user_data;
            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, serialization->runtime->storage_reference);
            if (!Serialize_Storage_With_References(lua_state, -1, &serialization->runtime->limits.storage, Get_Game_Storage_Reference_Adapter(), serialization->bytes, &serialization->diagnostic)) {
                lua_pushlstring(lua_state, serialization->diagnostic.data(), serialization->diagnostic.size());
                Mark_Runtime_Invocation_Error(lua_state, LUA_ERROR_RUNTIME);
                return lua_error(lua_state);
            }
            lua_pop(lua_state, 1);
            return 0;
        }

        void Add_Runtime_Identity(const config::s_mod_manifest& package,
            const std::vector<s_lua_runtime_source_record>& sources,
            const s_lua_mod_runtime* runtime,
            s_lua_registry* registry) {
            s_lua_runtime_package_record record;
            record.package_id = package.id;
            record.package_version = package.version_text;
            record.api = package.lua_runtime.api;
            record.storage_version = package.lua_runtime.storage_version;
            record.sources = sources;
            record.migrations.reserve(package.lua_runtime.migrations.size());
            for (const config::s_lua_migration_manifest& migration :
                package.lua_runtime.migrations) {
                for (const s_lua_runtime_source_record& source : sources) {
                    const std::string path =
                        "mods/" + package.id + "/" + migration.script;
                    if (source.path == path) {
                        record.migrations.push_back({migration.from,
                            migration.to,
                            source.path,
                            source.source_hash});
                        break;
                    }
                }
            }
            record.services.reserve(runtime->services.size());
            for (const s_registered_service& service : runtime->services) {
                record.services.push_back(
                    {service.info.name, service.info.version});
            }
            record.callbacks.reserve(runtime->callbacks.size());
            for (const s_registered_callback& callback : runtime->callbacks) {
                record.callbacks.push_back({
                    callback.info.kind,
                    callback.info.extension_name,
                    callback.info.schema,
                    callback.info.registration_ordinal,
                    callback.info.registration_source,
                    callback.info.registration_line,
                    callback.info.callback_source,
                    callback.info.callback_line,
                    callback.info.function_name,
                });
            }
            registry->runtime_packages.push_back(std::move(record));
        }

        const s_owned_event_descriptor* Find_Event(
            const s_lua_mod_runtime* runtime, const char* name) {
            for (const s_owned_event_descriptor& descriptor :
                runtime->event_descriptors) {
                if (descriptor.name == name) {
                    return &descriptor;
                }
            }
            return nullptr;
        }

        const s_registered_service* Find_Service(
            const s_lua_mod_runtime* runtime, std::string_view name) {
            for (const s_registered_service& service : runtime->services) {
                if (service.info.name == name) {
                    return &service;
                }
            }
            return nullptr;
        }

        const s_owned_rule_descriptor* Find_Rule(
            const s_lua_mod_runtime* runtime, const char* name) {
            for (const s_owned_rule_descriptor& descriptor :
                runtime->rule_descriptors) {
                if (descriptor.name == name) {
                    return &descriptor;
                }
            }
            return nullptr;
        }

        std::string Source_Name(const char* source) {
            if (source == nullptr) {
                return {};
            }
            return source[0] == '@' ? source + 1 : source;
        }

        bool Valid_Package_Id(std::string_view id) {
            if (id.empty() || id.front() < 'a' || id.front() > 'z') {
                return false;
            }
            for (const char value : id) {
                if ((value < 'a' || value > 'z') &&
                    (value < '0' || value > '9') && value != '_' &&
                    value != '-') {
                    return false;
                }
            }
            return true;
        }

        bool Module_Path(std::string_view name, std::string* relative_path) {
            if (name.empty()) {
                return false;
            }
            relative_path->clear();
            size_t segment_size = 0;
            for (const char value : name) {
                const bool first = segment_size == 0;
                const bool valid = (value >= 'a' && value <= 'z') ||
                    value == '_' ||
                    (!first && value >= '0' && value <= '9');
                if (value == '.') {
                    if (first) {
                        return false;
                    }
                    relative_path->push_back('/');
                    segment_size = 0;
                } else if (valid) {
                    relative_path->push_back(value);
                    ++segment_size;
                } else {
                    return false;
                }
            }
            if (segment_size == 0) {
                return false;
            }
            relative_path->append(".lua");
            return true;
        }

        void Set_Module_State(lua_State* lua_state, int reference, std::string_view name, bool value) {
            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, reference);
            lua_pushlstring(lua_state, name.data(), name.size());
            if (value) {
                lua_pushboolean(lua_state, 1);
            } else {
                lua_pushnil(lua_state);
            }
            lua_rawset(lua_state, -3);
            lua_pop(lua_state, 1);
        }

        void Push_Module_Error(lua_State* lua_state,
            const std::string& message) {
            lua_pushlstring(lua_state, message.data(), message.size());
        }

        bool Read_Module_Source(s_lua_mod_runtime* runtime,
            const std::string& relative_path,
            std::string* source,
            std::string* private_path,
            std::string* diagnostic) {
            *private_path = "mods/" + runtime->package_id + "/" + relative_path;
            s_vfs_file* file = vfs::fopen(private_path->c_str(), "rb");
            if (file == nullptr) {
                *diagnostic = *private_path + ": package Lua module not found";
                return false;
            }
            const int32_t signed_size = vfs::fsize(file);
            if (signed_size < 0) {
                vfs::fclose(file);
                *diagnostic = *private_path +
                    ": package Lua module has an invalid size";
                return false;
            }
            const size_t size = (size_t)signed_size;
            if (size > runtime->limits.runtime.source_bytes) {
                vfs::fclose(file);
                *diagnostic = *private_path +
                    ": Lua source byte limit exceeded";
                return false;
            }
            if (runtime->package_source_bytes >
                    runtime->limits.package_source_bytes ||
                size > runtime->limits.package_source_bytes -
                        runtime->package_source_bytes) {
                vfs::fclose(file);
                *diagnostic = *private_path +
                    ": Lua package source byte limit exceeded";
                return false;
            }
            try {
                source->resize(size);
            } catch (...) {
                vfs::fclose(file);
                throw;
            }
            const size_t read = size == 0 ? 0 : vfs::fread(source->data(), 1, size, file);
            const int32_t close_result = vfs::fclose(file);
            if (read != size || close_result != 0) {
                *diagnostic = *private_path +
                    ": unable to read package Lua module";
                return false;
            }
            return true;
        }

        int Require_Module_Impl(lua_State* lua_state,
            s_lua_mod_runtime* runtime,
            const char* name_data,
            size_t name_size) {
            const std::string name(name_data, name_size);
            std::string relative_path;
            if (!Module_Path(name, &relative_path)) {
                Push_Module_Error(lua_state,
                    "invalid package Lua module name '" + name + "'");
                return -1;
            }

            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, runtime->module_cache_reference);
            lua_pushvalue(lua_state, 1);
            lua_rawget(lua_state, -2);
            if (!lua_isnil(lua_state, -1)) {
                lua_remove(lua_state, -2);
                return 1;
            }
            lua_pop(lua_state, 2);

            if (!runtime->setup_active) {
                Push_Module_Error(lua_state,
                    "package Lua module '" + name +
                        "' was not loaded during main.lua setup");
                return -1;
            }

            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, runtime->loading_module_reference);
            lua_pushvalue(lua_state, 1);
            lua_rawget(lua_state, -2);
            const bool loading = lua_toboolean(lua_state, -1) != 0;
            lua_pop(lua_state, 2);
            if (loading) {
                Push_Module_Error(lua_state,
                    "package Lua module cycle while requiring '" + name + "'");
                return -1;
            }
            if (runtime->loading_modules.size() >=
                runtime->limits.module_nesting) {
                Push_Module_Error(lua_state,
                    "package Lua module nesting limit exceeded (" +
                        std::to_string(runtime->limits.module_nesting) +
                        " modules)");
                return -1;
            }

            Set_Module_State(lua_state, runtime->loading_module_reference, name, true);
            runtime->loading_modules.push_back(name);
            std::string source;
            std::string private_path;
            std::string diagnostic;
            size_t module_index = runtime->modules.size();
            for (size_t index = 0; index < runtime->modules.size();
                 ++index) {
                if (runtime->modules[index].name == name) {
                    module_index = index;
                    break;
                }
            }
            if (module_index < runtime->modules.size()) {
                source = runtime->module_sources[module_index];
                private_path = runtime->modules[module_index].path;
            } else {
                if (runtime->modules.size() >= runtime->limits.modules) {
                    runtime->loading_modules.pop_back();
                    Set_Module_State(lua_state,
                        runtime->loading_module_reference,
                        name,
                        false);
                    Push_Module_Error(lua_state,
                        "package Lua module limit exceeded (" +
                            std::to_string(runtime->limits.modules) +
                            " modules)");
                    return -1;
                }
                if (!Read_Module_Source(runtime, relative_path, &source, &private_path, &diagnostic)) {
                    runtime->loading_modules.pop_back();
                    Set_Module_State(lua_state,
                        runtime->loading_module_reference,
                        name,
                        false);
                    Push_Module_Error(lua_state, diagnostic);
                    return -1;
                }

                s_lua_module_info module;
                module.name = name;
                module.path = private_path;
                module.source_hash = crypto::sha256_hex(source);
                try {
                    runtime->modules.push_back(module);
                    runtime->module_sources.push_back(source);
                } catch (...) {
                    if (runtime->module_sources.size() <
                        runtime->modules.size()) {
                        runtime->modules.pop_back();
                    }
                    runtime->loading_modules.pop_back();
                    Set_Module_State(lua_state,
                        runtime->loading_module_reference,
                        name,
                        false);
                    throw;
                }
                runtime->package_source_bytes += source.size();
            }

            const bool compiled = Load_Runtime_Source_Chunk(lua_state,
                source.data(),
                source.size(),
                private_path.c_str());
            int status = LUA_ERRRUN;
            if (compiled) {
                status = lua_pcall(lua_state, 0, 1, 0);
            }
            runtime->loading_modules.pop_back();
            Set_Module_State(lua_state, runtime->loading_module_reference, name, false);
            if (!compiled || status != LUA_OK) {
                return -1;
            }
            if (lua_isnil(lua_state, -1)) {
                lua_pop(lua_state, 1);
                lua_pushboolean(lua_state, 1);
            }

            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, runtime->module_cache_reference);
            lua_pushvalue(lua_state, 1);
            lua_pushvalue(lua_state, -3);
            lua_rawset(lua_state, -3);
            lua_pop(lua_state, 1);
            return 1;
        }

        int Require_Module(lua_State* lua_state) {
            auto* runtime = (s_lua_mod_runtime*)lua_touserdata(
                lua_state, lua_upvalueindex(1));
            if (runtime == nullptr) {
                return luaL_error(lua_state, "invalid package Lua runtime");
            }
            if (lua_type(lua_state, 1) != LUA_TSTRING) {
                lua_pushliteral(lua_state,
                    "package Lua module name must be a string");
                Mark_Runtime_Invocation_Error(lua_state, LUA_ERROR_RUNTIME);
                return lua_error(lua_state);
            }
            size_t name_size = 0;
            const char* name = lua_tolstring(lua_state, 1, &name_size);
            int result = -1;
            try {
                result = Require_Module_Impl(lua_state, runtime, name, name_size);
            } catch (...) {
                lua_pushliteral(lua_state,
                    "unable to allocate package Lua module data");
            }
            if (result < 0) {
                Mark_Runtime_Invocation_Error(lua_state, LUA_ERROR_RUNTIME);
                return lua_error(lua_state);
            }
            return result;
        }

        bool Retain_Callback(lua_State* lua_state,
            s_lua_mod_runtime* runtime,
            e_lua_callback_kind kind,
            const char* extension_name,
            uint32_t schema) {
            lua_pushvalue(lua_state, 2);
            const int reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
            s_registered_callback callback;
            callback.reference = reference;
            try {
                callback.info.kind = kind;
                callback.info.extension_name = extension_name;
                callback.info.schema = schema;
                callback.info.registration_ordinal = runtime->callbacks.size();

                lua_Debug registration{};
                if (lua_getstack(lua_state, 1, &registration) != 0 &&
                    lua_getinfo(lua_state, "Sl", &registration) != 0) {
                    callback.info.registration_source =
                        Source_Name(registration.source);
                    callback.info.registration_line = registration.currentline;
                }

                lua_pushvalue(lua_state, 2);
                lua_Debug definition{};
                if (lua_getinfo(lua_state, ">Sln", &definition) != 0) {
                    callback.info.callback_source =
                        Source_Name(definition.source);
                    callback.info.callback_line = definition.linedefined;
                    if (definition.name != nullptr) {
                        callback.info.function_name = definition.name;
                    }
                }

                runtime->callbacks.push_back(std::move(callback));
            } catch (...) {
                luaL_unref(lua_state, LUA_REGISTRYINDEX, reference);
                return false;
            }
            return true;
        }

        int Register_Event(lua_State* lua_state) {
            auto* runtime = (s_lua_mod_runtime*)lua_touserdata(
                lua_state, lua_upvalueindex(1));
            if (runtime == nullptr || !runtime->setup_active) {
                return luaL_error(lua_state,
                    "event registration is available only during main.lua setup");
            }
            const char* event_name = luaL_checkstring(lua_state, 1);
            luaL_checktype(lua_state, 2, LUA_TFUNCTION);
            const s_owned_event_descriptor* descriptor =
                Find_Event(runtime, event_name);
            if (descriptor == nullptr) {
                return luaL_error(lua_state, "unknown event '%s'", event_name);
            }
            if (runtime->callbacks.size() >= runtime->limits.callbacks) {
                return luaL_error(lua_state,
                    "callback registration limit exceeded (%llu callbacks)",
                    (unsigned long long)runtime->limits.callbacks);
            }
            if (!Retain_Callback(lua_state,
                    runtime,
                    LUA_CALLBACK_EVENT,
                    event_name,
                    descriptor->schema)) {
                return luaL_error(lua_state,
                    "unable to retain Lua callback registration");
            }
            return 0;
        }

        int Register_Service(lua_State* lua_state) {
            auto* runtime = (s_lua_mod_runtime*)lua_touserdata(
                lua_state, lua_upvalueindex(1));
            if (runtime == nullptr || !runtime->setup_active) {
                return luaL_error(lua_state,
                    "service registration is available only during main.lua setup");
            }
            const char* name = luaL_checkstring(lua_state, 1);
            const lua_Integer version = luaL_checkinteger(lua_state, 2);
            luaL_checktype(lua_state, 3, LUA_TFUNCTION);
            if (version <= 0 || version > UINT32_MAX ||
                strcmp(name, "galaxy_generator") != 0 || version != 1) {
                return luaL_error(lua_state,
                    "unsupported Lua service '%s' version %lld",
                    name,
                    static_cast<long long>(version));
            }
            if (Find_Service(runtime, name) != nullptr) {
                return luaL_error(lua_state,
                    "duplicate Lua service registration '%s'",
                    name);
            }
            if (runtime->services.size() >= runtime->limits.services) {
                return luaL_error(lua_state,
                    "service registration limit exceeded (%llu services)",
                    (unsigned long long)runtime->limits.services);
            }

            lua_pushvalue(lua_state, 3);
            const int reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
            s_registered_service service;
            service.reference = reference;
            try {
                service.info.name = name;
                service.info.version = static_cast<uint32_t>(version);
                lua_Debug registration{};
                if (lua_getstack(lua_state, 1, &registration) != 0 &&
                    lua_getinfo(lua_state, "Sl", &registration) != 0) {
                    service.info.registration_source =
                        Source_Name(registration.source);
                    service.info.registration_line = registration.currentline;
                }
                lua_pushvalue(lua_state, 3);
                lua_Debug definition{};
                if (lua_getinfo(lua_state, ">Sln", &definition) != 0) {
                    service.info.callback_source =
                        Source_Name(definition.source);
                    service.info.callback_line = definition.linedefined;
                    if (definition.name != nullptr) {
                        service.info.function_name = definition.name;
                    }
                }
                runtime->services.push_back(std::move(service));
            } catch (...) {
                luaL_unref(lua_state, LUA_REGISTRYINDEX, reference);
                return luaL_error(lua_state,
                    "unable to retain Lua service registration");
            }
            return 0;
        }

        int Register_Rule(lua_State* lua_state) {
            auto* runtime = (s_lua_mod_runtime*)lua_touserdata(
                lua_state, lua_upvalueindex(1));
            if (runtime == nullptr || !runtime->setup_active) {
                return luaL_error(lua_state,
                    "rule registration is available only during main.lua setup");
            }
            const char* rule_name = luaL_checkstring(lua_state, 1);
            luaL_checktype(lua_state, 2, LUA_TFUNCTION);
            const s_owned_rule_descriptor* descriptor =
                Find_Rule(runtime, rule_name);
            if (descriptor == nullptr) {
                return luaL_error(lua_state, "unknown rule '%s'", rule_name);
            }
            if (runtime->callbacks.size() >= runtime->limits.callbacks) {
                return luaL_error(lua_state,
                    "callback registration limit exceeded (%llu callbacks)",
                    (unsigned long long)runtime->limits.callbacks);
            }
            if (!Retain_Callback(lua_state,
                    runtime,
                    LUA_CALLBACK_RULE,
                    rule_name,
                    descriptor->schema)) {
                return luaL_error(lua_state,
                    "unable to retain Lua rule modifier registration");
            }
            return 0;
        }

        const s_registered_export* Find_Export(
            const s_lua_mod_runtime* runtime, std::string_view name) {
            for (const s_registered_export& exported : runtime->exports) {
                if (exported.info.name == name) {
                    return &exported;
                }
            }
            return nullptr;
        }

        s_lua_mod_runtime* Find_Runtime(s_lua_mod_runtime_set* runtime_set,
            std::string_view package_id) {
            if (runtime_set == nullptr) {
                return nullptr;
            }
            for (s_lua_mod_runtime* runtime : runtime_set->runtimes) {
                if (runtime->package_id == package_id) {
                    return runtime;
                }
            }
            return nullptr;
        }

        bool Has_Dependency(const s_lua_mod_runtime* runtime,
            std::string_view package_id) {
            return std::find(runtime->dependencies.begin(),
                       runtime->dependencies.end(),
                       package_id) !=
                runtime->dependencies.end();
        }

        int Push_Callback_Error(lua_State* lua_state,
            const std::string& diagnostic) {
            lua_pushlstring(lua_state, diagnostic.data(), diagnostic.size());
            return -1;
        }

        int Register_Export_Impl(lua_State* lua_state) {
            auto* runtime = (s_lua_mod_runtime*)lua_touserdata(
                lua_state, lua_upvalueindex(1));
            if (runtime == nullptr || !runtime->setup_active) {
                return Push_Callback_Error(lua_state,
                    "export registration is available only during main.lua setup");
            }
            size_t name_size = 0;
            const char* name_data = lua_tolstring(lua_state, 1, &name_size);
            const std::string name(name_data, name_size);
            if (!Valid_Interface_Name(name)) {
                return Push_Callback_Error(lua_state,
                    "invalid cross-mod export name '" + name + "'");
            }
            const lua_Integer version_value = lua_tointeger(lua_state, 2);
            if (version_value <= 0 || version_value > UINT32_MAX) {
                return Push_Callback_Error(lua_state,
                    "cross-mod export version must be between 1 and " +
                        std::to_string(UINT32_MAX));
            }
            if (lua_getmetatable(lua_state, 3) != 0) {
                lua_pop(lua_state, 1);
                return Push_Callback_Error(lua_state,
                    "cross-mod export interface must not have a metatable");
            }
            if (Find_Export(runtime, name) != nullptr) {
                return Push_Callback_Error(lua_state,
                    "duplicate cross-mod export '" + name + "'");
            }
            if (runtime->exports.size() >= runtime->limits.exports) {
                return Push_Callback_Error(lua_state,
                    "cross-mod export limit exceeded (" +
                        std::to_string(runtime->limits.exports) + " exports)");
            }

            std::vector<std::string> methods;
            lua_pushnil(lua_state);
            while (lua_next(lua_state, 3) != 0) {
                if (lua_type(lua_state, -2) != LUA_TSTRING ||
                    !lua_isfunction(lua_state, -1)) {
                    lua_pop(lua_state, 2);
                    return Push_Callback_Error(lua_state,
                        "cross-mod export interface entries must be string-named functions");
                }
                size_t method_size = 0;
                const char* method_data = lua_tolstring(lua_state, -2, &method_size);
                const std::string method(method_data, method_size);
                if (!Valid_Interface_Name(method)) {
                    lua_pop(lua_state, 2);
                    return Push_Callback_Error(lua_state,
                        "invalid cross-mod export method '" + method + "'");
                }
                methods.push_back(method);
                lua_pop(lua_state, 1);
                if (methods.size() > runtime->limits.export_methods) {
                    lua_pop(lua_state, 1);
                    return Push_Callback_Error(lua_state,
                        "cross-mod export method limit exceeded (" +
                            std::to_string(runtime->limits.export_methods) +
                            " methods)");
                }
            }
            if (methods.empty()) {
                return Push_Callback_Error(lua_state,
                    "cross-mod export interface must contain at least one method");
            }
            std::sort(methods.begin(), methods.end());

            s_registered_export exported;
            exported.info.name = name;
            exported.info.version = (uint32_t)version_value;
            exported.info.registration_ordinal = runtime->exports.size();
            exported.info.methods = methods;
            lua_Debug registration{};
            if (lua_getstack(lua_state, 1, &registration) != 0 &&
                lua_getinfo(lua_state, "Sl", &registration) != 0) {
                exported.info.registration_source =
                    Source_Name(registration.source);
                exported.info.registration_line = registration.currentline;
            }
            exported.methods.reserve(methods.size());
            for (const std::string& method : methods) {
                lua_pushlstring(lua_state, method.data(), method.size());
                lua_rawget(lua_state, 3);
                const int reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
                exported.methods.push_back({method, reference});
            }
            runtime->exports.push_back(std::move(exported));
            return 0;
        }

        int Register_Export(lua_State* lua_state) {
            luaL_checktype(lua_state, 1, LUA_TSTRING);
            luaL_checkinteger(lua_state, 2);
            luaL_checktype(lua_state, 3, LUA_TTABLE);
            const int result = Register_Export_Impl(lua_state);
            return result < 0 ? lua_error(lua_state) : result;
        }

        int Invoke_Imported_Method_Impl(lua_State* lua_state) {
            auto* caller = (s_lua_mod_runtime*)lua_touserdata(
                lua_state, lua_upvalueindex(1));
            auto* provider = (s_lua_mod_runtime*)lua_touserdata(
                lua_state, lua_upvalueindex(2));
            const int reference = (int)lua_tointeger(lua_state,
                lua_upvalueindex(3));
            if (caller == nullptr || provider == nullptr ||
                caller->setup_active || provider->setup_active) {
                return Push_Callback_Error(lua_state,
                    "cross-mod exports are callable only after setup is sealed");
            }
            if (active_callback_kind_ == LUA_CALLBACK_RULE) {
                return Push_Callback_Error(lua_state,
                    "cross-mod exports are unavailable from rule modifiers");
            }
            if (cross_mod_call_depth_ >= caller->limits.cross_mod_call_depth) {
                return Push_Callback_Error(lua_state,
                    "cross-mod call depth limit exceeded (" +
                        std::to_string(caller->limits.cross_mod_call_depth) +
                        " calls)");
            }

            s_export_call call;
            call.caller_state = lua_state;
            call.caller_argument_count = lua_gettop(lua_state);
            call.caller_initial_top = call.caller_argument_count;
            call.limits = &caller->limits;
            if (provider->invocation_active) {
                return Push_Callback_Error(lua_state,
                    "cross-mod call would reenter package '" +
                        provider->package_id + "'");
            }
            Restore_Storage_Global(provider);
            Restore_Game_Global(provider);
            // A synchronous export continues under the caller's lifecycle
            // capability and cannot independently elevate game-state access.
            ++cross_mod_call_depth_;
            provider->invocation_active = true;
            s_lua_result result;
            const bool invoked = Invoke_Runtime_Function_Results(
                provider->state, reference, Build_Export_Arguments, &call, Consume_Export_Results, &call, &result);
            provider->invocation_active = false;
            --cross_mod_call_depth_;
            if (!invoked) {
                Restore_Storage_Global(provider);
                Restore_Game_Global(provider);
                lua_settop(lua_state, call.caller_initial_top);
                const std::string diagnostic = call.diagnostic.empty()
                    ? result.diagnostic
                    : call.diagnostic;
                return Push_Callback_Error(lua_state, diagnostic);
            }
            if (!Storage_Global_Unchanged(provider)) {
                Restore_Storage_Global(provider);
                Restore_Game_Global(provider);
                return Push_Callback_Error(lua_state,
                    "provider '" + provider->package_id +
                        "' replaced its engine-owned storage root");
            }
            if (!Game_Global_Unchanged(provider)) {
                Restore_Game_Global(provider);
                return Push_Callback_Error(lua_state,
                    "provider '" + provider->package_id +
                        "' replaced its engine-owned game root");
            }
            return call.caller_result_count;
        }

        int Invoke_Imported_Method(lua_State* lua_state) {
            const int result = Invoke_Imported_Method_Impl(lua_state);
            return result < 0 ? lua_error(lua_state) : result;
        }

        int Import_Export_Impl(lua_State* lua_state) {
            auto* caller = (s_lua_mod_runtime*)lua_touserdata(
                lua_state, lua_upvalueindex(1));
            if (caller == nullptr || !caller->setup_active) {
                return Push_Callback_Error(lua_state,
                    "cross-mod import is available only during main.lua setup");
            }
            size_t package_size = 0;
            const char* package_data = lua_tolstring(lua_state, 1, &package_size);
            size_t name_size = 0;
            const char* name_data = lua_tolstring(lua_state, 2, &name_size);
            const std::string package_id(package_data, package_size);
            const std::string name(name_data, name_size);
            if (!Valid_Package_Id(package_id) ||
                !Valid_Interface_Name(name)) {
                return Push_Callback_Error(lua_state,
                    "invalid cross-mod import identity");
            }
            if (!Has_Dependency(caller, package_id)) {
                return Push_Callback_Error(lua_state,
                    "package '" + package_id +
                        "' is not a declared dependency of '" +
                        caller->package_id + "'");
            }
            s_lua_mod_runtime* provider = Find_Runtime(caller->runtime_set,
                package_id);
            if (provider == nullptr) {
                return Push_Callback_Error(lua_state,
                    "cross-mod import provider '" + package_id +
                        "' is not active");
            }
            const s_registered_export* exported = Find_Export(provider, name);
            if (exported == nullptr) {
                return Push_Callback_Error(lua_state,
                    "package '" + package_id + "' does not export '" +
                        name + "'");
            }

            lua_createtable(lua_state, 0, (int)exported->methods.size());
            for (const s_export_method& method : exported->methods) {
                lua_pushlightuserdata(lua_state, caller);
                lua_pushlightuserdata(lua_state, provider);
                lua_pushinteger(lua_state, method.reference);
                lua_pushcclosure(lua_state, Invoke_Imported_Method, 3);
                lua_setfield(lua_state, -2, method.name.c_str());
            }
            return 1;
        }

        int Import_Export(lua_State* lua_state) {
            luaL_checktype(lua_state, 1, LUA_TSTRING);
            luaL_checktype(lua_state, 2, LUA_TSTRING);
            const int result = Import_Export_Impl(lua_state);
            return result < 0 ? lua_error(lua_state) : result;
        }

        int Install_Setup_Api(lua_State* lua_state, void* user_data) {
            auto* runtime = (s_lua_mod_runtime*)user_data;
            Install_Defines_Api(lua_state);
            lua_newtable(lua_state);
            lua_pushlightuserdata(lua_state, runtime);
            lua_pushcclosure(lua_state, Register_Event, 1);
            lua_setfield(lua_state, -2, "on");
            lua_setglobal(lua_state, "events");
            lua_newtable(lua_state);
            lua_pushlightuserdata(lua_state, runtime);
            lua_pushcclosure(lua_state, Register_Rule, 1);
            lua_setfield(lua_state, -2, "modify");
            lua_setglobal(lua_state, "rules");
            lua_newtable(lua_state);
            runtime->module_cache_reference =
                luaL_ref(lua_state, LUA_REGISTRYINDEX);
            lua_newtable(lua_state);
            runtime->loading_module_reference =
                luaL_ref(lua_state, LUA_REGISTRYINDEX);
            lua_pushlightuserdata(lua_state, runtime);
            lua_pushcclosure(lua_state, Require_Module, 1);
            lua_setglobal(lua_state, "require");
            lua_newtable(lua_state);
            lua_pushlightuserdata(lua_state, runtime);
            lua_pushcclosure(lua_state, Register_Export, 1);
            lua_setfield(lua_state, -2, "export");
            lua_pushlightuserdata(lua_state, runtime);
            lua_pushcclosure(lua_state, Import_Export, 1);
            lua_setfield(lua_state, -2, "import");
            lua_setglobal(lua_state, "mod");
            lua_newtable(lua_state);
            lua_pushlightuserdata(lua_state, runtime);
            lua_pushcclosure(lua_state, Register_Service, 1);
            lua_setfield(lua_state, -2, "register");
            lua_setglobal(lua_state, "services");
            lua_pushnil(lua_state);
            lua_setglobal(lua_state, "storage");
            if (game::Install_Game_State_Api(lua_state) != 0) {
                return luaL_error(lua_state,
                    "unable to install Lua game-state API");
            }
            lua_getglobal(lua_state, "game");
            runtime->game_reference =
                luaL_ref(lua_state, LUA_REGISTRYINDEX);
            if (runtime->game_traits != nullptr &&
                Install_Game_Traits_Api(lua_state, runtime->game_traits) != 0) {
                return luaL_error(lua_state,
                    "unable to install Lua race trait API");
            }
            return 0;
        }

        int Seal_Setup(lua_State* lua_state, void* user_data) {
            auto* runtime = (s_lua_mod_runtime*)user_data;
            if (!Game_Global_Unchanged(runtime)) {
                Restore_Game_Global(runtime);
                return luaL_error(lua_state,
                    "main.lua replaced the engine-owned game root");
            }
            runtime->setup_active = false;
            lua_pushnil(lua_state);
            lua_setglobal(lua_state, "events");
            lua_pushnil(lua_state);
            lua_setglobal(lua_state, "rules");
            lua_pushnil(lua_state);
            lua_setglobal(lua_state, "mod");
            lua_pushnil(lua_state);
            lua_setglobal(lua_state, "services");
            lua_pushnil(lua_state);
            lua_setglobal(lua_state, "prototypes");
            lua_newtable(lua_state);
            lua_pushvalue(lua_state, -1);
            runtime->storage_reference =
                luaL_ref(lua_state, LUA_REGISTRYINDEX);
            lua_setglobal(lua_state, "storage");
            return 0;
        }

        void Restore_Storage_Global(s_lua_mod_runtime* runtime) {
            lua_State* lua_state = Get_Runtime_State_Handle(runtime->state);
            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, runtime->storage_reference);
            lua_setglobal(lua_state, "storage");
        }

        bool Storage_Global_Unchanged(s_lua_mod_runtime* runtime) {
            lua_State* lua_state = Get_Runtime_State_Handle(runtime->state);
            lua_getglobal(lua_state, "storage");
            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, runtime->storage_reference);
            const bool unchanged = lua_rawequal(lua_state, -1, -2) != 0;
            lua_pop(lua_state, 2);
            return unchanged;
        }

        void Restore_Game_Global(s_lua_mod_runtime* runtime) {
            lua_State* lua_state = Get_Runtime_State_Handle(runtime->state);
            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, runtime->game_reference);
            lua_setglobal(lua_state, "game");
        }

        bool Game_Global_Unchanged(s_lua_mod_runtime* runtime) {
            lua_State* lua_state = Get_Runtime_State_Handle(runtime->state);
            lua_getglobal(lua_state, "game");
            lua_rawgeti(lua_state, LUA_REGISTRYINDEX, runtime->game_reference);
            const bool unchanged = lua_rawequal(lua_state, -1, -2) != 0;
            lua_pop(lua_state, 2);
            return unchanged;
        }

        void Set_Runtime_Error(s_lua_result* result,
            const std::string& diagnostic) {
            *result = {};
            result->error = LUA_ERROR_RUNTIME;
            result->diagnostic = diagnostic;
        }

        int Build_Rule_Arguments(lua_State* lua_state, void* user_data) {
            auto* invocation = static_cast<s_rule_invocation*>(user_data);
            const int initial_top = lua_gettop(lua_state);
            const int context_count = invocation->build_context == nullptr
                ? 0
                : invocation->build_context(lua_state, invocation->user_data);
            if (context_count != 1 ||
                lua_gettop(lua_state) - initial_top != 1) {
                lua_pushliteral(lua_state,
                    "unable to build Lua rule context");
                return -1;
            }
            lua_pushinteger(lua_state,
                static_cast<lua_Integer>(invocation->value));
            return 2;
        }

        int Consume_Rule_Result(lua_State* lua_state,
            int result_count,
            void* user_data) {
            auto* invocation = static_cast<s_rule_invocation*>(user_data);
            if (result_count != 1) {
                invocation->diagnostic =
                    "Lua rule modifier must return exactly one integer";
            } else if (!lua_isinteger(lua_state, 1)) {
                invocation->diagnostic =
                    "Lua rule modifier must return an integer";
            } else {
                const lua_Integer value = lua_tointeger(lua_state, 1);
                if (value < invocation->minimum ||
                    value > invocation->maximum) {
                    invocation->diagnostic =
                        "Lua rule modifier result is outside the permitted range";
                } else {
                    invocation->value = static_cast<std::int64_t>(value);
                    return 0;
                }
            }
            lua_pushlstring(lua_state,
                invocation->diagnostic.data(),
                invocation->diagnostic.size());
            return -1;
        }

        int Deserialize_Storage(lua_State* lua_state, void* user_data) {
            auto* restore = (s_storage_restore*)user_data;
            if (!Deserialize_Storage_With_References(lua_state, restore->bytes, restore->size, restore->limits, Get_Game_Storage_Reference_Adapter(), &restore->diagnostic)) {
                lua_pushlstring(lua_state, restore->diagnostic.data(), restore->diagnostic.size());
                Mark_Runtime_Invocation_Error(lua_state, LUA_ERROR_RUNTIME);
                return lua_error(lua_state);
            }
            restore->reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
            return 0;
        }

        bool Deserialize_Storage_Reference(s_lua_mod_runtime* runtime,
            const uint8_t* bytes,
            size_t size,
            int* reference,
            std::string* diagnostic) {
            s_storage_restore restore;
            restore.bytes = bytes;
            restore.size = size;
            restore.limits = &runtime->limits.storage;
            s_lua_result result;
            if (!Execute_Runtime_Operation(runtime->state,
                    Deserialize_Storage,
                    &restore,
                    &result)) {
                *diagnostic = restore.diagnostic.empty()
                    ? result.diagnostic
                    : restore.diagnostic;
                return false;
            }
            *reference = restore.reference;
            return true;
        }

        bool Replace_Storage_From_Bytes(s_lua_mod_runtime* runtime,
            const std::vector<uint8_t>& bytes,
            std::string* diagnostic) {
            int replacement_reference = LUA_NOREF;
            if (!Deserialize_Storage_Reference(runtime,
                    bytes.data(),
                    bytes.size(),
                    &replacement_reference,
                    diagnostic)) {
                return false;
            }
            lua_State* lua_state = Get_Runtime_State_Handle(runtime->state);
            const int replaced_reference = runtime->storage_reference;
            runtime->storage_reference = replacement_reference;
            Restore_Storage_Global(runtime);
            luaL_unref(lua_state, LUA_REGISTRYINDEX, replaced_reference);
            return true;
        }

        const s_runtime_migration* Find_Migration(
            const s_lua_mod_runtime* runtime, uint32_t from) {
            for (const s_runtime_migration& migration : runtime->migrations) {
                if (migration.from == from) {
                    return &migration;
                }
            }
            return nullptr;
        }

        int Reject_Immutable_Event_Write(lua_State* lua_state) {
            return luaL_error(lua_state,
                "%s",
                lua_tostring(lua_state, lua_upvalueindex(1)));
        }

        int Immutable_Sequence_Length(lua_State* lua_state) {
            lua_pushvalue(lua_state, lua_upvalueindex(1));
            return 1;
        }

        void Seal_Immutable_Event_Object(lua_State* lua_state,
            int object_index,
            int values_index,
            const char* diagnostic,
            bool sequence) {
            object_index = lua_absindex(lua_state, object_index);
            values_index = lua_absindex(lua_state, values_index);
            lua_newtable(lua_state);
            lua_pushvalue(lua_state, values_index);
            lua_setfield(lua_state, -2, "__index");
            lua_pushstring(lua_state, diagnostic);
            lua_pushcclosure(lua_state, Reject_Immutable_Event_Write, 1);
            lua_setfield(lua_state, -2, "__newindex");
            if (sequence) {
                lua_pushinteger(lua_state,
                    static_cast<lua_Integer>(lua_rawlen(
                        lua_state, values_index)));
                lua_pushcclosure(lua_state, Immutable_Sequence_Length, 1);
                lua_setfield(lua_state, -2, "__len");
            }
            lua_pushliteral(lua_state, "immutable Lua event object");
            lua_setfield(lua_state, -2, "__metatable");
            lua_setmetatable(lua_state, object_index);
            lua_remove(lua_state, values_index);
        }

        void Push_Configuration_Change(lua_State* lua_state,
            const s_lua_configuration_change& change) {
            lua_newtable(lua_state);
            const int object = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int values = lua_gettop(lua_state);
            lua_pushlstring(lua_state,
                change.package_id.data(),
                change.package_id.size());
            lua_setfield(lua_state, values, "package_id");
            lua_pushlstring(lua_state,
                change.old_package_version.data(),
                change.old_package_version.size());
            lua_setfield(lua_state, values, "old_package_version");
            lua_pushlstring(lua_state,
                change.new_package_version.data(),
                change.new_package_version.size());
            lua_setfield(lua_state, values, "new_package_version");
            if (change.old_storage_version != 0) {
                lua_pushinteger(lua_state, change.old_storage_version);
                lua_setfield(lua_state, values, "old_storage_version");
            }
            if (change.new_storage_version != 0) {
                lua_pushinteger(lua_state, change.new_storage_version);
                lua_setfield(lua_state, values, "new_storage_version");
            }
            Seal_Immutable_Event_Object(lua_state,
                object,
                values,
                "game.configuration_changed entries are immutable",
                false);
        }

        void Push_Immutable_String_Sequence(lua_State* lua_state,
            const std::vector<std::string>* values,
            const char* diagnostic) {
            lua_newtable(lua_state);
            const int sequence = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int sequence_values = lua_gettop(lua_state);
            if (values != nullptr) {
                for (size_t index = 0; index < values->size(); ++index) {
                    lua_pushinteger(lua_state,
                        static_cast<lua_Integer>(index + 1));
                    lua_pushlstring(lua_state,
                        (*values)[index].data(),
                        (*values)[index].size());
                    lua_rawset(lua_state, sequence_values);
                }
            }
            Seal_Immutable_Event_Object(lua_state,
                sequence,
                sequence_values,
                diagnostic,
                true);
        }

        int Build_Configuration_Changed_Event(lua_State* lua_state,
            void* user_data) {
            const auto* context =
                static_cast<const s_configuration_changed_context*>(user_data);
            lua_newtable(lua_state);
            const int event = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int event_values = lua_gettop(lua_state);
            lua_pushstring(lua_state, context->reason);
            lua_setfield(lua_state, event_values, "reason");
            lua_newtable(lua_state);
            const int sequence = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int sequence_values = lua_gettop(lua_state);
            if (context->changes != nullptr) {
                for (size_t index = 0;
                     index < context->changes->size();
                     ++index) {
                    lua_pushinteger(lua_state,
                        static_cast<lua_Integer>(index + 1));
                    Push_Configuration_Change(
                        lua_state, (*context->changes)[index]);
                    lua_rawset(lua_state, sequence_values);
                }
            }
            Seal_Immutable_Event_Object(lua_state,
                sequence,
                sequence_values,
                "game.configuration_changed change list is immutable",
                true);
            lua_setfield(lua_state, event_values, "changes");
            Push_Immutable_String_Sequence(lua_state,
                context->changed_groups,
                "game.configuration_changed group list is immutable");
            lua_setfield(lua_state, event_values, "changed_groups");
            Seal_Immutable_Event_Object(lua_state,
                event,
                event_values,
                "game.configuration_changed event payload is immutable",
                false);
            return 1;
        }

        int Build_Loaded_Event(lua_State* lua_state, void*) {
            lua_newtable(lua_state);
            const int event = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int values = lua_gettop(lua_state);
            Seal_Immutable_Event_Object(lua_state,
                event,
                values,
                "game.loaded event payload is immutable",
                false);
            return 1;
        }

        bool Dispatch_Loaded_Event_Impl(
            s_lua_mod_runtime_set* runtime_set,
            std::string* diagnostic) {
            for (size_t index = 0; index < runtime_set->runtimes.size(); ++index) {
                s_lua_mod_runtime* runtime = runtime_set->runtimes[index];
                std::vector<uint8_t> prior_storage;
                if (!Serialize_Mod_Storage(runtime, &prior_storage, diagnostic)) {
                    return false;
                }
                s_lua_result result;
                if (!Dispatch_Mod_Event(runtime,
                        "game.loaded",
                        Build_Loaded_Event,
                        nullptr,
                        &result)) {
                    *diagnostic = runtime->package_id +
                        ": game.loaded callback failed: " + result.diagnostic;
                    return false;
                }
                std::vector<uint8_t> current;
                if (!Serialize_Mod_Storage(runtime, &current, diagnostic)) {
                    return false;
                }
                if (current != prior_storage) {
                    *diagnostic = runtime->package_id +
                        ": game.loaded callback modified persistent storage";
                    return false;
                }
            }
            return true;
        }

    } // namespace

    bool Create_Mod_Runtime(const char* package_id, const char* package_version, const char* main_source, size_t main_source_size, const char* main_source_name, size_t package_source_bytes, const s_lua_event_descriptor* event_descriptors, size_t event_descriptor_count, const s_lua_rule_descriptor* rule_descriptors, size_t rule_descriptor_count, const s_lua_mod_limits* limits, s_lua_mod_runtime** runtime, s_lua_result* result) {
        if (package_id == nullptr || package_version == nullptr ||
            main_source == nullptr || main_source_name == nullptr ||
            (event_descriptor_count != 0 && event_descriptors == nullptr) ||
            (rule_descriptor_count != 0 && rule_descriptors == nullptr) ||
            limits == nullptr || runtime == nullptr || result == nullptr) {
            return false;
        }
        *runtime = nullptr;
        if (!Valid_Package_Id(package_id)) {
            Set_Runtime_Error(result, "invalid Lua package ID");
            return false;
        }
        if (package_source_bytes < main_source_size) {
            Set_Runtime_Error(result,
                "Lua package source accounting excludes main.lua");
            return false;
        }
        auto* candidate = new (std::nothrow) s_lua_mod_runtime;
        if (candidate == nullptr) {
            Set_Runtime_Error(result, "unable to allocate Lua mod runtime");
            result->error = LUA_ERROR_MEMORY_LIMIT;
            return false;
        }
        try {
            candidate->package_id = package_id;
            candidate->package_version = package_version;
            candidate->limits = *limits;
            candidate->runtime_set = creation_context_.runtime_set;
            candidate->game_traits = creation_context_.game_traits;
            if (creation_context_.package != nullptr) {
                const config::s_mod_manifest& package =
                    *creation_context_.package;
                candidate->dependencies.reserve(
                    package.dependencies.size() +
                    package.optional_dependencies.size());
                for (const config::s_mod_dependency& dependency :
                    package.dependencies) {
                    candidate->dependencies.push_back(dependency.id);
                }
                for (const config::s_mod_dependency& dependency :
                    package.optional_dependencies) {
                    candidate->dependencies.push_back(dependency.id);
                }
            }
            candidate->package_source_bytes = package_source_bytes;
            if (package_source_bytes > limits->package_source_bytes) {
                delete candidate;
                Set_Runtime_Error(result,
                    "Lua package source byte limit exceeded");
                result->error = LUA_ERROR_SOURCE_LIMIT;
                return false;
            }
            candidate->event_descriptors.reserve(event_descriptor_count);
            for (size_t index = 0; index < event_descriptor_count; ++index) {
                if (event_descriptors[index].name == nullptr) {
                    delete candidate;
                    Set_Runtime_Error(result,
                        "Lua event descriptor name must not be null");
                    return false;
                }
                candidate->event_descriptors.push_back(
                    {event_descriptors[index].name,
                        event_descriptors[index].schema,
                        event_descriptors[index].game_state});
            }
            candidate->rule_descriptors.reserve(rule_descriptor_count);
            for (size_t index = 0; index < rule_descriptor_count; ++index) {
                if (rule_descriptors[index].name == nullptr) {
                    delete candidate;
                    Set_Runtime_Error(result,
                        "Lua rule descriptor name must not be null");
                    return false;
                }
                candidate->rule_descriptors.push_back(
                    {rule_descriptors[index].name,
                        rule_descriptors[index].schema});
            }
        } catch (...) {
            delete candidate;
            Set_Runtime_Error(result, "unable to allocate Lua mod metadata");
            result->error = LUA_ERROR_MEMORY_LIMIT;
            return false;
        }

        if (!Create_Runtime_State(
                &limits->runtime, &candidate->state, result)) {
            delete candidate;
            return false;
        }
        if (!Execute_Runtime_Operation(candidate->state, Install_Setup_Api, candidate, result)) {
            Destroy_Mod_Runtime(candidate);
            return false;
        }
        if (!Execute_Runtime_Source(candidate->state, main_source, main_source_size, main_source_name, result)) {
            Destroy_Mod_Runtime(candidate);
            return false;
        }
        if (!Execute_Runtime_Operation(candidate->state, Seal_Setup, candidate, result)) {
            Destroy_Mod_Runtime(candidate);
            return false;
        }
        *runtime = candidate;
        return true;
    }

    bool Has_Active_Galaxy_Generator() {
        for (const s_lua_mod_runtime* runtime : active_runtimes_.runtimes) {
            if (Find_Service(runtime, "galaxy_generator") != nullptr) {
                return true;
            }
        }
        return false;
    }

    size_t Get_Mod_Service_Count(const s_lua_mod_runtime* runtime) {
        return runtime == nullptr ? 0 : runtime->services.size();
    }

    bool Get_Mod_Service_Info(const s_lua_mod_runtime* runtime,
        size_t index,
        s_lua_service_info* info) {
        if (runtime == nullptr || info == nullptr ||
            index >= runtime->services.size()) {
            return false;
        }
        *info = runtime->services[index].info;
        return true;
    }

    size_t Get_Mod_Export_Count(const s_lua_mod_runtime* runtime) {
        return runtime == nullptr ? 0 : runtime->exports.size();
    }

    bool Get_Mod_Export_Info(const s_lua_mod_runtime* runtime,
        size_t index,
        s_lua_export_info* info) {
        if (runtime == nullptr || info == nullptr ||
            index >= runtime->exports.size()) {
            return false;
        }
        *info = runtime->exports[index].info;
        return true;
    }

    size_t Get_Mod_Module_Count(const s_lua_mod_runtime* runtime) {
        return runtime == nullptr ? 0 : runtime->modules.size();
    }

    bool Get_Mod_Module_Info(const s_lua_mod_runtime* runtime,
        size_t index,
        s_lua_module_info* info) {
        if (runtime == nullptr || info == nullptr ||
            index >= runtime->modules.size()) {
            return false;
        }
        *info = runtime->modules[index];
        return true;
    }

    void Destroy_Mod_Runtime(s_lua_mod_runtime* runtime) {
        if (runtime != nullptr) {
            Destroy_Runtime_State(runtime->state);
            delete runtime;
        }
    }

    size_t Get_Mod_Callback_Count(const s_lua_mod_runtime* runtime) {
        return runtime == nullptr ? 0 : runtime->callbacks.size();
    }

    bool Get_Mod_Callback_Info(const s_lua_mod_runtime* runtime,
        size_t index,
        s_lua_callback_info* info) {
        if (runtime == nullptr || info == nullptr ||
            index >= runtime->callbacks.size()) {
            return false;
        }
        *info = runtime->callbacks[index].info;
        return true;
    }

    bool Dispatch_Mod_Event(s_lua_mod_runtime* runtime, const char* event_name, t_lua_argument_builder build_event, void* user_data, s_lua_result* result) {
        if (runtime == nullptr || event_name == nullptr || result == nullptr) {
            return false;
        }
        const s_owned_event_descriptor* descriptor =
            Find_Event(runtime, event_name);
        if (descriptor == nullptr) {
            Set_Runtime_Error(result,
                std::string("unknown event '") + event_name + "'");
            return false;
        }
        for (const s_registered_callback& callback : runtime->callbacks) {
            if (callback.info.kind != LUA_CALLBACK_EVENT ||
                callback.info.extension_name != event_name) {
                continue;
            }
            Restore_Storage_Global(runtime);
            Restore_Game_Global(runtime);
            runtime->invocation_active = true;
            bool invoked = false;
            {
                s_event_invocation_scope scope(descriptor->game_state);
                invoked = Invoke_Runtime_Function(runtime->state,
                    callback.reference,
                    build_event,
                    user_data,
                    result);
            }
            if (!invoked) {
                runtime->invocation_active = false;
                Restore_Storage_Global(runtime);
                Restore_Game_Global(runtime);
                return false;
            }
            runtime->invocation_active = false;
            if (!Storage_Global_Unchanged(runtime)) {
                Restore_Storage_Global(runtime);
                Restore_Game_Global(runtime);
                Set_Runtime_Error(result,
                    runtime->package_id + ": callback for '" + event_name +
                        "' replaced the engine-owned storage root");
                return false;
            }
            if (!Game_Global_Unchanged(runtime)) {
                Restore_Game_Global(runtime);
                Set_Runtime_Error(result,
                    runtime->package_id + ": callback for '" + event_name +
                        "' replaced the engine-owned game root");
                return false;
            }
        }
        *result = {};
        result->value_kind = LUA_VALUE_NIL;
        return true;
    }

    bool Dispatch_Mod_Rule(s_lua_mod_runtime* runtime,
        const char* rule_name,
        t_lua_argument_builder build_context,
        void* user_data,
        std::int64_t minimum,
        std::int64_t maximum,
        std::int64_t* value,
        s_lua_result* result) {
        if (result == nullptr) {
            return false;
        }
        *result = {};
        if (runtime == nullptr || rule_name == nullptr ||
            build_context == nullptr || value == nullptr) {
            Set_Runtime_Error(result, "invalid Lua rule dispatch arguments");
            return false;
        }
        if (minimum > maximum || *value < minimum || *value > maximum) {
            Set_Runtime_Error(result,
                "Lua rule input is outside the permitted range");
            return false;
        }
        if (runtime->invocation_active) {
            Set_Runtime_Error(result,
                runtime->package_id + ": rule dispatch would reenter package");
            return false;
        }
        if (Find_Rule(runtime, rule_name) == nullptr) {
            Set_Runtime_Error(result,
                std::string("unknown rule '") + rule_name + "'");
            return false;
        }
        for (const s_registered_callback& callback : runtime->callbacks) {
            if (callback.info.kind != LUA_CALLBACK_RULE ||
                callback.info.extension_name != rule_name) {
                continue;
            }
            std::vector<uint8_t> prior_storage;
            std::string storage_diagnostic;
            if (!Serialize_Mod_Storage(
                    runtime, &prior_storage, &storage_diagnostic)) {
                Set_Runtime_Error(result, storage_diagnostic);
                return false;
            }
            int isolated_storage_reference = LUA_NOREF;
            if (!Deserialize_Storage_Reference(runtime,
                    prior_storage.data(),
                    prior_storage.size(),
                    &isolated_storage_reference,
                    &storage_diagnostic)) {
                Set_Runtime_Error(result, storage_diagnostic);
                return false;
            }
            s_rule_invocation invocation;
            invocation.build_context = build_context;
            invocation.user_data = user_data;
            invocation.value = *value;
            invocation.minimum = minimum;
            invocation.maximum = maximum;
            bool invoked = false;
            bool storage_root_unchanged = false;
            bool game_root_unchanged = false;
            std::vector<uint8_t> isolated_storage;
            bool isolated_storage_valid = false;
            {
                s_rule_invocation_scope scope(
                    runtime, isolated_storage_reference);
                invoked = Invoke_Runtime_Function_Results(
                    runtime->state,
                    callback.reference,
                    Build_Rule_Arguments,
                    &invocation,
                    Consume_Rule_Result,
                    &invocation,
                    result);
                storage_root_unchanged =
                    Storage_Global_Unchanged(runtime);
                game_root_unchanged = Game_Global_Unchanged(runtime);
                isolated_storage_valid = storage_root_unchanged &&
                    Serialize_Mod_Storage(
                        runtime, &isolated_storage, &storage_diagnostic);
            }

            std::vector<uint8_t> persistent_storage;
            const bool persistent_storage_valid = Serialize_Mod_Storage(
                runtime, &persistent_storage, &storage_diagnostic);
            const bool persistent_storage_unchanged =
                persistent_storage_valid && persistent_storage == prior_storage;
            if (!persistent_storage_unchanged &&
                !Replace_Storage_From_Bytes(
                    runtime, prior_storage, &storage_diagnostic)) {
                Set_Runtime_Error(result,
                    runtime->package_id +
                        ": unable to restore storage after rule modifier: " +
                        storage_diagnostic);
                return false;
            }
            if (!game_root_unchanged) {
                Set_Runtime_Error(result,
                    runtime->package_id + ": rule modifier for '" +
                        rule_name +
                        "' replaced the engine-owned game root");
                return false;
            }

            if (!invoked) {
                return false;
            }
            if (!storage_root_unchanged) {
                Set_Runtime_Error(result,
                    runtime->package_id + ": rule modifier for '" +
                        rule_name +
                        "' replaced the engine-owned storage root");
                return false;
            }
            if (!isolated_storage_valid) {
                Set_Runtime_Error(result, storage_diagnostic);
                return false;
            }
            if (isolated_storage != prior_storage ||
                !persistent_storage_unchanged) {
                Set_Runtime_Error(result,
                    runtime->package_id + ": rule modifier for '" +
                        rule_name + "' modified persistent storage");
                return false;
            }
            *value = invocation.value;
        }
        *result = {};
        result->value_kind = LUA_VALUE_INTEGER;
        result->integer_value = *value;
        return true;
    }

    bool Game_State_Access_Allowed() {
        return active_callback_depth_ != 0;
    }

    bool Game_State_Mutation_Allowed() {
        return active_callback_depth_ != 0 &&
            active_callback_kind_ == LUA_CALLBACK_EVENT &&
            (active_game_state_ == LUA_GAME_STATE_MUTABLE ||
                active_game_state_ == LUA_GAME_STATE_PLAYER_SETUP_MUTABLE);
    }

    bool Player_Setup_State_Mutation_Allowed() {
        return active_callback_depth_ != 0 &&
            active_callback_kind_ == LUA_CALLBACK_EVENT &&
            active_game_state_ == LUA_GAME_STATE_PLAYER_SETUP_MUTABLE;
    }

    bool Technology_State_Mutation_Allowed() {
        return active_callback_depth_ != 0 &&
            active_callback_kind_ == LUA_CALLBACK_EVENT &&
            active_game_state_ == LUA_GAME_STATE_TECHNOLOGY_MUTABLE;
    }

    bool Planet_State_Mutation_Allowed() {
        return active_callback_depth_ != 0 &&
            active_callback_kind_ == LUA_CALLBACK_EVENT &&
            active_game_state_ == LUA_GAME_STATE_PLANET_MUTABLE;
    }

    bool Galaxy_State_Mutation_Allowed() {
        return active_callback_depth_ != 0 &&
            active_callback_kind_ == LUA_CALLBACK_EVENT &&
            active_game_state_ == LUA_GAME_STATE_GALAXY_MUTABLE;
    }

    bool Rule_Modifier_Active() {
        return active_callback_kind_ == LUA_CALLBACK_RULE;
    }

    bool Stage_Mod_Runtimes(
        const std::vector<config::s_mod_manifest>* packages_high_to_low_priority,
        const s_lua_mod_limits* limits,
        s_lua_registry* registry,
        s_lua_mod_runtime_set** candidate,
        config::s_cfg_diagnostic* diagnostic) {
        if (packages_high_to_low_priority == nullptr || limits == nullptr ||
            registry == nullptr || candidate == nullptr || diagnostic == nullptr) {
            return false;
        }
        *candidate = nullptr;
        auto* staged = new (std::nothrow) s_lua_mod_runtime_set;
        if (staged == nullptr) {
            return Fail("Lua activation", "unable to allocate Lua runtime set", diagnostic);
        }
        if (!game::Capture_Ship_Generation_State(
                staged->ship_generations, MAX_SHIPS)) {
            delete staged;
            return Fail("Lua activation",
                "unable to capture Lua ship generations",
                diagnostic);
        }
        if (!Create_Game_Traits(&staged->game_traits)) {
            delete staged;
            return Fail("Lua activation",
                "unable to allocate Lua game trait state",
                diagnostic);
        }
        registry->runtime_packages.clear();

        try {
            for (size_t offset = packages_high_to_low_priority->size();
                 offset > 0;
                 --offset) {
                const config::s_mod_manifest& package =
                    (*packages_high_to_low_priority)[offset - 1];
                if (!package.has_lua_runtime) {
                    continue;
                }
                const std::string manifest_path =
                    "mods/" + package.id + "/mod.json5";
                if (package.lua_runtime.api != 1) {
                    Destroy_Mod_Runtime_Set(staged);
                    return Fail(manifest_path,
                        "unsupported Lua API version " +
                            std::to_string(package.lua_runtime.api),
                        diagnostic);
                }
                size_t package_bytes = 0;
                std::string main_source;
                std::string main_path;
                if (!Read_Package_Source(package.id, "main.lua", &package_bytes, limits, &main_source, &main_path, diagnostic)) {
                    Destroy_Mod_Runtime_Set(staged);
                    return false;
                }
                std::vector<s_lua_runtime_source_record> sources;
                std::vector<s_runtime_migration> migrations;
                sources.push_back(
                    {main_path, crypto::sha256_hex(main_source)});
                for (const config::s_lua_migration_manifest& migration :
                    package.lua_runtime.migrations) {
                    std::string migration_source;
                    std::string migration_path;
                    if (!Read_Package_Source(package.id, migration.script, &package_bytes, limits, &migration_source, &migration_path, diagnostic)) {
                        Destroy_Mod_Runtime_Set(staged);
                        return false;
                    }
                    sources.push_back({migration_path,
                        crypto::sha256_hex(migration_source)});
                    migrations.push_back({migration.from, migration.to, migration_path, std::move(migration_source)});
                }

                s_lua_result result;
                s_lua_mod_runtime* runtime = nullptr;
                creation_context_ = {staged, &package, staged->game_traits};
                if (!Create_Mod_Runtime(package.id.c_str(),
                        package.version_text.c_str(),
                        main_source.data(),
                        main_source.size(),
                        main_path.c_str(),
                        package_bytes,
                        RUNTIME_EVENTS_,
                        std::size(RUNTIME_EVENTS_),
                        RUNTIME_RULES_,
                        std::size(RUNTIME_RULES_),
                        limits,
                        &runtime,
                        &result)) {
                    creation_context_ = {};
                    Destroy_Mod_Runtime_Set(staged);
                    return Fail(main_path,
                        "Lua activation failed: " + result.diagnostic,
                        diagnostic);
                }
                creation_context_ = {};
                runtime->api = package.lua_runtime.api;
                runtime->storage_version =
                    package.lua_runtime.storage_version;
                runtime->migrations = std::move(migrations);
                const size_t identity_count =
                    registry->runtime_packages.size();
                try {
                    for (size_t index = 0;
                         index < Get_Mod_Module_Count(runtime);
                         ++index) {
                        s_lua_module_info module;
                        Get_Mod_Module_Info(runtime, index, &module);
                        sources.push_back(
                            {module.path, module.source_hash});
                    }
                    Add_Runtime_Identity(package, sources, runtime, registry);
                    staged->runtimes.push_back(runtime);
                } catch (...) {
                    if (registry->runtime_packages.size() != identity_count) {
                        registry->runtime_packages.pop_back();
                    }
                    Destroy_Mod_Runtime(runtime);
                    throw;
                }
            }
            const s_lua_mod_runtime* selected_galaxy_generator = nullptr;
            for (const s_lua_mod_runtime* runtime : staged->runtimes) {
                if (Find_Service(runtime, "galaxy_generator") == nullptr) {
                    continue;
                }
                if (selected_galaxy_generator != nullptr) {
                    const std::string first =
                        selected_galaxy_generator->package_id;
                    const std::string second = runtime->package_id;
                    Destroy_Mod_Runtime_Set(staged);
                    registry->runtime_packages.clear();
                    return Fail("Lua activation",
                        "multiple providers for exclusive Lua service 'galaxy_generator': '" +
                            first + "' and '" + second + "'",
                        diagnostic);
                }
                selected_galaxy_generator = runtime;
            }
            Seal_Race_Trait_Prototypes(staged->game_traits);
            if (!Build_Race_Trait_Prototype_Identity(
                    staged->game_traits, &registry->prototype_identity)) {
                Destroy_Mod_Runtime_Set(staged);
                registry->runtime_packages.clear();
                return Fail("Lua activation",
                    "unable to build Lua race trait prototype identity",
                    diagnostic);
            }
        } catch (...) {
            creation_context_ = {};
            Destroy_Mod_Runtime_Set(staged);
            registry->runtime_packages.clear();
            return Fail("Lua activation",
                "unable to allocate staged Lua runtime data",
                diagnostic);
        }
        *diagnostic = {};
        *candidate = staged;
        return true;
    }

    bool Dispatch_Active_Galaxy_Generator(
        bool* invoked, std::string* diagnostic) {
        if (invoked == nullptr || diagnostic == nullptr) {
            return false;
        }
        *invoked = false;
        diagnostic->clear();
        s_lua_mod_runtime* provider = nullptr;
        const s_registered_service* service = nullptr;
        for (s_lua_mod_runtime* runtime : active_runtimes_.runtimes) {
            const s_registered_service* candidate =
                Find_Service(runtime, "galaxy_generator");
            if (candidate == nullptr) {
                continue;
            }
            if (provider != nullptr) {
                *diagnostic =
                    "multiple active galaxy_generator providers";
                return false;
            }
            provider = runtime;
            service = candidate;
        }
        if (provider == nullptr) {
            return true;
        }
        if (provider->invocation_active) {
            *diagnostic = provider->package_id +
                ": galaxy_generator service would reenter package";
            return false;
        }
        Restore_Storage_Global(provider);
        Restore_Game_Global(provider);
        provider->invocation_active = true;
        s_lua_result result;
        bool succeeded = false;
        {
            game::s_candidate_ship_world_scope ship_world;
            s_event_invocation_scope scope(LUA_GAME_STATE_GALAXY_MUTABLE);
            succeeded = Invoke_Runtime_Function(provider->state,
                service->reference,
                nullptr,
                nullptr,
                &result);
        }
        provider->invocation_active = false;
        *invoked = true;
        if (!succeeded) {
            Restore_Storage_Global(provider);
            Restore_Game_Global(provider);
            *diagnostic = provider->package_id +
                ": galaxy_generator service failed: " + result.diagnostic;
            return false;
        }
        if (!Storage_Global_Unchanged(provider)) {
            Restore_Storage_Global(provider);
            Restore_Game_Global(provider);
            *diagnostic = provider->package_id +
                ": galaxy_generator service replaced the engine-owned storage root";
            return false;
        }
        if (!Game_Global_Unchanged(provider)) {
            Restore_Game_Global(provider);
            *diagnostic = provider->package_id +
                ": galaxy_generator service replaced the engine-owned game root";
            return false;
        }
        return true;
    }

    void Destroy_Mod_Runtime_Set(s_lua_mod_runtime_set* runtime_set) {
        if (runtime_set != nullptr) {
            for (s_lua_mod_runtime* runtime : runtime_set->runtimes) {
                Destroy_Mod_Runtime(runtime);
            }
            Destroy_Game_Traits(runtime_set->game_traits);
            delete runtime_set;
        }
    }

    void Commit_Mod_Runtimes(s_lua_mod_runtime_set** candidate) {
        if (candidate != nullptr && *candidate != nullptr) {
            game::Restore_Ship_Generation_State(
                (*candidate)->ship_generations, MAX_SHIPS);
            for (s_lua_mod_runtime* runtime : active_runtimes_.runtimes) {
                Destroy_Mod_Runtime(runtime);
            }
            Destroy_Game_Traits(active_runtimes_.game_traits);
            active_runtimes_.runtimes = std::move((*candidate)->runtimes);
            active_runtimes_.game_traits = (*candidate)->game_traits;
            memcpy(active_runtimes_.ship_generations,
                (*candidate)->ship_generations,
                sizeof(active_runtimes_.ship_generations));
            (*candidate)->game_traits = nullptr;
            for (s_lua_mod_runtime* runtime : active_runtimes_.runtimes) {
                runtime->runtime_set = &active_runtimes_;
            }
            delete *candidate;
            *candidate = nullptr;
        }
    }

    uint64_t* Get_Mod_Runtime_Ship_Generations(
        s_lua_mod_runtime_set* runtime_set) {
        return runtime_set == nullptr ? nullptr : runtime_set->ship_generations;
    }

    const s_lua_mod_runtime_set* Get_Active_Mod_Runtimes() {
        return &active_runtimes_;
    }

    size_t Get_Mod_Runtime_Count(const s_lua_mod_runtime_set* runtime_set) {
        return runtime_set == nullptr ? 0 : runtime_set->runtimes.size();
    }

    const s_lua_mod_runtime* Get_Mod_Runtime(
        const s_lua_mod_runtime_set* runtime_set, size_t index) {
        if (runtime_set == nullptr || index >= runtime_set->runtimes.size()) {
            return nullptr;
        }
        return runtime_set->runtimes[index];
    }

    s_lua_mod_runtime* Get_Mod_Runtime(
        s_lua_mod_runtime_set* runtime_set, size_t index) {
        if (runtime_set == nullptr || index >= runtime_set->runtimes.size()) {
            return nullptr;
        }
        return runtime_set->runtimes[index];
    }

    s_lua_mod_runtime* Find_Mod_Runtime(
        s_lua_mod_runtime_set* runtime_set, const char* package_id) {
        return package_id == nullptr
            ? nullptr
            : Find_Runtime(runtime_set, package_id);
    }

    const s_lua_mod_runtime* Find_Mod_Runtime(
        const s_lua_mod_runtime_set* runtime_set, const char* package_id) {
        if (runtime_set == nullptr || package_id == nullptr) {
            return nullptr;
        }
        for (const s_lua_mod_runtime* runtime : runtime_set->runtimes) {
            if (runtime->package_id == package_id) {
                return runtime;
            }
        }
        return nullptr;
    }

    const char* Get_Mod_Runtime_Package_Id(const s_lua_mod_runtime* runtime) {
        return runtime == nullptr ? nullptr : runtime->package_id.c_str();
    }

    const char* Get_Mod_Runtime_Package_Version(
        const s_lua_mod_runtime* runtime) {
        return runtime == nullptr ? nullptr : runtime->package_version.c_str();
    }

    uint32_t Get_Mod_Runtime_Api(const s_lua_mod_runtime* runtime) {
        return runtime == nullptr ? 0 : runtime->api;
    }

    uint32_t Get_Mod_Runtime_Storage_Version(
        const s_lua_mod_runtime* runtime) {
        return runtime == nullptr ? 0 : runtime->storage_version;
    }

    bool Check_Network_Mod_Runtime_Compatibility(
        const s_lua_mod_runtime_set* runtime_set,
        std::string* diagnostic) {
        if (diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        if (runtime_set == nullptr) {
            return false;
        }
        for (const s_lua_mod_runtime* runtime : runtime_set->runtimes) {
            for (const s_registered_service& service : runtime->services) {
                if (service.info.name != "galaxy_generator" ||
                    service.info.version != 1) {
                    *diagnostic = "Lua package '" + runtime->package_id +
                        "' registers network-disabled service '" +
                        service.info.name + "'";
                    return false;
                }
            }
            for (const s_registered_callback& callback : runtime->callbacks) {
                const bool allowed = callback.info.kind == LUA_CALLBACK_EVENT
                    ? Network_Event_Allowed(callback.info.extension_name)
                    : Network_Rule_Allowed(callback.info.extension_name);
                if (!allowed) {
                    *diagnostic = "Lua package '" + runtime->package_id +
                        "' registers network-disabled " +
                        (callback.info.kind == LUA_CALLBACK_RULE ? "rule '" : "event '") +
                        callback.info.extension_name + "'";
                    return false;
                }
            }
        }
        return true;
    }

    bool Serialize_Mod_Storage(s_lua_mod_runtime* runtime,
        std::vector<uint8_t>* bytes,
        std::string* diagnostic) {
        if (runtime == nullptr || bytes == nullptr || diagnostic == nullptr) {
            return false;
        }
        s_storage_serialization serialization;
        serialization.runtime = runtime;
        serialization.bytes = bytes;
        s_lua_result result;
        const bool serialized = Execute_Runtime_Operation(runtime->state,
            Serialize_Storage,
            &serialization,
            &result);
        *diagnostic = serialization.diagnostic.empty()
            ? result.diagnostic
            : serialization.diagnostic;
        if (!serialized && !diagnostic->empty()) {
            *diagnostic = runtime->package_id + ": " + *diagnostic;
        }
        return serialized;
    }

    bool Restore_Mod_Storage(s_lua_mod_runtime* runtime,
        uint32_t saved_version,
        const uint8_t* bytes,
        size_t size,
        std::string* diagnostic) {
        if (runtime == nullptr || bytes == nullptr || diagnostic == nullptr ||
            saved_version == 0) {
            return false;
        }
        diagnostic->clear();
        if (runtime->runtime_set == &active_runtimes_) {
            *diagnostic = runtime->package_id +
                ": storage can be restored only into a staged Lua runtime";
            return false;
        }
        if (saved_version > runtime->storage_version) {
            *diagnostic = runtime->package_id +
                ": saved Lua storage version is newer than the active mod";
            return false;
        }

        lua_State* lua_state = Get_Runtime_State_Handle(runtime->state);
        int candidate_reference = LUA_NOREF;
        if (!Deserialize_Storage_Reference(runtime, bytes, size, &candidate_reference, diagnostic)) {
            *diagnostic = runtime->package_id + ": " + *diagnostic;
            return false;
        }
        const int prior_reference = runtime->storage_reference;
        runtime->storage_reference = candidate_reference;
        Restore_Storage_Global(runtime);

        uint32_t version = saved_version;
        while (version != runtime->storage_version) {
            const s_runtime_migration* migration =
                Find_Migration(runtime, version);
            if (migration == nullptr) {
                *diagnostic = runtime->package_id +
                    ": no migration path from Lua storage version " +
                    std::to_string(version);
                return false;
            }
            s_lua_result result;
            s_migration_context context = {
                migration->from,
                migration->to,
                true,
            };
            if (!Execute_Runtime_Operation(runtime->state,
                    Set_Migration_Context,
                    &context,
                    &result)) {
                *diagnostic = runtime->package_id +
                    ": unable to install Lua migration context: " +
                    result.diagnostic;
                return false;
            }
            if (!Execute_Runtime_Source(runtime->state,
                    migration->source.data(),
                    migration->source.size(),
                    migration->path.c_str(),
                    &result) ||
                !Storage_Global_Unchanged(runtime)) {
                context.install = false;
                s_lua_result ignored;
                Execute_Runtime_Operation(runtime->state,
                    Set_Migration_Context,
                    &context,
                    &ignored);
                *diagnostic = runtime->package_id +
                    ": Lua storage migration failed: " +
                    (result.diagnostic.empty()
                            ? "migration replaced the engine-owned storage root"
                            : result.diagnostic);
                return false;
            }
            context.install = false;
            if (!Execute_Runtime_Operation(runtime->state,
                    Set_Migration_Context,
                    &context,
                    &result)) {
                *diagnostic = runtime->package_id +
                    ": unable to clear Lua migration context: " +
                    result.diagnostic;
                return false;
            }
            version = migration->to;
        }

        std::vector<uint8_t> validated;
        if (!Serialize_Mod_Storage(runtime, &validated, diagnostic)) {
            return false;
        }
        luaL_unref(lua_state, LUA_REGISTRYINDEX, prior_reference);
        return true;
    }

    bool Prepare_Active_Mod_Storage_Restore(s_lua_mod_runtime* runtime,
        uint32_t saved_version,
        const uint8_t* bytes,
        size_t size,
        s_lua_storage_restore_transaction** transaction,
        std::string* diagnostic) {
        if (runtime == nullptr || bytes == nullptr || transaction == nullptr ||
            diagnostic == nullptr || saved_version == 0) {
            return false;
        }
        *transaction = nullptr;
        diagnostic->clear();
        if (runtime->runtime_set != &active_runtimes_) {
            *diagnostic = runtime->package_id +
                ": network storage can be restored only into an active Lua runtime";
            return false;
        }
        if (saved_version != runtime->storage_version) {
            *diagnostic = runtime->package_id +
                ": network Lua storage schema differs from the active mod";
            return false;
        }
        auto* prepared = new (std::nothrow) s_lua_storage_restore_transaction;
        if (prepared == nullptr) {
            *diagnostic = runtime->package_id +
                ": unable to allocate network Lua storage transaction";
            return false;
        }
        prepared->runtime = runtime;
        prepared->prior_reference = runtime->storage_reference;
        if (!Deserialize_Storage_Reference(runtime,
                bytes,
                size,
                &prepared->candidate_reference,
                diagnostic)) {
            *diagnostic = runtime->package_id + ": " + *diagnostic;
            delete prepared;
            return false;
        }

        runtime->storage_reference = prepared->candidate_reference;
        Restore_Storage_Global(runtime);
        std::vector<uint8_t> canonical;
        const bool valid = Serialize_Mod_Storage(runtime, &canonical, diagnostic) &&
            canonical.size() == size &&
            std::equal(canonical.begin(), canonical.end(), bytes);
        runtime->storage_reference = prepared->prior_reference;
        Restore_Storage_Global(runtime);
        if (!valid) {
            lua_State* lua_state = Get_Runtime_State_Handle(runtime->state);
            luaL_unref(lua_state,
                LUA_REGISTRYINDEX,
                prepared->candidate_reference);
            if (diagnostic->empty()) {
                *diagnostic = runtime->package_id +
                    ": noncanonical network Lua storage";
            }
            delete prepared;
            return false;
        }
        *transaction = prepared;
        return true;
    }

    void Commit_Active_Mod_Storage_Restore(
        s_lua_storage_restore_transaction** transaction) {
        if (transaction == nullptr || *transaction == nullptr) {
            return;
        }
        s_lua_storage_restore_transaction* prepared = *transaction;
        s_lua_mod_runtime* runtime = prepared->runtime;
        lua_State* lua_state = Get_Runtime_State_Handle(runtime->state);
        runtime->storage_reference = prepared->candidate_reference;
        Restore_Storage_Global(runtime);
        luaL_unref(lua_state, LUA_REGISTRYINDEX, prepared->prior_reference);
        delete prepared;
        *transaction = nullptr;
    }

    void Discard_Active_Mod_Storage_Restore(
        s_lua_storage_restore_transaction** transaction) {
        if (transaction == nullptr || *transaction == nullptr) {
            return;
        }
        s_lua_storage_restore_transaction* prepared = *transaction;
        lua_State* lua_state =
            Get_Runtime_State_Handle(prepared->runtime->state);
        luaL_unref(lua_state,
            LUA_REGISTRYINDEX,
            prepared->candidate_reference);
        delete prepared;
        *transaction = nullptr;
    }

    bool Dispatch_Active_Initialized_Event(std::string* diagnostic) {
        if (diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        for (s_lua_mod_runtime* runtime : active_runtimes_.runtimes) {
            s_lua_result result;
            if (!Dispatch_Mod_Event(runtime,
                    "game.initialized",
                    Build_Loaded_Event,
                    nullptr,
                    &result)) {
                *diagnostic = runtime->package_id +
                    ": game.initialized callback failed: " + result.diagnostic;
                return false;
            }
            std::vector<uint8_t> validated_storage;
            if (!Serialize_Mod_Storage(
                    runtime, &validated_storage, diagnostic)) {
                return false;
            }
        }
        return true;
    }

    bool Dispatch_Active_Loaded_Event(std::string* diagnostic) {
        if (diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        return Dispatch_Loaded_Event_Impl(&active_runtimes_, diagnostic);
    }

    bool Dispatch_Configuration_Changed_Event(
        s_lua_mod_runtime_set* runtime_set,
        const std::vector<s_lua_configuration_change>* changes,
        std::string* diagnostic) {
        if (runtime_set == nullptr || changes == nullptr ||
            diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        if (runtime_set == &active_runtimes_) {
            *diagnostic =
                "game.configuration_changed can be dispatched only to staged Lua runtimes";
            return false;
        }
        if (changes->empty()) {
            return true;
        }
        s_configuration_changed_context context;
        context.reason = "package_version_changed";
        context.changes = changes;
        for (s_lua_mod_runtime* runtime : runtime_set->runtimes) {
            s_lua_result result;
            if (!Dispatch_Mod_Event(runtime,
                    "game.configuration_changed",
                    Build_Configuration_Changed_Event,
                    &context,
                    &result)) {
                *diagnostic = runtime->package_id +
                    ": game.configuration_changed callback failed: " +
                    result.diagnostic;
                return false;
            }
            std::vector<uint8_t> validated_storage;
            if (!Serialize_Mod_Storage(
                    runtime, &validated_storage, diagnostic)) {
                return false;
            }
        }
        return true;
    }

    bool Dispatch_Active_Configuration_Reloaded_Event(
        const std::vector<std::string>* changed_groups,
        std::string* diagnostic) {
        if (changed_groups == nullptr || diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        if (changed_groups->empty()) {
            return true;
        }
        s_configuration_changed_context context;
        context.reason = "configuration_reloaded";
        context.changed_groups = changed_groups;
        for (s_lua_mod_runtime* runtime : active_runtimes_.runtimes) {
            s_lua_result result;
            if (!Dispatch_Mod_Event(runtime,
                    "game.configuration_changed",
                    Build_Configuration_Changed_Event,
                    &context,
                    &result)) {
                *diagnostic = runtime->package_id +
                    ": game.configuration_changed callback failed after configuration reload: " +
                    result.diagnostic;
                return false;
            }
            std::vector<uint8_t> validated_storage;
            if (!Serialize_Mod_Storage(
                    runtime, &validated_storage, diagnostic)) {
                return false;
            }
        }
        return true;
    }

    bool Dispatch_Loaded_Event(s_lua_mod_runtime_set* runtime_set,
        std::string* diagnostic) {
        if (runtime_set == nullptr || diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        if (runtime_set == &active_runtimes_) {
            *diagnostic =
                "game.loaded can be dispatched only to staged Lua runtimes";
            return false;
        }
        return Dispatch_Loaded_Event_Impl(runtime_set, diagnostic);
    }

    s_lua_game_traits* Get_Active_Game_Traits() {
        return active_runtimes_.game_traits;
    }

    s_lua_game_traits* Get_Game_Traits(
        s_lua_mod_runtime_set* runtime_set) {
        return runtime_set == nullptr ? nullptr : runtime_set->game_traits;
    }

    void Reset_Mod_Runtimes() {
        for (s_lua_mod_runtime* runtime : active_runtimes_.runtimes) {
            Destroy_Mod_Runtime(runtime);
        }
        active_runtimes_.runtimes.clear();
        Destroy_Game_Traits(active_runtimes_.game_traits);
        active_runtimes_.game_traits = nullptr;
    }

    void Shutdown_Lua() {
        Reset_Mod_Runtimes();
        Reset_Registry();
    }

} // namespace orion2re::lua