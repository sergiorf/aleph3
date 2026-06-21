#include "semantics/Validator.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace aleph3::semantics {

namespace {

struct TraversalState {
    std::size_t node_count = 0;
    std::size_t max_depth = 0;
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
    void validate_node(
        const ir::NodePtr& node,
        std::size_t depth,
        TraversalState& traversal,
        std::vector<Diagnostic>& diagnostics) const {
        if (node == nullptr) {
            return;
        }

        ++traversal.node_count;
        traversal.max_depth = std::max(traversal.max_depth, depth);

        if (const auto* string = node->as<ir::StringLiteralNode>()) {
            (void)string;
            if (!policy_.enable_strings()) {
                diagnostics.push_back(make_error(
                    "semantics.validator.string_literals_disabled",
                    "String literals are disabled by policy.",
                    node->span));
            }
            return;
        }

        if (const auto* variable = node->as<ir::VariableNode>()) {
            if (!schema_.allows_unknown_variables() &&
                !schema_.variables().contains(variable->name) &&
                !schema_.constants().contains(variable->name)) {
                diagnostics.push_back(make_error(
                    "semantics.validator.unknown_variable",
                    "Variable is not allowed by schema: " + variable->name,
                    node->span));
            }
            return;
        }

        if (const auto* unary = node->as<ir::UnaryOpNode>()) {
            if (!policy_.enable_arithmetic()) {
                diagnostics.push_back(make_error(
                    "semantics.validator.arithmetic_disabled",
                    "Arithmetic operators are disabled by policy.",
                    node->span));
            }
            validate_node(unary->operand, depth + 1, traversal, diagnostics);
            return;
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
            validate_node(binary->left, depth + 1, traversal, diagnostics);
            validate_node(binary->right, depth + 1, traversal, diagnostics);
            return;
        }

        if (const auto* call = node->as<ir::CallNode>()) {
            const auto function_it = schema_.functions().find(call->callee);
            const bool schema_allows = function_it != schema_.functions().end();
            const bool optional_builtin = is_optional_builtin(call->callee);

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
                validate_node(argument, depth + 1, traversal, diagnostics);
            }
            return;
        }

        if (const auto* if_node = node->as<ir::IfNode>()) {
            if (!policy_.enable_conditionals()) {
                diagnostics.push_back(make_error(
                    "semantics.validator.conditionals_disabled",
                    "Conditional expressions are disabled by policy.",
                    node->span));
            }
            validate_node(if_node->condition, depth + 1, traversal, diagnostics);
            validate_node(if_node->then_branch, depth + 1, traversal, diagnostics);
            validate_node(if_node->else_branch, depth + 1, traversal, diagnostics);
            return;
        }

        // Number and boolean literals need no semantic checks in v1.
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
