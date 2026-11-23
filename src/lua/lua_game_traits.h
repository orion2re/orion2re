#ifndef ORION2RE_LUA_GAME_TRAITS_H
#define ORION2RE_LUA_GAME_TRAITS_H

#include <cstddef>
#include <cstdint>
#include "game/object_ids.h"
#include <string>
#include <vector>

struct lua_State;

namespace orion2re::lua {

    struct s_lua_game_traits;
    struct s_lua_game_traits_assignment_transaction;

    struct s_lua_race_trait_prototype {
        const char* id = nullptr;
        std::int16_t cost = 0;
    };

    struct s_lua_player_race_trait_assignment {
        ::game::id_player player_id = ::game::NO_PLAYER;
        std::string trait_id;
    };

    bool Create_Game_Traits(s_lua_game_traits** traits);
    void Destroy_Game_Traits(s_lua_game_traits* traits);
    int Install_Game_Traits_Api(lua_State* lua_state, void* user_data);
    void Seal_Race_Trait_Prototypes(s_lua_game_traits* traits);
    std::size_t Get_Race_Trait_Prototype_Count(
        const s_lua_game_traits* traits);
    bool Get_Race_Trait_Prototype(
        const s_lua_game_traits* traits,
        std::size_t index,
        s_lua_race_trait_prototype* prototype);
    bool Find_Race_Trait_Prototype(
        const s_lua_game_traits* traits,
        const char* id,
        s_lua_race_trait_prototype* prototype);
    bool Set_Player_Race_Trait_Assignment(
        s_lua_game_traits* traits,
        ::game::id_player player_id,
        const char* trait_id,
        bool enabled);
    bool Has_Player_Race_Trait_Assignment(
        const s_lua_game_traits* traits,
        ::game::id_player player_id,
        const char* trait_id);
    std::size_t Get_Player_Race_Trait_Assignment_Count(
        const s_lua_game_traits* traits);
    bool Get_Player_Race_Trait_Assignment(
        const s_lua_game_traits* traits,
        std::size_t index,
        s_lua_player_race_trait_assignment* assignment);
    bool Replace_Player_Race_Trait_Assignments(
        s_lua_game_traits* traits,
        const std::vector<s_lua_player_race_trait_assignment>* assignments,
        std::string* diagnostic);
    bool Prepare_Player_Race_Trait_Assignment_Replacement(
        s_lua_game_traits* traits,
        const std::vector<s_lua_player_race_trait_assignment>* assignments,
        s_lua_game_traits_assignment_transaction** transaction,
        std::string* diagnostic);
    void Commit_Player_Race_Trait_Assignment_Replacement(
        s_lua_game_traits_assignment_transaction** transaction);
    void Discard_Player_Race_Trait_Assignment_Replacement(
        s_lua_game_traits_assignment_transaction** transaction);
    bool Build_Race_Trait_Prototype_Identity(
        const s_lua_game_traits* traits, std::string* identity);

} // namespace orion2re::lua

#endif
