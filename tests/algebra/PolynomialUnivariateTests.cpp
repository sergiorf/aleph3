#include "algebra/Polynomial.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <string>
#include <map>

using namespace aleph3;

// Helper: Construct a univariate polynomial in variable "x" from coefficients
Polynomial make_univariate(const std::vector<double>& coeffs, const std::string& var = "x") {
    std::map<Monomial, double> terms;
    for (size_t i = 0; i < coeffs.size(); ++i) {
        if (std::abs(coeffs[i]) < 1e-10) continue;
        Monomial m;
        if (i > 0) m[var] = static_cast<int>(i);
        terms[m] = coeffs[i];
    }
    return Polynomial(terms);
}

// Helper: Extract coefficients for univariate polynomials in "x"
std::vector<double> get_coefficients(const Polynomial& poly, const std::string& var = "x") {
    size_t max_deg = 0;
    for (const auto& [mono, coeff] : poly.terms) {
        int deg = 0;
        auto it = mono.find(var);
        if (it != mono.end()) deg = it->second;
        if (std::abs(coeff) >= 1e-10)
            max_deg = std::max(max_deg, static_cast<size_t>(deg));
    }
    std::vector<double> coeffs(max_deg + 1, 0.0);
    for (const auto& [mono, coeff] : poly.terms) {
        int deg = 0;
        auto it = mono.find(var);
        if (it != mono.end()) deg = it->second;
        coeffs[deg] = coeff;
    }
    // Remove trailing zeros for normalization
    while (coeffs.size() > 1 && std::abs(coeffs.back()) < 1e-10) coeffs.pop_back();
    return coeffs;
}

TEST_CASE("Polynomial: Constructor and normalization") {
    Polynomial p1 = make_univariate({ 0, 0, 3, 0 });
    REQUIRE(get_coefficients(p1) == std::vector<double>({ 0, 0, 3 }));
    Polynomial p2 = make_univariate({ 0, 0, 0 });
    REQUIRE(get_coefficients(p2) == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Addition") {
    Polynomial p1 = make_univariate({ 1, 2, 3 });
    Polynomial p2 = make_univariate({ 3, 4 });
    Polynomial sum = p1 + p2;
    REQUIRE(get_coefficients(sum) == std::vector<double>({ 4, 6, 3 }));
}

TEST_CASE("Polynomial: Subtraction") {
    Polynomial p1 = make_univariate({ 5, 3, 1 });
    Polynomial p2 = make_univariate({ 2, 1 });
    Polynomial diff = p1 - p2;
    REQUIRE(get_coefficients(diff) == std::vector<double>({ 3, 2, 1 }));
}

TEST_CASE("Polynomial: Multiplication") {
    Polynomial p1 = make_univariate({ 1, 2 });
    Polynomial p2 = make_univariate({ 2, 1 });
    Polynomial prod = p1 * p2;
    REQUIRE(get_coefficients(prod) == std::vector<double>({ 2, 5, 2 }));
}

TEST_CASE("Polynomial: Division") {
    Polynomial dividend = make_univariate({ 1, -3, 2 });
    Polynomial divisor = make_univariate({ 1, -1 });
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(get_coefficients(quot) == std::vector<double>({ 1, -2 }));
    REQUIRE(get_coefficients(rem) == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Division with nonzero remainder") {
    Polynomial dividend = make_univariate({ 2, 3, 1 });
    Polynomial divisor = make_univariate({ 1, 1 });
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(get_coefficients(quot) == std::vector<double>({ 2, 1 }));
    REQUIRE(get_coefficients(rem) == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Division with remainder") {
    Polynomial dividend = make_univariate({ 1, 0, 1 });
    Polynomial divisor = make_univariate({ 1, 1 });
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(get_coefficients(quot) == std::vector<double>({ -1, 1 }));
    REQUIRE(get_coefficients(rem) == std::vector<double>({ 2 }));
}

TEST_CASE("Polynomial: Division by higher degree divisor") {
    Polynomial dividend = make_univariate({ 1, 2 });
    Polynomial divisor = make_univariate({ 1, 0, 1 });
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(get_coefficients(quot) == std::vector<double>({ 0 }));
    REQUIRE(get_coefficients(rem) == std::vector<double>({ 1, 2 }));
}

TEST_CASE("Polynomial: Division by monomial") {
    Polynomial dividend = make_univariate({ 0, 2, 4 });
    Polynomial divisor = make_univariate({ 0, 2 });
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(get_coefficients(quot) == std::vector<double>({ 1, 2 }));
    REQUIRE(get_coefficients(rem) == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Division by itself") {
    Polynomial dividend = make_univariate({ 1, -3, 2 });
    Polynomial divisor = make_univariate({ 1, -3, 2 });
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(get_coefficients(quot) == std::vector<double>({ 1 }));
    REQUIRE(get_coefficients(rem) == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Division by constant") {
    Polynomial dividend = make_univariate({ 2, 4, 6 });
    Polynomial divisor = make_univariate({ 2 });
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(get_coefficients(quot) == std::vector<double>({ 1, 2, 3 }));
    REQUIRE(get_coefficients(rem) == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Division by zero throws") {
    Polynomial dividend = make_univariate({ 1, 2, 3 });
    Polynomial divisor = make_univariate({ 0 });
    REQUIRE_THROWS_AS(dividend.divide(divisor), std::domain_error);
}

TEST_CASE("Polynomial: GCD") {
    Polynomial p1 = make_univariate({ -1, 0, 1 });
    Polynomial p2 = make_univariate({ -1, 1 });
    Polynomial g = Polynomial::gcd(p1, p2, "x");
    REQUIRE(get_coefficients(g) == std::vector<double>({ -1, 1 }));
}

TEST_CASE("Polynomial: is_zero") {
    Polynomial p1 = make_univariate({ 0 });
    Polynomial p2 = make_univariate({ 0, 0, 0 });
    REQUIRE(p1.is_zero());
    REQUIRE(p2.is_zero());
    Polynomial p3 = make_univariate({ 1, 0 });
    REQUIRE_FALSE(p3.is_zero());
}

TEST_CASE("Polynomial: degree") {
    Polynomial p1 = make_univariate({ 1, 2, 0, 0 });
    REQUIRE(p1.degree() == 1);
    Polynomial p2 = make_univariate({ 0 });
    REQUIRE(p2.degree() == 0);
}

TEST_CASE("Polynomial: to_string") {
    Polynomial p1 = make_univariate({ 1, 0, 2 });
    std::string s = p1.to_string();
    REQUIRE(s.find("1") != std::string::npos);
    REQUIRE(s.find("2*x^2") != std::string::npos);
}