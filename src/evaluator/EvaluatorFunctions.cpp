#include "evaluator/EvaluatorFunctions.hpp"

#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/Evaluator.hpp"

namespace aleph3 {

namespace {

std::vector<ExprPtr> bind_user_function_arguments(
    const FunctionDefinition& def,
    const FunctionCall& func) {
    const size_t param_count = def.params.size();
    const size_t arg_count = func.args.size();
    std::vector<ExprPtr> final_args;
    final_args.reserve(param_count);

    for (size_t i = 0; i < param_count; ++i) {
        if (i < arg_count) {
            final_args.push_back(func.args[i]);
            continue;
        }

        if (def.params[i].default_value != nullptr) {
            final_args.push_back(def.params[i].default_value);
            continue;
        }

        throw_invalid_arity_at_least("Function " + func.head, i + 1, arg_count);
    }

    if (arg_count > param_count) {
        throw_invalid_arity_at_most("Function " + func.head, param_count, arg_count);
    }

    return final_args;
}

}  // namespace

bool is_user_defined_function(const std::string& name, const EvaluationContext& ctx) {
    return ctx.function_definitions.contains(name);
}

ExprPtr evaluate_user_defined_function(const FunctionCall& func, EvaluationContext& ctx) {
    const FunctionDefinition* def = ctx.function_definitions.lookup(func.head);
    if (def == nullptr) {
        throw_unsupported_construct("Unknown user-defined function: " + func.head);
    }
    auto final_args = bind_user_function_arguments(*def, func);

    EvaluationContext local_ctx = ctx;
    for (size_t i = 0; i < def->params.size(); ++i) {
        local_ctx.symbol_values.set(def->params[i].name, evaluate(final_args[i], ctx));
    }

    return evaluate(def->body, local_ctx);
}

ExprPtr register_user_defined_function(const FunctionDefinition& def, EvaluationContext& ctx) {
    if (def.delayed) {
        ctx.function_definitions.set(def.name, def);
        return make_expr<FunctionDefinition>(def.name, def.params, def.body, true);
    }

    EvaluationContext local_ctx = ctx;
    for (const auto& param : def.params) {
        local_ctx.symbol_values.set(param.name, make_expr<Symbol>(param.name));
    }

    auto evaluated_body = evaluate(def.body, local_ctx);
    ctx.function_definitions.set(
        def.name,
        FunctionDefinition(def.name, def.params, evaluated_body, false));
    return evaluated_body;
}

}  // namespace aleph3
