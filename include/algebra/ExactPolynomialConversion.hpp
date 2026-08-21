/*
 * ExactPolynomialConversion.hpp
 * -----------------------------
 * Conversion helpers between Expr and the exact polynomial representation.
 */

#pragma once

#include "algebra/ExactPolynomial.hpp"
#include "expr/Expr.hpp"

#include <string>
#include <vector>

namespace aleph3 {

bool is_exact_polynomial_candidate(const ExprPtr& expr);

ExprPtr exact_coefficient_to_expr(const ExactCoefficient& coefficient);

ExactPolynomial expr_to_exact_polynomial(
    const ExprPtr& expr,
    const std::vector<std::string>& variables);

ExprPtr exact_polynomial_to_expr(const ExactPolynomial& poly);

}  // namespace aleph3
