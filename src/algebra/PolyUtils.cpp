#include "algebra/PolyUtils.hpp"
#include "algebra/Polynomial.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"
#include <utility>
#include <set>
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <optional>
#include <tuple>

namespace aleph3 {

    namespace {

    constexpr double EPSILON = 1e-10;

    bool contains_variable(
        const std::vector<std::string>& variables,
        const std::string& name) {
        return std::find(variables.begin(), variables.end(), name) != variables.end();
    }

    int total_degree(const Monomial& mono) {
        int degree = 0;
        for (const auto& [_, exponent] : mono) {
            degree += exponent;
        }
        return degree;
    }

    bool is_near_zero(double value) {
        return std::abs(value) < EPSILON;
    }

    bool is_near_integer(double value) {
        return std::abs(value - std::round(value)) < EPSILON;
    }

    long long rounded_integer(double value) {
        return static_cast<long long>(std::llround(value));
    }

    struct ExactCoefficient {
        int64_t numerator = 0;
        int64_t denominator = 1;

        ExactCoefficient() = default;

        ExactCoefficient(int64_t num, int64_t den) {
            std::tie(numerator, denominator) = normalize_rational(num, den);
        }

        static ExactCoefficient zero() {
            return ExactCoefficient(0, 1);
        }

        static ExactCoefficient one() {
            return ExactCoefficient(1, 1);
        }

        bool is_zero() const {
            return numerator == 0;
        }

        bool is_one() const {
            return numerator == denominator;
        }
    };

    ExactCoefficient operator+(const ExactCoefficient& left, const ExactCoefficient& right) {
        return ExactCoefficient(
            left.numerator * right.denominator + right.numerator * left.denominator,
            left.denominator * right.denominator);
    }

    ExactCoefficient operator-(const ExactCoefficient& left, const ExactCoefficient& right) {
        return ExactCoefficient(
            left.numerator * right.denominator - right.numerator * left.denominator,
            left.denominator * right.denominator);
    }

    ExactCoefficient operator*(const ExactCoefficient& left, const ExactCoefficient& right) {
        return ExactCoefficient(
            left.numerator * right.numerator,
            left.denominator * right.denominator);
    }

    ExactCoefficient operator/(const ExactCoefficient& left, const ExactCoefficient& right) {
        if (right.numerator == 0) {
            throw std::domain_error("Polynomial division by zero");
        }
        return ExactCoefficient(
            left.numerator * right.denominator,
            left.denominator * right.numerator);
    }

    struct ExactPolynomial {
        std::map<Monomial, ExactCoefficient> terms;

        ExactPolynomial() : terms{{Monomial{}, ExactCoefficient::zero()}} {}

        explicit ExactPolynomial(const ExactCoefficient& constant) {
            terms[Monomial{}] = constant;
            normalize();
        }

        explicit ExactPolynomial(const std::map<Monomial, ExactCoefficient>& input_terms)
            : terms(input_terms) {
            normalize();
        }

        void normalize() {
            for (auto it = terms.begin(); it != terms.end();) {
                if (it->second.is_zero()) {
                    it = terms.erase(it);
                } else {
                    ++it;
                }
            }
            if (terms.empty()) {
                terms[Monomial{}] = ExactCoefficient::zero();
            }
        }

        bool is_zero() const {
            return terms.size() == 1
                && terms.begin()->first.empty()
                && terms.begin()->second.is_zero();
        }

        size_t degree() const {
            size_t max_degree = 0;
            for (const auto& [mono, coeff] : terms) {
                if (coeff.is_zero()) continue;
                size_t degree = 0;
                for (const auto& [_, exponent] : mono) {
                    degree += static_cast<size_t>(exponent);
                }
                max_degree = std::max(max_degree, degree);
            }
            return max_degree;
        }
    };

    ExactPolynomial operator+(const ExactPolynomial& left, const ExactPolynomial& right) {
        ExactPolynomial result = left;
        for (const auto& [mono, coeff] : right.terms) {
            result.terms[mono] = result.terms[mono] + coeff;
        }
        result.normalize();
        return result;
    }

    ExactPolynomial operator-(const ExactPolynomial& left, const ExactPolynomial& right) {
        ExactPolynomial result = left;
        for (const auto& [mono, coeff] : right.terms) {
            result.terms[mono] = result.terms[mono] - coeff;
        }
        result.normalize();
        return result;
    }

    ExactPolynomial operator*(const ExactPolynomial& left, const ExactPolynomial& right) {
        ExactPolynomial result;
        result.terms.clear();
        for (const auto& [left_mono, left_coeff] : left.terms) {
            if (left_coeff.is_zero()) continue;
            for (const auto& [right_mono, right_coeff] : right.terms) {
                if (right_coeff.is_zero()) continue;
                Monomial mono = left_mono;
                for (const auto& [var, exponent] : right_mono) {
                    mono[var] += exponent;
                }
                result.terms[mono] = result.terms[mono] + (left_coeff * right_coeff);
            }
        }
        result.normalize();
        return result;
    }

    struct RationalRoot {
        int64_t numerator = 0;
        int64_t denominator = 1;
    };

    std::vector<std::string> infer_variables(const ExprPtr& expr) {
        std::set<std::string> vars;
        std::function<void(const ExprPtr&)> visit = [&](const ExprPtr& e) {
            if (!e) return;
            if (auto sym = std::get_if<Symbol>(&(*e))) {
                vars.insert(sym->name);
            }
            else if (auto func = std::get_if<FunctionCall>(&(*e))) {
                for (const auto& arg : func->args) {
                    visit(arg);
                }
            }
        };
        visit(expr);
        return {vars.begin(), vars.end()};
    }

    std::vector<std::string> infer_variables(const Polynomial& poly) {
        std::set<std::string> vars;
        for (const auto& [mono, coeff] : poly.terms) {
            if (is_near_zero(coeff)) continue;
            for (const auto& [var, exponent] : mono) {
                if (exponent != 0) {
                    vars.insert(var);
                }
            }
        }
        return {vars.begin(), vars.end()};
    }

    std::vector<std::string> infer_variables(const ExactPolynomial& poly) {
        std::set<std::string> vars;
        for (const auto& [mono, coeff] : poly.terms) {
            if (coeff.is_zero()) continue;
            for (const auto& [var, exponent] : mono) {
                if (exponent != 0) {
                    vars.insert(var);
                }
            }
        }
        return {vars.begin(), vars.end()};
    }

    Polynomial make_polynomial_term(double coefficient, const Monomial& mono) {
        return Polynomial({{mono, coefficient}});
    }

    ExprPtr monomial_to_expr(double coefficient, const Monomial& mono) {
        return polynomial_to_expr(make_polynomial_term(coefficient, mono));
    }

    bool monomial_precedes(const Monomial& left, const Monomial& right) {
        const int left_degree = total_degree(left);
        const int right_degree = total_degree(right);
        if (left_degree != right_degree) {
            return left_degree > right_degree;
        }
        return left < right;
    }

    bool is_constant_one(const Polynomial& poly) {
        return poly.terms.size() == 1
            && poly.terms.begin()->first.empty()
            && std::abs(poly.terms.begin()->second - 1.0) < EPSILON;
    }

    bool is_constant_zero(const Polynomial& poly) {
        return poly.terms.size() == 1
            && poly.terms.begin()->first.empty()
            && is_near_zero(poly.terms.begin()->second);
    }

    bool is_exact_polynomial_candidate(const ExprPtr& expr) {
        if (!expr) return false;
        if (const auto* number = std::get_if<Number>(&(*expr))) {
            return is_near_integer(number->value);
        }
        if (std::holds_alternative<Rational>(*expr)) {
            return true;
        }
        if (const auto* call = std::get_if<FunctionCall>(&(*expr))) {
            for (const auto& arg : call->args) {
                if (!is_exact_polynomial_candidate(arg)) {
                    return false;
                }
            }
        }
        return true;
    }

    struct PolynomialContent {
        double coefficient = 1.0;
        Monomial monomial;
        Polynomial primitive;
    };

    PolynomialContent split_content(const Polynomial& poly) {
        PolynomialContent content;
        content.primitive = poly;

        if (is_constant_zero(poly)) {
            content.coefficient = 0.0;
            return content;
        }

        bool all_integer_coefficients = true;
        long long coefficient_gcd = 0;
        bool first_term = true;

        for (const auto& [mono, coeff] : poly.terms) {
            if (is_near_zero(coeff)) continue;

            if (first_term) {
                content.monomial = mono;
                first_term = false;
            } else {
                for (auto it = content.monomial.begin(); it != content.monomial.end(); ) {
                    const auto mono_it = mono.find(it->first);
                    if (mono_it == mono.end()) {
                        it = content.monomial.erase(it);
                    } else {
                        it->second = std::min(it->second, mono_it->second);
                        if (it->second == 0) {
                            it = content.monomial.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }

            if (!is_near_integer(coeff)) {
                all_integer_coefficients = false;
            } else {
                const long long abs_coeff = std::llabs(rounded_integer(coeff));
                coefficient_gcd = coefficient_gcd == 0 ? abs_coeff : std::gcd(coefficient_gcd, abs_coeff);
            }
        }

        if (first_term) {
            return content;
        }

        if (!all_integer_coefficients || coefficient_gcd == 0) {
            coefficient_gcd = 1;
        }

        bool leading_negative = false;
        for (const auto& [_, coeff] : poly.terms) {
            if (!is_near_zero(coeff)) {
                leading_negative = coeff < 0.0;
                break;
            }
        }
        content.coefficient = static_cast<double>(coefficient_gcd) * (leading_negative ? -1.0 : 1.0);

        Polynomial primitive;
        for (const auto& [mono, coeff] : poly.terms) {
            Monomial reduced = mono;
            for (const auto& [var, exponent] : content.monomial) {
                auto reduced_it = reduced.find(var);
                if (reduced_it != reduced.end()) {
                    reduced_it->second -= exponent;
                    if (reduced_it->second == 0) {
                        reduced.erase(reduced_it);
                    }
                }
            }
            primitive.terms[reduced] += coeff / content.coefficient;
        }
        primitive.normalize();

        auto leading_it = primitive.terms.end();
        for (auto it = primitive.terms.begin(); it != primitive.terms.end(); ++it) {
            if (is_near_zero(it->second)) continue;
            if (leading_it == primitive.terms.end()
                || monomial_precedes(it->first, leading_it->first)) {
                leading_it = it;
            }
        }
        if (leading_it != primitive.terms.end() && leading_it->second < 0.0) {
            for (auto& [_, coeff] : primitive.terms) {
                coeff = -coeff;
            }
            primitive.normalize();
            content.coefficient = -content.coefficient;
        }

        content.primitive = primitive;
        return content;
    }

    bool is_univariate_in(const Polynomial& poly, const std::string& var) {
        for (const auto& [mono, coeff] : poly.terms) {
            if (is_near_zero(coeff)) continue;
            for (const auto& [mono_var, exponent] : mono) {
                if (mono_var != var && exponent != 0) {
                    return false;
                }
            }
        }
        return true;
    }

    bool is_univariate_in(const ExactPolynomial& poly, const std::string& var) {
        for (const auto& [mono, coeff] : poly.terms) {
            if (coeff.is_zero()) continue;
            for (const auto& [mono_var, exponent] : mono) {
                if (mono_var != var && exponent != 0) {
                    return false;
                }
            }
        }
        return true;
    }

    int degree_in_variable(const Polynomial& poly, const std::string& var) {
        int degree = 0;
        for (const auto& [mono, coeff] : poly.terms) {
            if (is_near_zero(coeff)) continue;
            auto it = mono.find(var);
            if (it != mono.end()) {
                degree = std::max(degree, it->second);
            }
        }
        return degree;
    }

    int degree_in_variable(const ExactPolynomial& poly, const std::string& var) {
        int degree = 0;
        for (const auto& [mono, coeff] : poly.terms) {
            if (coeff.is_zero()) continue;
            auto it = mono.find(var);
            if (it != mono.end()) {
                degree = std::max(degree, it->second);
            }
        }
        return degree;
    }

    std::pair<Monomial, ExactCoefficient> leading_term(
        const ExactPolynomial& poly,
        const std::string& var) {
        Monomial lead_monomial;
        ExactCoefficient lead_coefficient = ExactCoefficient::zero();
        int max_degree = -1;
        for (const auto& [mono, coeff] : poly.terms) {
            if (coeff.is_zero()) continue;
            int degree = 0;
            auto it = mono.find(var);
            if (it != mono.end()) {
                degree = it->second;
            }
            if (degree > max_degree) {
                max_degree = degree;
                lead_monomial = mono;
                lead_coefficient = coeff;
            }
        }
        return {lead_monomial, lead_coefficient};
    }

    ExactCoefficient leading_coefficient(
        const ExactPolynomial& poly,
        const std::string& var) {
        return leading_term(poly, var).second;
    }

    std::pair<ExactPolynomial, ExactPolynomial> divide_exact(
        const ExactPolynomial& dividend,
        const ExactPolynomial& divisor,
        const std::vector<std::string>& variables) {
        if (variables.size() != 1) {
            throw_unsupported_construct("divide: only univariate division is implemented");
        }

        const std::string& var = variables[0];
        if (!is_univariate_in(dividend, var) || !is_univariate_in(divisor, var)) {
            throw_unsupported_construct("divide: only univariate division is implemented");
        }
        if (divisor.is_zero()) {
            throw std::domain_error("Polynomial division by zero");
        }

        const int divisor_degree = degree_in_variable(divisor, var);
        ExactPolynomial remainder = dividend;
        ExactPolynomial quotient;

        while (!remainder.is_zero() && degree_in_variable(remainder, var) >= divisor_degree) {
            const auto [remainder_mono, remainder_coeff] = leading_term(remainder, var);
            const auto [divisor_mono, divisor_coeff] = leading_term(divisor, var);

            Monomial quotient_mono = remainder_mono;
            for (const auto& [mono_var, exponent] : divisor_mono) {
                quotient_mono[mono_var] -= exponent;
                if (quotient_mono[mono_var] == 0) {
                    quotient_mono.erase(mono_var);
                }
            }

            ExactPolynomial quotient_term({
                {quotient_mono, remainder_coeff / divisor_coeff}
            });
            quotient = quotient + quotient_term;
            remainder = remainder - (divisor * quotient_term);
        }

        quotient.normalize();
        remainder.normalize();
        return {quotient, remainder};
    }

    ExactPolynomial gcd_exact(
        const ExactPolynomial& left,
        const ExactPolynomial& right,
        const std::vector<std::string>& variables) {
        if (variables.size() != 1) {
            throw_unsupported_construct("gcd: only univariate GCD is implemented");
        }

        const std::string& var = variables[0];
        if (!is_univariate_in(left, var) || !is_univariate_in(right, var)) {
            throw_unsupported_construct("gcd: only univariate GCD is implemented");
        }

        ExactPolynomial a = left;
        ExactPolynomial b = right;
        while (!b.is_zero()) {
            auto [_, remainder] = divide_exact(a, b, variables);
            a = b;
            b = remainder;
        }

        if (!a.is_zero()) {
            const auto lead = leading_coefficient(a, var);
            for (auto& [_, coeff] : a.terms) {
                coeff = coeff / lead;
            }
            a.normalize();
        }

        return a;
    }

    ExprPtr exact_coefficient_to_expr(const ExactCoefficient& coefficient) {
        if (coefficient.denominator == 1) {
            return make_expr<Number>(static_cast<double>(coefficient.numerator));
        }
        return make_expr<Rational>(coefficient.numerator, coefficient.denominator);
    }

    ExactPolynomial expr_to_exact_polynomial(
        const ExprPtr& expr,
        const std::vector<std::string>& variables) {
        if (!expr) throw_internal_inconsistency("Null expression");

        auto make_monomial = [&](const std::map<std::string, int>& exponents) -> Monomial {
            Monomial mono;
            for (const auto& variable : variables) {
                auto it = exponents.find(variable);
                if (it != exponents.end() && it->second != 0) {
                    mono[variable] = it->second;
                }
            }
            return mono;
        };

        std::function<ExactPolynomial(const ExprPtr&)> recur =
            [&](const ExprPtr& current) -> ExactPolynomial {
                if (const auto* number = std::get_if<Number>(&(*current))) {
                    if (!is_near_integer(number->value)) {
                        throw_unsupported_construct(
                            "Exact polynomial conversion does not accept inexact numeric coefficients");
                    }
                    return ExactPolynomial(
                        ExactCoefficient(rounded_integer(number->value), 1));
                }
                if (const auto* rational = std::get_if<Rational>(&(*current))) {
                    return ExactPolynomial(
                        ExactCoefficient(rational->numerator, rational->denominator));
                }
                if (const auto* symbol = std::get_if<Symbol>(&(*current))) {
                    if (!contains_variable(variables, symbol->name)) {
                        throw_invalid_form(
                            "expr_to_polynomial: Symbol `" + symbol->name +
                            "` is not in the selected polynomial variable set");
                    }
                    std::map<std::string, int> exponents;
                    exponents[symbol->name] = 1;
                    return ExactPolynomial({
                        {make_monomial(exponents), ExactCoefficient::one()}
                    });
                }
                if (const auto* plus = std::get_if<FunctionCall>(&(*current));
                    plus && plus->head == "Plus") {
                    ExactPolynomial result;
                    for (const auto& arg : plus->args) {
                        result = result + recur(arg);
                    }
                    return result;
                }
                if (const auto* minus = std::get_if<FunctionCall>(&(*current));
                    minus && minus->head == "Minus" && minus->args.size() == 2) {
                    return recur(minus->args[0]) - recur(minus->args[1]);
                }
                if (const auto* negate = std::get_if<FunctionCall>(&(*current));
                    negate && negate->head == "Negate" && negate->args.size() == 1) {
                    return ExactPolynomial(ExactCoefficient::zero()) - recur(negate->args[0]);
                }
                if (const auto* times = std::get_if<FunctionCall>(&(*current));
                    times && times->head == "Times") {
                    ExactPolynomial result(ExactCoefficient::one());
                    for (const auto& arg : times->args) {
                        result = result * recur(arg);
                    }
                    return result;
                }
                if (const auto* power = std::get_if<FunctionCall>(&(*current));
                    power && power->head == "Power" && power->args.size() == 2) {
                    const auto& base = power->args[0];
                    const auto& exponent = power->args[1];
                    if (const auto* symbol = std::get_if<Symbol>(&(*base))) {
                        if (!contains_variable(variables, symbol->name)) {
                            throw_invalid_form(
                                "expr_to_polynomial: Symbol `" + symbol->name +
                                "` is not in the selected polynomial variable set");
                        }
                        if (const auto* number_exponent = std::get_if<Number>(&(*exponent))) {
                            if (!is_near_integer(number_exponent->value)
                                || number_exponent->value < 0.0) {
                                throw_invalid_form(
                                    "expr_to_polynomial: Polynomial powers require non-negative integer exponents");
                            }
                            std::map<std::string, int> exponents;
                            exponents[symbol->name] = static_cast<int>(number_exponent->value);
                            return ExactPolynomial({
                                {make_monomial(exponents), ExactCoefficient::one()}
                            });
                        }
                    }
                }
                throw_unsupported_construct("expr_to_polynomial: Not implemented for this expression");
            };

        return recur(expr);
    }

    ExprPtr exact_polynomial_to_expr(const ExactPolynomial& poly) {
        std::vector<std::pair<Monomial, ExactCoefficient>> ordered_terms;
        ordered_terms.reserve(poly.terms.size());
        for (const auto& [mono, coeff] : poly.terms) {
            if (coeff.is_zero()) continue;
            ordered_terms.emplace_back(mono, coeff);
        }

        std::sort(
            ordered_terms.begin(),
            ordered_terms.end(),
            [](const auto& left, const auto& right) {
                const int left_degree = total_degree(left.first);
                const int right_degree = total_degree(right.first);
                if (left_degree != right_degree) {
                    return left_degree > right_degree;
                }
                return left.first < right.first;
            });

        std::vector<ExprPtr> terms;
        for (const auto& [mono, coeff] : ordered_terms) {
            if (mono.empty()) {
                terms.push_back(exact_coefficient_to_expr(coeff));
                continue;
            }

            std::vector<ExprPtr> factors;
            if (!coeff.is_one()) {
                factors.push_back(exact_coefficient_to_expr(coeff));
            }
            for (const auto& [var, exponent] : mono) {
                ExprPtr variable = make_expr<Symbol>(var);
                if (exponent == 1) {
                    factors.push_back(variable);
                } else {
                    factors.push_back(
                        make_fcall("Power", {variable, make_expr<Number>(static_cast<double>(exponent))}));
                }
            }
            ExprPtr term = factors.size() == 1 ? factors.front() : make_times(factors);
            terms.push_back(term);
        }

        if (terms.empty()) return make_expr<Number>(0.0);
        if (terms.size() == 1) return terms[0];
        return make_expr<FunctionCall>("Plus", terms);
    }

    std::vector<long long> univariate_coefficients(const Polynomial& poly, const std::string& var) {
        const int degree = degree_in_variable(poly, var);
        std::vector<long long> coefficients(static_cast<size_t>(degree) + 1, 0);
        for (const auto& [mono, coeff] : poly.terms) {
            if (!is_near_integer(coeff)) {
                throw_unsupported_construct("Polynomial factorization currently requires integer coefficients");
            }
            int exponent = 0;
            auto it = mono.find(var);
            if (it != mono.end()) {
                exponent = it->second;
            }
            coefficients[static_cast<size_t>(degree - exponent)] = rounded_integer(coeff);
        }
        return coefficients;
    }

    std::vector<long long> positive_divisors(long long value) {
        value = std::llabs(value);
        if (value == 0) {
            return {0};
        }
        std::vector<long long> divisors;
        for (long long candidate = 1; candidate <= value; ++candidate) {
            if (value % candidate == 0) {
                divisors.push_back(candidate);
            }
        }
        return divisors;
    }

    std::vector<RationalRoot> rational_root_candidates(const std::vector<long long>& coefficients) {
        if (coefficients.empty()) return {};

        const long long leading = coefficients.front();
        const long long constant = coefficients.back();
        if (leading == 0) return {};

        std::vector<RationalRoot> candidates;
        std::set<std::pair<int64_t, int64_t>> seen;
        for (const auto numerator : positive_divisors(constant)) {
            for (const auto denominator : positive_divisors(leading)) {
                if (denominator == 0) continue;
                auto [positive_n, positive_d] = normalize_rational(numerator, denominator);
                auto [negative_n, negative_d] = normalize_rational(-numerator, denominator);
                if (seen.insert({positive_n, positive_d}).second) {
                    candidates.push_back(RationalRoot{positive_n, positive_d});
                }
                if (seen.insert({negative_n, negative_d}).second) {
                    candidates.push_back(RationalRoot{negative_n, negative_d});
                }
            }
        }
        return candidates;
    }

    double evaluate_univariate_coefficients(
        const std::vector<long long>& coefficients,
        const RationalRoot& root) {
        const double x =
            static_cast<double>(root.numerator) / static_cast<double>(root.denominator);
        double value = 0.0;
        for (const auto coefficient : coefficients) {
            value = value * x + static_cast<double>(coefficient);
        }
        return value;
    }

    std::optional<std::vector<long long>> synthetic_divide(
        const std::vector<long long>& coefficients,
        const RationalRoot& root) {
        const double root_value =
            static_cast<double>(root.numerator) / static_cast<double>(root.denominator);
        std::vector<double> quotient(coefficients.size() - 1, 0.0);
        double carry = static_cast<double>(coefficients.front());
        quotient.front() = carry;
        for (size_t i = 1; i + 1 < coefficients.size(); ++i) {
            carry = carry * root_value + static_cast<double>(coefficients[i]);
            quotient[i] = carry;
        }
        const double remainder =
            carry * root_value + static_cast<double>(coefficients.back());
        if (!is_near_zero(remainder)) {
            return std::nullopt;
        }

        std::vector<long long> quotient_ints;
        quotient_ints.reserve(quotient.size());
        for (const auto value : quotient) {
            if (!is_near_integer(value)) {
                return std::nullopt;
            }
            quotient_ints.push_back(rounded_integer(value));
        }
        return quotient_ints;
    }

    Polynomial coefficients_to_polynomial(
        const std::vector<long long>& coefficients,
        const std::string& var) {
        const int degree = static_cast<int>(coefficients.size()) - 1;
        Polynomial poly;
        for (size_t i = 0; i < coefficients.size(); ++i) {
            const auto coeff = coefficients[i];
            if (coeff == 0) continue;
            const int exponent = degree - static_cast<int>(i);
            Monomial mono;
            if (exponent > 0) {
                mono[var] = exponent;
            }
            poly.terms[mono] += static_cast<double>(coeff);
        }
        poly.normalize();
        return poly;
    }

    ExprPtr monic_linear_factor_expr(
        const std::string& var,
        const RationalRoot& root) {
        auto variable_term = make_expr<Symbol>(var);
        if (root.denominator == 1) {
            if (root.numerator < 0) {
                return make_plus(
                    variable_term,
                    make_expr<Number>(static_cast<double>(std::abs(root.numerator))));
            }
            return make_expr<FunctionCall>(
                "Minus",
                std::vector<ExprPtr>{
                    variable_term,
                    make_expr<Number>(static_cast<double>(root.numerator))
                });
        }

        ExprPtr constant_term = make_expr<Rational>(std::abs(root.numerator), root.denominator);
        if (root.numerator < 0) {
            return make_plus(variable_term, constant_term);
        }
        return make_expr<FunctionCall>(
            "Minus",
            std::vector<ExprPtr>{variable_term, constant_term});
    }

    ExprPtr linear_polynomial_to_expr(
        double coefficient,
        const std::string& var,
        const RationalRoot& root) {
        if (is_near_zero(coefficient - 1.0)) {
            return monic_linear_factor_expr(var, root);
        }

        auto variable_term = make_times({
            make_expr<Number>(coefficient),
            make_expr<Symbol>(var)
        });

        ExprPtr constant_term = nullptr;
        const double constant_value =
            coefficient * static_cast<double>(root.numerator)
            / static_cast<double>(root.denominator);

        if (is_near_integer(constant_value)) {
            constant_term =
                make_expr<Number>(static_cast<double>(std::abs(rounded_integer(constant_value))));
        } else {
            const auto scaled_numerator =
                static_cast<int64_t>(std::llround(coefficient * root.numerator));
            auto [n, d] = normalize_rational(std::llabs(scaled_numerator), root.denominator);
            constant_term = make_expr<Rational>(n, d);
        }

        if (root.numerator < 0) {
            return make_plus(variable_term, constant_term);
        }
        return make_expr<FunctionCall>(
            "Minus",
            std::vector<ExprPtr>{variable_term, constant_term});
    }

    ExprPtr polynomial_linear_expr(
        const Polynomial& poly,
        const std::string& var) {
        double variable_coefficient = 0.0;
        double constant_term = 0.0;

        for (const auto& [mono, coeff] : poly.terms) {
            if (is_near_zero(coeff)) continue;

            if (mono.empty()) {
                constant_term += coeff;
                continue;
            }

            auto it = mono.find(var);
            if (it != mono.end() && it->second == 1 && mono.size() == 1) {
                variable_coefficient += coeff;
                continue;
            }

            return polynomial_to_expr(poly);
        }

        if (is_near_zero(variable_coefficient)) {
            return make_expr<Number>(constant_term);
        }

        auto scaled_numerator = rounded_integer(-constant_term);
        auto scaled_denominator = rounded_integer(variable_coefficient);
        auto [n, d] = normalize_rational(scaled_numerator, scaled_denominator);
        return linear_polynomial_to_expr(variable_coefficient, var, RationalRoot{n, d});
    }

    std::vector<ExprPtr> factor_univariate_polynomial(
        const Polynomial& poly,
        const std::string& var) {
        std::vector<std::pair<RationalRoot, ExprPtr>> linear_factors;
        std::vector<ExprPtr> factors;
        if (poly.degree() <= 1) {
            factors.push_back(polynomial_linear_expr(poly, var));
            return factors;
        }

        auto coefficients = univariate_coefficients(poly, var);
        while (coefficients.size() > 2) {
            bool found_root = false;
            for (const auto candidate : rational_root_candidates(coefficients)) {
                if (!is_near_zero(evaluate_univariate_coefficients(coefficients, candidate))) {
                    continue;
                }
                const auto quotient = synthetic_divide(coefficients, candidate);
                if (!quotient.has_value()) {
                    continue;
                }

                linear_factors.emplace_back(
                    candidate,
                    monic_linear_factor_expr(var, candidate));
                coefficients = *quotient;
                found_root = true;
                break;
            }
            if (!found_root) {
                break;
            }
        }

        const auto remainder = coefficients_to_polynomial(coefficients, var);
        if (remainder.degree() == 1) {
            double constant_term = 0.0;
            auto it = remainder.terms.find(Monomial{});
            if (it != remainder.terms.end()) {
                constant_term = it->second;
            }
            double variable_coefficient = 0.0;
            auto variable_it = remainder.terms.find(Monomial{{var, 1}});
            if (variable_it != remainder.terms.end()) {
                variable_coefficient = variable_it->second;
            }
            if (!is_near_zero(variable_coefficient)) {
                auto [n, d] = normalize_rational(
                    rounded_integer(-constant_term),
                    rounded_integer(variable_coefficient));
                linear_factors.emplace_back(
                    RationalRoot{n, d},
                    polynomial_linear_expr(remainder, var));
            } else if (!is_constant_one(remainder)) {
                factors.push_back(polynomial_to_expr(remainder));
            }
        } else if (!is_constant_one(remainder)) {
            factors.push_back(polynomial_to_expr(remainder));
        }

        std::sort(
            linear_factors.begin(),
            linear_factors.end(),
            [](const auto& left, const auto& right) {
                const double left_value =
                    static_cast<double>(left.first.numerator) / left.first.denominator;
                const double right_value =
                    static_cast<double>(right.first.numerator) / right.first.denominator;
                if (!is_near_zero(left_value - right_value)) {
                    return left_value > right_value;
                }
                return to_string(*left.second) < to_string(*right.second);
            });

        for (const auto& [_, factor] : linear_factors) {
            factors.push_back(factor);
        }

        return factors;
    }

    }  // namespace

    // --- Conversion utilities ---

    // Convert ExprPtr to Polynomial (supports multivariate, basic Plus/Times/Power/Number/Symbol)
    Polynomial expr_to_polynomial(const ExprPtr& expr, const std::vector<std::string>& variables) {
        if (!expr) throw_internal_inconsistency("Null expression");

        // Helper: build a monomial from a variable->exponent map
        auto make_monomial = [&](const std::map<std::string, int>& exps) -> Monomial {
            Monomial m;
            for (const auto& v : variables) {
                auto it = exps.find(v);
                if (it != exps.end() && it->second != 0)
                    m[v] = it->second;
            }
            return m;
            };

        // Recursive lambda
        std::function<Polynomial(const ExprPtr&)> recur = [&](const ExprPtr& e) -> Polynomial {
            if (auto num = std::get_if<Number>(&(*e))) {
                return Polynomial(static_cast<double>(num->value));
            }
            if (std::holds_alternative<Rational>(*e)) {
                throw_unsupported_construct(
                    "Polynomial functions do not yet support exact rational coefficients");
            }
            if (auto sym = std::get_if<Symbol>(&(*e))) {
                if (!contains_variable(variables, sym->name)) {
                    throw_invalid_form(
                        "expr_to_polynomial: Symbol `" + sym->name +
                        "` is not in the selected polynomial variable set");
                }
                std::map<std::string, int> exps;
                exps[sym->name] = 1;
                return Polynomial({ {make_monomial(exps), 1.0} });
            }
            if (auto plus = std::get_if<FunctionCall>(&(*e)); plus && plus->head == "Plus") {
                Polynomial result;
                for (const auto& arg : plus->args) {
                    result = result + recur(arg);
                }
                return result;
            }
            if (auto minus = std::get_if<FunctionCall>(&(*e)); minus && minus->head == "Minus") {
                if (minus->args.size() == 2) {
                    return recur(minus->args[0]) - recur(minus->args[1]);
                }
            }
            if (auto negate = std::get_if<FunctionCall>(&(*e)); negate && negate->head == "Negate") {
                if (negate->args.size() == 1) {
                    return Polynomial(0.0) - recur(negate->args[0]);
                }
            }
            if (auto times = std::get_if<FunctionCall>(&(*e)); times && times->head == "Times") {
                Polynomial result(1.0);
                for (const auto& arg : times->args) {
                    result = result * recur(arg);
                }
                return result;
            }
            if (auto pow = std::get_if<FunctionCall>(&(*e)); pow && pow->head == "Power") {
                if (pow->args.size() == 2) {
                    auto base = pow->args[0];
                    auto exp = pow->args[1];
                    if (auto s = std::get_if<Symbol>(&(*base))) {
                        if (auto n = std::get_if<Number>(&(*exp))) {
                            const double exponent = n->value;
                            if (!contains_variable(variables, s->name)) {
                                throw_invalid_form(
                                    "expr_to_polynomial: Symbol `" + s->name +
                                    "` is not in the selected polynomial variable set");
                            }
                            if (std::floor(exponent) != exponent || exponent < 0.0) {
                                throw_invalid_form(
                                    "expr_to_polynomial: Polynomial powers require non-negative integer exponents");
                            }
                            std::map<std::string, int> exps;
                            exps[s->name] = static_cast<int>(exponent);
                            return Polynomial({ {make_monomial(exps), 1.0} });
                        }
                    }
                }
            }
            throw_unsupported_construct("expr_to_polynomial: Not implemented for this expression");
            };

        return recur(expr);
    }

    // Convert Polynomial to ExprPtr (sum of terms, supports multivariate)
    ExprPtr polynomial_to_expr(const Polynomial& poly) {
        std::vector<std::pair<Monomial, double>> ordered_terms;
        ordered_terms.reserve(poly.terms.size());
        for (const auto& [mono, coeff] : poly.terms) {
            if (std::abs(coeff) < 1e-10) continue;
            ordered_terms.emplace_back(mono, coeff);
        }

        std::sort(
            ordered_terms.begin(),
            ordered_terms.end(),
            [](const auto& left, const auto& right) {
                const int left_degree = total_degree(left.first);
                const int right_degree = total_degree(right.first);
                if (left_degree != right_degree) {
                    return left_degree > right_degree;
                }
                return left.first < right.first;
            });

        std::vector<ExprPtr> terms;
        for (const auto& [mono, coeff] : ordered_terms) {
            if (mono.empty()) {
                terms.push_back(make_expr<Number>(coeff));
                continue;
            }

            std::vector<ExprPtr> factors;
            if (coeff != 1.0) {
                factors.push_back(make_expr<Number>(coeff));
            }
            for (const auto& [var, exp] : mono) {
                ExprPtr v = make_expr<Symbol>(var);
                if (exp == 1) {
                    factors.push_back(v);
                }
                else {
                    factors.push_back(
                        make_fcall("Power", {v, make_expr<Number>(static_cast<double>(exp))}));
                }
            }
            ExprPtr term = factors.size() == 1 ? factors.front() : make_times(factors);
            terms.push_back(term);
        }
        if (terms.empty()) return make_expr<Number>(0.0);
        if (terms.size() == 1) return terms[0];
        return make_expr<FunctionCall>("Plus", terms);
    }

    // --- High-level API ---

    ExprPtr expand_polynomial(const ExprPtr& expr, EvaluationContext& ctx) {
        const std::vector<std::string> variables = infer_variables(expr);
        if (is_exact_polynomial_candidate(expr)) {
            return exact_polynomial_to_expr(expr_to_exact_polynomial(expr, variables));
        }
        Polynomial poly = expr_to_polynomial(expr, variables);
        Polynomial expanded = expand(poly);
        return polynomial_to_expr(expanded);
    }

    ExprPtr factor_polynomial(const ExprPtr& expr, EvaluationContext& ctx) {
        const std::vector<std::string> variables = infer_variables(expr);
        Polynomial poly = expr_to_polynomial(expr, variables);
        if (is_constant_zero(poly)) {
            return polynomial_to_expr(poly);
        }

        const auto content = split_content(poly);
        std::vector<ExprPtr> factors;

        if (!is_near_zero(content.coefficient - 1.0)
            || !content.monomial.empty()) {
            factors.push_back(monomial_to_expr(content.coefficient, content.monomial));
        }

        Polynomial remaining = content.primitive;
        const auto remaining_variables = infer_variables(remaining);
        if (remaining_variables.size() == 1
            && is_univariate_in(remaining, remaining_variables.front())
            && remaining.degree() > 1) {
            auto univariate_factors =
                factor_univariate_polynomial(remaining, remaining_variables.front());
            factors.insert(
                factors.end(),
                univariate_factors.begin(),
                univariate_factors.end());
        } else if (!is_constant_one(remaining)) {
            factors.push_back(polynomial_to_expr(remaining));
        }

        if (factors.empty()) {
            return polynomial_to_expr(remaining);
        }
        if (factors.size() == 1) {
            return factors.front();
        }
        return make_times(factors);
    }

    ExprPtr collect_polynomial(const ExprPtr& expr, const std::vector<std::string>& variables, EvaluationContext& ctx) {
        auto polynomial_variables = infer_variables(expr);
        for (const auto& variable : variables) {
            if (!contains_variable(polynomial_variables, variable)) {
                polynomial_variables.push_back(variable);
            }
        }
        if (is_exact_polynomial_candidate(expr)) {
            return exact_polynomial_to_expr(
                expr_to_exact_polynomial(expr, polynomial_variables));
        }
        Polynomial poly = expr_to_polynomial(expr, polynomial_variables);
        Polynomial collected = collect(poly, variables);
        return polynomial_to_expr(collected);
    }

    ExprPtr gcd_polynomial(const ExprPtr& a, const ExprPtr& b, const std::vector<std::string>& variables, EvaluationContext& ctx) {
        if (is_exact_polynomial_candidate(a) && is_exact_polynomial_candidate(b)) {
            const ExactPolynomial left = expr_to_exact_polynomial(a, variables);
            const ExactPolynomial right = expr_to_exact_polynomial(b, variables);
            return exact_polynomial_to_expr(gcd_exact(left, right, variables));
        }
        Polynomial pa = expr_to_polynomial(a, variables);
        Polynomial pb = expr_to_polynomial(b, variables);
        Polynomial g = gcd(pa, pb, variables);
        return polynomial_to_expr(g);
    }

    std::pair<ExprPtr, ExprPtr> divide_polynomial(const ExprPtr& dividend, const ExprPtr& divisor, const std::vector<std::string>& variables, EvaluationContext& ctx) {
        if (is_exact_polynomial_candidate(dividend)
            && is_exact_polynomial_candidate(divisor)) {
            const ExactPolynomial left = expr_to_exact_polynomial(dividend, variables);
            const ExactPolynomial right = expr_to_exact_polynomial(divisor, variables);
            auto [quotient, remainder] = divide_exact(left, right, variables);
            return {
                exact_polynomial_to_expr(quotient),
                exact_polynomial_to_expr(remainder)
            };
        }
        Polynomial pdiv = expr_to_polynomial(dividend, variables);
        Polynomial pdis = expr_to_polynomial(divisor, variables);
        auto result = divide(pdiv, pdis, variables);
        return { polynomial_to_expr(result.first), polynomial_to_expr(result.second) };
    }

    // --- Low-level API ---

    Polynomial expand(const Polynomial& poly) {
        // Already expanded in this representation
        return poly;
    }

    Polynomial collect(const Polynomial& poly, const std::vector<std::string>& variables) {
        // Placeholder: no actual collection, just returns input
        return poly;
    }

    Polynomial gcd(const Polynomial& a, const Polynomial& b, const std::vector<std::string>& variables) {
        // Only supports univariate GCD for now
        if (variables.size() != 1)
            throw_unsupported_construct("gcd: only univariate GCD is implemented");
        return Polynomial::gcd(a, b, variables[0]);
    }

    std::pair<Polynomial, Polynomial> divide(const Polynomial& dividend, const Polynomial& divisor, const std::vector<std::string>& variables) {
        // Only supports univariate division for now
        if (variables.size() != 1)
            throw_unsupported_construct("divide: only univariate division is implemented");
        return dividend.divide(divisor);
    }

} // namespace aleph3
