#include "algebra/Polynomial.hpp"
#include "algebra/PolynomialTestHelpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <map>
#include <string>
#include <vector>

using namespace aleph3;
using namespace aleph3::test;

TEST_CASE("Polynomial Addition: Univariate simple") {
    // (1 + 2x + 3x^2) + (3 + 4x)
    Polynomial p1 = make_poly({{1.0, {}}, {2.0, {{"x",1}}}, {3.0, {{"x",2}}}});
    Polynomial p2 = make_poly({{3.0, {}}, {4.0, {{"x",1}}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {}) == 4.0);
    REQUIRE(get_coeff(sum, {{"x",1}}) == 6.0);
    REQUIRE(get_coeff(sum, {{"x",2}}) == 3.0);
}

TEST_CASE("Polynomial Addition: Multivariate") {
    // (x^2*y + y^2) + (2*x^2*y + 3)
    Polynomial p1 = make_poly({{1.0, {{"x",2},{"y",1}}}, {1.0, {{"y",2}}}});
    Polynomial p2 = make_poly({{2.0, {{"x",2},{"y",1}}}, {3.0, {}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",2},{"y",1}}) == 3.0);
    REQUIRE(get_coeff(sum, {{"y",2}}) == 1.0);
    REQUIRE(get_coeff(sum, {}) == 3.0);
}

TEST_CASE("Polynomial Addition: Zero polynomial") {
    Polynomial p1 = make_poly({{0.0, {}}});
    Polynomial p2 = make_poly({{2.0, {{"x",1}}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",1}}) == 2.0);
    REQUIRE(get_coeff(sum, {}) == 0.0);
}

TEST_CASE("Polynomial Addition: Like terms combine") {
    // (x^2*y + 2*x^2*y) = 3*x^2*y
    Polynomial p1 = make_poly({{1.0, {{"x",2},{"y",1}}}});
    Polynomial p2 = make_poly({{2.0, {{"x",2},{"y",1}}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",2},{"y",1}}) == 3.0);
}

TEST_CASE("Polynomial Addition: Negative coefficients") {
    // (x^2 - y) + (-x^2 + y) = 0
    Polynomial p1 = make_poly({{1.0, {{"x",2}}}, {-1.0, {{"y",1}}}});
    Polynomial p2 = make_poly({{-1.0, {{"x",2}}}, {1.0, {{"y",1}}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",2}}) == 0.0);
    REQUIRE(get_coeff(sum, {{"y",1}}) == 0.0);
}

TEST_CASE("Polynomial Addition: Many variables") {
    // (x*y*z + 2*x^2*y^2) + (3*x*y*z + 4)
    Polynomial p1 = make_poly({{1.0, {{"x",1},{"y",1},{"z",1}}}, {2.0, {{"x",2},{"y",2}}}});
    Polynomial p2 = make_poly({{3.0, {{"x",1},{"y",1},{"z",1}}}, {4.0, {}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",1},{"y",1},{"z",1}}) == 4.0);
    REQUIRE(get_coeff(sum, {{"x",2},{"y",2}}) == 2.0);
    REQUIRE(get_coeff(sum, {}) == 4.0);
}

TEST_CASE("Polynomial Addition: Constant polynomials") {
    // (5) + (-2) = 3
    Polynomial p1 = make_poly({{5.0, {}}});
    Polynomial p2 = make_poly({{-2.0, {}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {}) == 3.0);
}

TEST_CASE("Polynomial Addition: All terms cancel") {
    // (2x + 3y) + (-2x - 3y) = 0
    Polynomial p1 = make_poly({{2.0, {{"x",1}}}, {3.0, {{"y",1}}}});
    Polynomial p2 = make_poly({{-2.0, {{"x",1}}}, {-3.0, {{"y",1}}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",1}}) == 0.0);
    REQUIRE(get_coeff(sum, {{"y",1}}) == 0.0);
    REQUIRE(get_coeff(sum, {}) == 0.0);
}

TEST_CASE("Polynomial Addition: High degree terms") {
    // (x^10) + (2x^10) = 3x^10
    Polynomial p1 = make_poly({{1.0, {{"x",10}}}});
    Polynomial p2 = make_poly({{2.0, {{"x",10}}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",10}}) == 3.0);
}

TEST_CASE("Polynomial Addition: Mixed zero and nonzero terms") {
    // (0x^2 + y) + (2x^2 + 0y) = 2x^2 + y
    Polynomial p1 = make_poly({{0.0, {{"x",2}}}, {1.0, {{"y",1}}}});
    Polynomial p2 = make_poly({{2.0, {{"x",2}}}, {0.0, {{"y",1}}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",2}}) == 2.0);
    REQUIRE(get_coeff(sum, {{"y",1}}) == 1.0);
}

TEST_CASE("Polynomial Addition: Large number of variables") {
    // (x*y*z*w) + (2x*y*z*w) = 3x*y*z*w
    Polynomial p1 = make_poly({{1.0, {{"x",1},{"y",1},{"z",1},{"w",1}}}});
    Polynomial p2 = make_poly({{2.0, {{"x",1},{"y",1},{"z",1},{"w",1}}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",1},{"y",1},{"z",1},{"w",1}}) == 3.0);
}

TEST_CASE("Polynomial Addition: Add to zero polynomial") {
    // 0 + 7x^3 = 7x^3
    Polynomial zero = make_poly({});
    Polynomial p = make_poly({{7.0, {{"x",3}}}});
    Polynomial sum = zero + p;
    REQUIRE(get_coeff(sum, {{"x",3}}) == 7.0);
}

TEST_CASE("Polynomial Addition: Add polynomials with disjoint terms") {
    // (x) + (2y) = x + 2y
    Polynomial p1 = make_poly({{1.0, {{"x",1}}}});
    Polynomial p2 = make_poly({{2.0, {{"y",1}}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",1}}) == 1.0);
    REQUIRE(get_coeff(sum, {{"y",1}}) == 2.0);
}

TEST_CASE("Polynomial Addition: Multivariate with negative and positive terms") {
    // (x^2*y - 2xy^2 + 3) + (-x^2*y + 2xy^2 - 1) = 2
    Polynomial p1 = make_poly({{1.0, {{"x",2},{"y",1}}}, {-2.0, {{"x",1},{"y",2}}}, {3.0, {}}});
    Polynomial p2 = make_poly({{-1.0, {{"x",2},{"y",1}}}, {2.0, {{"x",1},{"y",2}}}, {-1.0, {}}});
    Polynomial sum = p1 + p2;
    REQUIRE(get_coeff(sum, {{"x",2},{"y",1}}) == 0.0);
    REQUIRE(get_coeff(sum, {{"x",1},{"y",2}}) == 0.0);
    REQUIRE(get_coeff(sum, {}) == 2.0);
}