/*
 * ExactRationalExpression.hpp
 * ---------------------------
 * Exact rational-expression representation and normalization helpers.
 */

#pragma once

#include "algebra/ExactPolynomial.hpp"
#include "expr/Expr.hpp"
#include "kernel/DomainRestrictions.hpp"

#include <string>
#include <vector>

namespace aleph3 {

struct ExactRationalExpression {
    ExactPolynomial numerator;
    ExactPolynomial denominator;
    std::vector<std::string> variables;
    kernel::DomainRestrictions restrictions;
};

ExactRationalExpression normalize_exact_rational_expression(
    ExactPolynomial numerator,
    ExactPolynomial denominator,
    std::vector<std::string> variables);

ExactRationalExpression normalize_exact_rational_expression(
    ExactPolynomial numerator,
    ExactPolynomial denominator,
    std::vector<std::string> variables,
    kernel::DomainRestrictions restrictions);

ExactRationalExpression exact_rational_expression_from_expr(
    const ExprPtr& expr,
    const std::vector<std::string>& variables);

ExprPtr exact_rational_expression_to_expr(
    const ExactRationalExpression& expression);

ExactRationalExpression cancel_exact_rational_expression(
    ExactRationalExpression expression);

}  // namespace aleph3
