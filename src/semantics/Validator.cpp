#include "semantics/Validator.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aleph3::semantics {

namespace {

struct TraversalState {
    std::size_t node_count = 0;
    std::size_t max_depth = 0;
};

enum class InferredType {
    unknown,
    any,
    number,
    boolean,
    string,
    list,
    invalid
};

Diagnostic make_error(std::string code, std::string message, SourceSpan span) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::error;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.span = span;
    return diagnostic;
}

bool is_optional_builtin(std::string_view name) noexcept {
    return name == "Min" || name == "Max" || name == "Abs";
}

FunctionArity optional_builtin_arity(std::string_view name) noexcept {
    if (name == "Abs") {
        return FunctionArity::exact(1);
    }
    return FunctionArity{1, static_cast<std::size_t>(-1)};
}

bool is_arithmetic_operator(ir::BinaryOperator op) noexcept {
    switch (op) {
        case ir::BinaryOperator::add:
        case ir::BinaryOperator::subtract:
        case ir::BinaryOperator::multiply:
        case ir::BinaryOperator::divide:
        case ir::BinaryOperator::power:
            return true;
        default:
            return false;
    }
}

bool is_comparison_operator(ir::BinaryOperator op) noexcept {
    switch (op) {
        case ir::BinaryOperator::equal:
        case ir::BinaryOperator::not_equal:
        case ir::BinaryOperator::less:
        case ir::BinaryOperator::less_equal:
        case ir::BinaryOperator::greater:
        case ir::BinaryOperator::greater_equal:
            return true;
        default:
            return false;
    }
}

std::string type_name(InferredType type) {
    switch (type) {
        case InferredType::unknown:
            return "unknown";
        case InferredType::any:
            return "any";
        case InferredType::number:
            return "number";
        case InferredType::boolean:
            return "boolean";
        case InferredType::string:
            return "string";
        case InferredType::list:
            return "list";
        case InferredType::invalid:
            return "invalid";
    }
    return "unknown";
}

InferredType inferred_type_from_value_type(ValueType type) noexcept {
    switch (type) {
        case ValueType::any:
            return InferredType::any;
        case ValueType::number:
            return InferredType::number;
        case ValueType::boolean:
            return InferredType::boolean;
        case ValueType::string:
            return InferredType::string;
        case ValueType::list:
            return InferredType::list;
    }
    return InferredType::unknown;
}

bool matches_expected_type(InferredType actual, ValueType expected) noexcept {
    if (expected == ValueType::any) {
        return actual != InferredType::invalid;
    }
    if (actual == InferredType::unknown || actual == InferredType::any) {
        return true;
    }
    return actual == inferred_type_from_value_type(expected);
}

bool is_known_non_numeric(InferredType type) noexcept {
    return type == InferredType::boolean ||
           type == InferredType::string ||
           type == InferredType::list;
}

bool is_known_concrete_type(InferredType type) noexcept {
    return type == InferredType::number ||
           type == InferredType::boolean ||
           type == InferredType::string ||
           type == InferredType::list;
}

bool branch_types_are_incompatible(InferredType left, InferredType right) noexcept {
    return is_known_concrete_type(left) &&
           is_known_concrete_type(right) &&
           left != right;
}

std::optional<InferredType> optional_builtin_return_type(std::string_view name) noexcept {
    if (name == "Abs" || name == "Min" || name == "Max") {
        return InferredType::number;
    }
    return std::nullopt;
}

std::optional<bool> boolean_literal_value(const ir::NodePtr& node) noexcept {
    if (node == nullptr) {
        return std::nullopt;
    }
    if (const auto* boolean = node->as<ir::BooleanLiteralNode>()) {
        return boolean->value;
    }
    return std::nullopt;
}

class ValidatorImpl {
public:
    ValidatorImpl(const Schema& schema, const Policy& policy)
        : schema_(schema), policy_(policy) {}

    ValidationSummary validate(const ir::NodePtr& root) {
        ValidationSummary summary;
        if (root == nullptr) {
            summary.diagnostics.push_back(make_error(
                "semantics.validator.empty_ir",
                "Cannot validate an empty IR tree.",
                {}));
            return summary;
        }

        TraversalState traversal;
        validate_node(root, 1, traversal, summary.diagnostics);

        if (traversal.node_count > policy_.budget().max_node_count) {
            summary.diagnostics.push_back(make_error(
                "semantics.validator.node_limit_exceeded",
                "Formula exceeds the configured node-count limit.",
                root->span));
        }

        if (traversal.max_depth > policy_.budget().max_ast_depth) {
            summary.diagnostics.push_back(make_error(
                "semantics.validator.depth_limit_exceeded",
                "Formula exceeds the configured AST-depth limit.",
                root->span));
        }

        return summary;
    }

private:
    InferredType validate_node(
        const ir::NodePtr& node,
        std::size_t depth,
        TraversalState& traversal,
        std::vector<Diagnostic>& diagnostics) const {
        if (node == nullptr) {
            return InferredType::unknown;
        }

        ++traversal.node_count;
        traversal.max_depth = std::max(traversal.max_depth, depth);

        if (node->as<ir::NumberLiteralNode>() != nullptr) {
            return InferredType::number;
        }

        if (node->as<ir::BooleanLiteralNode>() != nullptr) {
            return InferredType::boolean;
        }

        if (const auto* string = node->as<ir::StringLiteralNode>()) {
            (void)string;
            if (!policy_.enable_strings()) {
                diagnostics.push_back(make_error(
                    "semantics.validator.string_literals_disabled",
                    "String literals are disabled by policy.",
                    node->span));
                return InferredType::invalid;
            }
            return InferredType::string;
        }

        if (const auto* variable = node->as<ir::VariableNode>()) {
            const auto variable_it = schema_.variables().find(variable->name);
            if (variable_it != schema_.variables().end()) {
                return inferred_type_from_value_type(variable_it->second.type);
            }

            if (schema_.constants().contains(variable->name)) {
                return InferredType::any;
            }

            if (!schema_.allows_unknown_variables()) {
                diagnostics.push_back(make_error(
                    "semantics.validator.unknown_variable",
                    "Variable is not allowed by schema: " + variable->name,
                    node->span));
                return InferredType::unknown;
            }
            return InferredType::any;
        }

        if (const auto* unary = node->as<ir::UnaryOpNode>()) {
            if (!policy_.enable_arithmetic()) {
                diagnostics.push_back(make_error(
                    "semantics.validator.arithmetic_disabled",
                    "Arithmetic operators are disabled by policy.",
                    node->span));
            }
            const auto operand_type = validate_node(unary->operand, depth + 1, traversal, diagnostics);
            if (is_known_non_numeric(operand_type)) {
                diagnostics.push_back(make_error(
                    "semantics.validator.type_mismatch",
                    "Unary arithmetic expects a numeric operand, but found `" + type_name(operand_type) + "`.",
                    unary->operand != nullptr ? unary->operand->span : node->span));
                return InferredType::invalid;
            }
            if (operand_type == InferredType::invalid) {
                return InferredType::invalid;
            }
            return InferredType::number;
        }

        if (const auto* binary = node->as<ir::BinaryOpNode>()) {
            if (is_arithmetic_operator(binary->op) && !policy_.enable_arithmetic()) {
                diagnostics.push_back(make_error(
                    "semantics.validator.arithmetic_disabled",
                    "Arithmetic operators are disabled by policy.",
                    node->span));
            }
            if (is_comparison_operator(binary->op) && !policy_.enable_comparisons()) {
                diagnostics.push_back(make_error(
                    "semantics.validator.comparisons_disabled",
                    "Comparison operators are disabled by policy.",
                    node->span));
            }
            const auto left_type = validate_node(binary->left, depth + 1, traversal, diagnostics);
            const auto right_type = validate_node(binary->right, depth + 1, traversal, diagnostics);

            if (is_arithmetic_operator(binary->op)) {
                if (is_known_non_numeric(left_type)) {
                    diagnostics.push_back(make_error(
                        "semantics.validator.type_mismatch",
                        "Arithmetic operators expect numeric operands, but the left operand is `" + type_name(left_type) + "`.",
                        binary->left != nullptr ? binary->left->span : node->span));
                }
                if (is_known_non_numeric(right_type)) {
                    diagnostics.push_back(make_error(
                        "semantics.validator.type_mismatch",
                        "Arithmetic operators expect numeric operands, but the right operand is `" + type_name(right_type) + "`.",
                        binary->right != nullptr ? binary->right->span : node->span));
                }
                if (left_type == InferredType::invalid || right_type == InferredType::invalid ||
                    is_known_non_numeric(left_type) || is_known_non_numeric(right_type)) {
                    return InferredType::invalid;
                }
                return InferredType::number;
            }

            if (is_comparison_operator(binary->op)) {
                if (binary->op == ir::BinaryOperator::less ||
                    binary->op == ir::BinaryOperator::less_equal ||
                    binary->op == ir::BinaryOperator::greater ||
                    binary->op == ir::BinaryOperator::greater_equal) {
                    if (is_known_non_numeric(left_type)) {
                        diagnostics.push_back(make_error(
                            "semantics.validator.type_mismatch",
                            "Ordered comparisons expect numeric operands, but the left operand is `" + type_name(left_type) + "`.",
                            binary->left != nullptr ? binary->left->span : node->span));
                    }
                    if (is_known_non_numeric(right_type)) {
                        diagnostics.push_back(make_error(
                            "semantics.validator.type_mismatch",
                            "Ordered comparisons expect numeric operands, but the right operand is `" + type_name(right_type) + "`.",
                            binary->right != nullptr ? binary->right->span : node->span));
                    }
                } else if (is_known_concrete_type(left_type) && is_known_concrete_type(right_type) &&
                           left_type != right_type) {
                    diagnostics.push_back(make_error(
                        "semantics.validator.type_mismatch",
                        "Equality comparisons require comparable operand types, but found `" +
                            type_name(left_type) + "` and `" + type_name(right_type) + "`.",
                        node->span));
                }

                if (left_type == InferredType::invalid || right_type == InferredType::invalid) {
                    return InferredType::invalid;
                }
                return InferredType::boolean;
            }

            return InferredType::unknown;
        }

        if (const auto* call = node->as<ir::CallNode>()) {
            const auto function_it = schema_.functions().find(call->callee);
            const bool schema_allows = function_it != schema_.functions().end();
            const bool optional_builtin = is_optional_builtin(call->callee);
            std::vector<InferredType> argument_types;
            argument_types.reserve(call->arguments.size());

            if (optional_builtin) {
                if (!policy_.enable_optional_builtins()) {
                    diagnostics.push_back(make_error(
                        "semantics.validator.optional_builtin_disabled",
                        "Optional built-in function is disabled by policy: " + call->callee,
                        node->span));
                } else {
                    const auto arity = optional_builtin_arity(call->callee);
                    if (!arity.allows(call->arguments.size())) {
                        diagnostics.push_back(make_error(
                            "semantics.validator.invalid_arity",
                            "Function `" + call->callee + "` was called with an invalid arity.",
                            node->span));
                    }
                }
            } else {
                if (!policy_.enable_host_functions()) {
                    diagnostics.push_back(make_error(
                        "semantics.validator.host_functions_disabled",
                        "Function calls are disabled by policy.",
                        node->span));
                } else if (!schema_allows && !schema_.allows_unknown_functions()) {
                    diagnostics.push_back(make_error(
                        "semantics.validator.unknown_function",
                        "Function is not allowed by schema: " + call->callee,
                        node->span));
                } else if (schema_allows && !function_it->second.arity.allows(call->arguments.size())) {
                    diagnostics.push_back(make_error(
                        "semantics.validator.invalid_arity",
                        "Function `" + call->callee + "` was called with an invalid arity.",
                        node->span));
                }
            }

            for (const auto& argument : call->arguments) {
                argument_types.push_back(validate_node(argument, depth + 1, traversal, diagnostics));
            }

            if (optional_builtin && policy_.enable_optional_builtins()) {
                for (std::size_t index = 0; index < argument_types.size(); ++index) {
                    if (is_known_non_numeric(argument_types[index])) {
                        diagnostics.push_back(make_error(
                            "semantics.validator.invalid_argument_type",
                            "Optional built-in `" + call->callee +
                                "` expects numeric arguments, but argument " +
                                std::to_string(index + 1) + " is `" + type_name(argument_types[index]) + "`.",
                            call->arguments[index] != nullptr ? call->arguments[index]->span : node->span));
                    }
                }
                return optional_builtin_return_type(call->callee).value_or(InferredType::any);
            }

            if (schema_allows) {
                const auto& function = function_it->second;
                const std::size_t parameter_count = std::min(
                    function.parameter_types.size(),
                    argument_types.size());
                for (std::size_t index = 0; index < parameter_count; ++index) {
                    if (!matches_expected_type(argument_types[index], function.parameter_types[index])) {
                        diagnostics.push_back(make_error(
                            "semantics.validator.invalid_argument_type",
                            "Function `" + call->callee + "` expects argument " +
                                std::to_string(index + 1) + " to be `" +
                                type_name(inferred_type_from_value_type(function.parameter_types[index])) +
                                "`, but found `" + type_name(argument_types[index]) + "`.",
                            call->arguments[index] != nullptr ? call->arguments[index]->span : node->span));
                    }
                }

                if (function.return_type.has_value()) {
                    return inferred_type_from_value_type(*function.return_type);
                }
            }

            return schema_allows ? InferredType::any : InferredType::unknown;
        }

        if (const auto* if_node = node->as<ir::IfNode>()) {
            if (!policy_.enable_conditionals()) {
                diagnostics.push_back(make_error(
                    "semantics.validator.conditionals_disabled",
                    "Conditional expressions are disabled by policy.",
                    node->span));
            }
            const auto condition_type =
                validate_node(if_node->condition, depth + 1, traversal, diagnostics);
            const auto then_type =
                validate_node(if_node->then_branch, depth + 1, traversal, diagnostics);
            const auto else_type =
                validate_node(if_node->else_branch, depth + 1, traversal, diagnostics);

            if (condition_type != InferredType::unknown &&
                condition_type != InferredType::any &&
                condition_type != InferredType::boolean &&
                condition_type != InferredType::invalid) {
                diagnostics.push_back(make_error(
                    "semantics.validator.type_mismatch",
                    "If conditions must be boolean, but found `" + type_name(condition_type) + "`.",
                    if_node->condition != nullptr ? if_node->condition->span : node->span));
            }

            // A literal boolean condition gives the validator a trustworthy
            // branch choice, so only the reachable branch should affect
            // semantic success and inferred result type.
            if (const auto literal_condition = boolean_literal_value(if_node->condition)) {
                const auto reachable_type = *literal_condition ? then_type : else_type;
                if (reachable_type == InferredType::invalid) {
                    return InferredType::invalid;
                }
                return reachable_type;
            }

            if (then_type == InferredType::invalid || else_type == InferredType::invalid) {
                return InferredType::invalid;
            }
            if (then_type == else_type) {
                return then_type;
            }
            if (branch_types_are_incompatible(then_type, else_type)) {
                diagnostics.push_back(make_error(
                    "semantics.validator.incompatible_branch_types",
                    "If branches must resolve to compatible result types, but found `" +
                        type_name(then_type) + "` and `" + type_name(else_type) + "`.",
                    node->span));
                return InferredType::invalid;
            }
            if (then_type == InferredType::unknown || else_type == InferredType::unknown) {
                return InferredType::unknown;
            }
            if (then_type == InferredType::any || else_type == InferredType::any) {
                return InferredType::any;
            }
            return InferredType::any;
        }

        return InferredType::unknown;
    }

    const Schema& schema_;
    const Policy& policy_;
};

}  // namespace

Validator::Validator(const Schema& schema, const Policy& policy)
    : schema_(schema), policy_(policy) {}

ValidationSummary Validator::validate(const ir::NodePtr& root) const {
    ValidatorImpl impl(schema_, policy_);
    return impl.validate(root);
}

}  // namespace aleph3::semantics
