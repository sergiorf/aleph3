#include "parser/Parser.hpp"
#include "expr/Expr.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mathix;

TEST_CASE("Basic expressions are parsed correctly", "[parser]") {
    auto expr = parse_expression("2 + 3");
    REQUIRE(to_string(expr) == "Plus[2.000000, 3.000000]");
}

TEST_CASE("Variables are parsed correctly", "[parser]") {
    auto expr = parse_expression("x + 1");
    REQUIRE(to_string(expr) == "Plus[x, 1.000000]");
}

TEST_CASE("Function calls are parsed correctly", "[parser]") {
    auto expr = parse_expression("sin(x)");
    REQUIRE(to_string(expr) == "sin[x]");
}

TEST_CASE("Nested function calls are parsed correctly", "[parser]") {
    auto expr = parse_expression("max(2, min(3, 4))");
    REQUIRE(to_string(expr) == "max[2.000000, min[3.000000, 4.000000]]");
}

TEST_CASE("Parser correctly parses power expressions", "[parser]") {
    auto expr = parse_expression("2^3");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "Pow[2.000000, 3.000000]");
}
