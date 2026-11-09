#include "pch.h"

namespace MAPGEN {
    void* name_seg;
    uint16_t* name_index;

    namespace {
        std::array<std::array<char, 15>, MAX_STARS> generated_star_names;
        bool generated_star_names_ready = false;

        void Format_Star_Name(int16_t star_idx, char* dest_name, size_t dest_size) {
            int32_t name_id = name_index[star_idx % STAR_NAME_COUNT];
            int32_t numeral_id = star_idx / STAR_NAME_COUNT;
            const char(*name_table)[15] = (const char(*)[15])name_seg;
            const char* src_name = name_table[name_id];

            if (numeral_id == 0) {
                snprintf(dest_name, dest_size, "%.14s", src_name);
                return;
            }

            const char* numeral = MOX::roman_literals[numeral_id];
            int32_t base_name_length = (int32_t)dest_size -
                (int32_t)strlen(numeral) - 2;
            snprintf(dest_name, dest_size, "%.*s %s", base_name_length, src_name, numeral);
        }
    } // namespace

    /**
     * @brief Initializes and then shuffles an array of star name indices.
     *
     * This function fills the star name index array with every available name,
     * then performs one random swap per name to shuffle the order.
     */
    void __cdecl Randomize_Star_Names(uint16_t* star_name_indices) {
        for (uint16_t i = 0; i < STAR_NAME_COUNT; ++i) {
            star_name_indices[i] = i;
        }

        for (int i = 0; i < STAR_NAME_COUNT; ++i) {
            int32_t idx1 = game_random::Random(STAR_NAME_COUNT) - 1;
            int32_t idx2 = game_random::Random(STAR_NAME_COUNT) - 1;

            if (idx1 != idx2) {
                uint16_t swapped_name_idx = star_name_indices[idx1];
                star_name_indices[idx1] = star_name_indices[idx2];
                star_name_indices[idx2] = swapped_name_idx;
            }
        }
    }

    void __cdecl Get_Star_Name(int16_t star_idx) {
        Format_Star_Name(star_idx, MOX::star[star_idx].name, sizeof(MOX::star[star_idx].name));
    }

    void Reset_Generated_Star_Names() {
        generated_star_names_ready = false;
    }

    void Prepare_Generated_Star_Names() {
        generated_star_names_ready = false;
        for (int16_t i = 0; i < MAX_STARS; ++i) {
            Format_Star_Name(i, generated_star_names[i].data(), generated_star_names[i].size());
        }
        generated_star_names_ready = true;
    }

    bool Copy_Generated_Star_Name(int16_t star_idx, char* name, size_t name_size) {
        if (!generated_star_names_ready || star_idx < 0 ||
            star_idx >= MAX_STARS || name == nullptr || name_size == 0) {
            return false;
        }
        snprintf(name, name_size, "%s", generated_star_names[star_idx].data());
        return true;
    }
} // namespace MAPGEN
