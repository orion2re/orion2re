#pragma once

#include "colony_pop.h"
#include "consts.h"

namespace POP {
    constexpr uint8_t RACE_ANDROID = 8;
    constexpr uint8_t RACE_NATIVE = 9;

    constexpr game::id_player Group_Player(const s_colony_pop& pop) {
        return pop.group < MAX_PLAYERS ? game::id_player{pop.group} : game::NO_PLAYER;
    }
    inline bool Is_Farmer(const s_colony_pop& pop) { return pop.profession == PROF_FARMER; }
    inline bool Is_Worker(const s_colony_pop& pop) { return pop.profession == PROF_WORKER; }
    inline bool Is_Scientist(const s_colony_pop& pop) { return pop.profession == PROF_SCIENTIST; }
    inline bool Is_Android(const s_colony_pop& pop) { return pop.group == RACE_ANDROID; }
    inline bool Is_Native(const s_colony_pop& pop) { return pop.group == RACE_NATIVE; }
    inline bool Is_Android_Or_Native(const s_colony_pop& pop) { return Is_Android(pop) || Is_Native(pop); }
    inline uint32_t Get_Effective_Player(const s_colony_pop& pop, uint32_t colony_owner) {
        return Is_Android_Or_Native(pop) ? colony_owner : pop.group;
    }
}
