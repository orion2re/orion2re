#include "pch.h"

namespace AIBUILD {
    static void Initialize_Condition_Group_(production_rules::s_condition_group* conditions) {
        for (int32_t i = 0; i < production_rules::METRIC_COUNT; ++i) {
            conditions->thresholds[i].operator_ = production_rules::COMPARISON_IGNORE;
            conditions->ranges[i].minimum.operator_ = production_rules::COMPARISON_IGNORE;
            conditions->ranges[i].maximum.operator_ = production_rules::COMPARISON_IGNORE;
        }
        for (int32_t i = 0; i < production_rules::VALUE_CONDITION_COUNT; ++i) {
            conditions->value_conditions[i].operator_ = production_rules::COMPARISON_IGNORE;
        }
        for (int32_t i = 0; i < production_rules::METRIC_CONDITION_COUNT; ++i) {
            conditions->metric_conditions[i].operator_ = production_rules::COMPARISON_IGNORE;
        }
        for (int32_t i = 0; i < production_rules::SCALED_CONDITION_COUNT; ++i) {
            conditions->scaled_conditions[i].operator_ = production_rules::COMPARISON_IGNORE;
        }
        conditions->government_family.operator_ = production_rules::COMPARISON_IGNORE;
        conditions->climate.operator_ = production_rules::COMPARISON_IGNORE;
        conditions->mineral_class.operator_ = production_rules::COMPARISON_IGNORE;
    }

    static void Initialize_Eligibility_(production_rules::s_eligibility* eligibility) {
        Initialize_Condition_Group_(&eligibility->all);
        for (int32_t i = 0; i < production_rules::ALTERNATIVE_COUNT; ++i) {
            Initialize_Condition_Group_(&eligibility->alternatives[i]);
        }
    }

    static void Initialize_Term_Condition_(production_rules::s_term_condition* condition) {
        for (int32_t i = 0; i < production_rules::METRIC_COUNT; ++i) {
            condition->thresholds[i].operator_ = production_rules::COMPARISON_IGNORE;
        }
    }

    static void Initialize_Candidate_(production_rules::s_candidate* candidate) {
        candidate->enabled = true;
        candidate->cutoff_stage = production_rules::CUTOFF_STAGE_NORMAL;
        Initialize_Eligibility_(&candidate->eligibility);
        candidate->weight.formula = production_rules::WEIGHT_FORMULA_LEGACY;
        candidate->weight.clamp = {INT32_MIN, INT32_MAX};
        candidate->weight.strategic_need.wartime_ratio = {1, 1};
        for (int32_t i = 0; i < production_rules::METRIC_TERM_COUNT; ++i) {
            candidate->weight.metric_terms[i].factor = {1, 1};
            candidate->weight.metric_terms[i].clamp = {INT32_MIN, INT32_MAX};
            Initialize_Term_Condition_(&candidate->weight.metric_terms[i].when);
        }
        for (int32_t i = 0; i < production_rules::VALUE_TERM_COUNT; ++i) {
            candidate->weight.value_terms[i].clamp = {INT32_MIN, INT32_MAX};
            Initialize_Term_Condition_(&candidate->weight.value_terms[i].when);
        }
    }

    static s_autobuild_rules Initialize_Autobuild_Rules_() {
        s_autobuild_rules result = {};
        result.cutoff.mode = production_rules::CUTOFF_RELATIVE;
        result.cutoff.multiplier[0] = 6;
        result.cutoff.multiplier[1] = 5;
        result.cutoff.multiplier[2] = 4;
        result.cutoff.multiplier[3] = 3;
        result.cutoff.multiplier[4] = 2;
        result.cutoff.multiplier[5] = 2;
        for (int32_t i = 0; i < AUTOBUILD_CANDIDATE_COUNT; ++i) {
            Initialize_Candidate_(&result.candidates[i]);
        }
        result.candidates[BUILDING_NO_BUILDING].enabled = false;

        production_rules::s_candidate* housing = &result.candidates[BUILDING_COUNT];
        housing->cutoff_stage = production_rules::CUTOFF_STAGE_AFTER;
        housing->weight.formula = production_rules::WEIGHT_FORMULA_ADDITIVE;
        housing->eligibility.all.scaled_conditions[0] = {
            production_rules::METRIC_POPULATION,
            3,
            production_rules::COMPARISON_LE,
            production_rules::METRIC_POPULATION_LIMIT,
            1,
        };
        housing->weight.metric_terms[0].metric = production_rules::METRIC_FREE_POPULATION_CAPACITY;
        housing->weight.metric_terms[0].transform = production_rules::TRANSFORM_SQRT;
        housing->weight.metric_terms[0].factor = {3, 1};

        production_rules::s_candidate* trade_goods = &result.candidates[BUILDING_COUNT + 1];
        trade_goods->weight.formula = production_rules::WEIGHT_FORMULA_ADDITIVE;
        trade_goods->weight.base = 1;
        return result;
    }

    static s_strategic_production Initialize_Strategic_Production_() {
        s_strategic_production result = {};
        result.emergency_defense.enabled = true;
        int16_t emergency_order[EMERGENCY_DEFENSE_PRODUCT_COUNT] = {
            BUILDING_MISSILE_BASE,
            BUILDING_GROUND_BATTERIES,
            BUILDING_STAR_BASE,
            BUILDING_BATTLESTATION,
            BUILDING_STAR_FORTRESS,
            BUILDING_RADIATION_SHIELD,
            BUILDING_FLUX_SHIELD,
            BUILDING_BARRIER_SHIELD,
        };
        memcpy(result.emergency_defense.order, emergency_order, sizeof(emergency_order));
        result.economic_emergency.enabled = true;
        result.economic_emergency.product = COLONY_PRODUCTION_AI_TRADE_GOODS;
        result.expansion.enabled = true;
        result.expansion.colony_base_minimum_industry = 13;
        result.expansion.colony_ship_minimum_industry = 20;

        result.allocation.cutoff.mode = production_rules::CUTOFF_DISABLED;
        for (int32_t i = 0; i < STRATEGIC_ALLOCATION_CANDIDATE_COUNT; ++i) {
            Initialize_Candidate_(&result.allocation.candidates[i]);
        }
        result.allocation.candidates[0].weight.combat_need_ratio = {1000, 2};
        result.allocation.candidates[1].weight.strategic_need = {
            production_rules::METRIC_FREIGHTER_NEED, 500, 100, 1, production_rules::WARTIME_FACTOR_INVERSE_WAR_LEVEL_SQUARED, {1, 1}};
        result.allocation.candidates[3].weight.strategic_need = {
            production_rules::METRIC_SPY_NEED, 40, 4, 25, production_rules::WARTIME_FACTOR_INVERSE_WAR_LEVEL_SQUARED, {1, 1}};
        result.allocation.candidates[4].weight.strategic_need = {
            production_rules::METRIC_TRANSPORT_NEED, 20, 4, 25, production_rules::WARTIME_FACTOR_RATIO, {3, 2}};
        result.allocation.candidates[5].cutoff_stage = production_rules::CUTOFF_STAGE_PROTECTED;
        result.allocation.candidates[5].weight.defer = {333, 350, 100};
        return result;
    }

    s_rules rules{
        .autobuild = Initialize_Autobuild_Rules_(),
        .strategic = Initialize_Strategic_Production_(),
    };
} // namespace AIBUILD