#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"

namespace aleph3 {

ExprPtr evaluate_builtin_function(const FunctionCall& func, EvaluationContext& ctx);

}  // namespace aleph3
