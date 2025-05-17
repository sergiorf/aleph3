#pragma once

#include "expr/Expr.hpp"

namespace mathix {

    inline double get_number_value(const ExprPtr& expr) {
        if (auto num = std::get_if<Number>(&(*expr))) {
            return num->value;
        }
        else {
            throw std::runtime_error("Expected a Number during evaluation, but got something else");
        }
    }

    inline bool get_boolean_value(const ExprPtr& expr) {
        if (std::holds_alternative<Boolean>(*expr)) {
            return std::get<Boolean>(*expr).value;
        }
        throw std::runtime_error("Expression is not a Boolean");
    }

    inline bool is_zero(const ExprPtr& e) {
        return std::holds_alternative<Number>(*e) && get_number_value(e) == 0.0;
    }

    inline bool is_one(const ExprPtr& e) {
        return std::holds_alternative<Number>(*e) && get_number_value(e) == 1.0;
    }

    inline bool is_function(const ExprPtr& e, const std::string& name) {
        auto f = std::get_if<FunctionCall>(e.get());
        return f && f->head == name;
    }

    inline ExprPtr make_number(double value) {
        return make_expr<Number>(value);
    }

    inline ExprPtr make_plus(const ExprPtr& a, const ExprPtr& b) {
        return make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{a, b});
    }

    inline ExprPtr make_plus(std::initializer_list<ExprPtr> args) {
        return make_expr<FunctionCall>("Plus", std::vector<ExprPtr>(args));
    }

    inline ExprPtr make_plus(const std::vector<ExprPtr>& args) {
        return make_expr<FunctionCall>("Plus", args);
    }

    inline ExprPtr make_times(const std::vector<ExprPtr>& args) {
        std::vector<ExprPtr> flattened;
        double coefficient = 1.0;

        for (const auto& arg : args) {
            if (is_one(arg)) continue;

            if (auto num = std::get_if<Number>(arg.get())) {
                coefficient *= num->value;
            }
            else if (is_function(arg, "Times")) {
                const auto& inner = std::get<FunctionCall>(*arg);
                for (const auto& inner_arg : inner.args) {
                    flattened.push_back(inner_arg);
                }
            }
            else {
                flattened.push_back(arg);
            }
        }

        if (coefficient != 1.0) {
            flattened.insert(flattened.begin(), make_number(coefficient));
        }

        if (flattened.empty()) {
            return make_number(coefficient); // possibly 1 or 0
        }

        if (flattened.size() == 1) {
            return flattened[0]; // no need for Times head
        }

        return make_expr<FunctionCall>("Times", flattened);
    }

    inline ExprPtr make_times(const ExprPtr& a, const ExprPtr& b) {
        return make_times(std::vector<ExprPtr>{ a, b });
    }

    inline ExprPtr make_times(std::initializer_list<ExprPtr> args) {
        return make_times(std::vector<ExprPtr>(args));
    }

    inline ExprPtr make_pow(const ExprPtr& base, int exponent) {
        return make_expr<FunctionCall>("Pow", std::vector<ExprPtr>{
            base, make_expr<Number>((double)exponent)
        });
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

    inline ExprPtr make_fcall(std::string name, const std::vector<ExprPtr>& args) {
        return make_expr<FunctionCall>(name, args);
    }

    inline ExprPtr make_fcall(std::string name, std::initializer_list<ExprPtr> args) {
        return make_expr<FunctionCall>(name, std::vector<ExprPtr>(args));
    }

    inline ExprPtr make_fdef(std::string name, std::initializer_list<std::string> args, const ExprPtr& body, bool delayed) {
        return make_expr<FunctionDefinition>(name, std::vector<std::string>(args), body, delayed);
    }

} // namespace mathix
