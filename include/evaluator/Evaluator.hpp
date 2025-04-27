// evaluator/Evaluator.hpp
#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "util/Overloaded.hpp"

#include <stdexcept>
#include <cmath>

namespace mathix {

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx);

inline ExprPtr evaluate_function(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.head == "Plus") {
        if (func.args.size() != 2) throw std::runtime_error("Plus expects 2 arguments");
        auto left = evaluate(func.args[0], ctx);
        auto right = evaluate(func.args[1], ctx);
        auto lnum = std::get<Number>(*left);
        auto rnum = std::get<Number>(*right);
        return make_expr<Number>(lnum.value + rnum.value);
    }
    else if (func.head == "Minus") {
        if (func.args.size() != 2) throw std::runtime_error("Minus expects 2 arguments");
        auto left = evaluate(func.args[0], ctx);
        auto right = evaluate(func.args[1], ctx);
        auto lnum = std::get<Number>(*left);
        auto rnum = std::get<Number>(*right);
        return make_expr<Number>(lnum.value - rnum.value);
    }
    else if (func.head == "Times") {
        if (func.args.size() != 2) throw std::runtime_error("Times expects 2 arguments");
        auto left = evaluate(func.args[0], ctx);
        auto right = evaluate(func.args[1], ctx);
        auto lnum = std::get<Number>(*left);
        auto rnum = std::get<Number>(*right);
        return make_expr<Number>(lnum.value * rnum.value);
    }
    else if (func.head == "Divide") {
        if (func.args.size() != 2) throw std::runtime_error("Divide expects 2 arguments");
        auto left = evaluate(func.args[0], ctx);
        auto right = evaluate(func.args[1], ctx);
        auto lnum = std::get<Number>(*left);
        auto rnum = std::get<Number>(*right);
        if (rnum.value == 0.0) throw std::runtime_error("Division by zero");
        return make_expr<Number>(lnum.value / rnum.value);
    }
    else {
        throw std::runtime_error("Unknown function: " + func.head);
    }
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
