#include "pch.h"
#include "string_overrides.h"

namespace strings {
    const char s__00551100[1] = "";
    const char s_plus_00551118[2] = "+";
    const char s_minus_0055111c[2] = "-";
    const char s_space_00551120[2] = {'\x1B', '\0'};
    const char s_B_005560e4[2] = "B";
    const char s_S_005569cc[2] = "S";
    const char s_O_00556afc[2] = "O";
    const char s_A_00557ed4[2] = "A";
    const char s_M_00557ed8[2] = "M";
    const char s_fmt_plus_d_00557ed0[4] = "+%d";
    const char s_fmt_plus_15bc_00558cd4[6] = "+15BC";
    const char s_fmt_plus_10bc_00558cdc[6] = "+10BC";
    const char s_fmt_plus_d_percent_00558ce4[6] = "+%d%%";
    const char s_fmt_minus_d_bc_00558cec[6] = "-%dBC";
    const char s_fmt_minus_d_percent_00558cf4[6] = "-%d%%";
    const char s_fmt_plus_d_percent_short_00558cfc[5] = "+%d%";
    const char s_ld_00558280[4] = "%ld";
    /* fmtpara: switches paragraph justification to left alignment. */
    const char s_0_0055110c[3] = "\x1A"
                                 "0";
    /* fmtpara: switches paragraph justification to right alignment. */
    const char s_1_00551110[3] = "\x1A"
                                 "1";
    const char s__0055ac34[2] = "\t";
    /* fmtpara: clears tab stops, then defines tabs at X=0, 5, and 124. */
    const char s_t_T0_5_124__0055ac38[13] = "\x17"
                                            "t"
                                            "\a"
                                            "T0,5,124.";

    void Load_Strings() {
        string_overrides::Reset_Cache();
        HAROLD::Load_H_Strings();
        RUSS::Load_Russ_Strings();
        ESTRINGS::Load_E_Strings();
        RACESCRN::Load_Relations_Strings();
        COLSUM::colsum_strings_been_loaded = 0;
    }

    void __cdecl String_To_Lower(char* str) {
        if (*str == '\0') {
            return;
        }

        for (int32_t i = 0; str[i] != '\0'; ++i) {
            if (str[i] >= 0x41 && str[i] <= 0x5A) {
                str[i] += 0x20;
            }
        }
    }

    void __cdecl String_To_Upper(char* str) {
        if (str[0] == '\0') {
            return;
        }

        int32_t i = 0;
        do {
            if (str[i] >= 0x61 && str[i] <= 0x7f) {
                str[i] -= 0x20;
            }
            i++;
        } while (str[i] != '\0');
    }

    void __cdecl Swap_Short(int16_t* lhs, int16_t* rhs) {
        int16_t swapped_value = *lhs;
        *lhs = *rhs;
        *rhs = swapped_value;
    }

    void __cdecl Swap_Char(uint8_t* lhs, uint8_t* rhs) {
        uint8_t swapped_value = *lhs;
        *lhs = *rhs;
        *rhs = swapped_value;
    }

    void __cdecl Swap(int32_t* lhs, int32_t* rhs) {
        int32_t swapped_value = *lhs;
        *lhs = *rhs;
        *rhs = swapped_value;
    }

    void __cdecl Delete_Structure(int32_t index, void* array, uint32_t element_size_bytes, uint32_t element_count) {
        if (index < (int32_t)(element_count - 1)) {
            uint8_t* array_bytes = (uint8_t*)array;
            struct_::Copy_Structure(
                array_bytes + (uint32_t)index * element_size_bytes,
                array_bytes + ((uint32_t)index + 1) * element_size_bytes,
                (element_count - (uint32_t)index - 1) * element_size_bytes);
        }
    }

    void __cdecl Trim(char* str) {
        int32_t first_non_space = 0;
        while (str[first_non_space] == ' ') {
            first_non_space++;
        }

        if (first_non_space > 0) {
            memmove(str, str + first_non_space, strlen(str + first_non_space) + 1);
        }

        int32_t length = 0;
        while (str[length] != '\0') {
            length++;
        }

        for (int32_t i = length - 1; i >= 0; --i) {
            if (str[i] == ' ') {
                str[i] = '\0';
            } else {
                break;
            }
        }
    }
} // namespace strings