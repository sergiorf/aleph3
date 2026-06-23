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

std::string renormalized_simplified_evaluated_string(const std::string& source) {
    EvaluationContext ctx;
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
