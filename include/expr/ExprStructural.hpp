#pragma once

#include "expr/Expr.hpp"

namespace aleph3 {

[[nodiscard]] bool structural_equal(
    const ExprPtr& lhs,
    const ExprPtr& rhs) noexcept;

struct ExprEqual {
    bool operator()(const ExprPtr& lhs, const ExprPtr& rhs) const noexcept {
        return structural_equal(lhs, rhs);
    }
};

}  // namespace aleph3
