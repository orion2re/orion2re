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
#include <filesystem>
#include <fstream>
#include <memory>
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
            if ((event == game_events::DESIGNS_REPLACING && game_events::Replaced_Design_Owner() == store->viewer) || event == game_events::SHIP_IDENTITIES_RESETTING) rpc::Expire_Game_Inspection(store);
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
