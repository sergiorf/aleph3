#include "frontend/Lexer.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("Lexer tokenizes trusted-subset formulas with spans", "[frontend][lexer]") {
    frontend::Lexer lexer("If[x >= 10, \"ok\", False]");
    const auto result = lexer.tokenize();

    REQUIRE(result.ok());
    REQUIRE(result.tokens.size() == 11);

    REQUIRE(result.tokens[0].kind == frontend::TokenKind::identifier);
    REQUIRE(result.tokens[0].lexeme == "If");
    REQUIRE(result.tokens[0].span.start_offset == 0);

    REQUIRE(result.tokens[1].kind == frontend::TokenKind::left_bracket);
    REQUIRE(result.tokens[2].kind == frontend::TokenKind::identifier);
    REQUIRE(result.tokens[2].lexeme == "x");
    REQUIRE(result.tokens[3].kind == frontend::TokenKind::greater_equal);
    REQUIRE(result.tokens[4].kind == frontend::TokenKind::number_literal);
    REQUIRE(result.tokens[4].as<double>() != nullptr);
    REQUIRE(*result.tokens[4].as<double>() == 10.0);
    REQUIRE(result.tokens[6].kind == frontend::TokenKind::string_literal);
    REQUIRE(result.tokens[6].as<std::string>() != nullptr);
    REQUIRE(*result.tokens[6].as<std::string>() == "ok");
    REQUIRE(result.tokens[8].kind == frontend::TokenKind::boolean_literal);
    REQUIRE(result.tokens[8].as<bool>() != nullptr);
    REQUIRE_FALSE(*result.tokens[8].as<bool>());
    REQUIRE(result.tokens[9].kind == frontend::TokenKind::right_bracket);
    REQUIRE(result.tokens[10].kind == frontend::TokenKind::end_of_input);
}

TEST_CASE("Lexer tracks line and column information across newlines", "[frontend][lexer]") {
    frontend::Lexer lexer("x + 1\nFalse");
    const auto result = lexer.tokenize();

    REQUIRE(result.ok());
    REQUIRE(result.tokens[0].span.line == 1);
    REQUIRE(result.tokens[0].span.column == 1);
    REQUIRE(result.tokens[3].kind == frontend::TokenKind::boolean_literal);
    REQUIRE(result.tokens[3].span.line == 2);
    REQUIRE(result.tokens[3].span.column == 1);
}

TEST_CASE("Lexer reports invalid characters as structured diagnostics", "[frontend][lexer]") {
    frontend::Lexer lexer("x @ 1");
    const auto result = lexer.tokenize();

    REQUIRE_FALSE(result.ok());
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics[0].code == "frontend.lexer.invalid_character");
    REQUIRE(result.tokens[1].kind == frontend::TokenKind::invalid);
    REQUIRE(result.tokens[1].lexeme == "@");
}

TEST_CASE("Lexer reports unterminated strings", "[frontend][lexer]") {
    frontend::Lexer lexer("\"oops");
    const auto result = lexer.tokenize();

    REQUIRE_FALSE(result.ok());
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics[0].code == "frontend.lexer.unterminated_string");
    REQUIRE(result.tokens[0].kind == frontend::TokenKind::invalid);
}
