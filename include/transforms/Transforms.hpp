#pragma once
#include "expr/Expr.hpp"

namespace aleph3 {

    ExprPtr simplify(const ExprPtr& expr);

    ExprPtr expand(const ExprPtr& expr);

}