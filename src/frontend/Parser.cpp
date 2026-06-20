#include "frontend/Parser.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace aleph3::frontend {

namespace {

Diagnostic make_error(std::string code, std::string message, SourceSpan span) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::error;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.span = span;
    return diagnostic;
}

SourceSpan merge_spans(const SourceSpan& start, const SourceSpan& end) noexcept {
    return SourceSpan{start.start_offset, end.end_offset, start.line, start.column};
}

std::optional<ir::BinaryOperator> to_binary_operator(TokenKind kind) {
    switch (kind) {
        case TokenKind::plus:
            return ir::BinaryOperator::add;
        case TokenKind::minus:
            return ir::BinaryOperator::subtract;
        case TokenKind::star:
            return ir::BinaryOperator::multiply;
        case TokenKind::slash:
            return ir::BinaryOperator::divide;
        case TokenKind::caret:
            return ir::BinaryOperator::power;
        case TokenKind::equal_equal:
            return ir::BinaryOperator::equal;
        case TokenKind::bang_equal:
            return ir::BinaryOperator::not_equal;
        case TokenKind::less:
            return ir::BinaryOperator::less;
        case TokenKind::less_equal:
            return ir::BinaryOperator::less_equal;
        case TokenKind::greater:
            return ir::BinaryOperator::greater;
        case TokenKind::greater_equal:
            return ir::BinaryOperator::greater_equal;
        default:
            return std::nullopt;
    }
}

int precedence(TokenKind kind) noexcept {
    switch (kind) {
        case TokenKind::equal_equal:
        case TokenKind::bang_equal:
        case TokenKind::less:
        case TokenKind::less_equal:
        case TokenKind::greater:
        case TokenKind::greater_equal:
            return 10;
        case TokenKind::plus:
        case TokenKind::minus:
            return 20;
        case TokenKind::star:
        case TokenKind::slash:
            return 30;
        case TokenKind::caret:
            return 40;
        default:
            return -1;
    }
}

bool is_right_associative(TokenKind kind) noexcept {
    return kind == TokenKind::caret;
}

class ParserImpl {
public:
    explicit ParserImpl(std::vector<Token> token_stream)
        : tokens_(std::move(token_stream)) {}

    ParseResult parse() {
        ParseResult result;

        if (tokens_.empty()) {
            result.diagnostics.push_back(make_error(
                "frontend.parser.empty_token_stream",
                "Parser received an empty token stream.",
                {}));
            return result;
        }

        result.root = parse_expression(0);

        if (result.root == nullptr && diagnostics_.empty()) {
            diagnostics_.push_back(make_error(
                "frontend.parser.expected_expression",
                "Expected an expression.",
                current().span));
        }

        if (diagnostics_.empty() && !is_at_end()) {
            diagnostics_.push_back(make_error(
                "frontend.parser.trailing_tokens",
                "Unexpected trailing tokens after the end of the expression.",
                current().span));
        }

        result.diagnostics = std::move(diagnostics_);
        if (!result.diagnostics.empty()) {
            result.root.reset();
        }
        return result;
    }

private:
    const Token& current() const noexcept {
        return tokens_[position_];
    }

    const Token& previous() const noexcept {
        return tokens_[position_ - 1];
    }

    bool is_at_end() const noexcept {
        return current().kind == TokenKind::end_of_input;
    }

    bool match(TokenKind kind) noexcept {
        if (current().kind != kind) {
            return false;
        }
        advance();
        return true;
    }

    const Token& advance() noexcept {
        if (!is_at_end()) {
            ++position_;
        }
        return previous();
    }

    bool expect(TokenKind kind, std::string code, std::string message) {
        if (match(kind)) {
            return true;
        }

        diagnostics_.push_back(make_error(std::move(code), std::move(message), current().span));
        return false;
    }

    ir::NodePtr parse_expression(int min_precedence) {
        auto left = parse_unary();
        if (left == nullptr) {
            return nullptr;
        }

        while (true) {
            const TokenKind op_kind = current().kind;
            const int op_precedence = precedence(op_kind);
            if (op_precedence < min_precedence) {
                break;
            }

            const Token operator_token = advance();
            const auto binary_operator = to_binary_operator(operator_token.kind);
            if (!binary_operator.has_value()) {
                diagnostics_.push_back(make_error(
                    "frontend.parser.unsupported_operator",
                    "Encountered an unsupported operator.",
                    operator_token.span));
                return nullptr;
            }

            const int next_min_precedence = is_right_associative(operator_token.kind)
                ? op_precedence
                : op_precedence + 1;
            auto right = parse_expression(next_min_precedence);
            if (right == nullptr) {
                if (diagnostics_.empty()) {
                    diagnostics_.push_back(make_error(
                        "frontend.parser.expected_expression",
                        "Expected an expression after the operator.",
                        current().span));
                }
                return nullptr;
            }

            left = ir::make_node(
                merge_spans(left->span, right->span),
                ir::BinaryOpNode{*binary_operator, left, right});
        }

        return left;
    }

    ir::NodePtr parse_unary() {
        if (current().kind == TokenKind::plus || current().kind == TokenKind::minus) {
            const Token operator_token = advance();
            auto operand = parse_unary();
            if (operand == nullptr) {
                if (diagnostics_.empty()) {
                    diagnostics_.push_back(make_error(
                        "frontend.parser.expected_expression",
                        "Expected an expression after the unary operator.",
                        current().span));
                }
                return nullptr;
            }

            const auto unary_operator = operator_token.kind == TokenKind::plus
                ? ir::UnaryOperator::plus
                : ir::UnaryOperator::minus;
            return ir::make_node(
                merge_spans(operator_token.span, operand->span),
                ir::UnaryOpNode{unary_operator, operand});
        }

        return parse_primary();
    }

    ir::NodePtr parse_primary() {
        const Token token = current();
        switch (token.kind) {
            case TokenKind::number_literal:
                advance();
                return ir::make_node(token.span, ir::NumberLiteralNode{*token.as<double>()});
            case TokenKind::boolean_literal:
                advance();
                return ir::make_node(token.span, ir::BooleanLiteralNode{*token.as<bool>()});
            case TokenKind::string_literal:
                advance();
                return ir::make_node(token.span, ir::StringLiteralNode{*token.as<std::string>()});
            case TokenKind::identifier:
                return parse_identifier_expression();
            case TokenKind::left_paren:
                return parse_grouped_expression();
            case TokenKind::invalid:
                diagnostics_.push_back(make_error(
                    "frontend.parser.invalid_token",
                    "Encountered an invalid token while parsing.",
                    token.span));
                advance();
                return nullptr;
            default:
                diagnostics_.push_back(make_error(
                    "frontend.parser.expected_expression",
                    "Expected an expression.",
                    token.span));
                return nullptr;
        }
    }

    ir::NodePtr parse_identifier_expression() {
        const Token identifier = advance();
        if (!match(TokenKind::left_bracket)) {
            return ir::make_node(identifier.span, ir::VariableNode{identifier.lexeme});
        }

        std::vector<ir::NodePtr> arguments;
        if (current().kind != TokenKind::right_bracket) {
            while (true) {
                auto argument = parse_expression(0);
                if (argument == nullptr) {
                    return nullptr;
                }
                arguments.push_back(argument);

                if (!match(TokenKind::comma)) {
                    break;
                }
            }
        }

        if (!expect(
                TokenKind::right_bracket,
                "frontend.parser.expected_right_bracket",
                "Expected ']' to close the function call.")) {
            return nullptr;
        }

        const SourceSpan span = merge_spans(identifier.span, previous().span);
        if (identifier.lexeme == "If") {
            if (arguments.size() != 3) {
                diagnostics_.push_back(make_error(
                    "frontend.parser.invalid_if_arity",
                    "If requires exactly three arguments.",
                    span));
                return nullptr;
            }

            return ir::make_node(
                span,
                ir::IfNode{arguments[0], arguments[1], arguments[2]});
        }

        return ir::make_node(
            span,
            ir::CallNode{identifier.lexeme, std::move(arguments)});
    }

    ir::NodePtr parse_grouped_expression() {
        advance();
        auto expression = parse_expression(0);
        if (expression == nullptr) {
            return nullptr;
        }

        if (!expect(
                TokenKind::right_paren,
                "frontend.parser.expected_right_paren",
                "Expected ')' to close the grouped expression.")) {
            return nullptr;
        }

        return expression;
    }

    std::vector<Token> tokens_;
    std::size_t position_ = 0;
    std::vector<Diagnostic> diagnostics_;
};

}  // namespace

Parser::Parser(std::string_view source) : source_(source) {}

ParseResult Parser::parse() const {
    Lexer lexer(source_);
    auto lex_result = lexer.tokenize();

    ParseResult result;
    if (!lex_result.ok()) {
        result.diagnostics = std::move(lex_result.diagnostics);
        return result;
    }

    ParserImpl parser(std::move(lex_result.tokens));
    result = parser.parse();
    result.diagnostics.insert(
        result.diagnostics.begin(),
        lex_result.diagnostics.begin(),
        lex_result.diagnostics.end());
    return result;
}

}  // namespace aleph3::frontend
