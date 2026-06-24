#include "evaluator/EvaluatorSpecialForms.hpp"

#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorSemantics.hpp"

namespace aleph3 {

ExprPtr evaluate_special_form(const FunctionCall& func, EvaluationContext& ctx) {
    const std::string& name = func.head;
    const size_t nargs = func.args.size();

    if (name == "If") {
        const auto* semantics = lookup_function_semantics(name);
        if (!semantics) {
            throw_unsupported_construct("Missing evaluation semantics for special form: " + name);
        }
        if (!validate_function_arity(name, nargs)) {
            throw_invalid_arity_exact("If", 3);
        }

        auto condition = evaluate(func.args[0], ctx);
        if (std::holds_alternative<Boolean>(*condition)) {
            const bool cond = std::get<Boolean>(*condition).value;
            return cond ? evaluate(func.args[1], ctx) : evaluate(func.args[2], ctx);
        }
        return make_expr<FunctionCall>(name, func.args);
    }

    throw_unsupported_construct("Unknown special form: " + name);
}

}  // namespace aleph3
