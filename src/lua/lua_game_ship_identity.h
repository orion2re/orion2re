#ifndef ORION2RE_LUA_GAME_SHIP_IDENTITY_H
#define ORION2RE_LUA_GAME_SHIP_IDENTITY_H

#include <cstddef>
#include <cstdint>

namespace orion2re::lua::game {

    // An unpublished galaxy draft has its own handle lifetime, even if its keys
    // are copied from the live roster.
    class s_candidate_ship_world_scope {
      public:
        s_candidate_ship_world_scope();
        ~s_candidate_ship_world_scope();

        s_candidate_ship_world_scope(const s_candidate_ship_world_scope&) = delete;
        s_candidate_ship_world_scope& operator=(const s_candidate_ship_world_scope&) = delete;

      private:
        uint64_t previous_;
    };

    void Advance_Ship_World();
    uint64_t Ship_World();
    void Restore_Ship_World(uint64_t world);

    class s_ship_generation_scope {
      public:
        explicit s_ship_generation_scope(uint64_t* generations);
        ~s_ship_generation_scope();

        s_ship_generation_scope(const s_ship_generation_scope&) = delete;
        s_ship_generation_scope& operator=(const s_ship_generation_scope&) = delete;

      private:
        uint64_t* previous_;
    };

    void Reset_Ship_Generations();
    bool Advance_Ship_Generation(int16_t ship_id);
    uint64_t Ship_Generation(int16_t ship_id);
    size_t Ship_Generation_Count();
    bool Capture_Ship_Generations(
        uint64_t* generations, size_t capacity, size_t* count);
    bool Restore_Ship_Generations(
        const uint64_t* generations, size_t count);
    bool Capture_Ship_Generation_State(
        uint64_t* generations, size_t count);
    bool Restore_Ship_Generation_State(
        const uint64_t* generations, size_t count);
    void Restore_Legacy_Ship_Generations();

} // namespace orion2re::lua::game

#endif