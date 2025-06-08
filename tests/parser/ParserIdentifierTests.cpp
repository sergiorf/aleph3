#include "parser/Parser.hpp"
#include "ParserTestHelper.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>

using namespace aleph3;

TEST_CASE("parse_identifier basic ASCII") {
    Parser p("x");
    REQUIRE(ParserTestHelper::parse_identifier(p) == "x");

    Parser p2("foo123");
    REQUIRE(ParserTestHelper::parse_identifier(p2) == "foo123");
}

TEST_CASE("parse_identifier unicode") {
    // µ = U+00B5, α = U+03B1, β = U+03B2, γ = U+03B3 (UTF-8 byte sequences)
    std::string mu = "\xC2\xB5";
    Parser p(mu);
    REQUIRE(ParserTestHelper::parse_identifier(p) == mu);

    std::string alpha_beta_gamma = "\xCE\xB1\xCE\xB2\xCE\xB3";
    Parser p2(alpha_beta_gamma);
    REQUIRE(ParserTestHelper::parse_identifier(p2) == alpha_beta_gamma);
}

TEST_CASE("parse_identifier whitespace") {
    Parser p("   x");
    REQUIRE(ParserTestHelper::parse_identifier(p) == "x");

    Parser p2("\tfoo");
    REQUIRE(ParserTestHelper::parse_identifier(p2) == "foo");
}

TEST_CASE("parse_identifier anonymous pattern") {
    Parser p("_");
    REQUIRE(ParserTestHelper::parse_identifier(p) == "_");
}

TEST_CASE("parse_identifier invalid") {
    Parser p("1abc");
    REQUIRE_THROWS_AS(ParserTestHelper::parse_identifier(p), std::runtime_error);

    Parser p2("!");
    REQUIRE_THROWS_AS(ParserTestHelper::parse_identifier(p2), std::runtime_error);

    Parser p3("");
    REQUIRE_THROWS_AS(ParserTestHelper::parse_identifier(p3), std::runtime_error);
}

TEST_CASE("parse_identifier digits and underscores") {
    Parser p("x1");
    REQUIRE(ParserTestHelper::parse_identifier(p) == "x1");

    Parser p2("foo2bar");
    REQUIRE(ParserTestHelper::parse_identifier(p2) == "foo2bar");

    Parser p3("foo_bar");
    REQUIRE(ParserTestHelper::parse_identifier(p3) == "foo"); // stops at '_'
}

TEST_CASE("parse_identifier underscore start") {
    Parser p("_foo");
    REQUIRE(ParserTestHelper::parse_identifier(p) == "_"); // stops at '_'
}