#include "pch.h"

#include "lua/lua_game_ship_identity.h"
#include "game/game_events.h"

namespace orion2re::lua::game {
    namespace {

        uint64_t ship_generations_[MAX_SHIPS];
        thread_local uint64_t* scoped_ship_generations_ = nullptr;

        uint64_t* Ship_Generations() {
            return scoped_ship_generations_ == nullptr
                ? ship_generations_
                : scoped_ship_generations_;
        }

    } // namespace

    s_ship_generation_scope::s_ship_generation_scope(uint64_t* generations)
        : previous_(scoped_ship_generations_) {
        scoped_ship_generations_ = generations;
    }

    s_ship_generation_scope::~s_ship_generation_scope() {
        scoped_ship_generations_ = previous_;
    }

    void Reset_Ship_Generations() {
        if (scoped_ship_generations_ == nullptr) game_events::Publish(game_events::SHIP_IDENTITIES_RESETTING);
        memset(Ship_Generations(), 0, sizeof(ship_generations_));
    }

    bool Advance_Ship_Generation(int16_t ship_id) {
        if (ship_id < 0 || ship_id >= MAX_SHIPS ||
            Ship_Generations()[ship_id] == UINT64_MAX) {
            return false;
        }
        if (scoped_ship_generations_ == nullptr) game_events::Publish_Ship_Identity_Changing(ship_id);
        ++Ship_Generations()[ship_id];
        return true;
    }

    uint64_t Ship_Generation(int16_t ship_id) {
        return ship_id < 0 || ship_id >= MAX_SHIPS
            ? 0
            : Ship_Generations()[ship_id];
    }

    size_t Ship_Generation_Count() {
        return MOX::NUM_SHIPS < 0 || MOX::NUM_SHIPS > MAX_SHIPS
            ? 0
            : static_cast<size_t>(MOX::NUM_SHIPS);
    }

    bool Capture_Ship_Generations(
        uint64_t* generations, size_t capacity, size_t* count) {
        if (generations == nullptr || count == nullptr ||
            Ship_Generation_Count() > capacity) {
            return false;
        }
        *count = Ship_Generation_Count();
        memcpy(generations,
            Ship_Generations(),
            *count * sizeof(ship_generations_[0]));
        return true;
    }

    bool Restore_Ship_Generations(
        const uint64_t* generations, size_t count) {
        if ((count != 0 && generations == nullptr) || count > MAX_SHIPS ||
            count != Ship_Generation_Count()) {
            return false;
        }
        Reset_Ship_Generations();
        memcpy(Ship_Generations(),
            generations,
            count * sizeof(ship_generations_[0]));
        return true;
    }

    bool Capture_Ship_Generation_State(
        uint64_t* generations, size_t count) {
        if (generations == nullptr || count != MAX_SHIPS) {
            return false;
        }
        memcpy(generations, Ship_Generations(), sizeof(ship_generations_));
        return true;
    }

    bool Restore_Ship_Generation_State(
        const uint64_t* generations, size_t count) {
        if (generations == nullptr || count != MAX_SHIPS) {
            return false;
        }
        if (scoped_ship_generations_ == nullptr) game_events::Publish(game_events::SHIP_IDENTITIES_RESETTING);
        memcpy(Ship_Generations(), generations, sizeof(ship_generations_));
        return true;
    }

    void Restore_Legacy_Ship_Generations() {
        Reset_Ship_Generations();
        for (size_t index = 0; index < Ship_Generation_Count(); ++index) {
            Ship_Generations()[index] = 1;
        }
    }

} // namespace orion2re::lua::game