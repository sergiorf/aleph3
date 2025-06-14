#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

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

    REQUIRE(to_string(FunctionCall{ "Power", {
        make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{x, y}), make_expr<Number>(2)
    } }) == "(x + y)^2");

    REQUIRE(to_string(FunctionCall{ "Power", {
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
        "f",
        { Parameter("x"), Parameter("y") },
        make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{x, y}),
        true // Delayed assignment
        }) == "f[x_, y_] := x + y");

    // Test immediate assignment (=)
    REQUIRE(to_string(FunctionDefinition{
        "f",
        { Parameter("x"), Parameter("y") },
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

TEST_CASE("Pretty printing of immediate and delayed function definitions", "[prettyprint]") {
    auto delayed_func = make_fdef(
        "f", { "a" },
        make_fcall("Minus", {
            make_fcall("Power", {make_expr<Symbol>("a"), make_expr<Number>(3)}),
            make_expr<Symbol>("x")
            }),
        true // Delayed assignment
    );
    REQUIRE(to_string(*delayed_func) == "f[a_] := a^3 - x");

    auto immediate_func = make_fdef(
        "f", { "a" },
        make_fcall("Minus", {
            make_fcall("Power", {make_expr<Symbol>("a"), make_expr<Number>(3)}),
            make_expr<Symbol>("x")
            }),
        false // Immediate assignment
    );
    REQUIRE(to_string(*immediate_func) == "f[a_] = a^3 - x");
}

TEST_CASE("to_string handles Boolean type", "[prettyprint]") {
    ExprPtr true_expr = make_expr<Boolean>(true);
    ExprPtr false_expr = make_expr<Boolean>(false);

    REQUIRE(to_string(true_expr) == "True");
    REQUIRE(to_string(false_expr) == "False");
}

TEST_CASE("Pretty printing of symbolic comparisons", "[prettyprint][comparison]") {
    auto x = make_expr<Symbol>("x");
    auto y = make_expr<Symbol>("y");

    REQUIRE(to_string(FunctionCall{ "Equal", {x, y} }) == "x == y");
    REQUIRE(to_string(FunctionCall{ "NotEqual", {x, y} }) == "x != y");
    REQUIRE(to_string(FunctionCall{ "Less", {x, y} }) == "x < y");
    REQUIRE(to_string(FunctionCall{ "Greater", {x, y} }) == "x > y");
    REQUIRE(to_string(FunctionCall{ "LessEqual", {x, y} }) == "x <= y");
    REQUIRE(to_string(FunctionCall{ "GreaterEqual", {x, y} }) == "x >= y");
}

TEST_CASE("Pretty printing of complex numbers", "[prettyprint][complex]") {
    SECTION("Pure imaginary") {
        auto c = make_expr<Complex>(0.0, 1.0);
        REQUIRE(to_string(*c) == "1*I");
    }
    SECTION("Negative imaginary") {
        auto c = make_expr<Complex>(0.0, -2.0);
        REQUIRE(to_string(*c) == "-2*I");
    }
    SECTION("Pure real") {
        auto c = make_expr<Complex>(3.0, 0.0);
        REQUIRE(to_string(*c) == "3");
    }
    SECTION("a + b*I") {
        auto c = make_expr<Complex>(2.0, 5.0);
        REQUIRE(to_string(*c) == "2 + 5*I");
    }
    SECTION("a - b*I") {
        auto c = make_expr<Complex>(7.0, -4.0);
        REQUIRE(to_string(*c) == "7 - 4*I");
    }
    SECTION("Zero") {
        auto c = make_expr<Complex>(0.0, 0.0);
        REQUIRE(to_string(*c) == "0");
    }
}