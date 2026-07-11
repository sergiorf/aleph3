#include "frontend/Parser.hpp"
#include "syntax/Lexer.hpp"
#include "syntax/Parser.hpp"
#include "syntax/SymbolicLowering.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("Shared syntax parser builds source-aware nodes for symbolic syntax", "[syntax][parser]") {
    syntax::Parser parser(R"(f[a_, "x" <> "y"] -> g[a])", syntax::ParserOptions{true});
    const auto result = parser.parse();

    REQUIRE(result.ok());
    REQUIRE(result.root->kind == syntax::NodeKind::binary_op);
    REQUIRE(result.root->span.start_offset == 0);
    REQUIRE(result.root->span.line == 1);
    REQUIRE(result.root->span.column == 1);

    const auto* rule = result.root->as<syntax::BinaryOpNode>();
    REQUIRE(rule != nullptr);
    REQUIRE(rule->op == syntax::BinaryOperator::rule);
    REQUIRE(rule->left->as<syntax::CallNode>() != nullptr);
    REQUIRE(rule->right->as<syntax::CallNode>() != nullptr);
}

TEST_CASE("Shared syntax diagnostics include code and source location", "[syntax][parser]") {
    syntax::Parser parser("x +\n)");
    const auto result = parser.parse();

    REQUIRE_FALSE(result.ok());
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics.front().code == "syntax.parser.expected_expression");
    REQUIRE(result.diagnostics.front().span.line == 2);
    REQUIRE(result.diagnostics.front().span.column == 1);
}

TEST_CASE("Symbolic lowering preserves existing symbolic forms", "[syntax][symbolic-lowering]") {
    const auto rational = syntax::parse_symbolic_source("1/2");
    REQUIRE(rational.ok());
    REQUIRE(std::holds_alternative<Rational>(*rational.expr));
    REQUIRE(std::get<Rational>(*rational.expr).numerator == 1);
    REQUIRE(std::get<Rational>(*rational.expr).denominator == 2);

    const auto definition = syntax::parse_symbolic_source("f[x_] := x^2");
    REQUIRE(definition.ok());
    REQUIRE(std::holds_alternative<FunctionDefinition>(*definition.expr));
    const auto& def = std::get<FunctionDefinition>(*definition.expr);
    REQUIRE(def.name == "f");
    REQUIRE(def.params.size() == 1);
    REQUIRE(def.params.front().name == "x");
    REQUIRE(def.delayed);
}

TEST_CASE("Shared syntax tokenizes and lowers ReplaceAll shorthand", "[syntax][parser][rewrite]") {
    syntax::Lexer lexer("f[x] /. x / y");
    const auto lexed = lexer.tokenize();

    REQUIRE(lexed.ok());
    REQUIRE(lexed.tokens.size() == 9);
    REQUIRE(lexed.tokens[4].kind == syntax::TokenKind::replace_all);
    REQUIRE(lexed.tokens[6].kind == syntax::TokenKind::slash);

    const auto lowered = syntax::parse_symbolic_source("f[x] /. x -> y");
    REQUIRE(lowered.ok());
    const auto* replace_all = std::get_if<FunctionCall>(lowered.expr.get());
    REQUIRE(replace_all != nullptr);
    REQUIRE(replace_all->head == "ReplaceAll");
    REQUIRE(replace_all->args.size() == 2);
    REQUIRE(to_string(replace_all->args[0]) == "f[x]");
    REQUIRE(std::holds_alternative<Rule>(*replace_all->args[1]));
}

TEST_CASE("Shared syntax preserves parenthesized ReplaceAll rules", "[syntax][parser][rewrite]") {
    const auto lowered = syntax::parse_symbolic_source("f[x] /. (x -> y)");

    REQUIRE(lowered.ok());
    const auto* replace_all = std::get_if<FunctionCall>(lowered.expr.get());
    REQUIRE(replace_all != nullptr);
    REQUIRE(replace_all->head == "ReplaceAll");
    REQUIRE(replace_all->args.size() == 2);
    REQUIRE(std::holds_alternative<Rule>(*replace_all->args[1]));
}

TEST_CASE("Trusted frontend rejects symbolic-only shared syntax", "[syntax][frontend]") {
    frontend::Parser parser("x = 2");
    const auto result = parser.parse();

    REQUIRE_FALSE(result.ok());
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics.front().code == "frontend.parser.unsupported_syntax");
}

TEST_CASE("Trusted frontend keeps implicit multiplication outside the SDK subset", "[syntax][frontend]") {
    frontend::Parser explicit_parser("2*x + 1");
    const auto explicit_result = explicit_parser.parse();

    REQUIRE(explicit_result.ok());

    frontend::Parser implicit_parser("2x");
    const auto implicit_result = implicit_parser.parse();

    REQUIRE_FALSE(implicit_result.ok());
    REQUIRE(implicit_result.diagnostics.size() == 1);
    REQUIRE(implicit_result.diagnostics.front().code == "frontend.parser.trailing_tokens");
    REQUIRE(implicit_result.diagnostics.front().span.start_offset == 1);
}
