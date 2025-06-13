#include "algebra/Polynomial.hpp"
#include "algebra/PolynomialTestHelpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <map>
#include <string>
#include <vector>

using namespace aleph3;
using namespace aleph3::test;

TEST_CASE("Polynomial Multiplication: Univariate simple") {
    // (1 + 2x) * (2 + x) = 2 + 5x + 2x^2
    Polynomial p1 = make_poly({{1.0, {}}, {2.0, {{"x",1}}}});
    Polynomial p2 = make_poly({{2.0, {}}, {1.0, {{"x",1}}}});
    Polynomial prod = p1 * p2;
    REQUIRE(get_coeff(prod, {}) == 2.0);
    REQUIRE(get_coeff(prod, {{"x",1}}) == 5.0);
    REQUIRE(get_coeff(prod, {{"x",2}}) == 2.0);
}

TEST_CASE("Polynomial Multiplication: Multivariate simple") {
    // (x + y) * (x - y) = x^2 - y^2
    Polynomial p1 = make_poly({{1.0, {{"x",1}}}, {1.0, {{"y",1}}}});
    Polynomial p2 = make_poly({{1.0, {{"x",1}}}, {-1.0, {{"y",1}}}});
    Polynomial prod = p1 * p2;
    REQUIRE(get_coeff(prod, {{"x",2}}) == 1.0);
    REQUIRE(get_coeff(prod, {{"y",2}}) == -1.0);
    REQUIRE(get_coeff(prod, {{"x",1},{"y",1}}) == 0.0);
}

TEST_CASE("Polynomial Multiplication: Multivariate mixed degrees") {
    // (x*y) * (x^2*y^3) = x^3*y^4
    Polynomial p1 = make_poly({{1.0, {{"x",1},{"y",1}}}});
    Polynomial p2 = make_poly({{1.0, {{"x",2},{"y",3}}}});
    Polynomial prod = p1 * p2;
    REQUIRE(get_coeff(prod, {{"x",3},{"y",4}}) == 1.0);
}

TEST_CASE("Polynomial Multiplication: Distributive law") {
    // (x + 1) * (x + 2) = x^2 + 3x + 2
    Polynomial p1 = make_poly({{1.0, {{"x",1}}}, {1.0, {}}});
    Polynomial p2 = make_poly({{1.0, {{"x",1}}}, {2.0, {}}});
    Polynomial prod = p1 * p2;
    REQUIRE(get_coeff(prod, {{"x",2}}) == 1.0);
    REQUIRE(get_coeff(prod, {{"x",1}}) == 3.0);
    REQUIRE(get_coeff(prod, {}) == 2.0);
}

TEST_CASE("Polynomial Multiplication: Zero polynomial") {
    Polynomial p1 = make_poly({{0.0, {}}});
    Polynomial p2 = make_poly({{2.0, {{"x",1}}}});
    Polynomial prod = p1 * p2;
    REQUIRE(get_coeff(prod, {{"x",1}}) == 0.0);
    REQUIRE(get_coeff(prod, {}) == 0.0);
}

TEST_CASE("Polynomial Multiplication: Negative coefficients") {
    // (x - y) * (x + y) = x^2 - y^2
    Polynomial p1 = make_poly({{1.0, {{"x",1}}}, {-1.0, {{"y",1}}}});
    Polynomial p2 = make_poly({{1.0, {{"x",1}}}, {1.0, {{"y",1}}}});
    Polynomial prod = p1 * p2;
    REQUIRE(get_coeff(prod, {{"x",2}}) == 1.0);
    REQUIRE(get_coeff(prod, {{"y",2}}) == -1.0);
    REQUIRE(get_coeff(prod, {{"x",1},{"y",1}}) == 0.0);
}