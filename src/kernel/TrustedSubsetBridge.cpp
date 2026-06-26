#include "kernel/TrustedSubsetBridge.hpp"

namespace aleph3::kernel {

StagedTrustedSubsetFormula stage_trusted_subset_formula(const ir::NodePtr& root) {
    StagedTrustedSubsetFormula staged;
    staged.trusted_root = root;

    // Migration-only: preserve the validated trusted-subset tree while the SDK
    // runtime still exists, but establish the lowered kernel Expr beside it so
    // later execution migration does not need ad hoc conversions.
    auto lowered = lower_trusted_ir_to_expr(root);
    staged.kernel_expr = std::move(lowered.expr);
    staged.diagnostics = std::move(lowered.diagnostics);
    return staged;
}

}  // namespace aleph3::kernel
