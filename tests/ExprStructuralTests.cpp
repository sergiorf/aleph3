#include "expr/Expr.hpp"
#include "kernel/Rewrite.hpp"
#include "normalizer/Normalizer.hpp"
#include "parser/Parser.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace aleph3;

namespace {

static_assert(std::is_const_v<ExprPtr::element_type>);

ExprPtr symbol(std::string name) {
    return make_expr<Symbol>(std::move(name));
}

ExprPtr number(double value) {
    return make_expr<Number>(value);
}

ExprPtr call(std::string head, std::initializer_list<ExprPtr> args) {
    return make_expr<FunctionCall>(std::move(head), std::vector<ExprPtr>(args));
}

void require_structurally_equal(const ExprPtr& left, const ExprPtr& right) {
    REQUIRE(kernel::structurally_equal(left, right));
    REQUIRE(kernel::structurally_equal(right, left));
}

void require_structurally_unequal(const ExprPtr& left, const ExprPtr& right) {
    REQUIRE_FALSE(kernel::structurally_equal(left, right));
    REQUIRE_FALSE(kernel::structurally_equal(right, left));
}

void require_normalization_idempotent(std::string source) {
    const auto once = normalize_expr(parse_expression(source));
    const auto twice = normalize_expr(once);
    require_structurally_equal(once, twice);
}

}  // namespace

TEST_CASE("Structural equality compares atomic expression values", "[expr][structural]") {
    require_structurally_equal(symbol("x"), symbol("x"));
    require_structurally_unequal(symbol("x"), symbol("y"));

    require_structurally_equal(number(1.0), number(1.0));
    require_structurally_unequal(number(1.0), number(2.0));

    require_structurally_equal(make_expr<Rational>(1, 2), make_expr<Rational>(1, 2));
    require_structurally_unequal(make_expr<Rational>(1, 2), make_expr<Rational>(2, 3));

    require_structurally_equal(make_expr<Boolean>(true), make_expr<Boolean>(true));
    require_structurally_unequal(make_expr<Boolean>(true), make_expr<Boolean>(false));

    require_structurally_equal(make_expr<String>("a"), make_expr<String>("a"));
    require_structurally_unequal(make_expr<String>("a"), make_expr<String>("b"));

    require_structurally_equal(make_expr<Complex>(1.0, 2.0), make_expr<Complex>(1.0, 2.0));
    require_structurally_unequal(make_expr<Complex>(1.0, 2.0), make_expr<Complex>(2.0, 1.0));

    require_structurally_equal(make_expr<Infinity>(), make_expr<Infinity>());
    require_structurally_equal(make_expr<ComplexInfinity>(), make_expr<ComplexInfinity>());
    require_structurally_equal(make_expr<Indeterminate>(), make_expr<Indeterminate>());
    require_structurally_unequal(make_expr<Infinity>(), make_expr<ComplexInfinity>());
}

TEST_CASE("Structural equality handles null expression pointers explicitly", "[expr][structural]") {
    ExprPtr empty;

    require_structurally_equal(empty, nullptr);
    require_structurally_unequal(empty, symbol("x"));
}

TEST_CASE("Structural equality compares function heads, arity, and ordered arguments", "[expr][structural]") {
    require_structurally_equal(
        call("Sin", {symbol("x")}),
        call("Sin", {symbol("x")}));
    require_structurally_unequal(
        call("Sin", {symbol("x")}),
        call("Cos", {symbol("x")}));

    require_structurally_equal(
        call("Power", {symbol("x"), number(2.0)}),
        call("Power", {symbol("x"), number(2.0)}));
    require_structurally_unequal(
        call("Power", {symbol("x"), number(2.0)}),
        call("Power", {symbol("x"), number(3.0)}));

    require_structurally_equal(
        call("Plus", {symbol("x"), symbol("y")}),
        call("Plus", {symbol("x"), symbol("y")}));
    require_structurally_unequal(
        call("Plus", {symbol("x"), symbol("y")}),
        call("Plus", {symbol("y"), symbol("x")}));
    require_structurally_unequal(
        call("f", {symbol("x")}),
        call("f", {symbol("x"), symbol("y")}));
}

TEST_CASE("Structural equality is not algebraic equivalence", "[expr][structural]") {
    require_structurally_unequal(
        call("Plus", {symbol("x"), symbol("y")}),
        call("Plus", {symbol("y"), symbol("x")}));
    require_structurally_equal(
        normalize_expr(call("Plus", {symbol("x"), symbol("y")})),
        normalize_expr(call("Plus", {symbol("y"), symbol("x")})));

    require_structurally_unequal(
        call("Plus", {symbol("x"), symbol("x")}),
        call("Times", {number(2.0), symbol("x")}));
}

TEST_CASE("Structural equality compares nested expressions and lists", "[expr][structural]") {
    const auto left = call("f", {
        call("Plus", {symbol("x"), number(1.0)}),
        make_expr<List>(std::vector<ExprPtr>{symbol("a"), symbol("b")})
    });
    const auto same = call("f", {
        call("Plus", {symbol("x"), number(1.0)}),
        make_expr<List>(std::vector<ExprPtr>{symbol("a"), symbol("b")})
    });
    const auto different = call("f", {
        call("Plus", {symbol("x"), number(1.0)}),
        make_expr<List>(std::vector<ExprPtr>{symbol("b"), symbol("a")})
    });

    require_structurally_equal(left, same);
    require_structurally_unequal(left, different);
}

TEST_CASE("Structural equality compares rules and assignments", "[expr][structural]") {
    require_structurally_equal(
        make_expr<Rule>(symbol("x"), number(1.0)),
        make_expr<Rule>(symbol("x"), number(1.0)));
    require_structurally_unequal(
        make_expr<Rule>(symbol("x"), number(1.0)),
        make_expr<Rule>(symbol("y"), number(1.0)));
    require_structurally_unequal(
        make_expr<Rule>(symbol("x"), number(1.0)),
        make_expr<Rule>(symbol("x"), number(2.0)));

    require_structurally_equal(
        make_expr<Assignment>("x", number(1.0)),
        make_expr<Assignment>("x", number(1.0)));
    require_structurally_unequal(
        make_expr<Assignment>("x", number(1.0)),
        make_expr<Assignment>("y", number(1.0)));
    require_structurally_unequal(
        make_expr<Assignment>("x", number(1.0)),
        make_expr<Assignment>("x", number(2.0)));
}

TEST_CASE("Structural equality compares function definitions and default parameters", "[expr][structural]") {
    const auto body = call("Plus", {symbol("x"), symbol("offset")});

    const auto left = make_expr<FunctionDefinition>(
        "f",
        std::vector<Parameter>{Parameter("x"), Parameter("offset", number(1.0))},
        body,
        true);
    const auto same = make_expr<FunctionDefinition>(
        "f",
        std::vector<Parameter>{Parameter("x"), Parameter("offset", number(1.0))},
        body,
        true);
    const auto different_default = make_expr<FunctionDefinition>(
        "f",
        std::vector<Parameter>{Parameter("x"), Parameter("offset", number(2.0))},
        body,
        true);
    const auto different_timing = make_expr<FunctionDefinition>(
        "f",
        std::vector<Parameter>{Parameter("x"), Parameter("offset", number(1.0))},
        body,
        false);

    require_structurally_equal(left, same);
    require_structurally_unequal(left, different_default);
    require_structurally_unequal(left, different_timing);
}

TEST_CASE("Normalization remains structurally idempotent for representative expressions", "[expr][structural][normalizer]") {
    for (const auto* source : {
             "y + x + 2",
             "z * x * 2 * y",
             "(y + x)^2",
             "{y + x, z * x * 2}",
             "x -> y + z",
             "f[a_:y + x] := z * x * 2",
             "Sin[y] + x^2 + x + 2",
             "x * x^2 * y"}) {
        DYNAMIC_SECTION(source) {
            require_normalization_idempotent(source);
        }
    }
}
