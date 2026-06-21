#pragma once

#include <vector>

#include "ir/Node.hpp"
#include "sdk/Policy.hpp"
#include "sdk/Schema.hpp"
#include "sdk/Types.hpp"

namespace aleph3::semantics {

struct ValidationSummary {
    std::vector<aleph3::Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return diagnostics.empty();
    }
};

class Validator {
public:
    Validator(const Schema& schema, const Policy& policy);

    [[nodiscard]] ValidationSummary validate(const ir::NodePtr& root) const;

private:
    const Schema& schema_;
    const Policy& policy_;
};

}  // namespace aleph3::semantics
