#pragma once

#include "expr/Expr.hpp"

#include <optional>

namespace aleph3 {

std::optional<ExprPtr> simplify_gamma_argument(const ExprPtr& arg);

}  // namespace aleph3
