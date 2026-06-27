#include "evaluator/SimplificationRules.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/GammaUtils.hpp"
#include "evaluator/EvaluatorSemantics.hpp"
#include "kernel/Rewrite.hpp"
#include "normalizer/Normalizer.hpp"
#include "expr/ExprUtils.hpp"
#include <cmath>
#include <cstdint>
#include <map>

namespace aleph3 {

    namespace {

    bool is_integral(double value) {
        return std::floor(value) == value;
    }

    const std::vector<Rule>& power_identity_rules() {
        static const std::vector<Rule> rules = {
            Rule{
                make_fcall("Power", {make_expr<Symbol>("a_"), make_expr<Number>(0.0)}),
                make_expr<Number>(1.0)
            },
            Rule{
                make_fcall("Power", {make_expr<Symbol>("a_"), make_expr<Number>(1.0)}),
                make_expr<Symbol>("a")
            },
            Rule{
                make_fcall("Power", {make_expr<Number>(1.0), make_expr<Symbol>("a_")}),
                make_expr<Number>(1.0)
            }
        };
        return rules;
    }

    bool contains_list_argument(const std::vector<ExprPtr>& args) {
        for (const auto& arg : args) {
            if (std::holds_alternative<List>(*arg)) {
                return true;
            }
        }
        return false;
    }

    ExprPtr apply_power_identity_rewrite_simplification(
        const std::vector<ExprPtr>& args,
        EvaluationContext& ctx) {
        if (args.size() != 2 || contains_list_argument(args)) {
            return nullptr;
        }

        auto current = normalize_expr(make_fcall("Power", args));
        bool changed = false;

        for (std::size_t iteration = 0; iteration < 8; ++iteration) {
            bool applied_rule = false;
            for (const auto& rule : power_identity_rules()) {
                const auto rewritten = kernel::rewrite_repeated(current, rule, ctx, 1);
                if (!rewritten.changed) {
                    continue;
                }
                current = normalize_expr(rewritten.expr);
                changed = true;
                applied_rule = true;
                break;
            }
            if (!applied_rule) {
                break;
            }
        }

        return changed ? current : nullptr;
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

    }  // namespace

    const std::unordered_map<std::string, SimplifyRule> simplification_rules = {
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
        if (!std::holds_alternative<FunctionCall>(*current)) {
            return current;
        }

        if (const auto* plus = std::get_if<FunctionCall>(current.get())) {
            if (auto rewritten = kernel::rewrite_normalized_arithmetic_head(*plus, ctx)) {
                current = normalize_expr(*rewritten);
            }
        }
        if (!std::holds_alternative<FunctionCall>(*current)) {
            return current;
        }

        if (const auto* plus = std::get_if<FunctionCall>(current.get())) {
            if (auto rewritten = kernel::rewrite_normalized_symbolic_coefficient_head(*plus, ctx)) {
                return *rewritten;
            }
        }

        return current;
    }},
    {"Times", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
             const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        std::vector<ExprPtr> eval_args;
        for (const auto& arg : args) {
            eval_args.push_back(eval(arg, ctx));
        }

        const auto normalized_eval_args = normalize_expr(make_fcall("Times", eval_args));
        if (const auto* times = std::get_if<FunctionCall>(normalized_eval_args.get())) {
            if (auto rewritten = kernel::rewrite_normalized_arithmetic_head(*times, ctx)) {
                return *rewritten;
            }
        }

        const auto normalized_after_arithmetic = normalize_expr(make_fcall("Times", eval_args));
        if (const auto* times = std::get_if<FunctionCall>(normalized_after_arithmetic.get())) {
            if (auto rewritten = kernel::rewrite_normalized_algebraic_head(*times, ctx)) {
                return *rewritten;
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
            auto [nn, dd] = normalize_rational(a.numerator * b.numerator, a.denominator * b.denominator);
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
        int64_t rational_num = 1;
        int64_t rational_den = 1;
        std::vector<ExprPtr> symbolic_terms;

        for (const auto& e : flat_args) {
            if (std::holds_alternative<Number>(*e)) {
                double val = get_number_value(e);
                if (val == 0) return make_expr<Number>(0);
                if (val == 1) continue;
                numeric_result *= val;
                continue;
            }

            if (std::holds_alternative<Rational>(*e)) {
                const auto& rational = std::get<Rational>(*e);
                if (rational.numerator == 0) {
                    return make_expr<Number>(0);
                }
                if (!has_rational_result) {
                    rational_num = rational.numerator;
                    rational_den = rational.denominator;
                    has_rational_result = true;
                } else {
                    auto [nn, dd] = normalize_rational(
                        rational_num * rational.numerator,
                    rational_den * rational.denominator);
                    rational_num = nn;
                    rational_den = dd;
                }
                continue;
            }

            symbolic_terms.push_back(e);
        }

        std::vector<ExprPtr> simplified;
        if (has_rational_result) {
            if (is_integral(numeric_result)) {
                auto [nn, dd] = normalize_rational(
                    rational_num * static_cast<int64_t>(numeric_result),
                    rational_den);
                if (!(nn == 1 && dd == 1 && !symbolic_terms.empty())) {
                    simplified.push_back(dd == 1 ? make_expr<Number>(static_cast<double>(nn))
                                                 : make_expr<Rational>(nn, dd));
                }
            } else {
                numeric_result *= static_cast<double>(rational_num) / rational_den;
            }
        }

        if (numeric_result == 0.0) {
            return make_expr<Number>(0);
        }
        if (!(numeric_result == 1.0 && !symbolic_terms.empty())) {
            simplified.push_back(make_expr<Number>(numeric_result));
        }

        simplified.insert(simplified.end(), symbolic_terms.begin(), symbolic_terms.end());
        if (simplified.empty()) return make_expr<Number>(1);
        if (simplified.size() == 1) return simplified[0];
        return make_fcall("Times", simplified);
    }},
    {"Power", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
             const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        if (args.size() != 2) return make_fcall("Power", args);
        auto base = eval(args[0], ctx);
        auto exp = eval(args[1], ctx);
        if (auto rewritten = apply_power_identity_rewrite_simplification({base, exp}, ctx)) {
            return rewritten;
        }
        if (std::holds_alternative<Number>(*exp)) {
            double e = get_number_value(exp);
            if (e == 0) return make_expr<Number>(1.0);
            if (e == 1) return base;
        }
        if (std::holds_alternative<Number>(*base) && get_number_value(base) == 0.0 &&
            std::holds_alternative<Number>(*exp) && get_number_value(exp) > 0.0) {
            return make_expr<Number>(0.0);
        }
        if (std::holds_alternative<Number>(*base)) {
            double b = get_number_value(base);
            if (b == 1) return make_expr<Number>(1.0);
        }
        auto normalized_power = normalize_expr(make_fcall("Power", {base, exp}));
        if (const auto* power = std::get_if<FunctionCall>(normalized_power.get())) {
            if (auto rewritten = kernel::rewrite_normalized_algebraic_head(*power, ctx)) {
                return *rewritten;
            }
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
        if (std::holds_alternative<Rational>(*base) && std::holds_alternative<Rational>(*exp)) {
            const auto& b = std::get<Rational>(*base);
            const auto& r = std::get<Rational>(*exp);
            double b_val = static_cast<double>(b.numerator) / b.denominator;
            // Only handle positive denominator
            if (r.denominator > 0) {
                double root = std::pow(b_val, 1.0 / r.denominator);
                double result = std::pow(root, r.numerator);
                // If denominator is odd, allow negative base (real root)
                if (b_val < 0 && r.denominator % 2 == 1) {
                    result = -std::pow(-b_val, static_cast<double>(r.numerator) / r.denominator);
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
            if (std::holds_alternative<Number>(*denom) && get_number_value(denom) == 1.0) {
                return num;
            }
            if (std::holds_alternative<Number>(*num) && get_number_value(num) == 0.0 &&
                !(std::holds_alternative<Number>(*denom) && get_number_value(denom) == 0.0)) {
                return make_expr<Number>(0.0);
            }
            if (to_string_raw(num) == to_string_raw(denom)) {
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
}
