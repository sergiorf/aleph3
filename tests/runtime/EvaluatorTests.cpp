#include "frontend/Parser.hpp"
#include "runtime/Evaluator.hpp"

#include <algorithm>
#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace aleph3;

namespace {

ir::NodePtr parse_root(std::string_view source) {
    frontend::Parser parser(source);
    auto result = parser.parse();
    REQUIRE(result.ok());
    return result.root;
}

}  // namespace

TEST_CASE("Runtime evaluator handles arithmetic, comparisons, and conditionals", "[runtime][evaluator]") {
    const auto arithmetic = parse_root("2 + 3 * 4");
    const auto comparison = parse_root("3 < 4");
    const auto conditional = parse_root("If[3 < 4, 10, 20]");
    const auto integer_power = parse_root("(-2) ^ 3");
    const auto negative_zero = parse_root("-0");
    const auto zero_equality = parse_root("(-0) == 0");
    const auto zero_ordering = parse_root("(-0) < 0");

    const Bindings bindings;
    const Bindings constants;
    const std::unordered_map<std::string, HostFunctionSpec> host_functions;
    const Policy policy = Policy::default_policy();
    runtime::Evaluator evaluator({bindings, constants, host_functions, policy});

    const auto arithmetic_result = evaluator.evaluate(arithmetic);
    REQUIRE(arithmetic_result.ok());
    REQUIRE(arithmetic_result.value->as_number() != nullptr);
    REQUIRE(*arithmetic_result.value->as_number() == 14.0);

    const auto comparison_result = evaluator.evaluate(comparison);
    REQUIRE(comparison_result.ok());
    REQUIRE(comparison_result.value->as_boolean() != nullptr);
    REQUIRE(*comparison_result.value->as_boolean());

    const auto conditional_result = evaluator.evaluate(conditional);
    REQUIRE(conditional_result.ok());
    REQUIRE(conditional_result.value->as_number() != nullptr);
    REQUIRE(*conditional_result.value->as_number() == 10.0);

    const auto integer_power_result = evaluator.evaluate(integer_power);
    REQUIRE(integer_power_result.ok());
    REQUIRE(integer_power_result.value->as_number() != nullptr);
    REQUIRE(*integer_power_result.value->as_number() == -8.0);

    const auto negative_zero_result = evaluator.evaluate(negative_zero);
    REQUIRE(negative_zero_result.ok());
    REQUIRE(negative_zero_result.value->as_number() != nullptr);
    REQUIRE(*negative_zero_result.value->as_number() == 0.0);
    REQUIRE_FALSE(std::signbit(*negative_zero_result.value->as_number()));

    const auto zero_equality_result = evaluator.evaluate(zero_equality);
    REQUIRE(zero_equality_result.ok());
    REQUIRE(zero_equality_result.value->as_boolean() != nullptr);
    REQUIRE(*zero_equality_result.value->as_boolean());

    const auto zero_ordering_result = evaluator.evaluate(zero_ordering);
    REQUIRE(zero_ordering_result.ok());
    REQUIRE(zero_ordering_result.value->as_boolean() != nullptr);
    REQUIRE_FALSE(*zero_ordering_result.value->as_boolean());
}

TEST_CASE("Runtime evaluator supports SDK numeric built-ins", "[runtime][evaluator]") {
    const auto clamp = parse_root("Clamp[12, 0, 10]");
    const auto floor = parse_root("Floor[2.7]");
    const auto ceil = parse_root("Ceil[2.1]");
    const auto ceiling = parse_root("Ceiling[-3.2]");
    const auto round = parse_root("Round[2.6]");
    const auto sqrt = parse_root("Sqrt[9]");

    const Bindings bindings;
    const Bindings constants;
    const std::unordered_map<std::string, HostFunctionSpec> host_functions;
    const Policy policy = Policy::default_policy();
    runtime::Evaluator evaluator({bindings, constants, host_functions, policy});

    const auto clamp_result = evaluator.evaluate(clamp);
    REQUIRE(clamp_result.ok());
    REQUIRE(*clamp_result.value->as_number() == 10.0);

    const auto floor_result = evaluator.evaluate(floor);
    REQUIRE(floor_result.ok());
    REQUIRE(*floor_result.value->as_number() == 2.0);

    const auto ceil_result = evaluator.evaluate(ceil);
    REQUIRE(ceil_result.ok());
    REQUIRE(*ceil_result.value->as_number() == 3.0);

    const auto ceiling_result = evaluator.evaluate(ceiling);
    REQUIRE(ceiling_result.ok());
    REQUIRE(*ceiling_result.value->as_number() == -3.0);

    const auto round_result = evaluator.evaluate(round);
    REQUIRE(round_result.ok());
    REQUIRE(*round_result.value->as_number() == 3.0);

    const auto sqrt_result = evaluator.evaluate(sqrt);
    REQUIRE(sqrt_result.ok());
    REQUIRE(*sqrt_result.value->as_number() == 3.0);
}

TEST_CASE("Runtime evaluator resolves bindings and host functions", "[runtime][evaluator]") {
    const auto variable = parse_root("x + 1");
    const auto host_call = parse_root("Clamp[x, 0, 10]");

    Bindings bindings = {{"x", Value(12.0)}};
    const Bindings constants;
    std::unordered_map<std::string, HostFunctionSpec> host_functions;

    HostFunctionSpec clamp;
    clamp.name = "Clamp";
    clamp.arity = FunctionArity::exact(3);
    clamp.parameters = {
        {"value", ValueType::number, true},
        {"low", ValueType::number, true},
        {"high", ValueType::number, true}
    };
    clamp.return_type = ValueType::number;
    clamp.callback = [](std::span<const Value> args) {
        const double value = *args[0].as_number();
        const double low = *args[1].as_number();
        const double high = *args[2].as_number();
        EvaluationResult result;
        result.value = Value(std::max(low, std::min(value, high)));
        return result;
    };
    host_functions.emplace(clamp.name, clamp);

    const Policy policy = Policy::default_policy();
    runtime::Evaluator evaluator({bindings, constants, host_functions, policy});

    const auto variable_result = evaluator.evaluate(variable);
    REQUIRE(variable_result.ok());
    REQUIRE(*variable_result.value->as_number() == 13.0);

    const auto host_result = evaluator.evaluate(host_call);
    REQUIRE(host_result.ok());
    REQUIRE(*host_result.value->as_number() == 10.0);
}

TEST_CASE("Runtime evaluator enforces host function argument and return contracts", "[runtime][evaluator]") {
    const auto wrong_argument = parse_root("Clamp[label, 0, 10]");
    const auto wrong_return = parse_root("AsText[1]");
    const auto invalid_result = parse_root("Broken[1]");

    Bindings bindings = {{"label", Value("hello")}};
    const Bindings constants;
    std::unordered_map<std::string, HostFunctionSpec> host_functions;

    HostFunctionSpec clamp;
    clamp.name = "Clamp";
    clamp.arity = FunctionArity::exact(3);
    clamp.parameters = {
        {"value", ValueType::number, true},
        {"low", ValueType::number, true},
        {"high", ValueType::number, true}
    };
    clamp.return_type = ValueType::number;
    clamp.callback = [](std::span<const Value>) {
        EvaluationResult result;
        result.value = Value(0.0);
        return result;
    };
    host_functions.emplace(clamp.name, clamp);

    HostFunctionSpec as_text;
    as_text.name = "AsText";
    as_text.arity = FunctionArity::exact(1);
    as_text.parameters = {{"value", ValueType::number, true}};
    as_text.return_type = ValueType::number;
    as_text.callback = [](std::span<const Value>) {
        EvaluationResult result;
        result.value = Value("not a number");
        return result;
    };
    host_functions.emplace(as_text.name, as_text);

    HostFunctionSpec broken;
    broken.name = "Broken";
    broken.arity = FunctionArity::exact(1);
    broken.callback = [](std::span<const Value>) {
        return EvaluationResult{};
    };
    host_functions.emplace(broken.name, broken);

    const Policy policy = Policy::default_policy();
    runtime::Evaluator evaluator({bindings, constants, host_functions, policy});

    const auto argument_result = evaluator.evaluate(wrong_argument);
    REQUIRE_FALSE(argument_result.ok());
    REQUIRE(argument_result.error->code == "runtime.invalid_argument_type");

    const auto return_result = evaluator.evaluate(wrong_return);
    REQUIRE_FALSE(return_result.ok());
    REQUIRE(return_result.error->code == "runtime.invalid_host_result");

    const auto invalid_result_output = evaluator.evaluate(invalid_result);
    REQUIRE_FALSE(invalid_result_output.ok());
    REQUIRE(invalid_result_output.error->code == "runtime.invalid_host_result");
}

TEST_CASE("Runtime evaluator reports runtime failures deterministically", "[runtime][evaluator]") {
    const auto missing_binding = parse_root("x + 1");
    const auto divide_by_zero = parse_root("1 / 0");
    const auto wrong_if = parse_root("If[1, 2, 3]");
    const auto mixed_equality = parse_root("x == y");
    const auto non_finite_equality = parse_root("x == y");
    const auto non_finite_self_equality = parse_root("z == z");
    const auto invalid_sqrt = parse_root("Sqrt[-1]");
    const auto invalid_clamp = parse_root("Clamp[1, 5, 2]");
    const auto zero_to_zero = parse_root("0 ^ 0");
    const auto invalid_power = parse_root("(-1) ^ 0.5");
    const auto overflow_power = parse_root("10 ^ 1000");

    const Bindings bindings = {
        {"x", Value(1.0)},
        {"y", Value("1")},
        {"z", Value(std::numeric_limits<double>::quiet_NaN())}
    };
    const Bindings missing_bindings = {
        {"y", Value("1")},
        {"z", Value(std::numeric_limits<double>::quiet_NaN())}
    };
    const Bindings non_finite_bindings = {
        {"x", Value(std::numeric_limits<double>::infinity())},
        {"y", Value(1.0)}
    };
    const Bindings constants;
    const std::unordered_map<std::string, HostFunctionSpec> host_functions;
    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 1;

    runtime::Evaluator low_budget({bindings, constants, host_functions, policy});
    const auto budget_result = low_budget.evaluate(parse_root("1 + 2"));
    REQUIRE_FALSE(budget_result.ok());
    REQUIRE(budget_result.error->code == "runtime.step_budget_exhausted");

    policy.budget().max_evaluation_steps = 100;
    runtime::Evaluator evaluator({bindings, constants, host_functions, policy});
    runtime::Evaluator missing_binding_evaluator({missing_bindings, constants, host_functions, policy});

    const auto binding_result = missing_binding_evaluator.evaluate(missing_binding);
    REQUIRE_FALSE(binding_result.ok());
    REQUIRE(binding_result.error->code == "runtime.unknown_binding");

    const auto divide_result = evaluator.evaluate(divide_by_zero);
    REQUIRE_FALSE(divide_result.ok());
    REQUIRE(divide_result.error->code == "runtime.division_by_zero");

    const auto mixed_equality_result = evaluator.evaluate(mixed_equality);
    REQUIRE_FALSE(mixed_equality_result.ok());
    REQUIRE(mixed_equality_result.error->code == "runtime.type_mismatch");

    runtime::Evaluator non_finite_evaluator({non_finite_bindings, constants, host_functions, policy});
    const auto non_finite_equality_result = non_finite_evaluator.evaluate(non_finite_equality);
    REQUIRE_FALSE(non_finite_equality_result.ok());
    REQUIRE(non_finite_equality_result.error->code == "runtime.non_finite_number");

    const auto non_finite_self_equality_result = evaluator.evaluate(non_finite_self_equality);
    REQUIRE_FALSE(non_finite_self_equality_result.ok());
    REQUIRE(non_finite_self_equality_result.error->code == "runtime.non_finite_number");

    const auto invalid_sqrt_result = evaluator.evaluate(invalid_sqrt);
    REQUIRE_FALSE(invalid_sqrt_result.ok());
    REQUIRE(invalid_sqrt_result.error->code == "runtime.invalid_numeric_domain");

    const auto invalid_clamp_result = evaluator.evaluate(invalid_clamp);
    REQUIRE_FALSE(invalid_clamp_result.ok());
    REQUIRE(invalid_clamp_result.error->code == "runtime.invalid_numeric_domain");

    const auto zero_to_zero_result = evaluator.evaluate(zero_to_zero);
    REQUIRE_FALSE(zero_to_zero_result.ok());
    REQUIRE(zero_to_zero_result.error->code == "runtime.invalid_power_domain");

    const auto invalid_power_result = evaluator.evaluate(invalid_power);
    REQUIRE_FALSE(invalid_power_result.ok());
    REQUIRE(invalid_power_result.error->code == "runtime.invalid_power_domain");

    const auto overflow_power_result = evaluator.evaluate(overflow_power);
    REQUIRE_FALSE(overflow_power_result.ok());
    REQUIRE(overflow_power_result.error->code == "runtime.invalid_numeric_result");

    const auto if_result = evaluator.evaluate(wrong_if);
    REQUIRE_FALSE(if_result.ok());
    REQUIRE(if_result.error->code == "runtime.type_mismatch");
}
