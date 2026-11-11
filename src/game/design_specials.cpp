#include "pch.h"

namespace DESBOX {
    int16_t __cdecl Special_System_Exclusion(int16_t special_idx, int16_t* out_conflict_id) {
        int16_t found = 0;
        *out_conflict_id = 0;

        for (int16_t i = 0; i < 6; ++i) {
            // Check if conflict already found in previous iterations
            if (found != 0) {
                break;
            }

            int16_t exclude_id = (int16_t)TECHDATA::specials[special_idx].exclude[i];
            if (exclude_id > 0) {
                for (int16_t j = 0; j < 8; ++j) {
                    if (found != 0) {
                        break;
                    }

                    int16_t current_special = MOX::design->special_devices[j];
                    if (exclude_id == current_special) {
                        // A conflict is only valid if the conflicting special is not the one
                        // currently being replaced/selected in the design screen
                        if (MOX::design->special_devices[MOX::design_screen_replacement_slot] != current_special) {
                            found = 1;
                            *out_conflict_id = current_special;
                        }
                    }
                }
            }
        }

        return found;
    }
} // namespace DESBOX

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
