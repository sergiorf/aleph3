#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "algebra/ExactRationalExpression.hpp"
#include "kernel/Diagnostics.hpp"
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

template <typename Operation>
void require_exact_overflow(Operation&& operation) {
    try {
        operation();
        FAIL("Expected exact overflow");
    } catch (const kernel::RuntimeFailure& error) {
        REQUIRE(error.error().code == "runtime.exact_overflow");
        REQUIRE(std::string(error.what()) == "Exact coefficient overflow");
    }
}

template <typename Operation>
void require_runtime_diagnostic(
    Operation&& operation,
    std::string_view code,
    std::string_view message) {
    try {
        operation();
        FAIL("Expected runtime diagnostic");
    } catch (const kernel::RuntimeFailure& error) {
        REQUIRE(error.error().code == code);
        REQUIRE(std::string_view(error.what()) == message);
    }
}

std::vector<std::string> rational_restrictions(
    std::string_view source,
    const std::vector<std::string>& variables) {
    return exact_rational_expression_from_expr(
        parse_expression(std::string(source)),
        variables).restrictions.excluded_zero_strings();
}

std::vector<std::string> canceled_rational_restrictions(
    std::string_view source,
    const std::vector<std::string>& variables) {
    return cancel_exact_rational_expression(
        exact_rational_expression_from_expr(
            parse_expression(std::string(source)),
            variables)).restrictions.excluded_zero_strings();
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

TEST_CASE("Polynomial remainder exposes the supported division remainder", "[algebra][functions]") {
    EvaluationContext ctx;

    REQUIRE(simplify_string(evaluate_source("PolynomialRemainder[x^2 + 1, x + 1, x]", ctx)) == "2");
    REQUIRE(simplify_string(evaluate_source("PolynomialRemainder[x^2 - 1, x - 1, x]", ctx)) == "0");
    REQUIRE(simplify_string(evaluate_source("PolynomialRemainder[x^2 - 1/4, x - 1/2, x]", ctx)) == "0");
    REQUIRE(simplify_string(evaluate_source(
        "PolynomialRemainder[x^2*y + x*y^2 + y, x*y, {x, y}]", ctx)) == "y");

    try {
        static_cast<void>(evaluate_source("PolynomialRemainder[x*y, x]", ctx));
        FAIL("Expected PolynomialRemainder inference across multivariate input to reject");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(ex.what()) ==
            "divide: multivariate division requires an explicit variable selector");
    }
}

TEST_CASE("Polynomial inspection helpers return exact degree and leading coefficient", "[algebra][functions]") {
    EvaluationContext ctx;

    REQUIRE(to_string(*evaluate_source("PolynomialDegree[3*x^2 + 2*x + 1, x]", ctx)) == "2");
    REQUIRE(to_string(*evaluate_source("PolynomialDegree[7, x]", ctx)) == "0");
    REQUIRE(to_string(*evaluate_source("PolynomialDegree[(1/2)*x^3 + x, x]", ctx)) == "3");

    REQUIRE(to_string(*evaluate_source("LeadingCoefficient[3*x^2 + 2*x + 1, x]", ctx)) == "3");
    REQUIRE(to_string(*evaluate_source("LeadingCoefficient[(1/2)*x^2 + x, x]", ctx)) == "1/2");
    REQUIRE(to_string(*evaluate_source("LeadingCoefficient[7, x]", ctx)) == "7");
}

TEST_CASE("Polynomial inspection helpers reject zero and unsupported inputs explicitly", "[algebra][functions][diagnostics]") {
    EvaluationContext ctx;

    require_runtime_diagnostic(
        [&] { static_cast<void>(evaluate_source("PolynomialDegree[0, x]", ctx)); },
        "runtime.domain_violation",
        "PolynomialDegree of zero polynomial is undefined in the current subset");
    require_runtime_diagnostic(
        [&] { static_cast<void>(evaluate_source("LeadingCoefficient[0, x]", ctx)); },
        "runtime.domain_violation",
        "LeadingCoefficient of zero polynomial is undefined in the current subset");

    try {
        static_cast<void>(evaluate_source("PolynomialDegree[x*y + 1, x]", ctx));
        FAIL("Expected PolynomialDegree to reject unsupported multivariate input");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::invalid_form);
    }

    try {
        static_cast<void>(evaluate_source("LeadingCoefficient[0.5*x + 1, x]", ctx));
        FAIL("Expected LeadingCoefficient to reject inexact polynomial input");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
    }
}

TEST_CASE("Polynomial GCD supports bounded exact multivariate input", "[algebra][functions]") {
    EvaluationContext ctx;

    REQUIRE(simplify_string(evaluate_source("GCD[x*y + x, x, {x, y}]", ctx)) == "x");
    REQUIRE(simplify_string(evaluate_source("GCD[x^2*y, x*y^2, {x, y}]", ctx)) == "x * y");
    REQUIRE(simplify_string(evaluate_source("GCD[0, 2*x + 2*y, {x, y}]", ctx)) == "x + y");
    REQUIRE(simplify_string(evaluate_source("GCD[1, x*y, {x, y}]", ctx)) == "1");
    const auto division = evaluate_source("PolynomialQuotient[x*y, x, {x, y}]", ctx);
    const auto& parts = std::get<List>(*division).elements;
    REQUIRE(simplify_string(parts[0]) == "y");
    REQUIRE(simplify_string(parts[1]) == "0");
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
        REQUIRE(std::string(ex.what()) ==
            "gcd: multivariate GCD requires an explicit variable selector");
    }

    try {
        static_cast<void>(evaluate_source("PolynomialQuotient[x^2 - 1, y - 1]", ctx));
        FAIL("Expected PolynomialQuotient inference across mixed variables to reject unsupported multivariate input");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(ex.what()) ==
            "divide: multivariate division requires an explicit variable selector");
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

TEST_CASE("Polynomial algebra preserves exact rationals for supported helpers", "[algebra][functions][rational]") {
    EvaluationContext ctx;

    REQUIRE(to_string(*evaluate_source("Expand[(1/2) * (x + 1)]", ctx))
            == "1/2 * x + 1/2");
    REQUIRE(to_string(*evaluate_source("Expand[(1/2) * (x + y)]", ctx))
            == "1/2 * x + 1/2 * y");
    REQUIRE(to_string(*evaluate_source("Collect[(1/2) * x + 1, x]", ctx))
            == "1/2 * x + 1");
    REQUIRE(to_string(*evaluate_source("Collect[(1/2) * x * y + (3/2) * y, y]", ctx))
            == "1/2 * x * y + 3/2 * y");
    REQUIRE(simplify_string(evaluate_source("GCD[x^2 - 1/4, x - 1/2, x]", ctx))
            == "x - 1/2");

    const auto quotient_result =
        evaluate_source("PolynomialQuotient[x^2 - 1/4, x - 1/2, x]", ctx);
    REQUIRE(std::holds_alternative<List>(*quotient_result));
    const auto& result_list = std::get<List>(*quotient_result);
    REQUIRE(result_list.elements.size() == 2);
    REQUIRE(simplify_string(result_list.elements[0]) == "x + 1/2");
    REQUIRE(simplify_string(result_list.elements[1]) == "0");
}

TEST_CASE("Exact algebra dispatch keeps supported rational inputs on exact paths", "[algebra][functions][rational][dispatch]") {
    EvaluationContext ctx;

    REQUIRE(to_string(*evaluate_source("Expand[(1/3*x + 1/6) * 6]", ctx))
            == "2 * x + 1");
    REQUIRE(to_string(*evaluate_source("Expand[(1/2) * (x + y)]", ctx))
            == "1/2 * x + 1/2 * y");
    REQUIRE(to_string(*evaluate_source("Collect[(1/2)*x*y + (3/2)*y, y]", ctx))
            == "1/2 * x * y + 3/2 * y");
    REQUIRE(to_string(*evaluate_source("Collect[(1/3)*x*y + (2/3)*y, y]", ctx))
            == "1/3 * x * y + 2/3 * y");
    REQUIRE(simplify_string(evaluate_source("GCD[x^2 - 1/4, x - 1/2, x]", ctx))
            == "x - 1/2");
    REQUIRE(simplify_string(evaluate_source("GCD[x^2 - 1/9, x - 1/3, x]", ctx))
            == "x - 1/3");

    const auto quotient_result =
        evaluate_source("PolynomialQuotient[x^2 - 1/4, x - 1/2, x]", ctx);
    REQUIRE(std::holds_alternative<List>(*quotient_result));
    const auto& quotient_parts = std::get<List>(*quotient_result).elements;
    REQUIRE(quotient_parts.size() == 2);
    REQUIRE(simplify_string(quotient_parts[0]) == "x + 1/2");
    REQUIRE(simplify_string(quotient_parts[1]) == "0");

    const auto third_quotient_result =
        evaluate_source("PolynomialQuotient[x^2 - 1/9, x - 1/3, x]", ctx);
    REQUIRE(std::holds_alternative<List>(*third_quotient_result));
    const auto& third_quotient_parts = std::get<List>(*third_quotient_result).elements;
    REQUIRE(third_quotient_parts.size() == 2);
    REQUIRE(simplify_string(third_quotient_parts[0]) == "x + 1/3");
    REQUIRE(simplify_string(third_quotient_parts[1]) == "0");

    REQUIRE(to_string(*evaluate_source("Factor[(1/2)*x^2 + x + 1/2]", ctx))
            == "1/2 * (x + 1) * (x + 1)");
    REQUIRE(to_string(*evaluate_source("Factor[(1/3)*x^2 + (2/3)*x + 1/3]", ctx))
            == "1/3 * (x + 1) * (x + 1)");
    REQUIRE(simplify_string(evaluate_source("Together[1/2 + 1/x]", ctx))
            == "(x + 2) / (2 * x)");
    REQUIRE(simplify_string(evaluate_source("Together[1/3 + 1/x]", ctx))
            == "(x + 3) / (3 * x)");
    REQUIRE(simplify_string(evaluate_source("Cancel[(1/2*x)/(1/4)]", ctx))
            == "2 * x");
    REQUIRE(simplify_string(evaluate_source("Cancel[(1/3*x)/(1/9)]", ctx))
            == "3 * x");
}

TEST_CASE("Rational expression transformations preserve exact supported forms", "[algebra][functions][rational-expression]") {
    EvaluationContext ctx;

    REQUIRE(simplify_string(evaluate_source("Together[1/x + 1/y]", ctx)) == "(x + y) / (x * y)");
    REQUIRE(simplify_string(evaluate_source("Together[1/2 + 1/x]", ctx)) == "(x + 2) / (2 * x)");
    REQUIRE(simplify_string(evaluate_source("Together[x/(x + 1) + 1/(x + 1)]", ctx)) == "(x + 1) / (x + 1)");

    REQUIRE(simplify_string(evaluate_source("Cancel[(x^2 - 1)/(x - 1)]", ctx)) == "x + 1");
    REQUIRE(simplify_string(evaluate_source("Cancel[(1/2*x)/(1/4)]", ctx)) == "2 * x");
    REQUIRE(simplify_string(evaluate_source("Cancel[(x*y)/(x)]", ctx)) == "y");
}

TEST_CASE("Equivalent proves only the bounded exact algebra subset", "[algebra][functions][equivalence]") {
    EvaluationContext ctx;

    REQUIRE(simplify_string(evaluate_source("Equivalent[x + 1, 1 + x]", ctx)) == "True");
    REQUIRE(simplify_string(evaluate_source(
        "Equivalent[x^2 + 2*x + 1, x^2 + x + x + 1]", ctx)) == "True");
    REQUIRE(simplify_string(evaluate_source("Equivalent[x + 1, x + 2]", ctx)) == "False");
    REQUIRE(simplify_string(evaluate_source(
        "Equivalent[Sin[x]^2 + Cos[x]^2, 1]", ctx)) == "Unknown");
}

TEST_CASE("Equivalent preserves rational-expression domain boundaries", "[algebra][functions][equivalence][domain]") {
    EvaluationContext ctx;

    REQUIRE(simplify_string(evaluate_source(
        "Equivalent[(x^2 - 1)/(x - 1), x + 1]", ctx)) == "Unknown");
    REQUIRE(simplify_string(evaluate_source(
        "Equivalent[(x*y)/x, y]", ctx)) == "Unknown");
    REQUIRE(simplify_string(evaluate_source(
        "Equivalent[Cancel[(x*y)/x], y]", ctx)) == "True");
    REQUIRE(simplify_string(evaluate_source(
        "Equivalent[1/x, 1/x]", ctx)) == "True");

    require_runtime_diagnostic(
        [&] { static_cast<void>(evaluate_source("Equivalent[x/0, x]", ctx)); },
        "runtime.division_by_zero",
        "Rational expression denominator is zero");
}

TEST_CASE("Rational expression metadata records nonzero denominator restrictions", "[algebra][functions][rational-expression][domain]") {
    REQUIRE(rational_restrictions("x/(x + 1)", {"x"}) ==
            std::vector<std::string>{"x + 1"});
    REQUIRE(rational_restrictions("1/x + 1/y", {"x", "y"}) ==
            std::vector<std::string>{"x", "y"});
    REQUIRE(rational_restrictions("x/(1/y)", {"x", "y"}) ==
            std::vector<std::string>{"y"});
    REQUIRE(rational_restrictions("(x/(x + 1)) + (1/(x + 1))", {"x"}) ==
            std::vector<std::string>{"x + 1"});
}

TEST_CASE("Cancellation preserves excluded denominator metadata for removed factors", "[algebra][functions][rational-expression][domain]") {
    REQUIRE(canceled_rational_restrictions("(x^2 - 1)/(x - 1)", {"x"}) ==
            std::vector<std::string>{"x - 1"});
    REQUIRE(canceled_rational_restrictions("(x*y)/x", {"x", "y"}) ==
            std::vector<std::string>{"x"});

    const auto expression = cancel_exact_rational_expression(
        exact_rational_expression_from_expr(
            parse_expression("(x^2 - 1)/(x - 1)"),
            {"x"}));
    REQUIRE(simplify_string(exact_rational_expression_to_expr(expression)) == "x + 1");
}

TEST_CASE("Rational expression transformations reject unsupported and invalid inputs", "[algebra][functions][rational-expression]") {
    EvaluationContext ctx;

    try {
        static_cast<void>(evaluate_source("Together[0.5*x + 1/x]", ctx));
        FAIL("Expected Together to reject inexact rational-expression input");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
    }

    try {
        static_cast<void>(evaluate_source("Cancel[(x*y + x)/(x + 1)]", ctx));
        FAIL("Expected Cancel to reject unsupported multivariate cancellation");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
    }

    try {
        static_cast<void>(evaluate_source("Together[1/0 + x]", ctx));
        FAIL("Expected Together to reject unsupported denominator-zero input");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
    }
}

TEST_CASE("Inexact inputs do not enter exact multivariate algebra dispatch", "[algebra][functions][dispatch]") {
    EvaluationContext ctx;

    REQUIRE(to_string(*evaluate_source("Expand[0.5 * (x + y)]", ctx))
            == "0.5 * x + 0.5 * y");

    try {
        static_cast<void>(evaluate_source("Together[0.5*x + 1/x]", ctx));
        FAIL("Expected Together to reject inexact rational-expression input");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
    }

    try {
        static_cast<void>(evaluate_source("GCD[0.5*x*y, x, {x, y}]", ctx));
        FAIL("Expected inexact multivariate GCD to remain unsupported");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(ex.what()) ==
            "gcd: multivariate GCD requires exact polynomial coefficients");
    }

    try {
        static_cast<void>(evaluate_source("PolynomialQuotient[0.5*x*y, x, {x, y}]", ctx));
        FAIL("Expected inexact multivariate division to remain unsupported");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(ex.what()) ==
            "divide: multivariate division requires exact polynomial coefficients");
    }
}

TEST_CASE("Polynomial factor supports exact rational univariate coefficients", "[algebra][functions][rational]") {
    EvaluationContext ctx;

    REQUIRE(to_string(*evaluate_source("Expand[0.5 * (x + 1)]", ctx))
            == "0.5 * x + 0.5");

    REQUIRE(to_string(*evaluate_source("Factor[(1/2) * x^2 + x]", ctx))
            == "1/2 * x * (x + 2)");
    REQUIRE(to_string(*evaluate_source("Factor[(1/2) * x^2 + x + 1/2]", ctx))
            == "1/2 * (x + 1) * (x + 1)");
    REQUIRE(to_string(*evaluate_source("Factor[(-1/2) * x^2 + 1/2]", ctx))
            == "-1/2 * (x - 1) * (x + 1)");
    REQUIRE(to_string(*evaluate_source("Factor[(3/4) * x^2 + (3/2) * x + 3/4]", ctx))
            == "3/4 * (x + 1) * (x + 1)");
    REQUIRE(simplify_string(evaluate_source("Factor[(2/3) * x^3 - (2/3) * x]", ctx))
            == "x * 2/3 * (x - 1) * (x + 1)");

    try {
        static_cast<void>(evaluate_source("Factor[(1/2) * x * y + 1]", ctx));
        FAIL("Expected multivariate rational factorization rejection");
    } catch (const EvaluatorError& error) {
        REQUIRE(std::string(error.what()) ==
                "Factor does not support multivariate rational coefficients");
    }
}

TEST_CASE("Polynomial helpers map exact overflow to public diagnostics", "[algebra][functions][overflow]") {
    EvaluationContext ctx;

    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("Expand[(3037000500*x) * (3037000500*x)]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("Collect[(3037000500*x) * (3037000500*x), x]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("GCD[(3037000500*x) * (3037000500*x), 3037000500*x, x]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("PolynomialQuotient[3037000500*x, 1/3037000500, x]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("PolynomialRemainder[3037000500*x, 1/3037000500, x]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("PolynomialDegree[(3037000500*x) * (3037000500*x), x]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("LeadingCoefficient[(3037000500*x) * (3037000500*x), x]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("Coefficient[(3037000500*x) * (3037000500*x), x, 2]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("CoefficientList[(3037000500*x) * (3037000500*x), x]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("Factor[(3037000500*x) * (3037000500*x)]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("Numerator[1/3037000500 + 1/3037000501]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("Denominator[1/3037000500 + 1/3037000501]", ctx));
    });
    require_exact_overflow([&] {
        static_cast<void>(evaluate_source("Together[1/3037000500 + 1/3037000501]", ctx));
    });
}

TEST_CASE("Polynomial helpers map exact division by zero to public diagnostics", "[algebra][functions][diagnostics]") {
    EvaluationContext ctx;

    require_runtime_diagnostic(
        [&] { static_cast<void>(evaluate_source("PolynomialQuotient[x, 0, x]", ctx)); },
        "runtime.division_by_zero",
        "Polynomial division by zero");
    require_runtime_diagnostic(
        [&] { static_cast<void>(evaluate_source("PolynomialRemainder[x, 0, x]", ctx)); },
        "runtime.division_by_zero",
        "Polynomial division by zero");
    require_runtime_diagnostic(
        [&] { static_cast<void>(evaluate_source("Cancel[x/0]", ctx)); },
        "runtime.division_by_zero",
        "Rational expression denominator is zero");
}

TEST_CASE("Polynomial helpers preserve documented round-trip invariants", "[algebra][functions][contract]") {
    EvaluationContext ctx;

    const auto expanded = evaluate_source("Expand[(x + 1) * (x + 2)]", ctx);
    const auto recollected = evaluate_source("Collect[x^2 + 3*x + 2, x]", ctx);
    REQUIRE(simplify_string(expanded) == simplify_string(recollected));

    const auto factored = evaluate_source("Factor[x^3 - x]", ctx);
    const auto refolded = evaluate_source("Expand[x * (x - 1) * (x + 1)]", ctx);
    REQUIRE(simplify_string(refolded) == "x^3 - x");
    REQUIRE(simplify_string(factored) == "x * (x - 1) * (x + 1)");
}

TEST_CASE("Polynomial quotient and gcd satisfy documented consistency identities", "[algebra][functions][contract]") {
    EvaluationContext ctx;

    const auto quotient_result =
        evaluate_source("PolynomialQuotient[x^3 - x, x - 1, x]", ctx);
    REQUIRE(std::holds_alternative<List>(*quotient_result));
    const auto& result_list = std::get<List>(*quotient_result);
    REQUIRE(result_list.elements.size() == 2);
    REQUIRE(simplify_string(result_list.elements[0]) == "x^2 + x");
    REQUIRE(simplify_string(result_list.elements[1]) == "0");

    const auto reconstructed =
        evaluate_source("Expand[(x - 1) * (x^2 + x) + 0]", ctx);
    REQUIRE(simplify_string(reconstructed) == "x^3 - x");

    const auto gcd_result = evaluate_source("GCD[x^3 - x, x^2 - x, x]", ctx);
    REQUIRE(simplify_string(gcd_result) == "x^2 - x");
}

TEST_CASE("Polynomial outputs normalize zero one and sign cases deterministically", "[algebra][functions][contract]") {
    EvaluationContext ctx;

    REQUIRE(simplify_string(evaluate_source("Factor[0]", ctx)) == "0");
    REQUIRE(simplify_string(evaluate_source("Factor[1]", ctx)) == "1");
    REQUIRE(simplify_string(evaluate_source("Factor[(-2)*x^2 + (-4)*x]", ctx)) ==
            "-2 * x * (x + 2)");
    REQUIRE(simplify_string(evaluate_source("Expand[(y + x) * (x + z)]", ctx)) ==
            "x^2 + x * y + x * z + y * z");
}

TEST_CASE("Polynomial helpers keep multivariate support boundaries explicit", "[algebra][functions][contract]") {
    EvaluationContext ctx;

    REQUIRE(simplify_string(evaluate_source("Expand[(x + y) * (x + z)]", ctx)) ==
            "x^2 + x * y + x * z + y * z");
    REQUIRE(simplify_string(evaluate_source("Expand[(1/2) * (y + x)]", ctx)) ==
            "x * 1/2 + y * 1/2");
    REQUIRE(simplify_string(evaluate_source("Collect[x*y + y*z, y]", ctx)) ==
            "x * y + y * z");
    REQUIRE(simplify_string(evaluate_source("Collect[(3/2) * y + (1/2) * x * y, y]", ctx)) ==
            "x * y * 1/2 + y * 3/2");
    REQUIRE(simplify_string(evaluate_source("Factor[x*y + y*z]", ctx)) ==
            "y * (x + z)");

    REQUIRE(simplify_string(evaluate_source("GCD[x*y, x, {x, y}]", ctx)) == "x");

    try {
        static_cast<void>(evaluate_source("GCD[x + y, x - y, {x, y}]", ctx));
        FAIL("Expected two multi-term multivariate operands to remain unsupported");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(ex.what()) ==
            "gcd: at least one multivariate operand must be a monomial");
    }

    try {
        static_cast<void>(evaluate_source("GCD[0.5*x*y, x, {x, y}]", ctx));
        FAIL("Expected inexact multivariate GCD to remain unsupported");
    } catch (const EvaluatorError& ex) {
        REQUIRE(ex.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(ex.what()) ==
            "gcd: multivariate GCD requires exact polynomial coefficients");
    }

    const auto division = evaluate_source(
        "PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]", ctx);
    const auto& parts = std::get<List>(*division).elements;
    REQUIRE(simplify_string(parts[0]) == "x + y");
    REQUIRE(simplify_string(parts[1]) == "y");
    REQUIRE(simplify_string(evaluate_source(
        "Expand[x*y*(x+y)+y]", ctx)) == "x^2 * y + x * y^2 + y");
    REQUIRE(simplify_string(evaluate_source(
        "PolynomialRemainder[x^2*y + x*y^2 + y, x*y, {x, y}]", ctx)) == "y");

    REQUIRE_THROWS_AS(
        evaluate_source("PolynomialQuotient[0.5*x*y, x, {x, y}]", ctx),
        EvaluatorError);
}
