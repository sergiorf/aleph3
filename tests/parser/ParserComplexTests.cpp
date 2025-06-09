#include <catch2/catch_test_macros.hpp>
#include "parser/Parser.hpp"
#include "expr/Expr.hpp"

using namespace aleph3;

TEST_CASE("Parser: Complex number literals", "[parser][complex]") {
    SECTION("Complex[re, im] form") {
        auto expr = parse_expression("Complex[3, 4]");
        REQUIRE(expr);
        REQUIRE(std::holds_alternative<Complex>(*expr));
        const auto& c = std::get<Complex>(*expr);
        CHECK(c.real == 3.0);
        CHECK(c.imag == 4.0);
    }

    SECTION("Pure imaginary (I)") {
        auto expr = parse_expression("I");
        REQUIRE(expr);
        REQUIRE(std::holds_alternative<Complex>(*expr));
        const auto& c = std::get<Complex>(*expr);
        CHECK(c.real == 0.0);
        CHECK(c.imag == 1.0);
    }

    SECTION("Negative imaginary (-I)") {
        auto expr = parse_expression("-I");
        REQUIRE(expr);
        REQUIRE(std::holds_alternative<FunctionCall>(*expr));
        const auto& neg = std::get<FunctionCall>(*expr);
        CHECK(neg.head == "Negate");
        REQUIRE(neg.args.size() == 1);
        REQUIRE(std::holds_alternative<Complex>(*neg.args[0]));
        const auto& c = std::get<Complex>(*neg.args[0]);
        CHECK(c.real == 0.0);
        CHECK(c.imag == 1.0);
    }

    SECTION("a + b*I form") {
        auto expr = parse_expression("3 + 4*I");
        REQUIRE(expr);
        REQUIRE(std::holds_alternative<Complex>(*expr));
        const auto& c = std::get<Complex>(*expr);
        CHECK(c.real == 3.0);
        CHECK(c.imag == 4.0);
    }

    SECTION("a - b*I form") {
        auto expr = parse_expression("2 - 5*I");
        REQUIRE(expr);
        REQUIRE(std::holds_alternative<FunctionCall>(*expr));
        const auto& minus = std::get<FunctionCall>(*expr);
        CHECK(minus.head == "Minus");
        REQUIRE(minus.args.size() == 2);
        // Check left is 2
        REQUIRE(std::holds_alternative<Number>(*minus.args[0]));
        const auto& n = std::get<Number>(*minus.args[0]);
        CHECK(n.value == 2.0);
        // Check right is Times[5, I]
        REQUIRE(std::holds_alternative<FunctionCall>(*minus.args[1]));
        const auto& times = std::get<FunctionCall>(*minus.args[1]);
        CHECK(times.head == "Times");
        REQUIRE(times.args.size() == 2);
        REQUIRE(std::holds_alternative<Number>(*times.args[0]));
        CHECK(std::get<Number>(*times.args[0]).value == 5.0);
        REQUIRE(std::holds_alternative<Complex>(*times.args[1]));
        const auto& c = std::get<Complex>(*times.args[1]);
        CHECK(c.real == 0.0);
        CHECK(c.imag == 1.0);
    }

    SECTION("Pure real") {
        auto expr = parse_expression("7");
        REQUIRE(expr);
        REQUIRE(std::holds_alternative<Number>(*expr));
        const auto& n = std::get<Number>(*expr);
        CHECK(n.value == 7.0);
    }
}