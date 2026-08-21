#include "algebra/ExactPolynomialOps.hpp"

#include "evaluator/EvaluatorErrors.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <numeric>

namespace aleph3 {

namespace {

bool is_univariate_in(const ExactPolynomial& poly, const std::string& var) {
    for (const auto& [mono, coeff] : poly.terms) {
        if (coeff.is_zero()) continue;
        for (const auto& [mono_var, exponent] : mono) {
            if (mono_var != var && exponent != 0) return false;
        }
    }
    return true;
}

size_t nonzero_term_count(const ExactPolynomial& polynomial) {
    return static_cast<size_t>(std::count_if(
        polynomial.terms.begin(),
        polynomial.terms.end(),
        [](const auto& term) { return !term.second.is_zero(); }));
}

int valuation_in_variable(
    const ExactPolynomial& polynomial,
    const std::string& variable) {
    int valuation = std::numeric_limits<int>::max();
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        if (coefficient.is_zero()) continue;
        valuation = std::min(valuation, monomial_exponent(monomial, variable));
    }
    return valuation;
}

ExactPolynomial monomial_bounded_multivariate_gcd(
    const ExactPolynomial& left,
    const ExactPolynomial& right,
    const std::vector<std::string>& variables) {
    if (left.is_zero() && right.is_zero()) {
        throw_domain_violation("gcd: GCD of two zero polynomials is undefined");
    }
    if (left.is_zero()) return make_monic(right, variables);
    if (right.is_zero()) return make_monic(left, variables);

    if (nonzero_term_count(left) != 1 && nonzero_term_count(right) != 1) {
        throw_unsupported_construct("gcd: at least one multivariate operand must be a monomial");
    }

    Monomial common;
    for (const auto& variable : variables) {
        const int exponent =
            std::min(valuation_in_variable(left, variable), valuation_in_variable(right, variable));
        if (exponent > 0) common[variable] = exponent;
    }
    return ExactPolynomial({{common, ExactCoefficient::one()}});
}

std::pair<ExactPolynomial, ExactPolynomial> divide_exact(
    const ExactPolynomial& dividend,
    const ExactPolynomial& divisor,
    const std::vector<std::string>& variables) {
    if (variables.empty()) throw_invalid_form("divide: variable selector cannot be empty");
    if (divisor.is_zero()) throw std::domain_error("Polynomial division by zero");

    ExactPolynomial remainder = dividend;
    ExactPolynomial quotient;
    ExactPolynomial reduced;

    const auto leading = [&](const ExactPolynomial& polynomial) {
        auto selected = polynomial.terms.end();
        for (auto it = polynomial.terms.begin(); it != polynomial.terms.end(); ++it) {
            if (it->second.is_zero()) continue;
            if (selected == polynomial.terms.end() ||
                exact_monomial_precedes(
                    it->first,
                    selected->first,
                    MonomialOrder::graded_lexicographic,
                    variables)) {
                selected = it;
            }
        }
        return std::pair<Monomial, ExactCoefficient>{selected->first, selected->second};
    };
    const auto divides = [](const Monomial& divisor_mono, const Monomial& dividend_mono) {
        for (const auto& [variable, exponent] : divisor_mono) {
            if (monomial_exponent(dividend_mono, variable) < exponent) return false;
        }
        return true;
    };
    const auto subtract_monomial = [](Monomial dividend_mono, const Monomial& divisor_mono) {
        for (const auto& [variable, exponent] : divisor_mono) {
            dividend_mono[variable] -= exponent;
            if (dividend_mono[variable] == 0) dividend_mono.erase(variable);
        }
        return dividend_mono;
    };
    const auto [divisor_mono, divisor_coeff] = leading(divisor);

    while (!remainder.is_zero()) {
        const auto [remainder_mono, remainder_coeff] = leading(remainder);
        if (!divides(divisor_mono, remainder_mono)) {
            const ExactPolynomial remainder_term({{remainder_mono, remainder_coeff}});
            reduced = reduced + remainder_term;
            remainder = remainder - remainder_term;
            continue;
        }
        const Monomial quotient_mono = subtract_monomial(remainder_mono, divisor_mono);
        ExactPolynomial quotient_term({{quotient_mono, remainder_coeff / divisor_coeff}});
        quotient = quotient + quotient_term;
        remainder = remainder - (divisor * quotient_term);
    }

    quotient.normalize();
    reduced.normalize();
    return {quotient, reduced};
}

ExactPolynomial gcd_exact(
    const ExactPolynomial& left,
    const ExactPolynomial& right,
    const std::vector<std::string>& variables) {
    if (variables.empty()) throw_invalid_form("gcd: variable selector cannot be empty");
    if (variables.size() > 1) return monomial_bounded_multivariate_gcd(left, right, variables);

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

    return make_monic(a, variables);
}

}  // namespace

bool is_exact_constant_zero(const ExactPolynomial& poly) {
    return poly.terms.size() == 1 &&
        poly.terms.begin()->first.empty() &&
        poly.terms.begin()->second.is_zero();
}

bool is_exact_constant_one(const ExactPolynomial& poly) {
    return poly.terms.size() == 1 &&
        poly.terms.begin()->first.empty() &&
        poly.terms.begin()->second.is_one();
}

ExactPolynomial exact_constant(const ExactCoefficient& coefficient) {
    return ExactPolynomial(coefficient);
}

ExactPolynomial multiply_by_scalar(
    ExactPolynomial polynomial,
    const ExactCoefficient& coefficient) {
    for (auto& [_, term_coefficient] : polynomial.terms) {
        term_coefficient = term_coefficient * coefficient;
    }
    polynomial.normalize();
    return polynomial;
}

std::pair<Monomial, ExactCoefficient> leading_term(
    const ExactPolynomial& poly,
    const std::vector<std::string>& variables) {
    auto selected = poly.terms.end();
    for (auto it = poly.terms.begin(); it != poly.terms.end(); ++it) {
        if (it->second.is_zero()) continue;
        if (selected == poly.terms.end() ||
            exact_monomial_precedes(
                it->first,
                selected->first,
                MonomialOrder::graded_lexicographic,
                variables)) {
            selected = it;
        }
    }
    if (selected == poly.terms.end()) {
        throw_internal_inconsistency("A zero polynomial has no leading term");
    }
    return {selected->first, selected->second};
}

ExactPolynomial make_monic(
    ExactPolynomial polynomial,
    const std::vector<std::string>& variables) {
    if (polynomial.is_zero()) return polynomial;
    const auto lead = leading_term(polynomial, variables).second;
    for (auto& [_, coefficient] : polynomial.terms) coefficient = coefficient / lead;
    polynomial.normalize();
    return polynomial;
}

ExactCoefficient leading_coefficient_for_order(
    const ExactPolynomial& polynomial,
    const std::vector<std::string>& variables) {
    if (polynomial.is_zero()) return ExactCoefficient::zero();
    return leading_term(polynomial, variables).second;
}

int exact_degree_in_variable(
    const ExactPolynomial& poly,
    const std::string& variable) {
    int degree = 0;
    for (const auto& [monomial, coefficient] : poly.terms) {
        if (coefficient.is_zero()) continue;
        const auto it = monomial.find(variable);
        if (it != monomial.end()) degree = std::max(degree, it->second);
    }
    return degree;
}

int64_t coefficient_denominator_lcm(const ExactPolynomial& polynomial) {
    int64_t result = 1;
    for (const auto& [_, coefficient] : polynomial.terms) {
        const int64_t common = std::gcd(result, coefficient.denominator);
        result = checked_exact_multiply(result / common, coefficient.denominator);
    }
    return result;
}

int64_t checked_abs_int64(int64_t value) {
    if (value == std::numeric_limits<int64_t>::min()) {
        throw std::overflow_error("Exact coefficient overflow");
    }
    return std::llabs(value);
}

int64_t integer_content(const ExactPolynomial& polynomial) {
    int64_t result = 0;
    for (const auto& [_, coefficient] : polynomial.terms) {
        if (coefficient.is_zero()) continue;
        if (coefficient.denominator != 1) {
            throw_internal_inconsistency("Integer content requires cleared exact coefficients");
        }
        const int64_t abs_numerator = checked_abs_int64(coefficient.numerator);
        result = result == 0 ? abs_numerator : std::gcd(result, abs_numerator);
    }
    return result == 0 ? 1 : result;
}

ExactPolynomial divide_by_integer_content(
    ExactPolynomial polynomial,
    int64_t content) {
    if (content <= 1) return polynomial;
    for (auto& [_, coefficient] : polynomial.terms) {
        coefficient = coefficient / ExactCoefficient(content, 1);
    }
    polynomial.normalize();
    return polynomial;
}

ExactPolynomial expand(const ExactPolynomial& poly) {
    return poly;
}

ExactPolynomial collect(
    const ExactPolynomial& poly,
    const std::vector<std::string>& variables) {
    static_cast<void>(variables);
    return poly;
}

ExactPolynomial gcd(
    const ExactPolynomial& a,
    const ExactPolynomial& b,
    const std::vector<std::string>& variables) {
    return gcd_exact(a, b, variables);
}

std::pair<ExactPolynomial, ExactPolynomial> divide(
    const ExactPolynomial& dividend,
    const ExactPolynomial& divisor,
    const std::vector<std::string>& variables) {
    return divide_exact(dividend, divisor, variables);
}

}  // namespace aleph3
