#include "algebra/PolyUtils.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"
#include "parser/Parser.hpp"
#include "transforms/Transforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <vector>

using namespace aleph3;

namespace {

std::string simplify_string(const ExprPtr& expr) {
    return to_string(simplify(expr));
}

ExactCoefficient coeff(int64_t numerator, int64_t denominator = 1) {
    return ExactCoefficient(numerator, denominator);
}

}  // namespace

TEST_CASE("expr_to_polynomial converts constants and simple symbolic terms", "[algebra][conversion]") {
    const auto constant = expr_to_polynomial(parse_expression("5"), {"x"});
    REQUIRE(constant.terms.size() == 1);
    REQUIRE(constant.terms.begin()->first.empty());
    REQUIRE(constant.terms.begin()->second == 5.0);

    const auto symbol = expr_to_polynomial(parse_expression("x"), {"x"});
    REQUIRE(symbol.terms.size() == 1);
    REQUIRE(symbol.terms.begin()->first.at("x") == 1);
    REQUIRE(symbol.terms.begin()->second == 1.0);
}

TEST_CASE("expr_to_polynomial converts multivariate sums products and powers", "[algebra][conversion]") {
    const auto poly = expr_to_polynomial(parse_expression("3*x^2*y + 2*y"), {"x", "y"});

    REQUIRE(poly.terms.size() == 2);
    REQUIRE(poly.terms.at(Monomial{{"x", 2}, {"y", 1}}) == 3.0);
    REQUIRE(poly.terms.at(Monomial{{"y", 1}}) == 2.0);
}

TEST_CASE("expr_to_polynomial rejects unsupported symbolic calls", "[algebra][conversion]") {
    REQUIRE_THROWS_AS(
        expr_to_polynomial(parse_expression("Sin[x]"), {"x"}),
        std::runtime_error);

    try {
        static_cast<void>(expr_to_polynomial(parse_expression("Sin[x]"), {"x"}));
        FAIL("Expected unsupported-construct error");
    } catch (const EvaluatorError& err) {
        REQUIRE(err.kind() == EvaluatorErrorKind::unsupported_construct);
    }
}

TEST_CASE("expr_to_polynomial rejects symbols outside the selected variable set", "[algebra][conversion]") {
    REQUIRE_THROWS_AS(
        expr_to_polynomial(parse_expression("x + y"), {"x"}),
        std::runtime_error);
}

TEST_CASE("expr_to_polynomial rejects non-polynomial powers", "[algebra][conversion]") {
    REQUIRE_THROWS_AS(
        expr_to_polynomial(parse_expression("x^0.5"), {"x"}),
        std::runtime_error);
    REQUIRE_THROWS_AS(
        expr_to_polynomial(parse_expression("x^-1"), {"x"}),
        std::runtime_error);
}

TEST_CASE("polynomial_to_expr preserves zero constant and multivariate forms", "[algebra][conversion]") {
    REQUIRE(simplify_string(polynomial_to_expr(Polynomial{})) == "0");
    REQUIRE(simplify_string(polynomial_to_expr(Polynomial(7.0))) == "7");

    Polynomial multivariate({
        {Monomial{{"x", 2}, {"y", 1}}, 3.0},
        {Monomial{}, -1.0}
    });
    REQUIRE(simplify_string(polynomial_to_expr(multivariate)) == "3 * x^2 * y - 1");
}

TEST_CASE("polynomial conversion round-trips to a stable canonical form", "[algebra][conversion]") {
    const auto expr = parse_expression("1 + 2*x + x^2");
    const auto poly = expr_to_polynomial(expr, {"x"});
    const auto round_trip = polynomial_to_expr(poly);
    REQUIRE(simplify_string(round_trip) == "x^2 + 2 * x + 1");
}

TEST_CASE("exact polynomial conversion preserves multivariate rational coefficients", "[algebra][conversion][exact]") {
    const auto poly = expr_to_exact_polynomial(parse_expression("3/2*x^2*y + 2/3*y"), {"x", "y"});

    REQUIRE(poly.terms.size() == 2);
    REQUIRE(poly.terms.at(Monomial{{"x", 2}, {"y", 1}}) == coeff(3, 2));
    REQUIRE(poly.terms.at(Monomial{{"y", 1}}) == coeff(2, 3));
}

TEST_CASE("exact polynomial conversion round-trips to a stable canonical form", "[algebra][conversion][exact]") {
    const auto expr = parse_expression("1/3 + 2/3*x*y + x^2");
    const auto poly = expr_to_exact_polynomial(expr, {"x", "y"});
    const auto round_trip = exact_polynomial_to_expr(poly);
    REQUIRE(simplify_string(round_trip) == "x^2 + x * y * 2/3 + 1/3");
}

TEST_CASE("polynomial_to_expr emits higher-degree terms before lower-degree ones", "[algebra][conversion]") {
    Polynomial ordered({
        {Monomial{}, 5.0},
        {Monomial{{"x", 1}}, 2.0},
        {Monomial{{"x", 3}}, 1.0}
    });

    REQUIRE(simplify_string(polynomial_to_expr(ordered)) == "x^3 + 2 * x + 5");
}

TEST_CASE("exact polynomial_to_expr emits a shared canonical order", "[algebra][conversion][exact]") {
    ExactPolynomial ordered({
        {Monomial{}, coeff(-1, 2)},
        {Monomial{{"x", 1}, {"y", 1}}, coeff(3, 2)},
        {Monomial{{"x", 2}}, coeff(1)}
    });

    REQUIRE(simplify_string(exact_polynomial_to_expr(ordered)) == "x^2 + x * y * 3/2 - 1/2");
}

TEST_CASE("low-level polynomial helpers preserve supported normal forms", "[algebra][conversion]") {
    const auto poly = expr_to_polynomial(parse_expression("x^2 + 2*x + 1"), {"x"});

    REQUIRE(expand(poly).terms == poly.terms);
    REQUIRE(collect(poly, {"x"}).terms == poly.terms);
}

TEST_CASE("low-level exact polynomial helpers preserve supported normal forms", "[algebra][conversion][exact]") {
    const auto poly = expr_to_exact_polynomial(parse_expression("1/2*x*y + 3/2*y"), {"x", "y"});

    REQUIRE(expand(poly).terms == poly.terms);
    REQUIRE(collect(poly, {"y"}).terms == poly.terms);
}

TEST_CASE("low-level polynomial gcd and divide honor explicit variable selection", "[algebra][conversion]") {
    const auto left = expr_to_polynomial(parse_expression("x^2 - 1"), {"x"});
    const auto right = expr_to_polynomial(parse_expression("x - 1"), {"x"});

    const auto divisor_gcd = gcd(left, right, {"x"});
    REQUIRE(simplify_string(polynomial_to_expr(divisor_gcd)) == "x - 1");

    const auto [quotient, remainder] = divide(left, right, {"x"});
    REQUIRE(simplify_string(polynomial_to_expr(quotient)) == "x + 1");
    REQUIRE(simplify_string(polynomial_to_expr(remainder)) == "0");
}

TEST_CASE("low-level exact polynomial gcd and divide honor explicit variable selection", "[algebra][conversion][exact]") {
    const auto left = expr_to_exact_polynomial(parse_expression("x^2 - 1/4"), {"x"});
    const auto right = expr_to_exact_polynomial(parse_expression("x - 1/2"), {"x"});

    const auto divisor_gcd = gcd(left, right, {"x"});
    REQUIRE(simplify_string(exact_polynomial_to_expr(divisor_gcd)) == "x - 1/2");

    const auto [quotient, remainder] = divide(left, right, {"x"});
    REQUIRE(simplify_string(exact_polynomial_to_expr(quotient)) == "x + 1/2");
    REQUIRE(simplify_string(exact_polynomial_to_expr(remainder)) == "0");
}

TEST_CASE("low-level polynomial gcd and divide reject multivariate selectors", "[algebra][conversion]") {
    const auto left = expr_to_polynomial(parse_expression("x*y"), {"x", "y"});
    const auto right = expr_to_polynomial(parse_expression("x"), {"x", "y"});

    REQUIRE_THROWS_AS(gcd(left, right, {"x", "y"}), std::runtime_error);
    REQUIRE_THROWS_AS(divide(left, right, {"x", "y"}), std::runtime_error);

    try {
        static_cast<void>(gcd(left, right, {"x", "y"}));
        FAIL("Expected unsupported-construct error");
    } catch (const EvaluatorError& err) {
        REQUIRE(err.kind() == EvaluatorErrorKind::unsupported_construct);
    }
}

TEST_CASE("low-level exact polynomial conversion rejects inexact inputs and unsupported calls", "[algebra][conversion][exact]") {
    REQUIRE_THROWS_AS(
        expr_to_exact_polynomial(parse_expression("0.5*x"), {"x"}),
        std::runtime_error);
    REQUIRE_THROWS_AS(
        expr_to_exact_polynomial(parse_expression("Sin[x]"), {"x"}),
        std::runtime_error);
}

TEST_CASE("low-level exact polynomial gcd and divide reject multivariate selectors", "[algebra][conversion][exact]") {
    const auto left = expr_to_exact_polynomial(parse_expression("1/2*x*y"), {"x", "y"});
    const auto right = expr_to_exact_polynomial(parse_expression("x"), {"x", "y"});

    REQUIRE_THROWS_AS(gcd(left, right, {"x", "y"}), std::runtime_error);
    REQUIRE_THROWS_AS(divide(left, right, {"x", "y"}), std::runtime_error);
}
