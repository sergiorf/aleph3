#pragma once
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include <unordered_map>
#include <functional>

namespace aleph3 {
    using SimplifyRule = std::function<ExprPtr(
        const std::vector<ExprPtr>&,
        EvaluationContext&,
        const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>&
    )>;
    extern const std::unordered_map<std::string, SimplifyRule> simplification_rules;
}