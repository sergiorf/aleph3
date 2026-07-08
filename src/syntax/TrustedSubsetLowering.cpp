#include "syntax/TrustedSubsetLowering.hpp"

#include <string>
#include <utility>

namespace aleph3::syntax {

namespace {

Diagnostic make_error(std::string code, std::string message, SourceSpan span = {}) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::error;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.span = span;
    return diagnostic;
}

std::string unsupported_message() {
    return "Encountered syntax outside the trusted SDK subset.";
}

ir::UnaryOperator to_ir_unary(UnaryOperator op) {
    switch (op) {
        case UnaryOperator::plus: return ir::UnaryOperator::plus;
        case UnaryOperator::minus: return ir::UnaryOperator::minus;
    }
    return ir::UnaryOperator::plus;
}

bool to_ir_binary(BinaryOperator op, ir::BinaryOperator& out) {
    switch (op) {
        case BinaryOperator::add: out = ir::BinaryOperator::add; return true;
        case BinaryOperator::subtract: out = ir::BinaryOperator::subtract; return true;
        case BinaryOperator::multiply: out = ir::BinaryOperator::multiply; return true;
        case BinaryOperator::divide: out = ir::BinaryOperator::divide; return true;
        case BinaryOperator::power: out = ir::BinaryOperator::power; return true;
        case BinaryOperator::equal: out = ir::BinaryOperator::equal; return true;
        case BinaryOperator::not_equal: out = ir::BinaryOperator::not_equal; return true;
        case BinaryOperator::less: out = ir::BinaryOperator::less; return true;
        case BinaryOperator::less_equal: out = ir::BinaryOperator::less_equal; return true;
        case BinaryOperator::greater: out = ir::BinaryOperator::greater; return true;
        case BinaryOperator::greater_equal: out = ir::BinaryOperator::greater_equal; return true;
        default: return false;
    }
}

class Lowerer {
public:
    TrustedSubsetLoweringResult lower(const NodePtr& node) {
        TrustedSubsetLoweringResult result;
        result.root = lower_node(node);
        result.diagnostics = std::move(diagnostics_);
        if (!result.diagnostics.empty()) {
            result.root.reset();
        }
        return result;
    }

private:
    ir::NodePtr lower_node(const NodePtr& node) {
        if (node == nullptr) {
            diagnostics_.push_back(make_error(
                "frontend.parser.empty_node",
                "Cannot lower an empty trusted-subset syntax node."));
            return nullptr;
        }

        if (const auto* number = node->as<NumberLiteralNode>()) {
            return ir::make_node(node->span, ir::NumberLiteralNode{number->value});
        }
        if (const auto* boolean = node->as<BooleanLiteralNode>()) {
            return ir::make_node(node->span, ir::BooleanLiteralNode{boolean->value});
        }
        if (const auto* string = node->as<StringLiteralNode>()) {
            return ir::make_node(node->span, ir::StringLiteralNode{string->value});
        }
        if (const auto* symbol = node->as<SymbolNode>()) {
            return ir::make_node(node->span, ir::VariableNode{symbol->name});
        }
        if (const auto* unary = node->as<UnaryOpNode>()) {
            auto operand = lower_node(unary->operand);
            if (operand == nullptr) {
                return nullptr;
            }
            return ir::make_node(
                node->span,
                ir::UnaryOpNode{to_ir_unary(unary->op), operand});
        }
        if (const auto* binary = node->as<BinaryOpNode>()) {
            if (binary->implicit) {
                diagnostics_.push_back(make_error(
                    "frontend.parser.unsupported_syntax",
                    unsupported_message(),
                    node->span));
                return nullptr;
            }

            ir::BinaryOperator op;
            if (!to_ir_binary(binary->op, op)) {
                diagnostics_.push_back(make_error(
                    "frontend.parser.unsupported_syntax",
                    unsupported_message(),
                    node->span));
                return nullptr;
            }

            auto left = lower_node(binary->left);
            auto right = lower_node(binary->right);
            if (left == nullptr || right == nullptr) {
                return nullptr;
            }
            return ir::make_node(node->span, ir::BinaryOpNode{op, left, right});
        }
        if (const auto* call = node->as<CallNode>()) {
            if (call->callee == "Group" && call->arguments.size() == 1) {
                return lower_node(call->arguments.front());
            }

            std::vector<ir::NodePtr> arguments;
            arguments.reserve(call->arguments.size());
            for (const auto& argument_node : call->arguments) {
                auto argument = lower_node(argument_node);
                if (argument == nullptr) {
                    return nullptr;
                }
                arguments.push_back(argument);
            }

            if (call->callee == "If") {
                if (arguments.size() != 3) {
                    diagnostics_.push_back(make_error(
                        "frontend.parser.invalid_if_arity",
                        "If requires exactly three arguments.",
                        node->span));
                    return nullptr;
                }
                return ir::make_node(
                    node->span,
                    ir::IfNode{arguments[0], arguments[1], arguments[2]});
            }

            return ir::make_node(
                node->span,
                ir::CallNode{call->callee, std::move(arguments)});
        }

        diagnostics_.push_back(make_error(
            "frontend.parser.unsupported_syntax",
            unsupported_message(),
            node->span));
        return nullptr;
    }

    std::vector<Diagnostic> diagnostics_;
};

}  // namespace

TrustedSubsetLoweringResult lower_to_trusted_ir(const NodePtr& root) {
    Lowerer lowerer;
    return lowerer.lower(root);
}

}  // namespace aleph3::syntax
