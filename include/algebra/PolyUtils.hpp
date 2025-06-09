#pragma once

#include "Polynomial.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include <vector>
#include <utility>
#include <stdexcept>

namespace aleph3 {

    // High-level API: operate on ExprPtr for integration with evaluator
    ExprPtr expand_polynomial(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr factor_polynomial(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr collect_polynomial(const ExprPtr& expr, EvaluationContext& ctx);
    ExprPtr gcd_polynomial(const ExprPtr& a, const ExprPtr& b, EvaluationContext& ctx);
    std::pair<ExprPtr, ExprPtr> divide_polynomial(const ExprPtr& dividend, const ExprPtr& divisor, EvaluationContext& ctx);

    // Low-level API: operate directly on Polynomial objects
    Polynomial expand(const Polynomial& poly);
    Polynomial factor(const Polynomial& poly);
    Polynomial collect(const Polynomial& poly);
    Polynomial gcd(const Polynomial& a, const Polynomial& b);
    std::pair<Polynomial, Polynomial> divide(const Polynomial& dividend, const Polynomial& divisor);

} // namespace aleph3