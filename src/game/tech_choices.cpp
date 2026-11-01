#include "pch.h"

namespace TECH {
    bool Read_Research_Choice(const s_player* player, int16_t group, bool include_current, s_research_choice* choice) {
        if (player == nullptr || choice == nullptr || group < 1 || group > 8) return false;
        *choice = {};
        int16_t field = MOX::first_field_in_group[group];
        int visited = 0;
        while (field != 0) {
            if (field < 0 || field >= TECH_FIELD_COUNT || ++visited >= TECH_FIELD_COUNT) return false;
            if (player->tech_fields[field] == 2 && (include_current || (uint8_t)player->current_research_field != field)) break;
            field = TECHDATA::technology_fields[field].next_field_id;
        }
        if (field == 0) return true;
        choice->field = field;
        if (field >= TECH_FIELD_BIOLOGY) {
            choice->applications[choice->count++] = Get_Hyper_Tech_App_ID(field);
        } else {
            for (int16_t application : TECHDATA::technology_fields[field].tech) {
                if (application < 0 || application >= TECH_APP_COUNT) return false;
                if (player->tech_applications[application] == TECH_RESEARCH_STATUS_AVAILABLE) {
                    choice->applications[choice->count++] = application;
                }
            }
            if (choice->count == 0) choice->count = 1; // Native selectable no-application entry.
        }
        return true;
    }

    void Copy_Application_Name(int16_t application, int16_t player, char* buffer, size_t size) {
        if (size == 0) return;
        strlcpy(buffer, TECHDATA::technology_applications[application].name, size);
        if (application < TECH_APP_BIOLOGY) return;
        const uint16_t level = (uint16_t)MOX::player[player].hyper_advanced_tech[application - TECH_APP_BIOLOGY] + 1;
        strlcat(buffer, " ", size);
        if (level > 0 && level <= 20) {
            strlcat(buffer, MOX::roman_literals[level], size);
        } else {
            const size_t length = strlen(buffer);
            snprintf(buffer + length, size - length, "%u", (unsigned)level);
        }
    }
} // namespace TECH
