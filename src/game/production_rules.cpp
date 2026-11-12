#include "pch.h"

namespace production_rules {
    static uint32_t Integer_Square_Root(uint64_t value) {
        uint64_t result = 0;
        uint64_t bit = (uint64_t)1 << 62;
        while (bit > value) bit >>= 2;

        while (bit != 0) {
            if (value >= result + bit) {
                value -= result + bit;
                result = (result >> 1) + bit;
            } else {
                result >>= 1;
            }
            bit >>= 2;
        }
        return (uint32_t)result;
    }

    static int32_t Clamp(int64_t value, const s_clamp* clamp) {
        if (value < clamp->minimum) return clamp->minimum;
        if (value > clamp->maximum) return clamp->maximum;
        return (int32_t)value;
    }

    static bool Compare(int64_t left, uint8_t operator_, int64_t right) {
        switch (operator_) {
            case COMPARISON_IGNORE: return true;
            case COMPARISON_EQ: return left == right;
            case COMPARISON_NE: return left != right;
            case COMPARISON_LT: return left < right;
            case COMPARISON_LE: return left <= right;
            case COMPARISON_GT: return left > right;
            case COMPARISON_GE: return left >= right;
            default: return false;
        }
    }

    static bool Conditions_Active(const s_condition_group* conditions) {
        for (int32_t i = 0; i < FLAG_COUNT; ++i) {
            if (conditions->flags[i] != REQUIREMENT_IGNORED) return true;
        }
        for (int32_t i = 0; i < METRIC_COUNT; ++i) {
            if (conditions->thresholds[i].operator_ != COMPARISON_IGNORE ||
                conditions->ranges[i].minimum.operator_ != COMPARISON_IGNORE ||
                conditions->ranges[i].maximum.operator_ != COMPARISON_IGNORE) {
                return true;
            }
        }
        for (int32_t i = 0; i < VALUE_CONDITION_COUNT; ++i) {
            if (conditions->value_conditions[i].operator_ != COMPARISON_IGNORE) return true;
        }
        for (int32_t i = 0; i < METRIC_CONDITION_COUNT; ++i) {
            if (conditions->metric_conditions[i].operator_ != COMPARISON_IGNORE) return true;
        }
        for (int32_t i = 0; i < SCALED_CONDITION_COUNT; ++i) {
            if (conditions->scaled_conditions[i].operator_ != COMPARISON_IGNORE) return true;
        }
        return conditions->government_family.operator_ != COMPARISON_IGNORE ||
            conditions->climate.operator_ != COMPARISON_IGNORE ||
            conditions->mineral_class.operator_ != COMPARISON_IGNORE;
    }

    static bool Evaluate_Flags_And_Thresholds(const s_context* context, const uint8_t* flags, const s_threshold* thresholds) {
        for (int32_t i = 0; i < FLAG_COUNT; ++i) {
            if (flags[i] == REQUIREMENT_REQUIRED && context->flags[i] == 0) return false;
            if (flags[i] == REQUIREMENT_FORBIDDEN && context->flags[i] != 0) return false;
            if (flags[i] > REQUIREMENT_FORBIDDEN) return false;
        }
        for (int32_t i = 0; i < METRIC_COUNT; ++i) {
            if (!Compare(context->metrics[i], thresholds[i].operator_, thresholds[i].value)) return false;
        }
        return true;
    }

    static bool Evaluate_Group(const s_context* context, const s_condition_group* conditions) {
        if (!Evaluate_Flags_And_Thresholds(context, conditions->flags, conditions->thresholds)) return false;

        for (int32_t i = 0; i < METRIC_COUNT; ++i) {
            const s_range* range = &conditions->ranges[i];
            if (!Compare(context->metrics[i], range->minimum.operator_, range->minimum.value) ||
                !Compare(context->metrics[i], range->maximum.operator_, range->maximum.value)) {
                return false;
            }
        }
        for (int32_t i = 0; i < VALUE_CONDITION_COUNT; ++i) {
            const s_value_condition* condition = &conditions->value_conditions[i];
            if (condition->metric >= METRIC_COUNT ||
                !Compare(context->metrics[condition->metric], condition->operator_, condition->value)) {
                return false;
            }
        }
        for (int32_t i = 0; i < METRIC_CONDITION_COUNT; ++i) {
            const s_metric_condition* condition = &conditions->metric_conditions[i];
            if (condition->left >= METRIC_COUNT || condition->right >= METRIC_COUNT ||
                !Compare(context->metrics[condition->left], condition->operator_, context->metrics[condition->right])) {
                return false;
            }
        }
        for (int32_t i = 0; i < SCALED_CONDITION_COUNT; ++i) {
            const s_scaled_condition* condition = &conditions->scaled_conditions[i];
            if (condition->left_metric >= METRIC_COUNT || condition->right_metric >= METRIC_COUNT ||
                !Compare((int64_t)context->metrics[condition->left_metric] * condition->left_factor,
                    condition->operator_,
                    (int64_t)context->metrics[condition->right_metric] * condition->right_factor)) {
                return false;
            }
        }
        return Compare(context->government_family, conditions->government_family.operator_, conditions->government_family.value) &&
            Compare(context->climate, conditions->climate.operator_, conditions->climate.value) &&
            Compare(context->mineral_class, conditions->mineral_class.operator_, conditions->mineral_class.value);
    }

    bool Evaluate_Eligibility(const s_context* context, const s_eligibility* eligibility) {
        if (!Evaluate_Group(context, &eligibility->all)) return false;

        bool has_alternative = false;
        for (int32_t i = 0; i < ALTERNATIVE_COUNT; ++i) {
            if (!Conditions_Active(&eligibility->alternatives[i])) continue;
            has_alternative = true;
            if (Evaluate_Group(context, &eligibility->alternatives[i])) return true;
        }
        return !has_alternative;
    }

    static bool Evaluate_Term_Condition(const s_context* context, const s_term_condition* condition) {
        return Evaluate_Flags_And_Thresholds(context, condition->flags, condition->thresholds);
    }

    static int64_t Formula_Weight(const s_context* context, const s_weight* weight, int32_t legacy_weight) {
        switch (weight->formula) {
            case WEIGHT_FORMULA_ADDITIVE:
                return weight->base;
            case WEIGHT_FORMULA_LEGACY:
                return (int64_t)legacy_weight + weight->base;
            case WEIGHT_FORMULA_COMBAT_NEED_RATIO: {
                int64_t need = context->metrics[METRIC_COMBAT_NEED];
                int64_t current = context->metrics[METRIC_CURRENT_STRENGTH];
                int64_t denominator = current + need;
                if (need <= 0 || denominator <= 0) return 0;
                int64_t result = need * weight->combat_need_ratio.scale / denominator;
                if (context->flags[FLAG_AT_WAR]) result *= weight->combat_need_ratio.wartime_multiplier;
                return result;
            }
            case WEIGHT_FORMULA_STRATEGIC_NEED: {
                const s_strategic_need* formula = &weight->strategic_need;
                if (formula->metric >= METRIC_COUNT) return 0;
                int64_t result = ((int64_t)context->metrics[formula->metric] * formula->need_multiplier +
                                     formula->base_need) *
                    formula->result_multiplier;
                int32_t war_level = context->metrics[METRIC_WAR_LEVEL];
                if (war_level > 0 && formula->wartime_factor == WARTIME_FACTOR_RATIO) {
                    if (formula->wartime_ratio.denominator == 0) return 0;
                    result = result * formula->wartime_ratio.numerator /
                        formula->wartime_ratio.denominator;
                } else if (war_level > 0 && formula->wartime_factor == WARTIME_FACTOR_INVERSE_WAR_LEVEL_SQUARED) {
                    result /= (int64_t)war_level * war_level;
                }
                return result;
            }
            case WEIGHT_FORMULA_DEFER: {
                int32_t war_level = context->metrics[METRIC_WAR_LEVEL];
                if (war_level <= 0) return weight->defer.peace;
                return (int64_t)weight->defer.wartime_base -
                    (int64_t)war_level * weight->defer.wartime_penalty_per_level;
            }
            default:
                return 0;
        }
    }

    int32_t Evaluate_Weight(const s_context* context, const s_weight* weight, int32_t legacy_weight) {
        int64_t result = Formula_Weight(context, weight, legacy_weight);

        for (int32_t i = 0; i < METRIC_TERM_COUNT; ++i) {
            const s_metric_term* term = &weight->metric_terms[i];
            if (term->metric == METRIC_NONE || term->metric >= METRIC_COUNT ||
                !Evaluate_Term_Condition(context, &term->when) || term->factor.denominator == 0) {
                continue;
            }
            int64_t value = (int64_t)context->metrics[term->metric] * term->factor.numerator /
                term->factor.denominator;
            if (term->transform == TRANSFORM_SQRT) {
                value = value > 0 ? Integer_Square_Root((uint64_t)value) : 0;
            } else if (term->transform != TRANSFORM_LINEAR) {
                continue;
            }
            result += Clamp(value, &term->clamp);
        }
        for (int32_t i = 0; i < VALUE_TERM_COUNT; ++i) {
            const s_value_term* term = &weight->value_terms[i];
            if (term->value != 0 && Evaluate_Term_Condition(context, &term->when)) {
                result += Clamp(term->value, &term->clamp);
            }
        }
        return Clamp(result, &weight->clamp);
    }

    void Apply_Cutoff(const s_cutoff* cutoff, int32_t multiplier_index, const s_candidate* candidates, int32_t* weights, int32_t count) {
        if (cutoff->mode != CUTOFF_RELATIVE) return;

        int32_t reference_maximum = 0;
        for (int32_t i = 0; i < count; ++i) {
            if (candidates[i].cutoff_stage != CUTOFF_STAGE_AFTER && weights[i] > reference_maximum) {
                reference_maximum = weights[i];
            }
        }
        int32_t multiplier = cutoff->multiplier[multiplier_index >= 0 && multiplier_index < 6 ? multiplier_index : 0];
        for (int32_t i = 0; i < count; ++i) {
            if (candidates[i].cutoff_stage == CUTOFF_STAGE_NORMAL &&
                (int64_t)multiplier * weights[i] < reference_maximum) {
                weights[i] = 0;
            }
        }
    }
} // namespace production_rules

namespace COLCALC {
    int32_t __cdecl Production_Cost_To_Buy(int32_t product_cost, int32_t production_spent) {
        if (production_spent >= product_cost) {
            return 0;
        }

        const int32_t half_cost = product_cost / 2;
        const int32_t tenth_cost = product_cost / 10;
        if (production_spent >= half_cost) {
            return (product_cost - production_spent) * 2;
        }

        int32_t buy_cost = (product_cost - half_cost) * 2;
        if (production_spent >= tenth_cost) {
            return buy_cost + (half_cost - production_spent) * 5;
        }

        buy_cost += (half_cost - tenth_cost) * 5;
        return buy_cost + (tenth_cost - production_spent) * 10;
    }
} // namespace COLCALC