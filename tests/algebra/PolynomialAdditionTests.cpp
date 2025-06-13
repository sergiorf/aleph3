#include "algebra/Polynomial.hpp"
#include <catch2/catch_test_macros.hpp>
#include <map>
#include <string>
#include <vector>

using namespace aleph3;

// Helper: Construct a multivariate polynomial from a list of (coeff, {var,exp}...) tuples
Polynomial make_poly(const std::vector<std::pair<double, std::map<std::string, int>>>& terms) {
    std::map<Monomial, double> poly_terms;
    for (const auto& [coeff, exps] : terms) {
        Monomial m;
        for (const auto& [var, exp] : exps) {
            if (exp != 0) m[var] = exp;
        }
        poly_terms[m] += coeff;
    }
    return Polynomial(poly_terms);
}

// Helper: Get coefficient for a specific monomial
double get_coeff(const Polynomial& poly, const std::map<std::string, int>& exps) {
    Monomial m;
    for (const auto& [var, exp] : exps) {
        if (exp != 0) m[var] = exp;
    }
    auto it = poly.terms.find(m);
    return (it != poly.terms.end()) ? it->second : 0.0;
}

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