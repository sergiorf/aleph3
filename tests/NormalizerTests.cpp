#include "normalizer/Normalizer.hpp"

#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
#include "parser/Parser.hpp"
#include "transforms/Transforms.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

namespace {

void expect_normalizes_to(const std::string& source, const std::string& expected) {
    auto expr = parse_expression(source);
    auto normalized = normalize_expr(expr);
    REQUIRE(to_string(normalized) == expected);
    REQUIRE(to_string(normalize_expr(normalized)) == expected);
}

void expect_raw_normalizes_to(const std::string& source, const std::string& expected_raw) {
    auto expr = parse_expression(source);
    auto normalized = normalize_expr(expr);
    REQUIRE(to_string_raw(normalized) == expected_raw);
    REQUIRE(to_string_raw(normalize_expr(normalized)) == expected_raw);
}

std::string normalized_string(const std::string& source) {
    return to_string(normalize_expr(parse_expression(source)));
}

std::string evaluated_string(const std::string& source) {
    EvaluationContext ctx;
    return to_string(evaluate(parse_expression(source), ctx));
}

std::string simplified_evaluated_string(const std::string& source) {
    EvaluationContext ctx;
    return to_string(simplify(evaluate(parse_expression(source), ctx)));
}

std::string renormalized_evaluated_string(const std::string& source) {
    EvaluationContext ctx;
    return to_string(normalize_expr(evaluate(parse_expression(source), ctx)));
}

std::string renormalized_evaluated_string(
    const std::string& source,
    EvaluationContext& ctx) {
    return to_string(normalize_expr(evaluate(parse_expression(source), ctx)));
}

std::string renormalized_simplified_evaluated_string(const std::string& source) {
    EvaluationContext ctx;
    return to_string(normalize_expr(simplify(evaluate(parse_expression(source), ctx))));
}

std::string renormalized_simplified_evaluated_string(
    const std::string& source,
    EvaluationContext& ctx) {
    return to_string(normalize_expr(simplify(evaluate(parse_expression(source), ctx))));
}

}  // namespace

TEST_CASE("Normalizer canonicalizes commutative sums", "[normalizer]") {
    expect_normalizes_to("y + x + 2", "x + y + 2");
    expect_normalizes_to("x + (z + y) + 2", "x + y + z + 2");
}

TEST_CASE("Normalizer canonicalizes commutative products", "[normalizer]") {
    expect_normalizes_to("z * x * 2 * y", "2 * x * y * z");
    expect_normalizes_to("x * (z * y) * 2", "2 * x * y * z");
}

TEST_CASE("Normalizer uses semantics-declared Flat and Orderless only for supported heads", "[normalizer][semantics]") {
    auto plus = normalize_expr(make_fcall("Plus", {
        make_expr<Symbol>("y"),
        make_fcall("Plus", {make_expr<Symbol>("x"), make_expr<Number>(2.0)})
    }));
    REQUIRE(to_string(plus) == "x + y + 2");

    auto times = normalize_expr(make_fcall("Times", {
        make_expr<Symbol>("z"),
        make_fcall("Times", {make_expr<Symbol>("x"), make_expr<Number>(2.0)})
    }));
    REQUIRE(to_string(times) == "2 * x * z");

    auto opaque = normalize_expr(make_fcall("f", {
        make_expr<Symbol>("y"),
        make_fcall("f", {make_expr<Symbol>("x"), make_expr<Number>(2.0)})
    }));
    REQUIRE(to_string(opaque) == "f[y, f[x, 2]]");
}

TEST_CASE("Normalizer normalizes rationals and lowered subtraction forms", "[normalizer]") {
    auto rational = normalize_expr(make_expr<Rational>(2, -4));
    REQUIRE(to_string(rational) == "-1/2");

    auto minus_form = normalize_expr(parse_expression("x - y"));
    auto explicit_form = normalize_expr(parse_expression("x + (-1 * y)"));
    REQUIRE(to_string(minus_form) == "x - y");
    REQUIRE(to_string(explicit_form) == "x - y");
    REQUIRE(to_string_raw(minus_form) == to_string_raw(explicit_form));
}

TEST_CASE("Normalizer gives stable equivalent forms for supported subset", "[normalizer]") {
    auto lhs = normalize_expr(parse_expression("b + a + c"));
    auto rhs = normalize_expr(parse_expression("c + b + a"));

    REQUIRE(to_string(lhs) == "a + b + c");
    REQUIRE(to_string(rhs) == "a + b + c");
    REQUIRE(to_string_raw(lhs) == to_string_raw(rhs));
}

TEST_CASE("Normalizer orders algebraic terms ahead of opaque function calls", "[normalizer]") {
    expect_normalizes_to("Sin[y] + x^2 + x + 2", "x^2 + x + (Sin[y]) + 2");
    expect_normalizes_to("f[b] + f[a] + x", "x + (f[a]) + (f[b])");
}

TEST_CASE("Normalizer ignores numeric coefficients when ordering symbolic monomials", "[normalizer]") {
    expect_normalizes_to("2 * y + x + 3 * x^2", "3 * x^2 + x + 2 * y");
    expect_normalizes_to("z * x^2 * y * f[a]", "x^2 * y * z * (f[a])");
}

TEST_CASE("Normalizer preserves exact structure while ordering opaque calls deterministically", "[normalizer]") {
    expect_normalizes_to("g[b] + f[c] + f[a]", "(f[a]) + (f[c]) + (g[b])");
    expect_normalizes_to("g[b] * f[c] * f[a]", "(f[a]) * (f[c]) * (g[b])");
}

TEST_CASE("Normalizer recurses through opaque call arguments and nested mixed products", "[normalizer]") {
    expect_normalizes_to("f[y + x, z * x * 2, (b + a)^2]", "f[x + y, 2 * x * z, (a + b)^2]");
    expect_normalizes_to("f[b] * y * x^2 * f[a] * 3", "3 * x^2 * y * (f[a]) * (f[b])");
}

TEST_CASE("Normalizer canonicalizes lowered subtraction and negation in raw form", "[normalizer]") {
    expect_raw_normalizes_to("x - y", "x+-1*y");
    expect_raw_normalizes_to("-(x + y)", "-1*x+y");
    expect_raw_normalizes_to("x + (-1 * y)", "x+-1*y");
}

TEST_CASE("Normalizer folds supported complex additive forms", "[normalizer]") {
    auto simple = normalize_expr(parse_expression("2 + 5*I"));
    REQUIRE(std::holds_alternative<Complex>(*simple));
    REQUIRE(to_string(simple) == "2 + 5*I");

    auto negative = normalize_expr(parse_expression("2 - 5*I"));
    REQUIRE(std::holds_alternative<Complex>(*negative));
    REQUIRE(to_string(negative) == "2 - 5*I");
}

TEST_CASE("Normalizer recurses through lists, rules, assignments, and function definitions", "[normalizer]") {
    auto list = normalize_expr(parse_expression("{y + x, z * x * 2}"));
    REQUIRE(to_string(list) == "List[x + y, 2 * x * z]");

    auto rule = normalize_expr(make_expr<Rule>(
        parse_expression("y + x"),
        parse_expression("z * x * 2")));
    REQUIRE(to_string(rule) == "x + y -> 2 * x * z");

    auto assignment = normalize_expr(make_expr<Assignment>(
        "lhs",
        parse_expression("y + x + 2")));
    REQUIRE(to_string(assignment) == "lhs = x + y + 2");

    auto function = normalize_expr(make_fdef(
        "f",
        {Parameter("a", parse_expression("y + x"))},
        parse_expression("z * x * 2"),
        true));
    REQUIRE(to_string(function) == "f[a_:x + y] := 2 * x * z");
}

TEST_CASE("Normalizer keeps power and divide structure while normalizing arguments", "[normalizer]") {
    expect_normalizes_to("(y + x)^2", "(x + y)^2");
    expect_normalizes_to("(z * x * 2) / (y + x)", "2 * x * z / (x + y)");
}

TEST_CASE("Normalizer keeps nested canonical structure idempotent across recursive containers", "[normalizer][contract]") {
    expect_normalizes_to(
        "{f[y + x], (z * x * 2) / (b + a), lhs -> (d + c)}",
        "List[f[x + y], 2 * x * z / (a + b), lhs -> c + d]");
}

TEST_CASE("Canonical structure agrees across normalize evaluate and simplify after renormalization", "[normalizer][contract]") {
    struct Case {
        const char* source;
        const char* expected;
    };

    const std::vector<Case> cases = {
        {"b + a + c", "a + b + c"},
        {"x - y", "x - y"},
        {"2 * y + x + 3 * x^2", "3 * x^2 + x + 2 * y"},
        {"If[x, 1, 2]", "If[x, 1, 2]"},
        {"f[b] + f[a] + x", "x + (f[a]) + (f[b])"}
    };

    for (const auto& c : cases) {
        DYNAMIC_SECTION("Cross-layer canonical agreement for: " << c.source) {
            REQUIRE(normalized_string(c.source) == c.expected);
            REQUIRE(renormalized_evaluated_string(c.source) == c.expected);
            REQUIRE(renormalized_simplified_evaluated_string(c.source) == c.expected);
        }
    }
}

TEST_CASE("Cross-layer exact arithmetic differences are intentional", "[normalizer][contract]") {
    REQUIRE(normalized_string("1/2 + 1/3") == "1/2 + 1/3");
    REQUIRE(evaluated_string("1/2 + 1/3") == "5/6");
    REQUIRE(simplified_evaluated_string("1/2 + 1/3") == "5/6");

    REQUIRE(normalized_string("1/2 + 2") == "1/2 + 2");
    REQUIRE(evaluated_string("1/2 + 2") == "5/2");
    REQUIRE(simplified_evaluated_string("1/2 + 2") == "5/2");
}

TEST_CASE("Cross-layer symbolic fallback is preserved intentionally", "[normalizer][contract]") {
    REQUIRE(normalized_string("mystery[2 + 3]") == "mystery[2 + 3]");
    REQUIRE(evaluated_string("mystery[2 + 3]") == "mystery[2 + 3]");
    REQUIRE(simplified_evaluated_string("mystery[2 + 3]") == "mystery[2 + 3]");
}

TEST_CASE("Cross-layer canonical ordering stays stable after evaluation-driven rewrites", "[normalizer][contract]") {
    const auto normalized = normalized_string("x + (y + 2) + 3");
    const auto evaluated = evaluated_string("x + (y + 2) + 3");
    const auto simplified = simplified_evaluated_string("x + (y + 2) + 3");

    REQUIRE(normalized == "x + y + 2 + 3");
    REQUIRE(evaluated == "5 + x + y");
    REQUIRE(simplified == "x + y + 5");

    REQUIRE(to_string(normalize_expr(parse_expression(simplified))) == "x + y + 5");
}

TEST_CASE("Cross-layer UDF outputs normalize to a stable canonical form", "[normalizer][contract][udf]") {
    EvaluationContext ctx;
    evaluate(parse_expression("f[x_, y_:b + a] := y + x + c"), ctx);

    REQUIRE(renormalized_evaluated_string("f[z]", ctx) == "a + b + c + z");
    REQUIRE(renormalized_simplified_evaluated_string("f[z]", ctx) == "a + b + c + z");

    ctx.variables["offset"] = parse_expression("d + c");
    evaluate(parse_expression("shift[x_, y_:offset] := y + x + b + a"), ctx);

    REQUIRE(renormalized_evaluated_string("shift[z]", ctx) == "a + b + c + d + z");
    REQUIRE(renormalized_simplified_evaluated_string("shift[z]", ctx) == "a + b + c + d + z");
}

TEST_CASE("Cross-layer algebra outputs renormalize to stable canonical forms", "[normalizer][contract][algebra]") {
    EvaluationContext ctx;

    REQUIRE(
        renormalized_simplified_evaluated_string("Expand[(y + x) * (x + z)]", ctx) ==
        "x^2 + x * y + x * z + y * z");
    REQUIRE(
        renormalized_simplified_evaluated_string("Factor[x^2 + 3*x + 2]", ctx) ==
        "(x + 1) * (x + 2)");
    REQUIRE(
        renormalized_simplified_evaluated_string("Collect[y*x + x^2 + z*x, x]", ctx) ==
        "x^2 + x * y + x * z");
}
