#include "frontend/Parser.hpp"
#include "semantics/Validator.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

namespace {

bool contains_diagnostic_code(
    const std::vector<Diagnostic>& diagnostics,
    std::string_view code) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("Validator rejects unknown variables and disabled built-ins by default", "[semantics][validator]") {
    frontend::Parser parser("Clamp[x, 0, 1]");
    const auto parse_result = parser.parse();

    REQUIRE(parse_result.ok());

    Schema schema;
    Policy policy;
    semantics::Validator validator(schema, policy);
    const auto summary = validator.validate(parse_result.root);

    REQUIRE_FALSE(summary.ok());
    REQUIRE(summary.diagnostics.size() == 2);
    REQUIRE(summary.diagnostics[0].code == "semantics.validator.optional_builtin_disabled");
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
    REQUIRE(contains_diagnostic_code(arithmetic_summary.diagnostics, "semantics.validator.type_mismatch"));

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

TEST_CASE("Validator rejects constant division by zero before runtime", "[semantics][validator]") {
    frontend::Parser parser("1 / 0");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    Schema schema;
    semantics::Validator validator(schema, Policy::default_policy());
    const auto summary = validator.validate(parse_result.root);

    REQUIRE_FALSE(summary.ok());
    REQUIRE(summary.diagnostics.size() == 1);
    REQUIRE(summary.diagnostics[0].code == "semantics.validator.division_by_zero");
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

TEST_CASE("Validator recognizes SDK numeric built-ins behind the optional-builtins gate", "[semantics][validator]") {
    frontend::Parser parser("Clamp[Sqrt[9], Floor[2.7], Ceil[3.2]]");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    Schema schema;
    Policy policy = Policy::default_policy();
    policy.set_enable_optional_builtins(true);

    semantics::Validator validator(schema, policy);
    const auto summary = validator.validate(parse_result.root);

    REQUIRE(summary.ok());
    REQUIRE(summary.diagnostics.empty());
}

TEST_CASE("Validator enforces SDK numeric built-in arity", "[semantics][validator]") {
    frontend::Parser parser("Clamp[1, 2]");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    Schema schema;
    Policy policy = Policy::default_policy();
    policy.set_enable_optional_builtins(true);

    semantics::Validator validator(schema, policy);
    const auto summary = validator.validate(parse_result.root);

    REQUIRE_FALSE(summary.ok());
    REQUIRE(summary.diagnostics.size() == 1);
    REQUIRE(summary.diagnostics[0].code == "semantics.validator.invalid_arity");
}

TEST_CASE("Validator rejects incompatible If branch result types", "[semantics][validator]") {
    Schema schema;
    schema.allow_variable({"flag", ValueType::boolean, true});

    frontend::Parser parser(R"(If[flag, 1, "no"])");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    semantics::Validator validator(schema, Policy::default_policy());
    const auto summary = validator.validate(parse_result.root);

    REQUIRE_FALSE(summary.ok());
    REQUIRE(summary.diagnostics.size() == 1);
    REQUIRE(summary.diagnostics[0].code == "semantics.validator.incompatible_branch_types");
}

TEST_CASE("Validator prunes unreachable literal If branches", "[semantics][validator]") {
    Schema schema;

    frontend::Parser parser(R"(If[True, 1, missing + 1])");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    semantics::Validator validator(schema, Policy::default_policy());
    const auto summary = validator.validate(parse_result.root);

    REQUIRE(summary.ok());
    REQUIRE(summary.diagnostics.empty());
}

TEST_CASE("Validator prunes unreachable constant-condition If branches", "[semantics][validator]") {
    Schema schema;

    frontend::Parser parser(R"(If[1 + 1 == 2, 7, missing + 1])");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    semantics::Validator validator(schema, Policy::default_policy());
    const auto summary = validator.validate(parse_result.root);

    REQUIRE(summary.ok());
    REQUIRE(summary.diagnostics.empty());
}

TEST_CASE("Validator uses schema-valued constants as compile-time facts", "[semantics][validator]") {
    Schema schema;
    schema.allow_constant({"FeatureEnabled", Value(true)});
    schema.allow_constant({"Threshold", Value(2.0)});

    frontend::Parser parser("If[FeatureEnabled, Threshold + 1, missing + 1]");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    semantics::Validator validator(schema, Policy::default_policy());
    const auto summary = validator.validate(parse_result.root);

    REQUIRE(summary.ok());
    REQUIRE(summary.diagnostics.empty());
}

TEST_CASE("Validator ignores constant division by zero in unreachable branches", "[semantics][validator]") {
    Schema schema;

    frontend::Parser parser("If[False, 1 / 0, 9]");
    const auto parse_result = parser.parse();
    REQUIRE(parse_result.ok());

    semantics::Validator validator(schema, Policy::default_policy());
    const auto summary = validator.validate(parse_result.root);

    REQUIRE(summary.ok());
    REQUIRE(summary.diagnostics.empty());
}

TEST_CASE("Validator propagates host function return types into surrounding expressions", "[semantics][validator]") {
    Schema schema;
    schema.allow_function({
        "AsText",
        FunctionArity::exact(1),
        {ValueType::number},
        ValueType::string,
        true});
    schema.allow_function({
        "IsReady",
        FunctionArity::exact(1),
        {ValueType::number},
        ValueType::boolean,
        true});

    semantics::Validator validator(schema, Policy::default_policy());

    frontend::Parser arithmetic_parser("AsText[1] + 2");
    const auto arithmetic_parse_result = arithmetic_parser.parse();
    REQUIRE(arithmetic_parse_result.ok());
    const auto arithmetic_summary = validator.validate(arithmetic_parse_result.root);
    REQUIRE_FALSE(arithmetic_summary.ok());
    REQUIRE(contains_diagnostic_code(arithmetic_summary.diagnostics, "semantics.validator.type_mismatch"));

    frontend::Parser comparison_parser("IsReady[1] < 2");
    const auto comparison_parse_result = comparison_parser.parse();
    REQUIRE(comparison_parse_result.ok());
    const auto comparison_summary = validator.validate(comparison_parse_result.root);
    REQUIRE_FALSE(comparison_summary.ok());
    REQUIRE(contains_diagnostic_code(comparison_summary.diagnostics, "semantics.validator.type_mismatch"));
}
