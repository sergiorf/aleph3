#include "syntax/Parser.hpp"

#include <optional>
#include <string>
#include <utility>

namespace aleph3::syntax {

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

std::optional<BinaryOperator> to_binary_operator(TokenKind kind) {
    switch (kind) {
        case TokenKind::rule: return BinaryOperator::rule;
        case TokenKind::replace_all: return BinaryOperator::replace_all;
        case TokenKind::pipe_pipe: return BinaryOperator::or_op;
        case TokenKind::amp_amp: return BinaryOperator::and_op;
        case TokenKind::string_join: return BinaryOperator::string_join;
        case TokenKind::plus: return BinaryOperator::add;
        case TokenKind::minus: return BinaryOperator::subtract;
        case TokenKind::star: return BinaryOperator::multiply;
        case TokenKind::slash: return BinaryOperator::divide;
        case TokenKind::caret: return BinaryOperator::power;
        case TokenKind::equal_equal: return BinaryOperator::equal;
        case TokenKind::bang_equal: return BinaryOperator::not_equal;
        case TokenKind::less: return BinaryOperator::less;
        case TokenKind::less_equal: return BinaryOperator::less_equal;
        case TokenKind::greater: return BinaryOperator::greater;
        case TokenKind::greater_equal: return BinaryOperator::greater_equal;
        default: return std::nullopt;
    }
}

int precedence(TokenKind kind) noexcept {
    switch (kind) {
        case TokenKind::replace_all:
            return 1;
        case TokenKind::rule:
            return 2;
        case TokenKind::equal_equal:
        case TokenKind::bang_equal:
        case TokenKind::less:
        case TokenKind::less_equal:
        case TokenKind::greater:
        case TokenKind::greater_equal:
            return 3;
        case TokenKind::pipe_pipe:
            return 4;
        case TokenKind::amp_amp:
            return 5;
        case TokenKind::string_join:
            return 6;
        case TokenKind::plus:
        case TokenKind::minus:
            return 7;
        case TokenKind::star:
        case TokenKind::slash:
            return 8;
        case TokenKind::caret:
            return 9;
        default:
            return -1;
    }
}

bool is_right_associative(TokenKind kind) noexcept {
    return kind == TokenKind::caret || kind == TokenKind::rule;
}

bool can_start_primary(TokenKind kind) noexcept {
    switch (kind) {
        case TokenKind::identifier:
        case TokenKind::boolean_literal:
        case TokenKind::number_literal:
        case TokenKind::string_literal:
        case TokenKind::left_paren:
        case TokenKind::left_brace:
            return true;
        default:
            return false;
    }
}

class ParserImpl {
public:
    ParserImpl(std::vector<Token> token_stream, ParserOptions options)
        : tokens_(std::move(token_stream)), options_(options) {}

    ParseResult parse() {
        ParseResult result;

        if (tokens_.empty()) {
            result.diagnostics.push_back(make_error(
                "syntax.parser.empty_token_stream",
                "Parser received an empty token stream.",
                {}));
            return result;
        }

        result.root = parse_top_level();

        if (result.root == nullptr && diagnostics_.empty()) {
            diagnostics_.push_back(make_error(
                "syntax.parser.expected_expression",
                "Expected an expression.",
                current().span));
        }

        if (diagnostics_.empty() && !is_at_end()) {
            diagnostics_.push_back(make_error(
                "syntax.parser.trailing_tokens",
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

    NodePtr parse_top_level() {
        auto left = parse_expression(1);
        if (left == nullptr) {
            return nullptr;
        }

        if (current().kind != TokenKind::equal && current().kind != TokenKind::colon_equal) {
            return left;
        }

        const Token assignment_token = advance();
        auto right = parse_expression(1);
        if (right == nullptr) {
            if (diagnostics_.empty()) {
                diagnostics_.push_back(make_error(
                    "syntax.parser.expected_expression",
                    "Expected an expression after the assignment operator.",
                    current().span));
            }
            return nullptr;
        }

        if (const auto* symbol = left->as<SymbolNode>()) {
            if (assignment_token.kind == TokenKind::equal) {
                return make_node(
                    merge_spans(left->span, right->span),
                    AssignmentNode{symbol->name, right});
            }
        }

        if (const auto* call = left->as<CallNode>()) {
            return make_node(
                merge_spans(left->span, right->span),
                FunctionDefinitionNode{
                    call->callee,
                    call->arguments,
                    right,
                    assignment_token.kind == TokenKind::colon_equal});
        }

        diagnostics_.push_back(make_error(
            "syntax.parser.invalid_assignment_target",
            "Expected a symbol or function pattern before the assignment operator.",
            assignment_token.span));
        return nullptr;
    }

    NodePtr parse_expression(int min_precedence) {
        auto left = parse_unary();
        if (left == nullptr) {
            return nullptr;
        }

        while (true) {
            if (options_.allow_implicit_multiplication && can_start_primary(current().kind) &&
                precedence(TokenKind::star) >= min_precedence) {
                auto right = parse_expression(precedence(TokenKind::star) + 1);
                if (right == nullptr) {
                    return nullptr;
                }
                left = make_node(
                    merge_spans(left->span, right->span),
                    BinaryOpNode{BinaryOperator::multiply, left, right, true});
                continue;
            }

            const TokenKind op_kind = current().kind;
            const int op_precedence = precedence(op_kind);
            if (op_precedence < min_precedence) {
                break;
            }

            const Token operator_token = advance();
            const auto binary_operator = to_binary_operator(operator_token.kind);
            if (!binary_operator.has_value()) {
                diagnostics_.push_back(make_error(
                    "syntax.parser.unsupported_operator",
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
                        "syntax.parser.expected_expression",
                        "Expected an expression after the operator.",
                        current().span));
                }
                return nullptr;
            }

            if (options_.allow_implicit_multiplication &&
                operator_token.kind == TokenKind::slash) {
                while (can_start_primary(current().kind)) {
                    auto factor = parse_expression(precedence(TokenKind::star) + 1);
                    if (factor == nullptr) {
                        return nullptr;
                    }
                    right = make_node(
                        merge_spans(right->span, factor->span),
                        BinaryOpNode{BinaryOperator::multiply, right, factor, true});
                }
            }

            left = make_node(
                merge_spans(left->span, right->span),
                BinaryOpNode{*binary_operator, left, right, false});
        }

        return left;
    }

    NodePtr parse_unary() {
        if (current().kind == TokenKind::plus || current().kind == TokenKind::minus) {
            const Token operator_token = advance();
            auto operand = parse_unary();
            if (operand == nullptr) {
                if (diagnostics_.empty()) {
                    diagnostics_.push_back(make_error(
                        "syntax.parser.expected_expression",
                        "Expected an expression after the unary operator.",
                        current().span));
                }
                return nullptr;
            }

            const auto unary_operator = operator_token.kind == TokenKind::plus
                ? UnaryOperator::plus
                : UnaryOperator::minus;
            return make_node(
                merge_spans(operator_token.span, operand->span),
                UnaryOpNode{unary_operator, operand});
        }

        return parse_primary();
    }

    NodePtr parse_primary() {
        const Token token = current();
        switch (token.kind) {
            case TokenKind::number_literal:
                advance();
                return parse_number_literal(token);
            case TokenKind::boolean_literal:
                advance();
                return make_node(token.span, BooleanLiteralNode{*token.as<bool>()});
            case TokenKind::string_literal:
                advance();
                return make_node(token.span, StringLiteralNode{*token.as<std::string>()});
            case TokenKind::identifier:
                return parse_identifier_expression();
            case TokenKind::left_paren:
                return parse_grouped_expression();
            case TokenKind::left_brace:
                return parse_list_expression();
            case TokenKind::invalid:
                diagnostics_.push_back(make_error(
                    "syntax.parser.invalid_token",
                    "Encountered an invalid token while parsing.",
                    token.span));
                advance();
                return nullptr;
            default:
                diagnostics_.push_back(make_error(
                    "syntax.parser.expected_expression",
                    "Expected an expression.",
                    token.span));
                return nullptr;
        }
    }

    NodePtr parse_number_literal(const Token& token) {
        auto number = make_node(
            token.span,
            NumberLiteralNode{*token.as<double>(), token.lexeme});

        if (!options_.parse_exact_rational_literals ||
            current().kind != TokenKind::slash ||
            !has_rational_denominator_ahead()) {
            return number;
        }

        advance();
        bool denominator_is_negative = false;
        Token minus_token;
        if (current().kind == TokenKind::minus) {
            denominator_is_negative = true;
            minus_token = advance();
        }

        if (current().kind != TokenKind::number_literal) {
            diagnostics_.push_back(make_error(
                "syntax.parser.expected_rational_denominator",
                "Expected a numeric denominator in the rational literal.",
                current().span));
            return nullptr;
        }

        const Token denominator_token = advance();
        auto denominator = make_node(
            denominator_token.span,
            NumberLiteralNode{*denominator_token.as<double>(), denominator_token.lexeme});

        if (denominator_is_negative) {
            denominator = make_node(
                merge_spans(minus_token.span, denominator->span),
                UnaryOpNode{UnaryOperator::minus, denominator});
        }

        return make_node(
            merge_spans(number->span, denominator->span),
            FractionLiteralNode{number, denominator});
    }

    bool has_rational_denominator_ahead() const noexcept {
        std::size_t lookahead = position_ + 1;
        if (lookahead < tokens_.size() && tokens_[lookahead].kind == TokenKind::minus) {
            ++lookahead;
        }
        return lookahead < tokens_.size() && tokens_[lookahead].kind == TokenKind::number_literal;
    }

    NodePtr parse_identifier_expression() {
        const Token identifier = advance();
        if (!match(TokenKind::left_bracket)) {
            return make_node(identifier.span, SymbolNode{identifier.lexeme});
        }

        std::vector<NodePtr> arguments;
        if (current().kind != TokenKind::right_bracket) {
            while (true) {
                auto argument = parse_expression(1);
                if (argument == nullptr) {
                    return nullptr;
                }
                if (match(TokenKind::colon)) {
                    auto default_value = parse_expression(1);
                    if (default_value == nullptr) {
                        return nullptr;
                    }
                    argument = make_node(
                        merge_spans(argument->span, default_value->span),
                        DefaultParameterNode{argument, default_value});
                }
                arguments.push_back(argument);

                if (!match(TokenKind::comma)) {
                    break;
                }
            }
        }

        if (!expect(
                TokenKind::right_bracket,
                "syntax.parser.expected_right_bracket",
                "Expected ']' to close the function call.")) {
            return nullptr;
        }

        return make_node(
            merge_spans(identifier.span, previous().span),
            CallNode{identifier.lexeme, std::move(arguments)});
    }

    NodePtr parse_grouped_expression() {
        const Token left = advance();
        auto expression = parse_expression(1);
        if (expression == nullptr) {
            return nullptr;
        }

        if (!expect(
                TokenKind::right_paren,
                "syntax.parser.expected_right_paren",
                "Expected ')' to close the grouped expression.")) {
            return nullptr;
        }

        return make_node(
            merge_spans(left.span, previous().span),
            CallNode{"Group", {expression}});
    }

    NodePtr parse_list_expression() {
        const Token left = advance();
        std::vector<NodePtr> elements;
        if (current().kind != TokenKind::right_brace) {
            while (true) {
                auto element = parse_expression(1);
                if (element == nullptr) {
                    return nullptr;
                }
                elements.push_back(element);

                if (!match(TokenKind::comma)) {
                    break;
                }
            }
        }

        if (!expect(
                TokenKind::right_brace,
                "syntax.parser.expected_right_brace",
                "Expected '}' to close the list expression.")) {
            return nullptr;
        }

        return make_node(
            merge_spans(left.span, previous().span),
            ListNode{std::move(elements)});
    }

    std::vector<Token> tokens_;
    ParserOptions options_;
    std::size_t position_ = 0;
    std::vector<Diagnostic> diagnostics_;
};

}  // namespace

Parser::Parser(std::string_view source, ParserOptions options)
    : source_(source), options_(options) {}

ParseResult Parser::parse() const {
    Lexer lexer(source_);
    auto lex_result = lexer.tokenize();

    ParseResult result;
    if (!lex_result.ok()) {
        result.diagnostics = std::move(lex_result.diagnostics);
        return result;
    }

    ParserImpl parser(std::move(lex_result.tokens), options_);
    result = parser.parse();
    result.diagnostics.insert(
        result.diagnostics.begin(),
        lex_result.diagnostics.begin(),
        lex_result.diagnostics.end());
    return result;
}

}  // namespace aleph3::syntax
