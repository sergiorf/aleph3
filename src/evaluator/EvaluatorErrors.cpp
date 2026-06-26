#include "evaluator/EvaluatorErrors.hpp"

#include <utility>

namespace aleph3 {

EvaluatorError::EvaluatorError(EvaluatorErrorKind kind, std::string message)
    : std::runtime_error(std::move(message)), kind_(kind) {}

EvaluatorErrorKind EvaluatorError::kind() const noexcept {
    return kind_;
}

kernel::ErrorCode EvaluatorError::code() const noexcept {
    return kernel::kernel_error_code_for(kind_);
}

std::string_view EvaluatorError::code_string() const noexcept {
    return kernel::kernel_error_code_name(code());
}

[[noreturn]] void throw_evaluator_error(EvaluatorErrorKind kind, std::string message) {
    throw EvaluatorError(kind, std::move(message));
}

[[noreturn]] void throw_invalid_form(std::string message) {
    throw_evaluator_error(EvaluatorErrorKind::invalid_form, std::move(message));
}

[[noreturn]] void throw_domain_violation(std::string message) {
    throw_evaluator_error(EvaluatorErrorKind::domain_violation, std::move(message));
}

[[noreturn]] void throw_unsupported_construct(std::string message) {
    throw_evaluator_error(EvaluatorErrorKind::unsupported_construct, std::move(message));
}

[[noreturn]] void throw_internal_inconsistency(std::string message) {
    throw_evaluator_error(EvaluatorErrorKind::internal_inconsistency, std::move(message));
}

[[noreturn]] void throw_invalid_arity_exact(const std::string& head, size_t expected) {
    throw_evaluator_error(
        EvaluatorErrorKind::invalid_arity,
        head + " expects exactly " + std::to_string(expected) +
            " argument" + (expected == 1 ? "" : "s"));
}

[[noreturn]] void throw_invalid_arity_between(
    const std::string& head,
    size_t min_expected,
    size_t max_expected) {
    throw_evaluator_error(
        EvaluatorErrorKind::invalid_arity,
        head + " expects between " + std::to_string(min_expected) +
            " and " + std::to_string(max_expected) + " arguments");
}

[[noreturn]] void throw_invalid_arity_at_least(
    const std::string& head,
    size_t min_expected,
    size_t actual) {
    throw_evaluator_error(
        EvaluatorErrorKind::invalid_arity,
        head + " expects at least " + std::to_string(min_expected) +
            " arguments, got " + std::to_string(actual));
}

[[noreturn]] void throw_invalid_arity_at_most(
    const std::string& head,
    size_t max_expected,
    size_t actual) {
    throw_evaluator_error(
        EvaluatorErrorKind::invalid_arity,
        head + " expects at most " + std::to_string(max_expected) +
            " arguments, got " + std::to_string(actual));
}

}  // namespace aleph3
