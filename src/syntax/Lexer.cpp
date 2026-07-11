#include "syntax/Lexer.hpp"

#include <cctype>
#include <string>
#include <utility>

namespace aleph3::syntax {

namespace {

struct Cursor {
    std::string_view source;
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;

    [[nodiscard]] bool at_end() const noexcept {
        return offset >= source.size();
    }

    [[nodiscard]] char peek(std::size_t lookahead = 0) const noexcept {
        const std::size_t index = offset + lookahead;
        return index < source.size() ? source[index] : '\0';
    }

    char advance() noexcept {
        const char ch = peek();
        if (ch == '\0') {
            return ch;
        }

        ++offset;
        if (ch == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
        return ch;
    }

    [[nodiscard]] SourceSpan span_from(
        std::size_t start_offset,
        std::size_t start_line,
        std::size_t start_column) const noexcept {
        return SourceSpan{start_offset, offset, start_line, start_column};
    }
};

bool is_identifier_start(char ch) noexcept {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool is_identifier_continue(char ch) noexcept {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

void skip_whitespace(Cursor& cursor) noexcept {
    while (!cursor.at_end() && std::isspace(static_cast<unsigned char>(cursor.peek()))) {
        cursor.advance();
    }
}

Diagnostic make_error(std::string code, std::string message, SourceSpan span) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::error;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.span = span;
    return diagnostic;
}

Token make_simple_token(TokenKind kind, std::string lexeme, SourceSpan span) {
    Token token;
    token.kind = kind;
    token.lexeme = std::move(lexeme);
    token.span = span;
    return token;
}

Token lex_identifier_or_boolean(Cursor& cursor) {
    const std::size_t start_offset = cursor.offset;
    const std::size_t start_line = cursor.line;
    const std::size_t start_column = cursor.column;

    std::string lexeme;
    while (is_identifier_continue(cursor.peek())) {
        lexeme.push_back(cursor.advance());
    }

    Token token;
    token.span = cursor.span_from(start_offset, start_line, start_column);
    token.lexeme = lexeme;
    if (lexeme == "True") {
        token.kind = TokenKind::boolean_literal;
        token.value = true;
    } else if (lexeme == "False") {
        token.kind = TokenKind::boolean_literal;
        token.value = false;
    } else {
        token.kind = TokenKind::identifier;
    }
    return token;
}

Token lex_number(Cursor& cursor) {
    const std::size_t start_offset = cursor.offset;
    const std::size_t start_line = cursor.line;
    const std::size_t start_column = cursor.column;

    std::string lexeme;
    bool seen_dot = false;

    if (cursor.peek() == '.') {
        seen_dot = true;
        lexeme.push_back(cursor.advance());
    }

    while (true) {
        const char ch = cursor.peek();
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            lexeme.push_back(cursor.advance());
            continue;
        }
        if (ch == '.' && !seen_dot) {
            seen_dot = true;
            lexeme.push_back(cursor.advance());
            continue;
        }
        break;
    }

    Token token;
    token.kind = TokenKind::number_literal;
    token.lexeme = lexeme;
    token.span = cursor.span_from(start_offset, start_line, start_column);
    token.value = std::stod(lexeme);
    return token;
}

std::pair<Token, std::optional<Diagnostic>> lex_string(Cursor& cursor) {
    const std::size_t start_offset = cursor.offset;
    const std::size_t start_line = cursor.line;
    const std::size_t start_column = cursor.column;

    std::string lexeme;
    std::string value;
    lexeme.push_back(cursor.advance());

    while (!cursor.at_end()) {
        const char ch = cursor.advance();
        lexeme.push_back(ch);

        if (ch == '"') {
            Token token;
            token.kind = TokenKind::string_literal;
            token.lexeme = std::move(lexeme);
            token.span = cursor.span_from(start_offset, start_line, start_column);
            token.value = std::move(value);
            return {std::move(token), std::nullopt};
        }

        if (ch == '\\') {
            if (cursor.at_end()) {
                break;
            }

            const char escaped = cursor.advance();
            lexeme.push_back(escaped);
            switch (escaped) {
                case '"':
                    value.push_back('"');
                    break;
                case '\\':
                    value.push_back('\\');
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                default:
                    value.push_back(escaped);
                    break;
            }
            continue;
        }

        if (ch == '\n' || ch == '\r') {
            const auto span = cursor.span_from(start_offset, start_line, start_column);
            Token token = make_simple_token(TokenKind::invalid, std::move(lexeme), span);
            return {
                std::move(token),
                make_error(
                    "syntax.lexer.unterminated_string",
                    "String literal must be terminated before the end of the line.",
                    span)};
        }

        value.push_back(ch);
    }

    const auto span = cursor.span_from(start_offset, start_line, start_column);
    Token token = make_simple_token(TokenKind::invalid, std::move(lexeme), span);
    return {
        std::move(token),
        make_error(
            "syntax.lexer.unterminated_string",
            "String literal reached end of input before the closing quote.",
            span)};
}

std::pair<Token, std::optional<Diagnostic>> next_token(Cursor& cursor) {
    skip_whitespace(cursor);

    if (cursor.at_end()) {
        return {make_simple_token(
                    TokenKind::end_of_input,
                    "",
                    SourceSpan{cursor.offset, cursor.offset, cursor.line, cursor.column}),
                std::nullopt};
    }

    const std::size_t start_offset = cursor.offset;
    const std::size_t start_line = cursor.line;
    const std::size_t start_column = cursor.column;
    const char ch = cursor.peek();

    if (is_identifier_start(ch)) {
        return {lex_identifier_or_boolean(cursor), std::nullopt};
    }

    if (std::isdigit(static_cast<unsigned char>(ch)) ||
        (ch == '.' && std::isdigit(static_cast<unsigned char>(cursor.peek(1))))) {
        return {lex_number(cursor), std::nullopt};
    }

    const auto span = [&] {
        return cursor.span_from(start_offset, start_line, start_column);
    };

    switch (ch) {
        case '"':
            return lex_string(cursor);
        case '+':
            cursor.advance();
            return {make_simple_token(TokenKind::plus, "+", span()), std::nullopt};
        case '-':
            cursor.advance();
            if (cursor.peek() == '>') {
                cursor.advance();
                return {make_simple_token(TokenKind::rule, "->", span()), std::nullopt};
            }
            return {make_simple_token(TokenKind::minus, "-", span()), std::nullopt};
        case '*':
            cursor.advance();
            return {make_simple_token(TokenKind::star, "*", span()), std::nullopt};
        case '/':
            cursor.advance();
            if (cursor.peek() == '.') {
                cursor.advance();
                return {make_simple_token(TokenKind::replace_all, "/.", span()), std::nullopt};
            }
            return {make_simple_token(TokenKind::slash, "/", span()), std::nullopt};
        case '^':
            cursor.advance();
            return {make_simple_token(TokenKind::caret, "^", span()), std::nullopt};
        case '(':
            cursor.advance();
            return {make_simple_token(TokenKind::left_paren, "(", span()), std::nullopt};
        case ')':
            cursor.advance();
            return {make_simple_token(TokenKind::right_paren, ")", span()), std::nullopt};
        case '[':
            cursor.advance();
            return {make_simple_token(TokenKind::left_bracket, "[", span()), std::nullopt};
        case ']':
            cursor.advance();
            return {make_simple_token(TokenKind::right_bracket, "]", span()), std::nullopt};
        case '{':
            cursor.advance();
            return {make_simple_token(TokenKind::left_brace, "{", span()), std::nullopt};
        case '}':
            cursor.advance();
            return {make_simple_token(TokenKind::right_brace, "}", span()), std::nullopt};
        case ',':
            cursor.advance();
            return {make_simple_token(TokenKind::comma, ",", span()), std::nullopt};
        case ':':
            cursor.advance();
            if (cursor.peek() == '=') {
                cursor.advance();
                return {make_simple_token(TokenKind::colon_equal, ":=", span()), std::nullopt};
            }
            return {make_simple_token(TokenKind::colon, ":", span()), std::nullopt};
        case '=':
            cursor.advance();
            if (cursor.peek() == '=') {
                cursor.advance();
                return {make_simple_token(TokenKind::equal_equal, "==", span()), std::nullopt};
            }
            return {make_simple_token(TokenKind::equal, "=", span()), std::nullopt};
        case '!':
            cursor.advance();
            if (cursor.peek() == '=') {
                cursor.advance();
                return {make_simple_token(TokenKind::bang_equal, "!=", span()), std::nullopt};
            }
            break;
        case '<':
            cursor.advance();
            if (cursor.peek() == '=') {
                cursor.advance();
                return {make_simple_token(TokenKind::less_equal, "<=", span()), std::nullopt};
            }
            if (cursor.peek() == '>') {
                cursor.advance();
                return {make_simple_token(TokenKind::string_join, "<>", span()), std::nullopt};
            }
            return {make_simple_token(TokenKind::less, "<", span()), std::nullopt};
        case '>':
            cursor.advance();
            if (cursor.peek() == '=') {
                cursor.advance();
                return {make_simple_token(TokenKind::greater_equal, ">=", span()), std::nullopt};
            }
            return {make_simple_token(TokenKind::greater, ">", span()), std::nullopt};
        case '&':
            cursor.advance();
            if (cursor.peek() == '&') {
                cursor.advance();
                return {make_simple_token(TokenKind::amp_amp, "&&", span()), std::nullopt};
            }
            break;
        case '|':
            cursor.advance();
            if (cursor.peek() == '|') {
                cursor.advance();
                return {make_simple_token(TokenKind::pipe_pipe, "||", span()), std::nullopt};
            }
            break;
        default:
            break;
    }

    if (cursor.peek() == ch) {
        cursor.advance();
    }
    const auto invalid_span = cursor.span_from(start_offset, start_line, start_column);
    Token token = make_simple_token(TokenKind::invalid, std::string(1, ch), invalid_span);
    return {
        std::move(token),
        make_error(
            "syntax.lexer.invalid_character",
            "Encountered an unsupported character in formula source.",
            invalid_span)};
}

}  // namespace

const char* to_string(TokenKind kind) noexcept {
    switch (kind) {
        case TokenKind::end_of_input: return "end_of_input";
        case TokenKind::invalid: return "invalid";
        case TokenKind::identifier: return "identifier";
        case TokenKind::boolean_literal: return "boolean_literal";
        case TokenKind::number_literal: return "number_literal";
        case TokenKind::string_literal: return "string_literal";
        case TokenKind::plus: return "plus";
        case TokenKind::minus: return "minus";
        case TokenKind::star: return "star";
        case TokenKind::slash: return "slash";
        case TokenKind::caret: return "caret";
        case TokenKind::equal: return "equal";
        case TokenKind::colon: return "colon";
        case TokenKind::colon_equal: return "colon_equal";
        case TokenKind::equal_equal: return "equal_equal";
        case TokenKind::bang_equal: return "bang_equal";
        case TokenKind::less: return "less";
        case TokenKind::less_equal: return "less_equal";
        case TokenKind::greater: return "greater";
        case TokenKind::greater_equal: return "greater_equal";
        case TokenKind::amp_amp: return "amp_amp";
        case TokenKind::pipe_pipe: return "pipe_pipe";
        case TokenKind::string_join: return "string_join";
        case TokenKind::rule: return "rule";
        case TokenKind::replace_all: return "replace_all";
        case TokenKind::left_paren: return "left_paren";
        case TokenKind::right_paren: return "right_paren";
        case TokenKind::left_bracket: return "left_bracket";
        case TokenKind::right_bracket: return "right_bracket";
        case TokenKind::left_brace: return "left_brace";
        case TokenKind::right_brace: return "right_brace";
        case TokenKind::comma: return "comma";
    }

    return "unknown";
}

Lexer::Lexer(std::string_view source) : source_(source) {}

LexResult Lexer::tokenize() const {
    LexResult result;
    Cursor cursor{source_};

    while (true) {
        auto [token, diagnostic] = next_token(cursor);
        if (diagnostic.has_value()) {
            result.diagnostics.push_back(std::move(*diagnostic));
        }

        const bool reached_end = token.kind == TokenKind::end_of_input;
        result.tokens.push_back(std::move(token));
        if (reached_end) {
            break;
        }
    }

    return result;
}

}  // namespace aleph3::syntax
