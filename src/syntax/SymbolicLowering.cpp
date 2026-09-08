#include "syntax/SymbolicLowering.hpp"

#include "expr/ExprUtils.hpp"
#include "syntax/IntegerLiteral.hpp"
#include "syntax/Parser.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

std::optional<kernel::ExactInteger> exact_integer_from_expr(const ExprPtr& expr) {
    if (const auto* integer = std::get_if<Integer>(expr.get())) {
        return integer->value;
    }
    if (const auto* number = std::get_if<Number>(expr.get())) {
        auto integer = exact_int64_from_number(number->value);
        if (integer.has_value()) {
            return kernel::ExactInteger(*integer);
        }
    }
    if (const auto* rational = std::get_if<Rational>(expr.get());
        rational != nullptr && rational->denominator.is_one()) {
        return rational->numerator;
    }
    if (const auto* call = std::get_if<FunctionCall>(expr.get());
        call != nullptr && call->head == "Negate" && call->args.size() == 1) {
        if (auto value = exact_integer_from_expr(call->args[0])) {
            return -*value;
        }
    }
    return std::nullopt;
}

ExprPtr make_exact_rational(kernel::ExactInteger numerator, kernel::ExactInteger denominator) {
    if (denominator.is_zero()) {
        return numerator.is_zero() ? make_expr<Indeterminate>() : make_expr<Infinity>();
    }
    return make_exact_scalar_expr(kernel::ExactRational(std::move(numerator), std::move(denominator)));
}

ExprPtr make_rational_preserving_signs(kernel::ExactInteger numerator, kernel::ExactInteger denominator) {
    if (denominator.is_zero()) {
        return numerator.is_zero() ? make_expr<Indeterminate>() : make_expr<Infinity>();
    }
    return make_exact_scalar_expr(kernel::ExactRational(std::move(numerator), std::move(denominator)));
}

bool is_imaginary_unit(const ExprPtr& expr) {
    const auto* complex = std::get_if<Complex>(expr.get());
    return complex != nullptr && complex->real == 0.0 && complex->imag == 1.0;
}

std::string binary_head(BinaryOperator op) {
    switch (op) {
        case BinaryOperator::add: return "Plus";
        case BinaryOperator::subtract: return "Minus";
        case BinaryOperator::multiply: return "Times";
        case BinaryOperator::divide: return "Divide";
        case BinaryOperator::power: return "Power";
        case BinaryOperator::equal: return "Equal";
        case BinaryOperator::not_equal: return "NotEqual";
        case BinaryOperator::less: return "Less";
        case BinaryOperator::less_equal: return "LessEqual";
        case BinaryOperator::greater: return "Greater";
        case BinaryOperator::greater_equal: return "GreaterEqual";
        case BinaryOperator::and_op: return "And";
        case BinaryOperator::or_op: return "Or";
        case BinaryOperator::string_join: return "StringJoin";
        case BinaryOperator::rule: return "Rule";
        case BinaryOperator::replace_all: return "ReplaceAll";
    }
    return "Unknown";
}

std::string strip_pattern_marker(const std::string& name) {
    if (!name.empty() && name.back() == '_') {
        return name.substr(0, name.size() - 1);
    }
    return name;
}

class Lowerer {
public:
    SymbolicParseResult lower(const NodePtr& node) {
        SymbolicParseResult result;
        result.expr = lower_node(node);
        result.diagnostics = std::move(diagnostics_);
        if (!result.diagnostics.empty()) {
            result.expr.reset();
        }
        if (result.expr != nullptr) {
            result.expr = try_make_complex(result.expr);
        }
        return result;
    }

private:
    std::optional<kernel::ExactInteger> exact_integer_from_text(std::string_view text, SourceSpan span) {
        try {
            return kernel::ExactInteger::from_decimal_string(text);
        } catch (const std::invalid_argument&) {
            diagnostics_.push_back(make_error(
                "syntax.lowering.invalid_integer",
                "Integer literal is invalid.",
                span));
            return std::nullopt;
        }
    }

    std::optional<kernel::ExactInteger> exact_integer_from_node(const NodePtr& node) {
        if (const auto* integer = node->as<IntegerLiteralNode>()) {
            return exact_integer_from_text(integer->decimal_text, node->span);
        }
        if (const auto* unary = node->as<UnaryOpNode>();
            unary != nullptr && unary->op == UnaryOperator::minus) {
            if (const auto* integer = unary->operand->as<IntegerLiteralNode>()) {
                return exact_integer_from_text("-" + integer->decimal_text, node->span);
            }
        }
        return std::nullopt;
    }

    ExprPtr lower_integer_literal(std::string_view text, SourceSpan span) {
        const auto exact = exact_integer_from_text(text, span);
        if (!exact.has_value()) {
            return nullptr;
        }
        return make_expr<Integer>(*exact);
    }

    ExprPtr lower_node(const NodePtr& node) {
        if (node == nullptr) {
            diagnostics_.push_back(make_error(
                "syntax.lowering.empty_node",
                "Cannot lower an empty syntax node."));
            return nullptr;
        }

        if (const auto* integer = node->as<IntegerLiteralNode>()) {
            return lower_integer_literal(integer->decimal_text, node->span);
        }
        if (const auto* number = node->as<NumberLiteralNode>()) {
            return make_expr<Number>(number->value);
        }
        if (const auto* boolean = node->as<BooleanLiteralNode>()) {
            return make_expr<Boolean>(boolean->value);
        }
        if (const auto* string = node->as<StringLiteralNode>()) {
            return make_expr<String>(string->value);
        }
        if (const auto* symbol = node->as<SymbolNode>()) {
            if (symbol->name == "I") {
                return make_expr<Complex>(0.0, 1.0);
            }
            return make_expr<Symbol>(symbol->name);
        }
        if (const auto* fraction = node->as<FractionLiteralNode>()) {
            const auto literal_numerator = exact_integer_from_node(fraction->numerator);
            const auto literal_denominator = exact_integer_from_node(fraction->denominator);
            if (literal_numerator.has_value() && literal_denominator.has_value()) {
                return make_rational_preserving_signs(*literal_numerator, *literal_denominator);
            }
            if (!diagnostics_.empty()) {
                return nullptr;
            }

            auto numerator = lower_node(fraction->numerator);
            auto denominator = lower_node(fraction->denominator);
            if (numerator == nullptr || denominator == nullptr) {
                return nullptr;
            }

            const auto numerator_value = exact_integer_from_expr(numerator);
            const auto denominator_value = exact_integer_from_expr(denominator);
            if (numerator_value.has_value() && denominator_value.has_value()) {
                return make_rational_preserving_signs(*numerator_value, *denominator_value);
            }
            return make_expr<FunctionCall>(
                "Divide",
                std::vector<ExprPtr>{numerator, denominator});
        }
        if (const auto* unary = node->as<UnaryOpNode>()) {
            if (const auto* integer = unary->operand->as<IntegerLiteralNode>();
                integer != nullptr && unary->op == UnaryOperator::minus &&
                true) {
                return lower_integer_literal("-" + integer->decimal_text, node->span);
            }

            auto operand = lower_node(unary->operand);
            if (operand == nullptr) {
                return nullptr;
            }
            if (unary->op == UnaryOperator::plus) {
                return operand;
            }
            return lower_unary_minus(operand);
        }
        if (const auto* binary = node->as<BinaryOpNode>()) {
            auto left = lower_node(binary->left);
            auto right = lower_node(binary->right);
            if (left == nullptr || right == nullptr) {
                return nullptr;
            }
            return lower_binary(binary->op, left, right);
        }
        if (const auto* call = node->as<CallNode>()) {
            if (call->callee == "Group" && call->arguments.size() == 1) {
                return lower_node(call->arguments.front());
            }

            const bool preserve_call_unary_minus =
                call->callee != "Rational" && call->callee != "Complex";

            std::vector<ExprPtr> arguments;
            arguments.reserve(call->arguments.size());
            for (const auto& argument_node : call->arguments) {
                auto argument = preserve_call_unary_minus
                    ? lower_call_argument(argument_node)
                    : lower_node(argument_node);
                if (argument == nullptr) {
                    return nullptr;
                }
                arguments.push_back(argument);
            }

            if (call->callee == "Rational" && arguments.size() == 2) {
                const auto numerator = exact_integer_from_expr(arguments[0]);
                const auto denominator = exact_integer_from_expr(arguments[1]);
                if (numerator.has_value() && denominator.has_value()) {
                    return make_rational_preserving_signs(*numerator, *denominator);
                }
            }

            if (call->callee == "Complex" && arguments.size() == 2) {
                const auto real = finite_double_from_expr(arguments[0]);
                const auto imag = finite_double_from_expr(arguments[1]);
                if (real.has_value() && imag.has_value()) {
                    return make_expr<Complex>(*real, *imag);
                }
            }

            return make_expr<FunctionCall>(call->callee, arguments);
        }
        if (const auto* list = node->as<ListNode>()) {
            std::vector<ExprPtr> elements;
            elements.reserve(list->elements.size());
            for (const auto& element_node : list->elements) {
                auto element = lower_node(element_node);
                if (element == nullptr) {
                    return nullptr;
                }
                elements.push_back(element);
            }
            return make_expr<FunctionCall>("List", elements);
        }
        if (node->as<DefaultParameterNode>() != nullptr) {
            diagnostics_.push_back(make_error(
                "syntax.lowering.default_parameter_outside_definition",
                "Default parameters are only supported in function definitions.",
                node->span));
            return nullptr;
        }
        if (const auto* assignment = node->as<AssignmentNode>()) {
            auto value = lower_node(assignment->value);
            if (value == nullptr) {
                return nullptr;
            }
            return make_expr<Assignment>(assignment->name, value);
        }
        if (const auto* definition = node->as<FunctionDefinitionNode>()) {
            std::vector<Parameter> parameters;
            parameters.reserve(definition->parameters.size());
            for (const auto& parameter_node : definition->parameters) {
                auto parameter = lower_parameter(parameter_node);
                if (!parameter.has_value()) {
                    return nullptr;
                }
                parameters.push_back(std::move(*parameter));
            }

            auto body = lower_node(definition->body);
            if (body == nullptr) {
                return nullptr;
            }
            return make_expr<FunctionDefinition>(
                definition->name,
                parameters,
                body,
                definition->delayed);
        }

        diagnostics_.push_back(make_error(
            "syntax.lowering.unsupported_node",
            "Encountered an unsupported syntax node while lowering to Expr.",
            node->span));
        return nullptr;
    }

    ExprPtr lower_call_argument(const NodePtr& node) {
        if (const auto* unary = node->as<UnaryOpNode>();
            unary != nullptr && unary->op == UnaryOperator::minus) {
            auto operand = lower_node(unary->operand);
            if (operand == nullptr) {
                return nullptr;
            }
            return make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{operand});
        }
        return lower_node(node);
    }

    std::optional<Parameter> lower_parameter(const NodePtr& node) {
        if (const auto* default_parameter = node->as<DefaultParameterNode>()) {
            auto base = lower_parameter(default_parameter->parameter);
            if (!base.has_value()) {
                return std::nullopt;
            }
            auto default_value = lower_node(default_parameter->default_value);
            if (default_value == nullptr) {
                return std::nullopt;
            }
            base->default_value = default_value;
            return base;
        }

        if (const auto* symbol = node->as<SymbolNode>()) {
            if (!symbol->name.empty() && symbol->name.back() == '_') {
                return Parameter(strip_pattern_marker(symbol->name));
            }
        }

        diagnostics_.push_back(make_error(
            "syntax.lowering.invalid_function_parameter",
            "Function definitions require pattern parameters such as x_.",
            node->span));
        return std::nullopt;
    }

    ExprPtr lower_unary_minus(const ExprPtr& operand) {
        if (const auto* number = std::get_if<Number>(operand.get())) {
            return make_expr<Number>(-number->value);
        }
        if (const auto* rational = std::get_if<Rational>(operand.get())) {
            return make_exact_scalar_expr(-rational->exact());
        }
        if (std::holds_alternative<Symbol>(*operand)) {
            return make_expr<FunctionCall>(
                "Times",
                std::vector<ExprPtr>{make_expr<Number>(-1.0), operand});
        }
        return make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{operand});
    }

    ExprPtr lower_binary(BinaryOperator op, const ExprPtr& left, const ExprPtr& right) {
        if (op == BinaryOperator::rule) {
            return make_expr<Rule>(left, right);
        }

        if (op == BinaryOperator::replace_all) {
            return make_expr<FunctionCall>("ReplaceAll", std::vector<ExprPtr>{left, right});
        }

        if (op == BinaryOperator::divide) {
            const auto numerator = exact_integer_from_expr(left);
            const auto denominator = exact_integer_from_expr(right);
            if (numerator.has_value() && denominator.has_value()) {
                return make_exact_rational(*numerator, *denominator);
            }
        }

        if (op == BinaryOperator::string_join) {
            return lower_flattened_string_join(left, right);
        }

        return make_expr<FunctionCall>(binary_head(op), std::vector<ExprPtr>{left, right});
    }

    ExprPtr lower_flattened_string_join(const ExprPtr& left, const ExprPtr& right) {
        std::vector<ExprPtr> args;
        if (const auto* left_call = std::get_if<FunctionCall>(left.get());
            left_call != nullptr && left_call->head == "StringJoin") {
            args.insert(args.end(), left_call->args.begin(), left_call->args.end());
        } else {
            args.push_back(left);
        }
        if (const auto* right_call = std::get_if<FunctionCall>(right.get());
            right_call != nullptr && right_call->head == "StringJoin") {
            args.insert(args.end(), right_call->args.begin(), right_call->args.end());
        } else {
            args.push_back(right);
        }
        return make_expr<FunctionCall>("StringJoin", args);
    }

    ExprPtr try_make_complex(const ExprPtr& expr) {
        if (const auto* call = std::get_if<FunctionCall>(expr.get())) {
            if (call->head == "Plus") {
                if (call->args.size() == 2) {
                    const auto real = finite_double_from_expr(call->args[0]);
                    auto imag = imaginary_coefficient(call->args[1]);
                    if (real.has_value() && imag.has_value()) {
                        return make_expr<Complex>(*real, *imag);
                    }
                }
            }
            if (call->head == "Times") {
                auto imag = imaginary_coefficient(expr);
                if (imag.has_value()) {
                    return make_expr<Complex>(0.0, *imag);
                }
            }
        }
        return expr;
    }

    std::optional<double> imaginary_coefficient(const ExprPtr& expr) {
        if (is_imaginary_unit(expr)) {
            return 1.0;
        }
        const auto* call = std::get_if<FunctionCall>(expr.get());
        if (call == nullptr || call->head != "Times" || call->args.size() != 2) {
            return std::nullopt;
        }

        if (const auto* number = std::get_if<Number>(call->args[0].get());
            number != nullptr && is_imaginary_unit(call->args[1])) {
            return number->value;
        }
        if (const auto value = finite_double_from_expr(call->args[0]);
            value.has_value() && is_imaginary_unit(call->args[1])) {
            return *value;
        }
        if (const auto* number = std::get_if<Number>(call->args[1].get());
            number != nullptr && is_imaginary_unit(call->args[0])) {
            return number->value;
        }
        if (const auto value = finite_double_from_expr(call->args[1]);
            value.has_value() && is_imaginary_unit(call->args[0])) {
            return *value;
        }
        return std::nullopt;
    }

    std::vector<Diagnostic> diagnostics_;
};

std::string diagnostics_to_message(const std::vector<Diagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        return "Symbolic parse failed.";
    }
    std::ostringstream out;
    out << diagnostics.front().message;
    return out.str();
}

}  // namespace

SymbolicParseResult lower_to_expr(const NodePtr& root) {
    Lowerer lowerer;
    return lowerer.lower(root);
}

SymbolicParseResult parse_symbolic_source(std::string_view source) {
    ParserOptions options;
    options.allow_implicit_multiplication = true;
    options.parse_exact_rational_literals = true;
    Parser parser(source, options);
    auto parsed = parser.parse();
    if (!parsed.ok()) {
        return {nullptr, std::move(parsed.diagnostics)};
    }
    return lower_to_expr(parsed.root);
}

}  // namespace aleph3::syntax
