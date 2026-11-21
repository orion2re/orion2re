#pragma once

#include "planet_reference.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr uint16_t SHIP_MISSION_PLANETS_FLAG = 0x8000;
    inline constexpr size_t LEGACY_SHIP_RECORD_BYTES = 0x81;
    inline constexpr size_t SHIP_RECORD_BYTES = LEGACY_SHIP_RECORD_BYTES + sizeof(uint32_t);
    inline constexpr size_t SHIP_PACKET_BYTES = SHIP_RECORD_BYTES + sizeof(int16_t) + sizeof(int32_t);

    inline game::id_planet Decode_Legacy_Colonization_Target(s_ship_data& ship) {
        if ((ship.d.ship_type != SHIP_TYPE_COLONY && ship.d.ship_type != SHIP_TYPE_OUTPOST) ||
            ship.status >= 3 || ship.d.ship_weapon[1].type != 1) return game::NO_PLANET;
        const auto id = Decode_Legacy_Planet_Reference(ship.d.ship_weapon[0].type);
        ship.d.ship_weapon[0].type = -1;
        ship.d.ship_weapon[1].type = 0;
        return id;
    }

    inline void Write_Ship_Weapons(Writer_* writer, const s_ship_weapons* weapons) {
        writer->Write_Int16(weapons->type);
        writer->Write_Int8(weapons->count);
        writer->Write_Int8(weapons->current_count);
        writer->Write_Int8(weapons->firing_arc);
        writer->Write_Uint16(weapons->specials);
        writer->Write_Int8(weapons->ammo);
    }

    inline void Read_Ship_Weapons(Reader_* reader, s_ship_weapons* weapons) {
        weapons->type = reader->Read_Int16();
        weapons->count = reader->Read_Int8();
        weapons->current_count = reader->Read_Int8();
        weapons->firing_arc = reader->Read_Int8();
        weapons->specials = reader->Read_Uint16();
        weapons->ammo = reader->Read_Int8();
    }

    inline void Write_Ship_Design(Writer_* writer, const s_ship_design* design) {
        writer->Write_Bytes(design->name, sizeof(design->name));
        writer->Write_Int8(design->size);
        writer->Write_Int8(design->ship_type);
        writer->Write_Int8(design->shield_type);
        writer->Write_Int8(design->ftl_type);
        writer->Write_Uint8(design->speed);
        writer->Write_Int8(design->computer_type);
        writer->Write_Int8(design->armor_type);
        writer->Write_Bytes(design->special_device_flags, sizeof(design->special_device_flags));
        for (int i = 0; i < 8; ++i) Write_Ship_Weapons(writer, &design->ship_weapon[i]);
        writer->Write_Int8(design->picture_num);
        writer->Write_Uint8(design->previous_owner);
        writer->Write_Int16(design->cost);
        writer->Write_Uint8(design->combat_speed);
        writer->Write_Uint16(design->date_of_design);
    }

    inline void Read_Ship_Design(Reader_* reader, s_ship_design* design) {
        reader->Read_Bytes(design->name, sizeof(design->name));
        design->size = reader->Read_Int8();
        design->ship_type = reader->Read_Int8();
        design->shield_type = reader->Read_Int8();
        design->ftl_type = reader->Read_Int8();
        design->speed = reader->Read_Uint8();
        design->computer_type = reader->Read_Int8();
        design->armor_type = reader->Read_Int8();
        reader->Read_Bytes(design->special_device_flags, sizeof(design->special_device_flags));
        for (int i = 0; i < 8; ++i) Read_Ship_Weapons(reader, &design->ship_weapon[i]);
        design->picture_num = reader->Read_Int8();
        design->previous_owner = reader->Read_Uint8();
        design->cost = reader->Read_Int16();
        design->combat_speed = reader->Read_Uint8();
        design->date_of_design = reader->Read_Uint16();
    }

    inline void Write_Ship_Record(Writer_* writer, const s_ship_data* ship) {
        Write_Ship_Design(writer, &ship->d);
        writer->Write_Int8(ship->owner);
        writer->Write_Int8(ship->status);
        writer->Write_Int16(ship->location);
        writer->Write_Int16(ship->x);
        writer->Write_Int16(ship->y);
        writer->Write_Int8(ship->group_has_navigator);
        writer->Write_Uint8(ship->travelling_speed);
        writer->Write_Uint8(ship->turns_left);
        writer->Write_Int8(ship->shield_damage_percent);
        writer->Write_Int8(ship->drive_damage_percent);
        writer->Write_Int8(ship->computer_damage);
        writer->Write_Int8(ship->crew_quality);
        writer->Write_Uint16(ship->crew_experience);
        writer->Write_Int16(ship->officer_index);
        writer->Write_Bytes(ship->special_device_damage_flags, sizeof(ship->special_device_damage_flags));
        writer->Write_Int16(ship->armor_damage);
        writer->Write_Int16(ship->structural_damage);
        writer->Write_Int8(ship->mission);
        writer->Write_Int8(ship->just_built);
    }

    inline void Read_Ship_Record(Reader_* reader, s_ship_data* ship) {
        Read_Ship_Design(reader, &ship->d);
        ship->owner = reader->Read_Int8();
        ship->status = reader->Read_Int8();
        ship->location = reader->Read_Int16();
        ship->x = reader->Read_Int16();
        ship->y = reader->Read_Int16();
        ship->group_has_navigator = reader->Read_Int8();
        ship->travelling_speed = reader->Read_Uint8();
        ship->turns_left = reader->Read_Uint8();
        ship->shield_damage_percent = reader->Read_Int8();
        ship->drive_damage_percent = reader->Read_Int8();
        ship->computer_damage = reader->Read_Int8();
        ship->crew_quality = reader->Read_Int8();
        ship->crew_experience = reader->Read_Uint16();
        ship->officer_index = reader->Read_Int16();
        reader->Read_Bytes(ship->special_device_damage_flags, sizeof(ship->special_device_damage_flags));
        ship->armor_damage = reader->Read_Int16();
        ship->structural_damage = reader->Read_Int16();
        ship->mission = reader->Read_Int8();
        ship->just_built = reader->Read_Int8();
        ship->prior_jump_gate_destination = -1;
        ship->prior_jump_gate_stardate = 0;
    }

    inline void Write_Ship(Writer_& writer, const s_ship_data& ship) {
        Write_Ship_Record(&writer, &ship);
        writer.Write_Uint32(ship.colonization_target.get().id);
    }

    inline void Read_Ship(Reader_& reader, s_ship_data& ship, bool typed_mission) {
        Read_Ship_Record(&reader, &ship);
        ship.colonization_target = typed_mission ? game::id_planet{reader.Read_Uint32()}
                                                 : Decode_Legacy_Colonization_Target(ship);
    }

    inline void Write_Ship_Packet(Writer_& writer, const s_ship_data& ship) {
        Write_Ship(writer, ship);
        writer.Write_Int16(ship.prior_jump_gate_destination);
        writer.Write_Int32(ship.prior_jump_gate_stardate);
    }

    inline void Read_Ship_Packet(Reader_& reader, s_ship_data& ship) {
        if (!reader.Require(SHIP_PACKET_BYTES)) return;
        Read_Ship(reader, ship, true);
        ship.prior_jump_gate_destination = reader.Read_Int16();
        ship.prior_jump_gate_stardate = reader.Read_Int32();
    }
} // namespace SERIAL
