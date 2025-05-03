#pragma once

#include "expr/Expr.hpp"

namespace mathix {

    inline bool is_function(const ExprPtr& e, const std::string& name) {
        auto f = std::get_if<FunctionCall>(e.get());
        return f && f->head == name;
    }

    inline ExprPtr make_plus(const ExprPtr& a, const ExprPtr& b) {
        return make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{a, b});
    }

    inline ExprPtr make_plus(std::initializer_list<ExprPtr> args) {
        return make_expr<FunctionCall>("Plus", std::vector<ExprPtr>(args));
    }

    inline ExprPtr make_times(const ExprPtr& a, const ExprPtr& b) {
        return make_expr<FunctionCall>("Times", std::vector<ExprPtr>{a, b});
    }

    inline ExprPtr make_times(std::initializer_list<ExprPtr> args) {
        return make_expr<FunctionCall>("Times", std::vector<ExprPtr>(args));
    }

    inline ExprPtr make_pow(const ExprPtr& base, int exponent) {
        return make_expr<FunctionCall>("Pow", std::vector<ExprPtr>{
            base, make_expr<Number>((double)exponent)
        });
    }

    inline ExprPtr make_number(double value) {
        return make_expr<Number>(value);
    }

    inline ExprPtr make_number(int value) {
        return make_expr<Number>(static_cast<double>(value));
    }

    inline int get_integer_value(const ExprPtr& e) {
        if (auto n = std::get_if<Number>(e.get())) {
            return static_cast<int>(n->value);
        }
        throw std::runtime_error("Expected integer number");
    }


} // namespace mathix
