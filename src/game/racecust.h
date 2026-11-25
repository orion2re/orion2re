#pragma once

struct s_player;

namespace RACECUST {
    constexpr int AI_RACE_VARIANT_COUNT = 5;
    constexpr int AI_RACE_VARIANT_LIST_SIZE = AI_RACE_VARIANT_COUNT + 1;
    constexpr int AI_RACE_VARIANT_DIFFICULTY_COUNT = 2;

    extern uint8_t ai_race_variants[AI_RACE_VARIANT_LIST_SIZE];

    struct s_variants {
        s_racecust_race_customize
            customization[STOCK_RACE_COUNT][AI_RACE_VARIANT_DIFFICULTY_COUNT]
                         [AI_RACE_VARIANT_COUNT];
        s_racecust_race_evolution
            evolution[STOCK_RACE_COUNT][AI_RACE_VARIANT_DIFFICULTY_COUNT]
                     [AI_RACE_VARIANT_COUNT];
    };

    extern s_variants variants;

    /**
     * Applies the difficulty-selected race customization and evolutionary trait overrides for one player.
     * @param player Player whose trait arrays are updated.
     */
    void __cdecl Race_Customization(s_player& player);
} // namespace RACECUST