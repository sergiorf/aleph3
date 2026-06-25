#pragma once

#include <unordered_map>
#include <string>
#include "expr/Expr.hpp"
#include "symbols/SymbolState.hpp"

namespace aleph3 {

struct EvaluationContext {
    symbols::SymbolValueTable symbol_values;
    symbols::FunctionDefinitionTable function_definitions;

    std::unordered_map<std::string, ExprPtr>& variables;
    std::unordered_map<std::string, FunctionDefinition>& user_functions;

    EvaluationContext()
        : variables(symbol_values.entries()),
          user_functions(function_definitions.entries()) {}

    EvaluationContext(const EvaluationContext& other)
        : symbol_values(other.symbol_values),
          function_definitions(other.function_definitions),
          variables(symbol_values.entries()),
          user_functions(function_definitions.entries()) {}

    EvaluationContext(EvaluationContext&& other) noexcept
        : symbol_values(std::move(other.symbol_values)),
          function_definitions(std::move(other.function_definitions)),
          variables(symbol_values.entries()),
          user_functions(function_definitions.entries()) {}

    EvaluationContext& operator=(const EvaluationContext& other) {
        if (this != &other) {
            symbol_values = other.symbol_values;
            function_definitions = other.function_definitions;
        }
        return *this;
    }

    EvaluationContext& operator=(EvaluationContext&& other) noexcept {
        if (this != &other) {
            symbol_values = std::move(other.symbol_values);
            function_definitions = std::move(other.function_definitions);
        }
        return *this;
    }
};

}
