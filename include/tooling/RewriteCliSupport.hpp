#pragma once

#include "sdk/Types.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aleph3::tooling {

struct EvaluateCommandOptions {
    Bindings bindings;
    std::string formula;
};

struct EvaluateCommandParseResult {
    EvaluateCommandOptions options;
    std::string error_message;

    [[nodiscard]] bool ok() const noexcept {
        return error_message.empty();
    }
};

[[nodiscard]] EvaluateCommandParseResult parse_formula_cli_arguments(
    std::string_view command_name,
    std::span<const std::string_view> arguments);

[[nodiscard]] EvaluateCommandParseResult parse_formula_repl_arguments(
    std::string_view command_name,
    std::string_view input);

[[nodiscard]] EvaluateCommandParseResult parse_evaluate_cli_arguments(
    std::span<const std::string_view> arguments);

[[nodiscard]] EvaluateCommandParseResult parse_evaluate_repl_arguments(
    std::string_view input);

[[nodiscard]] ValueType infer_value_type(const Value& value) noexcept;

}  // namespace aleph3::tooling
