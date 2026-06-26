#include "runtime/Evaluator.hpp"

#include "kernel/Diagnostics.hpp"
#include "kernel/FunctionRegistry.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace aleph3::runtime {

namespace {

struct StepBudget {
    std::size_t used = 0;
    std::size_t max_steps = 0;
};

RuntimeError make_error(std::string code, std::string message, std::optional<SourceSpan> span = std::nullopt) {
    RuntimeError error;
    error.code = std::move(code);
    error.message = std::move(message);
    error.span = std::move(span);
    return error;
}

EvaluationResult make_success(Value value) {
    EvaluationResult result;
    result.value = std::move(value);
    return result;
}

EvaluationResult make_failure(std::string code, std::string message, std::optional<SourceSpan> span = std::nullopt) {
    EvaluationResult result;
    result.error = make_error(std::move(code), std::move(message), std::move(span));
    return result;
}

bool is_finite_number(double value) noexcept {
    return std::isfinite(value);
}

bool is_integer_value(double value) noexcept {
    return std::floor(value) == value;
}

EvaluationResult make_non_finite_input_error(const SourceSpan& span) {
    EvaluationResult result;
    result.error = kernel::make_runtime_error(
        kernel::ErrorCode::non_finite_number,
        "Numeric operations require finite input values.",
        span);
    return result;
}

EvaluationResult make_non_finite_result_error(const SourceSpan& span) {
    EvaluationResult result;
    result.error = kernel::make_runtime_error(
        kernel::ErrorCode::invalid_numeric_result,
        "Numeric evaluation produced a non-finite result.",
        span);
    return result;
}

EvaluationResult make_invalid_power_domain_error(const SourceSpan& span) {
    EvaluationResult result;
    result.error = kernel::make_runtime_error(
        kernel::ErrorCode::invalid_power_domain,
        "Power is undefined for the given numeric inputs.",
        span);
    return result;
}

EvaluationResult make_invalid_numeric_domain_error(
    const std::string& function_name,
    const std::string& message,
    const SourceSpan& span) {
    EvaluationResult result;
    result.error = kernel::make_runtime_error(
        kernel::ErrorCode::invalid_numeric_domain,
        function_name + " " + message,
        span);
    return result;
}

EvaluationResult make_finite_numeric_success(double value, const SourceSpan& span) {
    if (!is_finite_number(value)) {
        return make_non_finite_result_error(span);
    }
    if (value == 0.0) {
        value = std::fabs(value);
    }
    return make_success(Value(value));
}

std::string value_type_name(ValueType type) {
    switch (type) {
        case ValueType::any:
            return "any";
        case ValueType::number:
            return "number";
        case ValueType::boolean:
            return "boolean";
        case ValueType::string:
            return "string";
        case ValueType::list:
            return "list";
    }
    return "any";
}

std::string value_type_name(const Value& value) {
    if (value.is_number()) {
        return "number";
    }
    if (value.is_boolean()) {
        return "boolean";
    }
    if (value.is_string()) {
        return "string";
    }
    if (value.is_list()) {
        return "list";
    }
    return "null";
}

bool matches_value_type(const Value& value, ValueType expected) noexcept {
    switch (expected) {
        case ValueType::any:
            return true;
        case ValueType::number:
            return value.is_number();
        case ValueType::boolean:
            return value.is_boolean();
        case ValueType::string:
            return value.is_string();
        case ValueType::list:
            return value.is_list();
    }
    return false;
}

enum class EqualityResultKind {
    equal,
    incomparable_types,
    non_finite_numeric_input,
};

struct EqualityResult {
    EqualityResultKind kind;
    bool value = false;
};

EqualityResult values_equal(const Value& left, const Value& right) {
    if (const auto* left_number = left.as_number()) {
        if (const auto* right_number = right.as_number()) {
            if (!is_finite_number(*left_number) || !is_finite_number(*right_number)) {
                return {EqualityResultKind::non_finite_numeric_input};
            }
            return {EqualityResultKind::equal, *left_number == *right_number};
        }
        return {EqualityResultKind::incomparable_types};
    }
    if (const auto* left_boolean = left.as_boolean()) {
        if (const auto* right_boolean = right.as_boolean()) {
            return {EqualityResultKind::equal, *left_boolean == *right_boolean};
        }
        return {EqualityResultKind::incomparable_types};
    }
    if (const auto* left_string = left.as_string()) {
        if (const auto* right_string = right.as_string()) {
            return {EqualityResultKind::equal, *left_string == *right_string};
        }
        return {EqualityResultKind::incomparable_types};
    }
    if (left.is_null() && right.is_null()) {
        return {EqualityResultKind::equal, true};
    }
    return {EqualityResultKind::incomparable_types};
}

class EvaluatorImpl {
public:
    explicit EvaluatorImpl(EvaluationContext context)
        : context_(context) {
        budget_.max_steps = context_.policy().budget().max_evaluation_steps;
    }

    EvaluationResult evaluate(const ir::NodePtr& root) {
        if (root == nullptr) {
            return make_failure("runtime.empty_ir", "Cannot evaluate an empty IR tree.");
        }
        return evaluate_node(root);
    }

private:
    EvaluationResult evaluate_node(const ir::NodePtr& node) {
        if (node == nullptr) {
            return make_failure("runtime.empty_node", "Encountered an empty runtime node.");
        }

        if (budget_.used >= budget_.max_steps) {
            return make_failure(
                "runtime.step_budget_exhausted",
                "Evaluation exceeded the configured step budget.",
                node->span);
        }
        ++budget_.used;

        if (const auto* number = node->as<ir::NumberLiteralNode>()) {
            return make_success(Value(number->value));
        }
        if (const auto* boolean = node->as<ir::BooleanLiteralNode>()) {
            return make_success(Value(boolean->value));
        }
        if (const auto* string = node->as<ir::StringLiteralNode>()) {
            return make_success(Value(string->value));
        }
        if (const auto* variable = node->as<ir::VariableNode>()) {
            const auto& bindings = context_.bindings();
            const auto binding = bindings.find(variable->name);
            if (binding != bindings.end()) {
                return make_success(binding->second);
            }

            const auto& constants = context_.constants();
            const auto constant = constants.find(variable->name);
            if (constant != constants.end()) {
                return make_success(constant->second);
            }

            return make_failure(
                "runtime.unknown_binding",
                "No binding was provided for variable `" + variable->name + "`.",
                node->span);
        }
        if (const auto* unary = node->as<ir::UnaryOpNode>()) {
            auto operand = evaluate_node(unary->operand);
            if (!operand.ok()) {
                return operand;
            }
            const auto* number = operand.value->as_number();
            if (number == nullptr) {
                return make_failure(
                    "runtime.type_mismatch",
                    "Unary arithmetic operators require numeric values.",
                    node->span);
            }
            if (!is_finite_number(*number)) {
                return make_non_finite_input_error(node->span);
            }
            return make_finite_numeric_success(
                unary->op == ir::UnaryOperator::plus ? *number : -*number,
                node->span);
        }
        if (const auto* binary = node->as<ir::BinaryOpNode>()) {
            auto left = evaluate_node(binary->left);
            if (!left.ok()) {
                return left;
            }
            auto right = evaluate_node(binary->right);
            if (!right.ok()) {
                return right;
            }
            return evaluate_binary(*binary, *left.value, *right.value, node->span);
        }
        if (const auto* if_node = node->as<ir::IfNode>()) {
            auto condition = evaluate_node(if_node->condition);
            if (!condition.ok()) {
                return condition;
            }
            const auto* condition_value = condition.value->as_boolean();
            if (condition_value == nullptr) {
                return make_failure(
                    "runtime.type_mismatch",
                    "If condition must evaluate to a boolean.",
                    node->span);
            }
            return evaluate_node(*condition_value ? if_node->then_branch : if_node->else_branch);
        }
        if (const auto* call = node->as<ir::CallNode>()) {
            return evaluate_call(*call, node->span);
        }

        return make_failure("runtime.unsupported_node", "Encountered an unsupported runtime node.", node->span);
    }

    EvaluationResult evaluate_binary(
        const ir::BinaryOpNode& binary,
        const Value& left,
        const Value& right,
        const SourceSpan& span) {
        switch (binary.op) {
            case ir::BinaryOperator::add:
            case ir::BinaryOperator::subtract:
            case ir::BinaryOperator::multiply:
            case ir::BinaryOperator::divide:
            case ir::BinaryOperator::power:
                return evaluate_numeric_binary(binary.op, left, right, span);
            case ir::BinaryOperator::equal:
            case ir::BinaryOperator::not_equal: {
                const auto equal = values_equal(left, right);
                if (equal.kind == EqualityResultKind::non_finite_numeric_input) {
                    return make_non_finite_input_error(span);
                }
                if (equal.kind == EqualityResultKind::incomparable_types) {
                    return make_failure(
                        "runtime.type_mismatch",
                        "Equality comparisons require comparable value types.",
                        span);
                }
                return make_success(Value(
                    binary.op == ir::BinaryOperator::equal ? equal.value : !equal.value));
            }
            case ir::BinaryOperator::less:
            case ir::BinaryOperator::less_equal:
            case ir::BinaryOperator::greater:
            case ir::BinaryOperator::greater_equal:
                return evaluate_numeric_comparison(binary.op, left, right, span);
        }
        return make_failure("runtime.unsupported_operator", "Encountered an unsupported binary operator.", span);
    }

    EvaluationResult evaluate_numeric_binary(
        ir::BinaryOperator op,
        const Value& left,
        const Value& right,
        const SourceSpan& span) {
        const auto* left_number = left.as_number();
        const auto* right_number = right.as_number();
        if (left_number == nullptr || right_number == nullptr) {
            return make_failure(
                "runtime.type_mismatch",
                "Arithmetic operators require numeric values.",
                span);
        }
        if (!is_finite_number(*left_number) || !is_finite_number(*right_number)) {
            return make_non_finite_input_error(span);
        }

        switch (op) {
            case ir::BinaryOperator::add:
                return make_finite_numeric_success(*left_number + *right_number, span);
            case ir::BinaryOperator::subtract:
                return make_finite_numeric_success(*left_number - *right_number, span);
            case ir::BinaryOperator::multiply:
                return make_finite_numeric_success(*left_number * *right_number, span);
            case ir::BinaryOperator::divide:
                if (*right_number == 0.0) {
                    return make_failure("runtime.division_by_zero", "Division by zero is not allowed.", span);
                }
                return make_finite_numeric_success(*left_number / *right_number, span);
            case ir::BinaryOperator::power:
                if (*left_number == 0.0 && *right_number == 0.0) {
                    return make_invalid_power_domain_error(span);
                }
                if (*left_number < 0.0 && !is_integer_value(*right_number)) {
                    return make_invalid_power_domain_error(span);
                }
                return make_finite_numeric_success(std::pow(*left_number, *right_number), span);
            default:
                break;
        }

        return make_failure("runtime.unsupported_operator", "Encountered an unsupported arithmetic operator.", span);
    }

    EvaluationResult evaluate_numeric_comparison(
        ir::BinaryOperator op,
        const Value& left,
        const Value& right,
        const SourceSpan& span) {
        const auto* left_number = left.as_number();
        const auto* right_number = right.as_number();
        if (left_number == nullptr || right_number == nullptr) {
            return make_failure(
                "runtime.type_mismatch",
                "Comparison operators currently require numeric values.",
                span);
        }
        if (!is_finite_number(*left_number) || !is_finite_number(*right_number)) {
            return make_non_finite_input_error(span);
        }

        switch (op) {
            case ir::BinaryOperator::less:
                return make_success(Value(*left_number < *right_number));
            case ir::BinaryOperator::less_equal:
                return make_success(Value(*left_number <= *right_number));
            case ir::BinaryOperator::greater:
                return make_success(Value(*left_number > *right_number));
            case ir::BinaryOperator::greater_equal:
                return make_success(Value(*left_number >= *right_number));
            default:
                break;
        }

        return make_failure("runtime.unsupported_operator", "Encountered an unsupported comparison operator.", span);
    }

    EvaluationResult evaluate_call(const ir::CallNode& call, const SourceSpan& span) {
        std::vector<Value> arguments;
        arguments.reserve(call.arguments.size());
        for (const auto& argument_node : call.arguments) {
            auto argument = evaluate_node(argument_node);
            if (!argument.ok()) {
                return argument;
            }
            arguments.push_back(std::move(*argument.value));
        }

        const auto& host_functions = context_.host_functions();
        if (const auto* spec = kernel::FunctionRegistry::find_host_function(host_functions, call.callee)) {
            if (!spec->arity.allows(arguments.size())) {
                return make_failure(
                    "runtime.invalid_call",
                    "Host function `" + call.callee + "` was called with an invalid arity.",
                    span);
            }

            const std::size_t checked_parameters = std::min(spec->parameters.size(), arguments.size());
            for (std::size_t index = 0; index < checked_parameters; ++index) {
                if (!matches_value_type(arguments[index], spec->parameters[index].type)) {
                    return make_failure(
                        "runtime.invalid_argument_type",
                        "Host function `" + call.callee + "` expects argument " +
                            std::to_string(index + 1) + " to be `" +
                            value_type_name(spec->parameters[index].type) + "`, but found `" +
                            value_type_name(arguments[index]) + "`.",
                        call.arguments[index] != nullptr ? call.arguments[index]->span : span);
                }
            }

            auto callback_result = spec->callback(arguments);
            if (callback_result.value.has_value() == callback_result.error.has_value()) {
                return make_failure(
                    "runtime.invalid_host_result",
                    "Host function `" + call.callee + "` returned an invalid evaluation result.",
                    span);
            }
            if (!callback_result.ok() && callback_result.error && !callback_result.error->span.has_value()) {
                callback_result.error->span = span;
            }
            if (callback_result.ok() && spec->return_type.has_value() &&
                !matches_value_type(*callback_result.value, *spec->return_type)) {
                return make_failure(
                    "runtime.invalid_host_result",
                    "Host function `" + call.callee + "` declared return type `" +
                        value_type_name(*spec->return_type) + "`, but returned `" +
                        value_type_name(*callback_result.value) + "`.",
                    span);
            }
            return callback_result;
        }

        if (call.callee == "Abs") {
            if (arguments.size() != 1 || !arguments[0].is_number()) {
                return make_failure("runtime.invalid_call", "Abs expects one numeric argument.", span);
            }
            if (!is_finite_number(*arguments[0].as_number())) {
                return make_non_finite_input_error(span);
            }
            return make_finite_numeric_success(std::abs(*arguments[0].as_number()), span);
        }
        if (call.callee == "Clamp") {
            if (arguments.size() != 3 || !arguments[0].is_number() || !arguments[1].is_number() ||
                !arguments[2].is_number()) {
                return make_failure(
                    "runtime.invalid_call",
                    "Clamp expects three numeric arguments.",
                    span);
            }

            const double value = *arguments[0].as_number();
            const double low = *arguments[1].as_number();
            const double high = *arguments[2].as_number();
            if (!is_finite_number(value) || !is_finite_number(low) || !is_finite_number(high)) {
                return make_non_finite_input_error(span);
            }
            if (low > high) {
                return make_invalid_numeric_domain_error(
                    "Clamp",
                    "requires the lower bound to be less than or equal to the upper bound.",
                    span);
            }
            return make_finite_numeric_success(std::clamp(value, low, high), span);
        }
        if (call.callee == "Floor" || call.callee == "Ceil" || call.callee == "Ceiling" ||
            call.callee == "Round" || call.callee == "Sqrt") {
            if (arguments.size() != 1 || !arguments[0].is_number()) {
                return make_failure(
                    "runtime.invalid_call",
                    call.callee + " expects one numeric argument.",
                    span);
            }

            const double value = *arguments[0].as_number();
            if (!is_finite_number(value)) {
                return make_non_finite_input_error(span);
            }
            if (call.callee == "Floor") {
                return make_finite_numeric_success(std::floor(value), span);
            }
            if (call.callee == "Ceil" || call.callee == "Ceiling") {
                return make_finite_numeric_success(std::ceil(value), span);
            }
            if (call.callee == "Round") {
                return make_finite_numeric_success(std::round(value), span);
            }
            if (value < 0.0) {
                return make_invalid_numeric_domain_error(
                    "Sqrt",
                    "is undefined for negative inputs.",
                    span);
            }
            return make_finite_numeric_success(std::sqrt(value), span);
        }
        if (call.callee == "Min" || call.callee == "Max") {
            if (arguments.empty()) {
                return make_failure("runtime.invalid_call", call.callee + " expects at least one numeric argument.", span);
            }
            double result = 0.0;
            bool first = true;
            for (const auto& argument : arguments) {
                const auto* number = argument.as_number();
                if (number == nullptr) {
                    return make_failure("runtime.invalid_call", call.callee + " expects numeric arguments.", span);
                }
                if (!is_finite_number(*number)) {
                    return make_non_finite_input_error(span);
                }
                if (first) {
                    result = *number;
                    first = false;
                } else if (call.callee == "Min") {
                    result = std::min(result, *number);
                } else {
                    result = std::max(result, *number);
                }
            }
            return make_finite_numeric_success(result, span);
        }

        return make_failure(
            "runtime.unknown_function",
            "No runtime function implementation is registered for `" + call.callee + "`.",
            span);
    }

    EvaluationContext context_;
    StepBudget budget_;
};

}  // namespace

Evaluator::Evaluator(EvaluationContext context)
    : context_(std::move(context)) {}

EvaluationResult Evaluator::evaluate(const ir::NodePtr& root) const {
    EvaluatorImpl impl(context_);
    return impl.evaluate(root);
}

}  // namespace aleph3::runtime
