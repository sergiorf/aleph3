#include "parser/Parser.hpp"
#include "ParserTestHelper.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>

using namespace aleph3;

TEST_CASE("parse_symbol basic ASCII symbols") {
    Parser p("x");
    auto sym = ParserTestHelper::parse_symbol(p);
    REQUIRE(std::holds_alternative<Symbol>(*sym));
    REQUIRE(std::get<Symbol>(*sym).name == "x");

    Parser p2("foo_bar");
    auto sym2 = ParserTestHelper::parse_symbol(p2);
    REQUIRE(std::holds_alternative<Symbol>(*sym2));
    REQUIRE(std::get<Symbol>(*sym2).name == "foo_bar");

    Parser p3("foo123");
    auto sym3 = ParserTestHelper::parse_symbol(p3);
    REQUIRE(std::holds_alternative<Symbol>(*sym3));
    REQUIRE(std::get<Symbol>(*sym3).name == "foo123");
}

TEST_CASE("parse_symbol unicode symbols") {
    std::string mu = "\xC2\xB5";
    Parser p(mu);
    auto sym = ParserTestHelper::parse_symbol(p);
    REQUIRE(std::holds_alternative<Symbol>(*sym));
    REQUIRE(std::get<Symbol>(*sym).name == mu);

    std::string alpha_beta_gamma = "\xCE\xB1\xCE\xB2\xCE\xB3";
    Parser p2(alpha_beta_gamma);
    auto sym2 = ParserTestHelper::parse_symbol(p2);
    REQUIRE(std::holds_alternative<Symbol>(*sym2));
    REQUIRE(std::get<Symbol>(*sym2).name == alpha_beta_gamma);
}

TEST_CASE("parse_symbol anonymous pattern") {
    Parser p("_");
    auto sym = ParserTestHelper::parse_symbol(p);
    REQUIRE(std::holds_alternative<Symbol>(*sym));
    REQUIRE(std::get<Symbol>(*sym).name == "_");
}

TEST_CASE("parse_symbol boolean literals") {
    Parser p("True");
    auto expr = ParserTestHelper::parse_symbol(p);
    REQUIRE(std::holds_alternative<Boolean>(*expr));
    REQUIRE(std::get<Boolean>(*expr).value == true);

    Parser p2("False");
    auto expr2 = ParserTestHelper::parse_symbol(p2);
    REQUIRE(std::holds_alternative<Boolean>(*expr2));
    REQUIRE(std::get<Boolean>(*expr2).value == false);
}

TEST_CASE("parse_symbol function call") {
    Parser p("f[x, y]");
    auto call = ParserTestHelper::parse_symbol(p);
    REQUIRE(std::holds_alternative<FunctionCall>(*call));
    const auto& fc = std::get<FunctionCall>(*call);
    REQUIRE(fc.head == "f");
    REQUIRE(fc.args.size() == 2);
    REQUIRE(std::holds_alternative<Symbol>(*fc.args[0]));
    REQUIRE(std::get<Symbol>(*fc.args[0]).name == "x");
    REQUIRE(std::holds_alternative<Symbol>(*fc.args[1]));
    REQUIRE(std::get<Symbol>(*fc.args[1]).name == "y");

    Parser p2("foo[]");
    auto call2 = ParserTestHelper::parse_symbol(p2);
    REQUIRE(std::holds_alternative<FunctionCall>(*call2));
    const auto& fc2 = std::get<FunctionCall>(*call2);
    REQUIRE(fc2.head == "foo");
    REQUIRE(fc2.args.empty());
}

TEST_CASE("parse_symbol whitespace handling") {
    Parser p("   foo");
    auto sym = ParserTestHelper::parse_symbol(p);
    REQUIRE(std::holds_alternative<Symbol>(*sym));
    REQUIRE(std::get<Symbol>(*sym).name == "foo");

    Parser p2("\tbar");
    auto sym2 = ParserTestHelper::parse_symbol(p2);
    REQUIRE(std::holds_alternative<Symbol>(*sym2));
    REQUIRE(std::get<Symbol>(*sym2).name == "bar");
}

TEST_CASE("parse_symbol invalid input") {
    Parser p("1abc");
    REQUIRE_THROWS_AS(ParserTestHelper::parse_symbol(p), std::runtime_error);

    Parser p2("!");
    REQUIRE_THROWS_AS(ParserTestHelper::parse_symbol(p2), std::runtime_error);

    Parser p3("");
    REQUIRE_THROWS_AS(ParserTestHelper::parse_symbol(p3), std::runtime_error);
}

TEST_CASE("parse_symbol nested function call") {
    Parser p("f[g[x], y]");
    auto call = ParserTestHelper::parse_symbol(p);
    REQUIRE(std::holds_alternative<FunctionCall>(*call));
    const auto& fc = std::get<FunctionCall>(*call);
    REQUIRE(fc.head == "f");
    REQUIRE(fc.args.size() == 2);
    REQUIRE(std::holds_alternative<FunctionCall>(*fc.args[0]));
    const auto& inner_fc = std::get<FunctionCall>(*fc.args[0]);
    REQUIRE(inner_fc.head == "g");
    REQUIRE(inner_fc.args.size() == 1);
    REQUIRE(std::holds_alternative<Symbol>(*inner_fc.args[0]));
    REQUIRE(std::get<Symbol>(*inner_fc.args[0]).name == "x");
    REQUIRE(std::holds_alternative<Symbol>(*fc.args[1]));
    REQUIRE(std::get<Symbol>(*fc.args[1]).name == "y");
}

TEST_CASE("parse_symbol with negative argument") {
    Parser p("f[-x]");
    auto call = ParserTestHelper::parse_symbol(p);
    REQUIRE(std::holds_alternative<FunctionCall>(*call));
    const auto& fc = std::get<FunctionCall>(*call);
    REQUIRE(fc.head == "f");
    REQUIRE(fc.args.size() == 1);
    REQUIRE(std::holds_alternative<FunctionCall>(*fc.args[0]));
    const auto& neg = std::get<FunctionCall>(*fc.args[0]);
    REQUIRE(neg.head == "Negate");
    REQUIRE(neg.args.size() == 1);
    REQUIRE(std::holds_alternative<Symbol>(*neg.args[0]));
    REQUIRE(std::get<Symbol>(*neg.args[0]).name == "x");
}