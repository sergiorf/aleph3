#include "parser/Parser.hpp"
#include "transforms/Transforms.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
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

std::string evaluated_string(const std::string& source) {
    EvaluationContext ctx(kernel::default_function_registry());
    return to_string(evaluate(parse_expression(source), ctx));
}

void expect_evaluates_to(const std::string& source, const std::string& expected) {
    REQUIRE(evaluated_string(source) == expected);
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
    expect_simplifies_to("x - 0", "x");
    expect_simplifies_to("0 + x + y", "x + y");
    expect_simplifies_to("x * 1", "x");
    expect_simplifies_to("1 * x", "x");
    expect_simplifies_to("1 * x * y", "x * y");
    expect_simplifies_to("x * 0", "0");
    expect_simplifies_to("0 * x", "0");
    expect_simplifies_to("0 * Sin[x] * y", "0");
}

TEST_CASE("Simplify prints negative identity products without explicit unit factors", "[simplify][times]") {
    auto symbol = to_string(simplify(parse_expression("-1 * x")));
    auto call = to_string(simplify(parse_expression("-1 * Sin[x]")));
    auto product = to_string(simplify(parse_expression("-1 * x * y")));

    REQUIRE(symbol.find("-1 *") == std::string::npos);
    REQUIRE(call.find("-1 *") == std::string::npos);
    REQUIRE(product.find("-1 *") == std::string::npos);
}

TEST_CASE("Simplify collapses basic power identities and numeric powers", "[simplify]") {
    expect_simplifies_to("x^0", "x^0");
    expect_simplifies_to("x^1", "x");
    expect_simplifies_to("1^x", "1");
    expect_simplifies_to("0^3", "0");
    expect_simplifies_to("2^3", "8");
    expect_simplifies_to("(x^2)^3", "x^6");
    expect_simplifies_to("Sin[x]^0", "(Sin[x])^0");
    expect_simplifies_to("Exp[x]^1", "Exp[x]");
    expect_evaluates_to("Assuming[x != 0, Simplify[x^0]]", "1");
    expect_evaluates_to("Simplify[0^0]", "0^0");
    expect_evaluates_to("Simplify[0^-1]", "0^-1");
}

TEST_CASE("Simplify combines constants and like terms", "[simplify]") {
    expect_simplifies_to("2 + 3 + 4", "9");
    expect_simplifies_to("2 * 3 * 0", "0");
    expect_simplifies_to("2 * x * 3", "6 * x");
    expect_simplifies_to("2 * x * 3 * y", "6 * x * y");
    expect_simplifies_to("-2 * x * 3", "-6 * x");
    expect_simplifies_to("(1/2) * 4 * x", "2 * x");
    expect_simplifies_to("2*x + 3*x", "5 * x");
    expect_simplifies_to("x/2 + x/3", "5/6 * x");
    expect_simplifies_to("2*x * 3*x", "6 * x^2");
    expect_simplifies_to("2*x^2 * 3*x^3", "6 * x^5");
}

TEST_CASE("Simplify combines repeated Times factors and integer powers", "[simplify][times]") {
    expect_simplifies_to("x * x", "x^2");
    expect_simplifies_to("x * x * x", "x^3");
    expect_simplifies_to("Sin[x] * Sin[x]", "(Sin[x])^2");
    expect_simplifies_to("(x + 1) * (x + 1)", "(x + 1)^2");
    expect_simplifies_to("x^2 * x^3", "x^5");
    expect_simplifies_to("x^-1 * x^3", "x^2");
    expect_simplifies_to("x * x^4", "x^5");
    expect_simplifies_to("Sin[x]^2 * Sin[x]", "(Sin[x])^3");
    expect_simplifies_to("x^3 * x^-1", "x^2");
    expect_simplifies_to("x^3 * x^-2", "x");
    expect_simplifies_to("x^-2 * x^5", "x^3");
    expect_simplifies_to("x^-2 * x^-3", "x^-5");
    expect_simplifies_to("Exp[x] * Exp[x]", "(Exp[x])^2");
    expect_simplifies_to("Log[x] * Log[x]", "(Log[x])^2");
    expect_simplifies_to("(x + y) * (x + y)", "(x + y)^2");
    expect_simplifies_to("(x^2 + 1) * (x^2 + 1)", "(x^2 + 1)^2");
}

TEST_CASE("Simplify cancels exact-integer Times powers to one", "[simplify][times][contract]") {
    expect_simplifies_to("x * x^-1", "1");
    expect_simplifies_to("x^-1 * x", "1");
    expect_simplifies_to("x^2 * x^-2", "1");
    expect_simplifies_to("x^-2 * x^2", "1");
    expect_simplifies_to("x^5 * x^-5", "1");
    expect_simplifies_to("x^-5 * x^5", "1");
    expect_evaluates_to("Assuming[x != 0, Simplify[x * x^-1]]", "1");
    expect_evaluates_to("Assuming[x != 0, Simplify[x^5 * x^-5]]", "1");
}

TEST_CASE("Simplify combines mixed multiplicative monomials", "[simplify][times]") {
    expect_simplifies_to("2 * x * x", "2 * x^2");
    expect_simplifies_to("2 * x * x^-1", "2");
    expect_simplifies_to("x * x^-1 * y", "y");
    expect_simplifies_to("x * y * x^-1", "y");
    expect_simplifies_to("2 * x^3 * x^-3 * y", "2 * y");
    expect_simplifies_to("2 * x^2 * x^-1", "2 * x");
    expect_simplifies_to("3 * x^-2 * x^5", "3 * x^3");
    expect_simplifies_to("2*x*y * 3*x*z", "6 * x^2 * y * z");
    expect_simplifies_to("x*y*x", "x^2 * y");
    expect_simplifies_to("x^2*y*x^3*y^2", "x^5 * y^3");
}

TEST_CASE("Simplify partially cancels exact-integer Times powers", "[simplify][times]") {
    expect_simplifies_to("x^2 * x^-1", "x");
    expect_simplifies_to("x^-1 * x^2", "x");
    expect_simplifies_to("x^3 * x^-2", "x");
    expect_simplifies_to("x^-2 * x^5", "x^3");
    expect_simplifies_to("x^5 * x^-2", "x^3");
}

TEST_CASE("Simplify aggregates negative exact-integer Times powers", "[simplify][times]") {
    expect_simplifies_to("x^-1 * x^-1", "x^-2");
    expect_simplifies_to("x^-2 * x^-3", "x^-5");
}

TEST_CASE("Simplify preserves numeric coefficients through exact power cancellation", "[simplify][times]") {
    expect_simplifies_to("2 * x * x^-1", "2");
    expect_simplifies_to("2 * x^2 * 3 * x^-2", "6");
    expect_simplifies_to("-1 * x * x^-1", "-1");
}

TEST_CASE("Simplify cancels compound structurally identical exact-integer bases", "[simplify][times]") {
    expect_simplifies_to("Sin[x] * Sin[x]^-1", "1");
    expect_simplifies_to("Exp[x] * Exp[x]^-1", "1");
    expect_simplifies_to("(x + 1) * (x + 1)^-1", "1");
}

TEST_CASE("Simplify canonicalizes symbolic factor ordering", "[simplify][times][canonical]") {
    expect_simplifies_to("y * x", "x * y");
    expect_simplifies_to("z * x * y", "x * y * z");
    expect_simplifies_to("y^4 * z^4 * x^3", "x^3 * y^4 * z^4");

    const auto first = to_string(simplify(parse_expression("x*y*z")));
    REQUIRE(to_string(simplify(parse_expression("z*y*x"))) == first);
    REQUIRE(to_string(simplify(parse_expression("y*x*z"))) == first);
}

TEST_CASE("Simplify preserves symbolic structure when no numeric reduction applies", "[simplify]") {
    expect_simplifies_to("x == y", "x == y");
    expect_simplifies_to("x + y", "x + y");
    expect_simplifies_to("x * y", "x * y");
    expect_simplifies_to("x^2 + x", "x^2 + x");
    expect_simplifies_to("x*y + x*z", "x * y + x * z");
    expect_simplifies_to("Sin[x] + Cos[x]", "(Cos[x]) + (Sin[x])");
}

TEST_CASE("Simplify is idempotent on nested symbolic expressions", "[simplify]") {
    expect_simplifies_to("0 + (1 * x)", "x");
    expect_simplifies_to("(x * 0) + 1", "1");
    expect_simplifies_to("(x * 1) + (2*x + 0)", "3 * x");
    expect_simplifies_to("2*x*x*x^-1", "2 * x");
    expect_simplifies_to("x*x^-1", "1");
    expect_simplifies_to("2*x^3*x^-3*y", "2 * y");
    expect_simplifies_to("x^-2*x^5", "x^3");
    expect_simplifies_to("x*x*x^-2", "1");
    expect_simplifies_to("2*x + 3*x", "5 * x");
    expect_simplifies_to("x*y*x*z", "x^2 * y * z");
    expect_simplifies_to("(x^2)^3", "x^6");
}

TEST_CASE("Simplify builtin is idempotent after exact power cancellation", "[simplify][times]") {
    for (const auto* source : {
             "x*x^-1",
             "2*x^3*x^-3*y",
             "x^-2*x^5",
             "x*x*x^-2"}) {
        CAPTURE(source);
        REQUIRE(evaluated_string("Simplify[Simplify[" + std::string(source) + "]]") ==
                evaluated_string("Simplify[" + std::string(source) + "]"));
    }
}

TEST_CASE("Simplify combines supported multivariate like terms", "[simplify][plus]") {
    expect_simplifies_to("x + x", "2 * x");
    expect_simplifies_to("2*x + x", "3 * x");
    expect_simplifies_to("2*x + 3*x", "5 * x");
    expect_simplifies_to("x - x", "0");
    expect_simplifies_to("3*x - 2*x", "x");
    expect_simplifies_to("x*y + x*y", "2 * x * y");
    expect_simplifies_to("2*x*y + 3*x*y", "5 * x * y");
    expect_simplifies_to("x^2 + x^2", "2 * x^2");
}

TEST_CASE("Simplify normalizes commutative Plus permutations", "[simplify][plus][canonical]") {
    const auto first = to_string(simplify(parse_expression("x + y + z")));
    REQUIRE(to_string(simplify(parse_expression("z + x + y"))) == first);
    REQUIRE(to_string(simplify(parse_expression("y + z + x"))) == first);
}

TEST_CASE("Simplify does not perform evaluator-only builtin reduction on raw input", "[simplify][contract]") {
    expect_direct_simplify_to("Sin[0]", "Sin[0]");
    expect_direct_simplify_to("Gamma[6]", "Gamma[6]");
    expect_direct_simplify_to("mystery[2 + 3]", "mystery[2 + 3]");
}

TEST_CASE("Simplify preserves exact arithmetic boundaries on raw input", "[simplify][contract][rational]") {
    expect_direct_simplify_to("1/2 + 1/3", "1/2 + 1/3");
    expect_direct_simplify_to("1/2 + 2", "1/2 + 2");
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

TEST_CASE("Simplify preserves unsupported grouped coefficient and exponent contract shapes", "[simplify][contract][rewrite]") {
    expect_direct_simplify_to("(x + y) + 2 * (x + y)", "x + y + 2 * (x + y)");

    auto call_shaped = simplify(parse_expression("f[x] + 2 * f[x]"));
    REQUIRE(std::holds_alternative<FunctionCall>(*call_shaped));
    REQUIRE(to_string(call_shaped) != "3 * f[x]");
    REQUIRE(to_string(simplify(call_shaped)) == to_string(call_shaped));

    auto symbolic_power = simplify(parse_expression("(x^a)^b"));
    REQUIRE(std::holds_alternative<FunctionCall>(*symbolic_power));
    REQUIRE(to_string(symbolic_power) != "x^(a * b)");
    REQUIRE(to_string(simplify(symbolic_power)) == to_string(symbolic_power));
}

TEST_CASE("Simplify terminates on power and sign regressions", "[simplify][regression]") {
    expect_simplifies_to("x^-1", "x^-1");
    expect_simplifies_to("x^-10", "x^-10");
    expect_simplifies_to("x^100", "x^100");
    expect_simplifies_to("0*x^-1", "0");
    expect_simplifies_to("x*x^-1", "1");
    expect_evaluates_to("Simplify[0^0]", "0^0");
    expect_evaluates_to("Simplify[0^-1]", "0^-1");
    expect_simplifies_to("(-1)*(-1)*x", "x");
}
