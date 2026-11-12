#include "lua/lua_network.h"
#include "lua/lua_mod_runtime.h"

namespace TEST_LUA_NETWORK {
    bool network_lua_enabled;
    bool queued_load_state;
    int prepared_payload_count;
    bool authority_allowed = true;
    int authority_calls;
    int32_t authority_host = -1;
    int32_t authority_local = -1;

    void Set_Network_Lua_Enabled_(bool enabled) {
        network_lua_enabled = enabled;
    }

    void Set_Queued_Load_State_(bool queued) {
        queued_load_state = queued;
    }

    int Prepared_Payload_Count_() {
        return prepared_payload_count;
    }
} // namespace TEST_LUA_NETWORK

namespace orion2re::lua {

    bool Network_Lua_Enabled() {
        return TEST_LUA_NETWORK::network_lua_enabled;
    }

    bool Capture_Lua_Network_Payload(
        void* buffer,
        uint32_t capacity,
        uint32_t* size,
        uint32_t* flags,
        std::string*) {
        if (!TEST_LUA_NETWORK::queued_load_state || buffer == nullptr || capacity == 0 ||
            size == nullptr || flags == nullptr) {
            return false;
        }
        *static_cast<uint8_t*>(buffer) = 0xA5;
        *size = 1;
        *flags = 1;
        TEST_LUA_NETWORK::queued_load_state = false;
        return true;
    }

    bool Has_Queued_Lua_Network_Load_State() {
        return TEST_LUA_NETWORK::queued_load_state;
    }

    bool Has_Pending_Lua_Network_Setup() {
        return false;
    }

    void Reset_Lua_Network_Session() {
        TEST_LUA_NETWORK::network_lua_enabled = false;
        TEST_LUA_NETWORK::queued_load_state = false;
        TEST_LUA_NETWORK::prepared_payload_count = 0;
        TEST_LUA_NETWORK::authority_allowed = true;
        TEST_LUA_NETWORK::authority_calls = 0;
        TEST_LUA_NETWORK::authority_host = -1;
        TEST_LUA_NETWORK::authority_local = -1;
    }

    bool Prepare_Lua_Gameplay_Host_Authority(int32_t host, int32_t local, std::string* diagnostic) {
        ++TEST_LUA_NETWORK::authority_calls;
        TEST_LUA_NETWORK::authority_host = host;
        TEST_LUA_NETWORK::authority_local = local;
        if (!TEST_LUA_NETWORK::authority_allowed && diagnostic != nullptr) {
            *diagnostic = "Lua authority rejected";
        }
        return TEST_LUA_NETWORK::authority_allowed;
    }

    bool Process_Lua_Network_Setup_Offer(const void*,
        uint32_t,
        e_lua_network_setup_kind,
        void*,
        uint32_t,
        uint32_t*) {
        return false;
    }

    bool Commit_Lua_Network_Setup(e_lua_network_setup_kind,
        const void*,
        uint32_t,
        uint32_t,
        std::string*) {
        return false;
    }

    bool Has_Lua_Network_Setup_Commit(
        const void*, uint32_t, uint32_t) {
        return false;
    }

    void Discard_Pending_Lua_Network_Setup() {}

    const s_lua_mod_runtime_set* Get_Active_Mod_Runtimes() {
        return nullptr;
    }

    bool Check_Network_Mod_Runtime_Compatibility(
        const s_lua_mod_runtime_set*, std::string*) {
        return false;
    }

    bool Prepare_Lua_Network_Payload(
        const void* buffer,
        uint32_t size,
        uint32_t flags,
        uint32_t,
        s_lua_network_state_transaction** transaction,
        bool* committed,
        std::string*) {
        if (buffer == nullptr || size != 1 || flags != 1 ||
            *static_cast<const uint8_t*>(buffer) != 0xA5 ||
            transaction == nullptr || committed == nullptr) {
            return false;
        }
        *transaction = nullptr;
        *committed = true;
        ++TEST_LUA_NETWORK::prepared_payload_count;
        return true;
    }

    void Commit_Lua_Network_State(
        s_lua_network_state_transaction**) {
    }

    void Discard_Lua_Network_State(
        s_lua_network_state_transaction**) {
    }

} // namespace orion2re::lua