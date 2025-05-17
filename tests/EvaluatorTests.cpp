#include "parser/Parser.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>
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

TEST_CASE("Variables are evaluated correctly using context", "[evaluator]") {
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

TEST_CASE("Simplify Plus[2, 3, 4] to 9", "[evaluator][simplification]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("2 + 3 + 4");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(get_number_value(result) == 9.0);
}

TEST_CASE("Simplify Times[2, 3, 0] to 0", "[evaluator][simplification]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("2 * 3 * 0");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(get_number_value(result) == 0.0);
}

TEST_CASE("Evaluator handles variable assignments", "[evaluator]") {
    EvaluationContext ctx;

    // Assign a value to x
    auto assign = make_expr<Assignment>("x", make_expr<Number>(2));
    auto result = evaluate(assign, ctx);

    // Check that the feedback is the variable name
    REQUIRE(std::get<Symbol>(*result).name == "x");

    // Check that x is stored in the context
    REQUIRE(ctx.variables.find("x") != ctx.variables.end());
    REQUIRE(std::get<Number>(*ctx.variables["x"]).value == 2);

    // Evaluate x
    auto x = make_expr<Symbol>("x");
    result = evaluate(x, ctx);
    REQUIRE(std::get<Number>(*result).value == 2);
}

TEST_CASE("Evaluator handles immediate function definitions with x assigned first", "[evaluator]") {
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
    REQUIRE(stored_func_def.params[0] == "a");
}

TEST_CASE("Evaluator handles delayed function definitions", "[evaluator]") {
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
    REQUIRE(stored_func_def.params[0] == "a");
    REQUIRE(stored_func_def.delayed == true);
}

TEST_CASE("Evaluator handles division by zero as DirectedInfinity", "[evaluator][infinity]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("1 / 0");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    auto func = std::get<FunctionCall>(*result);
    REQUIRE(func.head == "DirectedInfinity");
    REQUIRE(func.args.size() == 1);
    REQUIRE(std::get<Number>(*func.args[0]).value == 1.0); // Positive infinity
}

TEST_CASE("Evaluator handles negative infinity", "[evaluator][infinity]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("-1 / 0");
    auto result = evaluate(expr, ctx);

    // Ensure the result is a symbolic representation of negative infinity
    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    auto func = std::get<FunctionCall>(*result);
    REQUIRE(func.head == "DirectedInfinity");
    REQUIRE(func.args.size() == 1);
    REQUIRE(std::get<Number>(*func.args[0]).value == -1.0); // Negative infinity
}

TEST_CASE("Evaluator handles 0/0 as Indeterminate", "[evaluator][indeterminate]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("0 / 0");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    auto func = std::get<FunctionCall>(*result);
    REQUIRE(func.head == "Indeterminate");
    REQUIRE(func.args.empty()); // Indeterminate should have no arguments
}

TEST_CASE("Unknown variables are treated as symbolic", "[evaluator]") {
    EvaluationContext ctx; // Empty context

    auto expr = parse_expression("z + 1");
    auto result = evaluate(expr, ctx);

    // Ensure the result is a symbolic expression: Plus[1, z]
    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    auto func = std::get<FunctionCall>(*result);
    REQUIRE(func.head == "Plus");
    REQUIRE(func.args.size() == 2);
    REQUIRE(std::holds_alternative<Number>(*func.args[0])); // Numeric argument comes first
    REQUIRE(std::get<Number>(*func.args[0]).value == 1.0);
    REQUIRE(std::holds_alternative<Symbol>(*func.args[1])); // Symbolic argument comes second
    REQUIRE(std::get<Symbol>(*func.args[1]).name == "z");
}

TEST_CASE("Evaluator handles nested parentheses correctly", "[evaluator]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("2 * (3 + (4 * (5 - 1)))");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 38.0);
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

TEST_CASE("Evaluator handles equality operator (==) with True result", "[evaluator]") {
    EvaluationContext ctx;
    ctx.variables["x"] = make_expr<Number>(5);

    auto expr = parse_expression("x == 5");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE(std::get<Boolean>(*result).value == true);
}

TEST_CASE("Evaluator handles equality operator (==) with False result", "[evaluator]") {
    EvaluationContext ctx;
    ctx.variables["x"] = make_expr<Number>(3);

    auto expr = parse_expression("x == 5");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE(std::get<Boolean>(*result).value == false);
}

TEST_CASE("Evaluator handles symbolic equality operator (==)", "[evaluator]") {
    EvaluationContext ctx; // Empty context, no variables defined

    auto expr = parse_expression("x == y");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    auto func = std::get<FunctionCall>(*result);

    REQUIRE(func.head == "Equal");
    REQUIRE(func.args.size() == 2);

    REQUIRE(std::holds_alternative<Symbol>(*func.args[0]));
    REQUIRE(std::get<Symbol>(*func.args[0]).name == "x");

    REQUIRE(std::holds_alternative<Symbol>(*func.args[1]));
    REQUIRE(std::get<Symbol>(*func.args[1]).name == "y");
}

TEST_CASE("Evaluator handles logical AND (&&)", "[evaluator][logic]") {
    EvaluationContext ctx;

    // True && False -> False
    auto expr = parse_expression("True && False");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE(std::get<Boolean>(*result).value == false);

    // True && True -> True
    expr = parse_expression("True && True");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE(std::get<Boolean>(*result).value == true);

    // True && x -> Unevaluated
    expr = parse_expression("True && x");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    auto func = std::get<FunctionCall>(*result);
    REQUIRE(func.head == "And");
    REQUIRE(func.args.size() == 2);
    REQUIRE(std::holds_alternative<Boolean>(*func.args[0]));
    REQUIRE(std::get<Boolean>(*func.args[0]).value == true);
    REQUIRE(std::holds_alternative<Symbol>(*func.args[1]));
    REQUIRE(std::get<Symbol>(*func.args[1]).name == "x");
}

TEST_CASE("Evaluator handles logical OR (||)", "[evaluator][logic]") {
    EvaluationContext ctx;

    // True || False -> True
    auto expr = parse_expression("True || False");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE(std::get<Boolean>(*result).value == true);

    // False || False -> False
    expr = parse_expression("False || False");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE(std::get<Boolean>(*result).value == false);

    // False || x -> Unevaluated
    expr = parse_expression("False || x");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    auto func = std::get<FunctionCall>(*result);
    REQUIRE(func.head == "Or");
    REQUIRE(func.args.size() == 2);
    REQUIRE(std::holds_alternative<Boolean>(*func.args[0]));
    REQUIRE(std::get<Boolean>(*func.args[0]).value == false);
    REQUIRE(std::holds_alternative<Symbol>(*func.args[1]));
    REQUIRE(std::get<Symbol>(*func.args[1]).name == "x");
}

TEST_CASE("Evaluator handles StringJoin", "[evaluator][string]") {
    EvaluationContext ctx;

    // Test "Hello" <> " " <> "World"
    auto expr = parse_expression("\"Hello\" <> \" \" <> \"World\"");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "Hello World");

    // Test "Mathix" <> " Rocks"
    expr = parse_expression("\"Mathix\" <> \" Rocks\"");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "Mathix Rocks");

    // Test empty string concatenation
    expr = parse_expression("\"\" <> \"Hello\"");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "Hello");
}

TEST_CASE("Evaluator handles StringLength", "[evaluator][string]") {
    EvaluationContext ctx;

    // Test StringLength["Hello"]
    auto expr = parse_expression("StringLength[\"Hello\"]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 5);

    // Test StringLength[""]
    expr = parse_expression("StringLength[\"\"]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 0);

    // Test StringLength["Mathix Rocks"]
    expr = parse_expression("StringLength[\"Mathix Rocks\"]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 12);
}

TEST_CASE("Evaluator handles StringReplace", "[evaluator][string]") {
    EvaluationContext ctx;

    // Test StringReplace["Hello World", "World" -> "Mathix"]
    auto expr = parse_expression("StringReplace[\"Hello World\", \"World\" -> \"Mathix\"]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "Hello Mathix");

    // Test StringReplace["abcabc", "abc" -> "x"]
    expr = parse_expression("StringReplace[\"abcabc\", \"abc\" -> \"x\"]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "xx");

    // Test StringReplace["Hello", "x" -> "y"] (no match)
    expr = parse_expression("StringReplace[\"Hello\", \"x\" -> \"y\"]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "Hello");
}

TEST_CASE("Evaluator handles StringTake", "[evaluator][string]") {
    EvaluationContext ctx;

    // Test StringTake["Hello", 3]
    auto expr = parse_expression("StringTake[\"Hello\", 3]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "Hel");

    // Test StringTake["Hello", -2]
    expr = parse_expression("StringTake[\"Hello\", -2]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "lo");

    // Test StringTake["Hello", {2, 4}]
    expr = parse_expression("StringTake[\"Hello\", {2, 4}]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "ell");

    // Test StringTake["Hello", 0] (invalid index)
    expr = parse_expression("StringTake[\"Hello\", 0]");
    REQUIRE_THROWS_WITH(evaluate(expr, ctx), "StringTake expects a valid index or range");
}

TEST_CASE("Evaluator throws error for invalid StringJoin arguments", "[evaluator][string]") {
    EvaluationContext ctx;

    // Test invalid argument: "Hello" <> 123
    auto expr = parse_expression("\"Hello\" <> 123");
    REQUIRE_THROWS_WITH(evaluate(expr, ctx), "StringJoin expects string arguments");
}