#include "pch.h"

#include "build_queue.h"

namespace BUILD_QUEUE {
    namespace {
        constexpr int QUEUE_CAPACITY = 7;

        enum e_mode {
            MODE_APPEND,
            MODE_PREPEND,
            MODE_MERGE,
        };

        bool Is_Mode(uint8_t instruction) {
            return instruction >= INSTRUCTION_APPEND && instruction <= INSTRUCTION_MERGE;
        }

        bool Is_Product(uint8_t instruction) {
            return instruction >= INSTRUCTION_HOUSING;
        }

        bool Is_Building(uint8_t instruction) {
            constexpr int building_instruction_base = INSTRUCTION_BUILDING_BASE;
            constexpr int building_count = BUILDING_COUNT;
            return instruction > building_instruction_base &&
                instruction - building_instruction_base < building_count;
        }

        int16_t Building_Product(uint8_t instruction) {
            return static_cast<int16_t>(instruction - INSTRUCTION_BUILDING_BASE);
        }

        int Queue_Length(const game::colony_product* queue) {
            for (int index = 0; index < QUEUE_CAPACITY; ++index) {
                if (queue[index] == COLONY_PRODUCTION_NONE) {
                    return index;
                }
            }
            return QUEUE_CAPACITY;
        }

        void Defragment(game::colony_product* queue) {
            int destination = 0;
            for (int source = 0; source < QUEUE_CAPACITY; ++source) {
                if (queue[source] != COLONY_PRODUCTION_NONE) {
                    queue[destination++] = queue[source];
                }
            }
            while (destination < QUEUE_CAPACITY) {
                queue[destination++] = COLONY_PRODUCTION_NONE;
            }
        }

        bool Queue_Contains(const game::colony_product* queue, game::colony_product product) {
            for (int index = 0; index < QUEUE_CAPACITY; ++index) {
                if (queue[index] == product) {
                    return true;
                }
            }
            return false;
        }

        void Delete_Ship_Product(game::colony_product product) {
            if (COLBLDG::Colony_Production_Is_Queued_Ship(product)) {
                if (auto* ship = game::Find_Queued_Ship(game::current_state, product)) COLBLDG::Delete_Ship(*ship);
            }
        }

        void Delete_At(game::colony_product* queue, int position, bool refits_only) {
            if (position < 0 || position >= QUEUE_CAPACITY) {
                return;
            }
            const game::colony_product product = queue[position];
            if (product == COLONY_PRODUCTION_NONE) {
                return;
            }
            int end = position + 1;
            if (end < QUEUE_CAPACITY && queue[end] == COLONY_PRODUCTION_REPEAT) {
                ++end;
            }
            if (COLBLDG::Colony_Production_Is_Queued_Ship(product)) {
                auto* ship = game::Find_Queued_Ship(game::current_state, product);
                const bool is_refit = ship && ship->status == 4;
                if (is_refit != refits_only) {
                    return;
                }
                if (ship) COLBLDG::Delete_Ship(*ship);
            } else if (refits_only) {
                return;
            }
            for (int index = position; index < end; ++index) {
                queue[index] = COLONY_PRODUCTION_NONE;
            }
        }

        int First_Unbought_Slot(const s_colony& colony, const game::colony_product* queue) {
            if (colony.bought_outright == 0) {
                return 0;
            }
            return queue[1] == COLONY_PRODUCTION_REPEAT ? 2 : 1;
        }

        bool Colony_Base_Available(game::id_colony colony_idx, const game::colony_product* queue) {
            int available = 0;
            int queued = 0;
            for (int orbit = 0; orbit < 5; ++orbit) {
                const auto planet_idx = game::current_state.stars.at(game::current_state.planets.at(game::current_state.colonies.at(colony_idx).planet_id).star_id).orbit_planet_ids[orbit];
                if (planet_idx == game::NO_PLANET || game::current_state.planets.at(planet_idx).planet_type != PLANET_TYPE_PLANET) {
                    continue;
                }
                const game::id_colony other_idx = game::current_state.planets.at(planet_idx).colony_id;
                if (other_idx == game::NO_COLONY ||
                    (game::current_state.colonies.at(other_idx).outpost_flag != 0 &&
                        legacy_world::Encode_Player_Slot(game::current_state.colonies.at(other_idx).owner) == legacy_world::Encode_Player_Slot(game::current_state.colonies.at(colony_idx).owner))) {
                    ++available;
                }
                const game::colony_product* other_queue = other_idx == colony_idx ? queue : (other_idx != game::NO_COLONY ? game::current_state.colonies.at(other_idx).producing : nullptr);
                if (other_queue != nullptr) {
                    for (int slot = 0; slot < QUEUE_CAPACITY; ++slot) {
                        queued += other_queue[slot] == BUILDING_COLONY_BASE ? 1 : 0;
                    }
                }
            }
            return queued < available;
        }

        bool Repeat_Is_Legal(const game::colony_product* queue, int position, int previous_position) {
            if (position == 0 || previous_position < 0) {
                return false;
            }
            const game::colony_product previous = queue[previous_position];
            if ((COLBLDG::Colony_Production_Is_Building(previous) &&
                    previous != BUILDING_TERRAFORMING && previous != BUILDING_COLONY_BASE) ||
                previous == COLONY_PRODUCTION_HOUSING ||
                previous == COLONY_PRODUCTION_TRADE_GOODS ||
                previous == COLONY_PRODUCTION_NONE ||
                previous == COLONY_PRODUCTION_REPEAT) {
                return false;
            }
            return true;
        }

        game::colony_product Product_For_Instruction(game::id_colony colony_idx, uint8_t instruction, const game::colony_product* queue) {
            s_colony& colony = game::current_state.colonies.at(colony_idx);
            game::colony_product product = COLONY_PRODUCTION_NONE;
            if (Is_Building(instruction)) {
                product = Building_Product(instruction);
            } else {
                switch (instruction) {
                    case INSTRUCTION_HOUSING: product = COLONY_PRODUCTION_HOUSING; break;
                    case INSTRUCTION_TRADE_GOODS: product = COLONY_PRODUCTION_TRADE_GOODS; break;
                    case INSTRUCTION_FREIGHTER_FLEET: product = COLONY_PRODUCTION_FREIGHTERS; break;
                    case INSTRUCTION_SPY: product = COLONY_PRODUCTION_SPY; break;
                    case INSTRUCTION_ANDROID_FARMER: product = COLONY_PRODUCTION_FARMER; break;
                    case INSTRUCTION_ANDROID_WORKER: product = COLONY_PRODUCTION_WORKER; break;
                    case INSTRUCTION_ANDROID_SCIENTIST: product = COLONY_PRODUCTION_SCIENTIST; break;
                    case INSTRUCTION_REPEAT: return COLONY_PRODUCTION_REPEAT;
                    default: break;
                }
            }
            if (product != COLONY_PRODUCTION_NONE) {
                if (COLCALC::Colony_Can_Build_Product(&colony, product) != 1) {
                    return COLONY_PRODUCTION_NONE;
                }
                if (Colony_Production_Is_Continuous_Project(product) &&
                    Queue_Contains(queue, product)) {
                    return COLONY_PRODUCTION_NONE;
                }
                if (Is_Building(instruction) && product != BUILDING_COLONY_BASE &&
                    Queue_Contains(queue, product)) {
                    return COLONY_PRODUCTION_NONE;
                }
                if (product == BUILDING_COLONY_BASE &&
                    !Colony_Base_Available(colony_idx, queue)) {
                    return COLONY_PRODUCTION_NONE;
                }
                return product;
            }

            int design_idx = -1;
            s_ship_design generated_design = {};
            s_ship_design* design = nullptr;
            if (instruction >= INSTRUCTION_DESIGN1 && instruction <= INSTRUCTION_DESIGN6) {
                design_idx = instruction - INSTRUCTION_DESIGN1;
            } else if (instruction >= INSTRUCTION_COLONY_SHIP &&
                instruction <= INSTRUCTION_TRANSPORT) {
                const auto owner = colony.owner;
                if (instruction == INSTRUCTION_COLONY_SHIP) {
                    INITSHIP::Load_Colony_Ship_Design(owner, &generated_design);
                } else if (instruction == INSTRUCTION_OUTPOST_SHIP) {
                    INITSHIP::Load_Outpost_Ship_Design(owner, &generated_design);
                } else {
                    INITSHIP::Load_Transport_Ship_Design(owner, &generated_design);
                }
                design = &generated_design;
            }
            if (design_idx < 0 && design == nullptr) {
                return COLONY_PRODUCTION_NONE;
            }
            if (design != nullptr) {
                const bool can_build =
                    (design->ship_type == SHIP_TYPE_COLONY &&
                        game::current_state.players.at(colony.owner.id).tech_applications[TECH_APP_COLONY_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED) ||
                    (design->ship_type == SHIP_TYPE_OUTPOST &&
                        game::current_state.players.at(colony.owner.id).tech_applications[TECH_APP_OUTPOST_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED) ||
                    (design->ship_type == SHIP_TYPE_TRANSPORT &&
                        colony.buildings[BUILDING_MARINE_BARRACKS] != 0 &&
                        game::current_state.players.at(colony.owner.id).tech_applications[TECH_APP_TRANSPORT] == TECH_RESEARCH_STATUS_RESEARCHED);
                if (!can_build) {
                    return COLONY_PRODUCTION_NONE;
                }
            } else if (COLCALC::Colony_Can_Build_Product(
                           &colony, COLBLDG::Colony_Production_From_Ship_Design_Index(design_idx)) != 1) {
                return COLONY_PRODUCTION_NONE;
            }
            const auto key = design != nullptr
                ? COLBLDG::Create_Ship_From_Design(colony.owner, design, colony_idx)
                : COLBLDG::Create_Ship(colony.owner, design_idx, colony_idx);
            const auto ship_entry = game::current_state.ships.find(key);
            auto* ship = ship_entry == game::current_state.ships.end() ? nullptr : &ship_entry->second;
            if (!ship) {
                return COLONY_PRODUCTION_NONE;
            }
            ship->status = 6;
            return game::Queued_Ship_Product(key);
        }

        bool Equivalent(uint8_t instruction, game::colony_product product) {
            if (Is_Building(instruction)) {
                return product == Building_Product(instruction);
            }
            switch (instruction) {
                case INSTRUCTION_HOUSING: return product == COLONY_PRODUCTION_HOUSING;
                case INSTRUCTION_TRADE_GOODS: return product == COLONY_PRODUCTION_TRADE_GOODS;
                case INSTRUCTION_FREIGHTER_FLEET: return product == COLONY_PRODUCTION_FREIGHTERS;
                case INSTRUCTION_SPY: return product == COLONY_PRODUCTION_SPY;
                case INSTRUCTION_ANDROID_FARMER: return product == COLONY_PRODUCTION_FARMER;
                case INSTRUCTION_ANDROID_WORKER: return product == COLONY_PRODUCTION_WORKER;
                case INSTRUCTION_ANDROID_SCIENTIST: return product == COLONY_PRODUCTION_SCIENTIST;
                case INSTRUCTION_REPEAT: return product == COLONY_PRODUCTION_REPEAT;
                default: break;
            }
            if (!COLBLDG::Colony_Production_Is_Queued_Ship(product)) {
                return false;
            }
            const auto* ship = game::Find_Queued_Ship(std::as_const(game::current_state), product);
            if (!ship) return false;
            const auto& design = ship->d;
            if (instruction == INSTRUCTION_COLONY_SHIP) return design.ship_type == SHIP_TYPE_COLONY;
            if (instruction == INSTRUCTION_OUTPOST_SHIP) return design.ship_type == SHIP_TYPE_OUTPOST;
            if (instruction == INSTRUCTION_TRANSPORT) return design.ship_type == SHIP_TYPE_TRANSPORT;
            return instruction >= INSTRUCTION_DESIGN1 && instruction <= INSTRUCTION_DESIGN6 &&
                design.ship_type == SHIP_TYPE_COMBAT &&
                ship->status == 6;
        }

        bool Insert_At(game::id_colony colony_idx, game::colony_product* queue, uint8_t instruction, int position, int previous_position) {
            if (position < 0 || position >= QUEUE_CAPACITY ||
                queue[position] != COLONY_PRODUCTION_NONE) {
                return false;
            }
            if (instruction == INSTRUCTION_REPEAT) {
                if (!Repeat_Is_Legal(queue, position, previous_position)) {
                    return false;
                }
                queue[position] = COLONY_PRODUCTION_REPEAT;
                return true;
            }
            const game::colony_product product = Product_For_Instruction(colony_idx, instruction, queue);
            if (product == COLONY_PRODUCTION_NONE) {
                return false;
            }
            queue[position] = product;
            return true;
        }

        void Rotate_To(game::colony_product* queue, int first, int last) {
            const game::colony_product product = queue[last];
            for (int index = last; index > first; --index) {
                queue[index] = queue[index - 1];
            }
            queue[first] = product;
        }

        bool Future_List_Contains(const uint8_t* list, int start, game::colony_product product) {
            for (int index = start; index < INSTRUCTION_CAPACITY; ++index) {
                const uint8_t instruction = list[index];
                if (instruction == INSTRUCTION_END || instruction == INSTRUCTION_STOP ||
                    Is_Mode(instruction)) {
                    return false;
                }
                if (Is_Product(instruction) && Equivalent(instruction, product)) {
                    return true;
                }
            }
            return false;
        }

        void Start_Merge(const s_colony& colony, game::colony_product* queue, const uint8_t* list, int next_instruction, bool* merge_cells) {
            int cells = 0;
            int empty = 0;
            const int first = First_Unbought_Slot(colony, queue);
            for (int position = first; position < QUEUE_CAPACITY; ++position) {
                const game::colony_product product = queue[position];
                if (product == COLONY_PRODUCTION_NONE) {
                    ++empty;
                } else if (!Future_List_Contains(list, next_instruction, product)) {
                    continue;
                }
                merge_cells[position] = true;
                ++cells;
                if (position + 1 < QUEUE_CAPACITY &&
                    queue[position + 1] == COLONY_PRODUCTION_REPEAT) {
                    merge_cells[++position] = true;
                    ++cells;
                }
            }
            if (cells != empty) {
                return;
            }
            const int products = QUEUE_CAPACITY - empty;
            if (products <= first) {
                return;
            }
            memset(merge_cells, 0, sizeof(bool) * QUEUE_CAPACITY);
            for (int position = products - 1; position >= first; --position) {
                queue[position + empty] = queue[position];
            }
            for (int position = first; position < first + empty; ++position) {
                queue[position] = COLONY_PRODUCTION_NONE;
                merge_cells[position] = true;
            }
        }

        void Buy(game::id_colony colony_idx, uint8_t instruction) {
            s_colony& colony = game::current_state.colonies.at(colony_idx);
            if (colony.bought_outright != 0 || COLCALC::Colony_Is_Blockaded(&colony)) {
                return;
            }
            const game::colony_product product = colony.producing[0];
            if (product == COLONY_PRODUCTION_NONE || product == COLONY_PRODUCTION_HOUSING ||
                product == COLONY_PRODUCTION_TRADE_GOODS || product == COLONY_PRODUCTION_REPEAT) {
                return;
            }
            const int64_t total = COLCALC::Colony_Product_Cost(&colony, product);
            const int64_t spent = colony.production_spent;
            if (total <= spent ||
                (instruction == INSTRUCTION_BUY10 && spent * 10 < total) ||
                (instruction == INSTRUCTION_BUY50 && spent * 2 < total) ||
                (instruction == INSTRUCTION_BUY75 && spent * 4 < total * 3)) {
                return;
            }
            const int32_t cost = COLCALC::Colony_Cost_To_Buy_Producing_Product(&colony);
            if (cost >= 0 && cost <= game::current_state.players.at(colony.owner.id).bc) {
                COLCALC::Colony_Buy_Outright(colony_idx);
            }
        }

        bool Apply(game::id_colony colony_idx, const uint8_t* list, bool recalculate) {
            if (!game::current_state.colonies.contains(colony_idx) || list == nullptr) {
                return false;
            }
            s_colony& colony = game::current_state.colonies.at(colony_idx);
            game::colony_product queue[QUEUE_CAPACITY];
            memcpy(queue, colony.producing, sizeof(queue));
            e_mode mode = MODE_APPEND;
            bool merge_cells[QUEUE_CAPACITY] = {};
            int insertion_position = 0;

            for (int index = 0; index < INSTRUCTION_CAPACITY; ++index) {
                const uint8_t instruction = list[index];
                if (instruction == INSTRUCTION_END || instruction == INSTRUCTION_STOP) {
                    break;
                }
                if (Is_Mode(instruction)) {
                    Defragment(queue);
                    memset(merge_cells, 0, sizeof(merge_cells));
                    insertion_position = 0;
                    mode = instruction == INSTRUCTION_APPEND ? MODE_APPEND : instruction == INSTRUCTION_PREPEND ? MODE_PREPEND
                                                                                                                : MODE_MERGE;
                    if (mode == MODE_MERGE) {
                        Start_Merge(colony, queue, list, index + 1, merge_cells);
                    }
                    continue;
                }
                if (instruction == INSTRUCTION_CLEAR || instruction == INSTRUCTION_CLEAR_REFITS) {
                    const bool refits = instruction == INSTRUCTION_CLEAR_REFITS;
                    const int first = First_Unbought_Slot(colony, queue);
                    for (int position = first; position < QUEUE_CAPACITY; ++position) {
                        if (mode != MODE_MERGE || merge_cells[position]) {
                            Delete_At(queue, position, refits);
                        }
                    }
                    if (instruction == INSTRUCTION_CLEAR && mode != MODE_MERGE) {
                        Defragment(queue);
                    }
                    insertion_position = 0;
                    continue;
                }
                if (instruction == INSTRUCTION_CLEAR_CONTINUOUS) {
                    const int first = First_Unbought_Slot(colony, queue);
                    for (int position = first; position < QUEUE_CAPACITY; ++position) {
                        if (Colony_Production_Is_Continuous_Project(queue[position])) {
                            Delete_At(queue, position, false);
                        }
                    }
                    Defragment(queue);
                    memset(merge_cells, 0, sizeof(merge_cells));
                    insertion_position = 0;
                    if (mode == MODE_MERGE) {
                        Start_Merge(colony, queue, list, index + 1, merge_cells);
                    }
                    continue;
                }
                if (instruction >= INSTRUCTION_BUY && instruction <= INSTRUCTION_BUY75) {
                    if (mode == MODE_MERGE && !merge_cells[0]) {
                        continue;
                    }
                    memcpy(colony.producing, queue, sizeof(queue));
                    Buy(colony_idx, instruction);
                    if (colony.bought_outright != 0) {
                        merge_cells[0] = false;
                    }
                    continue;
                }
                if (instruction == INSTRUCTION_UNBUY) {
                    memcpy(colony.producing, queue, sizeof(queue));
                    Unbuy_Product(&colony);
                    continue;
                }
                if (!Is_Product(instruction)) {
                    continue;
                }

                if (mode == MODE_APPEND) {
                    const int position = Queue_Length(queue);
                    if (position < QUEUE_CAPACITY) {
                        Insert_At(colony_idx, queue, instruction, position, position - 1);
                    }
                } else if (mode == MODE_PREPEND) {
                    const int empty = Queue_Length(queue);
                    if (empty >= QUEUE_CAPACITY) {
                        continue;
                    }
                    if (insertion_position == 0 && colony.bought_outright != 0) {
                        insertion_position = First_Unbought_Slot(colony, queue);
                    }
                    if (Insert_At(colony_idx, queue, instruction, empty, insertion_position - 1)) {
                        Rotate_To(queue, insertion_position, empty);
                        ++insertion_position;
                    }
                } else {
                    if ((Is_Building(instruction) &&
                            Building_Product(instruction) != BUILDING_COLONY_BASE) ||
                        instruction == INSTRUCTION_HOUSING ||
                        instruction == INSTRUCTION_TRADE_GOODS) {
                        for (int position = insertion_position;
                             position < QUEUE_CAPACITY;
                             ++position) {
                            if (merge_cells[position] && Equivalent(instruction, queue[position])) {
                                Delete_At(queue, position, false);
                            }
                        }
                    }
                    while (insertion_position < QUEUE_CAPACITY &&
                        !merge_cells[insertion_position]) {
                        ++insertion_position;
                    }
                    if (insertion_position < QUEUE_CAPACITY) {
                        Delete_At(queue, insertion_position, false);
                    }
                    if (insertion_position < QUEUE_CAPACITY &&
                        Insert_At(colony_idx, queue, instruction, insertion_position, insertion_position - 1)) {
                        ++insertion_position;
                    }
                }
            }

            Defragment(queue);
            const game::colony_product original_front = colony.producing[0];
            memcpy(colony.producing, queue, sizeof(queue));
            if (original_front != colony.producing[0]) {
                colony.bought_outright = 0;
            }
            if (recalculate) {
                COLCALC::Colony_Calculation(colony_idx);
            }
            return true;
        }
    } // namespace

    bool Unbuy_Product(s_colony* colony) {
        return Unbuy_Product(game::current_state, colony);
    }

    bool Unbuy_Product(game::s_game_state& state, s_colony* colony) {
        if (colony == nullptr || colony->bought_outright == 0) {
            return true;
        }

        int32_t product_cost = colony->production_spent;
        product_cost += colony->bought_outright;
        const int32_t refund =
            COLCALC::Production_Cost_To_Buy(product_cost, colony->production_spent);
        s_player* player = &state.players.at(colony->owner.id);
        uint32_t refunded_bc = player->bc;
        refunded_bc += refund;
        player->bc = refunded_bc;
        colony->bought_outright = 0;
        return true;
    }

    bool Colony_Production_Is_Continuous_Project(game::colony_product product) {
        return product == COLONY_PRODUCTION_HOUSING ||
            product == COLONY_PRODUCTION_TRADE_GOODS;
    }

    int Parse_Autobuild_Preset(const char* text) {
        if (text == nullptr || text[0] == '\0') {
            return -1;
        }
        int preset = 0;
        for (const char* cursor = text; *cursor != '\0'; ++cursor) {
            if (*cursor < '0' || *cursor > '9') {
                return -2;
            }
            preset = preset * 10 + (*cursor - '0');
            if (preset >= PRESET_COUNT) {
                return -2;
            }
        }
        return preset;
    }

    bool Apply_Preset_To_Colony(game::id_colony colony_idx, int preset_idx) {
        return preset_idx >= 0 && preset_idx < PRESET_COUNT &&
            Apply(colony_idx, presets[preset_idx], true);
    }

    void Apply_Preset_To_Empire(game::id_player player, int preset_idx) {
        if (player.id >= MAX_PLAYERS || preset_idx < 0 || preset_idx >= PRESET_COUNT) {
            return;
        }
        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            const auto colony_idx = roster_id;
            s_colony& colony = roster_colony;
            if (colony.owner == player && colony.outpost_flag == 0) {
                Apply(colony_idx, presets[preset_idx], true);
            }
        }
        COLCALC::Do_Player_Colony_Calculations(player);
    }

    bool Apply_Custom_Autobuild(game::id_colony colony_idx) {
        if (autobuild_settings.enabled == 0 ||
            !game::current_state.colonies.contains(colony_idx)) {
            return false;
        }
        const int preset_idx = Decode_Autobuild_Preset(
            game::current_state.colonies.at(colony_idx).auto_building);
        return preset_idx >= 0 && Apply(colony_idx, presets[preset_idx], false);
    }

    void Apply_Custom_Autobuild_For_Player(game::id_player player) {
        if (player.id >= MAX_PLAYERS || autobuild_settings.enabled == 0) {
            return;
        }
        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            const auto colony_idx = roster_id;
            const s_colony& colony = roster_colony;
            if (colony.owner == player && colony.outpost_flag == 0) {
                Apply_Custom_Autobuild(colony_idx);
            }
        }
    }
} // namespace BUILD_QUEUE
