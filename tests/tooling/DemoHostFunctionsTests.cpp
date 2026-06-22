#include "tooling/DemoHostFunctions.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("Demo host functions can be registered and evaluated through the SDK", "[tooling][host]") {
    Engine engine;
    Schema schema;
    tooling::register_demo_host_functions(engine, schema);
    schema.allow_variable({"x", ValueType::number, true});
    schema.allow_variable({"flag", ValueType::boolean, true});

    const auto numeric_formula = engine.compile("Clamp[ScaleAdd[x, 2, 1], 0, 10]", schema);
    REQUIRE(numeric_formula.ok());

    const auto numeric_result = engine.evaluate(*numeric_formula.formula, {{"x", Value(6.0)}});
    REQUIRE(numeric_result.ok());
    REQUIRE(numeric_result.value->as_number() != nullptr);
    REQUIRE(*numeric_result.value->as_number() == 10.0);

    const auto label_formula = engine.compile(R"(PickLabel[flag, "ok", "fail"])", schema);
    REQUIRE(label_formula.ok());

    const auto label_result = engine.evaluate(*label_formula.formula, {{"flag", Value(false)}});
    REQUIRE(label_result.ok());
    REQUIRE(label_result.value->as_string() != nullptr);
    REQUIRE(*label_result.value->as_string() == "fail");
}

TEST_CASE("Demo host function docs stay aligned with the registered demo bundle", "[tooling][host]") {
    const auto names = tooling::demo_host_function_names();
    const auto docs = tooling::demo_host_function_docs();

    REQUIRE(names.size() == docs.size());
    REQUIRE_FALSE(docs.empty());

    for (std::size_t i = 0; i < docs.size(); ++i) {
        REQUIRE(docs[i].name == names[i]);
        REQUIRE_FALSE(docs[i].signature.empty());
        REQUIRE_FALSE(docs[i].description.empty());
        REQUIRE_FALSE(docs[i].cli_example.empty());
        REQUIRE_FALSE(docs[i].repl_example.empty());
    }
}
