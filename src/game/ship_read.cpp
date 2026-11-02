#include "pch.h"
#include "ship_read.h"
#include "game_events.h"

#include <cstdio>
#include <cstring>

namespace ship_read {
    bool Flag_Present(const void* flags, uint16_t bit) {
        return struct_::Test_Bit_Field(const_cast<void*>(flags), bit) != 0;
    }
    int16_t Special_Count(bool detailed) {
        return detailed && COMBINIT::amoeba_regeneration != 0 ? SPECIAL_COUNT : SPECIAL_REGENERATION;
    }
    namespace {
        char arc_labels[4][8];
        bool arc_labels_ready = false;
        char crew_labels[CREW_QUALITY_COUNT][256];
        bool crew_labels_ready = false;

        bool H_String_Ready(int16_t id) {
            const size_t offset = MOX::h_string_ix[id];
            return offset < MOX::h_strings.size() && memchr(MOX::h_strings.data() + offset, 0, MOX::h_strings.size() - offset) != nullptr;
        }
    } // namespace

    void Count_Member(const s_ship_design* design, s_composition* out) {
        ++out->total;
        switch (design->ship_type) {
            case SHIP_TYPE_COMBAT: ++out->counts[design->size > SHIP_SIZE_DOOM_STAR ? SHIP_SIZE_DOOM_STAR : design->size]; break;
            case SHIP_TYPE_COLONY: ++out->counts[6]; break;
            case SHIP_TYPE_OUTPOST: ++out->counts[7]; break;
            case SHIP_TYPE_TRANSPORT: ++out->counts[8]; break;
            default: break;
        }
    }

    bool Composition_Line(int16_t category, int16_t count, char* out, uint32_t size) {
        if (size == 0) return false;
        out[0] = '\0';
        if (category < 0 || category >= 9 || count <= 0) return false;
        char line[200];
        if (category < 6) {
            const char* label = Hull_Name(category, count != 1);
            if (label == nullptr) return false;
            snprintf(line, sizeof(line), "%2d %s", (int)count, label);
        } else {
            const int16_t id = 0x164 + (category - 6) * 2 + (count == 1 ? 0 : 1);
            if (!H_String_Ready(id)) return false;
            snprintf(line, sizeof(line), HAROLD::H_Message(id), (int)count);
        }
        strlcpy(out, line, size);
        return true;
    }

    bool Fleet_Title(int16_t owner, const s_ship_data* head, char* out, uint32_t size) {
        if (size == 0) return false;
        out[0] = '\0';
        char title[120];
        if (owner >= 0 && owner < 9) {
            if (!H_String_Ready(0x66) || MOX::player == nullptr || (owner == 8 && MOX::race_names[13] == nullptr)) return false;
            char race[80];
            HACCESS::Race_Name(owner, race, sizeof(race));
            snprintf(title, sizeof(title), HAROLD::H_Message(0x66), race);
        } else {
            if (head == nullptr || memchr(head->d.name, 0, sizeof(head->d.name)) == nullptr) return false;
            strlcpy(title, head->d.name, sizeof(title));
            strings::String_To_Upper(title);
        }
        strlcpy(out, title, size);
        return true;
    }

    e_label Scanned_Label(int16_t viewer, const s_ship_data* ship, char* out, uint32_t size) {
        if (size == 0) return NO_LABEL;
        out[0] = '\0';
        if (ship->owner != viewer && ship->owner <= 8 && ship->d.ship_type == SHIP_TYPE_COMBAT) {
            if (ship->d.size < 0 || ship->d.size >= HULL_CLASS_COUNT || Hull_Name(ship->d.size, false) == nullptr) return NO_LABEL;
            strlcpy(out, Hull_Name(ship->d.size, false), size);
            return HULL_LABEL;
        }
        if (memchr(ship->d.name, 0, sizeof(ship->d.name)) == nullptr) return NO_LABEL;
        strlcpy(out, ship->d.name, size);
        return CANONICAL_NAME;
    }

    void Load_Strings() {
        game_events::Publish_Designs_Replacing(MOX::PLAYER_NUM);
        Clear_Strings();
        const int16_t ids[] = {3, 4, 6, 5};
        for (int i = 0; i < 4; ++i) KEN::Ken_Get_Text_Message(ids[i], arc_labels[i], 6);
        const int16_t experience[] = {0, 100, 200, 300, 300};
        for (int i = 0; i < CREW_QUALITY_COUNT; ++i) {
            char format[80];
            KEN::Ken_Get_Text_Message(0x1f + i, format, sizeof(format));
            snprintf(crew_labels[i], sizeof(crew_labels[i]), format, experience[i]);
        }
        arc_labels_ready = true;
        crew_labels_ready = true;
    }

    void Clear_Strings() {
        arc_labels_ready = crew_labels_ready = false;
    }

    void Strip_Crew_Parenthetical(char* text) {
        char* paren = strchr(text, '(');
        if (paren != nullptr) *paren = '\0';
    }

    bool Detail_Crew_Text(int16_t quality, char* out, uint32_t size) {
        if (size == 0) return false;
        out[0] = '\0';
        if (!crew_labels_ready || quality < 0 || quality >= CREW_QUALITY_COUNT) return false;
        strlcpy(out, crew_labels[quality], size);
        Strip_Crew_Parenthetical(out);
        return true;
    }

    bool Copy_Weapon_Arc(uint16_t arc, char* out, uint32_t size) {
        if (size == 0) return false;
        out[0] = '\0';
        const int16_t flags[] = {WEAPON_FIRING_ARC_FORWARD, WEAPON_FIRING_ARC_FORWARD_EXTENDED, WEAPON_FIRING_ARC_BACK_EXTENDED, WEAPON_FIRING_ARC_REAR};
        for (int i = 0; i < 4; ++i) {
            if ((arc & flags[i]) == 0) continue;
            if (!arc_labels_ready) return false;
            strlcpy(out, arc_labels[i], size);
            return true;
        }
        if (arc & WEAPON_FIRING_ARC_360) strlcpy(out, "360", size);
        return true;
    }

    int16_t Combat_Arc(int16_t design_arc) {
        return design_arc == WEAPON_FIRING_ARC_360 ? WEAPON_FIRING_ARC_ALL_SECTORS : design_arc;
    }

    int16_t Combat_Arc_Label_Id(int16_t arc) {
        switch (arc) {
            case WEAPON_FIRING_ARC_FORWARD: return 3;
            case WEAPON_FIRING_ARC_FORWARD_EXTENDED: return 4;
            case WEAPON_FIRING_ARC_BACK_EXTENDED: return 6;
            case WEAPON_FIRING_ARC_REAR: return 5;
            case WEAPON_FIRING_ARC_ALL_SECTORS: return 0;
            default: return -1;
        }
    }

    bool Copy_Combat_Weapon_Arc(int16_t arc, char* out, uint32_t size) {
        if (size == 0) return false;
        out[0] = '\0';
        const int16_t label = Combat_Arc_Label_Id(arc);
        if (label < 0) return false;
        if (label == 0) {
            strlcpy(out, "360", size);
            return true;
        }
        if (!arc_labels_ready) return false;
        const int16_t ids[] = {3, 4, 6, 5};
        for (int i = 0; i < 4; ++i)
            if (label == ids[i]) strlcpy(out, arc_labels[i], size);
        return true;
    }

    bool Same_Fleet(const s_ship_data* a, const s_ship_data* b) {
        s_fleet_key left, right;
        Fleet_Key(a, &left);
        Fleet_Key(b, &right);
        return left == right;
    }
    void Fleet_Key(const s_ship_data* ship, s_fleet_key* out) {
        out->owner = ship->owner;
        out->location = ship->location;
        out->x = ship->x;
        out->y = ship->y;
    }

    int16_t Member_Order(const s_ship_design* design) {
        switch (design->ship_type) {
            case SHIP_TYPE_COMBAT: return design->size * 50 + design->picture_num;
            case SHIP_TYPE_COLONY: return -1;
            case SHIP_TYPE_TRANSPORT: return -2;
            case SHIP_TYPE_OUTPOST: return -3;
            default: return -4;
        }
    }
    void Order_Member_Entries(int16_t* entries, int16_t count) {
        if (count > 1) qsort(entries, count, 2 * sizeof(int16_t), SHIPSTAK::qsort_ships_by_type);
    }

    bool Crew_Text(int16_t quality, char* out, uint32_t size) {
        if (size == 0) return false;
        out[0] = '\0';
        if (quality < CREW_QUALITY_GREEN || quality > CREW_QUALITY_ULTRA_ELITE) return false;
        if (!H_String_Ready(0x8A + quality)) return false;
        const char* text = HAROLD::H_Message(0x8A + quality);
        if (text == nullptr) return false;
        strlcpy(out, text, size);
        return true;
    }

    bool Format_Fleet_Crew_Text(const char* quality, uint16_t experience, char* out, uint32_t size) {
        if (size == 0) return false;
        char native[120];
        const int count = snprintf(native, sizeof(native), "%s (%u EP)", quality, experience);
        strlcpy(out, native, size);
        return count >= 0 && (size_t)count < sizeof(native);
    }

    bool Copy_Weapon_Modifications(int16_t mask, char* out, uint32_t size) {
        if (size == 0) return false;
        out[0] = '\0';
        if (mask == 0 && !H_String_Ready(HESTR_064_NO_MODIFICATIONS)) return false;
        for (int bit = 0; bit < WEAPON_MOD_COUNT; ++bit)
            if (((uint16_t)mask & (1u << bit)) && TECHDATA::weapon_modifications[bit].name_plural == nullptr) return false;
        DESIGN::Weapon_Mod_String(mask, out, size);
        return true;
    }

    const char* Type_Name(const s_ship_design* design) {
        if (design->ship_type == SHIP_TYPE_COMBAT) return Hull_Name(design->size, false);
        return MOX::ship_type_names[(uint8_t)design->ship_type][(uint8_t)MOX::settings.language];
    }

    const char* Hull_Name(int16_t hull, bool plural) {
        return plural ? TECHDATA::hull_data[hull].size_name : TECHDATA::hull_data[hull].name;
    }

    void Condition(int16_t ship, s_condition* out) {
        *out = {};
        out->structure_max = MOX::settings.strategic_combat_flag == 1 ? AIPOWER::Max_Ship_Hits(ship) : INITSHIP::Get_Ship_Structure(ship);
        out->structure = out->structure_max - MOX::ship[ship].structural_damage;
        out->armor_max = MOX::settings.strategic_combat_flag == 1 ? 0 : INITSHIP::Get_Ship_Armor_Hits(ship);
        out->armor = out->armor_max - (MOX::settings.strategic_combat_flag == 1 ? 0 : MOX::ship[ship].armor_damage);
        const auto* item = &MOX::ship[ship];
        s_ship_design design;
        INITSHIP::Copy_Current_Ship_Configuration(item->owner, &item->d, &design);
        Subsystem_Maxima(design.size, design.ftl_type, design.computer_type, design.shield_type, design.special_device_flags, item->special_device_damage_flags, &out->drive_max, &out->computer_max, &out->shield_max);
        out->drive = (uint16_t)((int32_t)(100 - item->drive_damage_percent) * out->drive_max / 100);
        out->shield = (uint16_t)((int32_t)(100 - item->shield_damage_percent) * out->shield_max / 100);
        out->computer = (uint16_t)(out->computer_max - item->computer_damage);
        if (design.shield_type > 0 && out->shield_max == 0) {
            out->available = false;
            return;
        }
        const bool ordinary = MOX::settings.strategic_combat_flag != 1 && design.ship_type == SHIP_TYPE_COMBAT;
        out->subsystems = ordinary;
        out->drive_visible = ordinary && design.ftl_type > DRIVE_NO_DRIVE;
        const int16_t display_owner = item->owner >= MAX_PLAYERS ? (int8_t)item->d.previous_owner : item->owner;
        out->warp_speed = out->drive_visible ? INITSHIP::Get_FTL_Speed(display_owner, design.ftl_type) : 0;
        if (ordinary) {
            out->combat_speed = design.combat_speed;
            if (Flag_Present(design.special_device_flags, SPECIAL_AUGMENTED_ENGINES) && !Flag_Present(item->special_device_damage_flags, SPECIAL_AUGMENTED_ENGINES)) out->combat_speed += DESIGN::augmented_engines_speed_bonus;
            out->combat_speed = COMBINIT::Damaged_Drive_Combat_Speed(out->combat_speed, out->drive, out->drive_max);
            if (display_owner >= 0 && display_owner < MAX_PLAYERS && MOX::player[display_owner].traits[TRAIT_TRANS_DIMENSIONAL] != 0) out->combat_speed += COMBAT1::td_combat_speed_bonus;
        }
        out->shield_strength = Shield_Strength(design.size, design.shield_type, design.special_device_flags, item->special_device_damage_flags, out->shield, out->shield_max);
        out->shield_strength_max = out->shield_strength;
        if (ordinary) {
            out->shield_strength_max = TECHDATA::Shield_Strength(design.size, design.shield_type);
            if (Flag_Present(design.special_device_flags, SPECIAL_MULTI_PHASED_SHIELDS) && !Flag_Present(item->special_device_damage_flags, SPECIAL_MULTI_PHASED_SHIELDS)) out->shield_strength_max = TECHDATA::Apply_Multi_Phased_Shields_Bonus(out->shield_strength_max);
        }
    }

    void Subsystem_Maxima(int16_t size, int16_t drive, int16_t computer, int16_t shield, const void* specials, const void* damage, uint16_t* drive_max, uint16_t* computer_max, uint16_t* shield_max) {
        *drive_max = drive <= 0 ? 0 : TECHDATA::hull_data[size].drive_hp;
        if (Flag_Present(specials, 27) && !Flag_Present(damage, 27)) *drive_max *= 3;
        *computer_max = computer > COMPUTER_NO_COMPUTER ? TECHDATA::hull_data[size].computer_hp : 0;
        *shield_max = shield <= 0 ? 0 : TECHDATA::hull_data[size].shield_hp;
    }

    int16_t Shield_Strength(int16_t size, int16_t shield, const void* specials, const void* damage, uint16_t current_hits, uint16_t max_hits) {
        if (shield <= 0) return 0;
        int16_t value;
        if (MOX::settings.strategic_combat_flag == 1) {
            value = TECHDATA::shields[shield].shield_class;
            if (Flag_Present(specials, SPECIAL_HARD_SHIELDS) && !Flag_Present(damage, SPECIAL_HARD_SHIELDS)) value += TECHDATA::hard_shields_damage_reduction;
        } else
            value = (int16_t)TECHDATA::Shield_Strength(size, shield);
        if (Flag_Present(specials, SPECIAL_MULTI_PHASED_SHIELDS) && !Flag_Present(damage, SPECIAL_MULTI_PHASED_SHIELDS)) value = (int16_t)TECHDATA::Apply_Multi_Phased_Shields_Bonus(value);
        return (int16_t)((value * current_hits + max_hits - 1) / max_hits);
    }

    bool Valid_Design(const s_ship_design* design) {
        if (design == nullptr || memchr(design->name, 0, sizeof(design->name)) == nullptr ||
            design->size < 0 || design->size >= SHIP_SIZE_COUNT ||
            (design->ship_type != SHIP_TYPE_COMBAT && design->ship_type != SHIP_TYPE_COLONY && design->ship_type != SHIP_TYPE_TRANSPORT && design->ship_type != SHIP_TYPE_OUTPOST) ||
            design->shield_type < 0 || design->shield_type >= SHIELD_COUNT || design->computer_type < 0 || design->computer_type >= COMPUTER_COUNT ||
            design->armor_type < 0 || design->armor_type >= ARMOR_COUNT || design->ftl_type < 0 || design->ftl_type >= DRIVE_COUNT ||
            (uint8_t)MOX::settings.language >= 6) return false;
        for (int i = 0; i < 8; ++i) {
            const auto* weapon = &design->ship_weapon[i];
            if (weapon->type < 0 || weapon->type >= WEAPON_COUNT) return false;
        }
        return true;
    }

    const char* Shield_Name(const s_ship_design* design) {
        return TECHDATA::shields[design->shield_type].name;
    }
    const char* Computer_Name(const s_ship_design* design) {
        return TECHDATA::computers[design->computer_type].name;
    }
    const char* Armor_Name(const s_ship_design* design) {
        return TECHDATA::armor[design->armor_type].name;
    }
    const char* Drive_Name(const s_ship_design* design) {
        return TECHDATA::drives[design->ftl_type].name;
    }

    bool Committed_Queue_Ship(int16_t viewer, int16_t ship, int16_t* colony) {
        if (MOX::ship[ship].owner != viewer || (MOX::ship[ship].status != 4 && MOX::ship[ship].status != 6)) return false;
        for (int16_t index = 0; index < MOX::NUM_COLONIES; ++index) {
            const auto* item = &MOX::colony[index];
            if (item->owner != viewer || item->outpost_flag != 0) continue;
            for (int row = 0; row < 7; ++row) {
                const int16_t product = item->producing[row];
                if (COLBLDG::Colony_Production_Is_Queued_Ship(product) && COLBLDG::Colony_Production_Ship_Index(product) == ship) {
                    *colony = index;
                    return true;
                }
            }
        }
        return false;
    }
} // namespace ship_read
