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
        DYNAMIC_SECTION("Evaluating: " << c.input) {
            auto expr = parse_expression(c.input);
            auto result = evaluate(expr, ctx);
            REQUIRE(result);
            REQUIRE(std::holds_alternative<Rational>(*result));
            auto r = std::get<Rational>(*result);
            INFO("Input: " << c.input << " | Expected: " << c.num << "/" << c.den
                << " | Got: " << r.numerator << "/" << r.denominator);
            CHECK(r.numerator == c.num);
            CHECK(r.denominator == c.den);
        }
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

TEST_CASE("Evaluator: Rational reduction to lowest terms", "[evaluator][rational][reduction]") {
    EvaluationContext ctx;
    struct Case { std::string input; int64_t num, den; };
    std::vector<Case> cases = {
        {"Rational[6,8]", 3, 4},
        {"Rational[100,250]", 2, 5},
        {"Rational[-10,20]", -1, 2},
        {"Rational[  8 ,  12 ]", 2, 3},
        {"Rational[123456789,987654321]", 13717421, 109739369}
    };
    for (const auto& c : cases) {
        DYNAMIC_SECTION("Evaluating: " << c.input) {
            auto expr = parse_expression(c.input);
            auto result = evaluate(expr, ctx);
            REQUIRE(result);
            REQUIRE(std::holds_alternative<Rational>(*result));
            auto r = std::get<Rational>(*result);
            INFO("Input: " << c.input << " | Expected: " << c.num << "/" << c.den
                << " | Got: " << r.numerator << "/" << r.denominator);
            CHECK(r.numerator == c.num);
            CHECK(r.denominator == c.den);
        }
    }
}

TEST_CASE("Evaluator: Rational normalization of signs", "[evaluator][rational][signs]") {
    EvaluationContext ctx;
    struct Case { std::string input; int64_t num, den; };
    std::vector<Case> cases = {
        {"Rational[3,4]", 3, 4},
        {"Rational[-3,4]", -3, 4},
        {"Rational[3,-4]", -3, 4},
        {"Rational[-3,-4]", 3, 4},
        {"Rational[0,5]", 0, 1},
        {"Rational[0,7]", 0, 1},
        {"Rational[5,1]", 5, 1},
        {"Rational[-5,1]", -5, 1},
        {"Rational[7,3]", 7, 3},
        {"Rational[-7,3]", -7, 3}
    };
    for (const auto& c : cases) {
        DYNAMIC_SECTION("Evaluating: " << c.input) {
            auto expr = parse_expression(c.input);
            auto result = evaluate(expr, ctx);
            REQUIRE(result);
            REQUIRE(std::holds_alternative<Rational>(*result));
            auto r = std::get<Rational>(*result);
            INFO("Input: " << c.input << " | Expected: " << c.num << "/" << c.den
                << " | Got: " << r.numerator << "/" << r.denominator);
            CHECK(r.numerator == c.num);
            CHECK(r.denominator == c.den);
        }
    }
}

TEST_CASE("Evaluator: Rational comparison operators", "[evaluator][rational][comparison]") {
    EvaluationContext ctx;
    struct Case {
        std::string input;
        bool expected;
    };

    SECTION("Equality (==)") {
        std::vector<Case> cases = {
            {"1/2 == 2/4", true},
            {"1/2 == 3/4", false},
            {"-1/2 == 1/-2", true},
            {"-1/2 == -2/4", true},
            {"0/5 == 0/7", true},
            {"3/4 == 6/8", true},
            {"3/4 == -3/4", false}
        };
        for (const auto& c : cases) {
            DYNAMIC_SECTION("Evaluating: " << c.input) {
                auto expr = parse_expression(c.input);
                auto result = evaluate(expr, ctx);
                REQUIRE(result);
                REQUIRE(std::holds_alternative<Boolean>(*result));
                CHECK(std::get<Boolean>(*result).value == c.expected);
            }
        }
    }

    SECTION("Inequality (!=)") {
        std::vector<Case> cases = {
            {"1/2 != 2/4", false},
            {"1/2 != 3/4", true},
            {"-1/2 != 1/-2", false},
            {"-1/2 != -2/4", false},
            {"0/5 != 0/7", false},
            {"3/4 != 6/8", false},
            {"3/4 != -3/4", true}
        };
        for (const auto& c : cases) {
            DYNAMIC_SECTION("Evaluating: " << c.input) {
                auto expr = parse_expression(c.input);
                auto result = evaluate(expr, ctx);
                REQUIRE(result);
                REQUIRE(std::holds_alternative<Boolean>(*result));
                CHECK(std::get<Boolean>(*result).value == c.expected);
            }
        }
    }

    SECTION("Less than (<)") {
        std::vector<Case> cases = {
            {"1/3 < 1/2", true},
            {"2/3 < 1/2", false},
            {"-1/2 < 0", true},
            {"0 < 1/2", true},
            {"-3/4 < -1/2", true}
        };
        for (const auto& c : cases) {
            DYNAMIC_SECTION("Evaluating: " << c.input) {
                auto expr = parse_expression(c.input);
                auto result = evaluate(expr, ctx);
                REQUIRE(result);
                REQUIRE(std::holds_alternative<Boolean>(*result));
                CHECK(std::get<Boolean>(*result).value == c.expected);
            }
        }
    }

    SECTION("Greater than (>)") {
        std::vector<Case> cases = {
            {"1/2 > 1/3", true},
            {"1/2 > 2/3", false},
            {"0 > -1/2", true},
            {"-1/2 > 0", false},
            {"-1/2 > -3/4", true}
        };
        for (const auto& c : cases) {
            DYNAMIC_SECTION("Evaluating: " << c.input) {
                auto expr = parse_expression(c.input);
                auto result = evaluate(expr, ctx);
                REQUIRE(result);
                REQUIRE(std::holds_alternative<Boolean>(*result));
                CHECK(std::get<Boolean>(*result).value == c.expected);
            }
        }
    }

    SECTION("Less than or equal (<=)") {
        std::vector<Case> cases = {
            {"1/2 <= 1/2", true},
            {"1/3 <= 1/2", true},
            {"2/3 <= 1/2", false},
            {"-1/2 <= 0", true},
            {"-3/4 <= -1/2", true}
        };
        for (const auto& c : cases) {
            DYNAMIC_SECTION("Evaluating: " << c.input) {
                auto expr = parse_expression(c.input);
                auto result = evaluate(expr, ctx);
                REQUIRE(result);
                REQUIRE(std::holds_alternative<Boolean>(*result));
                CHECK(std::get<Boolean>(*result).value == c.expected);
            }
        }
    }

    SECTION("Greater than or equal (>=)") {
        std::vector<Case> cases = {
            {"1/2 >= 1/2", true},
            {"1/2 >= 1/3", true},
            {"1/2 >= 2/3", false},
            {"0 >= -1/2", true},
            {"-1/2 >= 0", false},
            {"-1/2 >= -3/4", true}
        };
        for (const auto& c : cases) {
            DYNAMIC_SECTION("Evaluating: " << c.input) {
                auto expr = parse_expression(c.input);
                auto result = evaluate(expr, ctx);
                REQUIRE(result);
                REQUIRE(std::holds_alternative<Boolean>(*result));
                CHECK(std::get<Boolean>(*result).value == c.expected);
            }
        }
    }

    SECTION("Mixed Rational and Number") {
        std::vector<Case> cases = {
            {"1/2 == 0.5", true},
            {"2/3 == 0.6666666667", false}, // not exactly equal
            {"2/3 < 0.7", true},
            {"2/3 > 0.6", true},
            {"-1/2 < 0.0", true},
            {"0.25 == 1/4", true},
            {"0.3333333333 == 1/3", false}, // not exactly equal
            {"1/2 != 0.5", false},
            {"1/2 <= 0.5", true},
            {"1/2 >= 0.5", true},
            {"1/2 < 0.6", true},
            {"1/2 > 0.4", true},
            {"0.5 < 2/3", true},
            {"0.5 > 1/3", true},
            {"0.5 != 1/2", false}
        };
        for (const auto& c : cases) {
            DYNAMIC_SECTION("Evaluating: " << c.input) {
                auto expr = parse_expression(c.input);
                auto result = evaluate(expr, ctx);
                REQUIRE(result);
                REQUIRE(std::holds_alternative<Boolean>(*result));
                CHECK(std::get<Boolean>(*result).value == c.expected);
            }
        }
    }
}

