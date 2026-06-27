#include "evaluator/Evaluator.hpp"

#include "ExtraMath.hpp"
#include "evaluator/EvaluatorAlgebra.hpp"
#include "evaluator/EvaluatorBuiltins.hpp"
#include "evaluator/EvaluatorFunctions.hpp"
#include "evaluator/EvaluatorSemantics.hpp"
#include "evaluator/EvaluatorSpecialForms.hpp"
#include "kernel/Diagnostics.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "packs/PackRegistry.hpp"
#include "expr/ExprUtils.hpp"
#include "normalizer/Normalizer.hpp"
#include "util/Logging.hpp"
#include "util/Overloaded.hpp"

#include <algorithm>
#include <optional>
#include <unordered_set>

namespace aleph3 {

namespace {

ExprPtr evaluate_impl(const ExprPtr& expr, EvaluationContext& ctx, std::unordered_set<std::string>& visited);

struct FunctionDispatchContract {
    bool has_registered_symbolic_handler = false;
    bool has_user_function = false;
    bool has_host_function = false;
};

void ensure_symbol_metadata(
    EvaluationContext& ctx,
    const std::string& name,
    symbols::DefinitionOrigin origin,
    std::string provider = {},
    std::vector<symbols::SymbolAttribute> attributes = {},
    std::string documentation = {}) {
    ctx.symbol_metadata.ensure(name, symbols::SymbolMetadata{
        name,
        std::move(attributes),
        std::move(documentation),
        origin,
        std::move(provider)});
}

void ensure_definition_record(
    EvaluationContext& ctx,
    const std::string& name,
    symbols::SymbolDefinitionKind kind,
    symbols::DefinitionOrigin origin,
    std::string provider = {}) {
    ctx.definition_records.add_unique(name, symbols::SymbolDefinitionRecord{
        kind,
        origin,
        std::move(provider)});
}

void sync_registered_symbolic_function_metadata(
    const kernel::SymbolicFunctionSpec& spec,
    EvaluationContext& ctx) {
    const auto origin = spec.metadata.source == kernel::RegistrationSource::pack
        ? symbols::DefinitionOrigin::pack
        : symbols::DefinitionOrigin::builtin;
    ensure_symbol_metadata(
        ctx,
        spec.metadata.name,
        origin,
        spec.metadata.owning_package,
        {},
        spec.metadata.documentation);
    ensure_definition_record(
        ctx,
        spec.metadata.name,
        symbols::SymbolDefinitionKind::registered_handler,
        origin,
        spec.metadata.owning_package);
}

void sync_host_function_metadata(const std::string& name, EvaluationContext& ctx) {
    const auto* host_spec = kernel::FunctionRegistry::find_host_function(ctx.host_functions(), name);
    if (host_spec == nullptr) {
        return;
    }
    ensure_symbol_metadata(
        ctx,
        name,
        symbols::DefinitionOrigin::builtin,
        "host",
        {},
        host_spec->description);
    ensure_definition_record(
        ctx,
        name,
        symbols::SymbolDefinitionKind::registered_handler,
        symbols::DefinitionOrigin::builtin,
        "host");
}

void sync_user_function_metadata(const std::string& name, EvaluationContext& ctx) {
    if (!ctx.function_definitions.contains(name)) {
        return;
    }
    ensure_symbol_metadata(ctx, name, symbols::DefinitionOrigin::user);
    ensure_definition_record(
        ctx,
        name,
        symbols::SymbolDefinitionKind::user_function,
        symbols::DefinitionOrigin::user);
}

void sync_symbol_contracts_for_call(const FunctionCall& func, EvaluationContext& ctx) {
    auto& registry = packs::PackRegistry::instance();
    if (const auto* spec = registry.find_symbolic_function_spec(func.head)) {
        sync_registered_symbolic_function_metadata(*spec, ctx);
    }
    sync_user_function_metadata(func.head, ctx);
    sync_host_function_metadata(func.head, ctx);
}

FunctionDispatchContract build_function_dispatch_contract(
    const FunctionCall& func,
    EvaluationContext& ctx) {
    sync_symbol_contracts_for_call(func, ctx);

    FunctionDispatchContract contract;
    auto& registry = packs::PackRegistry::instance();
    contract.has_registered_symbolic_handler =
        ctx.definition_records.contains(func.head, symbols::SymbolDefinitionKind::registered_handler) &&
        registry.find_symbolic_function_spec(func.head) != nullptr;
    contract.has_user_function =
        ctx.definition_records.contains(func.head, symbols::SymbolDefinitionKind::user_function) &&
        is_user_defined_function(func.head, ctx);
    contract.has_host_function =
        ctx.definition_records.contains(func.head, symbols::SymbolDefinitionKind::registered_handler) &&
        kernel::FunctionRegistry::find_host_function(ctx.host_functions(), func.head) != nullptr;
    return contract;
}

bool matches_value_type(const Value& value, ValueType expected) noexcept {
    switch (expected) {
        case ValueType::any:
            return true;
        case ValueType::number:
            return value.is_number();
        case ValueType::boolean:
            return value.is_boolean();
        case ValueType::string:
            return value.is_string();
        case ValueType::list:
            return value.is_list();
    }
    return false;
}

std::string value_type_name(ValueType type) {
    switch (type) {
        case ValueType::any:
            return "any";
        case ValueType::number:
            return "number";
        case ValueType::boolean:
            return "boolean";
        case ValueType::string:
            return "string";
        case ValueType::list:
            return "list";
    }
    return "any";
}

std::string value_type_name(const Value& value) {
    if (value.is_number()) {
        return "number";
    }
    if (value.is_boolean()) {
        return "boolean";
    }
    if (value.is_string()) {
        return "string";
    }
    if (value.is_list()) {
        return "list";
    }
    return "null";
}

std::optional<Value> expr_to_sdk_value(const ExprPtr& expr) {
    if (expr == nullptr) {
        return std::nullopt;
    }

    if (const auto* number = std::get_if<Number>(&*expr)) {
        double value = number->value;
        if (value == 0.0) {
            value = 0.0;
        }
        return Value(value);
    }
    if (const auto* rational = std::get_if<Rational>(&*expr)) {
        return Value(static_cast<double>(rational->numerator) / rational->denominator);
    }
    if (const auto* boolean = std::get_if<Boolean>(&*expr)) {
        return Value(boolean->value);
    }
    if (const auto* string = std::get_if<String>(&*expr)) {
        return Value(string->value);
    }
    if (const auto* list = std::get_if<List>(&*expr)) {
        Value::List values;
        values.reserve(list->elements.size());
        for (const auto& element : list->elements) {
            auto converted = expr_to_sdk_value(element);
            if (!converted.has_value()) {
                return std::nullopt;
            }
            values.push_back(std::move(*converted));
        }
        return Value(std::move(values));
    }

    return std::nullopt;
}

ExprPtr sdk_value_to_expr(const Value& value) {
    if (const auto* number = value.as_number()) {
        return make_expr<Number>(*number);
    }
    if (const auto* boolean = value.as_boolean()) {
        return make_expr<Boolean>(*boolean);
    }
    if (const auto* string = value.as_string()) {
        return make_expr<String>(*string);
    }
    if (const auto* list = value.as_list()) {
        std::vector<ExprPtr> elements;
        elements.reserve(list->size());
        for (const auto& element : *list) {
            elements.push_back(sdk_value_to_expr(element));
        }
        return std::make_shared<Expr>(List{elements});
    }
    return make_expr<Indeterminate>();
}

ExprPtr evaluate_host_function(const FunctionCall& func, EvaluationContext& ctx) {
    const auto* spec = kernel::FunctionRegistry::find_host_function(ctx.host_functions(), func.head);
    if (spec == nullptr) {
        return nullptr;
    }

    if (!spec->arity.allows(func.args.size())) {
        kernel::throw_runtime_error(
            kernel::ErrorCode::invalid_call,
            "Host function `" + func.head + "` was called with an invalid arity.");
    }

    std::vector<Value> arguments;
    arguments.reserve(func.args.size());
    for (std::size_t index = 0; index < func.args.size(); ++index) {
        auto evaluated_arg = evaluate(func.args[index], ctx);
        auto converted = expr_to_sdk_value(evaluated_arg);
        if (!converted.has_value()) {
            kernel::throw_runtime_error(
                kernel::ErrorCode::invalid_argument_type,
                "Host function `" + func.head + "` requires SDK-compatible concrete argument values.");
        }
        if (index < spec->parameters.size() &&
            !matches_value_type(*converted, spec->parameters[index].type)) {
            kernel::throw_runtime_error(
                kernel::ErrorCode::invalid_argument_type,
                "Host function `" + func.head + "` expects argument " +
                    std::to_string(index + 1) + " to be `" +
                    value_type_name(spec->parameters[index].type) + "`, but found `" +
                    value_type_name(*converted) + "`.");
        }
        arguments.push_back(std::move(*converted));
    }

    auto callback_result = spec->callback(arguments);
    if (callback_result.value.has_value() == callback_result.error.has_value()) {
        kernel::throw_runtime_error(
            kernel::ErrorCode::invalid_host_result,
            "Host function `" + func.head + "` returned an invalid evaluation result.");
    }
    if (callback_result.error.has_value()) {
        throw kernel::RuntimeFailure(*callback_result.error);
    }
    if (spec->return_type.has_value() &&
        !matches_value_type(*callback_result.value, *spec->return_type)) {
        kernel::throw_runtime_error(
            kernel::ErrorCode::invalid_host_result,
            "Host function `" + func.head + "` declared return type `" +
                value_type_name(*spec->return_type) + "`, but returned `" +
                value_type_name(*callback_result.value) + "`.");
    }

    return sdk_value_to_expr(*callback_result.value);
}

enum class GeneralFunctionResolutionStage {
    special_form,
    registered_symbolic_function,
    builtin_function,
    user_defined_function,
    host_function,
    unresolved_symbolic_fallback
};

std::optional<ExprPtr> try_resolve_special_form(const FunctionCall& func, EvaluationContext& ctx) {
    if (is_special_form_function(func.head)) {
        return evaluate_special_form(func, ctx);
    }
    return std::nullopt;
}

std::optional<ExprPtr> try_resolve_registered_symbolic_function(
    const FunctionCall& func,
    EvaluationContext& ctx,
    const FunctionDispatchContract& contract) {
    if (!contract.has_registered_symbolic_handler) {
        return std::nullopt;
    }
    auto& registry = packs::PackRegistry::instance();
    if (const auto* spec = registry.find_symbolic_function_spec(func.head)) {
        return spec->handler(func, ctx);
    }
    return std::nullopt;
}

std::optional<ExprPtr> try_resolve_builtin_function(const FunctionCall& func, EvaluationContext& ctx) {
    if (auto builtin_result = evaluate_builtin_function(func, ctx)) {
        return builtin_result;
    }
    return std::nullopt;
}

std::optional<ExprPtr> try_resolve_user_defined_function(
    const FunctionCall& func,
    EvaluationContext& ctx,
    const FunctionDispatchContract& contract) {
    if (contract.has_user_function) {
        return evaluate_user_defined_function(func, ctx);
    }
    return std::nullopt;
}

std::optional<ExprPtr> try_resolve_host_function(
    const FunctionCall& func,
    EvaluationContext& ctx,
    const FunctionDispatchContract& contract) {
    if (contract.has_host_function) {
        return evaluate_host_function(func, ctx);
    }
    return std::nullopt;
}

ExprPtr unresolved_symbolic_fallback(const FunctionCall& func) {
    return make_expr<FunctionCall>(func.head, func.args);
}

ExprPtr evaluate_general_function(const FunctionCall& func, EvaluationContext& ctx) {
    const auto contract = build_function_dispatch_contract(func, ctx);

    constexpr GeneralFunctionResolutionStage stages[] = {
        GeneralFunctionResolutionStage::special_form,
        GeneralFunctionResolutionStage::registered_symbolic_function,
        GeneralFunctionResolutionStage::builtin_function,
        GeneralFunctionResolutionStage::user_defined_function,
        GeneralFunctionResolutionStage::host_function,
        GeneralFunctionResolutionStage::unresolved_symbolic_fallback
    };

    for (const auto stage : stages) {
        switch (stage) {
            case GeneralFunctionResolutionStage::special_form:
                if (auto resolved = try_resolve_special_form(func, ctx)) {
                    return *resolved;
                }
                break;
            case GeneralFunctionResolutionStage::registered_symbolic_function:
                if (auto resolved = try_resolve_registered_symbolic_function(func, ctx, contract)) {
                    return *resolved;
                }
                break;
            case GeneralFunctionResolutionStage::builtin_function:
                if (auto resolved = try_resolve_builtin_function(func, ctx)) {
                    return *resolved;
                }
                break;
            case GeneralFunctionResolutionStage::user_defined_function:
                if (auto resolved = try_resolve_user_defined_function(func, ctx, contract)) {
                    return *resolved;
                }
                break;
            case GeneralFunctionResolutionStage::host_function:
                if (auto resolved = try_resolve_host_function(func, ctx, contract)) {
                    return *resolved;
                }
                break;
            case GeneralFunctionResolutionStage::unresolved_symbolic_fallback:
                return unresolved_symbolic_fallback(func);
        }
    }

    return unresolved_symbolic_fallback(func);
}

ExprPtr resolve_symbol_value(const Symbol& sym, EvaluationContext& ctx, std::unordered_set<std::string>& visited) {
    if (visited.count(sym.name)) {
        return make_expr<Symbol>(sym.name);
    }

    const ExprPtr* value = ctx.symbol_values.lookup(sym.name);
    if (value == nullptr) {
        if (ctx.strict_runtime_semantics()) {
            kernel::throw_runtime_error(
                kernel::ErrorCode::unknown_binding,
                "No binding was provided for variable `" + sym.name + "`.");
        }
        return make_expr<Symbol>(sym.name);
    }

    visited.insert(sym.name);
    auto result = evaluate_impl(*value, ctx, visited);
    visited.erase(sym.name);
    return result;
}

ExprPtr evaluate_impl(const ExprPtr& expr, EvaluationContext& ctx, std::unordered_set<std::string>& visited) {
    ctx.consume_evaluation_step();
    ALEPH3_LOG("evaluate: input = " << to_string_raw(expr));

    auto result = std::visit(overloaded{
        [](const Number& num) -> ExprPtr {
            return make_expr<Number>(num.value);
        },
        [](const Complex& c) -> ExprPtr {
            return make_expr<Complex>(c.real, c.imag);
        },
        [](const Rational& r) -> ExprPtr {
            auto [n, d] = normalize_rational(r.numerator, r.denominator);
            return make_expr<Rational>(n, d);
        },
        [](const Boolean& boolean) -> ExprPtr {
            return make_expr<Boolean>(boolean.value);
        },
        [](const String& str) -> ExprPtr {
            return make_expr<String>(str.value);
        },
        [&](const Symbol& sym) -> ExprPtr {
            return resolve_symbol_value(sym, ctx, visited);
        },
        [&](const FunctionCall& func) -> ExprPtr {
            if (is_algebra_function(func.head)) {
                return evaluate_algebra_function(func, ctx);
            }

            if (is_structural_function(func.head)) {
                std::vector<ExprPtr> evaluated_elements;
                evaluated_elements.reserve(func.args.size());
                for (const auto& arg : func.args) {
                    evaluated_elements.push_back(evaluate(arg, ctx));
                }
                return make_expr<List>(evaluated_elements);
            }

            return evaluate_general_function(func, ctx);
        },
        [&](const FunctionDefinition& def) -> ExprPtr {
            return register_user_defined_function(def, ctx);
        },
        [&](const Assignment& assign) -> ExprPtr {
            ensure_symbol_metadata(ctx, assign.name, symbols::DefinitionOrigin::user);
            ensure_definition_record(
                ctx,
                assign.name,
                symbols::SymbolDefinitionKind::own_value,
                symbols::DefinitionOrigin::user);
            ctx.symbol_values.set(assign.name, evaluate(assign.value, ctx));
            return make_expr<Symbol>(assign.name);
        },
        [&](const Rule& rule) -> ExprPtr {
            auto lhs = evaluate_impl(rule.lhs, ctx, visited);
            auto rhs = evaluate_impl(rule.rhs, ctx, visited);
            return make_expr<Rule>(lhs, rhs);
        },
        [](const List& list) -> ExprPtr {
            return std::make_shared<Expr>(list);
        },
        [](const Infinity&) -> ExprPtr {
            return make_expr<Infinity>();
        },
        [](const ComplexInfinity&) -> ExprPtr {
            return make_expr<ComplexInfinity>();
        },
        [](const Indeterminate&) -> ExprPtr {
            return make_expr<Indeterminate>();
        }
    }, *expr);

    ALEPH3_LOG("evaluate: result = " << to_string_raw(result));
    return result;
}

}  // namespace

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx) {
    ExprPtr norm = normalize_expr(expr);
    std::unordered_set<std::string> visited;
    return evaluate_impl(norm, ctx, visited);
}

std::string expr_to_key(const ExprPtr& expr) {
    auto norm = normalize_expr(expr);
    std::string s = to_string_raw(norm);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

}  // namespace aleph3
