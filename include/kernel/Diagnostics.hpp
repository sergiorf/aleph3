/*
 * Kernel Diagnostics
 * ------------------
 * Canonical kernel-level error taxonomy and projection helpers for current
 * SDK/runtime-facing error surfaces.
 */

#pragma once

#include <optional>
#include <string_view>

#include "sdk/Types.hpp"

namespace aleph3 {
enum class EvaluatorErrorKind;
}

namespace aleph3::kernel {

enum class ErrorCode {
    invalid_form,
    invalid_arity,
    domain_violation,
    unsupported_construct,
    internal_inconsistency,
    unknown_binding,
    type_mismatch,
    division_by_zero,
    non_finite_number,
    invalid_numeric_result,
    invalid_numeric_domain,
    invalid_power_domain,
    invalid_call,
    invalid_argument_type,
    invalid_host_result,
    unknown_function,
    step_budget_exhausted,
    empty_ir,
    empty_node,
    unsupported_node,
    unsupported_operator
};

[[nodiscard]] constexpr std::string_view kernel_error_code_name(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::invalid_form:
            return "kernel.invalid_form";
        case ErrorCode::invalid_arity:
            return "kernel.invalid_arity";
        case ErrorCode::domain_violation:
            return "kernel.domain_violation";
        case ErrorCode::unsupported_construct:
            return "kernel.unsupported_construct";
        case ErrorCode::internal_inconsistency:
            return "kernel.internal_inconsistency";
        case ErrorCode::unknown_binding:
            return "kernel.unknown_binding";
        case ErrorCode::type_mismatch:
            return "kernel.type_mismatch";
        case ErrorCode::division_by_zero:
            return "kernel.division_by_zero";
        case ErrorCode::non_finite_number:
            return "kernel.non_finite_number";
        case ErrorCode::invalid_numeric_result:
            return "kernel.invalid_numeric_result";
        case ErrorCode::invalid_numeric_domain:
            return "kernel.invalid_numeric_domain";
        case ErrorCode::invalid_power_domain:
            return "kernel.invalid_power_domain";
        case ErrorCode::invalid_call:
            return "kernel.invalid_call";
        case ErrorCode::invalid_argument_type:
            return "kernel.invalid_argument_type";
        case ErrorCode::invalid_host_result:
            return "kernel.invalid_host_result";
        case ErrorCode::unknown_function:
            return "kernel.unknown_function";
        case ErrorCode::step_budget_exhausted:
            return "kernel.step_budget_exhausted";
        case ErrorCode::empty_ir:
            return "kernel.empty_ir";
        case ErrorCode::empty_node:
            return "kernel.empty_node";
        case ErrorCode::unsupported_node:
            return "kernel.unsupported_node";
        case ErrorCode::unsupported_operator:
            return "kernel.unsupported_operator";
    }
    return "kernel.internal_inconsistency";
}

[[nodiscard]] constexpr std::string_view sdk_runtime_projection_code(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::invalid_form:
            return "runtime.invalid_form";
        case ErrorCode::invalid_arity:
            return "runtime.invalid_arity";
        case ErrorCode::domain_violation:
            return "runtime.domain_violation";
        case ErrorCode::unsupported_construct:
            return "runtime.unsupported_construct";
        case ErrorCode::internal_inconsistency:
            return "runtime.internal_inconsistency";
        case ErrorCode::unknown_binding:
            return "runtime.unknown_binding";
        case ErrorCode::type_mismatch:
            return "runtime.type_mismatch";
        case ErrorCode::division_by_zero:
            return "runtime.division_by_zero";
        case ErrorCode::non_finite_number:
            return "runtime.non_finite_number";
        case ErrorCode::invalid_numeric_result:
            return "runtime.invalid_numeric_result";
        case ErrorCode::invalid_numeric_domain:
            return "runtime.invalid_numeric_domain";
        case ErrorCode::invalid_power_domain:
            return "runtime.invalid_power_domain";
        case ErrorCode::invalid_call:
            return "runtime.invalid_call";
        case ErrorCode::invalid_argument_type:
            return "runtime.invalid_argument_type";
        case ErrorCode::invalid_host_result:
            return "runtime.invalid_host_result";
        case ErrorCode::unknown_function:
            return "runtime.unknown_function";
        case ErrorCode::step_budget_exhausted:
            return "runtime.step_budget_exhausted";
        case ErrorCode::empty_ir:
            return "runtime.empty_ir";
        case ErrorCode::empty_node:
            return "runtime.empty_node";
        case ErrorCode::unsupported_node:
            return "runtime.unsupported_node";
        case ErrorCode::unsupported_operator:
            return "runtime.unsupported_operator";
    }
    return "runtime.internal_inconsistency";
}

[[nodiscard]] inline RuntimeError make_runtime_error(
    ErrorCode code,
    std::string message,
    std::optional<SourceSpan> span = std::nullopt) {
    RuntimeError error;
    error.code = std::string(sdk_runtime_projection_code(code));
    error.message = std::move(message);
    error.span = std::move(span);
    return error;
}

[[nodiscard]] constexpr ErrorCode kernel_error_code_for(EvaluatorErrorKind kind) noexcept;

}  // namespace aleph3::kernel
