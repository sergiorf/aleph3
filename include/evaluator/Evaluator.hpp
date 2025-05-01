// evaluator/Evaluator.hpp
#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "util/Overloaded.hpp"

#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <functional>

namespace mathix {

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx);

inline double get_number_value(const ExprPtr& expr) {
    if (auto num = std::get_if<Number>(&(*expr))) {
        return num->value;
    } else {
        throw std::runtime_error("Expected a Number during evaluation, but got something else");
    }
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

    if (auto it = unary_functions.find(name); it != unary_functions.end()) {
        if (evaluated_args.size() != 1) {
            throw std::runtime_error(name + " expects 1 argument");
        }
        double arg = get_number_value(evaluated_args[0]);
        return make_expr<Number>(it->second(arg));
    }

    if (auto it = binary_functions.find(name); it != binary_functions.end()) {
        if (evaluated_args.size() != 2) {
            throw std::runtime_error(name + " expects exactly 2 arguments");
        }

        // Try numeric evaluation
        auto left = evaluated_args[0];
        auto right = evaluated_args[1];

        if (std::holds_alternative<Number>(*left) && std::holds_alternative<Number>(*right)) {
            double arg1 = get_number_value(left);
            double arg2 = get_number_value(right);
            return make_expr<Number>(it->second(arg1, arg2));
        }

        // ❗ If not fully numeric, return symbolic expression
        return make_expr<FunctionCall>(name, evaluated_args);
    }

    // Special case: unary minus (negation)
    if (name == "Negate" && evaluated_args.size() == 1) {
        return make_expr<Number>(-get_number_value(evaluated_args[0]));
    }

    // Check for user-defined functions
    if (auto user_it = ctx.user_functions.find(name); user_it != ctx.user_functions.end()) {
        const FunctionDefinition& def = user_it->second;

        if (def.params.size() != evaluated_args.size()) {
            throw std::runtime_error("Function " + name + " expects " +
                std::to_string(def.params.size()) + " arguments, got " +
                std::to_string(evaluated_args.size()));
        }

        // Create a new context to evaluate the function body
        EvaluationContext local_ctx = ctx; // Copy current context (captures global vars/functions)

        // Bind formal parameters to evaluated arguments
        for (size_t i = 0; i < def.params.size(); ++i) {
            if (std::holds_alternative<Number>(*evaluated_args[i])) {
                double val = get_number_value(evaluated_args[i]);
                local_ctx.variables[def.params[i]] = val;
            }
            else {
                throw std::runtime_error("Only numeric arguments supported for user-defined functions for now.");
            }
        }

        return evaluate(def.body, local_ctx); // Recursively evaluate the body with new bindings
    }

    // If no matching function is found, throw an exception
    throw std::runtime_error("Unknown function: " + name);
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
            return make_expr<Number>(it->second);
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
                    local_ctx.variables[def.params[i]] = get_number_value(arg_value);
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
