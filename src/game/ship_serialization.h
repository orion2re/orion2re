#pragma once

#include "planet_reference.h"
#include "serialization.h"
#include "legacy_ship_location_codec.h"

namespace SERIAL {
    inline constexpr size_t LEGACY_SHIP_RECORD_BYTES = 0x81;
    inline constexpr size_t LEGACY_SHIP_DESIGN_BYTES = 99;
    inline constexpr size_t SHIP_LOCATION_OFFSET = 101;
    inline constexpr size_t TYPED_SHIP_LOCATION_BYTES = 2 + sizeof(uint32_t);
    inline constexpr size_t SHIP_MISSION_RECORD_BYTES = LEGACY_SHIP_RECORD_BYTES + sizeof(uint32_t);
    inline constexpr size_t SHIP_RECORD_BYTES = SHIP_MISSION_RECORD_BYTES - sizeof(int16_t) + TYPED_SHIP_LOCATION_BYTES;
    inline constexpr size_t KEYED_SHIP_RECORD_BYTES = SHIP_RECORD_BYTES + sizeof(uint32_t);
    inline constexpr size_t SHIP_PACKET_BYTES = KEYED_SHIP_RECORD_BYTES + sizeof(uint32_t) + sizeof(int32_t);

    inline void Write_Ship_Location(Writer_& writer, const game::s_ship_location& location) {
        writer.Write_Uint8(static_cast<uint8_t>(location.kind));
        writer.Write_Uint8(static_cast<uint8_t>(location.target));
        writer.Write_Uint32(location.star_id.id);
    }

    inline bool Read_Ship_Location(Reader_& reader, game::s_ship_location& location) {
        if (!reader.Require(TYPED_SHIP_LOCATION_BYTES)) return false;
        const auto kind = static_cast<game::e_ship_location>(reader.Read_Uint8());
        const auto target = static_cast<game::e_ship_target>(reader.Read_Uint8());
        const game::id_star star{reader.Read_Uint32()};
        if (kind > game::e_ship_location::in_wormhole || target > game::e_ship_target::none ||
            ((target == game::e_ship_target::star) != (star != game::NO_STAR)) ||
            (target == game::e_ship_target::none && kind != game::e_ship_location::at_star)) return false;
        location = {kind, star, target};
        return true;
    }

    inline bool Ship_Star_References_Are_Valid(const s_ship_data& ship, const game::s_game_state& state) {
        if (ship.status == 5) return true; // Retired slots may retain their old payload.
        const auto location = ship.position.Get();
        if (location.target == game::e_ship_target::star && !state.stars.contains(location.star_id)) return false;
        const auto redirect = ship.prior_jump_gate_destination;
        return ship.prior_jump_gate_stardate == 0 || redirect == game::NO_STAR || state.stars.contains(redirect);
    }

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
        writer->Write_Uint8(static_cast<uint8_t>(design->previous_owner.code));
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
        design->previous_owner = game::id_faction{reader->Read_Uint8()};
        design->cost = reader->Read_Int16();
        design->combat_speed = reader->Read_Uint8();
        design->date_of_design = reader->Read_Uint16();
    }

    inline void Write_Ship_Record(Writer_* writer, const s_ship_data* ship, const game::s_ship_location& location) {
        Write_Ship_Design(writer, &ship->d);
        writer->Write_Int8(static_cast<int8_t>(ship->owner.code));
        writer->Write_Int8(ship->status);
        Write_Ship_Location(*writer, location);
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

    inline bool Read_Ship_Record(Reader_* reader, s_ship_data* ship, bool typed_location, int16_t antares, bool classic) {
        Read_Ship_Design(reader, &ship->d);
        ship->owner = game::id_faction{reader->Read_Int8()};
        ship->status = reader->Read_Int8();
        game::s_ship_location location;
        if (typed_location) {
            if (!Read_Ship_Location(*reader, location)) return false;
        } else {
            location = legacy_world::Decode_Stored_Ship_Location(reader->Read_Int16(), antares, classic);
        }
        ship->position.Set(location);
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
        ship->prior_jump_gate_destination = game::NO_STAR;
        ship->prior_jump_gate_stardate = 0;
        return !reader->Failed();
    }

    inline void Write_Ship(Writer_& writer, const s_ship_data& ship) {
        Write_Ship_Record(&writer, &ship, ship.position.Get());
        writer.Write_Uint32(ship.colonization_target.id);
    }

    inline bool Read_Ship(Reader_& reader, s_ship_data& ship, bool wide, int16_t antares = -1, bool classic = false) {
        if (!Read_Ship_Record(&reader, &ship, wide, antares, classic)) return false;
        ship.colonization_target = wide ? game::id_planet{reader.Read_Uint32()}
                                                 : Decode_Legacy_Colonization_Target(ship);
        return !reader.Failed();
    }

    inline void Write_Ship_Packet(Writer_& writer, const s_ship_data& ship) {
        Write_Ship(writer, ship);
        writer.Write_Uint32(ship.id.id);
        writer.Write_Uint32(ship.prior_jump_gate_destination.id);
        writer.Write_Int32(ship.prior_jump_gate_stardate);
    }

    inline bool Read_Ship_Packet(Reader_& reader, s_ship_data& ship) {
        if (!reader.Require(SHIP_PACKET_BYTES) || !Read_Ship(reader, ship, true)) return false;
        ship.id = game::id_ship{reader.Read_Uint32()};
        ship.prior_jump_gate_destination = game::id_star{reader.Read_Uint32()};
        ship.prior_jump_gate_stardate = reader.Read_Int32();
        return !reader.Failed();
    }

    inline bool Read_Ship_Packet(Reader_& reader, s_ship_data& ship, const game::s_game_state& state) {
        s_ship_data incoming{};
        if (!Read_Ship_Packet(reader, incoming) || !Ship_Star_References_Are_Valid(incoming, state)) return false;
        ship = incoming;
        return true;
    }
} // namespace SERIAL
