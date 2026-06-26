#pragma once

#include "kernel/Diagnostics.hpp"

#include <stdexcept>
#include <string>

namespace aleph3 {

enum class EvaluatorErrorKind {
    invalid_form,
    invalid_arity,
    domain_violation,
    unsupported_construct,
    internal_inconsistency
};

class EvaluatorError : public std::runtime_error {
public:
    EvaluatorError(EvaluatorErrorKind kind, std::string message);

    [[nodiscard]] EvaluatorErrorKind kind() const noexcept;
    [[nodiscard]] kernel::ErrorCode code() const noexcept;
    [[nodiscard]] std::string_view code_string() const noexcept;

private:
    EvaluatorErrorKind kind_;
};

[[noreturn]] void throw_evaluator_error(EvaluatorErrorKind kind, std::string message);
[[noreturn]] void throw_invalid_form(std::string message);
[[noreturn]] void throw_domain_violation(std::string message);
[[noreturn]] void throw_unsupported_construct(std::string message);
[[noreturn]] void throw_internal_inconsistency(std::string message);
[[noreturn]] void throw_invalid_arity_exact(const std::string& head, size_t expected);
[[noreturn]] void throw_invalid_arity_between(const std::string& head, size_t min_expected, size_t max_expected);
[[noreturn]] void throw_invalid_arity_at_least(const std::string& head, size_t min_expected, size_t actual);
[[noreturn]] void throw_invalid_arity_at_most(const std::string& head, size_t max_expected, size_t actual);

namespace kernel {

[[nodiscard]] constexpr ErrorCode kernel_error_code_for(EvaluatorErrorKind kind) noexcept {
    switch (kind) {
        case EvaluatorErrorKind::invalid_form:
            return ErrorCode::invalid_form;
        case EvaluatorErrorKind::invalid_arity:
            return ErrorCode::invalid_arity;
        case EvaluatorErrorKind::domain_violation:
            return ErrorCode::domain_violation;
        case EvaluatorErrorKind::unsupported_construct:
            return ErrorCode::unsupported_construct;
        case EvaluatorErrorKind::internal_inconsistency:
            return ErrorCode::internal_inconsistency;
    }
    return ErrorCode::internal_inconsistency;
}

}  // namespace kernel

}  // namespace aleph3
