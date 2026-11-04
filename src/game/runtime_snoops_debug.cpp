#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

namespace RUNTIME_SNOOPS_DEBUG {
    namespace {
        constexpr const char* k_message_type_names[] = {
            "Colony",
            "Alliance declared",
            "War declared",
            "Unknown 0x03",
            "Financial",
            "Lost ship",
            "Scrap ship",
            "Lost colony",
            "Hyperspace flux",
            "Hyperspace beast",
            "Lost attacking spy",
            "Lost defending spy",
            "Artemis system",
            "Artemis leader",
            "Failed retreat",
            "Spy assassination",
            "Anomaly",
            "Diplomacy",
            "Eel spawned",
            "Eel vanished",
            "Artifacts",
            "April 1",
            "Scrap leader",
        };
        constexpr const char* k_colony_message_names[MSG_COLONY_SUBTYPE_COUNT] = {
            "Building scrapped",
            "Production completed",
            "Genocide",
            "Assimilation",
            "Starvation",
            "Settlers arrived",
            "Plague",
            "Population boom",
            "Comet",
            "Rebellion",
            "No room for android",
            "Android killed",
        };

        int16_t g_nebula_x = 0;
        int16_t g_nebula_y = 0;
        int16_t g_nebula_index = 0;
        int16_t g_nebula_star = 0;
        bool g_show_empty_message_slots = false;

        const char* Message_Type_Name_(int8_t type) {
            return type >= 0 && type < (int8_t)(sizeof(k_message_type_names) /
                       sizeof(k_message_type_names[0]))
                ? k_message_type_names[type]
                : "Unknown message";
        }

        const char* Colony_Message_Name_(MSG_COLONY_SUBTYPE subtype) {
            const uint8_t index = static_cast<uint8_t>(subtype);
            return index < MSG_COLONY_SUBTYPE_COUNT
                ? k_colony_message_names[index]
                : "Unknown colony event";
        }

        const char* Report_Type_Name_(const s_report& report) {
            if (report.type == 1) {
                return report.action == 0 ? "Your spy stole technology"
                                          : report.action == 1
                    ? "Your spy sabotaged a colony"
                    : "Your spy report";
            }
            if (report.type == 2) {
                return report.action == 0 ? "Enemy spy stole technology"
                                          : report.action == 1
                    ? "Enemy spy sabotaged a colony"
                    : "Enemy spy report";
            }
            return report.type == 3 ? "Artifacts discovered" : "Unknown report";
        }

        const char* Sync_State_Name_(uint8_t state) {
            switch (state) {
                case NET_SYNC_STATE_IDLE: return "Idle";
                case NET_SYNC_STATE_HOST_STARTUP: return "Host startup";
                case NET_SYNC_STATE_HOST_ASSIGN_PLAYERS: return "Host assigning players";
                case NET_SYNC_STATE_HOST_WAITING_FOR_PLAYER_SETUP: return "Host waiting for player setup";
                case NET_SYNC_STATE_READY: return "Ready";
                case NET_SYNC_STATE_CLIENT_JOINING: return "Client joining";
                case NET_SYNC_STATE_CLIENT_SELECT_RACE: return "Client selecting race";
                case NET_SYNC_STATE_WAITING_FOR_GAME_DATA: return "Waiting for game data";
                case NET_SYNC_STATE_NEXT_TURN_READY: return "Next turn ready";
                case NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS: return "Host waiting for orders";
                case NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA: return "Host receiving player data";
                case NET_SYNC_STATE_HOST_BEGIN_TURN: return "Host beginning turn";
                case NET_SYNC_STATE_RECEIVING_GAME_ORDERS: return "Receiving game orders";
                case NET_SYNC_STATE_COMBAT_READY: return "Combat ready";
                case NET_SYNC_STATE_PICKING_COMBAT_TARGET: return "Picking combat target";
                case NET_SYNC_STATE_COMBAT_TARGET_SENT: return "Combat target sent";
                case NET_SYNC_STATE_COMBAT_RESOLUTION_DISPATCH: return "Dispatching combat resolution";
                case NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER: return "Tactical combat attacker";
                case NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER: return "Tactical combat defender";
                case NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER_VS_NONHUMAN: return "Tactical attacker vs nonhuman";
                case NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER_VS_NONHUMAN: return "Tactical defender vs nonhuman";
                case NET_SYNC_STATE_COMBAT_STRATEGIC_ATTACKER: return "Strategic combat attacker";
                case NET_SYNC_STATE_COMBAT_RESULTS_DISPLAY: return "Displaying combat results";
                case NET_SYNC_STATE_STRATEGIC_RESULTS_RECEIVED: return "Strategic results received";
                case NET_SYNC_STATE_DRAGON_BRIBE_PROMPT: return "Dragon bribe prompt";
                case NET_SYNC_STATE_DRAGON_BRIBE_DECLINED: return "Dragon bribe declined";
                case NET_SYNC_STATE_COMBAT_COMPLETE: return "Combat complete";
                case NET_SYNC_STATE_LOAD_GAME_HOST_SETUP: return "Load-game host setup";
                case NET_SYNC_STATE_AI_DIPLOMACY_REQUESTED: return "AI diplomacy requested";
                case NET_SYNC_STATE_AI_DIPLOMACY_DONE: return "AI diplomacy done";
                case NET_SYNC_STATE_AI_DIPLOMACY_CANCEL_PENDING: return "AI diplomacy cancellation pending";
                case NET_SYNC_STATE_AI_DIPLOMACY_READY: return "AI diplomacy ready";
                case NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED: return "Human diplomacy requested";
                case NET_SYNC_STATE_HUMAN_DIPLOMACY_REJECTED: return "Human diplomacy rejected";
                case NET_SYNC_STATE_HUMAN_DIPLOMACY_READY: return "Human diplomacy ready";
                case NET_SYNC_STATE_CLIENT_PICK_POSITION: return "Client picking position";
                case NET_SYNC_STATE_CLIENT_NO_AVAILABLE_SLOT: return "Client has no available slot";
                case NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING: return "Human diplomacy connecting";
                case NET_SYNC_STATE_HUMAN_DIPLOMACY_BLOCKED: return "Human diplomacy blocked";
                default: return "Unknown sync state";
            }
        }

        const char* Player_State_Name_(int8_t state) {
            switch (state) {
                case NET_PLAYER_STATE_READY: return "Ready";
                case NET_PLAYER_STATE_AI_DIPLOMACY_REQUESTED: return "AI diplomacy requested";
                case NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE: return "AI diplomacy active";
                case NET_PLAYER_STATE_COMBAT_READY: return "Combat ready";
                case NET_PLAYER_STATE_PICK_TARGET_INFO: return "Picking combat target";
                case NET_PLAYER_STATE_COMBAT_TARGET_CHOSEN: return "Combat target chosen";
                case NET_PLAYER_STATE_COMBAT_TACTICAL_ATTACKER: return "Tactical attacker";
                case NET_PLAYER_STATE_COMBAT_TACTICAL_DEFENDER: return "Tactical defender";
                case NET_PLAYER_STATE_COMBAT_STRATEGIC_DEFENDER: return "Strategic defender";
                case NET_PLAYER_STATE_BATTLE_RESULTS_ACK: return "Battle results acknowledged";
                case NET_PLAYER_STATE_COMBAT_STRATEGIC_ATTACKER: return "Strategic attacker";
                case NET_PLAYER_STATE_COMBAT_RESULTS_POPUP: return "Combat results popup";
                case NET_PLAYER_STATE_HUMAN_DIPLOMACY_ACTIVE: return "Human diplomacy active";
                case NET_PLAYER_STATE_HUMAN_DIPLOMACY_READY_DATA: return "Human diplomacy data ready";
                case NET_PLAYER_STATE_HUMAN_DIPLOMACY_IGNORED: return "Human diplomacy ignored";
                case NET_PLAYER_STATE_DRAGON_BRIBE_DESTINATION: return "Dragon bribe destination";
                default: return "Unknown player state";
            }
        }

        const char* Player_Name_(int16_t player, char* buffer, size_t buffer_size) {
            const int16_t player_count = std::clamp<int16_t>(
                MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            if (MOX::player != nullptr && player >= 0 && player < player_count) {
                snprintf(buffer, buffer_size, "%d: %.20s (%.15s)", (int)player,
                    MOX::player[player].name, MOX::player[player].race_name);
            } else {
                snprintf(buffer, buffer_size, "%d: invalid/unassigned", (int)player);
            }
            return buffer;
        }

        const char* Star_Name_(int16_t star, char* buffer, size_t buffer_size) {
            if (MOX::star != nullptr && star >= 0 && star < MOX::NUM_STARS) {
                snprintf(buffer, buffer_size, "%d: %.15s", (int)star,
                    MOX::star[star].name);
            } else {
                snprintf(buffer, buffer_size, "%d: invalid", (int)star);
            }
            return buffer;
        }

        void Draw_Hex_(const void* data, size_t size) {
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            for (size_t offset = 0; offset < size; offset += 16) {
                char line[80];
                int written = snprintf(line, sizeof(line), "%04X:", (unsigned)offset);
                for (size_t index = offset;
                     index < std::min<size_t>(offset + 16, size) &&
                     written > 0 && written < (int)sizeof(line);
                     ++index) {
                    written += snprintf(line + written, sizeof(line) - written,
                        " %02X", bytes[index]);
                }
                ImGui::TextUnformatted(line);
            }
        }

        void Draw_Player_Mask_(const char* label, uint8_t mask) {
            char names[256] = {};
            size_t used = 0;
            const int16_t player_count = std::clamp<int16_t>(
                MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            for (int16_t player = 0; player < player_count; ++player) {
                if ((mask & (1u << player)) == 0) {
                    continue;
                }
                const int result = snprintf(names + used, sizeof(names) - used,
                    "%s%d:%.20s", used == 0 ? "" : ", ", (int)player,
                    MOX::player != nullptr ? MOX::player[player].name : "?");
                if (result < 0 || (size_t)result >= sizeof(names) - used) {
                    break;
                }
                used += (size_t)result;
            }
            ImGui::Text("%s: 0x%02X [%s]", label, (unsigned)mask,
                used == 0 ? "none" : names);
        }

        void Draw_Report_Payload_(const s_report& report) {
            char player_name[64];
            if (report.type == 1 && report.action == 0) {
                ImGui::Text("Framed player: %s", Player_Name_(
                    report.d.steal.framed_player_idx, player_name, sizeof(player_name)));
                ImGui::Text("Victim player: %d", (int)report.d.steal.victim_player_idx);
                ImGui::Text("Technology application: %d", (int)report.d.steal.tech_app_id);
            } else if (report.type == 1 && report.action == 1) {
                ImGui::Text("Framed player: %s", Player_Name_(
                    report.d.sabotage.framed_player_idx, player_name, sizeof(player_name)));
                ImGui::Text("Victim player: %u", (unsigned)report.d.sabotage.victim_player_idx);
                ImGui::Text("Planet: %d", (int)report.d.sabotage.planet_idx);
                ImGui::Text("Building: %u", (unsigned)report.d.sabotage.building_id);
            } else if (report.type == 2 && report.action == 0) {
                ImGui::Text("Enemy player: %d", (int)report.d.enemy_steal.enemy_player_idx);
                ImGui::Text("Technology application: %d", (int)report.d.enemy_steal.tech_app_id);
            } else if (report.type == 2 && report.action == 1) {
                ImGui::Text("Enemy player: %d", (int)report.d.enemy_sabotage.enemy_player_idx);
                ImGui::Text("Planet: %d", (int)report.d.enemy_sabotage.planet_idx);
                ImGui::Text("Building: %u", (unsigned)report.d.enemy_sabotage.building_id);
            } else if (report.type == 3) {
                ImGui::Text("Planet: %d", (int)report.d.artifacts.planet_idx);
                ImGui::Text("Technology application: %d",
                    (int)report.d.artifacts.tech_app_id);
            }
        }

        void Draw_Reports_() {
            const int16_t report_count = std::clamp<int16_t>(
                REPORT::n_reports, 0, MAX_REPORTS);
            ImGui::Text("Stored reports: %d / %d", (int)report_count, MAX_REPORTS);
            ImGui::Text("Done player reports: %u", (unsigned)REPORT::done_player_reports);
            ImGui::Text("Displayed messages: %u", (unsigned)REPORT::displayed_msgs);
            ImGui::Text("Displayed GNN: %u", (unsigned)REPORT::displayed_gnn);
            ImGui::Text("No outpost prompt: %u", (unsigned)REPORT::no_outpost_prompt);
            ImGui::Text("Initial tech prompt: %u", (unsigned)REPORT::initial_tech_prompt);

            for (int16_t player = 0; player < MAX_PLAYERS; ++player) {
                char player_label[64];
                char header[96];
                snprintf(header, sizeof(header), "%s - interesting: %u",
                    Player_Name_(player, player_label, sizeof(player_label)),
                    (unsigned)REPORT::something_interesting_happened[player]);
                if (!ImGui::CollapsingHeader(header)) {
                    continue;
                }
                bool found = false;
                for (int16_t index = 0; index < report_count; ++index) {
                    const s_report& report = REPORT::reports[index];
                    if (report.player_idx != player) {
                        continue;
                    }
                    found = true;
                    ImGui::PushID((int32_t)index);
                    char label[96];
                    snprintf(label, sizeof(label), "Report %d: %s", (int)index,
                        Report_Type_Name_(report));
                    if (ImGui::TreeNode(label)) {
                        ImGui::Text("Recipient: %d", (int)report.player_idx);
                        ImGui::Text("Type: %u", (unsigned)report.type);
                        ImGui::Text("Action: %u", (unsigned)report.action);
                        Draw_Report_Payload_(report);
                        if (ImGui::TreeNode("Raw record")) {
                            Draw_Hex_(&report, sizeof(report));
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (!found) {
                    ImGui::TextUnformatted("No reports for this player.");
                }
            }

            if (ImGui::CollapsingHeader("Consumed, unassigned, or invalid reports")) {
                for (int16_t index = 0; index < report_count; ++index) {
                    const s_report& report = REPORT::reports[index];
                    if (report.player_idx >= 0 && report.player_idx < MAX_PLAYERS) {
                        continue;
                    }
                    ImGui::PushID((int32_t)index);
                    char label[96];
                    snprintf(label, sizeof(label), "Report %d: %s, recipient %d",
                        (int)index, Report_Type_Name_(report), (int)report.player_idx);
                    if (ImGui::TreeNode(label)) {
                        Draw_Report_Payload_(report);
                        Draw_Hex_(&report, sizeof(report));
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
        }

        void Draw_Message_Payload_(const s_MSG& message) {
            char name_a[64];
            char name_b[64];
            switch (message.msg_type) {
                case MSG_TYPE_COLONY:
                    ImGui::Text("Planet: %d", (int)message.u.colony.planet_idx);
                    ImGui::Text("Event: %u - %s", (unsigned)message.u.colony.subtype,
                        Colony_Message_Name_(message.u.colony.subtype));
                    switch (message.u.colony.subtype) {
                        case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED:
                            ImGui::Text("Building: %d, BC: %d",
                                (int)message.u.colony.c.scrap.building_id,
                                (int)message.u.colony.c.scrap.bc);
                            break;
                        case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED:
                            ImGui::Text("Build ID: %d, product: %d, refit: %s, append format: %d",
                                (int)message.u.colony.c.build.build_id,
                                (int)message.u.colony.c.build.product_id,
                                message.u.colony.c.build.is_refit ? "yes" : "no",
                                (int)message.u.colony.c.build.append_fmt);
                            break;
                        case MSG_COLONY_SUBTYPE_GENOCIDE:
                        case MSG_COLONY_SUBTYPE_ASSIMILATION:
                            ImGui::Text("Race/player value: %u",
                                (unsigned)message.u.colony.c.assimilation.val);
                            break;
                        case MSG_COLONY_SUBTYPE_STARVATION:
                            ImGui::Text("Starvation values: %d, %d",
                                (int)message.u.colony.c.starvation.a,
                                (int)message.u.colony.c.starvation.b);
                            break;
                        case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED:
                            ImGui::Text("Settlers: %d, over limit: %d, blockaded: %s",
                                (int)message.u.colony.c.settlers.settlers,
                                (int)message.u.colony.c.settlers.over_limit,
                                message.u.colony.c.settlers.blockaded ? "yes" : "no");
                            break;
                        case MSG_COLONY_SUBTYPE_REBELLION:
                            ImGui::Text("Owner: %d, rebels: %d, lost: %d",
                                (int)message.u.colony.c.rebels.owner_player,
                                (int)message.u.colony.c.rebels.rebels,
                                (int)message.u.colony.c.rebels.lost);
                            break;
                        case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC:
                            ImGui::Text("Race: %d",
                                (int)message.u.colony.c.android_killed.race_idx);
                            break;
                        default:
                            break;
                    }
                    break;
                case MSG_TYPE_DECLARE_ALLIANCE:
                case MSG_TYPE_DECLARE_WAR:
                    ImGui::Text("Players: %s and %s",
                        Player_Name_(message.u.players.player_a_idx, name_a, sizeof(name_a)),
                        Player_Name_(message.u.players.player_b_idx, name_b, sizeof(name_b)));
                    break;
                case MSG_TYPE_FINANCIAL:
                    ImGui::Text("Values: %d, %d", (int)message.u.financial.a,
                        (int)message.u.financial.b);
                    break;
                case MSG_TYPE_LOST_SHIP:
                    ImGui::Text("Star: %d, ship: %d, leader: %d, fate: %d",
                        (int)message.u.lost_ship.star_idx,
                        (int)message.u.lost_ship.ship_idx,
                        (int)message.u.lost_ship.leader_idx,
                        (int)message.u.lost_ship.fate);
                    break;
                case MSG_TYPE_SCRAP_SHIP:
                    ImGui::Text("Location: %d, BC: %d, ship: %d",
                        (int)message.u.scrap_ship.where, (int)message.u.scrap_ship.bc,
                        (int)message.u.scrap_ship.ship_idx);
                    break;
                case MSG_TYPE_LOST_COLONY:
                    ImGui::Text("Planet: %d, reason: %d, leader: %d, colony/outpost: %d",
                        (int)message.u.lost_colony.planet_idx,
                        (int)message.u.lost_colony.reason,
                        (int)message.u.lost_colony.leader_idx,
                        (int)message.u.lost_colony.colony_or_outpost);
                    break;
                case MSG_TYPE_HYPERSPACE_BEAST:
                    ImGui::Text("Ship: %d, star: %d, officer: %d, status: %d, fate: %d, yours: %d",
                        (int)message.u.hyperspace_beast.ship_idx,
                        (int)message.u.hyperspace_beast.star_idx,
                        (int)message.u.hyperspace_beast.officer_idx,
                        (int)message.u.hyperspace_beast.leader_status,
                        (int)message.u.hyperspace_beast.fate,
                        (int)message.u.hyperspace_beast.your_ship);
                    break;
                case MSG_TYPE_LOST_ATTACKING_SPY:
                    ImGui::Text("Player: %d", (int)message.u.lost_attacking_spy.player_idx);
                    break;
                case MSG_TYPE_LOST_DEFENDING_SPY:
                    ImGui::Text("Count: %d", (int)message.u.lost_defending_spy.count);
                    break;
                case MSG_TYPE_ARTEMIS:
                    ImGui::Text("Star: %d", (int)message.u.artemis.star_idx);
                    break;
                case MSG_TYPE_ARTEMIS_LEADER:
                    ImGui::Text("Star: %d, leader: %d, fate: %d",
                        (int)message.u.artemis_leader.star_idx,
                        (int)message.u.artemis_leader.leader_idx,
                        (int)message.u.artemis_leader.fate);
                    break;
                case MSG_TYPE_FAILED_RETREAT:
                    ImGui::Text("Star: %d, reason: %d, ships lost: %d, leaders: %d",
                        (int)message.u.retreat.star_idx,
                        (int)message.u.retreat.reason,
                        (int)message.u.retreat.ships_lost,
                        (int)message.u.retreat.leader_count);
                    break;
                case MSG_TYPE_SPY_ASSASSINATION:
                    ImGui::Text("Players: %d, %d; leader: %d; victim: %d",
                        (int)message.u.spy_assasination.plr_a,
                        (int)message.u.spy_assasination.plr_b,
                        (int)message.u.spy_assasination.leader_id,
                        (int)message.u.spy_assasination.victim_id);
                    break;
                case MSG_TYPE_ANOMALY:
                    ImGui::Text("Star: %d", (int)message.u.anomaly.star_idx);
                    break;
                case MSG_TYPE_DIPLOMACY:
                    ImGui::Text("Diplomacy event: %d; players: %d, %d",
                        (int)message.u.diplomacy.diplomacy_idx,
                        (int)message.u.diplomacy.plr_a,
                        (int)message.u.diplomacy.plr_b);
                    break;
                case MSG_TYPE_EEL_SPAWNED:
                case MSG_TYPE_EEL_VANISHED:
                    ImGui::Text("Star: %d", (int)message.u.eel.star_idx);
                    break;
                case MSG_TYPE_ARTIFACTS:
                    ImGui::Text("Star: %d, technologies: %d, %d",
                        (int)message.u.artifacts.star_idx,
                        (int)message.u.artifacts.tech1,
                        (int)message.u.artifacts.tech2);
                    break;
                case MSG_TYPE_SCRAP_LEADER:
                    ImGui::Text("Leader: %d", (int)message.u.scrap_leader.leader_idx);
                    break;
                default:
                    break;
            }
        }

        void Draw_Messages_() {
            ImGui::Checkbox("Show unused allocated slots", &g_show_empty_message_slots);
            for (int16_t player = 0; player < MAX_PLAYERS; ++player) {
                const int32_t first = MSG_::first_msg[player];
                const int32_t capacity = std::min<int32_t>(MSG_::max_msgs[player],
                    first < MAX_MESSAGES ? MAX_MESSAGES - first : 0);
                const int32_t used = std::min<int32_t>(MSG_::n_msgs[player], capacity);
                char player_name[64];
                char header[128];
                snprintf(header, sizeof(header), "%s - %d / %d messages, first slot %d",
                    Player_Name_(player, player_name, sizeof(player_name)), (int)used,
                    (int)capacity, (int)first);
                if (!ImGui::CollapsingHeader(header)) {
                    continue;
                }
                if (first < 0 || first >= MAX_MESSAGES || capacity <= 0) {
                    ImGui::TextUnformatted("This player's message slice is invalid or empty.");
                    continue;
                }
                const int32_t shown = g_show_empty_message_slots ? capacity : used;
                for (int32_t offset = 0; offset < shown; ++offset) {
                    const int32_t index = first + offset;
                    const s_MSG& message = MSG_::msgs[index];
                    ImGui::PushID(index);
                    char label[128];
                    snprintf(label, sizeof(label), "Slot %d%s: %s, priority %u",
                        index, offset >= used ? " (unused)" : "",
                        Message_Type_Name_(message.msg_type), (unsigned)message.priority);
                    if (ImGui::TreeNode(label)) {
                        ImGui::Text("Type: %d - %s", (int)message.msg_type,
                            Message_Type_Name_(message.msg_type));
                        ImGui::Text("Priority: %u", (unsigned)message.priority);
                        Draw_Message_Payload_(message);
                        if (ImGui::TreeNode("Raw record")) {
                            Draw_Hex_(&message, sizeof(message));
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (shown == 0) {
                    ImGui::TextUnformatted("No messages for this player.");
                }
            }
        }

        bool Nebula_Assets_Ready_() {
            const int16_t count = std::clamp<int16_t>(
                MOX::NUM_NEBULAS, 0, MAX_NEBULAS);
            if (MOX::nebula == nullptr || count == 0) {
                return false;
            }
            for (int16_t index = 0; index < count; ++index) {
                const uint8_t type = (uint8_t)MOX::nebula[index].type % 12;
                if (MOX::nebula_pict_seg[type][3] == nullptr) {
                    return false;
                }
            }
            return true;
        }

        bool Point_In_Nebula_Safe_(int16_t x, int16_t y, int16_t nebula_index) {
            const int16_t count = std::clamp<int16_t>(
                MOX::NUM_NEBULAS, 0, MAX_NEBULAS);
            if (MOX::nebula == nullptr || nebula_index < 0 || nebula_index >= count) {
                return false;
            }
            const s_nebula& nebula = MOX::nebula[nebula_index];
            if (x < nebula.x || y < nebula.y) {
                return false;
            }
            const s_animation_header* animation =
                MOX::nebula_pict_seg[(uint8_t)nebula.type % 12][3];
            if (animation == nullptr || animation->frame_count <= 0 ||
                animation->width <= 0 || animation->height <= 0) {
                return false;
            }
            const int32_t relative_x = ((int32_t)x - nebula.x) / 3;
            const int32_t relative_y = ((int32_t)y - nebula.y) / 3;
            if (relative_x < 0 || relative_y < 0 || relative_x >= animation->width ||
                relative_y >= animation->height) {
                return false;
            }
            const uint8_t* pixels = Animation_Frame_Data_(animation, 0);
            return pixels != nullptr &&
                pixels[relative_y * animation->width + relative_x] > 5;
        }

        void Draw_Nebulas_() {
            const int16_t count = std::clamp<int16_t>(
                MOX::NUM_NEBULAS, 0, MAX_NEBULAS);
            ImGui::Text("Nebulas: %d / %d", (int)count, MAX_NEBULAS);
            ImGui::Text("Storage: %p", (void*)MOX::nebula);
            ImGui::Text("Geometry assets: %s",
                Nebula_Assets_Ready_() ? "ready" : "not fully loaded");

            if (MOX::nebula != nullptr) {
                if (ImGui::BeginTable("nebula_list", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Index");
                    ImGui::TableSetupColumn("X");
                    ImGui::TableSetupColumn("Y");
                    ImGui::TableSetupColumn("Type");
                    ImGui::TableSetupColumn("Mask");
                    ImGui::TableHeadersRow();
                    for (int16_t index = 0; index < count; ++index) {
                        const s_nebula& nebula = MOX::nebula[index];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", (int)index);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%d", (int)nebula.x);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%d", (int)nebula.y);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%d", (int)nebula.type);
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%s", MOX::nebula_pict_seg[(uint8_t)nebula.type % 12][3]
                                != nullptr ? "loaded" : "missing");
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::SeparatorText("Read-only point checker");
            ImGui::InputScalar("Point X", ImGuiDataType_S16, &g_nebula_x);
            ImGui::InputScalar("Point Y", ImGuiDataType_S16, &g_nebula_y);
            ImGui::InputScalar("Nebula index", ImGuiDataType_S16, &g_nebula_index);
            ImGui::InputScalar("Star index", ImGuiDataType_S16, &g_nebula_star);
            g_nebula_index = std::clamp<int16_t>(g_nebula_index, 0,
                std::max<int16_t>(count - 1, 0));
            const int16_t star_count = std::clamp<int16_t>(
                MOX::NUM_STARS, 0, MAX_STARS);
            g_nebula_star = std::clamp<int16_t>(g_nebula_star, 0,
                std::max<int16_t>(star_count - 1, 0));
            int16_t matching_nebula = -1;
            for (int16_t index = 0; index < count; ++index) {
                if (Point_In_Nebula_Safe_(g_nebula_x, g_nebula_y, index)) {
                    matching_nebula = index;
                    break;
                }
            }
            ImGui::Text("Point in selected nebula: %s",
                Point_In_Nebula_Safe_(g_nebula_x, g_nebula_y, g_nebula_index)
                    ? "yes" : "no");
            ImGui::Text("Point in any nebula: %s",
                matching_nebula >= 0 ? "yes" : "no");
            ImGui::Text("First matching nebula: %d", (int)matching_nebula);
            if (MOX::star != nullptr && star_count > 0) {
                const s_star_data& star = MOX::star[g_nebula_star];
                int16_t star_nebula = -1;
                for (int16_t index = 0; index < count; ++index) {
                    if (Point_In_Nebula_Safe_(star.x, star.y, index)) {
                        star_nebula = index;
                        break;
                    }
                }
                char star_name[64];
                ImGui::Text("Star: %s at (%d, %d)",
                    Star_Name_(g_nebula_star, star_name, sizeof(star_name)),
                    (int)star.x, (int)star.y);
                ImGui::Text("Calculated nebula: %d; cached flag: %u",
                    (int)star_nebula, (unsigned)star.in_nebula);
            }
        }

        const char* Network_Role_(const s_network& network) {
            if (network.is_network_active == 0 && netcode::netInitialized == 0) {
                return "Offline";
            }
            if (network.local_net_id == network.net_host_id) {
                return "Host";
            }
            return "Client";
        }

        const char* Traffic_Peer_Name_(uint32_t network_id, char* buffer,
            size_t buffer_size) {
            const int16_t player_count = std::clamp<int16_t>(
                MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            if (MOX::player != nullptr) {
                for (int16_t player = 0; player < player_count; ++player) {
                    if ((uint32_t)MOX::player[player].network_player_id == network_id) {
                        snprintf(buffer, buffer_size, "%u: %.20s (player %d)",
                            (unsigned)network_id, MOX::player[player].name,
                            (int)player);
                        return buffer;
                    }
                }
            }
            snprintf(buffer, buffer_size, "%u: unmapped peer", (unsigned)network_id);
            return buffer;
        }

        double Compression_Ratio_(uint64_t compressed, uint64_t uncompressed) {
            return uncompressed == 0 ? 0.0
                                     : 100.0 * (double)compressed / (double)uncompressed;
        }

        void Draw_Traffic_Stats_() {
            netadapter::s_traffic_stats stats;
            if (netcode::Net_Get_Traffic_Stats(&stats) == 0) {
                ImGui::TextUnformatted("Traffic statistics are unavailable for this backend.");
                return;
            }
            const double elapsed_seconds = stats.sampled_ms >= stats.started_ms
                ? (double)(stats.sampled_ms - stats.started_ms) / 1000.0
                : 0.0;
            ImGui::Text("Tracking elapsed: %.1f seconds; recent window: %.1f seconds",
                elapsed_seconds, (double)stats.recent_window_ms / 1000.0);
            if (ImGui::BeginTable("network_traffic_totals", 5,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Direction");
                ImGui::TableSetupColumn("Payload bytes");
                ImGui::TableSetupColumn("Packets");
                ImGui::TableSetupColumn("Average kbps");
                ImGui::TableSetupColumn("Recent kbps");
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("Sent");
                ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)stats.sent.payload_bytes);
                ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)stats.sent.packets);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", stats.average_sent_kbps);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", stats.recent_sent_kbps);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("Received");
                ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)stats.received.payload_bytes);
                ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)stats.received.packets);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", stats.average_received_kbps);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", stats.recent_received_kbps);
                ImGui::EndTable();
            }
            ImGui::TextUnformatted("Aggregate sent payload counts each API send once; broadcast peer rows show recipient fanout.");

            if (ImGui::TreeNode("Traffic by network player")) {
                if (stats.peers.empty()) {
                    ImGui::TextUnformatted("No logical game traffic recorded.");
                }
                for (const netadapter::s_peer_traffic_stats& peer : stats.peers) {
                    char peer_name[96];
                    if (ImGui::TreeNode(Traffic_Peer_Name_(
                            peer.network_id, peer_name, sizeof(peer_name)))) {
                        ImGui::Text("Sent: %llu bytes in %llu packets; average %.3f kbps; recent %.3f kbps",
                            (unsigned long long)peer.sent.payload_bytes,
                            (unsigned long long)peer.sent.packets,
                            peer.average_sent_kbps, peer.recent_sent_kbps);
                        ImGui::Text("Received: %llu bytes in %llu packets; average %.3f kbps; recent %.3f kbps",
                            (unsigned long long)peer.received.payload_bytes,
                            (unsigned long long)peer.received.packets,
                            peer.average_received_kbps, peer.recent_received_kbps);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Wire and compression")) {
                const network::router::s_router_client_stats& transport = stats.transport;
                const network::router::s_router_frame_codec_stats& compression =
                    transport.compression;
                ImGui::Text("Wire received: %llu bytes in %llu frames",
                    (unsigned long long)transport.wire_bytes_in,
                    (unsigned long long)transport.frames_in);
                ImGui::Text("Wire sent: %llu bytes in %llu frames; pending: %u bytes",
                    (unsigned long long)transport.wire_bytes_out,
                    (unsigned long long)transport.frames_out,
                    (unsigned)transport.pending_write_bytes);
                ImGui::Text("Failed queued writes: %llu bytes in %llu frames",
                    (unsigned long long)transport.failed_write_bytes,
                    (unsigned long long)transport.failed_write_frames);
                ImGui::Text("Outgoing payload compression: %llu -> %llu bytes (%.1f%% of original)",
                    (unsigned long long)compression.uncompressed_payload_bytes_out,
                    (unsigned long long)compression.compressed_payload_bytes_out,
                    Compression_Ratio_(compression.compressed_payload_bytes_out,
                        compression.uncompressed_payload_bytes_out));
                ImGui::Text("Incoming payload compression: %llu -> %llu bytes (%.1f%% of original)",
                    (unsigned long long)compression.uncompressed_payload_bytes_in,
                    (unsigned long long)compression.compressed_payload_bytes_in,
                    Compression_Ratio_(compression.compressed_payload_bytes_in,
                        compression.uncompressed_payload_bytes_in));
                ImGui::Text("Last incoming payload: %u -> %u bytes",
                    (unsigned)compression.last_uncompressed_payload_bytes_in,
                    (unsigned)compression.last_compressed_payload_bytes_in);
                ImGui::TextUnformatted("Wire totals include router control frames and framing overhead; compression totals cover frame payloads only.");
                ImGui::TreePop();
            }

            ImGui::TextUnformatted("Latency: unavailable. Game packets have no acknowledgement or ping/pong correlation, so no reliable RTT is reported.");
        }

        void Draw_Network_Player_(const s_network& network, int16_t player) {
            const s_net_player_info& info = network.player_net_info[player];
            char player_name[64];
            char label[128];
            const int16_t mapped_players = std::clamp<int16_t>(
                MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            if (player < mapped_players) {
                const int16_t network_id = MOX::player != nullptr
                    ? MOX::player[player].network_player_id
                    : -1;
                const bool local = network_id == network.local_net_id;
                const bool host = network_id == network.net_host_id;
                snprintf(label, sizeof(label), "%s%s%s - %s (0x%02X)",
                    Player_Name_(player, player_name, sizeof(player_name)),
                    local ? " [local]" : "",
                    host ? " [host]" : "",
                    Player_State_Name_(info.player_state), (unsigned)(uint8_t)info.player_state);
            } else {
                snprintf(label, sizeof(label), "Slot %d - %s (0x%02X)", (int)player,
                    Player_State_Name_(info.player_state), (unsigned)(uint8_t)info.player_state);
            }
            ImGui::PushID((int32_t)player);
            if (ImGui::TreeNode(label)) {
                if (player < mapped_players && MOX::player != nullptr) {
                    ImGui::Text("Strategic network player ID: %d",
                        (int)MOX::player[player].network_player_id);
                }
                ImGui::Text("Connected flag: %d",
                    player < MAX_PLAYERS ? (int)network.player_connected_flags[player] : -1);
                ImGui::Text("Combat star: %d", (int)info.combat_star_id);
                ImGui::Text("Combat defender: %d", (int)info.combat_defender_id);
                ImGui::Text("Combat planet: %d", (int)info.combat_planet_id);
                ImGui::Text("Combat target player: %d", (int)info.combat_target_player_id);
                ImGui::Text("Combat type: %u", (unsigned)info.combat_type);
                ImGui::Text("Combat attacker: %d", (int)info.combat_attacker_id);
                ImGui::Text("Combat result: %d", (int)info.combat_result);
                ImGui::Text("Diplomacy target: %d", (int)info.diplomacy_target_id);
                if (player < MAX_PLAYERS) {
                    ImGui::Text("Host election rank: %d",
                        (int)network.host_election_player_rank[player]);
                    ImGui::Text("Host election rank cache: %d",
                        (int)network.host_election_rank_sync_cache[player]);
                    ImGui::Text("Hit next turn: %d",
                        (int)network.players_hit_next_turn[player]);
                    ImGui::Text("Active: %d; orders received: %u; sync mask: 0x%02X; diplomacy sync: 0x%02X",
                        (int)network.player_is_active_flags[player],
                        (unsigned)network.host_orders_received_flags[player],
                        (unsigned)network.players_sync_mask[player],
                        (unsigned)network.diplomacy_sync_mask[player]);
                    ImGui::Text("Phase ready sent: %u; race selection sync pending: %u",
                        (unsigned)network.player_phase_ready_sent_flags[player],
                        (unsigned)network.player_race_selection_sync_pending[player]);
                    ImGui::Text("Color selected: %d; race selected: %d; alternate: %u; deselected: %u",
                        (int)network.player_color_selected_flags[player],
                        (int)network.player_race_selected_flags[player],
                        (unsigned)network.player_race_selected_alt_flags[player],
                        (unsigned)network.player_race_deselected_flags[player]);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        void Draw_Network_() {
            const s_network& network = NETMOX::net;
            ImGui::TextUnformatted("Live read-only view. This panel never pumps the network adapter.");
            ImGui::Text("Role: %s", Network_Role_(network));
            ImGui::Text("Game name: %.9s", network.cur_game_name);
            ImGui::Text("Endpoint: %.30s", network.online_settings.endpoint);
            ImGui::Text("Network active: %u; initialized: %u; online initialized: %d",
                (unsigned)network.is_network_active, (unsigned)netcode::netInitialized,
                (int)network.online_initialized);
            ImGui::Text("Backend mode: %u (%s); connection type: %d",
                (unsigned)network.mode, network.mode == 0 ? "LAN" : "direct/online",
                (int)netcode::connection_type);
            ImGui::Text("Connection state: %d; timeout: %d",
                (int)network.connection_state, (int)network.connection_timeout_flag);
            ImGui::Text("Sync state: %s (0x%02X)", Sync_State_Name_(network.sync_state),
                (unsigned)network.sync_state);
            ImGui::Text("Tactical sync: %u; sync complete: %u; sync phase active: %d",
                (unsigned)network.tactical_combat_sync_state,
                (unsigned)network.network_sync_complete_flag,
                (int)network.sync_phase_active_flag);

            if (ImGui::CollapsingHeader("Transport and membership", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Local ID: %d; host ID: %d; cached local host: %u; last-send status: 0x%08X",
                    (int)network.local_net_id, (int)network.net_host_id,
                    (unsigned)netcode::thisHost, (unsigned)netcode::Net_Status());
                ImGui::Text("Joining ID: %d; session ID: %u; connected players: %d",
                    (int)network.joining_net_id, (unsigned)network.cur_session_id,
                    (int)network.num_connected_players);
                ImGui::Text("Total bytes received: %u", (unsigned)network.total_bytes_received);
                ImGui::Text("Legacy chunks: %d / %d; data pointer: %p",
                    (int)network.received_chunk_count, (int)network.total_chunk_count,
                    network.network_data_ptr);
                ImGui::Text("Sending: %d; in comms: %u; in send callback: %d",
                    (int)netcode::tpSending, (unsigned)netcode::inComms,
                    (int)netcode::in_send_message);
                ImGui::Text("NETMOX sending: %u; receiving: %u; updating: %u; update counter: %u",
                    (unsigned)NETMOX::sending_msg, (unsigned)NETMOX::in_receive_message,
                    (unsigned)NETMOX::is_updating,
                    (unsigned)NETMOX::next_update_player_counter);
                ImGui::Text("Receive callback: %p", (void*)netcode::recv_fn);
                ImGui::Text("Snapshot buffer: %p; current: %p; size: %u",
                    (void*)NETMOX::net_snapshot_cache.buffer,
                    (void*)NETMOX::net_snapshot_cache.current_ptr,
                    (unsigned)NETMOX::net_snapshot_cache.buffer_size);
            }

            if (ImGui::CollapsingHeader("Traffic, rates, and compression", ImGuiTreeNodeFlags_DefaultOpen)) {
                Draw_Traffic_Stats_();
            }

            if (ImGui::CollapsingHeader("Host election")) {
                ImGui::Text("Election ID: %d; winner: %d; active host player: %d; sync counter: %d",
                    (int)network.host_election_id, (int)network.host_election_winner_id,
                    (int)network.host_active_player_id, (int)network.host_sync_counter);
            }

            if (ImGui::CollapsingHeader("Combat synchronization")) {
                ImGui::Text("Star: %d; planet: %d; defender: %d; target player: %d",
                    (int)network.combat_star_id, (int)network.combat_planet_id,
                    (int)network.combat_defender_id, (int)network.combat_target_player_id);
                ImGui::Text("Type: %u; attacker: %d; result: %d; battle results: %p",
                    (unsigned)network.combat_type, (int)network.combat_attacker_id,
                    (int)network.combat_result, (void*)network.combat_results_data);
            }

            if (ImGui::CollapsingHeader("Diplomacy synchronization")) {
                ImGui::Text("Target: %d; partner: %d; initiator: %d",
                    (int)network.diplomacy_target_id, (int)network.diplomacy_partner_id,
                    (int)network.diplomacy_initiator_id);
                ImGui::Text("Active: %d; initiator flag: %d; handshake state: %d",
                    (int)network.diplomacy_active_flag,
                    (int)network.is_diplomacy_initiator,
                    (int)network.diplomacy_handshake_state);
                ImGui::Text("Human diplomat wait flag: %d",
                    (int)network.human_diplomat_wait_flag);
                Draw_Player_Mask_("AI diplomacy done", network.ai_diplomacy_done_mask);
                Draw_Player_Mask_("AI diplomacy active", network.ai_diplomacy_active_mask);
                Draw_Player_Mask_("AI diplomacy cancel", network.ai_diplomacy_cancel_mask);
                Draw_Player_Mask_("Human diplomacy ignored", network.human_diplomacy_ignored_mask);
                Draw_Player_Mask_("Players active", network.players_active_mask);
                Draw_Player_Mask_("Human diplomacy requests", network.human_diplomacy_request_mask);
            }

            if (ImGui::CollapsingHeader("Player network slots", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (int16_t player = 0; player < 15; ++player) {
                    Draw_Network_Player_(network, player);
                }
            }

            if (ImGui::CollapsingHeader("Selection and miscellaneous state")) {
                ImGui::Text("Race selection changed: %u", (unsigned)network.race_selection_changed);
                ImGui::Text("Reserved field 0xc6: %u", (unsigned)network.field23_0xc6);
                ImGui::Text("Dragon bribe destination star: %d", (int)network.dragon_bribe_destination_star_id);
                ImGui::Text("Online init string: %.11s", network.online_init_str);
                if (ImGui::TreeNode("Raw online settings")) {
                    Draw_Hex_(&network.online_settings, sizeof(network.online_settings));
                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Raw packed network record")) {
                Draw_Hex_(&network, sizeof(network));
            }
        }

        void Draw_(const debug_overlay::s_panel_context& context) {
            (void)context;
            ImGui::TextUnformatted("Game records are read-only; checker inputs affect only this pane.");
            if (ImGui::BeginTabBar("runtime_snoop_tabs")) {
                if (ImGui::BeginTabItem("Reports")) {
                    Draw_Reports_();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Messages")) {
                    Draw_Messages_();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Nebulas")) {
                    Draw_Nebulas_();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Network")) {
                    Draw_Network_();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        debug_overlay::s_global_pane_registration g_registration("Runtime", Draw_);
    } // namespace
} // namespace RUNTIME_SNOOPS_DEBUG
#endif