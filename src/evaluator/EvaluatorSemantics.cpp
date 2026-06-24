#include "evaluator/EvaluatorSemantics.hpp"

#include <unordered_map>

namespace aleph3 {

namespace {

FunctionSemantics exact_arity_semantics(
    EvaluationMode mode,
    bool special_form,
    bool listable,
    bool comparison,
    size_t arity) {
    return FunctionSemantics{
        mode,
        special_form,
        listable,
        comparison,
        arity,
        arity
    };
}

FunctionSemantics arity_range_semantics(
    EvaluationMode mode,
    bool special_form,
    bool listable,
    bool comparison,
    size_t min_arity,
    size_t max_arity) {
    return FunctionSemantics{
        mode,
        special_form,
        listable,
        comparison,
        min_arity,
        max_arity
    };
}

const std::unordered_map<std::string, FunctionSemantics>& function_semantics_registry() {
    static const std::unordered_map<std::string, FunctionSemantics> value = {
        {"If", exact_arity_semantics(EvaluationMode::HoldRest, true, false, false, 3)},

        {"Sin", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Cos", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Tan", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Sinc", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Csc", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Sec", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Sinh", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Cosh", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Tanh", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Coth", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Sech", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Csch", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Cot", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Abs", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Sqrt", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Exp", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Ln", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Log", arity_range_semantics(EvaluationMode::Eager, false, true, false, 1, 2)},
        {"Floor", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Ceil", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Ceiling", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Round", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"ArcSin", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"ArcCos", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"ArcTan", arity_range_semantics(EvaluationMode::Eager, false, true, false, 1, 2)},
        {"ArcSec", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"ArcCsc", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"ArcCot", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Gamma", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 1)},
        {"Plus", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 2)},
        {"Minus", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 2)},
        {"Times", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 2)},
        {"Divide", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 2)},
        {"Power", exact_arity_semantics(EvaluationMode::Eager, false, true, false, 2)},
        {"Equal", exact_arity_semantics(EvaluationMode::Eager, false, false, true, 2)},
        {"NotEqual", exact_arity_semantics(EvaluationMode::Eager, false, false, true, 2)},
        {"Less", exact_arity_semantics(EvaluationMode::Eager, false, false, true, 2)},
        {"Greater", exact_arity_semantics(EvaluationMode::Eager, false, false, true, 2)},
        {"LessEqual", exact_arity_semantics(EvaluationMode::Eager, false, false, true, 2)},
        {"GreaterEqual", exact_arity_semantics(EvaluationMode::Eager, false, false, true, 2)}
    };
    return value;
}

}  // namespace

const FunctionSemantics* lookup_function_semantics(const std::string& name) {
    const auto& registry = function_semantics_registry();
    auto it = registry.find(name);
    if (it == registry.end()) {
        return nullptr;
    }
    return &it->second;
}

bool function_supports_arity(const std::string& name, size_t arity) {
    const auto* semantics = lookup_function_semantics(name);
    if (!semantics) {
        return false;
    }
    if (semantics->min_arity && arity < *semantics->min_arity) {
        return false;
    }
    if (semantics->max_arity && arity > *semantics->max_arity) {
        return false;
    }
    return true;
}

bool validate_function_arity(const std::string& name, size_t arity) {
    return function_supports_arity(name, arity);
}

bool is_special_form_function(const std::string& name) {
    const auto* semantics = lookup_function_semantics(name);
    return semantics && semantics->special_form;
}

bool is_listable_function(const std::string& name) {
    const auto* semantics = lookup_function_semantics(name);
    return semantics && semantics->listable;
}

bool is_comparison_function(const std::string& name) {
    const auto* semantics = lookup_function_semantics(name);
    return semantics && semantics->comparison;
}

}  // namespace aleph3
