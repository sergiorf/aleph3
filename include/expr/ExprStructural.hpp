#pragma once

#include "expr/Expr.hpp"

#include <cstddef>

namespace aleph3 {

[[nodiscard]] bool structural_equal(
    const ExprPtr& lhs,
    const ExprPtr& rhs) noexcept;

[[nodiscard]] std::size_t structural_hash(const ExprPtr& expr) noexcept;

[[nodiscard]] bool structural_less(
    const ExprPtr& lhs,
    const ExprPtr& rhs) noexcept;

struct ExprEqual {
    bool operator()(const ExprPtr& lhs, const ExprPtr& rhs) const noexcept {
        return structural_equal(lhs, rhs);
    }
};

struct ExprHash {
    std::size_t operator()(const ExprPtr& expr) const noexcept {
        return structural_hash(expr);
    }
};

struct ExprStructuralLess {
    bool operator()(const ExprPtr& lhs, const ExprPtr& rhs) const noexcept {
        return structural_less(lhs, rhs);
    }
};

}  // namespace aleph3
