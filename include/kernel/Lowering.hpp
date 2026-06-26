/*
 * Kernel Lowering Boundary
 * ------------------------
 * Migration-only adapter for lowering trusted-subset SDK IR into the kernel
 * expression model. This establishes `Expr` as the semantic execution form
 * while keeping `ir::Node` narrow and frontend-owned.
 */

#pragma once

#include <vector>

#include "expr/Expr.hpp"
#include "ir/Node.hpp"
#include "sdk/Types.hpp"

namespace aleph3::kernel {

struct LoweringResult {
    ExprPtr expr;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return expr != nullptr && diagnostics.empty();
    }
};

[[nodiscard]] LoweringResult lower_trusted_ir_to_expr(const ir::NodePtr& root);

}  // namespace aleph3::kernel
