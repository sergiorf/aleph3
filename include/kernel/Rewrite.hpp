/*
 * Kernel Rewrite
 * --------------
 * Minimal kernel-owned rewrite entrypoints for structural and first-pattern
 * rule application.
 */

#pragma once

#include <cstddef>

#include "expr/Expr.hpp"

namespace aleph3::kernel {

class EvaluationContext;

struct RewriteResult {
    ExprPtr expr;
    bool changed = false;
    std::size_t rewrites_applied = 0;
};

[[nodiscard]] bool structurally_equal(const ExprPtr& left, const ExprPtr& right);

[[nodiscard]] RewriteResult rewrite_once(const ExprPtr& expr, const Rule& rule);

[[nodiscard]] RewriteResult rewrite_repeated(
    const ExprPtr& expr,
    const Rule& rule,
    std::size_t max_rewrites = 16);

[[nodiscard]] RewriteResult rewrite_repeated(
    const ExprPtr& expr,
    const Rule& rule,
    EvaluationContext& ctx,
    std::size_t max_rewrites = 16);

}  // namespace aleph3::kernel
