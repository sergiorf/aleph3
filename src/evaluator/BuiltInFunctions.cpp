#include "evaluator/FunctionRegistry.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/ExprUtils.hpp"
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

        registry.register_function("StringReplace", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2) {
                throw std::runtime_error("StringReplace expects exactly 2 arguments");
            }
            auto str_arg = evaluate(func.args[0], ctx);
            auto rule_arg = evaluate(func.args[1], ctx);

            if (std::holds_alternative<String>(*str_arg)) {
                const auto& str = std::get<String>(*str_arg).value;
                if (auto* rule = std::get_if<Rule>(&(*rule_arg))) {
                    auto* lhs = std::get_if<String>(&(*rule->lhs));
                    auto* rhs = std::get_if<String>(&(*rule->rhs));
                    if (lhs && rhs) {
                        std::string result = str;
                        size_t pos = 0;
                        while ((pos = result.find(lhs->value, pos)) != std::string::npos) {
                            result.replace(pos, lhs->value.size(), rhs->value);
                            pos += rhs->value.size(); // Move past the replacement
                        }
                        return make_expr<String>(result);
                    }
                }
                // If not a rule, return the original string unchanged (Mathematica behavior)
                return make_expr<String>(str);
            }
            // If not a string, return unevaluated
            return make_expr<FunctionCall>("StringReplace", std::vector<ExprPtr>{str_arg, rule_arg});
            });

        registry.register_function("StringTake", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2) {
                throw std::runtime_error("StringTake expects exactly 2 arguments");
            }
            auto str_arg = evaluate(func.args[0], ctx);
            if (!std::holds_alternative<String>(*str_arg)) {
                throw std::runtime_error("StringTake expects the first argument to be a string");
            }
            const std::string& str = std::get<String>(*str_arg).value;

            auto idx_arg = evaluate(func.args[1], ctx);

            // Case 1: StringTake["Hello", 3] -> "Hel"
            if (std::holds_alternative<Number>(*idx_arg)) {
                int n = static_cast<int>(std::get<Number>(*idx_arg).value);
                if (n == 0 || std::abs(n) > static_cast<int>(str.size())) {
                    throw std::runtime_error("StringTake expects a valid index or range");
                }
                if (n > 0) {
                    return make_expr<String>(str.substr(0, n));
                }
                else { // n < 0
                    return make_expr<String>(str.substr(str.size() + n, -n));
                }
            }

            // Case 2: StringTake["Hello", {2, 4}] -> "ell"
            if (std::holds_alternative<FunctionCall>(*idx_arg)) {
                const auto& call = std::get<FunctionCall>(*idx_arg);
                if (call.head == "List" && call.args.size() == 2) {
                    auto* start_num = std::get_if<Number>(&(*call.args[0]));
                    auto* end_num = std::get_if<Number>(&(*call.args[1]));
                    if (start_num && end_num) {
                        int start = static_cast<int>(start_num->value);
                        int end = static_cast<int>(end_num->value);
                        if (start < 1 || end < start || end > static_cast<int>(str.size())) {
                            throw std::runtime_error("StringTake expects a valid index or range");
                        }
                        // Convert to 0-based index
                        return make_expr<String>(str.substr(start - 1, end - start + 1));
                    }
                }
            }

            throw std::runtime_error("StringTake expects a valid index or range");
            });
    }

} // namespace mathix
