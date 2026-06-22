#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "sdk/Types.hpp"

namespace aleph3 {

struct VariableSchema {
    std::string name;
    ValueType type = ValueType::any;
    bool required = false;
};

struct FunctionSchema {
    std::string name;
    FunctionArity arity = FunctionArity::exact(0);
    std::vector<ValueType> parameter_types;
    std::optional<ValueType> return_type;
    bool host_function = true;
};

struct ConstantSchema {
    std::string name;
    std::optional<Value> value;
};

class Schema {
public:
    Schema() = default;

    Schema& allow_variable(VariableSchema variable) {
        variables_[variable.name] = std::move(variable);
        return *this;
    }

    Schema& allow_function(FunctionSchema function) {
        functions_[function.name] = std::move(function);
        return *this;
    }

    Schema& allow_constant(std::string constant_name) {
        constants_.insert(std::move(constant_name));
        return *this;
    }

    Schema& allow_constant(ConstantSchema constant) {
        constants_.insert(constant.name);
        if (constant.value.has_value()) {
            constant_values_[constant.name] = std::move(*constant.value);
        } else {
            constant_values_.erase(constant.name);
        }
        return *this;
    }

    [[nodiscard]] const std::unordered_map<std::string, VariableSchema>& variables() const noexcept {
        return variables_;
    }

    [[nodiscard]] const std::unordered_map<std::string, FunctionSchema>& functions() const noexcept {
        return functions_;
    }

    [[nodiscard]] const std::unordered_set<std::string>& constants() const noexcept {
        return constants_;
    }

    [[nodiscard]] const std::unordered_map<std::string, Value>& constant_values() const noexcept {
        return constant_values_;
    }

    [[nodiscard]] bool allows_unknown_variables() const noexcept {
        return allow_unknown_variables_;
    }

    [[nodiscard]] bool allows_unknown_functions() const noexcept {
        return allow_unknown_functions_;
    }

    void set_allow_unknown_variables(bool allow) noexcept {
        allow_unknown_variables_ = allow;
    }

    void set_allow_unknown_functions(bool allow) noexcept {
        allow_unknown_functions_ = allow;
    }

private:
    std::unordered_map<std::string, VariableSchema> variables_;
    std::unordered_map<std::string, FunctionSchema> functions_;
    std::unordered_set<std::string> constants_;
    std::unordered_map<std::string, Value> constant_values_;
    bool allow_unknown_variables_ = false;
    bool allow_unknown_functions_ = false;
};

}  // namespace aleph3
