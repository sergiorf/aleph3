#include "tooling/SymbolicCliSupport.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("Symbolic CLI support evaluates polynomial expressions", "[tooling][symbolic-cli]") {
    const auto result =
        tooling::symbolic_evaluate_expression("Expand[(x + 1) * (x + 2)]");

    REQUIRE(result.ok);
    REQUIRE(result.output == "x^2 + 3 * x + 2");
}

TEST_CASE("Symbolic CLI support exposes polynomial factoring", "[tooling][symbolic-cli]") {
    const auto result =
        tooling::symbolic_evaluate_expression("Factor[x^2 + 3*x + 2]");

    REQUIRE(result.ok);
    REQUIRE(result.output == "(x + 1) * (x + 2)");
}

TEST_CASE("Symbolic CLI support canonicalizes linear factor output", "[tooling][symbolic-cli]") {
    const auto result =
        tooling::symbolic_evaluate_expression("Factor[x^5 - x^3]");

    REQUIRE(result.ok);
    REQUIRE(result.output == "x^3 * (x - 1) * (x + 1)");
}

TEST_CASE("Symbolic CLI support keeps rational roots in exact linear factors", "[tooling][symbolic-cli]") {
    const auto result =
        tooling::symbolic_evaluate_expression("Factor[2*x^2 - 3*x + 1]");

    REQUIRE(result.ok);
    REQUIRE(result.output == "(x - 1) * (2 * x - 1)");
}

TEST_CASE("Symbolic CLI support reports factor subset failures", "[tooling][symbolic-cli]") {
    const auto result =
        tooling::symbolic_evaluate_expression("Factor[0.5*x^2 + 1.5*x + 1]");

    REQUIRE_FALSE(result.ok);
    REQUIRE(result.error_message == "Polynomial factorization currently requires integer coefficients");
}

TEST_CASE("Symbolic CLI support simplifies symbolic expressions", "[tooling][symbolic-cli]") {
    const auto result = tooling::symbolic_simplify_expression("0 + (1 * x)");

    REQUIRE(result.ok);
    REQUIRE(result.output == "x");
}

TEST_CASE("Symbolic CLI support hardens nested algebraic simplification", "[tooling][symbolic-cli]") {
    const auto cancelled = tooling::symbolic_simplify_expression("x + (-1 * x)");
    REQUIRE(cancelled.ok);
    REQUIRE(cancelled.output == "0");

    const auto flattened = tooling::symbolic_simplify_expression("x + (y + 2) + 3");
    REQUIRE(flattened.ok);
    REQUIRE(flattened.output == "5 + x + y");

    const auto merged_powers = tooling::symbolic_simplify_expression("x * x * y");
    REQUIRE(merged_powers.ok);
    REQUIRE(merged_powers.output == "x^2 * y");
}

TEST_CASE("Symbolic CLI support preserves evaluator fallback contracts", "[tooling][symbolic-cli]") {
    const auto symbolic_if = tooling::symbolic_evaluate_expression("If[x, 1, 2]");
    REQUIRE(symbolic_if.ok);
    REQUIRE(symbolic_if.output == "If[x, 1, 2]");

    const auto unknown_call = tooling::symbolic_evaluate_expression("mystery[2 + 3]");
    REQUIRE(unknown_call.ok);
    REQUIRE(unknown_call.output == "mystery[2 + 3]");
}

TEST_CASE("Symbolic CLI support preserves special form laziness and malformed If failures", "[tooling][symbolic-cli]") {
    const auto lazy_if = tooling::symbolic_evaluate_expression("If[True, If[False, 1/0, 9], 1/0]");
    REQUIRE(lazy_if.ok);
    REQUIRE(lazy_if.output == "9");

    const auto missing_branch = tooling::symbolic_evaluate_expression("If[True, 1]");
    REQUIRE_FALSE(missing_branch.ok);
    REQUIRE_FALSE(missing_branch.error_message.empty());

    const auto extra_branch = tooling::symbolic_evaluate_expression("If[True, 1, 2, 3]");
    REQUIRE_FALSE(extra_branch.ok);
    REQUIRE_FALSE(extra_branch.error_message.empty());
}

TEST_CASE("Symbolic CLI support prints FullForm", "[tooling][symbolic-cli]") {
    const auto result = tooling::symbolic_fullform_expression("x^2 + 1");

    REQUIRE(result.ok);
    REQUIRE(result.output == "Plus[Power[x, 2], 1]");
}

TEST_CASE("Symbolic CLI support preserves builtin numeric and symbolic contracts", "[tooling][symbolic-cli]") {
    const auto exact_sum = tooling::symbolic_evaluate_expression("1/2 + 2");
    REQUIRE(exact_sum.ok);
    REQUIRE(exact_sum.output == "5/2");

    const auto rational_comparison = tooling::symbolic_evaluate_expression("1/2 < 3/4");
    REQUIRE(rational_comparison.ok);
    REQUIRE(rational_comparison.output == "True");

    const auto symbolic_fallback = tooling::symbolic_evaluate_expression("ArcSin[2]");
    REQUIRE(symbolic_fallback.ok);
    REQUIRE(symbolic_fallback.output == "ArcSin[2]");
}

TEST_CASE("Symbolic CLI support reports parse and evaluation failures", "[tooling][symbolic-cli]") {
    const auto parse_failure = tooling::symbolic_evaluate_expression("Expand[(x + 1]");
    REQUIRE_FALSE(parse_failure.ok);
    REQUIRE_FALSE(parse_failure.error_message.empty());

    const auto eval_failure = tooling::symbolic_evaluate_expression("Collect[x^2 + y, x]");
    REQUIRE_FALSE(eval_failure.ok);
    REQUIRE_FALSE(eval_failure.error_message.empty());

    const auto selector_failure = tooling::symbolic_evaluate_expression("Collect[x^2 + 1, 3]");
    REQUIRE_FALSE(selector_failure.ok);
    REQUIRE(selector_failure.error_message == "Variable argument must be a symbol or list of symbols");
}
