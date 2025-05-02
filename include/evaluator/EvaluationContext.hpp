#pragma once

#include <unordered_map>
#include <string>
#include "expr/Expr.hpp"

namespace mathix {

struct EvaluationContext {
    std::unordered_map<std::string, ExprPtr> variables;
    std::unordered_map<std::string, FunctionDefinition> user_functions;
};

} // namespace mathix
