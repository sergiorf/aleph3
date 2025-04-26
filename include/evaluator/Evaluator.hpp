#pragma once

#include "expr/Expr.hpp"

namespace mathix {

// Evaluate an expression (returns a new simplified expression)
ExprPtr evaluate(const ExprPtr& expr);

// Internal helpers (optional to expose later)
ExprPtr evaluate_function(const FunctionCall& func);

} // namespace mathix
