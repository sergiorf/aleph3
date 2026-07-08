#pragma once

#include <string_view>
#include <vector>

#include "expr/Expr.hpp"
#include "syntax/Node.hpp"

namespace aleph3::syntax {

struct SymbolicParseResult {
    ExprPtr expr;
    std::vector<aleph3::Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return expr != nullptr && diagnostics.empty();
    }
};

[[nodiscard]] SymbolicParseResult lower_to_expr(const NodePtr& root);
[[nodiscard]] SymbolicParseResult parse_symbolic_source(std::string_view source);

}  // namespace aleph3::syntax
