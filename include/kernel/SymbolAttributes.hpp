/*
 * Kernel Symbol Attributes
 * ------------------------
 * Small kernel-owned helpers for turning evaluator-facing attribute contracts
 * into shared symbol metadata. This slice only makes evaluation-control
 * attributes explicit and stable; it does not make attributes a general
 * dispatch mechanism.
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "evaluator/EvaluatorSemantics.hpp"
#include "symbols/SymbolState.hpp"

namespace aleph3::kernel {

class EvaluationContext;

[[nodiscard]] std::vector<symbols::SymbolAttribute> evaluation_control_attributes(EvaluationMode mode);
[[nodiscard]] std::vector<symbols::SymbolAttribute> builtin_symbol_attributes(std::string_view name);

void sync_symbol_attribute_metadata(
    EvaluationContext& ctx,
    const std::string& name,
    symbols::DefinitionOrigin origin,
    std::string provider,
    const std::vector<symbols::SymbolAttribute>& attributes,
    std::string documentation = {});

}  // namespace aleph3::kernel
