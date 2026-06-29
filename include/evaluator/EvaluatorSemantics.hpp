/*
 * Evaluator Semantics Registry
 * ----------------------------
 * Describes frontend-facing arity and evaluation-mode metadata for known
 * symbolic heads without hardwiring extra dispatch paths per subsystem.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace aleph3 {

enum class EvaluationMode {
    Eager,
    HoldRest,
    HoldAll
};

enum class DispatchKind {
    Default,
    Structural,
    SpecialForm
};

struct FunctionSemantics {
    EvaluationMode evaluation_mode = EvaluationMode::Eager;
    DispatchKind dispatch_kind = DispatchKind::Default;
    bool special_form = false;
    bool listable = false;
    bool comparison = false;
    bool numeric_function = false;
    bool orderless = false;
    bool flat = false;
    std::optional<size_t> min_arity;
    std::optional<size_t> max_arity;
};

const FunctionSemantics* lookup_function_semantics(const std::string& name);
bool function_supports_arity(const std::string& name, size_t arity);
bool validate_function_arity(const std::string& name, size_t arity);

bool is_special_form_function(const std::string& name);
bool is_listable_function(const std::string& name);
bool is_comparison_function(const std::string& name);
bool is_numeric_function(const std::string& name);
bool is_orderless_function(const std::string& name);
bool is_flat_function(const std::string& name);
bool is_structural_function(const std::string& name);

}  // namespace aleph3
