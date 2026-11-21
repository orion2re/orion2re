#pragma once

namespace debug_game_editors {
    enum e_planet_operation_result {
        PLANET_OPERATION_OK,
        PLANET_OPERATION_INVALID_RELATION,
        PLANET_OPERATION_NO_FREE_ORBIT,
        PLANET_OPERATION_NO_CAPACITY,
        PLANET_OPERATION_NOT_DENSE_TAIL,
        PLANET_OPERATION_HAS_COLONY,
        PLANET_OPERATION_IS_REFERENCED,
        PLANET_OPERATION_NO_COLONY_CAPACITY,
    };

    e_planet_operation_result Add_Planet(int16_t star_index, int16_t orbit, game::id_planet* new_planet_index);
    e_planet_operation_result Duplicate_Planet(
        game::id_planet source_planet_index,
        int16_t target_star_index,
        int16_t orbit,
        game::id_planet* new_planet_index);
    e_planet_operation_result Delete_Planet(game::id_planet planet_index);
    const char* Planet_Operation_Message(e_planet_operation_result result);

    void Open_Star(int16_t star_index);
    void Open_Planet(game::id_planet planet_index);
    void Open_Ship(int16_t ship_index);
    void Open_Colony(game::id_colony colony_index);
    void Open_Player(int16_t player_index);
    void Open_Antaran();
} // namespace debug_game_editors
