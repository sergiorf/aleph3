/*
 * ExactPolynomial.hpp
 * -------------------
 * Defines the exact polynomial layer used by the algebra pack for exact
 * integer and rational coefficient preservation. Coefficients use checked
 * int64_t numerator/denominator storage; overflow is reported explicitly
 * rather than wrapping or falling back to approximate arithmetic.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <tuple>
#include <string>
#include <set>
#include <vector>

#include "algebra/Polynomial.hpp"
#include "expr/ExprUtils.hpp"

namespace aleph3 {

enum class MonomialOrder { lexicographic, graded_lexicographic, graded_reverse_lexicographic };

inline int monomial_exponent(const Monomial& monomial, const std::string& variable) {
    const auto it = monomial.find(variable);
    return it == monomial.end() ? 0 : it->second;
}

inline std::vector<std::string> complete_variable_precedence(
    const Monomial& left,
    const Monomial& right,
    std::vector<std::string> precedence) {
    std::set<std::string> remaining;
    for (const auto& [name, _] : left) remaining.insert(name);
    for (const auto& [name, _] : right) remaining.insert(name);
    for (const auto& name : precedence) remaining.erase(name);
    precedence.insert(precedence.end(), remaining.begin(), remaining.end());
    return precedence;
}

inline bool exact_monomial_precedes(
    const Monomial& left,
    const Monomial& right,
    MonomialOrder order = MonomialOrder::graded_lexicographic,
    std::vector<std::string> variable_precedence = {}) {
    variable_precedence = complete_variable_precedence(left, right, std::move(variable_precedence));
    const auto degree = [](const Monomial& monomial) {
        int result = 0;
        for (const auto& [_, exponent] : monomial) result += exponent;
        return result;
    };
    if (order != MonomialOrder::lexicographic && degree(left) != degree(right)) {
        return degree(left) > degree(right);
    }
    if (order == MonomialOrder::graded_reverse_lexicographic) {
        for (auto it = variable_precedence.rbegin(); it != variable_precedence.rend(); ++it) {
            const int left_exponent = monomial_exponent(left, *it);
            const int right_exponent = monomial_exponent(right, *it);
            if (left_exponent != right_exponent) return left_exponent < right_exponent;
        }
        return false;
    }
    for (const auto& variable : variable_precedence) {
        const int left_exponent = monomial_exponent(left, variable);
        const int right_exponent = monomial_exponent(right, variable);
        if (left_exponent != right_exponent) return left_exponent > right_exponent;
    }
    return false;
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

    [[nodiscard]] bool is_zero() const {
        return numerator == 0;
    }

    [[nodiscard]] bool is_one() const {
        return numerator == denominator;
    }

    bool operator==(const ExactCoefficient&) const = default;
};

inline int64_t checked_exact_add(int64_t left, int64_t right) {
    if ((right > 0 && left > std::numeric_limits<int64_t>::max() - right) ||
        (right < 0 && left < std::numeric_limits<int64_t>::min() - right)) {
        throw std::overflow_error("Exact coefficient overflow");
    }
    return left + right;
}

inline int64_t checked_exact_subtract(int64_t left, int64_t right) {
    if ((right < 0 && left > std::numeric_limits<int64_t>::max() + right) ||
        (right > 0 && left < std::numeric_limits<int64_t>::min() + right)) {
        throw std::overflow_error("Exact coefficient overflow");
    }
    return left - right;
}

inline int64_t checked_exact_multiply(int64_t left, int64_t right) {
    if (left == 0 || right == 0) return 0;
    if ((left == -1 && right == std::numeric_limits<int64_t>::min()) ||
        (right == -1 && left == std::numeric_limits<int64_t>::min())) {
        throw std::overflow_error("Exact coefficient overflow");
    }
    if (left > 0) {
        if ((right > 0 && left > std::numeric_limits<int64_t>::max() / right) ||
            (right < 0 && right < std::numeric_limits<int64_t>::min() / left)) {
            throw std::overflow_error("Exact coefficient overflow");
        }
    } else if ((right > 0 && left < std::numeric_limits<int64_t>::min() / right) ||
               (right < 0 && left < std::numeric_limits<int64_t>::max() / right)) {
        throw std::overflow_error("Exact coefficient overflow");
    }
    return left * right;
}

inline ExactCoefficient operator+(const ExactCoefficient& left, const ExactCoefficient& right) {
    const int64_t common = std::gcd(left.denominator, right.denominator);
    const int64_t left_scale = right.denominator / common;
    const int64_t right_scale = left.denominator / common;
    return ExactCoefficient(
        checked_exact_add(
            checked_exact_multiply(left.numerator, left_scale),
            checked_exact_multiply(right.numerator, right_scale)),
        checked_exact_multiply(left.denominator, left_scale));
}

inline ExactCoefficient operator-(const ExactCoefficient& left, const ExactCoefficient& right) {
    const int64_t common = std::gcd(left.denominator, right.denominator);
    const int64_t left_scale = right.denominator / common;
    const int64_t right_scale = left.denominator / common;
    return ExactCoefficient(
        checked_exact_subtract(
            checked_exact_multiply(left.numerator, left_scale),
            checked_exact_multiply(right.numerator, right_scale)),
        checked_exact_multiply(left.denominator, left_scale));
}

inline ExactCoefficient operator*(const ExactCoefficient& left, const ExactCoefficient& right) {
    const int64_t left_cancel = std::gcd(left.numerator, right.denominator);
    const int64_t right_cancel = std::gcd(right.numerator, left.denominator);
    return ExactCoefficient(
        checked_exact_multiply(left.numerator / left_cancel, right.numerator / right_cancel),
        checked_exact_multiply(left.denominator / right_cancel, right.denominator / left_cancel));
}

inline ExactCoefficient operator/(const ExactCoefficient& left, const ExactCoefficient& right) {
    if (right.numerator == 0) {
        throw std::domain_error("Polynomial division by zero");
    }
    const int64_t numerator_cancel = std::gcd(left.numerator, right.numerator);
    const int64_t denominator_cancel = std::gcd(left.denominator, right.denominator);
    return ExactCoefficient(
        checked_exact_multiply(
            left.numerator / numerator_cancel,
            right.denominator / denominator_cancel),
        checked_exact_multiply(
            left.denominator / denominator_cancel,
            right.numerator / numerator_cancel));
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

    [[nodiscard]] bool is_zero() const {
        return terms.size() == 1
            && terms.begin()->first.empty()
            && terms.begin()->second.is_zero();
    }

    [[nodiscard]] size_t degree() const {
        size_t max_degree = 0;
        for (const auto& [mono, coeff] : terms) {
            if (coeff.is_zero()) {
                continue;
            }
            size_t degree = 0;
            for (const auto& [_, exponent] : mono) {
                degree += static_cast<size_t>(exponent);
            }
            max_degree = std::max(max_degree, degree);
        }
        return max_degree;
    }
};

inline ExactPolynomial operator+(const ExactPolynomial& left, const ExactPolynomial& right) {
    ExactPolynomial result = left;
    for (const auto& [mono, coeff] : right.terms) {
        result.terms[mono] = result.terms[mono] + coeff;
    }
    result.normalize();
    return result;
}

inline ExactPolynomial operator-(const ExactPolynomial& left, const ExactPolynomial& right) {
    ExactPolynomial result = left;
    for (const auto& [mono, coeff] : right.terms) {
        result.terms[mono] = result.terms[mono] - coeff;
    }
    result.normalize();
    return result;
}

inline ExactPolynomial operator*(const ExactPolynomial& left, const ExactPolynomial& right) {
    ExactPolynomial result;
    result.terms.clear();
    for (const auto& [left_mono, left_coeff] : left.terms) {
        if (left_coeff.is_zero()) {
            continue;
        }
        for (const auto& [right_mono, right_coeff] : right.terms) {
            if (right_coeff.is_zero()) {
                continue;
            }
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

}  // namespace aleph3
