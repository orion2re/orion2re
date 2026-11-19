#include "pch.h"
#include "record_serialization.h"
#include "legacy_location.h"
#include "settler_serialization.h"
#include "savegame.h"

namespace SAVEGAME::records {
        void Write_Ship(SERIAL::Writer_* writer, const s_ship_data* ship) {
            Write_Ship_Design(writer, &ship->d);
            writer->Write_Int8(ship->owner);
            writer->Write_Int8(ship->status);
            writer->Write_Uint8(static_cast<uint8_t>(ship->location.kind));
            const auto& destination = ship->location.destination;
            writer->Write_Uint32(destination.Star_Id_Or_None().id);
            // Preserve the version-8 flag on the wire, not the in-memory enum.
            writer->Write_Uint8(destination.Kind() == game::e_ship_destination::antares);
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
            writer->Write_Uint8(ship->auto_colonize_target.has_value());
            if (ship->auto_colonize_target) writer->Write_Uint32(ship->auto_colonize_target->id);
        }

        void Read_Ship(SERIAL::Reader_* reader, s_ship_data* ship, uint16_t version, int16_t legacy_star_count) {
            Read_Ship_Design(reader, &ship->d);
            ship->owner = reader->Read_Int8();
            ship->status = reader->Read_Int8();
            if (version >= 8) {
                const auto mode = reader->Read_Uint8();
                const game::id_star destination{reader->Read_Uint32()};
                const auto antares = reader->Read_Uint8();
                if (mode > static_cast<uint8_t>(game::e_ship_location::in_wormhole) || antares > 1 ||
                    (antares && destination != game::NO_STAR)) {
                    reader->Require(reader->Remaining() + 1);
                    return;
                }
                ship->location = {static_cast<game::e_ship_location>(mode),
                    antares ? game::s_ship_destination::Antares() : game::s_ship_destination::Star(destination)};
            } else {
                const auto encoded = reader->Read_Int16();
                const auto decoded = legacy_world::Decode_Ship_Location(
                    version == LEGACY_VERSION ? Convert_Legacy_Ship_Location(encoded) : encoded);
                ship->location = {};
                if (decoded) {
                    const bool antares = legacy_star_count >= 0 && decoded->star_id.id == static_cast<uint32_t>(legacy_star_count);
                    ship->location = {decoded->kind, antares
                        ? game::s_ship_destination::Antares() : game::s_ship_destination::Star(decoded->star_id)};
                }
            }
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
            ship->auto_colonize_target.reset();
            if (version >= PLANET_ID_VERSION) {
                const uint8_t present = reader->Read_Uint8();
                if (present > 1) { reader->Require(reader->Remaining() + size_t{1}); return; }
                if (present) {
                    const game::id_planet target{reader->Read_Uint32()};
                    if (target == game::NO_PLANET) { reader->Require(reader->Remaining() + size_t{1}); return; }
                    ship->auto_colonize_target = target;
                }
            } else if ((ship->d.ship_type == SHIP_TYPE_COLONY || ship->d.ship_type == SHIP_TYPE_OUTPOST) && ship->d.ship_weapon[1].type == 1) {
                const int16_t legacy_target = ship->d.ship_weapon[0].type;
                if (legacy_target >= 0) ship->auto_colonize_target = game::id_planet{static_cast<uint32_t>(legacy_target)};
                ship->d.ship_weapon[0].type = -1;
                ship->d.ship_weapon[1].type = 0;
            }
        }
        void Write_Planet(SERIAL::Writer_* writer, const s_planet_data* planet) {
            writer->Write_Int16(planet->colony_index);
            writer->Write_Uint32(planet->parent_star.id);
            writer->Write_Int8(planet->orbit);
            writer->Write_Int8(planet->planet_type);
            writer->Write_Int8(planet->size);
            writer->Write_Int8(planet->gravity_class);
            writer->Write_Int8(planet->group);
            writer->Write_Int8(planet->climate);
            writer->Write_Int8(planet->climate_bg_type);
            writer->Write_Int8(planet->mineral_class);
            writer->Write_Uint8(planet->food_per_farmer);
            writer->Write_Uint8(planet->n_terraforms);
            writer->Write_Uint8(planet->max_farms);
            writer->Write_Uint8(planet->max_population);
            writer->Write_Int8(planet->planet_special);
            writer->Write_Int8(planet->environmental_alterations);
        }

        void Read_Planet(SERIAL::Reader_* reader, s_planet_data* planet, uint16_t version) {
            planet->colony_index = reader->Read_Int16();
            if (version >= 8) {
                planet->parent_star = game::id_star{reader->Read_Uint32()};
            } else {
                const int16_t parent = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
                planet->parent_star = parent < 0 ? game::NO_STAR : game::id_star{static_cast<uint32_t>(parent)};
            }
            planet->orbit = reader->Read_Int8();
            planet->planet_type = reader->Read_Int8();
            planet->size = reader->Read_Int8();
            planet->gravity_class = reader->Read_Int8();
            planet->group = reader->Read_Int8();
            planet->climate = reader->Read_Int8();
            planet->climate_bg_type = reader->Read_Int8();
            planet->mineral_class = reader->Read_Int8();
            planet->food_per_farmer = reader->Read_Uint8();
            if (version < 2) {
                planet->food_per_farmer = (uint8_t)(planet->food_per_farmer * 2);
            }
            planet->n_terraforms = reader->Read_Uint8();
            planet->max_farms = reader->Read_Uint8();
            planet->max_population = reader->Read_Uint8();
            planet->planet_special = reader->Read_Int8();
            planet->environmental_alterations = reader->Read_Int8();
        }
        const uint8_t ZEROES[512] = {};
        game::id_planet Read_Planet_Reference(SERIAL::Reader_* reader, uint16_t version, bool original_byte) {
            if (version >= PLANET_ID_VERSION) return game::id_planet{reader->Read_Uint32()};
            if (version == LEGACY_VERSION && original_byte) return game::id_planet{reader->Read_Uint8()};
            const int16_t encoded = reader->Read_Int16();
            return encoded < 0 ? game::NO_PLANET : game::id_planet{static_cast<uint32_t>(encoded)};
        }

        void Write_Zeroes(SERIAL::Writer_* writer, size_t size) {
            while (size != 0) {
                size_t chunk = size < sizeof(ZEROES) ? size : sizeof(ZEROES);
                writer->Write_Bytes(ZEROES, chunk);
                size -= chunk;
            }
        }

        void Write_Int16_Array(SERIAL::Writer_* writer, const int16_t* values, size_t count) {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                writer->Write_Int16(unaligned::Load<int16_t>(bytes + i * sizeof(int16_t)));
            }
        }

        void Read_Int16_Array(SERIAL::Reader_* reader, int16_t* values, size_t count) {
            uint8_t* bytes = reinterpret_cast<uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                unaligned::Store<int16_t>(bytes + i * sizeof(int16_t), reader->Read_Int16());
            }
        }

        void Write_Uint16_Array(SERIAL::Writer_* writer, const void* values, size_t count) {
            const uint8_t* bytes = static_cast<const uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                writer->Write_Uint16(unaligned::Load<uint16_t>(bytes + i * sizeof(uint16_t)));
            }
        }

        void Read_Uint16_Array(SERIAL::Reader_* reader, void* values, size_t count) {
            uint8_t* bytes = static_cast<uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                unaligned::Store<uint16_t>(bytes + i * sizeof(uint16_t), reader->Read_Uint16());
            }
        }

        void Write_Int32_Array(SERIAL::Writer_* writer, const void* values, size_t count) {
            const uint8_t* bytes = static_cast<const uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                writer->Write_Int32(unaligned::Load<int32_t>(bytes + i * sizeof(int32_t)));
            }
        }

        void Read_Int32_Array(SERIAL::Reader_* reader, void* values, size_t count) {
            uint8_t* bytes = static_cast<uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                unaligned::Store<int32_t>(bytes + i * sizeof(int32_t), reader->Read_Int32());
            }
        }

        void Write_Ship_Weapons(SERIAL::Writer_* writer, const s_ship_weapons* weapons) {
            writer->Write_Int16(weapons->type);
            writer->Write_Int8(weapons->count);
            writer->Write_Int8(weapons->current_count);
            writer->Write_Int8(weapons->firing_arc);
            writer->Write_Uint16(weapons->specials);
            writer->Write_Int8(weapons->ammo);
        }

        void Read_Ship_Weapons(SERIAL::Reader_* reader, s_ship_weapons* weapons) {
            weapons->type = reader->Read_Int16();
            weapons->count = reader->Read_Int8();
            weapons->current_count = reader->Read_Int8();
            weapons->firing_arc = reader->Read_Int8();
            weapons->specials = reader->Read_Uint16();
            weapons->ammo = reader->Read_Int8();
        }

        void Write_Ship_Design(SERIAL::Writer_* writer, const s_ship_design* design) {
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

        void Read_Ship_Design(SERIAL::Reader_* reader, s_ship_design* design) {
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

        void Write_Colony(SERIAL::Writer_* writer, const s_colony* colony) {
            writer->Write_Int8(colony->owner);
            writer->Write_Int8(colony->allocated_to);
            writer->Write_Uint32(colony->planet.id);
            writer->Write_Int16(colony->officer);
            writer->Write_Uint8(colony->outpost_flag);
            writer->Write_Int8(colony->morale);
            writer->Write_Int16(colony->pollution);
            writer->Write_Uint8(colony->n_pops);
            writer->Write_Int8(colony->specialty);
            for (int i = 0; i < 42; ++i) writer->Write_Uint32(colony->pop[i]);
            Write_Int16_Array(writer, colony->pop_roundoff, COLONY_POP_RACE_COUNT);
            Write_Int16_Array(writer, colony->pop_growth, COLONY_POP_RACE_COUNT);
            writer->Write_Int8(colony->n_turns_existed);
            writer->Write_Uint8(colony->food2_per_farmer);
            writer->Write_Uint8(colony->industry_per_worker);
            writer->Write_Int8(colony->research_per_scientist);
            writer->Write_Uint8(colony->max_farms);
            writer->Write_Int8(colony->max_population);
            writer->Write_Uint8(colony->climate);
            writer->Write_Int16(colony->ground_strength);
            writer->Write_Int16(colony->space_strength);
            Write_Int16_Array(writer, colony->production, ECON_COUNT);
            writer->Write_Bytes(colony->maintenance, sizeof(colony->maintenance));
            Write_Int16_Array(writer, colony->imports, ECON_COUNT);
            writer->Write_Int8(colony->n_industry_recyclers);
            writer->Write_Uint8(colony->food2_needed_for_our_empire);
            writer->Write_Uint8(colony->food2_needed_for_assimilated);
            writer->Write_Uint8(colony->food2_needed_for_conquered);
            writer->Write_Uint8(colony->food2_needed_for_natives);
            writer->Write_Uint8(colony->industry2_needed_for_our_empire);
            writer->Write_Uint8(colony->industry2_needed_for_androids);
            writer->Write_Uint8(colony->industry2_needed_for_assimilated);
            writer->Write_Uint8(colony->industry2_needed_for_conquered);
            writer->Write_Bytes(colony->food2_needed_for_empire, sizeof(colony->food2_needed_for_empire));
            writer->Write_Bytes(colony->industry2_needed_for_empire, sizeof(colony->industry2_needed_for_empire));
            writer->Write_Int8(colony->n_food_replicated);
            Write_Int16_Array(writer, colony->producing, 7);
            writer->Write_Int16(colony->just_produced);
            writer->Write_Int16(colony->production_spent);
            writer->Write_Int16(colony->n_industry_taxed);
            writer->Write_Uint8(colony->auto_building);
            writer->Write_Int16(colony->production_surplus);
            writer->Write_Int16(colony->bought_outright);
            writer->Write_Int8(colony->occupation_points);
            writer->Write_Int8(colony->occupation_policy);
            Write_Int16_Array(writer, colony->military, 2);
            writer->Write_Int8(colony->tank_roundoff);
            writer->Write_Int8(colony->infantry_roundoff);
            writer->Write_Bytes(colony->buildings, sizeof(colony->buildings));
            writer->Write_Uint16(colony->last_turn_building_destroyed);
            writer->Write_Uint8(colony->buildings_scrapped_this_turn);
        }

        void Read_Colony(SERIAL::Reader_* reader, s_colony* colony, uint16_t version) {
            memset(colony, 0, sizeof(*colony));
            colony->owner = reader->Read_Int8();
            colony->allocated_to = reader->Read_Int8();
            colony->planet = Read_Planet_Reference(reader, version);
            colony->officer = reader->Read_Int16();
            colony->outpost_flag = reader->Read_Uint8();
            colony->morale = reader->Read_Int8();
            colony->pollution = reader->Read_Int16();
            colony->n_pops = reader->Read_Uint8();
            colony->specialty = reader->Read_Int8();
            for (int i = 0; i < 42; ++i) colony->pop[i] = reader->Read_Uint32();
            Read_Int16_Array(reader, colony->pop_roundoff, COLONY_POP_RACE_COUNT);
            Read_Int16_Array(reader, colony->pop_growth, COLONY_POP_RACE_COUNT);
            colony->n_turns_existed = reader->Read_Int8();
            colony->food2_per_farmer = reader->Read_Uint8();
            colony->industry_per_worker = reader->Read_Uint8();
            colony->research_per_scientist = reader->Read_Int8();
            colony->max_farms = reader->Read_Uint8();
            colony->max_population = reader->Read_Int8();
            colony->climate = reader->Read_Uint8();
            colony->ground_strength = reader->Read_Int16();
            colony->space_strength = reader->Read_Int16();
            Read_Int16_Array(reader, colony->production, ECON_COUNT);
            reader->Read_Bytes(colony->maintenance, sizeof(colony->maintenance));
            Read_Int16_Array(reader, colony->imports, ECON_COUNT);
            colony->n_industry_recyclers = reader->Read_Int8();
            colony->food2_needed_for_our_empire = reader->Read_Uint8();
            colony->food2_needed_for_assimilated = reader->Read_Uint8();
            colony->food2_needed_for_conquered = reader->Read_Uint8();
            colony->food2_needed_for_natives = reader->Read_Uint8();
            colony->industry2_needed_for_our_empire = reader->Read_Uint8();
            colony->industry2_needed_for_androids = reader->Read_Uint8();
            colony->industry2_needed_for_assimilated = reader->Read_Uint8();
            colony->industry2_needed_for_conquered = reader->Read_Uint8();
            reader->Read_Bytes(colony->food2_needed_for_empire, sizeof(colony->food2_needed_for_empire));
            reader->Read_Bytes(colony->industry2_needed_for_empire, sizeof(colony->industry2_needed_for_empire));
            colony->n_food_replicated = reader->Read_Int8();
            Read_Int16_Array(reader, colony->producing, 7);
            colony->just_produced = reader->Read_Int16();
            colony->production_spent = reader->Read_Int16();
            colony->n_industry_taxed = reader->Read_Int16();
            colony->auto_building = reader->Read_Uint8();
            colony->production_surplus = reader->Read_Int16();
            colony->bought_outright = reader->Read_Int16();
            colony->occupation_points = reader->Read_Int8();
            colony->occupation_policy = reader->Read_Int8();
            Read_Int16_Array(reader, colony->military, 2);
            colony->tank_roundoff = reader->Read_Int8();
            colony->infantry_roundoff = reader->Read_Int8();
            reader->Read_Bytes(colony->buildings, sizeof(colony->buildings));
            colony->last_turn_building_destroyed = reader->Read_Uint16();
            colony->buildings_scrapped_this_turn = version >= BUILDING_SCRAP_COUNT_VERSION
                ? reader->Read_Uint8()
                : (colony->last_turn_building_destroyed != 0 ? 1 : 0);
        }

        void Write_Star(SERIAL::Writer_* writer, const s_star_data* star) {
            writer->Write_Bytes(star->name, sizeof(star->name));
            writer->Write_Int16(star->x);
            writer->Write_Int16(star->y);
            writer->Write_Int8(star->size);
            writer->Write_Int8(star->owner);
            writer->Write_Int8(star->pict_type);
            writer->Write_Int8(star->spectral_class);
            writer->Write_Bytes(star->last_orbit_selected, sizeof(star->last_orbit_selected));
            writer->Write_Bytes(star->black_hole_blocks, sizeof(star->black_hole_blocks));
            writer->Write_Int8(star->system_special);
            writer->Write_Uint32(star->wormhole_star_id.id);
            writer->Write_Uint8(star->blockaded);
            writer->Write_Bytes(star->blockaded_by, sizeof(star->blockaded_by));
            writer->Write_Uint8(star->visited);
            writer->Write_Uint8(star->just_visited);
            writer->Write_Uint8(star->ignore_colony_ships);
            writer->Write_Uint8(star->ignore_combat_ships);
            writer->Write_Int8(star->colonize_player);
            writer->Write_Uint8(star->has_colony);
            writer->Write_Uint8(star->has_warp_field_interdictor);
            writer->Write_Int16(star->next_wfi_in_list);
            writer->Write_Uint8(star->has_tachyon);
            writer->Write_Uint8(star->has_subspace);
            writer->Write_Uint8(star->has_stargate);
            writer->Write_Uint8(star->has_jumpgate);
            writer->Write_Uint8(star->has_artemis_net);
            writer->Write_Uint8(star->has_dimensional_portal);
            writer->Write_Uint8(star->is_stagepoint);
            writer->Write_Bytes(star->officer_index, sizeof(star->officer_index));
            for (const auto id : star->planet_index) writer->Write_Uint32(id.id);
            for (const auto id : star->relocate_ship_to) writer->Write_Uint32(id.id);
            writer->Write_Int8(star->twinkling1);
            writer->Write_Int8(star->twinkling2);
            writer->Write_Uint8(star->not_used3);
            writer->Write_Bytes(star->surrender_to, sizeof(star->surrender_to));
            writer->Write_Uint8(star->in_nebula);
            writer->Write_Uint8(star->artifacts_gave_app);
        }

        void Read_Star(SERIAL::Reader_* reader, s_star_data* star, uint16_t version) {
            reader->Read_Bytes(star->name, sizeof(star->name));
            star->x = reader->Read_Int16();
            star->y = reader->Read_Int16();
            star->size = reader->Read_Int8();
            star->owner = reader->Read_Int8();
            star->pict_type = reader->Read_Int8();
            star->spectral_class = reader->Read_Int8();
            reader->Read_Bytes(star->last_orbit_selected, sizeof(star->last_orbit_selected));
            const size_t black_hole_blocks_size = version == LEGACY_VERSION ? BITMAP(ORIGINAL_MAX_STARS) : sizeof(star->black_hole_blocks);
            reader->Read_Bytes(star->black_hole_blocks, black_hole_blocks_size);
            memset(star->black_hole_blocks + black_hole_blocks_size, 0, sizeof(star->black_hole_blocks) - black_hole_blocks_size);
            star->system_special = reader->Read_Int8();
            if (version >= 8) {
                star->wormhole_star_id = game::id_star{reader->Read_Uint32()};
            } else {
                const int16_t position = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
                star->wormhole_star_id = position < 0 ? game::NO_STAR : game::id_star{static_cast<uint32_t>(position)};
            }
            star->blockaded = reader->Read_Uint8();
            reader->Read_Bytes(star->blockaded_by, sizeof(star->blockaded_by));
            star->visited = reader->Read_Uint8();
            star->just_visited = reader->Read_Uint8();
            star->ignore_colony_ships = reader->Read_Uint8();
            star->ignore_combat_ships = reader->Read_Uint8();
            star->colonize_player = reader->Read_Int8();
            star->has_colony = reader->Read_Uint8();
            star->has_warp_field_interdictor = reader->Read_Uint8();
            star->next_wfi_in_list = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
            star->has_tachyon = reader->Read_Uint8();
            star->has_subspace = reader->Read_Uint8();
            star->has_stargate = reader->Read_Uint8();
            star->has_jumpgate = reader->Read_Uint8();
            star->has_artemis_net = reader->Read_Uint8();
            star->has_dimensional_portal = reader->Read_Uint8();
            star->is_stagepoint = reader->Read_Uint8();
            reader->Read_Bytes(star->officer_index, sizeof(star->officer_index));
            for (auto& id : star->planet_index) id = Read_Planet_Reference(reader, version);
            for (auto& id : star->relocate_ship_to) {
                if (version >= 8) {
                    id = game::id_star{reader->Read_Uint32()};
                } else {
                    const auto position = reader->Read_Int16();
                    id = position < 0 ? game::NO_STAR : game::id_star{static_cast<uint32_t>(position)};
                }
            }
            star->twinkling1 = reader->Read_Int8();
            star->twinkling2 = reader->Read_Int8();
            star->not_used3 = reader->Read_Uint8();
            reader->Read_Bytes(star->surrender_to, sizeof(star->surrender_to));
            star->in_nebula = reader->Read_Uint8();
            star->artifacts_gave_app = reader->Read_Uint8();
        }

        void Write_Player(game::s_world_state* world_state, SERIAL::Writer_* writer, const s_player* player, game::id_player player_id) {
            writer->Write_Int8(player->picture);
            writer->Write_Bytes(player->name, sizeof(player->name));
            writer->Write_Bytes(player->race_name, sizeof(player->race_name));
            writer->Write_Uint8(player->eliminated);
            writer->Write_Uint8(player->race);
            writer->Write_Uint8(player->color);
            writer->Write_Uint8(player->personality);
            writer->Write_Uint8(player->objectives);
            writer->Write_Uint32(player->home_planet_id.id);
            writer->Write_Int16(player->network_player_id);
            writer->Write_Int8(player->player_done_flags);
            writer->Write_Int16(player->dead_field);
            writer->Write_Int8(player->research_breakthrough);
            writer->Write_Uint8(player->tax_rate);
            writer->Write_Int32(player->bc);
            writer->Write_Int16(player->n_freighters);
            writer->Write_Int16(player->surplus_freighters);
            writer->Write_Int16(player->command_points);
            writer->Write_Int16(player->command_points_used);
            writer->Write_Int16(player->food_freighted);
            Write_Settlers(world_state, writer, player_id);
            writer->Write_Int16(player->total_pop);
            writer->Write_Int16(player->food_produced);
            writer->Write_Int16(player->industry_produced);
            writer->Write_Int16(player->research_produced);
            writer->Write_Int16(player->bc_produced);
            writer->Write_Int16(player->surplus_food);
            writer->Write_Int16(player->surplus_bc);
            writer->Write_Int32(player->total_maintenance);
            Write_Int16_Array(writer, player->maintenance, 6);
            writer->Write_Bytes(player->tech_fields, sizeof(player->tech_fields));
            writer->Write_Bytes(player->tech_applications, sizeof(player->tech_applications));
            writer->Write_Int32(player->research_accumulated);
            writer->Write_Int8(player->captured_orion);
            writer->Write_Int8(player->captured_antares);
            writer->Write_Int8(player->won_council_vote);
            Write_Uint16_Array(writer, player->player_defeated, MAX_PLAYERS);
            writer->Write_Int16(player->captured_population);
            writer->Write_Int8(player->last_attacker);
            writer->Write_Uint8(player->ship_design_theme);
            writer->Write_Int8(player->ship_special_theme);
            writer->Write_Bytes(player->tech_apps_learned, sizeof(player->tech_apps_learned));
            writer->Write_Bytes(player->tech_modifiers, sizeof(player->tech_modifiers));
            writer->Write_Bytes(player->hyper_advanced_tech, sizeof(player->hyper_advanced_tech));
            writer->Write_Int32(player->total_known_tech_cost);
            writer->Write_Int8(player->knows_racial_stats);
            Write_Int32_Array(writer, player->last_tech_report_date, MAX_PLAYERS);
            writer->Write_Bytes(player->player_tech_applications, sizeof(player->player_tech_applications));
            writer->Write_Int8(player->current_research_field);
            writer->Write_Int8(player->current_research_application);
            writer->Write_Int8(player->got_new_app);
            writer->Write_Int16(player->ship_range);
            for (int i = 0; i < 6; ++i) Write_Ship_Design(writer, &player->ship_designs[i]);
            Write_Int16_Array(writer, player->ships_built, 6);
            writer->Write_Bytes(player->contact, sizeof(player->contact));
            writer->Write_Int8(player->just_established_contact);
            writer->Write_Bytes(player->n_times_established_contact, sizeof(player->n_times_established_contact));
            writer->Write_Bytes(player->within_range, sizeof(player->within_range));
            writer->Write_Int8(player->do_not_research);
            writer->Write_Int16(player->offensive_stagepoint);
            writer->Write_Int8(player->best_ftl_drive);
            writer->Write_Uint8(player->ship_speed);
            writer->Write_Int8(player->officer_for_hire);
            writer->Write_Int16(player->n_trade_pops);
            Write_Int16_Array(writer, player->current_trade_agreement_level, MAX_PLAYERS);
            Write_Int16_Array(writer, player->trade_agreement_goal, MAX_PLAYERS);
            writer->Write_Int16(player->n_research_pops);
            Write_Int16_Array(writer, player->current_research_agreement_level, MAX_PLAYERS);
            Write_Int16_Array(writer, player->research_agreement_goal, MAX_PLAYERS);
            writer->Write_Int8(player->fighter_beam_type);
            writer->Write_Int8(player->fighter_bomb_type);
            writer->Write_Int16(player->total_research);
            writer->Write_Int16(player->total_ships);
            Write_Int32_Array(writer, player->total_ship_strength, MAX_PLAYERS);
            writer->Write_Int16(player->total_colonies);
            writer->Write_Int8(player->failed_to_expand);
            writer->Write_Bytes(player->first_contact, sizeof(player->first_contact));
            writer->Write_Bytes(player->relations, sizeof(player->relations));
            writer->Write_Bytes(player->base_relations, sizeof(player->base_relations));
            writer->Write_Bytes(player->treaty, sizeof(player->treaty));
            writer->Write_Bytes(player->trade_treaty, sizeof(player->trade_treaty));
            writer->Write_Bytes(player->research_treaty, sizeof(player->research_treaty));
            Write_Int16_Array(writer, player->tribute_treaty, MAX_PLAYERS);
            writer->Write_Bytes(player->diplomacy_incident, sizeof(player->diplomacy_incident));
            writer->Write_Bytes(player->diplomacy_message, sizeof(player->diplomacy_message));
            Write_Int16_Array(writer, player->diplomacy_severity, MAX_PLAYERS);
            Write_Int16_Array(writer, player->diplomacy_value, MAX_PLAYERS);
            Write_Int16_Array(writer, player->diplomacy_system, MAX_PLAYERS);
            Write_Int16_Array(writer, player->treaty_modifier, MAX_PLAYERS);
            Write_Int16_Array(writer, player->trade_modifier, MAX_PLAYERS);
            Write_Int16_Array(writer, player->tech_exchange_modifier, MAX_PLAYERS);
            Write_Int16_Array(writer, player->peace_modifier, MAX_PLAYERS);
            writer->Write_Bytes(player->last_bad_diplomatic_incident, sizeof(player->last_bad_diplomatic_incident));
            writer->Write_Bytes(player->broken_treaties_modifier, sizeof(player->broken_treaties_modifier));
            writer->Write_Bytes(player->last_broken_treaty, sizeof(player->last_broken_treaty));
            writer->Write_Bytes(player->last_gift, sizeof(player->last_gift));
            Write_Int16_Array(writer, player->ship_war_losses, MAX_PLAYERS);
            Write_Int16_Array(writer, player->colony_war_losses, MAX_PLAYERS);
            writer->Write_Bytes(player->biological_weapons_flag, sizeof(player->biological_weapons_flag));
            writer->Write_Bytes(player->time_since_last_attack, sizeof(player->time_since_last_attack));
            writer->Write_Bytes(player->threats, sizeof(player->threats));
            writer->Write_Bytes(player->dishonored_flag, sizeof(player->dishonored_flag));
            writer->Write_Bytes(player->peace_duration, sizeof(player->peace_duration));
            Write_Int16_Array(writer, player->last_turn_diplomacy_severity, MAX_PLAYERS);
            writer->Write_Bytes(player->last_broken_treaty_type, sizeof(player->last_broken_treaty_type));
            writer->Write_Bytes(player->last_turn_treaty, sizeof(player->last_turn_treaty));
            writer->Write_Bytes(player->diplomacy_proposal_request, sizeof(player->diplomacy_proposal_request));
            writer->Write_Bytes(player->diplomacy_proposal_tribute_bribe, sizeof(player->diplomacy_proposal_tribute_bribe));
            writer->Write_Bytes(player->diplomacy_proposal_tech_bribe1, sizeof(player->diplomacy_proposal_tech_bribe1));
            writer->Write_Bytes(player->diplomacy_proposal_tech_bribe2, sizeof(player->diplomacy_proposal_tech_bribe2));
            Write_Int16_Array(writer, player->diplomacy_proposal_gold_bribe, MAX_PLAYERS);
            writer->Write_Bytes(player->diplomacy_proposal_war_player, sizeof(player->diplomacy_proposal_war_player));
            Write_Int32_Array(writer, player->diplomacy_proposal_exchange_max_value, MAX_PLAYERS);
            writer->Write_Bytes(player->diplomacy_proposal_exchange_tech, sizeof(player->diplomacy_proposal_exchange_tech));
            Write_Int16_Array(writer, player->diplomacy_proposal_system_bribe, MAX_PLAYERS);
            writer->Write_Uint32(player->sneak_attack_planet.id);
            writer->Write_Int8(player->sneak_attack_message);
            writer->Write_Int8(player->sneak_attack_player);
            writer->Write_Int8(player->council_threat);
            Write_Int16_Array(writer, player->reward_amount, MAX_PLAYERS);
            writer->Write_Bytes(player->reward_tech, sizeof(player->reward_tech));
            writer->Write_Bytes(player->reward_attack_player, sizeof(player->reward_attack_player));
            writer->Write_Int16(player->food_shortage_duration);
            writer->Write_Bytes(player->trust_worthiness, sizeof(player->trust_worthiness));
            writer->Write_Bytes(player->trust_breaker_player, sizeof(player->trust_breaker_player));
            writer->Write_Bytes(player->trust_breaker_treaty, sizeof(player->trust_breaker_treaty));
            writer->Write_Bytes(player->stop_spying_duration, sizeof(player->stop_spying_duration));
            writer->Write_Bytes(player->stop_blockading_duration, sizeof(player->stop_blockading_duration));
            writer->Write_Int8(player->war_buildup_duration);
            Write_Int16_Array(writer, player->last_diplomacy_proposal_turn, MAX_PLAYERS);
            Write_Int16_Array(writer, player->diplomacy_proposal_rejection, MAX_PLAYERS);
            for (const auto id : player->opportunity_planet_num) writer->Write_Uint32(id.id);
            Write_Int16_Array(writer, player->opportunity_planet_value, MAX_PLAYERS);
            Write_Int32_Array(writer, player->opportunity_actual_attacker_ratio, MAX_PLAYERS);
            Write_Int16_Array(writer, player->temp_diplomacy_dummy1, MAX_PLAYERS);
            writer->Write_Bytes(player->diplomacy_demand_rejection_message, sizeof(player->diplomacy_demand_rejection_message));
            writer->Write_Bytes(player->last_diplomacy_threat_turn, sizeof(player->last_diplomacy_threat_turn));
            writer->Write_Bytes(player->delayed_diplomacy_orders, sizeof(player->delayed_diplomacy_orders));
            writer->Write_Bytes(player->traits, sizeof(player->traits));
            writer->Write_Bytes(player->evolutionary_upgrade_traits, sizeof(player->evolutionary_upgrade_traits));
            writer->Write_Int8(player->picked_evolutionary_upgrade);
            writer->Write_Int8(player->applied_evolutionary_upgrade);
            writer->Write_Bytes(player->fleet_history, sizeof(player->fleet_history));
            writer->Write_Bytes(player->tech_history, sizeof(player->tech_history));
            writer->Write_Bytes(player->population_history, sizeof(player->population_history));
            writer->Write_Bytes(player->production_history, sizeof(player->production_history));
            writer->Write_Bytes(player->spies, sizeof(player->spies));
            writer->Write_Int8(player->history_btns);
            writer->Write_Uint8(player->ignoring);
            writer->Write_Bytes(player->peace_flags, sizeof(player->peace_flags));
            writer->Write_Int8(player->cheated);
            writer->Write_Uint16(player->stardate_last_offered_hero);
            writer->Write_Int8(player->current_zoom_level);
            writer->Write_Int16(player->cur_map_x);
            writer->Write_Int16(player->cur_map_y);
            writer->Write_Int8(player->council_voted_for);
            writer->Write_Int8(player->surrender_to);
            writer->Write_Int16(player->time_since_last_lucky_event);
            writer->Write_Int8(player->has_galactic_lore);
            writer->Write_Int8(player->warned_about_cheating_player);
            writer->Write_Int8(player->cheated_this_turn);
            writer->Write_Int32(player->tribute_income);
            writer->Write_Bytes(player->filler, sizeof(player->filler));
        }

        bool Read_Player(game::s_world_state* world_state, SERIAL::Reader_* reader, s_player* player, uint16_t version, game::id_player player_id) {
            player->picture = reader->Read_Int8();
            reader->Read_Bytes(player->name, sizeof(player->name));
            reader->Read_Bytes(player->race_name, sizeof(player->race_name));
            player->eliminated = reader->Read_Uint8();
            player->race = reader->Read_Uint8();
            player->color = reader->Read_Uint8();
            player->personality = reader->Read_Uint8();
            player->objectives = reader->Read_Uint8();
            player->home_planet_id = Read_Planet_Reference(reader, version);
            player->network_player_id = reader->Read_Int16();
            player->player_done_flags = reader->Read_Int8();
            player->dead_field = reader->Read_Int16();
            player->research_breakthrough = reader->Read_Int8();
            player->tax_rate = reader->Read_Uint8();
            player->bc = reader->Read_Int32();
            player->n_freighters = reader->Read_Int16();
            player->surplus_freighters = reader->Read_Int16();
            player->command_points = reader->Read_Int16();
            player->command_points_used = reader->Read_Int16();
            player->food_freighted = reader->Read_Int16();
            if (version >= PLANET_ID_VERSION) {
                if (!Read_Settlers(world_state, reader, player_id)) return false;
                player->settlers_freighted = static_cast<int16_t>(world_state->settlers[player_id].size());
            } else {
                player->settlers_freighted = reader->Read_Int16();
                if (player->settlers_freighted < 0 || player->settlers_freighted > MAX_FREIGHTED_SETTLERS) return false;
                auto& records = world_state->settlers[player_id];
                records.clear();
                for (int index = 0; index < ORIGINAL_MAX_FREIGHTED_SETTLERS; ++index) {
                    const uint64_t encoded = version == LEGACY_VERSION ? reader->Read_Uint32() : reader->Read_Uint64();
                    if (index < player->settlers_freighted) records.push_back(Decode_Legacy_Settler(encoded, version == LEGACY_VERSION));
                }
            }
            player->total_pop = reader->Read_Int16();
            player->food_produced = reader->Read_Int16();
            player->industry_produced = reader->Read_Int16();
            player->research_produced = reader->Read_Int16();
            player->bc_produced = reader->Read_Int16();
            player->surplus_food = reader->Read_Int16();
            player->surplus_bc = reader->Read_Int16();
            player->total_maintenance = reader->Read_Int32();
            Read_Int16_Array(reader, player->maintenance, 6);
            reader->Read_Bytes(player->tech_fields, sizeof(player->tech_fields));
            reader->Read_Bytes(player->tech_applications, sizeof(player->tech_applications));
            player->research_accumulated = reader->Read_Int32();
            player->captured_orion = reader->Read_Int8();
            player->captured_antares = reader->Read_Int8();
            player->won_council_vote = reader->Read_Int8();
            Read_Uint16_Array(reader, player->player_defeated, MAX_PLAYERS);
            player->captured_population = reader->Read_Int16();
            player->last_attacker = reader->Read_Int8();
            player->ship_design_theme = reader->Read_Uint8();
            player->ship_special_theme = reader->Read_Int8();
            reader->Read_Bytes(player->tech_apps_learned, sizeof(player->tech_apps_learned));
            reader->Read_Bytes(player->tech_modifiers, sizeof(player->tech_modifiers));
            reader->Read_Bytes(player->hyper_advanced_tech, sizeof(player->hyper_advanced_tech));
            player->total_known_tech_cost = reader->Read_Int32();
            player->knows_racial_stats = reader->Read_Int8();
            Read_Int32_Array(reader, player->last_tech_report_date, MAX_PLAYERS);
            reader->Read_Bytes(player->player_tech_applications, sizeof(player->player_tech_applications));
            player->current_research_field = reader->Read_Int8();
            player->current_research_application = reader->Read_Int8();
            player->got_new_app = reader->Read_Int8();
            player->ship_range = reader->Read_Int16();
            for (int i = 0; i < 6; ++i) Read_Ship_Design(reader, &player->ship_designs[i]);
            Read_Int16_Array(reader, player->ships_built, 6);
            reader->Read_Bytes(player->contact, sizeof(player->contact));
            player->just_established_contact = reader->Read_Int8();
            reader->Read_Bytes(player->n_times_established_contact, sizeof(player->n_times_established_contact));
            reader->Read_Bytes(player->within_range, sizeof(player->within_range));
            player->do_not_research = reader->Read_Int8();
            player->offensive_stagepoint = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
            player->best_ftl_drive = reader->Read_Int8();
            player->ship_speed = reader->Read_Uint8();
            player->officer_for_hire = reader->Read_Int8();
            player->n_trade_pops = reader->Read_Int16();
            Read_Int16_Array(reader, player->current_trade_agreement_level, MAX_PLAYERS);
            Read_Int16_Array(reader, player->trade_agreement_goal, MAX_PLAYERS);
            player->n_research_pops = reader->Read_Int16();
            Read_Int16_Array(reader, player->current_research_agreement_level, MAX_PLAYERS);
            Read_Int16_Array(reader, player->research_agreement_goal, MAX_PLAYERS);
            player->fighter_beam_type = reader->Read_Int8();
            player->fighter_bomb_type = reader->Read_Int8();
            player->total_research = reader->Read_Int16();
            player->total_ships = reader->Read_Int16();
            Read_Int32_Array(reader, player->total_ship_strength, MAX_PLAYERS);
            player->total_colonies = reader->Read_Int16();
            player->failed_to_expand = reader->Read_Int8();
            reader->Read_Bytes(player->first_contact, sizeof(player->first_contact));
            reader->Read_Bytes(player->relations, sizeof(player->relations));
            reader->Read_Bytes(player->base_relations, sizeof(player->base_relations));
            reader->Read_Bytes(player->treaty, sizeof(player->treaty));
            reader->Read_Bytes(player->trade_treaty, sizeof(player->trade_treaty));
            reader->Read_Bytes(player->research_treaty, sizeof(player->research_treaty));
            Read_Int16_Array(reader, player->tribute_treaty, MAX_PLAYERS);
            reader->Read_Bytes(player->diplomacy_incident, sizeof(player->diplomacy_incident));
            reader->Read_Bytes(player->diplomacy_message, sizeof(player->diplomacy_message));
            Read_Int16_Array(reader, player->diplomacy_severity, MAX_PLAYERS);
            Read_Int16_Array(reader, player->diplomacy_value, MAX_PLAYERS);
            Read_Int16_Array(reader, player->diplomacy_system, MAX_PLAYERS);
            Read_Int16_Array(reader, player->treaty_modifier, MAX_PLAYERS);
            Read_Int16_Array(reader, player->trade_modifier, MAX_PLAYERS);
            Read_Int16_Array(reader, player->tech_exchange_modifier, MAX_PLAYERS);
            Read_Int16_Array(reader, player->peace_modifier, MAX_PLAYERS);
            reader->Read_Bytes(player->last_bad_diplomatic_incident, sizeof(player->last_bad_diplomatic_incident));
            reader->Read_Bytes(player->broken_treaties_modifier, sizeof(player->broken_treaties_modifier));
            reader->Read_Bytes(player->last_broken_treaty, sizeof(player->last_broken_treaty));
            reader->Read_Bytes(player->last_gift, sizeof(player->last_gift));
            Read_Int16_Array(reader, player->ship_war_losses, MAX_PLAYERS);
            Read_Int16_Array(reader, player->colony_war_losses, MAX_PLAYERS);
            reader->Read_Bytes(player->biological_weapons_flag, sizeof(player->biological_weapons_flag));
            reader->Read_Bytes(player->time_since_last_attack, sizeof(player->time_since_last_attack));
            reader->Read_Bytes(player->threats, sizeof(player->threats));
            reader->Read_Bytes(player->dishonored_flag, sizeof(player->dishonored_flag));
            reader->Read_Bytes(player->peace_duration, sizeof(player->peace_duration));
            Read_Int16_Array(reader, player->last_turn_diplomacy_severity, MAX_PLAYERS);
            reader->Read_Bytes(player->last_broken_treaty_type, sizeof(player->last_broken_treaty_type));
            reader->Read_Bytes(player->last_turn_treaty, sizeof(player->last_turn_treaty));
            reader->Read_Bytes(player->diplomacy_proposal_request, sizeof(player->diplomacy_proposal_request));
            reader->Read_Bytes(player->diplomacy_proposal_tribute_bribe, sizeof(player->diplomacy_proposal_tribute_bribe));
            reader->Read_Bytes(player->diplomacy_proposal_tech_bribe1, sizeof(player->diplomacy_proposal_tech_bribe1));
            reader->Read_Bytes(player->diplomacy_proposal_tech_bribe2, sizeof(player->diplomacy_proposal_tech_bribe2));
            Read_Int16_Array(reader, player->diplomacy_proposal_gold_bribe, MAX_PLAYERS);
            reader->Read_Bytes(player->diplomacy_proposal_war_player, sizeof(player->diplomacy_proposal_war_player));
            Read_Int32_Array(reader, player->diplomacy_proposal_exchange_max_value, MAX_PLAYERS);
            reader->Read_Bytes(player->diplomacy_proposal_exchange_tech, sizeof(player->diplomacy_proposal_exchange_tech));
            Read_Int16_Array(reader, player->diplomacy_proposal_system_bribe, MAX_PLAYERS);
            player->sneak_attack_planet = Read_Planet_Reference(reader, version);
            player->sneak_attack_message = reader->Read_Int8();
            player->sneak_attack_player = reader->Read_Int8();
            player->council_threat = reader->Read_Int8();
            Read_Int16_Array(reader, player->reward_amount, MAX_PLAYERS);
            reader->Read_Bytes(player->reward_tech, sizeof(player->reward_tech));
            reader->Read_Bytes(player->reward_attack_player, sizeof(player->reward_attack_player));
            player->food_shortage_duration = reader->Read_Int16();
            reader->Read_Bytes(player->trust_worthiness, sizeof(player->trust_worthiness));
            reader->Read_Bytes(player->trust_breaker_player, sizeof(player->trust_breaker_player));
            reader->Read_Bytes(player->trust_breaker_treaty, sizeof(player->trust_breaker_treaty));
            reader->Read_Bytes(player->stop_spying_duration, sizeof(player->stop_spying_duration));
            reader->Read_Bytes(player->stop_blockading_duration, sizeof(player->stop_blockading_duration));
            player->war_buildup_duration = reader->Read_Int8();
            Read_Int16_Array(reader, player->last_diplomacy_proposal_turn, MAX_PLAYERS);
            Read_Int16_Array(reader, player->diplomacy_proposal_rejection, MAX_PLAYERS);
            for (auto& id : player->opportunity_planet_num) id = Read_Planet_Reference(reader, version);
            Read_Int16_Array(reader, player->opportunity_planet_value, MAX_PLAYERS);
            Read_Int32_Array(reader, player->opportunity_actual_attacker_ratio, MAX_PLAYERS);
            Read_Int16_Array(reader, player->temp_diplomacy_dummy1, MAX_PLAYERS);
            reader->Read_Bytes(player->diplomacy_demand_rejection_message, sizeof(player->diplomacy_demand_rejection_message));
            reader->Read_Bytes(player->last_diplomacy_threat_turn, sizeof(player->last_diplomacy_threat_turn));
            reader->Read_Bytes(player->delayed_diplomacy_orders, sizeof(player->delayed_diplomacy_orders));
            reader->Read_Bytes(player->traits, sizeof(player->traits));
            reader->Read_Bytes(player->evolutionary_upgrade_traits, sizeof(player->evolutionary_upgrade_traits));
            player->picked_evolutionary_upgrade = reader->Read_Int8();
            player->applied_evolutionary_upgrade = reader->Read_Int8();
            reader->Read_Bytes(player->fleet_history, sizeof(player->fleet_history));
            reader->Read_Bytes(player->tech_history, sizeof(player->tech_history));
            reader->Read_Bytes(player->population_history, sizeof(player->population_history));
            reader->Read_Bytes(player->production_history, sizeof(player->production_history));
            reader->Read_Bytes(player->spies, sizeof(player->spies));
            player->history_btns = reader->Read_Int8();
            player->ignoring = reader->Read_Uint8();
            reader->Read_Bytes(player->peace_flags, sizeof(player->peace_flags));
            player->cheated = reader->Read_Int8();
            player->stardate_last_offered_hero = reader->Read_Uint16();
            player->current_zoom_level = reader->Read_Int8();
            player->cur_map_x = reader->Read_Int16();
            player->cur_map_y = reader->Read_Int16();
            player->council_voted_for = reader->Read_Int8();
            player->surrender_to = reader->Read_Int8();
            player->time_since_last_lucky_event = reader->Read_Int16();
            player->has_galactic_lore = reader->Read_Int8();
            player->warned_about_cheating_player = reader->Read_Int8();
            player->cheated_this_turn = reader->Read_Int8();
            player->tribute_income = reader->Read_Int32();
            reader->Read_Bytes(player->filler, sizeof(player->filler));
            return !reader->Failed();
        }

        void Write_Report(SERIAL::Writer_* writer, const s_report* report) {
            writer->Write_Uint8(report->type);
            writer->Write_Int16(report->player_idx);
            writer->Write_Uint8(report->action);
            const uint32_t start = writer->Length();
            if (report->type == 1 && report->action == 0) {
                writer->Write_Int8(report->d.steal.framed_player_idx);
                writer->Write_Uint8(report->d.steal.victim_player_idx);
                writer->Write_Int16(report->d.steal.tech_app_id);
            } else if (report->type == 1 && report->action == 1) {
                writer->Write_Int8(report->d.sabotage.framed_player_idx);
                writer->Write_Uint8(report->d.sabotage.victim_player_idx);
                writer->Write_Uint32(report->d.sabotage.planet_idx.id);
                writer->Write_Uint8(report->d.sabotage.building_id);
            } else if (report->type == 2 && report->action == 0) {
                writer->Write_Int16(report->d.enemy_steal.enemy_player_idx);
                writer->Write_Int16(report->d.enemy_steal.tech_app_id);
            } else if (report->type == 2 && report->action == 1) {
                writer->Write_Int16(report->d.enemy_sabotage.enemy_player_idx);
                writer->Write_Uint32(report->d.enemy_sabotage.planet_idx.id);
                writer->Write_Uint8(report->d.enemy_sabotage.building_id);
            } else if (report->type == 3) {
                writer->Write_Uint32(report->d.artifacts.planet_idx.id);
                writer->Write_Int16(report->d.artifacts.tech_app_id);
            } else {
                writer->Write_Bytes(&report->d, 8);
            }
            Write_Zeroes(writer, 8 - (writer->Length() - start));
            writer->Write_Bytes(report->reserved, sizeof(report->reserved));
        }

        void Read_Report(SERIAL::Reader_* reader, s_report* report, uint16_t version) {
            report->type = reader->Read_Uint8();
            report->player_idx = reader->Read_Int16();
            report->action = reader->Read_Uint8();
            memset(&report->d, 0, sizeof(report->d));
            const uint32_t start = reader->Length();
            const uint32_t payload_size = version >= PLANET_ID_VERSION ? 8 : (version == LEGACY_VERSION ? 4 : 5);
            if (report->type == 1 && report->action == 0) {
                report->d.steal.framed_player_idx = reader->Read_Int8();
                report->d.steal.victim_player_idx = reader->Read_Uint8();
                report->d.steal.tech_app_id = reader->Read_Int16();
            } else if (report->type == 1 && report->action == 1) {
                report->d.sabotage.framed_player_idx = reader->Read_Int8();
                report->d.sabotage.victim_player_idx = reader->Read_Uint8();
                report->d.sabotage.planet_idx = Read_Planet_Reference(reader, version, true);
                report->d.sabotage.building_id = reader->Read_Uint8();
            } else if (report->type == 2 && report->action == 0) {
                report->d.enemy_steal.enemy_player_idx = reader->Read_Int16();
                report->d.enemy_steal.tech_app_id = reader->Read_Int16();
            } else if (report->type == 2 && report->action == 1) {
                report->d.enemy_sabotage.enemy_player_idx = reader->Read_Int16();
                report->d.enemy_sabotage.planet_idx = Read_Planet_Reference(reader, version, true);
                report->d.enemy_sabotage.building_id = reader->Read_Uint8();
            } else if (report->type == 3) {
                report->d.artifacts.planet_idx = Read_Planet_Reference(reader, version);
                report->d.artifacts.tech_app_id = reader->Read_Int16();
            } else {
                reader->Read_Bytes(&report->d, payload_size);
            }
            reader->Skip(payload_size - (reader->Length() - start));
            reader->Read_Bytes(report->reserved, sizeof(report->reserved));
        }

        void Write_Colony_Message(SERIAL::Writer_* writer, const s_MSG_colony* colony) {
            writer->Write_Uint32(colony->planet_idx.id);
            writer->Write_Int8(colony->subtype);
            switch (colony->subtype) {
                case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED:
                    writer->Write_Int16(colony->c.scrap.building_id);
                    writer->Write_Int16(colony->c.scrap.bc);
                    Write_Zeroes(writer, 2);
                    break;
                case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED:
                    writer->Write_Int16(colony->c.build.build_id);
                    writer->Write_Int16(colony->c.build.product_id);
                    writer->Write_Uint8(colony->c.build.is_refit ? 1 : 0);
                    writer->Write_Int8(colony->c.build.append_fmt);
                    break;
                case MSG_COLONY_SUBTYPE_GENOCIDE:
                case MSG_COLONY_SUBTYPE_ASSIMILATION:
                    writer->Write_Uint8(colony->c.assimilation.val);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_COLONY_SUBTYPE_STARVATION:
                    writer->Write_Int8(colony->c.starvation.a);
                    writer->Write_Int8(colony->c.starvation.b);
                    Write_Zeroes(writer, 4);
                    break;
                case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED:
                    writer->Write_Int8(colony->c.settlers.settlers);
                    writer->Write_Int8(colony->c.settlers.over_limit);
                    writer->Write_Uint8(colony->c.settlers.blockaded ? 1 : 0);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_COLONY_SUBTYPE_REBELLION:
                    writer->Write_Int8(colony->c.rebels.owner_player);
                    writer->Write_Int8(colony->c.rebels.rebels);
                    writer->Write_Int8(colony->c.rebels.lost);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC:
                    writer->Write_Int8(colony->c.android_killed.race_idx);
                    Write_Zeroes(writer, 5);
                    break;
                default:
                    Write_Zeroes(writer, 6);
                    break;
            }
        }

        void Read_Colony_Message(SERIAL::Reader_* reader, s_MSG_colony* colony, uint16_t version) {
            colony->planet_idx = Read_Planet_Reference(reader, version, true);
            colony->subtype = static_cast<MSG_COLONY_SUBTYPE>(reader->Read_Int8());
            switch (colony->subtype) {
                case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED:
                    colony->c.scrap.building_id = reader->Read_Int16();
                    colony->c.scrap.bc = reader->Read_Int16();
                    reader->Skip(2);
                    break;
                case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED:
                    colony->c.build.build_id = reader->Read_Int16();
                    colony->c.build.product_id = reader->Read_Int16();
                    colony->c.build.is_refit = reader->Read_Uint8() != 0;
                    colony->c.build.append_fmt = reader->Read_Int8();
                    break;
                case MSG_COLONY_SUBTYPE_GENOCIDE:
                case MSG_COLONY_SUBTYPE_ASSIMILATION:
                    colony->c.assimilation.val = reader->Read_Uint8();
                    reader->Skip(5);
                    break;
                case MSG_COLONY_SUBTYPE_STARVATION:
                    colony->c.starvation.a = reader->Read_Int8();
                    colony->c.starvation.b = reader->Read_Int8();
                    reader->Skip(4);
                    break;
                case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED:
                    colony->c.settlers.settlers = reader->Read_Int8();
                    colony->c.settlers.over_limit = reader->Read_Int8();
                    colony->c.settlers.blockaded = reader->Read_Uint8() != 0;
                    reader->Skip(3);
                    break;
                case MSG_COLONY_SUBTYPE_REBELLION:
                    colony->c.rebels.owner_player = reader->Read_Int8();
                    colony->c.rebels.rebels = reader->Read_Int8();
                    colony->c.rebels.lost = reader->Read_Int8();
                    reader->Skip(3);
                    break;
                case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC:
                    colony->c.android_killed.race_idx = reader->Read_Int8();
                    reader->Skip(5);
                    break;
                default:
                    reader->Skip(6);
                    break;
            }
        }

        void Write_Message(SERIAL::Writer_* writer, const s_MSG* message) {
            writer->Write_Int8(message->msg_type);
            writer->Write_Uint8(message->priority);
            switch (message->msg_type) {
                case MSG_TYPE_COLONY:
                    Write_Colony_Message(writer, &message->u.colony);
                    break;
                case MSG_TYPE_DECLARE_ALLIANCE:
                case MSG_TYPE_DECLARE_WAR:
                    writer->Write_Int16(message->u.players.player_a_idx);
                    writer->Write_Int16(message->u.players.player_b_idx);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_FINANCIAL:
                    writer->Write_Int16(message->u.financial.a);
                    writer->Write_Int16(message->u.financial.b);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_LOST_SHIP:
                    writer->Write_Int16(message->u.lost_ship.star_idx);
                    writer->Write_Int16(message->u.lost_ship.ship_idx);
                    writer->Write_Int8(message->u.lost_ship.leader_idx);
                    writer->Write_Int8(message->u.lost_ship.fate);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_TYPE_SCRAP_SHIP:
                    writer->Write_Int16(message->u.scrap_ship.where);
                    writer->Write_Int16(message->u.scrap_ship.bc);
                    writer->Write_Int16(message->u.scrap_ship.ship_idx);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_TYPE_LOST_COLONY:
                    writer->Write_Uint32(message->u.lost_colony.planet_idx.id);
                    writer->Write_Int16(message->u.lost_colony.reason);
                    writer->Write_Int16(message->u.lost_colony.leader_idx);
                    writer->Write_Int8(message->u.lost_colony.colony_or_outpost);
                    Write_Zeroes(writer, 2);
                    break;
                case MSG_TYPE_HYPERSPACE_BEAST:
                    writer->Write_Int16(message->u.hyperspace_beast.ship_idx);
                    writer->Write_Int16(message->u.hyperspace_beast.star_idx);
                    writer->Write_Int8(message->u.hyperspace_beast.officer_idx);
                    writer->Write_Int8(message->u.hyperspace_beast.leader_status);
                    writer->Write_Int8(message->u.hyperspace_beast.fate);
                    writer->Write_Int8(message->u.hyperspace_beast.your_ship);
                    Write_Zeroes(writer, 1);
                    break;
                case MSG_TYPE_LOST_ATTACKING_SPY:
                    writer->Write_Int16(message->u.lost_attacking_spy.player_idx);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_LOST_DEFENDING_SPY:
                    writer->Write_Int16(message->u.lost_defending_spy.count);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_ARTEMIS:
                    writer->Write_Int16(message->u.artemis.star_idx);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_ARTEMIS_LEADER:
                    writer->Write_Int16(message->u.artemis_leader.star_idx);
                    writer->Write_Int8(message->u.artemis_leader.leader_idx);
                    writer->Write_Int8(message->u.artemis_leader.fate);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_FAILED_RETREAT:
                    writer->Write_Int16(message->u.retreat.star_idx);
                    writer->Write_Int8(message->u.retreat.reason);
                    writer->Write_Int16(message->u.retreat.ships_lost);
                    writer->Write_Int8(message->u.retreat.leader_count);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_TYPE_SPY_ASSASSINATION:
                    writer->Write_Int8(message->u.spy_assasination.plr_a);
                    writer->Write_Int8(message->u.spy_assasination.plr_b);
                    writer->Write_Int8(message->u.spy_assasination.leader_id);
                    writer->Write_Int8(message->u.spy_assasination.victim_id);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_ANOMALY:
                    writer->Write_Int16(message->u.anomaly.star_idx);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_DIPLOMACY:
                    writer->Write_Int8(message->u.diplomacy.diplomacy_idx);
                    writer->Write_Int8(message->u.diplomacy.plr_a);
                    writer->Write_Int8(message->u.diplomacy.plr_b);
                    Write_Zeroes(writer, 6);
                    break;
                case MSG_TYPE_EEL_SPAWNED:
                case MSG_TYPE_EEL_VANISHED:
                    writer->Write_Int16(message->u.eel.star_idx);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_ARTIFACTS:
                    writer->Write_Int16(message->u.artifacts.star_idx);
                    writer->Write_Int8(message->u.artifacts.tech1);
                    writer->Write_Int8(message->u.artifacts.tech2);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_SCRAP_LEADER:
                    writer->Write_Int8(message->u.scrap_leader.leader_idx);
                    Write_Zeroes(writer, 8);
                    break;
                default:
                    Write_Zeroes(writer, 9);
                    break;
            }
            writer->Write_Bytes(message->reserved, sizeof(message->reserved));
        }

        void Read_Message(SERIAL::Reader_* reader, s_MSG* message, uint16_t version) {
            message->msg_type = reader->Read_Int8();
            message->priority = reader->Read_Uint8();
            memset(&message->u, 0, sizeof(message->u));
            switch (message->msg_type) {
                case MSG_TYPE_COLONY:
                    Read_Colony_Message(reader, &message->u.colony, version);
                    break;
                case MSG_TYPE_DECLARE_ALLIANCE:
                case MSG_TYPE_DECLARE_WAR:
                    message->u.players.player_a_idx = reader->Read_Int16();
                    message->u.players.player_b_idx = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 4 : 5);
                    break;
                case MSG_TYPE_FINANCIAL:
                    message->u.financial.a = reader->Read_Int16();
                    message->u.financial.b = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 4 : 5);
                    break;
                case MSG_TYPE_LOST_SHIP:
                    message->u.lost_ship.star_idx = reader->Read_Int16();
                    message->u.lost_ship.ship_idx = reader->Read_Int16();
                    message->u.lost_ship.leader_idx = reader->Read_Int8();
                    message->u.lost_ship.fate = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 2 : 3);
                    break;
                case MSG_TYPE_SCRAP_SHIP:
                    message->u.scrap_ship.where = reader->Read_Int16();
                    if (version == LEGACY_VERSION) {
                        message->u.scrap_ship.where = Convert_Legacy_Ship_Location(message->u.scrap_ship.where);
                    }
                    message->u.scrap_ship.bc = reader->Read_Int16();
                    message->u.scrap_ship.ship_idx = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 2 : 3);
                    break;
                case MSG_TYPE_LOST_COLONY:
                    message->u.lost_colony.planet_idx = Read_Planet_Reference(reader, version);
                    message->u.lost_colony.reason = reader->Read_Int16();
                    message->u.lost_colony.leader_idx = reader->Read_Int16();
                    message->u.lost_colony.colony_or_outpost = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 1 : 2);
                    break;
                case MSG_TYPE_HYPERSPACE_BEAST:
                    message->u.hyperspace_beast.ship_idx = reader->Read_Int16();
                    message->u.hyperspace_beast.star_idx = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
                    message->u.hyperspace_beast.officer_idx = reader->Read_Int8();
                    message->u.hyperspace_beast.leader_status = reader->Read_Int8();
                    message->u.hyperspace_beast.fate = reader->Read_Int8();
                    message->u.hyperspace_beast.your_ship = reader->Read_Int8();
                    reader->Skip(1);
                    break;
                case MSG_TYPE_LOST_ATTACKING_SPY:
                    message->u.lost_attacking_spy.player_idx = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 6 : 7);
                    break;
                case MSG_TYPE_LOST_DEFENDING_SPY:
                    message->u.lost_defending_spy.count = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 6 : 7);
                    break;
                case MSG_TYPE_ARTEMIS:
                    message->u.artemis.star_idx = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 6 : 7);
                    break;
                case MSG_TYPE_ARTEMIS_LEADER:
                    message->u.artemis_leader.star_idx = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
                    message->u.artemis_leader.leader_idx = reader->Read_Int8();
                    message->u.artemis_leader.fate = reader->Read_Int8();
                    reader->Skip(5);
                    break;
                case MSG_TYPE_FAILED_RETREAT:
                    message->u.retreat.star_idx = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
                    message->u.retreat.reason = reader->Read_Int8();
                    message->u.retreat.ships_lost = reader->Read_Int16();
                    message->u.retreat.leader_count = reader->Read_Int8();
                    reader->Skip(3);
                    break;
                case MSG_TYPE_SPY_ASSASSINATION:
                    message->u.spy_assasination.plr_a = reader->Read_Int8();
                    message->u.spy_assasination.plr_b = reader->Read_Int8();
                    message->u.spy_assasination.leader_id = reader->Read_Int8();
                    message->u.spy_assasination.victim_id = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 4 : 5);
                    break;
                case MSG_TYPE_ANOMALY:
                    message->u.anomaly.star_idx = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
                    reader->Skip(7);
                    break;
                case MSG_TYPE_DIPLOMACY:
                    message->u.diplomacy.diplomacy_idx = reader->Read_Int8();
                    message->u.diplomacy.plr_a = reader->Read_Int8();
                    message->u.diplomacy.plr_b = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 5 : 6);
                    break;
                case MSG_TYPE_EEL_SPAWNED:
                case MSG_TYPE_EEL_VANISHED:
                    message->u.eel.star_idx = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
                    reader->Skip(7);
                    break;
                case MSG_TYPE_ARTIFACTS:
                    message->u.artifacts.star_idx = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
                    message->u.artifacts.tech1 = reader->Read_Int8();
                    message->u.artifacts.tech2 = reader->Read_Int8();
                    reader->Skip(5);
                    break;
                case MSG_TYPE_SCRAP_LEADER:
                    message->u.scrap_leader.leader_idx = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 7 : 8);
                    break;
                default:
                    reader->Skip(version == LEGACY_VERSION ? 8 : 9);
                    break;
            }
            reader->Read_Bytes(message->reserved, sizeof(message->reserved));
        }

    int16_t Convert_Legacy_Ship_Location(int16_t location) {
        constexpr int16_t legacy_moving_offset = ORIGINAL_MAX_SHIPS;
        constexpr int16_t legacy_wormhole_offset = ORIGINAL_MAX_SHIPS * 2;
        constexpr int16_t legacy_location_limit = ORIGINAL_MAX_SHIPS * 3;

        if (location >= legacy_moving_offset && location < legacy_wormhole_offset) {
            return location - legacy_moving_offset + SHIP_LOCATION_MOVING_OFFSET;
        }
        if (location >= legacy_wormhole_offset && location < legacy_location_limit) {
            return location - legacy_wormhole_offset + SHIP_LOCATION_WORMHOLE_OFFSET;
        }
        return location;
    }

} // namespace SAVEGAME::records

namespace SAVEGAME {
    using namespace records;
    void Write_Player_Record(game::s_world_state* world, SERIAL::Writer_* writer, const s_player* player, game::id_player id) {
        Write_Player(world, writer, player, id);
    }

    bool Read_Player_Record(game::s_world_state* world, SERIAL::Reader_* reader, s_player* player, game::id_player id) {
        return Read_Player(world, reader, player, CURRENT_VERSION, id);
    }

    void Write_Colony_Record(SERIAL::Writer_* writer, const s_colony* record) {
        Write_Colony(writer, record);
    }

    bool Read_Colony_Record(SERIAL::Reader_* reader, s_colony* record) {
        Read_Colony(reader, record, CURRENT_VERSION);
        return !reader->Failed();
    }

    void Write_Star_Record(SERIAL::Writer_* writer, const s_star_data* record) {
        Write_Star(writer, record);
    }

    bool Read_Star_Record(SERIAL::Reader_* reader, s_star_data* record) {
        Read_Star(reader, record, CURRENT_VERSION);
        return !reader->Failed();
    }

    void Write_Report_Record(SERIAL::Writer_* writer, const s_report* record) {
        Write_Report(writer, record);
    }

    bool Read_Report_Record(SERIAL::Reader_* reader, s_report* record) {
        Read_Report(reader, record, CURRENT_VERSION);
        return !reader->Failed();
    }

    void Write_Message_Record(SERIAL::Writer_* writer, const s_MSG* record) {
        Write_Message(writer, record);
    }

    bool Read_Message_Record(SERIAL::Reader_* reader, s_MSG* record) {
        Read_Message(reader, record, CURRENT_VERSION);
        return !reader->Failed();
    }

} // namespace SAVEGAME

namespace SAVEGAME {

    void Write_Ship_Record(SERIAL::Writer_* writer, const s_ship_data* ship) {
        records::Write_Ship(writer, ship);
        writer->Write_Uint32(ship->prior_jump_gate_destination.id);
        writer->Write_Int32(ship->prior_jump_gate_stardate);
    }
    bool Read_Ship_Record(SERIAL::Reader_* reader, s_ship_data* ship) {
        records::Read_Ship(reader, ship, CURRENT_VERSION);
        ship->prior_jump_gate_destination = game::id_star{reader->Read_Uint32()};
        ship->prior_jump_gate_stardate = reader->Read_Int32();
        return !reader->Failed();
    }
}
