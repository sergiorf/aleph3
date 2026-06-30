#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "parser/Parser.hpp"
#include "packs/AlgebraPack.hpp"
#include "transforms/Transforms.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

namespace {

ExprPtr evaluate_source(std::string_view source, EvaluationContext& ctx) {
    return evaluate(parse_expression(std::string(source)), ctx);
}

std::string simplify_string(const ExprPtr& expr) {
    return to_string(simplify(expr));
}

}  // namespace

TEST_CASE("Algebra pack functions evaluate through registry-backed handlers", "[packs][algebra]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);
    EvaluationContext ctx(registry);

    const auto expanded = evaluate_source("Expand[(x + 1) * (x + 2)]", ctx);
    REQUIRE(simplify_string(expanded) == "x^2 + 3 * x + 2");

    const auto* expand_spec = registry.find_symbolic_function_spec("Expand");
    REQUIRE(expand_spec != nullptr);
    REQUIRE(expand_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(expand_spec->metadata.owning_package == "core-algebra");
}

TEST_CASE("Algebra pack exact-capable helpers preserve exact multivariate outputs through registration", "[packs][algebra][exact]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);
    EvaluationContext ctx(registry);

    const auto expanded = evaluate_source("Expand[(1/2) * (x + y)]", ctx);
    REQUIRE(simplify_string(expanded) == "x * 1/2 + y * 1/2");

    const auto collected = evaluate_source("Collect[(1/2) * x * y + (3/2) * y, y]", ctx);
    REQUIRE(simplify_string(collected) == "x * y * 1/2 + y * 3/2");

    const auto* expand_spec = registry.find_symbolic_function_spec("Expand");
    const auto* collect_spec = registry.find_symbolic_function_spec("Collect");
    REQUIRE(expand_spec != nullptr);
    REQUIRE(collect_spec != nullptr);
    REQUIRE(expand_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(collect_spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(expand_spec->metadata.owning_package == "core-algebra");
    REQUIRE(collect_spec->metadata.owning_package == "core-algebra");
}

TEST_CASE("Algebra pack registers the full documented helper surface", "[packs][algebra]") {
    kernel::FunctionRegistry registry;
    packs::register_algebra_pack(registry);

    for (const auto* name : {"Expand", "Factor", "Collect", "GCD", "PolynomialQuotient"}) {
        const auto* spec = registry.find_symbolic_function_spec(name);
        REQUIRE(spec != nullptr);
        REQUIRE(spec->metadata.source == kernel::RegistrationSource::pack);
        REQUIRE(spec->metadata.owning_package == "core-algebra");
    }
}

TEST_CASE("Default registry exposes algebra through the pack path", "[packs][algebra]") {
    EvaluationContext ctx(kernel::default_function_registry());

    const auto result = evaluate_source("Expand[(x + 1) * (x + 2)]", ctx);
    REQUIRE(simplify_string(result) == "x^2 + 3 * x + 2");

    const auto* expand =
        kernel::default_function_registry().find_symbolic_function_spec("Expand");
    REQUIRE(expand != nullptr);
    REQUIRE(expand->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(expand->metadata.owning_package == "core-algebra");
}
