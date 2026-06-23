#pragma once

#include "evaluator/EvaluationContext.hpp"
#include "expr/Expr.hpp"

namespace aleph3 {

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx);

std::string expr_to_key(const ExprPtr& expr);

}  // namespace aleph3
