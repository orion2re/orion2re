#include "pch.h"
#include "colony_read.h"
#include "build_queue.h"

#include <cstdio>
#include <cstring>

namespace colony_read {
    bool Strategic_Population_Disclosed() {
        const size_t offset = MOX::h_string_ix[0x181];
        if (offset >= MOX::h_strings.size() || memchr(MOX::h_strings.data() + offset, 0, MOX::h_strings.size() - offset) == nullptr) return false;
        const char* format = HAROLD::H_Message(0x181);
        if (format == nullptr) return false;
        uint32_t prefix = 0;
        for (const char* p = format; *p != 0; ++p) {
            if (*p != '%') {
                ++prefix;
                continue;
            }
            if (p[1] == '%') {
                ++prefix;
                ++p;
                continue;
            }
            return (p[1] == 'u' || p[1] == 'd') && prefix + 2 < 80;
        }
        return false;
    }

    bool Strategic_Owner_Disclosed(const char* format, int16_t argument, uint32_t capacity) {
        if (format == nullptr || (argument != 1 && argument != 3)) return false;
        uint32_t prefix = 0;
        int16_t current = 1;
        for (const char* p = format; *p != 0; ++p) {
            if (*p != '%') {
                ++prefix;
                continue;
            }
            if (p[1] == '%') {
                ++prefix;
                ++p;
                continue;
            }
            if (p[1] != 's') return false;
            if (current == argument) return prefix + 14 < capacity;
            prefix += current == 1 ? 2 : 30;
            ++current;
            ++p;
        }
        return false;
    }

    bool Strategic_Population_Text(const s_colony* colony, int16_t maximum, char* out, uint32_t size) {
        if (out == nullptr || size == 0) return false;
        out[0] = 0;
        const char* format = HAROLD::H_Message(colony != nullptr ? 0x181 : 0x162);
        if (format == nullptr) return false;
        if (colony != nullptr)
            snprintf(out, size, format, (uint32_t)(uint8_t)colony->n_pops, (int32_t)maximum);
        else
            snprintf(out, size, format, (int32_t)maximum);
        return colony == nullptr || Strategic_Population_Disclosed();
    }

    bool System_Settlements_Visible(int16_t system, int16_t viewer) {
        return SYS::Ok_To_View_System(system, viewer);
    }

    bool Strategic_Defenses_Visible(int16_t system, int16_t viewer, bool local_ship) {
        return local_ship || HAROLD::Player_Has_Colony(viewer, system) || MOX::player[viewer].traits[TRAIT_OMNISCIENCE] != 0;
    }

    bool Strategic_Local_Ship(const s_ship_data* ship, int16_t system, int16_t viewer) {
        return ship->owner == viewer && ship->status >= 0 && ship->status < 3 && ship->location == system;
    }

    int16_t Strategic_Defense(const s_colony* colony, int16_t ordinal) {
        const int16_t candidates[] = {BUILDING_STAR_FORTRESS, BUILDING_STAR_BASE, BUILDING_BATTLESTATION, BUILDING_MISSILE_BASE, BUILDING_GROUND_BATTERIES, BUILDING_STELLAR_CONVERTER, BUILDING_FIGHTER_GARRISON};
        for (int16_t building : candidates)
            if (colony->buildings[building] != 0 && ordinal-- == 0) return building;
        return -1;
    }

    bool Strategic_Climate(const s_colony* colony, int16_t planet, char* out, uint32_t size) {
        if (out == nullptr || size == 0) return false;
        out[0] = 0;
        const auto* location = &MOX::planet[planet];
        const auto climate = colony != nullptr ? colony->climate : location->climate;
        if (location->size < 0 || location->size >= PLANET_SIZE_COUNT || climate >= PLANET_CLIMATE_COUNT ||
            MOX::planet_size_string[location->size] == nullptr || MOX::planet_climate_string[climate] == nullptr) return false;
        snprintf(out, size, "%s, %s", MOX::planet_size_string[location->size], MOX::planet_climate_string[climate]);
        return true;
    }

    int16_t Status_String_Id(int16_t index, int16_t viewer) {
        const int16_t star = MOX::planet[MOX::colony[index].planet].star_index;
        if ((MOX::star[star].blockaded >> (viewer & 0x1f)) & 1) return ESTR_BLOCKADED1;
        if (EVENTS::Event_Check_Plague(index)) return ESTR_PLAGUE;
        if (EVENTS::Event_Check_Population_Boom(index)) return ESTR_POP_BOOM;
        return -1;
    }

    int32_t Inbound_Population(const s_colony* colony) {
        int32_t count = 0;
        const auto& owner = MOX::player[colony->owner];
        for (int16_t i = 0; i < owner.settlers_freighted; ++i)
            if (SETTLER::Get_Dest_Planet(owner.settlers[i].val) == colony->planet) ++count;
        return count;
    }

    bool Can_Buy(int16_t index, int16_t viewer) {
        const auto& colony = MOX::colony[index];
        if (colony.owner != viewer) return false;
        if (!COLCALC::Colony_Can_Afford_To_Buy_Product(index) || colony.bought_outright != 0) return false;
        const auto& star = MOX::star[MOX::planet[colony.planet].star_index];
        return ((star.blockaded >> (viewer & 0x1f)) & 1) == 0 && COLCALC::Colony_Cost_To_Buy_Producing_Product(&MOX::colony[index]) > 0;
    }
    bool Own_Full_Colony(const s_colony* colony, int16_t viewer) {
        return colony != nullptr && colony->owner == viewer && colony->outpost_flag == 0;
    }

    bool Building_Present(const s_colony* colony, int16_t building) {
        return colony->buildings[building] != 0;
    }

    bool Satellite_Replaced(const s_colony* colony, int16_t building) {
        if (building == BUILDING_STAR_BASE)
            return colony->buildings[BUILDING_BATTLESTATION] != 0 || colony->buildings[BUILDING_STAR_FORTRESS] != 0;
        return building == BUILDING_BATTLESTATION && colony->buildings[BUILDING_STAR_FORTRESS] != 0;
    }

    void Product(const s_colony* colony, int16_t product, s_product* out) {
        *out = {};
        if (COLBLDG::Colony_Production_Is_Building(product)) {
            out->kind = "building";
            if (TECHDATA::buildings[product].name == nullptr) return;
        } else if (COLBLDG::Colony_Production_Is_Queued_Ship(product)) {
            const int16_t index = COLBLDG::Colony_Production_Ship_Index(product);
            if (MOX::NUM_SHIPS < 0 || MOX::NUM_SHIPS > MAX_SHIPS || index < 0 || index >= MOX::NUM_SHIPS || MOX::ship == nullptr) return;
            const auto& ship = MOX::ship[index];
            if (ship.owner != colony->owner || (ship.status != 6 && ship.status != 4)) return;
            if (memchr(ship.d.name, 0, sizeof(ship.d.name)) == nullptr) return;
            if (ship.status == 4 && (ESTRINGS::E_Strings(45) == nullptr || COLBLDG::Read_Option_String(COLONY_PRODUCTION_REFIT, colony->owner) == nullptr)) return;
            out->kind = ship.status == 4 ? "refit" : "ship";
        } else if (COLBLDG::Colony_Production_Is_Ship_Design(product)) {
            if (COLBLDG::Colony_Production_Ship_Design_Index(product) >= 6) return;
            const auto& design = MOX::player[colony->owner].ship_designs[COLBLDG::Colony_Production_Ship_Design_Index(product)];
            if (memchr(design.name, 0, sizeof(design.name)) == nullptr) return;
            out->kind = "design";
        } else {
            switch (product) {
                case COLONY_PRODUCTION_NONE:
                case 0: out->kind = "none"; break;
                case COLONY_PRODUCTION_REPEAT: out->kind = "repeat"; break;
                case COLONY_PRODUCTION_HOUSING: out->kind = "housing"; break;
                case COLONY_PRODUCTION_TRADE_GOODS: out->kind = "trade-goods"; break;
                case COLONY_PRODUCTION_FREIGHTERS: out->kind = "freighters"; break;
                case COLONY_PRODUCTION_COLONY_SHIP: out->kind = "colony-ship"; break;
                case COLONY_PRODUCTION_OUTPOST_SHIP: out->kind = "outpost-ship"; break;
                case COLONY_PRODUCTION_TRANSPORT_SHIP: out->kind = "transport-ship"; break;
                case COLONY_PRODUCTION_SPY: out->kind = "spy"; break;
                case COLONY_PRODUCTION_FARMER: out->kind = "android-farmer"; break;
                case COLONY_PRODUCTION_WORKER: out->kind = "android-worker"; break;
                case COLONY_PRODUCTION_SCIENTIST: out->kind = "android-scientist"; break;
                case COLONY_PRODUCTION_REFIT: out->kind = "refit"; break;
                case COLONY_PRODUCTION_DOOM_STAR: out->kind = "doom-star"; break;
                case COLONY_PRODUCTION_CUSTOM_DESIGN: out->kind = "custom-design"; break;
                case COLONY_PRODUCTION_SEPARATOR: out->kind = "separator"; break;
                default: return;
            }
            if (COLBLDG::Read_Option_String(product, colony->owner) == nullptr) return;
        }
        COLBLDG::Copy_Selection_Name(product, out->label, sizeof(out->label), colony->owner);
        out->available = true;
    }

    void Population_Text(const s_colony* colony, char* out, uint32_t size) {
        int roundoff = 0, growth = 0, assigned = 0;
        for (int i = 0; i < 10; ++i) {
            roundoff += colony->pop_roundoff[i];
            growth += colony->pop_growth[i];
        }
        for (int i = 0; i < colony->n_pops; ++i) assigned += POP::Is_Assigned(colony->pop[i]);
        snprintf(out, size, ESTRINGS::E_Strings(ESTR_POP_D03D_K_DK), assigned + roundoff / 1000, roundoff % 1000, growth);
    }

    bool Heading(int16_t index, char* out, uint32_t size) {
        const auto& colony = MOX::colony[index];
        const bool annihilating = colony.occupation_policy == 0;
        const char* format = ESTRINGS::E_Strings(annihilating ? ESTR_ANNIHILATING_S : ESTR_SCOLONY_OF_S);
        const char* type = annihilating ? "" : COLONY::Colony_Type(index);
        const int16_t ordinal = HAROLD::Planet_Number(colony.planet) + 1;
        if (format == nullptr || type == nullptr || ordinal < 1 || ordinal > 5 || MOX::roman_literals[ordinal] == nullptr) return false;
        if (strcmp(MOX::roman_literals[ordinal], "I") == 0) {
            const size_t offset = MOX::h_string_ix[0xa0];
            if (offset >= MOX::h_strings.size() || memchr(MOX::h_strings.data() + offset, 0, MOX::h_strings.size() - offset) == nullptr) return false;
        }
        char planet[31];
        HACCESS::Do_Get_Planet_Name(colony.planet, planet, 1);
        if (annihilating)
            snprintf(out, size, format, planet);
        else
            snprintf(out, size, format, type, planet);
        return true;
    }

    bool Autobuild_Text(const s_colony* colony, char* out, uint32_t size) {
        const int preset = BUILD_QUEUE::autobuild_settings.enabled != 0 ? BUILD_QUEUE::Decode_Autobuild_Preset(colony->auto_building) : -1;
        if (preset >= 0)
            snprintf(out, size, "Auto Build Queue %d", preset);
        else
            strlcpy(out, BUILD_QUEUE::Is_Legacy_Autobuild(colony->auto_building) ? ESTRINGS::E_Strings(0xbd) : "", size);
        return preset >= 0 || BUILD_QUEUE::Is_Legacy_Autobuild(colony->auto_building);
    }

    const char* Event_Label(int16_t event) {
        const int16_t strings[36] = {0x104, 0xdd, 0xf0, 0x227, 0xcc, 0x171, 0x10e, 0x117, 0xa7, 0x127, 0xa7, 0x102, 0x103, 0x17d, 0x19e, 0x19d, 0x19f, 0xce, 0x1d2, 0xad, 0xf6, 0x110, 0x119, 0x144, 0x18e, 0xb6, 0xc6, 0x12e, 0x22e, 0x1b6, 0x1a1, 0x135, 0x193, 0xff, 0x1f4, 0x186};
        return event >= 0 && event < 36 ? ESTRINGS::E_Strings(strings[event]) : nullptr;
    }
} // namespace colony_read
