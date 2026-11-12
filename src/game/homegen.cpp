#include "pch.h"
#include "string_overrides.h"

namespace HOMEGEN {
    const char s_Orion[8] = "Orion";

    int32_t HOME_WORLD_GEN_MAX_CTR;
    s_good_star* good_stars;
    int16_t nearest_food_worlds[MAX_PLAYERS];

    static int16_t Generation_Update_Interval() {
        return MOX::NUM_STARS <= ORIGINAL_MAX_STARS ? 1 : 32;
    }

    static bool Should_Update_Generation(int16_t star_idx) {
        return star_idx % Generation_Update_Interval() == 0;
    }

    static int32_t __cdecl Compare_Star_Dist(const s_star_dist* candidate_a, const s_star_dist* candidate_b) {
        if (candidate_a->dist == candidate_b->dist) {
            return 0;
        }

        return (candidate_a->dist > candidate_b->dist) ? 1 : -1;
    }

    int __cdecl qsort_1(const void* candidate_a_ptr, const void* candidate_b_ptr) {
        const s_orion_candidate* candidate_a = (const s_orion_candidate*)candidate_a_ptr;
        const s_orion_candidate* candidate_b = (const s_orion_candidate*)candidate_b_ptr;
        return candidate_a->dist - candidate_b->dist;
    }

    int32_t __cdecl Dist_Between_Stars(int16_t star_idx_1, int16_t star_idx_2) {
        s_star_data* star1 = &MOX::star[star_idx_1];
        s_star_data* star2 = &MOX::star[star_idx_2];

        return Dist_Between_Points(star1->x, star1->y, star2->x, star2->y);
    }

    int32_t __cdecl Dist_Between_Points(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int32_t dx = (int32_t)x1 - (int32_t)x2;
        int32_t dy = (int32_t)y1 - (int32_t)y2;
        return (dx * dx) + (dy * dy);
    }

    void __cdecl Build_Min_Star_Distances(int32_t min_dist_sq) {
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if (Should_Update_Generation(i)) {
                RUSS::Mox_Update();
            }

            s_good_star* current_star = &good_stars[i];
            current_star->star_count = 0;

            for (int16_t j = 0; j < MOX::NUM_STARS; ++j) {
                int32_t dist = Dist_Between_Stars(i, j);

                if (dist >= min_dist_sq) {
                    struct_::Set_Bit_Field(current_star->star_bitmap, j);

                    // Note: Potential out-of-bounds access if MOX::NUM_STARS > MAX_STARS
                    current_star->star_distances[j] = dist;
                    current_star->star_count++;
                }
            }
        }
    }

    uint8_t __cdecl Build_Home_Star_List(int16_t* home_stars_out, int32_t min_distance_sq) {
        s_colors colors1 = *MISC::Font_Colors2(4, 0x77, 0x79);
        s_colors colors2 = *MISC::Font_Colors2(4, 0x77, 0x7c);

        CandidateList candidates[MAX_STARS];
        for (int i = 0; i < MAX_STARS; ++i) {
            candidates[i].count = 0;
            for (int j = 0; j < 20; ++j) candidates[i].stars[j] = 0;
        }

        if (MOX::NUM_STARS > 1) {
            for (int16_t current_star = 1; current_star < MOX::NUM_STARS; ++current_star) {
                bool update_progress = Should_Update_Generation(current_star);
                if (update_progress) {
                    RUSS::Mox_Update();
                }

                CandidateList* list = &candidates[current_star]; // Note: Original code uses index up to NUM_STARS
                list->count = 1;
                list->stars[0] = current_star;

                if (update_progress && MAPGEN::fill_msg_ctr < HOME_WORLD_GEN_MAX_CTR) {
                    fonts::Set_Font_Style(4, &colors1);
                    int16_t width = fonts::Get_String_Width(MAPGEN::fill_msg);
                    MISC::Draw_Advancing_Fill_Message(4, &MAPGEN::fill_msg_ctr, (uint16_t)HOME_WORLD_GEN_MAX_CTR, &colors1, &colors2, (640 - width) / 2, 215, MAPGEN::fill_msg, MAPGEN::fill_msg_bitmap, 0);
                    RUSS::Mox_Sync_Update();
                }

                for (int16_t star_j = 1; star_j < MOX::NUM_STARS; ++star_j) {
                    if (star_j == current_star) continue;

                    s_good_star* field = &good_stars[current_star];
                    if (struct_::Test_Bit_Field(field->star_bitmap, star_j)) {
                        bool valid = true;
                        for (int k = 0; k < list->count; ++k) {
                            int16_t existing_star = list->stars[k];
                            int32_t dist = Dist_Between_Stars(existing_star, star_j);
                            s_good_star* field_k = &good_stars[existing_star];
                            int16_t bit = struct_::Test_Bit_Field(field_k->star_bitmap, star_j);

                            if (existing_star != star_j && (bit == 0 || dist < min_distance_sq)) {
                                valid = false;
                                break;
                            }
                        }

                        if (valid) {
                            if (list->count < 20) {
                                list->stars[list->count++] = star_j;
                            }
                        }
                    }
                }
            }
        }

        int16_t selected_index = -1;
        int32_t pick_count = 1;
        for (int16_t si = 0; si < MAX_STARS; ++si) {
            if (Should_Update_Generation(si)) {
                RUSS::Mox_Update();
            }
            if (candidates[si].count >= MOX::NUM_PLAYERS) {
                if (game_random::Random(pick_count) == 1) {
                    selected_index = si;
                    pick_count++;
                }
            }
        }

        if (selected_index >= 0) {
            CandidateList* sel_list = &candidates[selected_index];
            for (int i = 0; i < 4; ++i) {
                int idx1 = game_random::Random(sel_list->count) - 1;
                int idx2 = game_random::Random(sel_list->count) - 1;
                int16_t swapped_star = sel_list->stars[idx1];
                sel_list->stars[idx1] = sel_list->stars[idx2];
                sel_list->stars[idx2] = swapped_star;
            }

            // Clear the player-slot output list.
            for (int i = 0; i < MAX_PLAYERS; ++i) home_stars_out[i] = -1;

            for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
                home_stars_out[i] = sel_list->stars[i];
            }
            return 0; // Success
        }

        return 1; // Failure
    }

    bool __cdecl Is_Added_Food_World(int16_t food_world_idx) {
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (nearest_food_worlds[i] == food_world_idx) {
                return true;
            }
        }
        return false;
    }

    void __cdecl Randomize_Home_Worlds(int16_t* home_star_ids) {
        for (int32_t i = 0; i < 100; ++i) {
            int16_t idx1 = (uint16_t)game_random::Random((int32_t)MOX::NUM_PLAYERS) - 1;
            int16_t idx2 = (uint16_t)game_random::Random((int32_t)MOX::NUM_PLAYERS) - 1;

            if (idx1 != idx2) {
                int16_t swapped_home_star = home_star_ids[idx1];
                home_star_ids[idx1] = home_star_ids[idx2];
                home_star_ids[idx2] = swapped_home_star;
            }
        }
    }

    uint16_t __cdecl Generate_Home_Worlds() {
        int16_t home_star_ids[MAX_PLAYERS];

        s_colors local_color1 = *MISC::Font_Colors2(4, 0x77, 0x79);
        s_colors local_color2 = *MISC::Font_Colors2(4, 0x77, 0x7c);

        good_stars = (s_good_star*)allocate::Allocate_First_Block(MOX::screen_seg, MAX_STARS * sizeof(*good_stars));
        memset(good_stars, 0, MAX_STARS * sizeof(*good_stars));

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            nearest_food_worlds[i] = -1;
        }

        RUSS::Mox_Update();
        video::Set_Refresh_Full();

        int16_t base_players = (int16_t)MOX::NUM_PLAYERS;
        int16_t adj_players = base_players;
        if (base_players > 6) {
            adj_players += (int16_t)MOX::g_galaxy_size;
        }
        int16_t max_retries = adj_players + 4;

        RUSS::Mox_Update();
        Set_Up_Home_Gen_Msg();

        while (true) {
            RUSS::Mox_Update();
            for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                MOX::star[i].owner = -1;
            }

            int32_t dx = ((int32_t)MOX::MAP_MAX_X * (int32_t)MOX::cur_map_scale) / (adj_players * 10);
            int32_t dy = ((int32_t)MOX::MAP_MAX_Y * (int32_t)MOX::cur_map_scale) / (adj_players * 10);
            int32_t min_dist_sq = dx * dx + dy * dy;

            if (min_dist_sq < 15000) {
                min_dist_sq = 15000;
            }

            if (MAPGEN::fill_msg_ctr < (uint16_t)HOME_WORLD_GEN_MAX_CTR) {
                fonts::Set_Font_Style(4, &local_color1);
                int16_t text_width = fonts::Get_String_Width(MAPGEN::fill_msg);
                int16_t x_pos = (0x280 - text_width) / 2;
                MISC::Draw_Advancing_Fill_Message(4, &MAPGEN::fill_msg_ctr, (uint16_t)HOME_WORLD_GEN_MAX_CTR, &local_color1, &local_color2, x_pos, 215, MAPGEN::fill_msg, MAPGEN::fill_msg_bitmap, 0);
                RUSS::Mox_Sync_Update();
            }

            RUSS::Mox_Update();
            Build_Min_Star_Distances(min_dist_sq);

            if (MAPGEN::fill_msg_ctr < (uint16_t)HOME_WORLD_GEN_MAX_CTR) {
                fonts::Set_Font_Style(4, &local_color1);
                int16_t text_width = fonts::Get_String_Width(MAPGEN::fill_msg);
                int16_t x_pos = (0x280 - text_width) / 2;
                MISC::Draw_Advancing_Fill_Message(4, &MAPGEN::fill_msg_ctr, (uint16_t)HOME_WORLD_GEN_MAX_CTR, &local_color1, &local_color2, x_pos, 215, MAPGEN::fill_msg, MAPGEN::fill_msg_bitmap, 0);
                RUSS::Mox_Sync_Update();
            }

            uint8_t ok = Build_Home_Star_List(home_star_ids, min_dist_sq);
            RUSS::Mox_Update();

            if (!ok) {
                break;
            }

            adj_players++;
            if (adj_players > max_retries) {
                return 0;
            }
        }

        Randomize_Home_Worlds(home_star_ids);

        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            MOX::star[home_star_ids[i]].owner = (char)i;
        }

        Modify_Home_Worlds();

        fonts::Set_Font_Style(4, &local_color1);
        int16_t text_width = fonts::Get_String_Width(MAPGEN::fill_msg);
        int16_t x_pos = (0x280 - text_width) / 2;
        MISC::Draw_Advancing_Fill_Message(4, &MAPGEN::fill_msg_ctr, (uint16_t)HOME_WORLD_GEN_MAX_CTR, &local_color1, &local_color2, x_pos, 215, MAPGEN::fill_msg, MAPGEN::fill_msg_bitmap, 1);
        RUSS::Mox_Sync_Update();

        if (MAPGEN::special_generation_config.generate_orion) {
            Generate_Orion();
        }
        return 1;
    }

    void __cdecl Set_Up_Home_Gen_Msg() {
        // Retrieve and cache font colors for rendering
        s_colors colors1 = *MISC::Font_Colors2(4, 0x77, 0x79);
        s_colors colors2 = *MISC::Font_Colors2(4, 0x77, 0x7C);
        (void)colors2;

        // Determine home world generation counter limits
        if (MOX::NUM_STARS <= ORIGINAL_MAX_STARS) {
            HOME_WORLD_GEN_MAX_CTR = ((int32_t)MOX::NUM_STARS * 25) / 20;
        } else {
            HOME_WORLD_GEN_MAX_CTR =
                (MOX::NUM_STARS + Generation_Update_Interval() - 1) / Generation_Update_Interval() + 2;
        }
        if (MOX::NUM_STARS > 54 && MOX::NUM_STARS <= ORIGINAL_MAX_STARS) {
            HOME_WORLD_GEN_MAX_CTR = 372;
        }

        // Potential redundant logic preserved from binary:
        // concatenates companion star name if fill_msg is short,
        // but fill_msg is completely overwritten later by H_Message.
        if (strlen(MAPGEN::fill_msg) < 35) {
            strlcat(MAPGEN::fill_msg, ".", sizeof(MAPGEN::fill_msg));
        }

        video::Set_Page_Back();
        s_animation_header* lbx_buf = (s_animation_header*)buffer::Buffer_Reload(MAPGEN::s_raceopt_lbx, 0, MOX::global_cache_seg);
        animate::Draw(0, 0, lbx_buf);
        video::Copy_Back_To_Off();

        // Initialize background bitmap for the progress message
        uint32_t bmp_size = bitmap::Get_Bitmap_Size(325, 30);
        MAPGEN::fill_msg_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bmp_size);
        bitmap::Create_Blank_Bitmap(325, 30, MAPGEN::fill_msg_bitmap);
        MAPGEN::fill_msg_ctr = 0;

        // Load specific UI string and render it to the cached bitmap
        const char* msg_str = HAROLD::H_Message(0xA1);
        strlcpy(MAPGEN::fill_msg, msg_str, sizeof(MAPGEN::fill_msg));

        fonts::Set_Font_Style_Outline_Heavy(4, &colors1);
        fonts::Set_Outline_Color(2, 1);
        fonts::Print_To_Bitmap(0, 0, MAPGEN::fill_msg, MAPGEN::fill_msg_bitmap);
    }

    void __cdecl Assign_Home_Star_Names() {
        uint8_t is_race_assigned[14];
        for (int i = 0; i < 14; ++i) {
            is_race_assigned[i] = 0;
        }

        char* star_names = (char*)farload::Far_Reload_Data(MAPGEN::s_starname_lbx, 0, MOX::screen_seg, 0, 0x0D, 0x0F);
        for (int i = 0; i < 13; ++i) {
            string_overrides::Apply_String(MAPGEN::s_starname_lbx, i + 1, star_names + i * 15, 15);
        }

        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            s_player* plr = &MOX::player[i];
            int race_idx = (int)plr->race;

            if (is_race_assigned[race_idx] == 0) {
                char* src_name = &star_names[race_idx * 15];
                int16_t home_planet_id = plr->home_planet_id;
                int16_t star_idx = MOX::planet[home_planet_id].star_index;
                s_star_data* star = &MOX::star[star_idx];

                char* dst = star->name;
                char* src = src_name;
                while ((*dst++ = *src++) != '\0');

                is_race_assigned[race_idx] = 1;
            }
        }
    }

    /**
     * @brief Generates space monsters at eligible star systems based on user preferences and map scale.
     */
    void __cdecl Make_System_Monsters() {
        int16_t scaled_x = HAROLD::Get_Scaled_Value(MOX::MAP_MAX_X);
        int16_t scaled_y = HAROLD::Get_Scaled_Value(MOX::MAP_MAX_Y);
        uint32_t dist_limit_sq = (uint32_t)(scaled_x * scaled_x + scaled_y * scaled_y);

        if (MOX2::user_wants_n_space_monsters == 255) {
            if (MAPGEN::n_space_monsters > MAPGEN::monster_generation_config.natural_count_cap) {
                MAPGEN::n_space_monsters = MAPGEN::monster_generation_config.natural_count_cap;
            }
        } else {
            int invalid_stars_count = 0;
            for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                // Determine stars that are ineligible for monsters
                if (HAROLD::Star_Is_Orion(i) ||
                    HAROLD::Is_Home_Star(i) ||
                    HAROLD::Is_Colony_Star(i) ||
                    Is_Added_Food_World(i) ||
                    HAROLD::Empty_System(i) ||
                    MOX::star[i].spectral_class == STAR_CLASS_BLACK_HOLE) {
                    invalid_stars_count++;
                }
            }

            int valid_stars_count = MOX::NUM_STARS - invalid_stars_count;
            int desired_monsters = (int)MOX2::user_wants_n_space_monsters;

            if (desired_monsters > valid_stars_count) {
                desired_monsters = valid_stars_count;
            }

            MAPGEN::n_space_monsters = (uint8_t)desired_monsters;
            MOX2::user_wants_n_space_monsters = MAPGEN::n_space_monsters;
            // Disable distance constraints for custom monster counts
            dist_limit_sq = 0;
        }

        if (MAPGEN::n_space_monsters == 0) {
            return;
        }

        uint16_t weights[MAX_STARS];
        uint8_t flags[MAX_STARS];
        int16_t placed_monster_stars[MAX_STARS];

        memset(weights, 0, sizeof(weights));
        memset(flags, 0, sizeof(flags));
        memset(placed_monster_stars, 0xFF, sizeof(placed_monster_stars));

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if (!HAROLD::Star_Is_Orion(i) &&
                !Is_Added_Food_World(i) &&
                !HAROLD::Is_Home_Star(i) &&
                !HAROLD::Is_Colony_Star(i) &&
                !HAROLD::Empty_System(i) &&
                MOX::star[i].spectral_class != STAR_CLASS_BLACK_HOLE) {
                weights[i] = 1;
            }
        }

        for (int m = 0; m < (int)MAPGEN::n_space_monsters; ++m) {
            int divisor = (m * 2 > 1) ? (m * 2) : 1;
            int32_t current_min_dist = dist_limit_sq / divisor;

            int16_t selected_star = -1;
            int attempts = 0;
            bool success = false;

            do {
                int16_t star_idx = RUSS::Get_Weighted_Choice_Short(weights, MOX::NUM_STARS);
                bool dist_ok = true;

                for (int j = 0; j < m; ++j) {
                    if (Dist_Between_Stars(star_idx, placed_monster_stars[j]) < current_min_dist) {
                        dist_ok = false;
                        break;
                    }
                }

                if (dist_ok) {
                    selected_star = star_idx;
                    success = true;
                } else {
                    attempts++;
                    if (attempts > 250) {
                        success = true;
                    }
                }
            } while (!success);

            if (selected_star == -1) {
                return;
            }

            s_star_data& star = MOX::star[selected_star];
            // Store original system special for later use
            star.twinkling1 = star.system_special;
            star.system_special = SYSTEM_SPECIAL_SPACE_MONSTER;

            flags[selected_star] = 1;
            weights[selected_star] = 0;
            placed_monster_stars[m] = selected_star;
        }
    }

    int16_t __cdecl Random_Planet(int16_t star_idx) {
        uint16_t weights[5] = {0, 0, 0, 0, 0};
        const s_star_data* star = &MOX::star[star_idx];

        for (int16_t i = 0; i < 5; ++i) {
            int16_t p_idx = star->planet_index[i];
            if (p_idx != -1) {
                int8_t type = MOX::planet[p_idx].planet_type;
                if (type >= PLANET_TYPE_ASTEROID && type <= PLANET_TYPE_PLANET) {
                    weights[i] = 1;
                }
            }
        }

        uint16_t choice_idx = RUSS::Get_Weighted_Choice_Short(weights, 5);
        return star->planet_index[choice_idx];
    }

    void __cdecl Upgrade_Planet_Guarded_By_Monster(s_planet_data* planet, int16_t monster_type) {
        bool should_upgrade_climate = true;
        uint16_t size_weights[PLANET_SIZE_COUNT] = {0};
        uint16_t mineral_weights[MINERAL_RESOURCE_COUNT] = {0};
        uint16_t climate_weights[PLANET_CLIMATE_COUNT] = {0};

        switch (monster_type) {
            case 10:
                size_weights[PLANET_SIZE_MEDIUM] = 1;
                size_weights[PLANET_SIZE_LARGE] = 1;
                size_weights[PLANET_SIZE_HUGE] = 1;
                mineral_weights[MINERAL_RESOURCE_RICH] = 1;
                mineral_weights[MINERAL_RESOURCE_ULTRA_RICH] = 1;
                if (planet->planet_type == PLANET_TYPE_PLANET) {
                    should_upgrade_climate = false;
                } else {
                    for (int i = 0; i < PLANET_CLIMATE_COUNT; ++i) climate_weights[i] = 1;
                }
                break;
            case 11:
            case 14:
                size_weights[PLANET_SIZE_LARGE] = 1;
                size_weights[PLANET_SIZE_HUGE] = 1;
                mineral_weights[MINERAL_RESOURCE_RICH] = 1;
                mineral_weights[MINERAL_RESOURCE_ULTRA_RICH] = 1;
                climate_weights[PLANET_CLIMATE_TERRAN] = 1;
                climate_weights[PLANET_CLIMATE_GAIA] = 1;
                break;
            case 12:
                size_weights[PLANET_SIZE_MEDIUM] = 1;
                size_weights[PLANET_SIZE_LARGE] = 1;
                size_weights[PLANET_SIZE_HUGE] = 1;
                mineral_weights[MINERAL_RESOURCE_RICH] = 1;
                mineral_weights[MINERAL_RESOURCE_ULTRA_RICH] = 1;
                climate_weights[PLANET_CLIMATE_TERRAN] = 1;
                climate_weights[PLANET_CLIMATE_GAIA] = 1;
                break;
            case 13:
                size_weights[PLANET_SIZE_LARGE] = 1;
                size_weights[PLANET_SIZE_HUGE] = 1;
                mineral_weights[MINERAL_RESOURCE_RICH] = 1;
                mineral_weights[MINERAL_RESOURCE_ULTRA_RICH] = 1;
                if (planet->planet_type == PLANET_TYPE_PLANET) {
                    should_upgrade_climate = false;
                } else {
                    for (int i = 0; i < PLANET_CLIMATE_COUNT; ++i) climate_weights[i] = 1;
                }
                break;
            default:
                break;
        }

        planet->planet_type = PLANET_TYPE_PLANET;

        int16_t new_size = RUSS::Get_Weighted_Choice_Short(size_weights, PLANET_SIZE_COUNT);
        if ((int16_t)planet->size < new_size) {
            planet->size = (int8_t)new_size;
        }

        int16_t new_minerals = RUSS::Get_Weighted_Choice_Short(mineral_weights, MINERAL_RESOURCE_COUNT);
        if ((int16_t)planet->mineral_class < new_minerals) {
            planet->mineral_class = (int8_t)new_minerals;
        }

        if (should_upgrade_climate) {
            int16_t new_climate = RUSS::Get_Weighted_Choice_Short(climate_weights, PLANET_CLIMATE_COUNT);
            if ((int16_t)planet->climate < new_climate) {
                planet->climate = (int8_t)new_climate;
            }
        }

        int16_t planet_index = (int16_t)(planet - MOX::planet);
        int16_t star_idx = planet->star_index;
        MAPGEN::Set_Secondary_Planet_Data(star_idx, planet_index);

        s_star_data* star = &MOX::star[star_idx];
        Set_Special_For_Star_Guarded_By_Monster(star, monster_type);
    }

    void __cdecl Modify_Home_Worlds() {
        int16_t home_star_ids[MAX_PLAYERS + 1];
        int16_t home_planet_id;
        int16_t home_star_count = 0;

        home_planet_id = 0;
        home_star_ids[1] = -1;
        home_star_ids[2] = -1;
        home_star_ids[3] = -1;
        home_star_ids[4] = -1;
        home_star_ids[5] = -1;
        home_star_ids[6] = -1;
        home_star_ids[7] = -1;
        home_star_ids[MAX_PLAYERS] = -1;

        // Pass 1: Collect star IDs of all home worlds
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            int8_t owner = MOX::star[i].owner;
            if (owner >= 0 && owner < MAX_PLAYERS) {
                home_star_ids[home_star_count + 1] = i;
                home_star_count++;
            }
        }

        // Pass 2: Process and enforce traits on each home world
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            int16_t owner_idx = (int16_t)MOX::star[i].owner;
            if (owner_idx >= 0 && owner_idx < MAX_PLAYERS) {
                s_player* player = &MOX::player[owner_idx];

                Enforce_3_Planet_Minimum(i);

                int16_t climate_threshold = PLANET_CLIMATE_TERRAN;
                if (player->traits[TRAIT_AQUATIC] != 0) { // Aquatic
                    climate_threshold = PLANET_CLIMATE_OCEAN;
                }

                Enforce_Climate(i, climate_threshold, &home_planet_id);
                Remove_Specials(i);

                player->home_planet_id = home_planet_id;

                if (player->traits[TRAIT_LARGE_HOME_WORLD] == 0) { // Not Large Home World
                    Enforce_Medium_Planet_Size(home_planet_id);
                } else {
                    Set_Planet_Size(home_planet_id, PLANET_SIZE_LARGE);
                }

                Enforce_Planet_Type(i, home_planet_id);
                Enforce_Planet_Abundant_Mineral_Class(home_planet_id);

                if (player->traits[TRAIT_RICH_HOME_WORLD] > 0) { // Rich Home World
                    Enforce_Mineral_Richness(home_planet_id, MINERAL_RESOURCE_RICH);
                } else if (player->traits[TRAIT_RICH_HOME_WORLD] < 0) { // Poor Home World
                    Enforce_Mineral_Richness(home_planet_id, MINERAL_RESOURCE_POOR);
                }

                if (player->traits[TRAIT_ANCIENT_HOME_WORLD] != 0) { // Ancient Home World (Artifacts)
                    Set_System_Planet_Artifact_Special(home_planet_id, i);
                }

                int8_t gravity_type;
                if (player->traits[TRAIT_LOW_G_WORLD] != 0) {
                    gravity_type = PLANET_GRAVITY_LOW_G;
                } else {
                    gravity_type = (player->traits[TRAIT_HEAVY_G_WORLD] == 0) ? PLANET_GRAVITY_NORMAL_G : PLANET_GRAVITY_HEAVY_G;
                }
                Enforce_Gravity(home_planet_id, gravity_type, owner_idx);

                MOX::planet[home_planet_id].food_per_farmer = MAPGEN::Generate_Food_Per_Farmer(i, home_planet_id);
                MOX::planet[home_planet_id].max_farms = MAPGEN::Generate_Max_Farms(i, home_planet_id);

                ERICNET::Init_Homeworld_Colony(home_planet_id, owner_idx);

                Guarantee_A_Planet_At_Min_Dist(i, 4, &home_star_ids[1]);

                if (MOX::star[i].system_special == SYSTEM_SPECIAL_SPACE_MONSTER) {
                    MOX::star[i].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                }
            }
        }

        MOX::NUM_COLONIES = MOX::NUM_PLAYERS;
        Assign_Home_Star_Names();
        Ensure_At_Least_1_Food_Planet();
    }

    void __cdecl Enforce_Climate(int16_t star_idx, int16_t climate_id, int16_t* out_planet_idx) {
        struct PlanetRef {
            int16_t planet_idx;
            int16_t orbit;
        };

        PlanetRef candidates[5];
        int16_t candidate_count = 0;
        bool found_climate = false;
        *out_planet_idx = -1;

        for (int16_t orbit = 0; orbit < 5; ++orbit) {
            int16_t p_idx = MOX::star[star_idx].planet_index[orbit];
            if (p_idx > -1) {
                candidates[candidate_count].planet_idx = p_idx;
                candidates[candidate_count].orbit = orbit;
                candidate_count++;

                if (MOX::planet[p_idx].climate == (int8_t)climate_id) {
                    found_climate = true;
                    *out_planet_idx = p_idx;
                    break;
                }
            }
        }

        if (candidate_count == 0) {
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "Found no satellites after 3_MIN enforcement: Star: %d", (int)star_idx);
            exitm::Exit_With_Message(error_msg);
            return;
        }

        if (found_climate) {
            int16_t p_idx = *out_planet_idx;
            MOX::planet[p_idx].food_per_farmer = MAPGEN::Generate_Food_Per_Farmer(star_idx, p_idx);
            return;
        }

        // If no planet has the requested climate, we force one.
        while (true) {
            int16_t valid_groups[PLANET_GROUP_COUNT];
            int16_t valid_group_count = 0;

            for (int16_t group_idx = 0; group_idx < PLANET_GROUP_COUNT; ++group_idx) {
                if (MOX::climate_roll_table[climate_id][group_idx] > 0) {
                    valid_groups[valid_group_count++] = group_idx;
                }
            }

            int8_t current_spectral = MOX::star[star_idx].spectral_class;
            bool possible_with_spectral = false;

            if (current_spectral >= 0 && current_spectral < STAR_CLASS_BLACK_HOLE) {
                for (int16_t i = 0; i < 5; ++i) {
                    int8_t allowed_group = MAPGEN::environment_group[current_spectral][i];
                    for (int16_t j = 0; j < valid_group_count; ++j) {
                        if ((int16_t)allowed_group == valid_groups[j]) {
                            possible_with_spectral = true;
                            break;
                        }
                    }
                    if (possible_with_spectral) break;
                }
            }

            if (possible_with_spectral) {
                // Pick a random planet from the candidates and force the climate/group
                int16_t chosen_candidate = game_random::Random(candidate_count) - 1;
                int16_t p_idx = candidates[chosen_candidate].planet_idx;
                *out_planet_idx = p_idx;

                MOX::planet[p_idx].climate = (int8_t)climate_id;

                int16_t chosen_group_idx = game_random::Random(valid_group_count) - 1;
                MOX::planet[p_idx].group = (int8_t)valid_groups[chosen_group_idx];

                MOX::planet[p_idx].food_per_farmer = MAPGEN::Generate_Food_Per_Farmer(star_idx, p_idx);
                return;
            } else {
                // If the current star's spectral class cannot support this climate, regenerate it
                MOX::star[star_idx].spectral_class = MAPGEN::Generate_Spectral_Class((int16_t)MOX::settings.galaxy_age);
            }
        }
    }

    void __cdecl Remove_Specials(int16_t star_idx) {
        s_star_data& star = MOX::star[star_idx];

        star.system_special = SYSTEM_SPECIAL_NO_SPECIAL;
        star.wormhole_star_id = -1;

        for (int i = 0; i < 5; ++i) {
            int16_t planet_idx = star.planet_index[i];
            if (planet_idx > -1) {
                MOX::planet[planet_idx].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
            }
        }
    }

    void __cdecl Set_Planet_Size(int16_t planet_index, int8_t size) {
        MOX::planet[planet_index].size = size;
    }

    void __cdecl Enforce_Medium_Planet_Size(int16_t planet_idx) {
        s_planet_data& planet = MOX::planet[planet_idx];

        if (planet.size != PLANET_SIZE_MEDIUM) {
            planet.size = PLANET_SIZE_MEDIUM;
            planet.max_farms = MOX::planet_max_farms[planet.size];
        }
    }

    void __cdecl Enforce_Planet_Type(int16_t star_idx, int16_t planet_idx) {
        bool found = false;
        for (int16_t i = 0; i < 5; ++i) {
            if (found) {
                break;
            }
            if (MOX::star[star_idx].planet_index[i] == planet_idx) {
                found = true;
            }
        }

        MOX::planet[planet_idx].planet_type = PLANET_TYPE_PLANET;
    }

    void __cdecl Enforce_Planet_Abundant_Mineral_Class(int16_t planet_idx) {
        if (MOX::planet[planet_idx].mineral_class != MINERAL_RESOURCE_ABUNDANT) {
            MOX::planet[planet_idx].mineral_class = MINERAL_RESOURCE_ABUNDANT;
        }
    }

    void __cdecl Set_System_Planet_Artifact_Special(int16_t planet_idx, int16_t star_idx) {
        MOX::planet[planet_idx].planet_special = (int8_t)SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
        MOX::star[star_idx].system_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
    }

    void __cdecl Enforce_Gravity(int16_t planet_idx, int8_t gravity_class, int16_t player_idx) {
        MOX::planet[planet_idx].gravity_class = gravity_class;

        // Mineral Class 4 = Ultra Rich
        if (MOX::planet[planet_idx].mineral_class == MINERAL_RESOURCE_ULTRA_RICH) {
            // Trait index 0 corresponds to Heavy-G World
            if (MOX::player[player_idx].traits[TRAIT_HEAVY_G_WORLD] == 0) {
                uint8_t new_minerals = MAPGEN::Generate_Mineral_Class(MOX::planet[planet_idx].star_index, planet_idx);

                // Logic quirk: The reassignment target is the planet at the player's slot index.
                MOX::planet[player_idx].mineral_class = (int8_t)new_minerals;
            }
        }
    }

    uint8_t __cdecl Ensure_At_Least_1_Food_Planet() {
        int16_t planet_buffer[MAX_PLANETS];
        uint8_t overall_success = 1;

        for (int i = 0; i < MAX_PLANETS; ++i) {
            planet_buffer[i] = -1;
        }

        for (int16_t colony_idx = 0; colony_idx < MOX::NUM_COLONIES; ++colony_idx) {
            s_colony& colony = MOX::colony[colony_idx];
            int8_t owner = colony.owner;
            int16_t planet_idx = colony.planet;

            if (planet_idx > -1 && MOX::player[owner].traits[TRAIT_EATS_MINERALS] == 0) {
                int16_t buffer_count = 0;
                int16_t food_star = -1;
                int16_t nearest_empty_star = -1;
                int16_t nearest_star_in_range = -1;
                int16_t min_dist = 999;
                uint8_t found_food_system = 0;

                int16_t colony_star = MOX::planet[planet_idx].star_index;

                for (int16_t star_idx = 1; star_idx < MOX::NUM_STARS; ++star_idx) {
                    if (found_food_system != 0) break;

                    if (star_idx != colony_star) {
                        int16_t dist = GEO::Parsecs_Between_Stars(colony_star, star_idx);

                        if (dist < min_dist) {
                            min_dist = dist;
                            if (dist <= 6) {
                                nearest_star_in_range = star_idx;
                            }
                        }

                        if (dist <= 6) {
                            if (HAROLD::N_Planets_In_System(star_idx) == 0) {
                                if (MOX::star[star_idx].spectral_class != STAR_CLASS_BLACK_HOLE) {
                                    nearest_empty_star = star_idx;
                                }
                            } else {
                                found_food_system = (uint8_t)HAROLD::N_Food_Planets_In_System(star_idx);
                                if (found_food_system == 0) {
                                    Add_Non_Food_Planets_To_List(star_idx, planet_buffer, &buffer_count);
                                } else {
                                    if (food_star == -1) {
                                        food_star = star_idx;
                                    }
                                }
                            }
                        }
                    }
                }

                if (found_food_system == 0) {
                    if (nearest_empty_star != -1) {
                        int16_t orbit = Usable_Orbit(nearest_empty_star);
                        int16_t new_planet_idx = MOX::planet_count;
                        Make_One_Planet(nearest_empty_star, new_planet_idx, orbit, 1);
                        food_star = nearest_empty_star;
                        MOX::planet_count++;
                    } else if (nearest_star_in_range != -1) {
                        int32_t rnd = game_random::Random(buffer_count);
                        int16_t target_p_idx = planet_buffer[rnd - 1];
                        int8_t original_special = MOX::planet[target_p_idx].planet_special;
                        int16_t star_of_planet = MOX::planet[target_p_idx].star_index;
                        food_star = star_of_planet;

                        uint16_t saved_planet_count = MOX::planet_count;
                        MOX::planet_count = target_p_idx;

                        uint32_t orbit_index = ERIC::Get_Sint_Index_From_List(target_p_idx, MOX::star[star_of_planet].planet_index, 5);
                        Make_One_Planet(star_of_planet, target_p_idx, (int16_t)orbit_index, 1);

                        MOX::planet[target_p_idx].planet_special = original_special;
                        MOX::planet_count = saved_planet_count;
                    }
                } else {
                    if (buffer_count > 0 && food_star == -1) {
                        food_star = MOX::planet[planet_buffer[0]].star_index;
                    }
                }

                nearest_food_worlds[owner] = food_star;

                if (food_star >= 0 &&
                    MOX::star[food_star].system_special == SYSTEM_SPECIAL_SPACE_MONSTER) {
                    MOX::star[food_star].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                }

                if (food_star == -1 && MOX::player[owner].traits[TRAIT_EATS_MINERALS] == 0) {
                    overall_success = 0;
                }
            }
        }

        return overall_success;
    }

    void __cdecl Add_Non_Food_Planets_To_List(int16_t star_idx, int16_t* planet_list, int16_t* count_in_out) {
        s_star_data* star = &MOX::star[star_idx];

        for (int i = 0; i < 5; ++i) {
            int16_t planet_id = star->planet_index[i];

            if (planet_id > -1) {
                planet_list[*count_in_out] = planet_id;
                *count_in_out += 1;
            }
        }
    }

    int16_t __cdecl Get_Orion_Star_Id() {
        int16_t scale = MOX::cur_map_scale;
        int16_t x_center = (scale * 253) / 10 + 21;
        int16_t y_center = (scale * 200) / 10 + 21;
        int16_t dist_limit = 4;

        int16_t out_x, out_y;
        MAINSCR::Get_PointXY_Draw_Coords(x_center, y_center, &out_x, &out_y);

        uint8_t visited[MAX_STARS];
        memset(visited, 0, sizeof(visited));

        s_orion_candidate candidates[MAX_STARS];
        memset(candidates, 0xFF, sizeof(candidates));

        int candidate_count = 0;
        do {
            if (dist_limit >= 8) break;

            for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                // Potential buffer overflow if NUM_STARS > MAX_STARS
                if (visited[i] == 0) {
                    if (HAROLD::Is_Home_Star(i) == 0 && Is_Added_Food_World(i) == 0) {
                        int16_t d = GEO::Parsecs_Between_Points(MOX::star[i].x, MOX::star[i].y, x_center, y_center);
                        if (d < dist_limit) {
                            visited[i] = 1;
                            candidates[candidate_count].dist = (int32_t)d;
                            candidates[candidate_count].star_idx = i;
                            candidate_count++;
                        }
                    }
                }
            }
            dist_limit++;
        } while (candidate_count < 4);

        // Potential buffer overflow if NUM_STARS > MAX_STARS
        qsort(candidates, (uint32_t)MOX::NUM_STARS, sizeof(s_orion_candidate), qsort_1);

        int valid_count = 0;
        for (int i = 0; i < (int)MOX::NUM_STARS; ++i) {
            if (candidates[i].dist > 0) {
                candidates[valid_count].star_idx = candidates[i].star_idx;
                candidates[valid_count].dist = candidates[i].dist;
                valid_count++;
            }
        }

        int16_t selected_star;
        if (valid_count < 2) {
            bool found = false;
            do {
                selected_star = game_random::Random((int)MOX::NUM_STARS) - 1;
                if (HAROLD::Is_Home_Star(selected_star) == 0 && Is_Added_Food_World(selected_star) == 0) {
                    found = true;
                }
            } while (!found);
        } else {
            int r = game_random::Random(valid_count);
            selected_star = candidates[r - 1].star_idx;
        }

        return selected_star;
    }

    int16_t __cdecl Generate_Orion() {
        int16_t orion_star_idx = Get_Orion_Star_Id();
        int16_t planet_indices_backup[5];
        int16_t selected_orbit = -1;

        for (int16_t i = 0; i < 5; ++i) {
            int16_t p_idx = MOX::star[0].planet_index[i];
            planet_indices_backup[i] = p_idx;
            if (p_idx > -1 && i > 2) {
                if (game_random::Random(i - 1) == 1) {
                    selected_orbit = i;
                }
            }
        }

        if (selected_orbit < 2) {
            selected_orbit = (int16_t)game_random::Random(3) + 1;
        }

        int16_t target_planet_idx = MOX::star[0].planet_index[selected_orbit];
        if (target_planet_idx == -1) {
            target_planet_idx = MOX::planet_count;
            MOX::star[0].planet_index[selected_orbit] = target_planet_idx;
            MOX::planet_count++;
        }

        for (int16_t i = 0; i < 5; ++i) {
            if (i != selected_orbit && MOX::star[0].planet_index[i] != -1) {
                MOX::planet[MOX::star[0].planet_index[i]].planet_type = PLANET_TYPE_ASTEROID;
            }
        }

        MOX::star[0].system_special = SYSTEM_SPECIAL_ORION_STAR;
        memcpy(MOX::star[0].name, "Orion", sizeof("Orion"));

        if (MOX::star[0].spectral_class == STAR_CLASS_BLACK_HOLE) {
            MOX::star[0].spectral_class =
                MAPGEN::Generate_Non_Black_Hole_Spectral_Class(MOX::settings.galaxy_age);
        }

        s_star_data star_temp;
        memcpy(&star_temp, &MOX::star[orion_star_idx], sizeof(s_star_data));

        int16_t star0_x = MOX::star[0].x;
        int16_t star0_y = MOX::star[0].y;

        MOX::star[orion_star_idx].x = star0_x;
        MOX::star[orion_star_idx].y = star0_y;

        MOX::star[0].x = star_temp.x;
        MOX::star[0].y = star_temp.y;

        s_planet_data* p = &MOX::planet[target_planet_idx];
        memset(p, 0, sizeof(s_planet_data));

        p->colony_index = -1;
        p->orbit = (int8_t)selected_orbit;
        p->planet_type = PLANET_TYPE_PLANET;
        p->size = PLANET_SIZE_HUGE;
        p->gravity_class = PLANET_GRAVITY_NORMAL_G;
        p->group = PLANET_GROUP_LIFE_SUPPORTING;
        p->climate = PLANET_CLIMATE_GAIA;
        p->mineral_class = MINERAL_RESOURCE_ULTRA_RICH;
        p->planet_special = SYSTEM_SPECIAL_ORION_STAR;
        p->food_per_farmer = MAPGEN::Generate_Food_Per_Farmer(0, target_planet_idx);
        p->max_farms = MAPGEN::Generate_Max_Farms(0, target_planet_idx);

        return orion_star_idx;
    }
} // namespace HOMEGEN