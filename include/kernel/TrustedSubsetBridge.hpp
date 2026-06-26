/*
 * Trusted Subset Bridge
 * ---------------------
 * Migration-only staging adapter between the SDK trusted-subset frontend and
 * the kernel expression model. This bridge is temporary scaffolding while SDK
 * execution collapses onto kernel semantics.
 */

#pragma once

#include "expr/Expr.hpp"
#include "ir/Node.hpp"
#include "kernel/Lowering.hpp"

namespace aleph3::kernel {

struct StagedTrustedSubsetFormula {
    ir::NodePtr trusted_root;
    ExprPtr kernel_expr;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return trusted_root != nullptr && kernel_expr != nullptr && diagnostics.empty();
    }
};

[[nodiscard]] StagedTrustedSubsetFormula stage_trusted_subset_formula(const ir::NodePtr& root);

}  // namespace aleph3::kernel
