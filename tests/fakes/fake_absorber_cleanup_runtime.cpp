#include "pch.h"

namespace {
    s_combat_data combat_data[2];
    int warning_count;
    int combat_refresh_count;
    std::string last_warning;
}

namespace TEST_ABSORBER_CLEANUP {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(TECHDATA::hull_data, 0, sizeof(TECHDATA::hull_data));
        memset(TECHDATA::armor, 0, sizeof(TECHDATA::armor));
        memset(TECHDATA::specials, 0, sizeof(TECHDATA::specials));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        combat_data[1].structure_max = 100;
        combat_data[1].drive_current_hits = 1;
        combat_data[1].ftl_type = 1;
        combat_data[1].size_class = HULL_CRUISER;
        TECHDATA::hull_data[HULL_CRUISER].drive_hp = 30;
        CMBTFIR2::ship_explosion = {5, 3};
        CMBTFIR2::systems_repair_percent = 50;
        COMBINIT::satellite_size_class_plus_one = 0;
        MOX::apply_damage_to_planet = 0;
        warning_count = 0;
        combat_refresh_count = 0;
        last_warning.clear();
    }

    void Set_Special_(int16_t ship_idx, int16_t special_id) {
        combat_data[ship_idx].special_device_flags[special_id / 8] |=
            (uint8_t)(1u << (special_id % 8));
    }

    int Warning_Count_() {
        return warning_count;
    }

    const char* Last_Warning_() {
        return last_warning.c_str();
    }

    int Combat_Refresh_Count_() {
        return combat_refresh_count;
    }
}

namespace MOX {
    s_combat_data* combat_data = ::combat_data;
    uint16_t apply_damage_to_planet;
}

namespace TECHDATA {
    s_tech_hull_data hull_data[HULL_CLASS_COUNT];
    s_tech_armor_data armor[ARMOR_COUNT];
    s_tech_specials_data specials[SPECIAL_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];
}

namespace DESIGN {
    int16_t __cdecl One_Weapon_Space_Without_M_Level(
        int16_t, int16_t, int16_t rack_type, int16_t) {
        return rack_type == 20 ? 60 : 20;
    }
}

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index) {
        const uint8_t* bytes = static_cast<const uint8_t*>(bit_field);
        return (bytes[bit_index / 8] >> (bit_index % 8)) & 1;
    }

    void __cdecl Clear_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bytes = static_cast<uint8_t*>(bit_field);
        bytes[bit_index / 8] &= (uint8_t)~(1u << (bit_index % 8));
    }
}

namespace COMBINIT {
    int16_t __cdecl Does_Combat_Ship_Have_Special(
        int16_t ship_idx, int16_t special_id) {
        uint8_t mask = (uint8_t)(1u << (special_id % 8));
        s_combat_data* ship = &combat_data[ship_idx];
        return (ship->special_device_flags[special_id / 8] & mask) != 0 &&
               (ship->special_device_damage_flags[special_id / 8] & mask) == 0;
    }

    int16_t __cdecl Calc_Combat_Shield_Max(int16_t) {
        return 0;
    }
}

namespace INITSHIP {
    int16_t __cdecl Get_Ship_Armor_Hits(int16_t) {
        return 0;
    }
}

namespace palette {
    void __cdecl Refresh_Palette() {
    }
}

namespace GENDRAW {
    void __cdecl Warning_Box(const char* message) {
        warning_count++;
        last_warning = message;
    }
}

namespace CMBTDRW1 {
    void __cdecl Refresh_Combat_Screen_Full() {
        combat_refresh_count++;
    }
}