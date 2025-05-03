#include "parser/Parser.hpp"
#include "transforms/Transforms.hpp"
#include "expr/Expr.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mathix;

TEST_CASE("Expand distributes multiplication over addition - left side") {
    auto expr = parse_expression("(a + b) * c");
    auto expanded = expand(expr);
    REQUIRE(to_string(expanded) == "a * c + b * c");
}

TEST_CASE("Expand distributes multiplication over addition - right side") {
    auto expr = parse_expression("a * (b + c)");
    auto expanded = expand(expr);
    REQUIRE(to_string(expanded) == "a * b + a * c");
}

TEST_CASE("Expand handles nested distribution") {
    auto expr = parse_expression("(a + b) * (c + d)");
    auto expanded = expand(expr);
    REQUIRE(to_string(expanded) == "a * c + a * d + b * c + b * d");
}

TEST_CASE("Expand handles binomial square (a + b)^2") {
    auto expr = parse_expression("(a + b)^2");
    auto expanded = expand(expr);
    REQUIRE(to_string(expanded) == "a^2 + b^2 + 2 * a * b");
}

TEST_CASE("Expand handles binomial square (x + 1)^2") {
    auto expr = parse_expression("(x + 1)^2");
    auto expanded = expand(expr);
    REQUIRE(to_string(expanded) == "x^2 + 2 * x + 1");
}

TEST_CASE("Expand handles binomial square with coefficients") {
    auto expr = parse_expression("(2x + 3)^2");
    auto expanded = expand(expr);
    REQUIRE(to_string(expanded) == "4 * x^2 + 12 * x + 9");
}

TEST_CASE("Expand does not change flat multiplication") {
    auto expr = parse_expression("a * b");
    auto expanded = expand(expr);
    REQUIRE(to_string(expanded) == "a * b");
}

TEST_CASE("Expand does not handle (a + b)^3 (yet)") {
    auto expr = parse_expression("(a + b)^3");
    auto expanded = expand(expr);
    REQUIRE(to_string(expanded) == "(a + b)^3");
}
