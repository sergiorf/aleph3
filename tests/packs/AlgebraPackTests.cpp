#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
#include "kernel/Diagnostics.hpp"
#include "expr/Expr.hpp"
#include "parser/Parser.hpp"
#include "packs/AlgebraPack.hpp"
#include "transforms/Transforms.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_map>

using namespace aleph3;

namespace {

ExprPtr evaluate_source(std::string_view source, EvaluationContext& ctx) {
    return evaluate(parse_expression(std::string(source)), ctx);
}

std::string simplify_string(const ExprPtr& expr) {
    return to_string(simplify(expr));
}

}  // namespace

TEST_CASE("Algebra pack functions evaluate through registry-backed handlers", "[packs][algebra]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);
    EvaluationContext ctx(registry);

    const auto expanded = evaluate_source("Expand[(x + 1) * (x + 2)]", ctx);
    REQUIRE(simplify_string(expanded) == "x^2 + 3 * x + 2");

    const auto* expand_spec = registry.find_symbolic_function_spec("Expand");
    REQUIRE(expand_spec != nullptr);
    REQUIRE(expand_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(expand_spec->metadata.owning_package == "core-algebra");
}

TEST_CASE("Algebra pack exact-capable helpers preserve exact multivariate outputs through registration", "[packs][algebra][exact]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);
    EvaluationContext ctx(registry);

    const auto expanded = evaluate_source("Expand[(1/2) * (x + y)]", ctx);
    REQUIRE(simplify_string(expanded) == "x * 1/2 + y * 1/2");

    const auto collected = evaluate_source("Collect[(1/2) * x * y + (3/2) * y, y]", ctx);
    REQUIRE(simplify_string(collected) == "x * y * 1/2 + y * 3/2");

    const auto* expand_spec = registry.find_symbolic_function_spec("Expand");
    const auto* collect_spec = registry.find_symbolic_function_spec("Collect");
    REQUIRE(expand_spec != nullptr);
    REQUIRE(collect_spec != nullptr);
    REQUIRE(expand_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(collect_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(expand_spec->metadata.owning_package == "core-algebra");
    REQUIRE(collect_spec->metadata.owning_package == "core-algebra");
}

TEST_CASE("Algebra pack owns exact rational factorization", "[packs][algebra][exact][factor]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);
    EvaluationContext ctx(registry);

    const auto factored = evaluate_source("Factor[(1/2) * x^2 + x + 1/2]", ctx);
    REQUIRE(to_string(*factored) == "1/2 * (x + 1) * (x + 1)");

    const auto* factor = registry.find_symbolic_function_spec("Factor");
    REQUIRE(factor != nullptr);
    REQUIRE(factor->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(factor->metadata.owning_package == "core-algebra");
}

TEST_CASE("Algebra pack owns bounded exact multivariate GCD", "[packs][algebra][exact][gcd]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);
    EvaluationContext ctx(registry);

    REQUIRE(simplify_string(evaluate_source("GCD[x*y + x, x, {x, y}]", ctx)) == "x");
    const auto* gcd_spec = registry.find_symbolic_function_spec("GCD");
    REQUIRE(gcd_spec != nullptr);
    REQUIRE(gcd_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(gcd_spec->metadata.owning_package == "core-algebra");
}

TEST_CASE("Algebra pack extracts exact polynomial coefficients", "[packs][algebra][exact][coefficient]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);
    EvaluationContext ctx(registry);

    REQUIRE(to_string(*evaluate_source("Coefficient[3*x^2 + 2*x + 1, x]", ctx)) == "2");
    REQUIRE(to_string(*evaluate_source("Coefficient[3*x^2 + 2*x + 1, x, 2]", ctx)) == "3");
    REQUIRE(to_string(*evaluate_source("Coefficient[3*x^2 + 2*x + 1, x, 0]", ctx)) == "1");
    REQUIRE(to_string(*evaluate_source("Coefficient[(1/2)*x^2 + x, x, 2]", ctx)) == "1/2");
    REQUIRE(to_string(*evaluate_source("CoefficientList[(1/2)*x^2 + x, x]", ctx)) == "{0, 1, 1/2}");

    const auto* coefficient_spec = registry.find_symbolic_function_spec("Coefficient");
    const auto* coefficient_list_spec = registry.find_symbolic_function_spec("CoefficientList");
    REQUIRE(coefficient_spec != nullptr);
    REQUIRE(coefficient_list_spec != nullptr);
    REQUIRE(coefficient_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(coefficient_list_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(coefficient_spec->metadata.owning_package == "core-algebra");
    REQUIRE(coefficient_list_spec->metadata.owning_package == "core-algebra");
}

TEST_CASE("Algebra pack extracts supported rational expression parts", "[packs][algebra][rational-expression]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);
    EvaluationContext ctx(registry);

    REQUIRE(to_string(*evaluate_source("Numerator[1/2]", ctx)) == "1");
    REQUIRE(to_string(*evaluate_source("Denominator[1/2]", ctx)) == "2");
    REQUIRE(to_string(*evaluate_source("Numerator[x]", ctx)) == "x");
    REQUIRE(to_string(*evaluate_source("Denominator[x]", ctx)) == "1");
    REQUIRE(to_string(*evaluate_source("Numerator[(1/2)*x]", ctx)) == "x");
    REQUIRE(to_string(*evaluate_source("Denominator[(1/2)*x]", ctx)) == "2");
    REQUIRE(to_string(*evaluate_source("Numerator[(2/3)*x]", ctx)) == "2 * x");
    REQUIRE(to_string(*evaluate_source("Denominator[(2/3)*x]", ctx)) == "3");
    REQUIRE(to_string(*evaluate_source("Numerator[x/(x + 1)]", ctx)) == "x");
    REQUIRE(to_string(*evaluate_source("Denominator[x/(x + 1)]", ctx)) == "x + 1");

    const auto* numerator_spec = registry.find_symbolic_function_spec("Numerator");
    const auto* denominator_spec = registry.find_symbolic_function_spec("Denominator");
    REQUIRE(numerator_spec != nullptr);
    REQUIRE(denominator_spec != nullptr);
    REQUIRE(numerator_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(denominator_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(numerator_spec->metadata.owning_package == "core-algebra");
    REQUIRE(denominator_spec->metadata.owning_package == "core-algebra");
}

TEST_CASE("Algebra pack owns rational expression transformations", "[packs][algebra][rational-expression]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);
    EvaluationContext ctx(registry);

    REQUIRE(simplify_string(evaluate_source("Together[1/x + 1/y]", ctx)) == "(x + y)/(x * y)");
    REQUIRE(simplify_string(evaluate_source("Cancel[(x^2 - 1)/(x - 1)]", ctx)) == "x + 1");

    const auto* together_spec = registry.find_symbolic_function_spec("Together");
    const auto* cancel_spec = registry.find_symbolic_function_spec("Cancel");
    REQUIRE(together_spec != nullptr);
    REQUIRE(cancel_spec != nullptr);
    REQUIRE(together_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(cancel_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(together_spec->metadata.owning_package == "core-algebra");
    REQUIRE(cancel_spec->metadata.owning_package == "core-algebra");
}

TEST_CASE("Algebra pack registers the full documented helper surface", "[packs][algebra]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);

    for (const auto* name : {"Expand", "Factor", "Collect", "GCD", "PolynomialQuotient",
             "Coefficient", "CoefficientList", "Numerator", "Denominator", "Together", "Cancel",
             "MatrixAdd", "MatrixMultiply", "IdentityMatrix", "Transpose", "Det", "RowReduce",
             "LinearSolve"}) {
        const auto* spec = registry.find_symbolic_function_spec(name);
        REQUIRE(spec != nullptr);
        REQUIRE(spec->metadata.source == kernel::RegistrationSource::pack);
        REQUIRE(spec->metadata.owning_package == "core-algebra");
    }
}

TEST_CASE("Algebra pack exposes exact dense matrix basics", "[packs][algebra][matrix]") {
    EvaluationContext ctx(kernel::default_function_registry());
    REQUIRE(to_string(*evaluate_source("MatrixAdd[{{1, 1/2}, {2, 3}}, {{4, 1/2}, {5, 6}}]", ctx)) ==
        "{{5, 1}, {7, 9}}");
    REQUIRE(to_string(*evaluate_source("MatrixMultiply[{{1, 2, 3}}, {{1}, {0}, {2}}]", ctx)) == "{{7}}");
    REQUIRE(to_string(*evaluate_source("IdentityMatrix[2]", ctx)) == "{{1, 0}, {0, 1}}");
    REQUIRE(to_string(*evaluate_source("Transpose[{{1, 2, 3}, {4, 5, 6}}]", ctx)) ==
        "{{1, 4}, {2, 5}, {3, 6}}");
}

TEST_CASE("Algebra pack exposes exact elimination workflows", "[packs][algebra][matrix]") {
    EvaluationContext ctx(kernel::default_function_registry());
    REQUIRE(to_string(*evaluate_source("Det[{{0, 1}, {2, 3}}]", ctx)) == "-2");
    REQUIRE(to_string(*evaluate_source("RowReduce[{{1, 2}, {3, 4}}]", ctx)) == "{{1, 0}, {0, 1}}");
    REQUIRE(to_string(*evaluate_source("LinearSolve[{{2, 1}, {1, -1}}, {5, 1}]", ctx)) == "{2, 1}");
}

TEST_CASE("Matrix pack failures use shared diagnostics", "[packs][algebra][matrix][diagnostics]") {
    EvaluationContext ctx(kernel::default_function_registry());
    const auto code_for = [&](std::string_view source) {
        try {
            (void)evaluate_source(source, ctx);
        } catch (const kernel::RuntimeFailure& failure) {
            return failure.error().code;
        }
        return std::string{};
    };
    REQUIRE(code_for("Transpose[{{1}, {2, 3}}]") == "runtime.invalid_form");
    REQUIRE(code_for("MatrixAdd[{{1}}, {{1, 2}}]") == "runtime.domain_violation");
    REQUIRE(code_for("Det[{{x}}]") == "runtime.unsupported_construct");
    REQUIRE(code_for("LinearSolve[{{1, 2}, {2, 4}}, {1, 2}]") == "runtime.domain_violation");
}

TEST_CASE("Matrix arithmetic consumes the shared evaluation budget", "[packs][algebra][matrix][budget]") {
    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 12;
    Bindings bindings;
    std::unordered_map<std::string, HostFunctionSpec> host_functions;
    EvaluationContext ctx(bindings, bindings, host_functions, policy);
    ctx.enable_runtime_strict_semantics(true);
    try {
        (void)evaluate_source("MatrixMultiply[{{1, 2}, {3, 4}}, {{1, 2}, {3, 4}}]", ctx);
        FAIL("Expected the shared evaluation budget to be exhausted");
    } catch (const kernel::RuntimeFailure& failure) {
        REQUIRE(failure.error().code == "runtime.step_budget_exhausted");
    }
}

TEST_CASE("Default registry exposes algebra through the pack path", "[packs][algebra]") {
    EvaluationContext ctx(kernel::default_function_registry());

    const auto result = evaluate_source("Expand[(x + 1) * (x + 2)]", ctx);
    REQUIRE(simplify_string(result) == "x^2 + 3 * x + 2");

    const auto* expand =
        kernel::default_function_registry().find_symbolic_function_spec("Expand");
    REQUIRE(expand != nullptr);
    REQUIRE(expand->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(expand->metadata.owning_package == "core-algebra");
}
