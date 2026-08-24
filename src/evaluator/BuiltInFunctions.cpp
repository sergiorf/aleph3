#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorBuiltins.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/EvaluatorSpecialForms.hpp"
#include "expr/FullForm.hpp"
#include "kernel/Assumptions.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "kernel/Rewrite.hpp"
#include "kernel/VariableAnalysis.hpp"
#include "packs/AlgebraPack.hpp"
#include "packs/CalculusPack.hpp"
#include "expr/ExprUtils.hpp"
#include "transforms/Transforms.hpp"
#include "util/Overloaded.hpp"
#include "Constants.hpp"
#include <cmath>
#include <limits>
#include <string>

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

    kernel::RewriteTraversal require_rewrite_traversal(
        const ExprPtr& expr,
        const std::string& name) {
        const auto require_depth = [&](const ExprPtr& value) -> std::size_t {
            const auto* number = std::get_if<Number>(value.get());
            if (number == nullptr || !std::isfinite(number->value) || number->value < 0.0 ||
                std::floor(number->value) != number->value ||
                number->value > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
                throw_invalid_form(name + " expects a nonnegative integral level or {min, max}");
            }
            return static_cast<std::size_t>(number->value);
        };
        if (std::holds_alternative<Number>(*expr)) {
            const auto depth = require_depth(expr);
            return {depth, depth};
        }
        const auto* list = std::get_if<List>(expr.get());
        if (list == nullptr || list->elements.size() != 2) {
            throw_invalid_form(name + " expects a nonnegative integral level or {min, max}");
        }
        const auto min_depth = require_depth(list->elements[0]);
        const auto max_depth = require_depth(list->elements[1]);
        if (min_depth > max_depth) {
            throw_invalid_form(name + " expects an ordered level range");
        }
        return {min_depth, max_depth};
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

    ExprPtr symbol_set_to_list(const kernel::SymbolSet& symbols) {
        std::vector<ExprPtr> elements;
        elements.reserve(symbols.size());
        for (const auto& symbol : symbols) {
            elements.push_back(make_expr<Symbol>(symbol));
        }
        return std::make_shared<Expr>(List{elements});
    }

    std::string require_symbol_name(const ExprPtr& expr, const std::string& name) {
        const auto* symbol = std::get_if<Symbol>(expr.get());
        if (symbol == nullptr) {
            throw_invalid_form(name + " expects the second argument to be a symbol");
        }
        return symbol->name;
    }

    std::string require_cleanup_symbol_name(const FunctionCall& func, const std::string& name) {
        if (func.args.size() != 1) {
            throw_invalid_arity_exact(name, 1);
        }
        const auto* symbol = std::get_if<Symbol>(func.args[0].get());
        if (symbol == nullptr) {
            throw_invalid_form(name + " expects its argument to be an unevaluated symbol");
        }
        return symbol->name;
    }

    bool has_session_definition_record(
        const EvaluationContext& ctx,
        const std::string& name,
        symbols::SymbolDefinitionKind kind) {
        const auto* records = ctx.definition_records.lookup(name);
        if (records == nullptr) {
            return false;
        }
        for (const auto& record : *records) {
            if (record.kind == kind && record.origin == symbols::DefinitionOrigin::user) {
                return true;
            }
        }
        return false;
    }

    bool has_provider_owned_behavior(const EvaluationContext& ctx, const std::string& name) {
        if (ctx.function_registry().find_symbolic_function_spec(name) != nullptr ||
            ctx.function_registry().find_special_form_spec(name) != nullptr ||
            ctx.function_registry().find_builtin_function_spec(name) != nullptr ||
            ctx.function_registry().has_head_rewrites(name) ||
            kernel::FunctionRegistry::find_host_function(ctx.host_functions(), name) != nullptr) {
            return true;
        }

        const auto* records = ctx.definition_records.lookup(name);
        if (records == nullptr) {
            return false;
        }
        for (const auto& record : *records) {
            if (record.origin != symbols::DefinitionOrigin::user) {
                return true;
            }
        }
        return false;
    }

    bool has_own_value_state(const EvaluationContext& ctx, const std::string& name) {
        return ctx.symbol_values.contains(name) ||
            has_session_definition_record(ctx, name, symbols::SymbolDefinitionKind::own_value);
    }

    bool has_user_function_state(const EvaluationContext& ctx, const std::string& name) {
        return ctx.function_definitions.contains(name) ||
            has_session_definition_record(ctx, name, symbols::SymbolDefinitionKind::user_function);
    }

    void erase_own_value_state(EvaluationContext& ctx, const std::string& name) {
        ctx.symbol_values.erase(name);
        ctx.definition_records.erase(
            name,
            symbols::SymbolDefinitionKind::own_value,
            symbols::DefinitionOrigin::user);
    }

    void erase_user_function_state(EvaluationContext& ctx, const std::string& name) {
        ctx.function_definitions.erase(name);
        ctx.definition_records.erase(
            name,
            symbols::SymbolDefinitionKind::user_function,
            symbols::DefinitionOrigin::user);
    }

    std::string expression_head_name(const ExprPtr& expr) {
        return std::visit(overloaded{
            [](const Symbol&) -> std::string { return "Symbol"; },
            [](const Number& number) -> std::string {
                return std::floor(number.value) == number.value ? "Integer" : "Real";
            },
            [](const Complex&) -> std::string { return "Complex"; },
            [](const Rational&) -> std::string { return "Rational"; },
            [](const Boolean&) -> std::string { return "Boolean"; },
            [](const String&) -> std::string { return "String"; },
            [](const FunctionCall& call) -> std::string { return call.head; },
            [](const FunctionDefinition&) -> std::string { return "FunctionDefinition"; },
            [](const Assignment&) -> std::string { return "Assignment"; },
            [](const Rule&) -> std::string { return "Rule"; },
            [](const List&) -> std::string { return "List"; },
            [](const Infinity&) -> std::string { return "Infinity"; },
            [](const ComplexInfinity&) -> std::string { return "ComplexInfinity"; },
            [](const Indeterminate&) -> std::string { return "Indeterminate"; }
        }, *expr);
    }

    std::size_t require_one_based_index(const ExprPtr& expr, const std::string& name) {
        const auto* number = std::get_if<Number>(expr.get());
        if (number == nullptr || !std::isfinite(number->value) ||
            std::floor(number->value) != number->value || number->value < 1.0 ||
            number->value > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
            throw_invalid_form(name + " expects a positive integer index");
        }
        return static_cast<std::size_t>(number->value);
    }

    const std::vector<ExprPtr>& require_list_argument(
        const ExprPtr& expr,
        const std::string& name,
        std::size_t argument_index) {
        const auto* list = std::get_if<List>(expr.get());
        if (list == nullptr) {
            throw_invalid_form(
                name + " expects argument " + std::to_string(argument_index) + " to be a list");
        }
        return list->elements;
    }

    std::string require_operator_symbol(const ExprPtr& expr, const std::string& name) {
        const auto* symbol = std::get_if<Symbol>(expr.get());
        if (symbol == nullptr) {
            throw_invalid_form(name + " expects its first argument to be a symbol head");
        }
        return symbol->name;
    }

    bool is_unresolved_predicate_call(
        const ExprPtr& evaluated,
        const std::string& predicate_name) {
        const auto* call = std::get_if<FunctionCall>(evaluated.get());
        return call != nullptr && call->head == predicate_name;
    }

    ExprPtr evaluate_fullform_tree(const ExprPtr& expr, EvaluationContext& ctx) {
        auto evaluated = evaluate(expr, ctx);
        if (const auto* call = std::get_if<FunctionCall>(evaluated.get())) {
            std::vector<ExprPtr> args;
            args.reserve(call->args.size());
            for (const auto& arg : call->args) {
                args.push_back(evaluate_fullform_tree(arg, ctx));
            }
            return make_expr<FunctionCall>(call->head, args);
        }
        if (const auto* list = std::get_if<List>(evaluated.get())) {
            std::vector<ExprPtr> elements;
            elements.reserve(list->elements.size());
            for (const auto& element : list->elements) {
                elements.push_back(evaluate_fullform_tree(element, ctx));
            }
            return std::make_shared<Expr>(List{elements});
        }
        if (const auto* rule = std::get_if<Rule>(evaluated.get())) {
            return make_expr<Rule>(
                evaluate_fullform_tree(rule->lhs, ctx),
                evaluate_fullform_tree(rule->rhs, ctx));
        }
        return evaluated;
    }

    }  // namespace

    void register_builtin_rewrite_specs(kernel::FunctionRegistry& registry) {
        registry.register_head_rewrite(
            "Plus",
            "arithmetic_bucket",
            kernel::rewrite_normalized_arithmetic_head,
            10);
        registry.register_head_rewrite(
            "Plus",
            "symbolic_coefficient",
            kernel::rewrite_normalized_symbolic_coefficient_head,
            20);
        registry.register_head_rewrite(
            "Times",
            "arithmetic_bucket",
            kernel::rewrite_normalized_arithmetic_head,
            10);
        registry.register_head_rewrite(
            "Times",
            "algebraic",
            kernel::rewrite_normalized_algebraic_head,
            20);
        registry.register_head_rewrite(
            "Power",
            "power_identity",
            kernel::rewrite_normalized_power_identity_head,
            10);
        registry.register_head_rewrite(
            "Power",
            "algebraic",
            kernel::rewrite_normalized_algebraic_head,
            20);
    }

    void register_symbolic_builtins(kernel::FunctionRegistry& registry) {
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
            if (const auto* call = std::get_if<FunctionCall>(arg.get())) {
                return make_expr<Number>(static_cast<double>(call->args.size()));
            }
            if (const auto* rule = std::get_if<Rule>(arg.get())) {
                return make_expr<Number>(2.0);
            }
            throw_invalid_form("Length expects a compound expression");
            });

        registry.register_function(
            "Clear",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                const auto name = require_cleanup_symbol_name(func, "Clear");
                const bool has_session_state =
                    has_own_value_state(ctx, name) || has_user_function_state(ctx, name);

                if (!has_session_state && has_provider_owned_behavior(ctx, name)) {
                    throw_invalid_form("Clear cannot remove provider-owned symbol `" + name + "`");
                }

                erase_own_value_state(ctx, name);
                erase_user_function_state(ctx, name);
                return make_expr<Symbol>(name);
            },
            {symbols::SymbolAttribute::hold_all});

        registry.register_function(
            "Unset",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                const auto name = require_cleanup_symbol_name(func, "Unset");
                const bool has_own_value = has_own_value_state(ctx, name);
                const bool has_user_function = has_user_function_state(ctx, name);

                if (!has_own_value && !has_user_function && has_provider_owned_behavior(ctx, name)) {
                    throw_invalid_form("Unset cannot remove provider-owned symbol `" + name + "`");
                }

                erase_own_value_state(ctx, name);
                return make_expr<Symbol>(name);
            },
            {symbols::SymbolAttribute::hold_all});

        registry.register_function(
            "Head",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                if (func.args.size() != 1) {
                    throw_invalid_arity_exact("Head", 1);
                }
                return make_expr<Symbol>(expression_head_name(evaluate(func.args[0], ctx)));
            });

        registry.register_function(
            "Part",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                if (func.args.size() != 2) {
                    throw_invalid_arity_exact("Part", 2);
                }
                auto expr = evaluate(func.args[0], ctx);
                const auto index = require_one_based_index(evaluate(func.args[1], ctx), "Part");

                const auto part_at = [&](const std::vector<ExprPtr>& parts) -> ExprPtr {
                    if (index > parts.size()) {
                        throw_invalid_form("Part index is out of range");
                    }
                    return parts[index - 1];
                };

                if (const auto* list = std::get_if<List>(expr.get())) {
                    return part_at(list->elements);
                }
                if (const auto* call = std::get_if<FunctionCall>(expr.get())) {
                    return part_at(call->args);
                }
                if (const auto* rule = std::get_if<Rule>(expr.get())) {
                    return part_at(std::vector<ExprPtr>{rule->lhs, rule->rhs});
                }

                throw_invalid_form("Part cannot extract from an atomic expression");
            });

        registry.register_function(
            "Map",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                if (func.args.size() != 2) {
                    throw_invalid_arity_exact("Map", 2);
                }
                const auto head = require_operator_symbol(func.args[0], "Map");
                auto list_expr = evaluate(func.args[1], ctx);
                const auto& elements = require_list_argument(list_expr, "Map", 2);

                std::vector<ExprPtr> mapped;
                mapped.reserve(elements.size());
                for (const auto& element : elements) {
                    mapped.push_back(evaluate(make_fcall(head, {element}), ctx));
                }
                return std::make_shared<Expr>(List{mapped});
            },
            {symbols::SymbolAttribute::hold_first});

        registry.register_function(
            "Apply",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                if (func.args.size() != 2) {
                    throw_invalid_arity_exact("Apply", 2);
                }
                const auto head = require_operator_symbol(func.args[0], "Apply");
                auto list_expr = evaluate(func.args[1], ctx);
                const auto& elements = require_list_argument(list_expr, "Apply", 2);
                return evaluate(make_fcall(head, elements), ctx);
            },
            {symbols::SymbolAttribute::hold_first});

        registry.register_function(
            "Select",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                if (func.args.size() != 2) {
                    throw_invalid_arity_exact("Select", 2);
                }
                const auto predicate = require_operator_symbol(func.args[1], "Select");
                auto list_expr = evaluate(func.args[0], ctx);
                const auto& elements = require_list_argument(list_expr, "Select", 1);

                std::vector<ExprPtr> selected;
                for (const auto& element : elements) {
                    auto predicate_result = evaluate(make_fcall(predicate, {element}), ctx);
                    if (const auto* boolean = std::get_if<Boolean>(predicate_result.get())) {
                        if (boolean->value) {
                            selected.push_back(element);
                        }
                        continue;
                    }
                    if (is_unresolved_predicate_call(predicate_result, predicate)) {
                        continue;
                    }
                    throw_invalid_form("Select predicate must evaluate to True or False");
                }
                return std::make_shared<Expr>(List{selected});
            },
            {symbols::SymbolAttribute::hold_rest});

        registry.register_function(
            "Cases",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                if (func.args.size() != 2) {
                    throw_invalid_arity_exact("Cases", 2);
                }
                auto list_expr = evaluate(func.args[0], ctx);
                const auto& elements = require_list_argument(list_expr, "Cases", 1);

                std::vector<ExprPtr> matches;
                for (const auto& element : elements) {
                    if (kernel::matches_pattern(func.args[1], element, ctx)) {
                        matches.push_back(element);
                    }
                }
                return std::make_shared<Expr>(List{matches});
            },
            {symbols::SymbolAttribute::hold_rest});

        registry.register_function("N", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 1) {
                throw_invalid_arity_exact("N", 1);
            }
            auto arg = evaluate(func.args[0], ctx);
            auto num_arg = numeric_eval(arg);
            return evaluate(num_arg, ctx);
            });

        registry.register_function("FullForm", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 1) {
                throw_invalid_arity_exact("FullForm", 1);
            }
            return make_expr<String>(to_fullform(evaluate_fullform_tree(func.args[0], ctx)));
            });

        registry.register_function("Simplify", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 1) {
                throw_invalid_arity_exact("Simplify", 1);
            }
            return simplify(evaluate(func.args[0], ctx));
            });

        registry.register_function("Replace", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2 && func.args.size() != 3) {
                throw_invalid_arity_between("Replace", 2, 3);
            }
            auto expr = evaluate(func.args[0], ctx);
            const Rule& rule = require_rule_argument(func.args[1], "Replace");
            const auto traversal = func.args.size() == 3
                ? require_rewrite_traversal(evaluate(func.args[2], ctx), "Replace")
                : kernel::RewriteTraversal{};
            const auto rewritten = kernel::rewrite_once(expr, rule, ctx, traversal);
            return rewritten.changed ? rewritten.expr : expr;
            });

        registry.register_function("ReplaceAll", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2) {
                throw_invalid_arity_exact("ReplaceAll", 2);
            }
            auto expr = evaluate(func.args[0], ctx);
            const Rule& rule = require_rule_argument(func.args[1], "ReplaceAll");
            const auto rewritten = kernel::rewrite_once(expr, rule, ctx, kernel::RewriteTraversal{});
            return rewritten.changed ? rewritten.expr : expr;
            });

        registry.register_function("ReplaceRepeated", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2 && func.args.size() != 3) {
                throw_invalid_arity_between("ReplaceRepeated", 2, 3);
            }
            auto expr = evaluate(func.args[0], ctx);
            const Rule& rule = require_rule_argument(func.args[1], "ReplaceRepeated");
            const auto traversal = func.args.size() == 3
                ? require_rewrite_traversal(evaluate(func.args[2], ctx), "ReplaceRepeated")
                : kernel::RewriteTraversal{};
            const auto rewritten = kernel::rewrite_repeated(
                expr,
                rule,
                ctx,
                traversal,
                REPLACE_REPEATED_MAX_REWRITES);
            return rewritten.changed ? rewritten.expr : expr;
            });

        registry.register_function("MatchQ", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            if (func.args.size() != 2) {
                throw_invalid_arity_exact("MatchQ", 2);
            }
            auto expr = evaluate(func.args[0], ctx);
            return make_expr<Boolean>(kernel::matches_pattern(func.args[1], expr, ctx));
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

        registry.register_function("IntegerQ", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            return evaluate_assumption_predicate("IntegerQ", func, ctx);
            });

        registry.register_function("RationalQ", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            return evaluate_assumption_predicate("RationalQ", func, ctx);
            });

        registry.register_function("RealQ", [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
            return evaluate_assumption_predicate("RealQ", func, ctx);
            });

        registry.register_function(
            "Assuming",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                if (func.args.size() != 2) {
                    throw_invalid_arity_exact("Assuming", 2);
                }
                auto scoped_ctx = with_added_assumptions(ctx, func.args[0]);
                return evaluate(func.args[1], scoped_ctx);
            },
            {symbols::SymbolAttribute::hold_first});

        registry.register_function(
            "Refine",
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                if (func.args.size() < 1 || func.args.size() > 2) {
                    throw_invalid_arity_between("Refine", 1, 2);
                }

                EvaluationContext scoped_ctx = ctx;
                if (func.args.size() == 2) {
                    scoped_ctx = with_added_assumptions(std::move(scoped_ctx), func.args[1]);
                }

                auto evaluated = evaluate(func.args[0], scoped_ctx);
                return kernel::refine_expr_with_assumptions(evaluated, scoped_ctx.assumptions);
            },
            {symbols::SymbolAttribute::hold_rest});

        registry.register_function(
            "FreeVariables",
            [](const FunctionCall& func, EvaluationContext&) -> ExprPtr {
                if (func.args.size() != 1) {
                    throw_invalid_arity_exact("FreeVariables", 1);
                }
                return symbol_set_to_list(kernel::free_variables(func.args[0]));
            },
            {symbols::SymbolAttribute::hold_all});

        registry.register_function(
            "BoundVariables",
            [](const FunctionCall& func, EvaluationContext&) -> ExprPtr {
                if (func.args.size() != 1) {
                    throw_invalid_arity_exact("BoundVariables", 1);
                }
                return symbol_set_to_list(kernel::bound_variables(func.args[0]));
            },
            {symbols::SymbolAttribute::hold_all});

        registry.register_function(
            "DependsOn",
            [](const FunctionCall& func, EvaluationContext&) -> ExprPtr {
                if (func.args.size() != 2) {
                    throw_invalid_arity_exact("DependsOn", 2);
                }
                return make_expr<Boolean>(
                    kernel::depends_on(func.args[0], require_symbol_name(func.args[1], "DependsOn")));
            },
            {symbols::SymbolAttribute::hold_all});
    }

void register_built_in_functions(kernel::FunctionRegistry& registry) {
    register_special_forms(registry);
    register_builtin_rewrite_specs(registry);
    register_symbolic_builtins(registry);
    packs::register_algebra_pack(registry);
    packs::register_calculus_pack(registry);
    register_builtin_evaluator_execution_specs(registry);
}

namespace kernel {

FunctionRegistry create_default_function_registry() {
    FunctionRegistry registry;
    ::aleph3::register_built_in_functions(registry);
    return registry;
}

const FunctionRegistry& default_function_registry() {
    static const FunctionRegistry registry = create_default_function_registry();
    return registry;
}

}  // namespace kernel

}  // namespace aleph3
