#pragma once

#include "evaluator/EvaluationContext.hpp"
#include "expr/Expr.hpp"

namespace aleph3 {

bool is_user_defined_function(const std::string& name, const EvaluationContext& ctx);

ExprPtr evaluate_user_defined_function(const FunctionCall& func, EvaluationContext& ctx);

ExprPtr register_user_defined_function(const FunctionDefinition& def, EvaluationContext& ctx);

}  // namespace aleph3
