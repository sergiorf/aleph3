#include "frontend/Parser.hpp"
#include "semantics/Validator.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("Validator rejects unknown variables and functions by default", "[semantics][validator]") {
    frontend::Parser parser("Clamp[x, 0, 1]");
    const auto parse_result = parser.parse();

    REQUIRE(parse_result.ok());

    Schema schema;
    Policy policy;
    semantics::Validator validator(schema, policy);
    const auto summary = validator.validate(parse_result.root);

    REQUIRE_FALSE(summary.ok());
    REQUIRE(summary.diagnostics.size() == 2);
    REQUIRE(summary.diagnostics[0].code == "semantics.validator.unknown_function");
    REQUIRE(summary.diagnostics[1].code == "semantics.validator.unknown_variable");
}

TEST_CASE("Validator accepts allowlisted variables and functions with valid arity", "[semantics][validator]") {
    frontend::Parser parser("Clamp[x, 0, 1]");
    const auto parse_result = parser.parse();

    REQUIRE(parse_result.ok());

    Schema schema;
    schema.allow_variable({"x", ValueType::number, true});
    schema.allow_function({"Clamp", FunctionArity::exact(3), {ValueType::number, ValueType::number, ValueType::number}, ValueType::number, true});

    semantics::Validator validator(schema, Policy::default_policy());
    const auto summary = validator.validate(parse_result.root);

    REQUIRE(summary.ok());
    REQUIRE(summary.diagnostics.empty());
}

TEST_CASE("Validator rejects invalid arity and disabled features", "[semantics][validator]") {
    frontend::Parser parser("Abs[\"x\"]");
    const auto parse_result = parser.parse();

    REQUIRE(parse_result.ok());

    Policy policy = Policy::default_policy();
    policy.set_enable_optional_builtins(true);
    policy.set_enable_strings(false);

    Schema schema;
    semantics::Validator validator(schema, policy);
    const auto summary = validator.validate(parse_result.root);

    REQUIRE_FALSE(summary.ok());
    REQUIRE(summary.diagnostics.size() == 1);
    REQUIRE(summary.diagnostics[0].code == "semantics.validator.string_literals_disabled");

    frontend::Parser arity_parser("Clamp[x]");
    const auto arity_parse_result = arity_parser.parse();
    REQUIRE(arity_parse_result.ok());

    schema.allow_variable({"x", ValueType::number, true});
    schema.allow_function({"Clamp", FunctionArity::exact(3), {}, ValueType::number, true});
    semantics::Validator arity_validator(schema, Policy::default_policy());
    const auto arity_summary = arity_validator.validate(arity_parse_result.root);

    REQUIRE_FALSE(arity_summary.ok());
    REQUIRE(arity_summary.diagnostics.size() == 1);
    REQUIRE(arity_summary.diagnostics[0].code == "semantics.validator.invalid_arity");
}

TEST_CASE("Validator enforces node and depth limits", "[semantics][validator]") {
    frontend::Parser parser("1 + 2 + 3");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    Policy policy = Policy::default_policy();
    policy.budget().max_node_count = 2;
    policy.budget().max_ast_depth = 2;

    Schema schema;
    semantics::Validator validator(schema, policy);
    const auto summary = validator.validate(parse_result.root);

    REQUIRE_FALSE(summary.ok());
    REQUIRE(summary.diagnostics.size() == 2);
    REQUIRE(summary.diagnostics[0].code == "semantics.validator.node_limit_exceeded");
    REQUIRE(summary.diagnostics[1].code == "semantics.validator.depth_limit_exceeded");
}
