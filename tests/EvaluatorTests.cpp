#include "parser/Parser.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <stdexcept>

using namespace mathix;

TEST_CASE("Basic operations are evaluated correctly", "[evaluator]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("2 + 3");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 5.0);
}

TEST_CASE("Multiplication and parentheses are evaluated correctly", "[evaluator]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("2 * (3 + 4)");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 14.0);
}

TEST_CASE("Trigonometric functions are evaluated correctly", "[evaluator]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("sin[0]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 0.0) < 1e-6);

    expr = parse_expression("cos[0]");
    result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 1.0) < 1e-6);
}

TEST_CASE("Square root and exponential functions are evaluated correctly", "[evaluator]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("sqrt[9]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 3.0) < 1e-6);

    expr = parse_expression("exp[1]");
    result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - std::exp(1)) < 1e-6);
}

TEST_CASE("Variables are evaluated correctly using context", "[evaluator][context]") {
    EvaluationContext ctx;
    ctx.variables["x"] = make_expr<Number>(10.0);
    ctx.variables["y"] = make_expr<Number>(5.0);

    auto expr = parse_expression("x + y");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 15.0);

    expr = parse_expression("x * y");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 50.0);
}

TEST_CASE("Unknown variables are treated as symbolic", "[evaluator][context]") {
    EvaluationContext ctx; // Empty context

    auto expr = parse_expression("z + 1");
    auto result = evaluate(expr, ctx);

    // Ensure the result is a symbolic expression: Plus[z, 1]
    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    auto func = std::get<FunctionCall>(*result);
    REQUIRE(func.head == "Plus");
    REQUIRE(func.args.size() == 2);
    REQUIRE(std::holds_alternative<Symbol>(*func.args[0]));
    REQUIRE(std::get<Symbol>(*func.args[0]).name == "z");
    REQUIRE(std::holds_alternative<Number>(*func.args[1]));
    REQUIRE(std::get<Number>(*func.args[1]).value == 1.0);
}

TEST_CASE("Evaluator correctly evaluates power expressions", "[evaluator][pow]") {
    auto expr = parse_expression("2^3");
    EvaluationContext ctx; // Empty context if required by evaluate
    auto result = evaluate(expr, ctx);

    REQUIRE(result != nullptr);

    auto num = std::get<Number>(*result);
    REQUIRE(std::abs(num.value - 8.0) < 1e-9); // Floating-point comparison
}

TEST_CASE("Exponential function is evaluated correctly", "[evaluator][exp]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("exp[1]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - std::exp(1)) < 1e-6);

    expr = parse_expression("exp[0]");
    result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 1.0) < 1e-6);
}

TEST_CASE("Floor function is evaluated correctly", "[evaluator][floor]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("floor[3.7]");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 3.0);

    expr = parse_expression("floor[-3.7]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == -4.0);
}

TEST_CASE("Ceil function is evaluated correctly", "[evaluator][ceil]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("ceil[3.2]");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 4.0);

    expr = parse_expression("ceil[-3.2]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == -3.0);
}

TEST_CASE("Round function is evaluated correctly", "[evaluator][round]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("round[3.5]");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 4.0);

    expr = parse_expression("round[3.4]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 3.0);

    expr = parse_expression("round[-3.5]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == -4.0);

    expr = parse_expression("round[-3.4]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == -3.0);
}

TEST_CASE("User-defined functions are parsed and evaluated correctly", "[evaluator][functions]") {
    EvaluationContext ctx;

    SECTION("Simple increment function") {
        // Define function f[x_] := x + 1
        auto def_expr = parse_expression("f[x_] := x + 1");
        evaluate(def_expr, ctx); // Should register the function

        // Call f[3]
        auto call_expr = parse_expression("f[3]");
        auto result = evaluate(call_expr, ctx);
        REQUIRE(get_number_value(result) == 4.0);
    }

    SECTION("Function using multiple variables") {
        // Define function add[a_, b_] := a + b
        auto def_expr = parse_expression("add[a_, b_] := a + b");
        evaluate(def_expr, ctx);

        auto call_expr = parse_expression("add[2, 5]");
        auto result = evaluate(call_expr, ctx);
        REQUIRE(get_number_value(result) == 7.0);
    }

    SECTION("Nested function call") {
        // Define double[x_] := x * 2
        auto def_expr = parse_expression("double[x_] := x * 2");
        evaluate(def_expr, ctx);

        // Call double(f[3]) where f[x_] := x + 1
        evaluate(parse_expression("f[x_] := x + 1"), ctx);
        auto call_expr = parse_expression("double[f[3]]");
        auto result = evaluate(call_expr, ctx);
        REQUIRE(get_number_value(result) == 8.0); // f(3) = 4, double(4) = 8 
    }
}

TEST_CASE("Simplify addition with zero", "[evaluator][simplification]") {
    EvaluationContext ctx; // Empty context

    auto expr = parse_expression("0 + x");
    auto result = evaluate(expr, ctx);

    // Ensure the result is a symbolic expression: x
    REQUIRE(std::holds_alternative<Symbol>(*result));
    REQUIRE(std::get<Symbol>(*result).name == "x");

    expr = parse_expression("x + 0");
    result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Symbol>(*result));
    REQUIRE(std::get<Symbol>(*result).name == "x");
}

TEST_CASE("Simplify multiplication with one", "[evaluator][simplification]") {
    EvaluationContext ctx; // Empty context

    auto expr = parse_expression("1 * x");
    auto result = evaluate(expr, ctx);

    // Ensure the result is a symbolic expression: x
    REQUIRE(std::holds_alternative<Symbol>(*result));
    REQUIRE(std::get<Symbol>(*result).name == "x");

    expr = parse_expression("x * 1");
    result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Symbol>(*result));
    REQUIRE(std::get<Symbol>(*result).name == "x");
}

TEST_CASE("Simplify multiplication with zero", "[evaluator][simplification]") {
    EvaluationContext ctx; // Empty context

    auto expr = parse_expression("0 * x");
    auto result = evaluate(expr, ctx);

    // Ensure the result is a numeric value: 0
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(get_number_value(result) == 0.0);

    expr = parse_expression("x * 0");
    result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(get_number_value(result) == 0.0);
}

TEST_CASE("Simplify exponentiation", "[evaluator][simplification]") {
    EvaluationContext ctx; // Empty context

    auto expr = parse_expression("x^0");
    auto result = evaluate(expr, ctx);

    // Ensure the result is a numeric value: 1
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(get_number_value(result) == 1.0);

    expr = parse_expression("x^1");
    result = evaluate(expr, ctx);

    // Ensure the result is a symbolic expression: x
    REQUIRE(std::holds_alternative<Symbol>(*result));
    REQUIRE(std::get<Symbol>(*result).name == "x");
}

TEST_CASE("Simplify nested expressions", "[evaluator][simplification]") {
    EvaluationContext ctx; // Empty context

    auto expr = parse_expression("0 + (1 * x)");
    auto result = evaluate(expr, ctx);

    // Ensure the result is a symbolic expression: x
    REQUIRE(std::holds_alternative<Symbol>(*result));
    REQUIRE(std::get<Symbol>(*result).name == "x");

    expr = parse_expression("(x * 0) + 1");
    result = evaluate(expr, ctx);

    // Ensure the result is a numeric value: 1
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(get_number_value(result) == 1.0);
}

