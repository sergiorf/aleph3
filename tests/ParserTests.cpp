#include "parser/Parser.hpp"
#include "expr/Expr.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mathix;

TEST_CASE("Basic expressions are parsed correctly", "[parser]") {
    auto expr = parse_expression("2 + 3");
    REQUIRE(to_string(expr) == "2 + 3");
}

TEST_CASE("Parser correctly parses negative numbers in basic expressions", "[parser]") {
    auto expr = parse_expression("-2 + 3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "-2 + 3");

    expr = parse_expression("2 + -3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "2 + -3");

    expr = parse_expression("-2 + -3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "-2 + -3");
}

TEST_CASE("Variables are parsed correctly", "[parser]") {
    auto expr = parse_expression("x + 1");
    REQUIRE(to_string(expr) == "x + 1");
}

TEST_CASE("Function calls are parsed correctly", "[parser]") {
    auto expr = parse_expression("sin[x]");
    REQUIRE(to_string(expr) == "sin[x]");
}

TEST_CASE("Parser correctly parses negative numbers in function calls", "[parser]") {
    auto expr = parse_expression("sin[-x]");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "sin[-x]");

    expr = parse_expression("max[-2, min[-3, -4]]");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "max[-2, min[-3, -4]]");
}

TEST_CASE("Nested function calls are parsed correctly", "[parser]") {
    auto expr = parse_expression("max[2, min[3, 4]]");
    REQUIRE(to_string(expr) == "max[2, min[3, 4]]");
}

TEST_CASE("Parser correctly parses power expressions", "[parser]") {
    auto expr = parse_expression("2^3");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "2^3");

    expr = parse_expression("-2^3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "-2^3");

    expr = parse_expression("2^-3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "2^-3");
}

TEST_CASE("Parser correctly parses exponential function", "[parser][exp]") {
    auto expr = parse_expression("exp[1]");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "exp[1]");
}

TEST_CASE("Parser correctly parses floor function", "[parser][floor]") {
    auto expr = parse_expression("floor[3.7]");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "floor[3.7]");
}

TEST_CASE("Parser correctly parses ceil function", "[parser][ceil]") {
    auto expr = parse_expression("ceil[3.2]");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "ceil[3.2]");
}

TEST_CASE("Parser correctly parses round function", "[parser][round]") {
    auto expr = parse_expression("round[3.5]");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "round[3.5]");
}