#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "kernel/VariableAnalysis.hpp"
#include "parser/Parser.hpp"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>

using namespace aleph3;

namespace {

ExprPtr parsed(std::string source) {
    return parse_expression(std::move(source));
}

void require_symbols(const kernel::SymbolSet& actual, kernel::SymbolSet expected) {
    REQUIRE(actual == expected);
}

}  // namespace

TEST_CASE("Kernel variable analysis reports free variables across expression shapes", "[architecture][kernel][variables]") {
    require_symbols(kernel::free_variables(parsed("x + y^2")), {"x", "y"});
    require_symbols(kernel::free_variables(parsed("{x, {y, 2}, \"label\"}")), {"x", "y"});
    require_symbols(kernel::free_variables(parsed("a = x + y")), {"x", "y"});
    require_symbols(kernel::free_variables(parsed("f[x_] := x^2 + y")), {"y"});
    require_symbols(kernel::free_variables(parsed("f[a_] -> g[a, y]")), {"y"});
}

TEST_CASE("Kernel variable analysis reports lexical binders without treating assignments as binders", "[architecture][kernel][variables]") {
    require_symbols(kernel::bound_variables(parsed("a = x")), {});
    require_symbols(kernel::bound_variables(parsed("f[x_] := x + y")), {"x"});
    require_symbols(kernel::bound_variables(parsed("f[a_, b_Integer] -> g[a, b, y]")), {"a", "b"});
}

TEST_CASE("Kernel DependsOn uses free-variable dependency only", "[architecture][kernel][variables]") {
    REQUIRE(kernel::depends_on(parsed("x * y + z"), "x"));
    REQUIRE_FALSE(kernel::depends_on(parsed("f[x_] := x + y"), "x"));
    REQUIRE(kernel::depends_on(parsed("f[x_] := x + y"), "y"));
    REQUIRE_FALSE(kernel::depends_on(parsed("f[a_] -> g[a]"), "a"));
}

TEST_CASE("Kernel capture-safe substitution avoids introducing captured variables", "[architecture][kernel][variables][substitution]") {
    kernel::SymbolSubstitutionMap substitutions;
    substitutions.emplace("y", parsed("x"));
    substitutions.emplace("z", parsed("w"));

    const auto substituted = kernel::substitute_symbols_capture_safe(
        parsed("f[x_] := y + z"),
        substitutions);

    require_symbols(kernel::free_variables(substituted), {"w", "y"});
    REQUIRE_FALSE(kernel::depends_on(substituted, "x"));
    REQUIRE(kernel::depends_on(substituted, "w"));
}

TEST_CASE("Kernel capture-safe substitution respects rule pattern binders", "[architecture][kernel][variables][substitution]") {
    kernel::SymbolSubstitutionMap substitutions;
    substitutions.emplace("y", parsed("a"));

    const auto substituted = kernel::substitute_symbols_capture_safe(
        parsed("f[a_] -> g[a, y]"),
        substitutions);

    require_symbols(kernel::free_variables(substituted), {"y"});
    REQUIRE_FALSE(kernel::depends_on(substituted, "a"));
}

TEST_CASE("Registered variable analysis functions inspect held expressions", "[evaluator][variables]") {
    EvaluationContext ctx(kernel::default_function_registry());

    REQUIRE(to_string(evaluate(parsed("FreeVariables[f[a_] -> g[a, y]]"), ctx)) == "{y}");
    REQUIRE(to_string(evaluate(parsed("BoundVariables[f[a_, b_Integer] -> g[a, b, y]]"), ctx)) == "{a, b}");
    REQUIRE(to_string(evaluate(parsed("DependsOn[f[a_] -> g[a, y], a]"), ctx)) == "False");
    REQUIRE(to_string(evaluate(parsed("DependsOn[f[a_] -> g[a, y], y]"), ctx)) == "True");
}

TEST_CASE("Registered DependsOn rejects invalid variable arguments", "[evaluator][variables][diagnostics]") {
    EvaluationContext ctx(kernel::default_function_registry());

    try {
        (void)evaluate(parsed("DependsOn[x + y, x + 1]"), ctx);
        FAIL("Expected DependsOn to reject a non-symbol variable");
    } catch (const EvaluatorError& error) {
        REQUIRE(error.code_string() == "kernel.invalid_form");
    }
}
