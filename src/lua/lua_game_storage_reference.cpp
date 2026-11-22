#include "lua/lua_game_internal.h"

#include "game/consts.h"
#include "lua/lua_game_storage_reference.h"

namespace orion2re::lua::game::internal {
    namespace {

        constexpr uint8_t REFERENCE_PLAYER = 1;
        constexpr uint8_t REFERENCE_SHIP = 2;
        constexpr uint8_t REFERENCE_SHIP_KEY = 3;

        e_lua_storage_reference_result Read_Reference(lua_State* lua_state,
            int index,
            s_lua_storage_reference* reference,
            std::string* diagnostic) {
            if (Read_Player_Storage_Reference(lua_state, index, reference)) {
                reference->kind = REFERENCE_PLAYER;
                return LUA_STORAGE_REFERENCE_OK;
            }
            if (Read_Ship_Storage_Reference(lua_state, index, reference)) {
                reference->kind = reference->generation == 0 ? REFERENCE_SHIP_KEY : REFERENCE_SHIP;
                return LUA_STORAGE_REFERENCE_OK;
            }
            return LUA_STORAGE_REFERENCE_UNSUPPORTED;
        }

        bool Write_Reference(lua_State* lua_state,
            const s_lua_storage_reference* reference,
            std::string* diagnostic) {
            if (reference->kind == REFERENCE_PLAYER &&
                reference->id >= 0 && reference->id < MAX_PLAYERS &&
                reference->generation == 0) {
                Push_Stored_Player(lua_state,
                    static_cast<int16_t>(reference->id));
                return true;
            }
            if (reference->kind == REFERENCE_SHIP &&
                reference->id >= 0 && reference->id < MAX_SHIPS &&
                reference->generation != 0) {
                Push_Stored_Ship(lua_state,
                    static_cast<int16_t>(reference->id),
                    reference->generation);
                return true;
            }
            if (reference->kind == REFERENCE_SHIP_KEY && reference->generation == 0 &&
                reference->id >= 0 && reference->id < UINT32_MAX) {
                Push_Stored_Ship(lua_state, id_ship{static_cast<uint32_t>(reference->id)});
                return true;
            }
            *diagnostic = "invalid Lua storage engine reference";
            return false;
        }

        const s_lua_storage_reference_adapter adapter_ = {
            Read_Reference,
            Write_Reference,
        };

    } // namespace

} // namespace orion2re::lua::game::internal

namespace orion2re::lua {

    const s_lua_storage_reference_adapter*
    Get_Game_Storage_Reference_Adapter() {
        using namespace game::internal;
        return &adapter_;
    }

} // namespace orion2re::lua