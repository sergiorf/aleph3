#include "kernel/SymbolAttributes.hpp"

#include <algorithm>
#include <utility>

#include "kernel/EvaluationContext.hpp"

namespace aleph3::kernel {

namespace {

void append_unique_attributes(
    std::vector<symbols::SymbolAttribute>& target,
    const std::vector<symbols::SymbolAttribute>& source) {
    for (const auto attribute : source) {
        if (std::find(target.begin(), target.end(), attribute) == target.end()) {
            target.push_back(attribute);
        }
    }
}

}  // namespace

std::vector<symbols::SymbolAttribute> evaluation_control_attributes(EvaluationMode mode) {
    switch (mode) {
        case EvaluationMode::HoldFirst:
            return {symbols::SymbolAttribute::hold_first};
        case EvaluationMode::HoldRest:
            return {symbols::SymbolAttribute::hold_rest};
        case EvaluationMode::HoldAll:
            return {symbols::SymbolAttribute::hold_all};
        case EvaluationMode::Eager:
            return {};
    }
    return {};
}

std::vector<symbols::SymbolAttribute> builtin_symbol_attributes(std::string_view name) {
    std::vector<symbols::SymbolAttribute> attributes;
    const auto* semantics = lookup_function_semantics(std::string(name));
    if (semantics == nullptr) {
        return attributes;
    }

    append_unique_attributes(attributes, evaluation_control_attributes(semantics->evaluation_mode));
    if (semantics->listable) {
        attributes.push_back(symbols::SymbolAttribute::listable);
    }
    if (semantics->numeric_function) {
        attributes.push_back(symbols::SymbolAttribute::numeric_function);
    }
    return attributes;
}

void sync_symbol_attribute_metadata(
    EvaluationContext& ctx,
    const std::string& name,
    symbols::DefinitionOrigin origin,
    std::string provider,
    const std::vector<symbols::SymbolAttribute>& attributes,
    std::string documentation) {
    if (auto* existing = ctx.symbol_metadata.lookup(name)) {
        append_unique_attributes(existing->attributes, attributes);
        if (existing->documentation.empty() && !documentation.empty()) {
            existing->documentation = std::move(documentation);
        }
        if (existing->provider.empty() && !provider.empty()) {
            existing->provider = std::move(provider);
        }
        if (existing->origin == symbols::DefinitionOrigin::user &&
            origin != symbols::DefinitionOrigin::user) {
            existing->origin = origin;
        }
        return;
    }

    ctx.symbol_metadata.set(name, symbols::SymbolMetadata{
        name,
        attributes,
        std::move(documentation),
        origin,
        std::move(provider)});
}

}  // namespace aleph3::kernel
