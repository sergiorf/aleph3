// evaluator/Evaluator.hpp
#pragma once

#include "expr/Expr.hpp"
#include "util/Overloaded.hpp"

#include <stdexcept>
#include <cmath>

namespace mathix {

inline ExprPtr evaluate(const ExprPtr& expr);

inline ExprPtr evaluate_function(const FunctionCall& func) {
    if (func.head == "Plus") {
        if (func.args.size() != 2) throw std::runtime_error("Plus expects 2 arguments");
        auto left = evaluate(func.args[0]);
        auto right = evaluate(func.args[1]);
        auto lnum = std::get<Number>(*left);
        auto rnum = std::get<Number>(*right);
        return make_expr<Number>(lnum.value + rnum.value);
    }
    else if (func.head == "Minus") {
        if (func.args.size() != 2) throw std::runtime_error("Minus expects 2 arguments");
        auto left = evaluate(func.args[0]);
        auto right = evaluate(func.args[1]);
        auto lnum = std::get<Number>(*left);
        auto rnum = std::get<Number>(*right);
        return make_expr<Number>(lnum.value - rnum.value);
    }
    else if (func.head == "Times") {
        if (func.args.size() != 2) throw std::runtime_error("Times expects 2 arguments");
        auto left = evaluate(func.args[0]);
        auto right = evaluate(func.args[1]);
        auto lnum = std::get<Number>(*left);
        auto rnum = std::get<Number>(*right);
        return make_expr<Number>(lnum.value * rnum.value);
    }
    else if (func.head == "Divide") {
        if (func.args.size() != 2) throw std::runtime_error("Divide expects 2 arguments");
        auto left = evaluate(func.args[0]);
        auto right = evaluate(func.args[1]);
        auto lnum = std::get<Number>(*left);
        auto rnum = std::get<Number>(*right);
        if (rnum.value == 0.0) throw std::runtime_error("Division by zero");
        return make_expr<Number>(lnum.value / rnum.value);
    }
    else {
        throw std::runtime_error("Unknown function: " + func.head);
    }
}

inline ExprPtr evaluate(const ExprPtr& expr) {
    return std::visit(overloaded {
        [](const Number& num) -> ExprPtr {
            return make_expr<Number>(num.value);
        },
        [](const Symbol& sym) -> ExprPtr {
            return make_expr<Symbol>(sym.name); // later we can lookup variables here
        },
        [](const FunctionCall& func) -> ExprPtr {
            return evaluate_function(func);
        }
    }, *expr);
}

} // namespace mathix
