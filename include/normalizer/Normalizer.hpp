#pragma once
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"
#include "util/Overloaded.hpp"
#include <memory>
#include <algorithm>
#include <vector>

namespace aleph3 {

inline ExprPtr normalize_expr(const ExprPtr& expr) {
    return std::visit(overloaded{
        [](const Number& num) -> ExprPtr {
            return make_expr<Number>(num.value);
        },
        [](const Complex& c) -> ExprPtr {
            return make_expr<Complex>(c.real, c.imag);
        },
        [](const Rational& r) -> ExprPtr {
            return make_expr<Rational>(r.numerator, r.denominator);
        },
        [](const Boolean& boolean) -> ExprPtr {
            return make_expr<Boolean>(boolean.value);
        },
        [](const String& str) -> ExprPtr {
            return make_expr<String>(str.value);
        },
        [](const Infinity&) -> ExprPtr {
            return make_expr<Infinity>();
        },
        [](const Indeterminate&) -> ExprPtr {
            return make_expr<Indeterminate>();
        },
        [](const Symbol& sym) -> ExprPtr {
            return make_expr<Symbol>(sym.name);
        },
        [](const FunctionCall& f) -> ExprPtr {
            if (f.head == "Minus" && f.args.size() == 2) {
                // Normalize Minus(a, b) -> Plus(a, Times(-1, b))
                auto a = normalize_expr(f.args[0]);
                auto b = normalize_expr(f.args[1]);
                return normalize_expr(make_fcall("Plus", {a, make_fcall("Times", {make_expr<Number>(-1), b})}));
            }
            if (f.head == "Plus" && f.args.size() == 2) {
                const auto& a = f.args[0];
                const auto& b = f.args[1];
                if (std::holds_alternative<Number>(*a)) {
                    // Times(Number, I) and Times(Number, -1, I)
                    if (auto* times = std::get_if<FunctionCall>(b.get())) {
                        if (times->head == "Times") {
                            // Case: Times(Number, I)
                            if (times->args.size() == 2 &&
                                std::holds_alternative<Number>(*times->args[0])) {
                                // Times(Number, Symbol("I"))
                                if (std::holds_alternative<Symbol>(*times->args[1]) &&
                                    std::get<Symbol>(*times->args[1]).name == "I") {
                                    return make_expr<Complex>(
                                        std::get<Number>(*a).value,
                                        std::get<Number>(*times->args[0]).value
                                    );
                                }
                                // Times(Number, Complex(0,1))
                                if (std::holds_alternative<Complex>(*times->args[1])) {
                                    const auto& c = std::get<Complex>(*times->args[1]);
                                    if (c.real == 0.0 && c.imag == 1.0) {
                                        return make_expr<Complex>(
                                            std::get<Number>(*a).value,
                                            std::get<Number>(*times->args[0]).value
                                        );
                                    }
                                }
                            }
                            // Case: Times(Number, Number(-1), I)
                            if (times->args.size() == 3 &&
                                std::holds_alternative<Number>(*times->args[0]) &&
                                std::holds_alternative<Number>(*times->args[1]) &&
                                std::holds_alternative<Symbol>(*times->args[2]) &&
                                std::get<Symbol>(*times->args[2]).name == "I") {
                                double coeff = std::get<Number>(*times->args[0]).value *
                                            std::get<Number>(*times->args[1]).value;
                                return make_expr<Complex>(
                                    std::get<Number>(*a).value,
                                    coeff
                                );
                            }
                            // Case: Times(Number, Number(-1), Complex(0,1))
                            if (times->args.size() == 3 &&
                                std::holds_alternative<Number>(*times->args[0]) &&
                                std::holds_alternative<Number>(*times->args[1]) &&
                                std::holds_alternative<Complex>(*times->args[2])) {
                                const auto& c = std::get<Complex>(*times->args[2]);
                                if (c.real == 0.0 && c.imag == 1.0) {
                                    double coeff = std::get<Number>(*times->args[0]).value *
                                                std::get<Number>(*times->args[1]).value;
                                    return make_expr<Complex>(
                                        std::get<Number>(*a).value,
                                        coeff
                                    );
                                }
                            }
                            // Case: Times(Number, Number(-1), I)
                            if (times->args.size() == 3 &&
                                std::holds_alternative<Number>(*times->args[0]) &&
                                std::holds_alternative<Number>(*times->args[1]) &&
                                std::holds_alternative<Symbol>(*times->args[2]) &&
                                std::get<Symbol>(*times->args[2]).name == "I") {
                                double coeff = std::get<Number>(*times->args[0]).value *
                                            std::get<Number>(*times->args[1]).value;
                                return make_expr<Complex>(
                                    std::get<Number>(*a).value,
                                    coeff
                                );
                            }
                        }
                    }
                }
            }
            // Normalize Negate(x)
            if (f.head == "Negate" && f.args.size() == 1) {
                auto arg = normalize_expr(f.args[0]);
                // If arg is Number, just negate it
                if (auto num = std::get_if<Number>(arg.get())) {
                    return make_expr<Number>(-num->value);
                }
                // If arg is already Times(-1, ...), flatten
                if (auto inner = std::get_if<FunctionCall>(arg.get())) {
                    if (inner->head == "Times" && !inner->args.empty()) {
                        if (auto n = std::get_if<Number>(inner->args[0].get()); n && n->value == -1) {
                            // Already normalized
                            return arg;
                        }
                    }
                }
                // Otherwise, return Times(-1, arg)
                return make_fcall("Times", { make_expr<Number>(-1), arg });
            }
            // Normalize Times
            if (f.head == "Times") {
                std::vector<ExprPtr> norm_args;
                for (const auto& arg : f.args) {
                    norm_args.push_back(normalize_expr(arg));
                }
                return make_fcall("Times", norm_args);
            }
            // Normalize Divide
            if (f.head == "Divide" && f.args.size() == 2) {
                return make_fcall("Divide", { normalize_expr(f.args[0]), normalize_expr(f.args[1]) });
            }
            // Normalize Power
            if (f.head == "Power" && f.args.size() == 2) {
                return make_fcall("Power", { normalize_expr(f.args[0]), normalize_expr(f.args[1]) });
            }
            // Default: normalize all arguments
            std::vector<ExprPtr> norm_args;
            for (const auto& arg : f.args) {
                norm_args.push_back(normalize_expr(arg));
            }
            return make_fcall(f.head, norm_args);
        },
        [](const List& list) -> ExprPtr {
            std::vector<ExprPtr> norm_elems;
            for (const auto& elem : list.elements) {
                norm_elems.push_back(normalize_expr(elem));
            }
            return std::make_shared<Expr>(List{norm_elems});
        },
        [](const FunctionDefinition& def) -> ExprPtr {
            return make_expr<FunctionDefinition>(def.name, def.params, def.body, def.delayed);
        },
        [](const Assignment& assign) -> ExprPtr {
            return make_expr<Assignment>(assign.name, assign.value);
        },
        [](const Rule& rule) -> ExprPtr {
            return make_expr<Rule>(normalize_expr(rule.lhs), normalize_expr(rule.rhs));
        }
        }, *expr);
}

}