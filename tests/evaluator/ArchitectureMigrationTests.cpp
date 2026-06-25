#include "evaluator/BuiltInFunctions.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
#include "packs/PackRegistry.hpp"
#include "parser/Parser.hpp"

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
