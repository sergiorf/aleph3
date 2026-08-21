/*
 * Polynomial Utilities
 * --------------------
 * Pack-owned helpers for converting between expressions and polynomial forms
 * and for running the current supported polynomial operations.
 */

#pragma once

#include "ExactPolynomialConversion.hpp"
#include "ExactPolynomialOps.hpp"
#include "ExactPolynomial.hpp"
#include "PolynomialOps.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include <vector>
#include <utility>
#include <string>

namespace aleph3 {

    // High-level API: operate on ExprPtr for integration with evaluator
    // For multivariate polynomials, variable list may be inferred or passed as argument if needed
    ExprPtr expand_polynomial(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr factor_polynomial(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr collect_polynomial(
        const ExprPtr& expr,
        const std::vector<std::string>& variables,
        EvaluationContext& ctx);
    ExprPtr gcd_polynomial(
        const ExprPtr& a,
        const ExprPtr& b,
        const std::vector<std::string>& variables,
        EvaluationContext& ctx);
    std::pair<ExprPtr, ExprPtr> divide_polynomial(
        const ExprPtr& dividend,
        const ExprPtr& divisor,
        const std::vector<std::string>& variables,
        EvaluationContext& ctx);
    ExprPtr coefficient_polynomial(
        const ExprPtr& expr,
        const std::string& variable,
        int exponent,
        EvaluationContext& ctx);
    ExprPtr coefficient_list_polynomial(
        const ExprPtr& expr,
        const std::string& variable,
        EvaluationContext& ctx);
    ExprPtr numerator_rational_expression(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr denominator_rational_expression(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr together_rational_expression(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr cancel_rational_expression(const ExprPtr& expr, EvaluationContext& ctx);
} // namespace aleph3
