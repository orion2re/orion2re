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
            if (bit_index >= (uint16_t)std::ssize(game::current_state.stars)) {
                return nullptr;
            }
            return Bounded_Name(game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, bit_index)).name, sizeof(game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, bit_index)).name));
        }

        const char* Player_Name(uint16_t bit_index, void* user_data) {
            (void)user_data;
            if (bit_index >= (uint16_t)game::current_state.player_count) {
                return nullptr;
            }
            return Bounded_Name(
                game::current_state.players[bit_index].race_name,
                sizeof(game::current_state.players[bit_index].race_name));
        }

        const char* Special_Name(uint16_t bit_index, void* user_data) {
            (void)user_data;
            if (bit_index >= SPECIAL_COUNT) {
                return nullptr;
            }

            return TECHDATA::specials[bit_index].name;
        }
    } // namespace

    bool Player_Bit(const char* label, game::player_bitmap* bits, int16_t slot) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (!label || !bits || slot < 0 || slot >= MAX_PLAYERS) return false;
        bool selected = bits->test_slot(slot);
        if (!ImGui::Checkbox(label, &selected)) return false;
        bits->set_slot(slot, selected);
        return true;
#else
        (void)label;
        (void)bits;
        (void)slot;
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
        const int16_t star_count = std::clamp<int16_t>(std::ssize(game::current_state.stars), 0, MAX_STARS);
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
        const auto count = std::clamp<int16_t>(game::current_state.player_count, 0, MAX_PLAYERS);
        const char* preview = "Invalid";
        if (*player_index == -2 && allow_contested) {
            preview = "Contested";
        } else if (*player_index < 0) {
            preview = "None";
        } else if (*player_index < count) {
            preview = Bounded_Name(
                game::current_state.players[*player_index].race_name,
                sizeof(game::current_state.players[*player_index].race_name));
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
                    game::current_state.players[index].race_name,
                    sizeof(game::current_state.players[index].race_name));
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

    bool Star_Reference(const game::s_game_state& state, const char* label, game::id_star* star_id, bool allow_none) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (label == nullptr || star_id == nullptr) {
            return false;
        }
        const auto current = state.stars.find(*star_id);
        const char* preview = *star_id == game::NO_STAR ? "None" : current == state.stars.end() ? "Invalid"
            : Bounded_Name(current->second.name, sizeof(current->second.name));
        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            if (allow_none && ImGui::Selectable("None", *star_id == game::NO_STAR)) {
                *star_id = game::NO_STAR;
                changed = true;
            }
            for (const auto key : state.star_order) {
                const auto& star = state.stars.at(key);
                ImGui::PushID(&star);
                const bool selected = *star_id == key;
                const char* name = Bounded_Name(star.name, sizeof(star.name));
                if (ImGui::Selectable(name, selected)) {
                    *star_id = key;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        return changed;
#else
        (void)label;
        (void)state;
        (void)star_id;
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
            snprintf(preview, sizeof(preview), "Colony %u (player %d)", colony_index->id, (int)legacy_world::Encode_Player_Slot(game::current_state.colonies.at(*colony_index).owner));
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
                snprintf(item, sizeof(item), "Colony %u (player %d)", id.id, (int)legacy_world::Encode_Player_Slot(colony.owner));
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

    bool Officer_Reference(const char* label, game::id_leader* officer, bool allow_none) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (label == nullptr || officer == nullptr) {
            return false;
        }
        const auto& state = game::current_state;
        const int16_t count = state.leaders.size();
        const char* preview = *officer == game::NO_LEADER
            ? "None"
            : ((officer->id < state.leaders.size())
                      ? Bounded_Name(state.leaders.at(officer->id).name, sizeof(s_leader_data::name))
                      : "Invalid");
        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            if (allow_none && ImGui::Selectable("None", *officer == game::NO_LEADER)) {
                *officer = game::NO_LEADER;
                changed = true;
            }
            for (int16_t index = 0; index < count; ++index) {
                const auto& entry = state.leaders.at(index);
                const char* name = Bounded_Name(entry.name, sizeof(s_leader_data::name));
                if (ImGui::Selectable(name, *officer == entry.id)) {
                    *officer = entry.id;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
#else
        (void)label;
        (void)officer;
        (void)allow_none;
        return false;
#endif
    }

    bool Player_Bitmap(const char* id, game::player_bitmap* bits, uint16_t column_count) {
#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        if (!id || !bits || column_count == 0) return false;
        bool changed = false;
        const auto count = std::clamp<int16_t>(game::current_state.player_count, 0, MAX_PLAYERS);
        ImGui::TextUnformatted(id);
        if (ImGui::BeginTable(id, column_count)) {
            for (int16_t slot = 0; slot < count; ++slot) {
                ImGui::TableNextColumn();
                ImGui::PushID(slot);
                changed |= Player_Bit(Player_Name(slot, nullptr), bits, slot);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        return changed;
#else
        (void)id;
        (void)bits;
        (void)column_count;
        return false;
#endif
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
