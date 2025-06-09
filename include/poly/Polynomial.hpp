/*
 * Polynomial.hpp
 * --------------
 * Defines the Polynomial class for Aleph3.
 *
 * This class represents univariate polynomials with double coefficients,
 * supporting basic arithmetic operations (addition, subtraction, multiplication, division),
 * GCD computation, normalization, and string conversion.
 *
 * Polynomials are stored as vectors of coefficients in increasing order of degree:
 *   [c0, c1, ..., cn] represents c0 + c1*x + ... + cn*x^n.
 *
 * This class is intended for internal use in polynomial manipulation routines.
 * Conversion utilities should be used to translate between ExprPtr and Polynomial.
 */
#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include "expr/Expr.hpp"

namespace aleph3 {

    class Polynomial {
    public:
        // Coefficients are stored in increasing order of degree
        std::vector<double> coefficients;

        Polynomial();
        Polynomial(const std::vector<double>& coeffs);

        // Normalize the polynomial by removing leading zeros
        void normalize();

        // Polynomial addition
        Polynomial operator+(const Polynomial& other) const;

        // Polynomial subtraction
        Polynomial operator-(const Polynomial& other) const;

        // Polynomial multiplication
        Polynomial operator*(const Polynomial& other) const;

        // Polynomial division (returns quotient and remainder)
        std::pair<Polynomial, Polynomial> divide(const Polynomial& divisor) const;

        // Calculate GCD of two polynomials
        static Polynomial gcd(const Polynomial& a, const Polynomial& b);

        // Check if the polynomial is zero
        bool is_zero() const;

        // Get the degree of the polynomial
        size_t degree() const;

        // Convert polynomial to string representation
        std::string to_string() const;
    };

} // namespace aleph3