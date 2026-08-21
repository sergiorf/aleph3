/*
 * PolynomialOps.hpp
 * -----------------
 * Legacy double-based polynomial conversion and operations.
 */

#pragma once

#include "algebra/Polynomial.hpp"
#include "expr/Expr.hpp"

#include <string>
#include <utility>
#include <vector>

namespace aleph3 {

std::vector<std::string> infer_polynomial_variables(const ExprPtr& expr);
std::vector<std::string> infer_polynomial_variables(const Polynomial& poly);

bool is_constant_zero(const Polynomial& poly);
bool is_constant_one(const Polynomial& poly);

Polynomial expr_to_polynomial(
    const ExprPtr& expr,
    const std::vector<std::string>& variables);

ExprPtr polynomial_to_expr(const Polynomial& poly);

ExprPtr factor_approximate_polynomial(const Polynomial& poly);

Polynomial expand(const Polynomial& poly);

Polynomial collect(
    const Polynomial& poly,
    const std::vector<std::string>& variables);

Polynomial gcd(
    const Polynomial& a,
    const Polynomial& b,
    const std::vector<std::string>& variables);

std::pair<Polynomial, Polynomial> divide(
    const Polynomial& dividend,
    const Polynomial& divisor,
    const std::vector<std::string>& variables);

}  // namespace aleph3
