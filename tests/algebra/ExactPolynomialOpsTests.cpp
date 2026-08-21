#include "algebra/ExactPolynomialConversion.hpp"
#include "algebra/ExactPolynomialOps.hpp"
#include "expr/Expr.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
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

TEST_CASE("Exact coefficients detect overflow instead of wrapping", "[algebra][exact][overflow]") {
    REQUIRE_THROWS_AS(
        ExactCoefficient(std::numeric_limits<int64_t>::max(), 1) + ExactCoefficient(1, 1),
        std::overflow_error);
    REQUIRE_THROWS_AS(
        ExactCoefficient(3037000500LL, 1) * ExactCoefficient(3037000500LL, 1),
        std::overflow_error);
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

TEST_CASE("Exact monomial ordering follows explicit variable precedence", "[algebra][exact][order]") {
    const Monomial xy{{"x", 1}, {"y", 1}};
    const Monomial x2{{"x", 2}};
    const Monomial y2{{"y", 2}};

    REQUIRE(exact_monomial_precedes(x2, xy, MonomialOrder::graded_lexicographic, {"x", "y"}));
    REQUIRE(exact_monomial_precedes(y2, xy, MonomialOrder::graded_lexicographic, {"y", "x"}));
    REQUIRE_FALSE(exact_monomial_precedes(xy, x2, MonomialOrder::graded_lexicographic, {"x", "y"}));
}
