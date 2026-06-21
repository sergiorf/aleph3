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

TEST_CASE("Validator rejects obvious type mismatches before runtime", "[semantics][validator]") {
    Schema schema;
    schema.allow_variable({"x", ValueType::number, true});
    schema.allow_variable({"flag", ValueType::boolean, true});
    schema.allow_variable({"label", ValueType::string, true});

    const Policy policy = Policy::default_policy();
    semantics::Validator validator(schema, policy);

    frontend::Parser arithmetic_parser("label + 1");
    const auto arithmetic_parse_result = arithmetic_parser.parse();
    REQUIRE(arithmetic_parse_result.ok());
    const auto arithmetic_summary = validator.validate(arithmetic_parse_result.root);
    REQUIRE_FALSE(arithmetic_summary.ok());
    REQUIRE(arithmetic_summary.diagnostics.size() == 1);
    REQUIRE(arithmetic_summary.diagnostics[0].code == "semantics.validator.type_mismatch");

    frontend::Parser conditional_parser("If[1, x, 0]");
    const auto conditional_parse_result = conditional_parser.parse();
    REQUIRE(conditional_parse_result.ok());
    const auto conditional_summary = validator.validate(conditional_parse_result.root);
    REQUIRE_FALSE(conditional_summary.ok());
    REQUIRE(conditional_summary.diagnostics.size() == 1);
    REQUIRE(conditional_summary.diagnostics[0].code == "semantics.validator.type_mismatch");

    frontend::Parser equality_parser("flag == 1");
    const auto equality_parse_result = equality_parser.parse();
    REQUIRE(equality_parse_result.ok());
    const auto equality_summary = validator.validate(equality_parse_result.root);
    REQUIRE_FALSE(equality_summary.ok());
    REQUIRE(equality_summary.diagnostics.size() == 1);
    REQUIRE(equality_summary.diagnostics[0].code == "semantics.validator.type_mismatch");
}

TEST_CASE("Validator checks function argument types from schema metadata", "[semantics][validator]") {
    frontend::Parser parser("Clamp[label, 0, 1]");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    Schema schema;
    schema.allow_variable({"label", ValueType::string, true});
    schema.allow_function({
        "Clamp",
        FunctionArity::exact(3),
        {ValueType::number, ValueType::number, ValueType::number},
        ValueType::number,
        true});

    semantics::Validator validator(schema, Policy::default_policy());
    const auto summary = validator.validate(parse_result.root);

    REQUIRE_FALSE(summary.ok());
    REQUIRE(summary.diagnostics.size() == 1);
    REQUIRE(summary.diagnostics[0].code == "semantics.validator.invalid_argument_type");
}

TEST_CASE("Validator checks optional built-in argument types", "[semantics][validator]") {
    frontend::Parser parser("Abs[\"x\"]");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    Schema schema;
    Policy policy = Policy::default_policy();
    policy.set_enable_optional_builtins(true);

    semantics::Validator validator(schema, policy);
    const auto summary = validator.validate(parse_result.root);

    REQUIRE_FALSE(summary.ok());
    REQUIRE(summary.diagnostics.size() == 1);
    REQUIRE(summary.diagnostics[0].code == "semantics.validator.invalid_argument_type");
}
