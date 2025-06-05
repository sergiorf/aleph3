#include "parser/Parser.hpp"
#include "expr/Expr.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace aleph3;

TEST_CASE("Rational parsing: numerator and denominator signs", "[parser][rational]") {
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
        auto expr = parse_expression(c.input);
        REQUIRE(expr);
        REQUIRE(std::holds_alternative<Rational>(*expr));
        auto r = std::get<Rational>(*expr);
        CHECK(r.numerator == c.num);
        CHECK(r.denominator == c.den);
    }
}

TEST_CASE("Rational parsing: reduction to lowest terms", "[parser][rational]") {
    struct Case { std::string input; int64_t num, den; };
    std::vector<Case> cases = {
        {"Rational[6,8]", 3, 4},
        {"Rational[100,250]", 2, 5},
        {"Rational[-10,20]", -1, 2},
        {"Rational[  8 ,  12 ]", 2, 3}
    };
    for (const auto& c : cases) {
        auto expr = parse_expression(c.input);
        REQUIRE(expr);
        REQUIRE(std::holds_alternative<Rational>(*expr));
        auto r = std::get<Rational>(*expr);
        CHECK(r.numerator == c.num);
        CHECK(r.denominator == c.den);
    }
}

TEST_CASE("Rational parsing: large values", "[parser][rational]") {
    auto expr = parse_expression("Rational[123456789,987654321]");
    REQUIRE(expr);
    REQUIRE(std::holds_alternative<Rational>(*expr));
    auto r = std::get<Rational>(*expr);
    CHECK(r.numerator == 13717421);
    CHECK(r.denominator == 109739369);
}

TEST_CASE("Rational parsing: invalid input", "[parser][rational]") {
    SECTION("Denominator zero") {
        auto expr = parse_expression("Rational[1,0]");
        REQUIRE(expr);
        CHECK(std::holds_alternative<Infinity>(*expr));
    }
    SECTION("Zero over zero is Indeterminate") {
        auto expr = parse_expression("Rational[0,0]");
        REQUIRE(expr);
        CHECK(std::holds_alternative<Indeterminate>(*expr));
    }
    SECTION("Non-integer numerator") {
        auto expr = parse_expression("Rational[1.5,2]");
        CHECK_FALSE(std::holds_alternative<Rational>(*expr));
    }
    SECTION("Non-integer denominator") {
        auto expr = parse_expression("Rational[2,2.5]");
        CHECK_FALSE(std::holds_alternative<Rational>(*expr));
    }
}

TEST_CASE("Parser: rational expressions with operators and mixed types", "[parser][rational][operators]") {
    struct OpCase {
        std::string input;
        std::string expected_head;
        std::vector<std::pair<std::string, std::pair<int64_t, int64_t>>> rationals; // {type, {num, den}}
        std::vector<std::pair<std::string, double>> numbers; // {type, value}
    };
    std::vector<OpCase> cases = {
        {"1/2 + 1/3", "Plus", {{"left", {1,2}}, {"right", {1,3}}}, {}},
        {"2 - 3/4", "Minus", {{"right", {3,4}}}, {{"left", 2.0}}},
        {"0.5 * 2/3", "Times", {{"right", {2,3}}}, {{"left", 0.5}}},
        {"3/4 / 5/6", "Divide", {{"left", {3,4}}, {"right", {5,6}}}, {}},
        {"(2/3)^4", "Power", {{"left", {2,3}}}, {{"right", 4.0}}},
        {"-2/5 + 1/5", "Plus", {{"left", {-2,5}}, {"right", {1,5}}}, {}},
        {"1 + -2/5", "Plus", {{"right", {-2,5}}}, {{"left", 1.0}}}
    };

    for (size_t i = 0; i < cases.size(); ++i) {
        const auto& c = cases[i];
        INFO("Test input: " << c.input << " (case #" << i << ")");
        auto expr = parse_expression(c.input);
        REQUIRE(expr);
        REQUIRE(std::holds_alternative<FunctionCall>(*expr));
        const auto& f = std::get<FunctionCall>(*expr);
        CHECK(f.head == c.expected_head);

        // Check rationals
        for (const auto& rat : c.rationals) {
            size_t idx = (rat.first == "left") ? 0 : 1;
            INFO("Checking rational at " << rat.first << " for input: " << c.input);
            REQUIRE(f.args.size() > idx);
            REQUIRE(std::holds_alternative<Rational>(*f.args[idx]));
            auto r = std::get<Rational>(*f.args[idx]);
            CHECK(r.numerator == rat.second.first);
            CHECK(r.denominator == rat.second.second);
        }
        // Check numbers
        for (const auto& num : c.numbers) {
            size_t idx = (num.first == "left") ? 0 : 1;
            INFO("Checking number at " << num.first << " for input: " << c.input);
            REQUIRE(f.args.size() > idx);
            REQUIRE(std::holds_alternative<Number>(*f.args[idx]));
            auto n = std::get<Number>(*f.args[idx]);
            CHECK(n.value == Catch::Approx(num.second));
        }
    }
}

TEST_CASE("Parser: rational expressions with all sign combinations and implicit multiplication", "[parser][rational][signs][implicit-mul]") {
    struct Case {
        std::string input;
        int64_t num, den;
        std::string var; // empty if no variable
        bool negate_outer = false; // true if Negate wraps Times or Rational
    };
    std::vector<Case> cases = {
        // Simple rationals
        {"2/3", 2, 3, "", false},           // Rational[2,3]
        {"-2/3", -2, 3, "", false},         // Rational[-2,3]
        {"2/-3", -2, 3, "", false},         // Rational[-2,3]
        {"-2/-3", 2, 3, "", false},         // Rational[2,3]
        // With variable, implicit multiplication
        {"2/3x", 2, 3, "x", false},         // Times[Rational[2,3], x]
        {"-2/3x", -2, 3, "x", false},       // Times[Rational[-2,3], x]
        {"2/-3x", -2, 3, "x", false},       // Times[Rational[-2,3], x]
        {"-2/-3x", 2, 3, "x", false},       // Times[Rational[2,3], x]
        // With variable, explicit multiplication
        {"2/3*x", 2, 3, "x", false},        // Times[Rational[2,3], x]
        {"-2/3*x", -2, 3, "x", false},      // Times[Rational[-2,3], x]
        {"2/-3*x", -2, 3, "x", false},      // Times[Rational[-2,3], x]
        {"-2/-3*x", 2, 3, "x", false},      // Times[Rational[2,3], x]
        // Parentheses
        {"-(2/3)x", -2, 3, "x", false},     // Times[Rational[-2,3], x]
        {"(-2/3)x", -2, 3, "x", false},     // Times[Rational[-2,3], x]
        {"(2/-3)x", -2, 3, "x", false},     // Times[Rational[-2,3], x]
        {"(-2/-3)x", 2, 3, "x", false},     // Times[Rational[2,3], x]
        // Variable numerator/denominator (should parse as Divide)
        {"a/b", 0, 0, "", false},           // not a rational, skip check
        {"-a/b", 0, 0, "", false},          // not a rational, skip check
    };

    for (const auto& c : cases) {
        INFO("Input: " << c.input);
        auto expr = parse_expression(c.input);
        REQUIRE(expr);

        // Variable numerator/denominator: skip rational check
        if (c.input == "a/b" || c.input == "-a/b") {
            REQUIRE(std::holds_alternative<FunctionCall>(*expr));
            continue;
        }

        if (!c.var.empty()) {
            // With variable: should be Times or Negate[Times]
            if (c.negate_outer) {
                auto* neg = std::get_if<FunctionCall>(&(*expr));
                REQUIRE(neg);
                INFO("Expected Negate at top level for input: " << c.input);
                REQUIRE(neg->head == "Negate");
                REQUIRE(neg->args.size() == 1);
                auto* times = std::get_if<FunctionCall>(&(*neg->args[0]));
                REQUIRE(times);
                INFO("Expected Times inside Negate for input: " << c.input);
                REQUIRE(times->head == "Times");
                REQUIRE(times->args.size() == 2);
                auto* rat = std::get_if<Rational>(&(*times->args[0]));
                REQUIRE(rat);
                INFO("Expected Rational as first argument of Times for input: " << c.input);
                CHECK(rat->numerator == c.num);
                CHECK(rat->denominator == c.den);
                auto* sym = std::get_if<Symbol>(&(*times->args[1]));
                REQUIRE(sym);
                INFO("Expected Symbol as second argument of Times for input: " << c.input);
                CHECK(sym->name == c.var);
            }
            else {
                auto* times = std::get_if<FunctionCall>(&(*expr));
                REQUIRE(times);
                INFO("Expected Times at top level for input: " << c.input);
                REQUIRE(times->head == "Times");
                REQUIRE(times->args.size() == 2);
                auto* rat = std::get_if<Rational>(&(*times->args[0]));
                REQUIRE(rat);
                INFO("Expected Rational as first argument of Times for input: " << c.input);
                CHECK(rat->numerator == c.num);
                CHECK(rat->denominator == c.den);
                auto* sym = std::get_if<Symbol>(&(*times->args[1]));
                REQUIRE(sym);
                INFO("Expected Symbol as second argument of Times for input: " << c.input);
                CHECK(sym->name == c.var);
            }
        }
        else {
            // No variable: should be Rational or Negate[Rational]
            if (c.negate_outer) {
                auto* neg = std::get_if<FunctionCall>(&(*expr));
                REQUIRE(neg);
                INFO("Expected Negate at top level for input: " << c.input);
                REQUIRE(neg->head == "Negate");
                REQUIRE(neg->args.size() == 1);
                auto* rat = std::get_if<Rational>(&(*neg->args[0]));
                REQUIRE(rat);
                INFO("Expected Rational inside Negate for input: " << c.input);
                CHECK(rat->numerator == c.num);
                CHECK(rat->denominator == c.den);
            }
            else {
                REQUIRE(std::holds_alternative<Rational>(*expr));
                auto rat = std::get<Rational>(*expr);
                INFO("Expected Rational for input: " << c.input);
                CHECK(rat.numerator == c.num);
                CHECK(rat.denominator == c.den);
            }
        }
    }
}