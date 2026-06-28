#include "kernel/Assumptions.hpp"

#include "evaluator/EvaluatorErrors.hpp"
#include "expr/ExprUtils.hpp"
#include "normalizer/Normalizer.hpp"

#include <array>
#include <cmath>

namespace aleph3::kernel {

namespace {

constexpr unsigned kPositiveSign = 1u;
constexpr unsigned kZeroSign = 2u;
constexpr unsigned kNegativeSign = 4u;

bool is_integral_number_value(double value) {
    return std::floor(value) == value;
}

bool is_sign_predicate(std::string_view head) {
    return head == "Positive" || head == "Negative" ||
           head == "NonNegative" || head == "NonPositive" ||
           head == "ZeroQ" || head == "NonZeroQ";
}

bool is_zero_literal(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(&*expr)) {
        return number->value == 0.0;
    }
    if (const auto* rational = std::get_if<Rational>(&*expr)) {
        return rational->numerator == 0;
    }
    return false;
}

std::optional<int> compare_numeric_to_zero(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(&*expr)) {
        if (number->value > 0.0) {
            return 1;
        }
        if (number->value < 0.0) {
            return -1;
        }
        return 0;
    }
    if (const auto* rational = std::get_if<Rational>(&*expr)) {
        if (rational->numerator > 0) {
            return 1;
        }
        if (rational->numerator < 0) {
            return -1;
        }
        return 0;
    }
    return std::nullopt;
}

bool is_exact_two(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(&*expr)) {
        return number->value == 2.0;
    }
    if (const auto* rational = std::get_if<Rational>(&*expr)) {
        return rational->numerator == 2 && rational->denominator == 1;
    }
    return false;
}

ExprPtr make_negated(const ExprPtr& expr) {
    return make_times({make_expr<Number>(-1), expr});
}

ExprPtr negate_known_nonpositive_expr(const ExprPtr& expr) {
    auto normalize_times_with_updated_lead = [](std::vector<ExprPtr> factors) -> ExprPtr {
        if (const auto* number = std::get_if<Number>(&*factors.front()); number != nullptr &&
            number->value == 1.0) {
            factors.erase(factors.begin());
        } else if (const auto* rational = std::get_if<Rational>(&*factors.front());
                   rational != nullptr &&
                   rational->numerator == 1 && rational->denominator == 1) {
            factors.erase(factors.begin());
        }

        if (factors.empty()) {
            return make_expr<Number>(1.0);
        }
        if (factors.size() == 1) {
            return factors.front();
        }
        return normalize_expr(make_expr<FunctionCall>("Times", factors));
    };

    if (const auto* number = std::get_if<Number>(&*expr)) {
        return make_expr<Number>(-number->value);
    }
    if (const auto* rational = std::get_if<Rational>(&*expr)) {
        return make_expr<Rational>(-rational->numerator, rational->denominator);
    }

    if (const auto* func = std::get_if<FunctionCall>(&*expr);
        func != nullptr && func->head == "Times" && !func->args.empty()) {
        std::vector<ExprPtr> factors = func->args;
        if (const auto* number = std::get_if<Number>(&*factors.front())) {
            factors.front() = make_expr<Number>(-number->value);
            return normalize_times_with_updated_lead(std::move(factors));
        }
        if (const auto* rational = std::get_if<Rational>(&*factors.front())) {
            factors.front() = make_expr<Rational>(-rational->numerator, rational->denominator);
            return normalize_times_with_updated_lead(std::move(factors));
        }
    }

    return normalize_expr(make_negated(expr));
}

std::optional<int64_t> exact_integer_value(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(&*expr)) {
        if (!is_integral_number_value(number->value)) {
            return std::nullopt;
        }
        return static_cast<int64_t>(number->value);
    }
    if (const auto* rational = std::get_if<Rational>(&*expr)) {
        if (rational->denominator != 1) {
            return std::nullopt;
        }
        return rational->numerator;
    }
    return std::nullopt;
}

void apply_sign_fact(SymbolAssumptionFacts& facts, std::string_view head) {
    if (head == "Greater" || head == "Positive") {
        facts.positive = true;
        facts.nonnegative = true;
        facts.nonzero = true;
        return;
    }
    if (head == "GreaterEqual" || head == "NonNegative") {
        facts.nonnegative = true;
        return;
    }
    if (head == "Less" || head == "Negative") {
        facts.negative = true;
        facts.nonpositive = true;
        facts.nonzero = true;
        return;
    }
    if (head == "LessEqual" || head == "NonPositive") {
        facts.nonpositive = true;
        return;
    }
    if (head == "Equal" || head == "ZeroQ") {
        facts.zero = true;
        facts.nonnegative = true;
        facts.nonpositive = true;
        return;
    }
    if (head == "NotEqual" || head == "NonZeroQ") {
        facts.nonzero = true;
    }
}

unsigned sign_mask_from_facts(const SymbolAssumptionFacts& facts) {
    unsigned signs = kPositiveSign | kZeroSign | kNegativeSign;

    if (facts.positive) {
        signs &= kPositiveSign;
    }
    if (facts.negative) {
        signs &= kNegativeSign;
    }
    if (facts.zero) {
        signs &= kZeroSign;
    }
    if (facts.nonnegative) {
        signs &= (kPositiveSign | kZeroSign);
    }
    if (facts.nonpositive) {
        signs &= (kZeroSign | kNegativeSign);
    }
    if (facts.nonzero) {
        signs &= (kPositiveSign | kNegativeSign);
    }

    return signs;
}

unsigned negate_sign_mask(unsigned signs) {
    unsigned result = 0u;
    if ((signs & kPositiveSign) != 0u) {
        result |= kNegativeSign;
    }
    if ((signs & kNegativeSign) != 0u) {
        result |= kPositiveSign;
    }
    if ((signs & kZeroSign) != 0u) {
        result |= kZeroSign;
    }
    return result;
}

unsigned multiply_sign_masks(unsigned left, unsigned right) {
    unsigned result = 0u;
    const std::array<unsigned, 3> sign_values{kNegativeSign, kZeroSign, kPositiveSign};

    for (const unsigned left_sign : sign_values) {
        if ((left & left_sign) == 0u) {
            continue;
        }
        for (const unsigned right_sign : sign_values) {
            if ((right & right_sign) == 0u) {
                continue;
            }
            if (left_sign == kZeroSign || right_sign == kZeroSign) {
                result |= kZeroSign;
            } else if (left_sign == right_sign) {
                result |= kPositiveSign;
            } else {
                result |= kNegativeSign;
            }
        }
    }

    return result;
}

std::optional<unsigned> derive_sign_mask(const ExprPtr& expr, const AssumptionStore& assumptions);

std::optional<unsigned> derive_symbol_sign_mask(
    const std::string& name,
    const AssumptionStore& assumptions) {
    const auto* facts = assumptions.find_symbol_facts(name);
    if (facts == nullptr) {
        return std::nullopt;
    }

    const unsigned signs = sign_mask_from_facts(*facts);
    if (signs == 0u) {
        return std::nullopt;
    }
    return signs;
}

std::optional<unsigned> derive_power_sign_mask(
    const ExprPtr& base,
    const ExprPtr& exponent,
    const AssumptionStore& assumptions) {
    const auto exact_exponent = exact_integer_value(exponent);
    if (!exact_exponent.has_value() || *exact_exponent <= 0) {
        return std::nullopt;
    }

    const auto base_signs = derive_sign_mask(base, assumptions);
    if (!base_signs.has_value()) {
        return std::nullopt;
    }

    if ((*exact_exponent % 2) == 0) {
        unsigned result = 0u;
        if (((*base_signs) & kZeroSign) != 0u) {
            result |= kZeroSign;
        }
        if (((*base_signs) & (kPositiveSign | kNegativeSign)) != 0u) {
            result |= kPositiveSign;
        }
        return result == 0u ? std::nullopt : std::optional<unsigned>(result);
    }

    return base_signs;
}

std::optional<unsigned> derive_times_sign_mask(
    const FunctionCall& func,
    const AssumptionStore& assumptions) {
    if (func.args.empty()) {
        return std::nullopt;
    }

    unsigned combined = kPositiveSign;
    for (const auto& arg : func.args) {
        const auto factor_signs = derive_sign_mask(arg, assumptions);
        if (!factor_signs.has_value()) {
            return std::nullopt;
        }
        combined = multiply_sign_masks(combined, *factor_signs);
    }

    if (combined == 0u) {
        return std::nullopt;
    }
    return combined;
}

std::optional<unsigned> derive_sign_mask(const ExprPtr& expr, const AssumptionStore& assumptions) {
    if (expr == nullptr) {
        return std::nullopt;
    }

    const auto normalized = normalize_expr(expr);
    if (const auto relation = compare_numeric_to_zero(normalized); relation.has_value()) {
        if (*relation > 0) {
            return kPositiveSign;
        }
        if (*relation < 0) {
            return kNegativeSign;
        }
        return kZeroSign;
    }

    if (const auto* symbol = std::get_if<Symbol>(&*normalized)) {
        return derive_symbol_sign_mask(symbol->name, assumptions);
    }

    const auto* func = std::get_if<FunctionCall>(&*normalized);
    if (func == nullptr) {
        return std::nullopt;
    }

    if (func->head == "Negate" && func->args.size() == 1) {
        const auto inner_signs = derive_sign_mask(func->args[0], assumptions);
        if (!inner_signs.has_value()) {
            return std::nullopt;
        }
        return negate_sign_mask(*inner_signs);
    }

    if (func->head == "Times") {
        return derive_times_sign_mask(*func, assumptions);
    }

    if (func->head == "Power" && func->args.size() == 2) {
        return derive_power_sign_mask(func->args[0], func->args[1], assumptions);
    }

    return std::nullopt;
}

std::optional<bool> evaluate_sign_predicate_from_mask(
    std::string_view predicate_name,
    unsigned signs) {
    if (signs == 0u) {
        return std::nullopt;
    }

    if (predicate_name == "Positive") {
        if (signs == kPositiveSign) {
            return true;
        }
        if ((signs & kPositiveSign) == 0u) {
            return false;
        }
        return std::nullopt;
    }
    if (predicate_name == "Negative") {
        if (signs == kNegativeSign) {
            return true;
        }
        if ((signs & kNegativeSign) == 0u) {
            return false;
        }
        return std::nullopt;
    }
    if (predicate_name == "NonNegative") {
        if ((signs & kNegativeSign) == 0u) {
            return true;
        }
        if ((signs & (kPositiveSign | kZeroSign)) == 0u) {
            return false;
        }
        return std::nullopt;
    }
    if (predicate_name == "NonPositive") {
        if ((signs & kPositiveSign) == 0u) {
            return true;
        }
        if ((signs & (kNegativeSign | kZeroSign)) == 0u) {
            return false;
        }
        return std::nullopt;
    }
    if (predicate_name == "ZeroQ") {
        if (signs == kZeroSign) {
            return true;
        }
        if ((signs & kZeroSign) == 0u) {
            return false;
        }
        return std::nullopt;
    }
    if (predicate_name == "NonZeroQ") {
        if ((signs & kZeroSign) == 0u) {
            return true;
        }
        if (signs == kZeroSign) {
            return false;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<bool> evaluate_zero_comparison(
    const std::string& head,
    unsigned signs) {
    if (head == "Greater") {
        return evaluate_sign_predicate_from_mask("Positive", signs);
    }
    if (head == "GreaterEqual") {
        return evaluate_sign_predicate_from_mask("NonNegative", signs);
    }
    if (head == "Less") {
        return evaluate_sign_predicate_from_mask("Negative", signs);
    }
    if (head == "LessEqual") {
        return evaluate_sign_predicate_from_mask("NonPositive", signs);
    }
    if (head == "Equal") {
        return evaluate_sign_predicate_from_mask("ZeroQ", signs);
    }
    if (head == "NotEqual") {
        return evaluate_sign_predicate_from_mask("NonZeroQ", signs);
    }
    return std::nullopt;
}

ExprPtr refine_function_call_with_assumptions(
    const FunctionCall& func,
    const AssumptionStore& assumptions) {
    std::vector<ExprPtr> refined_args;
    refined_args.reserve(func.args.size());
    for (const auto& arg : func.args) {
        refined_args.push_back(refine_expr_with_assumptions(arg, assumptions));
    }

    if (refined_args.size() == 2) {
        if (auto assumed = assumptions.evaluate_comparison(func.head, refined_args[0], refined_args[1]);
            assumed.has_value()) {
            return make_expr<Boolean>(*assumed);
        }
    }

    if (is_sign_predicate(func.head) && refined_args.size() == 1) {
        if (auto assumed = assumptions.evaluate_predicate(func.head, refined_args[0]);
            assumed.has_value()) {
            return make_expr<Boolean>(*assumed);
        }
    }

    if (func.head == "Abs" && refined_args.size() == 1) {
        if (auto nonnegative = assumptions.evaluate_predicate("NonNegative", refined_args[0]);
            nonnegative == std::optional<bool>(true)) {
            return refined_args[0];
        }
        if (auto nonpositive = assumptions.evaluate_predicate("NonPositive", refined_args[0]);
            nonpositive == std::optional<bool>(true)) {
            return negate_known_nonpositive_expr(refined_args[0]);
        }
    }

    if (func.head == "Sqrt" && refined_args.size() == 1) {
        if (const auto* power = std::get_if<FunctionCall>(&*refined_args[0]);
            power != nullptr && power->head == "Power" && power->args.size() == 2 &&
            is_exact_two(power->args[1])) {
            if (auto nonnegative = assumptions.evaluate_predicate("NonNegative", power->args[0]);
                nonnegative == std::optional<bool>(true)) {
                return power->args[0];
            }
            if (auto nonpositive = assumptions.evaluate_predicate("NonPositive", power->args[0]);
                nonpositive == std::optional<bool>(true)) {
                return negate_known_nonpositive_expr(power->args[0]);
            }
        }
    }

    return make_expr<FunctionCall>(func.head, refined_args);
}

}  // namespace

void AssumptionStore::assume_boolean_symbol(std::string name, bool value) {
    symbol_facts_[std::move(name)].boolean_value = value;
}

void AssumptionStore::assume_sign_fact(std::string symbol_name, const std::string& head) {
    auto& facts = symbol_facts_[std::move(symbol_name)];
    apply_sign_fact(facts, head);
}

void AssumptionStore::assume_comparison(const FunctionCall& comparison) {
    if (comparison.args.size() != 2) {
        throw_invalid_form("Assumptions only support binary comparisons.");
    }

    exact_true_forms_.insert(to_string(make_expr<FunctionCall>(comparison.head, comparison.args)));

    const auto* symbol = std::get_if<Symbol>(&*comparison.args[0]);
    if (symbol == nullptr || !is_zero_literal(comparison.args[1])) {
        return;
    }

    assume_sign_fact(symbol->name, comparison.head);
}

void AssumptionStore::assume(const ExprPtr& expr) {
    if (expr == nullptr) {
        throw_invalid_form("Assumptions cannot be empty.");
    }

    if (const auto* boolean = std::get_if<Boolean>(&*expr)) {
        if (boolean->value) {
            return;
        }
        throw_invalid_form("False assumptions are not supported yet.");
    }

    if (const auto* symbol = std::get_if<Symbol>(&*expr)) {
        assume_boolean_symbol(symbol->name, true);
        return;
    }

    if (const auto* list = std::get_if<List>(&*expr)) {
        for (const auto& item : list->elements) {
            assume(item);
        }
        return;
    }

    const auto* func = std::get_if<FunctionCall>(&*expr);
    if (func == nullptr) {
        throw_invalid_form("Unsupported assumption form.");
    }

    if (func->head == "And") {
        for (const auto& arg : func->args) {
            assume(arg);
        }
        return;
    }

    if (func->head == "Not") {
        if (func->args.size() != 1) {
            throw_invalid_arity_exact("Not", 1);
        }
        const auto* symbol = std::get_if<Symbol>(&*func->args[0]);
        if (symbol == nullptr) {
            throw_invalid_form("Assumptions only support Not[symbol] for boolean facts.");
        }
        assume_boolean_symbol(symbol->name, false);
        return;
    }

    if (is_sign_predicate(func->head)) {
        if (func->args.size() != 1) {
            throw_invalid_arity_exact(func->head, 1);
        }
        const auto* symbol = std::get_if<Symbol>(&*func->args[0]);
        if (symbol == nullptr) {
            throw_invalid_form("Assumption predicates only support symbol arguments.");
        }
        assume_sign_fact(symbol->name, func->head);
        return;
    }

    if (func->head == "Equal" || func->head == "NotEqual" ||
        func->head == "Less" || func->head == "LessEqual" ||
        func->head == "Greater" || func->head == "GreaterEqual") {
        assume_comparison(*func);
        return;
    }

    throw_invalid_form("Unsupported assumption form.");
}

std::optional<bool> AssumptionStore::find_boolean_value(std::string_view symbol_name) const {
    auto it = symbol_facts_.find(std::string(symbol_name));
    if (it == symbol_facts_.end()) {
        return std::nullopt;
    }
    return it->second.boolean_value;
}

std::optional<bool> AssumptionStore::evaluate_predicate(
    std::string_view predicate_name,
    const ExprPtr& expr) const {
    if (expr == nullptr || !is_sign_predicate(predicate_name)) {
        return std::nullopt;
    }

    if (const auto relation = compare_numeric_to_zero(expr); relation.has_value()) {
        if (predicate_name == "Positive") {
            return *relation > 0;
        }
        if (predicate_name == "Negative") {
            return *relation < 0;
        }
        if (predicate_name == "NonNegative") {
            return *relation >= 0;
        }
        if (predicate_name == "NonPositive") {
            return *relation <= 0;
        }
        if (predicate_name == "ZeroQ") {
            return *relation == 0;
        }
        if (predicate_name == "NonZeroQ") {
            return *relation != 0;
        }
    }

    if (const auto derived_signs = derive_sign_mask(expr, *this); derived_signs.has_value()) {
        return evaluate_sign_predicate_from_mask(predicate_name, *derived_signs);
    }

    return std::nullopt;
}

std::optional<bool> AssumptionStore::evaluate_comparison(
    const std::string& head,
    const ExprPtr& left,
    const ExprPtr& right) const {
    if (left == nullptr || right == nullptr) {
        return std::nullopt;
    }

    if (exact_true_forms_.contains(to_string(make_expr<FunctionCall>(head, std::vector<ExprPtr>{left, right})))) {
        return true;
    }

    if (is_zero_literal(right)) {
        if (const auto signs = derive_sign_mask(left, *this); signs.has_value()) {
            return evaluate_zero_comparison(head, *signs);
        }
    }
    if (is_zero_literal(left)) {
        if (const auto signs = derive_sign_mask(right, *this); signs.has_value()) {
            if (head == "Greater") {
                return evaluate_zero_comparison("Less", *signs);
            }
            if (head == "GreaterEqual") {
                return evaluate_zero_comparison("LessEqual", *signs);
            }
            if (head == "Less") {
                return evaluate_zero_comparison("Greater", *signs);
            }
            if (head == "LessEqual") {
                return evaluate_zero_comparison("GreaterEqual", *signs);
            }
            if (head == "Equal" || head == "NotEqual") {
                return evaluate_zero_comparison(head, *signs);
            }
        }
    }
    return std::nullopt;
}

const SymbolAssumptionFacts* AssumptionStore::find_symbol_facts(std::string_view symbol_name) const {
    auto it = symbol_facts_.find(std::string(symbol_name));
    return it == symbol_facts_.end() ? nullptr : &it->second;
}

ExprPtr refine_expr_with_assumptions(const ExprPtr& expr, const AssumptionStore& assumptions) {
    if (expr == nullptr) {
        return nullptr;
    }
    if (const auto* func = std::get_if<FunctionCall>(&*expr)) {
        return refine_function_call_with_assumptions(*func, assumptions);
    }
    return expr;
}

}  // namespace aleph3::kernel
