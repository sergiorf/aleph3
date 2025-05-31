#include "evaluator/FunctionRegistry.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/ExprUtils.hpp"
#include "Constants.hpp"
#include <cmath>

namespace aleph3 {

    // Helper for numeric evaluation of constants and expressions
    inline ExprPtr numeric_eval(const ExprPtr& expr) {
        return std::visit(overloaded{
            [](const Number& num) -> ExprPtr { return make_expr<Number>(num.value); },
            [](const Rational& rat) -> ExprPtr {
                return make_expr<Number>(static_cast<double>(rat.numerator) / rat.denominator);
            },
            [](const Boolean& boolean) -> ExprPtr { return make_expr<Boolean>(boolean.value); },
            [](const String& str) -> ExprPtr { return make_expr<String>(str.value); },
            [](const Symbol& sym) -> ExprPtr {
                if (sym.name == "Pi") return make_expr<Number>(PI);
                if (sym.name == "E") return make_expr<Number>(E);
                if (sym.name == "Degree") return make_expr<Number>(PI / 180.0);
                // Add more constants as needed
                return make_expr<Symbol>(sym.name);
            },
            [](const Infinity&) -> ExprPtr {
                return make_expr<Infinity>();
            },
            [](const Indeterminate&) -> ExprPtr {
                return make_expr<Indeterminate>();
            },
            [](const List& list) -> ExprPtr {
                std::vector<ExprPtr> evaluated;
                for (const auto& elem : list.elements) {
                    evaluated.push_back(numeric_eval(elem));
                }
                return std::make_shared<Expr>(List{evaluated});
            },
            [](const FunctionDefinition& def) -> ExprPtr {
                return make_expr<FunctionDefinition>(def.name, def.params, def.body, def.delayed);
            },
            [](const Assignment& assign) -> ExprPtr {
                return make_expr<Assignment>(assign.name, assign.value);
            },
            [](const Rule& rule) -> ExprPtr {
                return make_expr<Rule>(numeric_eval(rule.lhs), numeric_eval(rule.rhs));
            },
            [](const FunctionCall& func) -> ExprPtr {
                std::vector<ExprPtr> evaluated_args;
                for (const auto& arg : func.args) {
                    evaluated_args.push_back(numeric_eval(arg));
                }
                return make_expr<FunctionCall>(func.head, evaluated_args);
            }
            }, *expr);
    }

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
            if (std::holds_alternative<List>(*idx_arg)) {
                const auto& list = std::get<List>(*idx_arg);
                if (list.elements.size() == 2) {
                    auto* start_num = std::get_if<Number>(&(*list.elements[0]));
                    auto* end_num = std::get_if<Number>(&(*list.elements[1]));
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

        registry.register_function("Length", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 1) {
                throw std::runtime_error("Length expects exactly 1 argument");
            }
            auto arg = evaluate(func.args[0], ctx);
            if (std::holds_alternative<List>(*arg)) {
                const auto& list = std::get<List>(*arg);
                return make_expr<Number>(static_cast<double>(list.elements.size()));
            }
            throw std::runtime_error("Length expects a list argument");
            });

        registry.register_function("N", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 1) {
                throw std::runtime_error("N expects exactly 1 argument");
            }
            auto arg = evaluate(func.args[0], ctx);
            auto num_arg = numeric_eval(arg);
            return evaluate(num_arg, ctx);
            });
    }

}
