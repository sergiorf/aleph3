#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "parser/Parser.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "Constants.hpp"
#include <cmath>

using namespace aleph3;

TEST_CASE("Evaluator: Complex number arithmetic", "[evaluator][complex]") {
    EvaluationContext ctx;

    SECTION("Evaluate Complex[3, 4]") {
        auto expr = parse_expression("Complex[3, 4]");
        auto result = evaluate(expr, ctx);
        REQUIRE(std::holds_alternative<Complex>(*result));
        const auto& c = std::get<Complex>(*result);
        CHECK(c.real == 3.0);
        CHECK(c.imag == 4.0);
    }

    SECTION("Evaluate I") {
        auto expr = parse_expression("I");
        auto result = evaluate(expr, ctx);
        REQUIRE(std::holds_alternative<Complex>(*result));
        const auto& c = std::get<Complex>(*result);
        CHECK(c.real == 0.0);
        CHECK(c.imag == 1.0);
    }

    SECTION("Evaluate -I") {
        auto expr = parse_expression("-I");
        auto result = evaluate(expr, ctx);
        REQUIRE(std::holds_alternative<Complex>(*result));
        const auto& c = std::get<Complex>(*result);
        CHECK(c.real == 0.0);
        CHECK(c.imag == -1.0);
    }

    SECTION("Evaluate 3 + 4*I") {
        auto expr = parse_expression("3 + 4*I");
        auto result = evaluate(expr, ctx);
        REQUIRE(std::holds_alternative<Complex>(*result));
        const auto& c = std::get<Complex>(*result);
        CHECK(c.real == 3.0);
        CHECK(c.imag == 4.0);
    }

    SECTION("Evaluate 2 - 5*I") {
        auto expr = parse_expression("2 - 5*I");
        auto result = evaluate(expr, ctx);
        REQUIRE(std::holds_alternative<Complex>(*result));
        const auto& c = std::get<Complex>(*result);
        CHECK(c.real == 2.0);
        CHECK(c.imag == -5.0);
    }

    SECTION("Evaluate (1 + 2*I) + (3 + 4*I)") {
        auto expr = parse_expression("(1 + 2*I) + (3 + 4*I)");
        auto result = evaluate(expr, ctx);
        REQUIRE(std::holds_alternative<Complex>(*result));
        const auto& c = std::get<Complex>(*result); 
        CHECK(c.real == 4.0);
        CHECK(c.imag == 6.0);
    }

    SECTION("Evaluate (5 - 3*I) * (2 + I)") {
        auto expr = parse_expression("(5 - 3*I) * (2 + I)");
        auto result = evaluate(expr, ctx);
        REQUIRE(std::holds_alternative<Complex>(*result));
        const auto& c = std::get<Complex>(*result);
        CHECK(c.real == 13.0);
        CHECK(c.imag == -1.0);
    }

    SECTION("Evaluate Gamma[1 + I]") {
        auto expr = parse_expression("Gamma[1 + I]");
        auto result = evaluate(expr, ctx);
        REQUIRE(std::holds_alternative<Complex>(*result));
        const auto& c = std::get<Complex>(*result);
        CHECK(c.real == Catch::Approx(0.498015668118356).margin(1e-9));
        CHECK(c.imag == Catch::Approx(-0.154949828301811).margin(1e-9));
    }

    SECTION("Evaluate Gamma conjugation symmetry numerically") {
        auto left = evaluate(parse_expression("Gamma[1 + I]"), ctx);
        auto right = evaluate(parse_expression("Gamma[1 - I]"), ctx);
        REQUIRE(std::holds_alternative<Complex>(*left));
        REQUIRE(std::holds_alternative<Complex>(*right));
        const auto& lhs = std::get<Complex>(*left);
        const auto& rhs = std::get<Complex>(*right);
        CHECK(lhs.real == Catch::Approx(rhs.real).margin(1e-9));
        CHECK(lhs.imag == Catch::Approx(-rhs.imag).margin(1e-9));
    }

    SECTION("Evaluate Gamma recurrence on complex input") {
        auto gamma_1_plus_i = evaluate(parse_expression("Gamma[1 + I]"), ctx);
        auto gamma_2_plus_i = evaluate(parse_expression("Gamma[2 + I]"), ctx);
        REQUIRE(std::holds_alternative<Complex>(*gamma_1_plus_i));
        REQUIRE(std::holds_alternative<Complex>(*gamma_2_plus_i));

        const auto& g1 = std::get<Complex>(*gamma_1_plus_i);
        const auto& g2 = std::get<Complex>(*gamma_2_plus_i);
        const double expected_real = g1.real - g1.imag;
        const double expected_imag = g1.real + g1.imag;
        CHECK(g2.real == Catch::Approx(expected_real).margin(1e-9));
        CHECK(g2.imag == Catch::Approx(expected_imag).margin(1e-9));
    }

    SECTION("Evaluate Gamma pole on complex real-axis integer as ComplexInfinity") {
        auto expr = parse_expression("Gamma[Complex[-1, 0]]");
        auto result = evaluate(expr, ctx);
        REQUIRE(std::holds_alternative<ComplexInfinity>(*result));
    }
}
