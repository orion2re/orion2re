#pragma once

#include "colony_pop.h"

namespace POP {
    constexpr uint8_t RACE_ANDROID = 8;
    constexpr uint8_t RACE_NATIVE = 9;

    inline uint8_t Get_Race(const s_colony_pop& pop) { return pop.race; }
    inline uint32_t Get_Original_Owner(const s_colony_pop& pop) { return pop.original_owner.id; }
    inline e_prof Get_Prof(const s_colony_pop& pop) { return pop.profession; }
    inline void Set_Race(s_colony_pop* pop, uint32_t race) { pop->race = static_cast<uint8_t>(race); }
    inline void Set_Original_Owner(s_colony_pop* pop, uint32_t owner) {
        pop->original_owner = game::id_player{owner % 8};
    }
    inline void Set_Prof(s_colony_pop* pop, e_prof prof) { pop->profession = prof; }
    inline void Set_Assigned(s_colony_pop* pop, bool assigned) { pop->assigned = assigned; }
    inline void Set_Conquered(s_colony_pop* pop, bool conquered) { pop->conquered = conquered; }
    inline bool Is_Farmer(const s_colony_pop& pop) { return pop.profession == PROF_FARMER; }
    inline bool Is_Worker(const s_colony_pop& pop) { return pop.profession == PROF_WORKER; }
    inline bool Is_Scientist(const s_colony_pop& pop) { return pop.profession == PROF_SCIENTIST; }
    inline bool Is_Assigned(const s_colony_pop& pop) { return pop.assigned; }
    inline bool Is_Conquered(const s_colony_pop& pop) { return pop.conquered; }
    inline bool Is_Android(const s_colony_pop& pop) { return pop.race == RACE_ANDROID; }
    inline bool Is_Native(const s_colony_pop& pop) { return pop.race == RACE_NATIVE; }
    inline bool Is_Android_Or_Native(const s_colony_pop& pop) { return Is_Android(pop) || Is_Native(pop); }
    inline uint32_t Get_Effective_Player(const s_colony_pop& pop, uint32_t colony_owner) {
        return Is_Android_Or_Native(pop) ? colony_owner : pop.race;
    }
}
