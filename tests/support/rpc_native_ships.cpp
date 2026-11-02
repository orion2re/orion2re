#include "pch.h"
#include "rpc_native_ships.h"
#include "rpc_native_colonies.h"
#include "rpc/game_rpc.h"
#include "rpc/rpc_game.h"
#include "game/game_events.h"
#include "lua/lua_game_ship_identity.h"
#include "game/ship_read.h"
#include "game/string_overrides.h"

#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>

namespace test_native_ships {
    namespace {
        s_ship_data ships[MAX_SHIPS];
        rpc::s_game_store* store;
        rpc::s_map_observation* map;
        uint64_t subscription, identities;
        bool active;
        bool output_pending;
        std::unique_ptr<game_events::s_colony_queue_scope> staged;
        s_ship_data saved;
        std::filesystem::path resource_root;
        game_rpc::s_fleet_eta_adapter fleet_adapter;
        std::unique_ptr<game_events::s_fleet_box_view_scope> fleet_owner;
        void* fleet_pool = nullptr;
        void* old_pool = nullptr;
        bool fleet_active = false;
        s_star_data fleet_stars[4];

        void Load_Resources() {
            resource_root = std::filesystem::temp_directory_path() / ("orion-owned-ships-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            const auto directory = resource_root / "data/KENTEXT.LBX";
            std::filesystem::create_directories(directory);
            for (int language = 0; language < 6; ++language) {
                for (int message : {3, 4, 5, 6, 31, 32, 33, 34, 35}) {
                    char entry[4];
                    snprintf(entry, sizeof(entry), "%03d", message * 6 + language);
                    std::string text = message < 31 ? "F" + std::to_string(language) : "Support" + std::to_string(language) + "(%d)";
                    text.resize(100, '\0');
                    const uint16_t count = 1, width = 100;
                    std::ofstream file(directory / entry, std::ios::binary);
                    file.write((const char*)&count, sizeof(count));
                    file.write((const char*)&width, sizeof(width));
                    file.write(text.data(), text.size());
                    if (!file.good()) std::abort();
                }
            }
            platform::Reset_Exit();
            if (!vfs::Init(resource_root.string().c_str())) std::abort();
            string_overrides::Reset_Cache();
            ship_read::Load_Strings();
        }

        void Identity(int16_t slot, void*) {
            if (active && slot >= 0 && slot < MOX::NUM_SHIPS && MOX::ship[slot].owner == store->viewer && MOX::ship[slot].status != 5) rpc::Expire_Game_Inspection(store);
        }
        void Observe(game_events::e_event event, void*) {
            if (!active) return;
            if ((event == game_events::DESIGNS_REPLACING && game_events::Replaced_Design_Owner() == store->viewer) || event == game_events::SHIP_IDENTITIES_RESETTING || event == game_events::STRATEGIC_LABELS_REPLACING) rpc::Expire_Game_Inspection(store);
        }
        bool Read() {
            if (game_events::Colony_Edits_Active() || store->busy_depth != 0) return true;
            if (MOX::ship == nullptr) {
                rpc::Expire_Game_Inspection(store);
                return true;
            }
            rpc::s_map_observation fresh;
            if (!game_rpc::Read_Inspection_Observation(&fresh)) return false;
            *map = std::move(fresh);
            return rpc::Capture_Map(store, map);
        }

        void Message(int16_t id, const char* text) {
            MOX::h_string_ix[id] = MOX::h_strings.size();
            MOX::h_strings.insert(MOX::h_strings.end(), text, text + strlen(text) + 1);
        }

        bool Refresh_Fleets() {
            if (store->busy_depth != 0) return true;
            if (MOX::ship == nullptr) {
                rpc::Expire_Game_Inspection(store);
                return true;
            }
            SHIPS::Build_Ship_Icons();
            return !fleet_adapter.failed && !store->snapshots.empty() && store->snapshots.back().map.ships_captured;
        }

        bool Present_Label(int16_t slot, bool frame = true, bool name = true) {
            struct s_surface {
                s_font old_font = fonts::font_header;
                uint8_t* old_data = fonts::font_data;
                uint8_t* old_page = video::current_video_page;
                int32_t old_render = platform::g_render_buffer;
                s_animation_header picture = {};
                uint8_t glyph[2] = {1, 0x80};
                std::vector<uint8_t> pixels = std::vector<uint8_t>(640 * 480);
                s_surface() {
                    fonts::font_header = {};
                    for (size_t style = 0; style < std::size(fonts::font_header.font_glyph_widths); ++style) {
                        fonts::font_header.font_heights[style] = 1;
                        std::fill(std::begin(fonts::font_header.font_glyph_widths[style]), std::end(fonts::font_header.font_glyph_widths[style]), 1);
                    }
                    fonts::font_data = glyph;
                    video::current_video_page = pixels.data();
                    platform::g_render_buffer = 1;
                    MOX::fleet_box.name_seg = &picture;
                }
                ~s_surface() {
                    fonts::font_header = old_font;
                    fonts::font_data = old_data;
                    video::current_video_page = old_page;
                    platform::g_render_buffer = old_render;
                    MOX::fleet_box.name_seg = nullptr;
                }
            } surface;
            for (int stack = 0; stack < MOX::ship_stack_count; ++stack) {
                int node = MOX::ship_stack_start[stack];
                int index = 0;
                MOX::fleet_ship_scanned = -1;
                while (node != -1) {
                    MOX::fleet_movement_icon[index].ship_node_idx = node;
                    if (MOX::ship_node[node].ship_idx == slot) MOX::fleet_ship_scanned = index;
                    ++index;
                    node = MOX::ship_node[node].next_node;
                }
                if (MOX::fleet_ship_scanned == -1) continue;
                if (!name) MOX::fleet_ship_scanned = -1;
                MOX::fleet_box_ship_stack = stack;
                MOX::fleet_box.count = index;
                MOX::moveable_box[2].id = MOX::box_off_flag[2] + 1;
                MOX::eta_star_id = -1;
                game_events::s_main_view_input_scope input;
                if (frame) {
                    game_events::s_observation_scope draw(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
                    FLEETPOP::Print_Fleet_Box_Info();
                } else
                    FLEETPOP::Print_Fleet_Box_Info();
                return true;
            }
            return false;
        }

        void Seed_Fleets() {
            Seed();
            // Native XY lookup includes the Antares sentinel at NUM_STARS.
            memcpy(fleet_stars, MOX::star, 3 * sizeof(s_star_data));
            fleet_stars[3] = {};
            fleet_stars[3].x = fleet_stars[3].y = 32767;
            MOX::star = fleet_stars;
            MOX::NUM_SHIPS = 27;
            MOX::settings.enemy_moves = 1;
            MAINSCR::fleet_icon_chosen = -1;
            MOX::player[0].contact[1] = 1;
            MOX::player[0].contact[2] = 0;
            strlcpy(MOX::player[1].race_name, "Contact", sizeof(MOX::player[1].race_name));
            strlcpy(MOX::player[2].race_name, "Unidentified", sizeof(MOX::player[2].race_name));
            MOX::player[2].color = 2;
            for (int i = 0; i < 3; ++i) MOX::star[i].x = i * 30;
            const char* singular[] = {"Frigate", "Destroyer", "Cruiser", "Battleship", "Titan", "Doom Star"};
            const char* plural[] = {"Frigates", "Destroyers", "Cruisers", "Battleships", "Titans", "Doom Stars"};
            for (int i = 0; i < 6; ++i) {
                TECHDATA::hull_data[i].name = const_cast<char*>(singular[i]);
                TECHDATA::hull_data[i].size_name = const_cast<char*>(plural[i]);
            }
            MOX::race_names[13] = (char*)"Antaran";
            Message(0x66, "%s Fleet");
            Message(0x69, "%d turn to %s");
            Message(0x6a, "%d turns to %s");
            Message(0x6b, "%d turn");
            Message(0x6c, "%d turns");
            Message(0x164, "%d Colony Ship");
            Message(0x165, "%d Colony Ships");
            Message(0x166, "%d Outpost Ship");
            Message(0x167, "%d Outpost Ships");
            Message(0x168, "%d Transport");
            Message(0x169, "%d Transports");
            for (int16_t slot = 10; slot < 27; ++slot) {
                ships[slot] = ships[0];
                ships[slot].owner = 1;
                ships[slot].status = 0;
                ships[slot].location = 0;
                ships[slot].x = ships[slot].y = 0;
                orion2re::lua::game::Advance_Ship_Generation(slot);
                strlcpy(ships[slot].d.name, "Suppressed", sizeof(ships[slot].d.name));
            }
            ships[12].d.ship_type = SHIP_TYPE_TRANSPORT;
            ships[13].d.ship_type = SHIP_TYPE_COLONY;
            ships[14].d.ship_type = SHIP_TYPE_OUTPOST;
            strlcpy(ships[12].d.name, "Convoy", sizeof(ships[12].d.name));
            ships[15].owner = ships[16].owner = ships[26].owner = 2;
            ships[15].x = 30;
            ships[15].location = 1;
            for (int slot : {16, 26}) {
                ships[slot].x = 45;
                ships[slot].location = SHIP_LOCATION_MOVING_OFFSET + 2;
                ships[slot].status = 1;
                ships[slot].turns_left = -1;
            }
            ships[26].d.ship_type = SHIP_TYPE_TRANSPORT;
            for (int slot : {17, 18}) {
                ships[slot].owner = 8;
                ships[slot].x = 30;
                ships[slot].location = 1;
                ships[slot].d.size = slot == 17 ? 1 : 3;
            }
            const char* special[] = {"Guardian", "Amoeba", "Crystal", "Dragon", "Eel", "Hydra"};
            for (int slot = 19; slot < 25; ++slot) {
                ships[slot].owner = slot - 10;
                ships[slot].x = 60;
                ships[slot].location = 2;
                ships[slot].d.speed = 0;
                strlcpy(ships[slot].d.name, special[slot - 19], sizeof(ships[slot].d.name));
            }
            ships[25].owner = 2;
            ships[25].status = 1;
            ships[25].x = 30000;
            ships[25].location = SHIP_LOCATION_MOVING_OFFSET + 2;
            ships[8].status = 5;
        }

        void Wait_For_Output() {
            if (!output_pending) return;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (std::chrono::steady_clock::now() < deadline) {
                bool drained;
                {
                    std::lock_guard lock(store->bridge->mutex);
                    drained = store->bridge->ready.empty();
                }
                if (drained) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            {
                std::lock_guard lock(store->bridge->mutex);
                if (!store->bridge->ready.empty() || store->bridge->deliveries.empty()) std::abort();
            }
            output_pending = false;
        }

        bool Fleet_Command(const std::string& command) {
            if (!command.starts_with("fleet-")) return false;
            if (command == "fleet-native") {
                active = fleet_active = true;
                Seed_Fleets();
                game_events::Unsubscribe(subscription);
                game_events::Unsubscribe(identities);
                subscription = identities = 0;
                old_pool = MOX::global_data_seg;
                fleet_pool = MOX::global_data_seg = allocate::Allocate_Space(2 << 20);
                if (!game_rpc::Initialize_Fleet_Eta_Adapter(&fleet_adapter, store)) std::abort();
                fleet_owner = std::make_unique<game_events::s_fleet_box_view_scope>();
                fleet_owner->Initialized();
            } else if (!fleet_active)
                return false;
            else if (command == "fleet-large") {
                for (int16_t slot = 27; slot < 3000; ++slot) {
                    ships[slot] = ships[10];
                    orion2re::lua::game::Advance_Ship_Generation(slot);
                }
                rpc::s_request request;
                request.id_json = "99";
                request.method = "game.getState";
                request.params_json = "{\"include\":[\"ships\"]}";
                int low = 300, high = 3000;
                bool sized = false;
                while (low <= high) {
                    MOX::NUM_SHIPS = low + (high - low) / 2;
                    if (!Refresh_Fleets()) std::abort();
                    const auto response = rpc::Game_Query(store, &request);
                    if (response.find("\"result\":") != std::string::npos && response.size() >= 900000 && response.size() < 1048576) {
                        sized = true;
                        break;
                    }
                    if (response.find("\"result\":") != std::string::npos && response.size() < 900000)
                        low = MOX::NUM_SHIPS + 1;
                    else
                        high = MOX::NUM_SHIPS - 1;
                }
                if (!sized) std::abort();
                return true;
            } else if (command == "fleet-output")
                return Command("inspection-output");
            else if (command == "fleet-label")
                return Present_Label(12);
            else if (command == "fleet-special-label")
                return Present_Label(20);
            else if (command == "fleet-unframed-label")
                return Present_Label(12, false);
            else if (command == "fleet-cached")
                return Present_Label(16, true, false);
            else if (command == "fleet-interrupt") {
                Wait_For_Output();
                game_events::Publish(game_events::MAIN_VIEW_INPUT_INTERRUPTED);
                return true;
            } else if (command == "fleet-exit") {
                fleet_owner.reset();
                return true;
            } else if (command == "fleet-enter") {
                fleet_owner = std::make_unique<game_events::s_fleet_box_view_scope>();
                fleet_owner->Initialized();
            } else if (command == "fleet-partial") {
                ++MOX::player[0].bc;
                return Read();
            } else if (command == "fleet-hidden") {
                memset(ships[10].d.name, 'X', sizeof(ships[10].d.name));
                ships[10].crew_experience++;
                ships[10].d.cost++;
                ships[10].d.shield_type = -1;
                ships[25].d.size = -1;
                MOX::player[1].bc = -999999;
                // Read at the same native observation point, without rebuilding.
                rpc::s_map_observation fresh;
                if (!game_rpc::Read_Refreshed_Ship_Observation(&fresh) || !rpc::Capture_Map(store, &fresh)) std::abort();
                return true;
            } else if (command == "fleet-change")
                ships[11].d.size = 3;
            else if (command == "fleet-eta-zero")
                ships[16].turns_left = 0;
            else if (command == "fleet-depart") {
                int16_t list[] = {16, 26, -1};
                for (int slot : {16, 26}) {
                    ships[slot].status = 0;
                    ships[slot].location = 1;
                    ships[slot].x = 30;
                }
                s_ship_move_info movement = {};
                movement.turns_left = 3;
                movement.traveling_speed = 1;
                SHIPMOVE::Make_Ships_Move_To(list, 2, &movement);
            } else if (command == "fleet-special-destination") {
                for (int slot : {16, 26}) {
                    ships[slot].status = 1;
                    ships[slot].location = SHIP_LOCATION_MOVING_OFFSET + MOX::NUM_STARS;
                    ships[slot].x = 45;
                }
            } else if (command == "fleet-retire") {
                Wait_For_Output();
                game_events::Publish_Ship_Identity_Changing(10);
                ships[10].status = 5;
            } else if (command == "fleet-reuse") {
                orion2re::lua::game::Advance_Ship_Generation(10);
                ships[10].status = 0;
            } else if (command == "fleet-owner") {
                game_events::Publish_Ship_Identity_Changing(11);
                ships[11].owner = 0;
                strlcpy(ships[11].d.name, "Prize Two", sizeof(ships[11].d.name));
            } else if (command == "fleet-resources") {
                game_events::Publish(game_events::STRATEGIC_LABELS_REPLACING);
                return true;
            } else if (command == "fleet-unavailable") {
                MOX::ship = nullptr;
                return Read();
            } else if (command == "fleet-storage")
                MOX::ship = ships;
            else if (command == "fleet-empty") {
                rpc::Expire_Game_Inspection(store);
                for (auto& ship : ships) ship.status = 5;
            } else if (command == "fleet-advance") {
                rpc::Pause_Game_Queries(store);
                ++MOX::stardate;
                return true;
            } else if (command == "fleet-commit")
                rpc::Resume_Game_Queries(store);
            else if (command == "fleet-reset") {
                rpc::Begin_Game_Reset(store);
                rpc::Commit_Game_Reset(store);
            } else if (command == "fleet-switch") {
                rpc::Revoke_Game_View(store);
                MOX::PLAYER_NUM = 1;
            } else if (command != "fleet-refresh")
                return false;
            if (!Refresh_Fleets()) std::abort();
            return true;
        }
    } // namespace

    void Seed() {
        output_pending = false;
        test_native_colonies::Seed_World();
        MOX::settings.strategic_combat_flag = 0;
        MOX::settings.language = 0;
        MOX::game_type = 0;
        memset(ships, 0, sizeof(ships));
        MOX::ship = ships;
        MOX::NUM_SHIPS = 10;
        for (int i = 0; i < 6; ++i) {
            TECHDATA::hull_data[i].name = (char*)"Cruiser";
            TECHDATA::hull_data[i].structure_hp = 50;
            TECHDATA::hull_data[i].armor_hp = 40;
            TECHDATA::hull_data[i].drive_hp = 20;
            TECHDATA::hull_data[i].computer_hp = 10;
            TECHDATA::hull_data[i].shield_hp = 30;
            TECHDATA::hull_data[i].strat_hits = 80;
        }
        TECHDATA::hull_data[3].name = (char*)"Battleship";
        TECHDATA::hull_data[SHIP_SIZE_DOOM_STAR].name = (char*)"Doom Star";
        for (int i = 0; i < SHIELD_COUNT; ++i) TECHDATA::shields[i].name = (char*)"Class shield";
        for (int i = 0; i < COMPUTER_COUNT; ++i) TECHDATA::computers[i].name = (char*)"Battle computer";
        for (int i = 0; i < ARMOR_COUNT; ++i) TECHDATA::armor[i].name = (char*)"Titanium armor";
        for (int i = 0; i < DRIVE_COUNT; ++i) TECHDATA::drives[i].name = (char*)"Nuclear drive";
        for (int i = 0; i < WEAPON_COUNT; ++i) {
            TECHDATA::weapons[i].name = (char*)"Laser";
            TECHDATA::weapons[i].name_plural = (char*)"Lasers";
        }
        for (int i = 0; i < SPECIAL_COUNT; ++i) TECHDATA::specials[i].name = (char*)"Battle Scanner";
        for (int i = 0; i < WEAPON_MOD_COUNT; ++i) TECHDATA::weapon_modifications[i].name_plural = (char*)"Heavy";
        TECHDATA::shields[1].shield_class = 2;
        TECHDATA::computers[1].bonus = 25;
        TECHDATA::drives[1].warp_speed = 2;
        TECHDATA::armor[1].ships_bonus = 100;
        ESTRINGS::strings[0x51] = (char*)"%s%s%s";
        MOX::h_strings.assign({'P', 'r', 'i', 'm', 'e', '\0', 'V', 'e', 't', 'e', 'r', 'a', 'n', '\0', 'N', 'o', ' ', 'm', 'o', 'd', 'i', 'f', 'i', 'c', 'a', 't', 'i', 'o', 'n', 's', '\0'});
        MOX::h_string_ix[HESTR_064_NO_MODIFICATIONS] = 14;
        for (int i = 0; i < CREW_QUALITY_COUNT; ++i) {
            MOX::h_string_ix[0x8A + i] = 6;
            MOX::crew_data[i].attack = 10;
            MOX::crew_data[i].defense = 20;
        }
        const char* names[] = {"Aegis", "Prize", "Settler", "Trooper", "Pioneer", "Doom", "Refitting", "Building", "Secret enemy", "Relic"};
        for (int16_t i = 0; i < 10; ++i) {
            auto* ship = &ships[i];
            strlcpy(ship->d.name, names[i], sizeof(ship->d.name));
            ship->owner = 0;
            ship->officer_index = -1;
            ship->d.size = 2;
            ship->d.shield_type = 1;
            ship->d.computer_type = 1;
            ship->d.armor_type = 1;
            ship->d.ftl_type = 1;
            ship->d.combat_speed = 12;
            ship->d.cost = 120;
            ship->d.ship_weapon[0].type = 1;
            ship->d.ship_weapon[0].count = 2;
            ship->d.ship_weapon[0].current_count = 2;
            ship->d.ship_weapon[0].firing_arc = WEAPON_FIRING_ARC_360;
            ship->crew_quality = CREW_QUALITY_VETERAN;
            ship->crew_experience = 234;
            orion2re::lua::game::Advance_Ship_Generation(i);
        }
        ships[0].structural_damage = 7;
        ships[0].armor_damage = 13;
        ships[0].drive_damage_percent = 25;
        ships[0].shield_damage_percent = 33;
        ships[0].computer_damage = 2;
        ships[0].d.ship_weapon[0].current_count = 1;
        ships[0].d.ship_weapon[0].specials = WEAPON_MOD_FLAG_HEAVY_MOUNT;
        struct_::Set_Bit_Field(ships[0].d.special_device_flags, SPECIAL_BATTLE_SCANNER);
        struct_::Set_Bit_Field(ships[0].special_device_damage_flags, SPECIAL_BATTLE_SCANNER);
        ships[1].d.previous_owner = 8;
        ships[1].d.size = 3;
        ships[1].d.ship_weapon[0].firing_arc = WEAPON_FIRING_ARC_ALL_SECTORS;
        ships[2].d.ship_type = SHIP_TYPE_COLONY;
        ships[3].d.ship_type = SHIP_TYPE_TRANSPORT;
        for (int i : {2, 3}) {
            ships[i].location = 1;
            ships[i].x = 30;
        }
        ships[4].d.ship_type = SHIP_TYPE_OUTPOST;
        ships[4].location = SHIP_LOCATION_MOVING_OFFSET + 2;
        ships[4].status = 1;
        ships[4].x = 60;
        ships[4].turns_left = 255;
        ships[5].d.size = SHIP_SIZE_DOOM_STAR;
        ships[5].location = SHIP_LOCATION_WORMHOLE_OFFSET + 2;
        ships[5].status = 2;
        ships[5].x = 70;
        ships[5].turns_left = 0;
        ships[6].status = 4;
        ships[6].d.cost = 55;
        ships[7].status = 6;
        ships[7].d.ship_type = SHIP_TYPE_TRANSPORT;
        ships[8].owner = 2;
        ships[9].d.previous_owner = 10;
        ships[9].d.ftl_type = 0;
        ships[9].d.shield_type = 0;
        ships[9].x = 80;
        for (int colony = 0; colony < MOX::NUM_COLONIES; ++colony)
            for (int row = 3; row < 7; ++row) MOX::colony[colony].producing[row] = COLONY_PRODUCTION_NONE;
        MOX::colony[0].producing[3] = COLBLDG::Colony_Production_From_Ship_Index(6);
        MOX::colony[1].producing[4] = COLBLDG::Colony_Production_From_Ship_Index(7);
        MOX::player[0].ship_designs[0] = ships[0].d;
        strlcpy(MOX::player[0].ship_designs[0].name, "Committed slot", sizeof(MOX::player[0].ship_designs[0].name));
        Load_Resources();
    }

    void Initialize(rpc::s_game_store* target, rpc::s_map_observation* observation) {
        store = target;
        map = observation;
        subscription = game_events::Subscribe(Observe, nullptr);
        identities = game_events::Subscribe_Ship_Identity(Identity, nullptr);
    }

    bool Command(const std::string& command) {
        if (command.starts_with("fleet-")) return Fleet_Command(command);
        if (!command.starts_with("inspection-")) return false;
        if (command == "inspection-native") {
            active = true;
            Seed();
        } else if (!active)
            return false;
        else if (command == "inspection-hidden") {
            memset(ships[8].d.name, 'X', sizeof(ships[8].d.name));
            ships[8].d.size = -1;
            ships[8].d.shield_type = -1;
            MOX::player[2].bc = -999999;
        } else if (command == "inspection-noncombat-hidden") {
            ships[3].crew_experience = 65535;
            ships[3].drive_damage_percent = 99;
            ships[3].d.ship_weapon[0].count = 69;
            ships[3].d.ship_weapon[0].current_count = 69;
        } else if (command == "inspection-change") {
            ships[0].structural_damage += 3;
            ships[0].crew_experience += 1;
        } else if (command == "inspection-eta-zero") {
            ships[4].turns_left = 0;
        } else if (command == "inspection-large") {
            for (int16_t i = 10; i < 1500; ++i) {
                orion2re::lua::game::Advance_Ship_Generation(i);
                ships[i] = ships[0];
                snprintf(ships[i].d.name, sizeof(ships[i].d.name), "Copy %d", i);
            }
            rpc::s_request request;
            request.id_json = "99";
            request.method = "game.getState";
            request.params_json = "{\"include\":[\"ships\"]}";
            int16_t low = 300, high = 1500;
            bool sized = false;
            while (low <= high) {
                MOX::NUM_SHIPS = low + (high - low) / 2;
                if (!Read()) std::abort();
                const auto response = rpc::Game_Query(store, &request);
                const bool complete = response.find("\"result\":") != std::string::npos;
                if (complete && response.size() >= 900000 && response.size() < 1048576) {
                    sized = true;
                    break;
                }
                if (complete && response.size() < 900000)
                    low = MOX::NUM_SHIPS + 1;
                else
                    high = MOX::NUM_SHIPS - 1;
            }
            if (!sized) std::abort();
        } else if (command == "inspection-output") {
            std::vector<rpc::s_client_identity> clients;
            rpc::Get_Authorized_Clients(store->bridge, &clients);
            if (clients.size() != 1) return false;
            rpc::s_request request;
            request.id_json = "99";
            request.method = "game.getState";
            request.params_json = "{\"include\":[\"ships\"]}";
            for (int i = 0; i < 4; ++i) {
                if (rpc::Submit_Work(store->bridge, clients[0].client, clients[0].epoch, &request, UINT64_MAX) != 0) return false;
                rpc::s_work work;
                if (!rpc::Take_Work(store->bridge, 0, &work)) return false;
                uint64_t lease = 0;
                const auto response = rpc::Game_Query(store, &request, &lease);
                if (response.size() < 900000 || response.size() >= 1048576) std::abort();
                if (lease == 0 || !rpc::Complete_Work(store->bridge, &work, &response, lease)) return false;
            }
            output_pending = true;
            return true;
        } else if (command == "inspection-retire")
            COLBLDG::Delete_Ship(6);
        else if (command == "inspection-loss") {
            if (output_pending) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                while (std::chrono::steady_clock::now() < deadline) {
                    bool drained;
                    {
                        std::lock_guard lock(store->bridge->mutex);
                        drained = store->bridge->ready.empty();
                    }
                    if (drained) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                {
                    std::lock_guard lock(store->bridge->mutex);
                    if (!store->bridge->ready.empty() || store->bridge->deliveries.empty()) std::abort();
                }
                output_pending = false;
            }
            game_events::Publish_Ship_Identity_Changing(0);
            ships[0].owner = 1;
        } else if (command == "inspection-reuse") {
            orion2re::lua::game::Advance_Ship_Generation(0);
            ships[0].owner = 0;
        } else if (command == "inspection-design") {
            game_events::Publish_Designs_Replacing(0);
            strlcpy(MOX::player[0].ship_designs[0].name, "Replacement", sizeof(MOX::player[0].ship_designs[0].name));
        } else if (command == "inspection-staged") {
            staged = std::make_unique<game_events::s_colony_queue_scope>();
            saved = ships[6];
            strlcpy(ships[6].d.name, "Uncommitted", sizeof(ships[6].d.name));
        } else if (command == "inspection-restored") {
            ships[6] = saved;
            staged->Commit();
            staged.reset();
        } else if (command == "inspection-strategic")
            MOX::settings.strategic_combat_flag = 1;
        else if (command == "inspection-unavailable")
            MOX::ship = nullptr;
        else if (command == "inspection-storage")
            MOX::ship = ships;
        else if (command == "inspection-empty") {
            rpc::Expire_Game_Inspection(store);
            for (auto& ship : ships) ship.status = 5;
            for (int i = 0; i < 6; ++i) MOX::player[0].ship_designs[i].name[0] = '\0';
        } else if (command == "inspection-reset") {
            rpc::Begin_Game_Reset(store);
            rpc::Commit_Game_Reset(store);
        } else if (command == "inspection-switch") {
            rpc::Revoke_Game_View(store);
            MOX::PLAYER_NUM = 1;
        } else if (command == "inspection-advance") {
            rpc::Pause_Game_Queries(store);
            ++MOX::stardate;
            return true;
        } else if (command == "inspection-commit")
            rpc::Resume_Game_Queries(store);
        else
            return false;
        if (!Read()) std::abort();
        return true;
    }

    void Shutdown() {
        fleet_owner.reset();
        if (fleet_adapter.store != nullptr) game_rpc::Shutdown_Fleet_Eta_Adapter(&fleet_adapter);
        fleet_active = false;
        if (fleet_pool != nullptr) {
            free((allocate::s_alloc_header*)fleet_pool - 1);
            MOX::global_data_seg = old_pool;
            fleet_pool = nullptr;
        }
        staged.reset();
        game_events::Unsubscribe(subscription);
        game_events::Unsubscribe(identities);
        active = false;
        if (!resource_root.empty()) {
            ship_read::Clear_Strings();
            vfs::Shutdown();
            std::error_code error;
            std::filesystem::remove_all(resource_root, error);
            resource_root.clear();
        }
    }
} // namespace test_native_ships
