#include "parser/Parser.hpp"
#include "transforms/Transforms.hpp"
#include "expr/ExprUtils.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

namespace {

void expect_simplifies_to(const std::string& source, const std::string& expected) {
    auto expr = parse_expression(source);
    auto simplified = simplify(expr);
    REQUIRE(to_string(simplified) == expected);
    REQUIRE(to_string(simplify(simplified)) == expected);
}

void expect_direct_simplify_to(const std::string& source, const std::string& expected) {
    auto expr = parse_expression(source);
    auto simplified = simplify(expr);
    REQUIRE(to_string(simplified) == expected);
    REQUIRE(to_string(simplify(simplified)) == expected);
}

}  // namespace

TEST_CASE("Simplify relational functions to True/False", "[simplify]") {
    auto expr = make_expr<FunctionCall>("Equal", std::vector<ExprPtr>{
        make_number(2), make_number(2)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("NotEqual", std::vector<ExprPtr>{
        make_number(2), make_number(3)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("Less", std::vector<ExprPtr>{
        make_number(2), make_number(3)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("Greater", std::vector<ExprPtr>{
        make_number(3), make_number(2)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("LessEqual", std::vector<ExprPtr>{
        make_number(2), make_number(2)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("GreaterEqual", std::vector<ExprPtr>{
        make_number(3), make_number(2)
    });
    REQUIRE(to_string(simplify(expr)) == "True");
}

TEST_CASE("Simplify removes additive and multiplicative neutral elements", "[simplify]") {
    expect_simplifies_to("x + 0", "x");
    expect_simplifies_to("0 + x", "x");
    expect_simplifies_to("x * 1", "x");
    expect_simplifies_to("1 * x", "x");
    expect_simplifies_to("x * 0", "0");
    expect_simplifies_to("0 * x", "0");
}

TEST_CASE("Simplify collapses basic power identities and numeric powers", "[simplify]") {
    expect_simplifies_to("x^0", "1");
    expect_simplifies_to("x^1", "x");
    expect_simplifies_to("1^x", "1");
    expect_simplifies_to("2^3", "8");
}

TEST_CASE("Simplify combines constants and like terms", "[simplify]") {
    expect_simplifies_to("2 + 3 + 4", "9");
    expect_simplifies_to("2 * 3 * 0", "0");
    expect_simplifies_to("2*x + 3*x", "5 * x");
}

TEST_CASE("Simplify preserves symbolic structure when no numeric reduction applies", "[simplify]") {
    expect_simplifies_to("x == y", "x == y");
    expect_simplifies_to("x + y", "x + y");
    expect_simplifies_to("x * y", "x * y");
}

TEST_CASE("Simplify is idempotent on nested symbolic expressions", "[simplify]") {
    expect_simplifies_to("0 + (1 * x)", "x");
    expect_simplifies_to("(x * 0) + 1", "1");
    expect_simplifies_to("(x * 1) + (2*x + 0)", "3 * x");
}

TEST_CASE("Simplify does not perform evaluator-only builtin reduction on raw input", "[simplify][contract]") {
    expect_direct_simplify_to("Sin[0]", "Sin[0]");
    expect_direct_simplify_to("Gamma[6]", "Gamma[6]");
    expect_direct_simplify_to("mystery[2 + 3]", "mystery[2 + 3]");
}

TEST_CASE("Simplify preserves exact arithmetic boundaries on raw input", "[simplify][contract][rational]") {
    expect_direct_simplify_to("1/2 + 1/3", "1/2 + 1/3");
    expect_direct_simplify_to("1/2 + 2", "2 + 1/2");
    expect_direct_simplify_to("1/2 < 3/4", "1/2 < 3/4");
}

TEST_CASE("Simplify only distributes product powers for safe positive integer exponents", "[simplify][contract][power]") {
    expect_simplifies_to("(x * y)^2", "x^2 * y^2");
    expect_simplifies_to("(x * y)^3", "x^3 * y^3");
    expect_simplifies_to("(x * y)^0.5", "(x * y)^0.5");
    expect_simplifies_to("(x * y)^(1/2)", "(x * y)^1/2");
    expect_simplifies_to("(x * y)^-1", "(x * y)^-1");
}

TEST_CASE("Simplify keeps opaque arguments structurally intact", "[simplify][contract]") {
    expect_simplifies_to("f[x + 0]", "f[x + 0]");
    expect_simplifies_to("f[1 * x]", "f[1 * x]");
}
