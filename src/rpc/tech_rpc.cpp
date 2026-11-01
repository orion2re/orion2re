#include "pch.h"

#include "config/text_encoding.h"
#include "rpc/rpc_game.h"
#include "rpc/tech_rpc.h"

namespace tech_rpc {
    bool Read_Research(rpc::s_map_observation* observation) {
        if (observation == nullptr || MOX::player == nullptr || observation->viewer != MOX::PLAYER_NUM ||
            observation->viewer < 0 || observation->viewer >= MOX::NUM_PLAYERS || observation->viewer >= MAX_PLAYERS ||
            RUSS::all_ai_game != 0) return false;
        const int16_t viewer = (int16_t)observation->viewer;
        const s_player* player = &MOX::player[viewer];
        if (player->objectives != PLAYER_OBJECTIVE_HUMAN || player->eliminated != 0) return false;
        rpc::s_research_observation research;
        int32_t fields[TECH_FIELD_COUNT];
        int32_t applications[TECH_APP_COUNT];
        std::fill(std::begin(fields), std::end(fields), -1);
        std::fill(std::begin(applications), std::end(applications), -1);
        auto Technology = [&](int16_t id, bool application) -> int32_t {
            int32_t* index = application ? &applications[id] : &fields[id];
            if (*index >= 0) return *index;
            const char* name = application ? TECHDATA::technology_applications[id].name : (id >= TECH_FIELD_BIOLOGY ? MOX::hyper_field_title : TECHDATA::technology_fields[id].name);
            if (name == nullptr || strnlen(name, 128) == 128) return -1;
            char buffer[sizeof(MOX::temp_string_buf)];
            if (application) {
                TECH::Copy_Application_Name(id, viewer, buffer, sizeof(buffer));
                name = buffer;
            } else if (id >= TECH_FIELD_BIOLOGY) {
                strlcpy(buffer, name, sizeof(buffer));
                name = buffer;
            }
            rpc::s_technology_observation technology;
            technology.application = application;
            config::Decode_Cp850_To_Utf8(name, strlen(name), &technology.name);
            const int16_t field = application ? TECHDATA::technology_applications[id].tech_field_id : id;
            if (field > 0 && field < TECH_FIELD_COUNT) {
                technology.cost = TECH::New_Get_Tech_Cost(id, application, viewer);
                technology.cost_available = technology.cost >= 0;
                if (!technology.cost_available) technology.cost = 0;
            }
            *index = (int32_t)research.technologies.size();
            research.technologies.push_back(std::move(technology));
            return *index;
        };
        const uint8_t field = (uint8_t)player->current_research_field;
        if (field >= TECH_FIELD_COUNT) return false;
        const bool completed = player->research_breakthrough != 0 || (field != 0 && player->tech_fields[field] == 3);
        research.mode = completed ? "completed" : (field == 0 ? "no-selection" : (field >= TECH_FIELD_BIOLOGY ? "hyper" : "researching"));
        if (field != 0) {
            research.project = Technology(field, false);
            if (research.project < 0) return false;
            const uint8_t application = (uint8_t)player->current_research_application;
            if (application >= TECH_APP_COUNT) return false;
            if (application != 0) {
                research.application = Technology(application, true);
                if (research.application < 0) return false;
            }
        }
        for (int16_t group : TECH::entry_to_group) {
            TECH::s_research_choice choice;
            if (!TECH::Read_Research_Choice(player, group, true, &choice)) return false;
            if (choice.field == 0) continue;
            const int32_t field_index = Technology(choice.field, false);
            if (field_index < 0) return false;
            const int32_t cost = TECH::New_Get_Tech_Cost(choice.field, false, viewer);
            if (cost < 0) return false;
            const int64_t remaining = std::max<int64_t>(0, (int64_t)cost - player->research_accumulated);
            if (remaining > INT32_MAX) return false;
            for (int app = 0; app < choice.count; ++app) {
                const int16_t application = choice.applications[app];
                const int32_t application_index = application != 0 ? Technology(application, true) : -1;
                if (application != 0 && application_index < 0) return false;
                research.options.push_back({field_index, application_index, (int32_t)remaining});
            }
        }
        observation->research = std::move(research);
        observation->research_captured = true;
        return true;
    }
} // namespace tech_rpc
