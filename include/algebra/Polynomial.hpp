/*
 * Polynomial.hpp
 * --------------
 * Defines the multivariate Polynomial class for Aleph3.
 *
 * This class represents multivariate polynomials with double coefficients,
 * supporting basic arithmetic operations (addition, subtraction, multiplication, division),
 * GCD computation, normalization, and string conversion.
 *
 * Each monomial is represented as a mapping from variable names to exponents.
 * The polynomial is a mapping from monomials to coefficients.
 *
 * Example: 3*x^2*y + 2*y^3 is represented as:
 *   { {{"x",2},{"y",1}}: 3.0, {{"y",3}}: 2.0 }
 *
 * This class is intended for internal use in polynomial manipulation routines.
 * Conversion utilities should be used to translate between ExprPtr and Polynomial.
 */
#pragma once

#include <map>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include "expr/Expr.hpp"

namespace aleph3 {

    // Monomial: map from variable name to exponent, e.g. {{"x",2},{"y",1}}
    using Monomial = std::map<std::string, int>;

    // Comparison for Monomial (lexicographic)
    inline bool operator<(const Monomial& a, const Monomial& b) {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
    }

    class Polynomial {
    public:
        // Each monomial maps to a coefficient
        std::map<Monomial, double> terms;

        Polynomial();
        Polynomial(const std::map<Monomial, double>& terms);

        // Construct from a single constant
        explicit Polynomial(double constant);

        // Normalize the polynomial by removing zero coefficients
        void normalize();

        // Polynomial addition
        Polynomial operator+(const Polynomial& other) const;

        // Polynomial subtraction
        Polynomial operator-(const Polynomial& other) const;

        // Polynomial multiplication
        Polynomial operator*(const Polynomial& other) const;

        // Polynomial division (returns quotient and remainder)
        std::pair<Polynomial, Polynomial> divide(const Polynomial& divisor) const;

        // Calculate GCD of two polynomials (univariate only for now)
        static Polynomial gcd(const Polynomial& a, const Polynomial& b, const std::string& var);

        // Check if the polynomial is zero
        bool is_zero() const;

        // Get the total degree of the polynomial
        size_t degree() const;

        // Convert polynomial to string representation
        std::string to_string() const;
    };

} // namespace aleph3