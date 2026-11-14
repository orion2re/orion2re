#include "pch.h"

namespace COLMOVE {
    int16_t cluster_colony_n = -1;
    uint8_t g_hacky_dont_draw_cluster_flag;

    void __cdecl Clear_Cluster() {
        int16_t unemployed_idx = cluster_colony_n == -1 ? -1 : Get_First_Unemployed(cluster_colony_n);

        while (unemployed_idx != -1) {
            uint32_t* pop_flags = &MOX::colony[cluster_colony_n].pop[unemployed_idx];

            if (!POP::Is_Assigned(POP::Load(pop_flags))) {
                POP::Set_Assigned(pop_flags, true);
            }

            unemployed_idx = Get_First_Unemployed(cluster_colony_n);
        }

        cluster_colony_n = -1;
        COLONY::Reset_Mouse_Picture();
    }

    void __cdecl Get_Cluster(int16_t colony_idx, int16_t start_pop_idx) {
        s_colony* colony = &MOX::colony[colony_idx];

        if (POP::Is_Native(colony->pop[start_pop_idx]) && !native_population_transfer) {
            char* message = ESTRINGS::E_Strings(0x17e /* ESTR_NATIVES_CANNOT_BE_MOVED_TO_ANOTHER_JOB_OR_PLANET */);
            GENDRAW::Help(message);
            COLONY::Reset_Field_Mode();
            return;
        }

        cluster_colony_n = colony_idx;

        for (int16_t i = start_pop_idx; i < (int16_t)(uint8_t)colony->n_pops; ++i) {
            if (Pops_Identical(colony_idx, start_pop_idx, colony_idx, i)) {
                POP::Set_Assigned(&colony->pop[i], false);
            }
        }

        COLCALC::Colony_Calculation(colony_idx);
        COLONY::Clear_Mouse_Picture();
    }

    int16_t __cdecl Get_First_Unemployed(int16_t colony_idx) {
        s_colony& colony = MOX::colony[colony_idx];
        uint8_t n_pops = (uint8_t)colony.n_pops;

        if (n_pops != 0) {
            for (int16_t i = 0; i < (int16_t)n_pops; ++i) {
                if (!POP::Is_Assigned(colony.pop[i])) {
                    return i;
                }
            }
        }

        return -1;
    }

    int16_t __cdecl Get_Num_Unemployed(int16_t colony_idx) {
        int16_t unemployed_count = 0;
        int16_t n_pops = (uint8_t)MOX::colony[colony_idx].n_pops;

        for (int16_t i = 0; i < n_pops; ++i) {
            if (!POP::Is_Assigned(MOX::colony[colony_idx].pop[i])) {
                unemployed_count++;
            }
        }

        return unemployed_count;
    }

    bool __cdecl Pops_Identical(int16_t source_colony_idx, int16_t source_pop_idx, int16_t dest_colony_idx, int16_t dest_pop_idx) {
        uint32_t pop1 = MOX::colony[source_colony_idx].pop[source_pop_idx];
        uint32_t pop2 = MOX::colony[dest_colony_idx].pop[dest_pop_idx];

        if (POP::Get_Prof(pop1) != POP::Get_Prof(pop2)) {
            return false;
        }

        int16_t state2 = COLONY::Pop_To_Pop_State(dest_colony_idx, dest_pop_idx);
        int16_t state1 = COLONY::Pop_To_Pop_State(source_colony_idx, source_pop_idx);

        if (state2 != state1) {
            return false;
        }

        if (POP::Get_Race(pop1) != POP::Get_Race(pop2) ||
            POP::Is_Conquered(pop1) != POP::Is_Conquered(pop2)) {
            return false;
        }

        return true;
    }

    void __cdecl Send_Cluster(int16_t colony_idx, int16_t requested_job) {
        int16_t warning_state = 0;
        bool extra_success_notice = false;
        int16_t moved_count = 0;
        s_settler_settle_result settle_result;
        char source_planet_name[20];
        char destination_planet_name[20];
        char pop_string[52];
        char message[1000];

        if (colony_idx < 0) {
            GENDRAW::Help(ESTRINGS::E_Strings(0x210 /* ESTR_THIS_IS_NOT_A_VALID_COLONY */));
            return;
        }

        int16_t unemployed_pop_idx = COLMOVE::Get_First_Unemployed(COLMOVE::cluster_colony_n);
        if (unemployed_pop_idx == -1) {
            COLONY::Col_Calc_Wrapper(COLMOVE::cluster_colony_n);
            if (colony_idx != COLMOVE::cluster_colony_n) {
                COLONY::Col_Calc_Wrapper(colony_idx);
            }

            if (COLMOVE::Get_First_Unemployed(COLMOVE::cluster_colony_n) == -1) {
                COLONY::Reset_Mouse_Picture();
                COLONY::Reset_Field_Mode();
                COLMOVE::cluster_colony_n = -1;
            }
            return;
        }

        COLMOVE::Colony_Pop_String(COLMOVE::cluster_colony_n, unemployed_pop_idx, pop_string, sizeof(pop_string));

        while (unemployed_pop_idx != -1) {
            if (colony_idx == COLMOVE::cluster_colony_n) {
                uint32_t* pop_flags = &MOX::colony[COLMOVE::cluster_colony_n].pop[unemployed_pop_idx];
                const int32_t current_job = (int32_t)POP::Get_Prof(POP::Load(pop_flags));

                if (requested_job == -1 || current_job == requested_job) {
                    POP::Set_Assigned(pop_flags, true);
                } else {
                    if (COLMOVE::Give_Colonist_New_Job(
                            COLMOVE::cluster_colony_n,
                            unemployed_pop_idx,
                            requested_job,
                            false) == 0) {
                        return;
                    }
                }
            } else {
                s_colony* const source_colony = &MOX::colony[COLMOVE::cluster_colony_n];
                s_colony* const destination_colony = &MOX::colony[colony_idx];

                SETTLER::Pop_Tries_To_Settle(
                    source_colony,
                    unemployed_pop_idx,
                    destination_colony,
                    &settle_result);

                if (settle_result.can_settle != 0) {
                    if (settle_result.eta_turns != 0 || settle_result.no_room_at_destination != 0) {
                        bool show_confirmation = false;

                        if (settle_result.eta_turns != 0) {
                            if (settle_result.no_room_at_destination == 0) {
                                if (warning_state != 1) {
                                    snprintf(
                                        message,
                                        sizeof(message),
                                        ESTRINGS::E_Strings(0x209),
                                        (uint32_t)settle_result.eta_turns);
                                    warning_state = 1;
                                    COLONY::Reset_Mouse_Picture();
                                    show_confirmation = true;
                                }
                            } else if (warning_state != 3) {
                                const int16_t unemployed_count =
                                    COLMOVE::Get_Num_Unemployed(COLMOVE::cluster_colony_n);

                                if (moved_count == 0) {
                                    snprintf(
                                        message,
                                        sizeof(message),
                                        ESTRINGS::E_Strings(0x206),
                                        (uint32_t)settle_result.eta_turns);
                                } else {
                                    snprintf(
                                        message,
                                        sizeof(message),
                                        ESTRINGS::E_Strings(0x204),
                                        (int32_t)moved_count,
                                        (int32_t)unemployed_count,
                                        (uint32_t)settle_result.eta_turns);
                                }

                                warning_state = 3;
                                COLONY::Reset_Mouse_Picture();
                                show_confirmation = true;
                            }
                        } else if (settle_result.no_room_at_destination != 0 && warning_state != 2) {
                            const int16_t unemployed_count =
                                COLMOVE::Get_Num_Unemployed(COLMOVE::cluster_colony_n);

                            if (moved_count == 0) {
                                strlcpy(message, ESTRINGS::E_Strings(0x205), sizeof(message));
                            } else {
                                snprintf(
                                    message,
                                    sizeof(message),
                                    ESTRINGS::E_Strings(0x203),
                                    (int32_t)moved_count,
                                    (int32_t)unemployed_count);
                            }

                            warning_state = 2;
                            COLONY::Reset_Mouse_Picture();
                            show_confirmation = true;
                        }

                        if (show_confirmation) {
                            settle_result.can_settle = (uint8_t)GENDRAW::Do_Confirmation_Box(message);
                            COLONY::full_draw = 1;
                            COLONY::Clear_Mouse_Picture();
                        }
                    }

                    if (settle_result.can_settle != 0) {
                        if (settle_result.field10_0xa != 0) {
                            extra_success_notice = true;
                        }

                        uint32_t* pop_flags = &source_colony->pop[unemployed_pop_idx];
                        const int32_t current_job = (int32_t)POP::Get_Prof(POP::Load(pop_flags));

                        if (requested_job != -1 && current_job != requested_job) {
                            if (COLMOVE::Give_Colonist_New_Job(
                                    COLMOVE::cluster_colony_n,
                                    unemployed_pop_idx,
                                    requested_job,
                                    true) == 0) {
                                return;
                            }

                            extra_success_notice = true;
                            POP::Set_Assigned(pop_flags, false);
                        }

                        SETTLER::Settle_Pop(source_colony, unemployed_pop_idx, destination_colony);
                        ++moved_count;
                    } else {
                        int16_t line_count = 0;
                        unemployed_pop_idx = -1;

                        if (settle_result.eta_turns != 0) {
                            if (settle_result.no_room_at_destination == 0) {
                                ERIC::String_Builder2(
                                    message,
                                    sizeof(message),
                                    0,
                                    ESTRINGS::E_Strings(0xFE),
                                    (uint32_t)settle_result.eta_turns);
                                line_count = 1;
                            } else {
                                ERIC::String_Builder2(
                                    message,
                                    sizeof(message),
                                    0,
                                    ESTRINGS::E_Strings(0xFC),
                                    (uint32_t)settle_result.eta_turns);
                                line_count = 1;
                            }
                        } else if (settle_result.no_room_at_destination != 0) {
                            ERIC::String_Builder2(message, sizeof(message), 0, ESTRINGS::E_Strings(0xFD));
                            line_count = 1;
                        }

                        if (settle_result.dest_is_outpost != 0) {
                            ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x1FD));
                            ++line_count;
                        }

                        if (settle_result.only_one_pop_left != 0) {
                            ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x241));
                            ++line_count;
                        }

                        if (settle_result.max_settlers_in_transit != 0) {
                            ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x24D));
                            ++line_count;
                        }

                        if (settle_result.source_blockaded != 0) {
                            ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x207));
                            ++line_count;
                        }

                        if (settle_result.insufficient_freighters != 0) {
                            ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x184));
                            ++line_count;
                        }

                        if (settle_result.dest_owner_mismatch != 0) {
                            ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x211));
                            ++line_count;
                        }

                        if (settle_result.field10_0xa != 0) {
                            ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x20F));
                            ++line_count;
                        }

                        if (settle_result.selected_pop_flag_0x400 != 0) {
                            ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x20E));
                            ++line_count;
                        }

                        if (line_count > 0) {
                            if (moved_count > 0) {
                                ERIC::String_Builder2(
                                    message,
                                    sizeof(message),
                                    line_count,
                                    ESTRINGS::E_Strings(0x80),
                                    (int32_t)moved_count);
                            }

                            COLONY::Reset_Mouse_Picture();
                            COLONY::full_draw = 1;
                            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x1D6), message);
                            COLONY::Clear_Mouse_Picture();
                        } else {
                            COLONY::Reset_Mouse_Picture();
                            COLONY::full_draw = 1;
                            GENDRAW::Help(ESTRINGS::E_Strings(0xE4));
                            COLONY::Clear_Mouse_Picture();
                        }
                    }
                } else {
                    int16_t line_count = 0;
                    unemployed_pop_idx = -1;

                    if (settle_result.eta_turns != 0) {
                        if (settle_result.no_room_at_destination == 0) {
                            ERIC::String_Builder2(
                                message,
                                sizeof(message),
                                0,
                                ESTRINGS::E_Strings(0xFE),
                                (uint32_t)settle_result.eta_turns);
                            line_count = 1;
                        } else {
                            ERIC::String_Builder2(
                                message,
                                sizeof(message),
                                0,
                                ESTRINGS::E_Strings(0xFC),
                                (uint32_t)settle_result.eta_turns);
                            line_count = 1;
                        }
                    } else if (settle_result.no_room_at_destination != 0) {
                        ERIC::String_Builder2(message, sizeof(message), 0, ESTRINGS::E_Strings(0xFD));
                        line_count = 1;
                    }

                    if (settle_result.dest_is_outpost != 0) {
                        ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x1FD));
                        ++line_count;
                    }

                    if (settle_result.only_one_pop_left != 0) {
                        ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x241));
                        ++line_count;
                    }

                    if (settle_result.max_settlers_in_transit != 0) {
                        ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x24D));
                        ++line_count;
                    }

                    if (settle_result.source_blockaded != 0) {
                        ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x207));
                        ++line_count;
                    }

                    if (settle_result.insufficient_freighters != 0) {
                        ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x184));
                        ++line_count;
                    }

                    if (settle_result.dest_owner_mismatch != 0) {
                        ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x211));
                        ++line_count;
                    }

                    if (settle_result.field10_0xa != 0) {
                        ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x20F));
                        ++line_count;
                    }

                    if (settle_result.selected_pop_flag_0x400 != 0) {
                        ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x20E));
                        ++line_count;
                    }

                    if (line_count > 0) {
                        if (moved_count > 0) {
                            ERIC::String_Builder2(
                                message,
                                sizeof(message),
                                line_count,
                                ESTRINGS::E_Strings(0x80),
                                (int32_t)moved_count);
                        }

                        COLONY::Reset_Mouse_Picture();
                        COLONY::full_draw = 1;
                        TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x1D6), message);
                        COLONY::Clear_Mouse_Picture();
                    } else {
                        COLONY::Reset_Mouse_Picture();
                        COLONY::full_draw = 1;
                        GENDRAW::Help(ESTRINGS::E_Strings(0xE4));
                        COLONY::Clear_Mouse_Picture();
                    }
                }
            }

            if (unemployed_pop_idx == -1) {
                break;
            }

            unemployed_pop_idx = COLMOVE::Get_First_Unemployed(COLMOVE::cluster_colony_n);
        }

        COLONY::Col_Calc_Wrapper(COLMOVE::cluster_colony_n);
        if (colony_idx != COLMOVE::cluster_colony_n) {
            COLONY::Col_Calc_Wrapper(colony_idx);
        }

        if (moved_count > 0) {
            strlcpy(
                source_planet_name,
                HACCESS::Get_Planet_Name(MOX::colony[COLMOVE::cluster_colony_n].planet),
                sizeof(source_planet_name));
            strlcpy(
                destination_planet_name,
                HACCESS::Get_Planet_Name(MOX::colony[colony_idx].planet),
                sizeof(destination_planet_name));

            ERIC::String_Builder2(
                message,
                sizeof(message),
                0,
                ESTRINGS::E_Strings(0x1D1),
                (int32_t)moved_count,
                pop_string,
                source_planet_name,
                destination_planet_name,
                (uint32_t)settle_result.eta_turns);

            int16_t line_count = 1;

            if (settle_result.dest_blockaded != 0) {
                ERIC::String_Builder2(message, sizeof(message), 1, ESTRINGS::E_Strings(0x84));
                line_count = 2;
            }

            if (settle_result.field10_0xa != 0) {
                ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x85));
                ++line_count;
            }

            if (extra_success_notice) {
                ERIC::String_Builder2(message, sizeof(message), line_count, ESTRINGS::E_Strings(0x83));
            }

            COLONY::Reset_Mouse_Picture();
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x1D5), message);
            COLONY::Clear_Mouse_Picture();

            COLCALC::Colony_Calculation(COLMOVE::cluster_colony_n);
            COLCALC::Colony_Calculation(colony_idx);
        }

        if (COLMOVE::Get_First_Unemployed(COLMOVE::cluster_colony_n) == -1) {
            COLONY::Reset_Mouse_Picture();
            COLONY::Reset_Field_Mode();
            COLMOVE::cluster_colony_n = -1;
        }
    }

    uint8_t __cdecl Give_Colonist_New_Job(
        int16_t colony_idx,
        int16_t pop_idx,
        int16_t new_job,
        bool inter_colony_transfer) {
        int16_t pop_state = COLONY::Pop_To_Pop_State(colony_idx, pop_idx);

        if ((pop_state == 3 || pop_state == 6) && (new_job == ECON_RESEARCH || new_job == ECON_INDUSTRY)) {
            GENDRAW::Help(ESTRINGS::E_Strings(ESTR_THIS_COLONIST_IS_A_NATIVE_NATIVES_CAN_ONLY_FARM_OR_MINE));
            return 0;
        }

        if (pop_state == 4) {
            int16_t current_job = (int16_t)POP::Get_Prof(MOX::colony[colony_idx].pop[pop_idx]);
            if (current_job != new_job) {
                GENDRAW::Help(ESTRINGS::E_Strings(ESTR_THIS_COLONIST_IS_AN_ANDROID_AND_CANNOT_BE_RECONFIGURED_FOR_ANOTHER_JOB));
                return 0;
            }
        }

        int16_t sum_colonists = COLONY::Sum_Colonists(colony_idx, new_job, -1, -1, 0, -1);
        if (sum_colonists >= 42) {
            GENDRAW::Help(ESTRINGS::E_Strings(ESTR_NO_ROOM_FOR_ANOTHER_COLONIST_IN_THIS_JOB_FIELD));
            return 0;
        }

        uint8_t max_farms = MOX::colony[colony_idx].max_farms;
        int16_t sum_colonists_again = COLONY::Sum_Colonists(colony_idx, new_job, -1, -1, 0, -1);

        if (inter_colony_transfer || max_farms > sum_colonists_again || new_job != ECON_FOOD) {
            uint32_t* pop = &MOX::colony[colony_idx].pop[pop_idx];
            POP::Set_Prof(pop, (POP::e_prof)new_job);
            POP::Set_Assigned(pop, true);

            return 1;
        }

        GENDRAW::Help(ESTRINGS::E_Strings(ESTR_NO_ROOM_FOR_ANOTHER_COLONIST_IN_THIS_JOB_FIELD));
        return 0;
    }

    void __cdecl Colony_Pop_String(int16_t colony_idx, int16_t pop_idx, char* output_buffer, size_t output_buffer_size) {
        if (output_buffer_size == 0) {
            return;
        }

        uint32_t pop_value = MOX::colony[colony_idx].pop[pop_idx];

        if (MOX::settings.language == 4) {
            int16_t pop_state = COLONY::Pop_To_Pop_State(colony_idx, pop_idx);
            char* pop_state_string = COLONY::Pop_State_String(pop_state);
            char* conquered_string = COLONY::Conquered_String(POP::Is_Conquered(pop_value));
            char* pop_race_string = COLONY::Pop_Race_String(colony_idx, pop_idx);
            char* job_string = COLONY::Job_String((int16_t)POP::Get_Prof(pop_value));
            char* format = ESTRINGS::E_Strings(0x4C);

            snprintf(output_buffer, output_buffer_size, format, job_string, pop_race_string, conquered_string, pop_state_string);
            return;
        }

        char* job_string = COLONY::Job_String((int16_t)POP::Get_Prof(pop_value));
        int16_t pop_state = COLONY::Pop_To_Pop_State(colony_idx, pop_idx);
        char* pop_state_string = COLONY::Pop_State_String(pop_state);
        char* pop_race_string = COLONY::Pop_Race_String(colony_idx, pop_idx);
        char* conquered_string = COLONY::Conquered_String(POP::Is_Conquered(pop_value));
        char* format = ESTRINGS::E_Strings(0x4C);

        snprintf(output_buffer, output_buffer_size, format, conquered_string, pop_race_string, pop_state_string, job_string);
    }
} // namespace COLMOVE