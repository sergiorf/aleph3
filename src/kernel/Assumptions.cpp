#include "kernel/Assumptions.hpp"

#include "evaluator/EvaluatorErrors.hpp"
#include "expr/ExprUtils.hpp"

namespace aleph3::kernel {

namespace {

bool is_zero_literal(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(&*expr)) {
        return number->value == 0.0;
    }
    if (const auto* rational = std::get_if<Rational>(&*expr)) {
        return rational->numerator == 0;
    }
    return false;
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

std::optional<bool> evaluate_zero_comparison(
    const std::string& head,
    const SymbolAssumptionFacts& facts) {
    if (head == "Greater") {
        if (facts.positive || (facts.nonnegative && facts.nonzero)) {
            return true;
        }
        if (facts.nonpositive || facts.zero) {
            return false;
        }
    }
    if (head == "GreaterEqual") {
        if (facts.nonnegative || facts.positive || facts.zero) {
            return true;
        }
        if (facts.negative) {
            return false;
        }
    }
    if (head == "Less") {
        if (facts.negative || (facts.nonpositive && facts.nonzero)) {
            return true;
        }
        if (facts.nonnegative || facts.zero) {
            return false;
        }
    }
    if (head == "LessEqual") {
        if (facts.nonpositive || facts.negative || facts.zero) {
            return true;
        }
        if (facts.positive) {
            return false;
        }
    }
    if (head == "Equal") {
        if (facts.zero) {
            return true;
        }
        if (facts.nonzero || facts.positive || facts.negative) {
            return false;
        }
    }
    if (head == "NotEqual") {
        if (facts.nonzero || facts.positive || facts.negative) {
            return true;
        }
        if (facts.zero) {
            return false;
        }
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

    if (func.head == "Abs" && refined_args.size() == 1) {
        if (const auto* symbol = std::get_if<Symbol>(&*refined_args[0])) {
            if (const auto* facts = assumptions.find_symbol_facts(symbol->name)) {
                if (facts->nonnegative || facts->positive || facts->zero) {
                    return refined_args[0];
                }
                if (facts->nonpositive || facts->negative) {
                    return make_negated(refined_args[0]);
                }
            }
        }
    }

    if (func.head == "Sqrt" && refined_args.size() == 1) {
        if (const auto* power = std::get_if<FunctionCall>(&*refined_args[0]);
            power != nullptr && power->head == "Power" && power->args.size() == 2 &&
            is_exact_two(power->args[1])) {
            if (const auto* symbol = std::get_if<Symbol>(&*power->args[0])) {
                if (const auto* facts = assumptions.find_symbol_facts(symbol->name)) {
                    if (facts->nonnegative || facts->positive || facts->zero) {
                        return power->args[0];
                    }
                    if (facts->nonpositive || facts->negative) {
                        return make_negated(power->args[0]);
                    }
                }
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
    if (head == "Greater") {
        facts.positive = true;
        facts.nonnegative = true;
        facts.nonzero = true;
        return;
    }
    if (head == "GreaterEqual") {
        facts.nonnegative = true;
        return;
    }
    if (head == "Less") {
        facts.negative = true;
        facts.nonpositive = true;
        facts.nonzero = true;
        return;
    }
    if (head == "LessEqual") {
        facts.nonpositive = true;
        return;
    }
    if (head == "Equal") {
        facts.zero = true;
        facts.nonnegative = true;
        facts.nonpositive = true;
        return;
    }
    if (head == "NotEqual") {
        facts.nonzero = true;
    }
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

    const auto* symbol = std::get_if<Symbol>(&*left);
    if (symbol == nullptr || !is_zero_literal(right)) {
        return std::nullopt;
    }

    const auto* facts = find_symbol_facts(symbol->name);
    if (facts == nullptr) {
        return std::nullopt;
    }
    return evaluate_zero_comparison(head, *facts);
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
