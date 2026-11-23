#ifndef ORION2RE_LUA_GAME_SETUP_H
#define ORION2RE_LUA_GAME_SETUP_H

#include <cstdint>

namespace orion2re::lua::game {
    // Only the network host constructs a new game. Guests install its snapshot.
    class s_network_new_game_host_scope {
    public:
        s_network_new_game_host_scope() { active_ = true; }
        ~s_network_new_game_host_scope() { active_ = previous_; }

        s_network_new_game_host_scope(const s_network_new_game_host_scope&) = delete;
        s_network_new_game_host_scope& operator=(const s_network_new_game_host_scope&) = delete;

        static bool Allows_Setup(std::int8_t game_type) {
            return active_ || (game_type != 2 && game_type != 3);
        }

    private:
        static inline thread_local bool active_ = false;
        const bool previous_ = active_;
    };
} // namespace orion2re::lua::game

#endif
