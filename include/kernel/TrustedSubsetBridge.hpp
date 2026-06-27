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
#include "kernel/FunctionRegistry.hpp"
#include "kernel/Lowering.hpp"
#include "sdk/Policy.hpp"
#include "sdk/Types.hpp"

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

[[nodiscard]] EvaluationResult evaluate_trusted_subset_formula(
    const ExprPtr& kernel_expr,
    const Bindings& bindings,
    const Bindings& constants,
    const HostFunctionRegistry& host_functions,
    const Policy& policy);

}  // namespace aleph3::kernel
