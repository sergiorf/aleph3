#include "evaluator/Evaluator.hpp"

#include "ExtraMath.hpp"
#include "evaluator/EvaluatorAlgebra.hpp"
#include "evaluator/EvaluatorBuiltins.hpp"
#include "evaluator/EvaluatorFunctions.hpp"
#include "evaluator/EvaluatorSemantics.hpp"
#include "evaluator/EvaluatorSpecialForms.hpp"
#include "packs/PackRegistry.hpp"
#include "expr/ExprUtils.hpp"
#include "normalizer/Normalizer.hpp"
#include "util/Logging.hpp"
#include "util/Overloaded.hpp"

#include <algorithm>
#include <optional>
#include <unordered_set>

namespace aleph3 {

namespace {

ExprPtr evaluate_impl(const ExprPtr& expr, EvaluationContext& ctx, std::unordered_set<std::string>& visited);

enum class GeneralFunctionResolutionStage {
    special_form,
    registered_symbolic_function,
    builtin_function,
    user_defined_function,
    unresolved_symbolic_fallback
};

std::optional<ExprPtr> try_resolve_special_form(const FunctionCall& func, EvaluationContext& ctx) {
    if (is_special_form_function(func.head)) {
        return evaluate_special_form(func, ctx);
    }
    return std::nullopt;
}

std::optional<ExprPtr> try_resolve_registered_symbolic_function(const FunctionCall& func, EvaluationContext& ctx) {
    auto& registry = packs::PackRegistry::instance();
    if (registry.has_function(func.head)) {
        return registry.get_function(func.head)(func, ctx);
    }
    return std::nullopt;
}

std::optional<ExprPtr> try_resolve_builtin_function(const FunctionCall& func, EvaluationContext& ctx) {
    if (auto builtin_result = evaluate_builtin_function(func, ctx)) {
        return builtin_result;
    }
    return std::nullopt;
}

std::optional<ExprPtr> try_resolve_user_defined_function(const FunctionCall& func, EvaluationContext& ctx) {
    if (is_user_defined_function(func.head, ctx)) {
        return evaluate_user_defined_function(func, ctx);
    }
    return std::nullopt;
}

ExprPtr unresolved_symbolic_fallback(const FunctionCall& func) {
    return make_expr<FunctionCall>(func.head, func.args);
}

ExprPtr evaluate_general_function(const FunctionCall& func, EvaluationContext& ctx) {
    constexpr GeneralFunctionResolutionStage stages[] = {
        GeneralFunctionResolutionStage::special_form,
        GeneralFunctionResolutionStage::registered_symbolic_function,
        GeneralFunctionResolutionStage::builtin_function,
        GeneralFunctionResolutionStage::user_defined_function,
        GeneralFunctionResolutionStage::unresolved_symbolic_fallback
    };

    for (const auto stage : stages) {
        switch (stage) {
            case GeneralFunctionResolutionStage::special_form:
                if (auto resolved = try_resolve_special_form(func, ctx)) {
                    return *resolved;
                }
                break;
            case GeneralFunctionResolutionStage::registered_symbolic_function:
                if (auto resolved = try_resolve_registered_symbolic_function(func, ctx)) {
                    return *resolved;
                }
                break;
            case GeneralFunctionResolutionStage::builtin_function:
                if (auto resolved = try_resolve_builtin_function(func, ctx)) {
                    return *resolved;
                }
                break;
            case GeneralFunctionResolutionStage::user_defined_function:
                if (auto resolved = try_resolve_user_defined_function(func, ctx)) {
                    return *resolved;
                }
                break;
            case GeneralFunctionResolutionStage::unresolved_symbolic_fallback:
                return unresolved_symbolic_fallback(func);
        }
    }

    return unresolved_symbolic_fallback(func);
}

ExprPtr resolve_symbol_value(const Symbol& sym, EvaluationContext& ctx, std::unordered_set<std::string>& visited) {
    if (visited.count(sym.name)) {
        return make_expr<Symbol>(sym.name);
    }

    const ExprPtr* value = ctx.symbol_values.lookup(sym.name);
    if (value == nullptr) {
        return make_expr<Symbol>(sym.name);
    }

    visited.insert(sym.name);
    auto result = evaluate_impl(*value, ctx, visited);
    visited.erase(sym.name);
    return result;
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
            return resolve_symbol_value(sym, ctx, visited);
        },
        [&](const FunctionCall& func) -> ExprPtr {
            if (is_algebra_function(func.head)) {
                return evaluate_algebra_function(func, ctx);
            }

            if (is_structural_function(func.head)) {
                std::vector<ExprPtr> evaluated_elements;
                evaluated_elements.reserve(func.args.size());
                for (const auto& arg : func.args) {
                    evaluated_elements.push_back(evaluate(arg, ctx));
                }
                return make_expr<List>(evaluated_elements);
            }

            return evaluate_general_function(func, ctx);
        },
        [&](const FunctionDefinition& def) -> ExprPtr {
            return register_user_defined_function(def, ctx);
        },
        [&](const Assignment& assign) -> ExprPtr {
            ctx.symbol_values.set(assign.name, evaluate(assign.value, ctx));
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
        [](const ComplexInfinity&) -> ExprPtr {
            return make_expr<ComplexInfinity>();
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
