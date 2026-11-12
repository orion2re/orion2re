#include "pch.h"

namespace aidesign {
    int16_t __cdecl Special_Legal_For_Design(s_ship_design* design, int16_t tech_id) {
        int16_t result = 1;
        int16_t bit = 0;

        while (bit < 39 && result == 1) {
            if (struct_::Test_Bit_Field(design->special_device_flags, bit)) {
                for (int i = 0; i < 6; ++i) {
                    if (TECHDATA::specials[bit].exclude[i] == tech_id) {
                        result = 0;
                    }
                }
            }
            bit++;
        }

        if (design->shield_type == 0) {
            if (tech_id == SPECIAL_HARD_SHIELDS || tech_id == SPECIAL_MULTI_PHASED_SHIELDS || tech_id == SPECIAL_SHIELD_CAPACITOR) {
                result = 0;
            }
        }

        return result;
    }
} // namespace aidesign
