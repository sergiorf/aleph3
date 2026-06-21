#include "runtime/Evaluator.hpp"

#include <algorithm>
#include <cmath>
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

bool values_equal(const Value& left, const Value& right) {
    if (const auto* left_number = left.as_number()) {
        if (const auto* right_number = right.as_number()) {
            return *left_number == *right_number;
        }
        return false;
    }
    if (const auto* left_boolean = left.as_boolean()) {
        if (const auto* right_boolean = right.as_boolean()) {
            return *left_boolean == *right_boolean;
        }
        return false;
    }
    if (const auto* left_string = left.as_string()) {
        if (const auto* right_string = right.as_string()) {
            return *left_string == *right_string;
        }
        return false;
    }
    if (left.is_null() && right.is_null()) {
        return true;
    }
    return false;
}

class EvaluatorImpl {
public:
    explicit EvaluatorImpl(EvaluationContext context)
        : context_(context) {
        budget_.max_steps = context_.policy.budget().max_evaluation_steps;
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
            const auto binding = context_.bindings.find(variable->name);
            if (binding == context_.bindings.end()) {
                return make_failure(
                    "runtime.unknown_binding",
                    "No binding was provided for variable `" + variable->name + "`.",
                    node->span);
            }
            return make_success(binding->second);
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
            return make_success(Value(unary->op == ir::UnaryOperator::plus ? *number : -*number));
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
                return make_success(Value(values_equal(left, right)));
            case ir::BinaryOperator::not_equal:
                return make_success(Value(!values_equal(left, right)));
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

        switch (op) {
            case ir::BinaryOperator::add:
                return make_success(Value(*left_number + *right_number));
            case ir::BinaryOperator::subtract:
                return make_success(Value(*left_number - *right_number));
            case ir::BinaryOperator::multiply:
                return make_success(Value(*left_number * *right_number));
            case ir::BinaryOperator::divide:
                if (*right_number == 0.0) {
                    return make_failure("runtime.division_by_zero", "Division by zero is not allowed.", span);
                }
                return make_success(Value(*left_number / *right_number));
            case ir::BinaryOperator::power:
                return make_success(Value(std::pow(*left_number, *right_number)));
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

        if (call.callee == "Abs") {
            if (arguments.size() != 1 || !arguments[0].is_number()) {
                return make_failure("runtime.invalid_call", "Abs expects one numeric argument.", span);
            }
            return make_success(Value(std::abs(*arguments[0].as_number())));
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
                if (first) {
                    result = *number;
                    first = false;
                } else if (call.callee == "Min") {
                    result = std::min(result, *number);
                } else {
                    result = std::max(result, *number);
                }
            }
            return make_success(Value(result));
        }

        const auto host_function = context_.host_functions.find(call.callee);
        if (host_function == context_.host_functions.end()) {
            return make_failure(
                "runtime.unknown_function",
                "No runtime function implementation is registered for `" + call.callee + "`.",
                span);
        }

        auto callback_result = host_function->second.callback(arguments);
        if (!callback_result.ok() && callback_result.error && !callback_result.error->span.has_value()) {
            callback_result.error->span = span;
        }
        return callback_result;
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
