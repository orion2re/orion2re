#include "pch.h"

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
