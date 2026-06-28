#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "kernel/Assumptions.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "kernel/Rewrite.hpp"
#include "expr/ExprUtils.hpp"
#include "packs/PackRegistry.hpp"
#include "util/Overloaded.hpp"
#include "Constants.hpp"
#include <cmath>

namespace aleph3 {

    namespace {

    constexpr std::size_t REPLACE_REPEATED_MAX_REWRITES = 16;

    // Helper for numeric evaluation of constants and expressions
    inline ExprPtr numeric_eval(const ExprPtr& expr) {
        return std::visit(overloaded{
            [](const Number& num) -> ExprPtr { return make_expr<Number>(num.value); },
            [](const Complex& c) -> ExprPtr { return make_expr<Complex>(c.real, c.imag); },
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
            [](const ComplexInfinity&) -> ExprPtr {
                return make_expr<ComplexInfinity>();
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

    const Rule& require_rule_argument(const ExprPtr& expr, const std::string& name) {
        if (const auto* rule = std::get_if<Rule>(&(*expr))) {
            return *rule;
        }
        throw_invalid_form(name + " expects the second argument to be a Rule");
    }

    EvaluationContext with_added_assumptions(EvaluationContext ctx, const ExprPtr& assumptions) {
        ctx.assumptions.assume(assumptions);
        return ctx;
    }

    ExprPtr evaluate_assumption_predicate(
        const std::string& name,
        const FunctionCall& func,
        EvaluationContext& ctx) {
        if (func.args.size() != 1) {
            throw_invalid_arity_exact(name, 1);
        }

        auto arg = evaluate(func.args[0], ctx);
        if (auto assumed = ctx.assumptions.evaluate_predicate(name, arg); assumed.has_value()) {
            return make_expr<Boolean>(*assumed);
        }

        return make_expr<FunctionCall>(name, std::vector<ExprPtr>{arg});
    }

    }  // namespace

    void register_built_in_functions() {
        auto& registry = packs::PackRegistry::instance();
        auto& function_registry = kernel::FunctionRegistry::instance();

        function_registry.register_head_rewrite(
            "Plus",
            "arithmetic_bucket",
            kernel::rewrite_normalized_arithmetic_head,
            10);
        function_registry.register_head_rewrite(
            "Plus",
            "symbolic_coefficient",
            kernel::rewrite_normalized_symbolic_coefficient_head,
            20);
        function_registry.register_head_rewrite(
            "Times",
            "arithmetic_bucket",
            kernel::rewrite_normalized_arithmetic_head,
            10);
        function_registry.register_head_rewrite(
            "Times",
            "algebraic",
            kernel::rewrite_normalized_algebraic_head,
            20);
        function_registry.register_head_rewrite(
            "Power",
            "power_identity",
            kernel::rewrite_normalized_power_identity_head,
            10);
        function_registry.register_head_rewrite(
            "Power",
            "algebraic",
            kernel::rewrite_normalized_algebraic_head,
            20);

        // String functions
        registry.register_function("StringJoin", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            std::string result;
            for (const auto& arg : func.args) {
                auto evaluated_arg = evaluate(arg, ctx);
                if (std::holds_alternative<String>(*evaluated_arg)) {
                    result += std::get<String>(*evaluated_arg).value;
                }
                else {
                    throw_invalid_form("StringJoin expects string arguments");
                }
            }
            return make_expr<String>(result);
            });

        registry.register_function("StringLength", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 1) {
                throw_invalid_arity_exact("StringLength", 1);
            }
            auto evaluated_arg = evaluate(func.args[0], ctx);
            if (std::holds_alternative<String>(*evaluated_arg)) {
                return make_expr<Number>(static_cast<double>(std::get<String>(*evaluated_arg).value.size()));
            }
            else {
                throw_invalid_form("StringLength expects a string argument");
            }
            });

        registry.register_function("StringReplace", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2) {
                throw_invalid_arity_exact("StringReplace", 2);
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
                throw_invalid_arity_exact("StringTake", 2);
            }
            auto str_arg = evaluate(func.args[0], ctx);
            if (!std::holds_alternative<String>(*str_arg)) {
                throw_invalid_form("StringTake expects the first argument to be a string");
            }
            const std::string& str = std::get<String>(*str_arg).value;

            auto idx_arg = evaluate(func.args[1], ctx);

            // Case 1: StringTake["Hello", 3] -> "Hel"
            if (std::holds_alternative<Number>(*idx_arg)) {
                int n = static_cast<int>(std::get<Number>(*idx_arg).value);
                if (n == 0 || std::abs(n) > static_cast<int>(str.size())) {
                    throw_invalid_form("StringTake expects a valid index or range");
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
                            throw_invalid_form("StringTake expects a valid index or range");
                        }
                        // Convert to 0-based index
                        return make_expr<String>(str.substr(start - 1, end - start + 1));
                    }
                }
            }

            throw_invalid_form("StringTake expects a valid index or range");
            });

        registry.register_function("Length", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 1) {
                throw_invalid_arity_exact("Length", 1);
            }
            auto arg = evaluate(func.args[0], ctx);
            if (std::holds_alternative<List>(*arg)) {
                const auto& list = std::get<List>(*arg);
                return make_expr<Number>(static_cast<double>(list.elements.size()));
            }
            throw_invalid_form("Length expects a list argument");
            });

        registry.register_function("N", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 1) {
                throw_invalid_arity_exact("N", 1);
            }
            auto arg = evaluate(func.args[0], ctx);
            auto num_arg = numeric_eval(arg);
            return evaluate(num_arg, ctx);
            });

        registry.register_function("Replace", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2) {
                throw_invalid_arity_exact("Replace", 2);
            }
            auto expr = evaluate(func.args[0], ctx);
            const Rule& rule = require_rule_argument(func.args[1], "Replace");
            const auto rewritten = kernel::rewrite_once(expr, rule);
            return rewritten.changed ? rewritten.expr : expr;
            });

        registry.register_function("ReplaceRepeated", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2) {
                throw_invalid_arity_exact("ReplaceRepeated", 2);
            }
            auto expr = evaluate(func.args[0], ctx);
            const Rule& rule = require_rule_argument(func.args[1], "ReplaceRepeated");
            const auto rewritten = kernel::rewrite_repeated(
                expr,
                rule,
                ctx,
                REPLACE_REPEATED_MAX_REWRITES);
            return rewritten.changed ? rewritten.expr : expr;
            });

        registry.register_function("MatchQ", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2) {
                throw_invalid_arity_exact("MatchQ", 2);
            }
            auto expr = evaluate(func.args[0], ctx);
            return make_expr<Boolean>(kernel::matches_pattern(func.args[1], expr));
            });

        registry.register_function("Positive", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            return evaluate_assumption_predicate("Positive", func, ctx);
            });

        registry.register_function("Negative", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            return evaluate_assumption_predicate("Negative", func, ctx);
            });

        registry.register_function("NonNegative", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            return evaluate_assumption_predicate("NonNegative", func, ctx);
            });

        registry.register_function("NonPositive", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            return evaluate_assumption_predicate("NonPositive", func, ctx);
            });

        registry.register_function("ZeroQ", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            return evaluate_assumption_predicate("ZeroQ", func, ctx);
            });

        registry.register_function("NonZeroQ", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            return evaluate_assumption_predicate("NonZeroQ", func, ctx);
            });

        registry.register_function("Assuming", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2) {
                throw_invalid_arity_exact("Assuming", 2);
            }
            auto scoped_ctx = with_added_assumptions(ctx, func.args[0]);
            return evaluate(func.args[1], scoped_ctx);
            });

        registry.register_function("Refine", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() < 1 || func.args.size() > 2) {
                throw_invalid_arity_between("Refine", 1, 2);
            }

            EvaluationContext scoped_ctx = ctx;
            if (func.args.size() == 2) {
                scoped_ctx = with_added_assumptions(std::move(scoped_ctx), func.args[1]);
            }

            auto evaluated = evaluate(func.args[0], scoped_ctx);
            return kernel::refine_expr_with_assumptions(evaluated, scoped_ctx.assumptions);
            });
    }

}
