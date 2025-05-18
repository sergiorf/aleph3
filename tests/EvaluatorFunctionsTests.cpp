#include "parser/Parser.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>
#include <cmath>
#include <stdexcept>

using namespace mathix;

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

TEST_CASE("Evaluator handles function overwriting", "[evaluator][functions]") {
    EvaluationContext ctx;

    // Define a function f[x_] := x + 1
    auto def_expr = parse_expression("f[x_] := x + 1");
    evaluate(def_expr, ctx);

    // Overwrite the function f[x_] := x * 2
    def_expr = parse_expression("f[x_] := x * 2");
    evaluate(def_expr, ctx);

    // Call the overwritten function
    auto call_expr = parse_expression("f[3]");
    auto result = evaluate(call_expr, ctx);
    REQUIRE(get_number_value(result) == 6.0);
}

TEST_CASE("Evaluator handles recursive function calls", "[evaluator][functions]") {
    EvaluationContext ctx;

    // Define a recursive function factorial[n_] := If[n == 0, 1, n * factorial[n - 1]]
    auto def_expr = parse_expression("factorial[n_] := If[n == 0, 1, n * factorial[n - 1]]");
    evaluate(def_expr, ctx);

    // Call the recursive function
    auto call_expr = parse_expression("factorial[5]");
    auto result = evaluate(call_expr, ctx);
    REQUIRE(get_number_value(result) == 120.0); // 5! = 120
}

TEST_CASE("Evaluator handles simple function call", "[evaluator][functions]") {
    EvaluationContext ctx;
    evaluate(parse_expression("f[x_] := x^2"), ctx);

    auto expr = parse_expression("f[4]");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 16.0);
}

TEST_CASE("Evaluator handles function with multiple arguments", "[evaluator][functions]") {
    EvaluationContext ctx;
    evaluate(parse_expression("g[x_, y_] := x + y"), ctx);

    auto expr = parse_expression("g[3, 5]");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 8.0);
}

TEST_CASE("Evaluator handles function with default argument (missing)", "[evaluator][functions]") {
    EvaluationContext ctx;
    evaluate(parse_expression("h[x_, y_:2] := x + y"), ctx);

    auto expr = parse_expression("h[7]");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 9.0);
}

TEST_CASE("Evaluator handles function with default argument (provided)", "[evaluator][functions]") {
    EvaluationContext ctx;
    evaluate(parse_expression("h[x_, y_:2] := x + y"), ctx);

    auto expr = parse_expression("h[7, 10]");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 17.0);
}

TEST_CASE("Evaluator handles immediate function definitions with x assigned first", "[evaluator][functgions]") {
    EvaluationContext ctx;

    // Assign a value to x before defining the function
    ctx.variables["x"] = make_expr<Number>(2);

    // Define a function with immediate assignment
    auto func_def = make_fdef(
        "f", { "a" },
        make_fcall("Minus", {
            make_fcall("Pow", {make_expr<Symbol>("a"), make_expr<Number>(3)}),
            make_expr<Symbol>("x")
            }),
        false // Immediate assignment
    );
    evaluate(func_def, ctx);

    // Call the function with a specific value for 'a'
    auto result = evaluate(make_fcall("f", { make_expr<Number>(3) }), ctx);
    REQUIRE(std::get<Number>(*result).value == 25.0); // 3^3 - 2 = 25

    // Change the value of x
    ctx.variables["x"] = make_expr<Number>(5);

    // Call the function again with the same value for 'a'
    auto result_after_x_change = evaluate(make_fcall("f", { make_expr<Number>(3) }), ctx);
    REQUIRE(std::get<Number>(*result_after_x_change).value == 25.0); // Should still be 25, as f was defined with the old x

    // Validate that the function definition in the context has not changed
    const auto& stored_func_def = ctx.user_functions["f"];
    REQUIRE(stored_func_def.name == "f");
    REQUIRE(stored_func_def.params.size() == 1);
    REQUIRE(stored_func_def.params[0].name == "a");
}

TEST_CASE("Evaluator handles delayed function definitions", "[evaluator][functions]") {
    EvaluationContext ctx;

    // Define a function with delayed assignment
    auto func_def = make_fdef(
        "f", { "a" },
        make_fcall("Minus", {
            make_fcall("Pow", {make_expr<Symbol>("a"), make_expr<Number>(3)}),
            make_expr<Symbol>("x")
            }),
        true // Delayed assignment
    );
    evaluate(func_def, ctx);

    // Assign a value to x
    ctx.variables["x"] = make_expr<Number>(2);

    // Call the function with a specific value for 'a'
    auto result = evaluate(make_fcall("f", { make_expr<Number>(3) }), ctx);
    REQUIRE(std::get<Number>(*result).value == 25.0); // 3^3 - 2 = 25

    // Change the value of x
    ctx.variables["x"] = make_expr<Number>(5);

    // Call the function again with the same value for 'a'
    auto result_after_x_change = evaluate(make_fcall("f", { make_expr<Number>(3) }), ctx);
    REQUIRE(std::get<Number>(*result_after_x_change).value == 22.0); // 3^3 - 5 = 22

    // Validate that the function definition in the context remains delayed
    const auto& stored_func_def = ctx.user_functions["f"];
    REQUIRE(stored_func_def.name == "f");
    REQUIRE(stored_func_def.params.size() == 1);
    REQUIRE(stored_func_def.params[0].name == "a");
    REQUIRE(stored_func_def.delayed == true);
}