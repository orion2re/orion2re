#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

#include "config/config_runtime.h"
#include "config/text_encoding.h"

namespace CONFIG_DEBUG {
    namespace {
        struct s_tree_node {
            std::string label;
            std::string search_text;
            std::vector<uint32_t> children;
            config::t_variable_id variable_id = config::CONFIG_NO_VARIABLE;
        };

        config::t_variable_id g_selected_variable = config::CONFIG_NO_VARIABLE;
        std::vector<s_tree_node> g_tree;

        const char* Value_Type_Name_(config::e_value_type type) {
            switch (type) {
            case config::CONFIG_VALUE_BOOL: return "bool";
            case config::CONFIG_VALUE_INT8: return "int8";
            case config::CONFIG_VALUE_UINT8: return "uint8";
            case config::CONFIG_VALUE_INT16: return "int16";
            case config::CONFIG_VALUE_UINT16: return "uint16";
            case config::CONFIG_VALUE_INT32: return "int32";
            case config::CONFIG_VALUE_UINT32: return "uint32";
            case config::CONFIG_VALUE_STRING: return "string";
            case config::CONFIG_VALUE_BITMAP: return "bitmap";
            case config::CONFIG_VALUE_RATIO: return "ratio";
            }
            return "unknown";
        }

        std::string Integer_Value_(const config::s_type_descriptor* type,
                                   int64_t value) {
            if (type != nullptr && type->scalar_kind == config::CONFIG_SCALAR_ENUM) {
                for (size_t index = 0; index < type->value_count; ++index) {
                    if (type->values[index].value == value) {
                        return std::string(type->values[index].name) + " (" +
                               std::to_string(value) + ")";
                    }
                }
            }
            if (type != nullptr && type->scalar_kind == config::CONFIG_SCALAR_MASK) {
                std::string result;
                uint64_t remaining = (uint64_t)value;
                for (size_t index = 0; index < type->flag_count; ++index) {
                    const uint64_t flag = (uint64_t)type->flags[index].value;
                    if (flag != 0 && (remaining & flag) == flag) {
                        if (!result.empty()) result += ", ";
                        result += type->flags[index].name;
                        remaining &= ~flag;
                    }
                }
                if (!result.empty() && remaining == 0) return result;
            }
            return std::to_string(value);
        }

        std::string Value_Text_(config::t_variable_id variable_id,
                                const config::s_typed_value& value) {
            switch (value.type) {
            case config::CONFIG_VALUE_BOOL:
                return value.bool_value ? "true" : "false";
            case config::CONFIG_VALUE_STRING: {
                std::string decoded;
                config::Decode_Cp850_To_Utf8(value.string_value.data(),
                                              value.string_value.size(), &decoded);
                return '"' + decoded + '"';
            }
            case config::CONFIG_VALUE_BITMAP: {
                std::string result = "0x";
                char byte[3];
                for (uint8_t value_byte : value.bitmap_value) {
                    std::snprintf(byte, sizeof(byte), "%02X", value_byte);
                    result += byte;
                }
                return result;
            }
            case config::CONFIG_VALUE_RATIO:
                return std::to_string(value.ratio_numerator) + "/" +
                       std::to_string(value.ratio_denominator);
            default:
                return Integer_Value_(config::Get_Variable_Type(variable_id),
                                      value.integer_value);
            }
        }

        void Build_Tree_() {
            if (!g_tree.empty()) return;
            g_tree.push_back({});
            std::unordered_map<std::string, uint32_t> nodes_by_path;
            nodes_by_path.emplace("", 0);

            for (config::t_variable_id variable_id = 0;
                 variable_id < config::variable_count; ++variable_id) {
                std::string path;
                if (!config::Get_Variable_Name(variable_id, &path)) continue;

                uint32_t parent = 0;
                size_t segment_begin = 0;
                while (segment_begin < path.size()) {
                    const size_t segment_end = path.find('.', segment_begin);
                    const size_t end = segment_end == std::string::npos ?
                                           path.size() : segment_end;
                    const std::string prefix = path.substr(0, end);
                    auto found = nodes_by_path.find(prefix);
                    if (found == nodes_by_path.end()) {
                        const uint32_t node_index = (uint32_t)g_tree.size();
                        g_tree.push_back({path.substr(segment_begin, end - segment_begin)});
                        g_tree[parent].children.push_back(node_index);
                        nodes_by_path.emplace(prefix, node_index);
                        parent = node_index;
                    } else {
                        parent = found->second;
                    }
                    if (segment_end == std::string::npos) break;
                    segment_begin = segment_end + 1;
                }

                s_tree_node& leaf = g_tree[parent];
                leaf.variable_id = variable_id;
                leaf.search_text = path;
                const config::s_config_group_descriptor* group =
                    config::Get_Config_Group(config::Get_Variable_Group(variable_id));
                if (group != nullptr) {
                    leaf.search_text += ' ';
                    leaf.search_text += group->id;
                }
                leaf.search_text += ' ';
                leaf.search_text +=
                    Value_Type_Name_(config::Get_Variable_Type(variable_id)->value_type);
            }
        }

        void Update_Visible_(const ImGuiTextFilter& filter,
                             std::vector<uint8_t>* visible) {
            visible->assign(g_tree.size(), 0);
            for (size_t index = g_tree.size(); index-- > 0;) {
                const s_tree_node& node = g_tree[index];
                bool node_visible = node.variable_id != config::CONFIG_NO_VARIABLE &&
                                    filter.PassFilter(node.search_text.c_str());
                for (uint32_t child : node.children) {
                    node_visible = node_visible || (*visible)[child] != 0;
                }
                (*visible)[index] = node_visible;
            }
        }

        void Draw_Value_(config::t_variable_id variable_id) {
            config::s_typed_value value;
            if (!config::generated::Get_Current_Value(variable_id, &value)) {
                ImGui::TextDisabled("unavailable");
                return;
            }
            const std::string text = Value_Text_(variable_id, value);
            ImGui::TextUnformatted(text.c_str());
        }

        void Draw_Node_(uint32_t node_index, const std::vector<uint8_t>& visible,
                        bool filtering) {
            if (!visible[node_index]) return;
            const s_tree_node& node = g_tree[node_index];
            const bool leaf = node.children.empty();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (filtering && !leaf) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns |
                                       ImGuiTreeNodeFlags_OpenOnArrow |
                                       ImGuiTreeNodeFlags_OpenOnDoubleClick;
            if (leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (node.variable_id == g_selected_variable) flags |= ImGuiTreeNodeFlags_Selected;
            const bool open = ImGui::TreeNodeEx(
                (void*)(uintptr_t)(node_index + 1),
                flags, "%s", node.label.c_str());
            if (node.variable_id != config::CONFIG_NO_VARIABLE &&
                ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                g_selected_variable = node.variable_id;
            }

            if (node.variable_id != config::CONFIG_NO_VARIABLE) {
                const config::s_type_descriptor* type =
                    config::Get_Variable_Type(node.variable_id);
                const config::s_config_group_descriptor* group =
                    config::Get_Config_Group(config::Get_Variable_Group(node.variable_id));
                ImGui::TableSetColumnIndex(1);
                Draw_Value_(node.variable_id);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(group == nullptr ? "unknown" : group->id);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(type == nullptr ? "unknown" :
                                       Value_Type_Name_(type->value_type));
            }

            if (open && !leaf) {
                for (uint32_t child : node.children) {
                    Draw_Node_(child, visible, filtering);
                }
                ImGui::TreePop();
            }
        }

        void Draw_Details_() {
            if (g_selected_variable == config::CONFIG_NO_VARIABLE ||
                g_selected_variable >= config::variable_count) {
                ImGui::TextDisabled("Select a configuration value to see its details.");
                return;
            }

            std::string path;
            config::Get_Variable_Name(g_selected_variable, &path);
            const config::s_type_descriptor* type =
                config::Get_Variable_Type(g_selected_variable);
            const config::s_config_group_descriptor* group =
                config::Get_Config_Group(config::Get_Variable_Group(g_selected_variable));
            ImGui::TextUnformatted(path.c_str());
            ImGui::Text("Group: %s | Type: %s",
                        group == nullptr ? "unknown" : group->id,
                        type == nullptr ? "unknown" : Value_Type_Name_(type->value_type));
            ImGui::TextUnformatted("Current:");
            ImGui::SameLine();
            Draw_Value_(g_selected_variable);
            config::s_typed_value default_value;
            ImGui::TextUnformatted("Default:");
            ImGui::SameLine();
            if (config::generated::Get_Default_Value(g_selected_variable, &default_value)) {
                const std::string text = Value_Text_(g_selected_variable, default_value);
                ImGui::TextUnformatted(text.c_str());
            } else {
                ImGui::TextDisabled("native value");
            }
        }

        void Draw_(const debug_overlay::s_panel_context& context) {
            (void)context;
            static ImGuiTextFilter filter;
            static std::vector<uint8_t> visible;
            Build_Tree_();
            Update_Visible_(filter, &visible);

            filter.Draw("Search name, group, or type", -1.0f);
            ImGui::TextDisabled("%zu generated values; current values are read-only.",
                                config::variable_count);
            if (ImGui::BeginChild("configuration_tree", ImVec2(0.0f, -105.0f), true)) {
                if (ImGui::BeginTable("configuration_values", 4,
                        ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.50f);
                    ImGui::TableSetupColumn("Current value", ImGuiTableColumnFlags_WidthStretch, 0.30f);
                    ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                    ImGui::TableHeadersRow();
                    for (uint32_t child : g_tree[0].children) {
                        Draw_Node_(child, visible, filter.IsActive());
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            ImGui::SeparatorText("Details");
            Draw_Details_();
        }

        debug_overlay::s_global_pane_registration g_registration("Config", Draw_);
    } // namespace
} // namespace CONFIG_DEBUG
#endif