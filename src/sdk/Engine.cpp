#include "sdk/Engine.hpp"

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

}  // namespace

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

    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->host_functions[spec.name] = std::move(spec);
}

CompileResult Engine::compile(
    std::string_view source,
    const Schema&,
    const Policy&) const {
    CompileResult result;

    if (is_blank(source)) {
        result.diagnostics.push_back(
            make_error("sdk.source.empty", "Formula source must not be empty."));
        return result;
    }

    result.diagnostics.push_back(
        make_error(
            "sdk.compile.not_implemented",
            "Formula compilation for the rewrite path is not implemented yet."));
    return result;
}

ValidationResult Engine::validate(
    std::string_view source,
    const Schema&,
    const Policy&) const {
    ValidationResult result;

    if (is_blank(source)) {
        result.ok = false;
        result.diagnostics.push_back(
            make_error("sdk.source.empty", "Formula source must not be empty."));
        return result;
    }

    result.ok = false;
    result.diagnostics.push_back(
        make_error(
            "sdk.validate.not_implemented",
            "Formula validation for the rewrite path is not implemented yet."));
    return result;
}

EvaluationResult Engine::evaluate(
    const CompiledFormula& formula,
    const Bindings&) const {
    EvaluationResult result;

    if (formula.empty()) {
        result.error = make_runtime_error(
            "sdk.formula.empty",
            "Cannot evaluate an empty compiled formula.");
        return result;
    }

    result.error = make_runtime_error(
        "sdk.evaluate.not_implemented",
        "Formula evaluation for the rewrite path is not implemented yet.");
    return result;
}

}  // namespace aleph3
