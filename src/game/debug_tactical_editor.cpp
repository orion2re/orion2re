#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

namespace debug_tactical_editor {
    namespace {
        int16_t g_selected_ship = 1;
        bool g_show_destroyed_ships = true;
        bool g_show_inactive_missiles = false;
        int16_t g_grid_x = 0;
        int16_t g_grid_y = 0;
        int16_t g_missile_field_index = 0;
        const char* g_status = nullptr;

        constexpr const char* k_armor_types[] = {
            "None", "Titanium", "Tritanium", "Zortrium", "Neutronium", "Adamantium", "Xentronium"};
        constexpr const char* k_computer_types[] = {
            "None", "Electronic", "Optronic", "Positronic", "Cybertronic", "Moleculartronic"};
        constexpr const char* k_crew_qualities[] = {
            "Green", "Regular", "Veteran", "Elite", "Ultra Elite"};
        constexpr const char* k_drive_types[] = {
            "None", "Nuclear", "Fusion", "Ion", "Anti-Matter", "Hyper", "Interphased"};
        constexpr const char* k_shield_types[] = {
            "None", "Class I", "Class III", "Class V", "Class VII", "Class X"};
        constexpr const char* k_ship_sizes[] = {
            "Small", "Medium", "Large", "Huge", "Titan", "Doom Star"};
        constexpr const char* k_target_types[] = {"Ship", "Missile"};
        constexpr const char* k_retreat_states[] = {
            "Not retreating", "Retreating", "Retreat completed"};
        constexpr const char* k_weapon_active_states[] = {"Automatic", "Off", "On"};
        constexpr const char* k_weapon_statuses[] = {"Normal", "Reloadable", "Transitional"};
        constexpr const char* k_missile_warning_states[] = {
            "None", "Incoming impact", "Suppressed by auto-combat"};
        constexpr const char* k_weapon_arc_bits[] = {
            "Forward", "Forward extended", "Back extended", "Rear", "360 degrees"};
        constexpr const char* k_weapon_mod_bits[] = {
            "Reserved",
            "Heavy mount",
            "Point defense",
            "Armor piercing",
            "Continuous",
            "No range dissipation",
            "Shield piercing",
            "Auto fire",
            "Enveloping",
            "MIRV",
            "ECCM",
            "Heavily armored",
            "Fast",
            "Emission guidance",
            "Overloaded"};
        constexpr const char* k_combat_status_bits[] = {
            "Teleporter used",
            "Boarded",
            "Combat turn done",
            "Unknown bit 3",
            "Capturing",
            "Unknown bit 5",
            "Unknown bit 6",
            "Unknown bit 7"};
        constexpr const char* k_colony_defense_bits[] = {
            "Missile base", "Ground batteries", "Stellar converter", "Fighter garrison"};

        const char* Weapon_Arc_Label_(uint16_t bit_index, void*) {
            return k_weapon_arc_bits[bit_index];
        }

        const char* Weapon_Mod_Label_(uint16_t bit_index, void*) {
            return k_weapon_mod_bits[bit_index];
        }

        const char* Combat_Status_Label_(uint16_t bit_index, void*) {
            return k_combat_status_bits[bit_index];
        }

        const char* Colony_Defense_Label_(uint16_t bit_index, void*) {
            return k_colony_defense_bits[bit_index];
        }

        template <typename T>
        bool Checkbox_(const char* label, T* value) {
            bool selected = *value != 0;
            if (!ImGui::Checkbox(label, &selected)) {
                return false;
            }
            *value = selected ? 1 : 0;
            return true;
        }

        void Format_Combat_Owner_(int16_t owner, char* label, size_t label_size) {
            const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            if (owner >= 0 && owner < player_count && MOX::player != nullptr) {
                snprintf(label, label_size, "%d: %.14s", (int)owner, MOX::player[owner].race_name);
                return;
            }
            const char* name = nullptr;
            switch (owner) {
                case NONPLAYER_SHIP_TYPE_ANTARAN: name = "Antaran"; break;
                case NONPLAYER_SHIP_TYPE_GUARDIAN: name = "Guardian"; break;
                case NONPLAYER_SHIP_TYPE_AMOEBA: name = "Amoeba"; break;
                case NONPLAYER_SHIP_TYPE_CRYSTAL: name = "Crystal"; break;
                case NONPLAYER_SHIP_TYPE_DRAGON: name = "Dragon"; break;
                case NONPLAYER_SHIP_TYPE_EEL: name = "Eel"; break;
                case NONPLAYER_SHIP_TYPE_HYDRA: name = "Hydra"; break;
                default: break;
            }
            if (name != nullptr) {
                snprintf(label, label_size, "%d: %s", (int)owner, name);
            } else if (owner >= 0 && owner < MAX_PLAYERS) {
                snprintf(label, label_size, "%d: Inactive player slot", (int)owner);
            } else {
                snprintf(label, label_size, "Invalid (%d)", (int)owner);
            }
        }

        template <typename T>
        bool Combat_Owner_(const char* label, T* owner) {
            char preview[48];
            Format_Combat_Owner_((int16_t)*owner, preview, sizeof(preview));
            bool changed = false;
            if (ImGui::BeginCombo(label, preview)) {
                for (int16_t index = 0; index <= NONPLAYER_SHIP_TYPE_HYDRA; ++index) {
                    char item[48];
                    Format_Combat_Owner_(index, item, sizeof(item));
                    if (ImGui::Selectable(item, (int16_t)*owner == index)) {
                        *owner = (T)index;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        void Format_Weapon_(int16_t index, char* text, size_t text_size) {
            const char* name = TECHDATA::weapons[index].name;
            if (name != nullptr && name[0] != '\0') {
                snprintf(text, text_size, "%d: %.48s", (int)index, name);
            } else {
                snprintf(text, text_size, "Weapon %d", (int)index);
            }
        }

        bool Weapon_(const char* label, int16_t* weapon_id) {
            char preview[64];
            if (*weapon_id >= 0 && *weapon_id < WEAPON_COUNT) {
                Format_Weapon_(*weapon_id, preview, sizeof(preview));
            } else {
                snprintf(preview, sizeof(preview), "Invalid (%d)", (int)*weapon_id);
            }
            bool changed = false;
            if (ImGui::BeginCombo(label, preview)) {
                for (int16_t index = 0; index < WEAPON_COUNT; ++index) {
                    char item[64];
                    Format_Weapon_(index, item, sizeof(item));
                    ImGui::PushID((int32_t)index);
                    if (ImGui::Selectable(item, *weapon_id == index)) {
                        *weapon_id = index;
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        bool Input_S8_(const char* label, int8_t* value) {
            const int8_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S8, value, &step, nullptr, "%d");
        }

        bool Input_U8_(const char* label, uint8_t* value) {
            const uint8_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_U8, value, &step, nullptr, "%u");
        }

        bool Input_S16_(const char* label, int16_t* value) {
            const int16_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S16, value, &step, nullptr, "%d");
        }

        bool Input_U16_(const char* label, uint16_t* value) {
            const uint16_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_U16, value, &step, nullptr, "%u");
        }

        bool Input_S32_(const char* label, int32_t* value) {
            const int32_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S32, value, &step, nullptr, "%d");
        }

        bool Tactical_Active_() {
            return fields::function == CMBTDRW1::Draw_Main_Combat_Screen &&
                MOX::combat_data != nullptr && MOX::missile != nullptr &&
                MOX::combat_total_ship_count > 0 &&
                MOX::combat_total_ship_count <= MAX_COMBAT_SHIPS;
        }

        bool Valid_Ship_(int16_t index) {
            return Tactical_Active_() && index >= 0 &&
                index < MOX::combat_total_ship_count;
        }

        bool Is_Attacker_(const s_combat_data& ship) {
            return ship.owner == MOX::combat_attacker_player;
        }

        void Recount_Sides_() {
            int16_t attackers = 0;
            int16_t defenders = 0;
            for (int16_t index = 1; index < MOX::combat_total_ship_count; ++index) {
                const s_combat_data& ship = MOX::combat_data[index];
                if (ship.unit_status != 0 || ship.is_captured != 0) {
                    continue;
                }
                if (Is_Attacker_(ship)) {
                    ++attackers;
                } else {
                    ++defenders;
                }
            }
            MOX::combat_attacker_count = attackers;
            MOX::combat_defender_count = defenders;
        }

        void Synchronize_Combat_State_() {
            Recount_Sides_();
            if (Valid_Ship_(MOX::cur_ship) &&
                MOX::combat_data[MOX::cur_ship].unit_status == 0 &&
                MOX::combat_data[MOX::cur_ship].is_captured == 0) {
                COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
            }
        }

        void Add_To_Turn_Order_(int16_t ship_index) {
            for (int16_t index = 0; index < MAX_COMBAT_SHIPS; ++index) {
                if (MOX::ship_turn_order[index] == ship_index) {
                    return;
                }
            }
            for (int16_t index = 0; index < MAX_COMBAT_SHIPS; ++index) {
                if (MOX::ship_turn_order[index] == -10) {
                    MOX::ship_turn_order[index] = ship_index;
                    if (index + 1 < MAX_COMBAT_SHIPS) {
                        MOX::ship_turn_order[index + 1] = -10;
                    }
                    return;
                }
                if (MOX::ship_turn_order[index] == -1) {
                    MOX::ship_turn_order[index] = ship_index;
                    return;
                }
            }
        }

        bool Ready_Ship_(int16_t ship_index, bool synchronize = true) {
            if (!Valid_Ship_(ship_index) ||
                MOX::combat_data[ship_index].is_captured != 0) {
                g_status = "Captured ships cannot be readied by the tactical editor.";
                return false;
            }
            s_combat_data& ship = MOX::combat_data[ship_index];
            if (ship.ftl_type != DRIVE_NO_DRIVE && ship.drive_max_hits == 0) {
                g_status = "Cannot recalculate a mobile ship with zero maximum drive hits.";
                return false;
            }
            ship.unit_status = 0;
            ship.is_retreating = 0;
            ship.movement_left = ship.current_speed;
            for (int16_t slot = 0; slot < 8; ++slot) {
                ship.weapons[slot].active = 1;
                ship.weapons[slot].shots_left = ship.weapons[slot].count;
                ship.weapons[slot].status = 0;
                ship.weapon_ready_flags[slot] = 1;
            }
            COMBAT1::Recalculate_Combat_Ship(ship_index);
            Add_To_Turn_Order_(ship_index);
            if (synchronize) {
                Synchronize_Combat_State_();
            }
            return true;
        }

        bool Repair_Ship_(int16_t ship_index, bool synchronize = true) {
            if (!Valid_Ship_(ship_index) ||
                MOX::combat_data[ship_index].is_captured != 0) {
                g_status = "Captured ships cannot be repaired by the tactical editor.";
                return false;
            }
            s_combat_data& ship = MOX::combat_data[ship_index];
            if (ship.ftl_type != DRIVE_NO_DRIVE && ship.drive_max_hits == 0) {
                g_status = "Cannot repair a mobile ship with zero maximum drive hits.";
                return false;
            }
            for (int16_t slot = 0; slot < 8; ++slot) {
                if (ship.weapon_hits_per_unit[slot] <= 0) {
                    ship.weapon_hits_per_unit[slot] = std::max<int16_t>(
                        ship.weapon_unit_hits[slot], 1);
                }
            }
            CMBTFIR2::Repair_Combat_Ship(
                ship_index, std::numeric_limits<int16_t>::max());
            ship.unit_status = 0;
            ship.structure_damage = 0;
            ship.shield_current_hits = ship.shield_max_hits;
            ship.drive_current_hits = ship.drive_max_hits;
            ship.computer_current_hits = ship.computer_max_hits;
            for (int16_t arc = 0; arc < 4; ++arc) {
                ship.shield_arc_current[arc] = ship.shield_arc_max;
            }
            memset(ship.special_device_damage_flags, 0, sizeof(ship.special_device_damage_flags));
            for (int16_t slot = 0; slot < 8; ++slot) {
                ship.weapon_hits[slot] = ship.weapon_total_hits[slot];
                ship.special_hits[slot] = ship.special_device_hits[slot];
                if (ship.weapon_hits_per_unit[slot] > 0) {
                    ship.weapons[slot].count = (uint8_t)std::clamp<int32_t>(
                        (ship.weapon_hits[slot] + ship.weapon_hits_per_unit[slot] - 1) /
                            ship.weapon_hits_per_unit[slot],
                        0,
                        std::numeric_limits<uint8_t>::max());
                }
            }
            return Ready_Ship_(ship_index, synchronize);
        }

        void Remove_Ship_(int16_t ship_index) {
            if (!Valid_Ship_(ship_index) || ship_index == 0) {
                return;
            }
            s_combat_data& removed = MOX::combat_data[ship_index];
            removed.unit_status = 5;
            removed.structure_damage = removed.structure_max;
            removed.movement_left = 0;
            CMBTFIRE::Retarget_Missiles_Aimed_At(ship_index);
            COMBAT1::Remove_Continuing_Effect_Specials(ship_index);
            for (int16_t index = 0; index < MAX_COMBAT_SHIPS; ++index) {
                if (MOX::ship_turn_order[index] == ship_index) {
                    MOX::ship_turn_order[index] = -1;
                }
            }
            if (MOX::cur_ship == ship_index) {
                int16_t replacement = -1;
                for (int16_t index = 1; index < MOX::combat_total_ship_count; ++index) {
                    if (MOX::combat_data[index].unit_status == 0 &&
                        MOX::combat_data[index].is_captured == 0) {
                        replacement = index;
                        break;
                    }
                }
                if (replacement >= 0) {
                    Add_To_Turn_Order_(replacement);
                    for (int16_t index = 0; index < MAX_COMBAT_SHIPS; ++index) {
                        if (MOX::ship_turn_order[index] == replacement) {
                            MOX::cur_ptr = index;
                            MOX::cur_ship = replacement;
                            break;
                        }
                    }
                } else {
                    MOX::cur_ptr = -1;
                    MOX::cur_ship = -1;
                }
            }
            Synchronize_Combat_State_();
            g_status = "Ship removed from active combat. Its fixed combat slot is retained.";
        }

        int16_t Find_Side_Template_(int8_t owner) {
            for (int16_t index = 1; index < MOX::combat_total_ship_count; ++index) {
                if (MOX::combat_data[index].owner == owner &&
                    MOX::combat_data[index].unit_status == 0 &&
                    MOX::combat_data[index].is_captured == 0 &&
                    MOX::combat_data[index].ftl_type != DRIVE_NO_DRIVE &&
                    MOX::combat_data[index].drive_max_hits > 0) {
                    return index;
                }
            }
            for (int16_t index = 1; index < MOX::combat_total_ship_count; ++index) {
                if (MOX::combat_data[index].unit_status == 0 &&
                    MOX::combat_data[index].is_captured == 0 &&
                    MOX::combat_data[index].ftl_type != DRIVE_NO_DRIVE &&
                    MOX::combat_data[index].drive_max_hits > 0) {
                    return index;
                }
            }
            return -1;
        }

        int16_t Clone_Ship_(int16_t source_index, int8_t owner) {
            if (!Valid_Ship_(source_index) || source_index == 0 ||
                MOX::combat_data[source_index].ftl_type == DRIVE_NO_DRIVE ||
                MOX::combat_data[source_index].drive_max_hits == 0 ||
                MOX::combat_total_ship_count >= MAX_COMBAT_SHIPS - 1) {
                g_status = "No mobile ship template is available, or combat capacity was reached.";
                return -1;
            }
            const int16_t new_index = MOX::combat_total_ship_count;
            s_combat_data& clone = MOX::combat_data[new_index];
            clone = MOX::combat_data[source_index];
            clone.ship_idx = -1;
            clone.owner = owner;
            clone.previous_owner = 0;
            clone.unit_status = 0;
            clone.is_captured = 0;
            clone.is_retreating = 0;
            const int16_t footprint = clone.size_class >= SHIP_SIZE_HUGE ? 3 : 2;
            clone.x = std::clamp<int16_t>(
                clone.x + 2, 0, MOX::combat_grid_width - footprint);
            clone.y = std::clamp<int16_t>(
                clone.y + 1, 0, MOX::combat_grid_height - footprint);
            clone.special_status_flag = 0;
            clone.special_status_timer = 0;
            clone.special_cooldown = COMBAT_SHIP_NONE;
            clone.stasis_source_idx = COMBAT_SHIP_NONE;
            clone.black_hole_flag = 0;
            clone.black_hole_source_idx = COMBAT_SHIP_NONE;
            clone.plasma_web_damage = 0;
            clone.reflected_damage_pool = 0;
            char base_name[sizeof(clone.name)];
            strlcpy(base_name, clone.name, sizeof(base_name));
            snprintf(clone.name, sizeof(clone.name), "%.24s copy", base_name);
            ++MOX::combat_total_ship_count;
            Repair_Ship_(new_index);
            g_selected_ship = new_index;
            g_status = "Tactical-only clone added. It will not be written to the strategic fleet.";
            return new_index;
        }

        void Add_Side_Ship_(int8_t owner) {
            const int16_t source = Find_Side_Template_(owner);
            if (source < 0) {
                g_status = "No active mobile ship is available as a template.";
                return;
            }
            Clone_Ship_(source, owner);
        }

        void Remove_Last_Side_Ship_(bool attacker) {
            for (int16_t index = MOX::combat_total_ship_count - 1; index > 0; --index) {
                const s_combat_data& ship = MOX::combat_data[index];
                if (ship.unit_status == 0 && Is_Attacker_(ship) == attacker) {
                    Remove_Ship_(index);
                    g_selected_ship = std::clamp<int16_t>(
                        g_selected_ship, 0, MOX::combat_total_ship_count - 1);
                    return;
                }
            }
            g_status = "That side has no active ship to remove.";
        }

        void Draw_S16_Array_(const char* id, int16_t* values, int16_t count) {
            ImGui::PushID(id);
            for (int16_t index = 0; index < count; ++index) {
                ImGui::PushID((int32_t)index);
                char label[16];
                snprintf(label, sizeof(label), "[%d]", (int)index);
                Input_S16_(label, &values[index]);
                ImGui::PopID();
            }
            ImGui::PopID();
        }

        void Draw_U8_Array_(const char* id, uint8_t* values, int16_t count) {
            ImGui::PushID(id);
            for (int16_t index = 0; index < count; ++index) {
                ImGui::PushID((int32_t)index);
                char label[16];
                snprintf(label, sizeof(label), "[%d]", (int)index);
                Input_U8_(label, &values[index]);
                ImGui::PopID();
            }
            ImGui::PopID();
        }

        void Draw_Combat_Globals_() {
            ImGui::Text("Total fixed slots: %d / %d", (int)MOX::combat_total_ship_count, (int)MAX_COMBAT_SHIPS);
            ImGui::Text("Active attackers: %d", (int)MOX::combat_attacker_count);
            ImGui::Text("Active defenders: %d", (int)MOX::combat_defender_count);
            Combat_Owner_("Attacker player", &MOX::combat_attacker_player);
            Combat_Owner_("Defender player", &MOX::combat_defender_player);
            MOX::combat_attacker_player = std::clamp<int16_t>(
                MOX::combat_attacker_player, 0, NONPLAYER_SHIP_TYPE_HYDRA);
            MOX::combat_defender_player = std::clamp<int16_t>(
                MOX::combat_defender_player, 0, NONPLAYER_SHIP_TYPE_HYDRA);
            Input_S16_("Combat turn", &MOX::combat_turn);
            debug_game_widgets::Colony_Reference("Colony", &MOX::cmbt_colony_id);
            MOX::cmbt_colony_id = std::clamp<int16_t>(
                MOX::cmbt_colony_id, -1, std::max<int16_t>(MOX::NUM_COLONIES - 1, -1));
            Input_S16_("Current ship", &MOX::cur_ship);
            MOX::cur_ship = std::clamp<int16_t>(
                MOX::cur_ship, -1, MOX::combat_total_ship_count - 1);
            Input_S16_("Current turn-order pointer", &MOX::cur_ptr);
            MOX::cur_ptr = std::clamp<int16_t>(
                MOX::cur_ptr, -1, MAX_COMBAT_SHIPS - 1);
            Input_S16_("Viewport X", &MOX::cur_combat_x);
            Input_S16_("Viewport Y", &MOX::cur_combat_y);
            MOX::cur_combat_x = std::clamp<int16_t>(
                MOX::cur_combat_x, 0, MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH);
            MOX::cur_combat_y = std::clamp<int16_t>(
                MOX::cur_combat_y, 0, MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT);
            Checkbox_("Combat initialized", &MOX::combat_init);
            Input_U8_("Combat done waiting", &MOX::combat_done_waiting);
            ImGui::TextUnformatted("Colony defenses");
            debug_widgets::Bit_Field_Checkboxes(
                "Colony defense flags",
                &MOX::colony_defense_flags,
                sizeof(MOX::colony_defense_flags),
                4,
                Colony_Defense_Label_,
                nullptr,
                2);
            Checkbox_("Apply damage to planet", &MOX::apply_damage_to_planet);
            int8_t missile_warning = MOX::missile_warning < 3
                ? (int8_t)MOX::missile_warning
                : std::numeric_limits<int8_t>::max();
            if (debug_game_widgets::Enum(
                    "Missile warning", &missile_warning, k_missile_warning_states, 3)) {
                MOX::missile_warning = (uint16_t)missile_warning;
            }
            Checkbox_("Saved missile warning", &MOX::saved_msl_warning);
            Checkbox_("Show grid", &MOX::grid_flag);
            Checkbox_("Double speed", &MOX::speedx2_flag);
            Input_S16_("Combat clock frame", &MOX::combat_clock_frame);
            Checkbox_("Auto combat attacker", &MOX::auto_combat_flag[0]);
            Checkbox_("Auto combat defender", &MOX::auto_combat_flag[1]);
            if (ImGui::Button("Recount sides and rebuild grid")) {
                Synchronize_Combat_State_();
                g_status = "Combat counts and grids refreshed.";
            }
        }

        void Draw_Weapon_(s_combat_weapon_slot& weapon, int16_t slot) {
            ImGui::PushID((int32_t)slot);
            char label[24];
            snprintf(label, sizeof(label), "Slot %d", (int)slot + 1);
            if (ImGui::TreeNode(label)) {
                Weapon_("Weapon", &weapon.weapon_id);
                weapon.weapon_id = std::clamp<int16_t>(
                    weapon.weapon_id, 0, WEAPON_COUNT - 1);
                Input_U8_("Count", &weapon.count);
                ImGui::TextUnformatted("Firing arcs");
                debug_widgets::Bit_Field_Checkboxes(
                    "Arc",
                    &weapon.arc,
                    sizeof(weapon.arc),
                    5,
                    Weapon_Arc_Label_,
                    nullptr,
                    2);
                ImGui::TextUnformatted("Modifications");
                debug_widgets::Bit_Field_Checkboxes(
                    "Specials",
                    &weapon.specials,
                    sizeof(weapon.specials),
                    WEAPON_MOD_COUNT,
                    Weapon_Mod_Label_,
                    nullptr,
                    2);
                Input_S16_("Ammo", &weapon.ammo);
                debug_game_widgets::Enum(
                    "Active state", &weapon.active, k_weapon_active_states, 3, -1);
                Input_U8_("Shots left", &weapon.shots_left);
                int8_t status = weapon.status < 3
                    ? (int8_t)weapon.status
                    : std::numeric_limits<int8_t>::max();
                if (debug_game_widgets::Enum(
                        "Status", &status, k_weapon_statuses, 3)) {
                    weapon.status = (uint8_t)status;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        void Draw_Ship_Form_(s_combat_data& ship) {
            ImGui::InputText("Name", ship.name, sizeof(ship.name));
            Input_S16_("Strategic ship index", &ship.ship_idx);
            Combat_Owner_("Owner", &ship.owner);
            Combat_Owner_("Previous owner", &ship.previous_owner);
            Input_S8_("Unit status", &ship.unit_status);
            debug_game_widgets::Enum(
                "Size class", &ship.size_class, k_ship_sizes, SHIP_SIZE_COUNT);
            Input_S16_("Picture", &ship.picture_num);
            ship.ship_idx = std::clamp<int16_t>(
                ship.ship_idx, -1, std::max<int16_t>(MOX::NUM_SHIPS - 1, -1));
            ship.owner = (int8_t)std::clamp<int16_t>(
                ship.owner, 0, NONPLAYER_SHIP_TYPE_HYDRA);
            ship.previous_owner = (uint8_t)std::clamp<int16_t>(
                ship.previous_owner, 0, NONPLAYER_SHIP_TYPE_HYDRA);
            ship.size_class = (int8_t)std::clamp<int16_t>(
                ship.size_class, 0, SHIP_SIZE_COUNT - 1);
            ship.picture_num = std::clamp<int16_t>(ship.picture_num, 0, 47);
            ImGui::Text("Image pointer: %p", (void*)ship.ship_image_seg);

            if (ImGui::CollapsingHeader("Position and movement", ImGuiTreeNodeFlags_DefaultOpen)) {
                Input_S16_("X", &ship.x);
                Input_S16_("Y", &ship.y);
                const int16_t footprint = ship.size_class >= SHIP_SIZE_HUGE ? 3 : 2;
                ship.x = std::clamp<int16_t>(
                    ship.x, 0, MOX::combat_grid_width - footprint);
                ship.y = std::clamp<int16_t>(
                    ship.y, 0, MOX::combat_grid_height - footprint);
                Input_S8_("Facing direction", &ship.facing_dir);
                ship.facing_dir = (int8_t)std::clamp<int16_t>(ship.facing_dir, 0, 15);
                Input_S8_("Base combat speed", &ship.combat_speed_base);
                Input_S16_("Current speed", &ship.current_speed);
                Input_S16_("Movement left", &ship.movement_left);
                debug_game_widgets::Enum(
                    "Retreat state", &ship.is_retreating, k_retreat_states, 3);
                debug_game_widgets::Enum(
                    "Target type", &ship.target_type_flag, k_target_types, 2);
                Input_S8_("AI range tactic", &ship.ai_range_tactic);
            }

            if (ImGui::CollapsingHeader("Systems and tactical values")) {
                debug_game_widgets::Enum(
                    "Shield type", &ship.shield_type, k_shield_types, SHIELD_COUNT);
                debug_game_widgets::Enum(
                    "FTL drive", &ship.ftl_type, k_drive_types, DRIVE_COUNT);
                debug_game_widgets::Enum(
                    "Computer", &ship.computer_type, k_computer_types, COMPUTER_COUNT);
                debug_game_widgets::Enum(
                    "Armor", &ship.armor_type, k_armor_types, ARMOR_COUNT);
                ship.shield_type = (int8_t)std::clamp<int16_t>(
                    ship.shield_type, 0, SHIELD_COUNT - 1);
                ship.ftl_type = (int8_t)std::clamp<int16_t>(
                    ship.ftl_type, 0, DRIVE_COUNT - 1);
                ship.computer_type = (int8_t)std::clamp<int16_t>(
                    ship.computer_type, 0, COMPUTER_COUNT - 1);
                ship.armor_type = (int8_t)std::clamp<int16_t>(
                    ship.armor_type, 0, ARMOR_COUNT - 1);
                Input_S16_("Tactical attack", &ship.tac_val_attack);
                Input_S16_("Tactical defense", &ship.tac_val_defense);
                Input_S16_("Tactical evade", &ship.tac_val_evade);
                debug_game_widgets::Enum(
                    "Crew quality", &ship.crew_quality, k_crew_qualities, CREW_QUALITY_COUNT);
                int8_t officer_index = (int8_t)std::clamp<int16_t>(
                    ship.officer_idx, -1, MAX_LEADERS - 1);
                if (debug_game_widgets::Officer_Reference("Officer", &officer_index)) {
                    ship.officer_idx = officer_index;
                }
                ship.crew_quality = (int8_t)std::clamp<int16_t>(
                    ship.crew_quality, 0, CREW_QUALITY_COUNT - 1);
                ship.officer_idx = std::clamp<int16_t>(
                    ship.officer_idx, -1, MAX_LEADERS - 1);
                Input_U8_("Marines", &ship.marine_count);
                Checkbox_("Captured", &ship.is_captured);
            }

            if (ImGui::CollapsingHeader("Shields, armor and structure")) {
                Input_S16_("Structure maximum", &ship.structure_max);
                Input_S16_("Structure damage", &ship.structure_damage);
                Input_S16_("Armor remaining", &ship.armor_remaining);
                Input_S16_("Shield arc maximum", &ship.shield_arc_max);
                Draw_S16_Array_("Shield arcs", ship.shield_arc_current, 4);
                Input_U16_("Shield maximum hits", &ship.shield_max_hits);
                Input_U16_("Shield current hits", &ship.shield_current_hits);
            }

            if (ImGui::CollapsingHeader("Status and special effects")) {
                Input_S8_("Special status flag", &ship.special_status_flag);
                Input_U8_("Special status timer", &ship.special_status_timer);
                Input_U16_("Special cooldown/source", &ship.special_cooldown);
                Input_S16_("Plasma web damage", &ship.plasma_web_damage);
                Input_S16_("Reflected damage pool", &ship.reflected_damage_pool);
                Input_S8_("Unknown field 0x47", &ship.field40_0x47);
                Input_U8_("Unknown field 0x49", &ship.field42_0x49);
                Input_U16_("Stasis source", &ship.stasis_source_idx);
                Input_U8_("Unknown field 0x51", &ship.field46_0x51);
                Checkbox_("In black hole", &ship.black_hole_flag);
                Input_U16_("Black hole source", &ship.black_hole_source_idx);
                ImGui::TextUnformatted("Combat status");
                debug_widgets::Bit_Field_Checkboxes(
                    "Combat status flags",
                    &ship.combat_status_flags,
                    sizeof(ship.combat_status_flags),
                    8,
                    Combat_Status_Label_,
                    nullptr,
                    2);
                if (ImGui::TreeNode("Special device flags")) {
                    debug_game_widgets::Ship_Specials(
                        "Installed special devices",
                        ship.special_device_flags,
                        sizeof(ship.special_device_flags),
                        2);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Damaged special device flags")) {
                    debug_game_widgets::Ship_Specials("Damaged special devices",
                        ship.special_device_damage_flags,
                        sizeof(ship.special_device_damage_flags),
                        2);
                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Weapons")) {
                for (int16_t slot = 0; slot < 8; ++slot) {
                    Draw_Weapon_(ship.weapons[slot], slot);
                }
                if (ImGui::TreeNode("Weapon ready flags")) {
                    ImGui::PushID("Ready flags");
                    for (int16_t slot = 0; slot < 8; ++slot) {
                        char label[24];
                        snprintf(label, sizeof(label), "Slot %d", (int)slot + 1);
                        Checkbox_(label, &ship.weapon_ready_flags[slot]);
                    }
                    ImGui::PopID();
                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Component hit state")) {
                Input_U16_("Drive maximum hits", &ship.drive_max_hits);
                Input_U16_("Drive current hits", &ship.drive_current_hits);
                Input_U16_("Computer maximum hits", &ship.computer_max_hits);
                Input_U16_("Computer current hits", &ship.computer_current_hits);
                if (ImGui::TreeNode("Weapon total hits")) {
                    Draw_S16_Array_("Weapon total hits", ship.weapon_total_hits, 8);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Weapon unit hits")) {
                    Draw_S16_Array_("Weapon unit hits", ship.weapon_unit_hits, 8);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Weapon current hits")) {
                    Draw_S16_Array_("Weapon current hits", ship.weapon_hits, 8);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Weapon hits per unit")) {
                    Draw_S16_Array_("Weapon hits per unit", ship.weapon_hits_per_unit, 8);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Special device maximum hits")) {
                    Draw_S16_Array_("Special device maximum hits",
                        ship.special_device_hits,
                        8);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Special device current hits")) {
                    Draw_S16_Array_("Special device current hits", ship.special_hits, 8);
                    ImGui::TreePop();
                }
                Input_U8_("Unknown field 0xb7", &ship.field55_0xb7);
            }
        }

        void Draw_Ship_Actions_(int16_t ship_index) {
            ImGui::BeginDisabled(ship_index == 0);
            if (ImGui::Button("Repair")) {
                if (Repair_Ship_(ship_index)) {
                    g_status = "Ship fully repaired and readied.";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Ready")) {
                if (Ready_Ship_(ship_index)) {
                    g_status = "Movement and weapons readied.";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clone")) {
                Clone_Ship_(ship_index, MOX::combat_data[ship_index].owner);
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                Remove_Ship_(ship_index);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Make current")) {
                const s_combat_data& ship = MOX::combat_data[ship_index];
                if (ship_index == 0 || ship.unit_status != 0 || ship.is_captured != 0) {
                    g_status = "Only an active, uncaptured ship can be made current.";
                } else {
                    Add_To_Turn_Order_(ship_index);
                    for (int16_t index = 0; index < MAX_COMBAT_SHIPS; ++index) {
                        if (MOX::ship_turn_order[index] == ship_index) {
                            MOX::cur_ptr = index;
                            MOX::cur_ship = ship_index;
                            Synchronize_Combat_State_();
                            g_status = "Current tactical unit changed.";
                            break;
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Recalculate")) {
                const s_combat_data& ship = MOX::combat_data[ship_index];
                if (ship.ftl_type != DRIVE_NO_DRIVE && ship.drive_max_hits == 0) {
                    g_status = "Cannot recalculate a mobile ship with zero maximum drive hits.";
                } else {
                    COMBAT1::Recalculate_Combat_Ship(ship_index);
                    Synchronize_Combat_State_();
                    g_status = "Derived combat values recalculated.";
                }
            }
        }

        void Draw_Side_List_(const char* label, bool attacker) {
            if (!ImGui::BeginChild(label, ImVec2(0.0f, 180.0f), true)) {
                ImGui::EndChild();
                return;
            }
            ImGui::Text("%s", label);
            for (int16_t index = 1; index < MOX::combat_total_ship_count; ++index) {
                const s_combat_data& ship = MOX::combat_data[index];
                if (Is_Attacker_(ship) != attacker ||
                    (!g_show_destroyed_ships && ship.unit_status != 0)) {
                    continue;
                }
                char item_label[80];
                snprintf(item_label, sizeof(item_label), "%d: %s%s", (int)index, ship.name, ship.unit_status == 0 ? "" : " (inactive)");
                if (ImGui::Selectable(item_label, g_selected_ship == index)) {
                    g_selected_ship = index;
                }
            }
            ImGui::EndChild();
        }

        void Draw_Ships_() {
            ImGui::Checkbox("Show destroyed/inactive slots", &g_show_destroyed_ships);
            if (ImGui::Button("Add attacker ship")) {
                Add_Side_Ship_((int8_t)MOX::combat_attacker_player);
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove last attacker")) {
                Remove_Last_Side_Ship_(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Repair all attackers")) {
                for (int16_t index = 1; index < MOX::combat_total_ship_count; ++index) {
                    if (Is_Attacker_(MOX::combat_data[index]) &&
                        MOX::combat_data[index].is_captured == 0) {
                        Repair_Ship_(index, false);
                    }
                }
                Synchronize_Combat_State_();
                g_status = "All attacker ships repaired.";
            }
            if (ImGui::Button("Add defender ship")) {
                Add_Side_Ship_((int8_t)MOX::combat_defender_player);
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove last defender")) {
                Remove_Last_Side_Ship_(false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Repair all defenders")) {
                for (int16_t index = 1; index < MOX::combat_total_ship_count; ++index) {
                    if (!Is_Attacker_(MOX::combat_data[index]) &&
                        MOX::combat_data[index].is_captured == 0) {
                        Repair_Ship_(index, false);
                    }
                }
                Synchronize_Combat_State_();
                g_status = "All defender ships repaired.";
            }

            if (ImGui::BeginTable("side_lists", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextColumn();
                Draw_Side_List_("Attackers", true);
                ImGui::TableNextColumn();
                Draw_Side_List_("Defenders", false);
                ImGui::EndTable();
            }

            g_selected_ship = std::clamp<int16_t>(
                g_selected_ship, 0, MOX::combat_total_ship_count - 1);
            ImGui::Separator();
            ImGui::Text("Editing fixed combat slot %d", (int)g_selected_ship);
            Draw_Ship_Actions_(g_selected_ship);
            Draw_Ship_Form_(MOX::combat_data[g_selected_ship]);
        }

        void Draw_Missile_(s_missile& missile, int16_t index) {
            ImGui::PushID((int32_t)index);
            char label[64];
            snprintf(label, sizeof(label), "%d: type %d, source %d, target %d", (int)index, (int)missile.type, (int)missile.source_unit_idx, (int)missile.target_unit_idx);
            if (ImGui::TreeNode(label)) {
                Weapon_("Type", &missile.type);
                Combat_Owner_("Owner", &missile.owner);
                Input_S16_("Source unit", &missile.source_unit_idx);
                Input_S16_("Target unit/missile", &missile.target_unit_idx);
                Checkbox_("Target is missile", &missile.target_is_missile);
                missile.type = std::clamp<int16_t>(missile.type, 0, WEAPON_COUNT - 1);
                missile.owner = std::clamp<int16_t>(
                    missile.owner, 0, NONPLAYER_SHIP_TYPE_HYDRA);
                missile.source_unit_idx = std::clamp<int16_t>(
                    missile.source_unit_idx, 0, MOX::combat_total_ship_count - 1);
                missile.target_is_missile = missile.target_is_missile != 0 ? 1 : 0;
                missile.target_unit_idx = std::clamp<int16_t>(
                    missile.target_unit_idx,
                    0,
                    missile.target_is_missile != 0
                        ? MAX_COMBAT_FLYERS - 1
                        : MOX::combat_total_ship_count - 1);
                Input_S16_("X", &missile.x);
                Input_S16_("Y", &missile.y);
                Input_S16_("Hits taken", &missile.hits_taken);
                Input_S16_("Quantity", &missile.quantity);
                ImGui::TextUnformatted("Modifications");
                debug_widgets::Bit_Field_Checkboxes(
                    "Specials",
                    &missile.specials,
                    sizeof(missile.specials),
                    WEAPON_MOD_COUNT,
                    Weapon_Mod_Label_,
                    nullptr,
                    2);
                Input_S16_("Travel distance", &missile.travel_dist);
                Input_S8_("Facing", &missile.facing_dir);
                Input_U8_("Speed", &missile.speed);
                Input_S8_("Fighter count", &missile.fighter_count);
                Input_U8_("Unknown field 0x18", &missile.field14_0x18);
                Checkbox_("Active", &missile.is_active);
                if (ImGui::Button("Deactivate")) {
                    missile.is_active = 0;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        void Draw_Missiles_() {
            ImGui::Text("Reported current missile count: %d", (int)MOX::cur_missile_count);
            ImGui::Checkbox("Show inactive missile slots", &g_show_inactive_missiles);
            if (MOX::missile == nullptr) {
                ImGui::TextUnformatted("Missile storage is not allocated.");
                return;
            }
            for (int16_t index = 0; index < MAX_COMBAT_FLYERS; ++index) {
                if (g_show_inactive_missiles || MOX::missile[index].is_active != 0) {
                    Draw_Missile_(MOX::missile[index], index);
                }
            }
        }

        void Draw_Turn_Order_And_Grid_() {
            if (ImGui::CollapsingHeader("Turn order", ImGuiTreeNodeFlags_DefaultOpen)) {
                Draw_S16_Array_("Turn order", MOX::ship_turn_order, MAX_COMBAT_SHIPS);
            }
            if (ImGui::CollapsingHeader("Combat grid cell", ImGuiTreeNodeFlags_DefaultOpen)) {
                Input_S16_("Grid X", &g_grid_x);
                Input_S16_("Grid Y", &g_grid_y);
                g_grid_x = std::clamp<int16_t>(g_grid_x, 0, MOX::combat_grid_width - 1);
                g_grid_y = std::clamp<int16_t>(g_grid_y, 0, MOX::combat_grid_height - 1);
                Input_S16_("Grid value", &MOX::combat_grid[g_grid_x][g_grid_y]);
            }
            if (ImGui::CollapsingHeader("Missile field cell")) {
                Input_S16_("Missile field index", &g_missile_field_index);
                g_missile_field_index = std::clamp<int16_t>(
                    g_missile_field_index, 0, MAX_COMBAT_FLYERS - 1);
                Input_S16_("Missile field value",
                    &MOX::missile_field[g_missile_field_index]);
            }
            if (ImGui::CollapsingHeader("Network tactical message")) {
                Input_S16_("Message viewport X", &MOX::new_message.viewport_x);
                Input_S16_("Message viewport Y", &MOX::new_message.viewport_y);
                Input_S32_("Message random seed", &MOX::new_message.random_seed);
                Input_S16_("Message code", &MOX::new_message.message_code);
                Input_S16_("Message argument 1", &MOX::new_message.arg1);
                Input_S16_("Message argument 2", &MOX::new_message.arg2);
                Input_S16_("Message argument 3", &MOX::new_message.arg3);
            }
        }

        void Draw_(const debug_overlay::s_panel_context& context) {
            (void)context;
            if (!Tactical_Active_()) {
                return;
            }
            const float width = std::min(316.0f, ImGui::GetIO().DisplaySize.x - 16.0f);
            const float height = std::min(464.0f, ImGui::GetIO().DisplaySize.y - 16.0f);
            ImGui::SetNextWindowPos(
                ImVec2(ImGui::GetIO().DisplaySize.x - width - 8.0f, 8.0f),
                ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowContentSize(ImVec2(580.0f, 0.0f));
            if (ImGui::Begin(
                    "Tactical Combat Overview", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {
                if (ImGui::BeginTabBar("tactical_debug_tabs")) {
                    if (ImGui::BeginTabItem("Combat")) {
                        Draw_Combat_Globals_();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Ships")) {
                        Draw_Ships_();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Missiles")) {
                        Draw_Missiles_();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Order and grid")) {
                        Draw_Turn_Order_And_Grid_();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                if (g_status != nullptr) {
                    ImGui::Separator();
                    ImGui::TextWrapped("%s", g_status);
                }
            }
            ImGui::End();
        }

        debug_overlay::s_screen_panel_registration g_registration(
            debug_overlay::k_all_screens, Draw_);
    } // namespace
} // namespace debug_tactical_editor
#endif