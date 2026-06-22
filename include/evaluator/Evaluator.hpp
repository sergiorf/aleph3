/*
 * Aleph3 Evaluator
 * ----------------
 * This header defines the core evaluation logic for Aleph3, a modern C++20-based computer algebra system.
 * The evaluator traverses and computes the value of Expr trees, handling symbolic and numeric computation,
 * function application, user-defined functions, and built-in mathematical operations.
 *
 * Features:
 * - Recursive evaluation of expressions
 * - Support for numbers, rationals, booleans, lists, and user-defined functions
 * - Built-in function and operator handling (arithmetic, comparison, etc.)
 * - Extensible via function registry and simplification rules
 *
 * See README.md for project overview.
 */
#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluatorAlgebra.hpp"
#include "evaluator/EvaluatorBuiltins.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/EvaluatorSpecialForms.hpp"
#include "evaluator/FunctionRegistry.hpp"
#include "evaluator/SimplificationRules.hpp"
#include "normalizer/Normalizer.hpp"
#include "util/Overloaded.hpp"
#include "util/Logging.hpp"
#include "expr/ExprUtils.hpp"
#include "transforms/Transforms.hpp"
#include "ExtraMath.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace aleph3 {

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx);

inline std::string expr_to_key(const ExprPtr& expr) {
    auto norm = normalize_expr(expr);
    std::string s = to_string_raw(norm);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

inline ExprPtr evaluate_function(const FunctionCall& func, EvaluationContext& ctx) {
    const std::string& name = func.head;

    // 1. Try FunctionRegistry (for extensible built-ins)
    auto& registry = FunctionRegistry::instance();
    if (registry.has_function(name)) {
        auto handler = registry.get_function(name);
        return handler(func, ctx);
    }

    // 2. Special forms
    if (is_special_form_function(name)) {
        return evaluate_special_form(func, ctx);
    }
    // 3. Built-ins
    if (auto builtin_result = evaluate_builtin_function(func, ctx)) {
        return builtin_result;
    }

    // 4. User-defined functions
    auto user_it = ctx.user_functions.find(name);
    if (user_it != ctx.user_functions.end()) {
        const FunctionDefinition& def = user_it->second;
        size_t param_count = def.params.size();
        size_t arg_count = func.args.size();
        std::vector<ExprPtr> final_args;
        for (size_t i = 0; i < param_count; ++i) {
            if (i < arg_count) {
                final_args.push_back(func.args[i]);
            }
            else if (def.params[i].default_value != nullptr) {
                final_args.push_back(def.params[i].default_value);
            }
            else {
                throw std::runtime_error("Function " + name + " expects at least " +
                    std::to_string(i + 1) + " arguments, got " +
                    std::to_string(arg_count));
            }
        }
        if (arg_count > param_count) {
            throw std::runtime_error("Function " + name + " expects at most " +
                std::to_string(param_count) + " arguments, got " +
                std::to_string(arg_count));
        }
        EvaluationContext local_ctx = ctx;
        for (size_t i = 0; i < param_count; ++i) {
            local_ctx.variables[def.params[i].name] = final_args[i];
        }
        return evaluate(def.body, local_ctx);
    }

    // 9. Fallback: return unevaluated
    std::vector<ExprPtr> unevaluated_args;
    for (const auto& arg : func.args) {
        unevaluated_args.push_back(arg);
    }
    return make_expr<FunctionCall>(name, unevaluated_args);
}

inline ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx, std::unordered_set<std::string>& visited) {
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
                // Detected recursion, return symbol unevaluated
                return make_expr<Symbol>(sym.name);
            }
            auto it = ctx.variables.find(sym.name);
            if (it == ctx.variables.end()) {
                return make_expr<Symbol>(sym.name);
            }
            visited.insert(sym.name);
            auto result = evaluate(it->second, ctx, visited);
            visited.erase(sym.name);
            return result;
        },
        [&ctx](const FunctionCall& func) -> ExprPtr {
            if (is_symbolic_algebra_function(func.head)) {
                return evaluate_algebra_function(func, ctx);
            }
            // Special case: List
            if (func.head == "List") {
                std::vector<ExprPtr> evaluated_elements;
                for (const auto& arg : func.args) {
                    evaluated_elements.push_back(evaluate(arg, ctx));
                }
                return std::make_shared<Expr>(List{evaluated_elements});
            }

            // Check for user-defined functions
            auto user_it = ctx.user_functions.find(func.head);
            if (user_it != ctx.user_functions.end()) {
                const FunctionDefinition& def = user_it->second;
                size_t param_count = def.params.size();
                size_t arg_count = func.args.size();

                // Prepare argument list, filling in defaults if needed
                std::vector<ExprPtr> final_args;
                for (size_t i = 0; i < param_count; ++i) {
                    if (i < arg_count) {
                        final_args.push_back(func.args[i]);
                    }
                    else if (def.params[i].default_value != nullptr) {
                        final_args.push_back(def.params[i].default_value);
                    }
                    else {
                        throw std::runtime_error("Function " + func.head + " expects at least " +
                            std::to_string(i + 1) + " arguments, got " +
                            std::to_string(arg_count));
                    }
                }

                // Too many arguments
                if (arg_count > param_count) {
                    throw std::runtime_error("Function " + func.head + " expects at most " +
                        std::to_string(param_count) + " arguments, got " +
                        std::to_string(arg_count));
                }

                // Create a new context for the function evaluation
                EvaluationContext local_ctx = ctx;

                // Bind formal parameters to actual arguments
                for (size_t i = 0; i < param_count; ++i) {
                    auto arg_value = evaluate(final_args[i], ctx);
                    local_ctx.variables[def.params[i].name] = arg_value;
                }

                // Evaluate the function body in the new context
                return evaluate(def.body, local_ctx);
            }
            // If not a user-defined function, evaluate as a built-in function
            return evaluate_function(func, ctx);
        },
        [&ctx](const FunctionDefinition& def) -> ExprPtr {

            // Store the function definition in the context
            if (def.delayed) {
                // Delayed assignment: store the unevaluated body
                ctx.user_functions[def.name] = def;
            }
            else {
                // Immediate assignment: evaluate the body immediately
                EvaluationContext local_ctx = ctx;
                for (const auto& param : def.params) {
                    local_ctx.variables[param.name] = make_expr<Symbol>(param.name); // Bind parameters symbolically
                }
                auto evaluated_body = evaluate(def.body, local_ctx);
                ctx.user_functions[def.name] = FunctionDefinition(def.name, def.params, evaluated_body, false);
                return evaluated_body;
            }
            // Return the full function definition as feedback
            return make_expr<FunctionDefinition>(def.name, def.params, def.body, def.delayed);
        },
        [&ctx](const Assignment& assign) -> ExprPtr {
            // Evaluate the assigned value and store it in the context
            ctx.variables[assign.name] = evaluate(assign.value, ctx);

            // Return the variable name as feedback
            return make_expr<Symbol>(assign.name);
        },
        [&](const Rule& rule) -> ExprPtr {
            auto lhs = evaluate(rule.lhs, ctx, visited);
            auto rhs = evaluate(rule.rhs, ctx, visited);
            return make_expr<Rule>(lhs, rhs);
        },
        [](const List& list) -> ExprPtr {
            // Lists are already evaluated, just return as-is
            return std::make_shared<Expr>(list);
        },
        [](const Infinity&) -> ExprPtr {
            return make_expr<Infinity>();
        },
        [](const Indeterminate&) -> ExprPtr {
            return make_expr<Indeterminate>();
        },
        }, 
        *expr);
        ALEPH3_LOG("evaluate: result = " << to_string_raw(result));
        return result;
}

inline ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx) {
    ExprPtr norm = normalize_expr(expr);
    std::unordered_set<std::string> visited;
    return evaluate(norm, ctx, visited);
}

}
