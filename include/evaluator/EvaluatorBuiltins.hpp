#pragma once

#include <string_view>

#include "kernel/FunctionRegistry.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"

namespace aleph3 {

void register_builtin_evaluator_execution_specs(kernel::FunctionRegistry& registry);
bool is_builtin_evaluator_function(std::string_view name, const kernel::FunctionRegistry& registry);
ExprPtr evaluate_builtin_function(const FunctionCall& func, EvaluationContext& ctx);

}  // namespace aleph3
