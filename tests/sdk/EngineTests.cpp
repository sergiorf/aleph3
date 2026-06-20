#include "sdk/Engine.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using namespace aleph3;

TEST_CASE("Engine compile rejects blank source with structured diagnostics", "[sdk][engine]") {
    Engine engine;
    Schema schema;

    const auto result = engine.compile("   ", schema);

    REQUIRE_FALSE(result.ok());
    REQUIRE_FALSE(result.formula.has_value());
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics.front().code == "sdk.source.empty");
}

TEST_CASE("Engine validate returns a rewrite-path placeholder diagnostic", "[sdk][engine]") {
    Engine engine;
    Schema schema;

    const auto result = engine.validate("x + 1", schema);

    REQUIRE_FALSE(result.ok);
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics.front().code == "sdk.validate.not_implemented");
}

TEST_CASE("Engine evaluate rejects empty compiled formulas", "[sdk][engine]") {
    Engine engine;
    CompiledFormula formula;

    const auto result = engine.evaluate(formula, {});

    REQUIRE_FALSE(result.ok());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->code == "sdk.formula.empty");
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
}
