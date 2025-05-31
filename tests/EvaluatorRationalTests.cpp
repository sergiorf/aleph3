#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "parser/Parser.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"

using namespace aleph3;

TEST_CASE("Evaluator: Rational arithmetic", "[evaluator][rational]") {
    EvaluationContext ctx;
    struct Case {
        std::string input;
        int64_t num, den;
    };
    std::vector<Case> cases = {
        {"1/2 + 1/3", 5, 6},
        {"1/2 - 1/3", 1, 6},
        {"2/3 * 3/4", 1, 2},
        {"2/3 / 3/4", 8, 9},
        {"-2/5 + 1/5", -1, 5},
        {"-2/5 - 1/5", -3, 5},
        {"-2/5 * 3/7", -6, 35},
        {"-2/5 / 3/7", -14, 15},
        {"3/4 + 1/4", 1, 1},
        {"3/4 - 3/4", 0, 1},
        {"0/5 + 1/2", 1, 2},
        {"0/5 * 7/8", 0, 1},
        {"5/1 + 2/1", 7, 1},
        {"-3/4 + 3/4", 0, 1}
    };
    for (const auto& c : cases) {
        auto expr = parse_expression(c.input);
        auto result = evaluate(expr, ctx);
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Rational>(*result));
        auto r = std::get<Rational>(*result);
        CHECK(r.numerator == c.num);
        CHECK(r.denominator == c.den);
    }
}

TEST_CASE("Evaluator: Rational and integer/float mixing", "[evaluator][rational][number]") {
    EvaluationContext ctx;
    SECTION("Rational plus integer") {
        auto expr = parse_expression("1/2 + 2");
        auto result = evaluate(expr, ctx);
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Rational>(*result));
        auto r = std::get<Rational>(*result);
        CHECK(r.numerator == 5);
        CHECK(r.denominator == 2);
    }
    SECTION("Rational minus integer") {
        auto expr = parse_expression("1/2 - 2");
        auto result = evaluate(expr, ctx);
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Rational>(*result));
        auto r = std::get<Rational>(*result);
        CHECK(r.numerator == -3);
        CHECK(r.denominator == 2);
    }
    SECTION("Rational times float") {
        auto expr = parse_expression("1/2 * 0.5");
        auto result = evaluate(expr, ctx);
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Number>(*result));
        auto n = std::get<Number>(*result);
        CHECK(n.value == Catch::Approx(0.25));
    }
    SECTION("Float plus rational") {
        auto expr = parse_expression("0.5 + 1/4");
        auto result = evaluate(expr, ctx);
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Number>(*result));
        auto n = std::get<Number>(*result);
        CHECK(n.value == Catch::Approx(0.75));
    }
}

TEST_CASE("Evaluator: Rational edge cases", "[evaluator][rational][edge]") {
    EvaluationContext ctx;
    SECTION("Zero denominator") {
        auto expr = parse_expression("1/0");
        auto result = evaluate(expr, ctx);
        REQUIRE(result);
        CHECK(std::holds_alternative<Infinity>(*result));
    }
    SECTION("Zero over zero") {
        auto expr = parse_expression("0/0");
        auto result = evaluate(expr, ctx);
        REQUIRE(result);
        CHECK(std::holds_alternative<Indeterminate>(*result));
    }
    SECTION("Negative denominator") {
        auto expr = parse_expression("3/-4");
        auto result = evaluate(expr, ctx);
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Rational>(*result));
        auto r = std::get<Rational>(*result);
        CHECK(r.numerator == -3);
        CHECK(r.denominator == 4);
    }
    SECTION("Reduction to lowest terms") {
        auto expr = parse_expression("100/250");
        auto result = evaluate(expr, ctx);
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Rational>(*result));
        auto r = std::get<Rational>(*result);
        CHECK(r.numerator == 2);
        CHECK(r.denominator == 5);
    }
}