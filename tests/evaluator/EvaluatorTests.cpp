#include "parser/Parser.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/EvaluatorSemantics.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "kernel/Diagnostics.hpp"
#include "sdk/Policy.hpp"
#include "Constants.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <stdexcept>

using namespace aleph3;

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
    auto expr = parse_expression("Sin[0]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 0.0) < 1e-6);

    expr = parse_expression("Cos[0]");
    result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 1.0) < 1e-6);
}

TEST_CASE("Square root and exponential functions are evaluated correctly", "[evaluator]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("Sqrt[9]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 3.0) < 1e-6);

    expr = parse_expression("Exp[1]");
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
    auto expr = parse_expression("Exp[1]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - std::exp(1)) < 1e-6);

    expr = parse_expression("Exp[0]");
    result = evaluate(expr, ctx);
    REQUIRE(std::abs(get_number_value(result) - 1.0) < 1e-6);
}

TEST_CASE("Floor function is evaluated correctly", "[evaluator][floor]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("Floor[3.7]");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 3.0);

    expr = parse_expression("Floor[-3.7]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == -4.0);
}

TEST_CASE("Ceil function is evaluated correctly", "[evaluator][ceiling]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("Ceiling[3.2]");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 4.0);

    expr = parse_expression("Ceiling[-3.2]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == -3.0);
}

TEST_CASE("Round function is evaluated correctly", "[evaluator][round]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("Round[3.5]");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 4.0);

    expr = parse_expression("Round[3.4]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 3.0);

    expr = parse_expression("Round[-3.5]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == -4.0);

    expr = parse_expression("Round[-3.4]");
    result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == -3.0);
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

TEST_CASE("Rewrite-driven identity simplification preserves list-aware evaluator semantics", "[evaluator][simplification][rewrite]") {
    EvaluationContext ctx;

    auto plus_expr = parse_expression("0 + {x, y}");
    auto plus_result = evaluate(plus_expr, ctx);
    REQUIRE(std::holds_alternative<List>(*plus_result));
    const auto& plus_elements = std::get<List>(*plus_result).elements;
    REQUIRE(plus_elements.size() == 2);
    REQUIRE(to_string(plus_elements[0]) == "x");
    REQUIRE(to_string(plus_elements[1]) == "y");

    auto times_expr = parse_expression("0 * {x, y}");
    auto times_result = evaluate(times_expr, ctx);
    REQUIRE(std::holds_alternative<List>(*times_result));
    const auto& times_elements = std::get<List>(*times_result).elements;
    REQUIRE(times_elements.size() == 2);
    REQUIRE(to_string(times_elements[0]) == "0");
    REQUIRE(to_string(times_elements[1]) == "0");

    auto ones_expr = parse_expression("1 * {x, y}");
    auto ones_result = evaluate(ones_expr, ctx);
    REQUIRE(std::holds_alternative<List>(*ones_result));
    const auto& ones_elements = std::get<List>(*ones_result).elements;
    REQUIRE(ones_elements.size() == 2);
    REQUIRE(to_string(ones_elements[0]) == "x");
    REQUIRE(to_string(ones_elements[1]) == "y");
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

TEST_CASE("N-ary rewrite-driven arithmetic simplification folds scalar numeric buckets", "[evaluator][simplification][rewrite]") {
    EvaluationContext ctx;

    auto plus_expr = parse_expression("y + 0 + 2 + x + 3");
    auto plus_result = evaluate(plus_expr, ctx);
    REQUIRE(to_string(plus_result) == "x + y + 5");

    auto times_expr = parse_expression("y * 1 * 2 * x * 3");
    auto times_result = evaluate(times_expr, ctx);
    REQUIRE(to_string(times_result) == "6 * x * y");
}

TEST_CASE("N-ary rewrite-driven arithmetic simplification preserves exact rational buckets", "[evaluator][simplification][rewrite]") {
    EvaluationContext ctx;

    auto plus_expr = parse_expression("x + 1/2 + 1/3");
    auto plus_result = evaluate(plus_expr, ctx);
    REQUIRE(to_string(plus_result) == "x + 5/6");

    auto times_expr = parse_expression("x * 2 * 1/3");
    auto times_result = evaluate(times_expr, ctx);
    REQUIRE(to_string(times_result) == "2/3 * x");
}

TEST_CASE("Symbolic coefficient contract combines supported like terms", "[evaluator][simplification][coefficients]") {
    EvaluationContext ctx;

    const auto linear = evaluate(parse_expression("x + 2*x + 1/3*x"), ctx);
    REQUIRE(to_string(linear) == "10/3 * x");

    const auto power_basis = evaluate(parse_expression("x^2 + 2*x^2"), ctx);
    REQUIRE(to_string(power_basis) == "3 * x^2");

    const auto cancelled = evaluate(parse_expression("x + (-1 * x)"), ctx);
    REQUIRE(std::holds_alternative<Number>(*cancelled));
    REQUIRE(get_number_value(cancelled) == 0.0);
}

TEST_CASE("Symbolic coefficient contract stays out of unsupported multivariate terms", "[evaluator][simplification][coefficients]") {
    EvaluationContext ctx;

    const auto result = evaluate(parse_expression("x*y + 2*x*y"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    const auto& plus = std::get<FunctionCall>(*result);
    REQUIRE(plus.head == "Plus");
    REQUIRE(plus.args.size() == 2);
    REQUIRE(to_string(result) != "3 * x * y");
}

TEST_CASE("Symbolic coefficient contract preserves unsupported grouped and call-shaped bases", "[evaluator][simplification][coefficients]") {
    EvaluationContext ctx;

    const auto grouped = evaluate(parse_expression("(x + y) + 2*(x + y)"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*grouped));
    REQUIRE(to_string(grouped) != "3 * (x + y)");

    const auto call_shaped = evaluate(parse_expression("f[x] + 2*f[x]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*call_shaped));
    REQUIRE(to_string(call_shaped) != "3 * f[x]");
}

TEST_CASE("Algebra-aware layer merges supported exponent structure only", "[evaluator][simplification][algebra-aware]") {
    EvaluationContext ctx;

    const auto merged_product = evaluate(parse_expression("x * x^2 * y"), ctx);
    REQUIRE(to_string(merged_product) == "x^3 * y");

    const auto collapsed_power = evaluate(parse_expression("(x^2)^3"), ctx);
    REQUIRE(to_string(collapsed_power) == "x^6");

    const auto divide_identity = evaluate(parse_expression("x / x"), ctx);
    REQUIRE(std::holds_alternative<Number>(*divide_identity));
    REQUIRE(get_number_value(divide_identity) == 1.0);
}

TEST_CASE("Algebra-aware layer preserves unsupported exponent structure", "[evaluator][simplification][algebra-aware]") {
    EvaluationContext ctx;

    const auto symbolic_nested_power = evaluate(parse_expression("(x^a)^b"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*symbolic_nested_power));
    REQUIRE(to_string(symbolic_nested_power) != "x^(a * b)");

    const auto mixed_basis = evaluate(parse_expression("x * y * x*y"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*mixed_basis));
    REQUIRE(to_string(mixed_basis) != "x^2 * y^2");
}

TEST_CASE("Evaluator retains ownership of domain-sensitive power semantics", "[evaluator][simplification][algebra-aware]") {
    EvaluationContext ctx;

    const auto negative_fractional_power = evaluate(parse_expression("(-4)^0.5"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*negative_fractional_power));
    REQUIRE(to_string(negative_fractional_power) != "2");
    const auto& power_call = std::get<FunctionCall>(*negative_fractional_power);
    REQUIRE(power_call.head == "Power");
    REQUIRE(power_call.args.size() == 2);

    const auto safe_integer_power = evaluate(parse_expression("(-4)^2"), ctx);
    REQUIRE(std::holds_alternative<Number>(*safe_integer_power));
    REQUIRE(get_number_value(safe_integer_power) == 16.0);
}

TEST_CASE("Evaluator routes fixed Power identities through the kernel-owned entrypoint", "[evaluator][simplification][rewrite]") {
    EvaluationContext ctx;

    const auto zero_exponent = evaluate(parse_expression("mystery[x]^0"), ctx);
    REQUIRE(std::holds_alternative<Number>(*zero_exponent));
    REQUIRE(get_number_value(zero_exponent) == 1.0);

    const auto unit_exponent = evaluate(parse_expression("mystery[x]^1"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*unit_exponent));
    REQUIRE(to_string(unit_exponent) == "mystery[x]");

    const auto unit_base = evaluate(parse_expression("1^mystery[x]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*unit_base));
    REQUIRE(get_number_value(unit_base) == 1.0);
}

TEST_CASE("Evaluator retains ownership of list-aware arithmetic semantics", "[evaluator][simplification][rewrite]") {
    EvaluationContext ctx;

    const auto plus_result = evaluate(parse_expression("{1, 2} + 1/2"), ctx);
    REQUIRE(std::holds_alternative<List>(*plus_result));
    REQUIRE(to_string(plus_result) == "{3/2, 5/2}");

    const auto times_result = evaluate(parse_expression("0 * {x, y}"), ctx);
    REQUIRE(std::holds_alternative<List>(*times_result));
    REQUIRE(to_string(times_result) == "{0, 0}");
}

TEST_CASE("Evaluator contract keeps If lazy and symbolic when required", "[evaluator][contract]") {
    EvaluationContext ctx;

    auto true_branch_only = parse_expression("If[True, 1, 1/0]");
    auto true_result = evaluate(true_branch_only, ctx);
    REQUIRE(std::holds_alternative<Number>(*true_result));
    REQUIRE(get_number_value(true_result) == 1.0);

    auto false_branch_only = parse_expression("If[False, 1/0, 2]");
    auto false_result = evaluate(false_branch_only, ctx);
    REQUIRE(std::holds_alternative<Number>(*false_result));
    REQUIRE(get_number_value(false_result) == 2.0);

    auto symbolic_if = parse_expression("If[x, 1, 2]");
    auto symbolic_result = evaluate(symbolic_if, ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*symbolic_result));
    const auto& if_call = std::get<FunctionCall>(*symbolic_result);
    REQUIRE(if_call.head == "If");
    REQUIRE(to_string(symbolic_result) == "If[x, 1, 2]");
}

TEST_CASE("Evaluator special forms preserve nested branch laziness", "[evaluator][special-forms]") {
    EvaluationContext ctx;

    auto nested_true = parse_expression("If[True, If[False, 1/0, 9], 1/0]");
    auto nested_true_result = evaluate(nested_true, ctx);
    REQUIRE(std::holds_alternative<Number>(*nested_true_result));
    REQUIRE(get_number_value(nested_true_result) == 9.0);

    auto nested_false = parse_expression("If[False, 1/0, If[True, 7, 1/0]]");
    auto nested_false_result = evaluate(nested_false, ctx);
    REQUIRE(std::holds_alternative<Number>(*nested_false_result));
    REQUIRE(get_number_value(nested_false_result) == 7.0);
}

TEST_CASE("Evaluator special forms reject bad If arity", "[evaluator][special-forms]") {
    EvaluationContext ctx;

    const auto too_few =
        make_expr<FunctionCall>("If", std::vector<ExprPtr>{make_expr<Boolean>(true), make_expr<Number>(1.0)});
    REQUIRE_THROWS_AS(evaluate(too_few, ctx), std::runtime_error);

    const auto too_many = make_expr<FunctionCall>(
        "If",
        std::vector<ExprPtr>{
            make_expr<Boolean>(true),
            make_expr<Number>(1.0),
            make_expr<Number>(2.0),
            make_expr<Number>(3.0)
        });
    REQUIRE_THROWS_AS(evaluate(too_many, ctx), std::runtime_error);
}

TEST_CASE("Evaluator contract preserves unresolved symbolic calls", "[evaluator][contract]") {
    EvaluationContext ctx;

    auto unknown_call = parse_expression("mystery[2 + 3]");
    auto result = evaluate(unknown_call, ctx);

    REQUIRE(std::holds_alternative<FunctionCall>(*result));
    const auto& call = std::get<FunctionCall>(*result);
    REQUIRE(call.head == "mystery");
    REQUIRE(call.args.size() == 1);
    REQUIRE(to_string(result) == "mystery[2 + 3]");
}

TEST_CASE("Evaluator builtins preserve symbolic fallback and exact comparisons", "[evaluator][builtins][contract]") {
    EvaluationContext ctx;

    const auto domain_fallback = evaluate(parse_expression("ArcSin[2]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*domain_fallback));
    REQUIRE(to_string(domain_fallback) == "ArcSin[2]");

    const auto symbolic_angle = evaluate(parse_expression("Sin[Pi/7]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*symbolic_angle));
    REQUIRE(to_string(symbolic_angle) == "Sin[Pi / 7]");

    const auto rational_comparison = evaluate(parse_expression("1/2 < 3/4"), ctx);
    REQUIRE(std::holds_alternative<Boolean>(*rational_comparison));
    REQUIRE(std::get<Boolean>(*rational_comparison).value);

    const auto mixed_numeric_exact = evaluate(parse_expression("1/2 + 2"), ctx);
    REQUIRE(std::holds_alternative<Rational>(*mixed_numeric_exact));
    const auto& rational = std::get<Rational>(*mixed_numeric_exact);
    REQUIRE(rational.numerator == 5);
    REQUIRE(rational.denominator == 2);
}

TEST_CASE("Evaluator registry-backed builtins preserve Negate and Clamp behavior", "[evaluator][builtins][contract]") {
    EvaluationContext ctx;

    const auto negated_number = evaluate(parse_expression("-5"), ctx);
    REQUIRE(std::holds_alternative<Number>(*negated_number));
    REQUIRE(get_number_value(negated_number) == -5.0);

    const auto symbolic_negate = evaluate(parse_expression("-x"), ctx);
    REQUIRE(to_string(symbolic_negate) == "-x");

    const auto clamped = evaluate(parse_expression("Clamp[12, 0, 10]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*clamped));
    REQUIRE(get_number_value(clamped) == 10.0);

    const auto symbolic_clamp = evaluate(parse_expression("Clamp[x, 0, 10]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*symbolic_clamp));
    REQUIRE(to_string(symbolic_clamp) == "Clamp[x, 0, 10]");
}

TEST_CASE("Evaluator builtins cover forgotten aliases and inverse trig variants", "[evaluator][builtins]") {
    EvaluationContext ctx;

    const auto ln_result = evaluate(parse_expression("Ln[E]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*ln_result));
    REQUIRE(std::abs(get_number_value(ln_result) - 1.0) < 1e-12);

    const auto ceil_alias = evaluate(parse_expression("Ceil[2.1]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*ceil_alias));
    REQUIRE(std::abs(get_number_value(ceil_alias) - 3.0) < 1e-12);

    const auto sinc_zero = evaluate(parse_expression("Sinc[0]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*sinc_zero));
    REQUIRE(std::abs(get_number_value(sinc_zero) - 1.0) < 1e-12);

    const auto arcsec = evaluate(parse_expression("ArcSec[2]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*arcsec));
    REQUIRE(std::abs(get_number_value(arcsec) - std::acos(0.5)) < 1e-12);

    const auto arccsc_domain = evaluate(parse_expression("ArcCsc[0]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*arccsc_domain));
    REQUIRE(to_string(arccsc_domain) == "ArcCsc[0]");
}

TEST_CASE("Evaluator semantics make unary builtins explicitly listable", "[evaluator][semantics][listable]") {
    EvaluationContext ctx;

    const auto sin_list = evaluate(parse_expression("Sin[{0, Pi/2, Pi}]"), ctx);
    REQUIRE(std::holds_alternative<List>(*sin_list));
    const auto& sin_elements = std::get<List>(*sin_list).elements;
    REQUIRE(sin_elements.size() == 3);
    REQUIRE(std::holds_alternative<Number>(*sin_elements[0]));
    REQUIRE(std::abs(get_number_value(sin_elements[0])) < 1e-12);
    REQUIRE(std::holds_alternative<Number>(*sin_elements[1]));
    REQUIRE(std::abs(get_number_value(sin_elements[1]) - 1.0) < 1e-12);
    REQUIRE(std::holds_alternative<Number>(*sin_elements[2]));
    REQUIRE(std::abs(get_number_value(sin_elements[2])) < 1e-12);

    const auto arcsin_list = evaluate(parse_expression("ArcSin[{0, 2}]"), ctx);
    REQUIRE(std::holds_alternative<List>(*arcsin_list));
    const auto& arcsin_elements = std::get<List>(*arcsin_list).elements;
    REQUIRE(arcsin_elements.size() == 2);
    REQUIRE(std::holds_alternative<Number>(*arcsin_elements[0]));
    REQUIRE(std::abs(get_number_value(arcsin_elements[0])) < 1e-12);
    REQUIRE(std::holds_alternative<FunctionCall>(*arcsin_elements[1]));
    REQUIRE(to_string(arcsin_elements[1]) == "ArcSin[2]");
}

TEST_CASE("Evaluator semantics keep binary listability explicit and exact", "[evaluator][semantics][listable]") {
    EvaluationContext ctx;

    const auto result = evaluate(parse_expression("{1, 2} + 1/2"), ctx);
    REQUIRE(std::holds_alternative<List>(*result));
    const auto& elements = std::get<List>(*result).elements;
    REQUIRE(elements.size() == 2);
    REQUIRE(std::holds_alternative<Rational>(*elements[0]));
    REQUIRE(std::holds_alternative<Rational>(*elements[1]));

    const auto& first = std::get<Rational>(*elements[0]);
    REQUIRE(first.numerator == 3);
    REQUIRE(first.denominator == 2);

    const auto& second = std::get<Rational>(*elements[1]);
    REQUIRE(second.numerator == 5);
    REQUIRE(second.denominator == 2);
}

TEST_CASE("Evaluator semantics keep comparisons explicit, binary, and non-listable", "[evaluator][semantics][comparison]") {
    EvaluationContext ctx;

    const auto symbolic_list_comparison = evaluate(parse_expression("{1, 2} < 3"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*symbolic_list_comparison));
    REQUIRE(to_string(symbolic_list_comparison) == "{1, 2} < 3");

    const auto symbolic_equality = evaluate(parse_expression("{1, 2} == {1, 2}"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*symbolic_equality));
    REQUIRE(to_string(symbolic_equality) == "{1, 2} == {1, 2}");

    const auto too_few = make_expr<FunctionCall>("Less", std::vector<ExprPtr>{make_expr<Number>(1.0)});
    REQUIRE_THROWS_AS(evaluate(too_few, ctx), std::runtime_error);

    const auto too_many = make_expr<FunctionCall>(
        "ArcTan",
        std::vector<ExprPtr>{make_expr<Number>(1.0), make_expr<Number>(2.0), make_expr<Number>(3.0)});
    REQUIRE_THROWS_AS(evaluate(too_many, ctx), std::runtime_error);
}

TEST_CASE("Evaluator semantics drive numeric-function dispatch and edge-domain fallback", "[evaluator][semantics][numeric]") {
    EvaluationContext ctx;

    const auto arctan_numeric = evaluate(parse_expression("ArcTan[1, 1]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*arctan_numeric));
    REQUIRE(std::abs(get_number_value(arctan_numeric) - (PI / 4.0)) < 1e-12);

    const auto log_numeric = evaluate(parse_expression("Log[10, 100]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*log_numeric));
    REQUIRE(std::abs(get_number_value(log_numeric) - 2.0) < 1e-12);

    const auto symbolic_log_argument = evaluate(parse_expression("Log[10, x]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*symbolic_log_argument));
    REQUIRE(to_string(symbolic_log_argument) == "Log[10, x]");

    const auto invalid_log_base = evaluate(parse_expression("Log[1, 10]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*invalid_log_base));
    REQUIRE(to_string(invalid_log_base) == "Log[1, 10]");

    const auto invalid_log_value = evaluate(parse_expression("Log[-2, 8]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*invalid_log_value));
    REQUIRE(to_string(invalid_log_value) == "Log[-2, 8]");

    const auto negative_fractional_power = evaluate(parse_expression("(-4)^0.5"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*negative_fractional_power));
    const auto& power_call = std::get<FunctionCall>(*negative_fractional_power);
    REQUIRE(power_call.head == "Power");
    REQUIRE(power_call.args.size() == 2);
    REQUIRE(std::holds_alternative<Number>(*power_call.args[0]));
    REQUIRE(std::holds_alternative<Number>(*power_call.args[1]));
    REQUIRE(std::abs(get_number_value(power_call.args[0]) + 4.0) < 1e-12);
    REQUIRE(std::abs(get_number_value(power_call.args[1]) - 0.5) < 1e-12);
}

TEST_CASE("Evaluator semantics keep Flat and Orderless behavior out of opaque evaluator dispatch", "[evaluator][semantics][canonicalization]") {
    EvaluationContext ctx;

    const auto plus = evaluate(parse_expression("y + (x + 2)"), ctx);
    REQUIRE(to_string(plus) == "x + y + 2");

    const auto times = evaluate(parse_expression("z * (x * 2)"), ctx);
    REQUIRE(to_string(times) == "2 * x * z");

    const auto opaque = evaluate(parse_expression("f[y, f[x, 2]]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*opaque));
    REQUIRE(to_string(opaque) == "f[y, f[x, 2]]");
}

TEST_CASE("Evaluator numeric-function listability preserves edge-case fallback elementwise", "[evaluator][semantics][numeric][listable]") {
    EvaluationContext ctx;

    const auto log_list = evaluate(parse_expression("Log[{1, E, -1}]"), ctx);
    REQUIRE(std::holds_alternative<List>(*log_list));
    const auto& log_elements = std::get<List>(*log_list).elements;
    REQUIRE(log_elements.size() == 3);
    REQUIRE(std::holds_alternative<Number>(*log_elements[0]));
    REQUIRE(std::abs(get_number_value(log_elements[0])) < 1e-12);
    REQUIRE(std::holds_alternative<Number>(*log_elements[1]));
    REQUIRE(std::abs(get_number_value(log_elements[1]) - 1.0) < 1e-12);
    REQUIRE(std::holds_alternative<FunctionCall>(*log_elements[2]));
    REQUIRE(to_string(log_elements[2]) == "Log[-1]");

    const auto sqrt_list = evaluate(parse_expression("Sqrt[{4, 9, -1}]"), ctx);
    REQUIRE(std::holds_alternative<List>(*sqrt_list));
    const auto& sqrt_elements = std::get<List>(*sqrt_list).elements;
    REQUIRE(sqrt_elements.size() == 3);
    REQUIRE(std::holds_alternative<Number>(*sqrt_elements[0]));
    REQUIRE(std::abs(get_number_value(sqrt_elements[0]) - 2.0) < 1e-12);
    REQUIRE(std::holds_alternative<Number>(*sqrt_elements[1]));
    REQUIRE(std::abs(get_number_value(sqrt_elements[1]) - 3.0) < 1e-12);
    REQUIRE(std::holds_alternative<FunctionCall>(*sqrt_elements[2]));
    REQUIRE(to_string(sqrt_elements[2]) == "Sqrt[-1]");
}

TEST_CASE("Evaluator semantics registry drives structural and algebra dispatch", "[evaluator][semantics][dispatch]") {
    EvaluationContext ctx;

    REQUIRE(is_structural_function("List"));
    REQUIRE(is_algebra_function("Expand"));
    REQUIRE(is_algebra_function("PolynomialQuotient"));
    REQUIRE_FALSE(is_algebra_function("f"));
    REQUIRE_FALSE(is_structural_function("f"));

    const auto list_result = evaluate(parse_expression("{x, 1 + 2, Sin[0]}"), ctx);
    REQUIRE(std::holds_alternative<List>(*list_result));
    const auto& elements = std::get<List>(*list_result).elements;
    REQUIRE(elements.size() == 3);
    REQUIRE(std::holds_alternative<Symbol>(*elements[0]));
    REQUIRE(std::holds_alternative<Number>(*elements[1]));
    REQUIRE(std::abs(get_number_value(elements[1]) - 3.0) < 1e-12);
    REQUIRE(std::holds_alternative<Number>(*elements[2]));
    REQUIRE(std::abs(get_number_value(elements[2])) < 1e-12);

    const auto expanded = evaluate(parse_expression("Expand[(x + 1) * (x + 2)]"), ctx);
    REQUIRE(to_string(expanded) == "x^2 + 3 * x + 2");
}

TEST_CASE("Evaluator simplification rules flatten, cancel, and combine symbolic structure", "[evaluator][simplification]") {
    EvaluationContext ctx;

    const auto nested_plus = evaluate(parse_expression("x + (y + 2) + 3"), ctx);
    REQUIRE(to_string(nested_plus) == "x + y + 5");

    const auto cancelled_sum = evaluate(parse_expression("x + (-1 * x)"), ctx);
    REQUIRE(std::holds_alternative<Number>(*cancelled_sum));
    REQUIRE(get_number_value(cancelled_sum) == 0.0);

    const auto merged_product = evaluate(parse_expression("x * x * y"), ctx);
    REQUIRE(to_string(merged_product) == "x^2 * y");

    const auto collapsed_power = evaluate(parse_expression("(x^2)^3"), ctx);
    REQUIRE(to_string(collapsed_power) == "x^6");

    const auto divided_identity = evaluate(parse_expression("(x + 1) / (x + 1)"), ctx);
    REQUIRE(std::holds_alternative<Number>(*divided_identity));
    REQUIRE(get_number_value(divided_identity) == 1.0);
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

//TODO: needs support for complex numbers, disable temporarily
#if 0
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
#endif

TEST_CASE("Evaluator handles 0/0 as Indeterminate", "[evaluator][indeterminate]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("0 / 0");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Indeterminate>(*result));
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
    REQUIRE(std::holds_alternative<Symbol>(*func.args[0]));
    REQUIRE(std::get<Symbol>(*func.args[0]).name == "z");
    REQUIRE(std::holds_alternative<Number>(*func.args[1]));
    REQUIRE(std::get<Number>(*func.args[1]).value == 1.0);
}

TEST_CASE("Evaluator handles nested parentheses correctly", "[evaluator]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("2 * (3 + (4 * (5 - 1)))");
    auto result = evaluate(expr, ctx);
    REQUIRE(get_number_value(result) == 38.0);
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

TEST_CASE("Evaluator logic special forms preserve short-circuiting and symbolic tails", "[evaluator][logic][special-forms]") {
    EvaluationContext ctx;

    auto short_circuit_false = evaluate(parse_expression("False && (1/0)"), ctx);
    REQUIRE(std::holds_alternative<Boolean>(*short_circuit_false));
    REQUIRE(std::get<Boolean>(*short_circuit_false).value == false);

    auto short_circuit_true = evaluate(parse_expression("True || (1/0)"), ctx);
    REQUIRE(std::holds_alternative<Boolean>(*short_circuit_true));
    REQUIRE(std::get<Boolean>(*short_circuit_true).value == true);

    auto partially_evaluated_and = evaluate(make_expr<FunctionCall>("And", std::vector<ExprPtr>{
        parse_expression("1 < 2"),
        make_expr<Symbol>("x"),
        parse_expression("1/0")
    }), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*partially_evaluated_and));
    const auto& and_call = std::get<FunctionCall>(*partially_evaluated_and);
    REQUIRE(and_call.head == "And");
    REQUIRE(and_call.args.size() == 3);
    REQUIRE(std::holds_alternative<Boolean>(*and_call.args[0]));
    REQUIRE(std::get<Boolean>(*and_call.args[0]).value == true);
    REQUIRE(std::holds_alternative<Symbol>(*and_call.args[1]));
    REQUIRE(std::get<Symbol>(*and_call.args[1]).name == "x");

    auto partially_evaluated_or = evaluate(make_expr<FunctionCall>("Or", std::vector<ExprPtr>{
        parse_expression("1 > 2"),
        make_expr<Symbol>("x"),
        parse_expression("1/0")
    }), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*partially_evaluated_or));
    const auto& or_call = std::get<FunctionCall>(*partially_evaluated_or);
    REQUIRE(or_call.head == "Or");
    REQUIRE(or_call.args.size() == 3);
    REQUIRE(std::holds_alternative<Boolean>(*or_call.args[0]));
    REQUIRE(std::get<Boolean>(*or_call.args[0]).value == false);
    REQUIRE(std::holds_alternative<Symbol>(*or_call.args[1]));
    REQUIRE(std::get<Symbol>(*or_call.args[1]).name == "x");
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

TEST_CASE("Evaluator logic special forms support identity arities", "[evaluator][logic][special-forms]") {
    EvaluationContext ctx;

    auto empty_and = evaluate(make_expr<FunctionCall>("And", std::vector<ExprPtr>{}), ctx);
    REQUIRE(std::holds_alternative<Boolean>(*empty_and));
    REQUIRE(std::get<Boolean>(*empty_and).value == true);

    auto empty_or = evaluate(make_expr<FunctionCall>("Or", std::vector<ExprPtr>{}), ctx);
    REQUIRE(std::holds_alternative<Boolean>(*empty_or));
    REQUIRE(std::get<Boolean>(*empty_or).value == false);
}

TEST_CASE("Evaluator handles StringJoin", "[evaluator][string]") {
    EvaluationContext ctx;

    // Test "Aleph3" <> " Rocks"
    auto expr = parse_expression("\"Aleph3\" <> \" Rocks\"");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "Aleph3 Rocks");

    // Test "Hello" <> " " <> "World"
    expr = parse_expression("\"Hello\" <> \" \" <> \"World\"");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "Hello World");

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

    // Test StringLength["Aleph3 Rocks"]
    expr = parse_expression("StringLength[\"Aleph3 Rocks\"]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 12);
}

TEST_CASE("Evaluator handles StringReplace", "[evaluator][string]") {
    EvaluationContext ctx;

    // Test StringReplace["Hello World", "World" -> "Aleph3"]
    auto expr = parse_expression("StringReplace[\"Hello World\", \"World\" -> \"Aleph3\"]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<String>(*result));
    REQUIRE(std::get<String>(*result).value == "Hello Aleph3");

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

TEST_CASE("Evaluator exposes rule-driven symbolic transforms", "[evaluator][rewrite]") {
    EvaluationContext ctx;

    auto expr = parse_expression("Replace[f[x], f[a_] -> g[a]]");
    auto result = evaluate(expr, ctx);
    REQUIRE(to_string(result) == "g[x]");

    expr = parse_expression("ReplaceRepeated[f[f[x]], f[a_] -> g[a]]");
    result = evaluate(expr, ctx);
    REQUIRE(to_string(result) == "g[g[x]]");

    expr = parse_expression("MatchQ[f[x, x], f[a_, a_]]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE(std::get<Boolean>(*result).value);

    expr = parse_expression("MatchQ[f[x, y], f[a_, a_]]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE_FALSE(std::get<Boolean>(*result).value);
}

TEST_CASE("Evaluator keeps replacement explicit and bounded", "[evaluator][rewrite]") {
    EvaluationContext ctx;

    auto expr = parse_expression("Replace[f[y + x], f[a_] -> g[a]]");
    auto result = evaluate(expr, ctx);
    REQUIRE(to_string(result) == "g[x + y]");

    expr = parse_expression("Replace[f[x], 3]");
    REQUIRE_THROWS_WITH(evaluate(expr, ctx), "Replace expects the second argument to be a Rule");

    expr = parse_expression("ReplaceRepeated[f[x], 3]");
    REQUIRE_THROWS_WITH(
        evaluate(expr, ctx),
        "ReplaceRepeated expects the second argument to be a Rule");
}

TEST_CASE("Evaluator applies rewrite budget to ReplaceRepeated", "[evaluator][rewrite]") {
    Bindings bindings;
    Bindings constants;
    kernel::HostFunctionRegistry host_functions;
    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 1;

    EvaluationContext ctx(bindings, constants, host_functions, policy);
    ctx.enable_runtime_strict_semantics(true);
    ctx.reset_runtime_step_counter();

    auto expr = parse_expression("ReplaceRepeated[f[f[x]], f[a_] -> g[a]]");
    REQUIRE_THROWS_AS(evaluate(expr, ctx), kernel::RuntimeFailure);
}

TEST_CASE("Evaluator scopes assumptions through Assuming and Refine", "[evaluator][assumptions]") {
    EvaluationContext ctx;

    auto expr = parse_expression("Assuming[x > 0, If[x > 0, 1, 2]]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 1.0);

    expr = parse_expression("If[x > 0, 1, 2]");
    result = evaluate(expr, ctx);
    REQUIRE(to_string(result) == "If[x > 0, 1, 2]");

    expr = parse_expression("Refine[Abs[x], x >= 0]");
    result = evaluate(expr, ctx);
    REQUIRE(to_string(result) == "x");

    expr = parse_expression("Refine[Sqrt[x^2], x <= 0]");
    result = evaluate(expr, ctx);
    REQUIRE(to_string(result) == "-x");
}

TEST_CASE("Evaluator resolves boolean and comparison facts from assumptions", "[evaluator][assumptions]") {
    EvaluationContext ctx;

    auto expr = parse_expression("Assuming[flag, If[flag, 7, 9]]");
    auto result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 7.0);

    expr = parse_expression("Refine[x > 0, And[x >= 0, NotEqual[x, 0]]]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE(std::get<Boolean>(*result).value);

    expr = parse_expression("Refine[x == 0, x != 0]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Boolean>(*result));
    REQUIRE_FALSE(std::get<Boolean>(*result).value);
}

TEST_CASE("Evaluator rejects unsupported assumption forms explicitly", "[evaluator][assumptions]") {
    EvaluationContext ctx;

    auto expr = parse_expression("Assuming[Not[x > 0], x]");
    REQUIRE_THROWS_WITH(
        evaluate(expr, ctx),
        "Assumptions only support Not[symbol] for boolean facts.");

    expr = parse_expression("Refine[x, False]");
    REQUIRE_THROWS_WITH(
        evaluate(expr, ctx),
        "False assumptions are not supported yet.");
}

TEST_CASE("Evaluator throws error for invalid StringJoin arguments", "[evaluator][string]") {
    EvaluationContext ctx;

    // Test invalid argument: "Hello" <> 123
    auto expr = parse_expression("\"Hello\" <> 123");
    REQUIRE_THROWS_WITH(evaluate(expr, ctx), "StringJoin expects string arguments");
}

TEST_CASE("Evaluator handles list operations", "[evaluator][lists]") {
    EvaluationContext ctx; // Empty context
    auto expr = parse_expression("{1, 2, 3} + {4, 5, 6}");
    auto result = evaluate(expr, ctx);

    // Ensure the result is a list: {5, 7, 9}
    REQUIRE(std::holds_alternative<List>(*result));
    auto list = std::get<List>(*result);
    REQUIRE(list.elements.size() == 3);
    REQUIRE(get_number_value(list.elements[0]) == 5.0);
    REQUIRE(get_number_value(list.elements[1]) == 7.0);
    REQUIRE(get_number_value(list.elements[2]) == 9.0);
}

TEST_CASE("Evaluator handles scalar and list addition (broadcast)", "[evaluator][lists]") {
    EvaluationContext ctx;
    auto expr = parse_expression("10 + {1, 2, 3}");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<List>(*result));
    auto list = std::get<List>(*result);
    REQUIRE(list.elements.size() == 3);
    REQUIRE(get_number_value(list.elements[0]) == 11.0);
    REQUIRE(get_number_value(list.elements[1]) == 12.0);
    REQUIRE(get_number_value(list.elements[2]) == 13.0);

    expr = parse_expression("{1, 2, 3} + 10");
    result = evaluate(expr, ctx);
    list = std::get<List>(*result);
    REQUIRE(list.elements.size() == 3);
    REQUIRE(get_number_value(list.elements[0]) == 11.0);
    REQUIRE(get_number_value(list.elements[1]) == 12.0);
    REQUIRE(get_number_value(list.elements[2]) == 13.0);
}

TEST_CASE("Evaluator handles elementwise multiplication of lists", "[evaluator][lists]") {
    EvaluationContext ctx;
    auto expr = parse_expression("{1, 2, 3} * {4, 5, 6}");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<List>(*result));
    auto list = std::get<List>(*result);
    REQUIRE(list.elements.size() == 3);
    REQUIRE(get_number_value(list.elements[0]) == 4.0);
    REQUIRE(get_number_value(list.elements[1]) == 10.0);
    REQUIRE(get_number_value(list.elements[2]) == 18.0);
}

TEST_CASE("Evaluator handles scalar and list multiplication (broadcast)", "[evaluator][lists]") {
    EvaluationContext ctx;
    auto expr = parse_expression("2 * {4, 5, 6}");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<List>(*result));
    auto list = std::get<List>(*result);
    REQUIRE(list.elements.size() == 3);
    REQUIRE(get_number_value(list.elements[0]) == 8.0);
    REQUIRE(get_number_value(list.elements[1]) == 10.0);
    REQUIRE(get_number_value(list.elements[2]) == 12.0);

    expr = parse_expression("{4, 5, 6} * 2");
    result = evaluate(expr, ctx);
    list = std::get<List>(*result);
    REQUIRE(list.elements.size() == 3);
    REQUIRE(get_number_value(list.elements[0]) == 8.0);
    REQUIRE(get_number_value(list.elements[1]) == 10.0);
    REQUIRE(get_number_value(list.elements[2]) == 12.0);
}

TEST_CASE("Evaluator handles nested lists with elementwise addition", "[evaluator][lists]") {
    EvaluationContext ctx;
    auto expr = parse_expression("{{1, 2}, {3, 4}} + {{10, 20}, {30, 40}}");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<List>(*result));
    auto outer = std::get<List>(*result);
    REQUIRE(outer.elements.size() == 2);

    auto* inner1 = std::get_if<List>(outer.elements[0].get());
    auto* inner2 = std::get_if<List>(outer.elements[1].get());
    REQUIRE(inner1);
    REQUIRE(inner2);

    REQUIRE(get_number_value(inner1->elements[0]) == 11.0);
    REQUIRE(get_number_value(inner1->elements[1]) == 22.0);
    REQUIRE(get_number_value(inner2->elements[0]) == 33.0);
    REQUIRE(get_number_value(inner2->elements[1]) == 44.0);
}

TEST_CASE("Evaluator throws on mismatched list sizes", "[evaluator][lists]") {
    EvaluationContext ctx;
    auto expr = parse_expression("{1, 2} + {3, 4, 5}");
    REQUIRE_THROWS_WITH(evaluate(expr, ctx), "List sizes must match for elementwise operation");
}

TEST_CASE("Evaluator errors expose stable categories", "[evaluator][errors]") {
    EvaluationContext ctx;

    try {
        evaluate(make_fcall("If", {make_expr<Boolean>(true), make_expr<Number>(1)}), ctx);
        FAIL("Expected invalid arity error");
    }
    catch (const EvaluatorError& err) {
        REQUIRE(err.kind() == EvaluatorErrorKind::invalid_arity);
        REQUIRE(std::string(err.what()) == "If expects exactly 3 arguments");
    }

    try {
        evaluate(parse_expression("{1, 2} + {3, 4, 5}"), ctx);
        FAIL("Expected invalid form error");
    }
    catch (const EvaluatorError& err) {
        REQUIRE(err.kind() == EvaluatorErrorKind::invalid_form);
        REQUIRE(std::string(err.what()) == "List sizes must match for elementwise operation");
    }
}

TEST_CASE("Evaluator handles lists with symbolic elements", "[evaluator][lists]") {
    EvaluationContext ctx;
    auto expr = parse_expression("{x, y, 3} + {1, 2, z}");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<List>(*result));
    auto list = std::get<List>(*result);
    REQUIRE(list.elements.size() == 3);

    // x + 1
    REQUIRE(std::holds_alternative<FunctionCall>(*list.elements[0]));
    auto f0 = std::get<FunctionCall>(*list.elements[0]);
    REQUIRE(f0.head == "Plus");

    // y + 2
    REQUIRE(std::holds_alternative<FunctionCall>(*list.elements[1]));
    auto f1 = std::get<FunctionCall>(*list.elements[1]);
    REQUIRE(f1.head == "Plus");

    // 3 + z
    REQUIRE(std::holds_alternative<FunctionCall>(*list.elements[2]));
    auto f2 = std::get<FunctionCall>(*list.elements[2]);
    REQUIRE(f2.head == "Plus");
}

TEST_CASE("Evaluator handles Length for lists", "[evaluator][lists]") {
    EvaluationContext ctx;
    auto expr = parse_expression("Length[{1, 2, 3, 4}]");
    auto result = evaluate(expr, ctx);

    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 4.0);

    expr = parse_expression("Length[{}]");
    result = evaluate(expr, ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 0.0);
}

struct EvalCase {
    std::string input;
    std::map<std::string, double> env;
    double expected;
};

TEST_CASE("Evaluator handles division by products and negatives") {
    std::vector<EvalCase> cases = {
        {"x/-3x",   {{"x", 2.0}}, -1.0 / 3.0},
        {"y/2y",    {{"y", 5.0}}, 0.5},
        {"a/-b",    {{"a", 6.0}, {"b", 2.0}}, -3.0},
        {"z/4w",    {{"z", 8.0}, {"w", 2.0}}, 1.0},
        {"t/-7t",   {{"t", 7.0}}, -1.0 / 7.0},
        {"m/(-2m)", {{"m", 10.0}}, -0.5},
        {"p/(-q)",  {{"p", 9.0}, {"q", 3.0}}, -3.0},
    };

    for (const auto& c : cases) {
        CAPTURE(c.input, c.env, c.expected);

        EvaluationContext ctx;
        for (const auto& [var, val] : c.env) {
            ctx.variables[var] = make_expr<Number>(val);
        }

        ExprPtr expr = parse_expression(c.input);
        ExprPtr result_expr = evaluate(expr, ctx);

        double value = 0.0;
        if (auto* num = std::get_if<Number>(&(*result_expr))) {
            value = num->value;
        }
        else if (auto* rat = std::get_if<Rational>(&(*result_expr))) {
            value = static_cast<double>(rat->numerator) / rat->denominator;
        }
        else {
            FAIL("Evaluator did not return a numeric result");
        }
        REQUIRE(value == Catch::Approx(c.expected));
    }
}
