#include "pch.h"
#include "game/game_events.h"
#include "string_overrides.h"

namespace RUSS {
    int8_t all_ai_game;
    std::vector<char> russ_strings;
    const char s_Seed_ld[] = "Seed: %ld";
    const char s_Map_d[] = "Map:  %d";

    void __cdecl Assert_Settings() {
    }

    int16_t __cdecl Get_Weighted_Choice_Char(uint8_t* weights, int16_t count) {
        uint32_t total_weight = 0;
        for (int16_t i = 0; i < count; ++i) {
            total_weight += weights[i];
        }

        if (total_weight == 0) {
            return -1;
        }

        uint16_t r = (uint16_t)game_random::Random(total_weight);
        int16_t index = 0;

        if (weights[index] < r) {
            do {
                r -= (uint16_t)weights[index];
                index++;
            } while (weights[index] < r);
        }

        return index;
    }

    int16_t __cdecl Get_Weighted_Choice_Short(const uint16_t* weights, int16_t count) {
        const uint8_t* bytes = (const uint8_t*)weights;
        uint32_t total_weight = 0;

        if (count > 0) {
            for (int16_t i = 0; i < count; ++i) {
                total_weight += unaligned::Load<uint16_t>(bytes + i * sizeof(uint16_t));
            }
        }

        if (total_weight == 0) {
            return -1;
        }

        uint32_t random_val = (uint32_t)game_random::Random((int32_t)total_weight);
        int16_t index = 0;

        if (unaligned::Load<uint16_t>(bytes + index * sizeof(uint16_t)) < random_val) {
            do {
                uint32_t current_weight = unaligned::Load<uint16_t>(bytes + index * sizeof(uint16_t));
                index++;
                random_val -= current_weight;
            } while (unaligned::Load<uint16_t>(bytes + index * sizeof(uint16_t)) < random_val);
        }

        return index;
    }

    int16_t __cdecl Get_Weighted_Choice_Int(uint32_t* weights, int16_t count) {
        const uint8_t* bytes = (const uint8_t*)weights;
        uint32_t total_weight = 0;
        for (int16_t i = 0; i < count; ++i) {
            total_weight += unaligned::Load<uint32_t>(bytes + i * sizeof(uint32_t));
        }

        if (total_weight == 0) {
            return -1;
        }

        uint32_t roll = (uint32_t)game_random::Random((int32_t)total_weight);
        int16_t selected_index = 0;

        if (unaligned::Load<uint32_t>(bytes) < roll) {
            do {
                roll -= unaligned::Load<uint32_t>(bytes + selected_index * sizeof(uint32_t));
                ++selected_index;
            } while (unaligned::Load<uint32_t>(bytes + selected_index * sizeof(uint32_t)) < roll);
        }

        return selected_index;
    }

    void __cdecl Release_All_Blocks(void* segment) {
        allocate::Allocate_First_Block(segment, 1);
    }

    void __cdecl Load_Russ_Strings() {
        char filename_buffer[20];

        uint8_t language = MOX::settings.language;

        snprintf(filename_buffer, sizeof(filename_buffer), "rstring%d.lbx", (uint32_t)language);

        std::vector<char> loaded(0x1964);
        farload::Farload_Data_Static(filename_buffer, 0, loaded.data(), 0, 1, (int32_t)loaded.size());
        string_overrides::Apply_Table(filename_buffer, &loaded, 177);
        russ_strings.swap(loaded);

        char* string_ptr;
        string_ptr = COLCALC::Load_Colcalc_Strings(RUSS::russ_strings.data());
        string_ptr = COMBFIND::Load_Combfind_Strings(string_ptr);
        string_ptr = MSG_::Load_Msg_Strings(string_ptr);
        KEN::Ken_Get_Text_Message(0x68, MSG_::artifacts_and, 0x13);
        string_ptr = NETMOX::Load_Netmox_Strings(string_ptr);
        string_ptr = NETSTART::Load_Netstart_Strings(string_ptr);
        string_ptr = NETTURN::Load_Netturn_Strings(string_ptr);
        REPORT::Load_Report_Strings(string_ptr);
    }

    void __cdecl Make_All_AI_Game() {
        if (MOX::game_type == 0) {
            RUSS::all_ai_game = 1;

            if (MOX::player[MOX::PLAYER_NUM].objectives == PLAYER_OBJECTIVE_HUMAN) {
                Make_Player_Into_AI((int32_t)MOX::PLAYER_NUM);
            }

            NETMOX::net.all_ai_game_sync_flag = 1;
        } else {
            GENDRAW::Message_Box((char*)"This isn't a single player game, cheeky monkey!");
        }
    }

    void __cdecl Shuffle_Char(int8_t* buffer, int16_t length) {
        int16_t index = 0;

        if (length > 0) {
            do {
                int16_t current_index = index;
                int32_t random_offset = game_random::Random(length - current_index);
                int16_t swap_index = (int16_t)(index + random_offset - 1);
                ++index;

                int8_t swapped_value = buffer[current_index];
                buffer[current_index] = buffer[swap_index];
                buffer[swap_index] = swapped_value;
            } while (index < length);
        }
    }

    void __cdecl Shuffle_Int(int32_t* array, int16_t count) {
        if (count <= 0) {
            return;
        }

        // These arrays can reside in packed records, so access them as bytes.
        uint8_t* bytes = (uint8_t*)array;
        for (int16_t i = 0; i < count; ++i) {
            int32_t random_offset = game_random::Random((int32_t)count - i);
            int16_t target_idx = (int16_t)(i + random_offset - 1);

            uint8_t* element = bytes + i * sizeof(int32_t);
            uint8_t* target = bytes + target_idx * sizeof(int32_t);
            int32_t swapped_value = unaligned::Load<int32_t>(element);
            unaligned::Store<int32_t>(element, unaligned::Load<int32_t>(target));
            unaligned::Store<int32_t>(target, swapped_value);
        }
    }

    void __cdecl Shuffle_Sint(int16_t* data, int16_t count) {
        if (count <= 0) {
            return;
        }

        uint8_t* bytes = (uint8_t*)data;
        for (int16_t i = 0; i < count; ++i) {
            int32_t random_offset = game_random::Random((int32_t)count - i);

            int16_t target_idx = (int16_t)(i + random_offset - 1);

            uint8_t* element = bytes + i * sizeof(int16_t);
            uint8_t* target = bytes + target_idx * sizeof(int16_t);
            int16_t swapped_value = unaligned::Load<int16_t>(element);
            unaligned::Store<int16_t>(element, unaligned::Load<int16_t>(target));
            unaligned::Store<int16_t>(target, swapped_value);
        }
    }

    int16_t __cdecl Star_To_Monster(int16_t star_index) {
        return (int16_t)(star_index % 5 + 1);
    }

    char* __cdecl Star_Name(int16_t star_index) {
        if (star_index < MOX::NUM_STARS) {
            return MOX::star[star_index].name;
        }
        return (char*)"Antares";
    }

    void __cdecl Shuffle(void* buffer, int16_t element_size, int16_t num_elements) {
        if (element_size == 1) {
            RUSS::Shuffle_Char((int8_t*)buffer, num_elements);
            return;
        }

        if (element_size == 2) {
            RUSS::Shuffle_Sint((int16_t*)buffer, num_elements);
            return;
        }

        if (element_size == 4) {
            RUSS::Shuffle_Int((int32_t*)buffer, num_elements);
            return;
        }

        if (num_elements > 0) {
            uint8_t swap_buffer[128];
            for (int16_t i = 0; i < num_elements; ++i) {
                int32_t random_offset = game_random::Random((int32_t)num_elements - i);
                int16_t j = i + (int16_t)random_offset - 1;

                if (j != i) {
                    uint8_t* element_i = (uint8_t*)buffer + (int32_t)i * (int32_t)element_size;
                    uint8_t* element_j = (uint8_t*)buffer + (int32_t)j * (int32_t)element_size;

                    memcpy(swap_buffer, element_i, element_size);
                    memcpy(element_i, element_j, element_size);
                    memcpy(element_j, swap_buffer, element_size);
                }
            }
        }
    }

    void __cdecl Mox_Update() {
        platform::Check_Exit();
        sound::Sound_Update();
        NETMOX::Mox_Net_Update();
        game_events::Publish(game_events::IDLE_OBSERVATION_POINT);
    }

    void __cdecl Mox_Sync_Update() {
        Mox_Update();
        platform::Toggle_Pages_Stub();
    }

    void __cdecl Make_Player_Into_AI(int32_t player_idx) {
        MOX::player[player_idx].objectives = (uint8_t)game_random::Random(5);

        INITSHIP::Update_Player_Ship_Designs((int16_t)player_idx);

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            if (MOX::colony[i].owner == player_idx) {
                COLBLDG::Clear_Colony_Queue(i);
            }
        }

        COLCALC::Do_Player_Colony_Calculations((int16_t)player_idx);
    }

    int32_t __cdecl Compare_Ratios(uint32_t first_numerator, uint32_t first_denominator, uint32_t second_numerator, uint32_t second_denominator) {
        int32_t comparison = 2;

        do {
            uint32_t saved_first_numerator = first_numerator;

            if (first_numerator == 0) {
                comparison = (second_numerator != 0) ? -1 : 0;
            } else if (second_numerator == 0) {
                comparison = 1;
            } else if (first_denominator > first_numerator) {
                if (second_denominator <= second_numerator) {
                    comparison = -1;
                } else {
                    uint32_t second_quotient = second_denominator / second_numerator;
                    uint32_t first_quotient = first_denominator / first_numerator;

                    if (first_quotient > second_quotient) {
                        comparison = -1;
                    } else if (first_quotient < second_quotient) {
                        comparison = 1;
                    } else {
                        uint32_t second_remainder = second_denominator % second_numerator;
                        uint32_t first_remainder = first_denominator % first_numerator;

                        first_numerator = second_remainder;
                        first_denominator = second_numerator;
                        second_numerator = first_remainder;
                        second_denominator = saved_first_numerator;
                    }
                }
            } else {
                if (second_denominator > second_numerator) {
                    comparison = 1;
                } else {
                    uint32_t first_quotient = first_numerator / first_denominator;
                    uint32_t second_quotient = second_numerator / second_denominator;

                    if (second_quotient > first_quotient) {
                        comparison = -1;
                    } else if (second_quotient < first_quotient) {
                        comparison = 1;
                    } else {
                        first_numerator %= first_denominator;
                        second_numerator %= second_denominator;
                    }
                }
            }
        } while (comparison == 2);

        return comparison;
    }

    void __cdecl PFMT(int32_t unused1, int32_t unused2, const char* unused_str, ...) {
    }
} // namespace RUSS