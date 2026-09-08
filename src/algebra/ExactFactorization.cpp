#include "algebra/ExactFactorization.hpp"

#include "algebra/ExactPolynomialConversion.hpp"
#include "algebra/ExactPolynomialOps.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "expr/ExprUtils.hpp"
#include "expr/ExprStructural.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>

namespace aleph3 {

namespace {

constexpr int64_t kMaxRationalRootDivisorScan = 100000;

struct RationalRoot {
    kernel::ExactInteger numerator;
    kernel::ExactInteger denominator = kernel::ExactInteger(1);

    [[nodiscard]] kernel::ExactRational exact() const {
        return kernel::ExactRational(numerator, denominator);
    }
};

std::vector<std::string> infer_variables(const ExactPolynomial& poly) {
    std::set<std::string> vars;
    for (const auto& [mono, coeff] : poly.terms) {
        if (coeff.is_zero()) continue;
        for (const auto& [var, exponent] : mono) {
            if (exponent != 0) vars.insert(var);
        }
    }
    return {vars.begin(), vars.end()};
}

bool is_univariate_in(const ExactPolynomial& poly, const std::string& var) {
    for (const auto& [mono, coeff] : poly.terms) {
        if (coeff.is_zero()) continue;
        for (const auto& [mono_var, exponent] : mono) {
            if (mono_var != var && exponent != 0) return false;
        }
    }
    return true;
}

int degree_in_variable(const ExactPolynomial& poly, const std::string& var) {
    int degree = 0;
    for (const auto& [mono, coeff] : poly.terms) {
        if (coeff.is_zero()) continue;
        const auto it = mono.find(var);
        if (it != mono.end()) degree = std::max(degree, it->second);
    }
    return degree;
}

struct ExactPolynomialContent {
    ExactCoefficient coefficient = ExactCoefficient::one();
    Monomial monomial;
    ExactPolynomial primitive;
};

ExactPolynomial monomial_to_exact_polynomial(const Monomial& monomial) {
    return ExactPolynomial({{monomial, ExactCoefficient::one()}});
}

ExactPolynomialContent split_exact_content(
    const ExactPolynomial& poly,
    const std::vector<std::string>& variables) {
    ExactPolynomialContent content;
    content.primitive = poly;
    if (poly.is_zero()) {
        content.coefficient = ExactCoefficient::zero();
        return content;
    }

    kernel::ExactInteger denominator_lcm(1);
    bool first_term = true;
    for (const auto& [monomial, coefficient] : poly.terms) {
        if (coefficient.is_zero()) continue;
        const kernel::ExactInteger common =
            kernel::gcd(denominator_lcm, coefficient.denominator());
        denominator_lcm = (denominator_lcm / common) * coefficient.denominator();
        if (first_term) {
            content.monomial = monomial;
            first_term = false;
        } else {
            for (auto it = content.monomial.begin(); it != content.monomial.end();) {
                const auto monomial_it = monomial.find(it->first);
                if (monomial_it == monomial.end()) {
                    it = content.monomial.erase(it);
                } else {
                    it->second = std::min(it->second, monomial_it->second);
                    if (it->second == 0) {
                        it = content.monomial.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }

    ExactPolynomial integer_poly =
        multiply_by_scalar(poly, ExactCoefficient(denominator_lcm, kernel::ExactInteger(1)));
    const kernel::ExactInteger coefficient_content = integer_content(integer_poly);
    content.coefficient = ExactCoefficient(coefficient_content, denominator_lcm);

    ExactPolynomial primitive;
    primitive.terms.clear();
    for (const auto& [monomial, coefficient] : integer_poly.terms) {
        if (coefficient.is_zero()) continue;
        Monomial reduced = monomial;
        for (const auto& [variable, exponent] : content.monomial) {
            auto reduced_it = reduced.find(variable);
            if (reduced_it != reduced.end()) {
                reduced_it->second -= exponent;
                if (reduced_it->second == 0) reduced.erase(reduced_it);
            }
        }
        primitive.terms[reduced] =
            primitive.terms[reduced] +
            (coefficient / ExactCoefficient(coefficient_content, kernel::ExactInteger(1)));
    }
    primitive.normalize();

    const auto leading = leading_coefficient_for_order(primitive, variables);
    if (leading.is_negative()) {
        primitive = multiply_by_scalar(primitive, ExactCoefficient(-1, 1));
        content.coefficient = content.coefficient * ExactCoefficient(-1, 1);
    }
    content.primitive = std::move(primitive);
    return content;
}

std::vector<kernel::ExactInteger> univariate_integer_coefficients(
    const ExactPolynomial& poly,
    const std::string& var) {
    const int degree = degree_in_variable(poly, var);
    std::vector<kernel::ExactInteger> coefficients(
        static_cast<size_t>(degree) + 1,
        kernel::ExactInteger(0));
    for (const auto& [monomial, coefficient] : poly.terms) {
        if (coefficient.is_zero()) continue;
        if (!coefficient.is_integer()) {
            throw_unsupported_construct(
                "Polynomial factorization currently requires integer coefficients");
        }
        int exponent = 0;
        const auto exponent_it = monomial.find(var);
        if (exponent_it != monomial.end()) exponent = exponent_it->second;
        coefficients[static_cast<size_t>(degree - exponent)] = coefficient.numerator();
    }
    return coefficients;
}

std::vector<kernel::ExactInteger> positive_divisors(const kernel::ExactInteger& value) {
    const kernel::ExactInteger absolute = kernel::abs(value);
    if (absolute.is_zero()) return {kernel::ExactInteger(0)};
    const auto bounded = absolute.to_int64();
    if (!bounded.has_value() || *bounded > kMaxRationalRootDivisorScan) {
        throw_unsupported_construct(
            "Factor exceeded the rational-root divisor candidate budget");
    }
    std::vector<kernel::ExactInteger> divisors;
    for (int64_t candidate = 1; candidate <= *bounded; ++candidate) {
        const kernel::ExactInteger exact_candidate(candidate);
        if ((absolute % exact_candidate).is_zero()) divisors.push_back(exact_candidate);
    }
    return divisors;
}

std::vector<RationalRoot> rational_root_candidates(
    const std::vector<kernel::ExactInteger>& coefficients) {
    if (coefficients.empty()) return {};

    const kernel::ExactInteger& leading = coefficients.front();
    const kernel::ExactInteger& constant = coefficients.back();
    if (leading.is_zero()) return {};

    std::vector<RationalRoot> candidates;
    std::set<std::pair<kernel::ExactInteger, kernel::ExactInteger>> seen;
    for (const auto numerator : positive_divisors(constant)) {
        for (const auto denominator : positive_divisors(leading)) {
            if (denominator.is_zero()) continue;
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

ExactCoefficient evaluate_univariate_coefficients_exact(
    const std::vector<kernel::ExactInteger>& coefficients,
    const RationalRoot& root) {
    const ExactCoefficient root_value(root.exact());
    ExactCoefficient value = ExactCoefficient::zero();
    for (const auto coefficient : coefficients) {
        value = value * root_value + ExactCoefficient(coefficient, kernel::ExactInteger(1));
    }
    return value;
}

std::optional<std::vector<ExactCoefficient>> synthetic_divide_exact(
    const std::vector<kernel::ExactInteger>& coefficients,
    const RationalRoot& root) {
    if (coefficients.size() < 2) return std::nullopt;
    const ExactCoefficient root_value(root.exact());
    std::vector<ExactCoefficient> quotient(coefficients.size() - 1);
    ExactCoefficient carry(coefficients.front(), kernel::ExactInteger(1));
    quotient.front() = carry;
    for (size_t i = 1; i + 1 < coefficients.size(); ++i) {
        carry = carry * root_value + ExactCoefficient(coefficients[i], kernel::ExactInteger(1));
        quotient[i] = carry;
    }
    const ExactCoefficient remainder =
        carry * root_value + ExactCoefficient(coefficients.back(), kernel::ExactInteger(1));
    if (!remainder.is_zero()) return std::nullopt;
    return quotient;
}

bool all_integer_coefficients(const std::vector<ExactCoefficient>& coefficients) {
    return std::all_of(
        coefficients.begin(),
        coefficients.end(),
        [](const ExactCoefficient& coefficient) {
            return coefficient.is_integer();
        });
}

std::vector<kernel::ExactInteger> exact_coefficients_to_ints(
    const std::vector<ExactCoefficient>& coefficients) {
    std::vector<kernel::ExactInteger> result;
    result.reserve(coefficients.size());
    for (const auto& coefficient : coefficients) {
        if (!coefficient.is_integer()) {
            throw_internal_inconsistency("Expected integer exact coefficients");
        }
        result.push_back(coefficient.numerator());
    }
    return result;
}

ExactPolynomial exact_coefficients_to_polynomial(
    const std::vector<ExactCoefficient>& coefficients,
    const std::string& var) {
    const int degree = static_cast<int>(coefficients.size()) - 1;
    ExactPolynomial poly;
    poly.terms.clear();
    for (size_t i = 0; i < coefficients.size(); ++i) {
        const auto& coefficient = coefficients[i];
        if (coefficient.is_zero()) continue;
        const int exponent = degree - static_cast<int>(i);
        Monomial monomial;
        if (exponent > 0) monomial[var] = exponent;
        poly.terms[monomial] = poly.terms[monomial] + coefficient;
    }
    poly.normalize();
    return poly;
}

ExactPolynomial integer_coefficients_to_exact_polynomial(
    const std::vector<kernel::ExactInteger>& coefficients,
    const std::string& var) {
    std::vector<ExactCoefficient> exact_coefficients;
    exact_coefficients.reserve(coefficients.size());
    for (const auto coefficient : coefficients) {
        exact_coefficients.emplace_back(coefficient, kernel::ExactInteger(1));
    }
    return exact_coefficients_to_polynomial(exact_coefficients, var);
}

ExprPtr monic_linear_factor_expr(const std::string& var, const RationalRoot& root) {
    auto variable_term = make_expr<Symbol>(var);
    if (root.denominator.is_one()) {
        if (root.numerator.is_negative()) {
            return make_plus(
                variable_term,
                make_integer(kernel::abs(root.numerator)));
        }
        return make_expr<FunctionCall>(
            "Minus",
            std::vector<ExprPtr>{
                variable_term,
                make_integer(root.numerator)
            });
    }

    ExprPtr constant_term = make_exact_scalar_expr(
        kernel::ExactRational(kernel::abs(root.numerator), root.denominator));
    if (root.numerator.is_negative()) return make_plus(variable_term, constant_term);
    return make_expr<FunctionCall>("Minus", std::vector<ExprPtr>{variable_term, constant_term});
}

RationalRoot exact_linear_root(const ExactPolynomial& poly, const std::string& var) {
    ExactCoefficient variable_coefficient = ExactCoefficient::zero();
    ExactCoefficient constant_term = ExactCoefficient::zero();
    for (const auto& [monomial, coefficient] : poly.terms) {
        if (coefficient.is_zero()) continue;
        if (monomial.empty()) {
            constant_term = constant_term + coefficient;
            continue;
        }
        if (monomial.size() == 1 && monomial_exponent(monomial, var) == 1) {
            variable_coefficient = variable_coefficient + coefficient;
            continue;
        }
        throw_internal_inconsistency("Expected a univariate linear polynomial");
    }
    const ExactCoefficient root =
        (ExactCoefficient::zero() - constant_term) / variable_coefficient;
    return RationalRoot{root.numerator(), root.denominator()};
}

std::vector<ExprPtr> factor_univariate_exact_polynomial(
    const ExactPolynomial& poly,
    const std::string& var) {
    std::vector<std::pair<RationalRoot, ExprPtr>> linear_factors;
    std::vector<ExprPtr> factors;
    if (poly.degree() <= 1) {
        factors.push_back(exact_polynomial_to_expr(poly));
        return factors;
    }

    ExactPolynomial remainder_poly;
    bool has_remainder_poly = false;
    auto coefficients = univariate_integer_coefficients(poly, var);
    while (coefficients.size() > 2) {
        bool found_root = false;
        for (const auto candidate : rational_root_candidates(coefficients)) {
            if (!evaluate_univariate_coefficients_exact(coefficients, candidate).is_zero()) {
                continue;
            }
            const auto quotient = synthetic_divide_exact(coefficients, candidate);
            if (!quotient.has_value()) continue;

            linear_factors.emplace_back(candidate, monic_linear_factor_expr(var, candidate));
            if (all_integer_coefficients(*quotient)) {
                coefficients = exact_coefficients_to_ints(*quotient);
            } else {
                remainder_poly = exact_coefficients_to_polynomial(*quotient, var);
                has_remainder_poly = true;
                coefficients.clear();
            }
            found_root = true;
            break;
        }
        if (!found_root || has_remainder_poly) break;
    }

    if (!has_remainder_poly) {
        remainder_poly = integer_coefficients_to_exact_polynomial(coefficients, var);
    }

    if (remainder_poly.degree() == 1) {
        linear_factors.emplace_back(
            exact_linear_root(remainder_poly, var),
            exact_polynomial_to_expr(remainder_poly));
    } else if (!is_exact_constant_one(remainder_poly)) {
        factors.push_back(exact_polynomial_to_expr(remainder_poly));
    }

    std::sort(
        linear_factors.begin(),
        linear_factors.end(),
        [](const auto& left, const auto& right) {
            const int root_order = kernel::compare(left.first.exact(), right.first.exact());
            if (root_order != 0) return root_order > 0;
            return structural_less(left.second, right.second);
        });

    for (const auto& [_, factor] : linear_factors) factors.push_back(factor);
    return factors;
}

}  // namespace

ExprPtr factor_exact_polynomial(
    const ExactPolynomial& poly,
    const std::vector<std::string>& variables) {
    if (poly.is_zero()) return exact_polynomial_to_expr(poly);

    const auto content = split_exact_content(poly, variables);
    std::vector<ExprPtr> factors;
    if (!content.coefficient.is_one() || !content.monomial.empty()) {
        std::vector<ExprPtr> content_factors;
        if (!content.coefficient.is_one()) {
            content_factors.push_back(exact_coefficient_to_expr(content.coefficient));
        }
        if (!content.monomial.empty()) {
            content_factors.push_back(
                exact_polynomial_to_expr(monomial_to_exact_polynomial(content.monomial)));
        }
        factors.push_back(
            content_factors.size() == 1 ? content_factors.front() : make_times(content_factors));
    }

    const auto remaining_variables = infer_variables(content.primitive);
    if (remaining_variables.size() == 1 &&
        is_univariate_in(content.primitive, remaining_variables.front()) &&
        content.primitive.degree() > 1) {
        auto univariate_factors =
            factor_univariate_exact_polynomial(content.primitive, remaining_variables.front());
        factors.insert(factors.end(), univariate_factors.begin(), univariate_factors.end());
    } else if (!is_exact_constant_one(content.primitive)) {
        factors.push_back(exact_polynomial_to_expr(content.primitive));
    }

    if (factors.empty()) return exact_polynomial_to_expr(content.primitive);
    if (factors.size() == 1) return factors.front();
    return make_times(factors);
}

}  // namespace aleph3
