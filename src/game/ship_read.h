#pragma once

#include <cstdint>

struct s_ship_data;
struct s_ship_design;

namespace ship_read {
    struct s_fleet_key {
        int16_t owner, location, x, y;
        bool operator==(const s_fleet_key&) const = default;
    };
    struct s_condition {
        bool available = true;
        int32_t structure = 0;
        int16_t structure_max = 0;
        int16_t armor = 0;
        int16_t armor_max = 0;
        uint16_t drive = 0, drive_max = 0;
        uint16_t computer = 0, computer_max = 0;
        uint16_t shield = 0, shield_max = 0;
        int16_t combat_speed = 0;
        int16_t warp_speed = 0;
        int16_t shield_strength = 0;
        int32_t shield_strength_max = 0;
        bool subsystems = false;
        bool drive_visible = false;
    };

    bool Same_Fleet(const s_ship_data* a, const s_ship_data* b);
    void Fleet_Key(const s_ship_data* ship, s_fleet_key* out);
    bool Flag_Present(const void* flags, uint16_t bit);
    int16_t Special_Count(bool detailed);
    int16_t Member_Order(const s_ship_design* design);
    void Order_Member_Entries(int16_t* entries, int16_t count);
    bool Crew_Text(int16_t quality, char* out, uint32_t size);
    bool Format_Fleet_Crew_Text(const char* quality, uint16_t experience, char* out, uint32_t size);
    bool Copy_Weapon_Modifications(int16_t mask, char* out, uint32_t size);
    const char* Type_Name(const s_ship_design* design);
    const char* Hull_Name(int16_t hull, bool plural);
    void Condition(int16_t ship, s_condition* out);
    void Subsystem_Maxima(int16_t size, int16_t drive, int16_t computer, int16_t shield, const void* specials, const void* damage, uint16_t* drive_max, uint16_t* computer_max, uint16_t* shield_max);
    int16_t Shield_Strength(int16_t size, int16_t shield, const void* specials, const void* damage, uint16_t current_hits, uint16_t max_hits);
    bool Valid_Design(const s_ship_design* design);
    bool Committed_Queue_Ship(int16_t viewer, int16_t ship, int16_t* colony);
    const char* Shield_Name(const s_ship_design* design);
    const char* Computer_Name(const s_ship_design* design);
    const char* Armor_Name(const s_ship_design* design);
    const char* Drive_Name(const s_ship_design* design);
    void Load_Strings();
    void Clear_Strings();
    bool Detail_Crew_Text(int16_t quality, char* out, uint32_t size);
    void Strip_Crew_Parenthetical(char* text);
    bool Copy_Weapon_Arc(uint16_t arc, char* out, uint32_t size);
    int16_t Combat_Arc(int16_t design_arc);
    int16_t Combat_Arc_Label_Id(int16_t arc);
    bool Copy_Combat_Weapon_Arc(int16_t arc, char* out, uint32_t size);
} // namespace ship_read
