#include "evaluator/Evaluator.hpp"

#include "ExtraMath.hpp"
#include "evaluator/EvaluatorAlgebra.hpp"
#include "evaluator/EvaluatorBuiltins.hpp"
#include "evaluator/EvaluatorFunctions.hpp"
#include "evaluator/EvaluatorSpecialForms.hpp"
#include "evaluator/FunctionRegistry.hpp"
#include "expr/ExprUtils.hpp"
#include "normalizer/Normalizer.hpp"
#include "util/Logging.hpp"
#include "util/Overloaded.hpp"

#include <algorithm>
#include <unordered_set>

namespace aleph3 {

namespace {

ExprPtr evaluate_impl(const ExprPtr& expr, EvaluationContext& ctx, std::unordered_set<std::string>& visited);

ExprPtr evaluate_function(const FunctionCall& func, EvaluationContext& ctx) {
    const std::string& name = func.head;

    auto& registry = FunctionRegistry::instance();
    if (registry.has_function(name)) {
        return registry.get_function(name)(func, ctx);
    }

    if (is_special_form_function(name)) {
        return evaluate_special_form(func, ctx);
    }

    if (auto builtin_result = evaluate_builtin_function(func, ctx)) {
        return builtin_result;
    }

    if (is_user_defined_function(name, ctx)) {
        return evaluate_user_defined_function(func, ctx);
    }

    return make_expr<FunctionCall>(name, func.args);
}

ExprPtr evaluate_impl(const ExprPtr& expr, EvaluationContext& ctx, std::unordered_set<std::string>& visited) {
    ALEPH3_LOG("evaluate: input = " << to_string_raw(expr));

    auto result = std::visit(overloaded{
        [](const Number& num) -> ExprPtr {
            return make_expr<Number>(num.value);
        },
        [](const Complex& c) -> ExprPtr {
            return make_expr<Complex>(c.real, c.imag);
        },
        [](const Rational& r) -> ExprPtr {
            auto [n, d] = normalize_rational(r.numerator, r.denominator);
            return make_expr<Rational>(n, d);
        },
        [](const Boolean& boolean) -> ExprPtr {
            return make_expr<Boolean>(boolean.value);
        },
        [](const String& str) -> ExprPtr {
            return make_expr<String>(str.value);
        },
        [&](const Symbol& sym) -> ExprPtr {
            if (visited.count(sym.name)) {
                return make_expr<Symbol>(sym.name);
            }

            auto it = ctx.variables.find(sym.name);
            if (it == ctx.variables.end()) {
                return make_expr<Symbol>(sym.name);
            }

            visited.insert(sym.name);
            auto result = evaluate_impl(it->second, ctx, visited);
            visited.erase(sym.name);
            return result;
        },
        [&](const FunctionCall& func) -> ExprPtr {
            if (is_symbolic_algebra_function(func.head)) {
                return evaluate_algebra_function(func, ctx);
            }

            if (func.head == "List") {
                std::vector<ExprPtr> evaluated_elements;
                evaluated_elements.reserve(func.args.size());
                for (const auto& arg : func.args) {
                    evaluated_elements.push_back(evaluate(arg, ctx));
                }
                return make_expr<List>(evaluated_elements);
            }

            return evaluate_function(func, ctx);
        },
        [&](const FunctionDefinition& def) -> ExprPtr {
            return register_user_defined_function(def, ctx);
        },
        [&](const Assignment& assign) -> ExprPtr {
            ctx.variables[assign.name] = evaluate(assign.value, ctx);
            return make_expr<Symbol>(assign.name);
        },
        [&](const Rule& rule) -> ExprPtr {
            auto lhs = evaluate_impl(rule.lhs, ctx, visited);
            auto rhs = evaluate_impl(rule.rhs, ctx, visited);
            return make_expr<Rule>(lhs, rhs);
        },
        [](const List& list) -> ExprPtr {
            return std::make_shared<Expr>(list);
        },
        [](const Infinity&) -> ExprPtr {
            return make_expr<Infinity>();
        },
        [](const Indeterminate&) -> ExprPtr {
            return make_expr<Indeterminate>();
        }
    }, *expr);

    ALEPH3_LOG("evaluate: result = " << to_string_raw(result));
    return result;
}

}  // namespace

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx) {
    ExprPtr norm = normalize_expr(expr);
    std::unordered_set<std::string> visited;
    return evaluate_impl(norm, ctx, visited);
}

std::string expr_to_key(const ExprPtr& expr) {
    auto norm = normalize_expr(expr);
    std::string s = to_string_raw(norm);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

}  // namespace aleph3
