#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace aleph3 {

struct SourceSpan {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;

    [[nodiscard]] constexpr bool empty() const noexcept {
        return start_offset == end_offset;
    }
};

enum class DiagnosticSeverity {
    error,
    warning,
    note
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::error;
    std::string code;
    std::string message;
    SourceSpan span;
};

class Value {
public:
    using List = std::vector<Value>;
    using Storage = std::variant<std::monostate, double, bool, std::string, List>;

    Value() = default;
    Value(double number) : storage_(number) {}
    Value(bool boolean) : storage_(boolean) {}
    Value(std::string string) : storage_(std::move(string)) {}
    Value(const char* string) : storage_(std::string(string)) {}
    Value(List list) : storage_(std::move(list)) {}

    [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<std::monostate>(storage_); }
    [[nodiscard]] bool is_number() const noexcept { return std::holds_alternative<double>(storage_); }
    [[nodiscard]] bool is_boolean() const noexcept { return std::holds_alternative<bool>(storage_); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(storage_); }
    [[nodiscard]] bool is_list() const noexcept { return std::holds_alternative<List>(storage_); }

    [[nodiscard]] const double* as_number() const noexcept { return std::get_if<double>(&storage_); }
    [[nodiscard]] const bool* as_boolean() const noexcept { return std::get_if<bool>(&storage_); }
    [[nodiscard]] const std::string* as_string() const noexcept { return std::get_if<std::string>(&storage_); }
    [[nodiscard]] const List* as_list() const noexcept { return std::get_if<List>(&storage_); }

    [[nodiscard]] const Storage& storage() const noexcept { return storage_; }

private:
    Storage storage_;
};

using Bindings = std::unordered_map<std::string, Value>;

struct RuntimeError {
    std::string code;
    std::string message;
    std::optional<SourceSpan> span;
};

struct EngineOptions {
    bool enable_strings = true;
    bool enable_lists = false;
    bool simplify_before_evaluate = false;
    bool enable_optional_builtins = false;
    bool retain_source_text = true;
};

struct ValidationResult {
    bool ok = true;
    std::vector<Diagnostic> diagnostics;
};

namespace sdk_detail {
struct CompiledFormulaData;
}

class CompiledFormula {
public:
    CompiledFormula() = default;

    [[nodiscard]] bool empty() const noexcept { return !state_; }
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(state_); }

private:
    friend class Engine;

    explicit CompiledFormula(std::shared_ptr<const sdk_detail::CompiledFormulaData> state) noexcept
        : state_(std::move(state)) {}

    std::shared_ptr<const sdk_detail::CompiledFormulaData> state_;
};

struct CompileResult {
    std::optional<CompiledFormula> formula;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return formula.has_value() && diagnostics.empty();
    }
};

struct EvaluationResult {
    std::optional<Value> value;
    std::optional<RuntimeError> error;

    [[nodiscard]] bool ok() const noexcept {
        return value.has_value() && !error.has_value();
    }
};

enum class ValueType {
    any,
    number,
    boolean,
    string,
    list
};

struct FunctionArity {
    std::size_t min_arguments = 0;
    std::size_t max_arguments = 0;

    [[nodiscard]] static constexpr FunctionArity exact(std::size_t count) noexcept {
        return FunctionArity{count, count};
    }

    [[nodiscard]] constexpr bool allows(std::size_t count) const noexcept {
        return min_arguments <= count && count <= max_arguments;
    }
};

enum class HostFunctionPurity {
    pure,
    impure
};

struct HostFunctionParameter {
    std::string name;
    ValueType type = ValueType::any;
    bool required = true;
};

using HostFunctionCallback = std::function<EvaluationResult(std::span<const Value>)>;

struct HostFunctionSpec {
    std::string name;
    FunctionArity arity = FunctionArity::exact(0);
    std::vector<HostFunctionParameter> parameters;
    std::optional<ValueType> return_type;
    HostFunctionPurity purity = HostFunctionPurity::pure;
    HostFunctionCallback callback;
    std::string description;
};

}  // namespace aleph3
