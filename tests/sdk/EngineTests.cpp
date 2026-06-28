#include "sdk/Engine.hpp"

#include "evaluator/BuiltInFunctions.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "frontend/Parser.hpp"
#include "kernel/Diagnostics.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "kernel/TrustedSubsetBridge.hpp"
#include "semantics/Validator.hpp"

#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>

using namespace aleph3;

namespace {

std::optional<Value> expr_to_sdk_value(const ExprPtr& expr) {
    if (expr == nullptr) {
        return std::nullopt;
    }

    if (const auto* number = std::get_if<Number>(&*expr)) {
        double value = number->value;
        if (value == 0.0) {
            value = 0.0;
        }
        return Value(value);
    }
    if (const auto* rational = std::get_if<Rational>(&*expr)) {
        return Value(static_cast<double>(rational->numerator) / rational->denominator);
    }
    if (const auto* boolean = std::get_if<Boolean>(&*expr)) {
        return Value(boolean->value);
    }
    if (const auto* string = std::get_if<String>(&*expr)) {
        return Value(string->value);
    }
    if (const auto* list = std::get_if<List>(&*expr)) {
        Value::List values;
        values.reserve(list->elements.size());
        for (const auto& element : list->elements) {
            auto converted = expr_to_sdk_value(element);
            if (!converted.has_value()) {
                return std::nullopt;
            }
            values.push_back(std::move(*converted));
        }
        return Value(std::move(values));
    }

    return std::nullopt;
}

ExprPtr sdk_value_to_expr(const Value& value) {
    if (const auto* number = value.as_number()) {
        return make_expr<Number>(*number);
    }
    if (const auto* boolean = value.as_boolean()) {
        return make_expr<Boolean>(*boolean);
    }
    if (const auto* string = value.as_string()) {
        return make_expr<String>(*string);
    }
    if (const auto* list = value.as_list()) {
        std::vector<ExprPtr> elements;
        elements.reserve(list->size());
        for (const auto& element : *list) {
            elements.push_back(sdk_value_to_expr(element));
        }
        return std::make_shared<Expr>(List{elements});
    }
    return make_expr<Indeterminate>();
}

void seed_kernel_symbols(
    EvaluationContext& ctx,
    const Bindings& constants,
    const Bindings& bindings) {
    for (const auto& [name, value] : constants) {
        ctx.symbol_values.set(name, sdk_value_to_expr(value));
    }
    for (const auto& [name, value] : bindings) {
        ctx.symbol_values.set(name, sdk_value_to_expr(value));
    }
}

ExprPtr stage_sdk_source_as_kernel_expr(
    std::string_view source,
    const Schema& schema,
    const Policy& policy) {
    frontend::Parser parser(source);
    auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    semantics::Validator validator(schema, policy);
    auto validation = validator.validate(parse_result.root);
    REQUIRE(validation.ok());

    const auto staged = kernel::stage_trusted_subset_formula(parse_result.root);
    REQUIRE(staged.ok());
    return staged.kernel_expr;
}

EvaluationResult evaluate_direct_kernel_expr(
    const ExprPtr& expr,
    const Bindings& bindings,
    const Bindings& constants,
    const kernel::HostFunctionRegistry& host_functions,
    const Policy& policy) {
    const auto& registry = kernel::default_function_registry();

    try {
        EvaluationContext ctx(bindings, constants, host_functions, policy, registry);
        ctx.enable_runtime_strict_semantics(true);
        ctx.reset_runtime_step_counter();
        seed_kernel_symbols(ctx, constants, bindings);

        auto result_expr = evaluate(expr, ctx);
        auto value = expr_to_sdk_value(result_expr);
        if (value.has_value()) {
            EvaluationResult result;
            result.value = std::move(*value);
            return result;
        }

        return {};
    } catch (const kernel::RuntimeFailure& failure) {
        EvaluationResult result;
        result.error = failure.error();
        return result;
    } catch (const EvaluatorError& error) {
        EvaluationResult result;
        result.error = kernel::make_runtime_error(error.code(), error.what());
        return result;
    } catch (const std::runtime_error& error) {
        EvaluationResult result;
        result.error = kernel::make_runtime_error(
            kernel::ErrorCode::internal_inconsistency,
            error.what());
        return result;
    }
}

void require_same_value(const Value& left, const Value& right) {
    REQUIRE(left.storage().index() == right.storage().index());

    if (const auto* number = left.as_number()) {
        REQUIRE(right.as_number() != nullptr);
        REQUIRE(*number == *right.as_number());
        return;
    }
    if (const auto* boolean = left.as_boolean()) {
        REQUIRE(right.as_boolean() != nullptr);
        REQUIRE(*boolean == *right.as_boolean());
        return;
    }
    if (const auto* string = left.as_string()) {
        REQUIRE(right.as_string() != nullptr);
        REQUIRE(*string == *right.as_string());
        return;
    }
    if (const auto* list = left.as_list()) {
        REQUIRE(right.as_list() != nullptr);
        REQUIRE(list->size() == right.as_list()->size());
        for (std::size_t index = 0; index < list->size(); ++index) {
            require_same_value((*list)[index], (*right.as_list())[index]);
        }
        return;
    }

    REQUIRE(left.is_null());
    REQUIRE(right.is_null());
}

void require_same_evaluation_result(
    const EvaluationResult& sdk_result,
    const EvaluationResult& kernel_result) {
    REQUIRE(sdk_result.ok() == kernel_result.ok());
    REQUIRE(sdk_result.error.has_value() == kernel_result.error.has_value());
    REQUIRE(sdk_result.value.has_value() == kernel_result.value.has_value());

    if (sdk_result.ok()) {
        require_same_value(*sdk_result.value, *kernel_result.value);
        return;
    }

    REQUIRE(sdk_result.error->code == kernel_result.error->code);
}

}  // namespace

TEST_CASE("Engine compile rejects blank source with structured diagnostics", "[sdk][engine]") {
    Engine engine;
    Schema schema;

    const auto result = engine.compile("   ", schema);

    REQUIRE_FALSE(result.ok());
    REQUIRE_FALSE(result.formula.has_value());
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics.front().code == "sdk.source.empty");
}

TEST_CASE("Engine compile produces reusable compiled formulas for valid input", "[sdk][engine]") {
    Engine engine;
    Schema schema;
    schema.allow_variable({"x", ValueType::number, true});

    const auto result = engine.compile("x + 1", schema);

    REQUIRE(result.ok());
    REQUIRE(result.formula.has_value());
    REQUIRE_FALSE(result.formula->empty());
    REQUIRE(result.diagnostics.empty());

    const auto second_result = engine.compile("x + 1", schema);
    REQUIRE(second_result.ok());
    REQUIRE(second_result.formula.has_value());
    REQUIRE_FALSE(second_result.formula->empty());
}

TEST_CASE("Engine compile reports parser and validator failures", "[sdk][engine]") {
    Engine engine;
    Schema schema;

    const auto parse_failure = engine.compile("1 + (2 * 3", schema);
    REQUIRE_FALSE(parse_failure.ok());
    REQUIRE_FALSE(parse_failure.formula.has_value());
    REQUIRE(parse_failure.diagnostics.size() == 1);
    REQUIRE(parse_failure.diagnostics.front().code == "frontend.parser.expected_right_paren");

    const auto validation_failure = engine.compile("x + 1", schema);
    REQUIRE_FALSE(validation_failure.ok());
    REQUIRE_FALSE(validation_failure.formula.has_value());
    REQUIRE(validation_failure.diagnostics.size() == 1);
    REQUIRE(validation_failure.diagnostics.front().code == "semantics.validator.unknown_variable");
}

TEST_CASE("Engine validate accepts valid SDK formulas without diagnostics", "[sdk][engine]") {
    Engine engine;
    Schema schema;
    schema.allow_variable({"x", ValueType::number, true});

    const auto result = engine.validate("x + 1", schema);

    REQUIRE(result.ok);
    REQUIRE(result.diagnostics.empty());
}

TEST_CASE("Engine evaluate rejects empty compiled formulas", "[sdk][engine]") {
    Engine engine;
    CompiledFormula formula;

    const auto result = engine.evaluate(formula, {});

    REQUIRE_FALSE(result.ok());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->code == "sdk.formula.empty");
}

TEST_CASE("Engine evaluate accepts compiled formulas even before runtime exists", "[sdk][engine]") {
    Engine engine;
    Schema schema;
    schema.allow_variable({"x", ValueType::number, true});

    const auto compile_result = engine.compile("x + 1", schema);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const auto result = engine.evaluate(*compile_result.formula, {{"x", Value(2.0)}});
    REQUIRE(result.ok());
    REQUIRE(result.value.has_value());
    REQUIRE(result.value->as_number() != nullptr);
    REQUIRE(*result.value->as_number() == 3.0);

    const auto zero_formula = engine.compile("-0", schema);
    REQUIRE(zero_formula.ok());
    const auto zero_result = engine.evaluate(*zero_formula.formula, {});
    REQUIRE(zero_result.ok());
    REQUIRE(zero_result.value.has_value());
    REQUIRE(zero_result.value->as_number() != nullptr);
    REQUIRE(*zero_result.value->as_number() == 0.0);
    REQUIRE_FALSE(std::signbit(*zero_result.value->as_number()));
}

TEST_CASE("Engine evaluate reports runtime errors for compiled formulas", "[sdk][engine]") {
    Engine engine;
    Schema schema;
    schema.allow_variable({"x", ValueType::number, true});

    const auto unknown_binding_formula = engine.compile("x + 1", schema);
    REQUIRE(unknown_binding_formula.ok());
    const auto unknown_binding_result = engine.evaluate(*unknown_binding_formula.formula, {});
    REQUIRE_FALSE(unknown_binding_result.ok());
    REQUIRE(unknown_binding_result.error.has_value());
    REQUIRE(unknown_binding_result.error->code == "runtime.unknown_binding");

    Schema flexible_schema;
    flexible_schema.set_allow_unknown_variables(true);
    const auto mixed_equality_formula = engine.compile("x == y", flexible_schema);
    REQUIRE(mixed_equality_formula.ok());
    const auto mixed_equality_result = engine.evaluate(
        *mixed_equality_formula.formula,
        {{"x", Value(1.0)}, {"y", Value("1")}});
    REQUIRE_FALSE(mixed_equality_result.ok());
    REQUIRE(mixed_equality_result.error.has_value());
    REQUIRE(mixed_equality_result.error->code == "runtime.type_mismatch");

    const auto non_finite_equality_formula = engine.compile("x == y", flexible_schema);
    REQUIRE(non_finite_equality_formula.ok());
    const auto non_finite_equality_result = engine.evaluate(
        *non_finite_equality_formula.formula,
        {{"x", Value(std::numeric_limits<double>::infinity())}, {"y", Value(1.0)}});
    REQUIRE_FALSE(non_finite_equality_result.ok());
    REQUIRE(non_finite_equality_result.error.has_value());
    REQUIRE(non_finite_equality_result.error->code == "runtime.non_finite_number");

    const auto division_formula = engine.compile("1 / x", schema);
    REQUIRE(division_formula.ok());
    const auto division_result = engine.evaluate(*division_formula.formula, {{"x", Value(0.0)}});
    REQUIRE_FALSE(division_result.ok());
    REQUIRE(division_result.error.has_value());
    REQUIRE(division_result.error->code == "runtime.division_by_zero");

    const auto zero_to_zero_formula = engine.compile("0 ^ 0", schema);
    REQUIRE(zero_to_zero_formula.ok());
    const auto zero_to_zero_result = engine.evaluate(*zero_to_zero_formula.formula, {});
    REQUIRE_FALSE(zero_to_zero_result.ok());
    REQUIRE(zero_to_zero_result.error.has_value());
    REQUIRE(zero_to_zero_result.error->code == "runtime.invalid_power_domain");

    const auto invalid_power_formula = engine.compile("(-1) ^ 0.5", schema);
    REQUIRE(invalid_power_formula.ok());
    const auto invalid_power_result = engine.evaluate(*invalid_power_formula.formula, {});
    REQUIRE_FALSE(invalid_power_result.ok());
    REQUIRE(invalid_power_result.error.has_value());
    REQUIRE(invalid_power_result.error->code == "runtime.invalid_power_domain");

    const auto overflow_power_formula = engine.compile("10 ^ 1000", schema);
    REQUIRE(overflow_power_formula.ok());
    const auto overflow_power_result = engine.evaluate(*overflow_power_formula.formula, {});
    REQUIRE_FALSE(overflow_power_result.ok());
    REQUIRE(overflow_power_result.error.has_value());
    REQUIRE(overflow_power_result.error->code == "runtime.invalid_numeric_result");
}

TEST_CASE("Engine validate reports schema and policy failures with structured diagnostics", "[sdk][engine]") {
    Engine engine;
    Schema schema;
    Policy policy = Policy::default_policy();

    auto unknown_variable = engine.validate("x + 1", schema, policy);
    REQUIRE_FALSE(unknown_variable.ok);
    REQUIRE(unknown_variable.diagnostics.size() == 1);
    REQUIRE(unknown_variable.diagnostics.front().code == "semantics.validator.unknown_variable");

    schema.allow_variable({"x", ValueType::number, true});
    schema.allow_function({"Clamp", FunctionArity::exact(3), {}, ValueType::number, true});

    auto invalid_arity = engine.validate("Clamp[x]", schema, policy);
    REQUIRE_FALSE(invalid_arity.ok);
    REQUIRE(invalid_arity.diagnostics.size() == 1);
    REQUIRE(invalid_arity.diagnostics.front().code == "semantics.validator.invalid_arity");

    policy.set_enable_arithmetic(false);
    auto arithmetic_disabled = engine.validate("x + 1", schema, policy);
    REQUIRE_FALSE(arithmetic_disabled.ok);
    REQUIRE(arithmetic_disabled.diagnostics.size() == 1);
    REQUIRE(arithmetic_disabled.diagnostics.front().code == "semantics.validator.arithmetic_disabled");

    schema.allow_variable({"label", ValueType::string, true});
    auto type_mismatch = engine.validate("label + 1", schema, Policy::default_policy());
    REQUIRE_FALSE(type_mismatch.ok);
    REQUIRE(type_mismatch.diagnostics.size() == 1);
    REQUIRE(type_mismatch.diagnostics.front().code == "semantics.validator.type_mismatch");

    schema.allow_variable({"flag", ValueType::boolean, true});
    auto incompatible_if = engine.validate(R"(If[flag, 1, "no"])", schema, Policy::default_policy());
    REQUIRE_FALSE(incompatible_if.ok);
    REQUIRE(incompatible_if.diagnostics.size() == 1);
    REQUIRE(incompatible_if.diagnostics.front().code == "semantics.validator.incompatible_branch_types");

    schema.allow_function({"AsText", FunctionArity::exact(1), {ValueType::number}, ValueType::string, true});
    auto bad_host_composition = engine.validate("AsText[1] + 2", schema, Policy::default_policy());
    REQUIRE_FALSE(bad_host_composition.ok);
    REQUIRE(bad_host_composition.diagnostics.size() == 1);
    REQUIRE(bad_host_composition.diagnostics.front().code == "semantics.validator.type_mismatch");

    auto constant_division_by_zero = engine.validate("1 / 0", schema, Policy::default_policy());
    REQUIRE_FALSE(constant_division_by_zero.ok);
    REQUIRE(constant_division_by_zero.diagnostics.size() == 1);
    REQUIRE(constant_division_by_zero.diagnostics.front().code == "semantics.validator.division_by_zero");
}

TEST_CASE("Engine compile and evaluate honor literal If branch pruning", "[sdk][engine]") {
    Engine engine;
    Schema schema;

    const auto compile_result = engine.compile("If[True, 1, missing + 1]", schema);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const auto evaluation_result = engine.evaluate(*compile_result.formula, {});
    REQUIRE(evaluation_result.ok());
    REQUIRE(evaluation_result.value.has_value());
    REQUIRE(evaluation_result.value->as_number() != nullptr);
    REQUIRE(*evaluation_result.value->as_number() == 1.0);
}

TEST_CASE("Engine compile and evaluate honor constant-condition If branch pruning", "[sdk][engine]") {
    Engine engine;
    Schema schema;

    const auto compile_result = engine.compile("If[2 * 3 > 5, 11, missing + 1]", schema);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const auto evaluation_result = engine.evaluate(*compile_result.formula, {});
    REQUIRE(evaluation_result.ok());
    REQUIRE(evaluation_result.value.has_value());
    REQUIRE(evaluation_result.value->as_number() != nullptr);
    REQUIRE(*evaluation_result.value->as_number() == 11.0);
}

TEST_CASE("Engine compile and evaluate use schema-valued constants", "[sdk][engine]") {
    Engine engine;
    Schema schema;
    schema.allow_constant({"FeatureEnabled", Value(true)});
    schema.allow_constant({"Offset", Value(2.5)});

    const auto compile_result = engine.compile("If[FeatureEnabled, Offset + 1, missing + 1]", schema);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const auto evaluation_result = engine.evaluate(*compile_result.formula, {});
    REQUIRE(evaluation_result.ok());
    REQUIRE(evaluation_result.value.has_value());
    REQUIRE(evaluation_result.value->as_number() != nullptr);
    REQUIRE(*evaluation_result.value->as_number() == 3.5);
}

TEST_CASE("Engine compile and evaluate SDK numeric built-ins behind policy gating", "[sdk][engine]") {
    Engine engine;
    Schema schema;
    Policy policy = Policy::default_policy();
    policy.set_enable_optional_builtins(true);

    const auto compile_result = engine.compile(
        "Clamp[Sqrt[9] + Floor[2.7], 0, Round[4.4]]",
        schema,
        policy);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const auto evaluation_result = engine.evaluate(*compile_result.formula, {});
    REQUIRE(evaluation_result.ok());
    REQUIRE(evaluation_result.value.has_value());
    REQUIRE(evaluation_result.value->as_number() != nullptr);
    REQUIRE(*evaluation_result.value->as_number() == 4.0);

    const auto ceiling_alias = engine.compile("Ceiling[-3.2]", schema, policy);
    REQUIRE(ceiling_alias.ok());
    const auto ceiling_result = engine.evaluate(*ceiling_alias.formula, {});
    REQUIRE(ceiling_result.ok());
    REQUIRE(ceiling_result.value.has_value());
    REQUIRE(*ceiling_result.value->as_number() == -3.0);
}

TEST_CASE("Engine reports SDK numeric built-in contract violations", "[sdk][engine]") {
    Engine engine;
    Schema schema;
    Policy policy = Policy::default_policy();
    policy.set_enable_optional_builtins(true);

    const auto invalid_sqrt = engine.compile("Sqrt[-1]", schema, policy);
    REQUIRE(invalid_sqrt.ok());
    const auto invalid_sqrt_result = engine.evaluate(*invalid_sqrt.formula, {});
    REQUIRE_FALSE(invalid_sqrt_result.ok());
    REQUIRE(invalid_sqrt_result.error.has_value());
    REQUIRE(invalid_sqrt_result.error->code == "runtime.invalid_numeric_domain");

    const auto invalid_clamp = engine.compile("Clamp[1, 5, 2]", schema, policy);
    REQUIRE(invalid_clamp.ok());
    const auto invalid_clamp_result = engine.evaluate(*invalid_clamp.formula, {});
    REQUIRE_FALSE(invalid_clamp_result.ok());
    REQUIRE(invalid_clamp_result.error.has_value());
    REQUIRE(invalid_clamp_result.error->code == "runtime.invalid_numeric_domain");
}

TEST_CASE("Engine register_function validates the stable host function contract", "[sdk][engine]") {
    Engine engine;

    HostFunctionSpec valid_spec;
    valid_spec.name = "Clamp";
    valid_spec.arity = FunctionArity::exact(3);
    valid_spec.callback = [](std::span<const Value>) {
        EvaluationResult result;
        result.value = Value(0.0);
        return result;
    };

    REQUIRE_NOTHROW(engine.register_function(valid_spec));

    HostFunctionSpec missing_name = valid_spec;
    missing_name.name.clear();
    REQUIRE_THROWS_AS(engine.register_function(missing_name), std::invalid_argument);

    HostFunctionSpec missing_callback = valid_spec;
    missing_callback.callback = {};
    REQUIRE_THROWS_AS(engine.register_function(missing_callback), std::invalid_argument);

    HostFunctionSpec bad_metadata = valid_spec;
    bad_metadata.parameters = {
        {"value", ValueType::number, false},
        {"other", ValueType::number, true}
    };
    REQUIRE_THROWS_AS(engine.register_function(bad_metadata), std::invalid_argument);
}

TEST_CASE("Engine evaluate enforces registered host function contracts", "[sdk][engine]") {
    Engine engine;

    HostFunctionSpec as_text;
    as_text.name = "AsText";
    as_text.arity = FunctionArity::exact(1);
    as_text.parameters = {{"value", ValueType::number, true}};
    as_text.return_type = ValueType::number;
    as_text.callback = [](std::span<const Value>) {
        EvaluationResult result;
        result.value = Value("wrong");
        return result;
    };
    engine.register_function(as_text);

    Schema schema;
    schema.allow_function({"AsText", FunctionArity::exact(1), {ValueType::number}, ValueType::number, true});

    const auto compile_result = engine.compile("AsText[1]", schema);
    REQUIRE(compile_result.ok());

    const auto evaluation_result = engine.evaluate(*compile_result.formula, {});
    REQUIRE_FALSE(evaluation_result.ok());
    REQUIRE(evaluation_result.error.has_value());
    REQUIRE(evaluation_result.error->code == "runtime.invalid_host_result");
}

TEST_CASE("Engine evaluate matches direct kernel evaluation for trusted-subset success semantics", "[sdk][engine][kernel]") {
    Engine engine;
    Schema schema;
    schema.allow_variable({"enabled", ValueType::boolean, true});
    schema.allow_variable({"x", ValueType::number, true});
    schema.allow_constant({"Offset", Value(1.5)});

    Policy policy = Policy::default_policy();
    policy.set_enable_optional_builtins(true);

    constexpr std::string_view source = "If[enabled, Clamp[x + Offset, 0, 10], 0]";

    const auto compile_result = engine.compile(source, schema, policy);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const Bindings bindings = {
        {"enabled", Value(true)},
        {"x", Value(12.0)}
    };

    const auto sdk_result = engine.evaluate(*compile_result.formula, bindings);
    const auto kernel_expr = stage_sdk_source_as_kernel_expr(source, schema, policy);
    const auto kernel_result = evaluate_direct_kernel_expr(
        kernel_expr,
        bindings,
        schema.constant_values(),
        {},
        policy);

    require_same_evaluation_result(sdk_result, kernel_result);
    REQUIRE(sdk_result.ok());
    REQUIRE(*sdk_result.value->as_number() == 10.0);
}

TEST_CASE("Engine evaluate matches direct kernel evaluation for runtime failures", "[sdk][engine][kernel]") {
    Engine engine;
    Schema schema;
    schema.allow_variable({"x", ValueType::number, true});

    constexpr std::string_view source = "1 / x";

    const auto compile_result = engine.compile(source, schema);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const Bindings bindings = {
        {"x", Value(0.0)}
    };

    const auto sdk_result = engine.evaluate(*compile_result.formula, bindings);
    const auto kernel_expr = stage_sdk_source_as_kernel_expr(source, schema, Policy::default_policy());
    const auto kernel_result = evaluate_direct_kernel_expr(
        kernel_expr,
        bindings,
        schema.constant_values(),
        {},
        Policy::default_policy());

    require_same_evaluation_result(sdk_result, kernel_result);
    REQUIRE_FALSE(sdk_result.ok());
    REQUIRE(sdk_result.error->code == "runtime.division_by_zero");
}

TEST_CASE("Engine evaluate matches direct kernel evaluation for host functions", "[sdk][engine][kernel]") {
    Engine engine;

    HostFunctionSpec scale_add;
    scale_add.name = "ScaleAdd";
    scale_add.arity = FunctionArity::exact(3);
    scale_add.parameters = {
        {"value", ValueType::number, true},
        {"scale", ValueType::number, true},
        {"offset", ValueType::number, true}
    };
    scale_add.return_type = ValueType::number;
    scale_add.callback = [](std::span<const Value> args) {
        EvaluationResult result;
        result.value = Value(*args[0].as_number() * *args[1].as_number() + *args[2].as_number());
        return result;
    };

    engine.register_function(scale_add);
    kernel::HostFunctionRegistry host_functions;
    kernel::FunctionRegistry::register_host_function(host_functions, scale_add);

    Schema schema;
    schema.allow_function({"ScaleAdd", FunctionArity::exact(3), {ValueType::number, ValueType::number, ValueType::number}, ValueType::number, true});

    constexpr std::string_view source = "ScaleAdd[4, 1.5, 2]";

    const auto compile_result = engine.compile(source, schema);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const auto sdk_result = engine.evaluate(*compile_result.formula, {});
    const auto kernel_expr = stage_sdk_source_as_kernel_expr(source, schema, Policy::default_policy());
    const auto kernel_result = evaluate_direct_kernel_expr(
        kernel_expr,
        {},
        schema.constant_values(),
        host_functions,
        Policy::default_policy());

    require_same_evaluation_result(sdk_result, kernel_result);
    REQUIRE(sdk_result.ok());
    REQUIRE(*sdk_result.value->as_number() == 8.0);
}

TEST_CASE("Separate engines keep host registrations isolated", "[sdk][engine][kernel]") {
    Engine with_scale_add;
    Engine without_scale_add;

    HostFunctionSpec scale_add;
    scale_add.name = "ScaleAdd";
    scale_add.arity = FunctionArity::exact(3);
    scale_add.parameters = {
        {"value", ValueType::number, true},
        {"scale", ValueType::number, true},
        {"offset", ValueType::number, true}
    };
    scale_add.return_type = ValueType::number;
    scale_add.callback = [](std::span<const Value> args) {
        EvaluationResult result;
        result.value = Value(*args[0].as_number() * *args[1].as_number() + *args[2].as_number());
        return result;
    };
    with_scale_add.register_function(scale_add);

    Schema schema;
    schema.allow_function({"ScaleAdd", FunctionArity::exact(3), {ValueType::number, ValueType::number, ValueType::number}, ValueType::number, true});

    const auto compile_result = with_scale_add.compile("ScaleAdd[4, 1.5, 2]", schema);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const auto with_result = with_scale_add.evaluate(*compile_result.formula, {});
    REQUIRE(with_result.ok());
    REQUIRE(*with_result.value->as_number() == 8.0);

    const auto without_result = without_scale_add.evaluate(*compile_result.formula, {});
    REQUIRE_FALSE(without_result.ok());
    REQUIRE_FALSE(without_result.value.has_value());
}

TEST_CASE("Engine evaluate matches direct kernel evaluation for step-budget exhaustion", "[sdk][engine][kernel]") {
    Engine engine;
    Schema schema;
    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 1;

    constexpr std::string_view source = "1 + 2";

    const auto compile_result = engine.compile(source, schema, policy);
    REQUIRE(compile_result.ok());
    REQUIRE(compile_result.formula.has_value());

    const auto sdk_result = engine.evaluate(*compile_result.formula, {});
    const auto kernel_expr = stage_sdk_source_as_kernel_expr(source, schema, policy);
    const auto kernel_result = evaluate_direct_kernel_expr(
        kernel_expr,
        {},
        schema.constant_values(),
        {},
        policy);

    require_same_evaluation_result(sdk_result, kernel_result);
    REQUIRE_FALSE(sdk_result.ok());
    REQUIRE(sdk_result.error->code == "runtime.step_budget_exhausted");
}
