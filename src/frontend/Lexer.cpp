#include "frontend/Lexer.hpp"

#include "syntax/Lexer.hpp"

#include <string>
#include <utility>

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

std::string frontend_code_for(std::string code) {
    constexpr std::string_view syntax_prefix = "syntax.lexer.";
    if (code.starts_with(syntax_prefix)) {
        return "frontend.lexer." + code.substr(syntax_prefix.size());
    }
    return code;
}

bool map_token_kind(syntax::TokenKind kind, TokenKind& out) {
    switch (kind) {
        case syntax::TokenKind::end_of_input: out = TokenKind::end_of_input; return true;
        case syntax::TokenKind::invalid: out = TokenKind::invalid; return true;
        case syntax::TokenKind::identifier: out = TokenKind::identifier; return true;
        case syntax::TokenKind::boolean_literal: out = TokenKind::boolean_literal; return true;
        case syntax::TokenKind::number_literal: out = TokenKind::number_literal; return true;
        case syntax::TokenKind::string_literal: out = TokenKind::string_literal; return true;
        case syntax::TokenKind::plus: out = TokenKind::plus; return true;
        case syntax::TokenKind::minus: out = TokenKind::minus; return true;
        case syntax::TokenKind::star: out = TokenKind::star; return true;
        case syntax::TokenKind::slash: out = TokenKind::slash; return true;
        case syntax::TokenKind::caret: out = TokenKind::caret; return true;
        case syntax::TokenKind::equal_equal: out = TokenKind::equal_equal; return true;
        case syntax::TokenKind::bang_equal: out = TokenKind::bang_equal; return true;
        case syntax::TokenKind::less: out = TokenKind::less; return true;
        case syntax::TokenKind::less_equal: out = TokenKind::less_equal; return true;
        case syntax::TokenKind::greater: out = TokenKind::greater; return true;
        case syntax::TokenKind::greater_equal: out = TokenKind::greater_equal; return true;
        case syntax::TokenKind::left_paren: out = TokenKind::left_paren; return true;
        case syntax::TokenKind::right_paren: out = TokenKind::right_paren; return true;
        case syntax::TokenKind::left_bracket: out = TokenKind::left_bracket; return true;
        case syntax::TokenKind::right_bracket: out = TokenKind::right_bracket; return true;
        case syntax::TokenKind::comma: out = TokenKind::comma; return true;
        default:
            out = TokenKind::invalid;
            return false;
    }
}

Token map_token(const syntax::Token& syntax_token) {
    Token token;
    map_token_kind(syntax_token.kind, token.kind);
    token.lexeme = syntax_token.lexeme;
    token.span = syntax_token.span;
    token.value = syntax_token.value;
    return token;
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
        case TokenKind::equal_equal: return "equal_equal";
        case TokenKind::bang_equal: return "bang_equal";
        case TokenKind::less: return "less";
        case TokenKind::less_equal: return "less_equal";
        case TokenKind::greater: return "greater";
        case TokenKind::greater_equal: return "greater_equal";
        case TokenKind::left_paren: return "left_paren";
        case TokenKind::right_paren: return "right_paren";
        case TokenKind::left_bracket: return "left_bracket";
        case TokenKind::right_bracket: return "right_bracket";
        case TokenKind::comma: return "comma";
    }

    return "unknown";
}

Lexer::Lexer(std::string_view source) : source_(source) {}

LexResult Lexer::tokenize() const {
    syntax::Lexer lexer(source_);
    auto syntax_result = lexer.tokenize();

    LexResult result;
    result.tokens.reserve(syntax_result.tokens.size());
    for (const auto& syntax_token : syntax_result.tokens) {
        TokenKind mapped_kind;
        const bool supported = map_token_kind(syntax_token.kind, mapped_kind);
        Token token = map_token(syntax_token);
        if (!supported) {
            result.diagnostics.push_back(make_error(
                "frontend.lexer.invalid_character",
                "Encountered an unsupported character in formula source.",
                syntax_token.span));
        }
        result.tokens.push_back(std::move(token));
    }

    for (auto& diagnostic : syntax_result.diagnostics) {
        diagnostic.code = frontend_code_for(std::move(diagnostic.code));
        result.diagnostics.push_back(std::move(diagnostic));
    }
    return result;
}

}  // namespace aleph3::frontend
