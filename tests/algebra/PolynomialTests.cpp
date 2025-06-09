#include "algebra/Polynomial.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <string>

using namespace aleph3;

TEST_CASE("Polynomial: Constructor and normalization") {
    Polynomial p1({0, 0, 3, 0});
    REQUIRE(p1.coefficients == std::vector<double>({0, 0, 3}));
    Polynomial p2({0, 0, 0});
    REQUIRE(p2.coefficients == std::vector<double>({0}));
}

TEST_CASE("Polynomial: Addition") {
    Polynomial p1({1, 2, 3}); // 1 + 2x + 3x^2
    Polynomial p2({3, 4});    // 3 + 4x
    Polynomial sum = p1 + p2;
    REQUIRE(sum.coefficients == std::vector<double>({4, 6, 3}));
}

TEST_CASE("Polynomial: Subtraction") {
    Polynomial p1({5, 3, 1}); // 5 + 3x + x^2
    Polynomial p2({2, 1});    // 2 + x
    Polynomial diff = p1 - p2;
    REQUIRE(diff.coefficients == std::vector<double>({3, 2, 1}));
}

TEST_CASE("Polynomial: Multiplication") {
    Polynomial p1({1, 2}); // 1 + 2x
    Polynomial p2({2, 1}); // 2 + x
    Polynomial prod = p1 * p2; // (1 + 2x)(2 + x) = 2 + 5x + 2x^2
    REQUIRE(prod.coefficients == std::vector<double>({2, 5, 2}));
}

TEST_CASE("Polynomial: Division") {
    Polynomial dividend({1, -3, 2}); // 1 - 3x + 2x^2
    Polynomial divisor({1, -1});     // 1 - x
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(quot.coefficients == std::vector<double>({ 1, -2 })); // 1 - 2x
    REQUIRE(rem.coefficients == std::vector<double>({0}));
}

TEST_CASE("Polynomial: Division with nonzero remainder") {
    Polynomial dividend({ 2, 3, 1 }); // 2 + 3x + x^2
    Polynomial divisor({ 1, 1 });     // 1 + x
    auto [quot, rem] = dividend.divide(divisor);
    // (2 + 3x + x^2) / (1 + x) = x + 2, remainder 0
    REQUIRE(quot.coefficients == std::vector<double>({ 2, 1 }));
    REQUIRE(rem.coefficients == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Division with remainder") {
    Polynomial dividend({ 1, 0, 1 }); // 1 + x^2
    Polynomial divisor({ 1, 1 });     // 1 + x
    auto [quot, rem] = dividend.divide(divisor);
    // (1 + x^2) / (1 + x) = x - 1, remainder 2
    REQUIRE(quot.coefficients == std::vector<double>({ -1, 1 }));
    REQUIRE(rem.coefficients == std::vector<double>({ 2 }));
}

TEST_CASE("Polynomial: Division by higher degree divisor") {
    Polynomial dividend({ 1, 2 });    // 1 + 2x
    Polynomial divisor({ 1, 0, 1 });  // 1 + x^2
    auto [quot, rem] = dividend.divide(divisor);
    // Degree of divisor > dividend, so quotient 0, remainder is dividend
    REQUIRE(quot.coefficients == std::vector<double>({ 0 }));
    REQUIRE(rem.coefficients == std::vector<double>({ 1, 2 }));
}

TEST_CASE("Polynomial: Division by monomial") {
    Polynomial dividend({ 0, 2, 4 }); // 2x + 4x^2
    Polynomial divisor({ 0, 2 });     // 2x
    auto [quot, rem] = dividend.divide(divisor);
    // (2x + 4x^2) / (2x) = 1 + 2x, remainder 0
    REQUIRE(quot.coefficients == std::vector<double>({ 1, 2 }));
    REQUIRE(rem.coefficients == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Division by itself") {
    Polynomial dividend({ 1, -3, 2 }); // 1 - 3x + 2x^2
    Polynomial divisor({ 1, -3, 2 });  // 1 - 3x + 2x^2
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(quot.coefficients == std::vector<double>({ 1 }));
    REQUIRE(rem.coefficients == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Division by constant") {
    Polynomial dividend({ 2, 4, 6 }); // 2 + 4x + 6x^2
    Polynomial divisor({ 2 });        // 2
    auto [quot, rem] = dividend.divide(divisor);
    REQUIRE(quot.coefficients == std::vector<double>({ 1, 2, 3 }));
    REQUIRE(rem.coefficients == std::vector<double>({ 0 }));
}

TEST_CASE("Polynomial: Division by zero throws") {
    Polynomial dividend({ 1, 2, 3 });
    Polynomial divisor({ 0 });
    REQUIRE_THROWS_AS(dividend.divide(divisor), std::domain_error);
}

TEST_CASE("Polynomial: GCD") {
    Polynomial p1({-1, 0, 1}); // x^2 - 1
    Polynomial p2({-1, 1});    // x - 1
    Polynomial g = Polynomial::gcd(p1, p2);
    REQUIRE(g.coefficients == std::vector<double>({-1, 1}));
}

TEST_CASE("Polynomial: is_zero") {
    Polynomial p1({0});
    Polynomial p2({0, 0, 0});
    REQUIRE(p1.is_zero());
    REQUIRE(p2.is_zero());
    Polynomial p3({1, 0});
    REQUIRE_FALSE(p3.is_zero());
}

TEST_CASE("Polynomial: degree") {
    Polynomial p1({1, 2, 0, 0});
    REQUIRE(p1.degree() == 1);
    Polynomial p2({0});
    REQUIRE(p2.degree() == 0);
}

TEST_CASE("Polynomial: to_string") {
    Polynomial p1({1, 0, 2}); // 1 + 2x^2
    std::string s = p1.to_string();
    REQUIRE(s.find("1") != std::string::npos);
    REQUIRE(s.find("2x^2") != std::string::npos);
}