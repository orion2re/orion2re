#pragma once

#include <cstdint>

struct s_colony;
struct s_ship_data;

namespace colony_read {
    struct s_resource_row {
        int16_t net_tens, net_units;
        int16_t import_tens, import_units;
        int16_t secondary_tens, secondary_units;
        int16_t shortage_tens, shortage_units;
    };

    struct s_product {
        bool available = false;
        const char* kind = "unavailable";
        char label[800] = {};
    };

    bool Own_Full_Colony(const s_colony* colony, int16_t viewer);
    bool System_Settlements_Visible(int16_t system, int16_t viewer);
    bool Strategic_Defenses_Visible(int16_t system, int16_t viewer, bool local_ship);
    bool Strategic_Local_Ship(const s_ship_data* ship, int16_t system, int16_t viewer);
    int16_t Strategic_Defense(const s_colony* colony, int16_t ordinal);
    bool Strategic_Climate(const s_colony* colony, int16_t planet, char* out, uint32_t size);
    bool Strategic_Population_Text(const s_colony* colony, int16_t maximum, char* out, uint32_t size);
    bool Strategic_Population_Disclosed();
    bool Strategic_Owner_Disclosed(const char* format, int16_t argument, uint32_t capacity);
    void Resource_Row(const s_colony* colony, int16_t resource, s_resource_row* out);
    int16_t Population_Order(int16_t colony, int16_t job, int16_t* out);
    int16_t Morale_Icons(const s_colony* colony, bool* suppressed);
    bool Satellite_Replaced(const s_colony* colony, int16_t building);
    bool Building_Present(const s_colony* colony, int16_t building);
    void Product(const s_colony* colony, int16_t product, s_product* out);
    void Population_Text(const s_colony* colony, char* out, uint32_t size);
    bool Heading(int16_t colony, char* out, uint32_t size);
    bool Autobuild_Text(const s_colony* colony, char* out, uint32_t size);
    const char* Event_Label(int16_t event);
    int16_t Status_String_Id(int16_t colony, int16_t viewer);
    int32_t Inbound_Population(const s_colony* colony);
    bool Can_Buy(int16_t colony, int16_t viewer);
} // namespace colony_read
