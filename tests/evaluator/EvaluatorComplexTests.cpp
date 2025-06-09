#include <catch2/catch_test_macros.hpp>
#include "parser/Parser.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"

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
}