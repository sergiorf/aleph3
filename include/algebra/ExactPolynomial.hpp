/*
 * ExactPolynomial.hpp
 * -------------------
 * Defines the exact polynomial layer used by the algebra pack for exact
 * integer and rational coefficient preservation.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <tuple>

#include "algebra/Polynomial.hpp"
#include "expr/ExprUtils.hpp"

namespace aleph3 {

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

inline ExactCoefficient operator+(const ExactCoefficient& left, const ExactCoefficient& right) {
    return ExactCoefficient(
        left.numerator * right.denominator + right.numerator * left.denominator,
        left.denominator * right.denominator);
}

inline ExactCoefficient operator-(const ExactCoefficient& left, const ExactCoefficient& right) {
    return ExactCoefficient(
        left.numerator * right.denominator - right.numerator * left.denominator,
        left.denominator * right.denominator);
}

inline ExactCoefficient operator*(const ExactCoefficient& left, const ExactCoefficient& right) {
    return ExactCoefficient(
        left.numerator * right.numerator,
        left.denominator * right.denominator);
}

inline ExactCoefficient operator/(const ExactCoefficient& left, const ExactCoefficient& right) {
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
