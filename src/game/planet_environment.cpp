#include "pch.h"

namespace PLNTSUM {
    int16_t __cdecl Productivity_Penalty(int16_t player_idx, int16_t planet_gravity) {
        s_player* plr = &MOX::player[player_idx];

        int8_t low_g_trait = plr->traits[TRAIT_LOW_G_WORLD];
        int8_t heavy_g_trait = plr->traits[TRAIT_HEAVY_G_WORLD];

        // A race is considered "Normal Gravity" if it has neither Low-G nor Heavy-G traits.
        bool is_normal_g_race = (low_g_trait == 0 && heavy_g_trait == 0);

        // 50% Penalty Conditions:
        // - Low-G race on Heavy-G planet (2)
        // - Normal-G race on Heavy-G planet (2)
        if ((low_g_trait != 0 && planet_gravity == PLANET_GRAVITY_HEAVY_G) ||
            (is_normal_g_race && planet_gravity == PLANET_GRAVITY_HEAVY_G)) {
            return 50;
        }

        // 25% Penalty Conditions:
        // - Low-G race on Normal-G planet (1)
        // - Normal-G or Heavy-G race on Low-G planet (0)
        if ((low_g_trait != 0 && planet_gravity == PLANET_GRAVITY_NORMAL_G) ||
            (low_g_trait == 0 && planet_gravity == PLANET_GRAVITY_LOW_G)) {
            return 25;
        }

        return 0;
    }
} // namespace PLNTSUM

namespace COLCALC {
    bool __cdecl Biomorphic_Fungi_Applies(int16_t food2_per_farmer) {
        switch (fungi_climate) {
            case FUNGI_CLIMATE_REVERSED:
                return food2_per_farmer != 0;
            case FUNGI_CLIMATE_UNITED:
                return true;
            default:
                return food2_per_farmer == 0;
        }
    }

    int __cdecl Calc_Player_Climate(const s_player* player, int climate) {
        if (player != nullptr && player->traits[TRAIT_AQUATIC] != 0) {
            switch (climate) {
                case PLANET_CLIMATE_TUNDRA:
                    return aquatic_treats_tundra_as_swamp != 0
                        ? PLANET_CLIMATE_SWAMP
                        : PLANET_CLIMATE_TERRAN;
                case PLANET_CLIMATE_SWAMP:
                    return PLANET_CLIMATE_TERRAN;
                case PLANET_CLIMATE_OCEAN:
                case PLANET_CLIMATE_TERRAN:
                    return PLANET_CLIMATE_GAIA;
                default:
                    break;
            }
        }
        return climate;
    }
} // namespace COLCALC
