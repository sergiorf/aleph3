#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace aleph3 {

enum class EvaluationMode {
    Eager,
    HoldRest
};

struct FunctionSemantics {
    EvaluationMode evaluation_mode = EvaluationMode::Eager;
    bool special_form = false;
    bool listable = false;
    bool comparison = false;
    std::optional<size_t> min_arity;
    std::optional<size_t> max_arity;
};

const FunctionSemantics* lookup_function_semantics(const std::string& name);
bool function_supports_arity(const std::string& name, size_t arity);
bool validate_function_arity(const std::string& name, size_t arity);

bool is_special_form_function(const std::string& name);
bool is_listable_function(const std::string& name);
bool is_comparison_function(const std::string& name);

}  // namespace aleph3
