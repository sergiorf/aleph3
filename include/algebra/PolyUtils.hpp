#pragma once

#include "Polynomial.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include <vector>
#include <utility>
#include <stdexcept>
#include <string>

namespace aleph3 {

    // High-level API: operate on ExprPtr for integration with evaluator
    // For multivariate polynomials, variable list may be inferred or passed as argument if needed
    ExprPtr expand_polynomial(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr factor_polynomial(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr collect_polynomial(const ExprPtr& expr, const std::vector<std::string>& variables, EvaluationContext& ctx);
    ExprPtr gcd_polynomial(const ExprPtr& a, const ExprPtr& b, const std::vector<std::string>& variables, EvaluationContext& ctx);
    std::pair<ExprPtr, ExprPtr> divide_polynomial(const ExprPtr& dividend, const ExprPtr& divisor, const std::vector<std::string>& variables, EvaluationContext& ctx);

    // Low-level API: operate directly on Polynomial objects
    Polynomial expand(const Polynomial& poly);
    Polynomial factor(const Polynomial& poly);
    Polynomial collect(const Polynomial& poly, const std::vector<std::string>& variables);
    Polynomial gcd(const Polynomial& a, const Polynomial& b, const std::vector<std::string>& variables);
    std::pair<Polynomial, Polynomial> divide(const Polynomial& dividend, const Polynomial& divisor, const std::vector<std::string>& variables);

    // Conversion utilities
    Polynomial expr_to_polynomial(const ExprPtr& expr, const std::vector<std::string>& variables);
    ExprPtr polynomial_to_expr(const Polynomial& poly);

} // namespace aleph3