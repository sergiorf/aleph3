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

TEST_CASE("Symbolic CLI support preserves evaluator fallback contracts", "[tooling][symbolic-cli]") {
    const auto symbolic_if = tooling::symbolic_evaluate_expression("If[x, 1, 2]");
    REQUIRE(symbolic_if.ok);
    REQUIRE(symbolic_if.output == "If[x, 1, 2]");

    const auto unknown_call = tooling::symbolic_evaluate_expression("mystery[2 + 3]");
    REQUIRE(unknown_call.ok);
    REQUIRE(unknown_call.output == "mystery[2 + 3]");
}

TEST_CASE("Symbolic CLI support prints FullForm", "[tooling][symbolic-cli]") {
    const auto result = tooling::symbolic_fullform_expression("x^2 + 1");

    REQUIRE(result.ok);
    REQUIRE(result.output == "Plus[Power[x, 2], 1]");
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
