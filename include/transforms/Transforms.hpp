#pragma once
#include "expr/Expr.hpp"

namespace mathix {

    ExprPtr simplify(const ExprPtr& expr);

    ExprPtr expand(const ExprPtr& expr);

}