#include "expr/Expr.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mathix;

// Test for simple arithmetic pretty printing
TEST_CASE("Pretty printing of simple arithmetic expressions", "[prettyprint]") {
    auto x = make_expr<Symbol>("x");
    auto y = make_expr<Symbol>("y");
    auto z = make_expr<Symbol>("z");

    REQUIRE(to_string(FunctionCall{ "Plus", {x, y} }) == "x + y");
    REQUIRE(to_string(FunctionCall{ "Times", {x, y, z} }) == "x * y * z");
}

// Test for infix operators with precedence and parentheses
TEST_CASE("Pretty printing of infix operators with precedence", "[prettyprint]") {
    auto x = make_expr<Symbol>("x");
    auto y = make_expr<Symbol>("y");

    REQUIRE(to_string(FunctionCall{ "Pow", {
        make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{x, y}), make_expr<Number>(2)
    } }) == "(x + y)^2");

    REQUIRE(to_string(FunctionCall{ "Pow", {
        x, make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{y, make_expr<Number>(2)})
    } }) == "x^(y + 2)");
}

// Test for negation and function definitions with both delayed and immediate assignment
TEST_CASE("Pretty printing of negation and functions with delayed and immediate assignment", "[prettyprint]") {
    auto x = make_expr<Symbol>("x");
    auto y = make_expr<Symbol>("y");

    // Test negation
    REQUIRE(to_string(FunctionCall{ "Negate", {make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{x, y})} }) == "-(x + y)");

    // Test delayed assignment (:=)
    REQUIRE(to_string(FunctionDefinition{
        "f", {"x", "y"},
        make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{x, y}),
        true // Delayed assignment
        }) == "f[x_, y_] := x + y");

    // Test immediate assignment (=)
    REQUIRE(to_string(FunctionDefinition{
        "f", {"x", "y"},
        make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{x, y}),
        false // Immediate assignment
        }) == "f[x_, y_] = x + y");

    // Test function call
    REQUIRE(to_string(FunctionCall{ "f", {make_expr<Number>(1), make_expr<Number>(2)} }) == "f[1, 2]");
}

TEST_CASE("Pretty printing of assignments", "[prettyprint]") {
    auto assign = make_expr<Assignment>("x", make_expr<Number>(2));
    REQUIRE(to_string(*assign) == "x = 2");
}