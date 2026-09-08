#include "evaluator/SimplificationRules.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/GammaUtils.hpp"
#include "evaluator/EvaluatorSemantics.hpp"
#include "kernel/Rewrite.hpp"
#include "normalizer/Normalizer.hpp"
#include "expr/ExprUtils.hpp"
#include "expr/ExprStructural.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>

namespace aleph3 {

    namespace {

    bool is_integral(double value) {
        return std::floor(value) == value;
    }

    std::optional<int64_t> integer_exponent_value(const ExprPtr& expr) {
        return exact_int64_from_expr(expr);
    }

    std::optional<kernel::ExactRational> exact_rational_atom(const ExprPtr& expr) {
        if (const auto* integer = std::get_if<Integer>(expr.get())) {
            return kernel::ExactRational(integer->value, kernel::ExactInteger(1));
        }
        if (const auto* rational = std::get_if<Rational>(expr.get())) {
            return rational->exact();
        }
        return std::nullopt;
    }

    Complex multiply_complex(const Complex& lhs, const Complex& rhs) {
        return Complex{
            lhs.real * rhs.real - lhs.imag * rhs.imag,
            lhs.real * rhs.imag + lhs.imag * rhs.real};
    }

    std::optional<Complex> reciprocal_complex(const Complex& value) {
        const double norm_squared = value.real * value.real + value.imag * value.imag;
        if (norm_squared == 0.0 || !std::isfinite(norm_squared)) {
            return std::nullopt;
        }
        return Complex{value.real / norm_squared, -value.imag / norm_squared};
    }

    ExprPtr complex_integer_power(const Complex& base, int64_t exponent) {
        if (exponent == 0) {
            return make_expr<Number>(1.0);
        }

        Complex power_base = base;
        uint64_t remaining = 0;
        if (exponent < 0) {
            auto reciprocal = reciprocal_complex(base);
            if (!reciprocal.has_value()) {
                return nullptr;
            }
            power_base = *reciprocal;
            remaining = static_cast<uint64_t>(-(exponent + 1)) + 1;
        } else {
            remaining = static_cast<uint64_t>(exponent);
        }

        Complex result{1.0, 0.0};
        while (remaining > 0) {
            if ((remaining & 1U) != 0U) {
                result = multiply_complex(result, power_base);
            }
            remaining >>= 1U;
            if (remaining > 0) {
                power_base = multiply_complex(power_base, power_base);
            }
        }
        return make_expr<Complex>(result.real, result.imag);
    }

    void flatten_function_args(
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

    ExprPtr apply_registered_normalized_head_rewrites(
        const ExprPtr& normalized,
        EvaluationContext& ctx,
        std::size_t max_passes = 4) {
        ExprPtr current = normalized;
        for (std::size_t pass = 0; pass < max_passes; ++pass) {
            const auto* func = std::get_if<FunctionCall>(current.get());
            if (func == nullptr) {
                return current;
            }

            auto rewritten = kernel::rewrite_normalized_head(*func, ctx);
            if (!rewritten.has_value()) {
                return current;
            }

            auto next = normalize_expr(*rewritten);
            if (kernel::structurally_equal(current, next)) {
                return current;
            }
            current = std::move(next);
        }
        return current;
    }

    }  // namespace

    const std::unordered_map<std::string, SimplifyRule>& simplification_rules() {
        static const std::unordered_map<std::string, SimplifyRule> rules = {
    {"Plus", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
            const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        std::vector<ExprPtr> eval_args;
        for (const auto& arg : args) {
            eval_args.push_back(eval(arg, ctx));
        }

        std::vector<ExprPtr> flat_args;
        flatten_function_args("Plus", eval_args, flat_args);

        // Rational + Rational
        if (flat_args.size() == 2 &&
            std::holds_alternative<Rational>(*flat_args[0]) &&
            std::holds_alternative<Rational>(*flat_args[1])) {
            const auto& a = std::get<Rational>(*flat_args[0]);
            const auto& b = std::get<Rational>(*flat_args[1]);
            auto [nn, dd] = checked_rational_add(
                a.numerator, a.denominator, b.numerator, b.denominator);
            if (dd == 0) {
                if (nn == 0) return make_expr<Indeterminate>();
                return make_expr<Infinity>();
            }
            return make_expr<Rational>(nn, dd);
        }
        // Rational + Number or Number + Rational
        if (flat_args.size() == 2) {
            ExprPtr rat = nullptr, num = nullptr;
            if (std::holds_alternative<Rational>(*flat_args[0]) && std::holds_alternative<Number>(*flat_args[1])) {
                rat = flat_args[0]; num = flat_args[1];
            } else if (std::holds_alternative<Number>(*flat_args[0]) && std::holds_alternative<Rational>(*flat_args[1])) {
                num = flat_args[0]; rat = flat_args[1];
            }
            if (rat && num) {
                const auto& r = std::get<Rational>(*rat);
                double n = std::get<Number>(*num).value;
                if (auto integer = exact_int64_from_number(n)) {
                    auto [nn, dd] = checked_rational_add(
                        r.numerator, r.denominator, *integer, 1);
                    return make_expr<Rational>(nn, dd);
                } else {
                    double val = finite_double_from_exact_rational(r).value_or(0.0) + n;
                    return make_expr<Number>(val);
                }
            }
        }

        // Elementwise list support for two lists
        if (flat_args.size() == 2 &&
            std::holds_alternative<List>(*flat_args[0]) &&
            std::holds_alternative<List>(*flat_args[1])) {
            const auto& l1 = std::get<List>(*flat_args[0]).elements;
            const auto& l2 = std::get<List>(*flat_args[1]).elements;
            if (l1.size() != l2.size())
                throw_invalid_form("List sizes must match for elementwise Plus");
            std::vector<ExprPtr> result;
            for (size_t i = 0; i < l1.size(); ++i) {
                result.push_back(eval(make_fcall("Plus", { l1[i], l2[i] }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }

        // Scalar and list broadcasting (optional)
        if (flat_args.size() == 2) {
            if (std::holds_alternative<List>(*flat_args[0]) && std::holds_alternative<Number>(*flat_args[1])) {
                const auto& l1 = std::get<List>(*flat_args[0]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l1) {
                    result.push_back(eval(make_fcall("Plus", { elem, flat_args[1] }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
            if (std::holds_alternative<Number>(*flat_args[0]) && std::holds_alternative<List>(*flat_args[1])) {
                const auto& l2 = std::get<List>(*flat_args[1]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l2) {
                    result.push_back(eval(make_fcall("Plus", { flat_args[0], elem }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
        }

        ExprPtr current = normalize_expr(make_fcall("Plus", flat_args));
        return apply_registered_normalized_head_rewrites(current, ctx);
    }},
    {"Times", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
             const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        std::vector<ExprPtr> eval_args;
        for (const auto& arg : args) {
            eval_args.push_back(eval(arg, ctx));
        }

        const auto normalized_eval_args = normalize_expr(make_fcall("Times", eval_args));
        if (std::holds_alternative<FunctionCall>(*normalized_eval_args)) {
            auto rewritten = apply_registered_normalized_head_rewrites(normalized_eval_args, ctx);
            if (!kernel::structurally_equal(rewritten, normalized_eval_args)) {
                return rewritten;
            }
        }

        std::vector<ExprPtr> flat_args;
        flatten_function_args("Times", eval_args, flat_args);

        // Rational * Rational
        if (flat_args.size() == 2 &&
            std::holds_alternative<Rational>(*flat_args[0]) &&
            std::holds_alternative<Rational>(*flat_args[1])) {
            const auto& a = std::get<Rational>(*flat_args[0]);
            const auto& b = std::get<Rational>(*flat_args[1]);
            auto [nn, dd] = checked_rational_multiply(
                a.numerator, a.denominator, b.numerator, b.denominator);
            if (dd == 0) {
                if (nn == 0) return make_expr<Indeterminate>();
                return make_expr<Infinity>();
            }
            return make_expr<Rational>(nn, dd);
        }
        // Rational * Number or Number * Rational
        if (flat_args.size() == 2) {
            ExprPtr rat = nullptr, num = nullptr;
            if (std::holds_alternative<Rational>(*flat_args[0]) && std::holds_alternative<Number>(*flat_args[1])) {
                rat = flat_args[0]; num = flat_args[1];
            } else if (std::holds_alternative<Number>(*flat_args[0]) && std::holds_alternative<Rational>(*flat_args[1])) {
                num = flat_args[0]; rat = flat_args[1];
            }
            if (rat && num) {
                const auto& r = std::get<Rational>(*rat);
                double n = std::get<Number>(*num).value;
                if (auto integer = exact_int64_from_number(n)) {
                    auto [nn, dd] = checked_rational_multiply(
                        r.numerator, r.denominator, *integer, 1);
                    return make_expr<Rational>(nn, dd);
                } else {
                    double val = finite_double_from_exact_rational(r).value_or(0.0) * n;
                    return make_expr<Number>(val);
                }
            }
        }

        // Elementwise list support for two lists
        if (flat_args.size() == 2 &&
            std::holds_alternative<List>(*flat_args[0]) &&
            std::holds_alternative<List>(*flat_args[1])) {
            const auto& l1 = std::get<List>(*flat_args[0]).elements;
            const auto& l2 = std::get<List>(*flat_args[1]).elements;
            if (l1.size() != l2.size())
                throw_invalid_form("List sizes must match for elementwise Times");
            std::vector<ExprPtr> result;
            for (size_t i = 0; i < l1.size(); ++i) {
                result.push_back(eval(make_fcall("Times", { l1[i], l2[i] }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }

        // Scalar and list broadcasting
        if (flat_args.size() == 2) {
            if (std::holds_alternative<List>(*flat_args[0]) && std::holds_alternative<Number>(*flat_args[1])) {
                const auto& l1 = std::get<List>(*flat_args[0]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l1) {
                    result.push_back(eval(make_fcall("Times", { elem, flat_args[1] }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
            if (std::holds_alternative<Number>(*flat_args[0]) && std::holds_alternative<List>(*flat_args[1])) {
                const auto& l2 = std::get<List>(*flat_args[1]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l2) {
                    result.push_back(eval(make_fcall("Times", { flat_args[0], elem }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
        }

        double numeric_result = 1.0;
        bool has_rational_result = false;
        kernel::ExactInteger rational_num = 1;
        kernel::ExactInteger rational_den = 1;
        std::vector<ExprPtr> symbolic_terms;

        for (const auto& e : flat_args) {
            if (std::holds_alternative<Number>(*e)) {
                double val = get_number_value(e);
                if (val == 0) return make_expr<Number>(0);
                if (val == 1) continue;
                numeric_result *= val;
                continue;
            }

            if (std::holds_alternative<Integer>(*e)) {
                const auto& integer = std::get<Integer>(*e);
                if (integer.value.is_zero()) {
                    return make_expr<Integer>(0);
                }
                if (integer.value.is_one()) {
                    continue;
                }
                if (!has_rational_result) {
                    rational_num = integer.value;
                    rational_den = 1;
                    has_rational_result = true;
                } else {
                    auto [nn, dd] = checked_rational_multiply(
                        rational_num,
                        rational_den,
                        integer.value,
                        kernel::ExactInteger(1));
                    rational_num = nn;
                    rational_den = dd;
                }
                continue;
            }

            if (std::holds_alternative<Rational>(*e)) {
                const auto& rational = std::get<Rational>(*e);
                if (rational.numerator == 0) {
                    return make_expr<Integer>(0);
                }
                if (!has_rational_result) {
                    rational_num = rational.numerator;
                    rational_den = rational.denominator;
                    has_rational_result = true;
                } else {
                    auto [nn, dd] = checked_rational_multiply(
                        rational_num,
                        rational_den,
                        rational.numerator,
                        rational.denominator);
                    rational_num = nn;
                    rational_den = dd;
                }
                continue;
            }

            symbolic_terms.push_back(e);
        }

        std::vector<ExprPtr> simplified;
        if (has_rational_result) {
            if (auto integer = exact_int64_from_number(numeric_result)) {
                auto [nn, dd] = checked_rational_multiply(
                    rational_num, rational_den, *integer, 1);
                if (!(nn == 1 && dd == 1 && !symbolic_terms.empty())) {
                    simplified.push_back(
                        kernel::ExactInteger(dd).is_one() ? make_expr<Integer>(nn)
                                                          : make_expr<Rational>(nn, dd));
                }
            } else {
                numeric_result *= finite_double_from_exact_rational(
                    Rational(rational_num, rational_den)).value_or(0.0);
            }
        }

        if (numeric_result == 0.0) {
            return make_expr<Number>(0);
        }
        if (!(numeric_result == 1.0 && (!symbolic_terms.empty() || has_rational_result))) {
            simplified.push_back(make_expr<Number>(numeric_result));
        }

        simplified.insert(simplified.end(), symbolic_terms.begin(), symbolic_terms.end());
        if (simplified.empty()) return make_expr<Integer>(1);
        if (simplified.size() == 1) return simplified[0];
        return make_fcall("Times", simplified);
    }},
    {"Power", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
             const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        if (args.size() != 2) return make_fcall("Power", args);
        auto base = eval(args[0], ctx);
        auto exp = eval(args[1], ctx);
        auto normalized_power = normalize_expr(make_fcall("Power", {base, exp}));
        if (std::holds_alternative<FunctionCall>(*normalized_power)) {
            auto rewritten = apply_registered_normalized_head_rewrites(normalized_power, ctx);
            if (!kernel::structurally_equal(rewritten, normalized_power)) {
                return rewritten;
            }
        }
        if (is_zero(base)) {
            if (const auto integer_exponent = integer_exponent_value(exp);
                integer_exponent.has_value() && *integer_exponent > 0) {
                return make_expr<Integer>(0);
            }
        }
        if (std::holds_alternative<Complex>(*base)) {
            if (auto integer_exponent = integer_exponent_value(exp)) {
                if (auto result = complex_integer_power(std::get<Complex>(*base), *integer_exponent)) {
                    return result;
                }
            }
        }
        // Add this block for rational exponents:
        if (std::holds_alternative<Number>(*base) && std::holds_alternative<Rational>(*exp)) {
            double b = get_number_value(base);
            const auto& r = std::get<Rational>(*exp);
            // Only handle positive denominator
            const auto bounded = r.exact().to_bounded();
            if (bounded && bounded->second > 0) {
                double root = std::pow(b, 1.0 / static_cast<double>(bounded->second));
                double result = std::pow(root, static_cast<double>(bounded->first));
                // If denominator is odd, allow negative base (real root)
                if (b < 0 && bounded->second % 2 == 1) {
                    result = -std::pow(-b, static_cast<double>(bounded->first) / bounded->second);
                }
                return make_expr<Number>(result);
            }
        }
        if (std::holds_alternative<Rational>(*base) && std::holds_alternative<Rational>(*exp)) {
            const auto& b = std::get<Rational>(*base);
            const auto& r = std::get<Rational>(*exp);
            const auto b_value = finite_double_from_exact_rational(b);
            const auto bounded_exp = r.exact().to_bounded();
            if (!b_value || !bounded_exp) {
                return make_fcall("Power", {base, exp});
            }
            double b_val = *b_value;
            // Only handle positive denominator
            if (bounded_exp->second > 0) {
                double root = std::pow(b_val, 1.0 / static_cast<double>(bounded_exp->second));
                double result = std::pow(root, static_cast<double>(bounded_exp->first));
                // If denominator is odd, allow negative base (real root)
                if (b_val < 0 && bounded_exp->second % 2 == 1) {
                    result = -std::pow(-b_val, static_cast<double>(bounded_exp->first) / bounded_exp->second);
                }
                return make_expr<Number>(result);
            }
        }
        if (auto exact_base = exact_rational_atom(base), exact_exp = exact_rational_atom(exp);
            exact_base.has_value() && exact_exp.has_value()) {
            const auto bounded_base = exact_base->to_bounded();
            const auto bounded_exp = exact_exp->to_bounded();
            if (bounded_base && bounded_exp && bounded_exp->second == 1) {
                const double b = static_cast<double>(bounded_base->first) /
                    static_cast<double>(bounded_base->second);
                const double e = static_cast<double>(bounded_exp->first);
                if (b == 0.0 && e == 0.0) {
                    return make_fcall("Power", {base, exp});
                }
                if (b == 0.0 && e < 0.0) {
                    return make_fcall("Power", {base, exp});
                }
                if (bounded_base->second == 1 && bounded_exp->first >= 0) {
                    kernel::ExactInteger result(1);
                    kernel::ExactInteger factor(bounded_base->first);
                    for (int64_t i = 0; i < bounded_exp->first; ++i) {
                        result = result * factor;
                    }
                    return make_expr<Integer>(std::move(result));
                }
                return make_expr<Number>(std::pow(b, e));
            }
        }
        if (std::holds_alternative<Number>(*base) && std::holds_alternative<Number>(*exp)) {
            double b = get_number_value(base);
            double e = get_number_value(exp);
            if (b == 0.0 && e == 0.0) {
                return make_fcall("Power", {base, exp});
            }
            if (b == 0.0 && e < 0.0) {
                return make_fcall("Power", {base, exp});
            }
            return make_expr<Number>(std::pow(b, e));
        }
        return make_fcall("Power", {base, exp});
    }},
    {"Divide", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
              const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        if (args.size() != 2) return make_fcall("Divide", args);
            auto num = eval(args[0], ctx);
            auto denom = eval(args[1], ctx);
            if (std::holds_alternative<Number>(*denom) && get_number_value(denom) == 1.0) {
                return num;
            }
            if (std::holds_alternative<Number>(*num) && get_number_value(num) == 0.0 &&
                !(std::holds_alternative<Number>(*denom) && get_number_value(denom) == 0.0)) {
                return make_expr<Number>(0.0);
            }
            if (structural_equal(normalize_expr(num), normalize_expr(denom))) {
                return make_expr<Number>(1.0);
            }
            // Rational / Rational
            if (std::holds_alternative<Rational>(*num) && std::holds_alternative<Rational>(*denom)) {
                const auto& a = std::get<Rational>(*num);
                const auto& b = std::get<Rational>(*denom);
                if (b.numerator == 0) {
                    if (a.numerator == 0) return make_expr<Indeterminate>();
                    return make_expr<Infinity>();
                }
                auto [nn, dd] = checked_rational_divide(
                    a.numerator, a.denominator, b.numerator, b.denominator);
                if (dd == 0) {
                    if (nn == 0) return make_expr<Indeterminate>();
                    return make_expr<Infinity>();
                }
                return make_expr<Rational>(nn, dd);
            }
            // Rational / Number or Number / Rational
            if (std::holds_alternative<Rational>(*num) && std::holds_alternative<Number>(*denom)) {
                const auto& a = std::get<Rational>(*num);
                double b = std::get<Number>(*denom).value;
                if (auto integer = exact_int64_from_number(b)) {
                    auto [nn, dd] = checked_rational_divide(
                        a.numerator, a.denominator, *integer, 1);
                    return make_expr<Rational>(nn, dd);
                } else {
                    double val = finite_double_from_exact_rational(a).value_or(0.0) / b;
                    return make_expr<Number>(val);
                }
            }
            if (std::holds_alternative<Number>(*num) && std::holds_alternative<Rational>(*denom)) {
                double a = std::get<Number>(*num).value;
                const auto& b = std::get<Rational>(*denom);
                if (auto integer = exact_int64_from_number(a)) {
                    auto [nn, dd] = checked_rational_divide(
                        *integer, 1, b.numerator, b.denominator);
                    return make_expr<Rational>(nn, dd);
                } else {
                    double val = a / finite_double_from_exact_rational(b).value_or(0.0);
                    return make_expr<Number>(val);
                }
            }
            if (std::holds_alternative<Number>(*num) && std::holds_alternative<Number>(*denom)) {
                double a = get_number_value(num);
                double b = get_number_value(denom);
                if (b == 0.0) {
                    if (a == 0.0) return make_expr(Indeterminate{});
                    // TODO: Return Infinity or ComplexInfinity for a != 0
            }
            return make_expr<Number>(a / b);
        }
        return make_fcall("Divide", {num, denom});
    }},
    {"Gamma", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
             const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        if (args.size() != 1) {
            return make_fcall("Gamma", args);
        }

        auto arg = eval(args[0], ctx);
        if (auto simplified = simplify_gamma_argument(arg)) {
            return *simplified;
        }

        return make_fcall("Gamma", {arg});
    }},
        };
        return rules;
    }
}
