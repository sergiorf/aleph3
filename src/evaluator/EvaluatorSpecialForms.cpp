#include "evaluator/EvaluatorSpecialForms.hpp"

#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/Evaluator.hpp"

#include <unordered_set>

namespace aleph3 {

bool is_special_form_function(const std::string& name) {
    return name == "If";
}

ExprPtr evaluate_special_form(const FunctionCall& func, EvaluationContext& ctx) {
    const std::string& name = func.head;
    const size_t nargs = func.args.size();

    if (name == "If") {
        if (nargs != 3) {
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
