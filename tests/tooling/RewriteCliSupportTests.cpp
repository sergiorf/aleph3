#include "tooling/RewriteCliSupport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>

using namespace aleph3;

TEST_CASE("CLI evaluate argument parsing accepts bindings and formula text", "[tooling][cli]") {
    const std::array<std::string_view, 5> arguments = {
        "--var",
        "x=3.5",
        "--var",
        "enabled=True",
        "x + 1"
    };

    const auto result = tooling::parse_evaluate_cli_arguments(arguments);

    REQUIRE(result.ok());
    REQUIRE(result.options.formula == "x + 1");
    REQUIRE(result.options.bindings.size() == 2);
    REQUIRE(result.options.bindings.at("x").as_number() != nullptr);
    REQUIRE(*result.options.bindings.at("x").as_number() == 3.5);
    REQUIRE(result.options.bindings.at("enabled").as_boolean() != nullptr);
    REQUIRE(*result.options.bindings.at("enabled").as_boolean());
}

TEST_CASE("REPL evaluate parsing preserves quoted binding values", "[tooling][cli]") {
    const auto result = tooling::parse_evaluate_repl_arguments(
        R"(--var label="hello world" label)");

    REQUIRE(result.ok());
    REQUIRE(result.options.formula == "label");
    REQUIRE(result.options.bindings.size() == 1);
    REQUIRE(result.options.bindings.at("label").as_string() != nullptr);
    REQUIRE(*result.options.bindings.at("label").as_string() == "hello world");
}

TEST_CASE("CLI evaluate parsing rejects malformed bindings and missing formulas", "[tooling][cli]") {
    const std::array<std::string_view, 2> missing_assignment = {
        "--var",
        "x"
    };
    const auto missing_assignment_result =
        tooling::parse_evaluate_cli_arguments(missing_assignment);
    REQUIRE_FALSE(missing_assignment_result.ok());
    REQUIRE(missing_assignment_result.error_message == "Expected variable binding in the form name=value.");

    const std::array<std::string_view, 2> missing_formula = {
        "--var",
        "x=2"
    };
    const auto missing_formula_result =
        tooling::parse_evaluate_cli_arguments(missing_formula);
    REQUIRE_FALSE(missing_formula_result.ok());
    REQUIRE(missing_formula_result.error_message == "A formula is required for `evaluate`.");

    const auto host_missing_formula_result =
        tooling::parse_formula_cli_arguments("evaluate-host", missing_formula);
    REQUIRE_FALSE(host_missing_formula_result.ok());
    REQUIRE(host_missing_formula_result.error_message == "A formula is required for `evaluate-host`.");
}

TEST_CASE("CLI binding type inference matches the public value model", "[tooling][cli]") {
    REQUIRE(tooling::infer_value_type(Value(1.0)) == ValueType::number);
    REQUIRE(tooling::infer_value_type(Value(true)) == ValueType::boolean);
    REQUIRE(tooling::infer_value_type(Value("ok")) == ValueType::string);
    REQUIRE(tooling::infer_value_type(Value::List{Value(1.0)}) == ValueType::list);
    REQUIRE(tooling::infer_value_type(Value()) == ValueType::any);
}
