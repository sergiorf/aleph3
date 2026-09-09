#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "parser/Parser.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"
#include "kernel/Diagnostics.hpp"
#include "sdk/Policy.hpp"

#include <limits>
#include <stdexcept>
#include <unordered_map>

using namespace aleph3;

namespace {

void require_exact_scalar(const ExprPtr& expr, int64_t num, int64_t den) {
    REQUIRE(expr);
    if (den == 1) {
        REQUIRE(std::holds_alternative<Integer>(*expr));
        CHECK(std::get<Integer>(*expr).value == num);
        return;
    }

    REQUIRE(std::holds_alternative<Rational>(*expr));
    const auto& r = std::get<Rational>(*expr);
    INFO("Expected: " << num << "/" << den
        << " | Got: " << r.numerator << "/" << r.denominator);
    CHECK(r.numerator == num);
    CHECK(r.denominator == den);
}

}  // namespace

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
            INFO("Input: " << c.input);
            require_exact_scalar(result, c.num, c.den);
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

TEST_CASE("Evaluator: Rational arithmetic preserves large exact values", "[evaluator][rational][exact]") {
    EvaluationContext ctx;

    auto sum = evaluate(parse_expression("1/3037000500 + 1/3037000501"), ctx);
    REQUIRE(std::holds_alternative<Rational>(*sum));
    CHECK(to_string(sum) == "6074001001/9223372040037250500");

    auto product = evaluate(parse_expression("4611686018427387904/1 * 3/1"), ctx);
    REQUIRE(std::holds_alternative<Integer>(*product));
    CHECK(to_string(product) == "13835058055282163712");

    auto comparison = evaluate(parse_expression("4611686018427387904/1 < 1/3"), ctx);
    REQUIRE(std::holds_alternative<Boolean>(*comparison));
    CHECK_FALSE(std::get<Boolean>(*comparison).value);
}

TEST_CASE("Evaluator: Exact power preserves integer and rational precision", "[evaluator][rational][exact][power]") {
    EvaluationContext ctx;

    const auto large_square = evaluate(parse_expression("3037000500^2"), ctx);
    REQUIRE(std::holds_alternative<Integer>(*large_square));
    CHECK(to_string(large_square) == "9223372037000250000");

    const auto very_large_square =
        evaluate(parse_expression("12345678901234567890^2"), ctx);
    REQUIRE(std::holds_alternative<Integer>(*very_large_square));
    CHECK(to_string(very_large_square) == "152415787532388367501905199875019052100");

    const auto rational_square = evaluate(parse_expression("(2/3)^2"), ctx);
    REQUIRE(std::holds_alternative<Rational>(*rational_square));
    CHECK(to_string(rational_square) == "4/9");

    const auto rational_negative_power = evaluate(parse_expression("(2/3)^-2"), ctx);
    REQUIRE(std::holds_alternative<Rational>(*rational_negative_power));
    CHECK(to_string(rational_negative_power) == "9/4");

    const auto negative_rational_cube = evaluate(parse_expression("(-2/3)^3"), ctx);
    REQUIRE(std::holds_alternative<Rational>(*negative_rational_cube));
    CHECK(to_string(negative_rational_cube) == "-8/27");

    const auto comparison =
        evaluate(parse_expression("3037000500^2 > 9223372036854775807"), ctx);
    REQUIRE(std::holds_alternative<Boolean>(*comparison));
    CHECK(std::get<Boolean>(*comparison).value);

    const auto oversized_exponent =
        evaluate(parse_expression("2^9223372036854775808"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*oversized_exponent));
    CHECK(to_string(oversized_exponent) == "2^9223372036854775808");
}

TEST_CASE("Evaluator: Exact power growth observes strict runtime step budget", "[evaluator][rational][exact][power][budget]") {
    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 3;

    Bindings bindings;
    Bindings constants;
    std::unordered_map<std::string, HostFunctionSpec> host_functions;
    EvaluationContext ctx(bindings, constants, host_functions, policy);
    ctx.enable_runtime_strict_semantics(true);
    ctx.reset_runtime_step_counter();

    try {
        evaluate(parse_expression("2^64"), ctx);
        FAIL("Expected exact power to consume the strict runtime step budget");
    } catch (const kernel::RuntimeFailure& failure) {
        CHECK(failure.error().code == "runtime.step_budget_exhausted");
    }
}

TEST_CASE("Rational normalization handles int64 minimum boundaries", "[evaluator][rational][overflow]") {
    const auto min = std::numeric_limits<int64_t>::min();
    const auto normalized = normalize_rational(min, min);
    REQUIRE(normalized.first == 1);
    REQUIRE(normalized.second == 1);

    REQUIRE_THROWS_AS(normalize_rational(1, min), std::overflow_error);
    REQUIRE_THROWS_AS(normalize_rational(min, -1), std::overflow_error);
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
            INFO("Input: " << c.input);
            require_exact_scalar(result, c.num, c.den);
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
            INFO("Input: " << c.input);
            require_exact_scalar(result, c.num, c.den);
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

