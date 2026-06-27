#include "sdk/Engine.hpp"

#include "frontend/Parser.hpp"
#include "ir/Node.hpp"
#include "kernel/Diagnostics.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "kernel/TrustedSubsetBridge.hpp"
#include "semantics/Validator.hpp"

#include <cctype>
#include <mutex>
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

    return kernel::evaluate_trusted_subset_formula(
        formula.state_->kernel_expr,
        bindings,
        formula.state_->constants,
        host_functions,
        formula.state_->policy);
}

}  // namespace aleph3
