#include "parser/Parser.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "Constants.hpp"
#include "evaluator/EvaluationContext.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>
#include <cmath>
#include <stdexcept>

using namespace aleph3;

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
        auto def_expr = parse_expression("Add[a_, b_] := a + b");
        evaluate(def_expr, ctx);

        auto call_expr = parse_expression("Add[2, 5]");
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
            make_fcall("Power", {make_expr<Symbol>("a"), make_expr<Number>(3)}),
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
            make_fcall("Power", {make_expr<Symbol>("a"), make_expr<Number>(3)}),
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

void check_builtin_eval(const std::string& expr_str, double expected, double tol = 1e-12) {
    EvaluationContext ctx;
    try {
        auto expr = parse_expression(expr_str);
        auto result = evaluate(expr, ctx);

        // Check that result is a Number
        if (!std::holds_alternative<Number>(*result)) {
            CAPTURE(expr_str);
            FAIL("Result is not a Number, got: " << result->index());
        }

        double actual = get_number_value(result);
        CAPTURE(expr_str, expected, actual);
        REQUIRE(std::abs(actual - expected) < tol);
    }
    catch (const std::exception& ex) {
        CAPTURE(expr_str, expected);
        FAIL("Exception thrown: " << ex.what());
    }
    catch (...) {
        CAPTURE(expr_str, expected);
        FAIL("Unknown exception thrown.");
    }
}

// Helper to check symbolic result (expression remains unevaluated)
void check_symbolic_eval(const std::string& expr_str, const std::string& expected_head) {
    EvaluationContext ctx;
    auto expr = parse_expression(expr_str);
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    REQUIRE(std::get<FunctionCall>(*result).head == expected_head);
}

TEST_CASE("Evaluator: numeric and symbolic evaluation", "[evaluator][functions][builtins]") {
    struct NumericCase {
        std::string expr;
        double expected;
        const char* explanation;
    };
    struct SymbolicCase {
        std::string expr;
        std::string expected_head;
        const char* explanation;
    };

    // Numeric cases: argument is numeric or a known constant, so result is numeric
    std::vector<NumericCase> numeric_cases = {
        // Abs
        {"Abs[0]", 0.0, "Abs[0] is 0."},
        {"Abs[3.5]", 3.5, "Abs[3.5] is 3.5."},
        {"Abs[-1]", 1.0, "Abs[-1] is 1."},
        {"Abs[-3.5]", 3.5, "Abs[-3.5] is 3.5."},
        {"Abs[-5]", 5.0, "Abs[-5] is 5.0."},

        // ArcCos
        {"ArcCos[0]", PI / 2, "ArcCos[0] is Pi/2."},
        {"ArcCos[1]", 0.0, "ArcCos[1] is 0.0."},

        // ArcSin
        {"ArcSin[0]", 0.0, "ArcSin[0] is 0.0."},
        {"ArcSin[1]", PI / 2, "ArcSin[1] is Pi/2."},

        // ArcTan
        {"ArcTan[0]", 0.0, "ArcTan[0] is 0.0."},
        {"ArcTan[1]", PI / 4, "ArcTan[1] is Pi/4."},
        {"ArcTan[-1]", -PI / 4, "ArcTan[-1] is -Pi/4."},
        {"ArcTan[1, 1]", PI / 4, "ArcTan[1,1] is Pi/4."},

        // Ceiling
        {"Ceiling[2.1]", 3.0, "Ceiling[2.1] is 3.0."},

        // Cos
        {"Cos[0]", 1.0, "Cos[0] is 1.0."},
        {"Cos[Pi]", -1.0, "Cos[Pi] is -1.0."},
        {"Cos[2*Pi]", 1.0, "Cos[2*Pi] is 1.0."},
        {"Cos[Pi/2]", 0.0, "Cos[Pi/2] is 0.0."},
        {"Cos[Pi/4]", std::sqrt(2.0) / 2.0, "Cos[Pi/4] is sqrt(2)/2."},
        {"Cos[-Pi]", -1.0, "Cos[-Pi] is -1.0."},

        // Cosh
        {"Cosh[0]", 1.0, "Cosh[0] is 1.0."},
        {"Cosh[1]", std::cosh(1.0), "Cosh[1] is cosh(1)."},

        // Cot
        {"Cot[1]", 1.0 / std::tan(1.0), "Cot[1] is numeric."},
        {"Cot[Pi/4]", 1.0, "Cot[Pi/4] is 1.0."},
        {"Cot[-Pi/4]", -1.0, "Cot[-Pi/4] is -1.0."},

        // Csc
        {"Csc[Pi/2]", 1.0, "Csc[Pi/2] is 1.0."},

        // Degree
        {"Degree", PI / 180.0, "Degree is Pi/180."},

        // Exp
        {"Exp[0]", 1.0, "Exp[0] is 1.0."},
        {"Exp[1]", E, "Exp[1] is E."},

        // Floor
        {"Floor[2.7]", 2.0, "Floor[2.7] is 2.0."},

        // Gamma
        {"Gamma[6]", 120.0, "Gamma[6] = 5! = 120."},

        // Log
        {"Log[1]", 0.0, "Log[1] is 0.0."},
        {"Log[E]", 1.0, "Log[E] is 1.0."},
        {"Log[10, 100]", 2.0, "Log base 10 of 100 is 2."},
        {"Log[10, 1000]", 3.0, "Log base 10 of 1000 is 3."},

        // N (numeric evaluation)
        {"N[Pi]", PI, "N[Pi] evaluates Pi to its numeric value."},
        {"N[E]", E, "N[E] evaluates E to its numeric value."},
        {"N[Degree]", PI / 180.0, "N[Degree] evaluates Degree to its numeric value."},
        {"N[Sin[Pi/2]]", 1.0, "Sin[Pi/2] is a known value, N returns 1.0."},
        {"N[Cos[0]]", 1.0, "Cos[0] is a known value, N returns 1.0."},
        {"N[Csc[Pi/2]]", 1.0, "Csc[Pi/2] is a known value, N returns 1.0."},
        {"N[Sec[0]]", 1.0, "Sec[0] is a known value, N returns 1.0."},
        {"N[Cot[Pi/4]]", 1.0, "Cot[Pi/4] is a known value, N returns 1.0."},
        {"N[Exp[1]]", E, "Exp[1] is a known value, N returns E."},
        {"N[Log[E]]", 1.0, "Log[E] is a known value, N returns 1.0."},
        {"N[Sin[1]]", std::sin(1.0), "Sin[1] is not a known symbolic value, N returns numeric result."},
        {"N[Cot[1]]", 1.0 / std::tan(1.0), "Cot[1] is not a known symbolic value, N returns numeric result."},
        {"N[ArcSin[0.5]]", std::asin(0.5), "ArcSin[0.5] is numeric, N returns numeric result."},
        {"N[ArcCos[0.5]]", std::acos(0.5), "ArcCos[0.5] is numeric, N returns numeric result."},
        {"N[ArcTan[1, 1]]", std::atan2(1.0, 1.0), "ArcTan[1,1] is numeric, N returns numeric result."},

        // Pi
        {"Pi", PI, "Pi is Pi."},

        // Power
        {"Power[2, 3]", 8.0, "2^3 is 8."},
        {"Power[2, 5]", 32.0, "2^5 is 32."},
        {"Power[4, 0.5]", 2.0, "4^0.5 is 2."},
        {"Power[9, 0.5]", 3.0, "9^0.5 is 3."},
        {"Power[27, 1.0/3.0]", 3.0, "27^(1/3) is 3."},

        // Round
        {"Round[2.6]", 3.0, "Round[2.6] is 3.0."},

        // Sec
        {"Sec[0]", 1.0, "Sec[0] is 1.0."},

        // Sin
        {"Sin[0]", 0.0, "Sin[0] is 0.0."},
        {"Sin[Pi]", 0.0, "Sin[Pi] is 0.0."},
        {"Sin[2*Pi]", 0.0, "Sin[2*Pi] is 0.0."},
        {"Sin[Pi/2]", 1.0, "Sin[Pi/2] is 1.0."},
        {"Sin[Pi/4]", std::sqrt(2.0) / 2.0, "Sin[Pi/4] is sqrt(2)/2."},
        {"Sin[-Pi/2]", -1.0, "Sin[-Pi/2] is -1.0."},

        // Sinh
        {"Sinh[0]", 0.0, "Sinh[0] is 0.0."},
        {"Sinh[1]", std::sinh(1.0), "Sinh[1] is sinh(1)."},

        // Sqrt
        {"Sqrt[0]", 0.0, "Sqrt[0] is 0.0."},
        {"Sqrt[1]", 1.0, "Sqrt[1] is 1.0."},
        {"Sqrt[4]", 2.0, "Sqrt[4] is 2.0."},
        {"Sqrt[9]", 3.0, "Sqrt[9] is 3.0."},

        // Tanh
        {"Tanh[0]", 0.0, "Tanh[0] is 0.0."},
        {"Tanh[1]", std::tanh(1.0), "Tanh[1] is tanh(1)."},

        // Tan
        {"Tan[0]", 0.0, "Tan[0] is 0.0."},
        {"Tan[Pi]", 0.0, "Tan[Pi] is 0.0."},
        {"Tan[Pi/4]", 1.0, "Tan[Pi/4] is 1.0."},
        {"Tan[Pi/6]", std::tan(PI / 6), "Tan[Pi/6] is sqrt(3)/3."}
    };

    // Symbolic cases: argument is symbolic or out of domain, so result is symbolic
    std::vector<SymbolicCase> symbolic_cases = {
        {"Sin[Pi/7]", "Sin", "Pi/7 is not a known special angle, so Sin[Pi/7] remains symbolic."},
        {"Cot[Pi/7]", "Cot", "Pi/7 is not a known special angle, so Cot[Pi/7] remains symbolic."},
        {"ArcSin[2]", "ArcSin", "ArcSin[2] is out of domain (|x|>1), so remains symbolic."},
        {"Log[-1]", "Log", "Log[-1] is not a real number, so remains symbolic."}
    };

    SECTION("Numeric evaluation") {
        for (const auto& tc : numeric_cases) {
            CAPTURE(tc.expr, tc.explanation);
            check_builtin_eval(tc.expr, tc.expected);
        }
    }

    SECTION("Symbolic fallback (unevaluated)") {
        for (const auto& tc : symbolic_cases) {
            CAPTURE(tc.expr, tc.explanation);
            check_symbolic_eval(tc.expr, tc.expected_head);
        }
    }
}

/*
void validate_nested_function_call(
    const std::string& expr_str,
    const std::string& outer_func,
    const std::string& inner_func,
    const std::vector<double>& inner_args
) {
    auto expr = parse_expression(expr_str);
    REQUIRE(expr != nullptr);

    auto* outer = std::get_if<FunctionCall>(&(*expr));
    REQUIRE(outer != nullptr);
    REQUIRE(outer->head == outer_func);
    REQUIRE(outer->args.size() == 1);

    auto* inner = std::get_if<FunctionCall>(&(*outer->args[0]));
    REQUIRE(inner != nullptr);
    REQUIRE(inner->head == inner_func);
    REQUIRE(inner->args.size() == inner_args.size());

    for (size_t i = 0; i < inner_args.size(); ++i) {
        REQUIRE(std::holds_alternative<Number>(*inner->args[i]));
        REQUIRE(std::get<Number>(*inner->args[i]).value == inner_args[i]);
    }
}

TEST_CASE("Parser handles nested function calls (inverse pairs and more)", "[parser][functions]") {
    struct TestCase {
        std::string expr_str;
        std::string outer_func;
        std::string inner_func;
        std::vector<double> inner_args;
    };

    std::vector<TestCase> cases = {
        {"sin[asin[1]]", "sin", "asin", {1.0}},
        {"log[exp[2]]", "log", "exp", {2.0}},
        {"cos[acos[0.5]]", "cos", "acos", {0.5}},
        {"tan[atan[3]]", "tan", "atan", {3.0}},
        {"exp[log[7]]", "exp", "log", {7.0}},
        {"asin[sin[0.7]]", "asin", "sin", {0.7}},
        {"acos[cos[1.2]]", "acos", "cos", {1.2}},
        {"atan[tan[-2]]", "atan", "tan", {-2.0}},
        {"exp[log[3.5]]", "exp", "log", {3.5}},
        {"abs[abs[-7]]", "abs", "abs", {-7.0}},
        {"floor[ceil[3.2]]", "floor", "ceil", {3.2}},
    };

    for (const auto& tc : cases) {
        validate_nested_function_call(tc.expr_str, tc.outer_func, tc.inner_func, tc.inner_args);
    }
}
*/