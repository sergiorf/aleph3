#include "evaluator/SimplificationRules.hpp"
#include "expr/ExprUtils.hpp"
#include <cmath>
#include <cstdint>
#include <map>

namespace aleph3 {

    namespace {

    bool is_integral(double value) {
        return std::floor(value) == value;
    }

    void flatten_function_args(
        const std::string& head,
        const std::vector<ExprPtr>& input,
        std::vector<ExprPtr>& output) {
        for (const auto& expr : input) {
            if (const auto* inner = std::get_if<FunctionCall>(expr.get());
                inner != nullptr && inner->head == head) {
                output.insert(output.end(), inner->args.begin(), inner->args.end());
            } else {
                output.push_back(expr);
            }
        }
    }

    bool extract_linear_symbol_term(const ExprPtr& expr, std::string& symbol_name, double& coefficient) {
        if (const auto* symbol = std::get_if<Symbol>(expr.get())) {
            symbol_name = symbol->name;
            coefficient = 1.0;
            return true;
        }

        const auto* times = std::get_if<FunctionCall>(expr.get());
        if (times == nullptr || times->head != "Times" || times->args.size() != 2) {
            return false;
        }

        if (std::holds_alternative<Number>(*times->args[0]) && std::holds_alternative<Symbol>(*times->args[1])) {
            symbol_name = std::get<Symbol>(*times->args[1]).name;
            coefficient = get_number_value(times->args[0]);
            return true;
        }

        if (std::holds_alternative<Rational>(*times->args[0]) && std::holds_alternative<Symbol>(*times->args[1])) {
            const auto& rational = std::get<Rational>(*times->args[0]);
            symbol_name = std::get<Symbol>(*times->args[1]).name;
            coefficient = static_cast<double>(rational.numerator) / rational.denominator;
            return true;
        }

        return false;
    }

    bool extract_symbol_power_term(const ExprPtr& expr, std::string& symbol_name, double& exponent) {
        if (const auto* symbol = std::get_if<Symbol>(expr.get())) {
            symbol_name = symbol->name;
            exponent = 1.0;
            return true;
        }

        const auto* power = std::get_if<FunctionCall>(expr.get());
        if (power == nullptr || power->head != "Power" || power->args.size() != 2) {
            return false;
        }

        if (std::holds_alternative<Symbol>(*power->args[0]) && std::holds_alternative<Number>(*power->args[1])) {
            symbol_name = std::get<Symbol>(*power->args[0]).name;
            exponent = get_number_value(power->args[1]);
            return true;
        }

        return false;
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
                throw std::runtime_error("List sizes must match for elementwise Plus");
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

        double numeric_result = 0.0;
        bool has_rational_result = false;
        int64_t rational_num = 0;
        int64_t rational_den = 1;
        std::map<std::string, double> linear_terms;
        std::vector<ExprPtr> symbolic_terms;

        for (const auto& e : flat_args) {
            if (std::holds_alternative<Number>(*e)) {
                double val = get_number_value(e);
                if (val == 0) continue;
                numeric_result += val;
                continue;
            }

            if (std::holds_alternative<Rational>(*e)) {
                const auto& rational = std::get<Rational>(*e);
                if (!has_rational_result) {
                    rational_num = rational.numerator;
                    rational_den = rational.denominator;
                    has_rational_result = true;
                } else {
                    auto [nn, dd] = normalize_rational(
                        rational_num * rational.denominator + rational.numerator * rational_den,
                        rational_den * rational.denominator);
                    rational_num = nn;
                    rational_den = dd;
                }
                continue;
            }

            std::string symbol_name;
            double coefficient = 0.0;
            if (extract_linear_symbol_term(e, symbol_name, coefficient)) {
                linear_terms[symbol_name] += coefficient;
                continue;
            }

            symbolic_terms.push_back(e);
        }

        std::vector<ExprPtr> simplified;
        if (has_rational_result) {
            if (is_integral(numeric_result)) {
                auto [nn, dd] = normalize_rational(
                    rational_num + static_cast<int64_t>(numeric_result) * rational_den,
                    rational_den);
                if (nn != 0) {
                    simplified.push_back(dd == 1 ? make_expr<Number>(static_cast<double>(nn))
                                                 : make_expr<Rational>(nn, dd));
                }
            } else {
                numeric_result += static_cast<double>(rational_num) / rational_den;
            }
        }

        if (numeric_result != 0.0) {
            simplified.push_back(make_expr<Number>(numeric_result));
        }

        for (const auto& [symbol_name, coefficient] : linear_terms) {
            if (coefficient == 0.0) {
                continue;
            }
            if (coefficient == 1.0) {
                simplified.push_back(make_expr<Symbol>(symbol_name));
            } else {
                simplified.push_back(make_fcall(
                    "Times",
                    {make_expr<Number>(coefficient), make_expr<Symbol>(symbol_name)}));
            }
        }

        simplified.insert(simplified.end(), symbolic_terms.begin(), symbolic_terms.end());
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
                throw std::runtime_error("List sizes must match for elementwise Times");
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
        std::map<std::string, double> symbol_powers;
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

            std::string symbol_name;
            double exponent = 0.0;
            if (extract_symbol_power_term(e, symbol_name, exponent)) {
                symbol_powers[symbol_name] += exponent;
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
                if (!(nn == 1 && dd == 1 && (!symbol_powers.empty() || !symbolic_terms.empty()))) {
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
        if (!(numeric_result == 1.0 && (!symbol_powers.empty() || !symbolic_terms.empty()))) {
            simplified.push_back(make_expr<Number>(numeric_result));
        }

        for (const auto& [symbol_name, exponent] : symbol_powers) {
            if (exponent == 0.0) {
                continue;
            }
            if (exponent == 1.0) {
                simplified.push_back(make_expr<Symbol>(symbol_name));
            } else {
                simplified.push_back(make_fcall(
                    "Power",
                    {make_expr<Symbol>(symbol_name), make_expr<Number>(exponent)}));
            }
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
        if (const auto* nested_power = std::get_if<FunctionCall>(base.get());
            nested_power != nullptr && nested_power->head == "Power" && nested_power->args.size() == 2 &&
            std::holds_alternative<Number>(*nested_power->args[1]) &&
            std::holds_alternative<Number>(*exp)) {
            return make_fcall(
                "Power",
                {nested_power->args[0],
                 make_expr<Number>(get_number_value(nested_power->args[1]) * get_number_value(exp))});
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
    };
}
