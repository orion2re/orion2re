#include "lua/lua_game_traits.h"

#include <algorithm>
#include <climits>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#include "game/consts.h"
#include "game/sha256.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace orion2re::lua {
    namespace {

        struct s_owned_race_trait_prototype {
            std::string id;
            std::int16_t cost = 0;
        };

        bool Valid_Id(std::string_view id) {
            if (id.empty() ||
                ((id.front() < 'a' || id.front() > 'z') &&
                    id.front() != '_')) {
                return false;
            }
            for (const char value : id) {
                if ((value < 'a' || value > 'z') &&
                    (value < '0' || value > '9') && value != '_') {
                    return false;
                }
            }
            return true;
        }

        void Append_U32(std::string* output, std::uint32_t value) {
            for (int shift = 24; shift >= 0; shift -= 8) {
                output->push_back(static_cast<char>(value >> shift));
            }
        }

    } // namespace

    struct s_lua_game_traits {
        std::vector<s_owned_race_trait_prototype> race_traits;
        std::vector<s_lua_player_race_trait_assignment> player_race_traits;
        bool sealed = false;
    };

    struct s_lua_game_traits_assignment_transaction {
        s_lua_game_traits* traits = nullptr;
        std::vector<s_lua_player_race_trait_assignment> replacement;
    };

    namespace {

        const s_owned_race_trait_prototype* Find_Race_Trait(
            const s_lua_game_traits* traits,
            std::string_view id) {
            if (traits == nullptr) {
                return nullptr;
            }
            for (const s_owned_race_trait_prototype& prototype :
                traits->race_traits) {
                if (prototype.id == id) {
                    return &prototype;
                }
            }
            return nullptr;
        }

        int Add_Race_Trait(lua_State* lua_state) {
            auto* traits = static_cast<s_lua_game_traits*>(
                lua_touserdata(lua_state, lua_upvalueindex(1)));
            if (traits == nullptr || traits->sealed) {
                return luaL_error(lua_state,
                    "race trait prototypes are sealed");
            }
            const int prototype_index = lua_gettop(lua_state) >= 2 ? 2 : 1;
            luaL_checktype(lua_state, prototype_index, LUA_TTABLE);
            if (lua_getmetatable(lua_state, prototype_index) != 0) {
                lua_pop(lua_state, 1);
                return luaL_error(lua_state,
                    "race trait prototype must not have a metatable");
            }

            lua_getfield(lua_state, prototype_index, "id");
            std::size_t id_size = 0;
            const char* id_data = luaL_checklstring(lua_state, -1, &id_size);
            const std::string id(id_data, id_size);
            lua_pop(lua_state, 1);
            if (!Valid_Id(id)) {
                return luaL_error(lua_state,
                    "invalid race trait prototype ID '%s'",
                    id.c_str());
            }
            if (Find_Race_Trait(traits, id) != nullptr) {
                return luaL_error(lua_state,
                    "duplicate race trait prototype '%s'",
                    id.c_str());
            }

            lua_getfield(lua_state, prototype_index, "cost");
            const lua_Integer cost = luaL_checkinteger(lua_state, -1);
            lua_pop(lua_state, 1);
            if (cost < INT16_MIN || cost > INT16_MAX) {
                return luaL_error(lua_state,
                    "race trait prototype cost is outside the signed 16-bit range");
            }

            try {
                traits->race_traits.push_back(
                    {id, static_cast<std::int16_t>(cost)});
            } catch (...) {
                return luaL_error(lua_state,
                    "unable to allocate race trait prototype");
            }
            return 0;
        }

    } // namespace

    bool Create_Game_Traits(s_lua_game_traits** traits) {
        if (traits == nullptr) {
            return false;
        }
        *traits = new (std::nothrow) s_lua_game_traits;
        return *traits != nullptr;
    }

    void Destroy_Game_Traits(s_lua_game_traits* traits) {
        delete traits;
    }

    int Install_Game_Traits_Api(lua_State* lua_state, void* user_data) {
        auto* traits = static_cast<s_lua_game_traits*>(user_data);
        if (lua_state == nullptr || traits == nullptr) {
            return -1;
        }

        lua_newtable(lua_state);
        lua_newtable(lua_state);
        lua_pushlightuserdata(lua_state, traits);
        lua_pushcclosure(lua_state, Add_Race_Trait, 1);
        lua_setfield(lua_state, -2, "add");
        lua_setfield(lua_state, -2, "race_traits");
        lua_setglobal(lua_state, "prototypes");
        return 0;
    }

    void Seal_Race_Trait_Prototypes(s_lua_game_traits* traits) {
        if (traits != nullptr) {
            traits->sealed = true;
        }
    }

    std::size_t Get_Race_Trait_Prototype_Count(
        const s_lua_game_traits* traits) {
        return traits == nullptr ? 0 : traits->race_traits.size();
    }

    bool Get_Race_Trait_Prototype(
        const s_lua_game_traits* traits,
        std::size_t index,
        s_lua_race_trait_prototype* prototype) {
        if (traits == nullptr || prototype == nullptr ||
            index >= traits->race_traits.size()) {
            return false;
        }
        const s_owned_race_trait_prototype& owned =
            traits->race_traits[index];
        *prototype = {owned.id.c_str(), owned.cost};
        return true;
    }

    bool Find_Race_Trait_Prototype(
        const s_lua_game_traits* traits,
        const char* id,
        s_lua_race_trait_prototype* prototype) {
        if (id == nullptr) {
            return false;
        }
        const s_owned_race_trait_prototype* owned =
            Find_Race_Trait(traits, id);
        if (owned == nullptr) {
            return false;
        }
        if (prototype != nullptr) {
            *prototype = {owned->id.c_str(), owned->cost};
        }
        return true;
    }

    bool Set_Player_Race_Trait_Assignment(
        s_lua_game_traits* traits,
        ::game::id_player player_id,
        const char* trait_id,
        bool enabled) {
        if (traits == nullptr || player_id.id >= MAX_PLAYERS || trait_id == nullptr ||
            Find_Race_Trait(traits, trait_id) == nullptr) {
            return false;
        }
        const auto found = std::find_if(
            traits->player_race_traits.begin(),
            traits->player_race_traits.end(),
            [player_id, trait_id](const s_lua_player_race_trait_assignment& value) {
                return value.player_id == player_id &&
                    value.trait_id == trait_id;
            });
        try {
            if (enabled && found == traits->player_race_traits.end()) {
                traits->player_race_traits.push_back({player_id, trait_id});
            } else if (!enabled &&
                found != traits->player_race_traits.end()) {
                traits->player_race_traits.erase(found);
            }
        } catch (...) {
            return false;
        }
        return true;
    }

    bool Has_Player_Race_Trait_Assignment(
        const s_lua_game_traits* traits,
        ::game::id_player player_id,
        const char* trait_id) {
        if (traits == nullptr || trait_id == nullptr) {
            return false;
        }
        return std::find_if(traits->player_race_traits.begin(),
                   traits->player_race_traits.end(),
                   [player_id, trait_id](
                       const s_lua_player_race_trait_assignment& value) {
                       return value.player_id == player_id &&
                           value.trait_id == trait_id;
                   }) != traits->player_race_traits.end();
    }

    std::size_t Get_Player_Race_Trait_Assignment_Count(
        const s_lua_game_traits* traits) {
        return traits == nullptr ? 0 : traits->player_race_traits.size();
    }

    bool Get_Player_Race_Trait_Assignment(
        const s_lua_game_traits* traits,
        std::size_t index,
        s_lua_player_race_trait_assignment* assignment) {
        if (traits == nullptr || assignment == nullptr ||
            index >= traits->player_race_traits.size()) {
            return false;
        }
        *assignment = traits->player_race_traits[index];
        return true;
    }

    bool Replace_Player_Race_Trait_Assignments(
        s_lua_game_traits* traits,
        const std::vector<s_lua_player_race_trait_assignment>* assignments,
        std::string* diagnostic) {
        if (traits == nullptr || assignments == nullptr ||
            diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        std::vector<s_lua_player_race_trait_assignment> replacement;
        s_lua_game_traits_assignment_transaction* transaction = nullptr;
        if (!Prepare_Player_Race_Trait_Assignment_Replacement(
                traits, assignments, &transaction, diagnostic)) {
            return false;
        }
        Commit_Player_Race_Trait_Assignment_Replacement(&transaction);
        return true;
    }

    bool Prepare_Player_Race_Trait_Assignment_Replacement(
        s_lua_game_traits* traits,
        const std::vector<s_lua_player_race_trait_assignment>* assignments,
        s_lua_game_traits_assignment_transaction** transaction,
        std::string* diagnostic) {
        if (traits == nullptr || assignments == nullptr ||
            transaction == nullptr || diagnostic == nullptr) {
            return false;
        }
        *transaction = nullptr;
        diagnostic->clear();
        auto* prepared = new (std::nothrow)
            s_lua_game_traits_assignment_transaction;
        if (prepared == nullptr) {
            *diagnostic = "unable to allocate Lua race trait assignment transaction";
            return false;
        }
        prepared->traits = traits;
        try {
            prepared->replacement.reserve(assignments->size());
            for (const s_lua_player_race_trait_assignment& assignment :
                *assignments) {
                if (assignment.player_id.id >= MAX_PLAYERS) {
                    *diagnostic =
                        "Lua race trait assignment has an invalid player ID";
                    delete prepared;
                    return false;
                }
                if (Find_Race_Trait(traits, assignment.trait_id) == nullptr) {
                    *diagnostic = "Lua race trait assignment references unknown trait '" +
                        assignment.trait_id + "'";
                    delete prepared;
                    return false;
                }
                const auto duplicate = std::find_if(
                    prepared->replacement.begin(),
                    prepared->replacement.end(),
                    [&assignment](
                        const s_lua_player_race_trait_assignment& value) {
                        return value.player_id == assignment.player_id &&
                            value.trait_id == assignment.trait_id;
                    });
                if (duplicate != prepared->replacement.end()) {
                    *diagnostic = "duplicate Lua race trait assignment";
                    delete prepared;
                    return false;
                }
                prepared->replacement.push_back(assignment);
            }
        } catch (...) {
            delete prepared;
            *diagnostic = "unable to allocate Lua race trait assignments";
            return false;
        }
        *transaction = prepared;
        return true;
    }

    void Commit_Player_Race_Trait_Assignment_Replacement(
        s_lua_game_traits_assignment_transaction** transaction) {
        if (transaction != nullptr && *transaction != nullptr) {
            (*transaction)->traits->player_race_traits.swap((*transaction)->replacement);
            delete *transaction;
            *transaction = nullptr;
        }
    }

    void Discard_Player_Race_Trait_Assignment_Replacement(
        s_lua_game_traits_assignment_transaction** transaction) {
        if (transaction != nullptr && *transaction != nullptr) {
            delete *transaction;
            *transaction = nullptr;
        }
    }

    bool Build_Race_Trait_Prototype_Identity(
        const s_lua_game_traits* traits, std::string* identity) {
        if (traits == nullptr || identity == nullptr ||
            traits->race_traits.size() > UINT32_MAX) {
            return false;
        }
        std::vector<const s_owned_race_trait_prototype*> ordered;
        try {
            ordered.reserve(traits->race_traits.size());
            for (const s_owned_race_trait_prototype& prototype :
                traits->race_traits) {
                ordered.push_back(&prototype);
            }
            std::sort(ordered.begin(), ordered.end(), [](const s_owned_race_trait_prototype* left, const s_owned_race_trait_prototype* right) {
                return left->id < right->id;
            });
            std::string canonical = "O2LP";
            Append_U32(&canonical, 1);
            Append_U32(&canonical,
                static_cast<std::uint32_t>(ordered.size()));
            for (const s_owned_race_trait_prototype* prototype : ordered) {
                if (prototype->id.size() > UINT32_MAX) {
                    return false;
                }
                Append_U32(&canonical,
                    static_cast<std::uint32_t>(prototype->id.size()));
                canonical.append(prototype->id);
                canonical.push_back(static_cast<char>(
                    static_cast<std::uint16_t>(prototype->cost) >> 8));
                canonical.push_back(static_cast<char>(prototype->cost));
            }
            *identity = crypto::sha256_hex(canonical);
        } catch (...) {
            return false;
        }
        return true;
    }

} // namespace orion2re::lua
