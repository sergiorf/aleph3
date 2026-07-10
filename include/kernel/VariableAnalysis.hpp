/*
 * Kernel Variable Analysis
 * ------------------------
 * Shared free/bound variable analysis and capture-safe substitution helpers.
 */

#pragma once

#include "expr/Expr.hpp"

#include <map>
#include <set>
#include <string>

namespace aleph3::kernel {

using SymbolSet = std::set<std::string>;
using SymbolSubstitutionMap = std::map<std::string, ExprPtr>;

[[nodiscard]] SymbolSet free_variables(const ExprPtr& expr);
[[nodiscard]] SymbolSet bound_variables(const ExprPtr& expr);
[[nodiscard]] bool depends_on(const ExprPtr& expr, const std::string& symbol_name);

[[nodiscard]] ExprPtr substitute_symbols_capture_safe(
    const ExprPtr& expr,
    const SymbolSubstitutionMap& substitutions);

}  // namespace aleph3::kernel
