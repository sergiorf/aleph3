#pragma once

#include <string_view>

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"

namespace aleph3 {

bool is_builtin_evaluator_function(std::string_view name);
ExprPtr evaluate_builtin_function(const FunctionCall& func, EvaluationContext& ctx);

}  // namespace aleph3
