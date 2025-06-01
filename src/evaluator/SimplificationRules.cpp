#include "evaluator/SimplificationRules.hpp"
#include "expr/ExprUtils.hpp"
#include <cmath>

namespace aleph3 {
    // Helper for GCD and rational normalization
    inline int64_t gcd(int64_t a, int64_t b) {
        while (b != 0) {
            int64_t t = b;
            b = a % b;
            a = t;
        }
        return std::abs(a);
    }
    inline std::pair<int64_t, int64_t> normalize_rational(int64_t num, int64_t den) {
        if (den == 0) return {num, den};
        int64_t sign = (den < 0) ? -1 : 1;
        num *= sign;
        den *= sign;
        int64_t g = gcd(num, den);
        return {num / g, den / g};
    }

    const std::unordered_map<std::string, SimplifyRule> simplification_rules = {
    {"Plus", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
            const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        std::vector<ExprPtr> eval_args;
        for (const auto& arg : args) {
            eval_args.push_back(eval(arg, ctx));
        }

        // Rational + Rational
        if (eval_args.size() == 2 &&
            std::holds_alternative<Rational>(*eval_args[0]) &&
            std::holds_alternative<Rational>(*eval_args[1])) {
            const auto& a = std::get<Rational>(*eval_args[0]);
            const auto& b = std::get<Rational>(*eval_args[1]);
            int64_t n = a.numerator * b.denominator + b.numerator * a.denominator;
            int64_t d = a.denominator * b.denominator;
            auto [nn, dd] = normalize_rational(n, d);
            if (dd == 0) {
                if (nn == 0) return make_expr<Indeterminate>();
                return make_expr<Infinity>();
            }
            return make_expr<Rational>(nn, dd);
        }
        // Rational + Number or Number + Rational
        if (eval_args.size() == 2) {
            ExprPtr rat = nullptr, num = nullptr;
            if (std::holds_alternative<Rational>(*eval_args[0]) && std::holds_alternative<Number>(*eval_args[1])) {
                rat = eval_args[0]; num = eval_args[1];
            } else if (std::holds_alternative<Number>(*eval_args[0]) && std::holds_alternative<Rational>(*eval_args[1])) {
                num = eval_args[0]; rat = eval_args[1];
            }
            if (rat && num) {
                const auto& r = std::get<Rational>(*rat);
                double n = std::get<Number>(*num).value;
                if (std::floor(n) == n) {
                    auto [nn, dd] = normalize_rational(r.numerator + static_cast<int64_t>(n) * r.denominator, r.denominator);
                    return make_expr<Rational>(nn, dd);
                } else {
                    double val = static_cast<double>(r.numerator) / r.denominator + n;
                    return make_expr<Number>(val);
                }
            }
        }

        // Elementwise list support for two lists
        if (eval_args.size() == 2 &&
            std::holds_alternative<List>(*eval_args[0]) &&
            std::holds_alternative<List>(*eval_args[1])) {
            const auto& l1 = std::get<List>(*eval_args[0]).elements;
            const auto& l2 = std::get<List>(*eval_args[1]).elements;
            if (l1.size() != l2.size())
                throw std::runtime_error("List sizes must match for elementwise Plus");
            std::vector<ExprPtr> result;
            for (size_t i = 0; i < l1.size(); ++i) {
                result.push_back(eval(make_fcall("Plus", { l1[i], l2[i] }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }

        // Scalar and list broadcasting (optional)
        if (eval_args.size() == 2) {
            if (std::holds_alternative<List>(*eval_args[0]) && std::holds_alternative<Number>(*eval_args[1])) {
                const auto& l1 = std::get<List>(*eval_args[0]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l1) {
                    result.push_back(eval(make_fcall("Plus", { elem, eval_args[1] }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
            if (std::holds_alternative<Number>(*eval_args[0]) && std::holds_alternative<List>(*eval_args[1])) {
                const auto& l2 = std::get<List>(*eval_args[1]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l2) {
                    result.push_back(eval(make_fcall("Plus", { eval_args[0], elem }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
        }

        // Original Plus rule for scalars and mixed
        double result = 0;
        std::vector<ExprPtr> simplified;
        for (const auto& e : eval_args) {
            if (std::holds_alternative<Number>(*e)) {
                double val = get_number_value(e);
                if (val == 0) continue;
                result += val;
            }
            else {
                simplified.push_back(e);
            }
        }
        if (result != 0) simplified.insert(simplified.begin(), make_expr<Number>(result));
        if (simplified.empty()) return make_expr<Number>(0);
        if (simplified.size() == 1) return simplified[0];
        return make_fcall("Plus", simplified);
    }},
    {"Times", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
             const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        std::vector<ExprPtr> eval_args;
        for (const auto& arg : args) {
            eval_args.push_back(eval(arg, ctx));
        }

        // Rational * Rational
        if (eval_args.size() == 2 &&
            std::holds_alternative<Rational>(*eval_args[0]) &&
            std::holds_alternative<Rational>(*eval_args[1])) {
            const auto& a = std::get<Rational>(*eval_args[0]);
            const auto& b = std::get<Rational>(*eval_args[1]);
            auto [nn, dd] = normalize_rational(a.numerator * b.numerator, a.denominator * b.denominator);
            if (dd == 0) {
                if (nn == 0) return make_expr<Indeterminate>();
                return make_expr<Infinity>();
            }
            return make_expr<Rational>(nn, dd);
        }
        // Rational * Number or Number * Rational
        if (eval_args.size() == 2) {
            ExprPtr rat = nullptr, num = nullptr;
            if (std::holds_alternative<Rational>(*eval_args[0]) && std::holds_alternative<Number>(*eval_args[1])) {
                rat = eval_args[0]; num = eval_args[1];
            } else if (std::holds_alternative<Number>(*eval_args[0]) && std::holds_alternative<Rational>(*eval_args[1])) {
                num = eval_args[0]; rat = eval_args[1];
            }
            if (rat && num) {
                const auto& r = std::get<Rational>(*rat);
                double n = std::get<Number>(*num).value;
                if (std::floor(n) == n) {
                    auto [nn, dd] = normalize_rational(r.numerator * static_cast<int64_t>(n), r.denominator);
                    return make_expr<Rational>(nn, dd);
                } else {
                    double val = static_cast<double>(r.numerator) / r.denominator * n;
                    return make_expr<Number>(val);
                }
            }
        }

        // Elementwise list support for two lists
        if (eval_args.size() == 2 &&
            std::holds_alternative<List>(*eval_args[0]) &&
            std::holds_alternative<List>(*eval_args[1])) {
            const auto& l1 = std::get<List>(*eval_args[0]).elements;
            const auto& l2 = std::get<List>(*eval_args[1]).elements;
            if (l1.size() != l2.size())
                throw std::runtime_error("List sizes must match for elementwise Times");
            std::vector<ExprPtr> result;
            for (size_t i = 0; i < l1.size(); ++i) {
                result.push_back(eval(make_fcall("Times", { l1[i], l2[i] }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }

        // Scalar and list broadcasting
        if (eval_args.size() == 2) {
            if (std::holds_alternative<List>(*eval_args[0]) && std::holds_alternative<Number>(*eval_args[1])) {
                const auto& l1 = std::get<List>(*eval_args[0]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l1) {
                    result.push_back(eval(make_fcall("Times", { elem, eval_args[1] }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
            if (std::holds_alternative<Number>(*eval_args[0]) && std::holds_alternative<List>(*eval_args[1])) {
                const auto& l2 = std::get<List>(*eval_args[1]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l2) {
                    result.push_back(eval(make_fcall("Times", { eval_args[0], elem }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
        }

        // Original Times rule for scalars and mixed
        double result = 1;
        std::vector<ExprPtr> simplified;
        for (const auto& e : eval_args) {
            if (std::holds_alternative<Number>(*e)) {
                double val = get_number_value(e);
                if (val == 0) return make_expr<Number>(0);
                if (val == 1) continue;
                result *= val;
            }
            else {
                simplified.push_back(e);
            }
        }
        if (result != 1) simplified.insert(simplified.begin(), make_expr<Number>(result));
        if (simplified.empty()) return make_expr<Number>(1);
        if (simplified.size() == 1) return simplified[0];
        return make_fcall("Times", simplified);
    }},
    {"Power", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
             const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        if (args.size() != 2) return make_fcall("Power", args);
        auto base = eval(args[0], ctx);
        auto exp = eval(args[1], ctx);
        if (std::holds_alternative<Number>(*exp)) {
            double e = get_number_value(exp);
            if (e == 0) return make_expr<Number>(1.0);
            if (e == 1) return base;
        }
        if (std::holds_alternative<Number>(*base)) {
            double b = get_number_value(base);
            if (b == 0) return make_expr<Number>(0.0);
            if (b == 1) return make_expr<Number>(1.0);
        }
        // Add this block for rational exponents:
        if (std::holds_alternative<Number>(*base) && std::holds_alternative<Rational>(*exp)) {
            double b = get_number_value(base);
            const auto& r = std::get<Rational>(*exp);
            // Only handle positive denominator
            if (r.denominator > 0) {
                double root = std::pow(b, 1.0 / r.denominator);
                double result = std::pow(root, r.numerator);
                // If denominator is odd, allow negative base (real root)
                if (b < 0 && r.denominator % 2 == 1) {
                    result = -std::pow(-b, static_cast<double>(r.numerator) / r.denominator);
                }
                return make_expr<Number>(result);
            }
        }
        if (std::holds_alternative<Number>(*base) && std::holds_alternative<Number>(*exp)) {
            double b = get_number_value(base);
            double e = get_number_value(exp);
            return make_expr<Number>(std::pow(b, e));
        }
        return make_fcall("Power", {base, exp});
    }},
    {"Divide", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
              const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        if (args.size() != 2) return make_fcall("Divide", args);
            auto num = eval(args[0], ctx);
            auto denom = eval(args[1], ctx);
            // Rational / Rational
            if (std::holds_alternative<Rational>(*num) && std::holds_alternative<Rational>(*denom)) {
                const auto& a = std::get<Rational>(*num);
                const auto& b = std::get<Rational>(*denom);
                if (b.numerator == 0) {
                    if (a.numerator == 0) return make_expr<Indeterminate>();
                    return make_expr<Infinity>();
                }
                auto [nn, dd] = normalize_rational(a.numerator * b.denominator, a.denominator * b.numerator);
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
                if (std::floor(b) == b) {
                    auto [nn, dd] = normalize_rational(a.numerator, a.denominator * static_cast<int64_t>(b));
                    return make_expr<Rational>(nn, dd);
                } else {
                    double val = static_cast<double>(a.numerator) / a.denominator / b;
                    return make_expr<Number>(val);
                }
            }
            if (std::holds_alternative<Number>(*num) && std::holds_alternative<Rational>(*denom)) {
                double a = std::get<Number>(*num).value;
                const auto& b = std::get<Rational>(*denom);
                if (std::floor(a) == a) {
                    auto [nn, dd] = normalize_rational(static_cast<int64_t>(a) * b.denominator, b.numerator);
                    return make_expr<Rational>(nn, dd);
                } else {
                    double val = a / (static_cast<double>(b.numerator) / b.denominator);
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
    };
}