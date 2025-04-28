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
        double arg1 = get_number_value(evaluated_args[0]);
        double arg2 = get_number_value(evaluated_args[1]);
        return make_expr<Number>(it->second(arg1, arg2));
    }

    // Special case: unary minus (negation)
    if (name == "Negate" && evaluated_args.size() == 1) {
        return make_expr<Number>(-get_number_value(evaluated_args[0]));
    }

    // Unknown function: keep unevaluated
    return make_expr<FunctionCall>(func.head, evaluated_args);
}

inline ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx) {
    return std::visit(overloaded{
        [](const Number& num) -> ExprPtr {
            return make_expr<Number>(num.value);
        },
        [&ctx](const Symbol& sym) -> ExprPtr {
            auto it = ctx.variables.find(sym.name);
            if (it == ctx.variables.end()) {
                throw std::runtime_error("Unknown variable: " + sym.name);
            }
            return make_expr<Number>(it->second);
        },
        [&ctx](const FunctionCall& func) -> ExprPtr {
            return evaluate_function(func, ctx);
        }
        }, *expr);
}

} // namespace mathix
