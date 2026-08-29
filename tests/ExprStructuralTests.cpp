#include "expr/Expr.hpp"
#include "expr/ExprStructural.hpp"
#include "kernel/DomainRestrictions.hpp"
#include "kernel/Rewrite.hpp"
#include "kernel/VariableAnalysis.hpp"
#include "normalizer/Normalizer.hpp"
#include "parser/Parser.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <unordered_set>
#include <type_traits>
#include <utility>

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
    REQUIRE(structural_equal(left, right));
    REQUIRE(structural_equal(right, left));
    REQUIRE(structural_hash(left) == structural_hash(right));
    REQUIRE(ExprHash{}(left) == ExprHash{}(right));
}

void require_structurally_unequal(const ExprPtr& left, const ExprPtr& right) {
    REQUIRE_FALSE(structural_equal(left, right));
    REQUIRE_FALSE(structural_equal(right, left));
}

void require_structural_order(const ExprPtr& left, const ExprPtr& right) {
    REQUIRE(structural_less(left, right));
    REQUIRE_FALSE(structural_less(right, left));
    REQUIRE(ExprStructuralLess{}(left, right));
    REQUIRE_FALSE(ExprStructuralLess{}(right, left));
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
    REQUIRE(structural_hash(empty) != structural_hash(symbol("x")));
}

TEST_CASE("Structural hashes distinguish representative unequal expressions", "[expr][structural]") {
    REQUIRE(structural_hash(symbol("x")) != structural_hash(symbol("y")));
    REQUIRE(structural_hash(number(1.0)) != structural_hash(number(2.0)));
    REQUIRE(structural_hash(make_expr<Rational>(1, 2)) != structural_hash(make_expr<Rational>(2, 3)));
    REQUIRE(structural_hash(call("f", {symbol("x")})) != structural_hash(call("g", {symbol("x")})));
    REQUIRE(structural_hash(call("f", {symbol("x")})) != structural_hash(call("f", {symbol("x"), symbol("y")})));
    REQUIRE(structural_hash(call("Plus", {symbol("x"), symbol("y")})) !=
            structural_hash(call("Plus", {symbol("y"), symbol("x")})));
}

TEST_CASE("Structural hashes support unordered expression sets", "[expr][structural]") {
    std::unordered_set<ExprPtr, ExprHash, ExprEqual> expressions;
    const auto inserted = expressions.insert(call("f", {symbol("x"), number(1.0)}));

    REQUIRE(inserted.second);
    REQUIRE_FALSE(expressions.insert(call("f", {symbol("x"), number(1.0)})).second);
    REQUIRE(expressions.insert(call("f", {symbol("x"), number(2.0)})).second);
}

TEST_CASE("Structural ordering compares representative expression values deterministically", "[expr][structural]") {
    ExprPtr empty;

    require_structural_order(empty, symbol("x"));
    require_structural_order(symbol("a"), symbol("b"));
    require_structural_order(number(1.0), number(2.0));
    require_structural_order(make_expr<Rational>(1, 2), make_expr<Rational>(2, 3));
    require_structural_order(make_expr<Boolean>(false), make_expr<Boolean>(true));
    require_structural_order(make_expr<String>("a"), make_expr<String>("b"));
    require_structural_order(make_expr<Complex>(1.0, 2.0), make_expr<Complex>(2.0, 1.0));
    require_structural_order(make_expr<Infinity>(), make_expr<ComplexInfinity>());
}

TEST_CASE("Structural ordering compares compound expression fields recursively", "[expr][structural]") {
    require_structural_order(
        call("f", {symbol("x")}),
        call("g", {symbol("x")}));
    require_structural_order(
        call("f", {symbol("x")}),
        call("f", {symbol("x"), symbol("y")}));
    require_structural_order(
        call("f", {symbol("x"), symbol("a")}),
        call("f", {symbol("x"), symbol("b")}));

    require_structural_order(
        make_expr<List>(std::vector<ExprPtr>{symbol("a")}),
        make_expr<List>(std::vector<ExprPtr>{symbol("a"), symbol("b")}));
    require_structural_order(
        make_expr<Rule>(symbol("x"), number(1.0)),
        make_expr<Rule>(symbol("x"), number(2.0)));
    require_structural_order(
        make_expr<Assignment>("a", number(1.0)),
        make_expr<Assignment>("b", number(1.0)));

    require_structural_order(
        make_expr<FunctionDefinition>(
            "f",
            std::vector<Parameter>{Parameter("x", number(1.0))},
            symbol("x"),
            false),
        make_expr<FunctionDefinition>(
            "f",
            std::vector<Parameter>{Parameter("x", number(2.0))},
            symbol("x"),
            false));
}

TEST_CASE("Structural ordering supports ordered expression sets", "[expr][structural]") {
    std::set<ExprPtr, ExprStructuralLess> expressions;

    REQUIRE(expressions.insert(call("f", {symbol("x"), number(1.0)})).second);
    REQUIRE_FALSE(expressions.insert(call("f", {symbol("x"), number(1.0)})).second);
    REQUIRE(expressions.insert(call("f", {symbol("x"), number(2.0)})).second);
    REQUIRE(expressions.insert(call("f", {symbol("y"), number(1.0)})).second);
    REQUIRE(expressions.size() == 3);
}

TEST_CASE("Kernel structural equality delegates to expression-owned structural equality", "[expr][structural][rewrite]") {
    const auto left = call("f", {symbol("x"), number(1.0)});
    const auto same = call("f", {symbol("x"), number(1.0)});
    const auto different = call("f", {symbol("x"), number(2.0)});

    REQUIRE(kernel::structurally_equal(left, same));
    REQUIRE_FALSE(kernel::structurally_equal(left, different));
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

TEST_CASE("Shared and independently constructed equal trees hash equally", "[expr][structural]") {
    const auto shared_arg = call("Plus", {symbol("x"), number(1.0)});
    const auto shared_tree = call("f", {shared_arg, shared_arg});
    const auto independent_tree = call("f", {
        call("Plus", {symbol("x"), number(1.0)}),
        call("Plus", {symbol("x"), number(1.0)})
    });

    require_structurally_equal(shared_tree, independent_tree);
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

TEST_CASE("Normalized permutations have equal structural hashes", "[expr][structural][normalizer]") {
    for (const auto& [left, right] : {
             std::pair{"y + x + 2", "2 + x + y"},
             std::pair{"z * x * 2 * y", "y * 2 * z * x"},
             std::pair{"{y + x, z * x * 2}", "{x + y, 2 * x * z}"}}) {
        DYNAMIC_SECTION(left << " == " << right) {
            require_structurally_equal(
                normalize_expr(parse_expression(left)),
                normalize_expr(parse_expression(right)));
        }
    }
}

TEST_CASE("Domain restriction metadata deduplicates and orders by expression structure", "[expr][structural][domain]") {
    kernel::DomainRestrictions restrictions;

    restrictions.add_excluded_zero(normalize_expr(parse_expression("x + y*z")));
    restrictions.add_excluded_zero(normalize_expr(parse_expression("(x + y) * z")));
    restrictions.add_excluded_zero(normalize_expr(parse_expression("y*z + x")));

    REQUIRE(restrictions.excluded_zero_expressions.size() == 2);
    REQUIRE(structural_equal(
        restrictions.excluded_zero_expressions[0],
        normalize_expr(parse_expression("x + y*z"))));
    REQUIRE(structural_equal(
        restrictions.excluded_zero_expressions[1],
        normalize_expr(parse_expression("(x + y) * z"))));
}

TEST_CASE("Rewrite shares immutable expressions when no rewrite occurs", "[expr][structural][sharing][rewrite]") {
    const auto expr = call("f", {symbol("x")});
    const Rule rule{symbol("y"), symbol("z")};

    const auto result = kernel::rewrite_once(expr, rule);

    REQUIRE_FALSE(result.changed);
    REQUIRE(result.expr == expr);
}

TEST_CASE("Rewrite pattern substitution shares bound immutable expressions", "[expr][structural][sharing][rewrite]") {
    const auto shared_arg = call("Plus", {symbol("x"), number(1.0)});
    const auto expr = call("f", {shared_arg});
    const Rule rule{call("f", {symbol("a_")}), symbol("a")};

    const auto result = kernel::rewrite_once(expr, rule);

    REQUIRE(result.changed);
    REQUIRE(result.expr == shared_arg);
}

TEST_CASE("Rewrite parent rebuild preserves unchanged child pointers", "[expr][structural][sharing][rewrite]") {
    const auto shared_arg = call("Plus", {symbol("x"), number(1.0)});
    const auto expr = call("f", {shared_arg, symbol("y")});
    const Rule rule{symbol("y"), symbol("z")};

    const auto result = kernel::rewrite_once(expr, rule);

    REQUIRE(result.changed);
    const auto* rewritten = std::get_if<FunctionCall>(result.expr.get());
    REQUIRE(rewritten != nullptr);
    REQUIRE(rewritten->args.size() == 2);
    REQUIRE(rewritten->args[0] == shared_arg);
    REQUIRE(structural_equal(rewritten->args[1], symbol("z")));
}

TEST_CASE("Capture-safe substitution shares immutable replacements", "[expr][structural][sharing][variables]") {
    const auto replacement = call("Plus", {symbol("x"), number(1.0)});
    kernel::SymbolSubstitutionMap substitutions;
    substitutions.emplace("y", replacement);

    const auto substituted = kernel::substitute_symbols_capture_safe(
        symbol("y"),
        substitutions);

    REQUIRE(substituted == replacement);
}

TEST_CASE("Capture-safe substitution shares unchanged captured symbols", "[expr][structural][sharing][variables]") {
    const auto expr = make_expr<FunctionDefinition>(
        "f",
        std::vector<Parameter>{Parameter("x")},
        symbol("y"),
        true);
    kernel::SymbolSubstitutionMap substitutions;
    substitutions.emplace("y", symbol("x"));

    const auto substituted = kernel::substitute_symbols_capture_safe(expr, substitutions);

    const auto* original = std::get_if<FunctionDefinition>(expr.get());
    const auto* rewritten = std::get_if<FunctionDefinition>(substituted.get());
    REQUIRE(original != nullptr);
    REQUIRE(rewritten != nullptr);
    REQUIRE(rewritten->body == original->body);
}
