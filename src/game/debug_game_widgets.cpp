#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>
#endif

namespace debug_game_widgets {
    namespace {
        thread_local char g_label_buffer[64];

        const char* Bounded_Name(const char* source, size_t source_size) {
            if (source == nullptr) {
                return "Unknown";
            }
            const size_t destination_size = std::min(source_size + 1, sizeof(g_label_buffer));
            strlcpy(g_label_buffer, source, destination_size);
            return g_label_buffer;
        }

        const char* Safe_Name(const char* const* names, int32_t index, int32_t count) {
            if (names == nullptr || index < 0 || index >= count || names[index] == nullptr) {
                return "Unknown";
            }
            return names[index];
        }

        const char* Star_Name(uint16_t bit_index, void* user_data) {
            (void)user_data;
            if (bit_index >= (uint16_t)game::Current_Star_Count()) {
                return nullptr;
            }
            return Bounded_Name(game::current_state.stars.at(legacy_world::Star_Id_At_Position(bit_index)).name, sizeof(game::current_state.stars.at(legacy_world::Star_Id_At_Position(bit_index)).name));
        }

        const char* Player_Name(uint16_t bit_index, void* user_data) {
            (void)user_data;
            if (MOX::player == nullptr || bit_index >= (uint16_t)MOX::NUM_PLAYERS) {
                return nullptr;
            }
            return Bounded_Name(
                MOX::player[bit_index].race_name,
                sizeof(MOX::player[bit_index].race_name));
        }

        const char* Special_Name(uint16_t bit_index, void* user_data) {
            (void)user_data;
            if (bit_index >= SPECIAL_COUNT) {
                return nullptr;
            }

            return TECHDATA::specials[bit_index].name;
        }
    } // namespace

    bool Player_Bit(const char* label, uint8_t* bits, int16_t player_index) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (label == nullptr || bits == nullptr || player_index < 0 || player_index >= MAX_PLAYERS) {
            return false;
        }

        const uint8_t mask = (uint8_t)(1u << player_index);
        bool selected = (*bits & mask) != 0;
        if (!ImGui::Checkbox(label, &selected)) {
            return false;
        }
        if (selected) {
            *bits |= mask;
        } else {
            *bits &= (uint8_t)~mask;
        }
        return true;
#else
        (void)label;
        (void)bits;
        (void)player_index;
        return false;
#endif
    }

    bool Enum(
        const char* label,
        int8_t* value,
        const char* const* names,
        int16_t count,
        int16_t first_value) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (label == nullptr || value == nullptr || names == nullptr ||
            count <= 0 || first_value < 0 || first_value >= count) {
            return false;
        }

        bool changed = false;
        const char* preview = Safe_Name(names, *value, count);
        if (ImGui::BeginCombo(label, preview)) {
            for (int16_t index = first_value; index < count; ++index) {
                const bool selected = *value == index;
                if (ImGui::Selectable(Safe_Name(names, index, count), selected)) {
                    *value = (int8_t)index;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
#else
        (void)label;
        (void)value;
        (void)names;
        (void)count;
        (void)first_value;
        return false;
#endif
    }

    bool Star_Bitmap(
        const char* id,
        void* bits,
        uint16_t byte_count,
        uint16_t column_count) {
        const int16_t star_count = std::clamp<int16_t>(game::Current_Star_Count(), 0, MAX_STARS);
        return debug_widgets::Bit_Field_Checkboxes(
            id,
            bits,
            byte_count,
            (uint16_t)star_count,
            Star_Name,
            nullptr,
            column_count);
    }

    bool Player_Reference(
        const char* label,
        int8_t* player_index,
        bool allow_none,
        bool allow_contested) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (label == nullptr || player_index == nullptr) {
            return false;
        }
        const int16_t count = MOX::player == nullptr
            ? 0
            : std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        const char* preview = "Invalid";
        if (*player_index == -2 && allow_contested) {
            preview = "Contested";
        } else if (*player_index < 0) {
            preview = "None";
        } else if (*player_index < count) {
            preview = Bounded_Name(
                MOX::player[*player_index].race_name,
                sizeof(MOX::player[*player_index].race_name));
        }
        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            if (allow_contested && ImGui::Selectable("Contested", *player_index == -2)) {
                *player_index = -2;
                changed = true;
            }
            if (allow_none && ImGui::Selectable("None", *player_index == -1)) {
                *player_index = -1;
                changed = true;
            }
            for (int16_t index = 0; index < count; ++index) {
                const bool selected = *player_index == index;
                const char* name = Bounded_Name(
                    MOX::player[index].race_name,
                    sizeof(MOX::player[index].race_name));
                if (ImGui::Selectable(name, selected)) {
                    *player_index = (int8_t)index;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
#else
        (void)label;
        (void)player_index;
        (void)allow_none;
        (void)allow_contested;
        return false;
#endif
    }

    bool Star_Reference(const char* label, int16_t* star_index, bool allow_none) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (label == nullptr || star_index == nullptr) {
            return false;
        }
        const int16_t count = std::clamp<int16_t>(game::Current_Star_Count(), 0, MAX_STARS);
        const char* preview = *star_index < 0
            ? "None"
            : ((*star_index < count)
                      ? Bounded_Name(game::current_state.stars.at(legacy_world::Star_Id_At_Position(*star_index)).name, sizeof(game::current_state.stars.at(legacy_world::Star_Id_At_Position(*star_index)).name))
                      : "Invalid");
        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            if (allow_none && ImGui::Selectable("None", *star_index < 0)) {
                *star_index = -1;
                changed = true;
            }
            for (int16_t index = 0; index < count; ++index) {
                ImGui::PushID((int32_t)index);
                const bool selected = *star_index == index;
                const char* name = Bounded_Name(game::current_state.stars.at(legacy_world::Star_Id_At_Position(index)).name, sizeof(game::current_state.stars.at(legacy_world::Star_Id_At_Position(index)).name));
                if (ImGui::Selectable(name, selected)) {
                    *star_index = index;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        return changed;
#else
        (void)label;
        (void)star_index;
        (void)allow_none;
        return false;
#endif
    }

    bool Planet_Reference(const char* label, game::id_planet* planet_id, bool allow_none) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (label == nullptr || planet_id == nullptr) {
            return false;
        }
        char preview[64];
        if (*planet_id == game::NO_PLANET) {
            strlcpy(preview, "None", sizeof(preview));
        } else if (game::current_state.planets.contains(*planet_id)) {
            snprintf(preview, sizeof(preview), "Planet %u", planet_id->id);
        } else {
            strlcpy(preview, "Invalid", sizeof(preview));
        }
        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            if (allow_none && ImGui::Selectable("None", *planet_id == game::NO_PLANET)) {
                *planet_id = game::NO_PLANET;
                changed = true;
            }
            for (const auto id : game::current_state.planet_order) {
                char item[64];
                snprintf(item, sizeof(item), "Planet %u", id.id);
                if (ImGui::Selectable(item, *planet_id == id)) {
                    *planet_id = id;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
#else
        (void)label;
        (void)planet_id;
        (void)allow_none;
        return false;
#endif
    }

    bool Colony_Reference(const char* label, game::id_colony* colony_index, bool allow_none) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (label == nullptr || colony_index == nullptr) {
            return false;
        }
        char preview[48];
        if (*colony_index == game::NO_COLONY) {
            strlcpy(preview, "None", sizeof(preview));
        } else if (game::current_state.colonies.contains(*colony_index)) {
            snprintf(preview, sizeof(preview), "Colony %u (player %d)", colony_index->id, (int)game::current_state.colonies.at(*colony_index).owner);
        } else {
            strlcpy(preview, "Invalid", sizeof(preview));
        }
        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            if (allow_none && ImGui::Selectable("None", *colony_index == game::NO_COLONY)) {
                *colony_index = game::NO_COLONY;
                changed = true;
            }
            for (const auto& [id, colony] : game::current_state.colonies) {
                const auto index = id;
                char item[64];
                snprintf(item, sizeof(item), "Colony %u (player %d)", id.id, (int)colony.owner);
                if (ImGui::Selectable(item, *colony_index == index)) {
                    *colony_index = index;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
#else
        (void)label;
        (void)colony_index;
        (void)allow_none;
        return false;
#endif
    }

    bool Officer_Reference(const char* label, int8_t* officer_index, bool allow_none) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (label == nullptr || officer_index == nullptr) {
            return false;
        }
        const auto& state = game::current_state;
        const int16_t count = state.leaders.size();
        const char* preview = *officer_index < 0
            ? "None"
            : ((*officer_index < count)
                      ? Bounded_Name(state.leaders.at(*officer_index).name, sizeof(s_leader_data::name))
                      : "Invalid");
        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            if (allow_none && ImGui::Selectable("None", *officer_index < 0)) {
                *officer_index = -1;
                changed = true;
            }
            for (int16_t index = 0; index < count; ++index) {
                const char* name = Bounded_Name(state.leaders.at(index).name, sizeof(s_leader_data::name));
                if (ImGui::Selectable(name, *officer_index == index)) {
                    *officer_index = (int8_t)index;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
#else
        (void)label;
        (void)officer_index;
        (void)allow_none;
        return false;
#endif
    }

    bool Player_Bitmap(const char* id, uint8_t* bits, uint16_t column_count) {
        const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        return debug_widgets::Bit_Field_Checkboxes(
            id,
            bits,
            sizeof(*bits),
            (uint16_t)player_count,
            Player_Name,
            nullptr,
            column_count);
    }

    bool Ship_Specials(
        const char* id,
        void* special_bits,
        uint16_t byte_count,
        uint16_t column_count) {
        return debug_widgets::Bit_Field_Checkboxes(
            id,
            special_bits,
            byte_count,
            SPECIAL_COUNT,
            Special_Name,
            nullptr,
            column_count);
    }
} // namespace debug_game_widgets
