#include "FunctionRegistry.hpp"
#include "expr/ExprUtils.hpp"
#include "evaluator/Evaluator.hpp"
#include <cmath>

namespace mathix {

    void register_built_in_functions() {
        auto& registry = FunctionRegistry::instance();

        // Logical operators
        registry.register_function("And", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            for (const auto& arg : func.args) {
                auto evaluated_arg = evaluate(arg, ctx);
                if (std::holds_alternative<Boolean>(*evaluated_arg)) {
                    if (!std::get<Boolean>(*evaluated_arg).value) {
                        return make_expr<Boolean>(false); // Short-circuit if any argument is False
                    }
                }
                else {
                    return make_expr<FunctionCall>("And", func.args); // Return unevaluated
                }
            }
            return make_expr<Boolean>(true); // All arguments are True
            });

        registry.register_function("Or", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            for (const auto& arg : func.args) {
                auto evaluated_arg = evaluate(arg, ctx);
                if (std::holds_alternative<Boolean>(*evaluated_arg)) {
                    if (std::get<Boolean>(*evaluated_arg).value) {
                        return make_expr<Boolean>(true); // Short-circuit if any argument is True
                    }
                }
                else {
                    return make_expr<FunctionCall>("Or", func.args); // Return unevaluated
                }
            }
            return make_expr<Boolean>(false); // All arguments are False
            });

        // String functions
        registry.register_function("StringJoin", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            std::string result;
            for (const auto& arg : func.args) {
                auto evaluated_arg = evaluate(arg, ctx);
                if (std::holds_alternative<String>(*evaluated_arg)) {
                    result += std::get<String>(*evaluated_arg).value;
                }
                else {
                    throw std::runtime_error("StringJoin expects string arguments");
                }
            }
            return make_expr<String>(result);
            });

        registry.register_function("StringLength", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 1) {
                throw std::runtime_error("StringLength expects exactly 1 argument");
            }
            auto evaluated_arg = evaluate(func.args[0], ctx);
            if (std::holds_alternative<String>(*evaluated_arg)) {
                return make_expr<Number>(static_cast<double>(std::get<String>(*evaluated_arg).value.size()));
            }
            else {
                throw std::runtime_error("StringLength expects a string argument");
            }
            });
    }

} // namespace mathix
