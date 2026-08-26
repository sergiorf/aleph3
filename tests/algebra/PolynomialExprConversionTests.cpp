#include "algebra/PolyUtils.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"
#include "parser/Parser.hpp"
#include "transforms/Transforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <limits>
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
    REQUIRE(simplify_string(round_trip) == "x^2 + 2/3 * x * y + 1/3");
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

    REQUIRE(simplify_string(exact_polynomial_to_expr(ordered)) == "x^2 + 3/2 * x * y - 1/2");
}

TEST_CASE("exact monomial ordering policies honor explicit variable precedence", "[algebra][ordering][exact]") {
    const Monomial x2y{{"x", 2}, {"y", 1}};
    const Monomial xy2{{"x", 1}, {"y", 2}};
    const Monomial z3{{"z", 3}};
    const std::vector<std::string> variables{"x", "y", "z"};

    REQUIRE(exact_monomial_precedes(x2y, xy2, MonomialOrder::lexicographic, variables));
    REQUIRE(exact_monomial_precedes(x2y, xy2, MonomialOrder::graded_lexicographic, variables));
    REQUIRE(exact_monomial_precedes(x2y, xy2, MonomialOrder::graded_reverse_lexicographic, variables));
    REQUIRE(exact_monomial_precedes(xy2, z3, MonomialOrder::graded_lexicographic, variables));
    REQUIRE_FALSE(exact_monomial_precedes(z3, xy2, MonomialOrder::graded_lexicographic, variables));
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

TEST_CASE("low-level exact polynomial gcd supports monomial-bounded multivariate inputs", "[algebra][conversion][exact][gcd]") {
    const auto left = expr_to_exact_polynomial(parse_expression("1/2*x*y"), {"x", "y"});
    const auto right = expr_to_exact_polynomial(parse_expression("x"), {"x", "y"});

    REQUIRE(simplify_string(exact_polynomial_to_expr(gcd(left, right, {"x", "y"}))) == "x");
    const auto [quotient, remainder] = divide(left, right, {"x", "y"});
    REQUIRE(simplify_string(exact_polynomial_to_expr(quotient)) == "1/2 * y");
    REQUIRE(simplify_string(exact_polynomial_to_expr(remainder)) == "0");
}

TEST_CASE("exact multivariate gcd uses shared monomial valuations", "[algebra][conversion][exact][gcd]") {
    const auto mixed = expr_to_exact_polynomial(parse_expression("x*y + x"), {"x", "y"});
    const auto x = expr_to_exact_polynomial(parse_expression("x"), {"x", "y"});
    REQUIRE(simplify_string(exact_polynomial_to_expr(gcd(mixed, x, {"x", "y"}))) == "x");

    const auto left = expr_to_exact_polynomial(parse_expression("1/2*x^2*y"), {"x", "y"});
    const auto right = expr_to_exact_polynomial(parse_expression("3/4*x*y^2"), {"x", "y"});
    REQUIRE(simplify_string(exact_polynomial_to_expr(gcd(left, right, {"x", "y"}))) == "x * y");
    REQUIRE(simplify_string(exact_polynomial_to_expr(gcd(left, right, {"y", "x"}))) == "x * y");
}

TEST_CASE("exact multivariate gcd normalizes zero and unit cases", "[algebra][conversion][exact][gcd]") {
    const auto zero = expr_to_exact_polynomial(parse_expression("0"), {"x", "y"});
    const auto polynomial = expr_to_exact_polynomial(parse_expression("2*x + 2*y"), {"x", "y"});
    const auto one = expr_to_exact_polynomial(parse_expression("1"), {"x", "y"});
    const auto monomial = expr_to_exact_polynomial(parse_expression("x*y"), {"x", "y"});

    REQUIRE(simplify_string(exact_polynomial_to_expr(gcd(zero, polynomial, {"x", "y"}))) == "x + y");
    REQUIRE(simplify_string(exact_polynomial_to_expr(gcd(one, monomial, {"x", "y"}))) == "1");
    REQUIRE_THROWS_AS(gcd(zero, zero, {"x", "y"}), EvaluatorError);
}

TEST_CASE("exact multivariate gcd rejects two multi-term operands and reports overflow", "[algebra][conversion][exact][gcd]") {
    const auto left = expr_to_exact_polynomial(parse_expression("x + y"), {"x", "y"});
    const auto right = expr_to_exact_polynomial(parse_expression("x - y"), {"x", "y"});
    try {
        static_cast<void>(gcd(left, right, {"x", "y"}));
        FAIL("Expected bounded multivariate GCD rejection");
    } catch (const EvaluatorError& error) {
        REQUIRE(error.kind() == EvaluatorErrorKind::unsupported_construct);
        REQUIRE(std::string(error.what()) ==
            "gcd: at least one multivariate operand must be a monomial");
    }

    const auto maximum = std::numeric_limits<int64_t>::max();
    const ExactPolynomial overflow_candidate({
        {Monomial{{"x", 1}}, ExactCoefficient(1, maximum)},
        {Monomial{}, ExactCoefficient(maximum, 1)}});
    REQUIRE_THROWS_AS(
        gcd(ExactPolynomial{}, overflow_candidate, {"x", "y"}),
        std::overflow_error);
}

TEST_CASE("exact coefficient arithmetic rejects overflow", "[algebra][conversion][exact][overflow]") {
    const ExactCoefficient large(std::numeric_limits<int64_t>::max(), 1);
    const ExactCoefficient small(std::numeric_limits<int64_t>::min(), 1);
    REQUIRE_THROWS_AS(large * ExactCoefficient(2, 1), std::overflow_error);
    REQUIRE_THROWS_AS(large + ExactCoefficient(1, 1), std::overflow_error);
    REQUIRE_THROWS_AS(small - ExactCoefficient(1, 1), std::overflow_error);
    REQUIRE_THROWS_AS(large / ExactCoefficient(1, 2), std::overflow_error);
}

TEST_CASE("exact polynomial denominator LCM rejects overflow", "[algebra][conversion][exact][overflow]") {
    const ExactPolynomial polynomial({
        {Monomial{{"x", 1}}, ExactCoefficient(1, std::numeric_limits<int64_t>::max())},
        {Monomial{}, ExactCoefficient(1, 2)}
    });

    REQUIRE_THROWS_AS(coefficient_denominator_lcm(polynomial), std::overflow_error);
}
