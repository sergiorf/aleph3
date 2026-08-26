#include "transforms/Transforms.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"
#include "kernel/Rewrite.hpp"
#include "normalizer/Normalizer.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <algorithm>

namespace aleph3 {

    namespace {

    bool try_get_exact_integer(const ExprPtr& expr, int& value) {
        if (const auto* number = std::get_if<Number>(expr.get())) {
            if (std::floor(number->value) != number->value) {
                return false;
            }
            value = static_cast<int>(number->value);
            return true;
        }
        return false;
    }

    ExprPtr apply_normalized_head_rewrites(
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

    struct LinearCoefficient {
        int64_t numerator = 0;
        int64_t denominator = 1;

        void add(int64_t n, int64_t d) {
            const auto [scaled_n, scaled_d] =
                normalize_rational(numerator * d + n * denominator, denominator * d);
            numerator = scaled_n;
            denominator = scaled_d;
        }

        bool is_zero() const {
            return numerator == 0;
        }

        bool is_one() const {
            return numerator == 1 && denominator == 1;
        }

        ExprPtr to_expr() const {
            return denominator == 1
                ? make_number(static_cast<double>(numerator))
                : make_expr<Rational>(numerator, denominator);
        }
    };

    bool try_get_exact_scalar(const ExprPtr& expr, int64_t& numerator, int64_t& denominator) {
        if (const auto* rational = std::get_if<Rational>(expr.get())) {
            numerator = rational->numerator;
            denominator = rational->denominator;
            return true;
        }
        if (const auto* number = std::get_if<Number>(expr.get())) {
            if (std::floor(number->value) != number->value) {
                return false;
            }
            numerator = static_cast<int64_t>(number->value);
            denominator = 1;
            return true;
        }
        return false;
    }

    bool try_extract_exact_linear_term(
        const ExprPtr& expr,
        std::string& symbol,
        int64_t& numerator,
        int64_t& denominator) {
        if (const auto* direct_symbol = std::get_if<Symbol>(expr.get())) {
            symbol = direct_symbol->name;
            numerator = 1;
            denominator = 1;
            return true;
        }

        const auto* call = std::get_if<FunctionCall>(expr.get());
        if (call == nullptr) {
            return false;
        }

        if (call->head == "Times" && call->args.size() == 2) {
            const ExprPtr* coefficient_expr = nullptr;
            const ExprPtr* symbol_expr = nullptr;
            if (std::holds_alternative<Symbol>(*call->args[0])) {
                symbol_expr = &call->args[0];
                coefficient_expr = &call->args[1];
            } else if (std::holds_alternative<Symbol>(*call->args[1])) {
                symbol_expr = &call->args[1];
                coefficient_expr = &call->args[0];
            } else {
                return false;
            }
            if (!try_get_exact_scalar(*coefficient_expr, numerator, denominator)) {
                return false;
            }
            symbol = std::get<Symbol>(**symbol_expr).name;
            return true;
        }

        if (call->head == "Divide" && call->args.size() == 2 &&
            std::holds_alternative<Symbol>(*call->args[0])) {
            int64_t denom_numerator = 0;
            int64_t denom_denominator = 1;
            if (!try_get_exact_scalar(call->args[1], denom_numerator, denom_denominator) ||
                denom_numerator == 0) {
                return false;
            }
            const auto [n, d] = normalize_rational(denom_denominator, denom_numerator);
            symbol = std::get<Symbol>(*call->args[0]).name;
            numerator = n;
            denominator = d;
            return true;
        }

        return false;
    }

    ExprPtr make_symbol_term(const std::string& symbol, const LinearCoefficient& coefficient) {
        if (coefficient.is_zero()) {
            return make_number(0);
        }
        if (coefficient.is_one()) {
            return make_expr<Symbol>(symbol);
        }
        if (coefficient.denominator != 1) {
            return make_times(coefficient.to_expr(), make_expr<Symbol>(symbol));
        }
        return make_times(coefficient.to_expr(), make_expr<Symbol>(symbol));
    }

    ExprPtr make_symbol_term(const std::string& symbol, double coefficient) {
        if (coefficient == 0.0) {
            return make_number(0);
        }
        if (coefficient == 1.0) {
            return make_expr<Symbol>(symbol);
        }
        return make_times(make_number(coefficient), make_expr<Symbol>(symbol));
    }

    }  // namespace

    ExprPtr simplify_relational(const std::string& head, const ExprPtr& left, const ExprPtr& right) {
        auto simplified_left = simplify(left);
        auto simplified_right = simplify(right);

        // If both sides are numbers, evaluate the relational operation
        if (std::holds_alternative<Number>(*simplified_left) && std::holds_alternative<Number>(*simplified_right)) {
            double left_value = get_number_value(simplified_left);
            double right_value = get_number_value(simplified_right);

            if (head == "Equal") {
                return make_expr<Symbol>(left_value == right_value ? "True" : "False");
            }
            else if (head == "NotEqual") {
                return make_expr<Symbol>(left_value != right_value ? "True" : "False");
            }
            else if (head == "Less") {
                return make_expr<Symbol>(left_value < right_value ? "True" : "False");
            }
            else if (head == "Greater") {
                return make_expr<Symbol>(left_value > right_value ? "True" : "False");
            }
            else if (head == "LessEqual") {
                return make_expr<Symbol>(left_value <= right_value ? "True" : "False");
            }
            else if (head == "GreaterEqual") {
                return make_expr<Symbol>(left_value >= right_value ? "True" : "False");
            }
        }

        // Otherwise, return the simplified function call
        return make_fcall(head, { simplified_left, simplified_right });
    }

    // Simplify trivial cases
    ExprPtr simplify(const ExprPtr& expr) {
        if (auto f = std::get_if<FunctionCall>(expr.get())) {
            if (f->head == "Minus" && f->args.size() == 2) {
                return simplify(make_fcall(
                    "Plus",
                    {f->args[0], make_fcall("Times", {make_expr<Number>(-1.0), f->args[1]})}));
            }

            if (f->head == "Times") {
                std::vector<ExprPtr> simplified_args;
                simplified_args.reserve(f->args.size());
                for (const auto& arg : f->args) {
                    simplified_args.push_back(simplify(arg));
                }

                EvaluationContext ctx;
                return apply_normalized_head_rewrites(
                    normalize_expr(make_fcall("Times", simplified_args)),
                    ctx);
            }

            if (f->head == "Power") {
                auto base = simplify(f->args[0]);
                auto exponent = simplify(f->args[1]);

                // Simplify 1^n → 1
                if (is_one(base)) {
                    return make_number(1);
                }

                // Simplify x^1 → x
                if (auto num = std::get_if<Number>(exponent.get())) {
                    if (num->value == 1.0) {
                        return base;
                    }
                }

                // Simplify n^m where n and m are numbers
                if (auto base_num = std::get_if<Number>(base.get())) {
                    if (auto exp_num = std::get_if<Number>(exponent.get())) {
                        if (base_num->value == 0.0 && exp_num->value == 0.0) {
                            return make_expr<FunctionCall>("Power", std::vector<ExprPtr>{base, exponent});
                        }
                        if (base_num->value == 0.0 && exp_num->value < 0.0) {
                            return make_expr<FunctionCall>("Power", std::vector<ExprPtr>{base, exponent});
                        }
                        return make_number(std::pow(base_num->value, exp_num->value));
                    }
                }

                // Keep product powers structural unless the exponent is a safe positive integer.
                int integer_exponent = 0;
                if (auto base_func = std::get_if<FunctionCall>(base.get())) {
                    if (base_func->head == "Times" &&
                        try_get_exact_integer(exponent, integer_exponent) &&
                        integer_exponent > 1) {
                        std::vector<ExprPtr> expanded_terms;
                        for (const auto& term : base_func->args) {
                            expanded_terms.push_back(make_pow(term, integer_exponent));
                        }
                        return simplify(make_expr<FunctionCall>("Times", expanded_terms));
                    }
                }

                EvaluationContext ctx;
                return apply_normalized_head_rewrites(
                    normalize_expr(make_fcall("Power", {base, exponent})),
                    ctx);
            }

            if (f->head == "Plus") {
                std::vector<ExprPtr> simplified_args;
                for (const auto& arg : f->args) {
                    auto simplified_arg = simplify(arg);
                    if (auto plus = std::get_if<FunctionCall>(simplified_arg.get()); plus && plus->head == "Plus") {
                        simplified_args.insert(simplified_args.end(), plus->args.begin(), plus->args.end());
                    } else {
                        simplified_args.push_back(simplified_arg);
                    }
                }

                // Combine supported single-symbol terms without losing exact rational coefficients.
                std::map<std::string, LinearCoefficient> exact_term_coefficients;
                std::map<std::string, double> term_coefficients;
                std::vector<ExprPtr> non_numeric_terms;

                for (const auto& arg : simplified_args) {
                    std::string exact_symbol;
                    int64_t exact_numerator = 0;
                    int64_t exact_denominator = 1;
                    if (try_extract_exact_linear_term(
                            arg,
                            exact_symbol,
                            exact_numerator,
                            exact_denominator)) {
                        exact_term_coefficients[exact_symbol].add(
                            exact_numerator,
                            exact_denominator);
                        continue;
                    }

                    if (auto times_func = std::get_if<FunctionCall>(arg.get())) {
                        if (times_func->head == "Times" && times_func->args.size() == 2) {
                            if (auto coeff = std::get_if<Number>(times_func->args[0].get())) {
                                if (auto symbol = std::get_if<Symbol>(times_func->args[1].get())) {
                                    term_coefficients[symbol->name] += coeff->value;
                                    continue;
                                }
                            }
                        }
                    } else if (auto symbol = std::get_if<Symbol>(arg.get())) {
                        term_coefficients[symbol->name] += 1.0;
                        continue;
                    } else if (auto num = std::get_if<Number>(arg.get())) {
                        term_coefficients[""] += num->value; // Use empty string for constants
                        continue;
                    }
                    non_numeric_terms.push_back(arg);
                }

                for (const auto& [symbol, coeff] : exact_term_coefficients) {
                    if (coeff.is_zero()) {
                        continue;
                    }
                    non_numeric_terms.push_back(make_symbol_term(symbol, coeff));
                }

                for (const auto& [symbol, coeff] : term_coefficients) {
                    if (coeff == 0.0) {
                        continue;
                    }
                    if (!symbol.empty()) {
                        non_numeric_terms.push_back(make_symbol_term(symbol, coeff));
                    } else {
                        non_numeric_terms.push_back(make_number(coeff));
                    }
                }

                // Sort terms: symbols first, constants last
                std::sort(non_numeric_terms.begin(), non_numeric_terms.end(), [](const ExprPtr& a, const ExprPtr& b) {
                    auto degree = [](const ExprPtr& term) -> int {
                        if (auto pow = std::get_if<FunctionCall>(term.get())) {
                            if (pow->head == "Power") {
                                if (auto exp = std::get_if<Number>(pow->args[1].get())) {
                                    return static_cast<int>(exp->value);
                                }
                            }
                            if (pow->head == "Times") {
                                for (auto& arg : pow->args) {
                                    if (auto inner_pow = std::get_if<FunctionCall>(arg.get())) {
                                        if (inner_pow->head == "Power") {
                                            if (auto exp = std::get_if<Number>(inner_pow->args[1].get())) {
                                                return static_cast<int>(exp->value);
                                            }
                                        }
                                    }
                                    else if (std::holds_alternative<Symbol>(*arg)) {
                                        return 1; // x is degree 1
                                    }
                                }
                            }
                        }
                        else if (std::holds_alternative<Symbol>(*term)) {
                            return 1;
                        }
                        else if (std::holds_alternative<Number>(*term)) {
                            return 0;
                        }
                        return -1; // unknown terms last
                        };

                    int da = degree(a);
                    int db = degree(b);
                    if (da != db) return da > db;

                    return to_string(a) < to_string(b); // tie-breaker: lex order
                });

                if (non_numeric_terms.empty()) {
                    return make_number(0);
                }
                if (non_numeric_terms.size() == 1) {
                    return non_numeric_terms[0];
                }
                auto normalized_plus = normalize_expr(make_expr<FunctionCall>("Plus", non_numeric_terms));
                if (const auto* normalized_call = std::get_if<FunctionCall>(normalized_plus.get())) {
                    EvaluationContext ctx;
                    if (auto rewritten =
                            kernel::rewrite_normalized_symbolic_coefficient_head(*normalized_call, ctx)) {
                        return *rewritten;
                    }
                }
                return normalized_plus;
            }

            if (f->head == "Equal" || f->head == "NotEqual" || f->head == "Less" ||
                f->head == "Greater" || f->head == "LessEqual" || f->head == "GreaterEqual") {
                return simplify_relational(f->head, f->args[0], f->args[1]);
            }
        }
        return expr; // Return the original expression if no simplification is possible
    }

    // Recursive expand
    ExprPtr expand(const ExprPtr& expr) {
        if (auto f = std::get_if<FunctionCall>(expr.get())) {
            std::vector<ExprPtr> new_args;
            for (auto& arg : f->args) {
                new_args.push_back(expand(arg)); // Recursively expand arguments
            }

            if (f->head == "Times" && new_args.size() == 2) {
                auto lhs = new_args[0];
                auto rhs = new_args[1];

                // Expand: (a + b) * (c + d)
                if (is_function(lhs, "Plus") && is_function(rhs, "Plus")) {
                    const auto& lhs_func = std::get<FunctionCall>(*lhs);
                    const auto& rhs_func = std::get<FunctionCall>(*rhs);

                    std::vector<ExprPtr> distributed_terms;
                    for (const auto& lhs_arg : lhs_func.args) {
                        for (const auto& rhs_arg : rhs_func.args) {
                            distributed_terms.push_back(simplify(make_times(lhs_arg, rhs_arg)));
                        }
                    }
                    return simplify(make_plus(distributed_terms));
                }

                // Expand: (a + b) * c
                if (is_function(lhs, "Plus")) {
                    const auto& lhs_func = std::get<FunctionCall>(*lhs);
                    return simplify(make_plus({
                        make_times(lhs_func.args[0], rhs),
                        make_times(lhs_func.args[1], rhs)
                        }));
                }

                // Expand: a * (b + c)
                if (is_function(rhs, "Plus")) {
                    const auto& rhs_func = std::get<FunctionCall>(*rhs);
                    return simplify(make_plus({
                        make_times(lhs, rhs_func.args[0]),
                        make_times(lhs, rhs_func.args[1])
                        }));
                }

                // No expansion possible
                return simplify(make_expr<FunctionCall>("Times", new_args));
            }

            if (f->head == "Power" && new_args.size() == 2) {
                auto base = new_args[0];
                int exp = 0;
                if (!try_get_exact_integer(new_args[1], exp)) {
                    // Return unevaluated if exponent is not an exact integer.
                    return make_expr<FunctionCall>("Power", new_args);
                }

                // Check for (a + b)^2 pattern
                if (const auto* base_func = std::get_if<FunctionCall>(base.get())) {
                    if (base_func->head == "Plus" && base_func->args.size() == 2 && exp == 2) {
                        auto a = base_func->args[0];
                        auto b = base_func->args[1];

                        return simplify(make_plus({
                            make_pow(a, 2),                           // a^2
                            make_times({make_number(2), a, b}),       // 2 * a * b
                            make_pow(b, 2)                            // b^2
                            }));
                    }
                }

                // No special expansion case
                return simplify(make_expr<FunctionCall>("Power", new_args));
            }

            return simplify(make_expr<FunctionCall>(f->head, new_args));
        }

        return expr; // Base case: atom
    }


}
