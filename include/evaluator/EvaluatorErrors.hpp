#pragma once

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

}  // namespace aleph3
