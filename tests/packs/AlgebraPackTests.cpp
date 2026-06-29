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
