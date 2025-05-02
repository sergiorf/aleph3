// evaluator/Evaluator.hpp
#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "util/Overloaded.hpp"

#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <functional>
#include <algorithm>

namespace mathix {

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx);

inline double get_number_value(const ExprPtr& expr) {
    if (auto num = std::get_if<Number>(&(*expr))) {
        return num->value;
    } else {
        throw std::runtime_error("Expected a Number during evaluation, but got something else");
    }
}

inline bool is_zero(const ExprPtr& e) {
    return std::holds_alternative<Number>(*e) && get_number_value(e) == 0.0;
}

inline bool is_one(const ExprPtr& e) {
    return std::holds_alternative<Number>(*e) && get_number_value(e) == 1.0;
}

inline ExprPtr evaluate_function(const FunctionCall& func, EvaluationContext& ctx) {
    static const std::unordered_map<std::string, std::function<double(double)>> unary_functions = {
        {"sin", [](double x) { return std::sin(x); }},
        {"cos", [](double x) { return std::cos(x); }},
        {"tan", [](double x) { return std::tan(x); }},
        {"abs", [](double x) { return std::fabs(x); }},
        {"sqrt", [](double x) { return std::sqrt(x); }},
        {"log", [](double x) { return std::log(x); }},
        {"exp", [](double x) { return std::exp(x); }},
        {"floor", [](double x) { return std::floor(x); }},
        {"ceil", [](double x) { return std::ceil(x); }},
        {"round", [](double x) { return std::round(x); }}
    };

    static const std::unordered_map<std::string, std::function<double(double, double)>> binary_functions = {
        {"Plus", [](double a, double b) { return a + b; }},
        {"Minus", [](double a, double b) { return a - b; }},
        {"Times", [](double a, double b) { return a * b; }},
        {"Divide", [](double a, double b) { return a / b; }},
        {"Pow", [](double a, double b) { return std::pow(a, b); }}
    };

    std::vector<ExprPtr> evaluated_args;
    for (const auto& arg : func.args) {
        evaluated_args.push_back(evaluate(arg, ctx));
    }

    const std::string& name = func.head;

    // === Unary Built-in ===
    if (auto it = unary_functions.find(name); it != unary_functions.end()) {
        if (evaluated_args.size() != 1) {
            throw std::runtime_error(name + " expects 1 argument");
        }
        double arg = get_number_value(evaluated_args[0]);
        return make_expr<Number>(it->second(arg));
    }

    // === Binary Built-in with Simplification ===
    if (name == "Plus" || name == "Times") {
        bool all_numbers = std::all_of(evaluated_args.begin(), evaluated_args.end(),
            [](const ExprPtr& e) { return std::holds_alternative<Number>(*e); });

        if (all_numbers) {
            double result = (name == "Plus") ? 0 : 1;
            for (auto& arg : evaluated_args) {
                double val = get_number_value(arg);
                result = (name == "Plus") ? result + val : result * val;
            }
            return make_expr<Number>(result);
        }

        // Apply trivial simplification rules
        std::vector<ExprPtr> simplified;
        for (const auto& arg : evaluated_args) {
            if (name == "Plus" && !is_zero(arg)) simplified.push_back(arg);
            else if (name == "Times") {
                if (is_zero(arg)) return make_expr<Number>(0); // 0 * x = 0
                if (!is_one(arg)) simplified.push_back(arg);
            }
        }

        if (simplified.empty()) return make_expr<Number>((name == "Plus") ? 0 : 1);
        if (simplified.size() == 1) return simplified[0];
        return make_expr<FunctionCall>(name, simplified);
    }

    if (name == "Minus" || name == "Divide" || name == "Pow") {
        if (evaluated_args.size() != 2) {
            throw std::runtime_error(name + " expects 2 arguments");
        }

        auto left = evaluated_args[0];
        auto right = evaluated_args[1];

        if (std::holds_alternative<Number>(*left) && std::holds_alternative<Number>(*right)) {
            double a = get_number_value(left);
            double b = get_number_value(right);
            return make_expr<Number>(binary_functions.at(name)(a, b));
        }

        // Trivial simplification for exponentiation
        if (name == "Pow") {
            if (is_zero(right)) return make_expr<Number>(1);
            if (is_one(right)) return left;
        }

        // Return symbolic form if not fully numeric
        return make_expr<FunctionCall>(name, evaluated_args);
    }

    // Special case: unary negation
    if (name == "Negate" && evaluated_args.size() == 1) {
        if (std::holds_alternative<Number>(*evaluated_args[0])) {
            return make_expr<Number>(-get_number_value(evaluated_args[0]));
        }
        return make_expr<FunctionCall>("Times", std::vector({ make_expr<Number>(-1), evaluated_args[0] }));
    }

    // === User-defined Function ===
    if (auto user_it = ctx.user_functions.find(name); user_it != ctx.user_functions.end()) {
        const FunctionDefinition& def = user_it->second;

        if (def.params.size() != evaluated_args.size()) {
            throw std::runtime_error("Function " + name + " expects " +
                std::to_string(def.params.size()) + " arguments, got " +
                std::to_string(evaluated_args.size()));
        }

        EvaluationContext local_ctx = ctx;
        for (size_t i = 0; i < def.params.size(); ++i) {
            local_ctx.variables[def.params[i]] = func.args[i]; // keep unevaluated for symbolic binding
        }

        return evaluate(def.body, local_ctx);
    }

    // Unknown function: return unevaluated for symbolic preservation
    return make_expr<FunctionCall>(name, evaluated_args);
}

inline ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx) {
    return std::visit(overloaded{
        [](const Number& num) -> ExprPtr {
            return make_expr<Number>(num.value);
        },
        [&ctx](const Symbol& sym) -> ExprPtr {
            auto it = ctx.variables.find(sym.name);
            if (it == ctx.variables.end()) {
                return make_expr<Symbol>(sym.name);
            }
            return evaluate(it->second, ctx); // recursively evaluate bound expression
        },
        [&ctx](const FunctionCall& func) -> ExprPtr {
            // Check for user-defined functions
            auto user_it = ctx.user_functions.find(func.head);
            if (user_it != ctx.user_functions.end()) {
                const FunctionDefinition& def = user_it->second;

                if (def.params.size() != func.args.size()) {
                    throw std::runtime_error("Function " + func.head + " expects " +
                        std::to_string(def.params.size()) + " arguments, got " +
                        std::to_string(func.args.size()));
                }

                // Create a new context for the function evaluation
                EvaluationContext local_ctx = ctx;

                // Bind formal parameters to actual arguments
                for (size_t i = 0; i < def.params.size(); ++i) {
                    auto arg_value = evaluate(func.args[i], ctx);
                    local_ctx.variables[def.params[i]] = arg_value;
                }

                // Evaluate the function body in the new context
                return evaluate(def.body, local_ctx);
            }

            // If not a user-defined function, evaluate as a built-in function
            return evaluate_function(func, ctx);
        },
        [&ctx](const FunctionDefinition& def) -> ExprPtr {
            // Store the function definition in the context
            ctx.user_functions[def.name] = def;

            // Return the full function definition as feedback
            return make_expr<FunctionDefinition>(def.name, def.params, def.body);
        }
        }, *expr);
}

} // namespace mathix
