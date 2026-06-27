#include "sdk/Engine.hpp"

#include "frontend/Parser.hpp"
#include "evaluator/BuiltInFunctions.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "ir/Node.hpp"
#include "kernel/Diagnostics.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "kernel/TrustedSubsetBridge.hpp"
#include "runtime/Evaluator.hpp"
#include "semantics/Validator.hpp"

#include <cctype>
#include <cmath>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace aleph3 {

namespace {

bool is_blank(std::string_view source) {
    for (const char ch : source) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

Diagnostic make_error(std::string code, std::string message) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::error;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    return diagnostic;
}

RuntimeError make_runtime_error(std::string code, std::string message) {
    RuntimeError error;
    error.code = std::move(code);
    error.message = std::move(message);
    return error;
}

std::size_t required_parameter_count(const HostFunctionSpec& spec) noexcept {
    std::size_t required = 0;
    for (const auto& parameter : spec.parameters) {
        if (parameter.required) {
            ++required;
        }
    }
    return required;
}

struct FrontendPassResult {
    ir::NodePtr root;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return root != nullptr && diagnostics.empty();
    }
};

FrontendPassResult parse_and_validate(
    std::string_view source,
    const Schema& schema,
    const Policy& policy) {
    FrontendPassResult result;

    frontend::Parser parser(source);
    auto parse_result = parser.parse();
    if (!parse_result.ok()) {
        result.diagnostics = std::move(parse_result.diagnostics);
        return result;
    }

    semantics::Validator validator(schema, policy);
    auto summary = validator.validate(parse_result.root);
    if (!summary.ok()) {
        result.diagnostics = std::move(summary.diagnostics);
        return result;
    }

    result.root = std::move(parse_result.root);
    return result;
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

bool contains_non_finite_number(const Value& value) {
    if (const auto* number = value.as_number()) {
        return !std::isfinite(*number);
    }
    if (const auto* list = value.as_list()) {
        for (const auto& element : *list) {
            if (contains_non_finite_number(element)) {
                return true;
            }
        }
    }
    return false;
}

bool contains_non_finite_numbers(const Bindings& bindings) {
    for (const auto& [_, value] : bindings) {
        if (contains_non_finite_number(value)) {
            return true;
        }
    }
    return false;
}

bool has_runtime_sensitive_semantics(const ExprPtr& expr) {
    if (expr == nullptr) {
        return false;
    }

    if (const auto* call = std::get_if<FunctionCall>(&*expr)) {
        if (call->head == "Divide" ||
            call->head == "Power" ||
            call->head == "Equal" ||
            call->head == "NotEqual" ||
            call->head == "Less" ||
            call->head == "LessEqual" ||
            call->head == "Greater" ||
            call->head == "GreaterEqual") {
            return true;
        }
        for (const auto& arg : call->args) {
            if (has_runtime_sensitive_semantics(arg)) {
                return true;
            }
        }
        return false;
    }

    if (const auto* list = std::get_if<List>(&*expr)) {
        for (const auto& element : list->elements) {
            if (has_runtime_sensitive_semantics(element)) {
                return true;
            }
        }
    }

    return false;
}

void seed_kernel_symbols(
    kernel::EvaluationContext& ctx,
    const Bindings& constants,
    const Bindings& bindings) {
    for (const auto& [name, value] : constants) {
        ctx.symbol_values.set(name, sdk_value_to_expr(value));
    }
    for (const auto& [name, value] : bindings) {
        ctx.symbol_values.set(name, sdk_value_to_expr(value));
    }
}

EvaluationResult evaluate_via_kernel_bridge(
    const ExprPtr& kernel_expr,
    const Bindings& bindings,
    const Bindings& constants,
    const std::unordered_map<std::string, HostFunctionSpec>& host_functions,
    const Policy& policy) {
    static std::once_flag builtins_once;
    std::call_once(builtins_once, []() {
        register_built_in_functions();
    });

    kernel::EvaluationContext ctx(bindings, constants, host_functions, policy);
    seed_kernel_symbols(ctx, constants, bindings);

    auto result_expr = evaluate(kernel_expr, ctx);
    auto value = expr_to_sdk_value(result_expr);
    if (value.has_value()) {
        EvaluationResult result;
        result.value = std::move(*value);
        return result;
    }

    return {};
}

}  // namespace

namespace sdk_detail {
struct CompiledFormulaData {
    ir::NodePtr root;
    // Migration-only: cache the lowered kernel form beside the trusted-subset
    // IR so SDK execution can move without preserving both as semantic peers.
    ExprPtr kernel_expr;
    Policy policy;
    Bindings constants;
    std::string source;
};
}  // namespace sdk_detail

struct Engine::State {
    explicit State(EngineOptions engine_options) : options(std::move(engine_options)) {}

    EngineOptions options;
    std::unordered_map<std::string, HostFunctionSpec> host_functions;
    mutable std::mutex mutex;
};

Engine::Engine(EngineOptions options) : state_(std::make_shared<State>(std::move(options))) {}

void Engine::register_function(HostFunctionSpec spec) {
    if (spec.name.empty()) {
        throw std::invalid_argument("Host function name must not be empty.");
    }
    if (!spec.callback) {
        throw std::invalid_argument("Host function callback must be set.");
    }
    if (spec.arity.min_arguments > spec.arity.max_arguments) {
        throw std::invalid_argument("Host function arity range must be valid.");
    }
    if (!spec.parameters.empty()) {
        const std::size_t required_parameters = required_parameter_count(spec);
        if (required_parameters < spec.arity.min_arguments ||
            spec.parameters.size() > spec.arity.max_arguments) {
            throw std::invalid_argument(
                "Host function parameter metadata must fit within the declared arity.");
        }
        bool optional_seen = false;
        for (const auto& parameter : spec.parameters) {
            if (parameter.name.empty()) {
                throw std::invalid_argument("Host function parameter names must not be empty.");
            }
            if (!parameter.required) {
                optional_seen = true;
            } else if (optional_seen) {
                throw std::invalid_argument(
                    "Required host function parameters must not follow optional parameters.");
            }
        }
    }

    std::lock_guard<std::mutex> lock(state_->mutex);
    kernel::FunctionRegistry::register_host_function(state_->host_functions, std::move(spec));
}

CompileResult Engine::compile(
    std::string_view source,
    const Schema& schema,
    const Policy& policy) const {
    CompileResult result;

    if (is_blank(source)) {
        result.diagnostics.push_back(
            make_error("sdk.source.empty", "Formula source must not be empty."));
        return result;
    }

    auto frontend_result = parse_and_validate(source, schema, policy);
    if (!frontend_result.ok()) {
        result.diagnostics = std::move(frontend_result.diagnostics);
        return result;
    }

    auto staged_formula = kernel::stage_trusted_subset_formula(frontend_result.root);
    if (!staged_formula.ok()) {
        result.diagnostics = std::move(staged_formula.diagnostics);
        return result;
    }

    auto state = std::make_shared<sdk_detail::CompiledFormulaData>();
    state->root = staged_formula.trusted_root;
    state->kernel_expr = staged_formula.kernel_expr;
    state->policy = policy;
    state->constants = schema.constant_values();
    if (state_->options.retain_source_text) {
        state->source = std::string(source);
    }

    result.formula = CompiledFormula(std::move(state));
    return result;
}

ValidationResult Engine::validate(
    std::string_view source,
    const Schema& schema,
    const Policy& policy) const {
    ValidationResult result;

    if (is_blank(source)) {
        result.ok = false;
        result.diagnostics.push_back(
            make_error("sdk.source.empty", "Formula source must not be empty."));
        return result;
    }

    auto frontend_result = parse_and_validate(source, schema, policy);
    if (!frontend_result.ok()) {
        result.ok = false;
        result.diagnostics = std::move(frontend_result.diagnostics);
        return result;
    }

    result.ok = true;
    return result;
}

EvaluationResult Engine::evaluate(
    const CompiledFormula& formula,
    const Bindings& bindings) const {
    if (formula.empty()) {
        EvaluationResult result;
        result.error = make_runtime_error(
            "sdk.formula.empty",
            "Cannot evaluate an empty compiled formula.");
        return result;
    }

    std::unordered_map<std::string, HostFunctionSpec> host_functions;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        host_functions = state_->host_functions;
    }

    if (formula.state_->kernel_expr != nullptr &&
        !contains_non_finite_numbers(bindings) &&
        !contains_non_finite_numbers(formula.state_->constants) &&
        !has_runtime_sensitive_semantics(formula.state_->kernel_expr)) {
        try {
            auto bridged = evaluate_via_kernel_bridge(
                formula.state_->kernel_expr,
                bindings,
                formula.state_->constants,
                host_functions,
                formula.state_->policy);
            if (bridged.ok()) {
                return bridged;
            }
        } catch (const kernel::RuntimeFailure& failure) {
            EvaluationResult result;
            result.error = failure.error();
            return result;
        } catch (const EvaluatorError&) {
            // Preserve current SDK behavior while the kernel bridge is still incomplete.
        } catch (const std::runtime_error&) {
            // Preserve current SDK behavior while the kernel bridge is still incomplete.
        }
    }

    runtime::Evaluator evaluator({bindings, formula.state_->constants, host_functions, formula.state_->policy});
    return evaluator.evaluate(formula.state_->root);
}

}  // namespace aleph3
