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
    auto expr = parse_expression("sin(0)");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 0.0) < 1e-6);

    expr = parse_expression("cos(0)");
    result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 1.0) < 1e-6);
}

TEST_CASE("Square root and exponential functions are evaluated correctly", "[evaluator]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("sqrt(9)");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 3.0) < 1e-6);

    expr = parse_expression("exp(1)");
    result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - std::exp(1)) < 1e-6);
}

TEST_CASE("Variables are evaluated correctly using context", "[evaluator][context]") {
    EvaluationContext ctx;
    ctx.variables["x"] = 10.0;
    ctx.variables["y"] = 5.0;

    auto expr = parse_expression("x + y");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 15.0);

    expr = parse_expression("x * y");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 50.0);
}

TEST_CASE("Unknown variables throw an exception", "[evaluator][context]") {
    EvaluationContext ctx; // Empty context

    auto expr = parse_expression("z + 1");
    REQUIRE_THROWS(evaluate(expr, ctx));
}