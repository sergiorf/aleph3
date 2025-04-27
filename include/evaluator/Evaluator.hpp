// evaluator/Evaluator.hpp
#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "util/Overloaded.hpp"

#include <stdexcept>
#include <cmath>

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
    std::vector<ExprPtr> evaluated_args;
    for (const auto& arg : func.args) {
        evaluated_args.push_back(evaluate(arg, ctx));
    }

    const std::string& name = func.head;

    // Handle known built-in functions
    if (name == "Plus") {
        double sum = 0.0;
        for (const auto& arg : evaluated_args) {
            sum += get_number_value(arg);
        }
        return make_expr<Number>(sum);
    }
    else if (name == "Minus") {
        if (evaluated_args.size() == 1) {
            return make_expr<Number>(-get_number_value(evaluated_args[0]));
        } else if (evaluated_args.size() == 2) {
            return make_expr<Number>(get_number_value(evaluated_args[0]) - get_number_value(evaluated_args[1]));
        }
    }
    else if (name == "Times") {
        double product = 1.0;
        for (const auto& arg : evaluated_args) {
            product *= get_number_value(arg);
        }
        return make_expr<Number>(product);
    }
    else if (name == "Divide") {
        if (evaluated_args.size() != 2) {
            throw std::runtime_error("Divide expects exactly 2 arguments");
        }
        return make_expr<Number>(get_number_value(evaluated_args[0]) / get_number_value(evaluated_args[1]));
    }
    else if (name == "sin") {
        if (evaluated_args.size() != 1) {
            throw std::runtime_error("sin expects 1 argument");
        }
        return make_expr<Number>(std::sin(get_number_value(evaluated_args[0])));
    }
    else if (name == "cos") {
        if (evaluated_args.size() != 1) {
            throw std::runtime_error("cos expects 1 argument");
        }
        return make_expr<Number>(std::cos(get_number_value(evaluated_args[0])));
    }
    else if (name == "sqrt") {
        if (evaluated_args.size() != 1) {
            throw std::runtime_error("sqrt expects 1 argument");
        }
        return make_expr<Number>(std::sqrt(get_number_value(evaluated_args[0])));
    }
    else if (name == "log") {
        if (evaluated_args.size() != 1) {
            throw std::runtime_error("log expects 1 argument");
        }
        return make_expr<Number>(std::log(get_number_value(evaluated_args[0])));
    }
    else if (name == "exp") {
        if (evaluated_args.size() != 1) {
            throw std::runtime_error("exp expects 1 argument");
        }
        return make_expr<Number>(std::exp(get_number_value(evaluated_args[0])));
    }

    // If unknown function, keep it unevaluated
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
