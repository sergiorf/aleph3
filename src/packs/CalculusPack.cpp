#include "packs/CalculusPack.hpp"

#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "expr/ExprUtils.hpp"
#include "normalizer/Normalizer.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace aleph3::packs {

namespace {

constexpr std::string_view kPackageName = "core-calculus";
constexpr std::int64_t kMaxDerivativeOrder = 1024;

struct DerivativeResult {
    ExprPtr expr;
    bool contains_held_derivative = false;
};

struct DerivativeRequest {
    std::string variable;
    std::int64_t order = 1;
};

bool is_exact_or_inexact_constant_atom(const ExprPtr& expr) {
    return std::holds_alternative<Number>(*expr) ||
           std::holds_alternative<Rational>(*expr) ||
           std::holds_alternative<Complex>(*expr) ||
           std::holds_alternative<Boolean>(*expr) ||
           std::holds_alternative<String>(*expr) ||
           std::holds_alternative<Infinity>(*expr) ||
           std::holds_alternative<ComplexInfinity>(*expr) ||
           std::holds_alternative<Indeterminate>(*expr);
}

bool same_symbol(const ExprPtr& expr, const std::string& variable) {
    const auto* symbol = std::get_if<Symbol>(expr.get());
    return symbol != nullptr && symbol->name == variable;
}

bool depends_on(const ExprPtr& expr, const std::string& variable) {
    if (same_symbol(expr, variable)) {
        return true;
    }
    if (is_exact_or_inexact_constant_atom(expr) || std::holds_alternative<Symbol>(*expr)) {
        return false;
    }
    if (const auto* list = std::get_if<List>(expr.get())) {
        for (const auto& element : list->elements) {
            if (depends_on(element, variable)) {
                return true;
            }
        }
        return false;
    }
    if (const auto* call = std::get_if<FunctionCall>(expr.get())) {
        for (const auto& arg : call->args) {
            if (depends_on(arg, variable)) {
                return true;
            }
        }
        return false;
    }
    if (const auto* rule = std::get_if<Rule>(expr.get())) {
        return depends_on(rule->lhs, variable) || depends_on(rule->rhs, variable);
    }
    if (const auto* assignment = std::get_if<Assignment>(expr.get())) {
        return assignment->name == variable || depends_on(assignment->value, variable);
    }
    if (const auto* definition = std::get_if<FunctionDefinition>(expr.get())) {
        if (definition->name == variable) {
            return true;
        }
        for (const auto& param : definition->params) {
            if (param.name == variable) {
                return true;
            }
            if (param.default_value && depends_on(param.default_value, variable)) {
                return true;
            }
        }
        return depends_on(definition->body, variable);
    }
    return false;
}

ExprPtr numeric_zero() {
    return make_expr<Number>(0.0);
}

ExprPtr numeric_one() {
    return make_expr<Number>(1.0);
}

ExprPtr held_derivative(const ExprPtr& expr, const std::string& variable) {
    return make_fcall("D", {expr, make_expr<Symbol>(variable)});
}

ExprPtr maybe_reduce(const ExprPtr& expr, EvaluationContext& ctx, bool contains_held_derivative) {
    auto normalized = normalize_expr(expr);
    if (contains_held_derivative) {
        return normalized;
    }
    return evaluate(normalized, ctx);
}

DerivativeResult differentiate_expr(const ExprPtr& expr, const std::string& variable, EvaluationContext& ctx);

DerivativeResult derivative_sum(const FunctionCall& call, const std::string& variable, EvaluationContext& ctx) {
    std::vector<ExprPtr> terms;
    terms.reserve(call.args.size());
    bool held = false;
    for (const auto& arg : call.args) {
        auto term = differentiate_expr(arg, variable, ctx);
        terms.push_back(term.expr);
        held = held || term.contains_held_derivative;
    }
    return {maybe_reduce(make_fcall("Plus", terms), ctx, held), held};
}

DerivativeResult derivative_product(const FunctionCall& call, const std::string& variable, EvaluationContext& ctx) {
    std::vector<ExprPtr> product_terms;
    bool held = false;

    for (std::size_t i = 0; i < call.args.size(); ++i) {
        if (!depends_on(call.args[i], variable)) {
            continue;
        }
        auto differentiated_factor = differentiate_expr(call.args[i], variable, ctx);
        held = held || differentiated_factor.contains_held_derivative;

        std::vector<ExprPtr> factors;
        factors.reserve(call.args.size());
        for (std::size_t j = 0; j < call.args.size(); ++j) {
            factors.push_back(i == j ? differentiated_factor.expr : call.args[j]);
        }
        product_terms.push_back(make_fcall("Times", factors));
    }

    if (product_terms.empty()) {
        return {numeric_zero(), false};
    }
    return {maybe_reduce(make_fcall("Plus", product_terms), ctx, held), held};
}

bool is_supported_numeric_exponent(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(expr.get())) {
        return std::isfinite(number->value);
    }
    return std::holds_alternative<Rational>(*expr);
}

ExprPtr subtract_one(const ExprPtr& expr, EvaluationContext& ctx) {
    return evaluate(make_fcall("Plus", {expr, make_expr<Number>(-1.0)}), ctx);
}

DerivativeResult derivative_power(const FunctionCall& call, const std::string& variable, EvaluationContext& ctx) {
    if (call.args.size() != 2) {
        return {held_derivative(make_fcall(call.head, call.args), variable), true};
    }

    const auto& base = call.args[0];
    const auto& exponent = call.args[1];
    if (!depends_on(make_fcall(call.head, call.args), variable)) {
        return {numeric_zero(), false};
    }

    if (is_supported_numeric_exponent(exponent)) {
        auto base_derivative = differentiate_expr(base, variable, ctx);
        auto reduced_exponent = subtract_one(exponent, ctx);
        auto result = make_fcall("Times", {
            exponent,
            make_fcall("Power", {base, reduced_exponent}),
            base_derivative.expr
        });
        const bool held = base_derivative.contains_held_derivative;
        return {maybe_reduce(result, ctx, held), held};
    }

    return {held_derivative(make_fcall(call.head, call.args), variable), true};
}

DerivativeResult derivative_chain(
    const FunctionCall& call,
    const std::string& variable,
    EvaluationContext& ctx) {
    if (call.args.size() != 1) {
        return {held_derivative(make_fcall(call.head, call.args), variable), true};
    }

    const auto& inner = call.args.front();
    auto inner_derivative = differentiate_expr(inner, variable, ctx);

    ExprPtr outer_derivative;
    if (call.head == "Sin") {
        outer_derivative = make_fcall("Cos", {inner});
    } else if (call.head == "Cos") {
        outer_derivative = make_fcall("Times", {make_expr<Number>(-1.0), make_fcall("Sin", {inner})});
    } else if (call.head == "Exp") {
        outer_derivative = make_fcall("Exp", {inner});
    } else if (call.head == "Log") {
        outer_derivative = make_fcall("Power", {inner, make_expr<Number>(-1.0)});
    } else if (call.head == "Sqrt") {
        outer_derivative = make_fcall("Times", {
            make_expr<Rational>(1, 2),
            make_fcall("Power", {inner, make_expr<Rational>(-1, 2)})
        });
    } else {
        return {held_derivative(make_fcall(call.head, call.args), variable), true};
    }

    const bool held = inner_derivative.contains_held_derivative;
    return {maybe_reduce(make_fcall("Times", {outer_derivative, inner_derivative.expr}), ctx, held), held};
}

DerivativeResult differentiate_expr(const ExprPtr& expr, const std::string& variable, EvaluationContext& ctx) {
    ctx.consume_evaluation_step();

    if (is_exact_or_inexact_constant_atom(expr)) {
        return {numeric_zero(), false};
    }

    if (const auto* symbol = std::get_if<Symbol>(expr.get())) {
        return {make_expr<Number>(symbol->name == variable ? 1.0 : 0.0), false};
    }

    if (const auto* call = std::get_if<FunctionCall>(expr.get())) {
        if (!depends_on(expr, variable)) {
            return {numeric_zero(), false};
        }
        if (call->head == "Plus") {
            return derivative_sum(*call, variable, ctx);
        }
        if (call->head == "Times") {
            return derivative_product(*call, variable, ctx);
        }
        if (call->head == "Power") {
            return derivative_power(*call, variable, ctx);
        }
        return derivative_chain(*call, variable, ctx);
    }

    if (!depends_on(expr, variable)) {
        return {numeric_zero(), false};
    }
    return {held_derivative(expr, variable), true};
}

std::string require_variable_symbol(const ExprPtr& expr) {
    const auto* symbol = std::get_if<Symbol>(expr.get());
    if (symbol == nullptr) {
        throw_invalid_form("D expects a derivative variable to be a symbol");
    }
    return symbol->name;
}

std::int64_t require_derivative_order(const ExprPtr& expr) {
    std::int64_t order = 0;
    if (const auto* number = std::get_if<Number>(expr.get())) {
        if (!std::isfinite(number->value) ||
            number->value < 0.0 ||
            std::floor(number->value) != number->value) {
            throw_invalid_form("D derivative order must be a nonnegative exact integer");
        }
        if (number->value > static_cast<double>(kMaxDerivativeOrder)) {
            throw_invalid_form("D derivative order exceeds the supported limit");
        }
        order = static_cast<std::int64_t>(number->value);
    } else if (const auto* rational = std::get_if<Rational>(expr.get())) {
        if (rational->denominator != 1 || rational->numerator < 0) {
            throw_invalid_form("D derivative order must be a nonnegative exact integer");
        }
        if (rational->numerator > kMaxDerivativeOrder) {
            throw_invalid_form("D derivative order exceeds the supported limit");
        }
        order = rational->numerator;
    } else {
        throw_invalid_form("D derivative order must be a nonnegative exact integer");
    }
    return order;
}

DerivativeRequest require_derivative_request(const ExprPtr& expr) {
    if (std::holds_alternative<Symbol>(*expr)) {
        return {require_variable_symbol(expr), 1};
    }

    std::vector<ExprPtr> elements;
    if (const auto* list = std::get_if<List>(expr.get())) {
        elements = list->elements;
    } else if (const auto* call = std::get_if<FunctionCall>(expr.get());
               call != nullptr && call->head == "List") {
        elements = call->args;
    }

    if (elements.size() != 2) {
        throw_invalid_form("D expects the second argument to be a symbol or {symbol, nonnegative exact integer}");
    }

    return {
        require_variable_symbol(elements[0]),
        require_derivative_order(elements[1])
    };
}

ExprPtr evaluate_derivative_call(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 2) {
        throw_invalid_arity_exact(func.head, 2);
    }

    const DerivativeRequest request = require_derivative_request(func.args[1]);
    auto result = evaluate(func.args[0], ctx);
    for (std::int64_t order = 0; order < request.order; ++order) {
        result = differentiate_expr(result, request.variable, ctx).expr;
    }
    return result;
}

}  // namespace

void register_calculus_pack(kernel::FunctionRegistry& registry) {
    registry.register_pack_function(
        std::string(kPackageName),
        "D",
        evaluate_derivative_call,
        "Differentiate an expression with respect to a symbol.",
        true);
    registry.register_pack_function(
        std::string(kPackageName),
        "Differentiate",
        evaluate_derivative_call,
        "Alias for D.",
        true);
}

}  // namespace aleph3::packs
