#include "pch.h"
#include "string_overrides.h"
#include "game_events.h"

#include "config/config_runtime.h"
#include "game/config_refresh.h"
#include "lua/lua_game.h"

namespace TECHINIT {

    namespace {
        /**
 * Advances to the next non-empty NUL-terminated string in the loaded tech-name block.
 * @param tech_name_block Pointer to the contiguous string resource.
 * @param string_offset In/out byte offset within the resource.
 */
        static char* Advance_To_Next_String(char* tech_name_block, uint32_t& string_offset) {
            while (tech_name_block[string_offset] != '\0') {
                ++string_offset;
            }

            do {
                ++string_offset;
            } while (tech_name_block[string_offset] == '\0');

            return tech_name_block + string_offset;
        }

        static void Apply_Tech_Name(char** target, char* loaded_name) {
            if (!config::Is_Active_String(*target)) {
                *target = loaded_name;
            }
        }
    } // namespace

    void __cdecl Grant_Postwarp_Technologies() {
        const int16_t first_tech_idx =
            MOX::settings.strategic_combat_flag == 1
            ? 0
            : POSTWARP_STRATEGIC_EXTRA_TECH_COUNT;

        for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
            for (int16_t tech_idx = first_tech_idx;
                 tech_idx < POSTWARP_TECH_COUNT;
                 ++tech_idx) {
                const int16_t tech_app_id = postwarp_tech[tech_idx];
                if (tech_app_id >= 0 && tech_app_id < TECH_APP_COUNT) {
                    COLCALC::Player_Gets_Tech_App(
                        &MOX::player[player_idx], tech_app_id);
                }
            }
        }
    }

    void __cdecl Load_Tech_Names() {
        game_events::Publish(game_events::STRATEGIC_LABELS_REPLACING);
        LOADER::Load_Player_Special_Names();
        RUSS::Mox_Update();

        uint32_t string_offset = 0;
        const size_t name_count = size_t(TECH_FIELD_COUNT) + TECH_APP_COUNT + BUILDING_COUNT + SPECIAL_COUNT +
            ARMOR_COUNT + SHIELD_COUNT + 2 * WEAPON_COUNT + COMPUTER_COUNT + DRIVE_COUNT + UNIT_COUNT +
            FUEL_COUNT + 2 * WEAPON_MOD_COUNT + 2 * HULL_CLASS_COUNT;
        char* tech_name_block = string_overrides::Reload_Raw_Table(
            "techname.lbx",
            (uint32_t)MOX::settings.language,
            &MOX::techname_seg, name_count, 1, true);

        RUSS::Mox_Update();

        Apply_Tech_Name(&TECHDATA::technology_fields[TECH_FIELD_STARTING_TECH].name,
            tech_name_block);
        for (uint32_t i = 1; i < TECH_FIELD_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::technology_fields[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < TECH_APP_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::technology_applications[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < BUILDING_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::buildings[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < SPECIAL_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::specials[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < ARMOR_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::armor[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < SHIELD_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::shields[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < WEAPON_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::weapons[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < COMPUTER_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::computers[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        for (uint32_t i = 0; i < DRIVE_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::drives[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        for (uint32_t i = 0; i < UNIT_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::units[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < FUEL_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::fuel[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        for (uint32_t i = 0; i < WEAPON_MOD_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::weapon_modifications[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < WEAPON_MOD_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::weapon_modifications[i].name_plural,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < WEAPON_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::weapons[i].name_plural,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < HULL_CLASS_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::hull_data[i].name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }

        RUSS::Mox_Update();

        for (uint32_t i = 0; i < HULL_CLASS_COUNT; ++i) {
            Apply_Tech_Name(&TECHDATA::hull_data[i].size_name,
                Advance_To_Next_String(tech_name_block, string_offset));
        }
        CONFIG_REFRESH::Tech_Names_Loaded();
    }

    int16_t __cdecl Tech_Is_Legal_For_Player(int16_t player_idx, int16_t tech_id) {
        int16_t is_legal = 1;

        if (MOX::player[player_idx].traits[TRAIT_CYBERNETIC] != 0) {
            if (tech_id == TECH_APP_ADVANCED_DAMAGE_CONTROL) {
                is_legal = 0;
            }
        }

        if (MOX::player[player_idx].traits[TRAIT_CURRENT_GOVERNMENT] == GOVERNMENT_UNIFICATION) {
            if (tech_id == TECH_APP_HOLO_SIMULATOR ||
                tech_id == TECH_APP_VIRTUAL_REALITY_NETWORK ||
                tech_id == TECH_APP_PLEASURE_DOME) {
                is_legal = 0;
            }
        }

        if (MOX::player[player_idx].traits[TRAIT_ENVIRONMENT_IMMUNE] != 0) {
            if (tech_id == TECH_APP_POLLUTION_PROCESSOR ||
                tech_id == TECH_APP_ATMOSPHERIC_RENEWER ||
                tech_id == TECH_APP_CORE_WASTE_DUMPS ||
                tech_id == TECH_APP_NANO_DISASSEMBLERS) {
                is_legal = 0;
            }
        }

        if (MOX::player[player_idx].traits[TRAIT_EATS_MINERALS] != 0) {
            if (tech_id == TECH_APP_ANDROID_FARMERS ||
                tech_id == TECH_APP_FOOD_REPLICATORS ||
                tech_id == TECH_APP_HYDROPONIC_FARM ||
                tech_id == TECH_APP_SOIL_ENRICHMENT ||
                tech_id == TECH_APP_SUBTERRANEAN_FARMS) {
                is_legal = 0;
            }
            if (tech_id == TECH_APP_BIOMORPHIC_FUNGI &&
                lithovore_allows_biomorphic_fungi == 0) {
                is_legal = 0;
            }
        }

        if (MOX::settings.strategic_combat_flag == 1) {
            if (TECHDATA::technology_applications[tech_id].in_strategic == 0) {
                is_legal = 0;
            }
        }

        if (tech_id == 0x34) {
            if (MOX::settings.xenons_exist == 0) {
                is_legal = 0;
            }
        }

        return is_legal;
    }

    void __cdecl Init_Player_Tech(int16_t player_idx) {
        s_player* player = &MOX::player[player_idx];
        int16_t starting_tech_count = 0;

        player->tech_fields[TECH_FIELD_ASTRO_BIOLOGY] = 2;
        player->tech_fields[TECH_FIELD_NUCLEAR_FISSION] = 2;
        player->tech_fields[TECH_FIELD_PHYSICS] = 2;
        player->tech_fields[TECH_FIELD_ENGINEERING] = 2;
        player->tech_fields[TECH_FIELD_ADVANCED_MAGNETISM] = 2;
        player->tech_fields[TECH_FIELD_CHEMISTRY] = 2;
        player->tech_fields[TECH_FIELD_ELECTRONICS] = 2;
        player->tech_fields[TECH_FIELD_MILITARY_TACTICS] = 2;

        switch (Starting_Civilization_Level_For_Classic_Rules_(
            MOX::settings.starting_civilization_level)) {
            case STARTING_CIVILIZATION_LEVEL_PRE_WARP:
                starting_tech_count = 1;
                break;
            case STARTING_CIVILIZATION_LEVEL_AVERAGE:
                starting_tech_count = 6;
                break;
            case STARTING_CIVILIZATION_LEVEL_ADVANCED:
                starting_tech_count = 25;
                break;
            default:
                break;
        }

        player->tech_fields[TECH_FIELD_STARTING_TECH] = 3;

        for (int16_t tech_app_id = 1; tech_app_id < TECH_APP_COUNT; ++tech_app_id) {
            int16_t tech_field_id = TECHDATA::technology_applications[tech_app_id].tech_field_id;

            if (tech_field_id != TECH_FIELD_ADVANCED_GOVERNMENTS) {
                if (tech_field_id == TECH_FIELD_STARTING_TECH) {
                    player->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_RESEARCHED;
                } else if ((player->traits[TRAIT_UNCREATIVE] == 0 ||
                               COLCALC::Field_Is_Gray_Box(tech_field_id) ||
                               tech_field_id == TECH_FIELD_INVALID ||
                               TECHDATA::technology_fields[tech_field_id].cost == 0x32 ||
                               tech_field_id == 0x17) &&
                    TECHINIT::Tech_Is_Legal_For_Player(player_idx, tech_app_id)) {
                    player->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_AVAILABLE;
                } else {
                    player->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_UNAVAILABLE;
                }

                if (tech_app_id == 0x34 && MOX::settings.xenons_exist == 0) {
                    player->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_UNAVAILABLE;
                }
            }
        }

        for (int16_t tech_slot_idx = 0; tech_slot_idx < 4; ++tech_slot_idx) {
            int16_t tech_app_id =
                TECHDATA::technology_fields[TECH_FIELD_ADVANCED_GOVERNMENTS].tech[tech_slot_idx];
            if (tech_app_id <= 0) {
                break;
            }

            player->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_UNAVAILABLE;
        }

        switch (player->traits[TRAIT_CURRENT_GOVERNMENT]) {
            case GOVERNMENT_FEUDAL:
                player->tech_applications[TECH_APP_CONFEDERATION] = TECH_RESEARCH_STATUS_AVAILABLE;
                break;
            case GOVERNMENT_DICTATORSHIP:
                player->tech_applications[TECH_APP_IMPERIUM] = TECH_RESEARCH_STATUS_AVAILABLE;
                break;
            case GOVERNMENT_DEMOCRACY:
                player->tech_applications[TECH_APP_FEDERATION] = TECH_RESEARCH_STATUS_AVAILABLE;
                break;
            case GOVERNMENT_UNIFICATION:
                player->tech_applications[TECH_APP_GALACTIC_UNIFICATION] = TECH_RESEARCH_STATUS_AVAILABLE;
                player->tech_applications[TECH_APP_HOLO_SIMULATOR] = TECH_RESEARCH_STATUS_UNAVAILABLE;
                player->tech_applications[TECH_APP_PLEASURE_DOME] = TECH_RESEARCH_STATUS_UNAVAILABLE;
                player->tech_applications[TECH_APP_VIRTUAL_REALITY_NETWORK] = TECH_RESEARCH_STATUS_UNAVAILABLE;
                break;
        }

        player->tech_applications[0] = TECH_RESEARCH_STATUS_UNAVAILABLE;

        for (int16_t tech_field_id = 1; tech_field_id < TECH_FIELD_XENON_TECHNOLOGY; ++tech_field_id) {
            const int16_t* field_tech_ids = TECHDATA::technology_fields[tech_field_id].tech;
            int16_t legal_tech_ids[4] = {};
            int16_t legal_count = 0;
            int16_t configured_count = 0;
            bool has_app = false;

            if (player->tech_fields[tech_field_id] == 0) {
                player->tech_fields[tech_field_id] = 1;
            }

            for (int16_t tech_slot_idx = 0; tech_slot_idx < 4; ++tech_slot_idx) {
                int16_t tech_app_id = field_tech_ids[tech_slot_idx];
                if (tech_app_id == 0) {
                    break;
                }

                ++configured_count;
                if (player->tech_applications[tech_app_id] == TECH_RESEARCH_STATUS_UNAVAILABLE) {
                    if (TECHINIT::Tech_Is_Legal_For_Player(player_idx, tech_app_id)) {
                        legal_tech_ids[legal_count++] = tech_app_id;
                    }
                } else {
                    has_app = true;
                }
            }

            if (!has_app) {
                int16_t selected_tech_app_id = 0;
                if (legal_count > 0) {
                    selected_tech_app_id =
                        legal_tech_ids[game_random::Random(legal_count) - 1];
                } else if (configured_count > 0) {
                    selected_tech_app_id =
                        field_tech_ids[game_random::Random(configured_count) - 1];
                }

                if (selected_tech_app_id != 0) {
                    player->tech_applications[selected_tech_app_id] =
                        TECH_RESEARCH_STATUS_AVAILABLE;
                }
            }
        }

        player->tech_fields[TECH_FIELD_XENON_TECHNOLOGY] = 0;

        for (int16_t tech_field_id = TECH_FIELD_BIOLOGY; tech_field_id < TECH_FIELD_COUNT; ++tech_field_id) {
            if (player->tech_fields[tech_field_id] == 0) {
                player->tech_fields[tech_field_id] = 1;
            }
        }

        const bool restore_human_objective = player->objectives == PLAYER_OBJECTIVE_HUMAN;
        if (restore_human_objective) {
            player->objectives = PLAYER_OBJECTIVE_MILITARIST;
        }

        for (int16_t starting_tier_idx = 0; starting_tier_idx < starting_tech_count; ++starting_tier_idx) {
            if (starting_tier_idx < 6) {
                int16_t field_id = TECHDATA::starting_tech_field_ids[starting_tier_idx];
                const int16_t* field_tech_ids = TECHDATA::technology_fields[field_id].tech;

                player->tech_fields[field_id] = 3;
                player->tech_fields[TECHDATA::technology_fields[field_id].next_field_id] = 2;

                for (int16_t tech_slot_idx = 0; tech_slot_idx < 4; ++tech_slot_idx) {
                    int16_t tech_app_id = field_tech_ids[tech_slot_idx];
                    if (tech_app_id == 0) {
                        break;
                    }

                    if (TECHINIT::Tech_Is_Legal_For_Player(player_idx, tech_app_id)) {
                        player->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_RESEARCHED;
                    }
                }
            } else {
                int16_t selected_tech_app_id = newtech::Choose_Tech_Application(player_idx);
                int16_t field_id = TECHDATA::technology_applications[selected_tech_app_id].tech_field_id;
                const int16_t* field_tech_ids = TECHDATA::technology_fields[field_id].tech;

                player->tech_fields[field_id] = 3;
                player->tech_fields[TECHDATA::technology_fields[field_id].next_field_id] = 2;

                if (player->traits[TRAIT_CREATIVE] == 0) {
                    player->tech_applications[selected_tech_app_id] = TECH_RESEARCH_STATUS_RESEARCHED;
                } else {
                    for (int16_t tech_slot_idx = 0; tech_slot_idx < 4; ++tech_slot_idx) {
                        int16_t tech_app_id = field_tech_ids[tech_slot_idx];
                        if (tech_app_id == 0) {
                            break;
                        }

                        if (TECHINIT::Tech_Is_Legal_For_Player(player_idx, tech_app_id)) {
                            player->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_RESEARCHED;
                        }
                    }
                }

                if (field_id == TECH_FIELD_ADVANCED_GOVERNMENTS) {
                    ++player->traits[TRAIT_CURRENT_GOVERNMENT];
                }
            }
        }

        if (restore_human_objective) {
            player->objectives = PLAYER_OBJECTIVE_HUMAN;
        }

        player->best_ftl_drive = (char)INITSHIP::Best_Warp_Drive(player_idx);
        player->ship_speed = (uint8_t)INITSHIP::Calc_Player_FTL_Speed(player_idx);
    }

    void __cdecl Rebuild_Derived_Tech_Data() {
        char error_message[40];

        RUSS::Mox_Update();
        RUSS::Mox_Update();

        for (int16_t tech_app_index = 0; tech_app_index < TECH_APP_COUNT; ++tech_app_index) {
            TECHDATA::technology_applications[tech_app_index].type =
                TECH_APPLICATION_TYPE_UNCLASSIFIED;
        }
        for (int16_t tech_field_index = 0; tech_field_index < TECH_FIELD_COUNT;
             ++tech_field_index) {
            memset(TECHDATA::technology_fields[tech_field_index].tech, 0, sizeof(TECHDATA::technology_fields[tech_field_index].tech));
        }

        for (int16_t building_index = 1; building_index < BUILDING_COUNT; ++building_index) {
            TECHDATA::technology_applications[TECHDATA::buildings[building_index].tech_app].type = TECH_APPLICATION_TYPE_BUILDING;
        }

        RUSS::Mox_Update();

        for (int16_t special_index = 1; special_index < SPECIAL_COUNT; ++special_index) {
            TECHDATA::technology_applications[TECHDATA::specials[special_index].tech_app_id].type = TECH_APPLICATION_TYPE_SPECIAL;
        }

        RUSS::Mox_Update();

        for (int16_t armor_index = 1; armor_index < ARMOR_COUNT; ++armor_index) {
            TECHDATA::technology_applications[TECHDATA::armor[armor_index].tech_app_id].type = TECH_APPLICATION_TYPE_ARMOR;
        }

        RUSS::Mox_Update();

        for (int16_t shield_index = 1; shield_index < SHIELD_COUNT; ++shield_index) {
            TECHDATA::technology_applications[TECHDATA::shields[shield_index].tech_app_id].type = TECH_APPLICATION_TYPE_SHIELD;
        }

        RUSS::Mox_Update();

        for (int16_t weapon_index = 1; weapon_index < WEAPON_COUNT; ++weapon_index) {
            TECHDATA::technology_applications[TECHDATA::weapons[weapon_index].tech_app_id].type = TECH_APPLICATION_TYPE_WEAPON;
        }

        RUSS::Mox_Update();

        for (int16_t computer_index = 1; computer_index < COMPUTER_COUNT; ++computer_index) {
            TECHDATA::technology_applications[TECHDATA::computers[computer_index].tech_app_id].type = TECH_APPLICATION_TYPE_COMPUTER;
        }

        for (int16_t drive_index = 1; drive_index < DRIVE_COUNT; ++drive_index) {
            TECHDATA::technology_applications[TECHDATA::drives[drive_index].tech_app_id].type = TECH_APPLICATION_TYPE_DRIVE;
        }

        for (int16_t fuel_index = 1; fuel_index < FUEL_COUNT; ++fuel_index) {
            TECHDATA::technology_applications[TECHDATA::fuel[fuel_index].tech_app_id].type = TECH_APPLICATION_TYPE_FUEL;
        }

        RUSS::Mox_Update();

        for (int16_t tech_app_index = 0; tech_app_index < TECH_APP_COUNT; ++tech_app_index) {
            int16_t tech_field_id = TECHDATA::technology_applications[tech_app_index].tech_field_id;

            if (tech_field_id != TECH_FIELD_STARTING_TECH && tech_field_id != TECH_FIELD_INVALID &&
                tech_field_id != TECH_FIELD_XENON_TECHNOLOGY) {
                s_tech_field_data& tech_field = TECHDATA::technology_fields[tech_field_id];
                int16_t* tech_slot_ptrs[4] = {
                    &tech_field.tech[0],
                    &tech_field.tech[1],
                    &tech_field.tech[2],
                    &tech_field.tech[3],
                };
                int16_t free_slot_index = -1;

                for (int16_t slot_index = 0; slot_index < 4; ++slot_index) {
                    if (*tech_slot_ptrs[slot_index] == 0) {
                        free_slot_index = slot_index;
                        break;
                    }
                }

                if (free_slot_index == -1) {
                    snprintf(error_message, sizeof(error_message), "%i", tech_app_index);
                    strlcat(error_message, " tech has too many apps", sizeof(error_message));
                    exitm::Exit_With_Message(error_message);
                } else {
                    *tech_slot_ptrs[free_slot_index] = tech_app_index;
                }
            }

            RUSS::Mox_Update();
        }

        RUSS::Mox_Update();
    }

    bool Validate_And_Rebuild_Technology(s_tech_app_data* applications,
        s_tech_field_data* fields,
        const int16_t* starting_fields,
        std::string* diagnostic) {
        if (applications == nullptr || fields == nullptr ||
            starting_fields == nullptr || diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        for (int16_t field_id = 0; field_id < TECH_FIELD_COUNT; ++field_id) {
            if (fields[field_id].tech_field_id != field_id) {
                *diagnostic = "technology field identity mismatch";
                return false;
            }
            if (fields[field_id].cost > INT32_MAX) {
                *diagnostic = "technology field cost exceeds native limits";
                return false;
            }
            memset(fields[field_id].tech, 0, sizeof(fields[field_id].tech));
        }
        if (fields[TECH_FIELD_STARTING_TECH].prev_field_id !=
                TECH_FIELD_STARTING_TECH ||
            fields[TECH_FIELD_STARTING_TECH].next_field_id !=
                TECH_FIELD_STARTING_TECH ||
            fields[TECH_FIELD_XENON_TECHNOLOGY].prev_field_id !=
                TECH_FIELD_XENON_TECHNOLOGY ||
            fields[TECH_FIELD_XENON_TECHNOLOGY].next_field_id !=
                TECH_FIELD_XENON_TECHNOLOGY) {
            *diagnostic = "technology sentinel field links are invalid";
            return false;
        }
        for (int16_t application_id = 0; application_id < TECH_APP_COUNT;
             ++application_id) {
            s_tech_app_data& application = applications[application_id];
            if (application.tech_app_id != application_id) {
                *diagnostic = "technology application identity mismatch";
                return false;
            }
            const int16_t field_id = application.tech_field_id;
            if (field_id < TECH_FIELD_INVALID || field_id >= TECH_FIELD_COUNT) {
                *diagnostic = "technology application has an invalid field";
                return false;
            }
            if (field_id == TECH_FIELD_INVALID ||
                field_id == TECH_FIELD_STARTING_TECH ||
                field_id == TECH_FIELD_XENON_TECHNOLOGY) {
                continue;
            }
            if (application_id == TECH_APP_NO_TECH) {
                *diagnostic =
                    "technology application zero cannot occupy a field slot";
                return false;
            }
            int16_t* slots = fields[field_id].tech;
            int slot = 0;
            while (slot < 4 && slots[slot] != 0) {
                ++slot;
            }
            if (slot == 4) {
                *diagnostic = "technology field has more than four applications";
                return false;
            }
            slots[slot] = application_id;
        }

        bool covered[TECH_FIELD_COUNT] = {};
        covered[TECH_FIELD_STARTING_TECH] = true;
        covered[TECH_FIELD_XENON_TECHNOLOGY] = true;
        for (int branch = 1; branch <= 8; ++branch) {
            int16_t field_id = MOX::first_field_in_group[branch];
            if (field_id <= TECH_FIELD_STARTING_TECH ||
                field_id >= TECH_FIELD_COUNT ||
                field_id == TECH_FIELD_XENON_TECHNOLOGY) {
                *diagnostic = "technology branch has an invalid root";
                return false;
            }
            if (fields[field_id].prev_field_id != TECH_FIELD_STARTING_TECH) {
                *diagnostic = "technology branch root has a previous field";
                return false;
            }
            while (field_id != TECH_FIELD_STARTING_TECH) {
                if (field_id <= TECH_FIELD_STARTING_TECH ||
                    field_id >= TECH_FIELD_COUNT ||
                    field_id == TECH_FIELD_XENON_TECHNOLOGY ||
                    covered[field_id]) {
                    *diagnostic = "technology branch contains a cycle or overlap";
                    return false;
                }
                covered[field_id] = true;
                const int16_t next = fields[field_id].next_field_id;
                if (next < TECH_FIELD_STARTING_TECH ||
                    next >= TECH_FIELD_COUNT ||
                    next == TECH_FIELD_XENON_TECHNOLOGY) {
                    *diagnostic = "technology branch has an invalid next field";
                    return false;
                }
                if (next != TECH_FIELD_STARTING_TECH &&
                    fields[next].prev_field_id != field_id) {
                    *diagnostic =
                        "technology field previous and next links are not reciprocal";
                    return false;
                }
                field_id = next;
            }
        }
        for (int16_t field_id = 1; field_id < TECH_FIELD_COUNT;
             ++field_id) {
            if (field_id != TECH_FIELD_XENON_TECHNOLOGY &&
                !covered[field_id]) {
                *diagnostic = "technology branch graph does not cover field " +
                    std::to_string(field_id);
                return false;
            }
        }
        for (int position = 0; position < 6; ++position) {
            const int16_t field_id = starting_fields[position];
            if (field_id < TECH_FIELD_STARTING_TECH ||
                field_id >= TECH_FIELD_XENON_TECHNOLOGY) {
                *diagnostic = "starting technology field is invalid";
                return false;
            }
        }
        return true;
    }

    bool Finalize_Configured_New_Game_Technology(
        std::string* diagnostic) {
        if (diagnostic == nullptr) {
            return false;
        }
        const orion2re::lua::game::s_technology_setup_event event = {1};
        if (!orion2re::lua::game::Technology_Setup(&event, diagnostic)) {
            return false;
        }
        return Validate_And_Rebuild_Technology(
            TECHDATA::technology_applications,
            TECHDATA::technology_fields,
            TECHDATA::starting_tech_field_ids,
            diagnostic);
    }

    void __cdecl Init_Tech() {
        Rebuild_Derived_Tech_Data();
        TECHINIT::Load_Tech_Names();
        RUSS::Mox_Update();
    }

} // namespace TECHINIT