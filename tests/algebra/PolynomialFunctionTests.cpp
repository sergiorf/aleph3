#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "parser/Parser.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "transforms/Transforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using namespace aleph3;

namespace {

ExprPtr evaluate_source(std::string_view source, EvaluationContext& ctx) {
    auto expr = parse_expression(std::string(source));
    return evaluate(expr, ctx);
}

std::string simplify_string(const ExprPtr& expr) {
    return to_string(simplify(expr));
}

}  // namespace

TEST_CASE("Polynomial functions expand and collect to stable polynomial forms", "[algebra][functions]") {
    EvaluationContext ctx;

    const auto expanded = evaluate_source("Expand[(x + 1) * (x + 2)]", ctx);
    REQUIRE(simplify_string(expanded) == "x^2 + 3 * x + 2");

    const auto multivariate_expanded = evaluate_source("Expand[(x + y) * (x - y)]", ctx);
    REQUIRE(simplify_string(multivariate_expanded) == "-(y^2) + x^2");

    ctx.variables["x"] = make_expr<Number>(99.0);
    const auto collected = evaluate_source("Collect[x^2 + 2*x + 1, x]", ctx);
    REQUIRE(simplify_string(collected) == "x^2 + 2 * x + 1");

    const auto collected_with_list = evaluate_source("Collect[x^2 + 2*x + 1, {x}]", ctx);
    REQUIRE(simplify_string(collected_with_list) == "x^2 + 2 * x + 1");

    const auto collected_symbolic_coefficients = evaluate_source("Collect[y*x + x^2 + z*x, x]", ctx);
    REQUIRE(simplify_string(collected_symbolic_coefficients) == "x^2 + x * y + x * z");

    const auto collected_constant_coefficient = evaluate_source("Collect[x^2 + y, x]", ctx);
    REQUIRE(simplify_string(collected_constant_coefficient) == "x^2 + y");

    const auto collected_symbolic_coefficients_with_list =
        evaluate_source("Collect[y*x + x^2 + z*x, {x}]", ctx);
    REQUIRE(simplify_string(collected_symbolic_coefficients_with_list) == "x^2 + x * y + x * z");

    const auto collected_duplicate_selector =
        evaluate_source("Collect[y*x + x^2 + z*x, {x, x}]", ctx);
    REQUIRE(simplify_string(collected_duplicate_selector) == "x^2 + x * y + x * z");
}

TEST_CASE("Polynomial GCD supports an explicit variable selector", "[algebra][functions]") {
    EvaluationContext ctx;

    const auto gcd_result = evaluate_source("GCD[x^2 - 1, x - 1, x]", ctx);
    REQUIRE(simplify_string(gcd_result) == "x - 1");

    const auto inferred_gcd_result = evaluate_source("GCD[x^2 - 1, x - 1]", ctx);
    REQUIRE(simplify_string(inferred_gcd_result) == "x - 1");
}

TEST_CASE("Polynomial quotient returns quotient and remainder", "[algebra][functions]") {
    EvaluationContext ctx;

    const auto quotient_result = evaluate_source("PolynomialQuotient[x^2 - 1, x - 1, x]", ctx);
    REQUIRE(std::holds_alternative<List>(*quotient_result));

    const auto& result_list = std::get<List>(*quotient_result);
    REQUIRE(result_list.elements.size() == 2);
    REQUIRE(simplify_string(result_list.elements[0]) == "x + 1");
    REQUIRE(simplify_string(result_list.elements[1]) == "0");

    const auto inferred_result = evaluate_source("PolynomialQuotient[x^2 - 1, x - 1]", ctx);
    REQUIRE(std::holds_alternative<List>(*inferred_result));
    const auto& inferred_list = std::get<List>(*inferred_result);
    REQUIRE(inferred_list.elements.size() == 2);
    REQUIRE(simplify_string(inferred_list.elements[0]) == "x + 1");
    REQUIRE(simplify_string(inferred_list.elements[1]) == "0");
}

TEST_CASE("Polynomial quotient preserves nonzero remainders", "[algebra][functions]") {
    EvaluationContext ctx;

    const auto quotient_result = evaluate_source("PolynomialQuotient[x^2 + 1, x + 1, x]", ctx);
    REQUIRE(std::holds_alternative<List>(*quotient_result));

    const auto& result_list = std::get<List>(*quotient_result);
    REQUIRE(result_list.elements.size() == 2);
    REQUIRE(simplify_string(result_list.elements[0]) == "x - 1");
    REQUIRE(simplify_string(result_list.elements[1]) == "2");
}

TEST_CASE("Polynomial GCD and division reject unsupported multivariate inference", "[algebra][functions]") {
    EvaluationContext ctx;

    REQUIRE_THROWS_AS(
        evaluate_source("GCD[x*y, x, {x, y}]", ctx),
        std::runtime_error);
    REQUIRE_THROWS_AS(
        evaluate_source("PolynomialQuotient[x*y, x, {x, y}]", ctx),
        std::runtime_error);
}

TEST_CASE("Polynomial selectors reject empty lists and infer variables from both operands", "[algebra][functions]") {
    EvaluationContext ctx;

    try {
        static_cast<void>(evaluate_source("Collect[x^2 + y, {}]", ctx));
        FAIL("Expected Collect to reject an empty selector list");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::invalid_form);
        REQUIRE(std::string(ex.what()) == "Variable list must not be empty");
    }

    try {
        static_cast<void>(evaluate_source("GCD[x^2 - 1, y - 1]", ctx));
        FAIL("Expected GCD inference across mixed variables to reject unsupported multivariate input");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(ex.what()) == "gcd: only univariate GCD is implemented");
    }

    try {
        static_cast<void>(evaluate_source("PolynomialQuotient[x^2 - 1, y - 1]", ctx));
        FAIL("Expected PolynomialQuotient inference across mixed variables to reject unsupported multivariate input");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(ex.what()) == "divide: only univariate division is implemented");
    }
}

TEST_CASE("Polynomial factor supports supported content and linear-root factorization", "[algebra][functions]") {
    EvaluationContext ctx;

    const auto factor_result = evaluate_source("Factor[x^2 + 3*x + 2]", ctx);
    REQUIRE(simplify_string(factor_result) == "(x + 1) * (x + 2)");

    const auto repeated_factor_result = evaluate_source("Factor[x^2 - 2*x + 1]", ctx);
    REQUIRE(simplify_string(repeated_factor_result) == "(x - 1) * (x - 1)");

    const auto multivariate_factor_result = evaluate_source("Factor[x^2*y + 2*x*y]", ctx);
    REQUIRE(simplify_string(multivariate_factor_result) == "x * y * (x + 2)");

    const auto signed_content_factor_result =
        evaluate_source("Factor[(-2)*x^2 + (-4)*x]", ctx);
    REQUIRE(simplify_string(signed_content_factor_result) == "-2 * x * (x + 2)");

    const auto irreducible_factor_result = evaluate_source("Factor[x^2 + x + 1]", ctx);
    REQUIRE(simplify_string(irreducible_factor_result) == "x^2 + x + 1");
}

TEST_CASE("Polynomial factor handles zero constant and sparse high degree inputs", "[algebra][functions]") {
    EvaluationContext ctx;

    const auto zero_result = evaluate_source("Factor[0]", ctx);
    REQUIRE(simplify_string(zero_result) == "0");

    const auto constant_result = evaluate_source("Factor[7]", ctx);
    REQUIRE(simplify_string(constant_result) == "7");

    const auto sparse_result = evaluate_source("Factor[x^5 - x^3]", ctx);
    REQUIRE(simplify_string(sparse_result) == "x^3 * (x - 1) * (x + 1)");

    const auto mixed_multivariate_content_result = evaluate_source("Factor[x*y + y*z]", ctx);
    REQUIRE(simplify_string(mixed_multivariate_content_result) == "y * (x + z)");

    const auto rational_root_result = evaluate_source("Factor[2*x^2 - 3*x + 1]", ctx);
    REQUIRE(simplify_string(rational_root_result) == "(x - 1) * (2 * x - 1)");
}

TEST_CASE("Polynomial factor rejects unsupported non integer univariate coefficients", "[algebra][functions]") {
    EvaluationContext ctx;

    try {
        static_cast<void>(evaluate_source("Factor[0.5*x^2 + 1.5*x + 1]", ctx));
        FAIL("Expected factorization to reject non-integer coefficients");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(ex.what()) == "Polynomial factorization currently requires integer coefficients");
    }
}

TEST_CASE("Polynomial algebra rejects exact rational coefficients explicitly", "[algebra][functions][rational]") {
    EvaluationContext ctx;

    const std::vector<std::string> inputs = {
        "Expand[(1/2) * (x + 1)]",
        "Collect[(1/2) * x + 1, x]",
        "Factor[(1/2) * x^2 + x]"
    };

    for (const auto& input : inputs) {
        DYNAMIC_SECTION(input) {
            try {
                static_cast<void>(evaluate_source(input, ctx));
                FAIL("Expected exact rational polynomial coefficient rejection");
            } catch (const EvaluatorError& ex) {
                REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
                REQUIRE(std::string(ex.what()) ==
                        "Polynomial functions do not yet support exact rational coefficients");
            }
        }
    }
}
