#include "parser/Parser.hpp"
#include "expr/Expr.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mathix;

TEST_CASE("Basic expressions are parsed correctly", "[parser]") {
    auto expr = parse_expression("2 + 3");
    REQUIRE(to_string(expr) == "Plus[2.000000, 3.000000]");
}

TEST_CASE("Parser correctly parses negative numbers in basic expressions", "[parser]") {
    auto expr = parse_expression("-2 + 3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "Plus[Negate[2.000000], 3.000000]");

    expr = parse_expression("2 + -3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "Plus[2.000000, Negate[3.000000]]");

    expr = parse_expression("-2 + -3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "Plus[Negate[2.000000], Negate[3.000000]]");
}

TEST_CASE("Variables are parsed correctly", "[parser]") {
    auto expr = parse_expression("x + 1");
    REQUIRE(to_string(expr) == "Plus[x, 1.000000]");
}

TEST_CASE("Function calls are parsed correctly", "[parser]") {
    auto expr = parse_expression("sin(x)");
    REQUIRE(to_string(expr) == "sin[x]");
}

TEST_CASE("Parser correctly parses negative numbers in function calls", "[parser]") {
    auto expr = parse_expression("sin(-x)");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "sin[Negate[x]]");

    expr = parse_expression("max(-2, min(-3, -4))");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "max[Negate[2.000000], min[Negate[3.000000], Negate[4.000000]]]");
}

TEST_CASE("Nested function calls are parsed correctly", "[parser]") {
    auto expr = parse_expression("max(2, min(3, 4))");
    REQUIRE(to_string(expr) == "max[2.000000, min[3.000000, 4.000000]]");
}

TEST_CASE("Parser correctly parses power expressions", "[parser]") {
    auto expr = parse_expression("2^3");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "Pow[2.000000, 3.000000]");

    expr = parse_expression("-2^3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "Pow[Negate[2.000000], 3.000000]");

    expr = parse_expression("2^-3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "Pow[2.000000, Negate[3.000000]]");
}

TEST_CASE("Parser correctly parses exponential function", "[parser][exp]") {
    auto expr = parse_expression("exp(1)");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "exp[1.000000]");
}

TEST_CASE("Parser correctly parses floor function", "[parser][floor]") {
    auto expr = parse_expression("floor(3.7)");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "floor[3.700000]");
}

TEST_CASE("Parser correctly parses ceil function", "[parser][ceil]") {
    auto expr = parse_expression("ceil(3.2)");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "ceil[3.200000]");
}

TEST_CASE("Parser correctly parses round function", "[parser][round]") {
    auto expr = parse_expression("round(3.5)");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "round[3.500000]");
}