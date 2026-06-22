#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"

namespace aleph3 {

bool is_symbolic_algebra_function(const std::string& name);

ExprPtr evaluate_algebra_function(const FunctionCall& func, EvaluationContext& ctx);

}  // namespace aleph3
