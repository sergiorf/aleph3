#include "evaluator/BuiltInFunctions.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/Evaluator.hpp"
#include "kernel/Diagnostics.hpp"
#include "kernel/EvaluationContext.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "packs/PackRegistry.hpp"
#include "parser/Parser.hpp"
#include "sdk/Policy.hpp"
#include "sdk/Types.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("EvaluationContext exposes symbol tables through compatibility aliases", "[architecture][symbols]") {
    EvaluationContext ctx;

    ctx.symbol_values.set("x", make_expr<Number>(2.0));
    REQUIRE(ctx.variables.contains("x"));
    REQUIRE(std::get<Number>(*ctx.variables.at("x")).value == 2.0);

    ctx.variables["y"] = make_expr<Number>(3.0);
    const ExprPtr* y = ctx.symbol_values.lookup("y");
    REQUIRE(y != nullptr);
    REQUIRE(std::get<Number>(**y).value == 3.0);
}

TEST_CASE("EvaluationContext copies preserve symbol and function state", "[architecture][symbols]") {
    EvaluationContext ctx;
    ctx.variables["offset"] = make_expr<Number>(5.0);
    evaluate(parse_expression("f[x_] := x + offset"), ctx);

    EvaluationContext copy = ctx;
    REQUIRE(copy.variables.contains("offset"));
    REQUIRE(copy.user_functions.contains("f"));

    auto result = evaluate(parse_expression("f[7]"), copy);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 12.0);
}

TEST_CASE("Built-in symbolic surface is registered through the pack registry", "[architecture][packs]") {
    register_built_in_functions();

    auto& registry = packs::PackRegistry::instance();
    REQUIRE(registry.has_function("StringJoin"));
    REQUIRE(registry.has_function("N"));
}

TEST_CASE("Kernel evaluation context can carry symbolic and SDK runtime state", "[architecture][kernel]") {
    Bindings bindings = {{"x", Value(2.0)}};
    Bindings constants = {{"pi", Value(3.14159)}};

    HostFunctionSpec clamp;
    clamp.name = "Clamp";
    clamp.arity = FunctionArity::exact(3);
    clamp.callback = [](std::span<const Value>) {
        EvaluationResult result;
        result.value = Value(0.0);
        return result;
    };

    std::unordered_map<std::string, HostFunctionSpec> host_functions;
    host_functions.emplace(clamp.name, clamp);

    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 77;

    kernel::EvaluationContext ctx(bindings, constants, host_functions, policy);
    ctx.symbol_values.set("offset", make_expr<Number>(5.0));

    REQUIRE(ctx.variables.contains("offset"));
    REQUIRE(ctx.bindings().contains("x"));
    REQUIRE(ctx.constants().contains("pi"));
    REQUIRE(ctx.host_functions().contains("Clamp"));
    REQUIRE(ctx.policy().budget().max_evaluation_steps == 77);
}

TEST_CASE("Kernel diagnostic taxonomy maps symbolic and runtime surfaces", "[architecture][kernel]") {
    REQUIRE(kernel::kernel_error_code_for(EvaluatorErrorKind::unsupported_construct) ==
            kernel::ErrorCode::unsupported_construct);
    REQUIRE(kernel::kernel_error_code_name(kernel::ErrorCode::unsupported_construct) ==
            "kernel.unsupported_construct");
    REQUIRE(kernel::sdk_runtime_projection_code(kernel::ErrorCode::type_mismatch) ==
            "runtime.type_mismatch");

    const auto runtime_error = kernel::make_runtime_error(
        kernel::ErrorCode::invalid_numeric_domain,
        "Sqrt is undefined for negative inputs.");
    REQUIRE(runtime_error.code == "runtime.invalid_numeric_domain");
}

TEST_CASE("Kernel function registry backs symbolic and host registration paths", "[architecture][kernel]") {
    auto& registry = kernel::FunctionRegistry::instance();
    REQUIRE(registry.has_function("StringJoin"));

    kernel::HostFunctionRegistry host_registry;
    HostFunctionSpec clamp;
    clamp.name = "Clamp";
    clamp.arity = FunctionArity::exact(3);
    clamp.callback = [](std::span<const Value>) {
        EvaluationResult result;
        result.value = Value(1.0);
        return result;
    };

    kernel::FunctionRegistry::register_host_function(host_registry, clamp);
    const auto* host_spec = kernel::FunctionRegistry::find_host_function(host_registry, "Clamp");
    REQUIRE(host_spec != nullptr);
    REQUIRE(host_spec->arity.allows(3));
}

TEST_CASE("Registered symbolic handlers win before user-defined functions", "[architecture][precedence]") {
    EvaluationContext ctx;
    evaluate(parse_expression("Length[x_] := 99"), ctx);

    auto result = evaluate(parse_expression("Length[{1, 2, 3}]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 3.0);
}

TEST_CASE("Unknown symbolic functions remain symbolic when no handler owns the name", "[architecture][precedence]") {
    EvaluationContext ctx;

    auto result = evaluate(parse_expression("f[x]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*result));

    const auto& call = std::get<FunctionCall>(*result);
    REQUIRE(call.head == "f");
    REQUIRE(call.args.size() == 1);
}
