#include "algebra/ExactPolynomialConversion.hpp"
#include "algebra/ExactPolynomialOps.hpp"
#include "expr/Expr.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>
#include <vector>

using namespace aleph3;

namespace {

ExactCoefficient coeff(const ExactPolynomial& polynomial, const Monomial& monomial) {
    const auto it = polynomial.terms.find(monomial);
    if (it == polynomial.terms.end()) return ExactCoefficient::zero();
    return it->second;
}

void require_coefficient(
    const ExactPolynomial& polynomial,
    const Monomial& monomial,
    int64_t numerator,
    int64_t denominator = 1) {
    REQUIRE(coeff(polynomial, monomial) == ExactCoefficient(numerator, denominator));
}

}  // namespace

TEST_CASE("Exact coefficients normalize signs and preserve rational arithmetic", "[algebra][exact]") {
    REQUIRE(ExactCoefficient(2, -4) == ExactCoefficient(-1, 2));
    REQUIRE(ExactCoefficient(-2, -4) == ExactCoefficient(1, 2));

    REQUIRE(ExactCoefficient(1, 6) + ExactCoefficient(1, 3) == ExactCoefficient(1, 2));
    REQUIRE(ExactCoefficient(5, 6) - ExactCoefficient(1, 3) == ExactCoefficient(1, 2));
    REQUIRE(ExactCoefficient(2, 3) * ExactCoefficient(9, 4) == ExactCoefficient(3, 2));
    REQUIRE(ExactCoefficient(2, 3) / ExactCoefficient(4, 9) == ExactCoefficient(3, 2));
}

TEST_CASE("Exact coefficients preserve values beyond native integer bounds", "[algebra][exact][large]") {
    const auto large_integer =
        kernel::ExactInteger::from_decimal_string("9223372036854775808");
    const auto larger_integer =
        kernel::ExactInteger::from_decimal_string("9223372036854775809");

    REQUIRE(
        ExactCoefficient(large_integer, kernel::ExactInteger(1)) +
        ExactCoefficient(1, 1) ==
        ExactCoefficient(larger_integer, kernel::ExactInteger(1)));
    REQUIRE(
        ExactCoefficient(3037000500LL, 1) * ExactCoefficient(3037000500LL, 1) ==
        ExactCoefficient(
            kernel::ExactInteger::from_decimal_string("9223372037000250000"),
            kernel::ExactInteger(1)));
}

TEST_CASE("Exact polynomial operations normalize zero and preserve rational terms", "[algebra][exact]") {
    const ExactPolynomial left({
        {Monomial{{"x", 1}}, ExactCoefficient(1, 3)},
        {Monomial{}, ExactCoefficient(1, 6)}
    });
    const ExactPolynomial right({
        {Monomial{{"x", 1}}, ExactCoefficient(2, 3)},
        {Monomial{}, ExactCoefficient(-1, 6)}
    });

    const ExactPolynomial sum = left + right;
    REQUIRE(sum.terms.size() == 1);
    require_coefficient(sum, Monomial{{"x", 1}}, 1);
    require_coefficient(sum, Monomial{}, 0);

    const ExactPolynomial product = left * right;
    require_coefficient(product, Monomial{{"x", 2}}, 2, 9);
    require_coefficient(product, Monomial{{"x", 1}}, 1, 18);
    require_coefficient(product, Monomial{}, -1, 36);
}

TEST_CASE("Exact polynomial division reconstructs supported dividend", "[algebra][exact][divide]") {
    const std::vector<std::string> variables{"x"};
    const ExactPolynomial dividend({
        {Monomial{{"x", 2}}, ExactCoefficient(1, 1)},
        {Monomial{}, ExactCoefficient(-1, 9)}
    });
    const ExactPolynomial divisor({
        {Monomial{{"x", 1}}, ExactCoefficient(1, 1)},
        {Monomial{}, ExactCoefficient(-1, 3)}
    });

    const auto [quotient, remainder] = divide(dividend, divisor, variables);

    require_coefficient(quotient, Monomial{{"x", 1}}, 1);
    require_coefficient(quotient, Monomial{}, 1, 3);
    REQUIRE(remainder.is_zero());
    REQUIRE((divisor * quotient + remainder).terms == dividend.terms);
}

TEST_CASE("Exact polynomial degree and leading term helpers inspect supported polynomials", "[algebra][exact][inspect]") {
    const ExactPolynomial polynomial({
        {Monomial{{"x", 3}}, ExactCoefficient(2, 3)},
        {Monomial{{"x", 1}}, ExactCoefficient(-1, 2)},
        {Monomial{}, ExactCoefficient(4, 1)}
    });

    REQUIRE(exact_degree_in_variable(polynomial, "x") == 3);
    REQUIRE(leading_coefficient_for_order(polynomial, {"x"}) == ExactCoefficient(2, 3));

    const ExactPolynomial constant(ExactCoefficient(7, 1));
    REQUIRE(exact_degree_in_variable(constant, "x") == 0);
    REQUIRE(leading_coefficient_for_order(constant, {"x"}) == ExactCoefficient(7, 1));
}

TEST_CASE("Exact monomial ordering follows explicit variable precedence", "[algebra][exact][order]") {
    const Monomial xy{{"x", 1}, {"y", 1}};
    const Monomial x2{{"x", 2}};
    const Monomial y2{{"y", 2}};

    REQUIRE(exact_monomial_precedes(x2, xy, MonomialOrder::graded_lexicographic, {"x", "y"}));
    REQUIRE(exact_monomial_precedes(y2, xy, MonomialOrder::graded_lexicographic, {"y", "x"}));
    REQUIRE_FALSE(exact_monomial_precedes(xy, x2, MonomialOrder::graded_lexicographic, {"x", "y"}));
}
