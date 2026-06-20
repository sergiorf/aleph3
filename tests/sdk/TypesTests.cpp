#include "sdk/Policy.hpp"
#include "sdk/Schema.hpp"
#include "sdk/Types.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("Value exposes typed accessors for supported public runtime values", "[sdk][types]") {
    Value number(3.5);
    Value boolean(true);
    Value string("abc");
    Value list(Value::List{Value(1.0), Value(2.0)});

    REQUIRE(number.is_number());
    REQUIRE(number.as_number() != nullptr);
    REQUIRE(*number.as_number() == 3.5);

    REQUIRE(boolean.is_boolean());
    REQUIRE(boolean.as_boolean() != nullptr);
    REQUIRE(*boolean.as_boolean());

    REQUIRE(string.is_string());
    REQUIRE(string.as_string() != nullptr);
    REQUIRE(*string.as_string() == "abc");

    REQUIRE(list.is_list());
    REQUIRE(list.as_list() != nullptr);
    REQUIRE(list.as_list()->size() == 2);
}

TEST_CASE("Schema tracks allowed variables, functions, and constants", "[sdk][schema]") {
    Schema schema;
    schema.allow_variable({"x", ValueType::number, true});
    schema.allow_function({"Clamp", FunctionArity::exact(3), {ValueType::number, ValueType::number, ValueType::number}, ValueType::number, true});
    schema.allow_constant("Pi");

    REQUIRE(schema.variables().contains("x"));
    REQUIRE(schema.functions().contains("Clamp"));
    REQUIRE(schema.constants().contains("Pi"));

    schema.set_allow_unknown_variables(true);
    schema.set_allow_unknown_functions(true);
    REQUIRE(schema.allows_unknown_variables());
    REQUIRE(schema.allows_unknown_functions());
}

TEST_CASE("Policy profiles keep the rewrite boundary deterministic", "[sdk][policy]") {
    const auto default_policy = Policy::default_policy();
    const auto strict_policy = Policy::strict_numeric();

    REQUIRE(default_policy.enable_strings());
    REQUIRE(default_policy.enable_conditionals());
    REQUIRE(default_policy.enable_host_functions());

    REQUIRE_FALSE(strict_policy.enable_lists());
    REQUIRE_FALSE(strict_policy.enable_optional_builtins());
    REQUIRE_FALSE(strict_policy.allow_assignments());
}
