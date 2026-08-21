/*
 * ExactPolynomialOps.hpp
 * ----------------------
 * Exact polynomial operations and shared helper primitives.
 */

#pragma once

#include "algebra/ExactPolynomial.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace aleph3 {

bool is_exact_constant_zero(const ExactPolynomial& poly);
bool is_exact_constant_one(const ExactPolynomial& poly);

ExactPolynomial exact_constant(const ExactCoefficient& coefficient);

ExactPolynomial multiply_by_scalar(
    ExactPolynomial polynomial,
    const ExactCoefficient& coefficient);

std::pair<Monomial, ExactCoefficient> leading_term(
    const ExactPolynomial& poly,
    const std::vector<std::string>& variables);

ExactPolynomial make_monic(
    ExactPolynomial polynomial,
    const std::vector<std::string>& variables);

ExactCoefficient leading_coefficient_for_order(
    const ExactPolynomial& polynomial,
    const std::vector<std::string>& variables);

int exact_degree_in_variable(
    const ExactPolynomial& poly,
    const std::string& variable);

int64_t coefficient_denominator_lcm(const ExactPolynomial& polynomial);
int64_t checked_abs_int64(int64_t value);
int64_t integer_content(const ExactPolynomial& polynomial);

ExactPolynomial divide_by_integer_content(
    ExactPolynomial polynomial,
    int64_t content);

ExactPolynomial expand(const ExactPolynomial& poly);

ExactPolynomial collect(
    const ExactPolynomial& poly,
    const std::vector<std::string>& variables);

ExactPolynomial gcd(
    const ExactPolynomial& a,
    const ExactPolynomial& b,
    const std::vector<std::string>& variables);

std::pair<ExactPolynomial, ExactPolynomial> divide(
    const ExactPolynomial& dividend,
    const ExactPolynomial& divisor,
    const std::vector<std::string>& variables);

}  // namespace aleph3
