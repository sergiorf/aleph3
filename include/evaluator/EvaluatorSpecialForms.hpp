/*
 * Evaluator Special Forms
 * -----------------------
 * Implements and registers builtin heads whose handlers control argument
 * evaluation, including conditional and boolean short-circuit forms.
 */

#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"

namespace aleph3 {

namespace kernel {
class FunctionRegistry;
}

bool is_special_form_function(const std::string& name);

ExprPtr evaluate_special_form(const FunctionCall& func, EvaluationContext& ctx);

void register_special_forms(kernel::FunctionRegistry& registry);

}  // namespace aleph3
