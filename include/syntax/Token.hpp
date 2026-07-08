#pragma once

#include <optional>
#include <string>
#include <variant>

#include "sdk/Types.hpp"

namespace aleph3::syntax {

enum class TokenKind {
    end_of_input,
    invalid,
    identifier,
    boolean_literal,
    number_literal,
    string_literal,
    plus,
    minus,
    star,
    slash,
    caret,
    equal,
    colon,
    colon_equal,
    equal_equal,
    bang_equal,
    less,
    less_equal,
    greater,
    greater_equal,
    amp_amp,
    pipe_pipe,
    string_join,
    rule,
    left_paren,
    right_paren,
    left_bracket,
    right_bracket,
    left_brace,
    right_brace,
    comma
};

using TokenValue = std::variant<std::monostate, double, bool, std::string>;

struct Token {
    TokenKind kind = TokenKind::invalid;
    std::string lexeme;
    aleph3::SourceSpan span;
    TokenValue value;

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return std::get_if<T>(&value);
    }
};

[[nodiscard]] const char* to_string(TokenKind kind) noexcept;

}  // namespace aleph3::syntax
