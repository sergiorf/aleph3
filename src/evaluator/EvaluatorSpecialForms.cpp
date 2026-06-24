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

    if (name == "And") {
        if (nargs == 0) {
            return make_expr<Boolean>(true);
        }

        std::vector<ExprPtr> evaluated_prefix;
        evaluated_prefix.reserve(nargs);

        for (size_t i = 0; i < nargs; ++i) {
            auto evaluated_arg = evaluate(func.args[i], ctx);
            if (std::holds_alternative<Boolean>(*evaluated_arg)) {
                const bool value = std::get<Boolean>(*evaluated_arg).value;
                if (!value) {
                    return make_expr<Boolean>(false);
                }
                evaluated_prefix.push_back(evaluated_arg);
                continue;
            }

            std::vector<ExprPtr> residual_args = evaluated_prefix;
            residual_args.push_back(evaluated_arg);
            residual_args.insert(residual_args.end(), func.args.begin() + static_cast<std::ptrdiff_t>(i + 1), func.args.end());
            return make_expr<FunctionCall>("And", residual_args);
        }

        return make_expr<Boolean>(true);
    }

    if (name == "Or") {
        if (nargs == 0) {
            return make_expr<Boolean>(false);
        }

        std::vector<ExprPtr> evaluated_prefix;
        evaluated_prefix.reserve(nargs);

        for (size_t i = 0; i < nargs; ++i) {
            auto evaluated_arg = evaluate(func.args[i], ctx);
            if (std::holds_alternative<Boolean>(*evaluated_arg)) {
                const bool value = std::get<Boolean>(*evaluated_arg).value;
                if (value) {
                    return make_expr<Boolean>(true);
                }
                evaluated_prefix.push_back(evaluated_arg);
                continue;
            }

            std::vector<ExprPtr> residual_args = evaluated_prefix;
            residual_args.push_back(evaluated_arg);
            residual_args.insert(residual_args.end(), func.args.begin() + static_cast<std::ptrdiff_t>(i + 1), func.args.end());
            return make_expr<FunctionCall>("Or", residual_args);
        }

        return make_expr<Boolean>(false);
    }

    throw_unsupported_construct("Unknown special form: " + name);
}

}  // namespace aleph3
