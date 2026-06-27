/*
 * Kernel Rewrite
 * --------------
 * Minimal kernel-owned rewrite entrypoints for exact structural rule
 * application. This is the first rewrite subsystem slice, not a full pattern
 * matcher.
 */

#pragma once

#include <cstddef>

#include "expr/Expr.hpp"

namespace aleph3::kernel {

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

}  // namespace aleph3::kernel
