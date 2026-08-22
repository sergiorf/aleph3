#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "kernel/Diagnostics.hpp"
#include "parser/Parser.hpp"
#include "packs/CalculusPack.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_map>

using namespace aleph3;

namespace {

ExprPtr evaluate_source(std::string_view source, EvaluationContext& ctx) {
    return evaluate(parse_expression(std::string(source)), ctx);
}

std::string evaluated_string(std::string_view source) {
    EvaluationContext ctx(kernel::default_function_registry());
    return to_string(evaluate_source(source, ctx));
}

std::string code_for(std::string_view source) {
    EvaluationContext ctx(kernel::default_function_registry());
    try {
        (void)evaluate_source(source, ctx);
    } catch (const kernel::RuntimeFailure& failure) {
        return failure.error().code;
    } catch (const EvaluatorError& error) {
        return std::string(error.code_string());
    }
    return {};
}

}  // namespace

TEST_CASE("Calculus pack functions evaluate through registry-backed handlers", "[packs][calculus]") {
    auto registry = kernel::create_default_function_registry();
    EvaluationContext ctx(registry);

    REQUIRE(to_string(evaluate_source("D[x^2, x]", ctx)) == "2 * x");

    const auto* spec = registry.find_symbolic_function_spec("D");
    REQUIRE(spec != nullptr);
    REQUIRE(spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(spec->metadata.owning_package == "core-calculus");
}

TEST_CASE("Calculus pack differentiates constants and variables", "[packs][calculus]") {
    REQUIRE(evaluated_string("D[3, x]") == "0");
    REQUIRE(evaluated_string("D[1/2, x]") == "0");
    REQUIRE(evaluated_string("D[True, x]") == "0");
    REQUIRE(evaluated_string("D[x, x]") == "1");
    REQUIRE(evaluated_string("D[y, x]") == "0");
}

TEST_CASE("Calculus pack differentiates sums products and powers", "[packs][calculus]") {
    REQUIRE(evaluated_string("D[x^2 + 3*x, x]") == "2 * x + 3");
    REQUIRE(evaluated_string("D[x*y, x]") == "y");
    REQUIRE(evaluated_string("D[x*y*x, x]") == "2 * x * y");
    REQUIRE(evaluated_string("D[x^(3/2), x]") == "3/2 * x^1/2");
}

TEST_CASE("Calculus pack differentiates focused chain rules", "[packs][calculus]") {
    REQUIRE(evaluated_string("D[Sin[x], x]") == "Cos[x]");
    REQUIRE(evaluated_string("D[Cos[x], x]") == "-(Sin[x])");
    REQUIRE(evaluated_string("D[Exp[x^2], x]") == "2 * x * (Exp[x^2])");
    REQUIRE(evaluated_string("D[Log[x], x]") == "x^-1");
    REQUIRE(evaluated_string("D[Sqrt[x], x]") == "1/2 * x^-1/2");
}

TEST_CASE("Calculus pack supports higher-order derivatives", "[packs][calculus]") {
    REQUIRE(evaluated_string("D[x^3, {x, 0}]") == "x^3");
    REQUIRE(evaluated_string("D[x^3, {x, 1}]") == "3 * x^2");
    REQUIRE(evaluated_string("D[x^3, {x, 2}]") == "6 * x");
    REQUIRE(evaluated_string("D[x^3, {x, 3}]") == "6");
    REQUIRE(evaluated_string("D[x^3, {x, 4}]") == "0");
    REQUIRE(evaluated_string("Differentiate[x^3, {x, 2}]") == "6 * x");
    REQUIRE(evaluated_string("D[Sin[x], {x, 2}]") == "-(Sin[x])");
    REQUIRE(evaluated_string("D[x*y, {x, 2}]") == "0");
}

TEST_CASE("Calculus pack preserves unsupported dependent heads and validates variables", "[packs][calculus][diagnostics]") {
    REQUIRE(evaluated_string("D[f[x], x]") == "D[f[x], x]");
    REQUIRE(code_for("D[x, x + 1]") == "kernel.invalid_form");
    REQUIRE(code_for("D[x, {x, -1}]") == "kernel.invalid_form");
    REQUIRE(code_for("D[x, {x, 1/2}]") == "kernel.invalid_form");
    REQUIRE(code_for("D[x, {x, n}]") == "kernel.invalid_form");
    REQUIRE(code_for("D[x, {x}]") == "kernel.invalid_form");
    REQUIRE(code_for("D[x, {x, 1, 2}]") == "kernel.invalid_form");
    REQUIRE(code_for("D[x, {x + 1, 2}]") == "kernel.invalid_form");
    REQUIRE(code_for("D[x, {x, 1025}]") == "kernel.invalid_form");
}

TEST_CASE("Calculus pack consumes shared evaluation budget", "[packs][calculus][budget]") {
    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 3;
    Bindings bindings;
    std::unordered_map<std::string, HostFunctionSpec> host_functions;
    EvaluationContext ctx(bindings, bindings, host_functions, policy);
    ctx.enable_runtime_strict_semantics(true);

    try {
        (void)evaluate_source("D[x^2 + 3*x + Sin[x], x]", ctx);
        FAIL("Expected differentiation to exhaust the shared evaluation budget");
    } catch (const kernel::RuntimeFailure& failure) {
        REQUIRE(failure.error().code == "runtime.step_budget_exhausted");
    }
}

TEST_CASE("Higher-order differentiation consumes shared evaluation budget", "[packs][calculus][budget]") {
    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 3;
    Bindings bindings;
    std::unordered_map<std::string, HostFunctionSpec> host_functions;
    EvaluationContext ctx(bindings, bindings, host_functions, policy);
    ctx.enable_runtime_strict_semantics(true);

    try {
        (void)evaluate_source("D[x^5 + Sin[x], {x, 3}]", ctx);
        FAIL("Expected higher-order differentiation to exhaust the shared evaluation budget");
    } catch (const kernel::RuntimeFailure& failure) {
        REQUIRE(failure.error().code == "runtime.step_budget_exhausted");
    }
}
