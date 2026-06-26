#include "kernel/Lowering.hpp"

#include "expr/ExprUtils.hpp"

#include <string>
#include <utility>
#include <vector>

namespace aleph3::kernel {

namespace {

Diagnostic make_error(std::string code, std::string message, SourceSpan span = {}) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::error;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.span = span;
    return diagnostic;
}

std::string unary_head(ir::UnaryOperator op) {
    switch (op) {
        case ir::UnaryOperator::plus:
            return "Plus";
        case ir::UnaryOperator::minus:
            return "Times";
    }
    return "UnknownUnary";
}

std::string binary_head(ir::BinaryOperator op) {
    switch (op) {
        case ir::BinaryOperator::add:
            return "Plus";
        case ir::BinaryOperator::subtract:
            return "Plus";
        case ir::BinaryOperator::multiply:
            return "Times";
        case ir::BinaryOperator::divide:
            return "Divide";
        case ir::BinaryOperator::power:
            return "Power";
        case ir::BinaryOperator::equal:
            return "Equal";
        case ir::BinaryOperator::not_equal:
            return "NotEqual";
        case ir::BinaryOperator::less:
            return "Less";
        case ir::BinaryOperator::less_equal:
            return "LessEqual";
        case ir::BinaryOperator::greater:
            return "Greater";
        case ir::BinaryOperator::greater_equal:
            return "GreaterEqual";
    }
    return "UnknownBinary";
}

LoweringResult lower_node(const ir::NodePtr& node) {
    LoweringResult result;
    if (node == nullptr) {
        result.diagnostics.push_back(make_error(
            "kernel.lowering.empty_node",
            "Cannot lower an empty trusted-subset node."));
        return result;
    }

    if (const auto* number = node->as<ir::NumberLiteralNode>()) {
        result.expr = make_expr<Number>(number->value);
        return result;
    }
    if (const auto* boolean = node->as<ir::BooleanLiteralNode>()) {
        result.expr = make_expr<Boolean>(boolean->value);
        return result;
    }
    if (const auto* string = node->as<ir::StringLiteralNode>()) {
        result.expr = make_expr<String>(string->value);
        return result;
    }
    if (const auto* variable = node->as<ir::VariableNode>()) {
        result.expr = make_expr<Symbol>(variable->name);
        return result;
    }
    if (const auto* unary = node->as<ir::UnaryOpNode>()) {
        auto operand = lower_node(unary->operand);
        if (!operand.ok()) {
            return operand;
        }

        switch (unary->op) {
            case ir::UnaryOperator::plus:
                result.expr = make_fcall(unary_head(unary->op), {operand.expr});
                return result;
            case ir::UnaryOperator::minus:
                result.expr = make_fcall(unary_head(unary->op), {make_expr<Number>(-1.0), operand.expr});
                return result;
        }
    }
    if (const auto* binary = node->as<ir::BinaryOpNode>()) {
        auto left = lower_node(binary->left);
        if (!left.ok()) {
            return left;
        }

        auto right = lower_node(binary->right);
        if (!right.ok()) {
            return right;
        }

        switch (binary->op) {
            case ir::BinaryOperator::subtract:
                result.expr = make_fcall(
                    binary_head(binary->op),
                    {left.expr, make_fcall("Times", {make_expr<Number>(-1.0), right.expr})});
                return result;
            default:
                result.expr = make_fcall(binary_head(binary->op), {left.expr, right.expr});
                return result;
        }
    }
    if (const auto* call = node->as<ir::CallNode>()) {
        std::vector<ExprPtr> arguments;
        arguments.reserve(call->arguments.size());
        for (const auto& argument_node : call->arguments) {
            auto argument = lower_node(argument_node);
            if (!argument.ok()) {
                return argument;
            }
            arguments.push_back(argument.expr);
        }

        result.expr = make_fcall(call->callee, arguments);
        return result;
    }
    if (const auto* if_node = node->as<ir::IfNode>()) {
        auto condition = lower_node(if_node->condition);
        if (!condition.ok()) {
            return condition;
        }

        auto then_branch = lower_node(if_node->then_branch);
        if (!then_branch.ok()) {
            return then_branch;
        }

        auto else_branch = lower_node(if_node->else_branch);
        if (!else_branch.ok()) {
            return else_branch;
        }

        result.expr = make_fcall("If", {condition.expr, then_branch.expr, else_branch.expr});
        return result;
    }

    result.diagnostics.push_back(make_error(
        "kernel.lowering.unsupported_node",
        "Encountered an unsupported trusted-subset node while lowering into the kernel.",
        node->span));
    return result;
}

}  // namespace

LoweringResult lower_trusted_ir_to_expr(const ir::NodePtr& root) {
    return lower_node(root);
}

}  // namespace aleph3::kernel
