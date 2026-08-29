#pragma once
#include "expr/Expr.hpp"
#include "expr/ExprStructural.hpp"
#include "expr/ExprUtils.hpp"
#include "evaluator/EvaluatorSemantics.hpp"
#include "util/Overloaded.hpp"
#include <memory>
#include <algorithm>
#include <map>
#include <numeric>
#include <vector>

namespace aleph3 {

inline ExprPtr normalize_expr(const ExprPtr& expr);

namespace detail {

enum class NormalizedSortClass {
    constant,
    algebraic,
    opaque
};

inline bool is_numeric_constant(const ExprPtr& expr) {
    return std::holds_alternative<Number>(*expr) || std::holds_alternative<Rational>(*expr);
}

inline NormalizedSortClass normalized_sort_class(const ExprPtr& expr) {
    if (is_numeric_constant(expr)) {
        return NormalizedSortClass::constant;
    }
    if (std::holds_alternative<Symbol>(*expr)) {
        return NormalizedSortClass::algebraic;
    }
    if (const auto* call = std::get_if<FunctionCall>(expr.get())) {
        if (call->head == "Power" && call->args.size() == 2 &&
            std::holds_alternative<Symbol>(*call->args[0]) &&
            std::holds_alternative<Number>(*call->args[1])) {
            return NormalizedSortClass::algebraic;
        }
        if (call->head == "Times") {
            bool saw_symbolic_factor = false;
            for (const auto& arg : call->args) {
                if (is_numeric_constant(arg)) {
                    continue;
                }
                if (normalized_sort_class(arg) != NormalizedSortClass::algebraic) {
                    return NormalizedSortClass::opaque;
                }
                saw_symbolic_factor = true;
            }
            if (saw_symbolic_factor) {
                return NormalizedSortClass::algebraic;
            }
        }
    }
    return NormalizedSortClass::opaque;
}

inline int normalized_term_degree(const ExprPtr& expr) {
    if (std::holds_alternative<Number>(*expr) || std::holds_alternative<Rational>(*expr)) {
        return 0;
    }
    if (std::holds_alternative<Symbol>(*expr)) {
        return 1;
    }
    if (const auto* call = std::get_if<FunctionCall>(expr.get())) {
        if (call->head == "Power" && call->args.size() == 2 &&
            std::holds_alternative<Symbol>(*call->args[0]) &&
            std::holds_alternative<Number>(*call->args[1])) {
            return static_cast<int>(std::get<Number>(*call->args[1]).value);
        }
        if (call->head == "Times") {
            int degree = 0;
            bool saw_symbolic_factor = false;
            for (const auto& arg : call->args) {
                const int factor_degree = normalized_term_degree(arg);
                if (factor_degree > 0) {
                    degree += factor_degree;
                    saw_symbolic_factor = true;
                }
            }
            if (saw_symbolic_factor) {
                return degree;
            }
        }
    }
    return -1;
}

inline ExprPtr canonical_symbolic_basis(const ExprPtr& expr) {
    if (std::holds_alternative<Symbol>(*expr)) {
        return expr;
    }
    if (const auto* call = std::get_if<FunctionCall>(expr.get())) {
        if (call->head == "Power" && call->args.size() == 2) {
            return expr;
        }
        if (call->head == "Times") {
            std::vector<ExprPtr> non_numeric_factors;
            for (const auto& arg : call->args) {
                if (!is_numeric_constant(arg)) {
                    non_numeric_factors.push_back(arg);
                }
            }
            if (!non_numeric_factors.empty()) {
                if (non_numeric_factors.size() == 1) {
                    return non_numeric_factors.front();
                }
                return make_fcall("Times", non_numeric_factors);
            }
        }
    }
    return expr;
}

inline bool canonical_numeric_constant_less(const ExprPtr& left, const ExprPtr& right) {
    const bool left_rational = std::holds_alternative<Rational>(*left);
    const bool right_rational = std::holds_alternative<Rational>(*right);
    if (left_rational != right_rational) {
        return left_rational;
    }

    const auto numeric_value = [](const ExprPtr& expr) {
        if (const auto* number = std::get_if<Number>(expr.get())) {
            return number->value;
        }
        const auto& rational = std::get<Rational>(*expr);
        return static_cast<double>(rational.numerator) /
               static_cast<double>(rational.denominator);
    };
    const double left_value = numeric_value(left);
    const double right_value = numeric_value(right);
    if (left_value < 0.0 && right_value < 0.0 && left_value != right_value) {
        return left_value > right_value;
    }
    return ExprStructuralLess{}(left, right);
}

inline bool canonical_symbolic_less(const ExprPtr& left, const ExprPtr& right);

inline bool canonical_list_less(
    const std::vector<ExprPtr>& left,
    const std::vector<ExprPtr>& right) {
    const auto count = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < count; ++index) {
        if (canonical_symbolic_less(left[index], right[index])) {
            return true;
        }
        if (canonical_symbolic_less(right[index], left[index])) {
            return false;
        }
    }
    return left.size() < right.size();
}

inline bool canonical_power_like_less(const FunctionCall& left, const ExprPtr& right) {
    if (left.head != "Power" || left.args.size() != 2) {
        return false;
    }

    const auto right_call = std::get_if<FunctionCall>(right.get());
    if (right_call != nullptr && right_call->head == "Power" && right_call->args.size() == 2) {
        if (canonical_symbolic_less(left.args[0], right_call->args[0])) {
            return true;
        }
        if (canonical_symbolic_less(right_call->args[0], left.args[0])) {
            return false;
        }
        return canonical_symbolic_less(left.args[1], right_call->args[1]);
    }

    if (canonical_symbolic_less(left.args[0], right)) {
        return true;
    }
    if (canonical_symbolic_less(right, left.args[0])) {
        return false;
    }
    return false;
}

inline bool canonical_symbolic_less(const ExprPtr& left, const ExprPtr& right) {
    const auto left_basis = canonical_symbolic_basis(left);
    const auto right_basis = canonical_symbolic_basis(right);
    if (is_numeric_constant(left_basis) && is_numeric_constant(right_basis)) {
        return canonical_numeric_constant_less(left_basis, right_basis);
    }

    const auto* left_basis_call = std::get_if<FunctionCall>(left_basis.get());
    if (left_basis_call != nullptr &&
        canonical_power_like_less(*left_basis_call, right_basis)) {
        return true;
    }
    const auto* right_basis_call = std::get_if<FunctionCall>(right_basis.get());
    if (right_basis_call != nullptr &&
        canonical_power_like_less(*right_basis_call, left_basis)) {
        return false;
    }
    if (left_basis_call != nullptr && right_basis_call != nullptr) {
        if (left_basis_call->head != right_basis_call->head) {
            return left_basis_call->head < right_basis_call->head;
        }
        if (canonical_list_less(left_basis_call->args, right_basis_call->args)) {
            return true;
        }
        if (canonical_list_less(right_basis_call->args, left_basis_call->args)) {
            return false;
        }
    }

    const ExprStructuralLess structural_less;
    if (structural_less(left_basis, right_basis)) {
        return true;
    }
    if (structural_less(right_basis, left_basis)) {
        return false;
    }
    if (left_basis != left && structural_equal(left_basis, right)) {
        return true;
    }
    if (right_basis != right && structural_equal(right_basis, left)) {
        return false;
    }
    return structural_less(left, right);
}

inline int canonical_plus_algebraic_shape_rank(const ExprPtr& expr) {
    if (std::holds_alternative<Symbol>(*expr)) {
        return 0;
    }
    if (const auto* call = std::get_if<FunctionCall>(expr.get())) {
        if (call->head == "Power" && call->args.size() == 2 &&
            std::holds_alternative<Symbol>(*call->args[0]) &&
            std::holds_alternative<Number>(*call->args[1])) {
            return 0;
        }
        if (call->head == "Times") {
            return 1;
        }
    }
    return 2;
}

inline bool has_non_unit_negative_numeric_coefficient(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(expr.get())) {
        return number->value < 0.0 && number->value != -1.0;
    }
    if (const auto* rational = std::get_if<Rational>(expr.get())) {
        return rational->numerator < 0 &&
               !(rational->numerator == -1 && rational->denominator == 1);
    }
    if (const auto* call = std::get_if<FunctionCall>(expr.get());
        call != nullptr && call->head == "Times" && !call->args.empty()) {
        return has_non_unit_negative_numeric_coefficient(call->args.front());
    }
    return false;
}

inline bool canonical_plus_term_less(const ExprPtr& left, const ExprPtr& right) {
    const auto left_class = normalized_sort_class(left);
    const auto right_class = normalized_sort_class(right);
    if (left_class != right_class) {
        if (left_class == NormalizedSortClass::constant) {
            return false;
        }
        if (right_class == NormalizedSortClass::constant) {
            return true;
        }
        if (left_class == NormalizedSortClass::algebraic) {
            return true;
        }
        if (right_class == NormalizedSortClass::algebraic) {
            return false;
        }
    }

    const int left_degree = normalized_term_degree(left);
    const int right_degree = normalized_term_degree(right);
    if (left_degree != right_degree) {
        return left_degree > right_degree;
    }

    if (left_class == NormalizedSortClass::algebraic &&
        right_class == NormalizedSortClass::algebraic) {
        const int left_shape_rank = canonical_plus_algebraic_shape_rank(left);
        const int right_shape_rank = canonical_plus_algebraic_shape_rank(right);
        if (left_shape_rank != right_shape_rank) {
            return left_shape_rank < right_shape_rank;
        }
    }

    const bool left_negative = has_non_unit_negative_numeric_coefficient(left);
    const bool right_negative = has_non_unit_negative_numeric_coefficient(right);
    if (left_negative != right_negative) {
        return left_negative;
    }

    return canonical_symbolic_less(left, right);
}

inline bool canonical_times_factor_less(const ExprPtr& left, const ExprPtr& right) {
    const auto left_class = normalized_sort_class(left);
    const auto right_class = normalized_sort_class(right);
    if (left_class != right_class) {
        if (left_class == NormalizedSortClass::constant) {
            return true;
        }
        if (right_class == NormalizedSortClass::constant) {
            return false;
        }
        if (left_class == NormalizedSortClass::algebraic) {
            return true;
        }
        if (right_class == NormalizedSortClass::algebraic) {
            return false;
        }
    }

    if (left_class == NormalizedSortClass::opaque &&
        right_class == NormalizedSortClass::opaque) {
        const auto* left_call = std::get_if<FunctionCall>(left.get());
        const auto* right_call = std::get_if<FunctionCall>(right.get());
        if (left_call != nullptr && right_call != nullptr) {
            if (left_call->head == "Plus" && right_call->head == "Power") {
                return true;
            }
            if (right_call->head == "Plus" && left_call->head == "Power") {
                return false;
            }
        }
    }

    return canonical_symbolic_less(left, right);
}

inline void flatten_function_args(
    const std::string& head,
    const std::vector<ExprPtr>& input,
    std::vector<ExprPtr>& output) {
    for (const auto& expr : input) {
        if (const auto* inner = std::get_if<FunctionCall>(expr.get());
            inner != nullptr && inner->head == head && is_flat_function(head)) {
            output.insert(output.end(), inner->args.begin(), inner->args.end());
        } else {
            output.push_back(expr);
        }
    }
}

inline ExprPtr maybe_normalize_complex_sum(const std::vector<ExprPtr>& terms) {
    if (terms.size() != 2) {
        return nullptr;
    }

    const ExprPtr* real_term = nullptr;
    const ExprPtr* imag_term = nullptr;
    if (std::holds_alternative<Number>(*terms[0])) {
        real_term = &terms[0];
        imag_term = &terms[1];
    } else if (std::holds_alternative<Number>(*terms[1])) {
        real_term = &terms[1];
        imag_term = &terms[0];
    } else {
        return nullptr;
    }

    const auto* times = std::get_if<FunctionCall>(imag_term->get());
    if (times == nullptr || times->head != "Times") {
        return nullptr;
    }

    double imag = 1.0;
    bool saw_imaginary_unit = false;
    for (const auto& arg : times->args) {
        if (std::holds_alternative<Number>(*arg)) {
            imag *= std::get<Number>(*arg).value;
            continue;
        }
        if (std::holds_alternative<Symbol>(*arg) &&
            std::get<Symbol>(*arg).name == "I") {
            saw_imaginary_unit = true;
            continue;
        }
        if (std::holds_alternative<Complex>(*arg)) {
            const auto& complex = std::get<Complex>(*arg);
            if (complex.real == 0.0 && complex.imag == 1.0) {
                saw_imaginary_unit = true;
                continue;
            }
        }
        return nullptr;
    }

    if (!saw_imaginary_unit) {
        return nullptr;
    }

    return make_expr<Complex>(std::get<Number>(**real_term).value, imag);
}

inline ExprPtr normalize_plus_args(const std::vector<ExprPtr>& args);
inline ExprPtr normalize_times_args(const std::vector<ExprPtr>& args);

inline ExprPtr normalize_plus_args(const std::vector<ExprPtr>& args) {
    std::vector<ExprPtr> norm_args;
    norm_args.reserve(args.size());
    for (const auto& arg : args) {
        norm_args.push_back(normalize_expr(arg));
    }

    std::vector<ExprPtr> flat_args;
    flat_args.reserve(norm_args.size());
    flatten_function_args("Plus", norm_args, flat_args);

    std::vector<ExprPtr> terms;
    for (const auto& arg : flat_args) {
        terms.push_back(arg);
    }

    if (is_orderless_function("Plus")) {
        std::sort(terms.begin(), terms.end(), canonical_plus_term_less);
    }

    if (auto complex = maybe_normalize_complex_sum(terms)) {
        return complex;
    }

    if (terms.empty()) {
        return make_expr<Number>(0.0);
    }

    if (terms.size() == 1) {
        return terms.front();
    }
    return make_fcall("Plus", terms);
}

inline ExprPtr normalize_times_args(const std::vector<ExprPtr>& args) {
    std::vector<ExprPtr> norm_args;
    norm_args.reserve(args.size());
    for (const auto& arg : args) {
        norm_args.push_back(normalize_expr(arg));
    }

    std::vector<ExprPtr> flat_args;
    flat_args.reserve(norm_args.size());
    flatten_function_args("Times", norm_args, flat_args);

    std::vector<ExprPtr> factors;
    for (const auto& arg : flat_args) {
        if (const auto* number = std::get_if<Number>(arg.get());
            number != nullptr && number->value == 1.0) {
            continue;
        }
        if (const auto* rational = std::get_if<Rational>(arg.get());
            rational != nullptr && rational->numerator == rational->denominator) {
            continue;
        }
        factors.push_back(arg);
    }

    if (is_orderless_function("Times")) {
        std::sort(factors.begin(), factors.end(), canonical_times_factor_less);
    }

    if (factors.empty()) {
        return make_expr<Number>(1.0);
    }

    if (factors.size() == 1) {
        return factors.front();
    }
    return make_fcall("Times", factors);
}

}  // namespace detail

inline ExprPtr normalize_expr(const ExprPtr& expr) {
    return std::visit(overloaded{
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
        [](const Infinity&) -> ExprPtr {
            return make_expr<Infinity>();
        },
        [](const ComplexInfinity&) -> ExprPtr {
            return make_expr<ComplexInfinity>();
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
                return detail::normalize_plus_args({a, make_fcall("Times", {make_expr<Number>(-1), b})});
            }
            if (f.head == "Plus") {
                return detail::normalize_plus_args(f.args);
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
                return detail::normalize_times_args({make_expr<Number>(-1), arg});
            }
            // Normalize Times
            if (f.head == "Times") {
                return detail::normalize_times_args(f.args);
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
            std::vector<Parameter> normalized_params;
            normalized_params.reserve(def.params.size());
            for (const auto& param : def.params) {
                normalized_params.emplace_back(
                    param.name,
                    param.default_value ? normalize_expr(param.default_value) : nullptr);
            }
            return make_expr<FunctionDefinition>(
                def.name, normalized_params, normalize_expr(def.body), def.delayed);
        },
        [](const Assignment& assign) -> ExprPtr {
            return make_expr<Assignment>(assign.name, normalize_expr(assign.value));
        },
        [](const Rule& rule) -> ExprPtr {
            return make_expr<Rule>(normalize_expr(rule.lhs), normalize_expr(rule.rhs));
        }
        }, *expr);
}

}
