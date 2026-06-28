#include "tooling/SymbolicCliSupport.hpp"

#include "evaluator/BuiltInFunctions.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/Expr.hpp"
#include "expr/FullForm.hpp"
#include "parser/Parser.hpp"
#include "transforms/Transforms.hpp"

#include <exception>

namespace aleph3::tooling {

namespace {

SymbolicCliResult make_success(std::string output) {
    SymbolicCliResult result;
    result.ok = true;
    result.output = std::move(output);
    return result;
}

SymbolicCliResult make_failure(std::string message) {
    SymbolicCliResult result;
    result.error_message = std::move(message);
    return result;
}

template <typename Fn>
SymbolicCliResult run_symbolic_expression(std::string_view source, Fn&& transform) {
    if (source.empty()) {
        return make_failure("A symbolic expression is required.");
    }

    try {
        EvaluationContext ctx(kernel::default_function_registry());
        auto expr = parse_expression(std::string(source));
        auto result = transform(expr, ctx);
        return make_success(std::move(result));
    } catch (const std::exception& ex) {
        return make_failure(ex.what());
    }
}

}  // namespace

SymbolicCliResult symbolic_evaluate_expression(std::string_view source) {
    return run_symbolic_expression(source, [](const ExprPtr& expr, EvaluationContext& ctx) {
        return to_string(evaluate(expr, ctx));
    });
}

SymbolicCliResult symbolic_simplify_expression(std::string_view source) {
    return run_symbolic_expression(source, [](const ExprPtr& expr, EvaluationContext& ctx) {
        return to_string(simplify(evaluate(expr, ctx)));
    });
}

SymbolicCliResult symbolic_fullform_expression(std::string_view source) {
    return run_symbolic_expression(source, [](const ExprPtr& expr, EvaluationContext&) {
        return to_fullform(expr);
    });
}

}  // namespace aleph3::tooling
