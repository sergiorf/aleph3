#pragma once

#include <vector>

#include "ir/Node.hpp"
#include "syntax/Node.hpp"

namespace aleph3::syntax {

struct TrustedSubsetLoweringResult {
    ir::NodePtr root;
    std::vector<aleph3::Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return root != nullptr && diagnostics.empty();
    }
};

[[nodiscard]] TrustedSubsetLoweringResult lower_to_trusted_ir(const NodePtr& root);

}  // namespace aleph3::syntax
